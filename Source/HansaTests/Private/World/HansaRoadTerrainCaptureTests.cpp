#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaRoadSplineComponent.h"
#include "World/HansaRoadPresentation.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Components/ChildActorComponent.h"
#include "HAL/IConsoleManager.h"
#include "ShaderCompiler.h"
#include "../UI/HansaFrontendCaptureSupport.h"

namespace
{
class FRoadTerrainCapture : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(), Ready=0;
    int32 Stage=0;
    bool Prepared=false;
    TArray<uint8> Save;
    uint64 Fingerprint=0;
    FIntPoint Origin;
public:
    explicit FRoadTerrainCapture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>300){Test->AddError(TEXT("Road viewport capture timed out"));return true;}
        if(!GEngine || !GEngine->GameViewport)return false;
        auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();auto* C=W?W->GetFirstPlayerController():nullptr;
        auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
        if(!Hud || !Hud->GetRootWidget() || HansaWaitForFrontend(Hud))return false;
        auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());
        auto* Host=Mode?Mode->GetSimulationHost():nullptr;auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn());
        if(!Host || !Camera)return false;
        Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
        auto* Model=Hud->GetBuildMenuPresentationModel();
        if(!Prepared)
        {
            if(Stage==0)
            {
                Hud->GetScenarioPresentationModel()->AcknowledgeBriefing();Hud->GetScenarioPresentationModel()->DismissHelp();
                Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
                TSet<FIntPoint> Occupied;
                for(const auto& Building:Host->BuildProjection().Value.GetBuildingWorldProjections())
                    for(const auto Cell:Building.OccupiedCells)Occupied.Add({Cell.X,Cell.Y});
                bool Found=false;double BestRange=-1;
                for(int32 Y=10;Y<34;++Y)for(int32 X=10;X<32;++X)
                {
                    bool Free=true;
                    for(int32 DY=0;DY<7;++DY)for(int32 DX=0;DX<7;++DX)
                    {
                        const FVector P=Hansa::Game::LubeckPlacementGrid::GridToWorld({X+DX,Y+DY});
                        Free &= !Occupied.Contains({X+DX,Y+DY}) && Hansa::Game::LubeckPlacementGrid::TerrainAt(FVector2D(P))==Hansa::Simulation::EHansaPlacementTerrain::Land;
                    }
                    if(Free)
                    {
                        double Low=DBL_MAX,High=-DBL_MAX;
                        for(int32 DY:{0,3,6})for(int32 DX:{0,3,6})
                        {
                            const FVector P=Hansa::Game::LubeckPlacementGrid::GridToWorld({X+DX,Y+DY});
                            FHitResult Hit;
                            if(Hansa::Game::TerrainPlacement::Trace(W,P+FVector(0,0,1000000),P-FVector(0,0,1000000),Hit))
                            {Low=FMath::Min(Low,Hit.ImpactPoint.Z);High=FMath::Max(High,Hit.ImpactPoint.Z);}
                        }
                        if(High-Low>BestRange){BestRange=High-Low;Origin={X,Y};Found=true;}
                    }
                }
                if(!Found){Test->AddError(TEXT("No free seven-cell road acceptance area"));return true;}
                auto Draw=[&](FIntPoint A,FIntPoint B)
                {
                    A+=Origin;B+=Origin;
                    const bool Success=Model->SelectBuilding(TEXT("Building.Road")) && Model->BeginRoadDraw(A.X,A.Y) &&
                        Model->UpdateRoadDraw(B.X,B.Y) && Model->EndRoadDraw(true);
                    Test->TestTrue(FString::Printf(TEXT("Ordinary road drawing commits: %s"),*Model->GetSnapshot().LastResult.ToString()),Success);
                };
                Draw({0,3},{6,3});Draw({3,0},{3,6});Draw({6,3},{6,6});Draw({4,6},{6,6});Draw({0,6},{0,6});
                Model->CancelIntent();Model->SetOpen(false);Host->SynchronizeWorldProjection();
                Fingerprint=Host->BuildProjection().Value.GetFingerprint().Value;
                Test->TestTrue(TEXT("Road capture save"),Host->CaptureSaveBytes(Save,TEXT("Road terrain acceptance"),TEXT("2026-09-11T12:00:00Z")).IsSuccess());
            }
            Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->MinimumZoomDistance=1000;
            AHansaLubeckWorldFoundation* Foundation=nullptr;
            for(TActorIterator<AHansaLubeckWorldFoundation> I(W);I;++I){Foundation=*I;break;}
            if(!Foundation){Test->AddError(TEXT("Game foundation unavailable"));return true;}
            const FVector Focus=Foundation->PlacementCellToWorld(Origin.X+3,Origin.Y+3);
            Camera->FocusWorldLocationIntent(Focus);
            Camera->SetActorLocation(FVector(Camera->GetActorLocation().X,Camera->GetActorLocation().Y,Focus.Z));
            const float Zoom=Stage==2?6500.f:3500.f;
            Camera->AddZoomIntent((Camera->GetZoomDistance()-Zoom)/Camera->ZoomUnitsPerStep);
            if(Stage==3)
            {
                // Removing the under-construction road uses the existing authoritative command.
                const auto P=Host->BuildProjection();
                for(const auto& B:P.Value.GetBuildingWorldProjections())
                    if(B.Placement.BuildingDefinitionId.ToString()==TEXT("Building.Road") && B.Placement.Anchor.X==Origin.X+3 && B.Placement.Anchor.Y==Origin.Y+3)
                        Test->TestTrue(TEXT("Remove junction through command"),(B.Status==Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction?Host->CancelConstruction(B.BuildingId):Host->RemoveBuilding(B.BuildingId)).IsSuccess());
                Host->SynchronizeWorldProjection();
            }
            if(Stage==4)
            {
                Test->TestTrue(TEXT("Restore road save"),Host->RestoreSaveBytes(Save).IsSuccess());
                Test->TestEqual(TEXT("Save preserves authoritative checksum"),Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
                Host->SynchronizeWorldProjection();
            }
            if(Stage==5)IConsoleManager::Get().FindConsoleVariable(TEXT("hansa.Road.RVT"))->Set(0);
            FString Evidence;double TotalFitMs=0;int32 FittedRoads=0;
            for(TActorIterator<AHansaRoadPresentation> I(W);I;++I)
                if(auto* Spline=Cast<UHansaRoadSplineComponent>(I->Surface))
                {
                    if(Stage==0)
                    {
                        // Recreate the previous rigid/single-normal presentation in memory only.
                        Spline->InvalidateRoadRVT();Spline->RuntimeVirtualTextures.Reset();Spline->SetRenderInMainPass(true);
                        for(USceneComponent* Child:Spline->GetAttachChildren())Child->SetVisibility(false,true);
                        Spline->SetStartAndEnd(FVector(-200,0,0),FVector(400,0,0),FVector(200,0,0),FVector(400,0,0));
                        auto* RigidMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain.M_Road_Terrain")),Spline);
                        RigidMaterial->SetScalarParameterValue(TEXT("RoadFitEnabled"),0);
                        RigidMaterial->SetScalarParameterValue(TEXT("GroundEnabled"),0);
                        Spline->SetMaterial(0,RigidMaterial);
                        Spline->SetWorldRotation(Hansa::Game::TerrainPlacement::RoadRotation(W,Spline->GetComponentLocation(),Spline->GetComponentQuat()));
                        Spline->MarkRenderStateDirty();
                    }
                    else
                    {
                        FRotator Rotation=Spline->GetComponentRotation();Rotation.Pitch=0;Rotation.Roll=0;Spline->SetWorldRotation(Rotation);
                        const bool Fitted=Spline->FitTerrain(3,false,true);
                        Test->TestTrue(TEXT("Actual game Landscape supports complete road fitting"),Fitted);
                        ++FittedRoads;TotalFitMs+=Spline->LastFitMilliseconds;
                        TArray<UStaticMeshComponent*> Components;I->GetComponents(Components);
                        bool Writes=false;for(auto* Component:Components)Writes|=!Component->RuntimeVirtualTextures.IsEmpty();
                        Test->TestEqual(TEXT("RVT writes match enabled/disabled mode"),Writes,Stage!=5);
                        Evidence+=FString::Printf(TEXT("%s fitted=%d sections=%d traces=%d fitMs=%.3f diagnostic=%s\n"),*I->GetName(),int(Fitted),Spline->GetSectionCount(),Spline->LastTraceCount,Spline->LastFitMilliseconds,*Spline->FitDiagnostic);
                    }
                }
            Evidence+=FString::Printf(TEXT("origin=%d,%d roads=%d fullNetworkResampleMs=%.3f meanFitMs=%.3f\n"),Origin.X,Origin.Y,FittedRoads,TotalFitMs,FittedRoads?TotalFitMs/FittedRoads:0);
            FFileHelper::SaveStringToFile(Evidence,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("RoadTerrain/stage-%d.txt"),Stage)));
            Ready=FPlatformTime::Seconds()+5;Prepared=true;return false;
        }
        if(FPlatformTime::Seconds()<Ready || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()))return false;
        TArray<FColor> Pixels;FIntVector Size;
        if(!V->GetGameViewportWidget() || !FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size))
        {Test->AddError(TEXT("Game viewport screenshot unavailable"));return true;}
        const FIntPoint Extent=V->Viewport->GetSizeXY();
        Test->TestTrue(TEXT("Native screenshot dimensions"),Extent.X==Size.X && Extent.Y==Size.Y);
        const TCHAR* Names[]={TEXT("before"),TEXT("after-close"),TEXT("after-gameplay"),TEXT("removed-junction"),TEXT("restored"),TEXT("rvt-disabled")};
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
        Test->TestTrue(TEXT("Save native viewport evidence"),FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("RoadTerrain/%s-%dx%d.png"),Names[Stage],Size.X,Size.Y))));
        ++Stage;Prepared=false;
        if(Stage==6){IConsoleManager::Get().FindConsoleVariable(TEXT("hansa.Road.RVT"))->Set(1);return true;}
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadTerrainViewportTest,"Hansa.World.RoadTerrain.RealViewport",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaRoadTerrainViewportTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FRoadTerrainCapture(this));return true;
}
#endif

