#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaLandOverlay.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLandOverlayRenderer.h"
#include "World/HansaTerrainPlacement.h"
#include "ProceduralMeshComponent.h"
#include "../UI/HansaFrontendCaptureSupport.h"

namespace
{
class FLandUiViewport final : public IAutomationLatentCommand
{
public:
    explicit FLandUiViewport(FAutomationTestBase* In):Test(In),Start(FPlatformTime::Seconds()){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Land UI viewport timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* Viewport=GEngine->GameViewport.Get();auto* World=Viewport->GetWorld();
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Hud=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;
        auto* Mode=World?World->GetAuthGameMode<AHansaGameMode>():nullptr;
        auto* Host=Mode?Mode->GetSimulationHost():nullptr;
        auto* Camera=PC?Cast<AHansaStrategyCameraPawn>(PC->GetPawn()):nullptr;
        auto Root=Hud?Hud->GetRootWidget():nullptr;
        if(!Root||!Host||!Camera||HansaWaitForFrontend(Hud))return false;
        auto State=Root->GetLandState();
        if(!State){Test->AddError(TEXT("Land UI state missing"));return true;}
        if(!Initialized)
        {
            Hud->GetScenarioPresentationModel()->AcknowledgeBriefing();
            Hud->GetScenarioPresentationModel()->DismissHelp();
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
            Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();
            Fingerprint=Host->BuildProjection().Value.GetFingerprint().Value;
            Initialized=true;
        }
        const bool SurveyEvidence=FParse::Param(FCommandLine::Get(),TEXT("LandOverlaySurveyCapture"));
        if(!Prepared)
        {
            if(Stage==1)
            {
                Test->TestTrue(TEXT("Land rail activates"),Root->ActivateSemanticId(TEXT("HUD.Minimap.Land")));
                Test->TestTrue(TEXT("First open is buildable"),State->GetMode()==Hansa::Game::LandOverlay::EMode::Buildable);
                Test->TestTrue(TEXT("Land panel visible"),State->IsPanelOpen());
            }
            if(Stage==2)
            {
                Test->TestTrue(TEXT("Ownership tab activates"),Root->ActivateSemanticId(TEXT("LandOverlay.Mode.Ownership")));
                Test->TestTrue(TEXT("Owner is distinct mode"),State->GetMode()==Hansa::Game::LandOverlay::EMode::Ownership);
            }
            if(Stage==3)
            {
                AHansaLubeckWorldFoundation* Foundation=nullptr;
                for(TActorIterator<AHansaLubeckWorldFoundation> It(World);It;++It){Foundation=*It;break;}
                if(!Foundation){Test->AddError(TEXT("Land foundation missing"));return true;}
                FIntPoint Center;Foundation->WorldToPlacementCell(FVector(Camera->GetFocusLocation2D(),0),Center.X,Center.Y);
                FTransform Grid;
                AHansaLandOverlayRenderer::MakeGridTransform(Host->GetCityId(),*Foundation,Grid);
                const auto Query=Host->QueryLand(Host->GetHouseId(),Host->GetCityId(),
                    {Center.X-16,Center.Y-16},{Center.X+16,Center.Y+16});
                int32 Best=MAX_int32;
                FHitResult Chosen;
                for(const auto& Cell:Query.Cells)
                {
                    if(!Cell.bSurveyKnown||Cell.Terrain==Hansa::Simulation::EHansaPlacementTerrain::Water||Cell.OccupyingBuildingId.IsValid())continue;
                    const FIntPoint P(Cell.Coordinate.X,Cell.Coordinate.Y),D=P-Center;
                    const int32 Distance=D.X*D.X+D.Y*D.Y;
                    if(Distance>=Best)continue;
                    const FVector XY=Grid.TransformPosition(FVector(P.X+.5,P.Y+.5,0));
                    FHitResult Hit;
                    if(!Hansa::Game::TerrainPlacement::Trace(World,XY+FVector(0,0,1000000),XY-FVector(0,0,1000000),Hit))continue;
                    Best=Distance;Chosen=Hit;
                }
                Test->TestTrue(TEXT("Actual land selection"),Best<MAX_int32 && Root->SelectLandWorldHit(Chosen));
                Test->TestTrue(TEXT("Selected owner available"),State->HasSelection()&&State->GetSelectedView().RecordedOwnerId.IsValid());
                Test->TestTrue(TEXT("Selection retains exact access"),!State->GetAccessText().IsEmpty());
                Test->TestTrue(TEXT("Lubeck selection keeps another house as recorded owner"),State->HasSelection()&&State->GetSelectedView().RecordedOwnerId!=Host->GetHouseId());
                Test->TestTrue(TEXT("Cross-owner Lubeck construction is permitted"),State->GetSelectedView().Access==Hansa::Simulation::EHansaLandAccess::Permitted);
            }
            if(SurveyEvidence && Stage==4)
            {
                SelectedRegion=State->GetSelectedRegion();
                Test->TestTrue(TEXT("Selection is an entire ownership region"),SelectedRegion>0 && State->GetSurvey().RegionBounds(SelectedRegion).GetArea()>1);
                Test->TestTrue(TEXT("Frame territory action"),Root->ActivateSemanticId(TEXT("LandOverlay.Frame")));
            }
            if(SurveyEvidence && Stage==5)
            {
                auto View=Camera->GetViewState();View.ZoomDistance*=2.2f;View.YawDegrees+=35;
                Camera->RestoreViewState(View);
            }
            if(SurveyEvidence && Stage==6)
                Camera->FocusWorldLocationIntent(FVector(Camera->GetFocusLocation2D()+FVector2D(4000,-3000),0));
            State->Update();
            Ready=FPlatformTime::Seconds()+3;Prepared=true;return false;
        }
        if(FPlatformTime::Seconds()<Ready)return false;
        if(Stage>0)for(TActorIterator<AHansaLandOverlayRenderer> It(World);It;++It)
            if(It->GetStats().PendingChunks>0)return false;
        if(Stage==3)
        {
            bool HasBrassFootprint=false;
            for(TActorIterator<AHansaLandOverlayRenderer> It(World);It;++It)
            {
                TArray<UProceduralMeshComponent*> Meshes;It->GetComponents(Meshes);
                for(auto* Mesh:Meshes)if(auto* Section=Mesh->GetProcMeshSection(3))HasBrassFootprint|=!Section->ProcVertexBuffer.IsEmpty();
            }
            Test->TestTrue(TEXT("Selected territory has visible priority geometry"),HasBrassFootprint);
        }
        if(SurveyEvidence && Stage>=4)
        {
            Test->TestEqual(TEXT("Territory identity survives frame, zoom, rotation and pan"),State->GetSelectedRegion(),SelectedRegion);
            Test->TestTrue(TEXT("Ground footprint follows viewport"),State->GetCameraFootprint().Num()==8);
            for(TActorIterator<AHansaLandOverlayRenderer> It(World);It;++It)
                Test->TestTrue(TEXT("Camera coverage has bounded geometry"),It->GetStats().ActiveChunks>0 && It->GetStats().ActiveChunks<=64);
        }
        const auto Nodes=Root->GetSemanticSnapshot();
        auto Find=[&](const TCHAR* Id){return Nodes.FindByPredicate([&](const auto& N){return N.Id==Id;});};
        Test->TestTrue(TEXT("Land semantic action"),Find(TEXT("HUD.Minimap.Land"))!=nullptr);
        if(Stage>0)Test->TestTrue(TEXT("Mode semantic tab visible"),Find(TEXT("LandOverlay.Mode.Buildable"))&&Find(TEXT("LandOverlay.Mode.Buildable"))->State.bVisible);
        if(Stage==3)Test->TestTrue(TEXT("Selected owner semantic value"),Find(TEXT("LandOverlay.Owner"))&&Find(TEXT("LandOverlay.Owner"))->State.bVisible&&!Find(TEXT("LandOverlay.Owner"))->State.Value.IsEmpty());
        Test->TestEqual(TEXT("Land UI preserves simulation"),Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
        TArray<FColor> Pixels;FIntVector Size;
        if(!Viewport->GetGameViewportWidget()||!FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size))
        {Test->AddError(TEXT("Land UI capture failed"));return true;}
        Test->TestTrue(TEXT("Native screenshot"),Viewport->Viewport->GetSizeXY()==FIntPoint(Size.X,Size.Y));
        const TCHAR* Names[]={TEXT("off"),TEXT("buildable"),TEXT("ownership"),TEXT("selected"),TEXT("framed"),TEXT("distant-rotated"),TEXT("panned")};
        const TCHAR* Folder=FParse::Param(FCommandLine::Get(),TEXT("LandOverlaySurveyCapture"))?TEXT("SurveyImplementation"):
            FParse::Param(FCommandLine::Get(),TEXT("LandOverlayRibbonCapture"))?TEXT("RibbonImplementation"):TEXT("Implementation");
        const FString Base=FPaths::ProjectDir()/FString::Printf(TEXT("Docs/Images/UI/LandOverlay/%s/ui-%s--%dx%d"),Folder,Names[Stage],Size.X,Size.Y);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Base),true);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Save real HUD capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        if(SurveyEvidence)
        {
            FHansaLandOverlayStats Stats;
            for(TActorIterator<AHansaLandOverlayRenderer> It(World);It;++It)Stats=It->GetStats();
            const FString Metrics=FString::Printf(TEXT("viewport=%dx%d\nchunks=%d\npending=%d\ntriangles=%d\nmissingTerrainSamples=%d\nsurveyRuns=%d\nsurveyPages=%d\nselectedRegion=%d\n"),
                Size.X,Size.Y,Stats.ActiveChunks,Stats.PendingChunks,Stats.Triangles,Stats.MissingTerrainSamples,
                State->GetSurvey().Get().Cells.Num(),State->GetSurvey().Get().SurveyPages,State->GetSelectedRegion());
            FFileHelper::SaveStringToFile(Metrics,*(Base+TEXT(".txt")));
        }
        if(Stage==(SurveyEvidence?6:3))
        {
            const FIntPoint PlacementCell(State->GetSelectedView().Coordinate.X,State->GetSelectedView().Coordinate.Y);
            Test->TestTrue(TEXT("Close selected land"),Root->ActivateSemanticId(TEXT("LandOverlay.Selection.Close"))&&!State->HasSelection());
            Test->TestTrue(TEXT("Close panel keeps overlay mode"),Root->ActivateSemanticId(TEXT("LandOverlay.Close"))&&!State->IsPanelOpen()&&State->GetMode()==Hansa::Game::LandOverlay::EMode::Ownership);
            Test->TestTrue(TEXT("Land action reopens panel"),Root->ActivateSemanticId(TEXT("HUD.Minimap.Land"))&&State->IsPanelOpen());
            Root->ActivateSemanticId(TEXT("HUD.Minimap.Land"));
            Test->TestTrue(TEXT("Explicit off state"),!State->IsActive());
            Root->ActivateSemanticId(TEXT("HUD.Minimap.Land"));
            Test->TestTrue(TEXT("Session remembers ownership mode"),State->GetMode()==Hansa::Game::LandOverlay::EMode::Ownership);
            State->SetMode(Hansa::Game::LandOverlay::EMode::Off);
            auto* Build=Hud->GetBuildMenuPresentationModel();
            Test->TestNotNull(TEXT("Build presenter for placement assistance"),Build);
            if(Build && Build->SelectBuilding(TEXT("Building.Road")))
            {
                Test->TestTrue(TEXT("Road target accepted for preview"),Build->TargetGridCell(PlacementCell.X,PlacementCell.Y));
                State->Update();
                Test->TestTrue(TEXT("Explicit Off remains Off during placement"),State->GetMode()==Hansa::Game::LandOverlay::EMode::Off);
                Test->TestTrue(TEXT("Target shows local Buildable assistance"),State->IsPlacementAssistanceActive()&&State->GetRenderedMode()==Hansa::Game::LandOverlay::EMode::Buildable&&State->GetPlacementAssistanceCell().Get(FIntPoint::ZeroValue)==PlacementCell);
                for(TActorIterator<AHansaLandOverlayRenderer> It(World);It;++It)
                    Test->TestEqual(TEXT("Placement uses one nearby chunk"),It->GetStats().ActiveChunks,1);
                Test->TestTrue(TEXT("Road stroke begins"),Build->BeginRoadDraw(PlacementCell.X,PlacementCell.Y));
                Test->TestTrue(TEXT("Road stroke follows endpoint"),Build->UpdateRoadDraw(PlacementCell.X+2,PlacementCell.Y));
                State->Update();
                Test->TestTrue(TEXT("Permission assistance follows road stroke"),State->GetPlacementAssistanceCell().Get(FIntPoint::ZeroValue)==FIntPoint(PlacementCell.X+2,PlacementCell.Y));
                Build->EndRoadDraw(false);
                State->Update();
                Test->TestFalse(TEXT("Cancelled stroke restores Off"),State->IsPlacementAssistanceActive());
                Test->TestTrue(TEXT("Road retarget"),Build->TargetGridCell(PlacementCell.X,PlacementCell.Y));
                Test->TestTrue(TEXT("Rotation keeps target preview"),Build->RotateIntent());
                State->Update();
                Test->TestTrue(TEXT("Rotated target keeps nearby rights"),State->IsPlacementAssistanceActive());
                Test->TestTrue(TEXT("Repeat toggle"),Build->ToggleRepeatIntent());
                State->Update();
                Test->TestFalse(TEXT("Repeat toggle clears old target"),State->IsPlacementAssistanceActive());
                Test->TestTrue(TEXT("Repeat retarget"),Build->TargetGridCell(PlacementCell.X,PlacementCell.Y));
                State->SetMode(Hansa::Game::LandOverlay::EMode::Ownership);
                Test->TestTrue(TEXT("Ownership choice persists under temporary permission view"),State->GetMode()==Hansa::Game::LandOverlay::EMode::Ownership&&State->GetRenderedMode()==Hansa::Game::LandOverlay::EMode::Buildable);
                Build->CancelIntent();
                State->Update();
                Test->TestTrue(TEXT("Cancel restores Ownership without inspector"),!State->IsPlacementAssistanceActive()&&State->GetRenderedMode()==Hansa::Game::LandOverlay::EMode::Ownership&&!State->HasSelection());
                Build->SetPlacementCity(TEXT("City.Rostock"));
                State->Update();
                Test->TestFalse(TEXT("Different placement city cannot show home assistance"),State->IsPlacementAssistanceActive());
                Build->SetPlacementCity(TEXT("City.Lubeck"));
                State->SetMode(Hansa::Game::LandOverlay::EMode::Off);
            }
            else Test->AddError(TEXT("Road card unavailable for placement integration"));
        }
        ++Stage;Prepared=false;
        return Stage==(SurveyEvidence?7:4);
    }
private:
    FAutomationTestBase* Test;
    double Start,Ready=0;
    uint64 Fingerprint=0;
    int32 Stage=0,SelectedRegion=0;
    bool Initialized=false,Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandOverlayUiViewport,"Hansa.UI.LandOverlay.RealViewport",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaLandOverlayUiViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FLandUiViewport(this));return true;}
#endif
