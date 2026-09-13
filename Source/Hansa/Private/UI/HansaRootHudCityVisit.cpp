#include "UI/HansaRootHud.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaRostockQuarter.h"
#include "World/HansaStrategyCameraPawn.h"
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
        RostockLevel->OnLevelShown.AddDynamic(this,&AHansaRootHud::HandleRostockShown);
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
    CityVisitStatus=LOCTEXT("Visiting","Rostock · trade city · construction unavailable. Use Return to Lübeck to go home.");
    RefreshRostockPresentation();PublishCityVisit();if(RootHudWidget)RootHudWidget->FocusSemanticId(TEXT("HUD.TopStatus.CityOverview"));
}
void AHansaRootHud::CancelCityVisit()
{
    const bool HadVisit=bCityVisitLoading||ViewedCity==TEXT("City.Rostock");bCityVisitLoading=false;GetWorldTimerManager().ClearTimer(CityVisitTimeout);
    if(RostockLevel)RostockLevel->SetShouldBeVisible(false);
    for(TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)It->SetRostockVisible(false);
    ViewedCity=TEXT("City.Lubeck");if(BuildMenuPresentationModel)BuildMenuPresentationModel->SetConstructionAllowed(true);
    if(HadVisit)if(auto* C=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr){C->RestoreHomeBounds();C->RestoreViewState(HomeView);}
    CityVisitStatus=LOCTEXT("Home","Lübeck · construction available.");SelectedRostockRole=NAME_None;
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
            if(Projection && City)PresentationModel->ApplyRuntimeStatus(Projection.Value,City.Value,SimulationHost->GetHouseId());
        }
        auto S=PresentationModel->GetSnapshot();S.bRemoteCityView=ViewedCity==TEXT("City.Rostock")||bCityVisitLoading;
        S.CityBreadcrumb=ViewedCity==TEXT("City.Rostock")?LOCTEXT("Breadcrumb","Trade city / Rostock"):LOCTEXT("HomeBreadcrumb","Free City / Lübeck");
        PresentationModel->ApplySnapshot(S);
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
    if(ViewedCity!=TEXT("City.Rostock")||!SimulationHost)return;
    const auto P=SimulationHost->BuildProjection();if(!P)return;
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
    for(const auto& Route:P.Value.GetRoutes())
        if(Route.OwnerId==SimulationHost->GetHouseId() && Route.LastTransfer.Kind==Hansa::Simulation::EHansaRouteCargoActionKind::Unload && Route.LastTransfer.CityId.ToString()==TEXT("City.Rostock") && Route.LastTransfer.AppliedQuantity.GetRawValue()>0 && Route.LastTransfer.Tick.GetValue()>ReceiptTick)
        {
            ReceiptTick=Route.LastTransfer.Tick.GetValue();
            LatestReceipt=FText::Format(LOCTEXT("Receipt","Last recorded unload: {0} {1}, tick {2}."),FText::AsNumber(double(Route.LastTransfer.AppliedQuantity.GetRawValue())/1000.),FText::FromString(Route.LastTransfer.GoodId.ToString().RightChop(5)),FText::AsNumber(ReceiptTick));
        }
    RostockArrivalSummary=FText::Format(LOCTEXT("Arrival","{0} at berth · {1} approaching · {2} units aboard. Cargo is the current route projection; market reports retain their age."),FText::AsNumber(AtBerth),FText::AsNumber(Approaching),FText::AsNumber(double(Cargo)/1000.));
    if(Missing>0)RostockArrivalSummary=FText::Format(LOCTEXT("MissingCargoSkin","{0} Presentation unavailable for {1} vessel(s); inspect the route for cargo."),RostockArrivalSummary,FText::AsNumber(Missing));
    if(!LatestReceipt.IsEmpty())RostockArrivalSummary=FText::Format(LOCTEXT("ArrivalReceipt","{0} {1}"),RostockArrivalSummary,LatestReceipt);
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
#undef LOCTEXT_NAMESPACE
