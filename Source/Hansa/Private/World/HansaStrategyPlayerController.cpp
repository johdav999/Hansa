#include "World/HansaStrategyPlayerController.h"

#include "HansaLog.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaStrategyCameraPawn.h"

namespace
{
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
}

void AHansaStrategyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	if (AHansaStrategyCameraPawn* CameraPawn = GetStrategyCameraPawn())
	{
		CameraPawn->SetRotateIntent(Value.Get<float>());
	}
}

void AHansaStrategyPlayerController::HandleRotateCompleted(const FInputActionValue& Value)
{
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
	PerformWorldSelection();
}
