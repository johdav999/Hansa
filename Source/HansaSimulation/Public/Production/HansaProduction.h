#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"

namespace Hansa::Simulation
{
	enum class EHansaProductionKind : uint8
	{
		BuildingRecipe = 0,
		BackgroundSupply
	};

	enum class EHansaProductionBlocker : uint8
	{
		None = 0,
		Inactive,
		ConstructionIncomplete,
		MissingDefinition,
		InsufficientLaborerWorkforce,
		InsufficientArtisanWorkforce,
		MissingInput,
		StorageBlocked,
		InventoryTransactionFailed,
		NoNearbyTrees,
		HouseholdFuelProtected
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaProductionBlocker Blocker);
	/** Four-metre cells: standing trees within 48 metres of the camp footprint. */
	inline constexpr int32 LumberHarvestRadiusCells = 12;

	/**
	 * Converts partial staffing into an effective batch duration.
	 * One assigned worker is enough to operate a staffed recipe; speed is
	 * assigned workforce / required workforce, so duration grows by the inverse ratio.
	 */
	HANSASIMULATION_API int32 CalculateWorkforceAdjustedCycleTicks(
		int32 BaseCycleTicks,
		int32 AllocatedLaborerWorkforce,
		int32 RequiredLaborerWorkforce,
		int32 AllocatedArtisanWorkforce,
		int32 RequiredArtisanWorkforce);

	/** Initialization for either a building recipe or deterministic city background supply. */
	struct HANSASIMULATION_API FHansaProductionInitialization final
	{
		FHansaProductionId Id;
		EHansaProductionKind Kind = EHansaProductionKind::BuildingRecipe;
		FHansaBuildingId BuildingId;
		FHansaCityDefinitionId CityId;
		FHansaRecipeId RecipeId;
		FHansaGoodId SupplyGoodId;
		FHansaQuantity SupplyQuantityPerCycle;
		int32 SupplyCycleTicks = 0;
		FHansaInventoryId InputInventoryId;
		FHansaInventoryId OutputInventoryId;
		int32 AllocatedLaborerWorkforce = 0;
		int32 AllocatedArtisanWorkforce = 0;
		bool bUsesCityWorkforce = false;
		bool bActive = true;
	};

	struct HANSASIMULATION_API FHansaProductionInputReservation final
	{
		FHansaGoodId GoodId;
		FHansaReservationId ReservationId;
		FHansaQuantity Quantity;
	};

    struct FHansaProductionGoodTotal final { FHansaGoodId GoodId; int64 QuantityMilliUnits = 0; };

	/** Canonically ordered authoritative production state. */
	struct HANSASIMULATION_API FHansaProductionState final
	{
		FHansaProductionId Id;
		EHansaProductionKind Kind = EHansaProductionKind::BuildingRecipe;
		FHansaBuildingId BuildingId;
		FHansaCityDefinitionId CityId;
		FHansaRecipeId RecipeId;
		FHansaGoodId SupplyGoodId;
		FHansaQuantity SupplyQuantityPerCycle;
		int32 SupplyCycleTicks = 0;
		FHansaInventoryId InputInventoryId;
		FHansaInventoryId OutputInventoryId;
		int32 AllocatedLaborerWorkforce = 0;
		int32 AllocatedArtisanWorkforce = 0;
		bool bUsesCityWorkforce = false;
		bool bActive = true;
		int32 ProgressTicks = 0;
		uint64 CompletedCycles = 0;
        TArray<FHansaProductionGoodTotal> OutputTotals;
		bool bCompletedCycleLastTick = false;
		EHansaProductionBlocker Blocker = EHansaProductionBlocker::None;
		FHansaGoodId BlockingGoodId;
		FHansaQuantity BlockingRequiredQuantity;
		FHansaQuantity BlockingAvailableQuantity;
		TArray<FHansaProductionInputReservation> InputReservations;
        // Selected mode survives fallback. Changes apply only when no batch is in progress.
        FHansaRecipeId RequestedRecipeId;
        bool bFallbackToFresh = false;
        FHansaBuildingTypeId PendingUpgradeBuildingId;

	};

	struct HANSASIMULATION_API FHansaProductionThroughputProjection final
	{
		FHansaGoodId GoodId;
		FHansaQuantity NominalQuantityPerCycle;
		FHansaQuantity ActualQuantityLastTick;
	};

	/** Owning causal view used by game UI, diagnostics and later allowlisted automation queries. */
	struct HANSASIMULATION_API FHansaProductionProjection final
	{
		FHansaProductionId Id;
		EHansaProductionKind Kind = EHansaProductionKind::BuildingRecipe;
		FHansaBuildingId BuildingId;
		FHansaCityDefinitionId CityId;
		FHansaRecipeId RecipeId;
		FHansaInventoryId InputInventoryId;
		FHansaInventoryId OutputInventoryId;
		bool bActive = true;
		int32 ProgressTicks = 0;
		int32 CycleTicks = 0;
		uint64 CompletedCycles = 0;
        TArray<FHansaProductionGoodTotal> OutputTotals;
		int32 AllocatedLaborerWorkforce = 0;
		int32 RequiredLaborerWorkforce = 0;
		int32 AllocatedArtisanWorkforce = 0;
		int32 RequiredArtisanWorkforce = 0;
		bool bUsesCityWorkforce = false;
		EHansaProductionBlocker Blocker = EHansaProductionBlocker::None;
		FHansaGoodId BlockingGoodId;
		FHansaQuantity BlockingRequiredQuantity;
		FHansaQuantity BlockingAvailableQuantity;
		TArray<FHansaProductionThroughputProjection> Outputs;
        FHansaRecipeId RequestedRecipeId;
        bool bFallbackToFresh = false;
        FHansaBuildingTypeId PendingUpgradeBuildingId;
	};

	/** Owning immutable copy for save/network/asynchronous readers. */
	class HANSASIMULATION_API FHansaProductionSnapshot final
	{
	public:
		[[nodiscard]] uint64 GetNextReservationValue() const { return NextReservationValue; }
		[[nodiscard]] TConstArrayView<FHansaProductionState> GetProductions() const { return Productions; }

	private:
		friend class FHansaSimulationReadOnlyAccess;
		uint64 NextReservationValue = 1;
		TArray<FHansaProductionState> Productions;
	};
}
