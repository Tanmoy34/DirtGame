#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DartGameState.generated.h"



USTRUCT(BlueprintType)
struct FPlayerInfo
{
	GENERATED_BODY()

	
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	FString PlayerName;


	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 TotalScore = 0;

	
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 RoundScore = 0;

	
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	bool bIsActiveTurn = false;

	
	UPROPERTY(BlueprintReadOnly, Category = "Player")
	int32 DartsRemaining = 3;
};


USTRUCT(BlueprintType)
struct FPlayerTurnInfo
{
	GENERATED_BODY()

	
	TWeakObjectPtr<APlayerController> Controller;

	
	FString PlayerName;

	
	int32 TotalScore = 0;


	int32 RoundScore = 0;
};


//  ADartGameState


UCLASS()
class DARTGAME_API ADartGameState : public AGameState
{
	GENERATED_BODY()

public:
	
	 
	UPROPERTY(ReplicatedUsing = OnRep_Players, BlueprintReadOnly, Category = "Players")
	TArray<FPlayerInfo> Players;

	

	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Players")
	int32 GetPlayerCount() const { return Players.Num(); }

	/** True if the player at SlotIndex is the one currently allowed to throw. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Players")
	bool IsPlayerTurnActive(int32 SlotIndex) const
	{
		return Players.IsValidIndex(SlotIndex) && Players[SlotIndex].bIsActiveTurn;
	}

	/** Returns the total score for a given slot (-1 if invalid). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Players")
	int32 GetPlayerTotalScore(int32 SlotIndex) const
	{
		return Players.IsValidIndex(SlotIndex) ? Players[SlotIndex].TotalScore : -1;
	}

	/** Returns the round score for a given slot (-1 if invalid). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Players")
	int32 GetPlayerRoundScore(int32 SlotIndex) const
	{
		return Players.IsValidIndex(SlotIndex) ? Players[SlotIndex].RoundScore : -1;
	}

	/** Returns the display name for a given slot (empty string if invalid). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Players")
	FString GetPlayerName(int32 SlotIndex) const
	{
		return Players.IsValidIndex(SlotIndex) ? Players[SlotIndex].PlayerName : FString();
	}

	/** Returns how many darts the player at SlotIndex has left this turn. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Players")
	int32 GetPlayerDartsRemaining(int32 SlotIndex) const
	{
		return Players.IsValidIndex(SlotIndex) ? Players[SlotIndex].DartsRemaining : 0;
	}

	// =========================================================================
	//  GAME OVER — WINNER & LOSER DATA
	//
	//  These three variables are set on the server when the game ends and
	//  replicated to every client automatically.
	//
	//  Just bind WinnerNames → your Winner text box
	//  and  LoserNames  → your Loser text box.
	//
	//  Tie handling:
	//    If two or more players share the highest score, ALL of their names
	//    appear in WinnerNames separated by commas.
	//    Everyone else (lower score) goes into LoserNames.
	// =========================================================================

	/**
	 * Name(s) of the winner(s), set when the game ends.
	 *
	 * Single winner  →  "Player 1"
	 * Tie            →  "Player 1, Player 3"
	 *
	 * Empty until the game ends.
	 * Bind this to your WINNER text box in Blueprint.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_WinnerNames, BlueprintReadOnly, Category = "GameOver")
	FString WinnerNames;

	/**
	 * Name(s) of every player who did NOT win, comma-separated.
	 *
	 * e.g.  "Player 2, Player 4"
	 *
	 * Empty until the game ends (also empty if only one player played).
	 * Bind this to your LOSER text box in Blueprint.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_LoserNames, BlueprintReadOnly, Category = "GameOver")
	FString LoserNames;

	/**
	 * The highest score achieved — useful for a "Player 1 wins with 240 pts!" line.
	 * Set at the same time as WinnerNames / LoserNames.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GameOver")
	int32 WinningScore = 0;

	// =========================================================================
	//  Turn / Round state  (replicated → every client can render the HUD)
	// =========================================================================

	/**
	 * Slot index (0-based) into Players[] of the player who may currently throw.
	 * Widgets can compare against their own slot to show "YOUR TURN" / "WAITING".
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ActivePlayerIndex, BlueprintReadOnly, Category = "Turn")
	int32 ActivePlayerIndex = 0;

	/**
	 * Darts the active player still has left in this turn. Counts 3 → 2 → 1 → 0.
	 * When it hits 0 the GameMode calls AdvanceToNextPlayer().
	 */
	UPROPERTY(ReplicatedUsing = OnRep_DartsRemainingThisTurn, BlueprintReadOnly, Category = "Turn")
	int32 DartsRemainingThisTurn = 3;

	/**
	 * 1-based round counter. Increments once all players have each thrown
	 * or forfeited all 3 darts.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRound, BlueprintReadOnly, Category = "Turn")
	int32 CurrentRound = 1;

	/**
	 * Maximum number of rounds before the game ends automatically.
	 * 0 = no limit.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Turn")
	int32 MaxRounds = 0;

	/**
	 * Flipped to true by the GameMode when all rounds are complete.
	 * Replicated so every client can show a "Game Over" screen.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_bGameOver, BlueprintReadOnly, Category = "Turn")
	bool bGameOver = false;

	// =========================================================================
	//  Legacy flat arrays — kept so existing Blueprints/code still compiles
	// =========================================================================

	/** @deprecated  Use Players[i].TotalScore instead. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scores|Legacy")
	TArray<int32> PlayerScores;

	/** @deprecated  Use Players[i].PlayerName instead. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scores|Legacy")
	TArray<FString> PlayerNames;

	/** @deprecated  Use Players[i].RoundScore instead. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scores|Legacy")
	TArray<int32> PlayerRoundScores;

	/** @deprecated  Use ActivePlayerIndex instead. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPlayerIndex, BlueprintReadOnly, Category = "Scores|Legacy")
	int32 CurrentPlayerIndex = 0;

	/** @deprecated  Use DartsRemainingThisTurn instead. */
	UPROPERTY(ReplicatedUsing = OnRep_ThrowsRemaining, BlueprintReadOnly, Category = "Scores|Legacy")
	int32 ThrowsRemaining = 3;

	// =========================================================================
	//  Server-only helpers  (called exclusively from ADartGameMode)
	// =========================================================================

	/** Register a newly-joined player. Returns the slot index assigned to them. */
	int32 RegisterPlayer(APlayerController *PC);

	/**
	 * Consume one dart for the active player.
	 * Returns true when DartsRemainingThisTurn reaches 0 (turn is over).
	 */
	bool ConsumeDart();

	/**
	 * Move to the next player. Increments CurrentRound when the last player
	 * finishes. Returns true when the game ends (MaxRounds exceeded).
	 */
	bool AdvanceToNextPlayer();

	/** Award Points to the player at SlotIndex. Updates Players[], PlayerScores[], and PlayerRoundScores[]. */
	void AwardPoints(int32 SlotIndex, int32 Points);

	/** Returns the slot index of the given controller, or -1 if not found. */
	int32 GetSlotIndexForController(APlayerController *PC) const;

	/** Returns the PlayerController for the currently active slot, or nullptr. */
	APlayerController *GetActivePlayerController() const;

	/** Server-side ordered player list. NOT replicated (contains raw PC ptrs). */
	TArray<FPlayerTurnInfo> TurnOrder;

	// =========================================================================
	//  Replication
	// =========================================================================

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

	// ── OnRep callbacks ───────────────────────────────────────────────────────

	UFUNCTION()
	void OnRep_Players();
	UFUNCTION()
	void OnRep_ActivePlayerIndex();
	UFUNCTION()
	void OnRep_DartsRemainingThisTurn();
	UFUNCTION()
	void OnRep_CurrentRound();
	UFUNCTION()
	void OnRep_bGameOver();
	UFUNCTION()
	void OnRep_WinnerNames();
	UFUNCTION()
	void OnRep_LoserNames();

	// Legacy
	UFUNCTION()
	void OnRep_CurrentPlayerIndex();
	UFUNCTION()
	void OnRep_ThrowsRemaining();

	// =========================================================================
	//  Blueprint-implementable events
	// =========================================================================

	/**
	 * Fired on ALL clients whenever the Players[] array changes.
	 * Bind this in your scoreboard widget to refresh all player rows.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Players")
	void BP_OnPlayersUpdated();

	/**
	 * Fired on ALL clients when the active player changes or darts-remaining changes.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
	void BP_OnTurnChanged(int32 NewActivePlayerIndex, int32 NewDartsRemaining);

	/** Fired on ALL clients when the round number increments. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Turn")
	void BP_OnRoundChanged(int32 NewRound);

	/**
	 * Fired on ALL clients when the game ends.
	 *
	 * @param Winners   Put this string directly into your WINNER text box.
	 *                  Single winner  →  "Player 1"
	 *                  Tie            →  "Player 1, Player 3"
	 *
	 * @param Losers    Put this string directly into your LOSER text box.
	 *                  e.g.            "Player 2, Player 4"
	 *
	 * @param TopScore  The winning score value  (e.g. 240).
	 *                  Use it to display "Player 1 wins with 240 pts!"
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "GameOver")
	void BP_OnGameOver(const FString &Winners, const FString &Losers, int32 TopScore);
};