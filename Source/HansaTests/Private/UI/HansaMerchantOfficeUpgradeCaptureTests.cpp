#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/Text/STextBlock.h"

namespace {
class FMerchantOfficeUpgradeCapture : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false;
public:
 explicit FMerchantOfficeUpgradeCapture(FAutomationTestBase* In):Test(In){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(TEXT("Merchant Office native capture timed out"));return true;}
  auto* View=GEngine?GEngine->GameViewport.Get():nullptr;auto* World=View?View->GetWorld():nullptr;
  auto* Controller=World?Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController()):nullptr;
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Front=Hud->GetFrontendPresentationModel();
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Game=World->GetAuthGameMode<AHansaGameMode>();auto* Host=Game?Game->GetSimulationHost():nullptr;auto* Model=Hud->GetTradeMapPresentationModel();
  auto* Camera=Cast<AHansaStrategyCameraPawn>(Controller->GetPawn());if(!Host||!Camera)return false;
  if(!Prepared){
   if(Stage==0){
    Hud->GetScenarioPresentationModel()->DismissHelp();Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);FString Error;
    if(!Host->ApplyMerchantOfficeTestSetup(Error)){Test->AddError(Error);return true;}
    const auto P=Host->BuildProjection().Value;const auto* Station=P.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.OwnerId==Host->GetHouseId()&&S.Station.CityId.ToString()==TEXT("City.Rostock");});
    if(!Station){Test->AddError(TEXT("Native office fixture has no station"));return true;}
    Model->OpenWorldStation(TEXT("City.Rostock"),Station->Station.Id.GetValue());Root->ActivateSemanticId(TEXT("TradeMap.Station.ShowOnMap"));
    Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->MinimumZoomDistance=1000;Camera->AddZoomIntent((Camera->GetZoomDistance()-8500)/Camera->ZoomUnitsPerStep);
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});Root->ActivateSemanticId(TEXT("HUD.TopStatus.Speed.Pause"));
   }
   if(Stage==1){
    Test->TestTrue(TEXT("Overview tab opens lease disclosure"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
    Test->TestTrue(TEXT("Station terms open through native control"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
    Test->TestTrue(TEXT("Rights derive from authoritative capabilities"),Model->GetSnapshot().Establishment.Rights.Num()>0);
   }
   if(Stage==2){
    Test->TestTrue(TEXT("Station terms return to inspector"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Return")));
    AHansaTradeStationPresentation* Station=nullptr;for(TActorIterator<AHansaTradeStationPresentation> It(World);It;++It)if(It->GetStationId()==Model->GetSnapshot().Establishment.StationId){Station=*It;break;}
    if(!Station)return false;Model->CloseIntent();FHitResult Hit;
    Test->TestTrue(TEXT("World station can be selected by physical trace"),World->LineTraceSingleByChannel(Hit,Station->GetActorLocation()+FVector(0,0,3000),Station->GetActorLocation(),ECC_Visibility));Controller->OnWorldSelectionChanged.Broadcast(Hit.GetActor(),Hit);
    Test->TestTrue(TEXT("World click opens upgrade in station details"),Model->bWorldStationDetail&&Model->IsAutomaticMerchantOfficeUpgrade());
    Test->TestTrue(TEXT("Native dedicated Upgrade tab opens"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Upgrade")));
    Test->TestTrue(TEXT("Native Upgrade action opens review"),Root->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
    Host->AdvanceTicks(1);Test->TestTrue(TEXT("Review survives live authoritative tick"),Model->GetSnapshot().bPresenceReview);
   }
   if(Stage==3){Test->TestTrue(TEXT("Native confirmation funds the office"),Root->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));Test->TestFalse(TEXT("Confirmed payment disables repeated action"),Model->GetSnapshot().bCanPresenceUpgradeAction);Test->TestTrue(TEXT("Native construction reports remaining time"),Model->GetSnapshot().PresenceFundingDetail.ToString().Contains(TEXT("Ready in")));}
   if(Stage==4){Host->AdvanceTicks(4);const auto P=Host->BuildProjection().Value;Test->TestTrue(TEXT("Office completed in native world"),P.GetForeignPresences().ContainsByPredicate([&](const auto& Presence){return Presence.HouseId==Host->GetHouseId()&&Presence.CityId.ToString()==TEXT("City.Rostock")&&Presence.CurrentStageId==TEXT("PresenceStage.MerchantOffice");}));
    const int64 Id=Model->GetSnapshot().Establishment.StationId;AHansaTradeStationPresentation* Office=nullptr;for(TActorIterator<AHansaTradeStationPresentation> It(World);It;++It)if(It->GetStationId()==Id){Office=*It;break;}
    if(!Office){Test->AddError(TEXT("Completed office actor missing"));return true;}
    Model->CloseIntent();FHitResult Hit;Test->TestTrue(TEXT("Completed office remains physically selectable"),World->LineTraceSingleByChannel(Hit,Office->GetActorLocation()+FVector(0,0,3000),Office->GetActorLocation(),ECC_Visibility));Controller->OnWorldSelectionChanged.Broadcast(Hit.GetActor(),Hit);
    Test->TestTrue(TEXT("Reselect opens completed Merchant Office details"),Model->bWorldStationDetail&&Model->GetSnapshot().Establishment.bOfficeBuilt);
    const auto* Station=P.GetTradeStations().FindByPredicate([&](const auto& S){return int64(S.Station.Id.GetValue())==Id;});
    if(Station)Test->TestEqual(TEXT("Reselected office shows authoritative upgraded storage"),Model->GetSnapshot().Establishment.Storage.ToString(),FText::Format(FText::FromString(TEXT("{0} units storage")),FText::AsNumber(double(Station->StorageCapacity.GetRawValue())/1000.)).ToString());else Test->AddError(TEXT("Completed office projection missing"));
    auto Screen=Root->ResolveSemanticWidget(TEXT("TradeMap.Station.Identity"));if(Test->TestTrue(TEXT("Native office identity widget is reachable"),Screen.IsValid()))Test->TestTrue(TEXT("Native header identifies upgraded office"),StaticCastSharedPtr<STextBlock>(Screen)->GetText().ToString().Contains(TEXT("Merchant Office")));
   }
   if(Stage==5)Test->TestTrue(TEXT("Office terms open through shared control"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
   if(Stage==6)Test->TestTrue(TEXT("Controller focus reveals closure review"),Root->FocusSemanticId(TEXT("TradeMap.Station.Close")));
   Ready=FPlatformTime::Seconds()+3;Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native office screenshot failed"));return true;}
  if(Stage==2){const auto Nodes=Root->GetSemanticSnapshot();for(const TCHAR* Id:{TEXT("TradeMap.Presence.Upgrade"),TEXT("TradeMap.Presence.Cancel")}){const auto* N=Nodes.FindByPredicate([&](const auto& Node){return Node.Id==Id;});Test->TestTrue(TEXT("Review actions remain visible with usable hit areas"),N&&N->State.bVisible&&N->Bounds.Height()>=48&&N->Bounds.Min.Y>=0&&N->Bounds.Max.Y<=Size.Y);}}
  if(Stage==6){const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([](const auto& Node){return Node.Id==TEXT("TradeMap.Station.Close");});Test->TestTrue(TEXT("Scrolled closure review remains usable"),N&&N->State.bVisible&&N->Bounds.Height()>=48&&N->Bounds.Min.Y>=0&&N->Bounds.Max.Y<=Size.Y);}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);const TCHAR* States[]={TEXT("available"),TEXT("station-lease"),TEXT("review"),TEXT("construction"),TEXT("complete"),TEXT("office-lease"),TEXT("office-lease-closure")};
  const FString Dir=FPaths::ProjectDir()/TEXT("Docs/Images/UI/MerchantOfficeUpgrade");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("native-details--%s--%dx%d--scale%.1f"),States[Stage],Size.X,Size.Y,Scale);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Native capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("Fixture=isolated new campaign, authoritative placed Rostock station, local materials\nRole=owning house\nInput=physical world trace then shared native semantic actions\n");
  for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("TradeMap."))&&N.State.bVisible)Evidence+=FString::Printf(TEXT("%s\t%d,%d,%d,%d\t%s\n"),*N.Id,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));++Stage;Prepared=false;return Stage==7;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMerchantOfficeUpgradeViewport,"Hansa.UI.MerchantOfficeUpgrade.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FMerchantOfficeUpgradeViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FMerchantOfficeUpgradeCapture(this));return true;}
#endif
