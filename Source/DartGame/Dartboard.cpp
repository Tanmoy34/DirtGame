#include "Dartboard.h"
#include "DartProjectile.h"
#include "DartGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"           // GEngine->AddOnScreenDebugMessage
#include "GameFramework/PlayerController.h"
#include "DartCharacter.h"           // << added to update the throwing character's score

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ADartboard::ADartboard()
{
	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	RootComponent = BoardMesh;

	LastScore = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// CalculatePoints
//
//  Pure function — no side-effects, just maps a distance to a score.
//  Having this separate makes unit-testing trivial and keeps RegisterHit clean.
// ─────────────────────────────────────────────────────────────────────────────

int32 ADartboard::CalculatePoints(float Dist) const
{
	if (Dist <= BullseyeRadius) return 100;   // Red dot — dead centre
	if (Dist <= SkyBlueRadius)  return 80;    // Cyan / sky-blue ring
	if (Dist <= DeepBlueRadius) return 40;    // Deep blue ring
	if (Dist <= GreenRadius)    return 10;    // Green outer ring
	return 0;                                  // Miss — outside the board
}

// ─────────────────────────────────────────────────────────────────────────────
// PrintHitResult  (server helper — routes message to the correct screen)
//
//  On a dedicated server GEngine is usually valid but has no viewport;
//  the message won't be visible there anyway.  We call it on the server
//  because that's where RegisterHit runs.  If you later add a Client RPC
//  (like Client_PrintThrowDiagnostics in DartCharacter) you can mirror this
//  message to the throwing player's screen the same way.
// ─────────────────────────────────────────────────────────────────────────────

void ADartboard::PrintHitResult(int32 Points, float Dist, AController* OwnerController) const
{
	// ── Zone label ────────────────────────────────────────────────────────────
	FString ZoneLabel;
	FColor  ZoneColor;

	if (Points == 100) { ZoneLabel = TEXT("BULLSEYE  (red centre)");    ZoneColor = FColor::Red;    }
	else if (Points == 80) { ZoneLabel = TEXT("Sky-Blue ring");          ZoneColor = FColor::Cyan;   }
	else if (Points == 40) { ZoneLabel = TEXT("Deep-Blue ring");         ZoneColor = FColor(0, 80, 200); }
	else if (Points == 10) { ZoneLabel = TEXT("Green outer ring");       ZoneColor = FColor::Green;  }
	else                   { ZoneLabel = TEXT("MISS — outside board");   ZoneColor = FColor::White;  }

	// ── On-screen messages (slot 10 & 11 so they don't clash with DartCharacter slots 1-5) ──

	// Line 1 — zone + points
	GEngine->AddOnScreenDebugMessage(
		10,     // key (constant → new hit overwrites old, no spam)
		6.f,    // display time (seconds)
		ZoneColor,
		FString::Printf(TEXT("HIT ► %s  |  +%d pts"), *ZoneLabel, Points)
	);

	// Line 2 — radial distance from centre (useful for tuning radii in the editor)
	GEngine->AddOnScreenDebugMessage(
		11,
		6.f,
		FColor::White,
		FString::Printf(TEXT("    Distance from centre: %.2f cm"), Dist)
	);

	// ── Log (visible in Output Log / dedicated server console) ───────────────
	UE_LOG(LogTemp, Warning,
		TEXT("[Dartboard] Hit | Zone=%s | Points=%d | Dist=%.2f"),
		*ZoneLabel, Points, Dist);
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterHit  (server authoritative)
// ─────────────────────────────────────────────────────────────────────────────

int32 ADartboard::RegisterHit(const FVector& ImpactPoint, ADartProjectile* Dart)
{
	// Only the server should award points and print results.
	if (!HasAuthority()) return 0;

	// ── 1. Convert world-space impact to board-local space ───────────────────
	const FVector LocalHit = GetTransform().InverseTransformPosition(ImpactPoint);

	// Use Y and Z — the two axes in the board's face plane.
	const float Dist = FVector2D(LocalHit.Y, LocalHit.Z).Size();

	// ── 2. Map distance → points ─────────────────────────────────────────────
	const int32 Points = CalculatePoints(Dist);

	// ── 3. Print to screen ───────────────────────────────────────────────────
	AController* OwnerController = nullptr;
	if (Dart && Dart->GetOwner())
	{
		if (APawn* OwnerPawn = Cast<APawn>(Dart->GetOwner()))
		{
			OwnerController = OwnerPawn->GetController();
		}
	}

	PrintHitResult(Points, Dist, OwnerController);

	// ── 4. Award the score — credit the actual throwing player (server) ────────
	// Find the owning pawn/character and call its handler so RoundScore/PlayerScore
	// and LastScoredPoints live on the character (replicated per-player).
	if (Dart && Dart->GetOwner())
	{
		if (APawn* OwnerPawn = Cast<APawn>(Dart->GetOwner()))
		{
			if (ADartCharacter* OwnerChar = Cast<ADartCharacter>(OwnerPawn))
			{
				// Prefer calling AddPoints which routes to server logic if needed.
				// RegisterHit runs on the server so this will simply execute.
				OwnerChar->AddPoints(Points);

				// Optional: for board-side UI we can reflect the player's total.
				LastScore = OwnerChar->GetPlayerScore();

				UE_LOG(LogTemp, Warning,
					TEXT("[Dartboard] Awarded %d pts to player (char %s). New total: %d"),
					Points, *OwnerChar->GetName(), LastScore);

				// Notify GameMode that this player's dart has resolved (so it can
				// update turn state / advance to next player when needed).
				if (ADartGameMode* GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
				{
					// use the OwnerController variable declared earlier instead of
					// declaring a new local variable (avoids name hiding)
					if (OwnerController)
					{
						GM->RegisterThrow(Cast<APlayerController>(OwnerController), Points);
					}
				}
			}
		}
	}
	else
	{
		// Fallback: if we couldn't resolve an owner, you can still record via GameMode.
		// (Left commented so it doesn't silently credit player 0 anymore.)
		// if (ADartGameMode* GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
		// {
		//     GM->AddScore(0, Points);
		// }
	}

	// -------------------------------------------------------------------------
	// Aggregate into LastScore (cumulative for the board) removed in favor of
	// per-player totals handled on ADartCharacter. We still call the Blueprint
	// notify so any board-side UI can update if desired.
	// -------------------------------------------------------------------------

	// Notify any Blueprint listeners so widgets can update immediately.
	BP_OnLastScoreUpdated(LastScore);

	// Debug log (optional)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green,
			FString::Printf(TEXT("Board: hit points=%d  reported total=%d"), Points, LastScore));
	}

	return Points;
}