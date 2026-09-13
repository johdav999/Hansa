#include "UI/HansaRootHud.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaRostockQuarter.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UI/HansaFrontendPresentationModel.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/App.h"
#include "HAL/PlatformMisc.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "GameFramework/Actor.h"
#include "HansaLog.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "Save/HansaSaveSubsystem.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

#define LOCTEXT_NAMESPACE "HansaRootHud"

void AHansaRootHud::BeginPlay()
{
	Super::BeginPlay();
	PresentationModel = NewObject<UHansaHudPresentationModel>(this, TEXT("RootHudPresentation"));
	PresentationModel->InitializeDefaults();
    auto WaitingHud=PresentationModel->GetSnapshot();
    WaitingHud.Money=LOCTEXT("WaitingTreasury","Treasury unavailable");WaitingHud.MoneyTrend=FText();
    WaitingHud.Population=LOCTEXT("WaitingPopulation","Population unavailable");WaitingHud.Workforce=LOCTEXT("WaitingWorkforce","Workforce unavailable");
    WaitingHud.DateAndSeason=LOCTEXT("WaitingClock","Waiting for city");
    WaitingHud.Connection=GetNetMode()==NM_Standalone?LOCTEXT("LocalSession","Local game"):LOCTEXT("NetworkSession","Network game");
    PresentationModel->ApplySnapshot(WaitingHud);
	if (AHansaGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AHansaGameMode>() : nullptr)
	{
		SimulationHost = GameMode->GetSimulationHost();
	}
	BuildMenuPresentationModel = NewObject<UHansaBuildMenuPresentationModel>(this, TEXT("BuildMenuPresentation"));
	InspectorPresentationModel = NewObject<UHansaInspectorPresentationModel>(this, TEXT("InspectorPresentation"));
	InspectorPresentationModel->InitializeDefaults();
	InspectorPresentationModel->BindRuntime(SimulationHost);
	InspectorPresentationModel->OnFrameRequested().AddUObject(this, &AHansaRootHud::HandleInspectorFrameRequested);
	CityOverviewPresentationModel = NewObject<UHansaCityOverviewPresentationModel>(this, TEXT("CityOverviewPresentation"));
	CityOverviewPresentationModel->InitializeDefaults();
    CityOverviewPresentationModel->VisitRequested=[this](FName City){return VisitCityIntent(City);};
    CityOverviewPresentationModel->OnRefreshRequested().AddUObject(this,&AHansaRootHud::RefreshCityOverview);
	MarketTablePresentationModel = NewObject<UHansaMarketTablePresentationModel>(this, TEXT("MarketTablePresentation"));
	MarketTablePresentationModel->InitializeDefaults();
	TradeMapPresentationModel = NewObject<UHansaTradeMapPresentationModel>(this, TEXT("TradeMapPresentation"));
	TradeMapPresentationModel->InitializeDefaults();
    TradeMapPresentationModel->VisitRequested=[this](FName City){return VisitCityIntent(City);};
	TradeMapPresentationModel->BindRuntime(SimulationHost);
	ResearchPresentationModel = NewObject<UHansaResearchPresentationModel>(this, TEXT("ResearchPresentation"));
	ResearchPresentationModel->SetQueueIntent([this](const FString& TechnologyId)
	{
		return SimulationHost != nullptr && SimulationHost->QueueResearch(TechnologyId).IsSuccess();
	});
    ResearchPresentationModel->SetEffectIntent([this](const Hansa::Simulation::FHansaCompiledResearchEffect& Effect)
    {
        if(!SimulationHost || !GetWorld()) return false;
        const auto* Registry=SimulationHost->GetEconomicRegistry();
        const auto Projection=SimulationHost->BuildProjection();
        if(!Registry || !Projection) return false;
        if(Effect.TargetStableId.StartsWith(TEXT("City."))) {
            ResearchPresentationModel->Close();
            CityOverviewPresentationModel->SelectCityIntent(FName(*Effect.TargetStableId));
            CityOverviewPresentationModel->Open(TEXT("HUD.TopStatus.Research.Open"));
            CityOverviewPresentationModel->SelectTabIntent(EHansaCityOverviewTab::Market);
            return true;
        }
        for(TActorIterator<AHansaBuildingWorldProjectionActor> It(GetWorld());It;++It) {
            const auto* Building=Projection.Value.GetBuildingWorldProjections().FindByPredicate([&](const auto& B){return B.BuildingId==It->GetBuildingId();});
            if(!Building || Building->OwnerId!=SimulationHost->GetHouseId())continue;
            const auto* Definition=Registry->FindBuilding(It->GetBuildingDefinitionId());
            if(Definition && (Definition->StableId==Effect.TargetStableId || Definition->RecipeIds.Contains(Effect.TargetStableId))) {
                ResearchPresentationModel->Close();
                RefreshInspectorFromBuilding(**It);
                HandleInspectorFrameRequested(static_cast<int64>(It->GetBuildingId().GetValue()));
                return true;
            }
        }
        for(const auto& Route:Projection.Value.GetRoutes()) {
            if(Route.OwnerId!=SimulationHost->GetHouseId() || Route.Lifecycle==Hansa::Simulation::EHansaRouteLifecycleState::Cancelled)continue;
            bool Match=Route.RouteDefinitionId.ToString()==Effect.TargetStableId;
            if(Effect.TargetStableId.StartsWith(TEXT("Vehicle."))) {
                const auto* Vehicle=Projection.Value.GetVehicles().FindByPredicate([&](const auto& V){return V.Id==Route.VehicleId;});
                Match=Vehicle && Vehicle->DefinitionId.ToString()==Effect.TargetStableId;
            }
            if(Match) {
                ResearchPresentationModel->Close();
                TradeMapPresentationModel->Open(TEXT("HUD.TopStatus.Research.Open"));
                TradeMapPresentationModel->SelectRouteIntent(Route.Id.GetValue());
                return true;
            }
        }
        return false;
    });
	ScenarioPresentationModel = NewObject<UHansaScenarioPresentationModel>(this, TEXT("ScenarioPresentation"));
    ScenarioPresentationModel->OnChanged().AddWeakLambda(this,[this](const FHansaScenarioPresentationSnapshot& S,uint64){
        if(S.bOpen&&!bSessionClockHeld){bSessionClockHeld=true;SessionResumeSpeed=uint8(PresentationModel->GetSnapshot().Speed);PresentationModel->SetSpeed(EHansaHudGameSpeed::Paused);if(SimulationHost)SimulationHost->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);}
        else if(!S.bOpen&&bSessionClockHeld){bSessionClockHeld=false;PresentationModel->SetSpeed(static_cast<EHansaHudGameSpeed>(SessionResumeSpeed));}
    });
    ScenarioPresentationModel->SetSessionIntent([this](FName Action){if(Action==TEXT("Settings")&&FrontendPresentationModel){FrontendPresentationModel->OpenSettings();return;}if(Action==TEXT("ReturnTitle")&&FrontendPresentationModel){FrontendPresentationModel->Request(Action);return;}if(Action==TEXT("SaveLoad")&&SaveLoadPresentationModel){SaveLoadPresentationModel->Open(TEXT("Scenario.SaveLoad"));if(RootHudWidget)RootHudWidget->FocusSemanticId(TEXT("SaveLoad.Close"));}});
    ScenarioPresentationModel->InitializeDefaults();
    ScenarioPresentationModel->LoadHelpPreferences(FPaths::ProjectSavedDir()/TEXT("Config/HansaSessionHelp.ini"));
	SaveSubsystem = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UHansaSaveSubsystem>() : nullptr;
	if (SaveSubsystem != nullptr) {
        SaveSubsystem->BindRuntime(SimulationHost);
        SaveSubsystem->OnLoaded().AddWeakLambda(this,[this]{
            if(PresentationModel)PresentationModel->ResetCashHistory();
            if(FrontendPresentationModel)FrontendPresentationModel->SessionStarted();
            if(SaveLoadPresentationModel)SaveLoadPresentationModel->SetSavingAllowed(true);
            if(ScenarioPresentationModel)ScenarioPresentationModel->SessionRestored();
            SessionResumeSpeed=uint8(EHansaHudGameSpeed::Normal);
            if(PresentationModel)PresentationModel->SetSpeed(EHansaHudGameSpeed::Paused);
        });
    }
	SaveLoadPresentationModel = NewObject<UHansaSaveLoadPresentationModel>(this, TEXT("SaveLoadPresentation"));
	SaveLoadPresentationModel->Bind(SaveSubsystem);
    FrontendPresentationModel=NewObject<UHansaFrontendPresentationModel>(this);
    FrontendPresentationModel->Initialize(SimulationHost&&SimulationHost->IsReady());
    FrontendPresentationModel->LoadPreferences(FPaths::ProjectSavedDir()/TEXT("Config/HansaSystem.ini"));
    if(auto* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)FrontendPresentationModel->SetDisplayState(Settings->IsVSyncEnabled(),Settings->GetFullscreenMode()!=EWindowMode::Windowed);
    FrontendPresentationModel->SetIntent([this](FName Action){HandleFrontendIntent(Action);});
    if(SaveSubsystem){FrontendPresentationModel->RefreshSlots(SaveSubsystem->GetSlots());SaveSubsystem->OnChanged().AddWeakLambda(this,[this]{FrontendPresentationModel->RefreshSlots(SaveSubsystem->GetSlots());});}
    if(auto* Camera=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr)SessionStartCameraFocus=Camera->GetActorLocation();
    ApplySystemPreferences();
    GetWorldTimerManager().SetTimer(AutosaveTimer,FTimerDelegate::CreateWeakLambda(this,[this]{if(FrontendPresentationModel&&FrontendPresentationModel->GetSnapshot().bHasSession&&SimulationHost&&SimulationHost->GetSpeed()!=EHansaRuntimeSimulationSpeed::Paused&&!SaveLoadPresentationModel->GetSnapshot().bOpen){FText Error,Remedy;if(!SaveSubsystem->Save(EHansaSaveSlotId::Autosave,TEXT("Autosave"),Error,Remedy)){ScenarioPresentationModel->OpenPause();SaveLoadPresentationModel->Open(TEXT("Scenario.SaveLoad"));SaveLoadPresentationModel->ReportOperationFailure(Error,Remedy);if(RootHudWidget)RootHudWidget->FocusSemanticId(TEXT("SaveLoad.Close"));}}}),300.f,true);

	RefreshScenario();
	MarketTablePresentationModel->OnRouteRequested().AddUObject(this, &AHansaRootHud::HandleMarketRouteRequested);
    MarketTablePresentationModel->OnBuildingRequested().AddUObject(this, &AHansaRootHud::HandleCityOverviewRelatedTarget);
	CityOverviewPresentationModel->OnRelatedTargetRequested().AddUObject(this, &AHansaRootHud::HandleCityOverviewRelatedTarget);
    InspectorPresentationModel->OnRelatedTargetRequested().AddWeakLambda(this, [this](FName Target)
    {
        if (!CityOverviewPresentationModel) return;
        CityOverviewPresentationModel->Open(Target == TEXT("CityOverview.Market") ? TEXT("Inspector.Action.ViewStorage") : TEXT("Inspector.Action.OpenRelated"));
        CityOverviewPresentationModel->SelectTabIntent(Target == TEXT("CityOverview.Market") ? EHansaCityOverviewTab::Market : EHansaCityOverviewTab::Production);
        if (RootHudWidget) RootHudWidget->FocusSemanticId(TEXT("CityOverview.Close"));
    });
	FString BuildInitializationError;
	const bool bBuildInitialized = SimulationHost != nullptr
		? BuildMenuPresentationModel->InitializeForLubeck(GetWorld(), SimulationHost, BuildInitializationError)
		: BuildMenuPresentationModel->InitializeForLubeck(GetWorld(), BuildInitializationError);
	if (!bBuildInitialized)
	{
		UE_LOG(LogHansa, Error, TEXT("Hansa build menu initialization failed: %s"), *BuildInitializationError);
	}
	BuildMenuChangedHandle = BuildMenuPresentationModel->OnChanged().AddUObject(
		this, &AHansaRootHud::HandleBuildMenuPresentationChanged);
	HudPresentationChangedHandle = PresentationModel->OnChanged().AddUObject(
		this, &AHansaRootHud::HandleHudPresentationChanged);
	if (SimulationHost != nullptr)
	{
		SimulationAdvancedHandle = SimulationHost->OnSimulationAdvanced().AddUObject(
			this, &AHansaRootHud::HandleSimulationAdvanced);
	}
	RefreshCityOverview();

	FIntPoint ViewportSize(1280, 720);
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		FVector2D MeasuredViewportSize;
		GEngine->GameViewport->GetViewportSize(MeasuredViewportSize);
		if (MeasuredViewportSize.X > 0.0 && MeasuredViewportSize.Y > 0.0)
		{
			ViewportSize = FIntPoint(FMath::RoundToInt(MeasuredViewportSize.X), FMath::RoundToInt(MeasuredViewportSize.Y));
		}
	}
	Hansa::UI::FUiPreferences Preferences;
	if(GConfig){
		GConfig->GetFloat(TEXT("Hansa.UI"),TEXT("Scale"),Preferences.UiScale,GGameUserSettingsIni);
		GConfig->GetBool(TEXT("Hansa.UI"),TEXT("HighContrast"),Preferences.bHighContrast,GGameUserSettingsIni);
		GConfig->GetBool(TEXT("Hansa.UI"),TEXT("LargeText"),Preferences.bLargeText,GGameUserSettingsIni);
		GConfig->GetBool(TEXT("Hansa.UI"),TEXT("ReducedMotion"),Preferences.bReducedMotion,GGameUserSettingsIni);
	}
	SAssignNew(RootHudWidget, Hansa::UI::SHansaRootHud).Preferences(Preferences)
		.Model(PresentationModel)
		.BuildModel(BuildMenuPresentationModel)
		.InspectorModel(InspectorPresentationModel)
		.CityOverviewModel(CityOverviewPresentationModel)
		.MarketTableModel(MarketTablePresentationModel)
		.TradeMapModel(TradeMapPresentationModel)
		.ResearchModel(ResearchPresentationModel)
		.FrontendModel(FrontendPresentationModel)
		.ScenarioModel(ScenarioPresentationModel)
		.SaveLoadModel(SaveLoadPresentationModel)
		.PlacementController(Cast<AHansaStrategyPlayerController>(PlayerOwner))
		.InitialViewportSize(ViewportSize);
	ViewportContent = RootHudWidget;
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->AddViewportWidgetContent(ViewportContent.ToSharedRef(), 20);
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]{if(RootHudWidget&&ScenarioPresentationModel&&ScenarioPresentationModel->GetSnapshot().bOpen)RootHudWidget->FocusSemanticId(TEXT("Frontend.NewGame"));}));
	}
	FViewport::ViewportResizedEvent.AddUObject(this, &AHansaRootHud::HandleViewportResized);

	if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PlayerOwner))
	{
		Controller->OnWorldSelectionChanged.AddDynamic(this, &AHansaRootHud::HandleWorldSelectionChanged);
	}
}

void AHansaRootHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(CityVisitTimeout);

	FViewport::ViewportResizedEvent.RemoveAll(this);
	if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PlayerOwner))
	{
		Controller->OnWorldSelectionChanged.RemoveDynamic(this, &AHansaRootHud::HandleWorldSelectionChanged);
	}
	if (InspectorPresentationModel != nullptr) InspectorPresentationModel->OnFrameRequested().RemoveAll(this);
	if (CityOverviewPresentationModel != nullptr) CityOverviewPresentationModel->OnRelatedTargetRequested().RemoveAll(this);
	if (MarketTablePresentationModel != nullptr) { MarketTablePresentationModel->OnRouteRequested().RemoveAll(this); MarketTablePresentationModel->OnBuildingRequested().RemoveAll(this); }
	if (PresentationModel != nullptr && HudPresentationChangedHandle.IsValid())
	{
		PresentationModel->OnChanged().Remove(HudPresentationChangedHandle);
	}
	if (BuildMenuPresentationModel != nullptr && BuildMenuChangedHandle.IsValid())
	{
		BuildMenuPresentationModel->OnChanged().Remove(BuildMenuChangedHandle);
	}
	if (SimulationHost != nullptr && SimulationAdvancedHandle.IsValid())
	{
		SimulationHost->OnSimulationAdvanced().Remove(SimulationAdvancedHandle);
	}
	if (ViewportContent.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ViewportContent.ToSharedRef());
	}
	RootHudWidget.Reset();
	ViewportContent.Reset();
	CityOverviewPresentationModel = nullptr;
	MarketTablePresentationModel = nullptr;
	TradeMapPresentationModel = nullptr;
	ResearchPresentationModel = nullptr;
	ScenarioPresentationModel = nullptr;
	SaveLoadPresentationModel = nullptr;
	SaveSubsystem = nullptr;
	GetWorldTimerManager().ClearTimer(DisplayRevertTimer);GetWorldTimerManager().ClearTimer(AutosaveTimer);
	Super::EndPlay(EndPlayReason);
}

void AHansaRootHud::HandleViewportResized(FViewport* Viewport, const uint32 Unused)
{
	(void)Unused;
	if (RootHudWidget.IsValid() && Viewport != nullptr)
	{
		RootHudWidget->SetPresentationSize(Viewport->GetSizeXY());
	}
}

void AHansaRootHud::HandleWorldSelectionChanged(AActor* SelectedActor, const FHitResult& HitResult)
{
    if(auto* Cargo=Cast<AHansaCargoVehiclePresentation>(SelectedActor)) {InspectCargo(Cargo->SemanticId); return;}
    SelectedCargo=NAME_None;
    for(TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)It->ClearSelection();
	if(ViewedCity==TEXT("City.Rostock"))
    {
        if(Cast<AHansaRostockQuarter>(SelectedActor))InspectRostockRole(AHansaRostockQuarter::RoleFor(HitResult.GetComponent()));
        else if(SelectedActor&&SelectedActor->Tags.Contains(TEXT("City.Rostock")))InspectRostockRole(TEXT("Cargo"));
        return;
    }
    (void)HitResult;
	if (PresentationModel == nullptr)
	{
		return;
	}
	if (SelectedActor == nullptr)
	{
		if (InspectorPresentationModel != nullptr && InspectorPresentationModel->GetSnapshot().bOpen) InspectorPresentationModel->CloseIntent();
		PresentationModel->SetSelection(
			LOCTEXT("NoSelection", "Build and selection"), FText::GetEmpty(), FText::GetEmpty(), false);
		return;
	}
	if (const AHansaBuildingWorldProjectionActor* Building = Cast<AHansaBuildingWorldProjectionActor>(SelectedActor))
	{
		RefreshInspectorFromBuilding(*Building);
	}
	const FText SelectedName = FText::FromString(SelectedActor->GetName());
	PresentationModel->SetSelection(
		FText::Format(LOCTEXT("SelectedSummary", "Selected · {0}"), SelectedName),
		SelectedName,
		LOCTEXT("SelectedObjectDetails", "Selection details and actions will appear here as authoritative presentation data becomes available."),
		true);
}

void AHansaRootHud::HandleHudPresentationChanged(
	const FHansaHudPresentationSnapshot& Snapshot,
	const uint64 Revision)
{
	(void)Revision;
	if (SimulationHost == nullptr) return;
	switch (Snapshot.Speed)
	{
	case EHansaHudGameSpeed::Paused: SimulationHost->SetSpeed(EHansaRuntimeSimulationSpeed::Paused); break;
	case EHansaHudGameSpeed::Fast: SimulationHost->SetSpeed(EHansaRuntimeSimulationSpeed::Fast); break;
	case EHansaHudGameSpeed::Fastest: SimulationHost->SetSpeed(EHansaRuntimeSimulationSpeed::Fastest); break;
	default: SimulationHost->SetSpeed(EHansaRuntimeSimulationSpeed::Normal); break;
	}
    if (InspectorPresentationModel) InspectorPresentationModel->RefreshProductionClock();
}

void AHansaRootHud::HandleBuildMenuPresentationChanged(
	const FHansaBuildMenuSnapshot& Snapshot, const uint64 Revision)
{
    if(ScenarioPresentationModel&&Snapshot.bOpen)ScenarioPresentationModel->OfferHelp(Snapshot.SelectedCategory==EHansaBuildCategory::Roads?EHansaSessionHelpTopic::Roads:EHansaSessionHelpTopic::Construction);
    (void)Revision;
    if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PlayerOwner))
	{
		Controller->RefreshBuildingPlacementPresentation();
	}
}

void AHansaRootHud::HandleSimulationAdvanced(const int64 SimulationTick)
{
	(void)SimulationTick;
	RefreshCityOverview();
    if(!SelectedCargo.IsNone() && InspectorPresentationModel && InspectorPresentationModel->GetSnapshot().bOpen) {InspectCargo(SelectedCargo); return;}
	RefreshScenario();
	if (InspectorPresentationModel == nullptr || !InspectorPresentationModel->GetSnapshot().bOpen || GetWorld() == nullptr)
	{
		return;
	}
	const int64 BuildingValue = InspectorPresentationModel->GetSnapshot().BuildingValue;
	if (BuildingValue <= 0) return;
	const auto BuildingId = Hansa::Simulation::FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue));
	if (!BuildingId) return;
	for (TActorIterator<AHansaPlacementProjectionManager> It(GetWorld()); It; ++It)
	{
		if (const AHansaBuildingWorldProjectionActor* Building = It->FindProjectionActor(BuildingId.Value))
		{
			RefreshInspectorFromBuilding(*Building);
			return;
		}
	}
	InspectorPresentationModel->CloseIntent();
}

void AHansaRootHud::RefreshCityOverview()
{
    RefreshRostockPresentation();
	if (CityOverviewPresentationModel == nullptr || SimulationHost == nullptr) return;
	const Hansa::Simulation::FHansaEconomicRegistry* Registry = SimulationHost->GetEconomicRegistry();
	const auto Projection = SimulationHost->BuildProjection();
	if (Registry == nullptr || !Projection)
	{
		CityOverviewPresentationModel->SetError(
			LOCTEXT("CityOverviewProjectionFailure", "The city report could not be prepared."),
			LOCTEXT("CityOverviewProjectionRemedy", "Try again to request a fresh city report."));
		return;
	}
	CityOverviewPresentationModel->ApplyProjection(
		Projection.Value, *Registry, Hansa::Simulation::FHansaCityDefinitionId::TryParse(CityOverviewPresentationModel->GetSnapshot().CityStableId.ToString()).Value,
        CityOverviewPresentationModel->GetSnapshot().CityStableId==TEXT("City.Rostock")?LOCTEXT("Rostock","Rostock"):LOCTEXT("Lubeck", "Lübeck"), 0, SimulationHost);
	if (PresentationModel != nullptr)
	{
		// PublishCityVisit selects the current world city for every top-menu metric.
        PublishCityVisit();
		PresentationModel->ApplyMarketAlerts(Projection.Value, SimulationHost->GetCityId(), LOCTEXT("Lubeck", "Lübeck"));
	}
	if (MarketTablePresentationModel != nullptr)
	{
		MarketTablePresentationModel->ApplyProjection(Projection.Value, *Registry, SimulationHost->GetCityId());
	}
	if (TradeMapPresentationModel != nullptr)
	{
		TradeMapPresentationModel->ApplyProjection(Projection.Value, *Registry);
	}
	if (ResearchPresentationModel != nullptr)
	{
		const Hansa::Simulation::FHansaHouseResearchState* Research = Projection.Value.GetResearch().FindByPredicate(
			[this](const Hansa::Simulation::FHansaHouseResearchState& State)
			{
				return SimulationHost != nullptr && State.HouseId == SimulationHost->GetHouseId();
			});
		if (Research != nullptr)
		{
			if (ResearchPresentationModel->GetRevision() == 0) ResearchPresentationModel->Initialize(*Registry, *Research);
			else ResearchPresentationModel->Refresh(*Registry, *Research);
		} else ResearchPresentationModel->SetError(LOCTEXT("ResearchUnavailable", "Research is unavailable for this house."));
	}
}

void AHansaRootHud::RefreshScenario()
{
	if (ScenarioPresentationModel == nullptr || SimulationHost == nullptr) return;
	if (const Hansa::Simulation::FHansaScenarioProgress* Progress = SimulationHost->GetScenarioProgress())
	{
		ScenarioPresentationModel->ApplyProgress(*Progress);
        if(PresentationModel){
            auto State=PresentationModel->GetSnapshot();
            auto* Objective=State.Alerts.FindByPredicate([](const auto& A){return A.StableId==TEXT("Objectives");});
            const auto& Scenario=ScenarioPresentationModel->GetSnapshot();
            const auto* Path=Scenario.Paths.FindByPredicate([&](const auto& P){return P.StableId==Scenario.SelectedVictoryId;});
            if(Objective&&Path){
                const auto* Pending=Path->Objectives.FindByPredicate([](const auto& O){return !O.bMet;});
                Objective->Label=Pending?Pending->Label:Path->Label;
                Objective->Causal.Problem=Objective->Label;
                Objective->Causal.Cause=Pending?Pending->Status:Path->SustainProgress;
                Objective->Causal.Evidence=Pending?Pending->Progress:Scenario.StateLabel;
                Objective->Causal.Remedy=LOCTEXT("ReviewScenario","Review the scenario objectives and sustained progress.");
                Objective->Causal.RelatedSemanticId=TEXT("Scenario.Root");
                PresentationModel->ApplySnapshot(State);
            }
        }
	}
}

void AHansaRootHud::HandleMarketRouteRequested(const FName GoodStableId)
{
	if (CityOverviewPresentationModel != nullptr && CityOverviewPresentationModel->GetSnapshot().bOpen)
	{
		CityOverviewPresentationModel->CloseIntent();
	}
	if (TradeMapPresentationModel != nullptr)
	{
		const auto* Row = MarketTablePresentationModel ? MarketTablePresentationModel->GetSnapshot().AllRows.FindByPredicate(
            [GoodStableId](const auto& R){ return R.GoodStableId == GoodStableId; }) : nullptr;
        // The full market is the home-city report. Shortages need an import, not an export.
        TradeMapPresentationModel->Open(TEXT("Market.Detail.Action.BeginRoute"), GoodStableId,
            Row && Row->bShortage ? FName(TEXT("City.Rostock")) : FName(TEXT("City.Lubeck")));
	}
}

void AHansaRootHud::HandleCityOverviewRelatedTarget(const FName SemanticId, const int64 BuildingValue)
{
	// Goods targets are resolved inside the overview by switching to its production surface.
	// Only Inspector.Building targets may use the numeric building payload below.
	if (SemanticId.ToString().StartsWith(TEXT("CityOverview.Production.Good."))) return;
	if (BuildingValue <= 0 || GetWorld() == nullptr || InspectorPresentationModel == nullptr) return;
	const auto BuildingId = Hansa::Simulation::FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue));
	if (!BuildingId) return;
	for (TActorIterator<AHansaPlacementProjectionManager> It(GetWorld()); It; ++It)
	{
		if (const AHansaBuildingWorldProjectionActor* Building = It->FindProjectionActor(BuildingId.Value))
		{
			if (CityOverviewPresentationModel != nullptr) CityOverviewPresentationModel->CloseIntent();
			RefreshInspectorFromBuilding(*Building);
			InspectorPresentationModel->SetFocusedSemanticId(SemanticId);
            if(SemanticId.ToString().StartsWith(TEXT("Market.Detail."))) HandleInspectorFrameRequested(BuildingValue);
			return;
		}
	}
}

void AHansaRootHud::RefreshInspectorFromBuilding(const AHansaBuildingWorldProjectionActor& Building)
{
	if (InspectorPresentationModel == nullptr) return;
	const FHansaBuildCardPresentation* Card = BuildMenuPresentationModel != nullptr
		? BuildMenuPresentationModel->FindCardPresentation(FName(*Building.GetBuildingDefinitionId())) : nullptr;
	InspectorPresentationModel->ShowWorldBuilding(
		Building.GetBuildingDefinitionId(), Card != nullptr ? Card->Name : FText::GetEmpty(),
		Card != nullptr ? Card->InputOutput : FText::GetEmpty(), static_cast<int64>(Building.GetBuildingId().GetValue()),
		Hansa::Simulation::LexToString(Building.GetWorldStatus()),
		Hansa::Simulation::LexToString(Building.GetProductionBlocker()), TEXT("World.Selection"));
}

void AHansaRootHud::HandleInspectorFrameRequested(const int64 BuildingValue)
{
    if(BuildingValue==0 && !SelectedCargo.IsNone())
    {
        for(TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)
            if(const auto* O=It->FindObservation(SelectedCargo))
                if(O->bVisible) if(auto* Camera=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr)Camera->FocusWorldLocationIntent(O->Location);
        return;
    }
    if(ViewedCity==TEXT("City.Rostock")||bCityVisitLoading)CancelCityVisit();
	if (BuildingValue <= 0 || GetWorld() == nullptr) return;
	for (TActorIterator<AHansaPlacementProjectionManager> It(GetWorld()); It; ++It)
	{
		const auto BuildingId = Hansa::Simulation::FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue));
		if (!BuildingId) return;
		if (AHansaBuildingWorldProjectionActor* Actor = It->FindProjectionActor(BuildingId.Value))
		{
			It->SelectBuilding(BuildingId.Value);
			if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PlayerOwner))
			{
				if (AHansaStrategyCameraPawn* Camera = Cast<AHansaStrategyCameraPawn>(Controller->GetPawn())) Camera->FocusWorldLocationIntent(Actor->GetActorLocation());
			}
			return;
		}
	}
}


void AHansaRootHud::ApplySystemPreferences(){
 if(!FrontendPresentationModel)return;const auto& S=FrontendPresentationModel->GetSnapshot();FApp::SetVolumeMultiplier(S.Volume);
 if(auto* Camera=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr){Camera->PanUnitsPerSecond=2400.f*S.CameraSpeed;Camera->RotationDegreesPerSecond=75.f*S.CameraSpeed;Camera->ZoomUnitsPerStep=900.f*S.CameraSpeed;Camera->bEnableMouseEdgePan=S.bEdgeScroll;}
}
void AHansaRootHud::HandleFrontendIntent(FName Action){
 if(Action==TEXT("SettingsChanged")){ApplySystemPreferences();if(auto* G=GEngine->GetGameUserSettings()){G->SetVSyncEnabled(FrontendPresentationModel->GetSnapshot().bVSync);G->ApplyNonResolutionSettings();G->SaveSettings();}return;}
 if(Action==TEXT("PreviewDisplay")){if(auto* G=GEngine->GetGameUserSettings()){PreviousWindowMode=int32(G->GetFullscreenMode());G->SetFullscreenMode(FrontendPresentationModel->GetSnapshot().bBorderless?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);G->ApplyResolutionSettings(false);GetWorldTimerManager().SetTimer(DisplayRevertTimer,FTimerDelegate::CreateWeakLambda(this,[this]{if(FrontendPresentationModel->GetSnapshot().PendingAction==TEXT("Display"))FrontendPresentationModel->Back();}),15.f,false);}return;}
 if(Action==TEXT("Display")||Action==TEXT("RevertDisplay")){GetWorldTimerManager().ClearTimer(DisplayRevertTimer);if(auto* G=GEngine->GetGameUserSettings()){if(Action==TEXT("RevertDisplay")){G->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousWindowMode));G->ApplyResolutionSettings(false);}else{G->ConfirmVideoMode();G->SaveSettings();}FrontendPresentationModel->SetDisplayState(G->IsVSyncEnabled(),G->GetFullscreenMode()!=EWindowMode::Windowed);}return;}
 if(Action==TEXT("Load")){SaveLoadPresentationModel->SetSavingAllowed(FrontendPresentationModel->GetSnapshot().bHasSession);SaveLoadPresentationModel->Open(TEXT("Frontend.Load"));RootHudWidget->FocusSemanticId(TEXT("SaveLoad.Close"));return;}
 if(Action==TEXT("ReturnTitle")){BuildMenuPresentationModel->SetOpen(false);BuildMenuPresentationModel->CancelIntent();InspectorPresentationModel->CloseIntent();CityOverviewPresentationModel->CloseIntent();TradeMapPresentationModel->CloseIntent();ResearchPresentationModel->Close();ScenarioPresentationModel->OpenPause();FrontendPresentationModel->ReturnToTitle();return;}
 if(Action==TEXT("Quit")){FPlatformMisc::RequestExit(false);return;}
 if(Action!=TEXT("NewGame")&&Action!=TEXT("Continue"))return;
 // Defer until loading feedback has been painted; no blocking provider or filesystem work in Slate callbacks.
 FTimerHandle OperationTimer;GetWorldTimerManager().SetTimer(OperationTimer,FTimerDelegate::CreateWeakLambda(this,[this,Action]{
  FText Error,Remedy;bool Success=false;
  if(Action==TEXT("NewGame")){if(PresentationModel)PresentationModel->ResetCashHistory();CancelCityVisit();FString Reason;Success=SimulationHost&&SimulationHost->StartNewGame(Reason);Error=FText::FromString(Reason);if(Success){if(auto* Camera=PlayerOwner?Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()):nullptr){Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(SessionStartCameraFocus);}ScenarioPresentationModel->InitializeDefaults();ScenarioPresentationModel->LoadHelpPreferences(FPaths::ProjectSavedDir()/TEXT("Config/HansaSessionHelp.ini"));RefreshScenario();SessionResumeSpeed=uint8(EHansaHudGameSpeed::Normal);BuildMenuPresentationModel->CancelIntent();BuildMenuPresentationModel->SetOpen(false);}}
  else if(SaveSubsystem)Success=SaveSubsystem->Load(FrontendPresentationModel->GetSnapshot().ContinueSlot,Error,Remedy);
  FrontendPresentationModel->Complete(Success,Error);if(Success){SaveLoadPresentationModel->SetSavingAllowed(true);ApplySystemPreferences();GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,Action]{RootHudWidget->FocusSemanticId(Action==TEXT("NewGame")?TEXT("Scenario.Begin"):TEXT("Scenario.Resume"));}));}
 }),.1f,false);
}

#undef LOCTEXT_NAMESPACE
