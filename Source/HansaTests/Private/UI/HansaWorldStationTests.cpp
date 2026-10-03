#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeEstablishmentTestSupport.h"
#include "HansaTradeLedgerTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "World/HansaTradeStationPresentation.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Placement/HansaRostockPlacement.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "World/HansaTerrainPlacement.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaWorldStation,"Hansa.UI.TradeMap.Establishment.WorldStation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaWorldStation::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error)){AddError(Error);return false;}
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 FHansaTradeStationId Id;
 TestTrue(TEXT("Reserve actual site"),Host->ProposeTradeStation(City,TEXT("TradeStationSite.Rostock.Harbor.01"),Id).IsSuccess());
 TestTrue(TEXT("Fund actual station"),Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value).IsSuccess());
 Host->AdvanceTicks(4);
 TArray<uint8> Saved;TestTrue(TEXT("Save completed station"),Host->CaptureSaveBytes(Saved,TEXT("World station test"),TEXT("2026-09-29T00:00:00Z")).IsSuccess());
 TestTrue(TEXT("Restore station without a second payment"),Host->RestoreSaveBytes(Saved).IsSuccess());
 const auto Projection=Host->BuildProjection();if(!Projection)return false;
 const auto* Station=Projection.Value.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Id;});
 if(!TestNotNull(TEXT("Saved station projects"),Station))return false;
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Actor=World->SpawnActor<AHansaTradeStationPresentation>();
 TestTrue(TEXT("Warehouse and approved harbor resolve"),Actor->ApplyStation(*Station));
 TestEqual(TEXT("Actor retains authoritative identity"),Actor->GetStationId(),int64(Id.GetValue()));
 TestTrue(TEXT("Visible without entering city visit"),!Actor->IsHidden());
 const auto Bounds=Actor->Warehouse->GetStaticMesh()->GetBoundingBox().TransformBy(Actor->Warehouse->GetComponentTransform());
 const FVector Min=RostockPlacement::CellCenter(Station->Lease.BoundsMin.X,Station->Lease.BoundsMin.Y)-FVector(200,200,0);
 const FVector Max=RostockPlacement::CellCenter(Station->Lease.BoundsMax.X,Station->Lease.BoundsMax.Y)+FVector(200,200,0);
 TestTrue(TEXT("Full warehouse is inside the saved land lease"),Bounds.Min.X>=Min.X&&Bounds.Max.X<=Max.X&&Bounds.Min.Y>=Min.Y&&Bounds.Max.Y<=Max.Y);
 TestTrue(TEXT("Warehouse lies on land before the quay"),Bounds.Max.Y<2300);
 FHitResult Hit;TestTrue(TEXT("Warehouse can be selected through normal visibility trace"),World->LineTraceSingleByChannel(Hit,Actor->GetActorLocation()+FVector(0,0,2000),Actor->GetActorLocation(),ECC_Visibility));
 TestEqual(TEXT("Click resolves station actor"),Hit.GetActor(),static_cast<AActor*>(Actor));
 auto* Dock=Actor->Dock->GetChildActor();Actor->ApplyStation(*Station);TestEqual(TEXT("Refresh does not duplicate dock"),Actor->Dock->GetChildActor(),Dock);
 FTransform LastAccess;Actor->AccessPath->GetInstanceTransform(Actor->AccessPath->GetInstanceCount()-1,LastAccess,true);
 TestTrue(TEXT("Walkway reaches dock shore edge"),LastAccess.GetLocation().Y+200>=Actor->Dock->GetComponentLocation().Y-800);
 TestTrue(TEXT("Walkway and dock share deck height"),FMath::IsNearlyEqual(LastAccess.GetLocation().Z,Actor->Dock->GetComponentLocation().Z,1.));
 for(bool Remote:{false,true}){
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->SetViewerHouse(Host->GetHouseId());
  FHansaMultiplayerAuthority Authority;
  if(Remote){
   Authority.Initialize(*Host);FHansaClientInterest Interest;
   if(!Authority.RegisterAdmittedClient({808,FHansaParticipantId::TryCreate(1808).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   FHansaClientProjectionSnapshot Network;TestTrue(TEXT("Owner-scoped world and UI projection"),Authority.BuildProjection(808,0,true,Network,Error));Model->ApplyRemoteEstablishment(Network);
  }else {Model->BindRuntime(Host.Get());Model->ApplyProjection(Projection.Value,*Host->GetEconomicRegistry());}
  Model->Open(TEXT("Test"),NAME_None,NAME_None,false);Model->SelectCityIntent(TEXT("City.Rostock"));
  const auto Trade=Model->GetSnapshot().Establishment;
  Model->StationMapRequested=[&](FName Target,int64 StationId){return Model->OpenWorldStation(Target,StationId);};
  TestTrue(TEXT("Show on map enters station inspector"),Model->EstablishmentIntent(TEXT("ShowOnMap")));
  TestTrue(TEXT("World inspector mode"),Model->bWorldStationDetail);
  TestTrue(TEXT("Same authoritative station details in both views"),Trade==Model->GetSnapshot().Establishment);
  TestEqual(TEXT("Remote/local lease parity"),Trade.LeaseBoundsMin,FIntPoint(Station->Lease.BoundsMin.X,Station->Lease.BoundsMin.Y));
  auto Widget=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get()).InitialViewportSize(FIntPoint(460,900));
  TestTrue(TEXT("World selection exposes direct Details tab"),Widget->GetControllerFocusOrder().Contains(TEXT("TradeMap.WorldStation.Tab.Details")));
  TestTrue(TEXT("World selection exposes direct Orders tab"),Widget->GetControllerFocusOrder().Contains(TEXT("TradeMap.WorldStation.Tab.Orders")));
  TestTrue(TEXT("Orders opens inside the same world inspector"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders"))&&Model->bWorldStationDetail);
  TestEqual(TEXT("Local and remote Orders keep selected station"),Model->GetSnapshot().Establishment.StationId,Trade.StationId);
  TestEqual(TEXT("Direct tab opens shared Orders section"),Model->GetSnapshot().ActiveSection,FString(TEXT("Orders")));
  TestTrue(TEXT("Details returns inside the same inspector"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details"))&&Model->bWorldStationDetail);
  TestTrue(TEXT("Shared lease terms work from world selection"),Widget->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
  TestTrue(TEXT("Shared operations action works"),Widget->ActivateSemanticId(TEXT("TradeMap.Station.Action")));
  TestFalse(TEXT("Operations opens full trade view"),Model->bWorldStationDetail);
  TestEqual(TEXT("Operations targets station ledger"),Model->GetSnapshot().ActiveSection,FString(TEXT("Ledger")));
  TestTrue(TEXT("Reopen station for closure controls"),Model->OpenWorldStation(TEXT("City.Rostock"),int64(Id.GetValue())));
  TestTrue(TEXT("Shared safe closure controls"),Widget->ActivateSemanticId(TEXT("TradeMap.Station.Close")));
  TestFalse(TEXT("Closure opens full recovery workspace"),Model->bWorldStationDetail);
  TestEqual(TEXT("Closure requests review, never immediate spending"),Model->GetSnapshot().ActiveSection,FString(TEXT("Recovery")));
  TestFalse(TEXT("Another station ID cannot be inspected"),Model->OpenWorldStation(TEXT("City.Rostock"),9999));
 }
 TestEqual(TEXT("Viewing and framing never mutate simulation"),Host->BuildProjection().Value.GetFingerprint().Value,Projection.Value.GetFingerprint().Value);
 World->DestroyWorld(false);return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaWorldStationOrders,"Hansa.UI.TradeMap.Establishment.WorldStationOrders",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaWorldStationOrders::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::UI;
 for(bool Office:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  // Commands require an available market report. Seed one explicitly for this
  // save-flow check, while viewport fixtures retain unavailable/stale states.
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,EHansaTradeStationOperationalState::Active,false,Office,true)){AddError(Error);return false;}
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
  TestTrue(TEXT("Open authoritative world-selected building"),Model->OpenWorldStation(TEXT("City.Rostock"),100));
  TestEqual(TEXT("Inspector identifies the selected building stage"),Model->GetSnapshot().Establishment.bOfficeBuilt,Office);
  auto Widget=SNew(SHansaTradeMap).Model(Model.Get()).InitialViewportSize(FIntPoint(580,900));
  TestTrue(TEXT("Details tab receives keyboard focus"),Widget->FocusSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
  TestTrue(TEXT("Right arrow switches to Orders"),Widget->OnPreviewKeyDown(Widget->GetCachedGeometry(),FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0)).IsEventHandled());
  TestEqual(TEXT("Keyboard opens shared Orders section"),Model->GetSnapshot().ActiveSection,FString(TEXT("Orders")));
  TestTrue(TEXT("Controller shoulder returns to Details"),Widget->OnPreviewKeyDown(Widget->GetCachedGeometry(),FKeyEvent(EKeys::Gamepad_LeftShoulder,FModifierKeysState(),0,false,0,0)).IsEventHandled());
  TestEqual(TEXT("Controller opens shared Details section"),Model->GetSnapshot().ActiveSection,FString(TEXT("Presence")));
  TestTrue(TEXT("Open direct Orders tab"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders")));
  TestTrue(TEXT("Select shared existing order"),Widget->ActivateSemanticId(TEXT("TradeMap.Orders.Row.2")));
  TestTrue(TEXT("Edit target through shared order draft"),Model->SetStationOrderNumber(TEXT("Target"),TEXT("9")));
  TestTrue(TEXT("Edit purchase budget through shared draft"),Model->SetStationOrderNumber(TEXT("Budget"),TEXT("10000")));
  TestTrue(TEXT("Details remains reachable from editor"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
  TestTrue(TEXT("Return to editor through direct Orders tab"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders")));
  TestEqual(TEXT("Tab switching retains order draft"),Model->GetSnapshot().StationOrderTargetInput,FString(TEXT("9")));
  TestEqual(TEXT("Tab switching retains selected order"),Model->GetSnapshot().SelectedStationOrderId,int64(2));
  const bool SavedOrder=Widget->ActivateSemanticId(TEXT("TradeMap.Orders.Save"));
  TestTrue(Office?TEXT("Office save uses authoritative gateway"):TEXT("Station save uses authoritative gateway"),SavedOrder);
  if(!SavedOrder)AddInfo(FString::Printf(TEXT("Office=%d feedback=%s"),Office,*Model->GetSnapshot().StationOrderFeedback.ToString()));
  const auto P=Host->BuildProjection().Value;const auto* Station=P.GetTradeStations().FindByPredicate([](const auto& S){return S.Station.Id.GetValue()==100;});
  if(!TestNotNull(TEXT("Same authoritative station after save"),Station))return false;
  const auto* Order=Station->Station.Orders.FindByPredicate([](const auto& O){return O.Id==2;});
  if(!TestNotNull(TEXT("Shared command updates the existing order"),Order))return false;
  TestEqual(TEXT("Saved authoritative target"),Order->Terms.TargetOrReserveMilliUnits,int64(9000));
  TestTrue(TEXT("Save keeps local inspector open"),Model->bWorldStationDetail&&Model->GetSnapshot().bOpen);
  TestTrue(TEXT("Back returns to shared order list"),Widget->ActivateSemanticId(TEXT("TradeMap.Orders.Back")));
  TestTrue(TEXT("Details remains reachable from list"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
  TestTrue(TEXT("Reopen editor for keyboard back path"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders"))&&Widget->ActivateSemanticId(TEXT("TradeMap.Orders.Row.2")));
  TestTrue(TEXT("Escape returns from editor to reachable order list"),Widget->OnKeyDown(Widget->GetCachedGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0)).IsEventHandled());
  TestTrue(TEXT("Order list receives focus after Escape"),Widget->GetControllerFocusOrder().Contains(Model->GetSnapshot().FocusedSemanticId.ToString()));
  TestTrue(TEXT("Escape from order list returns to local Details"),Widget->OnKeyDown(Widget->GetCachedGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0)).IsEventHandled());
  TestEqual(TEXT("Escape does not open a hidden trade map page"),Model->GetSnapshot().ActiveSection,FString(TEXT("Presence")));
  TestTrue(TEXT("Escape from Details closes the local window"),Widget->OnKeyDown(Widget->GetCachedGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0)).IsEventHandled()&&!Model->GetSnapshot().bOpen);
  Model->Open();
  TestFalse(TEXT("World tabs are absent from full trade workspace"),Widget->GetControllerFocusOrder().Contains(TEXT("TradeMap.WorldStation.Tab.Orders")));
  TestFalse(TEXT("Hidden world tabs cannot activate in full workspace"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders")));
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeStationSelection,"Hansa.World.Projection.TradeStationSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaTradeStationSelection::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error)){AddError(Error);return false;}
 FHansaTradeStationId Id;
 if(!Host->ProposeTradeStation(FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,TEXT("TradeStationSite.Rostock.Harbor.01"),Id)||
    !Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value)){AddError(TEXT("Selection fixture funding failed"));return false;}
 Host->AdvanceTicks(4);
 const auto Projection=Host->BuildProjection().Value;
 const auto* SavedStation=Projection.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Id;});
 if(!TestNotNull(TEXT("Authoritative station"),SavedStation))return false;
 auto Station=*SavedStation;
 Station.Station.ConstructionSite.bLocalDelivery=true;
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
 auto* Actor=World->SpawnActor<AHansaTradeStationPresentation>();
 TestTrue(TEXT("Station resolves approved visuals"),Actor->ApplyStation(Station));
 TestEqual(TEXT("Four footprint corners"),Actor->SelectionCornerSegments.Num(),8);
 auto* Terrain=World->SpawnActor<AStaticMeshActor>();auto* Mesh=Terrain->GetStaticMeshComponent();
 Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Mesh->SetCollisionProfileName(TEXT("BlockAll"));Terrain->Tags.Add(TEXT("Hansa.Terrain"));
 Terrain->SetActorScale3D(FVector(80,80,1));Terrain->SetActorLocation(Actor->GetActorLocation()+FVector(0,0,50));Terrain->SetActorRotation(FRotator(8,0,0));
 TArray<UStaticMeshComponent*> Markers;Markers.Add(Actor->SelectionOutline);for(UStaticMeshComponent* Segment:Actor->SelectionCornerSegments)Markers.Add(Segment);
 for(bool Office:{false,true})for(auto Status:{EHansaTradeStationStatus::Active,EHansaTradeStationStatus::Suspended,EHansaTradeStationStatus::UnderConstruction}){
  Station.Station.Status=Status;Station.Station.ConstructionSite.bLocalDelivery=true;
  for(auto Rotation:{EHansaGridRotation::North,EHansaGridRotation::East}){
   Station.Station.ConstructionSite.Rotation=Rotation;
   TestTrue(TEXT("Refresh shell and stage"),Actor->ApplyStation(Station,Office));
   const int32 Components=Actor->GetComponents().Num();
   TArray<UMaterialInterface*> AuthoredMaterials;for(int32 Index=0;Index<Actor->Warehouse->GetNumMaterials();++Index)AuthoredMaterials.Add(Actor->Warehouse->GetMaterial(Index));
   Actor->SetSelected(true);TArray<FVector> Positions;
   for(auto* Marker:Markers){
    TestTrue(TEXT("Selected station/office displays bracket or diamond"),Marker->IsVisible());
    TestEqual(TEXT("Markers cannot intercept clicks"),Marker->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    Positions.Add(Marker->GetComponentLocation());
    for(double X:{-50.,50.})for(double Y:{-50.,50.}){
     const FVector Bottom=Marker->GetComponentTransform().TransformPosition(FVector(X,Y,-50));FHitResult Hit;
     if(TestTrue(TEXT("Terrain beneath marker"),Hansa::Game::TerrainPlacement::Trace(World,Bottom+FVector(0,0,10000),Bottom-FVector(0,0,10000),Hit)))
      TestTrue(TEXT("Marker clears sloping ground"),Bottom.Z>=Hit.ImpactPoint.Z+1.9);
    }
   }
   Actor->ApplyStation(Station,Office);Actor->Tick(1);
   TestTrue(TEXT("Refresh retains selection"),Actor->IsSelected());
   for(int32 Index=0;Index<Markers.Num();++Index)TestTrue(TEXT("Refresh retains visible markers without cumulative lift"),Markers[Index]->IsVisible()&&Markers[Index]->GetComponentLocation().Equals(Positions[Index],.01));
   TestEqual(TEXT("No enlarged copies of building geometry"),Actor->GetComponents().Num(),Components);
   for(int32 Index=0;Index<AuthoredMaterials.Num();++Index)TestEqual(TEXT("Selection preserves every authored shell material"),Actor->Warehouse->GetMaterial(Index),AuthoredMaterials[Index]);
   Actor->SetSelected(false);for(auto* Marker:Markers)TestFalse(TEXT("Deselection hides every marker"),Marker->IsVisible());
   Actor->ApplyStation(Station,Office);for(auto* Marker:Markers)TestFalse(TEXT("Refresh cannot revive deselected markers"),Marker->IsVisible());
   TestEqual(TEXT("Deselection retains construction depth state"),Actor->Warehouse->bRenderCustomDepth,Status==EHansaTradeStationStatus::UnderConstruction);
  }
 }
 return !HasAnyErrors();
}
#endif
