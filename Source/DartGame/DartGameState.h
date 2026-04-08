#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DartGameState.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FPlayerTurnInfo
//
//  One entry per registered player, stored in TurnOrder[] on the server.
//  TWeakObjectPtr<APlayerController> is NOT replication-safe, so this struct
//  is server-only.  Clients read the replicated flat arrays (PlayerScores,
//  PlayerNames) to render scoreboards.
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FPlayerTurnInfo
{
	GENERATED_BODY()

	/** The controller that owns this slot (server-side validation only). */
	TWeakObjectPtr<APlayerController> Controller;

	/** Display name shown in the HUD ("Player 1", "Player 2", …). */
	FString PlayerName;

	/** Cumulative score across all completed rounds. */
	int32 TotalScore = 0;

	/** Points scored in the current round (resets when this player's turn ends). */
	int32 RoundScore = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ADartGameState
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class DARTGAME_API ADartGameState : public AGameState
{
	GENERATED_BODY()

public:

	// =========================================================================
	//  Turn / Round state  (replicated → every client can render the HUD)
	// =========================================================================

	/**
	 * Slot index (0-based) into TurnOrder / PlayerScores of the player who may
	 * currently throw.  Replicated — widgets compare against their own slot to
	 * show "YOUR TURN" or "WAITING…" banners.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ActivePlayerIndex, BlueprintReadOnly, Category = "Turn")
	int32 ActivePlayerIndex = 0;

	/**
	 * Darts the active player still has left in this turn.
	 * Counts down  3 → 2 → 1 → 0.  When it hits 0 the GameMode calls
	 * AdvanceToNextPlayer().
	 */
	UPROPERTY(ReplicatedUsing = OnRep_DartsRemainingThisTurn, BlueprintReadOnly, Category = "Turn")
	int32 DartsRemainingThisTurn = 3;

	/**
	 * 1-based round counter.  Increments once all registered players have
	 * each completed a full turn (thrown or forfeited all 3 darts).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRound, BlueprintReadOnly, Category = "Turn")
	int32 CurrentRound = 1;

	/**
	 * Maximum number of rounds before the game ends automatically.
	 * 0 = no limit (play until someone manually stops).
	 * Configurable from the GameMode defaults or a lobby widget.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Turn")
	int32 MaxRounds = 0;

	/**
	 * Flipped to true by the GameMode when MaxRounds are complete.
	 * Replicated so every client can show a "Game Over" screen.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_bGameOver, BlueprintReadOnly, Category = "Turn")
	bool bGameOver = false;

	// =========================================================================
	//  Per-player score data  (replicated flat arrays, indexed by slot)
	// =========================================================================

	/**
	 * Cumulative total score per player slot (index matches TurnOrder on server).
	 * Replicated for scoreboard display.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scores")
	TArray<int32> PlayerScores;

	/**
	 * Display names per player slot ("Player 1", "Player 2", …).
	 * Replicated for scoreboard display.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scores")
	TArray<FString> PlayerNames;

	/**
	 * Round score (current turn only) per player slot.
	 * Replicated so spectators / other players can watch a running total.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scores")
	TArray<int32> PlayerRoundScores;

	// =========================================================================
	//  Legacy fields — kept so old Dartboard / GameMode code compiles as-is
	// =========================================================================

	/** @deprecated  Use ActivePlayerIndex. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPlayerIndex, BlueprintReadOnly, Category = "Scores")
	int32 CurrentPlayerIndex = 0;

	/** @deprecated  Use DartsRemainingThisTurn. */
	UPROPERTY(ReplicatedUsing = OnRep_ThrowsRemaining, BlueprintReadOnly, Category = "Scores")
	int32 ThrowsRemaining = 3;

	// =========================================================================
	//  Server-only helpers  (called exclusively from ADartGameMode)
	// =========================================================================

	/**
	 * Register a newly-joined player in TurnOrder.
	 * Called by ADartGameMode::PostLogin on the server.
	 * Returns the slot index assigned to this controller.
	 */
	int32 RegisterPlayer(APlayerController* PC);

	/**
	 * Consume one dart for the active player.
	 * Returns true when DartsRemainingThisTurn reaches 0 (turn is over).
	 */
	bool ConsumeDart();

	/**
	 * Move to the next player.  Increments CurrentRound when the last player in
	 * TurnOrder finishes their turn.
	 * Returns true when the game ends (MaxRounds exceeded).
	 */
	bool AdvanceToNextPlayer();

	/**
	 * Award Points to the player at SlotIndex.
	 * Updates both PlayerScores and PlayerRoundScores.
	 */
	void AwardPoints(int32 SlotIndex, int32 Points);

	/** Returns the slot index of the given controller, or -1 if not found. */
	int32 GetSlotIndexForController(APlayerController* PC) const;

	/** Returns the PlayerController for the currently active slot, or nullptr. */
	APlayerController* GetActivePlayerController() const;

	/** Server-side ordered player list.  NOT replicated (contains raw PC ptrs). */
	TArray<FPlayerTurnInfo> TurnOrder;

	// =========================================================================
	//  Replication
	// =========================================================================

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── OnRep callbacks ───────────────────────────────────────────────────────

	UFUNCTION() void OnRep_ActivePlayerIndex();
	UFUNCTION() void OnRep_DartsRemainingThisTurn();
	UFUNCTION() void OnRep_CurrentRound();
	UFUNCTION() void OnRep_bGameOver();

	// Legacy
	UFUNCTION() void OnRep_CurrentPlayerIndex();
	UFUNCTION() void OnRep_ThrowsRemaining();

	// =========================================================================
	//  Blueprint-implementable events  (override in BP_DartGameState if needed)
	// =========================================================================

	/**
	 * Fired on ALL clients when the active player changes or darts-remaining
	 * changes.  Use this to update "Player X's turn" banners and dart-pip icons.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
	void BP_OnTurnChanged(int32 NewActivePlayerIndex, int32 NewDartsRemaining);

	/**
	 * Fired on ALL clients when the round number increments.
	 * Use this to flash a "Round X" title card.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
	void BP_OnRoundChanged(int32 NewRound);

	/** Fired on ALL clients when the game ends. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
	void BP_OnGameOver(int32 WinnerSlotIndex, int32 WinnerScore);
};