#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "DartGameMode.generated.h"

class ADartCharacter;
class ADartGameState;

UCLASS()
class DARTGAME_API ADartGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ADartGameMode();

	// ── Player registration 

	
	virtual void PostLogin(APlayerController *NewPlayer) override;

	
	virtual void Logout(AController *Exiting) override;



	
	void RegisterThrow(APlayerController *OwnerPC, int32 Points);

	
	void AddScore(int32 PlayerIndex, int32 Points);

	
	void ForfeitDart(APlayerController *OwnerPC);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn", meta = (ClampMin = "5"))
	float TurnTimeoutSeconds = 30.f;

private:
	

	FTimerHandle TurnTimerHandle;

	
	void RestartTurnTimer();

	
	void OnTurnTimeout();

	

	
	void NotifyTurnAdvance(ADartGameState *GS);

	
	void NotifyPlayerTurnStart(int32 SlotIndex);

	
	void NotifyPlayerTurnEnd(int32 SlotIndex);

	
	ADartCharacter *GetCharacterForPC(APlayerController *PC) const;
};