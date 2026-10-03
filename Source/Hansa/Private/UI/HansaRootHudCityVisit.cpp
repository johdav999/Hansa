#include "UI/HansaRootHud.h"
#include "Placement/HansaRostockPlacement.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaRostockQuarter.h"
#include "World/HansaCityCentrePresentation.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaCargoProjectionManager.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "UI/HansaFrontendPresentationModel.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#define LOCTEXT_NAMESPACE "HansaCityVisit"
bool AHansaRootHud::VisitCityIntent(FName City)
{
    if(City!=TEXT("City.Lubeck")&&City!=TEXT("City.Rostock"))return false;
    auto* Camera=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr;
    if(!Camera||!SimulationHost||!FrontendPresentationModel||!FrontendPresentationModel->GetSnapshot().bHasSession)return false;
    if(City==TEXT("City.Lubeck"))
    {
        CancelCityVisit();CityOverviewPresentationModel->CloseIntent();TradeMapPresentationModel->CloseIntent();return true;
    }
    if(bCityVisitLoading)return false;
    if(Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(GetWorld()))
    {
        RefreshRostockPresentation();
        if(!IsValid(RostockCityCentre))return false;
        if(!bCampaignCityCentreVisit)HomeView=Camera->GetViewState();
        bCampaignCityCentreVisit=true;
        Camera->RestoreHomeBounds();Camera->FocusWorldLocationIntent(RostockCityCentre->GetMarketLocation());
        InspectorPresentationModel->CloseIntent();CityOverviewPresentationModel->CloseIntent();TradeMapPresentationModel->CloseIntent();
        return true;
    }
    if(ViewedCity==City){CityOverviewPresentationModel->CloseIntent();return true;}
    FString Path=TEXT("/Game/Hansa/World/Cities/Rostock/L_Rostock_Quarter");
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("P31Candidate")))Path=TEXT("/Game/Hansa/Generated/Staging/Rostock_P31/L_Rostock_Quarter");
#endif
    if(!FPackageName::DoesPackageExist(Path))
    {
        CityVisitStatus=LOCTEXT("Unavailable","Rostock presentation is unavailable. The current city and market report are preserved.");PublishCityVisit();return false;
    }
    HomeView=Camera->GetViewState();bCityVisitLoading=true;BuildMenuPresentationModel->SetConstructionAllowed(false);
    CityVisitStatus=LOCTEXT("Loading","Loading Rostock. Return to Lübeck to cancel.");PublishCityVisit();
    if(RostockLevel){RostockLevel->SetShouldBeVisible(true);if(RostockLevel->IsLevelVisible())HandleRostockShown();}
    else
    {
        bool Success=false;RostockLevel=ULevelStreamingDynamic::LoadLevelInstance(GetWorld(),Path,AHansaRostockQuarter::VisitOffset(),FRotator::ZeroRotator,Success);
        if(!Success||!RostockLevel){CancelCityVisit();CityVisitStatus=LOCTEXT("Failed","Rostock could not load. Try Visit city again.");PublishCityVisit();return false;}
        RostockLevel->OnLevelShown.AddDynamic(this,&AHansaRootHud::HandleStationQuarterShown);
    }
    GetWorldTimerManager().SetTimer(CityVisitTimeout,FTimerDelegate::CreateWeakLambda(this,[this]{if(bCityVisitLoading){CancelCityVisit();CityVisitStatus=LOCTEXT("Timeout","Rostock loading timed out. Lübeck is preserved; try again.");PublishCityVisit();}}),30,false);
    return true;
}
void AHansaRootHud::HandleRostockShown()
{
    if(!bCityVisitLoading||!RostockLevel)return;
    bool Found=false;for(AActor* Actor:RostockLevel->GetLoadedLevel()->Actors)if(auto* Q=Cast<AHansaRostockQuarter>(Actor)){Q->RefreshTerrainPlacement();Found=true;}
    if(!Found){CancelCityVisit();CityVisitStatus=LOCTEXT("Invalid","Rostock content failed validation. Return to the current market report.");PublishCityVisit();return;}
    bCityVisitLoading=false;GetWorldTimerManager().ClearTimer(CityVisitTimeout);ViewedCity=TEXT("City.Rostock");
    if(auto* C=Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()))
    {const auto O=AHansaRostockQuarter::VisitOffset();C->SetViewBounds(FVector2D(O.X-6500,-6000),FVector2D(O.X+6500,4500));C->FocusWorldLocationIntent(O+FVector(0,-500,100));}
    InspectorPresentationModel->CloseIntent();CityOverviewPresentationModel->CloseIntent();TradeMapPresentationModel->CloseIntent();
    BuildMenuPresentationModel->SetPlacementCity(TEXT("City.Rostock"));
    BuildMenuPresentationModel->SetConstructionAllowed(true);
    CityVisitStatus=LOCTEXT("Visiting","Rostock · lease boundaries mark construction rights. Return to Lübeck to go home.");
    if(!PendingConstructionBuilding.IsNone())
    {
        const auto Building=PendingConstructionBuilding;const auto Lease=PendingConstructionLease;
        PendingConstructionBuilding=NAME_None;PendingConstructionLease=0;
        if(!BeginForeignConstruction(TEXT("City.Rostock"),Lease,Building))CityVisitStatus=LOCTEXT("PlacementChanged","Construction permissions or materials changed while loading. Reopen Expansion and select an available building.");
    }
    RefreshRostockPresentation();PublishCityVisit();
    if (PendingCenterShip) { const int64 Id = PendingCenterShip; PendingCenterShip = 0; CenterShipIntent(Id); }
    else if(RootHudWidget)RootHudWidget->FocusSemanticId(TEXT("HUD.TopStatus.CityOverview"));
}
void AHansaRootHud::CancelCityVisit()
{
    PendingCenterShip = 0;
    const bool HadVisit=bCityVisitLoading||ViewedCity==TEXT("City.Rostock")||bCampaignCityCentreVisit;bCityVisitLoading=false;bCampaignCityCentreVisit=false;GetWorldTimerManager().ClearTimer(CityVisitTimeout);
    if(RostockLevel&&!RostockTradeStationPresentation)RostockLevel->SetShouldBeVisible(false);

    for(TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)It->SetRostockVisible(false);
    PendingConstructionBuilding=NAME_None;PendingConstructionLease=0;
    ViewedCity=TEXT("City.Lubeck");if(BuildMenuPresentationModel){BuildMenuPresentationModel->SetPlacementCity(ViewedCity);BuildMenuPresentationModel->SetConstructionAllowed(true);}
    if(HadVisit)if(auto* C=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr){C->RestoreHomeBounds();C->RestoreViewState(HomeView);}
    CityVisitStatus=LOCTEXT("Home","Lübeck · construction available.");SelectedRostockRole=NAME_None;SelectedTradeStationValue=0;
    if(HadVisit)RefreshCityOverview();else PublishCityVisit();
}
void AHansaRootHud::PublishCityVisit()
{
    if(CityOverviewPresentationModel)CityOverviewPresentationModel->SetVisitStatus(ViewedCity==TEXT("City.Rostock")&&CityOverviewPresentationModel->GetSnapshot().CityStableId==TEXT("City.Rostock")&&!SelectedRostockRole.IsNone()?FText::Format(LOCTEXT("Inspect","{0} · Prebuilt trade quarter. {1}"),AHansaRostockQuarter::LabelFor(SelectedRostockRole),RostockArrivalSummary):CityVisitStatus);
    if(PresentationModel)
    {
        if(SimulationHost){
            const auto Projection=SimulationHost->BuildProjection();
            const auto City=Hansa::Simulation::FHansaCityDefinitionId::TryParse(ViewedCity.ToString());
            if(Projection && City)PresentationModel->ApplyRuntimeStatus(Projection.Value,City.Value,SimulationHost->GetHouseId(),SimulationHost->GetEconomicRegistry());
        }
        auto S=PresentationModel->GetSnapshot();S.bRemoteCityView=ViewedCity==TEXT("City.Rostock")||bCityVisitLoading;
        PresentationModel->ApplySnapshot(S);
        RefreshCameraCity();
    }
}
bool AHansaRootHud::InspectRostockRole(FName RoleId)
{
    if(ViewedCity!=TEXT("City.Rostock")||RoleId.IsNone())return false;
    const TArray<FName> Roles={TEXT("Residences"),TEXT("Market"),TEXT("Bakery"),TEXT("Mill"),TEXT("Quay"),TEXT("Dock"),TEXT("Hoist"),TEXT("WarehouseYard"),TEXT("Mooring"),TEXT("Cargo")};
    if(!Roles.Contains(RoleId))return false;
    SelectedRostockRole=RoleId;
    CityOverviewPresentationModel->Open(TEXT("HUD.TopStatus.CityOverview"));
    if(CityOverviewPresentationModel->GetSnapshot().CityStableId!=TEXT("City.Rostock"))CityOverviewPresentationModel->SelectCityIntent(TEXT("City.Rostock"));
    CityOverviewPresentationModel->SelectTabIntent(EHansaCityOverviewTab::Market);RefreshCityOverview();
    CityOverviewPresentationModel->SetVisitStatus(FText::Format(LOCTEXT("Inspect","{0} · Prebuilt trade quarter. {1}"),AHansaRostockQuarter::LabelFor(RoleId),RostockArrivalSummary));
    if(RootHudWidget)RootHudWidget->FocusSemanticId(TEXT("CityOverview.Close"));return true;
}
void AHansaRootHud::RefreshRostockPresentation()
{
    TOptional<Hansa::Simulation::FHansaSimulationProjection> P;
    Hansa::Simulation::FHansaTradeStationProjection RemoteStation;
    const Hansa::Simulation::FHansaTradeStationProjection* Station=nullptr;
    bool bMerchantOfficeBuilt=false;
    if(GetNetMode()!=NM_Client&&SimulationHost){
        const auto Current=SimulationHost->BuildProjection();if(!Current)return;P=Current.Value;
        Station=P->GetTradeStations().FindByPredicate([this](const auto& V){return V.Station.OwnerId==SimulationHost->GetHouseId()&&V.Station.CityId.ToString()==TEXT("City.Rostock")&&(V.Station.Status==Hansa::Simulation::EHansaTradeStationStatus::Active||V.Station.Status==Hansa::Simulation::EHansaTradeStationStatus::Suspended||(V.Station.ConstructionSite.bLocalDelivery&&V.Station.Status!=Hansa::Simulation::EHansaTradeStationStatus::Closed));});
        if(Station)if(const auto* Presence=P->GetForeignPresences().FindByPredicate([&](const auto& V){return V.HouseId==SimulationHost->GetHouseId()&&V.CityId.ToString()==TEXT("City.Rostock")&&V.StationId==Station->Station.Id;}))
            bMerchantOfficeBuilt=Presence->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.MerchantOffice")&&C.bGranted;});
    }else if(auto* Controller=Cast<AHansaStrategyPlayerController>(PlayerOwner)){
        const auto& View=Controller->GetClientProjection();
        if(View.SchemaVersion==FHansaClientProjectionSnapshot::CurrentSchemaVersion&&View.OwnerHouseId>0)
        if(const auto* E=View.StationEstablishments.FindByPredicate([](const auto& V){return V.CityId==TEXT("City.Rostock")&&V.StationId>0&&(V.bComplete||V.bLocalDelivery);})){
            if(const auto* Presence=View.Presences.FindByPredicate([](const auto& V){return V.City==TEXT("City.Rostock");}))bMerchantOfficeBuilt=Presence->bOfficeBuilt;
            using namespace Hansa::Simulation;
            RemoteStation.Station.Id=FHansaTradeStationId::TryCreate(uint64(E->StationId)).Value;
            RemoteStation.Station.CityId=FHansaCityDefinitionId::TryParse(E->CityId.ToString()).Value;
            RemoteStation.Station.Status=E->bComplete?EHansaTradeStationStatus::Active:E->bConstructing?EHansaTradeStationStatus::UnderConstruction:EHansaTradeStationStatus::Proposed;
            RemoteStation.Station.ConstructionSite={E->bLocalDelivery,{E->PlacementAnchor.X,E->PlacementAnchor.Y},static_cast<EHansaGridRotation>(E->PlacementRotation)};
            RemoteStation.Lease.BoundsMin={E->LeaseBoundsMin.X,E->LeaseBoundsMin.Y};RemoteStation.Lease.BoundsMax={E->LeaseBoundsMax.X,E->LeaseBoundsMax.Y};
            RemoteStation.PresentationClassPath=E->WorldPresentationClass;Station=&RemoteStation;
        }
    }
    if(Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(GetWorld())){
        if(!IsValid(RostockCityCentre)){
            for(TActorIterator<AHansaCityCentrePresentation> It(GetWorld());It;++It)if(It->CityId==TEXT("City.Rostock")){RostockCityCentre=*It;break;}
            if(!RostockCityCentre){
                const FTransform Site=AHansaTradeStationPresentation::SiteTransform(GetWorld());
                const FTransform Centre(Site.GetRotation(),Site.TransformPosition(FVector(60000,0,100)));
                RostockCityCentre=GetWorld()->SpawnActor<AHansaCityCentrePresentation>(AHansaCityCentrePresentation::StaticClass(),Centre);
            }
            if(RostockCityCentre&&GetNetMode()==NM_Client){
                Hansa::Simulation::FHansaEconomicRegistry Registry;FString Error;
                if(FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Registry,Error))RostockCityCentre->ApplyCity(Registry);
            }
        }
        if(RostockCityCentre&&SimulationHost&&SimulationHost->GetEconomicRegistry())RostockCityCentre->ApplyCity(*SimulationHost->GetEconomicRegistry());
    }
    if(Station){
        // Keep the physical quarter available independently of the camera's current city.
        if(!RostockLevel&&!Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(GetWorld())){
            const FString Path=TEXT("/Game/Hansa/World/Cities/Rostock/L_Rostock_Quarter");
            if(FPackageName::DoesPackageExist(Path)){
                bool Loaded=false;RostockLevel=ULevelStreamingDynamic::LoadLevelInstance(GetWorld(),Path,AHansaRostockQuarter::VisitOffset(),FRotator::ZeroRotator,Loaded);
                if(Loaded&&RostockLevel)RostockLevel->OnLevelShown.AddDynamic(this,&AHansaRootHud::HandleStationQuarterShown);
            }
        }
        if(RostockLevel)RostockLevel->SetShouldBeVisible(true);
        if(!IsValid(RostockTradeStationPresentation))RostockTradeStationPresentation=GetWorld()->SpawnActor<AHansaTradeStationPresentation>();
        if(RostockTradeStationPresentation)RostockTradeStationPresentation->ApplyStation(*Station,bMerchantOfficeBuilt);
    }else if(RostockTradeStationPresentation){
        RostockTradeStationPresentation->Destroy();RostockTradeStationPresentation=nullptr;
        if(TradeMapPresentationModel&&TradeMapPresentationModel->bWorldStationDetail)TradeMapPresentationModel->CloseIntent();
        SelectedTradeStationValue=0;
    }
    if(ViewedCity!=TEXT("City.Rostock")||!P.IsSet()||!SimulationHost)return;
    WorldConstruction=Hansa::UI::BuildTradeConstruction(P.GetValue(),*SimulationHost->GetEconomicRegistry(),SimulationHost->GetHouseId(),TEXT("City.Rostock"),0,NAME_None);
    int32 AtBerth=0,Approaching=0,Missing=0;int64 Cargo=0;FText LatestReceipt;int64 ReceiptTick=-1;
    for(TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)
    {
        It->SetRostockVisible(true);
        for(const auto& O:It->QueryCargo())
        {
            if(O.CityId!=TEXT("City.Rostock") || O.Phase==EHansaCargoWorldPhase::Cancelled)continue;
            if(O.Phase==EHansaCargoWorldPhase::Arriving)++Approaching;
            else if(O.Phase==EHansaCargoWorldPhase::Berthed || O.Phase==EHansaCargoWorldPhase::Loading || O.Phase==EHansaCargoWorldPhase::Unloading)++AtBerth;
            Cargo+=O.CargoMilliUnits; if(!O.PresentationFailure.IsEmpty())++Missing;
            if(O.TransferTick>ReceiptTick && O.TransferMilliUnits>0 && O.Phase==EHansaCargoWorldPhase::Unloading)
            {ReceiptTick=O.TransferTick;LatestReceipt=FText::Format(LOCTEXT("Receipt","Last recorded unload: {0} {1}, tick {2}."),FText::AsNumber(double(O.TransferMilliUnits)/1000.),FText::FromString(O.GoodId.ToString().RightChop(5)),FText::AsNumber(ReceiptTick));}
        }
    }
    for(const auto& Route:P->GetRoutes())
        if(Route.OwnerId==SimulationHost->GetHouseId() && !Hansa::Simulation::IsRouteLoad(Route.LastTransfer.Kind) && Route.LastTransfer.CityId.ToString()==TEXT("City.Rostock") && Route.LastTransfer.AppliedQuantity.GetRawValue()>0 && Route.LastTransfer.Tick.GetValue()>ReceiptTick)
        {
            ReceiptTick=Route.LastTransfer.Tick.GetValue();
            LatestReceipt=FText::Format(LOCTEXT("Receipt","Last recorded unload: {0} {1}, tick {2}."),FText::AsNumber(double(Route.LastTransfer.AppliedQuantity.GetRawValue())/1000.),FText::FromString(Route.LastTransfer.GoodId.ToString().RightChop(5)),FText::AsNumber(ReceiptTick));
        }
    RostockArrivalSummary=FText::Format(LOCTEXT("Arrival","{0} at berth · {1} approaching · {2} units aboard. Cargo is the current route projection; market reports retain their age."),FText::AsNumber(AtBerth),FText::AsNumber(Approaching),FText::AsNumber(double(Cargo)/1000.));
    if(Missing>0)RostockArrivalSummary=FText::Format(LOCTEXT("MissingCargoSkin","{0} Presentation unavailable for {1} vessel(s); inspect the route for cargo."),RostockArrivalSummary,FText::AsNumber(Missing));
    if(!LatestReceipt.IsEmpty())RostockArrivalSummary=FText::Format(LOCTEXT("ArrivalReceipt","{0} {1}"),RostockArrivalSummary,LatestReceipt);
}
void AHansaRootHud::HandleStationQuarterShown()
{
    if(bCityVisitLoading){HandleRostockShown();return;}
    if(RostockLevel&&RostockLevel->GetLoadedLevel())for(const auto& Actor:RostockLevel->GetLoadedLevel()->Actors)
        if(auto* Quarter=Cast<AHansaRostockQuarter>(Actor.Get()))Quarter->RefreshTerrainPlacement();
    RefreshRostockPresentation();
}
bool AHansaRootHud::InspectRostockTradeStation()
{
    if(!IsValid(RostockTradeStationPresentation)||!TradeMapPresentationModel)return false;
    const int64 Id=RostockTradeStationPresentation->GetStationId();
    if(!TradeMapPresentationModel->OpenWorldStation(TEXT("City.Rostock"),Id))return false;
    SelectedCargo=NAME_None;SelectedTradeStationValue=Id;
    if(InspectorPresentationModel)InspectorPresentationModel->CloseIntent();
    if(CityOverviewPresentationModel)CityOverviewPresentationModel->CloseIntent();
    RostockTradeStationPresentation->SetSelected(true);
    if(RootHudWidget)RootHudWidget->FocusSemanticId(TEXT("TradeMap.City.Close"));return true;
}
bool AHansaRootHud::ShowStationOnMap(FName City,int64 StationId)
{
    if(City!=TEXT("City.Rostock"))return false;
    RefreshRostockPresentation();
    auto* Camera=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr;
    if(!Camera||!IsValid(RostockTradeStationPresentation)||RostockTradeStationPresentation->GetStationId()!=StationId)return false;
    Camera->RestoreHomeBounds();
    if(!Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(GetWorld()))
        Camera->SetViewBounds(FVector2D(-20000,-20000),FVector2D(75000,20000));
    Camera->ClearCameraIntents();
    Camera->AddZoomIntent((Camera->GetZoomDistance()-FMath::Max(8500.f,Camera->MinimumZoomDistance))/Camera->ZoomUnitsPerStep);
    Camera->FocusWorldLocationIntent(RostockTradeStationPresentation->GetActorLocation());
    return InspectRostockTradeStation();
}
bool AHansaRootHud::InspectCargo(FName Id)
{
    if(!InspectorPresentationModel)return false;
    for(TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)
    {
        const auto* O=It->FindObservation(Id);
        if(!O)continue;
        // A retained inspector may show a departed/offscreen entity; new selection requires visibility.
        if(SelectedCargo!=Id && !It->SelectCargo(Id))return false;
        SelectedCargo=Id;
        InspectorPresentationModel->ShowCargo(*O, TEXT("HUD.TopStatus.TradeMap"));
        return true;
    }
    SelectedCargo=NAME_None;
    InspectorPresentationModel->ShowStatus(EHansaInspectorDataState::Empty,LOCTEXT("CargoGone","This cargo entity is no longer active."),LOCTEXT("CargoGoneRemedy","Select another vehicle or inspect the route history."),TEXT("HUD.TopStatus.TradeMap"));
    return false;
}

void AHansaRootHud::RefreshShipMenu()
{
    if (!BuildMenuPresentationModel) return;
    TArray<FHansaShipMenuEntry> Ships;
    auto Add = [&Ships](int64 Id) { Ships.Add({Id, FText::Format(LOCTEXT("FleetCog", "Cog #{0}"), FText::AsNumber(Id))}); };
    if (SimulationHost)
    {
        const auto P = SimulationHost->BuildProjection();
        if (P) for (const auto& V : P.Value.GetVehicles())
            if (V.OwnerId == SimulationHost->GetHouseId() && V.Mode == Hansa::Simulation::EHansaRouteMode::Sea) Add(int64(V.Id.GetValue()));
    }
    else if (const auto* C = Cast<AHansaStrategyPlayerController>(PlayerOwner))
    {
        const auto& P = C->GetClientProjection();
        for (const auto& V : P.Vehicles)
            if (P.OwnerHouseId > 0 && V.OwnerHouseId == P.OwnerHouseId && V.Mode == TEXT("Sea")) Add(V.VehicleId);
    }
    BuildMenuPresentationModel->SetShips(MoveTemp(Ships));
}

bool AHansaRootHud::CenterShipIntent(int64 VehicleId)
{
    // Recheck ownership at activation, including removal/ownership changes since the menu opened.
    RefreshShipMenu();
    if (!BuildMenuPresentationModel || !BuildMenuPresentationModel->GetShips().ContainsByPredicate([VehicleId](const auto& V) { return V.Id == VehicleId; })) return false;
    for (TActorIterator<AHansaCargoProjectionManager> It(GetWorld()); It; ++It)
    {
        FName Id;
        for (const auto& Cargo : It->QueryCargo())
            if (!Cargo.VehicleId.IsEmpty() && FCString::Atoi64(*Cargo.VehicleId) == VehicleId) { Id = Cargo.SemanticId; break; }
        if (Id.IsNone()) continue;
        const auto* O = It->FindObservation(Id);
        if (!O || !O->PresentationFailure.IsEmpty()) continue;
        const FName City = O->CityId;
        if (City == TEXT("City.Rostock") && ViewedCity != City)
        {
            if (bCityVisitLoading) { PendingCenterShip = VehicleId; return true; }
            PendingCenterShip = VehicleId;
            if (VisitCityIntent(City)) return true;
            PendingCenterShip = 0; return false;
        }
        if (City != TEXT("City.Rostock") && (ViewedCity == TEXT("City.Rostock") || bCityVisitLoading)) CancelCityVisit();
        // City switching can refresh projections, so never retain an observation pointer across it.
        O = It->FindObservation(Id);
        if (!O || !O->bVisible) return false;
        if (auto* Camera = PlayerOwner ? Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()) : nullptr)
        {
            Camera->ClearCameraIntents();
            Camera->FocusWorldLocationIntent(O->Location);
            It->SelectCargo(Id);
            SelectedCargo = Id;
            return true;
        }
    }
    return false;
}
#undef LOCTEXT_NAMESPACE

bool AHansaRootHud::BeginForeignConstruction(FName City,uint64 Lease,FName Building)
{
 if(City!=TEXT("City.Rostock")||!BuildMenuPresentationModel||!SimulationHost)return false;
 if(ViewedCity!=City)
 {
  PendingConstructionBuilding=Building;PendingConstructionLease=Lease;
  if(!VisitCityIntent(City)){PendingConstructionBuilding=NAME_None;PendingConstructionLease=0;return false;}
  return true;
 }
 if(!BuildMenuPresentationModel->BeginLeasedPlacement(City,Lease,Building))return false;
 TradeMapPresentationModel->CloseIntent();CityOverviewPresentationModel->CloseIntent();
 const auto P=SimulationHost->BuildProjection();
 if(P)for(const auto& L:P.Value.GetLeasedPlots())if(L.Id.GetValue()==Lease)
  if(auto* C=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr)
   {
    FVector Focus=(Hansa::Simulation::RostockPlacement::CellCenter(L.BoundsMin.X,L.BoundsMin.Y)+Hansa::Simulation::RostockPlacement::CellCenter(L.BoundsMax.X,L.BoundsMax.Y))*.5;
    Hansa::Simulation::FHansaPlacementSpec Spec;Spec.CityId=L.CityId;Spec.BuildingDefinitionId=Hansa::Simulation::FHansaBuildingTypeId::TryParse(Building.ToString()).Value;
    bool Found=false;const auto* Map=SimulationHost->FindPlacementMap(L.CityId);if(!Map)return false;
    for(int32 Y=FMath::Max(L.BoundsMin.Y,Map->BoundsMin.Y);Y<=FMath::Min(L.BoundsMax.Y,Map->BoundsMax.Y)&&!Found;++Y)for(int32 X=FMath::Max(L.BoundsMin.X,Map->BoundsMin.X);X<=FMath::Min(L.BoundsMax.X,Map->BoundsMax.X)&&!Found;++X)
    {
     Spec.Anchor={X,Y};const auto V=SimulationHost->ValidatePlacement(Spec);
     if(V.CanPlace()&&!V.GetOccupiedCells().IsEmpty()){Focus=FVector::ZeroVector;for(const auto Cell:V.GetOccupiedCells())Focus+=Hansa::Simulation::RostockPlacement::CellCenter(Cell.X,Cell.Y);Focus/=V.GetOccupiedCells().Num();Found=true;}
    }
    // Reserve the lower screen for the construction tray, including the full footprint.
    Focus-=C->GetActorForwardVector()*C->GetZoomDistance()*.2;
    C->FocusWorldLocationIntent(Focus);
   }
 return true;
}
