#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/SHansaContextInspector.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"
#include "InputCoreTypes.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResidenceStockCauseTest,
 "Hansa.UI.ResidenceInspector.ExhaustedStockHasSpecificRemedy",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaResidenceStockCauseTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 FHansaPopulationCohortProjection Home;
 Home.bResidenceOperational=true;Home.bHasMarketAccess=true;Home.Residents=2;
 FHansaPopulationNeedState Bread;
 Bread.NeedId=FHansaNeedId::TryParse(TEXT("Need.Bread")).Value;
 Bread.GoodId=FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
 Bread.AffordabilityBasisPoints=10000;
 Home.Needs.Add(Bread);
 auto Cause=MakeResidenceCausalPresentation(Home);
 TestTrue(TEXT("Connected house identifies unavailable stock"),Cause.Cause.ToString().Contains(TEXT("no unreserved")));
 TestTrue(TEXT("Bread remedy explains the missing bakery"),Cause.Remedy.ToString().Contains(TEXT("Bakery")));
 Home.bHasMarketAccess=false;
 Cause=MakeResidenceCausalPresentation(Home);
 TestEqual(TEXT("Disconnected home still reports road access"),Cause.StableCode,FName(TEXT("ResidenceNoMarketAccess")));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResidenceInspectorDataTest,"Hansa.UI.ResidenceInspector.AuthoritativeOccupancyAndNeeds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaResidenceInspectorDataTest::RunTest(const FString&){
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Initialize population fixture"),Host->InitializeForLubeck(nullptr,Error)))return false;
 Host->AdvanceTicks(5);auto Projection=Host->BuildProjection();
 const int64 DiagnosticTick=Host->GetSimulationTick();
 TestTrue(TEXT("Economy diagnostic snapshot can run without advancing time"),Host->AdvanceRealTime(0.0));
 TestEqual(TEXT("Diagnostic logging preserves simulation tick"),Host->GetSimulationTick(),DiagnosticTick);
 if(!TestTrue(TEXT("Residence projection exists"),Projection && !Projection.Value.GetPopulationCohorts().IsEmpty()))return false;
 auto P=Projection.Value.GetPopulationCohorts()[0];auto* Registry=Host->GetEconomicRegistry();
 TestTrue(TEXT("Fixture contains multiple residences"),Projection.Value.GetPopulationCohorts().Num()>1);
 for(const auto& CityTotal:Projection.Value.GetCitizenConsumption().Goods){
  int64 Required=0,Consumed=0;
  for(const auto& Home:Projection.Value.GetPopulationCohorts())for(const auto& Total:Home.Consumption.Goods)
   if(Total.CityId==CityTotal.CityId && Total.GoodId==CityTotal.GoodId){Required+=Total.Required;Consumed+=Total.Consumed;}
  TestEqual(TEXT("House histories partition city demand exactly"),Required,CityTotal.Required);
  TestEqual(TEXT("House histories partition city consumption exactly"),Consumed,CityTotal.Consumed);
 }

 TStrongObjectPtr<UHansaInspectorPresentationModel> M(NewObject<UHansaInspectorPresentationModel>());M->InitializeDefaults();
 TestTrue(TEXT("Show actual house"),M->ShowResidence(P,*Registry,TEXT("Test")));
 auto W=SNew(Hansa::UI::SHansaContextInspector).Model(M.Get());
 TestEqual(TEXT("Residents match authoritative cohort"),M->GetSnapshot().Residence.Residents,P.Residents);
 TestEqual(TEXT("Capacity matches authoritative house"),M->GetSnapshot().Residence.Capacity,P.ResidenceCapacity);
 TestFalse(TEXT("New house begins with compact details"),M->GetSnapshot().bCauseExpanded);
 for(const auto& N:P.Needs){
  const auto* D=M->GetSnapshot().Residence.Needs.FindByPredicate([&](const auto& V){return V.NeedId==FName(*N.NeedId.ToString());});
  if(!TestNotNull(TEXT("Need is presented"),D))return false;
  if(D->bService)TestEqual(TEXT("Service retains current metric"),D->Fulfillment,N.SatisfactionBasisPoints);
  else {
   const auto* T=P.Consumption.Goods.FindByPredicate([&](const auto& V){return V.GoodId==N.GoodId;});
   TestTrue(TEXT("Product quantities use only this residence"),T && D->Required==T->Required && D->Consumed==T->Consumed);
  }
 }
 TestEqual(TEXT("Residence duration is authoritative"),M->GetSnapshot().Residence.CoveredMinutes,P.Consumption.CoveredMinutes);
 TestTrue(TEXT("Portrait is a native semantic target"),W->ResolveSemanticWidget(TEXT("Inspector.Residence.Portrait")).IsValid());
 TestTrue(TEXT("Need rows can receive controller focus"),W->FocusSemanticId(TEXT("Inspector.Residence.Need.Bread")));
 TestTrue(TEXT("Details can receive controller focus"),W->FocusSemanticId(TEXT("Inspector.Action.OpenCause")));
 TestTrue(TEXT("Details opens through normal action"),W->ActivateSemanticId(TEXT("Inspector.Action.OpenCause")) && M->GetSnapshot().bCauseExpanded);
 TestTrue(TEXT("Details closes through normal action"),W->ActivateSemanticId(TEXT("Inspector.Action.OpenCause")) && !M->GetSnapshot().bCauseExpanded);
 const auto Bread=Hansa::Simulation::FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
 P.Consumption.CoveredMinutes=6*1440;P.Consumption.Goods={{P.CityId,Bread,10000,7000}};
 M->ShowResidence(P,*Registry,TEXT("Test"));
 auto FindBread=[&](){return M->GetSnapshot().Residence.Needs.FindByPredicate([](const auto& N){return N.GoodId==TEXT("Good.Bread");});};
 if(!TestNotNull(TEXT("Bread row exists"),FindBread()))return false;
 TestEqual(TEXT("70 percent from summed quantities"),FindBread()->Percent.ToString(),FText::AsPercent(.7).ToString());
 TestEqual(TEXT("Consumed quantity"),FindBread()->Consumed,int64(7000));
 TestEqual(TEXT("Required quantity"),FindBread()->Required,int64(10000));
 TestTrue(TEXT("Partial period visible"),M->GetSnapshot().Residence.ConsumptionPeriod.ToString().Contains(TEXT("6")));
 auto Nodes=W->GetSemanticSnapshot();
 TestTrue(TEXT("Semantics contain residence quantities"),Nodes.ContainsByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Residence.Need.Bread")&&N.State.Value.Contains(TEXT("consumedMilliUnits=7000"));}));
 P.Residents=0;M->ShowResidence(P,*Registry,TEXT("Test"));
 TestEqual(TEXT("Former residents' actual consumption is retained"),FindBread()->Consumed,int64(7000));
 P.Consumption.CoveredMinutes=43200;P.Consumption.bFullWindow=true;P.Consumption.Goods[0].Required=0;P.Consumption.Goods[0].Consumed=0;
 M->ShowResidence(P,*Registry,TEXT("Test"));
 TestTrue(TEXT("Empty measured home shows no demand, no fake zero percent"),FindBread()->bKnown&&FindBread()->Percent.ToString()==TEXT("—")&&FindBread()->Amount.ToString().Contains(TEXT("No demand")));
 TestEqual(TEXT("Full duration label"),M->GetSnapshot().Residence.ConsumptionPeriod.ToString(),FString(TEXT("Last 30 days")));
 P.Consumption={};
 P.Needs.Reset();P.Residents=0;
 M->ShowResidence(P,*Registry,TEXT("Test"));
 TestTrue(TEXT("Unevaluated house retains authored need rows"),!M->GetSnapshot().Residence.Needs.IsEmpty());
 for(const auto& N:M->GetSnapshot().Residence.Needs)TestFalse(TEXT("Unevaluated need is pending, not fake zero fulfillment"),N.bKnown);
 TestEqual(TEXT("Empty house occupancy is explicit"),M->GetSnapshot().Residence.Residents,0);
 TestTrue(TEXT("Supplied empty house explicitly reports that it is accepting residents"),
  W->GetSemanticSnapshot().ContainsByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Residence.ConsumptionPeriod")&&N.State.Value.Contains(TEXT("accepting residents"));}));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructedResidenceInspectorWithoutMarketTest,
 "Hansa.UI.ResidenceInspector.ConstructedHomeWithoutMarketUsesCompactPanel",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaConstructedResidenceInspectorWithoutMarketTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
 FString Error;
 if(!TestTrue(TEXT("Empty city initializes"),Host->InitializeForLubeck(nullptr,Error,EHansaRuntimeScenario::EmptyLubeckBuild)))
 {
  AddError(Error);return false;
 }

 FHansaPlacementSpec Home;
 Home.CityId=Host->GetCityId();
 Home.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Residence.Laborer")).Value;
 bool bPlaced=false;
 for(int32 Y=0;Y<60&&!bPlaced;++Y)for(int32 X=0;X<60&&!bPlaced;++X)
 {
  Home.Anchor={X,Y};
  const auto Validation=Host->ValidatePlacement(Home);
  if(!Validation&&Validation.GetReasons().Num()==1&&Validation.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired)
  {
   FHansaPlacementSpec Road=Home;
   Road.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
   --Road.Anchor.X;
   if(Host->ValidatePlacement(Road))Host->PlaceBuildings({Road});
  }
  if(Host->ValidatePlacement(Home))bPlaced=Host->PlaceBuildings({Home}).IsSuccess();
 }
 if(!TestTrue(TEXT("Residence is placed through the player command gateway"),bPlaced))return false;

 auto Projection=Host->BuildProjection();
 const auto* WorldHome=Projection.Value.GetBuildingWorldProjections().FindByPredicate([](const auto& Building)
 {
  return Building.Placement.BuildingDefinitionId.ToString()==TEXT("Building.Residence.Laborer");
 });
 if(!TestNotNull(TEXT("Placed residence has a world projection"),WorldHome))return false;
 const FHansaBuildingId HomeId=WorldHome->BuildingId;
 const auto* Definition=Host->FindBuildingDefinition(TEXT("Building.Residence.Laborer"));
 if(!TestNotNull(TEXT("Residence definition exists"),Definition))return false;
 if(!TestTrue(TEXT("Residence construction completes"),Host->AdvanceTicks(Definition->BuildTicks)))return false;

 Projection=Host->BuildProjection();
 const auto* CompletedHome=Projection.Value.GetBuildingWorldProjections().FindByPredicate([&](const auto& Building)
 {
  return Building.BuildingId==HomeId;
 });
 const auto* Cohort=Projection.Value.GetPopulationCohorts().FindByPredicate([&](const auto& Value)
 {
  return Value.ResidenceBuildingId==HomeId;
 });
 if(!TestNotNull(TEXT("Completed residence remains projected"),CompletedHome) ||
    !TestNotNull(TEXT("Completed residence gets a cohort without a physical Market"),Cohort))return false;
 TestFalse(TEXT("Missing physical Market remains an explicit access state"),Cohort->bHasMarketAccess);

 TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());
 Model->InitializeDefaults();Model->BindRuntime(Host.Get());
 Model->ShowWorldBuilding(TEXT("Building.Residence.Laborer"),FText::FromString(TEXT("Laborer residence")),FText(),
  int64(HomeId.GetValue()),LexToString(CompletedHome->Status),LexToString(CompletedHome->ProductionBlocker),TEXT("World.Selection"));
 TestTrue(TEXT("Completed home selects compact residence data"),Model->GetSnapshot().Residence.bValid);
 TestEqual(TEXT("The residence cause names missing physical Market access"),
  Model->GetSnapshot().Causal.StableCode,FName(TEXT("ResidenceNoMarketAccess")));
 TestTrue(TEXT("The residence remedy explains the shared completed-road requirement"),
  Model->GetSnapshot().Causal.Remedy.ToString().Contains(TEXT("same completed road network")));
 auto Widget=SNew(Hansa::UI::SHansaContextInspector).Model(Model.Get());
 TestTrue(TEXT("Completed home renders the compact residence panel"),
  Widget->ResolveSemanticWidget(TEXT("Inspector.Residence.Portrait")).IsValid());
 TestTrue(TEXT("The compact status exposes the migration blocker without opening Details"),
  Widget->GetSemanticSnapshot().ContainsByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Residence.ConsumptionPeriod")&&N.State.Value.Contains(TEXT("No Market access"));}));
 return !HasAnyErrors();
}
#endif
