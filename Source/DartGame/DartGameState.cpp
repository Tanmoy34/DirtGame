#include "DartGameState.h"
#include "Net/UnrealNetwork.h"

void ADartGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADartGameState, CurrentPlayerIndex);
	DOREPLIFETIME(ADartGameState, ThrowsRemaining);
	DOREPLIFETIME(ADartGameState, PlayerScores);
}

void ADartGameState::OnRep_CurrentPlayerIndex() {}
void ADartGameState::OnRep_ThrowsRemaining() {}