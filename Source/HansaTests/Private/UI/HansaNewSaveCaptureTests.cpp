#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "Widgets/SViewport.h"

namespace {
class FNewSaveCapture final : public IAutomationLatentCommand {
public:
 explicit FNewSaveCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Save/load viewport timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=W->GetFirstPlayerController();auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto* M=H->GetSaveLoadPresentationModel();
  if(!Prepared){
   H->GetScenarioPresentationModel()->Close();
   auto* Saves=W->GetGameInstance()->GetSubsystem<UHansaSaveSubsystem>();
   Saves->UseIsolatedAutomationSlots();FText Error,Remedy;
   Test->TestTrue(TEXT("Original save captured in isolated storage"),Saves->Save(EHansaSaveSlotId::Manual,TEXT("Original voyage"),Error,Remedy));
   Test->TestTrue(TEXT("Save/load opens through HUD"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.SaveLoad")));
   M->SetSaveName(TEXT("New voyage"));
   Test->TestTrue(TEXT("New save action in assembled game"),Root->ActivateSemanticId(TEXT("SaveLoad.Action.NewSave")));
   Test->TestTrue(TEXT("Separate save selected"),M->GetSnapshot().SelectedSaveId!=TEXT("manual"));
   Test->TestEqual(TEXT("Both manual saves retained"),Saves->GetSlots().Num(),3);
   Test->TestTrue(TEXT("New save focus"),Root->FocusSemanticId(TEXT("SaveLoad.Action.NewSave")));
   Prepared=true;ReadyFrame=GFrameCounter;return false;
  }
  if(GFrameCounter<ReadyFrame+8)return false;
  const auto Tree=Root->GetSemanticSnapshot();
  for(const auto& Id:{TEXT("SaveLoad.Action.NewSave"),TEXT("SaveLoad.Action.Save"),TEXT("SaveLoad.Action.Load")}){
   const auto* N=Tree.FindByPredicate([Id](const auto& Node){return Node.Id==Id;});
   Test->TestTrue(TEXT("All save controls have visible target bounds"),N&&N->State.bVisible&&N->State.bEnabled&&N->Bounds.Height()>=40);
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
  const FString Dir=FPaths::ProjectDir()/TEXT("Docs/Images/UI/SaveLoad");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("new-save--ingame--%dx%d"),Size.X,Size.Y);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence;for(const auto& N:Tree)if(N.Id.StartsWith(TEXT("SaveLoad.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));return true;
 }
private:FAutomationTestBase* Test;double Start;uint64 ReadyFrame=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaNewSaveViewport,"Hansa.UI.SaveLoad.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaNewSaveViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FNewSaveCapture(this));return true;}
#endif
