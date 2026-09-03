#include "Definitions/HansaEconomicDefinitionSeedCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
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
		Market->Goods = MoveTemp(Goods);
		Market->RefreshContentHash();
		Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Market));
	}

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> CreateMvpDefinitionSet(UObject* Outer)
	{
		UObject* EffectiveOuter = Outer != nullptr ? Outer : GetTransientPackage();
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions;
		Definitions.Reserve(43);

		AddGood(Definitions, EffectiveOuter, TEXT("Grain"), TEXT("Grain"), EHansaGoodUnit::Kilogram, 1000, 12000, 25);
		AddGood(Definitions, EffectiveOuter, TEXT("Flour"), TEXT("Flour"), EHansaGoodUnit::Kilogram, 1700, 10500, 75);
		AddGood(Definitions, EffectiveOuter, TEXT("Bread"), TEXT("Bread"), EHansaGoodUnit::Item, 800, 13500, 250);
		AddGood(Definitions, EffectiveOuter, TEXT("Fish"), TEXT("Fish"), EHansaGoodUnit::Kilogram, 1800, 14000, 500);
		AddGood(Definitions, EffectiveOuter, TEXT("Salt"), TEXT("Salt"), EHansaGoodUnit::Kilogram, 2200, 9000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Timber"), TEXT("Timber"), EHansaGoodUnit::Kilogram, 700, 8000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Planks"), TEXT("Planks"), EHansaGoodUnit::Kilogram, 1300, 8500, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Iron"), TEXT("Iron"), EHansaGoodUnit::Kilogram, 2600, 7500, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Tools"), TEXT("Tools"), EHansaGoodUnit::Item, 6500, 10000, 0);
		AddGood(Definitions, EffectiveOuter, TEXT("Beer"), TEXT("Beer"), EHansaGoodUnit::Litre, 1500, 12500, 100);

		AddRecipe(Definitions, EffectiveOuter, TEXT("GrowGrain"), TEXT("Grow grain"), {}, { Amount(TEXT("Good.Grain"), 6000) }, 120, 8, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("MillFlour"), TEXT("Mill flour"), { Amount(TEXT("Good.Grain"), 4000) }, { Amount(TEXT("Good.Flour"), 3000) }, 60, 4, 1);
		AddRecipe(Definitions, EffectiveOuter, TEXT("BakeBread"), TEXT("Bake bread"), { Amount(TEXT("Good.Flour"), 2000) }, { Amount(TEXT("Good.Bread"), 3000) }, 45, 4, 2);
		AddRecipe(Definitions, EffectiveOuter, TEXT("CatchFish"), TEXT("Catch fish"), {}, { Amount(TEXT("Good.Fish"), 4000) }, 90, 8, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("FellTimber"), TEXT("Fell timber"), {}, { Amount(TEXT("Good.Timber"), 6000) }, 100, 8, 0, true);
		AddRecipe(Definitions, EffectiveOuter, TEXT("SawPlanks"), TEXT("Saw planks"), { Amount(TEXT("Good.Timber"), 5000) }, { Amount(TEXT("Good.Planks"), 3500) }, 75, 6, 1);
		AddRecipe(Definitions, EffectiveOuter, TEXT("SmithTools"), TEXT("Smith tools"), { Amount(TEXT("Good.Iron"), 3000) }, { Amount(TEXT("Good.Tools"), 1000) }, 120, 4, 4);
		AddRecipe(Definitions, EffectiveOuter, TEXT("BrewBeer"), TEXT("Brew beer"), { Amount(TEXT("Good.Grain"), 3000) }, { Amount(TEXT("Good.Beer"), 5000) }, 100, 4, 2);

		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Road"), TEXT("Road"), TEXT("Road"), { Amount(TEXT("Good.Timber"), 500) }, {}, 25, 1, 1, 5, 0, 0, 0, 0, false, false);
		UHansaBuildingDefinition* LaborerResidence = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Residence.Laborer"), TEXT("LaborerResidence"), TEXT("Laborer residence"), { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 2000) }, {}, 800, 2, 2, 90, 2000, 12, 0, 0, true, false);
		LaborerResidence->UpgradeTargetBuildingId = TEXT("Building.Residence.Artisan");
		LaborerResidence->ResidentPopulationTierId = TEXT("PopulationTier.Laborer");
		LaborerResidence->RefreshContentHash();
		UHansaBuildingDefinition* ArtisanResidence = AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Residence.Artisan"), TEXT("ArtisanResidence"), TEXT("Artisan residence"), { Amount(TEXT("Good.Planks"), 5000), Amount(TEXT("Good.Tools"), 1000) }, {}, 1400, 2, 2, 140, 3000, 8, 0, 0, true, false);
		ArtisanResidence->ResidentPopulationTierId = TEXT("PopulationTier.Artisan");
		ArtisanResidence->RefreshContentHash();
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Market"), TEXT("Market"), TEXT("Market"), { Amount(TEXT("Good.Planks"), 6000), Amount(TEXT("Good.Tools"), 1000) }, {}, 1800, 3, 3, 180, 50000, 0, 6, 2, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Warehouse"), TEXT("Warehouse"), TEXT("Warehouse"), { Amount(TEXT("Good.Planks"), 10000), Amount(TEXT("Good.Tools"), 2000) }, {}, 2500, 4, 3, 220, 200000, 0, 10, 2, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Dock"), TEXT("Dock"), TEXT("Dock"), { Amount(TEXT("Good.Timber"), 12000), Amount(TEXT("Good.Planks"), 8000), Amount(TEXT("Good.Tools"), 2000) }, {}, 4000, 5, 3, 300, 150000, 0, 16, 4, true, true);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.GrainFarm"), TEXT("GrainFarm"), TEXT("Grain farm"), { Amount(TEXT("Good.Timber"), 3000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.GrowGrain") }, 1200, 4, 4, 150, 30000, 0, 8, 0, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Mill"), TEXT("Mill"), TEXT("Mill"), { Amount(TEXT("Good.Timber"), 4000), Amount(TEXT("Good.Planks"), 3000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.MillFlour") }, 1600, 3, 3, 170, 25000, 0, 4, 1, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Bakery"), TEXT("Bakery"), TEXT("Bakery"), { Amount(TEXT("Good.Planks"), 4000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.BakeBread") }, 1400, 3, 2, 150, 20000, 0, 4, 2, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Fishery"), TEXT("Fishery"), TEXT("Fishery"), { Amount(TEXT("Good.Timber"), 5000), Amount(TEXT("Good.Planks"), 2000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.CatchFish") }, 1500, 3, 2, 150, 30000, 0, 8, 0, true, true);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.LumberCamp"), TEXT("LumberCamp"), TEXT("Lumber camp"), { Amount(TEXT("Good.Timber"), 2000), Amount(TEXT("Good.Tools"), 500) }, { TEXT("Recipe.FellTimber") }, 900, 3, 3, 120, 30000, 0, 8, 0, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Sawmill"), TEXT("Sawmill"), TEXT("Sawmill"), { Amount(TEXT("Good.Timber"), 5000), Amount(TEXT("Good.Tools"), 1000) }, { TEXT("Recipe.SawPlanks") }, 1700, 4, 3, 180, 40000, 0, 6, 1, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Smithy"), TEXT("Smithy"), TEXT("Smithy and tool workshop"), { Amount(TEXT("Good.Planks"), 5000), Amount(TEXT("Good.Iron"), 3000) }, { TEXT("Recipe.SmithTools") }, 2400, 3, 3, 220, 25000, 0, 4, 4, true, false);
		AddBuilding(Definitions, EffectiveOuter, TEXT("Building.Brewery"), TEXT("Brewery"), TEXT("Brewery"), { Amount(TEXT("Good.Planks"), 6000), Amount(TEXT("Good.Tools"), 1500) }, { TEXT("Recipe.BrewBeer") }, 2200, 4, 3, 220, 50000, 0, 4, 2, true, false);

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
			return TArray<FHansaMarketGoodProfile> {
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
		};
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Lubeck"), TEXT("Lübeck market"), CityGoods(TEXT("Lubeck")));
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Hamburg"), TEXT("Hamburg market"), CityGoods(TEXT("Hamburg")));
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Luneburg"), TEXT("Lüneburg market"), CityGoods(TEXT("Luneburg")));
		AddCityMarket(Definitions, EffectiveOuter, TEXT("Rostock"), TEXT("Rostock market"), CityGoods(TEXT("Rostock")));

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
	TArray<FString> SavedFiles;
	FString Error;
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
	const bool bReplace = FParse::Param(*Params, TEXT("Replace"));
	if (!Hansa::Editor::EconomicDefinitions::SaveMvpDefinitionAssets(bReplace, SavedFiles, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("S03-P01 economic content authoring failed: %s"), *Error);
		return 1;
	}
	UE_LOG(LogTemp, Display, TEXT("Authored %d missing MVP economic/population definition assets."), SavedFiles.Num());
	return 0;
}
