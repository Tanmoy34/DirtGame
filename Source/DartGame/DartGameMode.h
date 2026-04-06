#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DartGameMode.generated.h"

UCLASS()
class DARTGAME_API ADartGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	ADartGameMode();

	// Called by server to advance to next player's turn
	void AdvanceTurn();

	// Called when a throw scores; adds points server-side
	void AddScore(int32 PlayerIndex, int32 Points);

private:
	FTimerHandle TurnTimerHandle;
	void OnTurnTimeout();
};