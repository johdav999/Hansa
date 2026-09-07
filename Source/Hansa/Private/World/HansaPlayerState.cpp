#include "World/HansaPlayerState.h"

#include "Net/UnrealNetwork.h"

AHansaPlayerState::AHansaPlayerState()
{
	bReplicates = true;
}

void AHansaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHansaPlayerState, PublicHouseId);
	DOREPLIFETIME(AHansaPlayerState, bAuthorityReady);
}

void AHansaPlayerState::SetAuthorityIdentity(const int64 InHouseId)
{
	if (!HasAuthority()) return;
	PublicHouseId = InHouseId;
	bAuthorityReady = InHouseId > 0;
	ForceNetUpdate();
}
