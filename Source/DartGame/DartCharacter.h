#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DartCharacter.generated.h"

class ADartProjectile;

UCLASS()
class DARTGAME_API ADartCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ADartCharacter();

	UPROPERTY(EditAnywhere) TSubclassOf<ADartProjectile> ProjectileClass;
	UPROPERTY(EditAnywhere) float MaxThrowSpeed = 3000.f;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

	void StartAim();   // RMB pressed
	void Throw();      // RMB released

	UFUNCTION(Server, Reliable)
	void Server_Throw(FVector Origin, FVector Direction, float Speed);

private:
	bool bIsAiming = false;
	float AimStartTime = 0.f;
	float TimingAccuracy = 0.f; // Set by timing bar widget
};
