#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeJourneySupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaHudPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
using namespace Hansa::Simulation;
using namespace Hansa::Tests::TradeJourney;
namespace {
class FTradeJourneyViewport:public IAutomationLatentCommand
{
public:
    explicit FTradeJourneyViewport(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>240){Test->AddError(FString::Printf(TEXT("P34 timed out at stage %d"),Stage));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* View=GEngine->GameViewport.Get();auto* W=View->GetWorld();auto* PC=W?W->GetFirstPlayerController():nullptr;
        auto* H=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
        auto Root=H->GetRootWidget();auto* Host=Cast<AHansaGameMode>(W->GetAuthGameMode())->GetSimulationHost();
        auto* Trade=H->GetTradeMapPresentationModel();auto* Market=H->GetMarketTablePresentationModel();auto* Camera=Cast<AHansaStrategyCameraPawn>(PC->GetPawn());
        AHansaCargoProjectionManager* Manager=nullptr;for(TActorIterator<AHansaCargoProjectionManager> It(W);It;++It){Manager=*It;break;}if(!Manager)return false;
        auto Press=[&](const FString& Id){
            if(!Test->TestTrue(FString::Printf(TEXT("Keyboard focus %s"),*Id),Root->FocusSemanticId(Id)))return;
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
        };
        if(!Initialized)
        {
            FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});
            H->GetScenarioPresentationModel()->AcknowledgeBriefing();H->GetScenarioPresentationModel()->DismissHelp();
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Camera->bEnableMouseEdgePan=false;
            Initialized=true;
        }
        if(H->IsCityVisitLoading())return false;
        if(Stage==0&&!Prepared)
        {
            const auto* Row=Market->GetSnapshot().AllRows.FindByPredicate([](const auto& R){return R.GoodStableId==TEXT("Good.Bread");});
            if(!Row||!Row->bShortage){if(++Steps>80){Test->AddError(TEXT("No opening Bread shortage"));return true;}Host->AdvanceTicks(1);return false;}
        }
        if(!ActionsDone)
        {
            switch(Stage)
            {
            case 0:Press(TEXT("HUD.TopStatus.CityOverview"));Press(TEXT("CityOverview.Tab.Market"));Press(TEXT("CityOverview.Market.Details"));Press(TEXT("Market.Row.Good_Bread"));break;
            case 1:Press(TEXT("CityOverview.City.Rostock"));Press(TEXT("CityOverview.City.Rostock"));break;
            case 2:
                Press(TEXT("CityOverview.City.Lubeck"));Press(TEXT("Market.Row.Good_Bread"));Press(TEXT("Market.Detail.Action.BeginRoute"));
                if(Root->FocusSemanticId(TEXT("Session.Help.Hide")))Press(TEXT("Session.Help.Hide"));
                Test->TestTrue(TEXT("Market shortage opens an import draft"),Trade->GetSnapshot().bCreating&&Trade->GetDraftStops()[0].Actions[0].Kind==EHansaRouteCargoActionKind::Unload);break;
            case 3:{auto J=Fixture();if(!J){Test->AddError(TEXT("Missing fixture"));return true;}for(const auto& A:J->GetArrayField(TEXT("actions")))if(A->AsString()!=TEXT("TradeMap.Creator.Activate"))Press(A->AsString());Test->TestTrue(TEXT("Import review validates"),Trade->GetSnapshot().bCanCreate);break;}
            case 4:Press(TEXT("TradeMap.Creator.Activate"));Route=Trade->GetSnapshot().SelectedRouteValue;Press(TEXT("TradeMap.Stop.1"));Press(TEXT("TradeMap.Editor.Visit"));ActionsDone=true;return false;
            case 7:Press(TEXT("HUD.TopStatus.ReturnCity"));break;
            case 9:Press(TEXT("HUD.TopStatus.CityOverview"));Press(TEXT("CityOverview.Tab.Market"));Press(TEXT("Market.Row.Good_Bread"));break;
            case 10:Press(TEXT("CityOverview.Tab.Population"));break;
            case 11:Press(TEXT("CityOverview.Close"));Press(TEXT("HUD.TopStatus.TradeMap"));break;
            }
            ActionsDone=true;
        }
        FHansaCargoWorldObservation O;
        if(Stage>=4)
        {
            bool Found=false;for(const auto& X:Manager->QueryCargo())if(X.RouteId.StartsWith(FString::Printf(TEXT("%llu."),Route))){O=X;Found=true;break;}
            if(!Found){Test->AddError(TEXT("No typed observation for created Cog route"));return true;}
        }
        if(Stage>=4&&Stage<=8)
        {
            const EHansaCargoWorldPhase Wanted[]={EHansaCargoWorldPhase::Loading,EHansaCargoWorldPhase::Departing,EHansaCargoWorldPhase::Traveling,EHansaCargoWorldPhase::Arriving,EHansaCargoWorldPhase::Unloading};
            if(O.Phase!=Wanted[Stage-4]){if(++Steps>160){Test->AddError(TEXT("Expected cargo phase never observed"));return true;}Host->AdvanceTicks(1);return false;}
            Test->TestTrue(TEXT("Observed cargo is visible in the visited city"),O.bVisible);
            Test->TestTrue(TEXT("World transfer and simulation share a tick"),O.SimulationTick==Host->BuildProjection().Value.GetClock().GetTick().GetValue());
            if(Stage==4)Test->TestTrue(TEXT("Load uses real stock and protects reserve"),O.CargoMilliUnits>0&&Stock(Host->BuildProjection().Value,4)>=5000);
            if(Stage==4||Stage==8)
            {
                const bool Matched=Host->GetEventHistory().ContainsByPredicate([&](const auto& E){return E.GetRouteId().GetValue()==Route&&E.GetTick().GetValue()==O.TransferTick&&E.GetValue()==O.TransferMilliUnits&&(E.GetType()==EHansaDomainEventType::RouteCargoTransferred||E.GetType()==EHansaDomainEventType::RouteCargoMissed);});
                Test->TestTrue(TEXT("World receipt joins the authoritative domain event"),Matched);
            }
            if(Stage==8)Test->TestTrue(TEXT("Unload clears real hold and records quantity"),O.CargoMilliUnits==0&&O.TransferMilliUnits>0);
        }
        if(Stage==11&&!Prepared)
        {
            const auto P=Host->BuildProjection().Value;const auto* R=P.GetRoutes().FindByPredicate([&](const auto& X){return X.Id.GetValue()==Route;});
            if(!R||R->Lifecycle!=EHansaRouteLifecycleState::AtStop||Stock(P,1001)!=0){if(++Steps>200){Test->AddError(TEXT("No safe empty port for edit"));return true;}Host->AdvanceTicks(1);return false;}
            Press(TEXT("TradeMap.Editor.ToggleActive"));Press(TEXT("TradeMap.Stop.1"));Press(TEXT("TradeMap.Editor.Stop.Up"));Press(TEXT("TradeMap.Editor.Save"));Press(TEXT("TradeMap.Editor.Cancel"));
            Test->TestTrue(TEXT("Normal controls cancelled the repaired route"),Trade->GetSnapshot().EditorStatus.ToString().Contains(TEXT("cancelled")));
        }
        if(!Prepared)
        {
            if(Stage>=4&&Stage<=8){Camera->AddZoomIntent((Camera->GetZoomDistance()-5000)/Camera->ZoomUnitsPerStep);Camera->FocusWorldLocationIntent(O.Location);}
            Prepared=true;Ready=FPlatformTime::Seconds()+1.5;return false;
        }
        if(FPlatformTime::Seconds()<Ready)return false;
        TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native screenshot failed"));return true;}
        int X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);Test->TestTrue(TEXT("Screenshot is native resolution"),Size.X==X&&Size.Y==Y);
        const TCHAR* Names[]={TEXT("opportunity"),TEXT("source-report"),TEXT("import-draft"),TEXT("review"),TEXT("loading"),TEXT("departing"),TEXT("traveling"),TEXT("arriving"),TEXT("unloading"),TEXT("market-feedback"),TEXT("needs-feedback"),TEXT("cancelled")};
        const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P34/journey-%dx%d-%.1f-%s"),X,Y,Scale,Names[Stage]);
        for(auto& Pixel:Pixels)Pixel.A=255;TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png")));
        FString Log=Evidence(Host,Route)+FString::Printf(TEXT("semantic=%s\nphase=%d\nworldCargo=%lld\nworldTransfer=%lld\nworldTransferTick=%lld\n"),*O.SemanticId.ToString(),int(O.Phase),O.CargoMilliUnits,O.TransferMilliUnits,O.TransferTick);
        for(const auto& N:Root->GetSemanticSnapshot())if(N.State.bVisible)Log+=FString::Printf(TEXT("ui=%s | %s | %s | %d,%d,%d,%d\n"),*N.Id,*N.Label,*N.State.Value,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y);
        FFileHelper::SaveStringToFile(Log,*(Base+TEXT(".txt")));Prepared=false;ActionsDone=false;++Stage;return Stage==12;
    }
private:FAutomationTestBase* Test;double Started,Ready=0;int Stage=0,Steps=0;float Scale=1;bool Initialized=false,Prepared=false,ActionsDone=false;uint64 Route=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeJourneyNative,"Hansa.UI.TradeJourney.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FTradeJourneyNative::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeJourneyViewport(this));return true;}
#endif
