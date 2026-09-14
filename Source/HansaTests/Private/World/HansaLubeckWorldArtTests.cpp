#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckWorldArt.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaPresentationClock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckArtGrading,"Hansa.World.LubeckArt.GradingContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckArtGrading::RunTest(const FString&)
{
    using namespace Hansa::Game;
    for(int X=0;X<LubeckPlacementGrid::WidthCells;++X)for(int Y=0;Y<LubeckPlacementGrid::HeightCells;++Y)
    {
        const auto C=LubeckPlacementGrid::GridToWorld({X,Y});const FVector2D P(C.X,C.Y);
        const auto T=LubeckPlacementGrid::TerrainAt(P);const double H=LubeckWorldArt::GroundHeight(P);
        if(T!=Hansa::Simulation::EHansaPlacementTerrain::Water)
        {
            TestEqual(TEXT("Every legal cell retains its ground datum"),H,T==Hansa::Simulation::EHansaPlacementTerrain::Shore?85.:75.);
            TestFalse(TEXT("Ambient dressing cannot claim a buildable plot"),LubeckWorldArt::IsDressingLocation(P,100));
        }
        TestTrue(TEXT("Height remains finite within encoded Landscape range"),FMath::IsFinite(H)&&H>=-450&&H<=85);
    }
    TestEqual(TEXT("Far river bed remains below the water surface"),LubeckWorldArt::GroundHeight(FVector2D(20000,20000)),-450.);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckArtLighting,"Hansa.World.LubeckArt.SimulationLightingCurve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckArtLighting::RunTest(const FString&)
{
    using namespace Hansa::Game::LubeckWorldArt;
    Hansa::Simulation::FHansaCalendarProjection SpringNoon;SpringNoon.ElapsedDays=5;SpringNoon.HourOfDay=13;
    const FHansaLightingState Day=EvaluateLighting(SpringNoon,0,60);
    TestTrue(TEXT("Baltic daylight stays within the softer 14-16 klux band"),Day.SunIntensityLux>=14000&&Day.SunIntensityLux<=16000);
    TestTrue(TEXT("Daylight exposure is rebalanced within EV100 13.7-14.2"),Day.ExposureEV100>=13.7f&&Day.ExposureEV100<=14.2f);
    TestTrue(TEXT("The four-degree source angle softens the solar shadow"),Day.SunSourceAngleDegrees>=3.5f&&Day.SunSourceAngleDegrees<=4.5f);
    TestTrue(TEXT("Day skylight supplies stronger cool fill"),Day.SkyLightIntensity>=1.7f&&Day.SkyLightIntensity<=1.9f&&Day.SkyTemperatureKelvin>Day.SunTemperatureKelvin);

    Hansa::Simulation::FHansaCalendarProjection SpringNight=SpringNoon;SpringNight.HourOfDay=21;
    const FHansaLightingState Night=EvaluateLighting(SpringNight,0,60);
    TestTrue(TEXT("At 21:00 near spring the sun is below the horizon"),Night.SolarElevationDegrees<0);
    TestEqual(TEXT("A below-horizon sun contributes no direct light"),Night.SunIntensityLux,0.f);
    TestTrue(TEXT("Night exposure and sky fill follow their own continuous curves"),Night.ExposureEV100<Day.ExposureEV100&&Night.SkyLightIntensity<Day.SkyLightIntensity);

    Hansa::Simulation::FHansaCalendarProjection SummerEvening=SpringNight;SummerEvening.ElapsedDays=90;
    const FHansaLightingState Summer=EvaluateLighting(SummerEvening,0,60);
    TestTrue(TEXT("At 21:00 around midsummer the sun remains close to the horizon"),Summer.SolarElevationDegrees>-2&&Summer.SolarElevationDegrees<10);

    Hansa::Simulation::FHansaCalendarProjection Before=SpringNoon;Before.HourOfDay=12;Before.MinuteOfHour=59;
    Hansa::Simulation::FHansaCalendarProjection After=SpringNoon;After.HourOfDay=13;After.MinuteOfHour=1;
    const FHansaLightingState Left=EvaluateLighting(Before,0,60),Right=EvaluateLighting(After,0,60);
    TestTrue(TEXT("The curve is continuous across an hour boundary"),
        FMath::Abs(Left.SolarElevationDegrees-Right.SolarElevationDegrees)<1.f&&
        FMath::Abs(Left.SunIntensityLux-Right.SunIntensityLux)<1000.f&&
        FMath::Abs(Left.ExposureEV100-Right.ExposureEV100)<.1f);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckPresentationClock,"Hansa.World.LubeckArt.PresentationClockLockedMidday",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckPresentationClock::RunTest(const FString&)
{
	Hansa::Simulation::FHansaCalendarProjection Calendar;
	Calendar.ElapsedDays=17;Calendar.HourOfDay=23;Calendar.MinuteOfHour=45;
	double Fraction=.75;
	Hansa::Game::PresentationClock::LockToMidday(Calendar,&Fraction);
	TestEqual(TEXT("Presentation preserves the advancing simulation day"),Calendar.ElapsedDays,int64(17));
	TestEqual(TEXT("Presentation hour is locked to 12"),Calendar.HourOfDay,uint8(12));
	TestEqual(TEXT("Presentation minute is locked to zero"),Calendar.MinuteOfHour,uint8(0));
	TestEqual(TEXT("Fractional time cannot move lighting away from noon"),Fraction,0.0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckProductionLightingOwner,"Hansa.World.LubeckArt.ProductionLightingOwner",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckProductionLightingOwner::RunTest(const FString&)
{
    const AHansaLubeckWorldFoundation* Foundation=GetDefault<AHansaLubeckWorldFoundation>();
    TestNotNull(TEXT("Production foundation owns the directional light"),Foundation->SunLight.Get());
    TestNotNull(TEXT("Production foundation owns the skylight"),Foundation->SkyLight.Get());
    TestNotNull(TEXT("Production foundation owns the atmosphere"),Foundation->SkyAtmosphere.Get());
    TestNotNull(TEXT("Production foundation owns the exposure override"),Foundation->Exposure.Get());
    TestTrue(TEXT("Production lighting samples simulation time every frame"),Foundation->PrimaryActorTick.bCanEverTick);
    TestEqual(TEXT("Production sun is movable"),Foundation->SunLight->Mobility,EComponentMobility::Movable);
    TestTrue(TEXT("Production sun drives the atmosphere"),Foundation->SunLight->bAtmosphereSunLight);
    TestTrue(TEXT("Production skylight keeps a stable ambient source after sunset"),Foundation->SkyLight->SourceType==SLS_SpecifiedCubemap&&Foundation->SkyLight->Cubemap!=nullptr);
    TestTrue(TEXT("Production exposure overrides EV100"),Foundation->Exposure->Settings.bOverride_AutoExposureMinBrightness&&Foundation->Exposure->Settings.bOverride_AutoExposureMaxBrightness);
    TestEqual(TEXT("Production uses deterministic manual exposure"),Foundation->Exposure->Settings.AutoExposureMethod,AEM_Manual);
    TestEqual(TEXT("Production terrain exposure compensation stays separate from EV100"),Foundation->Exposure->Settings.AutoExposureBias,Hansa::Game::LubeckWorldArt::ExposureCompensationStops);
    TestTrue(TEXT("Production manual exposure uses physical-camera EV100"),Foundation->Exposure->Settings.AutoExposureApplyPhysicalCameraExposure);
    TestEqual(TEXT("Production local highlight exposure is neutral"),Foundation->Exposure->Settings.LocalExposureHighlightContrastScale,1.f);
    TestEqual(TEXT("Production local shadow exposure is neutral"),Foundation->Exposure->Settings.LocalExposureShadowContrastScale,1.f);
    TestEqual(TEXT("Production contact shadows stay disabled"),Foundation->SunLight->ContactShadowLength,0.f);
    TestEqual(TEXT("Production SSAO is restrained"),Foundation->Exposure->Settings.AmbientOcclusionIntensity,Hansa::Game::LubeckWorldArt::AmbientOcclusionIntensity);
    TestEqual(TEXT("Production Lumen AO is restrained"),Foundation->Exposure->Settings.LumenAmbientOcclusionIntensity,Hansa::Game::LubeckWorldArt::LumenAmbientOcclusionIntensity);
    return !HasAnyErrors();
}
#endif
