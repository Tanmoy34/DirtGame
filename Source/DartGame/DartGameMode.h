#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/PlayerController.h" // for ClientMessage
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
	// Declaration only — implementation lives in DartGameMode.cpp
	void AddScore(int32 PlayerIndex, int32 Points);

private:
	FTimerHandle TurnTimerHandle;
	void OnTurnTimeout();

	// --- Added bookkeeping ---
	// Tracks total points per player index
	TMap<int32, int32> PlayerScores;

	// Tracks how many darts each player has used in this round
	TMap<int32, int32> PlayerDartsUsed;

	// Send a simple textual scoreboard to every connected player (ClientMessage).
	// Implemented here inline to avoid touching extra files; called on server.
	void BroadcastTotalScores()
	{
		// Compose message
		FString Msg = TEXT("=== Round complete — Total Scores ===\n");
		for (const TPair<int32,int32>& KV : PlayerScores)
		{
			Msg += FString::Printf(TEXT("Player %d : %d\n"), KV.Key, KV.Value);
		}

		// Send to every connected player controller
		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					PC->ClientMessage(Msg);
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
	}
	// --- end bookkeeping ---
};