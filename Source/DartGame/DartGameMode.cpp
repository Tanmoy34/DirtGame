#include "DartGameMode.h"
#include "DartGameState.h"
#include "DartCharacter.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

ADartGameMode::ADartGameMode()
{
	GameStateClass = ADartGameState::StaticClass();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PostLogin  — register the player and hand them their turn if they're first
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS || !NewPlayer) return;

	const int32 SlotIndex = GS->RegisterPlayer(NewPlayer);

	UE_LOG(LogTemp, Log,
		TEXT("[GameMode] PostLogin — Player %d joined (slot %d)"),
		SlotIndex + 1, SlotIndex);

	// If this is the first player, start the turn timer for them now.
	if (SlotIndex == 0)
	{
		NotifyPlayerTurnStart(0);
		RestartTurnTimer();
	}
	else
	{
		// New player must wait — disable their throw immediately.
		NotifyPlayerTurnEnd(SlotIndex);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Logout  — nothing structural to undo for now; could skip that player's turn
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	APlayerController* PC = Cast<APlayerController>(Exiting);
	if (!PC) return;

	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	const int32 SlotIndex = GS->GetSlotIndexForController(PC);

	UE_LOG(LogTemp, Warning,
		TEXT("[GameMode] Player %d (slot %d) disconnected."),
		SlotIndex + 1, SlotIndex);

	// If the disconnecting player was the active one, forfeit their remaining
	// darts so the game does not stall.
	if (SlotIndex == GS->ActivePlayerIndex)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GameMode] Active player disconnected — forfeiting remaining darts."));

		// Consume all remaining darts for this slot without scoring.
		while (!GS->ConsumeDart()) { /* drain */ }
		NotifyTurnAdvance(GS);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  RegisterThrow  (primary scoring entry point)
//
//  Called from Dartboard::RegisterHit (which already runs on the server).
//  1. Validates the throwing player is the active player.
//  2. Awards the points in GameState.
//  3. Updates the character's personal score variables.
//  4. Consumes one dart.
//  5. Advances the turn if all darts are spent.
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::RegisterThrow(APlayerController* OwnerPC, int32 Points)
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	// ── 1. Identify the slot ─────────────────────────────────────────────────
	const int32 SlotIndex = GS->GetSlotIndexForController(OwnerPC);
	if (SlotIndex < 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GameMode] RegisterThrow — unknown PC, ignoring."));
		return;
	}

	// ── 2. Turn-order validation  (only the active player may score) ──────────
	if (SlotIndex != GS->ActivePlayerIndex)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GameMode] RegisterThrow — Player %d threw but it is Player %d's turn. Ignoring."),
			SlotIndex + 1, GS->ActivePlayerIndex + 1);
		return;
	}

	// ── 3. Stop the per-throw timer ──────────────────────────────────────────
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);

	// ── 4. Award points in GameState (updates replicated arrays) ────────────
	GS->AwardPoints(SlotIndex, Points);

	// ── 5. Mirror score onto the character for HUD binding ───────────────────
	if (ADartCharacter* Char = GetCharacterForPC(OwnerPC))
	{
		Char->Server_AddRoundScore(Points);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[GameMode] RegisterThrow | Player %d | +%d pts | DartsLeft=%d"),
		SlotIndex + 1, Points, GS->DartsRemainingThisTurn);

	// ── 6. Consume one dart and check for end-of-turn ────────────────────────
	const bool bTurnOver = GS->ConsumeDart();

	if (bTurnOver)
	{
		NotifyTurnAdvance(GS);
	}
	else
	{
		// Still darts left — restart the per-throw timer.
		RestartTurnTimer();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  AddScore  (legacy compatibility shim)
//
//  Old code in Dartboard.cpp calls  GM->AddScore(0, Points).
//  We resolve PlayerIndex 0 to the actual active player's PC and forward.
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::AddScore(int32 PlayerIndex, int32 Points)
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	// Use the active player rather than blindly trusting the index,
	// because the new system resolves the thrower via dart ownership anyway.
	APlayerController* ActivePC = GS->GetActivePlayerController();
	if (ActivePC)
	{
		RegisterThrow(ActivePC, Points);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GameMode] AddScore — no active PC found for index %d."), PlayerIndex);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  ForfeitDart  — called when a player times out without throwing
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::ForfeitDart(APlayerController* OwnerPC)
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	const int32 SlotIndex = GS->GetSlotIndexForController(OwnerPC);
	if (SlotIndex < 0 || SlotIndex != GS->ActivePlayerIndex) return;

	UE_LOG(LogTemp, Warning,
		TEXT("[GameMode] ForfeitDart — Player %d forfeits a dart (timeout)."), SlotIndex + 1);

	GetWorldTimerManager().ClearTimer(TurnTimerHandle);

	const bool bTurnOver = GS->ConsumeDart();
	if (bTurnOver)
	{
		NotifyTurnAdvance(GS);
	}
	else
	{
		RestartTurnTimer();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  NotifyTurnAdvance  (internal)
//
//  Called after the active player's last dart is consumed (hit or forfeit).
//  Locks the old player's input, advances GameState, unlocks the new player.
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::NotifyTurnAdvance(ADartGameState* GS)
{
	if (!GS) return;

	const int32 OldSlot = GS->ActivePlayerIndex;

	// Lock input for the player who just finished their turn.
	NotifyPlayerTurnEnd(OldSlot);

	// Advance state (may set bGameOver).
	const bool bGameOver = GS->AdvanceToNextPlayer();

	if (bGameOver)
	{
		GetWorldTimerManager().ClearTimer(TurnTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("[GameMode] Game over — no more turns."));
		return;
	}

	// Unlock input for the newly active player.
	const int32 NewSlot = GS->ActivePlayerIndex;
	NotifyPlayerTurnStart(NewSlot);

	// Start the throw timer for the new player.
	RestartTurnTimer();

	UE_LOG(LogTemp, Log,
		TEXT("[GameMode] Turn advanced: Player %d → Player %d | Round %d"),
		OldSlot + 1, NewSlot + 1, GS->CurrentRound);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NotifyPlayerTurnStart  (internal)
//
//  Tells the DartCharacter owned by slot SlotIndex that it's their turn.
//  Sets bIsMyTurn = true, resets DartsRemaining to 3, fires BP event.
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::NotifyPlayerTurnStart(int32 SlotIndex)
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	APlayerController* PC = GS->GetActivePlayerController();
	// If SlotIndex is not yet the active one (e.g. called at registration),
	// find the PC directly from TurnOrder.
	if (!PC && GS->TurnOrder.IsValidIndex(SlotIndex))
	{
		PC = GS->TurnOrder[SlotIndex].Controller.Get();
	}

	if (ADartCharacter* Char = GetCharacterForPC(PC))
	{
		Char->bIsMyTurn      = true;
		Char->DartsRemaining = 3;

		// Manually call BP event on server (listen-server host)
		Char->BP_OnTurnStarted(SlotIndex);

		UE_LOG(LogTemp, Log,
			TEXT("[GameMode] NotifyPlayerTurnStart — Player %d enabled."), SlotIndex + 1);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  NotifyPlayerTurnEnd  (internal)
//
//  Disables throw input for the player at SlotIndex.
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::NotifyPlayerTurnEnd(int32 SlotIndex)
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS || !GS->TurnOrder.IsValidIndex(SlotIndex)) return;

	APlayerController* PC = GS->TurnOrder[SlotIndex].Controller.Get();

	if (ADartCharacter* Char = GetCharacterForPC(PC))
	{
		Char->bIsMyTurn = false;

		// Manually call BP event on server (listen-server host)
		Char->BP_OnTurnEnded(SlotIndex);

		UE_LOG(LogTemp, Log,
			TEXT("[GameMode] NotifyPlayerTurnEnd — Player %d disabled."), SlotIndex + 1);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Turn timer
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameMode::RestartTurnTimer()
{
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().SetTimer(
		TurnTimerHandle,
		this,
		&ADartGameMode::OnTurnTimeout,
		TurnTimeoutSeconds,
		false   // not looping — restarted after each dart
	);
}

void ADartGameMode::OnTurnTimeout()
{
	ADartGameState* GS = GetGameState<ADartGameState>();
	if (!GS) return;

	APlayerController* ActivePC = GS->GetActivePlayerController();
	if (ActivePC)
	{
		ForfeitDart(ActivePC);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────────────────────────────────────

ADartCharacter* ADartGameMode::GetCharacterForPC(APlayerController* PC) const
{
	if (!PC) return nullptr;
	return Cast<ADartCharacter>(PC->GetPawn());
}