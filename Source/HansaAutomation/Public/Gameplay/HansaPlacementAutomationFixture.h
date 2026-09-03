#pragma once

#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"
#include "Placement/HansaPlacement.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Systems/HansaSimulationPipeline.h"

namespace Hansa::Automation
{
	/** Sticky observable milestones and latest causal values for the integrated Sprint 6 fixture. */
	struct HANSAAUTOMATION_API FHansaIntegratedLubeckCheckpointState final
	{
		bool bConstructionCompleted = false;
		bool bInventoryMoved = false;
		bool bProductionCompleted = false;
		bool bPopulationGrown = false;
		bool bBreadConsumed = false;
		int32 CompletedDeliveries = 0;
		uint64 CompletedProductionCycles = 0;
		int32 Residents = 0;
		int64 BreadConsumedLastTickMilliUnits = 0;
		int64 BreadConsumedTotalMilliUnits = 0;
	};

	/**
	 * Named empty Lübeck placement fixture and its normal-input intent adapter.
	 * Semantic automation may invoke these intents, but it cannot mutate authoritative state directly.
	 */
	class HANSAAUTOMATION_API FHansaPlacementAutomationFixture final
	{
	public:
		static constexpr const TCHAR* StableFixtureId = TEXT("empty_lubeck_build_v1");
		static constexpr const TCHAR* IntegratedFixtureId = TEXT("integrated_lubeck_city_v1");
		static constexpr uint32 FixtureVersion = 1;
		static constexpr uint64 RegistryHash = 0x534F35504C414345ULL;
		static constexpr uint32 IntegratedFixtureVersion = 1;
		static constexpr uint64 IntegratedRegistryHash = 0x5330365030344C42ULL;

		[[nodiscard]] bool Load(FHansaSemanticUiRegistry& InRegistry, FString& OutError);
		[[nodiscard]] bool LoadIntegrated(FHansaSemanticUiRegistry& InRegistry, FString& OutError);
		void SynchronizeSemantics();

		[[nodiscard]] bool IsLoaded() const { return bLoaded; }
		[[nodiscard]] bool IsIntegrated() const { return bIntegrated; }
		[[nodiscard]] const TCHAR* GetFixtureId() const { return bIntegrated ? IntegratedFixtureId : StableFixtureId; }
		[[nodiscard]] int32 GetPlacedBuildingCount() const;
		[[nodiscard]] int64 GetSimulationTick() const;
		[[nodiscard]] bool CanConfirm() const { return bPreviewCanPlace; }
		[[nodiscard]] Hansa::Simulation::EHansaPlacementFailure GetPrimaryFailure() const { return PrimaryFailure; }
		[[nodiscard]] FString GetLastPlacedEntityValue() const;
		[[nodiscard]] Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaSimulationProjection> BuildProjection() const;
		[[nodiscard]] TOptional<Hansa::Simulation::FHansaConstructionProjection> QueryConstruction(
			Hansa::Simulation::FHansaBuildingId BuildingId) const;
		[[nodiscard]] Hansa::Simulation::FHansaConstructionCostProjection QueryConstructionCost(
			Hansa::Simulation::FHansaBuildingTypeId BuildingDefinitionId) const;
		[[nodiscard]] Hansa::Simulation::FHansaBuildingId GetLastPlacedBuildingId() const { return LastPlacedBuildingId; }
		[[nodiscard]] const FHansaIntegratedLubeckCheckpointState& GetIntegratedCheckpoints() const
		{
			return IntegratedCheckpoints;
		}
		[[nodiscard]] bool AdvanceTicks(int32 TickCount);
		[[nodiscard]] bool CancelLastConstructionIntent();
		[[nodiscard]] bool RemoveLastBuildingIntent();
		void ObserveCameraState(const FVector2D& Focus, float YawDegrees, float ZoomDistance);
		[[nodiscard]] const FHansaSemanticUiRegistry* GetRegistry() const { return Registry; }

		// Device-neutral input intents. Slate click handlers and controlled semantic actions share these exact methods.
		bool SelectRoadIntent();
		bool SelectWarehouseIntent();
		bool TargetRoadCellIntent();
		bool TargetInvalidCellIntent();
		bool TargetValidCellIntent();
		bool RotateIntent();
		bool ToggleRepeatIntent();
		bool ConfirmIntent();
		bool CancelIntent();

	private:
		enum class ESelectedTool : uint8
		{
			None = 0,
			Road,
			Warehouse
		};

		[[nodiscard]] bool InitializeState(FString& OutError);
		[[nodiscard]] bool InitializeIntegratedState(FString& OutError);
		void UpdateIntegratedCheckpoints();
		void RegisterSemantics();
		void RefreshPreviewValidation();
		void FocusIntent(const FString& SemanticId);
		void UpdateSemanticState(const TCHAR* SemanticId, FHansaSemanticState State, const FString& Label = FString());
		[[nodiscard]] static FString HumanFailure(Hansa::Simulation::EHansaPlacementFailure Failure);
		[[nodiscard]] static FString HumanRemedy(Hansa::Simulation::EHansaPlacementFailure Failure);

		FHansaSemanticUiRegistry* Registry = nullptr;
		Hansa::Simulation::FHansaSimulationDefinitionContext Definitions;
		Hansa::Simulation::FHansaSimulationState State;
		Hansa::Simulation::FHansaSimulationTransientCache Cache;
		Hansa::Simulation::FHansaPlacementSession PlacementSession;
		TOptional<Hansa::Simulation::FHansaPlacementValidationResult> PreviewValidation;
		Hansa::Simulation::FHansaHouseId HouseId;
		Hansa::Simulation::FHansaCityDefinitionId CityId;
		Hansa::Simulation::FHansaBuildingTypeId RoadDefinitionId;
		Hansa::Simulation::FHansaBuildingTypeId WarehouseDefinitionId;
		Hansa::Simulation::FHansaBuildingId LastPlacedBuildingId;
		FString FocusedSemanticId;
		ESelectedTool SelectedTool = ESelectedTool::None;
		Hansa::Simulation::EHansaPlacementFailure PrimaryFailure = Hansa::Simulation::EHansaPlacementFailure::None;
		bool bLoaded = false;
		bool bIntegrated = false;
		bool bRepeat = true;
		bool bHasPreview = false;
		bool bPreviewCanPlace = false;
		FHansaIntegratedLubeckCheckpointState IntegratedCheckpoints;
		uint64 NextCommandId = 1;
		uint64 NextBuildingId = 1;
		FString CameraStateValue = TEXT("focus=-3200,-700;yawDegrees=35;zoomDistance=6500");
	};
}
