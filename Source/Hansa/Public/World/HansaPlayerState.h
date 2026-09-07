#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "HansaPlayerState.generated.h"

/** Public connection identity; private economy and research remain on the owning controller projection. */
UCLASS()
class HANSA_API AHansaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AHansaPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetAuthorityIdentity(int64 InHouseId);

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	int64 PublicHouseId = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	bool bAuthorityReady = false;
};
