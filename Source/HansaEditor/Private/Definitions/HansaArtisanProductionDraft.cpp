#include "Definitions/HansaArtisanProductionDraft.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"

namespace Hansa::Editor::ArtisanProduction
{
bool ApplyDraft(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, FString& OutError)
{
 auto Find = [&](const TCHAR* Id) -> UHansaDefinitionBase* {
  for (const auto& D : Definitions) if (D->StableDefinitionId == Id) return D.Get();
  return nullptr;
 };
 // Check before mutating so a repeated application cannot duplicate content.
 for (const TCHAR* Id : {TEXT("Good.Charcoal"),TEXT("Good.RawHides"),TEXT("Good.TanningBark"),TEXT("Good.Leather"),TEXT("Good.Shoes"),
  TEXT("Recipe.BurnCharcoal"),TEXT("Recipe.TanLeather"),TEXT("Recipe.MakeShoes"),
  TEXT("Building.CharcoalBurner"),TEXT("Building.Tannery"),TEXT("Building.Shoemaker"),TEXT("Need.Shoes")})
  if (Find(Id)) { OutError = TEXT("Artisan draft already contains ") + FString(Id); return false; }
 for (const TCHAR* Id : {TEXT("Good.Timber"),TEXT("Good.Tools"),TEXT("Recipe.SmithTools"),TEXT("Building.Smithy"),
  TEXT("Building.Cooperage"),TEXT("Need.Tools"),TEXT("PopulationTier.Artisan")})
  if (!Find(Id)) { OutError = TEXT("Required base definition missing: ") + FString(Id); return false; }
 for (const auto& D : Definitions)
  if (D->GetOutermost() != GetTransientPackage()) { OutError = TEXT("Draft authoring requires transient copies."); return false; }

 auto Clone = [&](const TCHAR* SourceId, const TCHAR* Id, const TCHAR* Label) -> UHansaDefinitionBase* {
  auto* D = DuplicateObject<UHansaDefinitionBase>(Find(SourceId), GetTransientPackage());
  D->StableDefinitionId = Id; D->DisplayName = FText::ChangeKey(TEXT("Hansa.ArtisanProduction"), Id, FText::FromString(Label));
  D->LocalizationKey = FName(*(FString(TEXT("Game.")) + Id + TEXT(".Name")));
  D->AuthoredRevision = 1; Definitions.Emplace(D); return D;
 };
 auto Amount = [](const TCHAR* Id, int64 Raw) { FHansaGoodAmount A; A.GoodId=Id; A.QuantityMilliUnits=Raw; return A; };
 struct GoodSpec { const TCHAR* Id; const TCHAR* Name; int64 Price; bool Item; };
 const GoodSpec Goods[] = {
  {TEXT("Good.Charcoal"),TEXT("Charcoal"),1200,false},
  {TEXT("Good.RawHides"),TEXT("Raw hides"),2000,false},
  {TEXT("Good.TanningBark"),TEXT("Tanning bark"),600,false},
  {TEXT("Good.Leather"),TEXT("Leather"),4500,false},
  {TEXT("Good.Shoes"),TEXT("Shoes"),8000,true}
 };
 for (const auto& G : Goods)
 {
  auto* D=CastChecked<UHansaGoodDefinition>(Clone(G.Item?TEXT("Good.Tools"):TEXT("Good.Timber"),G.Id,G.Name));
  D->BaseValueMilliMarks=G.Price; D->SpoilageBasisPointsPerDay=0; D->bSpoilageEnabled=false; D->Icon.Reset();
 }
 auto Recipe = [&](UHansaRecipeDefinition* R, TArray<FHansaGoodAmount> Inputs,TArray<FHansaGoodAmount> Outputs,int32 Ticks,int32 Labor,int32 Craft) {
  R->Inputs=MoveTemp(Inputs); R->Outputs=MoveTemp(Outputs); R->CycleTicks=Ticks;
  R->LaborerWorkforce=Labor; R->ArtisanWorkforce=Craft;
  R->bDeclaredSource=false; R->bDeclaredSink=false; R->InternalCatchRecipeId.Reset();
 };
 auto* Smith=CastChecked<UHansaRecipeDefinition>(Find(TEXT("Recipe.SmithTools")));
 ++Smith->AuthoredRevision;
 Recipe(Smith,{Amount(TEXT("Good.Iron"),1500),Amount(TEXT("Good.Charcoal"),500)},{Amount(TEXT("Good.Tools"),1000)},120,0,4);
 Recipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.SmithTools"),TEXT("Recipe.BurnCharcoal"),TEXT("Burn charcoal"))),
  {Amount(TEXT("Good.Timber"),6000)},{Amount(TEXT("Good.Charcoal"),1500)},120,2,0);
 Recipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.SmithTools"),TEXT("Recipe.TanLeather"),TEXT("Tan leather"))),
  {Amount(TEXT("Good.RawHides"),3000),Amount(TEXT("Good.TanningBark"),1000)},{Amount(TEXT("Good.Leather"),2000)},160,0,3);
 Recipe(CastChecked<UHansaRecipeDefinition>(Clone(TEXT("Recipe.SmithTools"),TEXT("Recipe.MakeShoes"),TEXT("Make shoes"))),
  {Amount(TEXT("Good.Leather"),1000)},{Amount(TEXT("Good.Shoes"),1000)},80,0,2);

 struct BuildingSpec { const TCHAR* Id; const TCHAR* Name; const TCHAR* Recipe; const TCHAR* Output; int32 Stage; int32 Count; int32 Labor; int32 Craft; int32 Width; int32 Height; int64 Cost; const TCHAR* Purpose; };
 const BuildingSpec Buildings[] = {
  {TEXT("Building.CharcoalBurner"),TEXT("Charcoal burner's hut"),TEXT("Recipe.BurnCharcoal"),TEXT("Good.Charcoal"),1,1,2,0,3,3,1800,TEXT("Convert delivered timber into charcoal for smithies.")},
  {TEXT("Building.Smithy"),TEXT("Smithy"),TEXT("Recipe.SmithTools"),TEXT("Good.Tools"),1,1,0,4,3,3,2800,TEXT("Forge imported iron bars with locally produced charcoal into tools.")},
  {TEXT("Building.Tannery"),TEXT("Tannery"),TEXT("Recipe.TanLeather"),TEXT("Good.Shoes"),1,2,0,3,4,3,2400,TEXT("Process imported raw hides and tanning bark into leather for shoemakers.")},
  {TEXT("Building.Shoemaker"),TEXT("Shoemaker's workshop"),TEXT("Recipe.MakeShoes"),TEXT("Good.Shoes"),2,2,0,2,2,3,2000,TEXT("Make shoes from delivered leather for artisan households.")}
 };
 for (const auto& S : Buildings)
 {
  auto* B=Cast<UHansaBuildingDefinition>(Find(S.Id));
  if (B) ++B->AuthoredRevision;
  else B=CastChecked<UHansaBuildingDefinition>(Clone(TEXT("Building.Cooperage"),S.Id,S.Name));
  B->RecipeIds={S.Recipe}; B->ConstructionTier=S.Craft?EHansaConstructionTier::Craftsmen:EHansaConstructionTier::DayLaborers;
  B->ConstructionMenuCategory=EHansaConstructionMenuCategory::Production; B->bShowInConstructionMenu=true;
  B->ConstructionChainOutputGoodId=S.Output; B->ConstructionChainStage=S.Stage; B->ConstructionChainStageCount=S.Count;
  B->ConstructionMenuOrder=S.Stage-1; B->ConstructionPresentationPurpose=FText::FromString(S.Purpose);
  B->LaborerWorkforce=S.Labor; B->ArtisanWorkforce=S.Craft; B->FootprintWidthCells=S.Width; B->FootprintHeightCells=S.Height;
  B->ConstructionCostPfennig=S.Cost; B->BuildTicks=120; B->StorageCapacityMilliUnits=80000;
  B->ConstructionCosts={Amount(TEXT("Good.Planks"),6000),Amount(TEXT("Good.Timber"),4000),Amount(TEXT("Good.Tools"),500)};
  B->bRequiresRoad=true; B->bRequiresShoreline=false; B->bProvidesMarketAccess=false;
  B->RequiredConstructionTechnologyId.Reset(); B->bUpgradeOnly=false; B->UpgradeTargetBuildingId.Reset();
  B->ResidentialCompound.Reset(); B->PresentationActorClass.Reset();
  // Inherited meshes are economy-preview stand-ins only. The staging commandlet
  // records them explicitly; this function never authorizes runtime promotion.
 }
 auto* Shoes=CastChecked<UHansaNeedDefinition>(Clone(TEXT("Need.Tools"),TEXT("Need.Shoes"),TEXT("Shoes")));
 Shoes->GoodId=TEXT("Good.Shoes"); Shoes->Alternatives.Reset(); Shoes->bSeasonal=false;
 auto* Artisan=CastChecked<UHansaPopulationTierDefinition>(Find(TEXT("PopulationTier.Artisan")));
 FHansaPopulationTierNeed N; N.NeedId=TEXT("Need.Shoes"); N.ConsumptionMilliUnitsPerResidentPerTick=1; N.ImportanceBasisPoints=1000;
 Artisan->Needs.Add(N); ++Artisan->AuthoredRevision;
 for (const auto& D : Definitions)
 {
  if (auto* City=Cast<UHansaCityMarketProfileDefinition>(D.Get()))
  {
   for (const auto& G : Goods)
   {
    FHansaMarketGoodProfile P; P.GoodId=G.Id; P.InitialPriceMilliMarks=G.Price;
    const bool Imported=P.GoodId==TEXT("Good.RawHides")||P.GoodId==TEXT("Good.TanningBark");
    P.InitialStockMilliUnits=Imported?30000:10000; P.DesiredReserveMilliUnits=20000;
    if (City->bMarketOnly)
    {
     P.BackgroundProductionMilliUnitsPerUpdate=Imported?1500:250;
     P.BackgroundCitizenDemandMilliUnitsPerUpdate=P.GoodId==TEXT("Good.Shoes")?125:0;
    }
    City->Goods.Add(P);
   }
   ++City->AuthoredRevision;
  }
  D->RefreshContentHash();
 }
 return true;
}
}
