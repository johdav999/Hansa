#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "Widgets/SViewport.h"
namespace {
class FConstructionProgressionCapture final : public IAutomationLatentCommand {
public:
 explicit FConstructionProgressionCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Construction progression capture timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();
  if(!Prepared){
   H->GetScenarioPresentationModel()->Close();
   H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   H->GetInspectorPresentationModel()->CloseIntent();
   if(auto* P=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){P->bEnableMouseEdgePan=false;P->ClearCameraIntents();}
	   const TCHAR* Category=Stage==0||Stage==2||Stage>=6?TEXT("BuildMenu.Category.Production"):Stage==5?TEXT("BuildMenu.Category.Storage"):TEXT("BuildMenu.Category.Residences");
	   const TCHAR* Tier=Stage<=1||Stage>=6?TEXT("BuildMenu.Tier.DayLaborers"):Stage<=3?TEXT("BuildMenu.Tier.Craftsmen"):TEXT("BuildMenu.Tier.Merchants");
   Test->TestTrue(TEXT("Native category action"),Root->ActivateSemanticId(Category));
   Test->TestTrue(TEXT("Native tier action"),Root->ActivateSemanticId(Tier));
	   if(Stage==6) Root->SetPreferences({true,true,true,1.f});
	   if(Stage==7) Root->SetPreferences({false,true,false,1.4f});
	   if(Stage==8) Root->SetPreferences({false,true,false,.8f});
   Test->TestTrue(TEXT("Tier can receive controller focus"),Root->FocusSemanticId(Tier));
   Prepared=true;ReadyFrame=GFrameCounter;return false;
  }
  if(GFrameCounter<ReadyFrame+8)return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport screenshot failed"));return true;}
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("ConstructionProgression");
  IFileManager::Get().MakeDirectory(*Directory,true);
  const FString Base=Directory/FString::Printf(TEXT("tiers-%dx%d-%d"),Size.X,Size.Y,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  Test->TestTrue(TEXT("Saved native game viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;
  for(const auto& N:Root->GetSemanticSnapshot()){
   if(!N.Id.StartsWith(TEXT("BuildMenu.")))continue;
   Evidence+=FString::Printf(TEXT("%s\tvisible=%d\tselected=%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bSelected,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
   if(N.Id==TEXT("BuildMenu.Root") || N.Id.StartsWith(TEXT("BuildMenu.Tier.")))
    Test->TestTrue(*FString::Printf(TEXT("%s fits at stage %d"),*N.Id,Stage),N.State.bVisible&&N.Bounds.Min.X>=0&&N.Bounds.Min.Y>=0&&N.Bounds.Max.X<=Size.X&&N.Bounds.Max.Y<=Size.Y);
  }
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
	  Prepared=false;return ++Stage==9;
 }
private:
 FAutomationTestBase* Test;double Start;uint64 ReadyFrame=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionProgressionViewport,"Hansa.UI.ConstructionProgression.RealViewport",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaConstructionProgressionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FConstructionProgressionCapture(this));return true;}
#endif
