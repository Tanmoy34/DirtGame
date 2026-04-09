#include "Dartboard.h"
#include "DartProjectile.h"
#include "DartGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h" 
#include "GameFramework/PlayerController.h"
#include "DartCharacter.h" 


ADartboard::ADartboard()
{
	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	RootComponent = BoardMesh;

	LastScore = 0;
}


int32 ADartboard::CalculatePoints(float Dist) const
{
	if (Dist <= BullseyeRadius)
		return 100; // Red dot — dead centre
	if (Dist <= SkyBlueRadius)
		return 80; // Cyan / sky-blue ring
	if (Dist <= DeepBlueRadius)
		return 40; // Deep blue ring
	if (Dist <= GreenRadius)
		return 10; // Green outer ring
	return 0;	    // Miss — outside board
}




void ADartboard::PrintHitResult(int32 Points, float Dist, AController *OwnerController) const
{

	FString ZoneLabel;
	FColor ZoneColor;

	if (Points == 100)
	{
		ZoneLabel = TEXT("BULLSEYE  (red centre)");
		ZoneColor = FColor::Red;
	}
	else if (Points == 80)
	{
		ZoneLabel = TEXT("Sky-Blue ring");
		ZoneColor = FColor::Cyan;
	}
	else if (Points == 40)
	{
		ZoneLabel = TEXT("Deep-Blue ring");
		ZoneColor = FColor(0, 80, 200);
	}
	else if (Points == 10)
	{
		ZoneLabel = TEXT("Green outer ring");
		ZoneColor = FColor::Green;
	}
	else
	{
		ZoneLabel = TEXT("MISS — outside board");
		ZoneColor = FColor::White;
	}

	

	
	GEngine->AddOnScreenDebugMessage(
		10,	 
		6.f, 
		ZoneColor,
		FString::Printf(TEXT("HIT ► %s  |  +%d pts"), *ZoneLabel, Points));


	GEngine->AddOnScreenDebugMessage(
		11,
		6.f,
		FColor::White,
		FString::Printf(TEXT("    Distance from centre: %.2f cm"), Dist));

	
	UE_LOG(LogTemp, Warning,
		   TEXT("[Dartboard] Hit | Zone=%s | Points=%d | Dist=%.2f"),
		   *ZoneLabel, Points, Dist);
}


// RegisterHit  (server authoritative)


int32 ADartboard::RegisterHit(const FVector &ImpactPoint, ADartProjectile *Dart)
{
	// Only the server should award points and print results.
	if (!HasAuthority())
		return 0;

	
	const FVector LocalHit = GetTransform().InverseTransformPosition(ImpactPoint);

	
	const float Dist = FVector2D(LocalHit.Y, LocalHit.Z).Size();

	
	const int32 Points = CalculatePoints(Dist);

	
	AController *OwnerController = nullptr;
	if (Dart && Dart->GetOwner())
	{
		if (APawn *OwnerPawn = Cast<APawn>(Dart->GetOwner()))
		{
			OwnerController = OwnerPawn->GetController();
		}
	}

	PrintHitResult(Points, Dist, OwnerController);

	
	if (ADartGameMode *GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		if (OwnerController)
		{
			GM->RegisterThrow(Cast<APlayerController>(OwnerController), Points);
		}
	}

	
	LastScore = Points;

	
	BP_OnLastScoreUpdated(LastScore);


	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green,
										 FString::Printf(TEXT("Board: hit points=%d  reported total=%d"), Points, LastScore));
	}

	return Points;
}