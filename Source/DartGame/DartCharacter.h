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

    /**
     * Set by the GameMode on the server when it's this player's turn.
     * Replicated to the owning client so the HUD and input can respond.
     *   true  → player may aim and throw
     *   false → player must wait (another player's turn)
     */
    UPROPERTY(ReplicatedUsing = OnRep_bIsMyTurn, BlueprintReadOnly, Category = "Darts")
    bool bIsMyTurn = false;

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
    void Server_ConsumeDart(bool bForfeit);

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

    // Called when bIsMyTurn replicates to clients
    UFUNCTION()
    void OnRep_bIsMyTurn();

    // Called when replicated PlayerScore updates on clients
    UFUNCTION()
    void OnRep_PlayerScore();

    // Called when replicated RoundScore updates on clients
    UFUNCTION()
    void OnRep_RoundScore();

    // ---- New: player identity & leaderboard helpers -----------------------
    /** Player display name (replicated) — use this in UMG leaderboard rows. */
    UPROPERTY(ReplicatedUsing=OnRep_PlayerDisplayName, BlueprintReadOnly, Category="Player")
    FString PlayerDisplayName;

    /** Slot/index in turn order (0-based). Set by GameMode on the server. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Player")
    int32 PlayerSlot = -1;

    /** Called when PlayerDisplayName replicates to clients. */
    UFUNCTION()
    void OnRep_PlayerDisplayName();

    /** Notify Blueprints when the player's display name becomes available (useful for UMG binding). */
    UFUNCTION(BlueprintImplementableEvent, Category="Player")
    void BP_OnPlayerNameUpdated(const FString& NewName);

    // Simple Blueprint-callable helpers for UMG / widgets
    UFUNCTION(BlueprintCallable, Category="HUD")
    FString GetPlayerName() const { return PlayerDisplayName; }

    UFUNCTION(BlueprintCallable, Category="HUD")
    int32 GetPlayerSlot() const { return PlayerSlot; }

    UFUNCTION(BlueprintCallable, Category="HUD")
    bool IsActiveTurn() const { return bIsMyTurn; }

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

    /**
     * Fired on this character's owning client when the GameMode enables their
     * turn.  Use this to show "YOUR TURN!" UI, enable aiming visuals, etc.
     * @param SlotIndex  This player's slot in the turn order (0-based).
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
    void BP_OnTurnStarted(int32 SlotIndex);

    /**
     * Fired on this character's owning client when the GameMode ends their
     * turn.  Use this to hide the aim indicator, show "Waiting…" etc.
     * @param SlotIndex  This player's slot in the turn order (0-based).
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
    void BP_OnTurnEnded(int32 SlotIndex);

    // ---- New: Last-scored points (replicated per-player) --------------------
    // LastScoredPoints: the points scored by the player's most recent hit.
    UPROPERTY(ReplicatedUsing = OnRep_LastScoredPoints, BlueprintReadOnly, Category = "Darts")
    int32 LastScoredPoints = 0;

    // Called when LastScoredPoints replicates to clients
    UFUNCTION()
    void OnRep_LastScoredPoints();

    // Notify Blueprints when last scored points change
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_OnLastScoredUpdated(int32 NewLastPoints);

    // Blueprint-callable helper so Blueprints/Board can credit this player.
    // If called on client it will route to the server via Server_RequestAddPoints.
    UFUNCTION(BlueprintCallable, Category = "Dart")
    void AddPoints(int32 Points);

    // RPC: client -> server request to add points (if AddPoints was called on client)
    UFUNCTION(Server, Reliable)
    void Server_RequestAddPoints(int32 Points);
};

