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

	// ── Player registration ───────────────────────────────────────────────────

	/** Called automatically by the engine when a player joins. */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Called when a player leaves (clean up TurnOrder slot if needed). */
	virtual void Logout(AController* Exiting) override;

	// ── Turn / scoring API  (called from Dartboard and DartCharacter) ─────────

	/**
	 * Award Points to the player whose dart just hit.
	 * Finds the correct slot via the projectile's owner PC, updates GameState,
	 * updates the character's RoundScore/PlayerScore, then calls NotifyTurnAdvance.
	 *
	 * @param OwnerPC    PlayerController that owns the dart that just hit.
	 * @param Points     Score to award (0 = miss, still consumes a dart).
	 */
	void RegisterThrow(APlayerController* OwnerPC, int32 Points);

	/**
	 * Legacy entry point kept for backward compatibility with Dartboard.cpp
	 * which still calls  GM->AddScore(0, Points).
	 * Routes into RegisterThrow using the PlayerIndex as a slot offset.
	 */
	void AddScore(int32 PlayerIndex, int32 Points);

	/**
	 * Called by GameMode itself (timeout) or externally to forfeit one dart
	 * for the active player without awarding points.
	 */
	void ForfeitDart(APlayerController* OwnerPC);

private:

	// ── Turn timer ────────────────────────────────────────────────────────────

	/** Seconds allowed per throw while the player is not actively aiming. */
	UPROPERTY(EditDefaultsOnly, Category = "Turn", meta=(ClampMin="5"))
	float TurnTimeoutSeconds = 30.f;

	FTimerHandle TurnTimerHandle;

	/** Restart the per-throw timer for the currently active player. */
	void RestartTurnTimer();

	/** Fired when the active player's throw timer expires. */
	void OnTurnTimeout();

	// ── Internal helpers ──────────────────────────────────────────────────────

	/**
	 * After a dart is consumed (hit or forfeit), check if the turn is over.
	 * If so, advance to the next player and notify them via Client RPC.
	 * If the game just ended, broadcast game-over.
	 */
	void NotifyTurnAdvance(ADartGameState* GS);

	/**
	 * Send a Client RPC to the player at SlotIndex telling them it's now their
	 * turn and enabling their throw input.
	 */
	void NotifyPlayerTurnStart(int32 SlotIndex);

	/**
	 * Disable throw input on the player at SlotIndex (their turn just ended).
	 */
	void NotifyPlayerTurnEnd(int32 SlotIndex);

	/** Returns the DartCharacter for a given PlayerController (may be nullptr). */
	ADartCharacter* GetCharacterForPC(APlayerController* PC) const;
};