#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "Trade/HansaWaterNavigation.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Engine/AssetManager.h"
#if !UE_BUILD_SHIPPING
#include "AssetRegistry/AssetRegistryModule.h"
#endif
#include "HansaLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Inventory/HansaInventory.h"
#include "Market/HansaMarket.h"
#include "Population/HansaPopulation.h"
#include "Presence/HansaForeignPresenceInitialization.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Production/HansaProduction.h"

using namespace Hansa::Simulation;

namespace
{
	template <typename TValue>
	bool Assign(const THansaValueResult<TValue>& Result, TValue& Out)
	{
		if (!Result) return false;
		Out = Result.Value;
		return true;
	}

	template <typename TId>
	bool Entity(const uint64 Value, TId& Out)
	{
		return Assign(TId::TryCreate(Value), Out);
	}

	bool AddBuilding(
		FHansaSimulationInitialization& Initialization,
		const uint64 Value,
		const TCHAR* DefinitionText,
		const FHansaHouseId HouseId,
		FHansaBuildingState*& OutBuilding)
	{
		FHansaBuildingState Building;
		if (!Entity(Value, Building.Id) || !Assign(FHansaBuildingTypeId::TryParse(DefinitionText), Building.DefinitionId))
		{
			return false;
		}
		Building.OwnerId = HouseId;
		Building.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
		Building.ConstructionState = EHansaConstructionState::Completed;
		const int32 AddedIndex = Initialization.Buildings.Add(MoveTemp(Building));
		OutBuilding = &Initialization.Buildings[AddedIndex];
		return true;
	}

	bool AddProduction(
		FHansaSimulationInitialization& Initialization,
		const uint64 Value,
		const FHansaBuildingId BuildingId,
		const TCHAR* RecipeText,
		const int32 Laborers,
		const int32 Artisans,
		const bool bActive)
	{
		FHansaProductionInitialization Production;
		if (!Entity(Value, Production.Id) || !Assign(FHansaRecipeId::TryParse(RecipeText), Production.RecipeId) ||
			!Entity(100 + Value, Production.InputInventoryId) || !Entity(100 + Value, Production.OutputInventoryId))
		{
			return false;
		}
		Production.BuildingId = BuildingId;
		Production.AllocatedLaborerWorkforce = Laborers;
		Production.AllocatedArtisanWorkforce = Artisans;
		Production.bActive = bActive;
		Initialization.Productions.Add(MoveTemp(Production));
		return true;
	}

	bool AddBuildingInventory(FHansaSimulationInitialization& Initialization,
		const FHansaBuildingState& Building, const uint64 InventoryValue,
		const FHansaEconomicRegistry& Registry)
	{
		const FHansaCompiledBuildingDefinition* Definition = Registry.FindBuilding(Building.DefinitionId.ToString());
		FHansaInventoryInitialization Inventory;
		if (Definition == nullptr || Definition->StorageCapacityMilliUnits <= 0 ||
			!Entity(InventoryValue, Inventory.Id)) return false;
		Inventory.OwnerKind = EHansaInventoryOwnerKind::Building;
		Inventory.BuildingId = Building.Id;
		Inventory.Capacity = FHansaQuantity::FromRaw(Definition->StorageCapacityMilliUnits);
		for (const FHansaCompiledGoodDefinition& GoodDefinition : Registry.GetGoods())
		{
			FHansaGoodId GoodId;
			if (!Assign(FHansaGoodId::TryParse(GoodDefinition.StableId), GoodId)) return false;
			Inventory.AcceptedGoods.Add(GoodId);
		}
		Initialization.Inventories.Add(MoveTemp(Inventory));
		return true;
	}

	bool AddPlacement(
		FHansaPlacementInitialization& Placement,
		const FHansaBuildingState& Building,
		const FHansaCityDefinitionId CityId,
		const FHansaEconomicRegistry& Registry,
		const int32 X,
		const int32 Y)
	{
		const FHansaCompiledBuildingDefinition* Definition = Registry.FindBuilding(Building.DefinitionId.ToString());
		if (Definition == nullptr || Definition->FootprintWidthCells <= 0 || Definition->FootprintHeightCells <= 0)
		{
			return false;
		}
		FHansaPlacedBuildingRecord Record;
		Record.BuildingId = Building.Id;
		Record.OwnerId = Building.OwnerId;
		Record.Spec.CityId = CityId;
		Record.Spec.BuildingDefinitionId = Building.DefinitionId;
		Record.Spec.Anchor = { X, Y };
		for (int32 DeltaX = 0; DeltaX < Definition->FootprintWidthCells; ++DeltaX)
		{
			for (int32 DeltaY = 0; DeltaY < Definition->FootprintHeightCells; ++DeltaY)
			{
				Record.OccupiedCells.Add({ X + DeltaX, Y + DeltaY });
			}
		}
		Placement.Placements.Add(MoveTemp(Record));
		return true;
	}

	bool InitializeCommon(
		FHansaSimulationInitialization& Initialization,
		FHansaHouseId& OutHouseId,
		FHansaHouseId& OutRivalHouseId,
		TArray<FHansaHouseId>& OutHouseIds,
		FHansaCityDefinitionId& OutCityId,
		const int64 StartingMoney)
	{
		FHansaSimulationVersion Version;
		FHansaSimulationTick Tick;
		if (!Entity(1, OutHouseId) || !Entity(2, OutRivalHouseId) ||
			!Assign(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")), OutCityId) ||
			!Assign(FHansaSimulationVersion::TryCreate(1), Version) ||
			!Assign(FHansaSimulationTick::TryCreate(0), Tick) ||
			!Assign(FHansaSimulationClock::TryCreate(Version, Tick), Initialization.Clock))
		{
			return false;
		}
		OutHouseIds.Reset();
		for (uint64 Value = 1; Value <= 8; ++Value)
		{
			FHansaHouseId HouseId;
			if (!Entity(Value, HouseId)) return false;
			OutHouseIds.Add(HouseId);
			Initialization.Houses.Add({HouseId,
				FHansaMoney::FromRaw(Value == 1 ? StartingMoney : 50'000)});
			Initialization.Research.Add({HouseId, Value == 1 ? 1'000 : 400});
		}
		Initialization.Cities.Add({ OutCityId, FHansaQuantity() });
		return true;
	}

	bool ConfigureEightHouseBuildableOpportunities(
		FHansaPlacementInitialization& Placement,
		TConstArrayView<FHansaHouseId> Houses,
		const FHansaCityDefinitionId CityId,
		TArray<FHansaHouseStartOpportunity>& OutOpportunities)
	{
		if (Houses.Num() != 8) return false;
		FHansaPlacementMapInitialization* Map = Placement.Maps.FindByPredicate(
			[CityId](const FHansaPlacementMapInitialization& Candidate) { return Candidate.CityId == CityId; });
		if (Map == nullptr) return false;
		TArray<FHansaGridCoordinate> Occupied;
		for (const FHansaPlacedBuildingRecord& Record : Placement.Placements)
		{
			for (const FHansaGridCoordinate Cell : Record.OccupiedCells) Occupied.AddUnique(Cell);
		}
		int32 Counts[8] = {};
		TArray<int32> BuildableCellIndices;
		for (int32 CellIndex = 0; CellIndex < Map->Cells.Num(); ++CellIndex)
		{
			const FHansaPlacementGridCell& Cell = Map->Cells[CellIndex];
			if (Cell.Terrain == EHansaPlacementTerrain::Water || Cell.bBlocked) continue;
			BuildableCellIndices.Add(CellIndex);
		}
		if (BuildableCellIndices.Num() < Houses.Num()) return false;
		const FHansaGridCoordinate PlayerStart = Map->BoundsMin.X < 0
			? Hansa::Game::LubeckPlacementGrid::WorldToGrid(
				Hansa::Game::LubeckPlacementGrid::SurveyStartLocation())
			: Hansa::Game::LubeckPlacementGrid::WorldToGrid(
				Hansa::Game::LubeckMap::AutomationStartTransform().GetLocation());
		int32 PlayerStartOrdinal = 0;
		int64 BestPlayerStartDistance = MAX_int64;
		for (int32 Ordinal = 0; Ordinal < BuildableCellIndices.Num(); ++Ordinal)
		{
			const FHansaGridCoordinate Coordinate = Map->Cells[BuildableCellIndices[Ordinal]].Coordinate;
			const int64 Distance = FMath::Abs(static_cast<int64>(Coordinate.X) - PlayerStart.X) +
				FMath::Abs(static_cast<int64>(Coordinate.Y) - PlayerStart.Y);
			if (Distance < BestPlayerStartDistance)
			{
				BestPlayerStartDistance = Distance;
				PlayerStartOrdinal = Ordinal;
			}
		}
		const int32 PlayerBand = FMath::Min(7,
			PlayerStartOrdinal * Houses.Num() / BuildableCellIndices.Num());
		int32 HouseByBand[8] = {};
		int32 NextOtherHouse = 1;
		for (int32 Band = 0; Band < Houses.Num(); ++Band)
		{
			HouseByBand[Band] = Band == PlayerBand ? 0 : NextOtherHouse++;
		}
		for (int32 Ordinal = 0; Ordinal < BuildableCellIndices.Num(); ++Ordinal)
		{
			const int32 Band = FMath::Min(7, Ordinal * Houses.Num() / BuildableCellIndices.Num());
			const int32 HouseIndex = HouseByBand[Band];
			Map->Cells[BuildableCellIndices[Ordinal]].OwnerId = Houses[HouseIndex];
			++Counts[HouseIndex];
		}
		for (const FHansaPlacedBuildingRecord& Record : Placement.Placements)
		{
			for (const FHansaGridCoordinate Coordinate : Record.OccupiedCells)
			{
				if (FHansaPlacementGridCell* Cell = Map->Cells.FindByPredicate(
					[Coordinate](const FHansaPlacementGridCell& Candidate) { return Candidate.Coordinate == Coordinate; }))
				{
					Cell->OwnerId = Record.OwnerId;
				}
			}
		}
		FMemory::Memzero(Counts, sizeof(Counts));
		for (const FHansaPlacementGridCell& Cell : Map->Cells)
		{
			if (Cell.Terrain == EHansaPlacementTerrain::Water || Cell.bBlocked) continue;
			const int32 HouseIndex = Houses.IndexOfByKey(Cell.OwnerId);
			if (HouseIndex != INDEX_NONE) ++Counts[HouseIndex];
		}
		TArray<FHansaPlacementEntitlement> TemplateEntitlements;
		for (const FHansaPlacementEntitlement& Entitlement : Placement.Entitlements)
		{
			if (Entitlement.HouseId == Houses[0]) TemplateEntitlements.Add(Entitlement);
		}
		for (int32 Index = 1; Index < Houses.Num(); ++Index)
		{
			for (const FHansaPlacementEntitlement& Template : TemplateEntitlements)
			{
				if (!Placement.Entitlements.ContainsByPredicate([&, Index](const FHansaPlacementEntitlement& Candidate)
					{ return Candidate.HouseId == Houses[Index] && Candidate.BuildingDefinitionId == Template.BuildingDefinitionId; }))
				{
					Placement.Entitlements.Add({Houses[Index], Template.BuildingDefinitionId});
				}
			}
		}
		OutOpportunities.Reset();
		for (int32 Index = 0; Index < Houses.Num(); ++Index)
		{
			const FHansaPlacementGridCell* Start = Map->Cells.FindByPredicate(
				[&, Index](const FHansaPlacementGridCell& Cell)
				{
					return Cell.OwnerId == Houses[Index] && Cell.Terrain != EHansaPlacementTerrain::Water &&
						!Cell.bBlocked && !Occupied.Contains(Cell.Coordinate);
				});
			if (Start == nullptr || Counts[Index] <= 0) return false;
			OutOpportunities.Add({Houses[Index], CityId, Start->Coordinate, Counts[Index]});
		}
		return true;
	}

	bool ConfigureMarket(
		FHansaCityMarketInitialization& Market,
		const FHansaCompiledCityMarketProfileDefinition& CityProfile,
		const FHansaCompiledMarketGoodProfile& GoodProfile,
		const FHansaCityDefinitionId CityId,
		const FHansaInventoryId InventoryId)
	{
		Market.CityId = CityId;
		if (!Assign(FHansaGoodId::TryParse(GoodProfile.GoodId), Market.GoodId)) return false;
		Market.InventoryIds.Add(InventoryId);
		Market.DesiredReserve = FHansaQuantity::FromRaw(GoodProfile.DesiredReserveMilliUnits);
		Market.ConfirmedIncomingSupplyPerUpdate =
			FHansaQuantity::FromRaw(GoodProfile.ConfirmedIncomingSupplyMilliUnits);
		Market.bMarketOnly = CityProfile.bMarketOnly;
		Market.BackgroundProductionPerUpdate =
			FHansaQuantity::FromRaw(GoodProfile.BackgroundProductionMilliUnitsPerUpdate);
		Market.BackgroundCitizenDemandPerUpdate =
			FHansaQuantity::FromRaw(GoodProfile.BackgroundCitizenDemandMilliUnitsPerUpdate);
		Market.BackgroundIndustrialDemandPerUpdate =
			FHansaQuantity::FromRaw(GoodProfile.BackgroundIndustrialDemandMilliUnitsPerUpdate);
		Market.ReportPolicy.ReportCadenceTicks = CityProfile.ReportCadenceTicks;
		Market.ReportPolicy.CurrentMaxAgeTicks = CityProfile.CurrentReportMaxAgeTicks;
		Market.ReportPolicy.RecentMaxAgeTicks = CityProfile.RecentReportMaxAgeTicks;
		Market.ReportPolicy.StaleMaxAgeTicks = CityProfile.StaleReportMaxAgeTicks;
		Market.ReportPolicy.EstimatedMaxAgeTicks = CityProfile.EstimatedReportMaxAgeTicks;
		Market.SeasonModifierBasisPoints = GoodProfile.SeasonModifierBasisPoints;
		Market.CityModifierBasisPoints = GoodProfile.CityModifierBasisPoints;
		Market.MinimumPriceMilliMarks = GoodProfile.MinimumPriceMilliMarks;
		Market.MaximumPriceMilliMarks = GoodProfile.MaximumPriceMilliMarks;
		Market.InitialPriceMilliMarks = GoodProfile.InitialPriceMilliMarks;
		Market.InitialReportTick = 0;
		return true;
	}

	bool AddMarketOnlyCity(FHansaSimulationInitialization& Initialization,
		const FHansaEconomicRegistry& Registry, const TCHAR* CityStableId, const uint64 InventoryValue)
	{
		const FHansaCompiledCityMarketProfileDefinition* CityProfile = Registry.FindCityMarket(CityStableId);
		FHansaCityDefinitionId CityId;
		FHansaInventoryId InventoryId;
		if (CityProfile == nullptr || !CityProfile->bMarketOnly ||
			!Assign(FHansaCityDefinitionId::TryParse(CityStableId), CityId) ||
			!Entity(InventoryValue, InventoryId))
		{
			return false;
		}
		Initialization.Cities.Add({ CityId, FHansaQuantity() });
		FHansaInventoryInitialization Inventory;
		Inventory.Id = InventoryId;
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = CityId;
		Inventory.Capacity = FHansaQuantity::FromRaw(20'000'000);
		for (const FHansaCompiledGoodDefinition& GoodDefinition : Registry.GetGoods())
		{
			FHansaGoodId GoodId;
			if (!Assign(FHansaGoodId::TryParse(GoodDefinition.StableId), GoodId)) return false;
			const FHansaCompiledMarketGoodProfile* GoodProfile = CityProfile->Goods.FindByPredicate(
				[&GoodDefinition](const FHansaCompiledMarketGoodProfile& Candidate)
				{
					return Candidate.GoodId == GoodDefinition.StableId;
				});
			if (GoodProfile == nullptr) return false;
			Inventory.AcceptedGoods.Add(GoodId);
			Inventory.InitialStock.Add({ GoodId, FHansaQuantity::FromRaw(GoodProfile->InitialStockMilliUnits) });
		}
		Initialization.Inventories.Add(MoveTemp(Inventory));
		for (const FHansaCompiledMarketGoodProfile& GoodProfile : CityProfile->Goods)
		{
			FHansaCityMarketInitialization Market;
			if (!ConfigureMarket(Market, *CityProfile, GoodProfile, CityId, InventoryId)) return false;
			Initialization.Markets.Add(MoveTemp(Market));
		}
		return true;
	}

	bool AddCanonicalTrade(FHansaSimulationInitialization& Initialization,
		const FHansaEconomicRegistry& Registry, TConstArrayView<FHansaHouseId> HouseIds,
		const FHansaCityDefinitionId Lubeck)
	{
		if (HouseIds.Num() != 8) return false;
		struct FSpec
		{
			uint64 VehicleValue;
			uint64 InventoryValue;
			uint64 RouteValue;
			const TCHAR* VehicleDefinition;
			const TCHAR* RouteDefinition;
			const TCHAR* Destination;
			const TCHAR* Good;
			EHansaRouteMode Mode;
			int32 OwnerIndex = 0;
		};
		const FSpec Specs[] = {
			{ 1, 1001, 1, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea },
			{ 2, 1002, 2, TEXT("Vehicle.Wagon"), TEXT("Route.SaltRoad"), TEXT("City.Luneburg"), TEXT("Good.Salt"), EHansaRouteMode::Land },
			{ 3, 1003, 3, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 1 },
			{ 4, 1004, 4, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 2 },
			{ 5, 1005, 5, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 3 },
			{ 6, 1006, 6, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 4 },
			{ 7, 1007, 7, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 5 },
			{ 8, 1008, 8, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 6 },
			{ 9, 1009, 9, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, 7 }
		};
		for (const FSpec& Spec : Specs)
		{
			const FHansaCompiledVehicleDefinition* Definition = Registry.FindVehicle(Spec.VehicleDefinition);
			FHansaVehicleState Vehicle;
			FHansaInventoryInitialization Cargo;
			FHansaRouteState Route;
			FHansaCityDefinitionId Destination;
			FHansaGoodId GoodId;
			if (Definition == nullptr || !Entity(Spec.VehicleValue, Vehicle.Id) ||
				!Assign(FHansaVehicleDefinitionId::TryParse(Spec.VehicleDefinition), Vehicle.DefinitionId) ||
				!Entity(Spec.InventoryValue, Vehicle.CargoInventoryId) || !Entity(Spec.RouteValue, Route.Id) ||
				!Assign(FHansaRouteDefinitionId::TryParse(Spec.RouteDefinition), Route.RouteDefinitionId) ||
				!Assign(FHansaCityDefinitionId::TryParse(Spec.Destination), Destination) ||
				!Assign(FHansaGoodId::TryParse(Spec.Good), GoodId)) return false;
			const FHansaHouseId OwnerId = HouseIds[Spec.OwnerIndex];
			Vehicle.OwnerId = OwnerId;
			Vehicle.Mode = Spec.Mode;
			Vehicle.Capacity = FHansaQuantity::FromRaw(Definition->CargoCapacityMilliUnits);
			Vehicle.CurrentCityId = Lubeck;
			Vehicle.UpkeepPfennigPerTravelTick = Definition->UpkeepPfennigPerTravelTick;
			Cargo.Id = Vehicle.CargoInventoryId;
			Cargo.OwnerKind = EHansaInventoryOwnerKind::Vehicle;
			Cargo.VehicleId = Vehicle.Id;
			Cargo.Capacity = Vehicle.Capacity;
			for (const FHansaCompiledGoodDefinition& GoodDefinition : Registry.GetGoods())
			{
				FHansaGoodId Accepted;
				if (!Assign(FHansaGoodId::TryParse(GoodDefinition.StableId), Accepted)) return false;
				Cargo.AcceptedGoods.Add(Accepted);
			}
			Route.OwnerId = OwnerId;
			Route.VehicleId = Vehicle.Id;
			Route.Mode = Spec.Mode;
			FHansaRouteStop First;
			First.CityId = Lubeck;
			First.Actions.Add({ EHansaRouteCargoActionKind::Unload, EHansaRouteCargoCondition::Always,
				GoodId, FHansaQuantity::FromRaw(20'000), FHansaQuantity() });
			FHansaRouteStop Second;
			Second.CityId = Destination;
			Second.Actions.Add({ EHansaRouteCargoActionKind::Load, EHansaRouteCargoCondition::Always,
				GoodId, FHansaQuantity::FromRaw(20'000),
				FHansaQuantity::FromRaw(Spec.Mode == EHansaRouteMode::Sea ? 30'000 : 12'000) });
			Route.Stops = { MoveTemp(First), MoveTemp(Second) };
			Initialization.Vehicles.Add(MoveTemp(Vehicle));
			Initialization.Inventories.Add(MoveTemp(Cargo));
			Initialization.Routes.Add(MoveTemp(Route));
		}
		return true;
	}

	bool BuildShortageInitialization(
		FHansaSimulationInitialization& Initialization,
		const FHansaEconomicRegistry& Registry,
		const FHansaHouseId HouseId,
		const FHansaCityDefinitionId CityId)
	{
		const FHansaCompiledCityMarketProfileDefinition* CityMarket = Registry.FindCityMarket(TEXT("City.Lubeck"));
		if (CityMarket == nullptr) return false;
		const bool bEnhanced = Registry.GetRegistryHash() == FHansaLubeckScenarioInitializer::P33CandidateRegistryHash;

		FHansaBuildingState* GrainFarm = nullptr;
		FHansaBuildingState* Mill = nullptr;
		FHansaBuildingState* Bakery = nullptr;
		FHansaBuildingState* Brewery = nullptr;
		FHansaBuildingState* Residence = nullptr;
		FHansaBuildingState* MarketBuilding = nullptr;
		FHansaBuildingState* Dock = nullptr;
		// AddBuilding returns pointers into this array. Reserve the complete scenario set so later adds cannot
		// invalidate those pointers before production and placement records are assembled.
		Initialization.Buildings.Reserve(80);
		if (!AddBuilding(Initialization, 1, TEXT("Building.GrainFarm"), HouseId, GrainFarm) ||
			!AddBuilding(Initialization, 2, TEXT("Building.Mill"), HouseId, Mill) ||
			!AddBuilding(Initialization, 3, TEXT("Building.Bakery"), HouseId, Bakery) ||
			(!bEnhanced && !AddBuilding(Initialization, 7, TEXT("Building.Brewery"), HouseId, Brewery)) ||
			!AddBuilding(Initialization, 9, TEXT("Building.Residence.Laborer"), HouseId, Residence) ||
			!AddBuilding(Initialization, 14, TEXT("Building.Market"), HouseId, MarketBuilding) ||
			!AddBuilding(Initialization, 15, TEXT("Building.Dock"), HouseId, Dock))
		{
			return false;
		}

		if (!Initialization.Placement.Maps.IsEmpty() &&
			(!AddPlacement(Initialization.Placement, *GrainFarm, CityId, Registry, 10, 20) ||
			 !AddPlacement(Initialization.Placement, *Mill, CityId, Registry, 15, 20) ||
			 !AddPlacement(Initialization.Placement, *Bakery, CityId, Registry, 19, 20) ||
			 (!bEnhanced && !AddPlacement(Initialization.Placement, *Brewery, CityId, Registry, 23, 20)) ||
			 !AddPlacement(Initialization.Placement, *Residence, CityId, Registry, 27, 20) ||
			 !AddPlacement(Initialization.Placement, *MarketBuilding, CityId, Registry, 10, 15) ||
			 !AddPlacement(Initialization.Placement, *Dock, CityId, Registry, 26, 15)))
		{
			return false;
		}
		if (!Initialization.Placement.Maps.IsEmpty())
		{
			uint64 RoadBuildingValue = 20;
			const auto AddRoadCell = [&](const int32 X, const int32 Y)
			{
				FHansaBuildingState* Road = nullptr;
				return AddBuilding(Initialization, RoadBuildingValue++, TEXT("Building.Road"), HouseId, Road) &&
					AddPlacement(Initialization.Placement, *Road, CityId, Registry, X, Y);
			};
			for (int32 X = 10; X <= 30; ++X) if (!AddRoadCell(X, 18)) return false;
			for (int32 X = 10; X <= 29; ++X) if (!AddRoadCell(X, 19)) return false;
			for (int32 Y = 20; Y <= 24; ++Y) if (!AddRoadCell(22, Y)) return false;
			for (int32 X = 23; X <= 30; ++X) if (!AddRoadCell(X, 24)) return false;
		}

		FHansaInventoryInitialization Inventory;
		if (!Entity(1, Inventory.Id)) return false;
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = CityId;
		Inventory.BuildingId = MarketBuilding->Id;
		Inventory.Capacity = FHansaQuantity::FromRaw(2'000'000);
		for (const FHansaCompiledGoodDefinition& GoodDefinition : Registry.GetGoods())
		{
			FHansaGoodId GoodId;
			if (!Assign(FHansaGoodId::TryParse(GoodDefinition.StableId), GoodId)) return false;
			Inventory.AcceptedGoods.Add(GoodId);
			const FHansaCompiledMarketGoodProfile* MarketProfile = CityMarket->Goods.FindByPredicate([&GoodDefinition](const auto& Profile)
			{
				return Profile.GoodId == GoodDefinition.StableId;
			});
			if (MarketProfile == nullptr) return false;
			// Keep every non-shortage good safely above its authored reserve so all ten rows receive
			// meaningful reports without manufacturing unrelated opening alerts.
			const int64 InitialStock = bEnhanced ? MarketProfile->InitialStockMilliUnits : GoodDefinition.StableId == TEXT("Good.Grain")
				? 16'000
				: MarketProfile->DesiredReserveMilliUnits + 10'000;
			Inventory.InitialStock.Add({ GoodId, FHansaQuantity::FromRaw(InitialStock) });
		}
		Initialization.Inventories.Add(MoveTemp(Inventory));
		if (!AddBuildingInventory(Initialization, *GrainFarm, 101, Registry) ||
			!AddBuildingInventory(Initialization, *Mill, 102, Registry) ||
			!AddBuildingInventory(Initialization, *Bakery, 103, Registry) ||
			(!bEnhanced && !AddBuildingInventory(Initialization, *Brewery, 107, Registry)))
		{
			return false;
		}

		if (!AddProduction(Initialization, 1, GrainFarm->Id, TEXT("Recipe.GrowGrain"), 8, 0, false) ||
			!AddProduction(Initialization, 2, Mill->Id, TEXT("Recipe.MillFlour"), 4, 1, true) ||
			!AddProduction(Initialization, 3, Bakery->Id, TEXT("Recipe.BakeBread"), 4, 2, true) ||
			(!bEnhanced && !AddProduction(Initialization, 7, Brewery->Id, TEXT("Recipe.BrewBeer"), 4, 2, true)))
		{
			return false;
		}

		FHansaPopulationCohortInitialization Cohort;
		if (!Entity(1, Cohort.Id) || !Entity(1, Cohort.ConsumptionInventoryId) ||
			!Assign(FHansaPopulationTierId::TryParse(TEXT("PopulationTier.Laborer")), Cohort.TierId))
		{
			return false;
		}
		Cohort.ResidenceBuildingId = Residence->Id;
		Cohort.CityId = CityId;
		Cohort.Residents = bEnhanced ? 10 : 12;
		Cohort.ResidenceCapacity = 12;
		Initialization.PopulationCohorts.Add(MoveTemp(Cohort));
		if (bEnhanced)
		{
			for (auto& Production : Initialization.Productions) Production.bUsesCityWorkforce = true;
			for (uint64 Id = 10; Id <= 12; ++Id)
			{
				const bool bArtisan = Id == 12;
				FHansaBuildingState* Home = nullptr;
				if (!AddBuilding(Initialization, Id, bArtisan ? TEXT("Building.Residence.Artisan") : TEXT("Building.Residence.Laborer"), HouseId, Home)) return false;
				if (!Initialization.Placement.Maps.IsEmpty() && !AddPlacement(Initialization.Placement, *Home, CityId, Registry, 23 + int32(Id - 10) * 3, 25)) return false;
				FHansaPopulationCohortInitialization NewCohort;
				if (!Entity(Id, NewCohort.Id) || !Entity(1, NewCohort.ConsumptionInventoryId) ||
					!Assign(FHansaPopulationTierId::TryParse(bArtisan ? TEXT("PopulationTier.Artisan") : TEXT("PopulationTier.Laborer")), NewCohort.TierId)) return false;
				NewCohort.ResidenceBuildingId = Home->Id;
				NewCohort.CityId = CityId;
				NewCohort.Residents = bArtisan ? 8 : 10;
				NewCohort.ResidenceCapacity = bArtisan ? 8 : 12;
				Initialization.PopulationCohorts.Add(NewCohort);
			}
		}


        // A completed, selectable home beside the starting road. Keep it empty
        // initially so adding an inspection target does not grant extra workforce.
        FHansaBuildingState* StarterHome = nullptr;
        if (!AddBuilding(Initialization, 13, TEXT("Building.Residence.Laborer"), HouseId, StarterHome) ||
            (!Initialization.Placement.Maps.IsEmpty() &&
             !AddPlacement(Initialization.Placement, *StarterHome, CityId, Registry, 15, 16))) return false;
        FHansaPopulationCohortInitialization StarterCohort;
        if (!Entity(13, StarterCohort.Id) || !Entity(1, StarterCohort.ConsumptionInventoryId) ||
            !Assign(FHansaPopulationTierId::TryParse(TEXT("PopulationTier.Laborer")), StarterCohort.TierId)) return false;
        StarterCohort.ResidenceBuildingId = StarterHome->Id;
        StarterCohort.CityId = CityId;
        StarterCohort.Residents = 0;
        StarterCohort.ResidenceCapacity = 12;
        Initialization.PopulationCohorts.Add(MoveTemp(StarterCohort));

		Initialization.MarketSettings.UpdateCadenceTicks = CityMarket->UpdateCadenceTicks;
		Initialization.MarketSettings.PriceHistoryCapacity = CityMarket->PriceHistoryCapacity;
		Initialization.MarketSettings.TargetSmoothingBasisPoints = CityMarket->TargetSmoothingBasisPoints;
		Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate = CityMarket->MaximumMovementBasisPointsPerUpdate;
		Initialization.MarketSettings.StaleAfterTicks = CityMarket->StaleAfterTicks;
		const auto CityInventoryId = FHansaInventoryId::TryCreate(1);
		if (!CityInventoryId) return false;
		for (const FHansaCompiledMarketGoodProfile& Profile : CityMarket->Goods)
		{
			FHansaCityMarketInitialization Market;
			if (!ConfigureMarket(Market, *CityMarket, Profile, CityId, CityInventoryId.Value)) return false;
			Initialization.Markets.Add(MoveTemp(Market));
		}
		uint64 RemoteInventoryValue=2;
		for(const auto& RemoteCity:Registry.GetCityMarkets())
		{
			if(!RemoteCity.bMarketOnly||RemoteCity.StableId==TEXT("City.Lubeck"))continue;
			if(!AddMarketOnlyCity(Initialization,Registry,*RemoteCity.StableId,RemoteInventoryValue++))return false;
		}
		return true;
	}

	bool BuildEmptyPlacementInitialization(
		FHansaSimulationInitialization& Initialization,
		const FHansaEconomicRegistry& Registry,
		const FHansaCityDefinitionId CityId)
	{
		FHansaInventoryInitialization Inventory;
		if (!Entity(1, Inventory.Id)) return false;
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = CityId;
		Inventory.Capacity = FHansaQuantity::FromRaw(FMath::Max<int64>(20'000'000,
			static_cast<int64>(Registry.GetGoods().Num()) * 1'000'000));
		for (const FHansaCompiledGoodDefinition& GoodDefinition : Registry.GetGoods())
		{
			FHansaGoodId GoodId;
			if (!Assign(FHansaGoodId::TryParse(GoodDefinition.StableId), GoodId)) return false;
			Inventory.AcceptedGoods.Add(GoodId);
			Inventory.InitialStock.Add({ GoodId, FHansaQuantity::FromRaw(1'000'000) });
		}
		Initialization.Inventories.Add(MoveTemp(Inventory));
		return true;
	}

}

bool FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(
	FHansaEconomicRegistry& OutRegistry,
	FString& OutError)
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (AssetManager == nullptr)
	{
		OutError = TEXT("The runtime Asset Manager is not initialized.");
		return false;
	}
	const FPrimaryAssetType Types[] = {
		TEXT("HansaGoodDefinition"), TEXT("HansaRecipeDefinition"), TEXT("HansaBuildingDefinition"),
		TEXT("HansaNeedDefinition"), TEXT("HansaPopulationTierDefinition"), TEXT("HansaCityMarketProfileDefinition"),
		TEXT("HansaProductionChainDefinition"), TEXT("HansaRegionEconomicProfileDefinition"),
		TEXT("HansaVehicleDefinition"), TEXT("HansaRouteDefinition"), TEXT("HansaTechnologyDefinition"),
		TEXT("HansaMerchantAITuningDefinition"), TEXT("HansaScenarioObjectiveDefinition"),
		TEXT("HansaVictoryDefinition"), TEXT("HansaScenarioDefinition"), TEXT("HansaResidentialCompoundDefinition"),
		TEXT("HansaPresenceCapabilityDefinition"), TEXT("HansaForeignPresenceStageDefinition"),
		TEXT("HansaCityTradePolicyDefinition")
	};
	bool bP33Candidate = false;
    bool bFirewoodCandidate = false;
    bool bArtisanCandidate = false;
	bool bTextileCandidate = false;
#if !UE_BUILD_SHIPPING
	bP33Candidate = FParse::Param(FCommandLine::Get(), TEXT("P33Candidate"));
    bFirewoodCandidate = FParse::Param(FCommandLine::Get(), TEXT("FirewoodCandidate"));
    bArtisanCandidate = FParse::Param(FCommandLine::Get(), TEXT("ArtisanProductionCandidate"));
	bTextileCandidate = FParse::Param(FCommandLine::Get(), TEXT("TextileProductionCandidate"));
    if (int32(bP33Candidate) + int32(bFirewoodCandidate) + int32(bArtisanCandidate) + int32(bTextileCandidate) > 1) { OutError = TEXT("Choose one candidate catalog."); return false; }
#endif
	TArray<const UHansaDefinitionBase*> Definitions;
#if !UE_BUILD_SHIPPING
	if (bTextileCandidate)
	{
		// A pinned snapshot owns its inventory. New Core definitions must not change it.
		const FName Root(TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2"));
		auto& CandidateAssets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		CandidateAssets.ScanPathsSynchronous({Root.ToString()}, true);
		TArray<FAssetData> Rows;
		CandidateAssets.GetAssetsByPath(Root, Rows, true);
		for (const FAssetData& Row : Rows)
		{
			if (const auto* Definition = Cast<UHansaDefinitionBase>(Row.GetAsset())) Definitions.Add(Definition);
		}
	}
#endif
	for (const FPrimaryAssetType& Type : Types)
	{
		if (bTextileCandidate) break;
		if ((bP33Candidate || bFirewoodCandidate || bArtisanCandidate) &&
			(Type == FPrimaryAssetType(TEXT("HansaPresenceCapabilityDefinition")) ||
			 Type == FPrimaryAssetType(TEXT("HansaForeignPresenceStageDefinition")) ||
			 Type == FPrimaryAssetType(TEXT("HansaCityTradePolicyDefinition")))) continue;
		TArray<FPrimaryAssetId> AssetIds;
		AssetManager->GetPrimaryAssetIdList(Type, AssetIds);
		AssetIds.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
		{
			return Left.ToString() < Right.ToString();
		});
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			const FSoftObjectPath AssetPath = AssetManager->GetPrimaryAssetPath(AssetId);
			const UHansaDefinitionBase* Definition = Cast<UHansaDefinitionBase>(AssetPath.TryLoad());
#if !UE_BUILD_SHIPPING
            if ((bP33Candidate || bFirewoodCandidate) && Definition &&
                (Definition->StableDefinitionId == TEXT("Good.Charcoal") || Definition->StableDefinitionId == TEXT("Good.RawHides") ||
                 Definition->StableDefinitionId == TEXT("Good.TanningBark") || Definition->StableDefinitionId == TEXT("Good.Leather") ||
                 Definition->StableDefinitionId == TEXT("Good.Shoes") || Definition->StableDefinitionId == TEXT("Recipe.BurnCharcoal") ||
                 Definition->StableDefinitionId == TEXT("Recipe.TanLeather") || Definition->StableDefinitionId == TEXT("Recipe.MakeShoes") ||
                 Definition->StableDefinitionId == TEXT("Building.CharcoalBurner") || Definition->StableDefinitionId == TEXT("Building.Tannery") ||
                 Definition->StableDefinitionId == TEXT("Building.Shoemaker") || Definition->StableDefinitionId == TEXT("Need.Shoes"))) continue;
            if (bArtisanCandidate && Definition)
            {
                const FString Name = TEXT("DA_") + Definition->StableDefinitionId.Replace(TEXT("."), TEXT("_"));
                Definition = Cast<UHansaDefinitionBase>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionV1/") + Name + TEXT(".") + Name).TryLoad());
            }
			if (bTextileCandidate && Definition)
			{
				const FString Name = TEXT("DA_") + Definition->StableDefinitionId.Replace(TEXT("."), TEXT("_"));
				Definition = Cast<UHansaDefinitionBase>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2/") + Name + TEXT(".") + Name).TryLoad());
			}
			if ((bP33Candidate || bFirewoodCandidate) && Definition)
			{
                // Existing staged catalogs predate preservation; keep their exact reviewed identities.
                if (bP33Candidate && (Definition->StableDefinitionId == TEXT("Good.Firewood") || Definition->StableDefinitionId == TEXT("Recipe.SplitFirewood") || Definition->StableDefinitionId == TEXT("Building.WoodcutterYard") || Definition->StableDefinitionId == TEXT("Need.Heating"))) continue;
                if (Definition->StableDefinitionId == TEXT("Good.PreservedFish") || Definition->StableDefinitionId == TEXT("Recipe.SaltedCatch") || Definition->StableDefinitionId == TEXT("Building.Fishery.SaltingShed")) continue;
				const FString Name = TEXT("DA_") + Definition->StableDefinitionId.Replace(TEXT("."), TEXT("_"));
				const FString CandidatePath = FString(bFirewoodCandidate ? TEXT("/Game/Hansa/Generated/Staging/Firewood/") : TEXT("/Game/Hansa/Generated/Staging/EconomyP33/")) + Name + TEXT(".") + Name;
				Definition = Cast<UHansaDefinitionBase>(FSoftObjectPath(CandidatePath).TryLoad());
			}
#endif
			if (Definition == nullptr)
			{
				OutError = FString::Printf(TEXT("Unable to load authored definition %s."), *AssetId.ToString());
				return false;
			}
			Definitions.Add(Definition);
		}
	}
#if !UE_BUILD_SHIPPING
    if (bArtisanCandidate)
    {
        for (const TCHAR* Id : {TEXT("Good.Charcoal"),TEXT("Good.RawHides"),TEXT("Good.TanningBark"),TEXT("Good.Leather"),TEXT("Good.Shoes"),
            TEXT("Recipe.BurnCharcoal"),TEXT("Recipe.TanLeather"),TEXT("Recipe.MakeShoes"),
            TEXT("Building.CharcoalBurner"),TEXT("Building.Tannery"),TEXT("Building.Shoemaker"),TEXT("Need.Shoes")})
        {
            const FString Name=TEXT("DA_")+FString(Id).Replace(TEXT("."),TEXT("_"));
            auto* D=Cast<UHansaDefinitionBase>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionV1/")+Name+TEXT(".")+Name).TryLoad());
            if (!D) { OutError=TEXT("Missing staged artisan definition: ")+FString(Id); return false; }
            Definitions.AddUnique(D);
        }
    }
	if (bTextileCandidate)
	{
		for (const TCHAR* Id : {TEXT("Good.Flax"),TEXT("Good.Hemp"),TEXT("Good.Beeswax"),TEXT("Good.LinenCloth"),TEXT("Good.LinenClothing"),TEXT("Good.Candles"),TEXT("Good.Rope"),
			TEXT("Recipe.WeaveLinen"),TEXT("Recipe.SewLinenClothing"),TEXT("Recipe.DipCandles"),TEXT("Recipe.LayHempRope"),TEXT("Recipe.LayFlaxRope"),
			TEXT("Building.Weaver"),TEXT("Building.Tailor"),TEXT("Building.Chandler"),TEXT("Building.Ropewalk"),TEXT("Need.LinenClothing"),TEXT("Need.Candles")})
		{
			const FString Name = TEXT("DA_") + FString(Id).Replace(TEXT("."), TEXT("_"));
			auto* Definition = Cast<UHansaDefinitionBase>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2/") + Name + TEXT(".") + Name).TryLoad());
			if (!Definition) { OutError = TEXT("Missing staged textile production definition: ") + FString(Id); return false; }
			Definitions.AddUnique(Definition);
		}
	}
    if (bFirewoodCandidate)
    {
        for (const TCHAR* Id : {TEXT("Good.Firewood"),TEXT("Recipe.SplitFirewood"),TEXT("Building.WoodcutterYard"),TEXT("Need.Heating")})
        {
            const FString Name = TEXT("DA_") + FString(Id).Replace(TEXT("."), TEXT("_"));
            auto* D = Cast<UHansaDefinitionBase>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/Firewood/") + Name + TEXT(".") + Name).TryLoad());
            if (!D) { OutError = TEXT("Missing staged firewood definition: ") + FString(Id); return false; }
            Definitions.AddUnique(D);
        }
    }
#endif
	// Keep the baseline completeness guard while allowing explicitly bound compound additions.
    int32 BaselineDefinitionCount = 0;
    for (const UHansaDefinitionBase* D : Definitions)
    {
        const auto* B = Cast<UHansaBuildingDefinition>(D);
        if (!D->IsA<UHansaResidentialCompoundDefinition>() && (!B || B->ResidentialCompound.IsNull())) ++BaselineDefinitionCount;
    }
    if (BaselineDefinitionCount != (bTextileCandidate ? 118 : bArtisanCandidate ? 100 : bFirewoodCandidate ? 85 : bP33Candidate ? 81 : 190))
	{
		OutError = FString::Printf(TEXT("The selected catalog has an unexpected number of definitions: %d (compound additions excluded)."), BaselineDefinitionCount);
		return false;
	}
	FHansaEconomicRegistryCompileResult Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
	if (!Compiled.IsValid())
	{
		TArray<FString> ErrorDescriptions;
		for (const FHansaDefinitionValidationIssue& Issue : Compiled.Issues)
		{
			if (Issue.Severity != EHansaDefinitionValidationSeverity::Error) continue;
			ErrorDescriptions.Add(FString::Printf(
				TEXT("%s at %s: %s"), *Issue.Code.ToString(), *Issue.PropertyPath, *Issue.Cause.ToString()));
		}
		OutError = FString::Printf(TEXT("The authored MVP economic registry is invalid: %s"),
			*FString::Join(ErrorDescriptions, TEXT("; ")));
		UE_LOG(LogHansa, Error, TEXT("%s"), *OutError);
		return false;
	}
	if (Compiled.Registry.GetRegistryHash() != (bTextileCandidate ? TextileProductionCandidateRegistryHash : bArtisanCandidate ? ArtisanProductionCandidateRegistryHash : bFirewoodCandidate ? FirewoodCandidateRegistryHash : bP33Candidate ? P33CandidateRegistryHash : MvpRegistryHash))
	{
		OutError = FString::Printf(
			TEXT("The loaded registry hash %016llX does not match %s expected hash %016llX (%d definitions). Run Hansa.Integration.Authoring.EconomicAssetReload for per-definition evidence."),
			static_cast<unsigned long long>(Compiled.Registry.GetRegistryHash()),
			bTextileCandidate ? TEXT("Textile production review candidate") : bArtisanCandidate ? TEXT("Artisan production review candidate") : bFirewoodCandidate ? TEXT("Firewood review candidate") : bP33Candidate ? TEXT("P33 review candidate") : TEXT("accepted MVP catalog"),
			static_cast<unsigned long long>(bTextileCandidate ? TextileProductionCandidateRegistryHash : bArtisanCandidate ? ArtisanProductionCandidateRegistryHash : bFirewoodCandidate ? FirewoodCandidateRegistryHash : bP33Candidate ? P33CandidateRegistryHash : MvpRegistryHash),Definitions.Num());
		return false;
	}
	OutRegistry = MoveTemp(Compiled.Registry);
	return true;
}

bool FHansaLubeckScenarioInitializer::TryCreate(
	const EHansaRuntimeScenario Scenario,
	FHansaEconomicRegistry Registry,
	FHansaPlacementInitialization Placement,
	FHansaLubeckScenarioState& OutState,
	FString& OutError,
	const uint64 CampaignSeedOverride,
    const bool bEmptyPlayerCity)
{
	FHansaSimulationInitialization Initialization;
	Initialization.Placement = MoveTemp(Placement);
	if (!InitializeCommon(Initialization, OutState.HouseId, OutState.RivalHouseId, OutState.HouseIds, OutState.CityId,
		Scenario == EHansaRuntimeScenario::LubeckGrainShortage ? 100'000 : 1'000'000))
	{
		OutError = TEXT("Unable to create the Lübeck scenario identities.");
		return false;
	}
	Initialization.CampaignSeed = CampaignSeedOverride != 0 ? CampaignSeedOverride :
		(Scenario == EHansaRuntimeScenario::LubeckGrainShortage
			? 0x4C554245434B4752ULL : 0x5330375030334255ULL);
	if (Scenario == EHansaRuntimeScenario::LubeckGrainShortage &&
		!BuildShortageInitialization(Initialization, Registry, OutState.HouseId, OutState.CityId))
	{
		OutError = TEXT("Unable to create the playable Lübeck shortage state.");
		return false;
	}
	if (Scenario == EHansaRuntimeScenario::EmptyLubeckBuild &&
		!BuildEmptyPlacementInitialization(Initialization, Registry, OutState.CityId))
	{
		OutError = TEXT("Unable to create the empty Lübeck placement stockpile.");
		return false;
	}
	if (Scenario == EHansaRuntimeScenario::LubeckGrainShortage &&
		!AddCanonicalTrade(Initialization, Registry, OutState.HouseIds, OutState.CityId))
	{
		OutError = TEXT("Unable to create the canonical cog and wagon routes.");
		return false;
	}

	if (bEmptyPlayerCity && Scenario == EHansaRuntimeScenario::LubeckGrainShortage)
    {
        // New Game starts with land and supplies. The historical shortage setup remains
        // available to explicit scenario fixtures; existing saves retain their own state.
        Initialization.Buildings.Reset();
        Initialization.Placement.Placements.Reset();
        Initialization.Productions.RemoveAll([](const auto& P) { return P.BuildingId.IsValid(); });
        Initialization.PopulationCohorts.Reset();
        Initialization.Inventories.RemoveAll([](const auto& I) {
            return I.OwnerKind == EHansaInventoryOwnerKind::Building || I.OwnerKind == EHansaInventoryOwnerKind::Warehouse;
        });
        for (auto& Inventory : Initialization.Inventories)
            if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City)
            {
                Inventory.BuildingId = FHansaBuildingId();
                if (Inventory.CityId == OutState.CityId)
                {
                    // New Game grants a construction stockpile across thirteen goods. Keep the
                    // shared city inventory large enough for that grant and the retuned costs.
                    Inventory.Capacity = FHansaQuantity::FromRaw(20'000'000);
                    for (auto& Stock : Inventory.InitialStock)
                        if (Stock.GoodId.ToString() == TEXT("Good.Planks"))
                            Stock.Quantity = FHansaQuantity::FromRaw(
                                Stock.Quantity.GetRawValue() * 4 + 1'000'000);
                        else if (Stock.GoodId.ToString() == TEXT("Good.Timber"))
                            Stock.Quantity = FHansaQuantity::FromRaw(
                                Stock.Quantity.GetRawValue() + 100'000);
                        else if (Stock.GoodId.ToString() == TEXT("Good.Tools"))
                            Stock.Quantity = FHansaQuantity::FromRaw(
                                Stock.Quantity.GetRawValue() + 1'000'000);
                }
            }
        Initialization.LocalLogisticsRequests.Reset();
	}

	if (!ConfigureEightHouseBuildableOpportunities(Initialization.Placement, OutState.HouseIds,
		OutState.CityId, OutState.StartingOpportunities))
	{
		OutError = TEXT("Unable to partition finite non-overlapping buildable opportunities for all eight houses.");
		return false;
	}

	FHansaScenarioId ScenarioId;
	const TCHAR* ScenarioStableId = Scenario == EHansaRuntimeScenario::LubeckGrainShortage
		? TEXT("Scenario.LubeckGrainShortageV1") : TEXT("Scenario.EmptyLubeckBuildV1");
	TMap<uint64, FString> DefinitionMigrations;
	DefinitionMigrations.Add(ImmediatePreviousMvpRegistryHash,
		TEXT("Hansa.Content.34To35.AddPresenceSpecializations"));
	DefinitionMigrations.Add(PreviousTradeStationSitesMvpRegistryHash,
		TEXT("Hansa.Content.33To34.AddTradeStationSites"));
	DefinitionMigrations.Add(PreviousTradePresenceMvpRegistryHash,
		TEXT("Hansa.Content.32To33.AddInertTradePresence"));
	// Catalogs 11 and 16 change consumption/staffing and staple batch economics. Do not silently
	// reinterpret in-flight production or old consumption history; preserve old saves on disk.
	auto CreatedTopology = FHansaPlacementTopology::TryCreate(MoveTemp(Initialization.Placement.Maps));
	if (!CreatedTopology ||
		!Assign(FHansaScenarioId::TryParse(ScenarioStableId), ScenarioId) ||
		!Assign(FHansaSimulationDefinitionContext::TryCreate(
			ScenarioId, Registry.GetRegistryHash(), MoveTemp(Registry), MoveTemp(CreatedTopology.Value),
			MoveTemp(DefinitionMigrations)), OutState.Definitions))
	{
		OutError = TEXT("Unable to validate the authoritative Lübeck scenario definitions.");
		return false;
	}
	if (bEmptyPlayerCity)
    {
        const auto* Map=OutState.Definitions.GetPlacementTopology()->FindMap(OutState.CityId);
        const bool Survey=Map && Map->BoundsMin.X<0;
        const auto Near=Survey?Hansa::Game::LubeckPlacementGrid::WorldToGrid(Hansa::Game::LubeckPlacementGrid::SurveyStartLocation()):
            Hansa::Game::LubeckPlacementGrid::WorldToGrid(FVector(2650,850,0));
        FHansaGridCoordinate Start;
        if (!Map || !FHansaWaterNavigation::FindStart(*Map,Near,Start))
        { OutError=TEXT("No navigable water near the starting waterfront.");return false; }
        for (auto& V:Initialization.Vehicles) if (V.OwnerId==OutState.HouseId && V.Mode==EHansaRouteMode::Sea)
        { V.Navigation.CityId=OutState.CityId;V.Navigation.Home=Start;V.Navigation.Cell=Start; }
    }
    const FHansaEconomicRegistry* EconomicRegistry = OutState.Definitions.GetEconomicRegistry();
    if (EconomicRegistry == nullptr ||
        !FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Initialization, *EconomicRegistry))
    { OutError=TEXT("Unable to seed authored foreign presence."); return false; }
    auto CreatedState = FHansaSimulationState::TryCreate(
		MoveTemp(Initialization), OutState.Definitions.GetPlacementTopologyShared());
	if (!CreatedState)
	{
		OutError = FString::Printf(TEXT("Unable to validate the authoritative Lübeck scenario state: %s."),
			LexToString(CreatedState.Error));
		return false;
	}
	OutState.State = MoveTemp(CreatedState.Value);
	OutState.NextBuildingId = 1;
	for (const auto& Building : OutState.State.CreateReadOnlyAccess(OutState.Definitions).GetBuildings())
		OutState.NextBuildingId = FMath::Max(OutState.NextBuildingId, Building.Id.GetValue() + 1);
	return true;
}
