#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
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
#include "UI/HansaFrontendPresentationModel.h"
#include "Misc/App.h"
#include "Misc/SecureHash.h"
#include "World/HansaStrategyCameraPawn.h"

namespace {
class FFrontendCapture final : public IAutomationLatentCommand {
public:
 explicit FFrontendCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("Frontend native flow timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
  if(!Hud||!Hud->GetRootWidget()||!Host)return false;
  auto Root=Hud->GetRootWidget();auto* Model=Hud->GetFrontendPresentationModel();auto* Saves=W->GetGameInstance()->GetSubsystem<UHansaSaveSubsystem>();
  auto Press=[&](const TCHAR* Id){
   if(!Test->TestTrue(FString::Printf(TEXT("Native focus %s"),Id),Root->FocusSemanticId(Id)))return;
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
  };
  if(!Prepared){
   switch(Stage){
    case 0:FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Saves->UseIsolatedAutomationSlots();Model->LoadPreferences(FPaths::ProjectSavedDir()/TEXT("Automation/Frontend")/(FGuid::NewGuid().ToString()+TEXT(".ini")));Root->SetPreferences({false,false,false,Scale});Tick=Host->GetSimulationTick();break;
    case 1:Press(TEXT("Frontend.Settings"));break;
    case 2:Press(TEXT("Frontend.Settings.VolumeDown"));Test->TestTrue(TEXT("Volume setting applied"),Model->GetSnapshot().Volume<1.f&&FMath::IsNearlyEqual(FApp::GetVolumeMultiplier(),Model->GetSnapshot().Volume));break;
    case 3:Press(TEXT("Frontend.Settings.CameraUp"));Test->TestTrue(TEXT("Camera setting applied"),Model->GetSnapshot().CameraSpeed>1.f&&Cast<AHansaStrategyCameraPawn>(C->GetPawn())->PanUnitsPerSecond>2400.f);break;
    case 4:Press(TEXT("Frontend.Back"));break;
    case 5:Press(TEXT("Frontend.Credits"));break;
    case 6:Press(TEXT("Frontend.Back"));break;
    case 7:Press(TEXT("Frontend.NewGame"));break;
    case 8:Press(TEXT("Scenario.Begin"));break;
    case 9:Press(TEXT("HUD.TopStatus.Session"));break;
    case 10:Press(TEXT("Scenario.SaveLoad"));Hud->GetSaveLoadPresentationModel()->SetSaveName(TEXT("Baltic venture"));Press(TEXT("SaveLoad.Action.Save"));Test->TestTrue(TEXT("Named native save succeeded"),Saves->FindSlot(EHansaSaveSlotId::Manual)->bCanLoad);break;
    case 11:Press(TEXT("SaveLoad.Action.Save"));break;
    case 12:Press(TEXT("SaveLoad.Confirmation.Cancel"));Press(TEXT("SaveLoad.Close"));Press(TEXT("Scenario.ReturnTitle"));break;
    case 13:Press(TEXT("Frontend.Confirm"));break;
    case 14:Press(TEXT("Frontend.Continue"));break;
    case 15:Press(TEXT("Scenario.SaveLoad"));{const uint8 Bad[]={1,2,3};Saves->WriteAutomationSlot(EHansaSaveSlotId::Autosave,Bad);Saves->Refresh();}Press(TEXT("SaveLoad.Slot.autosave"));break;
    case 16:{TArray<uint8> Bytes;Host->CaptureSaveBytes(Bytes,TEXT("Newer save"),TEXT("2026-09-09T00:00:00Z"));Bytes[4]=99;Bytes[5]=Bytes[6]=Bytes[7]=0;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num()-20,Bytes.GetData()+Bytes.Num()-20);Saves->WriteAutomationSlot(EHansaSaveSlotId::Autosave,Bytes);Saves->Refresh();Test->TestTrue(TEXT("Unsupported format is incompatible"),Saves->FindSlot(EHansaSaveSlotId::Autosave)->Compatibility==EHansaSaveSlotCompatibility::Incompatible);break;}
    case 17:Press(TEXT("SaveLoad.Close"));Press(TEXT("Scenario.Settings"));Root->SetPreferences({true,true,true,Scale});break;
    case 18:Press(TEXT("Frontend.Back"));break;
    case 19:Press(TEXT("Scenario.ReturnTitle"));break;
    case 20:Press(TEXT("Frontend.Confirm"));break;
    case 21:Press(TEXT("Frontend.Load"));Press(TEXT("SaveLoad.Slot.manual"));break;
    case 22:Press(TEXT("SaveLoad.Action.Load"));break;
    case 23:Press(TEXT("SaveLoad.Confirmation.Confirm"));Test->TestTrue(TEXT("Title Load enables saving after restore"),Hud->GetSaveLoadPresentationModel()->GetSnapshot().bSavingAllowed);break;
    case 24:Press(TEXT("SaveLoad.Close"));break;
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<(Stage==1?1.8:.8))return false;
  if(Stage==0)Test->TestEqual(TEXT("Title holds simulation clock"),Host->GetSimulationTick(),Tick);
  if(Stage==7)Test->TestTrue(TEXT("New game enters production briefing"),Model->GetSnapshot().bHasSession&&Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing);
  if(Stage==14)Test->TestTrue(TEXT("Continue restores a paused session"),Model->GetSnapshot().bHasSession&&Hud->GetScenarioPresentationModel()->GetSnapshot().bLoadedSession&&Host->GetSpeed()==EHansaRuntimeSimulationSpeed::Paused);
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native screenshot failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  if(Stage==0){const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([](const auto& It){return It.Id==TEXT("Frontend.NewGame");});Test->TestTrue(TEXT("New game visible with native 48px target"),N&&N->State.bEnabled&&N->Bounds.Height()>=47&&N->Bounds.Min.X>=0&&N->Bounds.Min.Y>=0&&N->Bounds.Max.X<=X&&N->Bounds.Max.Y<=Y);Test->TestFalse(TEXT("Empty Continue is excluded"),Root->GetControllerFocusOrder().Contains(TEXT("Frontend.Continue")));}
  const TCHAR* Names[]={TEXT("title-empty"),TEXT("settings"),TEXT("audio"),TEXT("controls"),TEXT("title-return"),TEXT("credits"),TEXT("title-new"),TEXT("new-game"),TEXT("playing"),TEXT("pause"),TEXT("saved"),TEXT("overwrite"),TEXT("return-confirmation"),TEXT("title-continue"),TEXT("continued"),TEXT("corrupt-save"),TEXT("incompatible-save"),TEXT("accessible-settings"),TEXT("settings-back"),TEXT("second-return"),TEXT("title-load"),TEXT("load-selection"),TEXT("load-confirmation"),TEXT("loaded"),TEXT("loaded-paused")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P29/frontend-%dx%d-scale%d-%s"),X,Y,FMath::RoundToInt(Scale*100),Names[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Native capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==25;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;float Scale=1.f;int64 Tick=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrontendViewport,"Hansa.UI.Frontend.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FFrontendViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FFrontendCapture(this));return true;}
#endif
