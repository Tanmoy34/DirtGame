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

    /**
     * Maximum angular deviation (degrees) applied when TimingAccuracy = 0.0.
     * At accuracy 1.0 the deviation is 0°. Tweak in the editor to taste —
     * 15° is a good starting point for a dart game.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dart",
              meta=(ClampMin="0.0", ClampMax="45.0"))
    float MaxInaccuracyAngle = 15.f;

    /**
     * Written by the HUD widget's GetTimingAccuracy function (0.0 – 1.0).
     *   1.0  →  green zone perfectly centred  →  perfect throw, no deviation
     *   0.0  →  green zone at the edge        →  maximum random deviation
     */
    UPROPERTY(BlueprintReadWrite, Category="Dart")
    float TimingAccuracy = 0.5f;

    // HUD / gameplay state (player-specific, replicated)
    UPROPERTY(ReplicatedUsing = OnRep_DartsRemaining, BlueprintReadOnly, Category = "Darts")
    int32 DartsRemaining = 3;   // default 3 darts

    UPROPERTY(ReplicatedUsing = OnRep_PlayerScore, BlueprintReadOnly, Category = "Darts")
    int32 PlayerScore = 0;      // player's total score (all rounds combined)

    /**
     * Sum of points scored with the current set of 3 darts (this round only).
     * Resets to 0 when a new round begins (DartsRemaining resets to 3).
     * Replicated so WBP_DartHUD can bind to it on every client.
     */
    UPROPERTY(ReplicatedUsing = OnRep_RoundScore, BlueprintReadOnly, Category = "Darts")
    int32 RoundScore = 0;

    /** Countdown seconds while aiming. Widget-bindable. */
    UPROPERTY(BlueprintReadOnly, Category="Dart")
    int32 AimCountdown = 0;

    // Blueprint-callable getters for widget binding
    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetDartsRemaining() const { return DartsRemaining; }

    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetPlayerScore() const { return PlayerScore; }

    UFUNCTION(BlueprintCallable, Category = "HUD")
    int32 GetRoundScore() const { return RoundScore; }

    // Server RPC to consume one dart (server-authoritative)
    UFUNCTION(Server, Reliable)
    void Server_ConsumeDart();

    /**
     * Called by DartGameMode (server) to add points to both RoundScore and
     * PlayerScore, and to reset RoundScore when a new round begins.
     * Must be called on the server — replication propagates to clients.
     */
    void Server_AddRoundScore(int32 Points);

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

    // Called when replicated DartsRemaining updates on clients
    UFUNCTION()
    void OnRep_DartsRemaining();

    // Called when replicated PlayerScore updates on clients
    UFUNCTION()
    void OnRep_PlayerScore();

    // Called when replicated RoundScore updates on clients
    UFUNCTION()
    void OnRep_RoundScore();

private:
    // ── Components ──────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
    UPROPERTY(VisibleAnywhere) UCameraComponent*    Camera;

    // ── Movement ────────────────────────────────────────────────────────────
    void MoveForward(float Value);
    void MoveRight  (float Value);
    void Turn       (float Value);
    void LookUp     (float Value);

    // ── Throwing ────────────────────────────────────────────────────────────
    void StartAim();
    void Throw();
    bool bIsAiming = false;

    // ── Aim-forfeit timer ────────────────────────────────────────────────────
    FTimerHandle AimTimerHandle;
    void AimTick();
    void ClearAimTimer();

    // ── RPCs ─────────────────────────────────────────────────────────────────

    /**
     * Sends the raw camera direction + the widget accuracy value to the server.
     * The SERVER is authoritative: it applies the random angular deviation so
     * every client sees the same deflected trajectory through replication.
     */
    UFUNCTION(Server, Reliable)
    void Server_Throw(FVector Origin, FVector Direction, float Speed, float Accuracy);

    /**
     * Server calls this back on the OWNING CLIENT to print accuracy diagnostics.
     * Running the print on the client ensures it appears on the correct player's
     * screen even in a listen-server setup where Server_ runs on the host.
     */
    UFUNCTION(Client, Reliable)
    void Client_PrintThrowDiagnostics(float Accuracy, float Inaccuracy, float DeviationDeg);

public:
    // Hookable events for Blueprint (Widget) to update visuals when values change
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnDartsRemainingUpdated(int32 NewRemaining);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnPlayerScoreUpdated(int32 NewScore);

    /** Fires on every client (via OnRep) whenever RoundScore changes.
     *  Bind this in WBP_DartHUD to refresh the round-score text block. */
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnRoundScoreUpdated(int32 NewRoundScore);
};