#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeEstablishmentTestSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"

namespace {
class FConstructionDeliveryCapture : public IAutomationLatentCommand {
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Fixture;
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false;
public:
 explicit FConstructionDeliveryCapture(FAutomationTestBase* In):Test(In){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>240){Test->AddError(TEXT("Construction delivery capture timed out"));return true;}
  auto* View=GEngine?GEngine->GameViewport.Get():nullptr;auto* World=View?View->GetWorld():nullptr;
  auto* Controller=World?Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController()):nullptr;
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Front=Hud->GetFrontendPresentationModel();
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Game=World->GetAuthGameMode<AHansaGameMode>();auto* WorldHost=Game?Game->GetSimulationHost():nullptr;auto* Host=Fixture.IsValid()?Fixture.Get():WorldHost;auto* Model=Hud->GetTradeMapPresentationModel();if(!Host||!Model)return false;
  using namespace Hansa::Simulation;
  const FString Focus=Stage==1?TEXT("TradeMap.Station.Confirm"):TEXT("TradeMap.Station.Action");
  if(!Prepared){
   if(Stage==0){
    Hud->GetScenarioPresentationModel()->DismissHelp();WorldHost->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);FString Error;
    // Controlled native UI fixture: canonical completed facilities use the same authoritative commands.
    // The survey world remains independent; no user campaign or world terrain is changed.
    Fixture=TStrongObjectPtr<UHansaRuntimeSimulationHost>(NewObject<UHansaRuntimeSimulationHost>());Host=Fixture.Get();
    if(!Host->InitializeForLubeck(nullptr,Error)){Test->AddError(Error);return true;}Model->BindRuntime(Host);Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    if(!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[](FHansaSimulationInitialization& Init){
     auto& Cargo=Init.Inventories[1];const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;Cargo.InitialStock.Reset();Cargo.AcceptedGoods.Add(Grain);
     const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
     auto& City=Init.Inventories[3];City.AcceptedGoods={Timber,Planks,Grain};City.Capacity=FHansaQuantity::FromRaw(1000000);City.InitialStock={{Timber,FHansaQuantity::FromRaw(100000)},{Planks,FHansaQuantity::FromRaw(100000)},{Grain,FHansaQuantity::FromRaw(500000)}};
     auto& Destination=Init.Inventories[4];Destination.AcceptedGoods.Add(Grain);Destination.Capacity=FHansaQuantity::FromRaw(1000000);FHansaCityMarketInitialization M;M.CityId=Destination.CityId;M.GoodId=Grain;M.InventoryIds={Destination.Id};M.InitialPriceMilliMarks=1000;Init.Markets.Add(M);
     FHansaRouteState R;const auto& V=Init.Vehicles[1];R.Id=FHansaRouteId::TryCreate(1).Value;R.OwnerId=V.OwnerId;R.VehicleId=V.Id;R.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;R.Mode=EHansaRouteMode::Sea;R.Lifecycle=EHansaRouteLifecycleState::AtStop;R.bPendingStopActions=true;
     FHansaRouteCargoAction Load;Load.Kind=EHansaRouteCargoActionKind::OwnedCityLoad;Load.GoodId=Grain;Load.QuantityLimit=FHansaQuantity::FromRaw(60000);Load.CargoSlotIndex=0;auto Unload=Load;Unload.Kind=EHansaRouteCargoActionKind::Unload;R.Stops={{V.CurrentCityId,{Load}},{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,{Unload}}};Init.Routes.Add(R);
    },true)){Test->AddError(Error);return true;}
    FHansaPlacementSpec Spec;Spec.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.TradeHouse")).Value;bool Found=false;
    for(int32 X=8;X<19&&!Found;++X)for(int32 Y=8;Y<19;++Y){Spec.Anchor={X,Y};if(Host->TradeHousePlacementError(Spec).IsEmpty()){Found=true;break;}}
    if(!Found||!Host->PlaceBuildings({Spec}).IsSuccess()){Test->AddError(TEXT("Could not place capture station"));return true;}
    Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));Model->SelectSectionIntent(TEXT("Presence"));Model->OpenWorldStation(TEXT("City.Rostock"),Host->BuildProjection().Value.GetTradeStations()[0].Station.Id.GetValue());
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});
    Test->TestTrue(TEXT("Arrange through actual visible action"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Action")));Test->TestTrue(TEXT("Full recurring plan gives remedy"),Model->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("Waiting alone")));
   }
   if(Stage==1){Test->TestTrue(TEXT("Open native priority review"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Priority")));Test->TestTrue(TEXT("Separate review is visible"),Model->GetSnapshot().Establishment.bReview);}
   if(Stage==2){Test->TestTrue(TEXT("Confirm reviewed priority"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Confirm")));for(int32 T=0;T<24&&Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty();++T)Host->AdvanceTicks(1);Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Test->TestTrue(TEXT("Materials remain physically aboard"),!Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty());}
   if(Stage==3){Host->AdvanceTicks(40);Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Test->TestTrue(TEXT("Completed operations view"),Model->GetSnapshot().Establishment.bComplete);}
   Test->TestTrue(TEXT("Keyboard focus reaches workflow control"),Root->FocusSemanticId(Focus));Ready=FPlatformTime::Seconds()+1;Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  const auto Nodes=Root->GetSemanticSnapshot();const auto* Control=Nodes.FindByPredicate([&](const auto& N){return N.Id==Focus;});Test->TestTrue(TEXT("Focused action remains visible with usable height"),Control&&Control->State.bVisible&&Control->Bounds.Height()>=40);
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native delivery screenshot failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);const TCHAR* States[]={TEXT("waiting-full-plan"),TEXT("priority-review"),TEXT("materials-aboard"),TEXT("complete")};
  const FString Dir=FPaths::ProjectDir()/TEXT("Docs/Images/UI/ConstructionDelivery");IFileManager::Get().MakeDirectory(*Dir,true);const FString Base=Dir/FString::Printf(TEXT("delivery--%s--%dx%d--scale%.1f"),States[Stage],Size.X,Size.Y,Scale);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("Role=local house\nFixture=canonical isolated simulation host with completed dock, market and roads; native survey viewport UI; normal route fills hold\nTick=%lld\n"),Host->BuildProjection().Value.GetClock().GetTick().GetValue());for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.Station.")))Evidence+=FString::Printf(TEXT("%s\tVisible=%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));
  ++Stage;Prepared=false;if(Stage==4){Model->BindRuntime(WorldHost);Model->ApplyProjection(WorldHost->BuildProjection().Value,*WorldHost->GetEconomicRegistry());}return Stage==4;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstructionDeliveryViewport,"Hansa.UI.ConstructionDelivery.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FConstructionDeliveryViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FConstructionDeliveryCapture(this));return true;}
#endif
