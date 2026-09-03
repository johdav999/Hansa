#include "Fixtures/HansaProductionFixture.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"

#if WITH_HANSA_AUTOMATION
namespace Hansa::Simulation
{
	namespace ProductionFixture
	{
		template <typename TValue>
		bool Assign(const THansaValueResult<TValue>& Result, TValue& OutValue)
		{
			if (!Result)
			{
				return false;
			}
			OutValue = Result.Value;
			return true;
		}

		template <typename TId>
		bool Entity(const uint64 Value, TId& OutId)
		{
			return Assign(TId::TryCreate(Value), OutId);
		}

		bool Good(const TCHAR* Text, FHansaGoodId& OutId) { return Assign(FHansaGoodId::TryParse(Text), OutId); }
		bool Recipe(const TCHAR* Text, FHansaRecipeId& OutId) { return Assign(FHansaRecipeId::TryParse(Text), OutId); }
		bool BuildingType(const TCHAR* Text, FHansaBuildingTypeId& OutId) { return Assign(FHansaBuildingTypeId::TryParse(Text), OutId); }

		FHansaCompiledGoodAmount Amount(const TCHAR* GoodId, const int64 Quantity)
		{
			return { GoodId, Quantity };
		}

		FHansaCompiledRecipeDefinition RecipeDefinition(
			const TCHAR* StableId,
			TArray<FHansaCompiledGoodAmount> Inputs,
			TArray<FHansaCompiledGoodAmount> Outputs,
			const int32 CycleTicks,
			const int32 Laborers,
			const int32 Artisans)
		{
			FHansaCompiledRecipeDefinition Result;
			Result.StableId = StableId;
			Result.Inputs = MoveTemp(Inputs);
			Result.Outputs = MoveTemp(Outputs);
			Result.CycleTicks = CycleTicks;
			Result.LaborerWorkforce = Laborers;
			Result.ArtisanWorkforce = Artisans;
			Result.bDeclaredSource = Result.Inputs.IsEmpty();
			return Result;
		}

		FHansaCompiledBuildingDefinition BuildingDefinition(
			const TCHAR* StableId,
			const TCHAR* RecipeId,
			const int32 Laborers,
			const int32 Artisans)
		{
			FHansaCompiledBuildingDefinition Result;
			Result.StableId = StableId;
			Result.RecipeIds.Add(RecipeId);
			Result.LaborerWorkforce = Laborers;
			Result.ArtisanWorkforce = Artisans;
			return Result;
		}

		void ApplyConstructionPolicy(FHansaCompiledBuildingDefinition& Building)
		{
			Building.CancellationRefundBasisPoints = 5000;
			if (Building.StableId == TEXT("Building.Road"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 500) };
				Building.ConstructionCostPfennig = 25;
				Building.BuildTicks = 5;
			}
			else if (Building.StableId == TEXT("Building.Residence.Laborer"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 2000) };
				Building.ConstructionCostPfennig = 800;
				Building.BuildTicks = 90;
				Building.FootprintWidthCells = 2;
				Building.FootprintHeightCells = 2;
				Building.ResidenceCapacity = 12;
				Building.ResidentPopulationTierId = TEXT("PopulationTier.Laborer");
				Building.UpgradeTargetBuildingId = TEXT("Building.Residence.Artisan");
			}
			else if (Building.StableId == TEXT("Building.Residence.Artisan"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Planks"), 5000), Amount(TEXT("Good.Tools"), 1000) };
				Building.ConstructionCostPfennig = 1400;
				Building.BuildTicks = 140;
				Building.FootprintWidthCells = 2;
				Building.FootprintHeightCells = 2;
				Building.ResidenceCapacity = 8;
				Building.ResidentPopulationTierId = TEXT("PopulationTier.Artisan");
			}
			else if (Building.StableId == TEXT("Building.Market"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Planks"), 6000), Amount(TEXT("Good.Tools"), 1000) };
				Building.ConstructionCostPfennig = 1800;
				Building.BuildTicks = 180;
			}
			else if (Building.StableId == TEXT("Building.Warehouse"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Planks"), 10000), Amount(TEXT("Good.Tools"), 2000) };
				Building.ConstructionCostPfennig = 2500;
				Building.BuildTicks = 220;
			}
			else if (Building.StableId == TEXT("Building.Dock"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 12000), Amount(TEXT("Good.Planks"), 8000), Amount(TEXT("Good.Tools"), 2000) };
				Building.ConstructionCostPfennig = 4000;
				Building.BuildTicks = 300;
			}
			else if (Building.StableId == TEXT("Building.GrainFarm"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 3000), Amount(TEXT("Good.Tools"), 500) };
				Building.ConstructionCostPfennig = 1200;
				Building.BuildTicks = 150;
			}
			else if (Building.StableId == TEXT("Building.Mill"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 3000), Amount(TEXT("Good.Tools"), 1000) };
				Building.ConstructionCostPfennig = 1600;
				Building.BuildTicks = 170;
			}
			else if (Building.StableId == TEXT("Building.Bakery"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Planks"), 4000), Amount(TEXT("Good.Tools"), 1000) };
				Building.ConstructionCostPfennig = 1400;
				Building.BuildTicks = 150;
			}
			else if (Building.StableId == TEXT("Building.Fishery"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 5000), Amount(TEXT("Good.Planks"), 2000), Amount(TEXT("Good.Tools"), 500) };
				Building.ConstructionCostPfennig = 1500;
				Building.BuildTicks = 150;
			}
			else if (Building.StableId == TEXT("Building.LumberCamp"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 2000), Amount(TEXT("Good.Tools"), 500) };
				Building.ConstructionCostPfennig = 900;
				Building.BuildTicks = 120;
			}
			else if (Building.StableId == TEXT("Building.Sawmill"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Timber"), 5000), Amount(TEXT("Good.Tools"), 1000) };
				Building.ConstructionCostPfennig = 1700;
				Building.BuildTicks = 180;
			}
			else if (Building.StableId == TEXT("Building.Smithy"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Planks"), 5000), Amount(TEXT("Good.Iron"), 3000) };
				Building.ConstructionCostPfennig = 2400;
				Building.BuildTicks = 220;
			}
			else if (Building.StableId == TEXT("Building.Brewery"))
			{
				Building.ConstructionCosts = { Amount(TEXT("Good.Planks"), 6000), Amount(TEXT("Good.Tools"), 1500) };
				Building.ConstructionCostPfennig = 2200;
				Building.BuildTicks = 220;
			}
		}

		FHansaCompiledMarketGoodProfile MarketGood(const TCHAR* GoodId, const int64 BasePrice,
			const int64 Reserve, const int64 Incoming = 0, const int32 CityModifier = 0)
		{
			FHansaCompiledMarketGoodProfile Result;
			Result.GoodId = GoodId;
			Result.DesiredReserveMilliUnits = Reserve;
			Result.ConfirmedIncomingSupplyMilliUnits = Incoming;
			Result.CityModifierBasisPoints = CityModifier;
			Result.MinimumPriceMilliMarks = FMath::Max<int64>(1, BasePrice / 2);
			Result.MaximumPriceMilliMarks = BasePrice * 4;
			Result.InitialPriceMilliMarks = BasePrice;
			return Result;
		}

		FHansaCompiledCityMarketProfileDefinition CityMarket(const TCHAR* CityName)
		{
			const FString City(CityName);
			const bool bLuneburg = City == TEXT("Luneburg");
			const bool bRostock = City == TEXT("Rostock");
			const bool bHamburg = City == TEXT("Hamburg");
			FHansaCompiledCityMarketProfileDefinition Result;
			Result.StableId = FString::Printf(TEXT("City.%s"), CityName);
			Result.UpdateCadenceTicks = 5;
			Result.PriceHistoryCapacity = 64;
			Result.TargetSmoothingBasisPoints = 2500;
			Result.MaximumMovementBasisPointsPerUpdate = 1000;
			Result.StaleAfterTicks = 10;
			Result.Goods = {
				MarketGood(TEXT("Good.Grain"), 1000, 30000, bRostock ? 4000 : 0),
				MarketGood(TEXT("Good.Flour"), 1700, 20000),
				MarketGood(TEXT("Good.Bread"), 800, 24000, bHamburg ? 2000 : 0),
				MarketGood(TEXT("Good.Fish"), 1800, 18000, (bHamburg || bRostock) ? 3000 : 0),
				MarketGood(TEXT("Good.Salt"), 2200, 12000, bLuneburg ? 5000 : 0, bLuneburg ? -500 : 0),
				MarketGood(TEXT("Good.Timber"), 700, 24000),
				MarketGood(TEXT("Good.Planks"), 1300, 18000),
				MarketGood(TEXT("Good.Iron"), 2600, 10000),
				MarketGood(TEXT("Good.Tools"), 6500, 8000),
				MarketGood(TEXT("Good.Beer"), 1500, 16000)
			};
			return Result;
		}

		FHansaEconomicRegistry BuildRegistry()
		{
			TArray<FHansaCompiledGoodDefinition> Goods;
			for (const TCHAR* StableId : {
				TEXT("Good.Beer"), TEXT("Good.Bread"), TEXT("Good.Fish"), TEXT("Good.Flour"), TEXT("Good.Grain"),
				TEXT("Good.Iron"), TEXT("Good.Planks"), TEXT("Good.Salt"), TEXT("Good.Timber"), TEXT("Good.Tools") })
			{
				FHansaCompiledGoodDefinition Definition;
				Definition.StableId = StableId;
				Definition.Unit = TEXT("milli-unit");
				if (Definition.StableId == TEXT("Good.Beer")) Definition.BaseValueMilliMarks = 1500;
				else if (Definition.StableId == TEXT("Good.Bread")) Definition.BaseValueMilliMarks = 800;
				else if (Definition.StableId == TEXT("Good.Fish")) Definition.BaseValueMilliMarks = 1800;
				else if (Definition.StableId == TEXT("Good.Flour")) Definition.BaseValueMilliMarks = 1700;
				else if (Definition.StableId == TEXT("Good.Grain")) Definition.BaseValueMilliMarks = 1000;
				else if (Definition.StableId == TEXT("Good.Iron")) Definition.BaseValueMilliMarks = 2600;
				else if (Definition.StableId == TEXT("Good.Planks")) Definition.BaseValueMilliMarks = 1300;
				else if (Definition.StableId == TEXT("Good.Salt")) Definition.BaseValueMilliMarks = 2200;
				else if (Definition.StableId == TEXT("Good.Timber")) Definition.BaseValueMilliMarks = 700;
				else if (Definition.StableId == TEXT("Good.Tools")) Definition.BaseValueMilliMarks = 6500;
				Goods.Add(MoveTemp(Definition));
			}

			TArray<FHansaCompiledRecipeDefinition> Recipes = {
				RecipeDefinition(TEXT("Recipe.BakeBread"), { Amount(TEXT("Good.Flour"), 2'000) }, { Amount(TEXT("Good.Bread"), 3'000) }, 2, 4, 2),
				RecipeDefinition(TEXT("Recipe.BrewBeer"), { Amount(TEXT("Good.Grain"), 3'000) }, { Amount(TEXT("Good.Beer"), 5'000) }, 2, 4, 2),
				RecipeDefinition(TEXT("Recipe.CatchFish"), {}, { Amount(TEXT("Good.Fish"), 4'000) }, 2, 8, 0),
				RecipeDefinition(TEXT("Recipe.FellTimber"), {}, { Amount(TEXT("Good.Timber"), 6'000) }, 2, 8, 0),
				RecipeDefinition(TEXT("Recipe.GrowGrain"), {}, { Amount(TEXT("Good.Grain"), 6'000) }, 3, 8, 0),
				RecipeDefinition(TEXT("Recipe.MillFlour"), { Amount(TEXT("Good.Grain"), 4'000) }, { Amount(TEXT("Good.Flour"), 3'000) }, 2, 4, 1),
				RecipeDefinition(TEXT("Recipe.SawPlanks"), { Amount(TEXT("Good.Timber"), 5'000) }, { Amount(TEXT("Good.Planks"), 3'500) }, 2, 6, 1),
				RecipeDefinition(TEXT("Recipe.SmithTools"), { Amount(TEXT("Good.Iron"), 3'000) }, { Amount(TEXT("Good.Tools"), 1'000) }, 2, 4, 4)
			};
			TArray<FHansaCompiledBuildingDefinition> Buildings = {
				BuildingDefinition(TEXT("Building.Bakery"), TEXT("Recipe.BakeBread"), 4, 2),
				BuildingDefinition(TEXT("Building.Brewery"), TEXT("Recipe.BrewBeer"), 4, 2),
				BuildingDefinition(TEXT("Building.Fishery"), TEXT("Recipe.CatchFish"), 8, 0),
				BuildingDefinition(TEXT("Building.GrainFarm"), TEXT("Recipe.GrowGrain"), 8, 0),
				BuildingDefinition(TEXT("Building.LumberCamp"), TEXT("Recipe.FellTimber"), 8, 0),
				BuildingDefinition(TEXT("Building.Mill"), TEXT("Recipe.MillFlour"), 4, 1),
				BuildingDefinition(TEXT("Building.Sawmill"), TEXT("Recipe.SawPlanks"), 6, 1),
				BuildingDefinition(TEXT("Building.Smithy"), TEXT("Recipe.SmithTools"), 4, 4)
			};
			for (const TCHAR* StableId : {
				TEXT("Building.Dock"), TEXT("Building.Market"), TEXT("Building.Residence.Artisan"),
				TEXT("Building.Residence.Laborer"), TEXT("Building.Road"), TEXT("Building.Warehouse") })
			{
				FHansaCompiledBuildingDefinition Definition;
				Definition.StableId = StableId;
				Buildings.Add(MoveTemp(Definition));
			}
			for (FHansaCompiledBuildingDefinition& Building : Buildings)
			{
				ApplyConstructionPolicy(Building);
			}
			TArray<FHansaCompiledNeedDefinition> Needs = {
				{ TEXT("Need.BasicServices"), EHansaCompiledNeedKind::Service, TEXT(""), 0 },
				{ TEXT("Need.Beer"), EHansaCompiledNeedKind::Good, TEXT("Good.Beer"), 0 },
				{ TEXT("Need.Bread"), EHansaCompiledNeedKind::Good, TEXT("Good.Bread"), 0 },
				{ TEXT("Need.Fish"), EHansaCompiledNeedKind::Good, TEXT("Good.Fish"), 0 },
				{ TEXT("Need.Grain"), EHansaCompiledNeedKind::Good, TEXT("Good.Grain"), 0 },
				{ TEXT("Need.Tools"), EHansaCompiledNeedKind::Good, TEXT("Good.Tools"), 0 }
			};
			FHansaCompiledPopulationTierDefinition Laborer;
			Laborer.StableId = TEXT("PopulationTier.Laborer");
			Laborer.Needs = { { TEXT("Need.BasicServices"), 0, 1500 }, { TEXT("Need.Beer"), 40, 1000 },
				{ TEXT("Need.Bread"), 100, 3500 }, { TEXT("Need.Fish"), 60, 2000 },
				{ TEXT("Need.Grain"), 100, 2000 } };
			Laborer.WorkforcePerResidentBasisPoints = 6000;
			Laborer.GrowthSatisfactionBasisPoints = 8000;
			Laborer.DeclineSatisfactionBasisPoints = 3500;
			Laborer.EvaluationTicks = 60;
			Laborer.GrowthResidentsPerEvaluation = 1;
			Laborer.DeclineResidentsPerEvaluation = 1;
			FHansaCompiledPopulationTierDefinition Artisan;
			Artisan.StableId = TEXT("PopulationTier.Artisan");
			Artisan.PreviousTierId = TEXT("PopulationTier.Laborer");
			Artisan.Needs = { { TEXT("Need.BasicServices"), 0, 2000 }, { TEXT("Need.Beer"), 70, 2000 },
				{ TEXT("Need.Bread"), 140, 3000 }, { TEXT("Need.Fish"), 60, 1500 }, { TEXT("Need.Tools"), 20, 1500 } };
			Artisan.WorkforcePerResidentBasisPoints = 7000;
			Artisan.GrowthSatisfactionBasisPoints = 8500;
			Artisan.DeclineSatisfactionBasisPoints = 4000;
			Artisan.EvaluationTicks = 60;
			Artisan.GrowthResidentsPerEvaluation = 1;
			Artisan.DeclineResidentsPerEvaluation = 1;
			TArray<FHansaCompiledPopulationTierDefinition> Tiers { MoveTemp(Laborer), MoveTemp(Artisan) };
			TArray<FHansaCompiledCityMarketProfileDefinition> CityMarkets {
				CityMarket(TEXT("Hamburg")), CityMarket(TEXT("Lubeck")),
				CityMarket(TEXT("Luneburg")), CityMarket(TEXT("Rostock"))
			};
			return FHansaEconomicRegistry(MoveTemp(Goods), MoveTemp(Recipes), MoveTemp(Buildings),
				FHansaProductionFixture::RegistryHash, MoveTemp(Needs), MoveTemp(Tiers), MoveTemp(CityMarkets));
		}

		bool AddBuildingProduction(
			FHansaSimulationInitialization& Initialization,
			const uint64 Value,
			const TCHAR* RecipeId,
			const int32 Laborers,
			const int32 Artisans)
		{
			FHansaProductionInitialization Production;
			if (!Entity(Value, Production.Id) || !Entity(Value, Production.BuildingId) ||
				!Recipe(RecipeId, Production.RecipeId) ||
				!Entity(1, Production.InputInventoryId) || !Entity(1, Production.OutputInventoryId))
			{
				return false;
			}
			Production.AllocatedLaborerWorkforce = Laborers;
			Production.AllocatedArtisanWorkforce = Artisans;
			Initialization.Productions.Add(MoveTemp(Production));
			return true;
		}

		FString Hex64(const uint64 Value)
		{
			return FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Value));
		}
	}

	THansaValueResult<FHansaProductionFixture> FHansaProductionFixture::TryCreate()
	{
		using namespace ProductionFixture;
		FHansaProductionFixture Fixture;
		FHansaScenarioId Scenario;
		if (!Assign(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")), Scenario) ||
			!Assign(FHansaSimulationDefinitionContext::TryCreate(Scenario, RegistryHash, BuildRegistry()), Fixture.Definitions))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}

		FHansaSimulationInitialization Initialization;
		FHansaSimulationVersion Version;
		FHansaSimulationTick Tick;
		if (!Assign(FHansaSimulationVersion::TryCreate(1), Version) ||
			!Assign(FHansaSimulationTick::TryCreate(0), Tick) ||
			!Assign(FHansaSimulationClock::TryCreate(Version, Tick), Initialization.Clock))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Initialization.CampaignSeed = 0x33445566;
		FHansaHouseId House;
		FHansaCityDefinitionId City;
		if (!Entity(1, House) || !Assign(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")), City))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Initialization.Houses.Add({ House, FHansaMoney::FromRaw(100'000) });
		Initialization.Cities.Add({ City, FHansaQuantity() });

		const TCHAR* BuildingIds[] = {
			TEXT("Building.GrainFarm"), TEXT("Building.Mill"), TEXT("Building.Bakery"), TEXT("Building.LumberCamp"),
			TEXT("Building.Sawmill"), TEXT("Building.Smithy"), TEXT("Building.Brewery"), TEXT("Building.Fishery")
		};
		for (uint64 Index = 0; Index < UE_ARRAY_COUNT(BuildingIds); ++Index)
		{
			FHansaBuildingId Building;
			FHansaBuildingTypeId Type;
			if (!Entity(Index + 1, Building) || !BuildingType(BuildingIds[Index], Type))
			{
				return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
			}
			FHansaBuildingState BuildingState;
			BuildingState.Id = Building;
			BuildingState.DefinitionId = Type;
			BuildingState.OwnerId = House;
			BuildingState.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
			BuildingState.ConstructionState = EHansaConstructionState::Completed;
			Initialization.Buildings.Add(BuildingState);
		}

		FHansaInventoryInitialization Inventory;
		if (!Entity(1, Inventory.Id))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(2'000'000);
		for (const TCHAR* GoodId : {
			TEXT("Good.Beer"), TEXT("Good.Bread"), TEXT("Good.Fish"), TEXT("Good.Flour"), TEXT("Good.Grain"),
			TEXT("Good.Iron"), TEXT("Good.Planks"), TEXT("Good.Salt"), TEXT("Good.Timber"), TEXT("Good.Tools") })
		{
			FHansaGoodId Parsed;
			if (!Good(GoodId, Parsed))
			{
				return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
			}
			Inventory.AcceptedGoods.Add(Parsed);
		}
		FHansaGoodId Iron;
		Good(TEXT("Good.Iron"), Iron);
		Inventory.InitialStock.Add({ Iron, FHansaQuantity::FromRaw(60'000) });
		Initialization.Inventories.Add(MoveTemp(Inventory));

		if (!AddBuildingProduction(Initialization, 1, TEXT("Recipe.GrowGrain"), 8, 0) ||
			!AddBuildingProduction(Initialization, 2, TEXT("Recipe.MillFlour"), 4, 1) ||
			!AddBuildingProduction(Initialization, 3, TEXT("Recipe.BakeBread"), 4, 2) ||
			!AddBuildingProduction(Initialization, 4, TEXT("Recipe.FellTimber"), 8, 0) ||
			!AddBuildingProduction(Initialization, 5, TEXT("Recipe.SawPlanks"), 6, 1) ||
			!AddBuildingProduction(Initialization, 6, TEXT("Recipe.SmithTools"), 4, 4) ||
			!AddBuildingProduction(Initialization, 7, TEXT("Recipe.BrewBeer"), 4, 2) ||
			!AddBuildingProduction(Initialization, 8, TEXT("Recipe.CatchFish"), 8, 0))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		FHansaProductionInitialization Salt;
		if (!Entity(9, Salt.Id) || !Entity(1, Salt.OutputInventoryId) ||
			!Good(TEXT("Good.Salt"), Salt.SupplyGoodId))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Salt.Kind = EHansaProductionKind::BackgroundSupply;
		Salt.CityId = City;
		Salt.SupplyQuantityPerCycle = FHansaQuantity::FromRaw(2'000);
		Salt.SupplyCycleTicks = 3;
		Initialization.Productions.Add(Salt);

		if (!Assign(FHansaSimulationState::TryCreate(MoveTemp(Initialization)), Fixture.State))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		return THansaValueResult<FHansaProductionFixture>::Success(Fixture);
	}

	THansaValueResult<FHansaProductionFixture> FHansaProductionFixture::TryCreateGrainShortage()
	{
		using namespace ProductionFixture;
		FHansaProductionFixture Fixture;
		Fixture.FixtureId = GrainShortageFixtureId;
		FHansaScenarioId Scenario;
		if (!Assign(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")), Scenario) ||
			!Assign(FHansaSimulationDefinitionContext::TryCreate(Scenario, RegistryHash, BuildRegistry()), Fixture.Definitions))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}

		FHansaSimulationInitialization Initialization;
		FHansaSimulationVersion Version;
		FHansaSimulationTick Tick;
		if (!Assign(FHansaSimulationVersion::TryCreate(1), Version) ||
			!Assign(FHansaSimulationTick::TryCreate(0), Tick) ||
			!Assign(FHansaSimulationClock::TryCreate(Version, Tick), Initialization.Clock))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Initialization.CampaignSeed = 0x4C554245434B4752ULL;
		Initialization.MarketSettings.UpdateCadenceTicks = 5;
		Initialization.MarketSettings.PriceHistoryCapacity = 64;
		Initialization.MarketSettings.TargetSmoothingBasisPoints = 2500;
		Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate = 1000;
		Initialization.MarketSettings.StaleAfterTicks = 10;

		FHansaHouseId House;
		FHansaCityDefinitionId City;
		if (!Entity(1, House) || !Assign(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")), City))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Initialization.Houses.Add({ House, FHansaMoney::FromRaw(100'000) });
		Initialization.Cities.Add({ City, FHansaQuantity() });

		const TCHAR* BuildingIds[] = {
			TEXT("Building.GrainFarm"), TEXT("Building.Mill"), TEXT("Building.Bakery"), TEXT("Building.LumberCamp"),
			TEXT("Building.Sawmill"), TEXT("Building.Smithy"), TEXT("Building.Brewery"), TEXT("Building.Fishery"),
			TEXT("Building.Residence.Laborer")
		};
		for (uint64 Index = 0; Index < UE_ARRAY_COUNT(BuildingIds); ++Index)
		{
			FHansaBuildingId Building;
			FHansaBuildingTypeId Type;
			if (!Entity(Index + 1, Building) || !BuildingType(BuildingIds[Index], Type))
			{
				return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
			}
			FHansaBuildingState BuildingState;
			BuildingState.Id = Building;
			BuildingState.DefinitionId = Type;
			BuildingState.OwnerId = House;
			BuildingState.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
			BuildingState.ConstructionState = EHansaConstructionState::Completed;
			Initialization.Buildings.Add(BuildingState);
		}

		FHansaInventoryInitialization Inventory;
		if (!Entity(1, Inventory.Id))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(2'000'000);
		for (const TCHAR* GoodText : {
			TEXT("Good.Beer"), TEXT("Good.Bread"), TEXT("Good.Fish"), TEXT("Good.Flour"), TEXT("Good.Grain"),
			TEXT("Good.Iron"), TEXT("Good.Planks"), TEXT("Good.Salt"), TEXT("Good.Timber"), TEXT("Good.Tools") })
		{
			FHansaGoodId Parsed;
			if (!Good(GoodText, Parsed))
			{
				return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
			}
			Inventory.AcceptedGoods.Add(Parsed);
		}
		FHansaGoodId Grain;
		FHansaGoodId Bread;
		FHansaGoodId Beer;
		FHansaGoodId Fish;
		if (!Good(TEXT("Good.Grain"), Grain) || !Good(TEXT("Good.Bread"), Bread) ||
			!Good(TEXT("Good.Beer"), Beer) || !Good(TEXT("Good.Fish"), Fish))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Inventory.InitialStock.Add({ Grain, FHansaQuantity::FromRaw(16'000) });
		Inventory.InitialStock.Add({ Bread, FHansaQuantity::FromRaw(30'000) });
		Inventory.InitialStock.Add({ Beer, FHansaQuantity::FromRaw(20'000) });
		Inventory.InitialStock.Add({ Fish, FHansaQuantity::FromRaw(20'000) });
		Initialization.Inventories.Add(MoveTemp(Inventory));

		if (!AddBuildingProduction(Initialization, 1, TEXT("Recipe.GrowGrain"), 8, 0) ||
			!AddBuildingProduction(Initialization, 2, TEXT("Recipe.MillFlour"), 4, 1) ||
			!AddBuildingProduction(Initialization, 7, TEXT("Recipe.BrewBeer"), 4, 2))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Initialization.Productions[0].bActive = false;

		FHansaProductionInitialization RecoverySupply;
		if (!Entity(10, RecoverySupply.Id) || !Entity(1, RecoverySupply.OutputInventoryId))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		RecoverySupply.Kind = EHansaProductionKind::BackgroundSupply;
		RecoverySupply.CityId = City;
		RecoverySupply.SupplyGoodId = Grain;
		RecoverySupply.SupplyQuantityPerCycle = FHansaQuantity::FromRaw(80'000);
		RecoverySupply.SupplyCycleTicks = 1;
		RecoverySupply.bActive = false;
		Initialization.Productions.Add(MoveTemp(RecoverySupply));

		FHansaPopulationCohortInitialization Cohort;
		FHansaPopulationTierId Tier;
		if (!Entity(1, Cohort.Id) || !Entity(9, Cohort.ResidenceBuildingId) ||
			!Entity(1, Cohort.ConsumptionInventoryId) ||
			!Assign(FHansaPopulationTierId::TryParse(TEXT("PopulationTier.Laborer")), Tier))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		Cohort.CityId = City;
		Cohort.TierId = Tier;
		Cohort.Residents = 12;
		Cohort.ResidenceCapacity = 12;
		Initialization.PopulationCohorts.Add(MoveTemp(Cohort));

		FHansaCityMarketInitialization Market;
		Market.CityId = City;
		Market.GoodId = Grain;
		Market.InventoryIds.Add(FHansaInventoryId::TryCreate(1).Value);
		Market.DesiredReserve = FHansaQuantity::FromRaw(30'000);
		Market.MinimumPriceMilliMarks = 500;
		Market.MaximumPriceMilliMarks = 4'000;
		Market.InitialPriceMilliMarks = 1'000;
		Initialization.Markets.Add(MoveTemp(Market));

		if (!Assign(FHansaSimulationState::TryCreate(MoveTemp(Initialization)), Fixture.State))
		{
			return THansaValueResult<FHansaProductionFixture>::Failure(EHansaValueError::InvalidFormat);
		}
		return THansaValueResult<FHansaProductionFixture>::Success(Fixture);
	}

	FHansaStateHashReport FHansaProductionFixture::BuildStateHashes() const
	{
		return State.CreateReadOnlyAccess(Definitions).BuildStateHashReport();
	}

	THansaValueResult<FHansaSimulationProjection> FHansaProductionFixture::BuildProjection() const
	{
		return State.CreateReadOnlyAccess(Definitions).BuildProjection();
	}

	FHansaCommandGatewayResult FHansaProductionFixture::Step(const int32 TickCount)
	{
		FHansaCommandGatewayResult Last;
		if (TickCount <= 0 || TickCount > 10'000)
		{
			return Last;
		}
		for (int32 Index = 0; Index < TickCount; ++Index)
		{
			Last = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache);
			if (!Last)
			{
				return Last;
			}
			Events.Append(Last.GetEvents());
		}
		return Last;
	}

	FHansaCommandGatewayResult FHansaProductionFixture::SetProductionActive(
		const FHansaProductionId ProductionId,
		const bool bActive)
	{
		FHansaCommandGatewayResult Invalid;
		if (!ProductionId.IsValid())
		{
			return Invalid;
		}
		const FHansaSimulationReadOnlyAccess ReadOnly = State.CreateReadOnlyAccess(Definitions);
		const auto CommandId = FHansaCommandId::TryCreate(ReadOnly.GetLastProcessedCommandId().IsValid()
			? ReadOnly.GetLastProcessedCommandId().GetValue() + 1 : 1);
		const auto HouseId = FHansaHouseId::TryCreate(1);
		if (!CommandId || !HouseId)
		{
			return Invalid;
		}
		FHansaCommandHeader Header;
		Header.CommandId = CommandId.Value;
		Header.Authority.IssuingHouseId = HouseId.Value;
		Header.Authority.PrincipalId = 0x4C554245434BULL;
		Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
			Header, FHansaSetProductionActiveCommand { ProductionId, bActive });
		const TArray<FHansaGameplayCommand> Commands { Command };
		FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
		if (Result)
		{
			Events.Append(Result.GetEvents());
		}
		return Result;
	}

	FHansaCommandGatewayResult FHansaProductionFixture::UpgradeResidence(const FHansaBuildingId BuildingId)
	{
		FHansaCommandGatewayResult Invalid;
		if (!BuildingId.IsValid())
		{
			return Invalid;
		}
		const FHansaSimulationReadOnlyAccess ReadOnly = State.CreateReadOnlyAccess(Definitions);
		const auto CommandId = FHansaCommandId::TryCreate(ReadOnly.GetLastProcessedCommandId().IsValid()
			? ReadOnly.GetLastProcessedCommandId().GetValue() + 1 : 1);
		const auto HouseId = FHansaHouseId::TryCreate(1);
		if (!CommandId || !HouseId)
		{
			return Invalid;
		}
		FHansaCommandHeader Header;
		Header.CommandId = CommandId.Value;
		Header.Authority.IssuingHouseId = HouseId.Value;
		Header.Authority.PrincipalId = 0x4C554245434BULL;
		Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		const TArray<FHansaGameplayCommand> Commands {
			FHansaGameplayCommand::Create(Header, FHansaUpgradeResidenceCommand { BuildingId })
		};
		FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
			State, Definitions, Commands, Cache);
		if (Result)
		{
			Events.Append(Result.GetEvents());
		}
		return Result;
	}

	FString FHansaProductionEvidenceWriter::WriteJson(
		const FHansaProductionFixture& Fixture,
		const FHansaStateHashReport& InitialState,
		const int32 TicksRun)
	{
		using namespace ProductionFixture;
		const FHansaStateHashReport FinalState = Fixture.BuildStateHashes();
		const THansaValueResult<FHansaSimulationProjection> Projection = Fixture.BuildProjection();
		FString Json = TEXT("{\n");
		Json += FString::Printf(TEXT("  \"evidenceSchemaVersion\": %u,\n"), EvidenceSchemaVersion);
		Json += FString::Printf(TEXT("  \"fixtureId\": \"%s\",\n"), *Fixture.GetFixtureId());
		Json += FString::Printf(TEXT("  \"fixtureVersion\": %u,\n"), Fixture.GetFixtureVersion());
		Json += FString::Printf(TEXT("  \"registryHash\": \"%s\",\n"), *Hex64(Fixture.GetRegistryHash()));
		Json += FString::Printf(TEXT("  \"initialStateHash\": \"%s\",\n"), *Hex64(InitialState.GetOverallHash()));
		Json += FString::Printf(TEXT("  \"finalStateHash\": \"%s\",\n"), *Hex64(FinalState.GetOverallHash()));
		Json += FString::Printf(TEXT("  \"ticksRun\": %d,\n"), TicksRun);
		Json += TEXT("  \"events\": [\n");
		const TConstArrayView<FHansaDomainEvent> Events = Fixture.GetEvents();
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			const FHansaDomainEvent& Event = Events[Index];
			Json += FString::Printf(
				TEXT("    {\"sequence\": \"%llu\", \"tick\": %lld, \"type\": \"%s\", \"productionId\": \"%llu\", \"recipeId\": \"%s\", \"blocker\": \"%s\"}%s\n"),
				static_cast<unsigned long long>(Event.GetGlobalSequence()),
				static_cast<long long>(Event.GetTick().GetValue()),
				LexToString(Event.GetType()),
				static_cast<unsigned long long>(Event.GetProductionId().GetValue()),
				*Event.GetRecipeId().ToString(),
				LexToString(Event.GetProductionBlocker()),
				Index + 1 < Events.Num() ? TEXT(",") : TEXT(""));
		}
		Json += TEXT("  ],\n  \"causalProjections\": [\n");
		if (Projection)
		{
			const TConstArrayView<FHansaProductionProjection> Productions = Projection.Value.GetProductions();
			for (int32 Index = 0; Index < Productions.Num(); ++Index)
			{
				const FHansaProductionProjection& Production = Productions[Index];
				Json += FString::Printf(
					TEXT("    {\"productionId\": \"%llu\", \"recipeId\": \"%s\", \"completedCycles\": \"%llu\", \"progressTicks\": %d, \"cycleTicks\": %d, \"blocker\": \"%s\", \"blockingGoodId\": \"%s\"}%s\n"),
					static_cast<unsigned long long>(Production.Id.GetValue()),
					*Production.RecipeId.ToString(),
					static_cast<unsigned long long>(Production.CompletedCycles),
					Production.ProgressTicks,
					Production.CycleTicks,
					LexToString(Production.Blocker),
					*Production.BlockingGoodId.ToString(),
					Index + 1 < Productions.Num() ? TEXT(",") : TEXT(""));
			}
		}
		Json += TEXT("  ]\n}\n");
		return Json;
	}
}
#endif
