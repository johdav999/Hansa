#include "UI/HansaRootHud.h"

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
	if (AHansaGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AHansaGameMode>() : nullptr)
	{
		SimulationHost = GameMode->GetSimulationHost();
	}
	BuildMenuPresentationModel = NewObject<UHansaBuildMenuPresentationModel>(this, TEXT("BuildMenuPresentation"));
	InspectorPresentationModel = NewObject<UHansaInspectorPresentationModel>(this, TEXT("InspectorPresentation"));
	InspectorPresentationModel->InitializeDefaults();
	InspectorPresentationModel->OnFrameRequested().AddUObject(this, &AHansaRootHud::HandleInspectorFrameRequested);
	CityOverviewPresentationModel = NewObject<UHansaCityOverviewPresentationModel>(this, TEXT("CityOverviewPresentation"));
	CityOverviewPresentationModel->InitializeDefaults();
	MarketTablePresentationModel = NewObject<UHansaMarketTablePresentationModel>(this, TEXT("MarketTablePresentation"));
	MarketTablePresentationModel->InitializeDefaults();
	TradeMapPresentationModel = NewObject<UHansaTradeMapPresentationModel>(this, TEXT("TradeMapPresentation"));
	TradeMapPresentationModel->InitializeDefaults();
	TradeMapPresentationModel->BindRuntime(SimulationHost);
	ResearchPresentationModel = NewObject<UHansaResearchPresentationModel>(this, TEXT("ResearchPresentation"));
	ResearchPresentationModel->SetQueueIntent([this](const FString& TechnologyId)
	{
		return SimulationHost != nullptr && SimulationHost->QueueResearch(TechnologyId).IsSuccess();
	});
	ScenarioPresentationModel = NewObject<UHansaScenarioPresentationModel>(this, TEXT("ScenarioPresentation"));
	ScenarioPresentationModel->InitializeDefaults();
	SaveSubsystem = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UHansaSaveSubsystem>() : nullptr;
	if (SaveSubsystem != nullptr) SaveSubsystem->BindRuntime(SimulationHost);
	SaveLoadPresentationModel = NewObject<UHansaSaveLoadPresentationModel>(this, TEXT("SaveLoadPresentation"));
	SaveLoadPresentationModel->Bind(SaveSubsystem);
	RefreshScenario();
	MarketTablePresentationModel->OnRouteRequested().AddUObject(this, &AHansaRootHud::HandleMarketRouteRequested);
	CityOverviewPresentationModel->OnRelatedTargetRequested().AddUObject(this, &AHansaRootHud::HandleCityOverviewRelatedTarget);
	FString BuildInitializationError;
	const bool bBuildInitialized = SimulationHost != nullptr
		? BuildMenuPresentationModel->InitializeForLubeck(GetWorld(), SimulationHost, BuildInitializationError)
		: BuildMenuPresentationModel->InitializeForLubeck(GetWorld(), BuildInitializationError);
	if (!bBuildInitialized)
	{
		UE_LOG(LogHansa, Error, TEXT("Hansa build menu initialization failed: %s"), *BuildInitializationError);
	}
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
	SAssignNew(RootHudWidget, Hansa::UI::SHansaRootHud)
		.Model(PresentationModel)
		.BuildModel(BuildMenuPresentationModel)
		.InspectorModel(InspectorPresentationModel)
		.CityOverviewModel(CityOverviewPresentationModel)
		.MarketTableModel(MarketTablePresentationModel)
		.TradeMapModel(TradeMapPresentationModel)
		.ResearchModel(ResearchPresentationModel)
		.ScenarioModel(ScenarioPresentationModel)
		.SaveLoadModel(SaveLoadPresentationModel)
		.InitialViewportSize(ViewportSize);
	ViewportContent = RootHudWidget;
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->AddViewportWidgetContent(ViewportContent.ToSharedRef(), 20);
	}
	FViewport::ViewportResizedEvent.AddUObject(this, &AHansaRootHud::HandleViewportResized);

	if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PlayerOwner))
	{
		Controller->OnWorldSelectionChanged.AddDynamic(this, &AHansaRootHud::HandleWorldSelectionChanged);
	}
}

void AHansaRootHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FViewport::ViewportResizedEvent.RemoveAll(this);
	if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PlayerOwner))
	{
		Controller->OnWorldSelectionChanged.RemoveDynamic(this, &AHansaRootHud::HandleWorldSelectionChanged);
	}
	if (InspectorPresentationModel != nullptr) InspectorPresentationModel->OnFrameRequested().RemoveAll(this);
	if (CityOverviewPresentationModel != nullptr) CityOverviewPresentationModel->OnRelatedTargetRequested().RemoveAll(this);
	if (MarketTablePresentationModel != nullptr) MarketTablePresentationModel->OnRouteRequested().RemoveAll(this);
	if (PresentationModel != nullptr && HudPresentationChangedHandle.IsValid())
	{
		PresentationModel->OnChanged().Remove(HudPresentationChangedHandle);
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
}

void AHansaRootHud::HandleSimulationAdvanced(const int64 SimulationTick)
{
	(void)SimulationTick;
	RefreshCityOverview();
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
}

void AHansaRootHud::RefreshCityOverview()
{
	if (CityOverviewPresentationModel == nullptr || SimulationHost == nullptr) return;
	const Hansa::Simulation::FHansaEconomicRegistry* Registry = SimulationHost->GetEconomicRegistry();
	const auto Projection = SimulationHost->BuildProjection();
	if (Registry == nullptr || !Projection)
	{
		CityOverviewPresentationModel->SetError(
			LOCTEXT("CityOverviewProjectionFailure", "The authoritative city projection could not be prepared."),
			LOCTEXT("CityOverviewProjectionRemedy", "Keep Lübeck selected and try again."));
		return;
	}
	CityOverviewPresentationModel->ApplyProjection(
		Projection.Value, *Registry, SimulationHost->GetCityId(), LOCTEXT("Lubeck", "Lübeck"), 0);
	if (PresentationModel != nullptr)
	{
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
		}
	}
}

void AHansaRootHud::RefreshScenario()
{
	if (ScenarioPresentationModel == nullptr || SimulationHost == nullptr) return;
	if (const Hansa::Simulation::FHansaScenarioProgress* Progress = SimulationHost->GetScenarioProgress())
	{
		ScenarioPresentationModel->ApplyProgress(*Progress);
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
		TradeMapPresentationModel->Open(TEXT("Market.Detail.Action.BeginRoute"), GoodStableId);
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

#undef LOCTEXT_NAMESPACE
