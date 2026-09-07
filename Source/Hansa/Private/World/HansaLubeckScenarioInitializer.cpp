#include "World/HansaLubeckScenarioInitializer.h"

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Engine/AssetManager.h"
#include "HansaLog.h"
#include "Inventory/HansaInventory.h"
#include "Market/HansaMarket.h"
#include "Population/HansaPopulation.h"
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
			!Entity(1, Production.InputInventoryId) || !Entity(1, Production.OutputInventoryId))
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
		Initialization.Houses.Add({ OutHouseId, FHansaMoney::FromRaw(StartingMoney) });
		Initialization.Houses.Add({ OutRivalHouseId, FHansaMoney::FromRaw(50'000) });
		Initialization.Research.Add({ OutHouseId, 1'000 });
		Initialization.Research.Add({ OutRivalHouseId, 400 });
		Initialization.Cities.Add({ OutCityId, FHansaQuantity() });
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
		const FHansaEconomicRegistry& Registry, const FHansaHouseId HouseId, const FHansaHouseId RivalHouseId,
		const FHansaCityDefinitionId Lubeck)
	{
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
			bool bRival = false;
		};
		const FSpec Specs[] = {
			{ 1, 1001, 1, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea },
			{ 2, 1002, 2, TEXT("Vehicle.Wagon"), TEXT("Route.SaltRoad"), TEXT("City.Luneburg"), TEXT("Good.Salt"), EHansaRouteMode::Land },
			{ 3, 1003, 3, TEXT("Vehicle.Cog"), TEXT("Route.BalticSea"), TEXT("City.Rostock"), TEXT("Good.Grain"), EHansaRouteMode::Sea, true }
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
			const FHansaHouseId OwnerId = Spec.bRival ? RivalHouseId : HouseId;
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

		FHansaBuildingState* GrainFarm = nullptr;
		FHansaBuildingState* Mill = nullptr;
		FHansaBuildingState* Bakery = nullptr;
		FHansaBuildingState* Brewery = nullptr;
		FHansaBuildingState* Residence = nullptr;
		// AddBuilding returns pointers into this array. Reserve the complete scenario set so later adds cannot
		// invalidate those pointers before production and placement records are assembled.
		Initialization.Buildings.Reserve(5);
		if (!AddBuilding(Initialization, 1, TEXT("Building.GrainFarm"), HouseId, GrainFarm) ||
			!AddBuilding(Initialization, 2, TEXT("Building.Mill"), HouseId, Mill) ||
			!AddBuilding(Initialization, 3, TEXT("Building.Bakery"), HouseId, Bakery) ||
			!AddBuilding(Initialization, 7, TEXT("Building.Brewery"), HouseId, Brewery) ||
			!AddBuilding(Initialization, 9, TEXT("Building.Residence.Laborer"), HouseId, Residence))
		{
			return false;
		}

		if (!Initialization.Placement.Maps.IsEmpty() &&
			(!AddPlacement(Initialization.Placement, *GrainFarm, CityId, Registry, 10, 20) ||
			 !AddPlacement(Initialization.Placement, *Mill, CityId, Registry, 15, 20) ||
			 !AddPlacement(Initialization.Placement, *Bakery, CityId, Registry, 19, 20) ||
			 !AddPlacement(Initialization.Placement, *Brewery, CityId, Registry, 23, 20) ||
			 !AddPlacement(Initialization.Placement, *Residence, CityId, Registry, 27, 20)))
		{
			return false;
		}

		FHansaInventoryInitialization Inventory;
		if (!Entity(1, Inventory.Id)) return false;
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = CityId;
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
			const int64 InitialStock = GoodDefinition.StableId == TEXT("Good.Grain")
				? 16'000
				: MarketProfile->DesiredReserveMilliUnits + 10'000;
			Inventory.InitialStock.Add({ GoodId, FHansaQuantity::FromRaw(InitialStock) });
		}
		Initialization.Inventories.Add(MoveTemp(Inventory));

		if (!AddProduction(Initialization, 1, GrainFarm->Id, TEXT("Recipe.GrowGrain"), 8, 0, false) ||
			!AddProduction(Initialization, 2, Mill->Id, TEXT("Recipe.MillFlour"), 4, 1, true) ||
			!AddProduction(Initialization, 3, Bakery->Id, TEXT("Recipe.BakeBread"), 4, 2, true) ||
			!AddProduction(Initialization, 7, Brewery->Id, TEXT("Recipe.BrewBeer"), 4, 2, true))
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
		Cohort.Residents = 12;
		Cohort.ResidenceCapacity = 12;
		Initialization.PopulationCohorts.Add(MoveTemp(Cohort));

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
		return AddMarketOnlyCity(Initialization, Registry, TEXT("City.Hamburg"), 2) &&
			AddMarketOnlyCity(Initialization, Registry, TEXT("City.Luneburg"), 3) &&
			AddMarketOnlyCity(Initialization, Registry, TEXT("City.Rostock"), 4);
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
		Inventory.Capacity = FHansaQuantity::FromRaw(20'000'000);
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
		TEXT("HansaVehicleDefinition"), TEXT("HansaRouteDefinition"), TEXT("HansaTechnologyDefinition"),
		TEXT("HansaMerchantAITuningDefinition"), TEXT("HansaScenarioObjectiveDefinition"),
		TEXT("HansaVictoryDefinition"), TEXT("HansaScenarioDefinition")
	};
	TArray<const UHansaDefinitionBase*> Definitions;
	for (const FPrimaryAssetType& Type : Types)
	{
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
			if (Definition == nullptr)
			{
				OutError = FString::Printf(TEXT("Unable to load authored definition %s."), *AssetId.ToString());
				return false;
			}
			Definitions.Add(Definition);
		}
	}
	if (Definitions.Num() != 72)
	{
		OutError = FString::Printf(TEXT("Expected 72 cooked MVP definitions, including the authored scenario, objectives, victories, technologies and merchant AI tuning, but found %d."), Definitions.Num());
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
	if (Compiled.Registry.GetRegistryHash() != MvpRegistryHash)
	{
		OutError = FString::Printf(
			TEXT("The cooked MVP registry hash %016llX does not match reviewed runtime hash %016llX."),
			static_cast<unsigned long long>(Compiled.Registry.GetRegistryHash()),
			static_cast<unsigned long long>(MvpRegistryHash));
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
	const uint64 CampaignSeedOverride)
{
	FHansaSimulationInitialization Initialization;
	Initialization.Placement = MoveTemp(Placement);
	if (!InitializeCommon(Initialization, OutState.HouseId, OutState.RivalHouseId, OutState.CityId,
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
		!AddCanonicalTrade(Initialization, Registry, OutState.HouseId, OutState.RivalHouseId, OutState.CityId))
	{
		OutError = TEXT("Unable to create the canonical cog and wagon routes.");
		return false;
	}

	FHansaScenarioId ScenarioId;
	const TCHAR* ScenarioStableId = Scenario == EHansaRuntimeScenario::LubeckGrainShortage
		? TEXT("Scenario.LubeckGrainShortageV1") : TEXT("Scenario.EmptyLubeckBuildV1");
	if (!Assign(FHansaScenarioId::TryParse(ScenarioStableId), ScenarioId) ||
		!Assign(FHansaSimulationDefinitionContext::TryCreate(
			ScenarioId, Registry.GetRegistryHash(), MoveTemp(Registry)), OutState.Definitions))
	{
		OutError = TEXT("Unable to validate the authoritative Lübeck scenario definitions.");
		return false;
	}
	auto CreatedState = FHansaSimulationState::TryCreate(MoveTemp(Initialization));
	if (!CreatedState)
	{
		OutError = FString::Printf(TEXT("Unable to validate the authoritative Lübeck scenario state: %s."),
			LexToString(CreatedState.Error));
		return false;
	}
	OutState.State = MoveTemp(CreatedState.Value);
	OutState.NextBuildingId = Scenario == EHansaRuntimeScenario::LubeckGrainShortage ? 10 : 1;
	return true;
}
