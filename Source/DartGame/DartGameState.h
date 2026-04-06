#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DartGameState.generated.h"

UCLASS()
class DARTGAME_API ADartGameState : public AGameState
{
	GENERATED_BODY()
public:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentPlayerIndex, BlueprintReadOnly)
	int32 CurrentPlayerIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_ThrowsRemaining, BlueprintReadOnly)
	int32 ThrowsRemaining = 3;

	UPROPERTY(Replicated, BlueprintReadOnly)
	TArray<int32> PlayerScores;

	UFUNCTION()
	void OnRep_CurrentPlayerIndex();

	UFUNCTION()
	void OnRep_ThrowsRemaining();

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};