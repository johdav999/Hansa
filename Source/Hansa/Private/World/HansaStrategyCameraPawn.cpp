#include "World/HansaStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "HansaLog.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "World/HansaLubeckWorldFoundation.h"

AHansaStrategyCameraPawn::AHansaStrategyCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = 6500.0f;
	CameraBoom->SetRelativeRotation(FRotator(-55.0, 0.0, 0.0));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	CameraState.Focus = FVector2D(-3200.0, -700.0);
	CameraState.YawDegrees = 35.0f;
	CameraState.ZoomDistance = CameraBoom->TargetArmLength;
}

void AHansaStrategyCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	ResolveMapBounds();
	CameraState.Focus = FVector2D(GetActorLocation().X, GetActorLocation().Y);
	CameraState.YawDegrees = GetActorRotation().Yaw;
	CameraState.PitchDegrees = CameraBoom->GetRelativeRotation().Pitch;
	CameraState.ZoomDistance = FMath::Clamp(CameraBoom->TargetArmLength, MinimumZoomDistance, MaximumZoomDistance);
	ApplyCameraState();
}

void AHansaStrategyCameraPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Hansa::Game::FHansaStrategyCameraIntent EffectiveIntent = PendingIntent;
	if (!bDragPanActive && !bDragRotateActive && EffectiveIntent.Pan.IsNearlyZero())
	{
		EffectiveIntent.Pan = GetMouseEdgePanIntent();
	}

	Hansa::Game::FHansaStrategyCameraSettings Settings;
	Settings.BoundsMin = MapBoundsMin;
	Settings.BoundsMax = MapBoundsMax;
	Settings.PanUnitsPerSecond = PanUnitsPerSecond;
	Settings.FastPanMultiplier = FastPanMultiplier;
	Settings.RotationDegreesPerSecond = RotationDegreesPerSecond;
	Settings.ZoomUnitsPerStep = ZoomUnitsPerStep;
	Settings.MinimumZoomDistance = MinimumZoomDistance;
	Settings.MaximumZoomDistance = MaximumZoomDistance;
	const float PreviousYaw = CameraState.YawDegrees;
	CameraState = Hansa::Game::FHansaStrategyCameraModel::Advance(CameraState, EffectiveIntent, Settings, DeltaSeconds);
	PendingIntent.ZoomSteps = 0.0f;
    PendingIntent.YawDisplacement = 0.0f;
    PendingIntent.PitchDisplacement = 0.0f;
    PendingIntent.PanDisplacement = FVector2D::ZeroVector;
    ApplyCameraState();
    const double Now = FPlatformTime::Seconds();
    if (bDragRotateActive && Now - LastCameraYawDiagnosticTime >= 0.5)
    {
        LastCameraYawDiagnosticTime = Now;
        UE_LOG(LogHansa, Log, TEXT("[CameraDrag] Applied pawn=%s dt=%.4f requestedYawDelta=%.3f yawBefore=%.3f yawAfter=%.3f actorYaw=%.3f cameraYaw=%.3f requestedPitchDelta=%.3f pitch=%.3f cameraPitch=%.3f intentValid=%d settingsValid=%d"),
            *GetName(), DeltaSeconds, EffectiveIntent.YawDisplacement, PreviousYaw, CameraState.YawDegrees,
            GetActorRotation().Yaw, Camera->GetComponentRotation().Yaw, EffectiveIntent.PitchDisplacement,
            CameraState.PitchDegrees, Camera->GetComponentRotation().Pitch, EffectiveIntent.IsFinite(), Settings.IsValid());
    }
}

void AHansaStrategyCameraPawn::SetPanIntent(const FVector2D Value)
{
	PendingIntent.Pan = Value;
}

void AHansaStrategyCameraPawn::SetDragPanIntent(const FVector2D PixelDelta, const bool bActive)
{
    bDragPanActive = bActive;
    if (!bActive)
    {
        PendingIntent.PanDisplacement = FVector2D::ZeroVector;
    }
    else if (!PixelDelta.ContainsNaN() && FMath::IsFinite(DragPanUnitsPerPixel))
    {
        PendingIntent.PanDisplacement += FVector2D(-PixelDelta.X, PixelDelta.Y) *
            FMath::Max(0.0f, DragPanUnitsPerPixel);
    }
}

void AHansaStrategyCameraPawn::SetDragRotateIntent(const float HorizontalPixelDelta, const bool bActive)
{
    bDragRotateActive = bActive;
    if (!bActive)
    {
        PendingIntent.YawDisplacement = 0.0f;
        PendingIntent.PitchDisplacement = 0.0f;
    }
    else if (FMath::IsFinite(HorizontalPixelDelta) && FMath::IsFinite(DragRotationDegreesPerPixel))
    {
        PendingIntent.YawDisplacement += HorizontalPixelDelta * FMath::Max(0.0f, DragRotationDegreesPerPixel);
    }
}

void AHansaStrategyCameraPawn::SetDragOrbitIntent(const FVector2D PixelDelta, const bool bActive)
{
    const FVector2D SafeDelta = PixelDelta.ContainsNaN() ? FVector2D::ZeroVector : PixelDelta;
    SetDragRotateIntent(SafeDelta.X, bActive);
    if (bActive && FMath::IsFinite(DragRotationDegreesPerPixel))
    {
        // Screen Y increases downward; dragging up raises the viewing direction.
        PendingIntent.PitchDisplacement -= SafeDelta.Y * FMath::Max(0.0f, DragRotationDegreesPerPixel);
    }
}

void AHansaStrategyCameraPawn::SetRotateIntent(const float Value)
{
	PendingIntent.Rotate = Value;
}

void AHansaStrategyCameraPawn::AddZoomIntent(const float Steps)
{
	if (FMath::IsFinite(Steps))
	{
		PendingIntent.ZoomSteps += Steps;
	}
}

void AHansaStrategyCameraPawn::SetFastPanIntent(const bool bValue)
{
	PendingIntent.bFastPan = bValue;
}

void AHansaStrategyCameraPawn::ClearCameraIntents()
{
	PendingIntent = Hansa::Game::FHansaStrategyCameraIntent();
    bDragPanActive = false;
    bDragRotateActive = false;
}

void AHansaStrategyCameraPawn::FocusWorldLocationIntent(const FVector WorldLocation)
{
	if (!WorldLocation.ContainsNaN())
	{
		CameraState.Focus.X = FMath::Clamp(WorldLocation.X, MapBoundsMin.X, MapBoundsMax.X);
		CameraState.Focus.Y = FMath::Clamp(WorldLocation.Y, MapBoundsMin.Y, MapBoundsMax.Y);
		PendingIntent.Pan = FVector2D::ZeroVector;
		ApplyCameraState();
	}
}

FVector2D AHansaStrategyCameraPawn::GetMouseEdgePanIntent() const
{
	if (!bEnableMouseEdgePan || EdgePanMarginPixels <= 0.0f)
	{
		return FVector2D::ZeroVector;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return FVector2D::ZeroVector;
	}

	int32 Width = 0;
	int32 Height = 0;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PlayerController->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0 || !PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return FVector2D::ZeroVector;
	}

	FVector2D Result = FVector2D::ZeroVector;
	Result.X = MouseX <= EdgePanMarginPixels ? -1.0f : (MouseX >= Width - EdgePanMarginPixels ? 1.0f : 0.0f);
	Result.Y = MouseY <= EdgePanMarginPixels ? 1.0f : (MouseY >= Height - EdgePanMarginPixels ? -1.0f : 0.0f);
	return Result.GetClampedToMaxSize(1.0f);
}

void AHansaStrategyCameraPawn::ResolveMapBounds()
{
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It)
		{
			MapBoundsMin = It->GetCameraBoundsMin();
			MapBoundsMax = It->GetCameraBoundsMax();
			return;
		}
	}

	MapBoundsMin = Hansa::Game::LubeckMap::CameraBoundsMin();
	MapBoundsMax = Hansa::Game::LubeckMap::CameraBoundsMax();
}

void AHansaStrategyCameraPawn::ApplyCameraState()
{
	SetActorLocation(FVector(CameraState.Focus.X, CameraState.Focus.Y, GetActorLocation().Z));
	SetActorRotation(FRotator(0.0, CameraState.YawDegrees, 0.0));
	CameraBoom->TargetArmLength = CameraState.ZoomDistance;
	const Hansa::Game::FHansaStrategyCameraSettings Settings;
	CameraState.PitchDegrees = FMath::Clamp(CameraState.PitchDegrees,
		Settings.MinimumPitchDegrees, Settings.MaximumPitchDegrees);
	CameraBoom->SetRelativeRotation(FRotator(CameraState.PitchDegrees, 0.0, 0.0));
}

void AHansaStrategyCameraPawn::SetViewBounds(FVector2D Min,FVector2D Max)
{
    if(Min.ContainsNaN()||Max.ContainsNaN()||Min.X>=Max.X||Min.Y>=Max.Y)return;
    ClearCameraIntents();MapBoundsMin=Min;MapBoundsMax=Max;
}
void AHansaStrategyCameraPawn::RestoreHomeBounds(){ClearCameraIntents();ResolveMapBounds();}

void AHansaStrategyCameraPawn::RestoreViewState(const Hansa::Game::FHansaStrategyCameraState& State)
{
    ClearCameraIntents();
    if (!State.IsFinite()) return;
    CameraState = State;
    ApplyCameraState();
}
