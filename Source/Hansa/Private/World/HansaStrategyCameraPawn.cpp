#include "World/HansaStrategyCameraPawn.h"

#include "Camera/CameraComponent.h"
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
	CameraState.ZoomDistance = FMath::Clamp(CameraBoom->TargetArmLength, MinimumZoomDistance, MaximumZoomDistance);
	ApplyCameraState();
}

void AHansaStrategyCameraPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Hansa::Game::FHansaStrategyCameraIntent EffectiveIntent = PendingIntent;
	if (EffectiveIntent.Pan.IsNearlyZero())
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
	CameraState = Hansa::Game::FHansaStrategyCameraModel::Advance(CameraState, EffectiveIntent, Settings, DeltaSeconds);
	PendingIntent.ZoomSteps = 0.0f;
	ApplyCameraState();
}

void AHansaStrategyCameraPawn::SetPanIntent(const FVector2D Value)
{
	PendingIntent.Pan = Value;
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
}
