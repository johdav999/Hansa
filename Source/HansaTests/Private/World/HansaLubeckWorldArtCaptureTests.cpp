#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ImageUtils.h"
#include "RenderTimer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaGameMode.h"
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
#endif
