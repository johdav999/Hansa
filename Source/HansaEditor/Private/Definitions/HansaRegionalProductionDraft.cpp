#include "Definitions/HansaRegionalProductionDraft.h"

#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPresenceDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"

namespace Hansa::Editor::RegionalProduction
{
	namespace
	{
		void Base(UHansaDefinitionBase& D,const FString& Id,const FString& Name)
		{
			D.StableDefinitionId=Id;D.DisplayName=FText::FromString(Name);D.LocalizationKey=FName(*(TEXT("Game.")+Id+TEXT(".Name")));D.ContentSet=TEXT("MVP");D.AuthoredRevision=1;D.Tags={TEXT("MVP"),TEXT("RegionalProduction")};
		}

		FHansaProductionChainStageDefinition Stage(const TCHAR* Key,const TCHAR* Recipe,EHansaProductionStageRole Role,TArray<FString> Prerequisites={})
		{
			FHansaProductionChainStageDefinition S;S.StageKey=Key;S.RecipeId=Recipe;S.Role=Role;S.PrerequisiteStageKeys=MoveTemp(Prerequisites);S.IntendedConstructionTier=Role==EHansaProductionStageRole::FinishedGoods?TEXT("Craftsmen"):TEXT("DayLaborers");return S;
		}

		FHansaCityIndustryBindingDefinition Industry(const TCHAR* Chain,TArray<FString> Stages,int32 Rank=0,int32 Cycles=1)
		{
			FHansaCityIndustryBindingDefinition B;B.ProductionChainId=Chain;B.EnabledStageKeys=MoveTemp(Stages);B.SignatureRank=Rank;B.CyclesPerMarketUpdate=Cycles;B.EfficiencyBasisPoints=10000+Rank*1000;B.InputReserveMilliUnits=6000;B.OutputReserveMilliUnits=30000;return B;
		}
	}

	bool ApplyDraft(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions,FString& OutError)
	{
		auto Find=[&](const FString& Id)->UHansaDefinitionBase*{for(const auto& D:Definitions)if(D->StableDefinitionId==Id)return D.Get();return nullptr;};
		if(Find(TEXT("Region.WendishLowerElbe"))){OutError=TEXT("Regional economy already applied.");return false;}
		for(const TCHAR* Id:{TEXT("Recipe.GrowGrain"),TEXT("Recipe.SaltedCatch"),TEXT("Recipe.SplitFirewood"),TEXT("Recipe.BurnCharcoal"),TEXT("Recipe.TanLeather"),TEXT("Recipe.MakeShoes")})if(!Find(Id)){OutError=TEXT("Required accepted recipe missing: ")+FString(Id);return false;}

		auto AddSourceRecipe=[&](const TCHAR* Name,const TCHAR* Label,const TCHAR* Good,int64 Quantity)
		{
			auto* R=DuplicateObject<UHansaRecipeDefinition>(CastChecked<UHansaRecipeDefinition>(Find(TEXT("Recipe.GrowGrain"))),GetTransientPackage());Base(*R,TEXT("Recipe.")+FString(Name),Label);R->Inputs.Reset();R->Outputs={{Good,Quantity}};R->CycleTicks=120;R->LaborerWorkforce=4;R->ArtisanWorkforce=0;R->bDeclaredSource=true;R->InternalCatchRecipeId.Reset();R->RefreshContentHash();Definitions.Emplace(R);
		};
		AddSourceRecipe(TEXT("ExtractSalt"),TEXT("Extract salt"),TEXT("Good.Salt"),5000);
		AddSourceRecipe(TEXT("SourceIron"),TEXT("Supply iron"),TEXT("Good.Iron"),3000);
		AddSourceRecipe(TEXT("SourceRawHides"),TEXT("Supply raw hides"),TEXT("Good.RawHides"),4000);
		AddSourceRecipe(TEXT("StripTanningBark"),TEXT("Strip tanning bark"),TEXT("Good.TanningBark"),2500);

		auto AddChain=[&](const TCHAR* Name,const TCHAR* Label,TArray<FHansaProductionChainStageDefinition> Stages)
		{
			auto* C=NewObject<UHansaProductionChainDefinition>(GetTransientPackage());Base(*C,TEXT("ProductionChain.")+FString(Name),Label);C->Stages=MoveTemp(Stages);C->RefreshContentHash();Definitions.Emplace(C);
		};
		AddChain(TEXT("Bread"),TEXT("Bread"),{Stage(TEXT("GrowGrain"),TEXT("Recipe.GrowGrain"),EHansaProductionStageRole::Source),Stage(TEXT("MillFlour"),TEXT("Recipe.MillFlour"),EHansaProductionStageRole::IntermediateProcessing,{TEXT("GrowGrain")}),Stage(TEXT("BakeBread"),TEXT("Recipe.BakeBread"),EHansaProductionStageRole::FinishedGoods,{TEXT("MillFlour")})});
		AddChain(TEXT("FreshFish"),TEXT("Fresh fish"),{Stage(TEXT("CatchFish"),TEXT("Recipe.CatchFish"),EHansaProductionStageRole::Source)});
		AddChain(TEXT("PreservedFish"),TEXT("Preserved fish"),{Stage(TEXT("CatchFish"),TEXT("Recipe.CatchFish"),EHansaProductionStageRole::Source),Stage(TEXT("SaltedCatch"),TEXT("Recipe.SaltedCatch"),EHansaProductionStageRole::FinishedGoods,{TEXT("CatchFish")})});
		AddChain(TEXT("Planks"),TEXT("Planks"),{Stage(TEXT("FellTimber"),TEXT("Recipe.FellTimber"),EHansaProductionStageRole::Source),Stage(TEXT("SawPlanks"),TEXT("Recipe.SawPlanks"),EHansaProductionStageRole::FinishedGoods,{TEXT("FellTimber")})});
		AddChain(TEXT("Beer"),TEXT("Beer"),{Stage(TEXT("GrowGrain"),TEXT("Recipe.GrowGrain"),EHansaProductionStageRole::Source),Stage(TEXT("GrowHops"),TEXT("Recipe.GrowHops"),EHansaProductionStageRole::Source),Stage(TEXT("MaltGrain"),TEXT("Recipe.MaltGrain"),EHansaProductionStageRole::IntermediateProcessing,{TEXT("GrowGrain")}),Stage(TEXT("MakeBarrels"),TEXT("Recipe.MakeBarrels"),EHansaProductionStageRole::IntermediateProcessing),Stage(TEXT("BrewBeer"),TEXT("Recipe.BrewBeer"),EHansaProductionStageRole::FinishedGoods,{TEXT("GrowHops"),TEXT("MaltGrain"),TEXT("MakeBarrels")})});
		AddChain(TEXT("Firewood"),TEXT("Firewood"),{Stage(TEXT("FellTimber"),TEXT("Recipe.FellTimber"),EHansaProductionStageRole::Source),Stage(TEXT("SplitFirewood"),TEXT("Recipe.SplitFirewood"),EHansaProductionStageRole::FinishedGoods,{TEXT("FellTimber")})});
		AddChain(TEXT("Charcoal"),TEXT("Charcoal"),{Stage(TEXT("FellTimber"),TEXT("Recipe.FellTimber"),EHansaProductionStageRole::Source),Stage(TEXT("BurnCharcoal"),TEXT("Recipe.BurnCharcoal"),EHansaProductionStageRole::FinishedGoods,{TEXT("FellTimber")})});
		AddChain(TEXT("Tools"),TEXT("Tools"),{Stage(TEXT("SourceIron"),TEXT("Recipe.SourceIron"),EHansaProductionStageRole::Source),Stage(TEXT("BurnCharcoal"),TEXT("Recipe.BurnCharcoal"),EHansaProductionStageRole::IntermediateProcessing),Stage(TEXT("SmithTools"),TEXT("Recipe.SmithTools"),EHansaProductionStageRole::FinishedGoods,{TEXT("SourceIron"),TEXT("BurnCharcoal")})});
		AddChain(TEXT("Shoes"),TEXT("Shoes"),{Stage(TEXT("SourceRawHides"),TEXT("Recipe.SourceRawHides"),EHansaProductionStageRole::Source),Stage(TEXT("StripTanningBark"),TEXT("Recipe.StripTanningBark"),EHansaProductionStageRole::Source),Stage(TEXT("TanLeather"),TEXT("Recipe.TanLeather"),EHansaProductionStageRole::IntermediateProcessing,{TEXT("SourceRawHides"),TEXT("StripTanningBark")}),Stage(TEXT("MakeShoes"),TEXT("Recipe.MakeShoes"),EHansaProductionStageRole::FinishedGoods,{TEXT("TanLeather")})});
		AddChain(TEXT("Salt"),TEXT("Salt"),{Stage(TEXT("ExtractSalt"),TEXT("Recipe.ExtractSalt"),EHansaProductionStageRole::Source)});

		struct FCity{const TCHAR* Id;const TCHAR* Label;const TCHAR* Region;TArray<FHansaCityIndustryBindingDefinition> Industries;};
		const TArray<FCity> Cities={
			{TEXT("Lubeck"),TEXT("Lübeck market"),TEXT("Region.WendishLowerElbe"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("MillFlour"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("MaltGrain"),TEXT("MakeBarrels"),TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("SaltedCatch")}),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("TanLeather"),TEXT("MakeShoes")})}},
			{TEXT("Hamburg"),TEXT("Hamburg market"),TEXT("Region.WendishLowerElbe"),{Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1,2),Industry(TEXT("ProductionChain.Bread"),{TEXT("MillFlour"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("BrewBeer")}),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("MakeShoes")})}},
			{TEXT("Luneburg"),TEXT("Lüneburg market"),TEXT("Region.WendishLowerElbe"),{Industry(TEXT("ProductionChain.Salt"),{TEXT("ExtractSalt")},1,2),Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")}),Industry(TEXT("ProductionChain.Beer"),{TEXT("GrowGrain"),TEXT("MaltGrain"),TEXT("BrewBeer")})}},
			{TEXT("Rostock"),TEXT("Rostock market"),TEXT("Region.WendishLowerElbe"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")},1,2),Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1,2),Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber"),TEXT("SawPlanks")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides"),TEXT("StripTanningBark"),TEXT("TanLeather")})}},
			{TEXT("Wismar"),TEXT("Wismar market"),TEXT("Region.WendishLowerElbe"),{Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("GrowHops"),TEXT("BrewBeer")}),Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("StripTanningBark"),TEXT("TanLeather")})}},
			{TEXT("Stralsund"),TEXT("Stralsund market"),TEXT("Region.WendishLowerElbe"),{Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("CatchFish"),TEXT("SaltedCatch")},1),Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("StripTanningBark"),TEXT("TanLeather")})}},
			{TEXT("Danzig"),TEXT("Danzig market"),TEXT("Region.PrussianPomeranian"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("MillFlour"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("MakeBarrels"),TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("SaltedCatch")}),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")})}},
			{TEXT("Elbing"),TEXT("Elbing market"),TEXT("Region.PrussianPomeranian"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("MillFlour")},1),Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber"),TEXT("SawPlanks")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("MakeBarrels")})}},
			{TEXT("Konigsberg"),TEXT("Königsberg market"),TEXT("Region.PrussianPomeranian"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")},1),Industry(TEXT("ProductionChain.Firewood"),{TEXT("FellTimber"),TEXT("SplitFirewood")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides"),TEXT("StripTanningBark"),TEXT("TanLeather")})}},
			{TEXT("Thorn"),TEXT("Thorn market"),TEXT("Region.PrussianPomeranian"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("GrowHops"),TEXT("MaltGrain")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("MakeShoes")})}},
			{TEXT("Stettin"),TEXT("Stettin market"),TEXT("Region.PrussianPomeranian"),{Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber"),TEXT("SawPlanks")},1),Industry(TEXT("ProductionChain.Charcoal"),{TEXT("BurnCharcoal")},1),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")}),Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")})}},
			{TEXT("Greifswald"),TEXT("Greifswald market"),TEXT("Region.PrussianPomeranian"),{Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("SaltedCatch")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides"),TEXT("MakeShoes")})}},
			{TEXT("Riga"),TEXT("Riga market"),TEXT("Region.LivonianRus"),{Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber"),TEXT("SawPlanks")},1),Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("MillFlour")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("TanLeather"),TEXT("MakeShoes")}),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")})}},
			{TEXT("Reval"),TEXT("Reval market"),TEXT("Region.LivonianRus"),{Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("SaltedCatch")},1),Industry(TEXT("ProductionChain.Planks"),{TEXT("SawPlanks")})}},
			{TEXT("Dorpat"),TEXT("Dorpat market"),TEXT("Region.LivonianRus"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")},1),Industry(TEXT("ProductionChain.Firewood"),{TEXT("FellTimber"),TEXT("SplitFirewood")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides")})}},
			{TEXT("Narva"),TEXT("Narva market"),TEXT("Region.LivonianRus"),{Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber"),TEXT("SawPlanks")},1),Industry(TEXT("ProductionChain.Charcoal"),{TEXT("BurnCharcoal")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("StripTanningBark")})}},
			{TEXT("Pskov"),TEXT("Pskov market"),TEXT("Region.LivonianRus"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides"),TEXT("TanLeather")},1),Industry(TEXT("ProductionChain.Firewood"),{TEXT("SplitFirewood")})}},
			{TEXT("Novgorod"),TEXT("Novgorod market"),TEXT("Region.LivonianRus"),{Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides"),TEXT("StripTanningBark")},1),Industry(TEXT("ProductionChain.Tools"),{TEXT("SourceIron"),TEXT("SmithTools")})}},
			{TEXT("Bergen"),TEXT("Bergen market"),TEXT("Region.Scandinavian"),{Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("CatchFish"),TEXT("SaltedCatch")},1,2),Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides")})}},
			{TEXT("Oslo"),TEXT("Oslo market"),TEXT("Region.Scandinavian"),{Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber"),TEXT("SawPlanks")},1),Industry(TEXT("ProductionChain.Firewood"),{TEXT("SplitFirewood")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("SourceRawHides"),TEXT("TanLeather")})}},
			{TEXT("Stockholm"),TEXT("Stockholm market"),TEXT("Region.Scandinavian"),{Industry(TEXT("ProductionChain.Tools"),{TEXT("SourceIron"),TEXT("BurnCharcoal"),TEXT("SmithTools")},1,2),Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.Planks"),{TEXT("SawPlanks")})}},
			{TEXT("Visby"),TEXT("Visby market"),TEXT("Region.Scandinavian"),{Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("SaltedCatch")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("BrewBeer")})}},
			{TEXT("Kalmar"),TEXT("Kalmar market"),TEXT("Region.Scandinavian"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("MillFlour")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("GrowHops"),TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.Planks"),{TEXT("FellTimber")})}},
			{TEXT("Malmo"),TEXT("Malmö market"),TEXT("Region.Scandinavian"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("GrowHops"),TEXT("MaltGrain"),TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("MakeShoes")})}},
			{TEXT("Bruges"),TEXT("Bruges market"),TEXT("Region.WesternNorthSea"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("MillFlour"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("TanLeather"),TEXT("MakeShoes")}),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")})}},
			{TEXT("Antwerp"),TEXT("Antwerp market"),TEXT("Region.WesternNorthSea"),{Industry(TEXT("ProductionChain.Beer"),{TEXT("MaltGrain"),TEXT("MakeBarrels"),TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")},1),Industry(TEXT("ProductionChain.Bread"),{TEXT("BakeBread")})}},
			{TEXT("London"),TEXT("London market"),TEXT("Region.WesternNorthSea"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("MillFlour"),TEXT("BakeBread")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.Tools"),{TEXT("SmithTools")}),Industry(TEXT("ProductionChain.Shoes"),{TEXT("MakeShoes")})}},
			{TEXT("Boston"),TEXT("Boston market"),TEXT("Region.WesternNorthSea"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain"),TEXT("MillFlour")},1),Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("GrowHops")})}},
			{TEXT("KingsLynn"),TEXT("King's Lynn market"),TEXT("Region.WesternNorthSea"),{Industry(TEXT("ProductionChain.Bread"),{TEXT("GrowGrain")},1),Industry(TEXT("ProductionChain.PreservedFish"),{TEXT("CatchFish"),TEXT("SaltedCatch")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("MakeBarrels")})}},
			{TEXT("Kampen"),TEXT("Kampen market"),TEXT("Region.WesternNorthSea"),{Industry(TEXT("ProductionChain.FreshFish"),{TEXT("CatchFish")},1),Industry(TEXT("ProductionChain.Beer"),{TEXT("BrewBeer")},1),Industry(TEXT("ProductionChain.Shoes"),{TEXT("TanLeather"),TEXT("MakeShoes")})}}
		};

		auto* Template=CastChecked<UHansaCityMarketProfileDefinition>(Find(TEXT("City.Lubeck")));
		for(const auto& Spec:Cities)
		{
			auto* City=Cast<UHansaCityMarketProfileDefinition>(Find(TEXT("City.")+FString(Spec.Id)));
			if(!City){City=DuplicateObject<UHansaCityMarketProfileDefinition>(Template,GetTransientPackage());Base(*City,TEXT("City.")+FString(Spec.Id),Spec.Label);City->bMarketOnly=true;City->ReportCadenceTicks=20;City->RecentReportMaxAgeTicks=4;City->EstimatedReportMaxAgeTicks=19;for(auto& Good:City->Goods){Good.InitialStockMilliUnits=Good.DesiredReserveMilliUnits;Good.BackgroundCitizenDemandMilliUnitsPerUpdate=700;Good.BackgroundIndustrialDemandMilliUnitsPerUpdate=300;}Definitions.Emplace(City);}
			struct FReviewedCity { const TCHAR* Id; int32 Longitude; int32 Latitude; EHansaCityPresentationClass Presentation; };
			static const FReviewedCity ReviewedCities[] = {
				{TEXT("City.Lubeck"),10687,53868,EHansaCityPresentationClass::RenderedBuildable},
				{TEXT("City.Hamburg"),9993,53550,EHansaCityPresentationClass::MarketOnly},
				{TEXT("City.Luneburg"),10415,53249,EHansaCityPresentationClass::MarketOnly},
				{TEXT("City.Rostock"),12140,54089,EHansaCityPresentationClass::RenderedVisitable},
				{TEXT("City.Wismar"),11466,53891,EHansaCityPresentationClass::MarketOnly},
				{TEXT("City.Stralsund"),13091,54315,EHansaCityPresentationClass::MarketOnly}
			};
			for (const FReviewedCity& Reviewed : ReviewedCities) if (City->StableDefinitionId == Reviewed.Id)
			{
				City->PresentationClass=Reviewed.Presentation;
				City->bMarketOnly=Reviewed.Presentation==EHansaCityPresentationClass::MarketOnly;
				City->MapLongitudeMilliDegrees=Reviewed.Longitude;City->MapLatitudeMilliDegrees=Reviewed.Latitude;break;
			}
			City->RegionId=Spec.Region;City->IndustryBindings=Spec.Industries;for(auto& Good:City->Goods)Good.BackgroundProductionMilliUnitsPerUpdate=0;++City->AuthoredRevision;City->RefreshContentHash();
		}

		// Reviewed abstract cities expose reports, public trade and route access, but no fabricated
		// station site, leased plot, specialization or visit action. Their inventory remains physical.
		for (const TCHAR* CityName : {TEXT("Wismar"),TEXT("Stralsund"),TEXT("Danzig"),TEXT("Bergen")})
		{
			auto* Policy=NewObject<UHansaCityTradePolicyDefinition>(GetTransientPackage());
			Base(*Policy,TEXT("CityTradePolicy.")+FString(CityName),FString(CityName)+TEXT(" abstract trade policy"));
			Policy->Tags.Add(TEXT("TradePresence"));Policy->CityId=TEXT("City.")+FString(CityName);
			Policy->InitialStageId=TEXT("PresenceStage.VisitingContact");Policy->AllowedStageIds={TEXT("PresenceStage.VisitingContact")};
			Policy->AllowedPlotCategories.Reset();Policy->AllowedBuildingCategories.Reset();Policy->TradeStationSites.Reset();
			Policy->Specializations.Reset();Policy->Privileges.Reset();Policy->CityProjects.Reset();Policy->bPublicMarketAccess=true;
			Policy->RefreshContentHash();Definitions.Emplace(Policy);
		}
		if (auto* BalticRoute=Cast<UHansaRouteDefinition>(Find(TEXT("Route.BalticSea"))))
		{
			for (const FHansaRouteConnectionDefinition Connection : {
				FHansaRouteConnectionDefinition{TEXT("City.Lubeck"),TEXT("City.Wismar"),8},
				FHansaRouteConnectionDefinition{TEXT("City.Lubeck"),TEXT("City.Stralsund"),12},
				FHansaRouteConnectionDefinition{TEXT("City.Lubeck"),TEXT("City.Danzig"),24},
				FHansaRouteConnectionDefinition{TEXT("City.Lubeck"),TEXT("City.Bergen"),30}})
			{
				BalticRoute->Connections.Add(Connection);
			}
			++BalticRoute->AuthoredRevision;BalticRoute->RefreshContentHash();
		}

		struct FRegion{const TCHAR* Id;const TCHAR* Label;TArray<FString> Cities;TArray<TTuple<FString,EHansaResourceEndowment,int64>> Resources;};
		const TArray<FRegion> Regions={
			{TEXT("WendishLowerElbe"),TEXT("Wendish and Lower Elbe"),{TEXT("City.Lubeck"),TEXT("City.Hamburg"),TEXT("City.Luneburg"),TEXT("City.Rostock"),TEXT("City.Wismar"),TEXT("City.Stralsund")},{{TEXT("Good.Grain"),EHansaResourceEndowment::Abundant,18000},{TEXT("Good.Fish"),EHansaResourceEndowment::Abundant,16000},{TEXT("Good.Timber"),EHansaResourceEndowment::Available,18000},{TEXT("Good.Salt"),EHansaResourceEndowment::Signature,12000},{TEXT("Good.RawHides"),EHansaResourceEndowment::Available,8000},{TEXT("Good.TanningBark"),EHansaResourceEndowment::Available,7000},{TEXT("Good.Iron"),EHansaResourceEndowment::ImportOnly,3000}}},
			{TEXT("PrussianPomeranian"),TEXT("Prussian and Pomeranian"),{TEXT("City.Danzig"),TEXT("City.Elbing"),TEXT("City.Konigsberg"),TEXT("City.Thorn"),TEXT("City.Stettin"),TEXT("City.Greifswald")},{{TEXT("Good.Grain"),EHansaResourceEndowment::Signature,22000},{TEXT("Good.Fish"),EHansaResourceEndowment::Available,12000},{TEXT("Good.Timber"),EHansaResourceEndowment::Abundant,22000},{TEXT("Good.RawHides"),EHansaResourceEndowment::Available,9000},{TEXT("Good.TanningBark"),EHansaResourceEndowment::Available,8000},{TEXT("Good.Iron"),EHansaResourceEndowment::ImportOnly,3000}}},
			{TEXT("LivonianRus"),TEXT("Livonian and Rus"),{TEXT("City.Riga"),TEXT("City.Reval"),TEXT("City.Dorpat"),TEXT("City.Narva"),TEXT("City.Pskov"),TEXT("City.Novgorod")},{{TEXT("Good.Grain"),EHansaResourceEndowment::Available,16000},{TEXT("Good.Fish"),EHansaResourceEndowment::Available,10000},{TEXT("Good.Timber"),EHansaResourceEndowment::Signature,26000},{TEXT("Good.RawHides"),EHansaResourceEndowment::Abundant,12000},{TEXT("Good.TanningBark"),EHansaResourceEndowment::Abundant,11000},{TEXT("Good.Iron"),EHansaResourceEndowment::Available,6000}}},
			{TEXT("Scandinavian"),TEXT("Scandinavian"),{TEXT("City.Bergen"),TEXT("City.Oslo"),TEXT("City.Stockholm"),TEXT("City.Visby"),TEXT("City.Kalmar"),TEXT("City.Malmo")},{{TEXT("Good.Grain"),EHansaResourceEndowment::Available,12000},{TEXT("Good.Fish"),EHansaResourceEndowment::Signature,26000},{TEXT("Good.Timber"),EHansaResourceEndowment::Abundant,24000},{TEXT("Good.RawHides"),EHansaResourceEndowment::Available,9000},{TEXT("Good.TanningBark"),EHansaResourceEndowment::Available,7000},{TEXT("Good.Iron"),EHansaResourceEndowment::Signature,9000}}},
			{TEXT("WesternNorthSea"),TEXT("Western North Sea"),{TEXT("City.Bruges"),TEXT("City.Antwerp"),TEXT("City.London"),TEXT("City.Boston"),TEXT("City.KingsLynn"),TEXT("City.Kampen")},{{TEXT("Good.Grain"),EHansaResourceEndowment::Abundant,22000},{TEXT("Good.Fish"),EHansaResourceEndowment::Available,14000},{TEXT("Good.Timber"),EHansaResourceEndowment::ImportOnly,4000},{TEXT("Good.RawHides"),EHansaResourceEndowment::ImportOnly,4000},{TEXT("Good.TanningBark"),EHansaResourceEndowment::ImportOnly,3000},{TEXT("Good.Iron"),EHansaResourceEndowment::ImportOnly,3000}}}
		};
		for(const auto& Spec:Regions)
		{
			auto* Region=NewObject<UHansaRegionEconomicProfileDefinition>(GetTransientPackage());Base(*Region,TEXT("Region.")+FString(Spec.Id),Spec.Label);Region->MemberCityIds=Spec.Cities;Region->ExchangeCapacityMilliUnitsPerUpdate=30000;Region->ExchangeDelayUpdates=1;Region->TransportLossBasisPoints=100;
			for(const auto& Chain:Definitions)if(auto* C=Cast<UHansaProductionChainDefinition>(Chain.Get())){FHansaRegionPermittedStageDefinition P;P.ProductionChainId=C->StableDefinitionId;for(const auto& S:C->Stages)P.StageKeys.Add(S.StageKey);Region->PermittedStages.Add(MoveTemp(P));}
			for(const auto& Resource:Spec.Resources){FHansaRegionResourceEndowmentDefinition E;E.GoodId=Resource.Get<0>();E.Endowment=Resource.Get<1>();E.SourceCapacityMilliUnitsPerUpdate=Resource.Get<2>();Region->ResourceEndowments.Add(E);}Region->RefreshContentHash();Definitions.Emplace(Region);
		}
		return true;
	}
}
