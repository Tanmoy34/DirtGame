#include "DartGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Replication
// ─────────────────────────────────────────────────────────────────────────────

void ADartGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// ── Centralized player data ───────────────────────────────────────────────
	DOREPLIFETIME(ADartGameState, Players);

	// ── Game-over result strings ──────────────────────────────────────────────
	DOREPLIFETIME(ADartGameState, WinnerNames);
	DOREPLIFETIME(ADartGameState, LoserNames);
	DOREPLIFETIME(ADartGameState, WinningScore);

	// ── Turn state ────────────────────────────────────────────────────────────
	DOREPLIFETIME(ADartGameState, ActivePlayerIndex);
	DOREPLIFETIME(ADartGameState, DartsRemainingThisTurn);
	DOREPLIFETIME(ADartGameState, CurrentRound);
	DOREPLIFETIME(ADartGameState, MaxRounds);
	DOREPLIFETIME(ADartGameState, bGameOver);

	// ── Legacy flat arrays ────────────────────────────────────────────────────
	DOREPLIFETIME(ADartGameState, PlayerScores);
	DOREPLIFETIME(ADartGameState, PlayerNames);
	DOREPLIFETIME(ADartGameState, PlayerRoundScores);
	DOREPLIFETIME(ADartGameState, CurrentPlayerIndex);
	DOREPLIFETIME(ADartGameState, ThrowsRemaining);
}


//  RegisterPlayer  


int32 ADartGameState::RegisterPlayer(APlayerController *PC)
{
	if (!PC)
		return -1;

	// Avoid double-registration
	for (int32 i = 0; i < TurnOrder.Num(); ++i)
	{
		if (TurnOrder[i].Controller == PC)
			return i;
	}

	const int32 SlotIndex = TurnOrder.Num();
	const FString Name = FString::Printf(TEXT("Player %d"), SlotIndex + 1);

	
	FPlayerTurnInfo Info;
	Info.Controller = PC;
	Info.PlayerName = Name;
	Info.TotalScore = 0;
	Info.RoundScore = 0;
	TurnOrder.Add(Info);

	
	PlayerScores.Add(0);
	PlayerNames.Add(Name);
	PlayerRoundScores.Add(0);

	
	FPlayerInfo NewPlayer;
	NewPlayer.PlayerName = Name;
	NewPlayer.TotalScore = 0;
	NewPlayer.RoundScore = 0;
	NewPlayer.bIsActiveTurn = false;
	NewPlayer.DartsRemaining = 3;
	Players.Add(NewPlayer);

	UE_LOG(LogTemp, Log,
		   TEXT("[GameState] RegisterPlayer — %s registered at slot %d (total: %d)"),
		   *Name, SlotIndex, TurnOrder.Num());

	return SlotIndex;
}


//  ConsumeDart  


bool ADartGameState::ConsumeDart()
{
	if (DartsRemainingThisTurn > 0)
	{
		--DartsRemainingThisTurn;
		ThrowsRemaining = DartsRemainingThisTurn; // legacy sync

		if (Players.IsValidIndex(ActivePlayerIndex))
		{
			Players[ActivePlayerIndex].DartsRemaining = DartsRemainingThisTurn;
		}

		UE_LOG(LogTemp, Log,
			   TEXT("[GameState] ConsumeDart — Player %d has %d dart(s) left"),
			   ActivePlayerIndex + 1, DartsRemainingThisTurn);
	}

	return (DartsRemainingThisTurn <= 0);
}



bool ADartGameState::AdvanceToNextPlayer()
{
	const int32 NumPlayers = TurnOrder.Num();
	if (NumPlayers == 0)
		return false;

	
	if (Players.IsValidIndex(ActivePlayerIndex))
	{
		Players[ActivePlayerIndex].bIsActiveTurn = false;
		Players[ActivePlayerIndex].RoundScore = 0;
		Players[ActivePlayerIndex].DartsRemaining = 0;
	}
	if (PlayerRoundScores.IsValidIndex(ActivePlayerIndex))
		PlayerRoundScores[ActivePlayerIndex] = 0;
	if (TurnOrder.IsValidIndex(ActivePlayerIndex))
		TurnOrder[ActivePlayerIndex].RoundScore = 0;

	
	ActivePlayerIndex = (ActivePlayerIndex + 1) % NumPlayers;
	CurrentPlayerIndex = ActivePlayerIndex;

	
	if (ActivePlayerIndex == 0)
	{
		++CurrentRound;

		UE_LOG(LogTemp, Log, TEXT("[GameState] All players done — starting Round %d"), CurrentRound);

		BP_OnRoundChanged(CurrentRound);

		
		if (MaxRounds > 0 && CurrentRound > MaxRounds)
		{
			bGameOver = true;

			
			int32 TopScore = 0;
			for (const FPlayerInfo &P : Players)
			{
				if (P.TotalScore > TopScore)
				{
					TopScore = P.TotalScore;
				}
			}

			
			FString WinnersStr;
			FString LosersStr;

			for (const FPlayerInfo &P : Players)
			{
				if (P.TotalScore == TopScore)
				{
					
					if (!WinnersStr.IsEmpty())
					{
						WinnersStr += TEXT(", ");
					}
					WinnersStr += P.PlayerName;
				}
				else
				{
					
					if (!LosersStr.IsEmpty())
					{
						LosersStr += TEXT(", ");
					}
					LosersStr += P.PlayerName;
				}
			}

			
			WinnerNames = WinnersStr;
			LoserNames = LosersStr;
			WinningScore = TopScore;

			UE_LOG(LogTemp, Log,
				   TEXT("[GameState] Game over — Winner(s): %s | Loser(s): %s | Top Score: %d"),
				   *WinnerNames, *LoserNames, WinningScore);

			
			BP_OnGameOver(WinnerNames, LoserNames, WinningScore);

			return true;
		}
	}

	
	DartsRemainingThisTurn = 3;
	ThrowsRemaining = 3;

	if (Players.IsValidIndex(ActivePlayerIndex))
	{
		Players[ActivePlayerIndex].bIsActiveTurn = true;
		Players[ActivePlayerIndex].DartsRemaining = 3;
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[GameState] Turn → Player %d | Round %d"),
		   ActivePlayerIndex + 1, CurrentRound);

	BP_OnTurnChanged(ActivePlayerIndex, DartsRemainingThisTurn);

	return false;
}


//  AwardPoints  (server only)


void ADartGameState::AwardPoints(int32 SlotIndex, int32 Points)
{
	if (!TurnOrder.IsValidIndex(SlotIndex))
		return;

	TurnOrder[SlotIndex].TotalScore += Points;
	TurnOrder[SlotIndex].RoundScore += Points;

	if (PlayerScores.IsValidIndex(SlotIndex))
		PlayerScores[SlotIndex] += Points;

	if (PlayerRoundScores.IsValidIndex(SlotIndex))
		PlayerRoundScores[SlotIndex] += Points;

	if (Players.IsValidIndex(SlotIndex))
	{
		Players[SlotIndex].TotalScore += Points;
		Players[SlotIndex].RoundScore += Points;
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[GameState] AwardPoints | Player %d | +%d pts | Total=%d | Round=%d"),
		   SlotIndex + 1, Points,
		   Players.IsValidIndex(SlotIndex) ? Players[SlotIndex].TotalScore : -1,
		   Players.IsValidIndex(SlotIndex) ? Players[SlotIndex].RoundScore : -1);
}



int32 ADartGameState::GetSlotIndexForController(APlayerController *PC) const
{
	for (int32 i = 0; i < TurnOrder.Num(); ++i)
	{
		if (TurnOrder[i].Controller == PC)
			return i;
	}
	return -1;
}

APlayerController *ADartGameState::GetActivePlayerController() const
{
	if (TurnOrder.IsValidIndex(ActivePlayerIndex))
	{
		return TurnOrder[ActivePlayerIndex].Controller.Get();
	}
	return nullptr;
}



void ADartGameState::OnRep_Players()
{
	BP_OnPlayersUpdated();
	UE_LOG(LogTemp, Log, TEXT("[GameState][Client] Players[] updated — %d entries"), Players.Num());
}

void ADartGameState::OnRep_ActivePlayerIndex()
{
	BP_OnTurnChanged(ActivePlayerIndex, DartsRemainingThisTurn);
	UE_LOG(LogTemp, Log, TEXT("[GameState][Client] Active player → %d"), ActivePlayerIndex + 1);
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
		UE_LOG(LogTemp, Log, TEXT("[GameState][Client] bGameOver received."));
	}
}

void ADartGameState::OnRep_WinnerNames()
{
	
	if (bGameOver)
	{
		BP_OnGameOver(WinnerNames, LoserNames, WinningScore);

		UE_LOG(LogTemp, Log,
			   TEXT("[GameState][Client] BP_OnGameOver fired — Winner(s): %s | Loser(s): %s | Score: %d"),
			   *WinnerNames, *LoserNames, WinningScore);
	}
}

void ADartGameState::OnRep_LoserNames()
{
	
	if (bGameOver && !WinnerNames.IsEmpty())
	{
		BP_OnGameOver(WinnerNames, LoserNames, WinningScore);
	}
}


void ADartGameState::OnRep_CurrentPlayerIndex() {}
void ADartGameState::OnRep_ThrowsRemaining() {}