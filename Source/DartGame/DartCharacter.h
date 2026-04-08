#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "DartCharacter.generated.h"

class ADartProjectile;

UCLASS()
class DARTGAME_API ADartCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADartCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dart")
    TSubclassOf<ADartProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dart")
    float MaxThrowSpeed = 3000.f;

    // Set by HUD widget (0.0 to 1.0 from timing bar)
    UPROPERTY(BlueprintReadWrite, Category="Dart")
    float TimingAccuracy = 0.5f;

    // How many darts this player has left to throw (starts at 3)
    UPROPERTY(BlueprintReadOnly, Category="Dart")
    int32 DartsRemaining = 3;

    // Countdown value exposed so a widget can bind to it later
    UPROPERTY(BlueprintReadOnly, Category="Dart")
    int32 AimCountdown = 0;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

private:
    // Components
    UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
    UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;

    // Movement
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    // Throwing
    void StartAim();
    void Throw();
    bool bIsAiming = false;

    // 30-second aim forfeit timer
    FTimerHandle AimTimerHandle;
    void AimTick();          // Called every second while aiming
    void ClearAimTimer();    // Helper to cancel the timer cleanly

    UFUNCTION(Server, Reliable)
    void Server_Throw(FVector Origin, FVector Direction, float Speed);
};