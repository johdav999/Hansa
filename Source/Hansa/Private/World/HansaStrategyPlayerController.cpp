#include "World/HansaStrategyPlayerController.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaLubeckPlacementGrid.h"

#include "HansaLog.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "Net/UnrealNetwork.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"

namespace
{
	template <typename TItem, typename TKey>
	void UpsertProjectionItems(TArray<TItem>& Target, const TArray<TItem>& Updates, TKey KeyOf)
	{
		for (const TItem& Update : Updates)
		{
			if (TItem* Existing = Target.FindByPredicate([&Update, &KeyOf](const TItem& Item)
				{ return KeyOf(Item) == KeyOf(Update); })) *Existing = Update;
			else Target.Add(Update);
		}
	}
	UInputModifierNegate* AddNegateModifier(UInputMappingContext& Context, FEnhancedActionKeyMapping& Mapping,
		const bool bX, const bool bY, const bool bZ = false)
	{
		UInputModifierNegate* Modifier = NewObject<UInputModifierNegate>(&Context);
		Modifier->bX = bX;
		Modifier->bY = bY;
		Modifier->bZ = bZ;
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}

	UInputModifierSwizzleAxis* AddYAxisModifier(UInputMappingContext& Context, FEnhancedActionKeyMapping& Mapping)
	{
		UInputModifierSwizzleAxis* Modifier = NewObject<UInputModifierSwizzleAxis>(&Context);
		Modifier->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}

	void AddScaleByDeltaTimeModifier(UInputMappingContext& Context, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(&Context));
	}
}

AHansaStrategyPlayerController::AHansaStrategyPlayerController()
{
	bReplicates = true;
	bShowMouseCursor = true;
	bEnableMouseOverEvents = true;
	bEnableClickEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AHansaStrategyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureStrategyInputObjects();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->AddMappingContext(StrategyMappingContext, 0);
		}
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
    UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Diagnostics v3 (yaw+pitch) initialized controller=%s executable=%s"), *GetName(), FPlatformProcess::ExecutableName());
}

void AHansaStrategyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AHansaBuildingPlacementGhost* Ghost = PlacementGhost.Get()) Ghost->Destroy();
	PlacementGhost.Reset();
	if (StrategyMappingContext != nullptr)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				Subsystem->RemoveMappingContext(StrategyMappingContext);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AHansaStrategyPlayerController::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHansaStrategyPlayerController, ClientProjection, COND_OwnerOnly);
}

void AHansaStrategyPlayerController::ServerSubmitHansaIntent_Implementation(
	const FHansaClientCommandIntent& Intent)
{
	if (AHansaGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AHansaGameMode>() : nullptr)
	{
		Mode->SubmitMultiplayerIntent(*this, Intent);
	}
}

void AHansaStrategyPlayerController::ServerSetHansaInterest_Implementation(
	const FHansaClientInterest& Interest)
{
	if (AHansaGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AHansaGameMode>() : nullptr)
	{
		Mode->UpdateMultiplayerInterest(*this, Interest);
	}
}

void AHansaStrategyPlayerController::ServerRequestHansaProjectionRefresh_Implementation(
	const int64 ClientKnownRevision)
{
	if (AHansaGameMode* Mode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AHansaGameMode>() : nullptr)
	{
		Mode->RequestMultiplayerProjectionRefresh(*this, ClientKnownRevision);
	}
}

void AHansaStrategyPlayerController::ClientReceiveHansaCommandFeedback_Implementation(
	const FHansaClientCommandFeedback& Feedback)
{
	LastCommandFeedback = Feedback;
	UE_LOG(LogHansa, Display,
		TEXT("S11-P04 client feedback sequence=%lld accepted=%s rejection=%d serverOrder=%lld"),
		static_cast<long long>(Feedback.ClientSequence),
		Feedback.bAccepted ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Feedback.Rejection),
		static_cast<long long>(Feedback.AcceptedGlobalSequence));
}

void AHansaStrategyPlayerController::SetServerAuthorityIdentity(
	const uint64 PrincipalId, const int64 HouseId)
{
	if (!HasAuthority()) return;
	AuthorityPrincipalId = PrincipalId;
	AuthorityHouseId = HouseId;
}

void AHansaStrategyPlayerController::PublishServerProjection(
	const FHansaClientProjectionSnapshot& Projection)
{
	if (!HasAuthority()) return;
	ApplyClientProjectionUpdate(Projection);
	ClientProjection = Projection;
	ForceNetUpdate();
}

void AHansaStrategyPlayerController::PublishCommandFeedback(
	const FHansaClientCommandFeedback& Feedback)
{
	if (!HasAuthority()) return;
	ClientReceiveHansaCommandFeedback(Feedback);
}

bool AHansaStrategyPlayerController::SubmitLocalHansaIntent(FHansaClientCommandIntent Intent)
{
	if (!IsLocalController()) return false;
	Intent.SchemaVersion = FHansaClientCommandIntent::CurrentSchemaVersion;
	Intent.ClientSequence = NextClientCommandSequence++;
	Intent.ClientNonce = NextClientCommandNonce++;
	Intent.ExpectedServerTick = ClientProjection.ServerTick;
	LastCommandFeedback = {};
	LastCommandFeedback.State = EHansaClientCommandState::Pending;
	LastCommandFeedback.ClientSequence = Intent.ClientSequence;
	LastCommandFeedback.ClientNonce = Intent.ClientNonce;
	LastCommandFeedback.Message = TEXT("Waiting for the authoritative server.");
	LastCommandFeedback.Remedy = TEXT("Keep the selected object open until the server responds.");
	ServerSubmitHansaIntent(Intent);
	return true;
}

void AHansaStrategyPlayerController::OnRep_HansaClientProjection()
{
	ApplyClientProjectionUpdate(ClientProjection);
	UE_LOG(LogHansa, Display,
		TEXT("S11-P04 client projection revision=%lld full=%s tick=%lld authoritativeHash=%s projectionDigest=%s"),
		static_cast<long long>(ClientProjection.Revision),
		ClientProjection.bFullRefresh ? TEXT("true") : TEXT("false"),
		static_cast<long long>(ClientProjection.ServerTick),
		*ClientProjection.AuthoritativeHash,
		*ClientProjection.ProjectionDigest);
}

void AHansaStrategyPlayerController::ApplyClientProjectionUpdate(
	const FHansaClientProjectionSnapshot& Update)
{
	if (Update.bFullRefresh || ClientProjectionCache.Revision <= 0)
	{
		ClientProjectionCache = Update;
		return;
	}
	FHansaClientProjectionSnapshot Next = Update;
	Next.Placements = ClientProjectionCache.Placements;
	Next.Markets = ClientProjectionCache.Markets;
	Next.Routes = ClientProjectionCache.Routes;
	Next.Inventories = ClientProjectionCache.Inventories;
	Next.Productions = ClientProjectionCache.Productions;
	Next.PopulationCohorts = ClientProjectionCache.PopulationCohorts;
	Next.CitySummaries = ClientProjectionCache.CitySummaries;
	Next.Vehicles = ClientProjectionCache.Vehicles;
	Next.LogisticsJobs = ClientProjectionCache.LogisticsJobs;
	Next.AuthorizedResearchReports = ClientProjectionCache.AuthorizedResearchReports;
	Next.Events = ClientProjectionCache.Events;
	for (const FHansaProjectionRemoval& Removal : Update.Removed)
	{
		const int64 Id = FCString::Atoi64(*Removal.StableId);
		if (Removal.Collection == TEXT("placements")) Next.Placements.RemoveAll([Id](const auto& V) { return V.BuildingId == Id; });
		else if (Removal.Collection == TEXT("markets")) Next.Markets.RemoveAll([&Removal](const auto& V) { return V.CityId + TEXT("|") + V.GoodId == Removal.StableId; });
		else if (Removal.Collection == TEXT("routes")) Next.Routes.RemoveAll([Id](const auto& V) { return V.RouteId == Id; });
		else if (Removal.Collection == TEXT("inventories")) Next.Inventories.RemoveAll([Id](const auto& V) { return V.InventoryId == Id; });
		else if (Removal.Collection == TEXT("productions")) Next.Productions.RemoveAll([Id](const auto& V) { return V.ProductionId == Id; });
		else if (Removal.Collection == TEXT("population")) Next.PopulationCohorts.RemoveAll([Id](const auto& V) { return V.CohortId == Id; });
		else if (Removal.Collection == TEXT("cities")) Next.CitySummaries.RemoveAll([&Removal](const auto& V) { return V.CityId == Removal.StableId; });
		else if (Removal.Collection == TEXT("vehicles")) Next.Vehicles.RemoveAll([Id](const auto& V) { return V.VehicleId == Id; });
		else if (Removal.Collection == TEXT("logistics")) Next.LogisticsJobs.RemoveAll([Id](const auto& V) { return V.JobId == Id; });
		else if (Removal.Collection == TEXT("reports")) Next.AuthorizedResearchReports.RemoveAll([Id](const auto& V) { return V.HouseId == Id; });
	}
	UpsertProjectionItems(Next.Placements, Update.Placements, [](const auto& V) { return V.BuildingId; });
	UpsertProjectionItems(Next.Markets, Update.Markets, [](const auto& V) { return V.CityId + TEXT("|") + V.GoodId; });
	UpsertProjectionItems(Next.Routes, Update.Routes, [](const auto& V) { return V.RouteId; });
	UpsertProjectionItems(Next.Inventories, Update.Inventories, [](const auto& V) { return V.InventoryId; });
	UpsertProjectionItems(Next.Productions, Update.Productions, [](const auto& V) { return V.ProductionId; });
	UpsertProjectionItems(Next.PopulationCohorts, Update.PopulationCohorts, [](const auto& V) { return V.CohortId; });
	UpsertProjectionItems(Next.CitySummaries, Update.CitySummaries, [](const auto& V) { return V.CityId; });
	UpsertProjectionItems(Next.Vehicles, Update.Vehicles, [](const auto& V) { return V.VehicleId; });
	UpsertProjectionItems(Next.LogisticsJobs, Update.LogisticsJobs, [](const auto& V) { return V.JobId; });
	UpsertProjectionItems(Next.AuthorizedResearchReports, Update.AuthorizedResearchReports, [](const auto& V) { return V.HouseId; });
	Next.Events.Append(Update.Events);
	if (Next.Events.Num() > 256) Next.Events.RemoveAt(0, Next.Events.Num() - 256);
	ClientProjectionCache = MoveTemp(Next);
}
void AHansaStrategyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	EnsureStrategyInputObjects();

	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (Enhanced == nullptr)
	{
		UE_LOG(LogHansa, Error, TEXT("Hansa strategy controller requires EnhancedPlayerInput and EnhancedInputComponent."));
		return;
	}

	Enhanced->BindAction(PanAction, ETriggerEvent::Triggered, this, &AHansaStrategyPlayerController::HandlePan);
	Enhanced->BindAction(PanAction, ETriggerEvent::Completed, this, &AHansaStrategyPlayerController::HandlePanCompleted);
	Enhanced->BindAction(PanAction, ETriggerEvent::Canceled, this, &AHansaStrategyPlayerController::HandlePanCompleted);
	Enhanced->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHansaStrategyPlayerController::HandleZoom);
	Enhanced->BindAction(RotateAction, ETriggerEvent::Triggered, this, &AHansaStrategyPlayerController::HandleRotate);
	Enhanced->BindAction(RotateAction, ETriggerEvent::Completed, this, &AHansaStrategyPlayerController::HandleRotateCompleted);
	Enhanced->BindAction(RotateAction, ETriggerEvent::Canceled, this, &AHansaStrategyPlayerController::HandleRotateCompleted);
	Enhanced->BindAction(FastPanAction, ETriggerEvent::Triggered, this, &AHansaStrategyPlayerController::HandleFastPan);
	Enhanced->BindAction(FastPanAction, ETriggerEvent::Completed, this, &AHansaStrategyPlayerController::HandleFastPanCompleted);
	Enhanced->BindAction(FastPanAction, ETriggerEvent::Canceled, this, &AHansaStrategyPlayerController::HandleFastPanCompleted);
	Enhanced->BindAction(SelectAction, ETriggerEvent::Started, this, &AHansaStrategyPlayerController::HandleSelect);
	Enhanced->BindAction(SelectAction, ETriggerEvent::Triggered, this, &AHansaStrategyPlayerController::HandleSelectHeld);
	Enhanced->BindAction(SelectAction, ETriggerEvent::Completed, this, &AHansaStrategyPlayerController::HandleSelectReleased);
	Enhanced->BindAction(SelectAction, ETriggerEvent::Canceled, this, &AHansaStrategyPlayerController::HandleSelectReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AHansaStrategyPlayerController::HandleCameraDragPressed);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AHansaStrategyPlayerController::HandleRightMouseReleased);
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AHansaStrategyPlayerController::HandleShipMoveIntent);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHansaStrategyPlayerController::HandleEscapeIntent);
    InputComponent->BindKey(EKeys::Gamepad_Special_Right,IE_Pressed,this,&AHansaStrategyPlayerController::HandleSessionMenu);
    InputComponent->BindKey(EKeys::F1,IE_Pressed,this,&AHansaStrategyPlayerController::HandleContextHelp);
    InputComponent->BindKey(EKeys::Gamepad_Special_Left,IE_Pressed,this,&AHansaStrategyPlayerController::HandleContextHelp);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AHansaStrategyPlayerController::HandleEscapeIntent);
}

bool AHansaStrategyPlayerController::TraceWorldSelection(FHitResult& OutHit) const
{
	OutHit = FHitResult();
	if (!IsLocalController() || GetWorld() == nullptr)
	{
		return false;
	}

	if (GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility), true, OutHit) && OutHit.GetActor() != nullptr)
	{
		return true;
	}

	int32 Width = 0;
	int32 Height = 0;
	GetViewportSize(Width, Height);
	FVector Origin;
	FVector Direction;
	if (Width <= 0 || Height <= 0 ||
		!DeprojectScreenPositionToWorld(Width * 0.5f, Height * 0.5f, Origin, Direction))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HansaWorldSelection), true, GetPawn());
	return GetWorld()->LineTraceSingleByChannel(
		OutHit, Origin, Origin + Direction * SelectionTraceDistance, ECC_Visibility, QueryParams) &&
		OutHit.GetActor() != nullptr;
}

void AHansaStrategyPlayerController::PerformWorldSelection()
{
    if (!IsPointerOverWorldViewport()) return;
	if (auto* Build = GetBuildMenuModel(); Build && Build->GetSnapshot().bDemolitionMode)
	{
		if (!IsPointerOverWorldViewport()) return;
		FHitResult DemolitionHit;
		TraceWorldSelection(DemolitionHit);
		const auto* Building = Cast<AHansaBuildingWorldProjectionActor>(DemolitionHit.GetActor());
		if (Build->DemolishBuildingIntent(Building ? static_cast<int64>(Building->GetBuildingId().GetValue()) : 0))
		{
			SelectedWorldActor.Reset();
			UpdateProjectionSelection(nullptr);
			OnWorldSelectionChanged.Broadcast(nullptr, FHitResult());
		}
		return;
	}
	FHitResult Hit;
	if (TraceWorldSelection(Hit))
	{
		SelectedWorldActor = Hit.GetActor();
		UpdateProjectionSelection(Hit.GetActor());
		OnWorldSelectionChanged.Broadcast(Hit.GetActor(), Hit);
		return;
	}

	SelectedWorldActor.Reset();
	UpdateProjectionSelection(nullptr);
	OnWorldSelectionChanged.Broadcast(nullptr, Hit);
}

void AHansaStrategyPlayerController::UpdateProjectionSelection(AActor* SelectedActor)
{
	AHansaBuildingWorldProjectionActor* ProjectedBuilding = Cast<AHansaBuildingWorldProjectionActor>(SelectedActor);
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHansaPlacementProjectionManager> It(World); It; ++It)
		{
			if (ProjectedBuilding != nullptr)
			{
				It->SelectBuilding(ProjectedBuilding->GetBuildingId());
			}
			else
			{
				It->ClearSelection();
			}
		}
	}
}

void AHansaStrategyPlayerController::EnsureStrategyInputObjects()
{
	const bool bCompleteInputSet = StrategyMappingContext != nullptr && PanAction != nullptr &&
		ZoomAction != nullptr && RotateAction != nullptr && FastPanAction != nullptr && SelectAction != nullptr;
	if (bCompleteInputSet)
	{
		return;
	}

	if (StrategyMappingContext != nullptr && !bOwnsRuntimeMappingContext)
	{
		UE_LOG(LogHansa, Warning,
			TEXT("Ignoring partial authored strategy input set; assign the mapping context and all five actions together."));
		StrategyMappingContext = nullptr;
	}

	if (PanAction == nullptr)
	{
		PanAction = NewObject<UInputAction>(this, TEXT("IA_StrategyPan"));
		PanAction->ValueType = EInputActionValueType::Axis2D;
	}
	if (ZoomAction == nullptr)
	{
		ZoomAction = NewObject<UInputAction>(this, TEXT("IA_StrategyZoom"));
		ZoomAction->ValueType = EInputActionValueType::Axis1D;
	}
	if (RotateAction == nullptr)
	{
		RotateAction = NewObject<UInputAction>(this, TEXT("IA_StrategyRotate"));
		RotateAction->ValueType = EInputActionValueType::Axis1D;
	}
	if (FastPanAction == nullptr)
	{
		FastPanAction = NewObject<UInputAction>(this, TEXT("IA_StrategyFastPan"));
		FastPanAction->ValueType = EInputActionValueType::Boolean;
	}
	if (SelectAction == nullptr)
	{
		SelectAction = NewObject<UInputAction>(this, TEXT("IA_WorldSelect"));
		SelectAction->ValueType = EInputActionValueType::Boolean;
	}

	if (StrategyMappingContext == nullptr)
	{
		StrategyMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_StrategyCamera"));
		bOwnsRuntimeMappingContext = true;
		AddDefaultMappings();
	}
}

void AHansaStrategyPlayerController::AddDefaultMappings()
{
	if (!bOwnsRuntimeMappingContext || StrategyMappingContext == nullptr)
	{
		return;
	}

	StrategyMappingContext->MapKey(PanAction, EKeys::D);
	FEnhancedActionKeyMapping& PanLeft = StrategyMappingContext->MapKey(PanAction, EKeys::A);
	AddNegateModifier(*StrategyMappingContext, PanLeft, true, false);
	FEnhancedActionKeyMapping& PanForward = StrategyMappingContext->MapKey(PanAction, EKeys::W);
	AddYAxisModifier(*StrategyMappingContext, PanForward);
	FEnhancedActionKeyMapping& PanBackward = StrategyMappingContext->MapKey(PanAction, EKeys::S);
	AddYAxisModifier(*StrategyMappingContext, PanBackward);
	AddNegateModifier(*StrategyMappingContext, PanBackward, false, true);
	StrategyMappingContext->MapKey(PanAction, EKeys::Right);
	FEnhancedActionKeyMapping& ArrowLeft = StrategyMappingContext->MapKey(PanAction, EKeys::Left);
	AddNegateModifier(*StrategyMappingContext, ArrowLeft, true, false);
	FEnhancedActionKeyMapping& ArrowUp = StrategyMappingContext->MapKey(PanAction, EKeys::Up);
	AddYAxisModifier(*StrategyMappingContext, ArrowUp);
	FEnhancedActionKeyMapping& ArrowDown = StrategyMappingContext->MapKey(PanAction, EKeys::Down);
	AddYAxisModifier(*StrategyMappingContext, ArrowDown);
	AddNegateModifier(*StrategyMappingContext, ArrowDown, false, true);
	StrategyMappingContext->MapKey(PanAction, EKeys::Gamepad_Left2D);

	StrategyMappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	FEnhancedActionKeyMapping& ZoomInController = StrategyMappingContext->MapKey(ZoomAction, EKeys::Gamepad_RightTriggerAxis);
	AddScaleByDeltaTimeModifier(*StrategyMappingContext, ZoomInController);
	FEnhancedActionKeyMapping& ZoomOutController = StrategyMappingContext->MapKey(ZoomAction, EKeys::Gamepad_LeftTriggerAxis);
	AddNegateModifier(*StrategyMappingContext, ZoomOutController, true, false);
	AddScaleByDeltaTimeModifier(*StrategyMappingContext, ZoomOutController);

	StrategyMappingContext->MapKey(RotateAction, EKeys::E);
	FEnhancedActionKeyMapping& RotateLeft = StrategyMappingContext->MapKey(RotateAction, EKeys::Q);
	AddNegateModifier(*StrategyMappingContext, RotateLeft, true, false);
	StrategyMappingContext->MapKey(RotateAction, EKeys::Gamepad_RightX);

	StrategyMappingContext->MapKey(FastPanAction, EKeys::LeftShift);
	StrategyMappingContext->MapKey(FastPanAction, EKeys::Gamepad_RightShoulder);
	StrategyMappingContext->MapKey(SelectAction, EKeys::LeftMouseButton);
	StrategyMappingContext->MapKey(SelectAction, EKeys::SpaceBar);
	StrategyMappingContext->MapKey(SelectAction, EKeys::Gamepad_FaceButton_Bottom);
}

AHansaStrategyCameraPawn* AHansaStrategyPlayerController::GetStrategyCameraPawn() const
{
	return Cast<AHansaStrategyCameraPawn>(GetPawn());
}

void AHansaStrategyPlayerController::HandlePan(const FInputActionValue& Value)
{
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetPanIntent(Value.Get<FVector2D>());
	}
}

void AHansaStrategyPlayerController::HandlePanCompleted(const FInputActionValue& Value)
{
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetPanIntent(FVector2D::ZeroVector);
	}
}

void AHansaStrategyPlayerController::HandleZoom(const FInputActionValue& Value)
{
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->AddZoomIntent(Value.Get<float>());
	}
}

void AHansaStrategyPlayerController::HandleRotate(const FInputActionValue& Value)
{
	if (UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
		BuildModel != nullptr && !BuildModel->GetSnapshot().SelectedBuildingId.IsNone())
	{
		if (!bPlacementRotateHeld && FMath::Abs(Value.Get<float>()) > 0.25f)
		{
			bPlacementRotateHeld = true;
			BuildModel->RotateIntent();
			SyncPlacementGhostAndCursor();
		}
		return;
	}
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetRotateIntent(Value.Get<float>());
	}
}

void AHansaStrategyPlayerController::HandleRotateCompleted(const FInputActionValue& Value)
{
	bPlacementRotateHeld = false;
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetRotateIntent(0.0f);
	}
}

void AHansaStrategyPlayerController::HandleFastPan(const FInputActionValue& Value)
{
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetFastPanIntent(Value.Get<bool>());
	}
}

void AHansaStrategyPlayerController::HandleFastPanCompleted(const FInputActionValue& Value)
{
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetFastPanIntent(false);
	}
}

void AHansaStrategyPlayerController::HandleSelect(const FInputActionValue& Value)
{
    if(const auto* Hud=Cast<AHansaRootHud>(GetHUD());Hud&&((Hud->GetScenarioPresentationModel()&&Hud->GetScenarioPresentationModel()->GetSnapshot().bOpen)||(Hud->GetSaveLoadPresentationModel()&&Hud->GetSaveLoadPresentationModel()->GetSnapshot().bOpen)))return;
	(void)Value;
	if (UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
		BuildModel != nullptr && !BuildModel->GetSnapshot().SelectedBuildingId.IsNone())
	{
		float MouseX = 0.0f, MouseY = 0.0f;
		int32 Width = 0, Height = 0;
		GetViewportSize(Width, Height);
		// Use the position recorded with this click for construction.
        const FVector2D Pointer = GetMousePosition(MouseX, MouseY)
            ? FVector2D(MouseX, MouseY) : ResolveCurrentPointer(FVector2D(Width * 0.5, Height * 0.5));
		if (BuildModel->GetSnapshot().SelectedBuildingId == TEXT("Building.Road"))
		{
			bRoadPointerHeld = BeginRoadDrawing(Pointer, false);
			return;
		}
		FIntPoint Cell;
		FVector WorldLocation;
		if (ResolvePlacementCellAtScreenPosition(Pointer, Cell, WorldLocation))
		{
			BuildModel->TargetGridCell(Cell.X, Cell.Y);
			if (IsPointerOverWorldViewport()) BuildModel->BeginBuildingStroke(Cell.X,Cell.Y);
			SyncPlacementGhostAndCursor();
			return;
		}
	}
	PerformWorldSelection();
}

void AHansaStrategyPlayerController::HandleSelectHeld(const FInputActionValue& Value)
{
	(void)Value;
	if (!bRoadPointerHeld) return;
	float MouseX = 0.0f, MouseY = 0.0f;
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const FVector2D Pointer = ResolveCurrentPointer(FVector2D(Width * 0.5, Height * 0.5));
	UpdateRoadDrawing(Pointer, false);
}

void AHansaStrategyPlayerController::HandleSelectReleased(const FInputActionValue& Value)
{
	if(auto* Model=GetBuildMenuModel())Model->EndBuildingStroke();
	(void)Value;
	if (!bRoadPointerHeld) return;
	float MouseX = 0.0f, MouseY = 0.0f;
	int32 Width = 0, Height = 0;
	GetViewportSize(Width, Height);
	const FVector2D Pointer = ResolveCurrentPointer(FVector2D(Width * 0.5, Height * 0.5));
	FIntPoint Cell;
	FVector WorldLocation;
	const bool bOverWorld = ResolvePlacementCellAtScreenPosition(Pointer, Cell, WorldLocation);
	EndRoadDrawing(Pointer, bOverWorld, false);
	bRoadPointerHeld = false;
}

void AHansaStrategyPlayerController::HandleCameraDragPressed()
{
    UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Press controller=%s cachedCtrl=%d cachedRMB=%d"),
        *GetName(), IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl), IsInputKeyDown(EKeys::RightMouseButton));
    HandleCameraDragReleased();
    bool CancelledBuild=false;
    // Preserve construction cancellation without opening the session menu.
    if (const auto* Build = GetBuildMenuModel(); Build && (Build->GetSnapshot().bDemolitionMode || !Build->GetSnapshot().SelectedBuildingId.IsNone()))
    {
        CancelBuildingPlacement();CancelledBuild=true;
    }
    const bool bLocal = IsLocalController();
    const bool bOverWorld = IsPointerOverWorldViewport();
    const bool bPointerValid = TryGetPlacementPointer(PreviousCameraDragPointer);
    if (!bLocal || !bOverWorld || !bPointerValid)
    {
        UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Start rejected local=%d overWorld=%d pointerValid=%d"), bLocal, bOverWorld, bPointerValid);
        return;
    }
    if (auto* CameraPawn = GetStrategyCameraPawn())
    {
        bCameraDragHeld = true;
        CameraPressPointer=PreviousCameraDragPointer;
        bShipClickCandidate=!CancelledBuild && !IsInputKeyDown(EKeys::LeftControl) && !IsInputKeyDown(EKeys::RightControl);
        CameraPawn->SetDragPanIntent(FVector2D::ZeroVector, true);
        UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Started pointer=%s pawn=%s viewTarget=%s yaw=%.3f"),
            *PreviousCameraDragPointer.ToString(), *CameraPawn->GetName(), *GetNameSafe(GetViewTarget()), CameraPawn->GetCameraYawDegrees());
    }
    else
    {
        UE_LOG(LogHansa, Warning, TEXT("[CameraDrag] Start rejected: no strategy camera pawn; possessed=%s viewTarget=%s"),
            *GetNameSafe(GetPawn()), *GetNameSafe(GetViewTarget()));
    }
}

void AHansaStrategyPlayerController::HandleCameraDragReleased()
{
    if (bCameraDragHeld)
    {
        UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Released/reset controller=%s"), *GetName());
    }
    bCameraDragHeld = false;
    bShipClickCandidate = false;
    if (auto* CameraPawn = GetStrategyCameraPawn())
    {
        CameraPawn->SetDragPanIntent(FVector2D::ZeroVector, false);
        CameraPawn->SetDragOrbitIntent(FVector2D::ZeroVector, false);
    }
}

void AHansaStrategyPlayerController::HandleRightMouseReleased()
{
    const bool Click=bCameraDragHeld && bShipClickCandidate && IsPointerOverWorldViewport();
    HandleCameraDragReleased();
    if (!Click) return;
    FVector2D Pointer;
    if (!TryGetPlacementPointer(Pointer) || FVector2D::Distance(Pointer,CameraPressPointer)>6.0) return;
    HandleShipMoveIntent();
}

void AHansaStrategyPlayerController::HandleShipMoveIntent()
{
    if (!IsPointerOverWorldViewport()) return;
    FVector2D Pointer;
    if (!TryGetPlacementPointer(Pointer)) return;
    FVector Origin,Direction;
    if (!DeprojectScreenPositionToWorld(Pointer.X,Pointer.Y,Origin,Direction) || Direction.Z>=-UE_SMALL_NUMBER) return;
    for (TActorIterator<AHansaCargoProjectionManager> It(GetWorld());It;++It)
    {
        auto* Ship=It->FindActor(It->GetSelectedCargo());
        if (!Ship || !Ship->bSeaVehicle) continue;
        const double T=(Ship->GetActorLocation().Z-Origin.Z)/Direction.Z;
        if (T<=0) return;
        const auto Cell=Hansa::Game::LubeckPlacementGrid::WorldToGrid(Origin+Direction*T);
		if (GetNetMode() != NM_Standalone)
		{
			FHansaClientCommandIntent Intent;
			Intent.Type = EHansaClientIntentType::MoveShip;
			Intent.VehicleId = static_cast<int64>(Ship->GetVehicleId().GetValue());
			Intent.TargetX = Cell.X;
			Intent.TargetY = Cell.Y;
			SubmitLocalHansaIntent(Intent);
		}
		else
		{
			It->MoveSelectedShip(Cell);
		}
        if (auto* Hud=Cast<AHansaRootHud>(GetHUD())) Hud->InspectCargo(It->GetSelectedCargo());
        return;
    }
}

void AHansaStrategyPlayerController::UpdateCameraDrag()
{
    const bool bSlateReady = FSlateApplication::IsInitialized();
    const bool bSlateRMB = bSlateReady && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::RightMouseButton);
    const bool bCachedRMB = IsInputKeyDown(EKeys::RightMouseButton);
    const double Now = FPlatformTime::Seconds();
    const bool bLogSample = (bCameraDragHeld || bSlateRMB || bCachedRMB) && Now - LastCameraDragDiagnosticTime >= 0.5;
    if (bLogSample)
    {
        LastCameraDragDiagnosticTime = Now;
        UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Input held=%d slateRMB=%d cachedRMB=%d slateCtrl=%d cachedLeftCtrl=%d cachedRightCtrl=%d active=%d"),
            bCameraDragHeld, bSlateRMB, bCachedRMB,
            bSlateReady && FSlateApplication::Get().GetModifierKeys().IsControlDown(),
            IsInputKeyDown(EKeys::LeftControl), IsInputKeyDown(EKeys::RightControl),
            bSlateReady && FSlateApplication::Get().IsActive());
    }
    if (!bCameraDragHeld) return;
    FVector2D Pointer = FVector2D::ZeroVector;
    const bool bActive = bSlateReady && FSlateApplication::Get().IsActive();
    const bool bOverWorld = IsPointerOverWorldViewport();
    const bool bPointerValid = TryGetPlacementPointer(Pointer);
    // Once the world starts the captured gesture, Slate may report a HUD child as
    // the deepest hovered widget. Pointer validity is the geometry-based viewport
    // boundary; bOverWorld must not cancel a drag that is still inside it.
    if (!Hansa::Game::FHansaStrategyCameraModel::ShouldContinuePointerDrag(
        bCachedRMB, bSlateReady, bActive, bPointerValid))
    {
        UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Cancel cachedRMB=%d slateReady=%d active=%d overWorld=%d pointerValid=%d"),
            bCachedRMB, bSlateReady, bActive, bOverWorld, bPointerValid);
        HandleCameraDragReleased();
        return;
    }
    if (auto* CameraPawn = GetStrategyCameraPawn())
    {
        // Slate owns the cursor and modifier state, including Ctrl held before viewport focus.
        const bool bRotate = FSlateApplication::Get().GetModifierKeys().IsControlDown();
        const FVector2D Delta = Pointer - PreviousCameraDragPointer;
        if (bRotate || FVector2D::Distance(Pointer,CameraPressPointer)>6.0) bShipClickCandidate=false;
        CameraPawn->SetDragPanIntent(bRotate ? FVector2D::ZeroVector : Delta, !bRotate);
        // Camera yaw is driven only by horizontal pointer travel. Including Y here
        // made ordinary diagonal drags cancel or reverse their horizontal yaw.
        const float YawPixels = bRotate
            ? Hansa::Game::FHansaStrategyCameraModel::YawPointerDisplacement(Delta)
            : 0.0f;
        CameraPawn->SetDragOrbitIntent(bRotate ? FVector2D(YawPixels, Delta.Y) : FVector2D::ZeroVector, bRotate);
        if (bLogSample)
        {
            UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Move pointer=%s delta=%s rotate=%d yawPixels=%.3f sensitivity=%.3f pawn=%s viewTarget=%s yaw=%.3f"),
                *Pointer.ToString(), *Delta.ToString(), bRotate, YawPixels,
                CameraPawn->DragRotationDegreesPerPixel, *CameraPawn->GetName(), *GetNameSafe(GetViewTarget()), CameraPawn->GetCameraYawDegrees());
        }
    }
    else if (bLogSample)
    {
        UE_LOG(LogHansa, Warning, TEXT("[CameraDrag] Move has no strategy camera pawn"));
    }
    PreviousCameraDragPointer = Pointer;
}
void AHansaStrategyPlayerController::HandleEscapeIntent()
{
    if(const auto* Build=GetBuildMenuModel();Build&&(Build->GetSnapshot().bDemolitionMode || !Build->GetSnapshot().SelectedBuildingId.IsNone())){bRoadPointerHeld=false;CancelBuildingPlacement();return;}
    if(auto* Hud=Cast<AHansaRootHud>(GetHUD());Hud&&Hud->GetRootWidget())Hud->GetRootWidget()->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
}
void AHansaStrategyPlayerController::HandleSessionMenu(){if(auto* Hud=Cast<AHansaRootHud>(GetHUD());Hud&&Hud->GetRootWidget())Hud->GetRootWidget()->ActivateSemanticId(TEXT("HUD.TopStatus.Session"));}
void AHansaStrategyPlayerController::HandleContextHelp(){if(auto* Hud=Cast<AHansaRootHud>(GetHUD());Hud&&Hud->GetRootWidget())Hud->GetRootWidget()->ActivateSemanticId(TEXT("Session.Help.Dismiss"));}

UHansaBuildMenuPresentationModel* AHansaStrategyPlayerController::GetBuildMenuModel() const
{
	const AHansaRootHud* RootHud = Cast<AHansaRootHud>(GetHUD());
	return RootHud != nullptr ? RootHud->GetBuildMenuPresentationModel() : nullptr;
}

AHansaLubeckWorldFoundation* AHansaStrategyPlayerController::FindPlacementFoundation() const
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It) return *It;
	}
	return nullptr;
}

FVector2D AHansaStrategyPlayerController::ResolveCurrentPointer(const FVector2D Fallback) const
{
	FVector2D Pointer;
	return TryGetPlacementPointer(Pointer) ? Pointer : Fallback;
}

bool AHansaStrategyPlayerController::ResolvePlacementCellAtScreenPosition(
	const FVector2D ScreenPosition, FIntPoint& OutCell, FVector& OutWorldLocation) const
{
	AHansaLubeckWorldFoundation* Foundation = FindPlacementFoundation();
	if (Foundation == nullptr || GetWorld() == nullptr) return false;
	FVector Candidate = FVector::ZeroVector;
	FHitResult Hit;
	FVector RayOrigin, RayDirection;
    const bool bRay = DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection);
    if ((bRay && Hansa::Game::TerrainPlacement::Trace(GetWorld(), RayOrigin,
        RayOrigin + RayDirection * SelectionTraceDistance, Hit)) ||
        (GetHitResultAtScreenPosition(ScreenPosition,
        UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit) && Hit.bBlockingHit))
	{
		Candidate = Hit.ImpactPoint;
	}
	else
	{
		FVector Origin, Direction;
		if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction) ||
			FMath::IsNearlyZero(Direction.Z)) return false;
		const double SurfaceZ = Foundation->PlacementCellToWorld(0, 0).Z;
		const double Distance = (SurfaceZ - Origin.Z) / Direction.Z;
		if (Distance <= 0.0 || Distance > SelectionTraceDistance) return false;
		Candidate = Origin + Direction * Distance;
	}
	int32 X = 0, Y = 0;
	if (!Foundation->WorldToPlacementCell(Candidate, X, Y)) return false;
	OutCell = FIntPoint(X, Y);
	OutWorldLocation = Foundation->PlacementCellToWorld(X, Y, 106.0f);
	return true;
}

bool AHansaStrategyPlayerController::BeginBuildingPlacementDrag(
	const FName BuildingDefinitionId, const FVector2D ScreenPosition, const bool bPointerOverMenu, const bool bUseLivePointer)
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	if (BuildModel == nullptr || !BuildModel->BeginCardDrag(BuildingDefinitionId)) return false;
	UpdateBuildingPlacementDrag(ScreenPosition, bPointerOverMenu, bUseLivePointer);
	return true;
}

bool AHansaStrategyPlayerController::UpdateBuildingPlacementDrag(
	const FVector2D ScreenPosition, const bool bPointerOverMenu, const bool bUseLivePointer)
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	if (BuildModel == nullptr || !BuildModel->GetSnapshot().bDraggingCard) return false;
	FIntPoint Cell = FIntPoint::ZeroValue;
	FVector WorldLocation;
	const bool bOverWorld = !bPointerOverMenu && ResolvePlacementCellAtScreenPosition(
		bUseLivePointer ? ResolveCurrentPointer(ScreenPosition) : ScreenPosition, Cell, WorldLocation);
	if (bOverWorld) BuildModel->UpdateCardDragTarget(Cell.X, Cell.Y);
	else BuildModel->ClearCardDragTarget();
	SyncPlacementGhostAndCursor();
	return bOverWorld;
}

bool AHansaStrategyPlayerController::EndBuildingPlacementDrag(
	const FVector2D ScreenPosition, const bool bPointerOverMenu, const bool bUseLivePointer)
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	if (BuildModel == nullptr || !BuildModel->GetSnapshot().bDraggingCard) return false;
	const bool bOverWorld = UpdateBuildingPlacementDrag(ScreenPosition, bPointerOverMenu, bUseLivePointer);
	const bool bCommitted = BuildModel->EndCardDrag(bOverWorld);
	SyncPlacementGhostAndCursor();
	return bCommitted;
}

bool AHansaStrategyPlayerController::BeginRoadDrawing(
	const FVector2D ScreenPosition, const bool bUseLivePointer)
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	FIntPoint Cell;
	FVector WorldLocation;
	if (BuildModel == nullptr || !ResolvePlacementCellAtScreenPosition(
		bUseLivePointer ? ResolveCurrentPointer(ScreenPosition) : ScreenPosition, Cell, WorldLocation))
	{
		return false;
	}
	const bool bStarted = BuildModel->BeginRoadDraw(Cell.X, Cell.Y);
	SyncPlacementGhostAndCursor();
	return bStarted;
}

bool AHansaStrategyPlayerController::UpdateRoadDrawing(
	const FVector2D ScreenPosition, const bool bUseLivePointer)
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	FIntPoint Cell;
	FVector WorldLocation;
	if (BuildModel == nullptr || !BuildModel->GetSnapshot().bRoadDrawing ||
		!ResolvePlacementCellAtScreenPosition(
			bUseLivePointer ? ResolveCurrentPointer(ScreenPosition) : ScreenPosition, Cell, WorldLocation))
	{
		return false;
	}
	const bool bUpdated = BuildModel->UpdateRoadDraw(Cell.X, Cell.Y);
	SyncPlacementGhostAndCursor();
	return bUpdated;
}

bool AHansaStrategyPlayerController::EndRoadDrawing(
	const FVector2D ScreenPosition, const bool bPointerOverWorld, const bool bUseLivePointer)
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	if (BuildModel == nullptr || !BuildModel->GetSnapshot().bRoadDrawing) return false;
	bool bOverWorld = bPointerOverWorld;
	if (bPointerOverWorld)
	{
		bOverWorld = UpdateRoadDrawing(ScreenPosition, bUseLivePointer);
	}
	const bool bCommitted = BuildModel->EndRoadDraw(bOverWorld);
	SyncPlacementGhostAndCursor();
	return bCommitted;
}

void AHansaStrategyPlayerController::CancelBuildingPlacement()
{
	bRoadPointerHeld = false;
	if (UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel()) BuildModel->CancelIntent();
	SyncPlacementGhostAndCursor();
}

void AHansaStrategyPlayerController::RefreshBuildingPlacementPresentation()
{
	SyncPlacementGhostAndCursor();
}

void AHansaStrategyPlayerController::SyncPlacementGhostAndCursor()
{
	UHansaBuildMenuPresentationModel* BuildModel = GetBuildMenuModel();
	AHansaLubeckWorldFoundation* Foundation = FindPlacementFoundation();
	if (BuildModel == nullptr || Foundation == nullptr) return;
	const FHansaBuildMenuSnapshot& Snapshot = BuildModel->GetSnapshot();
	if (!Snapshot.SelectedBuildingId.IsNone() && Snapshot.bHasTarget)
	{
		AHansaBuildingPlacementGhost* Ghost = PlacementGhost.Get();
		if (Ghost == nullptr && GetWorld() != nullptr)
		{
			Ghost = GetWorld()->SpawnActor<AHansaBuildingPlacementGhost>();
			PlacementGhost = Ghost;
		}
		if (Ghost != nullptr)
		{
			if (Snapshot.bRoadDrawing)
			{
				Ghost->ApplyRoadPreview(Snapshot.RoadPreviewCells, Snapshot.Feedback,
					Snapshot.ValidationCause, *Foundation);
			}
			else
			{
                const uint8 AdjacentRoadMask=BuildModel->GetAdjacentRoadMaskForPreview();
                Ghost->ApplyPreview(Snapshot.SelectedBuildingId, Snapshot.AnchorCell,
                    Snapshot.RotationQuarterTurns, Snapshot.FootprintCells, Snapshot.Feedback,
                    Snapshot.ValidationCause, *Foundation, false, AdjacentRoadMask,
                    BuildModel->GetParcelSeedForPreview());
			}
		}
	}
	else if (AHansaBuildingPlacementGhost* Ghost = PlacementGhost.Get())
	{
		Ghost->HidePreview();
	}
	CurrentMouseCursor = Snapshot.bDemolitionMode ? EMouseCursor::Crosshairs : Snapshot.SelectedBuildingId.IsNone() ? EMouseCursor::Default
		: Snapshot.Feedback == EHansaPlacementFeedback::Invalid ? EMouseCursor::SlashedCircle
		: Snapshot.Feedback == EHansaPlacementFeedback::Warning ? EMouseCursor::CardinalCross
		: EMouseCursor::Crosshairs;
}


bool AHansaStrategyPlayerController::TryGetPlacementPointer(FVector2D& OutPointer) const
{
 const ULocalPlayer* LocalPlayer = GetLocalPlayer();
 const UGameViewportClient* Client = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
 const auto Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
 if (!Viewport.IsValid() || !FSlateApplication::IsInitialized()) return false;
 const FGeometry& Geometry = Viewport->GetCachedGeometry();
 const FVector2D LocalSize = Geometry.GetLocalSize();
 int32 Width = 0, Height = 0;
 GetViewportSize(Width, Height);
 if (LocalSize.X <= 0 || LocalSize.Y <= 0 || Width <= 0 || Height <= 0) return false;
 // GetMousePosition reads FSceneViewport's event cache, which is cleared on
 // mouse leave. Placement must follow the released cursor across UI/focus changes.
 const FVector2D Local = Geometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
 if (Local.X < 0 || Local.Y < 0 || Local.X >= LocalSize.X || Local.Y >= LocalSize.Y) return false;
 OutPointer = FVector2D(Local.X * Width / LocalSize.X, Local.Y * Height / LocalSize.Y);
 return true;
}

bool AHansaStrategyPlayerController::IsPointerOverWorldViewport() const
{
 if(!FSlateApplication::IsInitialized())return false;
 const ULocalPlayer* LocalPlayer = GetLocalPlayer();
 const auto Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->GetGameViewportWidget() : nullptr;
 if (!Viewport.IsValid()) return false;
 auto& Slate=FSlateApplication::Get();
 const auto Path=Slate.LocateWindowUnderMouse(Slate.GetCursorPos(),Slate.GetInteractiveTopLevelWindows());
 return Path.IsValid() && Path.Widgets.Last().Widget == Viewport;
}

void AHansaStrategyPlayerController::PlayerTick(float DeltaTime)
{
 Super::PlayerTick(DeltaTime);
 UpdateCameraDrag();
 auto* Model=GetBuildMenuModel();
 if(!Model || Model->GetSnapshot().SelectedBuildingId.IsNone()) {
  CurrentMouseCursor = Model && Model->GetSnapshot().bDemolitionMode ? EMouseCursor::Crosshairs : EMouseCursor::Default;
  if(auto* Ghost=PlacementGhost.Get();Ghost && Ghost->IsPreviewVisible())Ghost->HidePreview();
  return;
 }
 if(Model->IsBuildingStrokeActive() && !IsInputKeyDown(EKeys::LeftMouseButton))Model->EndBuildingStroke();
 if(Model->GetSnapshot().bDraggingCard || Model->GetSnapshot().bRoadDrawing)return;
 FVector2D Pointer;
 FIntPoint Cell;FVector Location;
 // Re-evaluate even at a stationary cursor: camera, UI and selection may change.
 const bool HasPointer=TryGetPlacementPointer(Pointer);
 const bool OverWorld=IsPointerOverWorldViewport();
 const bool Resolved=HasPointer && OverWorld && ResolvePlacementCellAtScreenPosition(Pointer,Cell,Location);
 if(Resolved) {
  if (!Model->GetSnapshot().bHasTarget || Model->GetSnapshot().AnchorCell != Cell) {
   if(Model->IsBuildingStrokeActive())Model->UpdateBuildingStroke(Cell.X,Cell.Y);
   else Model->TargetGridCell(Cell.X,Cell.Y);
   SyncPlacementGhostAndCursor();
  } else if (!PlacementGhost.IsValid() || !PlacementGhost->IsPreviewVisible())SyncPlacementGhostAndCursor();
 } else if (Model->GetSnapshot().bHasTarget) {
  // Suspend targets over UI/capture transitions. Only release/cancel ends the
  // held stroke, so moving back into the scene can continue placing safely.
  Model->ClearPointerTarget();SyncPlacementGhostAndCursor();
 }
}
