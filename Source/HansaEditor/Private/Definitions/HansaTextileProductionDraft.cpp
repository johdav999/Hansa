#include "Definitions/HansaTextileProductionDraft.h"

#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"

namespace Hansa::Editor::TextileProduction
{
bool ApplyDraft(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, FString& OutError)
{
	auto Find = [&](const TCHAR* Id) -> UHansaDefinitionBase*
	{
		for (const auto& Definition : Definitions)
		{
			if (Definition->StableDefinitionId == Id) return Definition.Get();
		}
		return nullptr;
	};

	static const TCHAR* AddedIds[] = {
		TEXT("Good.Flax"), TEXT("Good.Hemp"), TEXT("Good.Beeswax"), TEXT("Good.LinenCloth"),
		TEXT("Good.LinenClothing"), TEXT("Good.Candles"), TEXT("Good.Rope"),
		TEXT("Recipe.WeaveLinen"), TEXT("Recipe.SewLinenClothing"), TEXT("Recipe.DipCandles"),
		TEXT("Recipe.LayHempRope"), TEXT("Recipe.LayFlaxRope"),
		TEXT("Building.Weaver"), TEXT("Building.Tailor"), TEXT("Building.Chandler"), TEXT("Building.Ropewalk"),
		TEXT("Need.LinenClothing"), TEXT("Need.Candles")
	};
	for (const TCHAR* Id : AddedIds)
	{
		if (Find(Id)) { OutError = TEXT("Textile production draft already contains ") + FString(Id); return false; }
	}
	for (const TCHAR* Id : {TEXT("Good.Timber"), TEXT("Good.Tools"), TEXT("Recipe.MakeShoes"),
		TEXT("Building.Cooperage"), TEXT("Need.Tools"), TEXT("PopulationTier.Artisan")})
	{
		if (!Find(Id)) { OutError = TEXT("Required accepted definition missing: ") + FString(Id); return false; }
	}
	for (const auto& Definition : Definitions)
	{
		if (Definition->GetOutermost() != GetTransientPackage())
		{
			OutError = TEXT("Textile production authoring requires transient copies.");
			return false;
		}
	}

	auto Clone = [&](const TCHAR* SourceId, const TCHAR* Id, const TCHAR* Label) -> UHansaDefinitionBase*
	{
		auto* Definition = DuplicateObject<UHansaDefinitionBase>(Find(SourceId), GetTransientPackage());
		Definition->StableDefinitionId = Id;
		Definition->DisplayName = FText::ChangeKey(TEXT("Hansa.TextileProduction"), Id, FText::FromString(Label));
		Definition->LocalizationKey = FName(*(FString(TEXT("Game.")) + Id + TEXT(".Name")));
		Definition->AuthoredRevision = 1;
		Definitions.Emplace(Definition);
		return Definition;
	};
	auto Amount = [](const TCHAR* Id, const int64 Raw)
	{
		FHansaGoodAmount Value; Value.GoodId = Id; Value.QuantityMilliUnits = Raw; return Value;
	};

	struct FGoodSpec { const TCHAR* Id; const TCHAR* Name; int64 Price; bool bItem; };
	const FGoodSpec Goods[] = {
		{TEXT("Good.Flax"), TEXT("Flax"), 1400, false},
		{TEXT("Good.Hemp"), TEXT("Hemp"), 1500, false},
		{TEXT("Good.Beeswax"), TEXT("Beeswax"), 3000, false},
		{TEXT("Good.LinenCloth"), TEXT("Linen cloth"), 4200, false},
		{TEXT("Good.LinenClothing"), TEXT("Linen clothing"), 8500, true},
		{TEXT("Good.Candles"), TEXT("Candles"), 6500, true},
		{TEXT("Good.Rope"), TEXT("Rope"), 4800, true}
	};
	for (const FGoodSpec& Spec : Goods)
	{
		auto* Good = CastChecked<UHansaGoodDefinition>(Clone(Spec.bItem ? TEXT("Good.Tools") : TEXT("Good.Timber"), Spec.Id, Spec.Name));
		Good->BaseValueMilliMarks = Spec.Price;
		Good->SpoilageBasisPointsPerDay = 0;
		Good->bSpoilageEnabled = false;
		Good->Icon.Reset();
	}

	auto ConfigureRecipe = [&](UHansaRecipeDefinition* Recipe, TArray<FHansaGoodAmount> Inputs,
		TArray<FHansaGoodAmount> Outputs, const int32 Ticks, const int32 Artisans)
	{
		Recipe->Inputs = MoveTemp(Inputs);
		Recipe->Outputs = MoveTemp(Outputs);
		Recipe->CycleTicks = Ticks;
		Recipe->LaborerWorkforce = 0;
		Recipe->ArtisanWorkforce = Artisans;
		Recipe->bDeclaredSource = false;
		Recipe->bDeclaredSink = false;
		Recipe->InternalCatchRecipeId.Reset();
	};
	ConfigureRecipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeShoes"), TEXT("Recipe.WeaveLinen"), TEXT("Weave linen"))),
		{Amount(TEXT("Good.Flax"), 2000)}, {Amount(TEXT("Good.LinenCloth"), 1000)}, 120, 3);
	ConfigureRecipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeShoes"), TEXT("Recipe.SewLinenClothing"), TEXT("Sew linen clothing"))),
		{Amount(TEXT("Good.LinenCloth"), 1500)}, {Amount(TEXT("Good.LinenClothing"), 1000)}, 100, 2);
	// Linen wicks are included in the workshop's operating supplies rather than modeled as a separate commodity.
	ConfigureRecipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeShoes"), TEXT("Recipe.DipCandles"), TEXT("Dip candles"))),
		{Amount(TEXT("Good.Beeswax"), 2000)}, {Amount(TEXT("Good.Candles"), 1000)}, 120, 2);
	ConfigureRecipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeShoes"), TEXT("Recipe.LayHempRope"), TEXT("Lay hemp rope"))),
		{Amount(TEXT("Good.Hemp"), 2000)}, {Amount(TEXT("Good.Rope"), 1000)}, 140, 3);
	ConfigureRecipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.MakeShoes"), TEXT("Recipe.LayFlaxRope"), TEXT("Lay flax rope"))),
		{Amount(TEXT("Good.Flax"), 2500)}, {Amount(TEXT("Good.Rope"), 1000)}, 160, 3);

	struct FBuildingSpec
	{
		const TCHAR* Id; const TCHAR* Name; TArray<FString> Recipes; const TCHAR* ChainOutput;
		int32 Stage; int32 Count; int32 Artisans; int32 Width; int32 Height; int64 Price;
		int32 Planks; int32 Timber; int32 Tools; const TCHAR* Purpose;
	};
	const FBuildingSpec Buildings[] = {
		{TEXT("Building.Weaver"), TEXT("Weaver"), {TEXT("Recipe.WeaveLinen")}, TEXT("Good.LinenClothing"),
			1, 2, 3, 3, 3, 2400, 7000, 4000, 500, TEXT("Weave delivered prepared flax fibre into linen cloth for tailors.")},
		{TEXT("Building.Tailor"), TEXT("Tailor"), {TEXT("Recipe.SewLinenClothing")}, TEXT("Good.LinenClothing"),
			2, 2, 2, 3, 2, 2000, 5000, 3000, 500, TEXT("Cut and sew delivered linen cloth into clothing for craftsmen households.")},
		{TEXT("Building.Chandler"), TEXT("Chandler"), {TEXT("Recipe.DipCandles")}, TEXT("Good.Candles"),
			1, 1, 2, 3, 2, 2100, 5000, 3500, 500, TEXT("Dip linen-wicked candles from delivered beeswax; wick supplies are abstracted.")},
		{TEXT("Building.Ropewalk"), TEXT("Ropewalk"), {TEXT("Recipe.LayHempRope"), TEXT("Recipe.LayFlaxRope")}, TEXT("Good.Rope"),
			1, 1, 3, 8, 2, 3200, 10000, 6000, 1000, TEXT("Lay rope from either prepared hemp fibre or prepared flax fibre using a selected recipe.")}
	};
	for (const FBuildingSpec& Spec : Buildings)
	{
		auto* Building = CastChecked<UHansaBuildingDefinition>(Clone(TEXT("Building.Cooperage"), Spec.Id, Spec.Name));
		Building->RecipeIds = Spec.Recipes;
		// Card ownership is explicit and never inferred from ingredients or helper workers.
		Building->ConstructionTier = EHansaConstructionTier::Craftsmen;
		Building->ConstructionMenuCategory = EHansaConstructionMenuCategory::Production;
		Building->bShowInConstructionMenu = true;
		Building->ConstructionChainOutputGoodId = Spec.ChainOutput;
		Building->ConstructionChainStage = Spec.Stage;
		Building->ConstructionChainStageCount = Spec.Count;
		Building->ConstructionMenuOrder = Spec.Stage - 1;
		Building->ConstructionPresentationPurpose = FText::ChangeKey(TEXT("Hansa.TextileProduction"), FString(Spec.Id) + TEXT(".Purpose"), FText::FromString(Spec.Purpose));
		Building->LaborerWorkforce = 0;
		Building->ArtisanWorkforce = Spec.Artisans;
		Building->FootprintWidthCells = Spec.Width;
		Building->FootprintHeightCells = Spec.Height;
		Building->ConstructionCostPfennig = Spec.Price;
		Building->BuildTicks = Spec.Id == FString(TEXT("Building.Ropewalk")) ? 160 : 120;
		Building->StorageCapacityMilliUnits = Spec.Id == FString(TEXT("Building.Ropewalk")) ? 120000 : 80000;
		Building->ConstructionCosts = {Amount(TEXT("Good.Planks"), Spec.Planks), Amount(TEXT("Good.Timber"), Spec.Timber), Amount(TEXT("Good.Tools"), Spec.Tools)};
		Building->bRequiresRoad = true;
		Building->bRequiresShoreline = false;
		Building->bProvidesMarketAccess = false;
		Building->RequiredConstructionTechnologyId.Reset();
		Building->bUpgradeOnly = false;
		Building->UpgradeTargetBuildingId.Reset();
		Building->ResidentialCompound.Reset();
		Building->PresentationActorClass.Reset();
		// Keep the accepted cooperage mesh as an explicit transient placeholder.
		// The staging commandlet replaces it only after verified workshop imports exist.
	}

	auto AddNeed = [&](const TCHAR* Id, const TCHAR* Name, const TCHAR* GoodId, const int32 Importance)
	{
		auto* Need = CastChecked<UHansaNeedDefinition>(Clone(TEXT("Need.Tools"), Id, Name));
		Need->GoodId = GoodId;
		Need->Alternatives.Reset();
		Need->bSeasonal = false;
		auto* Artisan = CastChecked<UHansaPopulationTierDefinition>(Find(TEXT("PopulationTier.Artisan")));
		FHansaPopulationTierNeed Entry;
		Entry.NeedId = Id;
		Entry.ConsumptionMilliUnitsPerResidentPerTick = 1;
		Entry.ImportanceBasisPoints = Importance;
		Artisan->Needs.Add(Entry);
		++Artisan->AuthoredRevision;
	};
	AddNeed(TEXT("Need.LinenClothing"), TEXT("Linen clothing"), TEXT("Good.LinenClothing"), 750);
	AddNeed(TEXT("Need.Candles"), TEXT("Candles"), TEXT("Good.Candles"), 500);

	for (const auto& Definition : Definitions)
	{
		if (auto* City = Cast<UHansaCityMarketProfileDefinition>(Definition.Get()))
		{
			for (const FGoodSpec& Spec : Goods)
			{
				FHansaMarketGoodProfile Profile;
				Profile.GoodId = Spec.Id;
				Profile.InitialPriceMilliMarks = Spec.Price;
				const bool bImportedFibre = Profile.GoodId == TEXT("Good.Flax") || Profile.GoodId == TEXT("Good.Hemp") || Profile.GoodId == TEXT("Good.Beeswax");
				Profile.InitialStockMilliUnits = bImportedFibre ? 30000 : 10000;
				Profile.DesiredReserveMilliUnits = 20000;
				if (City->bMarketOnly)
				{
					Profile.BackgroundProductionMilliUnitsPerUpdate = bImportedFibre ? 1500 : 250;
					Profile.BackgroundCitizenDemandMilliUnitsPerUpdate =
						(Profile.GoodId == TEXT("Good.LinenClothing") || Profile.GoodId == TEXT("Good.Candles")) ? 100 :
						Profile.GoodId == TEXT("Good.Rope") ? 75 : 0;
				}
				City->Goods.Add(Profile);
			}
			++City->AuthoredRevision;
		}
		Definition->RefreshContentHash();
	}
	return true;
}
}
