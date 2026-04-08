#include "DartGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Replication
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Turn state
	DOREPLIFETIME(ADartGameState, ActivePlayerIndex);
	DOREPLIFETIME(ADartGameState, DartsRemainingThisTurn);
	DOREPLIFETIME(ADartGameState, CurrentRound);
	DOREPLIFETIME(ADartGameState, MaxRounds);
	DOREPLIFETIME(ADartGameState, bGameOver);

	// Score arrays
	DOREPLIFETIME(ADartGameState, PlayerScores);
	DOREPLIFETIME(ADartGameState, PlayerNames);
	DOREPLIFETIME(ADartGameState, PlayerRoundScores);

	// Legacy
	DOREPLIFETIME(ADartGameState, CurrentPlayerIndex);
	DOREPLIFETIME(ADartGameState, ThrowsRemaining);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RegisterPlayer  (server only)
//
//  Adds the controller to TurnOrder and expands the replicated score arrays.
//  Returns the new slot index.
// ─────────────────────────────────────────────────────────────────────────────

int32 ADartGameState::RegisterPlayer(APlayerController* PC)
{
	if (!PC) return -1;

	// Avoid double-registration
	for (int32 i = 0; i < TurnOrder.Num(); ++i)
	{
		if (TurnOrder[i].Controller == PC) return i;
	}

	const int32 SlotIndex = TurnOrder.Num();

	FPlayerTurnInfo Info;
	Info.Controller  = PC;
	Info.PlayerName  = FString::Printf(TEXT("Player %d"), SlotIndex + 1);
	Info.TotalScore  = 0;
	Info.RoundScore  = 0;
	TurnOrder.Add(Info);

	// Expand replicated arrays to match
	PlayerScores.Add(0);
	PlayerNames.Add(Info.PlayerName);
	PlayerRoundScores.Add(0);

	UE_LOG(LogTemp, Log,
		TEXT("[GameState] Registered %s at slot %d (total players: %d)"),
		*Info.PlayerName, SlotIndex, TurnOrder.Num());

	return SlotIndex;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ConsumeDart  (server only)
//
//  Decrements DartsRemainingThisTurn.
//  Returns true when the turn is over (darts hit 0).
// ─────────────────────────────────────────────────────────────────────────────

bool ADartGameState::ConsumeDart()
{
	if (DartsRemainingThisTurn > 0)
	{
		--DartsRemainingThisTurn;

		// Keep legacy field in sync
		ThrowsRemaining = DartsRemainingThisTurn;

		UE_LOG(LogTemp, Log,
			TEXT("[GameState] Dart consumed — %d remaining for Player %d"),
			DartsRemainingThisTurn, ActivePlayerIndex + 1);
	}

	return (DartsRemainingThisTurn <= 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AdvanceToNextPlayer  (server only)
//
//  Flow:
//   1. Reset the current player's RoundScore for their NEXT visit.
//   2. Increment ActivePlayerIndex.
//   3. If we've wrapped all the way around → increment CurrentRound.
//   4. If CurrentRound > MaxRounds (and MaxRounds > 0) → set bGameOver.
//   5. Reset DartsRemainingThisTurn = 3 for the next player.
//
//  Returns true if the game just ended.
// ─────────────────────────────────────────────────────────────────────────────

bool ADartGameState::AdvanceToNextPlayer()
{
	const int32 NumPlayers = TurnOrder.Num();
	if (NumPlayers == 0) return false;

	// ── Reset round score for the player who just finished ───────────────────
	if (PlayerRoundScores.IsValidIndex(ActivePlayerIndex))
	{
		PlayerRoundScores[ActivePlayerIndex] = 0;
		if (TurnOrder.IsValidIndex(ActivePlayerIndex))
		{
			TurnOrder[ActivePlayerIndex].RoundScore = 0;
		}
	}

	// ── Advance index ────────────────────────────────────────────────────────
	const int32 PreviousIndex = ActivePlayerIndex;
	ActivePlayerIndex = (ActivePlayerIndex + 1) % NumPlayers;

	// Keep legacy field in sync
	CurrentPlayerIndex = ActivePlayerIndex;

	// ── Did we just wrap around? (last player → player 0) ───────────────────
	if (ActivePlayerIndex == 0)
	{
		++CurrentRound;

		UE_LOG(LogTemp, Log, TEXT("[GameState] All players done — starting Round %d"), CurrentRound);

		// Fire BP event for "Round X" title card
		BP_OnRoundChanged(CurrentRound);

		// ── Check for game-over ───────────────────────────────────────────────
		if (MaxRounds > 0 && CurrentRound > MaxRounds)
		{
			bGameOver = true;

			// Find winner (highest TotalScore)
			int32 WinnerIndex = 0;
			int32 WinnerScore = PlayerScores.IsValidIndex(0) ? PlayerScores[0] : 0;
			for (int32 i = 1; i < PlayerScores.Num(); ++i)
			{
				if (PlayerScores[i] > WinnerScore)
				{
					WinnerScore = PlayerScores[i];
					WinnerIndex = i;
				}
			}

			UE_LOG(LogTemp, Log,
				TEXT("[GameState] Game over — Winner: Player %d with %d pts"),
				WinnerIndex + 1, WinnerScore);

			BP_OnGameOver(WinnerIndex, WinnerScore);
			return true;
		}
	}

	// ── Reset darts for new active player ────────────────────────────────────
	DartsRemainingThisTurn = 3;
	ThrowsRemaining        = 3;   // legacy

	UE_LOG(LogTemp, Log,
		TEXT("[GameState] Turn → Player %d | Round %d"),
		ActivePlayerIndex + 1, CurrentRound);

	// Notify all clients
	BP_OnTurnChanged(ActivePlayerIndex, DartsRemainingThisTurn);

	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AwardPoints  (server only)
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameState::AwardPoints(int32 SlotIndex, int32 Points)
{
	if (!TurnOrder.IsValidIndex(SlotIndex)) return;

	// Update server-side struct
	TurnOrder[SlotIndex].TotalScore += Points;
	TurnOrder[SlotIndex].RoundScore += Points;

	// Update replicated arrays (clients see these)
	if (PlayerScores.IsValidIndex(SlotIndex))
	{
		PlayerScores[SlotIndex] += Points;
	}
	if (PlayerRoundScores.IsValidIndex(SlotIndex))
	{
		PlayerRoundScores[SlotIndex] += Points;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[GameState] AwardPoints | Player %d | +%d pts | Total=%d | RoundTotal=%d"),
		SlotIndex + 1, Points,
		PlayerScores.IsValidIndex(SlotIndex) ? PlayerScores[SlotIndex] : -1,
		PlayerRoundScores.IsValidIndex(SlotIndex) ? PlayerRoundScores[SlotIndex] : -1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Utility helpers
// ─────────────────────────────────────────────────────────────────────────────

int32 ADartGameState::GetSlotIndexForController(APlayerController* PC) const
{
	for (int32 i = 0; i < TurnOrder.Num(); ++i)
	{
		if (TurnOrder[i].Controller == PC) return i;
	}
	return -1;
}

APlayerController* ADartGameState::GetActivePlayerController() const
{
	if (TurnOrder.IsValidIndex(ActivePlayerIndex))
	{
		return TurnOrder[ActivePlayerIndex].Controller.Get();
	}
	return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  OnRep callbacks  — forward to BP events so widgets update automatically
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameState::OnRep_ActivePlayerIndex()
{
	BP_OnTurnChanged(ActivePlayerIndex, DartsRemainingThisTurn);

	UE_LOG(LogTemp, Log,
		TEXT("[GameState][Client] Active player → %d"), ActivePlayerIndex + 1);
}

void ADartGameState::OnRep_DartsRemainingThisTurn()
{
	BP_OnTurnChanged(ActivePlayerIndex, DartsRemainingThisTurn);
}

void ADartGameState::OnRep_CurrentRound()
{
	BP_OnRoundChanged(CurrentRound);

	UE_LOG(LogTemp, Log, TEXT("[GameState][Client] Round → %d"), CurrentRound);
}

void ADartGameState::OnRep_bGameOver()
{
	if (bGameOver)
	{
		// Winner info is not cached here separately; the HUD can read
		// PlayerScores[] to determine the winner locally.
		// Pass -1 / -1 as sentinels; BP can search PlayerScores itself.
		BP_OnGameOver(-1, -1);

		UE_LOG(LogTemp, Log, TEXT("[GameState][Client] Game over received."));
	}
}

// Legacy OnRep stubs — kept so old bindings don't break
void ADartGameState::OnRep_CurrentPlayerIndex() {}
void ADartGameState::OnRep_ThrowsRemaining()    {}