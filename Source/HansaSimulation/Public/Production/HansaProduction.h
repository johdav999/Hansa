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
		InventoryTransactionFailed
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaProductionBlocker Blocker);

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
		bool bCompletedCycleLastTick = false;
		EHansaProductionBlocker Blocker = EHansaProductionBlocker::None;
		FHansaGoodId BlockingGoodId;
		FHansaQuantity BlockingRequiredQuantity;
		FHansaQuantity BlockingAvailableQuantity;
		TArray<FHansaProductionInputReservation> InputReservations;
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
