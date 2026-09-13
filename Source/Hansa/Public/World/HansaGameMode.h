#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaLubeckScenarioInitializer.h"

#include "HansaGameMode.generated.h"

class AHansaStrategyPlayerController;

/** Server-owned MVP game mode and two-client authority composition root. */
UCLASS()
class HANSA_API AHansaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHansaGameMode();
	virtual ~AHansaGameMode() override;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	class UHansaRuntimeSimulationHost* GetSimulationHost();
	void SubmitMultiplayerIntent(AHansaStrategyPlayerController& Controller,
		const FHansaClientCommandIntent& Intent);
	void UpdateMultiplayerInterest(AHansaStrategyPlayerController& Controller,
		const FHansaClientInterest& Interest);
	void RequestMultiplayerProjectionRefresh(AHansaStrategyPlayerController& Controller,
		int64 ClientKnownRevision);
	void RefreshMultiplayerProjections(bool bForceFullRefresh);
	[[nodiscard]] int32 GetRegisteredAuthorityClientCount() const;
	[[nodiscard]] bool IsAuthorityFixtureMode() const { return bAuthorityFixtureMode; }

private:
	void EnsureLubeckWorldComposition();
	bool EnsureMultiplayerAuthority();
	uint64 FindPrincipal(const AHansaStrategyPlayerController& Controller) const;

	UPROPERTY(Transient)
	TObjectPtr<class UHansaRuntimeSimulationHost> SimulationHost;

	TUniquePtr<Hansa::Multiplayer::FHansaMultiplayerAuthority> MultiplayerAuthority;
	TMap<TWeakObjectPtr<AHansaStrategyPlayerController>, uint64> ClientPrincipals;
	uint64 NextPrincipalId = 1;
	uint64 RuntimeCampaignSeedOverride = 0;
	EHansaRuntimeScenario RuntimeScenario = EHansaRuntimeScenario::LubeckGrainShortage;
	bool bAuthorityFixtureMode = false;
	bool bSimulationHostInitializationAttempted = false;
};
