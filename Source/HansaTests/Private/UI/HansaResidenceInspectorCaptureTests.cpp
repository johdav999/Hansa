#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
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
bool PlaceCaptureBuilding(UHansaRuntimeSimulationHost& Host,const TCHAR* StableId)
{
 using namespace Hansa::Simulation;
 FHansaPlacementSpec Spec;
 Spec.CityId=Host.GetCityId();
 Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(StableId).Value;
 for(int32 Y=0;Y<60;++Y)for(int32 X=0;X<60;++X)
 {
  Spec.Anchor={X,Y};
  auto Validation=Host.ValidatePlacement(Spec);
  if(!Validation&&Validation.GetReasons().Num()==1&&Validation.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired)
  {
   FHansaPlacementSpec Road=Spec;
   Road.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
   --Road.Anchor.X;
   if(Host.ValidatePlacement(Road))Host.PlaceBuildings({Road});
   Validation=Host.ValidatePlacement(Spec);
  }
  if(Validation&&Host.PlaceBuildings({Spec}).IsSuccess())return true;
 }
 return false;
}
class FResidenceCapture final:public IAutomationLatentCommand {
public:
 explicit FResidenceCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 ~FResidenceCapture()override{if(SavedCursor&&FSlateApplication::IsInitialized()){FSlateApplication::Get().CloseToolTip();FSlateApplication::Get().SetCursorPos(Original);}}
 bool Update()override{
  if(FPlatformTime::Seconds()-Start>100){Test->AddError(TEXT("Residence capture timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();
  auto* C=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;auto* GM=W?W->GetAuthGameMode<AHansaGameMode>():nullptr;auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!H||!Host||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto* M=H->GetInspectorPresentationModel();auto& Slate=FSlateApplication::Get();
  if(!Prepared){
   if(!SavedCursor){Original=Slate.GetCursorPos();SavedCursor=true;}
   Slate.CloseToolTip();Slate.SetCursorPos(FVector2D(2,2));
   H->GetScenarioPresentationModel()->Close();H->GetBuildMenuPresentationModel()->SetOpen(false);H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   if(Stage==0){
    if(!Test->TestTrue(TEXT("Construct a residence in the current empty-city start"),
     PlaceCaptureBuilding(*Host,TEXT("Building.Residence.Laborer"))))return true;
    const auto* HomeDefinition=Host->FindBuildingDefinition(TEXT("Building.Residence.Laborer"));
    const auto* RoadDefinition=Host->FindBuildingDefinition(TEXT("Building.Road"));
    if(!Test->TestNotNull(TEXT("Residence definition exists"),HomeDefinition)||
       !Test->TestNotNull(TEXT("Road definition exists"),RoadDefinition))return true;
    if(!Test->TestTrue(TEXT("Complete the residence before any physical Market exists"),
     Host->AdvanceTicks(FMath::Max(HomeDefinition->BuildTicks,RoadDefinition->BuildTicks))))return true;
    Host->SynchronizeWorldProjection();
    bool Found=false;
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Residence.Laborer")){
     C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
     if(M->GetSnapshot().Residence.bValid){Found=true;if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(It->GetActorLocation());}break;}
    }
    if(!Test->TestTrue(TEXT("Select an actual residence in the world"),Found))return true;
   }
   if(Stage==1)Host->AdvanceTicks(5);
   if(Stage==2){M->OpenCauseIntent();Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.Frame"));}
   if(Stage==3){if(M->GetSnapshot().bCauseExpanded)M->OpenCauseIntent();Root->SetPreferences({true,true,true});Test->TestTrue(TEXT("Last product is reachable with large text"),Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Residence.Need.Fish")));}
   if(Stage==4){Root->SetPreferences({});if(M->GetSnapshot().bCauseExpanded)M->OpenCauseIntent();}
   if(Stage==5){
    Slate.CloseToolTip();
    const auto P=Host->BuildProjection();bool Found=false;
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It){
     const auto* Home=P.Value.GetPopulationCohorts().FindByPredicate([&](const auto& X){return X.ResidenceBuildingId==It->GetBuildingId()&&X.Residents==0;});
     if(!Home)continue;
     C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());Found=M->GetSnapshot().Residence.bValid;if(Found)break;
    }
    if(!Test->TestTrue(TEXT("Select an empty real residence"),Found))return true;
   }
   if(Stage==6){
    const auto P=Host->BuildProjection();
    const auto* Home=P.Value.GetPopulationCohorts().FindByPredicate([&](const auto& X){return int64(X.ResidenceBuildingId.GetValue())==M->GetSnapshot().BuildingValue;});
    if(!Home)return true;
    const int32 Ticks=int32((Hansa::Simulation::FHansaConsumptionHistory::WindowMinutes-Home->Consumption.CoveredMinutes)/P.Value.GetClock().GetMinutesPerTick());
    if(!Test->TestTrue(TEXT("Advance full residence history"),Host->AdvanceTicks(Ticks)))return true;
    H->GetScenarioPresentationModel()->Close();
    Test->TestTrue(TEXT("Full-window label shown"),M->GetSnapshot().Residence.ConsumptionPeriod.ToString().Contains(TEXT("Last 30 days")));
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.4)return false;
  const auto Projection=Host->BuildProjection();
  const auto* P=Projection.Value.GetPopulationCohorts().FindByPredicate([&](const auto& X){return int64(X.ResidenceBuildingId.GetValue())==M->GetSnapshot().BuildingValue;});
  if(!Test->TestNotNull(TEXT("Selected house still has a population cohort"),P))return true;
  Test->TestEqual(TEXT("Displayed residents equal simulation"),M->GetSnapshot().Residence.Residents,P->Residents);
  Test->TestEqual(TEXT("Displayed capacity equals simulation"),M->GetSnapshot().Residence.Capacity,P->ResidenceCapacity);
  Test->TestEqual(TEXT("Displayed history duration matches selected home"),M->GetSnapshot().Residence.CoveredMinutes,P->Consumption.CoveredMinutes);
  for(const auto& D:M->GetSnapshot().Residence.Needs){
   if(D.bService){
    const auto* N=P->Needs.FindByPredicate([&](const auto& X){return FName(*X.NeedId.ToString())==D.NeedId;});
    if(N)Test->TestEqual(TEXT("Services remain current"),D.Fulfillment,N->SatisfactionBasisPoints);
   }else{
    int64 Required=0,Consumed=0;
    for(const auto& T:P->Consumption.Goods)if(FName(*T.GoodId.ToString())==D.GoodId){Required=T.Required;Consumed=T.Consumed;}
    Test->TestEqual(TEXT("Residence required quantity is authoritative"),D.Required,Required);
    Test->TestEqual(TEXT("Residence consumed quantity is authoritative"),D.Consumed,Consumed);
    if(D.bKnown&&Required==0)Test->TestEqual(TEXT("No demand shows a dash"),D.Percent.ToString(),FString(TEXT("—")));
   }
  }
  if(Stage==4){
   if(M->GetSnapshot().Residence.Needs.IsEmpty()){Test->AddError(TEXT("No needs to hover"));return true;}
   FString S=M->GetSnapshot().Residence.Needs[0].NeedId.ToString();S.RemoveFromStart(TEXT("Need."));auto Owner=Root->GetInspector()->ResolveSemanticWidget(TEXT("Inspector.Residence.Need.")+S);
   if(!Test->TestTrue(TEXT("Need has explanatory native tooltip"),Owner.IsValid()&&Owner->GetToolTip().IsValid()&&!Owner->GetToolTip()->IsEmpty()))return true;
   if(!Hovered){auto Pos=Owner->GetCachedGeometry().GetAbsolutePosition()+Owner->GetCachedGeometry().GetAbsoluteSize()*.5;Slate.SetCursorPos(Pos);Slate.ProcessMouseMoveEvent(FPointerEvent(0,Pos,Original,{},EKeys::Invalid,0,FModifierKeysState()));Hovered=true;Ready=FPlatformTime::Seconds();return false;}
   Slate.UpdateToolTip(true);
   auto Tip=Owner->GetToolTip()->AsWidget();TArray<FColor> Pixels;FIntVector Size;TArray64<uint8> Png;
   if(Slate.TakeScreenshot(Tip,Pixels,Size)){FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("ResidenceInspector")),true);FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectSavedDir()/TEXT("ResidenceInspector/need-tooltip.png")));}
  }
  TArray<FColor> Pixels;FIntVector Size;TArray64<uint8> Png;
  if(!Slate.TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native residence screenshot failed"));return true;}
  const TCHAR* Names[]={TEXT("initial"),TEXT("evaluated"),TEXT("details"),TEXT("accessible"),TEXT("hover"),TEXT("empty"),TEXT("30-days")};
  auto Dir=FPaths::ProjectSavedDir()/TEXT("ResidenceInspector");IFileManager::Get().MakeDirectory(*Dir,true);
  auto Base=Dir/FString::Printf(TEXT("residence-%dx%d-%s"),Size.X,Size.Y,Names[Stage]);FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence=TEXT("id\tvisible\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("Inspector."))){Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   if(N.Id==TEXT("Inspector.Root"))Test->TestTrue(TEXT("Residence panel fits the real viewport"),N.Bounds.Min.X>=0&&N.Bounds.Min.Y>=0&&N.Bounds.Max.X<=Size.X&&N.Bounds.Max.Y<=Size.Y);
  }
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage==7;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false,SavedCursor=false,Hovered=false;FVector2D Original;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResidenceViewport,"Hansa.UI.ResidenceInspector.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaResidenceViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FResidenceCapture(this));return true;}
#endif
