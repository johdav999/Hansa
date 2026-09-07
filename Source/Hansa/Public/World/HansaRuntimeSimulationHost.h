#pragma once

#include "CoreMinimal.h"
#include "AI/HansaMerchantAI.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Placement/HansaPlacement.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Scenario/HansaScenario.h"
#include "Save/HansaSaveEnvelope.h"
#include "UObject/Object.h"
#include "World/HansaLubeckScenarioInitializer.h"

#include "HansaRuntimeSimulationHost.generated.h"

class UWorld;
namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	struct FHansaCompiledBuildingDefinition;
}
struct FHansaRuntimeSimulationState;

UENUM(BlueprintType)
enum class EHansaRuntimeSimulationSpeed : uint8
{
	Paused,
	Normal,
	Fast,
	Fastest
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHansaRuntimeSimulationAdvanced, int64);

/**
 * World-owned authoritative runtime for the playable Lübeck slice. UI presenters submit intents and consume
 * projections; only this host owns the mutable simulation state and advances its clock.
 */
UCLASS(NotBlueprintable)
class HANSA_API UHansaRuntimeSimulationHost final : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UHansaRuntimeSimulationHost() override;

	bool InitializeForLubeck(
		UWorld* World,
		FString& OutError,
		EHansaRuntimeScenario Scenario = EHansaRuntimeScenario::LubeckGrainShortage,
		uint64 CampaignSeedOverride = 0);
	[[nodiscard]] EHansaRuntimeScenario GetScenario() const;
	void SetSpeed(EHansaRuntimeSimulationSpeed NewSpeed);
	[[nodiscard]] EHansaRuntimeSimulationSpeed GetSpeed() const { return Speed; }
	[[nodiscard]] bool IsReady() const;

	/** Frame-time adapter used by the game mode. It preserves fractional tick debt across frames. */
	bool AdvanceRealTime(double DeltaSeconds);
	/** Deterministic explicit stepping seam for automation and focused presenter tests. */
	bool AdvanceTicks(int32 TickCount);

	[[nodiscard]] const Hansa::Simulation::FHansaCompiledBuildingDefinition* FindBuildingDefinition(
		const FString& StableId) const;
	[[nodiscard]] const Hansa::Simulation::FHansaPlacementMapInitialization* FindPlacementMap() const;
	[[nodiscard]] Hansa::Simulation::FHansaPlacementValidationResult ValidatePlacement(
		const Hansa::Simulation::FHansaPlacementSpec& Spec) const;
	Hansa::Simulation::FHansaCommandGatewayResult PlaceBuildings(
		TConstArrayView<Hansa::Simulation::FHansaPlacementSpec> Specs);
	Hansa::Simulation::FHansaCommandGatewayResult EditRoute(
		Hansa::Simulation::FHansaRouteId RouteId,
		TConstArrayView<Hansa::Simulation::FHansaRouteStop> Stops);
	Hansa::Simulation::FHansaCommandGatewayResult SetRouteActive(
		Hansa::Simulation::FHansaRouteId RouteId,
		bool bActive);
	Hansa::Simulation::FHansaCommandGatewayResult SetProductionActive(
		Hansa::Simulation::FHansaProductionId ProductionId,
		bool bActive);
	Hansa::Simulation::FHansaCommandGatewayResult QueueResearch(const FString& TechnologyId);

	/** Server-only seams that derive command identity, tick and global ordering after principal authorization. */
	Hansa::Simulation::FHansaCommandGatewayResult PlaceBuildingsForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		TConstArrayView<Hansa::Simulation::FHansaPlacementSpec> Specs);
	Hansa::Simulation::FHansaCommandGatewayResult SetRouteActiveForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaRouteId RouteId,
		bool bActive);
	Hansa::Simulation::FHansaCommandGatewayResult QueueResearchForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		const FString& TechnologyId);

	[[nodiscard]] Hansa::Simulation::FHansaHouseId GetHouseId() const;
	[[nodiscard]] Hansa::Simulation::FHansaCityDefinitionId GetCityId() const;
	[[nodiscard]] int32 GetPlacedBuildingCount() const;
	[[nodiscard]] int64 GetSimulationTick() const;
	[[nodiscard]] uint64 GetLastProcessedCommandSequence() const;
	[[nodiscard]] FString GetBuildingWorldStatus(int64 BuildingValue) const;
	[[nodiscard]] Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaSimulationProjection> BuildProjection() const;
	[[nodiscard]] const Hansa::Simulation::FHansaEconomicRegistry* GetEconomicRegistry() const;
	[[nodiscard]] Hansa::Simulation::FHansaHouseId GetRivalHouseId() const;
	void SetMerchantAIEnabled(bool bEnabled);
	[[nodiscard]] bool IsMerchantAIEnabled() const;
	[[nodiscard]] const Hansa::Simulation::FHansaMerchantAIDecisionTrace* GetLastMerchantAIDecision() const;
	[[nodiscard]] TConstArrayView<Hansa::Simulation::FHansaMerchantAIDecisionTrace> GetMerchantAIDecisionHistory() const;
	[[nodiscard]] const Hansa::Simulation::FHansaScenarioProgress* GetScenarioProgress() const;
	[[nodiscard]] uint64 GetCampaignSeed() const;
	[[nodiscard]] TConstArrayView<Hansa::Simulation::FHansaDomainEvent> GetEventHistory() const;
	bool SynchronizeWorldProjection();
	/** Authority-only tick-boundary save seam; slot/filesystem/UI orchestration is separate. */
	Hansa::Simulation::FHansaSaveResult CaptureSaveBytes(TArray<uint8>& OutBytes,
		const FString& DisplayName, const FString& SavedUtc) const;
	/** Read-only compatibility/metadata inspection used by the bounded save-slot service. */
	Hansa::Simulation::FHansaSaveResult InspectSaveBytes(TConstArrayView<uint8> Bytes,
		Hansa::Simulation::FHansaSaveSnapshot& OutSnapshot, int64& OutSimulationTick) const;
	Hansa::Simulation::FHansaSaveResult RestoreSaveBytes(TConstArrayView<uint8> Bytes);

	FHansaRuntimeSimulationAdvanced& OnSimulationAdvanced() { return SimulationAdvanced; }

private:
	bool PublishStateChange(TConstArrayView<Hansa::Simulation::FHansaDomainEvent> Events);
	double TicksPerSecond() const;

	TSharedPtr<FHansaRuntimeSimulationState> Runtime;
	TWeakObjectPtr<UWorld> BoundWorld;
	EHansaRuntimeSimulationSpeed Speed = EHansaRuntimeSimulationSpeed::Normal;
	double TickAccumulator = 0.0;
	FHansaRuntimeSimulationAdvanced SimulationAdvanced;
};
