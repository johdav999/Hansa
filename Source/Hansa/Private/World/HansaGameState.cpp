#include "World/HansaGameState.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HansaGameMode.h"
#include "World/HansaLubeckWorldArt.h"
#include "World/HansaPresentationClock.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"

AHansaGameState::AHansaGameState()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SimulationLightingRoot")));
	LightingExposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("SimulationLightingExposure"));
	LightingExposure->SetupAttachment(RootComponent);
	LightingExposure->bAutoActivate = true;
	LightingExposure->bUnbound = true;
	LightingExposure->Priority = 200.0f;
	LightingExposure->Settings.bOverride_AutoExposureMethod = true;
	LightingExposure->Settings.AutoExposureMethod = AEM_Manual;
	LightingExposure->Settings.bOverride_AutoExposureMinBrightness = true;
	LightingExposure->Settings.bOverride_AutoExposureMaxBrightness = true;
	LightingExposure->Settings.bOverride_AutoExposureBias = true;
	LightingExposure->Settings.AutoExposureBias = Hansa::Game::LubeckWorldArt::ExposureCompensationStops;
	LightingExposure->Settings.bOverride_CameraShutterSpeed = true;
	LightingExposure->Settings.CameraShutterSpeed = Hansa::Game::LubeckWorldArt::ExposureShutterSpeedForEV100(14.0f);
	LightingExposure->Settings.bOverride_CameraISO = true;
	LightingExposure->Settings.CameraISO = 100.0f;
	LightingExposure->Settings.bOverride_DepthOfFieldFstop = true;
	LightingExposure->Settings.DepthOfFieldFstop = 4.0f;
	LightingExposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	LightingExposure->Settings.AutoExposureApplyPhysicalCameraExposure = true;
	LightingExposure->Settings.bOverride_LocalExposureHighlightContrastScale = true;
	LightingExposure->Settings.LocalExposureHighlightContrastScale = 1.0f;
	LightingExposure->Settings.bOverride_LocalExposureShadowContrastScale = true;
	LightingExposure->Settings.LocalExposureShadowContrastScale = 1.0f;
	LightingExposure->Settings.bOverride_LocalExposureDetailStrength = true;
	LightingExposure->Settings.LocalExposureDetailStrength = 1.0f;
	LightingExposure->Settings.bOverride_LocalExposureHighlightContrastCurve = true;
	LightingExposure->Settings.LocalExposureHighlightContrastCurve = nullptr;
	LightingExposure->Settings.bOverride_LocalExposureShadowContrastCurve = true;
	LightingExposure->Settings.LocalExposureShadowContrastCurve = nullptr;
	LightingExposure->Settings.bOverride_AmbientOcclusionIntensity = true;
	LightingExposure->Settings.AmbientOcclusionIntensity = Hansa::Game::LubeckWorldArt::AmbientOcclusionIntensity;
	LightingExposure->Settings.bOverride_LumenAmbientOcclusionIntensity = true;
	LightingExposure->Settings.LumenAmbientOcclusionIntensity = Hansa::Game::LubeckWorldArt::LumenAmbientOcclusionIntensity;
	static ConstructorHelpers::FObjectFinder<UTextureCube> AmbientCube(
		TEXT("/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube"));
	AmbientLightingCube = AmbientCube.Object;
}

void AHansaGameState::BeginPlay()
{
	Super::BeginPlay();
	LightingExposure->Activate(true);
	RefreshStandaloneLightingActors();
	ApplySimulationLighting();
}

void AHansaGameState::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplySimulationLighting();
}

void AHansaGameState::RefreshStandaloneLightingActors()
{
	UWorld* World = GetWorld();
	if (World == nullptr) return;
	if (!StandaloneSun.IsValid())
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			if (!It->IsHidden() && It->GetLightComponent() != nullptr) { StandaloneSun = *It; break; }
		}
	}
	if (!StandaloneSky.IsValid())
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			if (!It->IsHidden() && It->GetLightComponent() != nullptr) { StandaloneSky = *It; break; }
		}
	}
	if (ADirectionalLight* SunActor = StandaloneSun.Get())
	{
		if (UDirectionalLightComponent* Component = Cast<UDirectionalLightComponent>(SunActor->GetLightComponent()))
		{
			Component->SetMobility(EComponentMobility::Movable);
			Component->bAtmosphereSunLight = true;
			Component->ContactShadowLength = 0.0f;
		}
	}
	if (ASkyLight* SkyActor = StandaloneSky.Get())
	{
		USkyLightComponent* Component = SkyActor->GetLightComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->bRealTimeCapture = false;
		Component->SourceType = SLS_SpecifiedCubemap;
		Component->SetCubemap(AmbientLightingCube);
	}
}

void AHansaGameState::ApplySimulationLighting()
{
	if (!StandaloneSun.IsValid() || !StandaloneSky.IsValid()) RefreshStandaloneLightingActors();
	Hansa::Simulation::FHansaCalendarProjection Calendar;
	double Fraction = 0.0;
	uint16 MinutesPerTick = 60;
	bool bHasCalendar = false;
	if (UWorld* World = GetWorld())
	{
		if (AHansaGameMode* Mode = World->GetAuthGameMode<AHansaGameMode>())
		{
			if (UHansaRuntimeSimulationHost* Host = Mode->GetSimulationHost())
				bHasCalendar = Host->TryGetPresentationCalendar(Calendar, Fraction, MinutesPerTick);
		}
	}
	if (!bHasCalendar && ServerSimulationTick >= 0)
	{
		Calendar.ElapsedDays = ServerSimulationTick / 24;
		Hansa::Game::PresentationClock::LockToMidday(Calendar, &Fraction);
		bHasCalendar = true;
	}
	if (!bHasCalendar) return;
	const Hansa::Game::LubeckWorldArt::FHansaLightingState Lighting =
		Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar, Fraction, MinutesPerTick);
	if (ADirectionalLight* SunActor = StandaloneSun.Get())
	{
		UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(SunActor->GetLightComponent());
		if (Sun == nullptr) return;
		Sun->SetWorldRotation(FRotator(-Lighting.SolarElevationDegrees, Lighting.SunYawDegrees, 0.0f));
		Sun->SetIntensity(Lighting.SunIntensityLux);
		Sun->SetUseTemperature(true);
		Sun->SetTemperature(Lighting.SunTemperatureKelvin);
		Sun->SetLightColor(FLinearColor::White);
		Sun->SetLightSourceAngle(Lighting.SunSourceAngleDegrees);
		Sun->ContactShadowLength = 0.0f;
	}
	if (ASkyLight* SkyActor = StandaloneSky.Get())
	{
		USkyLightComponent* Sky = SkyActor->GetLightComponent();
		Sky->SetIntensity(Lighting.SkyLightIntensity);
		Sky->SetLightColor(FLinearColor::MakeFromColorTemperature(Lighting.SkyTemperatureKelvin));
	}
	if (LightingExposure != nullptr)
	{
		LightingExposure->Settings.AutoExposureMinBrightness = Lighting.ExposureEV100;
		LightingExposure->Settings.AutoExposureMaxBrightness = Lighting.ExposureEV100;
		// f/4 and ISO 100 make this shutter-speed curve a direct EV100 representation.
		LightingExposure->Settings.CameraShutterSpeed =
			Hansa::Game::LubeckWorldArt::ExposureShutterSpeedForEV100(Lighting.ExposureEV100);
	}
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (AHansaStrategyCameraPawn* CameraPawn = Cast<AHansaStrategyCameraPawn>(Controller->GetPawn()))
				CameraPawn->SetPresentationExposureEV100(Lighting.ExposureEV100);
		}
	}
}

void AHansaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHansaGameState, ServerSimulationTick);
	DOREPLIFETIME(AHansaGameState, PublicProjectionRevision);
	DOREPLIFETIME(AHansaGameState, ServerAuthoritativeHash);
	DOREPLIFETIME(AHansaGameState, ScenarioOutcome);
	DOREPLIFETIME(AHansaGameState, WinningVictoryId);
	DOREPLIFETIME(AHansaGameState, VictoryObjectives);
}

void AHansaGameState::PublishAuthoritativeProjection(const FHansaClientProjectionSnapshot& Projection)
{
	if (!HasAuthority()) return;
	ServerSimulationTick = Projection.ServerTick;
	PublicProjectionRevision = FMath::Max(PublicProjectionRevision + 1, Projection.Revision);
	ServerAuthoritativeHash = Projection.AuthoritativeHash;
	ScenarioOutcome = Projection.ScenarioOutcome;
	WinningVictoryId = Projection.WinningVictoryId;
	VictoryObjectives = Projection.VictoryObjectives;
	ForceNetUpdate();
}
