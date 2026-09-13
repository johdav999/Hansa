#if WITH_DEV_AUTOMATION_TESTS
#include "../UI/HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "RenderTimer.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaRostockQuarter.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Landscape.h"
#include "StaticMeshResources.h"
namespace {
class FRostockCapture:public IAutomationLatentCommand
{
public:
    explicit FRostockCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
    bool Update()override
    {
        using namespace Hansa::Simulation;
        if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Rostock native flow timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();auto* C=W?W->GetFirstPlayerController():nullptr;
        auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
        auto* Host=Cast<AHansaGameMode>(W->GetAuthGameMode())->GetSimulationHost();auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn());auto Root=H->GetRootWidget();auto* City=H->GetCityOverviewPresentationModel();auto* Trade=H->GetTradeMapPresentationModel();
        auto Press=[&](const TCHAR* Id){if(!Test->TestTrue(FString::Printf(TEXT("Focus %s"),Id),Root->FocusSemanticId(Id)))return;FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));};
        if(!Prepared)
        {
            switch(Stage)
            {
            case 0: {float Scale=1.f;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});} H->GetScenarioPresentationModel()->AcknowledgeBriefing();H->GetScenarioPresentationModel()->DismissHelp();H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Camera->bEnableMouseEdgePan=false;Home=Camera->GetViewState();Initial=Host->BuildProjection().Value.GetFingerprint().Value;break;
            case 1: Press(TEXT("HUD.TopStatus.CityOverview"));Press(TEXT("CityOverview.City.Rostock"));Press(TEXT("CityOverview.Visit"));break;
            case 2: Test->TestFalse(TEXT("Remote card drag denied"),H->GetBuildMenuPresentationModel()->BeginCardDrag(TEXT("Building.Residence.Laborer")));Test->TestFalse(TEXT("Remote direct placement denied"),H->GetBuildMenuPresentationModel()->ConfirmIntent());Camera->AddZoomIntent((Camera->GetZoomDistance()-2500)/Camera->ZoomUnitsPerStep);Camera->FocusWorldLocationIntent(AHansaRostockQuarter::VisitOffset()+FVector(-700,2300,100));break;
            case 3: Test->TestTrue(TEXT("Typed remote market inspection"),H->InspectRostockRole(TEXT("Market")));break;
            case 4:
                City->CloseIntent();Camera->AddZoomIntent((Camera->GetZoomDistance()-6500)/Camera->ZoomUnitsPerStep);Camera->FocusWorldLocationIntent(AHansaRostockQuarter::VisitOffset()+FVector(0,1800,100));
                Press(TEXT("HUD.TopStatus.TradeMap"));Press(TEXT("TradeMap.New"));Press(TEXT("TradeMap.Creator.Cog"));Press(TEXT("TradeMap.Stop.0"));Press(TEXT("TradeMap.Editor.Reserve.Decrease"));Press(TEXT("TradeMap.Creator.Review"));Press(TEXT("TradeMap.Creator.Activate"));RouteId=Trade->GetSnapshot().SelectedRouteValue;Test->TestTrue(TEXT("Normal route created"),RouteId>3);Press(TEXT("TradeMap.Close"));break;
            case 5: break;
            case 6: break;
            case 7: SavedFingerprint=Host->BuildProjection().Value.GetFingerprint().Value;Host->CaptureSaveBytes(Save,TEXT("Rostock visit"),TEXT("2026-09-09T00:00:00Z"));Host->AdvanceTicks(2);Test->TestTrue(TEXT("Restore remote session"),bool(Host->RestoreSaveBytes(Save)));Test->TestEqual(TEXT("Authority restored"),Host->BuildProjection().Value.GetFingerprint().Value,SavedFingerprint);break;
            case 8: Press(TEXT("HUD.TopStatus.ReturnCity"));break;
            }
            Prepared=true;Ready=FPlatformTime::Seconds()+2;Samples=0;GameMs=RenderMs=0;return false;
        }
        if(Stage==1&&H->GetViewedCity()!=TEXT("City.Rostock")){if(!H->IsCityVisitLoading()){Test->AddError(H->GetCityVisitStatus().ToString());return true;}Ready=FPlatformTime::Seconds()+3;return false;}
        if(Stage==4||Stage==5||Stage==6)
        {
            const auto P=Host->BuildProjection();const auto* R=P.Value.GetRoutes().FindByPredicate([this](const auto& X){return X.Id.GetValue()==RouteId;});
            const bool Reached=R&&(Stage==4?(R->Lifecycle==EHansaRouteLifecycleState::Traveling&&R->Stops.IsValidIndex(R->NextStopIndex)&&R->Stops[R->NextStopIndex].CityId.ToString()==TEXT("City.Rostock")&&R->RemainingTravelTicks<=FMath::Max(1,R->TotalTravelTicks/4)):Stage==5?(R->Lifecycle!=EHansaRouteLifecycleState::Traveling&&R->Stops[R->CurrentStopIndex].CityId.ToString()==TEXT("City.Rostock")):(R->LastTransfer.Kind==EHansaRouteCargoActionKind::Unload&&R->LastTransfer.CityId.ToString()==TEXT("City.Rostock")&&R->LastTransfer.AppliedQuantity.GetRawValue()>0));
            if(!Reached){if(++Steps>600){Test->AddError(FString::Printf(TEXT("No authoritative approach/delivery within bound: stage %d route %lld state %s"),Stage,RouteId,*Trade->GetSnapshot().EditorStatus.ToString()));return true;}Host->AdvanceTicks(1);Ready=FPlatformTime::Seconds()+2;return false;}
        }
        if(FPlatformTime::Seconds()<Ready){++Samples;GameMs+=FPlatformTime::ToMilliseconds(GGameThreadTime);RenderMs+=FPlatformTime::ToMilliseconds(GRenderThreadTime);return false;}
        if(Stage<=3)Test->TestEqual(TEXT("Travel/inspection do not mutate authority"),Host->BuildProjection().Value.GetFingerprint().Value,Initial);
        if(Stage>0&&Stage<8){Test->TestEqual(TEXT("Remote view identity"),H->GetViewedCity(),FName(TEXT("City.Rostock")));for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("BuildMenu.")))Test->TestFalse(TEXT("No exposed remote construction"),N.State.bVisible);}
        if(Stage==4||Stage==5)
        {
            const auto P=Host->BuildProjection();const auto* Route=P.Value.GetRoutes().FindByPredicate([this](const auto& R){return R.Id.GetValue()==RouteId;});int RemoteVessels=0;
            for(TActorIterator<AHansaCargoVehiclePresentation> I(W);I;++I)if(I->Tags.Contains(TEXT("City.Rostock"))&&!I->IsHidden())
            {++RemoteVessels;Test->TestTrue(TEXT("Rendered Cog keeps authoritative vehicle identity"),Route&&I->GetVehicleId()==Route->VehicleId);FHitResult Hit;const FVector At=I->GetActorLocation()+FVector(0,0,500);Test->TestTrue(TEXT("Rendered Cog has a selectable target"),W->LineTraceSingleByChannel(Hit,At+FVector(0,0,5000),At,ECC_Visibility)&&Hit.GetActor()==*I);}
            Test->TestEqual(TEXT("One real arriving vessel"),RemoteVessels,1);
        }
        if(Stage==8){Test->TestTrue(TEXT("Construction restored at home"),H->GetBuildMenuPresentationModel()->IsConstructionAllowed());Test->TestFalse(TEXT("Home civic values restored while paused"),H->GetPresentationModel()->GetSnapshot().Workforce.ToString().Contains(TEXT("No construction")));Test->TestTrue(TEXT("Home camera focus restored"),Camera->GetFocusLocation2D().Equals(Home.Focus,.1));Test->TestEqual(TEXT("Home zoom restored"),Camera->GetZoomDistance(),Home.ZoomDistance);}
        TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native screenshot failed"));return true;}
        int X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);Test->TestTrue(TEXT("Original pixel dimensions"),Size.X==X&&Size.Y==Y);
        if(Stage>0&&Stage<8)for(FVector2D Corner:{FVector2D(1,1),FVector2D(X-2,1),FVector2D(1,Y-2),FVector2D(X-2,Y-2)})
        {FVector Origin,Direction;FHitResult Hit;Test->TestTrue(TEXT("Native terrain covers viewport corners"),C->DeprojectScreenPositionToWorld(Corner.X,Corner.Y,Origin,Direction)&&W->LineTraceSingleByChannel(Hit,Origin,Origin+Direction*100000,ECC_Visibility));}
        const TCHAR* Names[]={TEXT("home"),TEXT("visited"),TEXT("waterfront"),TEXT("market"),TEXT("approaching"),TEXT("berthed"),TEXT("delivered"),TEXT("restored"),TEXT("returned")};
        const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P31/rostock-%dx%d-%s"),X,Y,Names[Stage]);for(FColor& Pixel:Pixels)Pixel.A=255;TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(X,Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png")));
        int Actors=0,Instances=0,EngineShapes=0;int64 Triangles=0;FString MeshRows=TEXT("actor\tmesh\tinstances\tlods\tlod0Triangles\n");
        for(TActorIterator<AActor> I(W);I;++I)if(I->GetActorLocation().X>40000&&!I->IsHidden())
        {++Actors;TArray<UStaticMeshComponent*> Comps;I->GetComponents(Comps);for(auto* Comp:Comps)if(Comp->IsVisible()&&Comp->GetStaticMesh()){UStaticMesh* M=Comp->GetStaticMesh();auto* ISM=Cast<UInstancedStaticMeshComponent>(Comp);int Count=ISM?ISM->GetInstanceCount():1;Instances+=Count;const auto* R=M->GetRenderData();int Tris=R&&!R->LODResources.IsEmpty()?R->LODResources[0].GetNumTriangles():0;Triangles+=int64(Tris)*Count;EngineShapes+=M->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"));MeshRows+=FString::Printf(TEXT("%s\t%s\t%d\t%d\t%d\n"),*I->GetName(),*M->GetPathName(),Count,M->GetNumLODs(),Tris);}}
        if(Stage>0&&Stage<8)Test->TestEqual(TEXT("No remote Engine basic shapes"),EngineShapes,0);
        FFileHelper::SaveStringToFile(MeshRows,*(Base+TEXT(".meshes.tsv")));
        FString Evidence=FString::Printf(TEXT("city=%s\nstate=%s\ntick=%lld\nfingerprint=%llu\nrouteId=%lld\nremoteActors=%d\nmeshInstances=%d\nlod0UpperBoundTriangles=%lld\nengineShapes=%d\ngameThreadMeanMs=%.3f\nrenderThreadMeanMs=%.3f\narrival=%s\n"),*H->GetViewedCity().ToString(),Names[Stage],Host->GetSimulationTick(),Host->BuildProjection().Value.GetFingerprint().Value,RouteId,Actors,Instances,Triangles,EngineShapes,Samples?GameMs/Samples:0,Samples?RenderMs/Samples:0,*H->GetRostockArrivalSummary().ToString());
        const auto P=Host->BuildProjection();if(const auto* R=P.Value.GetRoutes().FindByPredicate([this](const auto& It){return It.Id.GetValue()==RouteId;}))Evidence+=FString::Printf(TEXT("transferCity=%s\ntransferQuantity=%lld\n"),*R->LastTransfer.CityId.ToString(),R->LastTransfer.AppliedQuantity.GetRawValue());
        if(Stage>0&&Stage<8)for(double RiverY:{0.,5000.})
        {
            FHitResult Hit;const FVector At=AHansaRostockQuarter::VisitOffset()+FVector(6000,RiverY,0);
            if(W->LineTraceSingleByChannel(Hit,At+FVector(0,0,10000),At-FVector(0,0,10000),ECC_Visibility))
            {Evidence+=FString::Printf(TEXT("groundY=%.0f z=%.2f actor=%s\n"),RiverY,Hit.ImpactPoint.Z,*GetNameSafe(Hit.GetActor()));Test->TestTrue(TEXT("Native terrain matches grading"),FMath::Abs(Hit.ImpactPoint.Z-AHansaRostockQuarter::GroundHeight(RiverY))<2.);}
            else Test->AddError(TEXT("Remote terrain collision missing"));
        }
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));++Stage;Prepared=false;return Stage==9;
    }
private:FAutomationTestBase* Test;double Start,Ready=0,GameMs=0,RenderMs=0;int Stage=0,Steps=0,Samples=0;bool Prepared=false;uint64 Initial=0,SavedFingerprint=0;int64 RouteId=0;TArray<uint8> Save;Hansa::Game::FHansaStrategyCameraState Home;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRostockViewport,"Hansa.World.Rostock.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FRostockViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FRostockCapture(this));return true;}
#endif
