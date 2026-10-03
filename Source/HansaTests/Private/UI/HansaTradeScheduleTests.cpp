#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Fixtures/HansaProductionFixture.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "UI/HansaTradeSchedulePresentation.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
using namespace Hansa::Simulation;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeScheduleAuthority,"Hansa.UI.TradeSchedule.AuthoritativeJourney",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeScheduleAuthority::RunTest(const FString&) {
 auto F=FHansaProductionFixture::TryCreateRouteDelivery();if(!F)return false;const auto* Registry=F.Value.GetDefinitions().GetEconomicRegistry();if(!Registry)return false;
 const auto Initial=F.Value.BuildProjection();if(!Initial||Initial.Value.GetRoutes().IsEmpty())return false;const auto RouteId=Initial.Value.GetRoutes()[0].Id;
 TestTrue(TEXT("Start fixture through authoritative route command"),!!F.Value.SetRouteActive(RouteId,true));
 bool Travel=false,Receipt=false,Berth=false;FString Evidence;
 for(int32 Tick=0;Tick<60;++Tick){
  const auto Result=F.Value.BuildProjection();if(!Result)return false;const auto& P=Result.Value;
  for(const auto& R:P.GetRoutes()){
   const auto S=Hansa::UI::BuildTradeSchedule(P,*Registry,R.Id.GetValue(),R.VehicleId.GetValue(),R.OwnerId.GetValue());
   TestEqual(TEXT("Snapshot tick correlates"),S.Tick,P.GetClock().GetTick().GetValue());TestEqual(TEXT("Route identity correlates"),S.RouteId,int64(R.Id.GetValue()));
   const auto* V=P.GetVehicles().FindByPredicate([&](const auto& X){return X.Id==R.VehicleId;});if(!V)return false;
   TestEqual(TEXT("Inventory identity correlates"),S.InventoryId,int64(V->CargoInventoryId.GetValue()));
   TestFalse(TEXT("Owned journey identifies its departure"),S.DepartureCity.IsEmpty());TestFalse(TEXT("Owned journey identifies its arrival"),S.ArrivalCity.IsEmpty());TestFalse(TEXT("Owned journey has an explicit traveling or stopped state"),S.TravelStatus.IsEmpty());
   const auto* Hold=P.GetInventories().FindByPredicate([&](const auto& X){return X.Id==V->CargoInventoryId;});
   int32 Instructions=0;for(const auto& Stop:R.Stops)Instructions+=Stop.Actions.Num();int32 Seen=0;
   for(const auto& Row:S.Rows)if(Row.Group==TEXT("Cargo")&&Row.ActionIndex!=INDEX_NONE){++Seen;
    const auto& A=R.Stops[Row.StopIndex].Actions[Row.ActionIndex];TestEqual(TEXT("Requested is authoritative"),Row.Requested,A.QuantityLimit.GetRawValue());TestEqual(TEXT("Reserve is authoritative"),Row.Reserve,A.MinimumSourceReserve.GetRawValue());
    if(Hold){const auto* Stock=Hold->Stocks.FindByPredicate([&](const auto& X){return X.GoodId==A.GoodId;});TestTrue(TEXT("Carried is known only from hold"),Row.Carried.IsSet());TestEqual(TEXT("Carried matches physical inventory"),Row.Carried.GetValue(),Stock?Stock->Stock.GetRawValue():int64(0));}
    if(Row.TransferTick>=0){Receipt=true;TestTrue(TEXT("Transfer request remains recorded separately from editable plan"),Row.TransferRequested.IsSet());TestEqual(TEXT("Transfer request matches receipt"),Row.TransferRequested.Get(0),R.LastTransfer.RequestedQuantity.GetRawValue());TestEqual(TEXT("Receipt amount correlates"),Row.Applied,R.LastTransfer.AppliedQuantity.GetRawValue());TestEqual(TEXT("Receipt tick correlates"),Row.TransferTick,R.LastTransfer.Tick.GetValue());}
   }
   TestEqual(TEXT("Every cargo action represented"),Seen,Instructions);
   if(R.Lifecycle==EHansaRouteLifecycleState::Traveling){Travel=true;TestTrue(TEXT("Travel has ETA"),S.ArrivalTick.IsSet());TestEqual(TEXT("ETA uses authoritative remaining ticks"),S.ArrivalTick.GetValue(),S.Tick+R.RemainingTravelTicks);TestTrue(TEXT("Native progress bounded"),S.Progress.IsSet()&&S.Progress.GetValue()>=0&&S.Progress.GetValue()<=1);}
   else {Berth=true;TestFalse(TEXT("No fictional stopped ETA"),S.ArrivalTick.IsSet());}
   const auto Private=Hansa::UI::BuildTradeSchedule(P,*Registry,R.Id.GetValue(),R.VehicleId.GetValue(),0);TestTrue(TEXT("Unknown owner cannot see cargo"),Private.Rows.IsEmpty());TestFalse(TEXT("Private arrival withheld"),Private.ArrivalTick.IsSet());TestTrue(TEXT("Private timeline identities withheld"),Private.Identity.IsEmpty()&&Private.DepartureCity.IsEmpty()&&Private.ArrivalCity.IsEmpty());
   Evidence+=S.Evidence.ToString()+TEXT("\t")+S.Context.ToString()+TEXT("\n");
  }
  if(!F.Value.Step())return false;
 }
 TestTrue(TEXT("Fixture covers travel"),Travel);TestTrue(TEXT("Fixture covers transfer receipts"),Receipt);TestTrue(TEXT("Fixture covers berth"),Berth);
 FFileHelper::SaveStringToFile(Evidence,*(FPaths::ProjectSavedDir()/TEXT("TG06-authority.tsv")));return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeScheduleSelection,"Hansa.UI.TradeSchedule.SelectionPrivacyAndDraft",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeScheduleSelection::RunTest(const FString&) {
 auto F=FHansaProductionFixture::TryCreateRouteDelivery();if(!F)return false;const auto P=F.Value.BuildProjection();const auto* Registry=F.Value.GetDefinitions().GetEconomicRegistry();if(!P||!Registry||P.Value.GetRoutes().IsEmpty())return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(P.Value.GetRoutes()[0].OwnerId);M->ApplyProjection(P.Value,*Registry);M->Open();
 auto S=M->GetSchedulePresentation();const auto* Row=S.Rows.FindByPredicate([](const auto& X){return X.Group==TEXT("Cargo");});if(!Row)return false;
 TestTrue(TEXT("Cargo selection accepted"),M->SelectScheduleRowIntent(Row->Id));TestEqual(TEXT("City synchronized"),M->GetSnapshot().SelectedCityStableId,Row->CityId);TestEqual(TEXT("Good synchronized"),M->GetSnapshot().PreferredGoodStableId,Row->GoodId);TestEqual(TEXT("Stop synchronized"),M->GetSnapshot().SelectedStopIndex,Row->StopIndex);
 TestFalse(TEXT("Forged schedule row rejected"),M->SelectScheduleRowIntent(TEXT("TradeMap.Schedule.Cargo.999.999.999")));
 if(FSlateApplication::IsInitialized()){
  auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).InitialViewportSize(FIntPoint(1280,720));
  TestTrue(TEXT("Schedule tab intent"),Screen->ActivateSemanticId(TEXT("TradeMap.Schedule.Tab.Cargo")));
  TestTrue(TEXT("Controller focus reaches virtual cargo"),Screen->GetControllerFocusOrder().Contains(Row->Id));
  TestTrue(TEXT("Virtual focus reveal accepted"),Screen->FocusSemanticId(Row->Id));
  const auto Rev=M->GetRevision();M->ApplyProjection(P.Value,*Registry);TestEqual(TEXT("Identical projection preserves revision"),M->GetRevision(),Rev);
 }
 M->BeginCreateIntent();TestTrue(TEXT("Draft does not claim execution"),M->GetSchedulePresentation().Rows.IsEmpty());return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeScheduleLong,"Hansa.UI.TradeSchedule.MaximumStops",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeScheduleLong::RunTest(const FString&) {
 auto F=FHansaProductionFixture::TryCreateRouteDelivery();if(!F)return false;const auto Initial=F.Value.BuildProjection();if(!Initial||Initial.Value.GetRoutes().IsEmpty())return false;
 const auto Route=Initial.Value.GetRoutes()[0];TArray<FHansaRouteStop> Stops;for(int I=0;I<16;++I)Stops.Add(Route.Stops[I%2]);
 TestTrue(TEXT("Maximum 16-stop plan accepted through gateway"),!!F.Value.EditRoute(Route.Id,Stops));
 const auto P=F.Value.BuildProjection();const auto* Registry=F.Value.GetDefinitions().GetEconomicRegistry();if(!P||!Registry)return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(Route.OwnerId);M->ApplyProjection(P.Value,*Registry);M->Open();
 const auto Data=M->GetSchedulePresentation();int Count=0;FString Last;for(const auto& R:Data.Rows)if(R.Group==TEXT("Stops")){++Count;Last=R.Id;}
 TestEqual(TEXT("No long schedule truncation"),Count,16);
 Hansa::UI::FUiPreferences Pref;Pref.UiScale=1.4f;Pref.bLargeText=true;Pref.bHighContrast=true;Pref.bReducedMotion=true;
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).Preferences(Pref).InitialViewportSize(FIntPoint(914,514));
 TestTrue(TEXT("Compact schedule tab reachable"),Screen->ActivateSemanticId(TEXT("TradeMap.Schedule.Tab.Stops")));
 TestTrue(TEXT("Last stop remains in controller order"),Screen->GetControllerFocusOrder().Contains(Last));
 TestTrue(TEXT("Unmaterialized last stop can be focus revealed"),Screen->FocusSemanticId(Last));
 TestEqual(TEXT("Focus synchronizes last stop"),M->GetSnapshot().SelectedStopIndex,15);
 TestEqual(TEXT("Focus preserves selected city"),M->GetSnapshot().SelectedCityStableId,FName(*Stops[15].CityId.ToString()));
 TestTrue(TEXT("Compact stays in schedule page"),M->GetSnapshot().WorkspacePage==TEXT("Schedule"));
 return !HasAnyErrors();
}
#endif
