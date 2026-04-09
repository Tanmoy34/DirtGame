#include "DartGameMode.h"
#include "DartGameState.h"
#include "DartCharacter.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"



ADartGameMode::ADartGameMode()
{
	GameStateClass = ADartGameState::StaticClass();
}


//  PostLogin  — 


void ADartGameMode::PostLogin(APlayerController *NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS || !NewPlayer)
		return;

	const int32 SlotIndex = GS->RegisterPlayer(NewPlayer);

	UE_LOG(LogTemp, Log,
		   TEXT("[GameMode] PostLogin — Player %d joined (slot %d)"),
		   SlotIndex + 1, SlotIndex);

	
	if (SlotIndex == 0)
	{
		NotifyPlayerTurnStart(0);
		RestartTurnTimer();
	}
	else
	{
		
		NotifyPlayerTurnEnd(SlotIndex);
	}
}


//  Logout


void ADartGameMode::Logout(AController *Exiting)
{
	Super::Logout(Exiting);

	APlayerController *PC = Cast<APlayerController>(Exiting);
	if (!PC)
		return;

	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS)
		return;

	const int32 SlotIndex = GS->GetSlotIndexForController(PC);

	UE_LOG(LogTemp, Warning,
		   TEXT("[GameMode] Player %d (slot %d) disconnected."),
		   SlotIndex + 1, SlotIndex);

	
	if (SlotIndex == GS->ActivePlayerIndex)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[GameMode] Active player disconnected — forfeiting remaining darts."));

		while (!GS->ConsumeDart())
		{ 
		}
		NotifyTurnAdvance(GS);
	}
}



void ADartGameMode::RegisterThrow(APlayerController *OwnerPC, int32 Points)
{
	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS)
		return;

	
	const int32 SlotIndex = GS->GetSlotIndexForController(OwnerPC);
	if (SlotIndex < 0)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[GameMode] RegisterThrow — unknown PC, ignoring."));
		return;
	}

	if (SlotIndex != GS->ActivePlayerIndex)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[GameMode] RegisterThrow — Player %d threw but it is Player %d's turn. Ignoring."),
			   SlotIndex + 1, GS->ActivePlayerIndex + 1);
		return;
	}

	
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);


	GS->AwardPoints(SlotIndex, Points);


	if (ADartCharacter *Char = GetCharacterForPC(OwnerPC))
	{
		Char->Server_AddRoundScore(Points);
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[GameMode] RegisterThrow | Player %d | +%d pts | DartsLeft=%d"),
		   SlotIndex + 1, Points, GS->DartsRemainingThisTurn);

	
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



void ADartGameMode::AddScore(int32 PlayerIndex, int32 Points)
{
	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS)
		return;

	APlayerController *ActivePC = GS->GetActivePlayerController();
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



void ADartGameMode::ForfeitDart(APlayerController *OwnerPC)
{
	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS)
		return;

	const int32 SlotIndex = GS->GetSlotIndexForController(OwnerPC);
	if (SlotIndex < 0 || SlotIndex != GS->ActivePlayerIndex)
		return;

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



void ADartGameMode::NotifyTurnAdvance(ADartGameState *GS)
{
	if (!GS)
		return;

	const int32 OldSlot = GS->ActivePlayerIndex;

	NotifyPlayerTurnEnd(OldSlot);

	const bool bGameOver = GS->AdvanceToNextPlayer();

	if (bGameOver)
	{
		GetWorldTimerManager().ClearTimer(TurnTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("[GameMode] Game over — no more turns."));
		return;
	}

	const int32 NewSlot = GS->ActivePlayerIndex;
	NotifyPlayerTurnStart(NewSlot);

	RestartTurnTimer();

	UE_LOG(LogTemp, Log,
		   TEXT("[GameMode] Turn advanced: Player %d → Player %d | Round %d"),
		   OldSlot + 1, NewSlot + 1, GS->CurrentRound);
}



void ADartGameMode::NotifyPlayerTurnStart(int32 SlotIndex)
{
	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS)
		return;

	APlayerController *PC = GS->GetActivePlayerController();
	if (!PC && GS->TurnOrder.IsValidIndex(SlotIndex))
	{
		PC = GS->TurnOrder[SlotIndex].Controller.Get();
	}

	
	if (ADartCharacter *Char = GetCharacterForPC(PC))
	{
		Char->bIsMyTurn = true;
		Char->DartsRemaining = 3;

		
		Char->BP_OnTurnStarted(SlotIndex);

		UE_LOG(LogTemp, Log,
			   TEXT("[GameMode] NotifyPlayerTurnStart — Player %d enabled."), SlotIndex + 1);
	}

	
	if (GS->Players.IsValidIndex(SlotIndex))
	{
		GS->Players[SlotIndex].bIsActiveTurn = true;
		GS->Players[SlotIndex].DartsRemaining = 3;
	}
}



void ADartGameMode::NotifyPlayerTurnEnd(int32 SlotIndex)
{
	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS || !GS->TurnOrder.IsValidIndex(SlotIndex))
		return;

	APlayerController *PC = GS->TurnOrder[SlotIndex].Controller.Get();

	
	if (ADartCharacter *Char = GetCharacterForPC(PC))
	{
		Char->bIsMyTurn = false;

		Char->BP_OnTurnEnded(SlotIndex);

		UE_LOG(LogTemp, Log,
			   TEXT("[GameMode] NotifyPlayerTurnEnd — Player %d disabled."), SlotIndex + 1);
	}

	
	if (GS->Players.IsValidIndex(SlotIndex))
	{
		GS->Players[SlotIndex].bIsActiveTurn = false;
	}
}


//  Turn timer


void ADartGameMode::RestartTurnTimer()
{
	GetWorldTimerManager().ClearTimer(TurnTimerHandle);
	GetWorldTimerManager().SetTimer(
		TurnTimerHandle,
		this,
		&ADartGameMode::OnTurnTimeout,
		TurnTimeoutSeconds,
		false);
}

void ADartGameMode::OnTurnTimeout()
{
	ADartGameState *GS = GetGameState<ADartGameState>();
	if (!GS)
		return;

	APlayerController *ActivePC = GS->GetActivePlayerController();
	if (ActivePC)
	{
		ForfeitDart(ActivePC);
	}
}



ADartCharacter *ADartGameMode::GetCharacterForPC(APlayerController *PC) const
{
	if (!PC)
		return nullptr;
	return Cast<ADartCharacter>(PC->GetPawn());
}