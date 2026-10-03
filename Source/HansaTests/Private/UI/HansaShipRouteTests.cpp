#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteCellDraftTest,"Hansa.TradeRoute.UI.CellDraftIsolation",(EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext)|EAutomationTestFlags::EngineFilter)
bool FShipRouteCellDraftTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());M->Open();M->BeginCreateIntent();
 TestTrue(TEXT("New route automatically selects an owned Cog"),M->GetSnapshot().CogValue>0);
 TestTrue(TEXT("Selected Cog has usable cargo capacity"),M->GetShipCapacity()>0);
 const auto Before=M->GetDraftStops();const auto Sequence=Host->GetLastProcessedCommandSequence();
 TestTrue(TEXT("Open load opposite the destination grain unload"),M->OpenCargoCell(1,0,true));
 const auto* BlockedGrain=M->CargoEditor.Products.FindByPredicate([](const auto& P){return P.GoodId.ToString()==TEXT("Good.Grain");});
 TestTrue(TEXT("Opposite product disabled with remedy"),BlockedGrain&&!BlockedGrain->bEnabled&&BlockedGrain->Detail.ToString().Contains(TEXT("Remove")));
 TestFalse(TEXT("Cannot select grain for load in its unloading city"),M->SetCargoProduct(TEXT("Good.Grain")));
 M->CargoEditor.Draft.GoodId=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
 TestFalse(TEXT("Confirm rechecks conflict even for a stale selection"),M->ConfirmCargoCell());
 TestEqual(TEXT("Rejected confirm preserves actions"),M->GetDraftStops()[1].Actions.Num(),Before[1].Actions.Num());
 M->CancelCargoCell();
 TestTrue(TEXT("Open load opposite destination unload in another slot"),M->OpenCargoCell(1,2,true));
 TestFalse(TEXT("Different slot cannot bypass city direction"),M->SetCargoProduct(TEXT("Good.Grain")));
 M->CancelCargoCell();
 TestTrue(TEXT("Load cell opens"),M->OpenCargoCell(0,0,true));TestTrue(TEXT("Quantity edits local popup draft"),M->SetCargoQuantity(7000));M->CancelCargoCell();
 TestEqual(TEXT("Cancel preserves route quantity"),M->GetDraftStops()[0].Actions[0].QuantityLimit.GetRawValue(),Before[0].Actions[0].QuantityLimit.GetRawValue());
 TestEqual(TEXT("Cancel emits no command"),Host->GetLastProcessedCommandSequence(),Sequence);
 TestTrue(TEXT("Unload planned before actual loading"),M->OpenCargoCell(1,0,false));
 TestTrue(TEXT("Planned grain selectable"),M->SetCargoProduct(TEXT("Good.Grain")));TestTrue(TEXT("Set exact quantity"),M->SetCargoQuantity(6000));TestTrue(TEXT("Confirm selected cell"),M->ConfirmCargoCell());
 TestEqual(TEXT("Other stop preserved"),M->GetDraftStops()[0].Actions[0].QuantityLimit.GetRawValue(),Before[0].Actions[0].QuantityLimit.GetRawValue());
 TestEqual(TEXT("Selected unload changed"),M->GetDraftStops()[1].Actions[0].QuantityLimit.GetRawValue(),int64(6000));
 TestEqual(TEXT("Physical slot retained"),M->GetDraftStops()[1].Actions[0].CargoSlotIndex,0);
 M->SelectStopIntent(1);TestTrue(TEXT("Move entire stop pair"),M->MoveStopIntent(-1));TestEqual(TEXT("Town moves with instruction"),M->GetDraftStops()[0].CityId,Before[1].CityId);
 TestEqual(TEXT("Moved slot identity stable"),M->GetDraftStops()[0].Actions[0].CargoSlotIndex,0);
 TestFalse(TEXT("Out-of-range slot cannot open"),M->OpenCargoCell(0,3,true));
 TestTrue(TEXT("Open an empty load cell at the owned home port"),M->OpenCargoCell(1,2,true));
 TestEqual(TEXT("New home load uses inventory transfer"),M->CargoEditor.Draft.Kind,EHansaRouteCargoActionKind::OwnedCityLoad);
 TestTrue(TEXT("Reserve edits the selected load draft"),M->SetCargoQuantity(1000,true));
 TestTrue(TEXT("Legacy city stock source remains selectable"),M->SetCargoSource(uint8(EHansaRouteCargoActionKind::Load)));
 TestTrue(TEXT("Owned city source is described as a transfer"),M->CargoEditor.SourceLabel.ToString().Contains(TEXT("no payment")));
 TestTrue(TEXT("Home source remains selectable"),M->SetCargoSource(uint8(EHansaRouteCargoActionKind::OwnedCityLoad)));
 TestEqual(TEXT("Changing source preserves reserve"),M->CargoEditor.Draft.MinimumSourceReserve.GetRawValue(),int64(1000));
 const auto* Available=M->CargoEditor.Products.FindByPredicate([](const FHansaCargoProductChoice& Choice){return Choice.bEnabled;});
 if(!TestNotNull(TEXT("At least one home product is available"),Available))return false;
 TestTrue(TEXT("Select load product"),M->SetCargoProduct(Available->GoodId.ToString()));
 TestTrue(TEXT("Increase load quantity"),M->SetCargoQuantity(2000));
 TestTrue(TEXT("Set load confirms the selected product and quantity"),M->ConfirmCargoCell());
 TestFalse(TEXT("Successful Set load closes the popup"),M->CargoEditor.bOpen);
 const auto ConfirmedStops=M->GetDraftStops();
 const auto* Loaded=ConfirmedStops[1].Actions.FindByPredicate([](const FHansaRouteCargoAction& Action){return Action.CargoSlotIndex==2&&IsRouteLoad(Action.Kind);});
 if(!TestNotNull(TEXT("Confirmed load occupies the selected cell"),Loaded))return false;
 TestEqual(TEXT("Confirmed load keeps the entered quantity"),Loaded->QuantityLimit.GetRawValue(),int64(2000));
 TestEqual(TEXT("Confirmed load keeps the source reserve"),Loaded->MinimumSourceReserve.GetRawValue(),int64(1000));
 // A single-cell edit may temporarily conflict with another stop. Keep the draft
 // so the player can repair the other cell before saving the complete route.
 TestTrue(TEXT("Open grain slot for another product"),M->OpenCargoCell(1,0,true));
 const auto* Alternative=M->CargoEditor.Products.FindByPredicate([](const FHansaCargoProductChoice& Choice){return Choice.bEnabled&&Choice.GoodId.ToString()!=TEXT("Good.Grain");});
 if(!TestNotNull(TEXT("A different home product is available"),Alternative))return false;
 const auto AlternativeId=Alternative->GoodId;
 TestTrue(TEXT("Select conflicting product"),M->SetCargoProduct(AlternativeId.ToString()));
 TestTrue(TEXT("Set conflicting load in draft"),M->ConfirmCargoCell());
 TestFalse(TEXT("Confirmed draft closes popup"),M->CargoEditor.bOpen);
 const auto* DraftLoad=M->GetDraftStops()[1].Actions.FindByPredicate([&](const FHansaRouteCargoAction& Action){return Action.CargoSlotIndex==0&&IsRouteLoad(Action.Kind)&&Action.GoodId==AlternativeId;});
 TestNotNull(TEXT("New load remains in route draft"),DraftLoad);
 TestTrue(TEXT("Route conflict is explained"),M->GetSnapshot().EditorStatus.ToString().Contains(TEXT("conflicting product")));
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).InitialViewportSize(FIntPoint(1536,1024));
 TestTrue(TEXT("Ship owns visible route detail"),M->bShipDetailOpen);
 TestFalse(TEXT("City inspector has no legacy Route tab"),Screen->ResolveSemanticWidget(TEXT("TradeMap.Navigate.Route")).IsValid());
 const auto Nodes=Screen->GetSemanticSnapshot();
 const auto* LegacyStop=Nodes.FindByPredicate([](const Hansa::UI::FHansaHudSemanticNode& Node){return Node.Id==TEXT("TradeMap.Editor.Visit");});
 TestFalse(TEXT("Legacy route controls are not visible"),LegacyStop&&LegacyStop->State.bVisible);
 for(int32 Stop=0;Stop<2;++Stop)for(int32 Slot=0;Slot<3;++Slot)for(const TCHAR* Row:{TEXT("Unload"),TEXT("Load")})TestTrue(TEXT("Matrix cell has native widget"),Screen->ResolveSemanticWidget(FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"),Stop,Slot,Row)).IsValid());
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteCellResetTest,"Hansa.TradeRoute.UI.CellReset",(EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext)|EAutomationTestFlags::EngineFilter)
bool FShipRouteCellResetTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());M->Open();M->BeginCreateIntent();
 const auto Sequence=Host->GetLastProcessedCommandSequence();
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).InitialViewportSize(FIntPoint(1536,1024));
 for(bool bLoad:{true,false})
 {
  const int32 Stop=bLoad?0:1;
  const auto StopsBeforeClear=M->GetDraftStops();
  TestTrue(TEXT("Open occupied cell to clear"),M->OpenCargoCell(Stop,0,bLoad));
  TestTrue(TEXT("Clear is reachable without expanding source and reserve"),Screen->FocusSemanticId(TEXT("TradeMap.Cargo.Remove")));
  TestTrue(TEXT("Clear selected instruction"),Screen->ActivateSemanticId(TEXT("TradeMap.Cargo.Remove")));
  TestFalse(TEXT("Clear closes selector"),M->CargoEditor.bOpen);
  const auto& ClearedStops=M->GetDraftStops();
  TestFalse(TEXT("Selected cell is empty"),ClearedStops[Stop].Actions.ContainsByPredicate([&](const auto& A){return A.CargoSlotIndex==0&&IsRouteLoad(A.Kind)==bLoad;}));
  TestEqual(TEXT("Only selected instruction removed"),ClearedStops[Stop].Actions.Num(),StopsBeforeClear[Stop].Actions.Num()-1);
  TestEqual(TEXT("Other stop preserved"),ClearedStops[1-Stop].Actions.Num(),StopsBeforeClear[1-Stop].Actions.Num());
  TestTrue(TEXT("Clear remains an unsaved route edit"),M->GetSnapshot().bDirty);
  TestEqual(TEXT("Clear emits no simulation command"),Host->GetLastProcessedCommandSequence(),Sequence);
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteInTransitCellsTest,"Hansa.TradeRoute.UI.InTransitCells",(EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext)|EAutomationTestFlags::EngineFilter)
bool FShipRouteInTransitCellsTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());M->Open();M->SelectRouteIntent(1);
 TestTrue(TEXT("Start owned route"),M->ToggleActiveIntent());
 TestTrue(TEXT("Advance into voyage"),Host->AdvanceTicks(1));
 M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
 const auto* Route=M->GetSelectedRoutePresentation();
 if(!TestTrue(TEXT("Cog is in transit"),Route&&Route->bTraveling))return false;
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).InitialViewportSize(FIntPoint(1536,1024));
 const auto Sequence=Host->GetLastProcessedCommandSequence();
 const auto Before=M->GetDraftStops();
 for(int32 Stop=0;Stop<Before.Num();++Stop)for(int32 Slot=0;Slot<3;++Slot)for(bool Load:{false,true})
 {
  auto Cell=Screen->ResolveSemanticWidget(FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"),Stop,Slot,Load?TEXT("Load"):TEXT("Unload")));
  if(!TestTrue(TEXT("Cell exists while sailing"),Cell.IsValid()))return false;
  if(!M->CanOpenCargoCell(Stop,Slot,Load))
  {
   TestFalse(TEXT("Empty unload stays disabled while sailing"),Cell->IsEnabled());
   TestFalse(TEXT("Empty unload cannot open through model"),M->OpenCargoCell(Stop,Slot,Load));
   continue;
  }
  TestTrue(TEXT("Cargo cell is enabled while sailing"),Cell->IsEnabled());
  TestTrue(TEXT("Activate sailing cargo cell"),Screen->ActivateSemanticId(FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"),Stop,Slot,Load?TEXT("Load"):TEXT("Unload"))));
  TestTrue(TEXT("Opens correct stop, slot and direction"),M->CargoEditor.bOpen&&M->CargoEditor.Stop==Stop&&M->CargoEditor.Slot==Slot&&M->CargoEditor.bLoad==Load);
  M->CancelCargoCell();
 }
 TestEqual(TEXT("Opening and cancelling cells emits no commands"),Host->GetLastProcessedCommandSequence(),Sequence);
 TestFalse(TEXT("Opening and cancelling does not dirty route"),M->GetSnapshot().bDirty);
 TestTrue(TEXT("Voyage remains in progress"),M->GetSelectedRoutePresentation()->bTraveling);
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteRightClickResetTest,"Hansa.TradeRoute.UI.RightClickReset",(EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext)|EAutomationTestFlags::EngineFilter)
bool FShipRouteRightClickResetTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());M->Open();M->BeginCreateIntent();
 const auto Sequence=Host->GetLastProcessedCommandSequence();
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).InitialViewportSize(FIntPoint(1536,1024));
 const FPointerEvent Down(0,FVector2D::ZeroVector,FVector2D::ZeroVector,{EKeys::RightMouseButton},EKeys::RightMouseButton,0,FModifierKeysState());
 const FPointerEvent Up(0,FVector2D::ZeroVector,FVector2D::ZeroVector,{},EKeys::RightMouseButton,0,FModifierKeysState());
 for(bool bLoad:{true,false})
 {
  const int32 Stop=bLoad?0:1;
  const auto Before=M->GetDraftStops();
  auto Cell=Screen->ResolveSemanticWidget(FString::Printf(TEXT("TradeMap.Cargo.%d.0.%s"),Stop,bLoad?TEXT("Load"):TEXT("Unload")));
  if(!TestTrue(TEXT("Cell exists"),Cell.IsValid()))return false;
  TestTrue(TEXT("Right press consumed"),Cell->OnMouseButtonDown(FGeometry(),Down).IsEventHandled());
  TestTrue(TEXT("Right release consumed"),Cell->OnMouseButtonUp(FGeometry(),Up).IsEventHandled());
  TestFalse(TEXT("Selector stays closed"),M->CargoEditor.bOpen);
  TestFalse(TEXT("Clicked direction cleared"),M->GetDraftStops()[Stop].Actions.ContainsByPredicate([&](const auto& A){return A.CargoSlotIndex==0&&IsRouteLoad(A.Kind)==bLoad;}));
  TestEqual(TEXT("Only clicked instruction removed"),M->GetDraftStops()[Stop].Actions.Num(),Before[Stop].Actions.Num()-1);
  TestEqual(TEXT("Other stop preserved"),M->GetDraftStops()[1-Stop].Actions.Num(),Before[1-Stop].Actions.Num());
  TestTrue(TEXT("Draft is dirty"),M->GetSnapshot().bDirty);
  TestEqual(TEXT("No simulation command"),Host->GetLastProcessedCommandSequence(),Sequence);
  Cell->OnMouseButtonDown(FGeometry(),Down);
  TestEqual(TEXT("Repeated clear of empty cell is harmless"),M->GetDraftStops()[Stop].Actions.Num(),Before[Stop].Actions.Num()-1);
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteUnloadDefaultTest,"Hansa.TradeRoute.UI.UnloadDefaultQuantity",(EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext)|EAutomationTestFlags::EngineFilter)
bool FShipRouteUnloadDefaultTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());M->Open();M->BeginCreateIntent();
 const auto Sequence=Host->GetLastProcessedCommandSequence();
 TestTrue(TEXT("Open existing destination unload"),M->OpenCargoCell(1,0,false));
 TestTrue(TEXT("Clear destination instruction"),M->ConfirmCargoCell(true));
 const auto Before=M->GetDraftStops();
 TestTrue(TEXT("Open empty destination unload"),M->OpenCargoCell(1,0,false));
 const auto* Choice=M->CargoEditor.Products.FindByPredicate([](const auto& P){return P.bEnabled&&P.GoodId.ToString()==TEXT("Good.Grain");});
 if(!TestNotNull(TEXT("Planned cargo is available"),Choice))return false;
 TestTrue(TEXT("Planned arrival quantity exceeds old one-unit default"),Choice->Planned>1000);
 TestEqual(TEXT("Planned product is selected on opening"),M->CargoEditor.Draft.GoodId,Choice->GoodId);
 TestEqual(TEXT("New unload defaults to planned arrival quantity"),M->CargoEditor.Draft.QuantityLimit.GetRawValue(),Choice->Planned);
 TestTrue(TEXT("Player can choose partial unload"),M->SetCargoQuantity(6000));
 TestTrue(TEXT("Reselect same product"),M->SetCargoProduct(TEXT("Good.Grain")));
 TestEqual(TEXT("Reselection preserves entered quantity"),M->CargoEditor.Draft.QuantityLimit.GetRawValue(),int64(6000));
 TestTrue(TEXT("Confirm partial unload"),M->ConfirmCargoCell());
 TestTrue(TEXT("Reopen existing unload"),M->OpenCargoCell(1,0,false));
 TestEqual(TEXT("Existing unload preserves chosen quantity"),M->CargoEditor.Draft.QuantityLimit.GetRawValue(),int64(6000));
 M->CancelCargoCell();
 TestEqual(TEXT("Source load remains unchanged"),M->GetDraftStops()[0].Actions[0].QuantityLimit.GetRawValue(),Before[0].Actions[0].QuantityLimit.GetRawValue());
 TestEqual(TEXT("Popup changes emit no simulation commands"),Host->GetLastProcessedCommandSequence(),Sequence);
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteEmptySlotProductsTest,"Hansa.TradeRoute.UI.EmptySlotProducts",(EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext)|EAutomationTestFlags::EngineFilter)
bool FShipRouteEmptySlotProductsTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());M->Open();M->BeginCreateIntent();
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get()).InitialViewportSize(FIntPoint(1536,1024));
 TestTrue(TEXT("Open slot 1 home load"),Screen->ActivateSemanticId(TEXT("TradeMap.Cargo.0.0.Load")));
 if(!TestTrue(TEXT("Bread available at home"),M->SetCargoProduct(TEXT("Good.Bread"))))return false;
 TestTrue(TEXT("Set bread quantity"),M->SetCargoQuantity(30000));
 TestTrue(TEXT("Set bread load"),M->ConfirmCargoCell());
 TestTrue(TEXT("Open slot 1 destination unload"),Screen->ActivateSemanticId(TEXT("TradeMap.Cargo.1.0.Unload")));
 TestTrue(TEXT("Select planned bread"),M->SetCargoProduct(TEXT("Good.Bread")));
 TestTrue(TEXT("Set bread unload"),M->ConfirmCargoCell());
 const auto Before=M->GetDraftStops();const auto Sequence=Host->GetLastProcessedCommandSequence();
 for(int32 Stop=0;Stop<2;++Stop)
 {
  auto Cell=Screen->ResolveSemanticWidget(FString::Printf(TEXT("TradeMap.Cargo.%d.1.Unload"),Stop));
  TestTrue(TEXT("Empty slot unload widget is disabled"),Cell.IsValid()&&!Cell->IsEnabled());
  const auto Nodes=Screen->GetSemanticSnapshot();
  const auto* Node=Nodes.FindByPredicate([&](const auto& N){return N.Id==FString::Printf(TEXT("TradeMap.Cargo.%d.1.Unload"),Stop);});
  TestTrue(TEXT("Empty unload exposes disabled semantic state"),Node&&!Node->State.bEnabled&&!Node->bCanActivate&&!Node->bCanFocus);
  TestFalse(TEXT("Empty slot unload cannot open through automation"),Screen->ActivateSemanticId(FString::Printf(TEXT("TradeMap.Cargo.%d.1.Unload"),Stop)));
  TestFalse(TEXT("Empty slot unload cannot bypass widget guard"),M->OpenCargoCell(Stop,1,false));
  TestFalse(TEXT("Popup stays closed"),M->CargoEditor.bOpen);
 }
 TestTrue(TEXT("Open slot 2 home load row"),Screen->ActivateSemanticId(TEXT("TradeMap.Cargo.0.1.Load")));
 TestTrue(TEXT("Load direction retained"),M->CargoEditor.bLoad);
 TestFalse(TEXT("New load starts without an inherited product"),M->CargoEditor.Draft.GoodId.IsValid());
 TestTrue(TEXT("Choose grain independently of slot 1 bread"),M->SetCargoProduct(TEXT("Good.Grain")));
 TestTrue(TEXT("Set slot 2 load"),M->ConfirmCargoCell());
 const auto* Load=M->GetDraftStops()[0].Actions.FindByPredicate([](const auto& A){return A.CargoSlotIndex==1&&IsRouteLoad(A.Kind);});
 if(!TestNotNull(TEXT("New load assigned to slot 2"),Load))return false;
 TestEqual(TEXT("Slot 2 retains selected grain"),Load->GoodId.ToString(),FString(TEXT("Good.Grain")));
 TestEqual(TEXT("Slot 1 bread preserved"),M->GetDraftStops()[0].Actions[0].GoodId,Before[0].Actions[0].GoodId);
 TestEqual(TEXT("Destination preserved"),M->GetDraftStops()[1].Actions.Num(),Before[1].Actions.Num());
 TestTrue(TEXT("Slot 2 destination unload enables after planning its load"),M->CanOpenCargoCell(1,1,false));
 TestTrue(TEXT("Open slot 2 planned unload"),M->OpenCargoCell(1,1,false));
 TestFalse(TEXT("Slot 1 bread never appears in slot 2 unload choices"),M->CargoEditor.Products.ContainsByPredicate([](const auto& P){return P.GoodId.ToString()==TEXT("Good.Bread");}));
 TestEqual(TEXT("Slot 2 selects its own planned grain"),M->CargoEditor.Draft.GoodId.ToString(),FString(TEXT("Good.Grain")));
 M->CancelCargoCell();
 TestEqual(TEXT("Draft edits emit no commands"),Host->GetLastProcessedCommandSequence(),Sequence);
 return !HasAnyErrors();
}
#endif
