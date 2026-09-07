#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Network/HansaMultiplayerTypes.h"

#include "HansaGameState.generated.h"

/** Small public campaign projection replicated to every connected client. */
UCLASS()
class HANSA_API AHansaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AHansaGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void PublishAuthoritativeProjection(const FHansaClientProjectionSnapshot& Projection);

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	int64 ServerSimulationTick = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	int64 PublicProjectionRevision = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	FString ServerAuthoritativeHash;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	FString ScenarioOutcome;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	FString WinningVictoryId;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Hansa|Multiplayer")
	TArray<FHansaReplicatedVictoryObjective> VictoryObjectives;
};
