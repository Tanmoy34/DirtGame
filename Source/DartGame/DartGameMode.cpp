#include "DartGameMode.h"
#include "DartGameState.h"
#include "TimerManager.h"

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
	AdvanceTurn();
}

void ADartGameMode::OnTurnTimeout()
{
	// Forfeit — advance without scoring
	AdvanceTurn();
}