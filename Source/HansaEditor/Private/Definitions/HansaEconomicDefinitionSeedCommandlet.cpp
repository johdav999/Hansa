#include "Definitions/HansaEconomicDefinitionSeedCommandlet.h"
#include "GameFramework/Actor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Algo/Reverse.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaResearchDefinitions.h"
#include "Definitions/HansaScenarioDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace Hansa::Editor::EconomicDefinitions
{
	FHansaGoodAmount Amount(const TCHAR* GoodId, const int64 QuantityMilliUnits)
	{
		FHansaGoodAmount Result;
		Result.GoodId = GoodId;
		Result.QuantityMilliUnits = QuantityMilliUnits;
		return Result;
	}

	void ConfigureBase(UHansaDefinitionBase& Definition, const TCHAR* StableId, const TCHAR* DisplayName, const TCHAR* LocalizationKey)
	{
		Definition.StableDefinitionId = StableId;
		Definition.DisplayName = FText::FromString(DisplayName);
		Definition.LocalizationKey = FName(LocalizationKey);
		Definition.ContentSet = TEXT("MVP");
		Definition.AuthoredRevision = 1;
		Definition.Tags = { TEXT("MVP"), TEXT("EconomicVerticalSlice") };
	}

	template <typename TDefinition>
	TDefinition* NewDefinition(UObject* Outer, const TCHAR* ObjectName)
	{
		return NewObject<TDefinition>(
			Outer,
			MakeUniqueObjectName(Outer, TDefinition::StaticClass(), FName(ObjectName)),
			RF_Transactional);
	}

	UHansaGoodDefinition* AddGood(
		TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions,
		UObject* Outer,
		const TCHAR* Name,
		const TCHAR* DisplayName,
		const EHansaGoodUnit Unit,
		const int64 BaseValue,
		const int32 Elasticity,
		const int32 Spoilage)
	{
		UHansaGoodDefinition* Good = NewDefinition<UHansaGoodDefinition>(Outer, Name);
		ConfigureBase(*Good, *FString::Printf(TEXT("Good.%s"), Name), DisplayName, *FString::Printf(TEXT("Game.Good.%s.Name"), Name));
		Good->QuantityUnit = Unit;
		Good->BaseValueMilliMarks = BaseValue;
		Good->PriceElasticityBasisPoints = Elasticity;
		Good->SpoilageBasisPointsPerDay = Spoilage;
		Good->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Good));
		return Good;
	}

	UHansaRecipeDefinition* AddRecipe(
		TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions,
		UObject* Outer,
		const TCHAR* Name,
		const TCHAR* DisplayName,
		TArray<FHansaGoodAmount> Inputs,
		TArray<FHansaGoodAmount> Outputs,
		const int32 CycleTicks,
		const int32 Laborers,
		const int32 Artisans,
		const bool bSource = false)
	{
		UHansaRecipeDefinition* Recipe = NewDefinition<UHansaRecipeDefinition>(Outer, Name);
		ConfigureBase(*Recipe, *FString::Printf(TEXT("Recipe.%s"), Name), DisplayName, *FString::Printf(TEXT("Game.Recipe.%s.Name"), Name));
		Recipe->Inputs = MoveTemp(Inputs);
		Recipe->Outputs = MoveTemp(Outputs);
		Recipe->CycleTicks = CycleTicks;
		Recipe->LaborerWorkforce = Laborers;
		Recipe->ArtisanWorkforce = Artisans;
		Recipe->bDeclaredSource = bSource;
		Recipe->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Recipe));
		return Recipe;
	}

	UHansaBuildingDefinition* AddBuilding(
		TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions,
		UObject* Outer,
		const TCHAR* StableId,
		const TCHAR* ObjectName,
		const TCHAR* DisplayName,
		TArray<FHansaGoodAmount> Costs,
		TArray<FString> Recipes,
		const int64 CurrencyCost,
		const int32 Width,
		const int32 Height,
		const int32 BuildTicks,
		const int32 Storage,
		const int32 Residents,
		const int32 Laborers,
		const int32 Artisans,
		const bool bRoad,
		const bool bShoreline)
	{
		UHansaBuildingDefinition* Building = NewDefinition<UHansaBuildingDefinition>(Outer, ObjectName);
		ConfigureBase(*Building, StableId, DisplayName, *FString::Printf(TEXT("Game.Building.%s.Name"), ObjectName));
		Building->ConstructionCosts = MoveTemp(Costs);
		Building->ConstructionCostPfennig = CurrencyCost;
		Building->CancellationRefundBasisPoints = 5000;
		Building->RecipeIds = MoveTemp(Recipes);
		Building->FootprintWidthCells = Width;
		Building->FootprintHeightCells = Height;
		Building->BuildTicks = BuildTicks;
		Building->StorageCapacityMilliUnits = Storage;
		Building->ResidenceCapacity = Residents;
		Building->LaborerWorkforce = Laborers;
		Building->ArtisanWorkforce = Artisans;
		Building->bRequiresRoad = bRoad;
		Building->bRequiresShoreline = bShoreline;
		Building->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Building));
		return Building;
	}

	UHansaBuildingDefinition* ConfigureConstructionCard(
		UHansaBuildingDefinition* Building,
		const EHansaConstructionMenuCategory Category,
		const int32 Order,
		const TCHAR* Purpose,
		const TCHAR* ChainOutputGoodId = TEXT(""),
		const int32 ChainStage = 0,
		const int32 ChainStageCount = 0,
		const bool bUpgradeOnly = false,
		const TCHAR* RequiredTechnologyId = TEXT(""))
	{
		Building->bShowInConstructionMenu = true;
		Building->ConstructionMenuCategory = Category;
		Building->ConstructionMenuOrder = Order;
		Building->ConstructionChainOutputGoodId = ChainOutputGoodId;
		Building->ConstructionChainStage = ChainStage;
		Building->ConstructionChainStageCount = ChainStageCount;
		Building->RequiredConstructionTechnologyId = RequiredTechnologyId;
		Building->bUpgradeOnly = bUpgradeOnly;
		Building->ConstructionPresentationPurpose = FText::FromString(Purpose);
		Building->RefreshContentHash();
		return Building;
	}

	UHansaNeedDefinition* AddNeed(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* Name, const TCHAR* DisplayName, const EHansaNeedKind Kind, const TCHAR* GoodId = TEXT(""))
	{
		UHansaNeedDefinition* Need = NewDefinition<UHansaNeedDefinition>(Outer, Name);
		ConfigureBase(*Need, *FString::Printf(TEXT("Need.%s"), Name), DisplayName,
			*FString::Printf(TEXT("Game.Need.%s.Name"), Name));
		Need->Kind = Kind;
		Need->GoodId = GoodId;
		Need->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Need));
		return Need;
	}

	FHansaPopulationTierNeed TierNeed(const TCHAR* NeedId, const int32 Consumption, const int32 Importance)
	{
		FHansaPopulationTierNeed Result;
		Result.NeedId = NeedId;
		Result.ConsumptionMilliUnitsPerResidentPerTick = Consumption;
		Result.ImportanceBasisPoints = Importance;
		return Result;
	}

	UHansaPopulationTierDefinition* AddPopulationTier(
		TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer, const TCHAR* Name,
		const TCHAR* DisplayName, const TCHAR* PreviousTierId, TArray<FHansaPopulationTierNeed> Needs,
		const int32 WorkforceShare, const int32 GrowthThreshold, const int32 DeclineThreshold)
	{
		UHansaPopulationTierDefinition* Tier = NewDefinition<UHansaPopulationTierDefinition>(Outer, Name);
		ConfigureBase(*Tier, *FString::Printf(TEXT("PopulationTier.%s"), Name), DisplayName,
			*FString::Printf(TEXT("Game.PopulationTier.%s.Name"), Name));
		Tier->PreviousTierId = PreviousTierId;
		Tier->Needs = MoveTemp(Needs);
		Tier->WorkforcePerResidentBasisPoints = WorkforceShare;
		Tier->GrowthSatisfactionBasisPoints = GrowthThreshold;
		Tier->DeclineSatisfactionBasisPoints = DeclineThreshold;
		Tier->EvaluationTicks = 60;
		Tier->GrowthResidentsPerEvaluation = 1;
		Tier->DeclineResidentsPerEvaluation = 1;
		Tier->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Tier));
		return Tier;
	}

	FHansaMarketGoodProfile MarketGood(const TCHAR* GoodId, const int64 BasePrice, const int64 Reserve,
		const int64 Incoming = 0, const int32 CityModifier = 0)
	{
		FHansaMarketGoodProfile Result;
		Result.GoodId = GoodId;
		Result.DesiredReserveMilliUnits = Reserve;
		Result.ConfirmedIncomingSupplyMilliUnits = Incoming;
		Result.CityModifierBasisPoints = CityModifier;
		Result.MinimumPriceMilliMarks = FMath::Max<int64>(1, BasePrice / 2);
		Result.MaximumPriceMilliMarks = BasePrice * 4;
		Result.InitialPriceMilliMarks = BasePrice;
		return Result;
	}

	void AddCityMarket(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* CityName, const TCHAR* DisplayName, TArray<FHansaMarketGoodProfile> Goods)
	{
		UHansaCityMarketProfileDefinition* Market = NewDefinition<UHansaCityMarketProfileDefinition>(
			Outer, *FString::Printf(TEXT("%sMarket"), CityName));
		ConfigureBase(*Market, *FString::Printf(TEXT("City.%s"), CityName), DisplayName,
			*FString::Printf(TEXT("Game.City.%s.MarketProfile"), CityName));
		Market->bMarketOnly = FCString::Strcmp(CityName, TEXT("Lubeck")) != 0;
		Market->ReportCadenceTicks = Market->bMarketOnly ? 20 : 5;
		Market->CurrentReportMaxAgeTicks = 0;
		Market->RecentReportMaxAgeTicks = Market->bMarketOnly ? 4 : 5;
		Market->StaleReportMaxAgeTicks = 10;
		Market->EstimatedReportMaxAgeTicks = Market->bMarketOnly ? 19 : 20;
		Market->Goods = MoveTemp(Goods);
		Market->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Market));
	}

	void AddVehicle(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* Name, const TCHAR* DisplayName, const EHansaAuthoredRouteMode Mode,
		const int64 Capacity, const int64 Upkeep)
	{
		UHansaVehicleDefinition* Vehicle = NewDefinition<UHansaVehicleDefinition>(Outer, Name);
		ConfigureBase(*Vehicle, *FString::Printf(TEXT("Vehicle.%s"), Name), DisplayName,
			*FString::Printf(TEXT("Game.Vehicle.%s.Name"), Name));
		Vehicle->Mode = Mode;
		Vehicle->CargoCapacityMilliUnits = Capacity;
		Vehicle->UpkeepPfennigPerTravelTick = Upkeep;
		Vehicle->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Vehicle));
	}

	FHansaRouteConnectionDefinition RouteConnection(const TCHAR* Source, const TCHAR* Destination,
		const int32 TravelTicks)
	{
		FHansaRouteConnectionDefinition Result;
		Result.SourceCityId = Source;
		Result.DestinationCityId = Destination;
		Result.TravelTicks = TravelTicks;
		return Result;
	}

	void AddRoute(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* Name, const TCHAR* DisplayName, const EHansaAuthoredRouteMode Mode,
		TArray<FHansaRouteConnectionDefinition> Connections)
	{
		UHansaRouteDefinition* Route = NewDefinition<UHansaRouteDefinition>(Outer, Name);
		ConfigureBase(*Route, *FString::Printf(TEXT("Route.%s"), Name), DisplayName,
			*FString::Printf(TEXT("Game.Route.%s.Name"), Name));
		Route->Mode = Mode;
		Route->Connections = MoveTemp(Connections);
		Route->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Route));
	}

	FHansaResearchEffectDefinition ResearchEffect(const EHansaAuthoredResearchEffectKind Kind,
		const TCHAR* TargetStableId, const int32 Magnitude)
	{
		FHansaResearchEffectDefinition Result;
		Result.Kind = Kind;
		Result.TargetStableId = TargetStableId;
		Result.Magnitude = Magnitude;
		return Result;
	}

	void AddTechnology(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* StableId, const TCHAR* ObjectName, const TCHAR* DisplayName,
		const EHansaAuthoredResearchBranch Branch, TArray<FString> Prerequisites,
		const int32 Cost, const int32 Duration, const TCHAR* Explanation,
		TArray<FHansaResearchEffectDefinition> Effects)
	{
		UHansaTechnologyDefinition* Technology = NewDefinition<UHansaTechnologyDefinition>(Outer, ObjectName);
		ConfigureBase(*Technology, StableId, DisplayName, *FString::Printf(TEXT("Game.Technology.%s.Name"), ObjectName));
		Technology->Branch = Branch;
		Technology->PrerequisiteTechnologyIds = MoveTemp(Prerequisites);
		Technology->CostResearchPoints = Cost;
		Technology->DurationTicks = Duration;
		Technology->UnlockExplanation = FText::FromString(Explanation);
		Technology->Effects = MoveTemp(Effects);
		Technology->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Technology));
	}

	void AddMerchantAITuning(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer)
	{
		UHansaMerchantAITuningDefinition* Tuning = NewDefinition<UHansaMerchantAITuningDefinition>(Outer, TEXT("MerchantRival"));
		ConfigureBase(*Tuning, TEXT("AITuning.MerchantRival"), TEXT("Lübeck merchant rival"), TEXT("Game.AI.MerchantRival.Name"));
		Tuning->DecisionCadenceTicks = 5;
		Tuning->DecisionHistoryCapacity = 32;
		Tuning->MinimumDestinationDemandGapMilliUnits = 1'000;
		Tuning->MinimumGrossMarginMilliMarks = 1;
		Tuning->ShortageUtilityPerUnit = 10;
		Tuning->MarginUtilityPerMilliMark = 1;
		Tuning->ResearchUtility = 50;
		Tuning->ProductionUtility = 25;
		Tuning->TargetCompletedTradeLegs = 2;
		Tuning->PreferredResearchTechnologyIds = {
			TEXT("Technology.Commerce.MarketReports"),
			TEXT("Technology.Logistics.WarehouseHandling")
		};
		FHansaMerchantAITradePlanDefinition GrainPlan;
		GrainPlan.StablePlanId = TEXT("MerchantPlan.RostockGrainRelief");
		GrainPlan.RouteDefinitionId = TEXT("Route.BalticSea");
		GrainPlan.VehicleDefinitionId = TEXT("Vehicle.Cog");
		GrainPlan.SourceCityId = TEXT("City.Rostock");
		GrainPlan.DestinationCityId = TEXT("City.Lubeck");
		GrainPlan.GoodId = TEXT("Good.Grain");
		GrainPlan.QuantityLimitMilliUnits = 20'000;
		GrainPlan.MinimumSourceReserveMilliUnits = 30'000;
		GrainPlan.UtilityBias = 100;
		Tuning->TradePlans = {MoveTemp(GrainPlan)};
		Tuning->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Tuning));
	}


	void AddObjective(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* Name, const TCHAR* DisplayName, const EHansaAuthoredObjectiveMetric Metric,
		const int64 Target, const TCHAR* Unit, const TCHAR* CityId = TEXT(""), const TCHAR* GoodId = TEXT(""))
	{
		UHansaScenarioObjectiveDefinition* Objective = NewDefinition<UHansaScenarioObjectiveDefinition>(Outer, Name);
		ConfigureBase(*Objective, *FString::Printf(TEXT("ScenarioObjective.%s"), Name), DisplayName,
			*FString::Printf(TEXT("Game.ScenarioObjective.%s.Name"), Name));
		Objective->Metric = Metric;
		Objective->TargetValue = Target;
		Objective->ProgressUnit = FText::FromString(Unit);
		Objective->CityId = CityId;
		Objective->GoodId = GoodId;
		Objective->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Objective));
	}

	void AddVictory(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
		const TCHAR* Name, const TCHAR* DisplayName, TArray<FString> Objectives, const int32 Priority,
		const TCHAR* Summary)
	{
		UHansaVictoryDefinition* Victory = NewDefinition<UHansaVictoryDefinition>(Outer, Name);
		ConfigureBase(*Victory, *FString::Printf(TEXT("Victory.%s"), Name), DisplayName,
			*FString::Printf(TEXT("Game.Victory.%s.Name"), Name));
		Victory->ObjectiveIds = MoveTemp(Objectives);
		Victory->SustainTicks = 5;
		Victory->EndingPriority = Priority;
		Victory->Summary = FText::FromString(Summary);
		Victory->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Victory));
	}

	void AddScenarioDefinitions(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer)
	{
		AddObjective(Definitions, Outer, TEXT("SolventHouse"), TEXT("Maintain a solvent house"),
			EHansaAuthoredObjectiveMetric::HouseMoneyAtLeast, 90'000, TEXT("pfennig"));
		AddObjective(Definitions, Outer, TEXT("SafeBreadReserve"), TEXT("Restore the bread reserve"),
			EHansaAuthoredObjectiveMetric::MarketStockAtLeast, 24'000, TEXT("milli-units"), TEXT("City.Lubeck"), TEXT("Good.Bread"));
		AddObjective(Definitions, Outer, TEXT("AffordableBread"), TEXT("Keep bread affordable"),
			EHansaAuthoredObjectiveMetric::MarketPriceAtMost, 1'200, TEXT("milli-marks"), TEXT("City.Lubeck"), TEXT("Good.Bread"));
		AddObjective(Definitions, Outer, TEXT("ActiveProductionEconomy"), TEXT("Operate the full production economy"),
			EHansaAuthoredObjectiveMetric::ActiveProductionsAtLeast, 4, TEXT("productions"));
		AddObjective(Definitions, Outer, TEXT("ActiveSeaRoute"), TEXT("Operate a sea route"),
			EHansaAuthoredObjectiveMetric::ActiveSeaRoutesAtLeast, 1, TEXT("routes"));
		AddObjective(Definitions, Outer, TEXT("ActiveLandRoute"), TEXT("Operate a land route"),
			EHansaAuthoredObjectiveMetric::ActiveLandRoutesAtLeast, 1, TEXT("routes"));
		AddObjective(Definitions, Outer, TEXT("CompletedTradeLegs"), TEXT("Complete trade legs"),
			EHansaAuthoredObjectiveMetric::CompletedTradeLegsAtLeast, 2, TEXT("legs"));
		AddObjective(Definitions, Outer, TEXT("SafeGrainReserve"), TEXT("Restore the grain reserve"),
			EHansaAuthoredObjectiveMetric::MarketStockAtLeast, 20'000, TEXT("milli-units"), TEXT("City.Lubeck"), TEXT("Good.Grain"));
		AddObjective(Definitions, Outer, TEXT("CompletedResearch"), TEXT("Complete research"),
			EHansaAuthoredObjectiveMetric::CompletedTechnologiesAtLeast, 2, TEXT("technologies"));
		AddObjective(Definitions, Outer, TEXT("CivicSatisfaction"), TEXT("Secure civic satisfaction"),
			EHansaAuthoredObjectiveMetric::CitySatisfactionAtLeast, 8'000, TEXT("basis points"), TEXT("City.Lubeck"));
		AddObjective(Definitions, Outer, TEXT("StablePopulation"), TEXT("Maintain Lübeck's population"),
			EHansaAuthoredObjectiveMetric::CityPopulationAtLeast, 12, TEXT("residents"), TEXT("City.Lubeck"));

		AddVictory(Definitions, Outer, TEXT("ProsperityEconomic"), TEXT("City of prosperity"),
			{TEXT("ScenarioObjective.SolventHouse"), TEXT("ScenarioObjective.SafeBreadReserve"),
			 TEXT("ScenarioObjective.AffordableBread"), TEXT("ScenarioObjective.ActiveProductionEconomy")}, 10,
			TEXT("Recover staple supply and establish a stable, prosperous civic economy."));
		AddVictory(Definitions, Outer, TEXT("TradeNetwork"), TEXT("Trade network"),
			{TEXT("ScenarioObjective.SafeGrainReserve"), TEXT("ScenarioObjective.ActiveSeaRoute"),
			 TEXT("ScenarioObjective.ActiveLandRoute"), TEXT("ScenarioObjective.CompletedTradeLegs")}, 20,
			TEXT("Bind sea and land supply into a working Lübeck relief network."));
		AddVictory(Definitions, Outer, TEXT("ResearchCivic"), TEXT("Research and civic resilience"),
			{TEXT("ScenarioObjective.CompletedResearch"), TEXT("ScenarioObjective.StablePopulation"),
			 TEXT("ScenarioObjective.SolventHouse")}, 30,
			TEXT("Use knowledge and civic stability to make Lübeck resilient."));

		UHansaScenarioDefinition* Scenario = NewDefinition<UHansaScenarioDefinition>(Outer, TEXT("LubeckGrainShortageV1"));
		ConfigureBase(*Scenario, TEXT("Scenario.LubeckGrainShortageV1"), TEXT("Lübeck grain shortage"),
			TEXT("Game.Scenario.LubeckGrainShortageV1.Name"));
		Scenario->HomeCityId = TEXT("City.Lubeck");
		Scenario->VictoryIds = {TEXT("Victory.ProsperityEconomic"), TEXT("Victory.TradeNetwork"), TEXT("Victory.ResearchCivic")};
		Scenario->InsolvencyThresholdPfennig = -5'000;
		Scenario->FailureSustainTicks = 20;
		Scenario->Briefing = FText::FromString(TEXT("Lübeck's grain and bread reserves are under pressure. Restore a durable supply through prosperity, trade, or research-led civic resilience."));
		Scenario->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Scenario));
	}
	bool ApplyStarterEconomyBalance(UHansaDefinitionBase* Definition)
	{
		const FString Id = Definition->StableDefinitionId;
		if (auto* Recipe = Cast<UHansaRecipeDefinition>(Definition))
		{
			if (Id == TEXT("Recipe.GrowGrain"))
			{
				Recipe->Inputs = {};
				Recipe->Outputs = { Amount(TEXT("Good.Grain"), 30'000) };
				Recipe->CycleTicks = 60;
			}
			else if (Id == TEXT("Recipe.MillFlour"))
			{
				Recipe->Inputs = { Amount(TEXT("Good.Grain"), 20'000) };
				Recipe->Outputs = { Amount(TEXT("Good.Flour"), 15'000) };
				Recipe->CycleTicks = 45;
			}
			else if (Id == TEXT("Recipe.BakeBread"))
			{
				Recipe->Inputs = { Amount(TEXT("Good.Flour"), 10'000) };
				Recipe->Outputs = { Amount(TEXT("Good.Bread"), 15'000) };
				Recipe->CycleTicks = 20;
			}
			else if (Id == TEXT("Recipe.CatchFish"))
			{
				Recipe->Inputs = {};
				Recipe->Outputs = { Amount(TEXT("Good.Fish"), 20'000) };
				Recipe->CycleTicks = 60;
			}
			else return false;
			Recipe->LaborerWorkforce = 2;
			Recipe->ArtisanWorkforce = 0;
		}
		else if (auto* Building = Cast<UHansaBuildingDefinition>(Definition))
		{
			if (Id == TEXT("Building.GrainFarm"))
			{
				Building->ConstructionCosts = {
					Amount(TEXT("Good.Timber"), 6'000), Amount(TEXT("Good.Tools"), 1'000) };
				Building->ConstructionCostPfennig = 3'000;
			}
			else if (Id == TEXT("Building.Mill"))
			{
				Building->ConstructionCosts = { Amount(TEXT("Good.Timber"), 8'000),
					Amount(TEXT("Good.Planks"), 6'000), Amount(TEXT("Good.Tools"), 2'000) };
				Building->ConstructionCostPfennig = 4'000;
			}
			else if (Id == TEXT("Building.Bakery"))
			{
				Building->ConstructionCosts = {
					Amount(TEXT("Good.Planks"), 8'000), Amount(TEXT("Good.Tools"), 2'000) };
				Building->ConstructionCostPfennig = 3'500;
			}
			else if (Id == TEXT("Building.Fishery"))
			{
				Building->ConstructionCosts = { Amount(TEXT("Good.Timber"), 10'000),
					Amount(TEXT("Good.Planks"), 4'000), Amount(TEXT("Good.Tools"), 1'000) };
				Building->ConstructionCostPfennig = 3'750;
			}
			else return false;
			Building->LaborerWorkforce = 2;
			Building->ArtisanWorkforce = 0;
		}
		else if (auto* Tier = Cast<UHansaPopulationTierDefinition>(Definition))
		{
			if (Id != TEXT("PopulationTier.Laborer")) return false;
			Tier->Needs = { TierNeed(TEXT("Need.Bread"), 10, 6000),
				TierNeed(TEXT("Need.Fish"), 6, 1500), TierNeed(TEXT("Need.Beer"), 4, 500),
				TierNeed(TEXT("Need.BasicServices"), 0, 2000) };
			Tier->GrowthSatisfactionBasisPoints = 9000;
			Tier->DeclineSatisfactionBasisPoints = 7500;
			Tier->EvaluationTicks = 72;
			Tier->GrowthResidentsPerEvaluation = 1;
			Tier->DeclineResidentsPerEvaluation = 1;
		}
		else return false;
		Definition->RefreshContentHash();
		return true;
	}

	int32 StarterEconomyAuthoredRevision(const FString& Id)
	{
		if (Id == TEXT("Recipe.GrowGrain") || Id == TEXT("Recipe.MillFlour") ||
			Id == TEXT("Recipe.CatchFish")) return 3;
		if (Id == TEXT("Recipe.BakeBread")) return 4;
		if (Id == TEXT("Building.GrainFarm") || Id == TEXT("Building.Bakery") ||
			Id == TEXT("Building.Fishery")) return 5;
		if (Id == TEXT("Building.Mill")) return 4;
		if (Id == TEXT("PopulationTier.Laborer")) return 2;
		return 0;
	}

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> CreateMvpDefinitionSet(UObject* Outer)
	{
		UObject* EffectiveOuter = Outer != nullptr ? Outer : GetTransientPackage();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions;
		Definitions.Reserve(81);

		AddGood(Definitions, EffectiveOuter, TEXT("Grain"), TEXT("Grain"), EHansaGoodUnit::Kilogram, 1000, 12000, 25);
		AddGood(Definitions, EffectiveOuter, TEXT("Flour"), TEXT("Flour"), EHansaGoodUnit::Kilogram, 1700, 10500, 75);
		AddGood(Definitions, EffectiveOuter, TEXT("Hops"), TEXT("Hops"), EHansaGoodUnit::Kilogram, 1800, 11000, 100);
		AddGood(Definitions, EffectiveOuter, TEXT("Malt"), TEXT("Malt"), EHansaGoodUnit::Kilogram, 1400, 10000, 50);
		AddGood(Definitions, EffectiveOuter, TEXT("Bread"), TEXT("Bread"), EHansaGoodUnit::Item, 800, 13500, 250);
		AddGood(Definitions, EffectiveOuter, TEXT("Fish"), TEXT("Fish"), EHansaGoodUnit::Kilogram, 1800, 14000, 500);
		AddGood(Definitions, EffectiveOuter, TEXT("Salt"), TEXT("Salt"), EHansaGoodUnit::Kilogram, 2200, 9000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Timber"), TEXT("Timber"), EHansaGoodUnit::Kilogram, 700, 8000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Planks"), TEXT("Planks"), EHansaGoodUnit::Kilogram, 1300, 8500, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Iron"), TEXT("Iron"), EHansaGoodUnit::Kilogram, 2600, 7500, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Tools"), TEXT("Tools"), EHansaGoodUnit::Item, 6500, 10000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Barrels"), TEXT("Barrels"), EHansaGoodUnit::Item, 2500, 9000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Beer"), TEXT("Beer"), EHansaGoodUnit::Litre, 1500, 12500, 100);

		AddRecipe(Definitions, EffectiveOuter, TEXT("GrowGrain"), TEXT("Grow grain"), {}, { Amount(TEXT("Good.Grain"), 6000) }, 120, 8, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("MillFlour"), TEXT("Mill flour"), { Amount(TEXT("Good.Grain"), 4000) }, { Amount(TEXT("Good.Flour"), 3000) }, 60, 4, 1);
		AddRecipe(Definitions, EffectiveOuter, TEXT("BakeBread"), TEXT("Bake bread"), { Amount(TEXT("Good.Flour"), 2000) }, { Amount(TEXT("Good.Bread"), 3000) }, 45, 4, 2);
		AddRecipe(Definitions, EffectiveOuter, TEXT("CatchFish"), TEXT("Catch fish"), {}, { Amount(TEXT("Good.Fish"), 4000) }, 90, 8, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("FellTimber"), TEXT("Fell timber"), {}, { Amount(TEXT("Good.Timber"), 6000) }, 100, 8, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("SawPlanks"), TEXT("Saw planks"), { Amount(TEXT("Good.Timber"), 5000) }, { Amount(TEXT("Good.Planks"), 3500) }, 75, 6, 1);
		AddRecipe(Definitions, EffectiveOuter, TEXT("SmithTools"), TEXT("Smith tools"), { Amount(TEXT("Good.Iron"), 3000) }, { Amount(TEXT("Good.Tools"), 1000) }, 120, 4, 4);
		AddRecipe(Definitions, EffectiveOuter, TEXT("GrowHops"), TEXT("Grow hops"), {}, { Amount(TEXT("Good.Hops"), 4000) }, 90, 4, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("MaltGrain"), TEXT("Malt grain"), { Amount(TEXT("Good.Grain"), 4000) }, { Amount(TEXT("Good.Malt"), 3000) }, 60, 3, 1);
		AddRecipe(Definitions, EffectiveOuter, TEXT("MakeBarrels"), TEXT("Make barrels"), { Amount(TEXT("Good.Timber"), 3000) }, { Amount(TEXT("Good.Barrels"), 1000) }, 75, 3, 2);
		UHansaRecipeDefinition* BrewBeer = AddRecipe(Definitions, EffectiveOuter, TEXT("BrewBeer"), TEXT("Brew beer"),
			{ Amount(TEXT("Good.Malt"), 3000), Amount(TEXT("Good.Hops"), 1000), Amount(TEXT("Good.Barrels"), 1000) },
			{ Amount(TEXT("Good.Beer"), 5000) }, 100, 4, 2);
		BrewBeer->AuthoredRevision = 2;
		BrewBeer->RefreshContentHash();

		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Road"), TEXT("Road"), TEXT("Road"), { Amount(TEXT("Good.Timber"), 500) }, {}, 25, 1, 1, 5, 0, 0, 0, 0, false, false), EHansaConstructionMenuCategory::Roads, 0, TEXT("Road connection"));
		UHansaBuildingDefinition* LaborerResidence = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Residence.Laborer"), TEXT("LaborerResidence"), TEXT("Laborer residence"), { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 2000) }, {}, 800, 2, 2, 90, 2000, 12, 0, 0, true, false);
		ConfigureConstructionCard(LaborerResidence, EHansaConstructionMenuCategory::Residences, 0, TEXT("Provides laborer homes and workforce"));
		LaborerResidence->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Mesh/LaborerResidence/Materials_R02/Meshes/SM_LaborerResidence.SM_LaborerResidence")));
		LaborerResidence->UpgradeTargetBuildingId = TEXT("Building.Residence.Artisan");
		LaborerResidence->ResidentPopulationTierId = TEXT("PopulationTier.Laborer");
		LaborerResidence->RefreshContentHash();
		UHansaBuildingDefinition* ArtisanResidence = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Residence.Artisan"), TEXT("ArtisanResidence"), TEXT("Artisan residence"), { Amount(TEXT("Good.Planks"), 5000), Amount(TEXT("Good.Tools"), 1000) }, {}, 1400, 2, 2, 140, 3000, 8, 0, 0, true, false);
		ConfigureConstructionCard(ArtisanResidence, EHansaConstructionMenuCategory::Residences, 1, TEXT("Provides artisan homes and skilled workforce"), TEXT(""), 0, 0, true);
		ArtisanResidence->ResidentPopulationTierId = TEXT("PopulationTier.Artisan");
		ArtisanResidence->RefreshContentHash();
		UHansaBuildingDefinition* MarketBuilding = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Market"), TEXT("Market"), TEXT("Market"), { Amount(TEXT("Good.Planks"), 6000), Amount(TEXT("Good.Tools"), 1000) }, {}, 1800, 3, 3, 180, 50000, 0, 6, 2, true, false);
		MarketBuilding->bProvidesMarketAccess = true;
		ConfigureConstructionCard(MarketBuilding, EHansaConstructionMenuCategory::Storage, 0, TEXT("Local goods distribution and services"));
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Warehouse"), TEXT("Warehouse"), TEXT("Warehouse"), { Amount(TEXT("Good.Planks"), 10000), Amount(TEXT("Good.Tools"), 2000) }, {}, 2500, 4, 3, 220, 200000, 0, 10, 2, true, false), EHansaConstructionMenuCategory::Storage, 1, TEXT("Cart transfer and city storage"));
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Dock"), TEXT("Dock"), TEXT("Dock"), { Amount(TEXT("Good.Timber"), 12000), Amount(TEXT("Good.Planks"), 8000), Amount(TEXT("Good.Tools"), 2000) }, {}, 4000, 5, 3, 300, 150000, 0, 16, 4, false, true), EHansaConstructionMenuCategory::Harbor, 0, TEXT("Ship transfer and harbor storage"));
		UHansaBuildingDefinition* GrainFarm = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.GrainFarm"), TEXT("GrainFarm"), TEXT("Grain farm"), { Amount(TEXT("Good.Timber"), 3000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.GrowGrain") }, 1200, 4, 4, 150, 30000, 0, 8, 0, true, false);
		ConfigureConstructionCard(GrainFarm, EHansaConstructionMenuCategory::Production, 0, TEXT(""), TEXT("Good.Bread"), 1, 3);
		GrainFarm->AuthoredRevision = 3;
		GrainFarm->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Mesh/hansa-grain-farm/Final/SM_HansaGrainFarm.SM_HansaGrainFarm")));
		GrainFarm->PresentationActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/Hansa.HansaGrainFarmPresentation")));
		GrainFarm->RefreshContentHash();
		UHansaBuildingDefinition* Mill = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Mill"), TEXT("Mill"), TEXT("Mill"), { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 3000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.MillFlour") }, 1600, 3, 3, 170, 25000, 0, 4, 1, true, false);
		ConfigureConstructionCard(Mill, EHansaConstructionMenuCategory::Production, 1, TEXT(""), TEXT("Good.Bread"), 2, 3);
		Mill->AuthoredRevision = 2;
		Mill->PresentationActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated.BP_HansaWindmill_Animated_C")));
		Mill->RefreshContentHash();
		UHansaBuildingDefinition* Bakery = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Bakery"), TEXT("Bakery"), TEXT("Bakery"), { Amount(TEXT("Good.Planks"), 4000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.BakeBread") }, 1400, 3, 2, 150, 20000, 0, 4, 2, true, false);
		ConfigureConstructionCard(Bakery, EHansaConstructionMenuCategory::Production, 2, TEXT(""), TEXT("Good.Bread"), 3, 3);
		Bakery->AuthoredRevision = 3;
		Bakery->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Mesh/hansa-bakery/P10/Meshes/SM_Bakery_Body.SM_Bakery_Body")));
		Bakery->PresentationActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/Hansa.HansaBakeryPresentation")));
		Bakery->RefreshContentHash();
		UHansaBuildingDefinition* Fishery = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Fishery"), TEXT("Fishery"), TEXT("Fishery"), { Amount(TEXT("Good.Timber"), 5000), Amount(TEXT("Good.Planks"), 2000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.CatchFish") }, 1500, 3, 2, 150, 30000, 0, 8, 0, true, true);
		ConfigureConstructionCard(Fishery, EHansaConstructionMenuCategory::Production, 0, TEXT(""), TEXT("Good.Fish"), 1, 1);
		Fishery->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Mesh/hansa-fishery/SM_HansaFishery.SM_HansaFishery")));
		Fishery->AuthoredRevision = 4;
		Fishery->RefreshContentHash();
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.LumberCamp"), TEXT("LumberCamp"), TEXT("Lumber camp"), { Amount(TEXT("Good.Timber"), 2000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.FellTimber") }, 900, 3, 3, 120, 30000, 0, 8, 0, true, false), EHansaConstructionMenuCategory::Production, 0, TEXT(""), TEXT("Good.Planks"), 1, 2);
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Sawmill"), TEXT("Sawmill"), TEXT("Sawmill"), { Amount(TEXT("Good.Timber"), 5000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.SawPlanks") }, 1700, 4, 3, 180, 40000, 0, 6, 1, true, false), EHansaConstructionMenuCategory::Production, 1, TEXT(""), TEXT("Good.Planks"), 2, 2);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Smithy"), TEXT("Smithy"), TEXT("Smithy and tool workshop"), { Amount(TEXT("Good.Planks"), 5000), Amount(TEXT("Good.Iron"), 3000) }, { TEXT("Recipe.SmithTools") }, 2400, 3, 3, 220, 25000, 0, 4, 4, true, false);
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.HopFarm"), TEXT("HopFarm"), TEXT("Hop farm"), { Amount(TEXT("Good.Timber"), 3000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.GrowHops") }, 1100, 4, 4, 140, 30000, 0, 4, 0, true, false), EHansaConstructionMenuCategory::Production, 0, TEXT(""), TEXT("Good.Beer"), 1, 4);
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.MaltHouse"), TEXT("MaltHouse"), TEXT("Malt house"), { Amount(TEXT("Good.Timber"), 3000), Amount(TEXT("Good.Planks"), 3000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.MaltGrain") }, 1600, 3, 3, 160, 30000, 0, 3, 1, true, false), EHansaConstructionMenuCategory::Production, 1, TEXT("Requires generic grain from a grain farm"), TEXT("Good.Beer"), 2, 4);
		ConfigureConstructionCard(AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Cooperage"), TEXT("Cooperage"), TEXT("Cooperage"), { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 2000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.MakeBarrels") }, 1800, 3, 3, 180, 30000, 0, 3, 2, true, false), EHansaConstructionMenuCategory::Production, 2, TEXT("Requires timber from a lumber camp"), TEXT("Good.Beer"), 3, 4);
		UHansaBuildingDefinition* Brewery = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Brewery"), TEXT("Brewery"), TEXT("Brewery"), { Amount(TEXT("Good.Planks"), 6000), Amount(TEXT("Good.Tools"), 1500) }, { TEXT("Recipe.BrewBeer") }, 2200, 4, 3, 220, 50000, 0, 4, 2, true, false);
		ConfigureConstructionCard(Brewery, EHansaConstructionMenuCategory::Production, 3, TEXT("Requires malt, hops, and empty barrels"), TEXT("Good.Beer"), 4, 4);
		Brewery->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Mesh/hansa-brewery-huexstrasse128/Production/SM_HansaBrewery_Production.SM_HansaBrewery_Production")));
		Brewery->AuthoredRevision = 4;
		Brewery->FootprintHeightCells = 4;
		Brewery->RefreshContentHash();

		AddNeed(Definitions, EffectiveOuter, TEXT("Bread"), TEXT("Bread"), EHansaNeedKind::Good, TEXT("Good.Bread"));
		AddNeed(Definitions, EffectiveOuter, TEXT("Fish"), TEXT("Fish"), EHansaNeedKind::Good, TEXT("Good.Fish"));
		AddNeed(Definitions, EffectiveOuter, TEXT("Beer"), TEXT("Beer"), EHansaNeedKind::Good, TEXT("Good.Beer"));
		AddNeed(Definitions, EffectiveOuter, TEXT("Tools"), TEXT("Tools"), EHansaNeedKind::Good, TEXT("Good.Tools"));
		AddNeed(Definitions, EffectiveOuter, TEXT("BasicServices"), TEXT("Basic services"), EHansaNeedKind::Service);

		AddPopulationTier(Definitions, EffectiveOuter, TEXT("Laborer"), TEXT("Laborers"), TEXT(""),
			{ TierNeed(TEXT("Need.Bread"), 100, 4000), TierNeed(TEXT("Need.Fish"), 60, 2500),
				TierNeed(TEXT("Need.Beer"), 40, 1500), TierNeed(TEXT("Need.BasicServices"), 0, 2000) },
			6000, 8000, 3500);
		AddPopulationTier(Definitions, EffectiveOuter, TEXT("Artisan"), TEXT("Artisans"), TEXT("PopulationTier.Laborer"),
			{ TierNeed(TEXT("Need.Bread"), 140, 3000), TierNeed(TEXT("Need.Fish"), 60, 1500),
				TierNeed(TEXT("Need.Beer"), 70, 2000), TierNeed(TEXT("Need.Tools"), 20, 1500),
				TierNeed(TEXT("Need.BasicServices"), 0, 2000) },
			7000, 8500, 4000);

		const auto CityGoods = [](const TCHAR* CityName)
		{
			const FString City(CityName);
			const bool bLuneburg = City == TEXT("Luneburg");
			const bool bRostock = City == TEXT("Rostock");
			const bool bHamburg = City == TEXT("Hamburg");
			TArray<FHansaMarketGoodProfile> Result {
				MarketGood(TEXT("Good.Grain"), 1000, 30000, bRostock ? 4000 : 0),
				MarketGood(TEXT("Good.Flour"), 1700, 20000),
				MarketGood(TEXT("Good.Hops"), 1800, 8000),
				MarketGood(TEXT("Good.Malt"), 1400, 12000),
				MarketGood(TEXT("Good.Bread"), 800, 24000, bHamburg ? 2000 : 0),
				MarketGood(TEXT("Good.Fish"), 1800, 18000, (bHamburg || bRostock) ? 3000 : 0),
				MarketGood(TEXT("Good.Salt"), 2200, 12000, bLuneburg ? 5000 : 0, bLuneburg ? -500 : 0),
				MarketGood(TEXT("Good.Timber"), 700, 24000),
				MarketGood(TEXT("Good.Planks"), 1300, 18000),
				MarketGood(TEXT("Good.Iron"), 2600, 10000),
				MarketGood(TEXT("Good.Tools"), 6500, 8000),
				MarketGood(TEXT("Good.Barrels"), 2500, 6000),
				MarketGood(TEXT("Good.Beer"), 1500, 16000)
			};
			if (City != TEXT("Lubeck"))
			{
				for (FHansaMarketGoodProfile& Good : Result)
				{
					Good.InitialStockMilliUnits = Good.DesiredReserveMilliUnits;
					Good.BackgroundCitizenDemandMilliUnitsPerUpdate = 700;
					Good.BackgroundIndustrialDemandMilliUnitsPerUpdate = 300;
					Good.BackgroundProductionMilliUnitsPerUpdate = 1000;
				}
				const auto AddExportStrength = [&Result](const TCHAR* GoodId, const int64 OpeningSurplus,
					const int64 AdditionalProduction)
				{
					FHansaMarketGoodProfile* Good = Result.FindByPredicate([GoodId](const FHansaMarketGoodProfile& Candidate)
					{
						return Candidate.GoodId == GoodId;
					});
					check(Good != nullptr);
					Good->InitialStockMilliUnits += OpeningSurplus;
					Good->BackgroundProductionMilliUnitsPerUpdate += AdditionalProduction;
				};
				if (bHamburg)
				{
					AddExportStrength(TEXT("Good.Fish"), 8000, 1500);
					AddExportStrength(TEXT("Good.Bread"), 4000, 750);
				}
				if (bLuneburg) AddExportStrength(TEXT("Good.Salt"), 12000, 2000);
				if (bRostock)
				{
					AddExportStrength(TEXT("Good.Grain"), 10000, 2000);
					AddExportStrength(TEXT("Good.Fish"), 6000, 1000);
				}
			}
			return Result;
		};
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Lubeck"), TEXT("Lübeck market"), CityGoods(TEXT("Lubeck")));
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Hamburg"), TEXT("Hamburg market"), CityGoods(TEXT("Hamburg")));
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Luneburg"), TEXT("Lüneburg market"), CityGoods(TEXT("Luneburg")));
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Rostock"), TEXT("Rostock market"), CityGoods(TEXT("Rostock")));

		AddVehicle(Definitions, EffectiveOuter, TEXT("Cog"), TEXT("Cog"),
			EHansaAuthoredRouteMode::Sea, 60'000, 12);
		AddVehicle(Definitions, EffectiveOuter, TEXT("Wagon"), TEXT("Wagon"),
			EHansaAuthoredRouteMode::Land, 20'000, 5);
		AddRoute(Definitions, EffectiveOuter, TEXT("BalticSea"), TEXT("Baltic sea connections"),
			EHansaAuthoredRouteMode::Sea,
			{ RouteConnection(TEXT("City.Lubeck"), TEXT("City.Hamburg"), 8),
				RouteConnection(TEXT("City.Lubeck"), TEXT("City.Rostock"), 10) });
		AddRoute(Definitions, EffectiveOuter, TEXT("SaltRoad"), TEXT("Lüneburg salt road"),
			EHansaAuthoredRouteMode::Land,
			{ RouteConnection(TEXT("City.Lubeck"), TEXT("City.Luneburg"), 6) });

		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Commerce.MarketReports"), TEXT("CommerceMarketReports"),
			TEXT("Better market reports"), EHansaAuthoredResearchBranch::Commerce, {}, 100, 4,
			TEXT("Market reports remain current for longer and expose opportunities sooner."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::MarketReportAgeReductionTicks, TEXT("City.Lubeck"), 5)});
		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Commerce.TransactionFriction"), TEXT("CommerceTransactionFriction"),
			TEXT("Letters of credit"), EHansaAuthoredResearchBranch::Commerce, {TEXT("Technology.Commerce.MarketReports")}, 160, 6,
			TEXT("Standardized credit instruments reduce transaction friction."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::TransactionFrictionReductionBasisPoints, TEXT("City.Lubeck"), 500)});
		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Commerce.ReserveAutomation"), TEXT("CommerceReserveAutomation"),
			TEXT("Reserve instructions"), EHansaAuthoredResearchBranch::Commerce, {TEXT("Technology.Commerce.TransactionFriction")}, 220, 8,
			TEXT("Unlocks minimum-reserve automation for trade orders."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::ReserveAutomation, TEXT("Route.BalticSea"), 1)});

		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Production.ImprovedMilling"), TEXT("ProductionImprovedMilling"),
			TEXT("Improved milling"), EHansaAuthoredResearchBranch::Production, {}, 100, 4,
			TEXT("Improved gearing raises flour throughput."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::ProductionThroughputBasisPoints, TEXT("Recipe.MillFlour"), 1000)});
		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Production.ImprovedSawmilling"), TEXT("ProductionImprovedSawmilling"),
			TEXT("Improved sawmilling"), EHansaAuthoredResearchBranch::Production, {TEXT("Technology.Production.ImprovedMilling")}, 160, 6,
			TEXT("Standard saw frames raise plank throughput."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::ProductionThroughputBasisPoints, TEXT("Recipe.SawPlanks"), 1000)});
		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Production.ImprovedSmithing"), TEXT("ProductionImprovedSmithing"),
			TEXT("Improved smithing"), EHansaAuthoredResearchBranch::Production, {TEXT("Technology.Production.ImprovedSawmilling")}, 220, 8,
			TEXT("Specialized tooling raises smithy throughput."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::ProductionThroughputBasisPoints, TEXT("Recipe.SmithTools"), 1000)});

		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Logistics.WarehouseHandling"), TEXT("LogisticsWarehouseHandling"),
			TEXT("Warehouse handling"), EHansaAuthoredResearchBranch::Logistics, {}, 100, 4,
			TEXT("Ledger-directed handling improves warehouse transfer capacity."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::WarehouseHandlingBasisPoints, TEXT("Building.Warehouse"), 1000)});
		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Logistics.CartCapacity"), TEXT("LogisticsCartCapacity"),
			TEXT("Reinforced wagons"), EHansaAuthoredResearchBranch::Logistics, {TEXT("Technology.Logistics.WarehouseHandling")}, 160, 6,
			TEXT("Reinforced wagon frames increase land cargo capacity."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::VehicleCapacityBasisPoints, TEXT("Vehicle.Wagon"), 1500)});
		AddTechnology(Definitions, EffectiveOuter, TEXT("Technology.Logistics.RouteScheduling"), TEXT("LogisticsRouteScheduling"),
			TEXT("Route scheduling"), EHansaAuthoredResearchBranch::Logistics, {TEXT("Technology.Logistics.CartCapacity")}, 220, 8,
			TEXT("Unlocks scheduled departure policies for established routes."),
			{ResearchEffect(EHansaAuthoredResearchEffectKind::RouteScheduling, TEXT("Route.SaltRoad"), 1)});

		AddMerchantAITuning(Definitions, EffectiveOuter);
		AddScenarioDefinitions(Definitions, EffectiveOuter);

		// User-approved P12-P15, P17 and P18 presentation catalog v7. Economics are unchanged.
		const TArray<TTuple<FString, FString, FString>> Presentations = {
			{TEXT("Building.LumberCamp"), TEXT("/Game/Mesh/hansa-lumber-camp/MeshesCM/SM_HansaLumberCamp.SM_HansaLumberCamp"), TEXT("/Game/Mesh/hansa-lumber-camp/BP_LumberCamp_Review.BP_LumberCamp_Review_C")},
			{TEXT("Building.Sawmill"), TEXT("/Game/Mesh/hansa-sawmill/Meshes/SM_HansaSawmill.SM_HansaSawmill"), TEXT("/Game/Mesh/hansa-sawmill/BP_Sawmill_Review.BP_Sawmill_Review_C")},
			{TEXT("Building.Residence.Laborer"), TEXT("/Game/Mesh/hansa-residences/Meshes_R06/SM_Residence_Laborer_A.SM_Residence_Laborer_A"), TEXT("/Game/Mesh/hansa-residences/BP_Residence_Laborer_Review.BP_Residence_Laborer_Review_C")},
			{TEXT("Building.Residence.Artisan"), TEXT("/Game/Mesh/hansa-residences/Meshes_R06/SM_Residence_Artisan_A.SM_Residence_Artisan_A"), TEXT("/Game/Mesh/hansa-residences/BP_Residence_Artisan_Review.BP_Residence_Artisan_Review_C")},
			{TEXT("Building.Market"), TEXT("/Game/Mesh/hansa-market/Meshes/SM_HansaMarket.SM_HansaMarket"), TEXT("/Game/Mesh/hansa-market/BP_Market_Review.BP_Market_Review_C")},
			{TEXT("Building.Dock"), TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaDock_Deck4m.SM_HansaDock_Deck4m"), TEXT("/Game/Mesh/hansa-harbor/BP_Harbor_Review.BP_Harbor_Review_C")},
			{TEXT("Building.Road"), TEXT("/Game/Mesh/hansa-dirt-road/Meshes/SM_HansaRoad_Straight.SM_HansaRoad_Straight"), TEXT("/Game/Mesh/hansa-dirt-road/BP_Road_Review.BP_Road_Review_C")}
		};
		for (const auto& Definition : Definitions)
		{
			auto* Building = Cast<UHansaBuildingDefinition>(Definition.Get());
			if (!Building) continue;
			for (const auto& Presentation : Presentations)
			{
				if (Building->StableDefinitionId != Presentation.Get<0>()) continue;
				Building->AuthoredRevision = 2;
				Building->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Presentation.Get<1>()));
				Building->PresentationActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(Presentation.Get<2>()));
				Building->RefreshContentHash();
			}
		}

		// Explicitly approved P19 catalog v8: stable identity and economics are unchanged.
		for (const auto& Definition : Definitions)
		{
			auto* Vehicle = Cast<UHansaVehicleDefinition>(Definition.Get());
			if (!Vehicle) continue;
			const bool bCog = Vehicle->StableDefinitionId == TEXT("Vehicle.Cog");
			if (!bCog && Vehicle->StableDefinitionId != TEXT("Vehicle.Wagon")) continue;
			Vehicle->AuthoredRevision = 2;
			Vehicle->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(bCog
				? TEXT("/Game/Mesh/hansa-vehicles/Meshes/SM_HansaCog_Hull.SM_HansaCog_Hull")
				: TEXT("/Game/Mesh/hansa-vehicles/Meshes/SM_HansaWagon_Body.SM_HansaWagon_Body")));
			Vehicle->PresentationActorClass = TSoftClassPtr<AHansaCargoVehiclePresentation>(FSoftObjectPath(bCog
				? TEXT("/Game/Mesh/hansa-vehicles/BP_Cog_Review.BP_Cog_Review_C")
				: TEXT("/Game/Mesh/hansa-vehicles/BP_Wagon_Review.BP_Wagon_Review_C")));
			Vehicle->RefreshContentHash();
		}
		for (auto& Definition : Definitions)
			if (ApplyStarterEconomyBalance(Definition.Get()))
			{
				Definition->AuthoredRevision = StarterEconomyAuthoredRevision(Definition->StableDefinitionId);
				Definition->RefreshContentHash();
			}
		return Definitions;
	}

	FString AssetNameForDefinition(const UHansaDefinitionBase& Definition)
	{
		FString Name = Definition.StableDefinitionId;
		Name.ReplaceInline(TEXT("."), TEXT("_"));
		return TEXT("DA_") + Name;
	}

	FString PackageDirectoryForDefinition(const UHansaDefinitionBase& Definition)
	{
		if (Definition.IsA<UHansaGoodDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Goods");
		}
		if (Definition.IsA<UHansaRecipeDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Recipes");
		}
		if (Definition.IsA<UHansaNeedDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Needs");
		}
		if (Definition.IsA<UHansaPopulationTierDefinition>())
		{
			return TEXT("/Game/Hansa/Core/PopulationTiers");
		}
		if (Definition.IsA<UHansaCityMarketProfileDefinition>())
		{
			return TEXT("/Game/Hansa/Core/CityMarkets");
		}
		if (Definition.IsA<UHansaVehicleDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Vehicles");
		}
		if (Definition.IsA<UHansaRouteDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Routes");
		}
		if (Definition.IsA<UHansaTechnologyDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Technologies");
		}
		if (Definition.IsA<UHansaMerchantAITuningDefinition>())
		{
			return TEXT("/Game/Hansa/Core/AI");
		}
		if (Definition.IsA<UHansaScenarioObjectiveDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Scenarios/Objectives");
		}
		if (Definition.IsA<UHansaVictoryDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Scenarios/Victories");
		}
		if (Definition.IsA<UHansaScenarioDefinition>())
		{
			return TEXT("/Game/Hansa/Core/Scenarios");
		}
		return TEXT("/Game/Hansa/Core/Buildings");
	}

	bool SaveMvpDefinitionAssets(const bool bReplaceExisting, TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions = CreateMvpDefinitionSet(GetTransientPackage());
		FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		for (const TStrongObjectPtr<UHansaDefinitionBase>& Source : Definitions)
		{
			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			if (IFileManager::Get().FileExists(*Filename))
			{
				if (!bReplaceExisting)
				{
					continue;
				}
				OutError = FString::Printf(TEXT("-Replace is not supported while an existing asset package may be loaded: %s"), *Filename);
				return false;
			}

			UPackage* Package = CreatePackage(*PackageName);
			UHansaDefinitionBase* Asset = DuplicateObject<UHansaDefinitionBase>(Source.Get(), Package, *AssetName);
			Asset->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
			Asset->RefreshContentHash();
			AssetRegistry.AssetCreated(Asset);
			Package->MarkPackageDirty();
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to save economic definition asset: %s"), *Filename);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return true;
	}

	bool MigrateMvpBuildingConstructionCosts(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaBuildingDefinition* Source = Cast<UHansaBuildingDefinition>(SourceBase.Get());
			if (Source == nullptr)
			{
				continue;
			}
			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			UHansaBuildingDefinition* Target = Cast<UHansaBuildingDefinition>(
				StaticLoadObject(UHansaBuildingDefinition::StaticClass(), nullptr, *ObjectPath));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored building asset for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			Target->ConstructionCostPfennig = Source->ConstructionCostPfennig;
			Target->CancellationRefundBasisPoints = Source->CancellationRefundBasisPoints;
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate construction cost fields for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return OutSavedFiles.Num() == 14;
	}

	bool MigrateEnhancedConstructionCatalog(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaBuildingDefinition* Source = Cast<UHansaBuildingDefinition>(SourceBase.Get());
			if (Source == nullptr) continue;
			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			UHansaBuildingDefinition* Target = Cast<UHansaBuildingDefinition>(
				StaticLoadObject(UHansaBuildingDefinition::StaticClass(), nullptr, *ObjectPath));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored building asset for %s."), *Source->StableDefinitionId);
				return false;
			}
			Target->SchemaVersion = Source->SchemaVersion;
			Target->bShowInConstructionMenu = Source->bShowInConstructionMenu;
			Target->ConstructionMenuCategory = Source->ConstructionMenuCategory;
			Target->ConstructionMenuOrder = Source->ConstructionMenuOrder;
			Target->ConstructionChainOutputGoodId = Source->ConstructionChainOutputGoodId;
			Target->ConstructionChainStage = Source->ConstructionChainStage;
			Target->ConstructionChainStageCount = Source->ConstructionChainStageCount;
			Target->RequiredConstructionTechnologyId = Source->RequiredConstructionTechnologyId;
			Target->bUpgradeOnly = Source->bUpgradeOnly;
			Target->ConstructionPresentationPurpose = Source->ConstructionPresentationPurpose;
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate EMVP-P03 construction catalog fields for %s."), *Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return OutSavedFiles.Num() == 14;
	}

	bool MigrateMvpResidencePopulationFields(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaBuildingDefinition* Source = Cast<UHansaBuildingDefinition>(SourceBase.Get());
			if (Source == nullptr || Source->ResidentPopulationTierId.IsEmpty()) continue;

			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			UHansaBuildingDefinition* Target = Cast<UHansaBuildingDefinition>(
				StaticLoadObject(UHansaBuildingDefinition::StaticClass(), nullptr, *ObjectPath));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored residence asset for %s."),
					*Source->StableDefinitionId);
				return false;
			}

			Target->ResidenceCapacity = Source->ResidenceCapacity;
			Target->ResidentPopulationTierId = Source->ResidentPopulationTierId;
			Target->UpgradeTargetBuildingId = Source->UpgradeTargetBuildingId;
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate residence population fields for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return OutSavedFiles.Num() == 2;
	}

	bool MigratePhysicalMarketAccessP04(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaBuildingDefinition* Source = Cast<UHansaBuildingDefinition>(SourceBase.Get());
			if (Source == nullptr) continue;
			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			UHansaBuildingDefinition* Target = Cast<UHansaBuildingDefinition>(StaticLoadObject(
				UHansaBuildingDefinition::StaticClass(), nullptr, *(PackageName + TEXT(".") + AssetName)));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored building asset for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			Target->SchemaVersion = Source->SchemaVersion;
			Target->bProvidesMarketAccess = Source->bProvidesMarketAccess;
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target,
				*FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate physical market-access fields for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension()));
		}
		return OutSavedFiles.Num() == 14;
	}

	bool MigrateFisheryRoadRequirementP05(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaBuildingDefinition* Source = Cast<UHansaBuildingDefinition>(SourceBase.Get());
			if (Source == nullptr) continue;
			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			UHansaBuildingDefinition* Target = Cast<UHansaBuildingDefinition>(StaticLoadObject(
				UHansaBuildingDefinition::StaticClass(), nullptr, *(PackageName + TEXT(".") + AssetName)));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored building asset for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			Target->SchemaVersion = Source->SchemaVersion;
			if (Source->StableDefinitionId == TEXT("Building.Fishery"))
			{
				Target->bRequiresRoad = Source->bRequiresRoad;
				Target->AuthoredRevision = Source->AuthoredRevision;
			}
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target,
				*FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate the production-road contract for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension()));
		}
		return OutSavedFiles.Num() == 14;
	}

	bool MigrateMvpShorelinePlacementFields(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaBuildingDefinition* Source = Cast<UHansaBuildingDefinition>(SourceBase.Get());
			if (Source == nullptr || !Source->bRequiresShoreline) continue;

			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			UHansaBuildingDefinition* Target = Cast<UHansaBuildingDefinition>(
				StaticLoadObject(UHansaBuildingDefinition::StaticClass(), nullptr, *ObjectPath));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored shoreline asset for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			Target->bRequiresRoad = Source->bRequiresRoad;
			Target->bRequiresShoreline = Source->bRequiresShoreline;
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate shoreline placement fields for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return OutSavedFiles.Num() == 2;
	}

	bool MigrateIntercityMarketsS09P01(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaCityMarketProfileDefinition* Source =
				Cast<UHansaCityMarketProfileDefinition>(SourceBase.Get());
			if (Source == nullptr)
			{
				continue;
			}

			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			UHansaCityMarketProfileDefinition* Target = Cast<UHansaCityMarketProfileDefinition>(
				StaticLoadObject(UHansaCityMarketProfileDefinition::StaticClass(), nullptr, *ObjectPath));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored city-market asset for %s."),
					*Source->StableDefinitionId);
				return false;
			}

			Target->bMarketOnly = Source->bMarketOnly;
			Target->UpdateCadenceTicks = Source->UpdateCadenceTicks;
			Target->ReportCadenceTicks = Source->ReportCadenceTicks;
			Target->CurrentReportMaxAgeTicks = Source->CurrentReportMaxAgeTicks;
			Target->RecentReportMaxAgeTicks = Source->RecentReportMaxAgeTicks;
			Target->StaleReportMaxAgeTicks = Source->StaleReportMaxAgeTicks;
			Target->EstimatedReportMaxAgeTicks = Source->EstimatedReportMaxAgeTicks;
			Target->PriceHistoryCapacity = Source->PriceHistoryCapacity;
			Target->TargetSmoothingBasisPoints = Source->TargetSmoothingBasisPoints;
			Target->MaximumMovementBasisPointsPerUpdate = Source->MaximumMovementBasisPointsPerUpdate;
			Target->StaleAfterTicks = Source->StaleAfterTicks;
			Target->Goods = Source->Goods;
			Target->AuthoredRevision = Source->AuthoredRevision;
			Target->RefreshContentHash();

			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(
				PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate S09-P01 city-market fields for %s."),
					*Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return OutSavedFiles.Num() == 4;
	}
	bool MigrateScenarioS10P03(TArray<FString>& OutSavedFiles, FString& OutError)
	{
		OutSavedFiles.Reset();
		OutError.Reset();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Sources = CreateMvpDefinitionSet(GetTransientPackage());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& SourceBase : Sources)
		{
			const UHansaDefinitionBase* Source = SourceBase.Get();
			if (!Source->IsA<UHansaScenarioObjectiveDefinition>() && !Source->IsA<UHansaVictoryDefinition>() && !Source->IsA<UHansaScenarioDefinition>()) continue;
			const FString AssetName = AssetNameForDefinition(*Source);
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetName;
			const FString ObjectPath = PackageName + TEXT(".") + AssetName;
			UHansaDefinitionBase* Target = Cast<UHansaDefinitionBase>(StaticLoadObject(Source->GetClass(), nullptr, *ObjectPath));
			if (Target == nullptr || Target->StableDefinitionId != Source->StableDefinitionId)
			{
				OutError = FString::Printf(TEXT("Could not load the exact authored S10-P03 asset for %s."), *Source->StableDefinitionId);
				return false;
			}
			if (const UHansaScenarioObjectiveDefinition* SourceObjective = Cast<UHansaScenarioObjectiveDefinition>(Source))
			{
				UHansaScenarioObjectiveDefinition* TargetObjective = CastChecked<UHansaScenarioObjectiveDefinition>(Target);
				TargetObjective->Metric = SourceObjective->Metric; TargetObjective->TargetValue = SourceObjective->TargetValue;
				TargetObjective->CityId = SourceObjective->CityId; TargetObjective->GoodId = SourceObjective->GoodId;
				TargetObjective->ProgressUnit = SourceObjective->ProgressUnit;
			}
			else if (const UHansaVictoryDefinition* SourceVictory = Cast<UHansaVictoryDefinition>(Source))
			{
				UHansaVictoryDefinition* TargetVictory = CastChecked<UHansaVictoryDefinition>(Target);
				TargetVictory->ObjectiveIds = SourceVictory->ObjectiveIds; TargetVictory->SustainTicks = SourceVictory->SustainTicks;
				TargetVictory->EndingPriority = SourceVictory->EndingPriority; TargetVictory->Summary = SourceVictory->Summary;
			}
			else
			{
				const UHansaScenarioDefinition* SourceScenario = CastChecked<UHansaScenarioDefinition>(Source);
				UHansaScenarioDefinition* TargetScenario = CastChecked<UHansaScenarioDefinition>(Target);
				TargetScenario->HomeCityId = SourceScenario->HomeCityId; TargetScenario->VictoryIds = SourceScenario->VictoryIds;
				TargetScenario->InsolvencyThresholdPfennig = SourceScenario->InsolvencyThresholdPfennig;
				TargetScenario->FailureSustainTicks = SourceScenario->FailureSustainTicks; TargetScenario->Briefing = SourceScenario->Briefing;
			}
			Target->RefreshContentHash();
			UPackage* Package = Target->GetOutermost(); Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone; SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target, *Filename, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to migrate S10-P03 fields for %s."), *Source->StableDefinitionId);
				return false;
			}
			OutSavedFiles.Add(Filename);
		}
		return OutSavedFiles.Num() == 15;
	}

}

UHansaEconomicDefinitionSeedCommandlet::UHansaEconomicDefinitionSeedCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UHansaEconomicDefinitionSeedCommandlet::Main(const FString& Params)
{
	if (FParse::Param(*Params, TEXT("ExpandBeerProductionChain")) ||
		FParse::Param(*Params, TEXT("EnableBeerProductionChain")))
	{
		using namespace Hansa::Editor::EconomicDefinitions;
		const bool bApply = FParse::Param(*Params, TEXT("Apply"));
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Seeds = CreateMvpDefinitionSet(GetTransientPackage());
		TArray<const UHansaDefinitionBase*> SeedPointers;
		for (const auto& Seed : Seeds) SeedPointers.Add(Seed.Get());
		const auto SeedCompile = FHansaEconomicDefinitionCompiler::Compile(SeedPointers);
		Algo::Reverse(SeedPointers);
		const auto ReversedSeedCompile = FHansaEconomicDefinitionCompiler::Compile(SeedPointers);
		if (!SeedCompile.IsValid() || !ReversedSeedCompile.IsValid() ||
			SeedCompile.Registry.GetRegistryHash() != ReversedSeedCompile.Registry.GetRegistryHash()) return 1;
		if (!bApply)
		{
			UE_LOG(LogTemp, Display, TEXT("Expanded beer production chain preview valid: %d definitions; registry %016llX."),
				Seeds.Num(), SeedCompile.Registry.GetRegistryHash());
			return 0;
		}

		TArray<FString> NewFiles;
		FString SaveError;
		if (!SaveMvpDefinitionAssets(false, NewFiles, SaveError))
		{
			UE_LOG(LogTemp, Error, TEXT("%s"), *SaveError);
			return 1;
		}
		const auto FindSeed = [&Seeds](const TCHAR* StableId) -> const UHansaDefinitionBase*
		{
			for (const auto& Seed : Seeds) if (Seed->StableDefinitionId == StableId) return Seed.Get();
			return nullptr;
		};
		const auto LoadTarget = [](const UHansaDefinitionBase& Source) -> UHansaDefinitionBase*
		{
			const FString Name = AssetNameForDefinition(Source);
			const FString Package = PackageDirectoryForDefinition(Source) + TEXT("/") + Name;
			return Cast<UHansaDefinitionBase>(StaticLoadObject(Source.GetClass(), nullptr, *(Package + TEXT(".") + Name)));
		};
		TArray<UHansaDefinitionBase*> Changed;
		const auto RecordIfChanged = [&Changed](UHansaDefinitionBase* Target, const uint64 Before)
		{
			Target->RefreshContentHash();
			if (Target->ContentHash != Before) Changed.Add(Target);
		};

		const auto* RecipeSource = Cast<UHansaRecipeDefinition>(FindSeed(TEXT("Recipe.BrewBeer")));
		auto* RecipeTarget = RecipeSource ? Cast<UHansaRecipeDefinition>(LoadTarget(*RecipeSource)) : nullptr;
		if (!RecipeSource || !RecipeTarget) return 1;
		uint64 Before = RecipeTarget->ComputeDeterministicContentHash();
		RecipeTarget->DisplayName = RecipeSource->DisplayName;
		RecipeTarget->Inputs = RecipeSource->Inputs;
		RecipeTarget->Outputs = RecipeSource->Outputs;
		RecipeTarget->CycleTicks = RecipeSource->CycleTicks;
		RecipeTarget->LaborerWorkforce = RecipeSource->LaborerWorkforce;
		RecipeTarget->ArtisanWorkforce = RecipeSource->ArtisanWorkforce;
		RecipeTarget->bDeclaredSource = RecipeSource->bDeclaredSource;
		RecipeTarget->bDeclaredSink = RecipeSource->bDeclaredSink;
		RecipeTarget->AuthoredRevision = RecipeSource->AuthoredRevision;
		RecordIfChanged(RecipeTarget, Before);

		const auto* BuildingSource = Cast<UHansaBuildingDefinition>(FindSeed(TEXT("Building.Brewery")));
		auto* BuildingTarget = BuildingSource ? Cast<UHansaBuildingDefinition>(LoadTarget(*BuildingSource)) : nullptr;
		if (!BuildingSource || !BuildingTarget) return 1;
		Before = BuildingTarget->ComputeDeterministicContentHash();
		BuildingTarget->bShowInConstructionMenu = BuildingSource->bShowInConstructionMenu;
		BuildingTarget->ConstructionMenuCategory = BuildingSource->ConstructionMenuCategory;
		BuildingTarget->ConstructionMenuOrder = BuildingSource->ConstructionMenuOrder;
		BuildingTarget->ConstructionPresentationPurpose = BuildingSource->ConstructionPresentationPurpose;
		BuildingTarget->ConstructionChainOutputGoodId = BuildingSource->ConstructionChainOutputGoodId;
		BuildingTarget->ConstructionChainStage = BuildingSource->ConstructionChainStage;
		BuildingTarget->ConstructionChainStageCount = BuildingSource->ConstructionChainStageCount;
		BuildingTarget->PresentationMesh = BuildingSource->PresentationMesh;
		BuildingTarget->FootprintHeightCells = BuildingSource->FootprintHeightCells;
		BuildingTarget->AuthoredRevision = BuildingSource->AuthoredRevision;
		RecordIfChanged(BuildingTarget, Before);

		for (const TCHAR* CityId : { TEXT("City.Lubeck"), TEXT("City.Hamburg"), TEXT("City.Luneburg"), TEXT("City.Rostock") })
		{
			const auto* CitySource = Cast<UHansaCityMarketProfileDefinition>(FindSeed(CityId));
			auto* CityTarget = CitySource ? Cast<UHansaCityMarketProfileDefinition>(LoadTarget(*CitySource)) : nullptr;
			if (!CitySource || !CityTarget) return 1;
			Before = CityTarget->ComputeDeterministicContentHash();
			bool bAddedProfile = false;
			for (const FHansaMarketGoodProfile& Profile : CitySource->Goods)
			{
				if (Profile.GoodId != TEXT("Good.Hops") && Profile.GoodId != TEXT("Good.Malt") &&
					Profile.GoodId != TEXT("Good.Barrels")) continue;
				if (!CityTarget->Goods.ContainsByPredicate([&Profile](const FHansaMarketGoodProfile& Existing)
					{ return Existing.GoodId == Profile.GoodId; }))
				{
					CityTarget->Goods.Add(Profile);
					bAddedProfile = true;
				}
			}
			if (bAddedProfile) ++CityTarget->AuthoredRevision;
			RecordIfChanged(CityTarget, Before);
		}

		IAssetRegistry& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Assets.ScanPathsSynchronous({ TEXT("/Game/Hansa/Core") }, true);
		TArray<FAssetData> Rows;
		Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Rows, true, false);
		TArray<const UHansaDefinitionBase*> Definitions;
		for (const auto& Row : Rows)
			if (const auto* Definition = Cast<UHansaDefinitionBase>(Row.GetAsset())) Definitions.Add(Definition);
		const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		Algo::Reverse(Definitions);
		const auto Reversed = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		if (!Compiled.IsValid() || !Reversed.IsValid() ||
			Compiled.Registry.GetRegistryHash() != Reversed.Registry.GetRegistryHash()) return 1;

		for (UHansaDefinitionBase* Target : Changed)
		{
			UPackage* Package = Target->GetOutermost();
			Package->MarkPackageDirty();
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Target,
				*FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args)) return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Expanded beer production chain applied: %d new assets, %d migrated assets, registry %016llX."),
			NewFiles.Num(), Changed.Num(), Compiled.Registry.GetRegistryHash());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("RebalanceStarterEconomy")))
	{
		const bool bApply = FParse::Param(*Params, TEXT("Apply"));
		IAssetRegistry& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Assets.ScanPathsSynchronous({ TEXT("/Game/Hansa/Core") }, true);
		TArray<FAssetData> Rows;
		Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Rows, true, false);
		TArray<const UHansaDefinitionBase*> Definitions;
		TArray<UHansaDefinitionBase*> Changed;
		for (const auto& Row : Rows)
		{
			auto* Definition = Cast<UHansaDefinitionBase>(Row.GetAsset());
			if (!Definition) continue;
			const uint64 Before = Definition->ContentHash;
			if (Hansa::Editor::EconomicDefinitions::ApplyStarterEconomyBalance(Definition) &&
				Before != Definition->ContentHash)
			{
				Definition->AuthoredRevision = Hansa::Editor::EconomicDefinitions::StarterEconomyAuthoredRevision(
					Definition->StableDefinitionId);
				Definition->RefreshContentHash();
				UE_LOG(LogTemp, Display, TEXT("Starter capacity rebalance %s: hash %llu -> %llu (%s)"),
					*Definition->StableDefinitionId, Before, Definition->ContentHash, bApply ? TEXT("apply") : TEXT("preview"));
				Changed.Add(Definition);
			}
			Definitions.Add(Definition);
		}
		const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		if (!Compiled.IsValid()) return 1;
		if (bApply) for (auto* Definition : Changed)
		{
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
			UPackage* Package = Definition->GetOutermost();
			if (!UPackage::SavePackage(Package, Definition,
				*FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args)) return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Starter capacity rebalance validated; %d assets %s."),
			Changed.Num(), bApply ? TEXT("saved") : TEXT("previewed"));
		return 0;
	}
	if (FParse::Param(*Params, TEXT("StageEconomyP33"))) return Hansa::Editor::EconomicDefinitions::StageP33EconomyCandidate();
	if (FParse::Param(*Params, TEXT("ReportCurrentCatalog")))
	{
		// Read-only acceptance evidence: never reseed or repair production definitions.
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({ TEXT("/Game/Hansa/Core") }, true);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Assets, true, false);
		TArray<const UHansaDefinitionBase*> Definitions;
		for (const auto& Asset : Assets)
			if (const auto* Definition = Cast<UHansaDefinitionBase>(Asset.GetAsset())) Definitions.Add(Definition);
		const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		Algo::Reverse(Definitions);
		const auto Reversed = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		if (!Compiled.IsValid() || !Reversed.IsValid() || Compiled.Registry.GetRegistryHash() != Reversed.Registry.GetRegistryHash()) return 1;
		auto Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("reverseOrderVerified"), true);
		Root->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), Compiled.Registry.GetRegistryHash()));
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (const auto& Evidence : Compiled.DefinitionHashes)
		{
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("stableId"), Evidence.StableId);
			Row->SetStringField(TEXT("classPath"), Evidence.DefinitionClassPath);
			Row->SetStringField(TEXT("contentHash"), FString::Printf(TEXT("%016llX"), Evidence.ContentHash));
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("definitions"), Rows);
		FString Json;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
		return FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("CurrentCatalog.json"))) ? 0 : 1;
	}
	if (FParse::Param(*Params, TEXT("MigrateBreadPresentationP10")))
	{
		using namespace Hansa::Editor::EconomicDefinitions;
		const bool bApply = FParse::Param(*Params, TEXT("Apply"));
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Seeds = CreateMvpDefinitionSet(GetTransientPackage());
		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const auto& Seed : Seeds)
		{
			const auto* Source = Cast<UHansaBuildingDefinition>(Seed.Get());
			if (!Source || (Source->StableDefinitionId != TEXT("Building.GrainFarm") && Source->StableDefinitionId != TEXT("Building.Bakery"))) continue;
			const FString PackageName = PackageDirectoryForDefinition(*Source) + TEXT("/") + AssetNameForDefinition(*Source);
			auto* Target = LoadObject<UHansaBuildingDefinition>(nullptr, *(PackageName + TEXT(".") + AssetNameForDefinition(*Source)));
			if (!Target || Target->StableDefinitionId != Source->StableDefinitionId) return 1;
			auto Change = MakeShared<FJsonObject>();
			Change->SetStringField(TEXT("stableId"), Target->StableDefinitionId);
			Change->SetStringField(TEXT("beforeHash"), FString::Printf(TEXT("%016llX"), Target->ComputeDeterministicContentHash()));
			Change->SetStringField(TEXT("beforeMesh"), Target->PresentationMesh.ToSoftObjectPath().ToString());
			Change->SetStringField(TEXT("beforeActor"), Target->PresentationActorClass.ToSoftObjectPath().ToString());
			// Repair only the missing P03 card fields on the P08 farm; preserve economics and text identity.
			Target->SchemaVersion = Source->SchemaVersion;
			Target->bShowInConstructionMenu = Source->bShowInConstructionMenu;
			Target->ConstructionMenuCategory = Source->ConstructionMenuCategory;
			Target->ConstructionMenuOrder = Source->ConstructionMenuOrder;
			Target->ConstructionChainOutputGoodId = Source->ConstructionChainOutputGoodId;
			Target->ConstructionChainStage = Source->ConstructionChainStage;
			Target->ConstructionChainStageCount = Source->ConstructionChainStageCount;
			Target->AuthoredRevision = Source->AuthoredRevision;
			Target->PresentationMesh = Source->PresentationMesh;
			Target->PresentationActorClass = Source->PresentationActorClass;
			Target->RefreshContentHash();
			Change->SetStringField(TEXT("afterHash"), FString::Printf(TEXT("%016llX"), Target->ComputeDeterministicContentHash()));
			Change->SetStringField(TEXT("afterMesh"), Target->PresentationMesh.ToSoftObjectPath().ToString());
			Change->SetStringField(TEXT("afterActor"), Target->PresentationActorClass.ToSoftObjectPath().ToString());
			Changes.Add(MakeShared<FJsonValueObject>(Change));
			if (bApply)
			{
				Target->GetOutermost()->MarkPackageDirty();
				FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
				if (!UPackage::SavePackage(Target->GetOutermost(), Target, *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), Args)) return 1;
			}
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({ TEXT("/Game/Hansa/Core") }, true);
		TArray<FAssetData> Assets; Registry.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Assets, true, false);
		TArray<const UHansaDefinitionBase*> Definitions;
		for (const auto& Asset : Assets) if (const auto* Definition = Cast<UHansaDefinitionBase>(Asset.GetAsset())) Definitions.Add(Definition);
		const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		Algo::Reverse(Definitions);
		const auto Reversed = FHansaEconomicDefinitionCompiler::Compile(Definitions);
		if (!Compiled.IsValid() || !Reversed.IsValid() || Compiled.Registry.GetRegistryHash() != Reversed.Registry.GetRegistryHash()) return 1;
		auto Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("catalogVersion"), 4);
		Root->SetBoolField(TEXT("applied"), bApply);
		Root->SetBoolField(TEXT("reverseOrderVerified"), true);
		Root->SetArrayField(TEXT("changes"), Changes);
		Root->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), Compiled.Registry.GetRegistryHash()));
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (const auto& Evidence : Compiled.DefinitionHashes)
		{
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("stableId"), Evidence.StableId);
			Row->SetStringField(TEXT("classPath"), Evidence.DefinitionClassPath);
			Row->SetStringField(TEXT("contentHash"), FString::Printf(TEXT("%016llX"), Evidence.ContentHash));
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("definitions"), Rows);
		FString Json; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
		const FString Output = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GenerationJobs/hansa-bakery-p10_20260907/evidence"), bApply ? TEXT("catalog_applied.json") : TEXT("catalog_dry_run.json"));
		return FFileHelper::SaveStringToFile(Json, *Output) ? 0 : 1;
	}
	TArray<FString> SavedFiles;
	FString Error;
	if (FParse::Param(*Params, TEXT("MigratePhysicalMarketAccessP04")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigratePhysicalMarketAccessP04(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("P04 physical market-access migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated physical market-access capability on %d MVP building assets."), SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateFisheryRoadRequirementP05")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateFisheryRoadRequirementP05(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("P05 fishery road-requirement migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated the production-road contract on %d MVP building assets."), SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateEnhancedConstructionCatalog")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateEnhancedConstructionCatalog(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("EMVP-P03 construction catalog migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated EMVP-P03 construction catalog fields on %d MVP building assets."), SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateConstructionS06P01")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateMvpBuildingConstructionCosts(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("S06-P01 building construction migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated construction cost/refund fields on %d MVP building assets."),
			SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateResidencePopulation")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateMvpResidencePopulationFields(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("MVP residence population migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated population fields on %d MVP residence assets."),
			SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateShorelinePlacement")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateMvpShorelinePlacementFields(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("MVP shoreline placement migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated placement fields on %d MVP shoreline assets."),
			SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateIntercityMarketsS09P01")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateIntercityMarketsS09P01(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("S09-P01 intercity market migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated S09-P01 fields on %d MVP city-market assets."),
			SavedFiles.Num());
		return 0;
	}
	if (FParse::Param(*Params, TEXT("MigrateScenarioS10P03")))
	{
		if (!Hansa::Editor::EconomicDefinitions::MigrateScenarioS10P03(SavedFiles, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("S10-P03 scenario migration failed: %s"), *Error);
			return 1;
		}
		UE_LOG(LogTemp, Display, TEXT("Migrated S10-P03 fields on %d scenario definition assets."), SavedFiles.Num());
		return 0;
	}
	const bool bReplace = FParse::Param(*Params, TEXT("Replace"));
	if (!Hansa::Editor::EconomicDefinitions::SaveMvpDefinitionAssets(bReplace, SavedFiles, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("S03-P01 economic content authoring failed: %s"), *Error);
		return 1;
	}
	UE_LOG(LogTemp, Display, TEXT("Authored %d missing MVP economic/population definition assets."), SavedFiles.Num());
	return 0;
}
