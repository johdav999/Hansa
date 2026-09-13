#if WITH_DEV_AUTOMATION_TESTS
#include "../UI/HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "Components/StaticMeshComponent.h"
namespace
{
class FCargoViewportFlow:public IAutomationLatentCommand
{
public:
    explicit FCargoViewportFlow(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Cargo viewport flow timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* View=GEngine->GameViewport.Get();auto* W=View->GetWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* H=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
        auto Root=H->GetRootWidget();auto* Host=Cast<AHansaGameMode>(W->GetAuthGameMode())->GetSimulationHost();
        auto* Camera=Cast<AHansaStrategyCameraPawn>(PC->GetPawn());auto* Trade=H->GetTradeMapPresentationModel();
        AHansaCargoProjectionManager* Manager=nullptr;for(TActorIterator<AHansaCargoProjectionManager> It(W);It;++It){Manager=*It;break;}if(!Manager)return false;
        auto Press=[&](const TCHAR* Id)
        {
            if(!Test->TestTrue(FString::Printf(TEXT("Focus %s"),Id),Root->FocusSemanticId(Id)))return;
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
        };
        if(!Initialized)
        {
            float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});
            H->GetScenarioPresentationModel()->AcknowledgeBriefing();H->GetScenarioPresentationModel()->DismissHelp();
            H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Camera->bEnableMouseEdgePan=false;
            Press(TEXT("HUD.TopStatus.TradeMap"));Press(TEXT("TradeMap.New"));Press(TEXT("TradeMap.Creator.Cog"));Press(TEXT("TradeMap.Stop.0"));Press(TEXT("TradeMap.Editor.Reserve.Decrease"));Press(TEXT("TradeMap.Creator.Review"));Press(TEXT("TradeMap.Creator.Activate"));
            RouteValue=Trade->GetSnapshot().SelectedRouteValue;Press(TEXT("TradeMap.Close"));Initialized=true;
        }
        if(Stage==3&&!VisitRequested){Test->TestTrue(TEXT("Visit destination through ordinary intent"),H->VisitCityIntent(TEXT("City.Rostock")));VisitRequested=true;return false;}
        if(Stage>=3&&H->IsCityVisitLoading())return false;
        const FHansaCargoWorldObservation* Found=nullptr;
        for(const auto& O:Manager->QueryCargo())if(O.RouteId.StartsWith(FString::Printf(TEXT("%lld."),RouteValue))){Key=O.SemanticId;Found=Manager->FindObservation(Key);break;}
        if(!Found){Test->AddError(TEXT("Cargo observation missing"));return true;}
        const auto O=*Found;
        const EHansaCargoWorldPhase Wanted[]={EHansaCargoWorldPhase::Loading,EHansaCargoWorldPhase::Departing,EHansaCargoWorldPhase::Traveling,EHansaCargoWorldPhase::Arriving,EHansaCargoWorldPhase::Berthed,EHansaCargoWorldPhase::Unloading};
        if(Stage<6&&O.Phase!=Wanted[Stage])
        {if(++Steps>80){Test->AddError(FString::Printf(TEXT("Missing cargo phase at stage %d"),Stage));return true;}Host->AdvanceTicks(1);return false;}
        if(!Prepared)
        {
            if(Stage==6)
            {
                const auto Hash=Host->BuildProjection().Value.GetFingerprint().Value;
                Test->TestTrue(TEXT("Semantic cargo selection opens native inspector"),H->InspectCargo(Key));Press(TEXT("Inspector.Action.Frame"));
                Test->TestEqual(TEXT("Selection and frame are read-only"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);
            }
            if(Stage==7)
            {
                TArray<uint8> Bytes;const auto Hash=Host->BuildProjection().Value.GetFingerprint().Value;
                Test->TestTrue(TEXT("Save at unload"),!!Host->CaptureSaveBytes(Bytes,TEXT("P32 viewport"),TEXT("2026-09-09T00:00:00Z")));
                Host->AdvanceTicks(2);Test->TestTrue(TEXT("Restore unload"),!!Host->RestoreSaveBytes(Bytes));
                Test->TestEqual(TEXT("Reconstructed exact inventory state"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);
            }
            Camera->AddZoomIntent((Camera->GetZoomDistance()-5000)/Camera->ZoomUnitsPerStep);Camera->FocusWorldLocationIntent(O.Location);
            Prepared=true;Ready=FPlatformTime::Seconds()+2;return false;
        }
        if(FPlatformTime::Seconds()<Ready)return false;
        auto* Actor=Manager->FindActor(Key);if(!Test->TestNotNull(TEXT("Verified visible cargo actor"),Actor))return true;
        Test->TestTrue(TEXT("Loaded city renders cargo"),O.bVisible&&!Actor->IsHidden());
        Test->TestEqual(TEXT("Cargo skin uses exact inventory presence"),Actor->Cargo->IsVisible(),O.CargoMilliUnits>0);
        FHitResult Hit;const auto At=Actor->GetActorLocation()+FVector(0,0,500);
        Test->TestTrue(TEXT("Native world selection hits same vehicle"),W->LineTraceSingleByChannel(Hit,At+FVector(0,0,5000),At,ECC_Visibility)&&Hit.GetActor()==Actor);
        TArray<FColor> Pixels;FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Screenshot failed"));return true;}
        int X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
        Test->TestTrue(TEXT("Native pixels preserved"),Size.X==X&&Size.Y==Y);
        const auto Semantics=Root->GetSemanticSnapshot();
        const auto* Header=Semantics.FindByPredicate([](const auto& N){return N.Id==TEXT("HUD.TopStatus");});
        if(Header)for(const auto& N:Semantics)if(N.Id.StartsWith(TEXT("HUD.TopStatus.")) && N.State.bVisible)
            Test->TestTrue(*FString::Printf(TEXT("Header contains %s"),*N.Id),N.Bounds.Min.Y>=Header->Bounds.Min.Y && N.Bounds.Max.Y<=Header->Bounds.Max.Y);
        const TCHAR* Names[]={TEXT("loading"),TEXT("departing"),TEXT("traveling"),TEXT("arriving"),TEXT("berthed"),TEXT("unloading"),TEXT("selected"),TEXT("restored")};
        const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P32/cargo-%dx%d-%s"),X,Y,Names[Stage]);
        for(auto& Pixel:Pixels)Pixel.A=255;TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png")));
        const FString Evidence=FString::Printf(TEXT("semantic=%s\nvehicle=%s\nroute=%s\ninventory=%s\ntick=%lld\ncargoMilli=%lld\ntransferMilli=%lld\ntransferTick=%lld\nprogress=%.6f\nlocation=%s\nfingerprint=%llu\n"),*O.SemanticId.ToString(),*O.VehicleId,*O.RouteId,*O.CargoInventoryId,O.SimulationTick,O.CargoMilliUnits,O.TransferMilliUnits,O.TransferTick,O.Progress,*O.Location.ToString(),static_cast<unsigned long long>(Host->BuildProjection().Value.GetFingerprint().Value));
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));Prepared=false;++Stage;return Stage==8;
    }
private:
    FAutomationTestBase* Test;double Started,Ready=0;int Stage=0,Steps=0;bool Initialized=false,Prepared=false,VisitRequested=false;int64 RouteValue=0;FName Key;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoWorldViewport,"Hansa.World.CargoProjection.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FCargoWorldViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FCargoViewportFlow(this));return true;}

namespace
{
class FVisibleLocalWagonViewportFlow final:public IAutomationLatentCommand
{
public:
    explicit FVisibleLocalWagonViewportFlow(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Visible local wagon viewport flow timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* View=GEngine->GameViewport.Get();auto* World=View->GetWorld();auto* Controller=World?World->GetFirstPlayerController():nullptr;
        auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
        auto* Mode=World?World->GetAuthGameMode<AHansaGameMode>():nullptr;auto* Host=Mode?Mode->GetSimulationHost():nullptr;
        if(!Hud||!Host||!Hud->GetRootWidget()||HansaWaitForFrontend(Hud))return false;
        AHansaCargoProjectionManager* Manager=nullptr;for(TActorIterator<AHansaCargoProjectionManager> It(World);It;++It){Manager=*It;break;}
        auto* Camera=Cast<AHansaStrategyCameraPawn>(Controller->GetPawn());if(!Manager||!Camera)return false;
        if(!Initialized)
        {
            Hud->GetScenarioPresentationModel()->Close();Hud->GetBuildMenuPresentationModel()->SetOpen(false);
            Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
            Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Initialized=true;
        }
        if(!Prepared)
        {
            TArray<FHansaCargoWorldObservation> Local;
            int32 LoadedGrainIndex=INDEX_NONE;
            for(const FHansaCargoWorldObservation& Observation:Manager->QueryCargo())
            {
                if(Observation.JobId.IsEmpty()||!Observation.PresentationFailure.IsEmpty())continue;
                Local.Add(Observation);
                const bool bWantedEndpoints=Stage==0
                    ? Observation.SourceBuildingId==TEXT("14.0")&&Observation.DestinationBuildingId==TEXT("2.0")
                    : Observation.SourceBuildingId==TEXT("1.0")&&Observation.DestinationBuildingId==TEXT("14.0");
                if(Observation.Phase==EHansaCargoWorldPhase::Traveling&&Observation.CargoMilliUnits>0&&
                    Observation.GoodId==TEXT("Good.Grain")&&bWantedEndpoints&&
                    Observation.Progress>=0.45&&Observation.Progress<=0.75)LoadedGrainIndex=Local.Num()-1;
            }
            if((Stage==0&&Local.Num()<2)||LoadedGrainIndex==INDEX_NONE)
            {
                if(++Steps>140){Test->AddError(Stage==0
                    ? TEXT("Playable production chain did not produce two visible deliveries including a mid-route Market-to-Mill grain wagon")
                    : TEXT("Playable production chain did not produce a mid-route Grain-Farm-to-Market wagon"));return true;}
                Test->TestTrue(TEXT("Advance playable production logistics"),Host->AdvanceTicks(1));return false;
            }
            Test->TestEqual(TEXT("Every playable local job has a wagon"),Manager->GetActiveLocalWagonCount(),Local.Num());
            for(const FHansaCargoWorldObservation& Observation:Local)
            {
                auto* Actor=Manager->FindActor(Observation.SemanticId);
                if(!Test->TestNotNull(TEXT("Visible job actor"),Actor))continue;
                Test->TestTrue(TEXT("Approved generated wagon Blueprint is used"),
                    Actor->GetClass()->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-vehicles/")));
                Test->TestTrue(TEXT("Wagon carries its exact authoritative good"),
                    Actor->GetCargoGoodId().ToString()==Observation.GoodId.ToString());
            }
            Selected=Local[LoadedGrainIndex];
            Test->TestTrue(TEXT("Native wagon selection opens the inspector"),Hud->InspectCargo(Selected.SemanticId));
            Camera->AddZoomIntent((Camera->GetZoomDistance()-2400.0)/Camera->ZoomUnitsPerStep);
            Camera->FocusWorldLocationIntent(Selected.Location);
            Prepared=true;Ready=FPlatformTime::Seconds()+2.0;return false;
        }
        if(FPlatformTime::Seconds()<Ready)return false;
        auto* Actor=Manager->FindActor(Selected.SemanticId);
        if(!Test->TestNotNull(TEXT("Selected wagon remains visible for capture"),Actor))return true;
        Test->TestTrue(TEXT("Selected wagon visibly carries grain"),Actor->Cargo->IsVisible()&&!Actor->IsHidden());
        TArray<FColor> Pixels;FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Visible wagon screenshot failed"));return true;}
        int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
        Test->TestTrue(TEXT("Visible wagon capture preserves native dimensions"),Size.X==X&&Size.Y==Y);
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("VisibleWagons");IFileManager::Get().MakeDirectory(*Directory,true);
        const FString Filename=Stage==0
            ? FString::Printf(TEXT("local-wagons-%dx%d"),X,Y)
            : FString::Printf(TEXT("local-wagons-%dx%d-farm-to-market"),X,Y);
        const FString Base=Directory/Filename;
        for(FColor& Pixel:Pixels)Pixel.A=255;TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Visible wagon PNG written"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        FString Evidence=FString::Printf(TEXT("selected=%s\ngood=%s\nsourceBuilding=%s\nsourceDefinition=%s\ndestinationBuilding=%s\ndestinationDefinition=%s\nquantityMilli=%lld\ncargoMilli=%lld\nactiveWagons=%d\npeakWagons=%d\nfingerprint=%llu\n"),
            *Selected.SemanticId.ToString(),*Selected.GoodId.ToString(),*Selected.SourceBuildingId,*Selected.SourceBuildingDefinitionId.ToString(),
            *Selected.DestinationBuildingId,*Selected.DestinationBuildingDefinitionId.ToString(),
            Selected.QuantityMilliUnits,Selected.CargoMilliUnits,Manager->GetActiveLocalWagonCount(),Manager->GetPeakLocalWagonCount(),
            static_cast<unsigned long long>(Host->BuildProjection().Value.GetFingerprint().Value));
        for(const FHansaCargoWorldObservation& Observation:Manager->QueryCargo())if(!Observation.JobId.IsEmpty())
            Evidence+=FString::Printf(TEXT("wagon=%s good=%s status=%s cargoMilli=%lld progress=%.6f visible=%d failure=%s\n"),
                *Observation.SemanticId.ToString(),*Observation.GoodId.ToString(),*Observation.LogisticsStatus.ToString(),
                Observation.CargoMilliUnits,Observation.Progress,Observation.bVisible?1:0,*Observation.PresentationFailure);
        Test->TestTrue(TEXT("Typed visible wagon evidence written"),FFileHelper::SaveStringToFile(
            Evidence,*(Base+TEXT(".txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        if(Stage==0)
        {
            const auto ProductionId=Hansa::Simulation::FHansaProductionId::TryCreate(1);
            if(!Test->TestTrue(TEXT("Playable grain farm production identity"),ProductionId.IsSuccess()))return true;
            Test->TestTrue(TEXT("Resume grain farm through the validated player command path"),
                Host->SetProductionActive(ProductionId.Value,true).IsSuccess());
            Stage=1;Prepared=false;Steps=0;Hud->GetInspectorPresentationModel()->CloseIntent();return false;
        }
        return true;
    }
private:
    FAutomationTestBase* Test=nullptr;double Started=0,Ready=0;int32 Stage=0,Steps=0;bool Initialized=false,Prepared=false;
    FHansaCargoWorldObservation Selected;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibleLocalWagonViewport,
    "Hansa.Integration.VisibleWagon.RealViewport",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FVisibleLocalWagonViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FVisibleLocalWagonViewportFlow(this));return true;}
#endif
