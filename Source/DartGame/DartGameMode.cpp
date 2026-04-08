#include "DartGameMode.h"
#include "DartGameState.h"
#include "DartCharacter.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

ADartGameMode::ADartGameMode()
{
	GameStateClass = ADartGameState::StaticClass();
}

void ADartGameMode::AdvanceTurn()
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	GetWorldTimerManager().ClearTimer(TurnTimerHandle);

	GS->ThrowsRemaining--;
	if (GS->ThrowsRemaining <= 0)
	{
		GS->ThrowsRemaining = 3;
		GS->CurrentPlayerIndex = (GS->CurrentPlayerIndex + 1) % FMath::Max(1, GS->PlayerScores.Num());
	}

	// Start 30-second timer for next throw
	GetWorldTimerManager().SetTimer(TurnTimerHandle, this, &ADartGameMode::OnTurnTimeout, 30.f, false);
}

void ADartGameMode::AddScore(int32 PlayerIndex, int32 Points)
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS || !GS->PlayerScores.IsValidIndex(PlayerIndex)) return;
	GS->PlayerScores[PlayerIndex] += Points;

	// ── Forward points to the owning DartCharacter so RoundScore is updated ──
	//
	//  Iterate player controllers to find the one at PlayerIndex.
	//  (PlayerIndex 0 = first connected controller — matches the placeholder in Dartboard.cpp)
	if (UWorld* World = GetWorld())
	{
		int32 Idx = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It, ++Idx)
		{
			if (Idx == PlayerIndex)
			{
				if (APlayerController* PC = It->Get())
				{
					if (ADartCharacter* Char = Cast<ADartCharacter>(PC->GetPawn()))
					{
						Char->Server_AddRoundScore(Points);
					}
				}
				break;
			}
		}
	}

	AdvanceTurn();
}

void ADartGameMode::OnTurnTimeout()
{
	// Forfeit — advance without scoring
	AdvanceTurn();
}