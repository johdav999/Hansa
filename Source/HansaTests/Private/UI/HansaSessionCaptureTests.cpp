#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "World/HansaBuildingWorldProjection.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/SViewport.h"
#include "Engine/GameInstance.h"
#include "Save/HansaSaveSubsystem.h"
#include "UI/HansaSaveLoadPresentationModel.h"

namespace {
class FSessionCapture final : public IAutomationLatentCommand {
public:
 explicit FSessionCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("Session native flow timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
  if(!Hud||!Hud->GetRootWidget()||!Host)return false;
  if(Stage<14 && HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto* Model=Hud->GetScenarioPresentationModel();auto* Saves=W->GetGameInstance()->GetSubsystem<UHansaSaveSubsystem>();
  auto Press=[&](const TCHAR* Id){
   if(!Test->TestTrue(FString::Printf(TEXT("Native focus %s"),Id),Root->FocusSemanticId(Id)))return;
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
  };
  if(!Prepared){
   switch(Stage){
    case 0:Test->TestTrue(TEXT("Normal New Game has no starting buildings or roads"),Host->BuildProjection().Value.GetBuildingWorldProjections().IsEmpty());Test->TestTrue(TEXT("First launch focuses Begin without test navigation"),Root->ResolveSemanticWidget(TEXT("Scenario.Begin"))->HasKeyboardFocus());FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});Saves->UseIsolatedAutomationSlots();Model->LoadHelpPreferences(FPaths::ProjectSavedDir()/TEXT("Automation/Session")/(FGuid::NewGuid().ToString()+TEXT(".ini")));Model->ResetHelp();Tick=Host->GetSimulationTick();Test->TestTrue(TEXT("First launch opens briefing"),Model->GetSnapshot().bOpen&&Model->GetSnapshot().bReady);break;
    case 1:{int32 VisibleBuildings=0;for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(!It->IsActorBeingDestroyed()&&!It->IsHidden())++VisibleBuildings;Test->TestEqual(TEXT("Old world building actors are removed on New Game"),VisibleBuildings,0);}Press(TEXT("Scenario.Begin"));Test->TestTrue(TEXT("Begin offers camera guidance"),Model->GetSnapshot().bCoachVisible);break;
    case 2:
     Press(TEXT("Session.Help.Dismiss"));
     FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
     FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
     Test->TestTrue(TEXT("Escape opens the paused session menu"),Model->GetSnapshot().bOpen&&Model->GetSnapshot().bPauseMenu);
     Tick=Host->GetSimulationTick();
     break;
    case 3:Press(TEXT("Scenario.Progress"));break;
    case 4:Press(TEXT("Scenario.SaveLoad"));Press(TEXT("SaveLoad.Slot.manual"));Press(TEXT("SaveLoad.Action.Save"));Test->TestTrue(TEXT("Native save succeeded"),Saves->FindSlot(EHansaSaveSlotId::Manual)->bCanLoad);break;
    case 5:Press(TEXT("SaveLoad.Action.Load"));Test->TestTrue(TEXT("Load asks before replacing session"),Hud->GetSaveLoadPresentationModel()->GetSnapshot().Confirmation==EHansaSaveLoadConfirmation::Load);break;
    case 6:Press(TEXT("SaveLoad.Confirmation.Confirm"));Test->TestTrue(TEXT("Actual restore opens paused session"),Model->GetSnapshot().bLoadedSession&&Model->GetSnapshot().bPauseMenu);Press(TEXT("SaveLoad.Close"));break;
    case 7:Press(TEXT("Scenario.Resume"));Hud->GetBuildMenuPresentationModel()->SelectCategory(EHansaBuildCategory::Production);Hud->GetBuildMenuPresentationModel()->SetOpen(true);Test->TestTrue(TEXT("Construction context offers help"),Model->GetSnapshot().HelpTopic==EHansaSessionHelpTopic::Construction);Hud->GetBuildMenuPresentationModel()->SetOpen(false);break;
    case 8:Hud->GetBuildMenuPresentationModel()->SelectCategory(EHansaBuildCategory::Roads);Hud->GetBuildMenuPresentationModel()->SetOpen(true);Test->TestTrue(TEXT("Road context offers help"),Model->GetSnapshot().HelpTopic==EHansaSessionHelpTopic::Roads);Hud->GetBuildMenuPresentationModel()->SetOpen(false);break;
    case 9:Model->OfferHelp(EHansaSessionHelpTopic::Inspection);break;
    case 10:Press(TEXT("HUD.TopStatus.CityOverview"));Hud->GetCityOverviewPresentationModel()->SelectTabIntent(EHansaCityOverviewTab::Market);Test->TestTrue(TEXT("Market context offers help"),Model->GetSnapshot().HelpTopic==EHansaSessionHelpTopic::Market);Hud->GetCityOverviewPresentationModel()->CloseIntent();break;
    case 11:Press(TEXT("HUD.TopStatus.TradeMap"));Test->TestTrue(TEXT("Trade map offers route guidance"),Model->GetSnapshot().HelpTopic==EHansaSessionHelpTopic::Routes);Hud->GetTradeMapPresentationModel()->CloseIntent();break;
    case 12:{Root->SetPreferences({true,true,true,Scale});auto P=*Host->GetScenarioProgress();P.Outcome=Hansa::Simulation::EHansaScenarioOutcome::Failure;P.FailureReason=TEXT("Sustained insolvency. Your city could no longer cover its costs.");Model->ApplyProgress(P);break;}
    case 13:{auto P=*Host->GetScenarioProgress();P.Outcome=Hansa::Simulation::EHansaScenarioOutcome::Victory;P.WinningVictoryId=P.VictoryPaths[0].VictoryId;Model->ApplyProgress(P);break;}
    case 14:Root->SetPreferences({false,false,false,Scale});Model->OpenPause();Press(TEXT("Scenario.ReturnTitle"));Test->TestTrue(TEXT("Return to title asks for confirmation"),Hud->GetFrontendPresentationModel()->GetSnapshot().Page==EHansaFrontendPage::Confirmation);break;
    case 15:Press(TEXT("Frontend.Confirm"));Test->TestTrue(TEXT("Confirmed return opens main menu"),Hud->GetFrontendPresentationModel()->GetSnapshot().Page==EHansaFrontendPage::Title);Test->TestTrue(TEXT("Saved game survives return to menu"),Saves->FindSlot(EHansaSaveSlotId::Manual)->bCanLoad);break;
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<(Stage==1?1.8:.8))return false;
  if(Stage==0||Stage==2)Test->TestEqual(TEXT("Session modal holds authoritative clock"),Host->GetSimulationTick(),Tick);
  if(Stage==1){Test->TestTrue(TEXT("Begin resumes authoritative clock"),Host->GetSimulationTick()>Tick);
   FString Actors;for(TActorIterator<AActor> It(W);It;++It){TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);for(auto* Mesh:Meshes)if(Mesh->GetStaticMesh()&&Mesh->IsVisible()&&!It->IsHidden())Actors+=It->GetName()+TEXT("\t")+It->GetClass()->GetName()+TEXT("\t")+It->GetActorLocation().ToString()+TEXT("\t")+Mesh->GetStaticMesh()->GetPathName()+TEXT("\n");}
   Test->TestFalse(TEXT("No static bakery showcase remains"),Actors.Contains(TEXT("/Game/HansaBakery_20260906_01/")));
   Test->TestFalse(TEXT("No static residence showcase remains"),Actors.Contains(TEXT("/Game/Mesh/LaborerResidence/Materials_R04/Meshes/SM_LaborerResidence")));
   FFileHelper::SaveStringToFile(Actors,*(FPaths::ProjectSavedDir()/TEXT("P28/empty-city-visible-meshes.tsv")));
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native screenshot failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  if(Stage==0||Stage==2||Stage==6){const auto Nodes=Root->GetSemanticSnapshot();const FString Id=Stage==0?TEXT("Scenario.Begin"):TEXT("Scenario.Resume");const auto* N=Nodes.FindByPredicate([&](const auto& It){return It.Id==Id;});Test->TestTrue(TEXT("Pinned session action visible and 48px"),N&&N->State.bEnabled&&N->Bounds.Height()>=47&&N->Bounds.Min.X>=0&&N->Bounds.Min.Y>=0&&N->Bounds.Max.X<=X&&N->Bounds.Max.Y<=Y);}
  const TCHAR* Names[]={TEXT("opening"),TEXT("camera"),TEXT("pause"),TEXT("progress"),TEXT("saved"),TEXT("load-confirmation"),TEXT("restored-paused"),TEXT("construction"),TEXT("roads"),TEXT("inspection"),TEXT("market"),TEXT("routes"),TEXT("failure-fixture"),TEXT("victory-fixture"),TEXT("return-confirmation"),TEXT("returned-title")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P28/session-%dx%d-scale%d-%s"),X,Y,FMath::RoundToInt(Scale*100),Names[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Native capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==16 || (Stage==3 && FParse::Param(FCommandLine::Get(),TEXT("HansaEmptyCityOnly")));
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;float Scale=1.f;int64 Tick=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionViewport,"Hansa.UI.Session.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FSessionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FSessionCapture(this));return true;}
#endif
