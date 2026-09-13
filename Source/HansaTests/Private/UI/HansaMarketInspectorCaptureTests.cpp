#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaMarketInspectorTestSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaContextInspector.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

namespace {
class FMarketInspectorCapture final : public IAutomationLatentCommand {
public:
 explicit FMarketInspectorCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>100){Test->AddError(TEXT("Market capture timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();
  auto* C=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;auto* GM=W?W->GetAuthGameMode<AHansaGameMode>():nullptr;auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!H||!Host||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto* M=H->GetInspectorPresentationModel();auto& Slate=FSlateApplication::Get();
  if(!Prepared){
   H->GetScenarioPresentationModel()->Close();H->GetBuildMenuPresentationModel()->SetOpen(false);H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   Slate.CloseToolTip();
   if(Stage==0){
    if(!Test->TestTrue(TEXT("Construct market for world selection"),EnsureMarketInspectorTestBuilding(Host)))return true;
    Host->AdvanceTicks(5);bool Found=false;
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Market")){
     C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
     if(M->GetSnapshot().Kind==EHansaInspectorObjectKind::Market){
      Found=true;
      if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(It->GetActorLocation());}
      break;
     }
    }
    if(!Test->TestTrue(TEXT("Select actual market through world selection"),Found))return true;
   }
   if(Stage==1){M->OpenCauseIntent();}
   if(Stage==2){if(M->GetSnapshot().bCauseExpanded)M->OpenCauseIntent();Root->SetPreferences({true,true,true});}
   if(Stage==3){
    Root->SetPreferences({false,false,false});
    const auto Before=Host->BuildProjection();
    const auto Remaining=(Hansa::Simulation::FHansaConsumptionHistory::WindowMinutes-Before.Value.GetCitizenConsumption().CoveredMinutes)/Before.Value.GetClock().GetMinutesPerTick();
    if(!Test->TestTrue(TEXT("Advance to full rolling window"),Host->AdvanceTicks(int32(Remaining))))return true;
    H->GetScenarioPresentationModel()->Close();
    Test->TestTrue(TEXT("Full thirty-day window available"),Host->BuildProjection().Value.GetCitizenConsumption().bFullWindow);
    Test->TestTrue(TEXT("Panel identifies last thirty days"),M->GetSnapshot().PrimaryResult.ToString().Contains(TEXT("Last 30 days")));
   }
   if(Stage!=1 && !M->GetSnapshot().Flows.IsEmpty()){
    const auto Id=TEXT("Inspector.Flows.Item.")+M->GetSnapshot().Flows[0].StableId.ToString().Replace(TEXT("."),TEXT("_"));
    Test->TestTrue(TEXT("Demand row keyboard/controller focus"),Root->GetInspector()->FocusSemanticId(Id));
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  if(Stage==1 && !DetailsFocused){
   Test->TestTrue(TEXT("Expanded action can receive focus after layout"),Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.Frame")));
   DetailsFocused=true;Ready=FPlatformTime::Seconds();return false;
  }
  const auto P=Host->BuildProjection();
  const auto* B=P.Value.GetBuildingWorldProjections().FindByPredicate([&](const auto& X){return int64(X.BuildingId.GetValue())==M->GetSnapshot().BuildingValue;});
  if(!Test->TestNotNull(TEXT("Market still in projection"),B))return true;
  Test->TestTrue(TEXT("Visible goods have measured rows"),!M->GetSnapshot().Flows.IsEmpty());
  for(const auto& Row:M->GetSnapshot().Flows){
   int64 Required=0,Supplied=0;
   for(const auto& Total:P.Value.GetCitizenConsumption().Goods)
    if(Total.CityId==B->Placement.CityId && FName(*Total.GoodId.ToString())==Row.DemandGoodId){Required+=Total.Required;Supplied+=Total.Consumed;}
   Test->TestEqual(TEXT("Visible demand matches actual consumption projection"),Row.DemandRequired,Required);
   Test->TestEqual(TEXT("Visible supply matches actual consumption projection"),Row.DemandSupplied,Supplied);
  }
  TArray<FColor> Pixels;FIntVector Size;TArray64<uint8> Png;
  if(!Slate.TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Market screenshot failed"));return true;}
  const TCHAR* Names[]={TEXT("demand"),TEXT("details"),TEXT("accessible"),TEXT("30-days")};
  auto Dir=FPaths::ProjectSavedDir()/TEXT("MarketInspector");IFileManager::Get().MakeDirectory(*Dir,true);
  auto Base=Dir/FString::Printf(TEXT("market-%dx%d-%s"),Size.X,Size.Y,Names[Stage]);
  FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence=TEXT("id\tvisible\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("Inspector."))){
   Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   if(Stage==1 && N.Id==TEXT("Inspector.Action.Frame")){
    const auto Bounds=Root->GetInspector()->GetCachedGeometry();
    const auto Bottom=Bounds.GetAbsolutePosition().Y+Bounds.GetAbsoluteSize().Y;
    Test->TestTrue(TEXT("Focused Frame action is revealed inside inspector"),N.Bounds.Min.Y>=Bounds.GetAbsolutePosition().Y && N.Bounds.Max.Y<=Bottom);
   }
   if(N.Id==TEXT("Inspector.Root"))Test->TestTrue(TEXT("Inspector fits real viewport"),N.Bounds.Min.X>=0&&N.Bounds.Min.Y>=0&&N.Bounds.Max.X<=Size.X&&N.Bounds.Max.Y<=Size.Y);
  }
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==4;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false,DetailsFocused=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketInspectorViewport,"Hansa.UI.MarketInspector.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaMarketInspectorViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FMarketInspectorCapture(this));return true;}
#endif
