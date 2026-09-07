#include "World/HansaGameState.h"

#include "Net/UnrealNetwork.h"

AHansaGameState::AHansaGameState()
{
	bReplicates = true;
}

void AHansaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHansaGameState, ServerSimulationTick);
	DOREPLIFETIME(AHansaGameState, PublicProjectionRevision);
	DOREPLIFETIME(AHansaGameState, ServerAuthoritativeHash);
	DOREPLIFETIME(AHansaGameState, ScenarioOutcome);
	DOREPLIFETIME(AHansaGameState, WinningVictoryId);
	DOREPLIFETIME(AHansaGameState, VictoryObjectives);
}

void AHansaGameState::PublishAuthoritativeProjection(const FHansaClientProjectionSnapshot& Projection)
{
	if (!HasAuthority()) return;
	ServerSimulationTick = Projection.ServerTick;
	PublicProjectionRevision = FMath::Max(PublicProjectionRevision + 1, Projection.Revision);
	ServerAuthoritativeHash = Projection.AuthoritativeHash;
	ScenarioOutcome = Projection.ScenarioOutcome;
	WinningVictoryId = Projection.WinningVictoryId;
	VictoryObjectives = Projection.VictoryObjectives;
	ForceNetUpdate();
}
