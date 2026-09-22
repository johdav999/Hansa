#include "World/HansaLubeckWorldArt.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaGameMode.h"
#include "World/HansaGameState.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaPresentationClock.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/TextureCube.h"
#include "UObject/ConstructorHelpers.h"

namespace Hansa::Game::LubeckWorldArt
{
    namespace
    {
        constexpr double LubeckLatitudeDegrees = 53.87;
        constexpr double SolarNoonHour = 13.0;
        constexpr double PresentationDaysPerYear = 360.0;
        // Art-directed ceiling: the seasonal/elevation curve yields roughly 15 klux at the
        // locked spring-noon presentation, leaving the cooler sky to carry more of the fill.
        constexpr float BalticDaylightLux = 17500.0f;

        double Distance(const FVector2D& P, const LubeckPlacementGrid::FHansaSurfaceBox& Box)
        {
            const FVector V = Box.Rotation.UnrotateVector(FVector(P.X-Box.Location.X,P.Y-Box.Location.Y,0));
            const FVector2D Q(FMath::Abs(V.X)-Box.Scale.X*50,FMath::Abs(V.Y)-Box.Scale.Y*50);
            return FVector2D(FMath::Max(Q.X,0.),FMath::Max(Q.Y,0.)).Length()+FMath::Min(FMath::Max(Q.X,Q.Y),0.);
        }

        double SmoothStep(const double Minimum, const double Maximum, const double Value)
        {
            const double T = FMath::Clamp((Value - Minimum) / (Maximum - Minimum), 0.0, 1.0);
            return T * T * (3.0 - 2.0 * T);
        }
    }
    double GroundHeight(const FVector2D& P)
    {
        double Land=1.e9,Shore=1.e9;
        for(const auto& S:LubeckPlacementGrid::GetLandSurfaces())Land=FMath::Min(Land,Distance(P,S));
        for(const auto& S:LubeckPlacementGrid::GetShoreSurfaces())Shore=FMath::Min(Shore,Distance(P,S));
        // Exact datum across every permitted cell. Only the non-buildable bank falls to the river bed.
        const double D=FMath::Min(Land,Shore);
        const double Bank=FMath::Clamp(D/650.,0.,1.);
        const double Terrace=Shore<=0?85.:75.;
        return FMath::Lerp(Terrace,-450.,Bank*Bank*(3.-2.*Bank));
    }
    bool IsDressingLocation(const FVector2D& P,double Radius)
    {
        // Decorative solids may not claim any of the authoritative buildable land/shore.
        for(const auto& S:LubeckPlacementGrid::GetLandSurfaces())if(Distance(P,S)<=Radius+200.)return false;
        for(const auto& S:LubeckPlacementGrid::GetShoreSurfaces())if(Distance(P,S)<=Radius+200.)return false;
        return true;
    }

    FHansaLightingState EvaluateLighting(
        const Hansa::Simulation::FHansaCalendarProjection& Calendar,
        const double PresentationTickFraction,
        const uint16 MinutesPerTick)
    {
        // The simulation currently exposes elapsed days rather than a named season. Day zero is treated as the
        // spring equinox; the 360-day presentation year gives day 90/270 summer/winter solar declination.
        const double Fraction = FMath::Clamp(PresentationTickFraction, 0.0, 0.999999);
        const double MinuteOfDay = double(Calendar.HourOfDay) * 60.0 + Calendar.MinuteOfHour + Fraction * MinutesPerTick;
        const double AbsoluteDay = double(Calendar.ElapsedDays) + MinuteOfDay / 1440.0;
        const double DayOfYear = FMath::Fmod(FMath::Fmod(AbsoluteDay, PresentationDaysPerYear) + PresentationDaysPerYear,
            PresentationDaysPerYear);
        const double SolarHour = FMath::Fmod(FMath::Fmod(MinuteOfDay / 60.0, 24.0) + 24.0, 24.0);

        const double Declination = FMath::DegreesToRadians(23.44 * FMath::Sin(2.0 * PI * DayOfYear / PresentationDaysPerYear));
        const double Latitude = FMath::DegreesToRadians(LubeckLatitudeDegrees);
        const double HourAngleDegrees = (SolarHour - SolarNoonHour) * 15.0;
        const double HourAngle = FMath::DegreesToRadians(HourAngleDegrees);
        const double SinElevation = FMath::Clamp(
            FMath::Sin(Latitude) * FMath::Sin(Declination) +
            FMath::Cos(Latitude) * FMath::Cos(Declination) * FMath::Cos(HourAngle), -1.0, 1.0);
        const double ElevationDegrees = FMath::RadiansToDegrees(FMath::Asin(SinElevation));

        const double DirectDaylight = SmoothStep(0.0, 12.0, ElevationDegrees);
        const double ElevationEnergy = FMath::Lerp(0.68, 1.0, FMath::Clamp(SinElevation, 0.0, 1.0));
        const double SkyDaylight = SmoothStep(-6.0, 15.0, ElevationDegrees);
        const double SunWarmth = SmoothStep(0.0, 25.0, ElevationDegrees);

        FHansaLightingState State;
        State.SolarHour = SolarHour;
        // Raise the daytime presentation sun; retain continuous dawn, dusk and night.
        State.SolarElevationDegrees = float(FMath::Lerp(ElevationDegrees,
            FMath::Max(ElevationDegrees, 60.0), SmoothStep(8.0, 12.0, ElevationDegrees)));
        State.SunYawDegrees = float(-35.0 + HourAngleDegrees);
        State.SunIntensityLux = float(BalticDaylightLux * DirectDaylight * ElevationEnergy);
        State.SunTemperatureKelvin = float(FMath::Lerp(3900.0, 5700.0, SunWarmth));
        State.SunSourceAngleDegrees = 4.0f;
        // GrayLightTextureCube needs daylight-scale luminance alongside the 15 klux sun.
        // Calibrated on shaded brick facades at fixed exposure; preserve the night floor.
        State.SkyLightIntensity = float(FMath::Lerp(0.65, 2000.0, SkyDaylight));
        State.SkyTemperatureKelvin = float(FMath::Lerp(9000.0, 7500.0, SkyDaylight));
        State.ExposureEV100 = float(FMath::Lerp(3.5, 14.0, SmoothStep(-6.0, 15.0, ElevationDegrees)));
        return State;
    }
}
AHansaLubeckWorldArt::AHansaLubeckWorldArt()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    bReplicates=false;
#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WorldArtRoot")));
    Tags.Add(TEXT("Presentation.Environment.Lubeck"));
    Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));Sun->SetupAttachment(RootComponent);Sun->SetMobility(EComponentMobility::Movable);Sun->bAtmosphereSunLight=true;
    Sun->ContactShadowLength=0.0f;
    Sky=CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));Sky->SetupAttachment(RootComponent);Sky->SetMobility(EComponentMobility::Movable);Sky->bRealTimeCapture=false;Sky->SourceType=SLS_SpecifiedCubemap;
    static ConstructorHelpers::FObjectFinder<UTextureCube> AmbientCube(TEXT("/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube"));Sky->Cubemap=AmbientCube.Object;
    Atmosphere=CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));Atmosphere->SetupAttachment(RootComponent);
    Fog=CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("RiverHaze"));Fog->SetupAttachment(RootComponent);Fog->SetFogDensity(.007f);Fog->SetFogHeightFalloff(.25f);Fog->SetStartDistance(8000.f);
    Exposure=CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));Exposure->SetupAttachment(RootComponent);Exposure->bUnbound=true;Exposure->Priority=100.f;
    Exposure->Settings.bOverride_AutoExposureMethod=true;Exposure->Settings.AutoExposureMethod=AEM_Manual;
    Exposure->Settings.bOverride_AutoExposureMinBrightness=true;Exposure->Settings.bOverride_AutoExposureMaxBrightness=true;
    Exposure->Settings.AutoExposureMinBrightness=14;Exposure->Settings.AutoExposureMaxBrightness=14;
    Exposure->Settings.bOverride_AutoExposureBias=true;Exposure->Settings.AutoExposureBias=Hansa::Game::LubeckWorldArt::ExposureCompensationStops;
    Exposure->Settings.bOverride_CameraShutterSpeed=true;Exposure->Settings.CameraShutterSpeed=Hansa::Game::LubeckWorldArt::ExposureShutterSpeedForEV100(14);
    Exposure->Settings.bOverride_CameraISO=true;Exposure->Settings.CameraISO=100;
    Exposure->Settings.bOverride_DepthOfFieldFstop=true;Exposure->Settings.DepthOfFieldFstop=4;
    Exposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Exposure->Settings.AutoExposureApplyPhysicalCameraExposure=true;
    Exposure->Settings.bOverride_LocalExposureHighlightContrastScale=true;Exposure->Settings.LocalExposureHighlightContrastScale=1;
    Exposure->Settings.bOverride_LocalExposureShadowContrastScale=true;Exposure->Settings.LocalExposureShadowContrastScale=1;
    Exposure->Settings.bOverride_LocalExposureDetailStrength=true;Exposure->Settings.LocalExposureDetailStrength=1;
    Exposure->Settings.bOverride_LocalExposureHighlightContrastCurve=true;Exposure->Settings.LocalExposureHighlightContrastCurve=nullptr;
    Exposure->Settings.bOverride_LocalExposureShadowContrastCurve=true;Exposure->Settings.LocalExposureShadowContrastCurve=nullptr;
    Exposure->Settings.bOverride_AmbientOcclusionIntensity=true;Exposure->Settings.AmbientOcclusionIntensity=Hansa::Game::LubeckWorldArt::AmbientOcclusionIntensity;
    Exposure->Settings.bOverride_LumenAmbientOcclusionIntensity=true;Exposure->Settings.LumenAmbientOcclusionIntensity=Hansa::Game::LubeckWorldArt::LumenAmbientOcclusionIntensity;
    Exposure->Settings.bOverride_MotionBlurAmount=true;Exposure->Settings.MotionBlurAmount=0;
    Quay=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("QuayEdge"));
    Moorings=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Moorings"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Q(TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaQuay_Edge4m.SM_HansaQuay_Edge4m"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> M(TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaMooring_Post.SM_HansaMooring_Post"));
    Quay->SetStaticMesh(Q.Object);Moorings->SetStaticMesh(M.Object);
    for(auto* C:{Quay.Get(),Moorings.Get()}){C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetCullDistances(0,18000);C->ComponentTags.Add(TEXT("Presentation.Environment.Harbor"));}
}
void AHansaLubeckWorldArt::OnConstruction(const FTransform& T)
{
    Super::OnConstruction(T);
    Hansa::Simulation::FHansaCalendarProjection EditorPreviewCalendar;
    EditorPreviewCalendar.ElapsedDays=45;Hansa::Game::PresentationClock::LockToMidday(EditorPreviewCalendar);
    ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(EditorPreviewCalendar,0,60));
    Quay->ClearInstances();Moorings->ClearInstances();
    // Shore outside the playable central strip. No decorative pier or road suggests unowned connectivity.
    for(int I=0;I<8;++I){const FVector P(290,-1550+I*400,85);Quay->AddInstance(FTransform(FRotator::ZeroRotator,P));if(I%2==0)Moorings->AddInstance(FTransform(FVector(300,P.Y,85)));}
}
void AHansaLubeckWorldArt::BeginPlay()
{
    Super::BeginPlay();
    ApplyAuthoritativeSimulationLighting();
}
void AHansaLubeckWorldArt::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ApplyAuthoritativeSimulationLighting();
}
void AHansaLubeckWorldArt::SetLightingPreset(bool bEvening)
{
    bLightingPresetOverride=true;
    Hansa::Simulation::FHansaCalendarProjection Calendar;
    Calendar.ElapsedDays=bEvening?90:45;
    Calendar.HourOfDay=bEvening?20:13;
    ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar,0,60));
}
void AHansaLubeckWorldArt::ClearLightingPresetOverride()
{
    bLightingPresetOverride=false;
    ApplyAuthoritativeSimulationLighting();
}
void AHansaLubeckWorldArt::ApplyLightingState(const Hansa::Game::LubeckWorldArt::FHansaLightingState& State)
{
    if(!Sun||!Sky||!Exposure)return;
    Sun->SetRelativeRotation(FRotator(-State.SolarElevationDegrees,State.SunYawDegrees,0));
    Sun->SetIntensity(State.SunIntensityLux);
    Sun->SetUseTemperature(true);Sun->SetTemperature(State.SunTemperatureKelvin);Sun->SetLightColor(FLinearColor::White);
    Sun->SetLightSourceAngle(State.SunSourceAngleDegrees);
    Sun->ContactShadowLength=0.0f;
    Sky->SetIntensity(State.SkyLightIntensity);
    Sky->SetLightColor(FLinearColor::MakeFromColorTemperature(State.SkyTemperatureKelvin));
    Exposure->Settings.AutoExposureMinBrightness=State.ExposureEV100;
    Exposure->Settings.AutoExposureMaxBrightness=State.ExposureEV100;
    Exposure->Settings.CameraShutterSpeed=Hansa::Game::LubeckWorldArt::ExposureShutterSpeedForEV100(State.ExposureEV100);
    // Unreal 5.8 disables local exposure when both contrast scales and detail strength are neutral.
    Exposure->Settings.LocalExposureHighlightContrastScale=1;
    Exposure->Settings.LocalExposureShadowContrastScale=1;
    Exposure->Settings.LocalExposureDetailStrength=1;
    Exposure->Settings.LocalExposureHighlightContrastCurve=nullptr;
    Exposure->Settings.LocalExposureShadowContrastCurve=nullptr;
}
bool AHansaLubeckWorldArt::ApplyAuthoritativeSimulationLighting()
{
    if(bLightingPresetOverride)return false;
    UWorld* World=GetWorld();if(!World)return false;
    if(AHansaGameMode* Mode=World->GetAuthGameMode<AHansaGameMode>())
    {
        if(UHansaRuntimeSimulationHost* Host=Mode->GetSimulationHost())
        {
            Hansa::Simulation::FHansaCalendarProjection Calendar;double Fraction=0;uint16 MinutesPerTick=60;
            if(Host->TryGetPresentationCalendar(Calendar,Fraction,MinutesPerTick))
            {ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar,Fraction,MinutesPerTick));return true;}
        }
    }
    // Clients receive the authoritative tick through GameState. The hour-sized MVP clock is still evaluated by the
    // same continuous curve; the authority path above additionally applies the shared presentation-clock policy.
    if(const AHansaGameState* State=World->GetGameState<AHansaGameState>();State&&State->ServerSimulationTick>=0)
    {
        const int64 Tick=State->ServerSimulationTick;
        Hansa::Simulation::FHansaCalendarProjection Calendar;
        Calendar.ElapsedDays=Tick/24;Hansa::Game::PresentationClock::LockToMidday(Calendar);
        ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar,0,60));return true;
    }
    return false;
}
