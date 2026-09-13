#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
#include "Misc/AutomationTest.h"
#include "Fixtures/HansaProductionFixture.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/SHansaMarketTable.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarketProductionContract,"Hansa.UI.MarketTable.ProductionConsumptionAndBuildingContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMarketProductionContract::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 auto Created=FHansaProductionFixture::TryCreateGrainShortage();if(!Created)return false;
 auto Fixture=Created.Value;if(!Fixture.Step(5))return false;
 auto Projection=Fixture.BuildProjection();auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
 const auto* Registry=Fixture.GetDefinitions().GetEconomicRegistry();if(!Projection || !City || !Registry)return false;
 TStrongObjectPtr<UHansaMarketTablePresentationModel> Model(NewObject<UHansaMarketTablePresentationModel>());
 Model->InitializeDefaults();Model->ApplyProjection(Projection.Value,*Registry,City.Value);
 int64 Revealed=0;Model->OnBuildingRequested().AddLambda([&](FName,int64 Building){Revealed=Building;});
 auto Quantity=[](int64 Raw){return FString::Printf(TEXT("%.1f"),double(Raw)/1000.0);};
 int32 Links=0;
 for(const auto& Row:Model->GetSnapshot().AllRows){
  Model->SelectGoodIntent(Row.GoodStableId);const auto& Detail=Model->GetSnapshot().SelectedGood;
  auto Good=FHansaGoodId::TryParse(Row.GoodStableId.ToString());if(!Good)continue;
  const auto* Market=Projection.Value.GetMarkets().FindByPredicate([&](const auto& M){return M.CityId==City.Value && M.GoodId==Good.Value;});
  if(!Market){TestEqual(TEXT("Unreported production stays unknown"),Detail.Production.ToString(),FString(TEXT("—")));continue;}
  TestEqual(TEXT("Production equals authoritative recent local production"),Detail.Production.ToString(),Quantity(Market->RecentLocalProduction.GetRawValue()));
  int64 Total=0;for(const auto& C:Projection.Value.GetMarketConsumers())if(C.CityId==City.Value && C.GoodId==Good.Value)Total+=C.FulfilledLastTick.GetRawValue();
  TestEqual(TEXT("Consumption equals fulfilled quantities, not demand"),Detail.Consumption.ToString(),Quantity(Total));
  TestFalse(TEXT("Supply balance explains shortage or surplus"),Detail.SupplyBalance.IsEmpty());
  for(const auto& R:Detail.Producers){
   Revealed=0;TestEqual(TEXT("Only a world building can be revealed"),Model->RevealRelationshipIntent(true,R.StableId),R.BuildingValue>0);
   if(R.BuildingValue>0){
    ++Links;TestEqual(TEXT("Reveal preserves exact building identity"),Revealed,R.BuildingValue);
    const auto* Building=Projection.Value.GetBuildingWorldProjections().FindByPredicate([&](const auto& B){return int64(B.BuildingId.GetValue())==R.BuildingValue;});
    if(Building){FString Name=Building->Placement.BuildingDefinitionId.ToString();int32 Separator;if(Name.FindLastChar(TEXT('.'),Separator))Name=Name.Mid(Separator+1);
     TestEqual(TEXT("Producer names the actual building instead of a production-system index"),R.Label.ToString(),FName::NameToDisplayString(Name,false));}
    else TestEqual(TEXT("Missing building identity is explicitly unavailable"),R.Label.ToString(),FString(TEXT("Building details unavailable")));
   }
  }
 }
 TestTrue(TEXT("Fixture covers at least one building relationship"),Links>0);
 TestFalse(TEXT("Arbitrary relationship IDs cannot navigate"),Model->RevealRelationshipIntent(true,TEXT("Unknown")));
 auto Table=SNew(Hansa::UI::SHansaMarketTable).Model(Model.Get());
 Model->SelectGoodIntent(TEXT("Good.Grain"));int32 Before=Table->GetListRefreshCountForTesting();
 Model->SelectGoodIntent(TEXT("Good.Bread"));
 TestEqual(TEXT("Selection preserves virtualized row instances"),Table->GetListRefreshCountForTesting(),Before);
 Model->SetSearchTextIntent(FText::FromString(TEXT("no-match")));
 TestFalse(TEXT("Filtered row cannot receive semantic focus"),Table->FocusSemanticId(TEXT("Market.Row.Good_Grain")));
 TestFalse(TEXT("Filtered row cannot activate"),Table->ActivateSemanticId(TEXT("Market.Row.Good_Grain")));
 return !HasAnyErrors();
}
#endif
