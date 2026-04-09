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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dart")
    TSubclassOf<ADartProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dart")
    float MaxThrowSpeed = 3000.f;

  
     
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dart",
              meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float MaxInaccuracyAngle = 15.f;

    
    UPROPERTY(BlueprintReadWrite, Category = "Dart")
    float TimingAccuracy = 0.5f;

    // HUD / gameplay state (player-specific, replicated)
    UPROPERTY(ReplicatedUsing = OnRep_DartsRemaining, BlueprintReadOnly, Category = "Darts")
    int32 DartsRemaining = 3; // default 3 darts

    
    UPROPERTY(ReplicatedUsing = OnRep_bIsMyTurn, BlueprintReadOnly, Category = "Darts")
    bool bIsMyTurn = false;

    UPROPERTY(ReplicatedUsing = OnRep_PlayerScore, BlueprintReadOnly, Category = "Darts")
    int32 PlayerScore = 0; // player's total score (all rounds combined)

   
    UPROPERTY(ReplicatedUsing = OnRep_RoundScore, BlueprintReadOnly, Category = "Darts")
    int32 RoundScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Dart")
    int32 AimCountdown = 0;

   
    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetDartsRemaining() const { return DartsRemaining; }

    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetPlayerScore() const { return PlayerScore; }

    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetRoundScore() const { return RoundScore; }

    UFUNCTION(Server, Reliable)
    void Server_ConsumeDart(bool bForfeit);

    
    void Server_AddRoundScore(int32 Points);

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent *Input) override;

    
    UFUNCTION()
    void OnRep_DartsRemaining();

    
    UFUNCTION()
    void OnRep_bIsMyTurn();

    
    UFUNCTION()
    void OnRep_PlayerScore();

    
    UFUNCTION()
    void OnRep_RoundScore();

   
    UPROPERTY(ReplicatedUsing = OnRep_PlayerDisplayName, BlueprintReadOnly, Category = "Player")
    FString PlayerDisplayName;

    
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player")
    int32 PlayerSlot = -1;

    
    UFUNCTION()
    void OnRep_PlayerDisplayName();

    
    UFUNCTION(BlueprintImplementableEvent, Category = "Player")
    void BP_OnPlayerNameUpdated(const FString &NewName);

    // Simple Blueprint-callable helpers for UMG / widgets
    UFUNCTION(BlueprintCallable, Category = "HUD")
    FString GetPlayerName() const { return PlayerDisplayName; }

    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetPlayerSlot() const { return PlayerSlot; }

    UFUNCTION(BlueprintCallable, Category = "HUD")
    bool IsActiveTurn() const { return bIsMyTurn; }

private:
    // ── Components 
    UPROPERTY(VisibleAnywhere)
    USpringArmComponent *SpringArm;
    UPROPERTY(VisibleAnywhere)
    UCameraComponent *Camera;

    // ── Movement 
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    // ── Throwing
    void StartAim();
    void Throw();
    bool bIsAiming = false;

    // ── Aim-forfeit timer 
    FTimerHandle AimTimerHandle;
    void AimTick();
    void ClearAimTimer();

    // ── RPCs

    
    UFUNCTION(Server, Reliable)
    void Server_Throw(FVector Origin, FVector Direction, float Speed, float Accuracy);

   
    UFUNCTION(Client, Reliable)
    void Client_PrintThrowDiagnostics(float Accuracy, float Inaccuracy, float DeviationDeg);

public:
    
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnDartsRemainingUpdated(int32 NewRemaining);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnPlayerScoreUpdated(int32 NewScore);

    
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnRoundScoreUpdated(int32 NewRoundScore);

    
    UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
    void BP_OnTurnStarted(int32 SlotIndex);

   
    UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
    void BP_OnTurnEnded(int32 SlotIndex);


    UPROPERTY(ReplicatedUsing = OnRep_LastScoredPoints, BlueprintReadOnly, Category = "Darts")
    int32 LastScoredPoints = 0;

   
    UFUNCTION()
    void OnRep_LastScoredPoints();

   
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnLastScoredUpdated(int32 NewLastPoints);


    UFUNCTION(BlueprintCallable, Category = "Dart")
    void AddPoints(int32 Points);

    
    UFUNCTION(Server, Reliable)
    void Server_RequestAddPoints(int32 Points);
};