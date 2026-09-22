#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "ImageUtils.h"
#include "RenderTimer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaGameState.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckWorldArt.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "../UI/HansaFrontendCaptureSupport.h"

namespace
{
class FLubeckArtCapture:public IAutomationLatentCommand
{
public:
    explicit FLubeckArtCapture(FAutomationTestBase* In):Test(In),Start(FPlatformTime::Seconds()){}
    bool Update()override
    {
        if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("World art viewport timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();
        auto* C=W?W->GetFirstPlayerController():nullptr;auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
        if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
        auto* Host=Cast<AHansaGameMode>(W->GetAuthGameMode())->GetSimulationHost();auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn());
        if(!Host||!Camera)return false;
        auto* Model=H->GetBuildMenuPresentationModel();
        if(!Prepared)
        {
            if(Stage==0)
            {
                H->GetScenarioPresentationModel()->AcknowledgeBriefing();H->GetScenarioPresentationModel()->DismissHelp();Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
                Fingerprint=Host->BuildProjection().Value.GetFingerprint().Value;
            }
            Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->MaximumZoomDistance=12000;
            Camera->FocusWorldLocationIntent(FVector(-3200,-700,100));
            const float Zoom=Stage<6?TArray<float>{2500,6500,12000}[Stage%3]:6500;
            Camera->AddZoomIntent((Camera->GetZoomDistance()-Zoom)/Camera->ZoomUnitsPerStep);
            for(TActorIterator<AHansaLubeckWorldArt> I(W);I;++I)I->SetLightingPreset(Stage>=3&&Stage<6);
            if(Stage==6)
            {
                Test->TestTrue(TEXT("Road card starts ordinary placement"),Model->BeginCardDrag(TEXT("Building.Road")));
                Model->UpdateCardDragTarget(18,16);Test->TestTrue(TEXT("Road placed through normal command"),Model->EndCardDrag(true));Host->AdvanceTicks(60);
                bool Placed=false;Test->TestTrue(TEXT("Native building card starts placement"),Model->BeginCardDrag(TEXT("Building.Residence.Laborer")));
                for(int X=11;X<30&&!Placed;++X)for(int Y=10;Y<30&&!Placed;++Y)if(Model->UpdateCardDragTarget(X,Y)&&Model->GetSnapshot().bCanConfirm)Placed=Model->EndCardDrag(true);
                Test->TestTrue(TEXT("Construction submitted through ordinary card placement"),Placed);if(!Placed)Test->AddError(Model->GetSnapshot().LastResult.ToString());Model->CancelIntent();Model->SetOpen(false);Host->AdvanceTicks(60);Host->SynchronizeWorldProjection();
                Fingerprint=Host->BuildProjection().Value.GetFingerprint().Value;Host->CaptureSaveBytes(Save,TEXT("World art review"),TEXT("2026-09-09T00:00:00Z"));
            }
            if(Stage==7)
            {
                Host->AdvanceTicks(2);Host->RestoreSaveBytes(Save);Test->TestEqual(TEXT("Save restores authoritative state"),Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
            }
            GameMs=RenderMs=0;Samples=0;ReadyAt=FPlatformTime::Seconds()+(Stage==0?8:2);Prepared=true;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt){if(FPlatformTime::Seconds()>ReadyAt-1.5){GameMs+=FPlatformTime::ToMilliseconds(GGameThreadTime);RenderMs+=FPlatformTime::ToMilliseconds(GRenderThreadTime);++Samples;}return false;}
        if(Stage<6)Test->TestEqual(TEXT("Lighting and framing never mutate authority"),Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
        TArray<FColor> Pixels;FIntVector Size;if(!V->GetGameViewportWidget()||!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)||Pixels.IsEmpty()){Test->AddError(TEXT("Native screenshot failed"));return true;}
        const FIntPoint Extent=V->Viewport->GetSizeXY();int X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
        Test->TestTrue(TEXT("No capture resampling"),Extent==FIntPoint(X,Y)&&Size.X==X&&Size.Y==Y);
        Test->TestTrue(TEXT("Reject blank readback"),Pixels.ContainsByPredicate([](const FColor& P){return P.R>30||P.G>30||P.B>30;}));
        const TCHAR* States[]={TEXT("day-25m"),TEXT("day-65m"),TEXT("day-120m"),TEXT("evening-25m"),TEXT("evening-65m"),TEXT("evening-120m"),TEXT("constructed"),TEXT("restored")};
        const bool Candidate=W->GetPackage()->GetName().Contains(TEXT("LubeckWorldArt_P30"));
        const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P30/%s-%dx%d-%s"),Candidate?TEXT("candidate"):TEXT("baseline"),X,Y,States[Stage]);
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(X,Y,Pixels,PNG);Test->TestTrue(TEXT("Write original capture"),FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png"))));
        FString Audit=TEXT("actor\tdefinition\tcomponent\tmesh\tinstances\tlods\tlod0Triangles\tmaterials\tcastShadow\tenginePrimitive\n");int Actors=0,Primitives=0,Staging=0,Instances=0;int64 Triangles=0;TSet<FString> Materials;
        for(TActorIterator<AActor> I(W);I;++I)
        {
            ++Actors;TArray<UStaticMeshComponent*> Comps;I->GetComponents(Comps);
            for(auto* P:Comps)if(P->IsVisible()&&!P->bHiddenInGame&&P->GetStaticMesh()&&!I->IsHidden())
            {
                UStaticMesh* M=P->GetStaticMesh();const FString Path=M->GetPathName();const bool Primitive=Path.StartsWith(TEXT("/Engine/BasicShapes/"));Primitives+=Primitive;Staging+=Path.Contains(TEXT("/Generated/Staging/"));
                auto* ISM=Cast<UInstancedStaticMeshComponent>(P);const int Count=ISM?ISM->GetInstanceCount():1;Instances+=Count;
                const auto* R=M->GetRenderData();const int Tris=R&&!R->LODResources.IsEmpty()?R->LODResources[0].GetNumTriangles():0;Triangles+=int64(Tris)*Count;
                for(int N=0;N<P->GetNumMaterials();++N)if(P->GetMaterial(N))Materials.Add(P->GetMaterial(N)->GetPathName());
                Audit+=FString::Printf(TEXT("%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n"),*I->GetName(),Cast<AHansaBuildingWorldProjectionActor>(*I)?*Cast<AHansaBuildingWorldProjectionActor>(*I)->GetStableBuildingDefinitionId():TEXT("environment"),*P->GetName(),*Path,Count,R?R->LODResources.Num():0,Tris,P->GetNumMaterials(),int(P->CastShadow),int(Primitive));
            }
        }
        FFileHelper::SaveStringToFile(Audit,*(Base+TEXT(".tsv")));
        FString RuntimeGeometry;
        for(TActorIterator<AActor> I(W);I;++I)if(I->GetClass()->GetName().Contains(TEXT("Landscape"))||I->GetClass()->GetName().Contains(TEXT("Water")))
            RuntimeGeometry+=FString::Printf(TEXT("%s %s transform=%s bounds=%s\n"),*I->GetName(),*I->GetClass()->GetName(),*I->GetActorTransform().ToString(),*I->GetComponentsBoundingBox().ToString());
        for(const FVector2D P:{FVector2D(-3200,-700),FVector2D(0,0),FVector2D(2000,0)})
        {
            FHitResult Hit;W->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,500),FVector(P.X,P.Y,-1000),ECC_Visibility);
            RuntimeGeometry+=FString::Printf(TEXT("trace %.0f %.0f hit=%s z=%.2f expected=%.2f\n"),P.X,P.Y,Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),Hit.ImpactPoint.Z,Hansa::Game::LubeckWorldArt::GroundHeight(P));
        }
        FFileHelper::SaveStringToFile(RuntimeGeometry,*(Base+TEXT(".geometry.txt")));
        const FString Summary=FString::Printf(TEXT("map=%s\nstate=%s\ntick=%lld\nfingerprint=%llu\nactors=%d\nvisibleMeshInstances=%d\nlod0UpperBoundTriangles=%lld\nmaterialInterfaces=%d\nenginePrimitiveComponents=%d\nstagingMeshComponents=%d\nreleaseAccepted=false\n"),*W->GetPackage()->GetName(),States[Stage],Host->GetSimulationTick(),Fingerprint,Actors,Instances,Triangles,Materials.Num(),Primitives,Staging);
        FFileHelper::SaveStringToFile(Summary+FString::Printf(TEXT("gameThreadMeanMs=%.3f\nrenderThreadMeanMs=%.3f\nframeSamples=%d\n"),Samples?GameMs/Samples:0,Samples?RenderMs/Samples:0,Samples),*(Base+TEXT(".txt")));
        if(Candidate)for(TActorIterator<AHansaLubeckWorldFoundation> I(W);I;++I){TArray<UStaticMeshComponent*> Comps;I->GetComponents(Comps);for(auto* P:Comps)Test->TestFalse(TEXT("Authored terrain suppresses every legacy foundation cube"),P->IsVisible());}
        ++Stage;Prepared=false;if(Stage==8){return true;}return false;
    }
private:
    FAutomationTestBase* Test;double Start,ReadyAt=0,GameMs=0,RenderMs=0;int Samples=0;int Stage=0;bool Prepared=false;uint64 Fingerprint=0;TArray<uint8> Save;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckArtViewport,"Hansa.World.LubeckArt.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FLubeckArtViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FLubeckArtCapture(this));return true;}

namespace
{
class FLubeckProductionLockedMiddayCapture final:public IAutomationLatentCommand
{
public:
    explicit FLubeckProductionLockedMiddayCapture(FAutomationTestBase* In):Test(In),Start(FPlatformTime::Seconds()){}
    bool Update()override
    {
        if(FPlatformTime::Seconds()-Start>60){Test->AddError(TEXT("Production locked-midday viewport timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* Viewport=GEngine->GameViewport.Get();auto* World=Viewport->GetWorld();
        auto* Controller=World?World->GetFirstPlayerController():nullptr;auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
        auto* Mode=World?Cast<AHansaGameMode>(World->GetAuthGameMode()):nullptr;auto* Host=Mode?Mode->GetSimulationHost():nullptr;
        auto* Camera=Controller?Cast<AHansaStrategyCameraPawn>(Controller->GetPawn()):nullptr;
        if(!Hud||!Hud->GetRootWidget()||!Host||!Camera||HansaWaitForFrontend(Hud))return false;
        if(Prepared)Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
        if(!Prepared)
        {
            Hud->GetScenarioPresentationModel()->AcknowledgeBriefing();Hud->GetScenarioPresentationModel()->DismissHelp();
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
            constexpr int64 DayFiveAtUnderlying2200=4*24+22;
            if(Host->GetSimulationTick()<DayFiveAtUnderlying2200)Host->AdvanceTicks(DayFiveAtUnderlying2200-Host->GetSimulationTick());
            Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();
            ReadyAt=FPlatformTime::Seconds()+8;Prepared=true;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        Hansa::Simulation::FHansaCalendarProjection Calendar;double Fraction=0;uint16 MinutesPerTick=0;
        Test->TestTrue(TEXT("Production exposes the presentation calendar"),Host->TryGetPresentationCalendar(Calendar,Fraction,MinutesPerTick));
        Test->TestEqual(TEXT("Production capture is displayed day five"),Calendar.ElapsedDays,int64(4));
        Test->TestEqual(TEXT("Underlying simulation advanced to 22:00"),Host->GetSimulationTick(),int64(4*24+22));
        Test->TestEqual(TEXT("Production presentation hour is locked to noon"),Calendar.HourOfDay,uint8(12));
        Test->TestEqual(TEXT("Production presentation minute is locked to zero"),Calendar.MinuteOfHour,uint8(0));
        Test->TestEqual(TEXT("Fractional presentation time is locked"),Fraction,0.0);
        Test->TestTrue(TEXT("HUD displays the same noon lock"),Hud->GetPresentationModel()->GetSnapshot().DateAndSeason.ToString().Contains(TEXT("12:00")));
        AHansaLubeckWorldFoundation* Foundation=nullptr;for(TActorIterator<AHansaLubeckWorldFoundation> It(World);It;++It){Foundation=*It;break;}
        ADirectionalLight* StandaloneSunActor=nullptr;for(TActorIterator<ADirectionalLight> It(World);It;++It){StandaloneSunActor=*It;break;}
        ASkyLight* StandaloneSkyActor=nullptr;for(TActorIterator<ASkyLight> It(World);It;++It){StandaloneSkyActor=*It;break;}
        UDirectionalLightComponent* ActiveSun=StandaloneSunActor?Cast<UDirectionalLightComponent>(StandaloneSunActor->GetLightComponent()):(Foundation?Foundation->SunLight.Get():nullptr);
        USkyLightComponent* ActiveSky=StandaloneSkyActor?StandaloneSkyActor->GetLightComponent():(Foundation?Foundation->SkyLight.Get():nullptr);
        AHansaGameState* GameState=World->GetGameState<AHansaGameState>();
        UPostProcessComponent* ActiveExposure=GameState?GameState->LightingExposure.Get():(Foundation?Foundation->Exposure.Get():nullptr);
        Test->TestNotNull(TEXT("Loaded map exposes an active sun"),ActiveSun);
        Test->TestNotNull(TEXT("Loaded map exposes an active skylight"),ActiveSky);
        Test->TestNotNull(TEXT("Loaded map exposes simulation exposure"),ActiveExposure);
        if(ActiveSun)
        {
            Test->TestTrue(TEXT("Direct sunlight remains in the softer Baltic midday band"),ActiveSun->Intensity>=14000.f&&ActiveSun->Intensity<=16000.f);
            Test->TestEqual(TEXT("Production contact shadows remain disabled"),ActiveSun->ContactShadowLength,0.f);
        }
        if(ActiveSky)Test->TestTrue(TEXT("Production skylight provides the stronger cool fill"),ActiveSky->Intensity>=1900.f&&ActiveSky->Intensity<=2100.f);
		const Hansa::Game::LubeckWorldArt::FHansaLightingState ExpectedLighting=
			Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar,Fraction,MinutesPerTick);
		Test->TestEqual(TEXT("Final camera consumes the continuous EV100 curve"),Camera->Camera->PostProcessSettings.AutoExposureMinBrightness,ExpectedLighting.ExposureEV100);
		Test->TestEqual(TEXT("Final camera uses deterministic manual exposure"),Camera->Camera->PostProcessSettings.AutoExposureMethod,AEM_Manual);
		Test->TestEqual(TEXT("Final camera keeps terrain compensation separate"),Camera->Camera->PostProcessSettings.AutoExposureBias,Hansa::Game::LubeckWorldArt::ExposureCompensationStops);
		Test->TestEqual(TEXT("Final camera disables local highlight exposure"),Camera->Camera->PostProcessSettings.LocalExposureHighlightContrastScale,1.f);
		Test->TestEqual(TEXT("Final camera disables local shadow exposure"),Camera->Camera->PostProcessSettings.LocalExposureShadowContrastScale,1.f);
		Test->TestEqual(TEXT("Final camera restrains SSAO"),Camera->Camera->PostProcessSettings.AmbientOcclusionIntensity,Hansa::Game::LubeckWorldArt::AmbientOcclusionIntensity);
		Test->TestEqual(TEXT("Final camera restrains Lumen AO"),Camera->Camera->PostProcessSettings.LumenAmbientOcclusionIntensity,Hansa::Game::LubeckWorldArt::LumenAmbientOcclusionIntensity);
        TArray<FColor> Pixels;FIntVector Size;
        if(!Viewport->GetGameViewportWidget()||!FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size)||Pixels.IsEmpty())
        {Test->AddError(TEXT("Production locked-midday screenshot failed"));return true;}
        int32 X=1920,Y=1080;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
        Test->TestTrue(TEXT("Production locked-midday capture remains native size"),Size.X==X&&Size.Y==Y);
		if(World->GetPackage()->GetName().Contains(TEXT("LubeckTerrain")))
		{
			double LuminanceSum=0.0;int32 SampleCount=0,NearWhiteCount=0;
			const int32 StartX=Size.X*220/1920,EndX=Size.X*1760/1920;
			const int32 StartY=Size.Y*140/1080,EndY=Size.Y*1000/1080;
			for(int32 PixelY=StartY;PixelY<EndY;PixelY+=4)for(int32 PixelX=StartX;PixelX<EndX;PixelX+=4)
			{
				const FColor& Pixel=Pixels[PixelY*Size.X+PixelX];
				const double Luminance=.2126*Pixel.R+.7152*Pixel.G+.0722*Pixel.B;
				LuminanceSum+=Luminance;NearWhiteCount+=Luminance>=242.0;++SampleCount;
			}
			const double MeanLuminance=SampleCount?LuminanceSum/SampleCount:0.0;
			const double NearWhiteFraction=SampleCount?double(NearWhiteCount)/SampleCount:1.0;
			Test->TestTrue(TEXT("Locked-midday terrain remains in the calibrated luminance band"),MeanLuminance>=90.0&&MeanLuminance<=170.0);
			Test->TestTrue(TEXT("Terrain capture avoids chalky near-white clipping"),NearWhiteFraction<.005);
		}
        const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P30/production-day5-locked-1200-%dx%d"),X,Y);
        if(ActiveSun&&ActiveSky&&ActiveExposure)
        {
            FString Diagnostics=FString::Printf(TEXT("map=%s owner=%s\ncamera=%s rotation=%s zoom=%.1f\ncameraPostProcessWeight=%.3f cameraMethod=%d cameraEV=%.3f cameraBias=%.3f\nsunVisible=%d sunLux=%.3f\nskyVisible=%d skyIntensity=%.3f source=%d cubemap=%s\nexposureEnabled=%d registered=%d active=%d priority=%.3f method=%d ev=%.3f bias=%.3f\n"),
                *World->GetPackage()->GetName(),StandaloneSunActor?TEXT("standalone-map-lights"):TEXT("foundation-components"),
                *Camera->GetActorLocation().ToString(),*Camera->GetActorRotation().ToString(),Camera->GetZoomDistance(),
				Camera->Camera->PostProcessBlendWeight,int32(Camera->Camera->PostProcessSettings.AutoExposureMethod),
				Camera->Camera->PostProcessSettings.AutoExposureMinBrightness,Camera->Camera->PostProcessSettings.AutoExposureBias,
                int32(ActiveSun->IsVisible()),ActiveSun->Intensity,
                int32(ActiveSky->IsVisible()),ActiveSky->Intensity,int32(ActiveSky->SourceType.GetValue()),ActiveSky->Cubemap?*ActiveSky->Cubemap->GetPathName():TEXT("none"),
                int32(ActiveExposure->bEnabled),int32(ActiveExposure->IsRegistered()),int32(ActiveExposure->IsActive()),
                ActiveExposure->Priority,int32(ActiveExposure->Settings.AutoExposureMethod),
                ActiveExposure->Settings.AutoExposureMinBrightness,ActiveExposure->Settings.AutoExposureBias);
            for(TActorIterator<APostProcessVolume> It(World);It;++It)
                Diagnostics+=FString::Printf(TEXT("postProcessVolume=%s enabled=%d unbound=%d priority=%.3f blendWeight=%.3f minOverride=%d maxOverride=%d min=%.3f max=%.3f biasOverride=%d bias=%.3f\n"),
                    *It->GetName(),int32(It->bEnabled),int32(It->bUnbound),It->Priority,It->BlendWeight,
                    int32(It->Settings.bOverride_AutoExposureMinBrightness),int32(It->Settings.bOverride_AutoExposureMaxBrightness),
                    It->Settings.AutoExposureMinBrightness,It->Settings.AutoExposureMaxBrightness,
                    int32(It->Settings.bOverride_AutoExposureBias),It->Settings.AutoExposureBias);
            FFileHelper::SaveStringToFile(Diagnostics,*(Base+TEXT("-diagnostics.txt")));
        }
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Production lighting capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        return true;
    }
private:
    FAutomationTestBase* Test;double Start=0,ReadyAt=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckProductionLockedMiddayViewport,"Hansa.World.LubeckArt.ProductionLockedMiddayViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FLubeckProductionLockedMiddayViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FLubeckProductionLockedMiddayCapture(this));return true;}
#endif
