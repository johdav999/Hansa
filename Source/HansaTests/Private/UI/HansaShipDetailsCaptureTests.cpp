#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaStrategyPlayerController.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaContextInspector.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SScrollBox.h"
namespace
{
class FShipDetailsCapture final : public IAutomationLatentCommand
{
public:
    explicit FShipDetailsCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Ship detail viewport timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
        auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
        if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
        auto Root=H->GetRootWidget();auto* M=H->GetInspectorPresentationModel();
        if(H->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
        if(!Prepared)
        {
            if(Stage==0)
            {
                FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,Scale>1,Scale>1,Scale});
                H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
                bool Found=false;
                for(TActorIterator<AHansaCargoProjectionManager> It(W);It&&!Found;++It)
                    for(const auto& O:It->QueryCargo())if(O.JobId.IsEmpty())
                    {
                        M->ShowCargo(O,O.SemanticId);Found=true;
                        Test->TestEqual(TEXT("Capacity comes from observed vehicle"),M->GetSnapshot().Ship.Capacity,O.CapacityMilliUnits);
                        Test->TestEqual(TEXT("Slot count comes from actual inventory"),M->GetSnapshot().Ship.Slots.Num(),O.CargoSlots.Num());
                        Test->TestTrue(TEXT("Ship inventory available"),M->GetSnapshot().Ship.bSlotsKnown);break;
                    }
                if(!Found)return false;
            }
            if(Stage==1)
            {
                auto Button=Root->ResolveSemanticWidget(TEXT("Inspector.Action.Pin"));
                Test->TestTrue(TEXT("Pin accepts keyboard focus"),Root->FocusSemanticId(TEXT("Inspector.Action.Pin")));
                const bool Pinned=M->GetSnapshot().bPinned;
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
                FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
                Test->TestTrue(TEXT("Keyboard toggles pin"),Pinned!=M->GetSnapshot().bPinned);
                Test->TestTrue(TEXT("Live refresh preserves button identity"),Button==Root->ResolveSemanticWidget(TEXT("Inspector.Action.Pin")));
                Root->ActivateSemanticId(TEXT("Inspector.Action.OpenCause"));
                const auto Scroll=StaticCastSharedPtr<SScrollBox>(Root->ResolveSemanticWidget(TEXT("Inspector.Ship.Scroll")));
                Test->TestTrue(TEXT("Ship scroll resolves through root"),Scroll.IsValid());if(!Scroll)return true;
                Scroll->ScrollToEnd();
            }
            if(Stage==2)
            {
                Test->TestTrue(TEXT("Trade map action works independently"),Root->ActivateSemanticId(TEXT("Inspector.Action.OpenRelated")));
                Test->TestTrue(TEXT("Trade map opens"),H->GetTradeMapPresentationModel()->GetSnapshot().bOpen);
                Test->TestFalse(TEXT("Trade map action does not open route editor"),H->GetTradeMapPresentationModel()->bShipDetailOpen);
                H->GetTradeMapPresentationModel()->CloseIntent();
                Test->TestTrue(TEXT("Route action opens ship route"),Root->ActivateSemanticId(TEXT("Inspector.Ship.Route")));
                Test->TestTrue(TEXT("Existing route workflow retained"),H->GetTradeMapPresentationModel()->bShipDetailOpen);return true;
            }
            Prepared=true;Ready=FPlatformTime::Seconds();return false;
        }
        if(FPlatformTime::Seconds()-Ready<1)return false;
        const auto Scroll=Root->ResolveSemanticWidget(TEXT("Inspector.Ship.Scroll"));
        Test->TestTrue(TEXT("Scrollable body has usable height"),Scroll&&Scroll->GetCachedGeometry().GetAbsoluteSize().Y>=100);
        const auto Nodes=Root->GetSemanticSnapshot();
        for(const TCHAR* Id:{TEXT("Inspector.Ship.Route"),TEXT("Inspector.Action.Pin"),TEXT("Inspector.Close")})
        {
            const auto* N=Nodes.FindByPredicate([&](const auto& Item){return Item.Id==Id;});
            Test->TestTrue(FString::Printf(TEXT("Visible reachable action %s"),Id),N&&N->State.bVisible&&!N->State.bClipped&&N->Bounds.Height()>=40);
        }
        TArray<FColor> Pixels;FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Screenshot failed"));return true;}
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("ShipDetails");IFileManager::Get().MakeDirectory(*Dir,true);
        const FString Base=Dir/FString::Printf(TEXT("actual-%dx%d-scale-%.1f-%d"),Size.X,Size.Y,Scale,Stage);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
        FString Evidence;for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("Inspector.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return false;
    }
private:
    FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;float Scale=1;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipDetailsViewport,"Hansa.ShipDetails.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FShipDetailsViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FShipDetailsCapture(this));return true;}
#endif
