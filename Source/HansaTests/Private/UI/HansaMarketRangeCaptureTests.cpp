#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"
namespace
{
class FMarketRangeCapture final : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;
 Hansa::Simulation::FHansaPlacementSpec MarketSpec;
public:
 explicit FMarketRangeCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  using namespace Hansa::Simulation;
  if(FPlatformTime::Seconds()-Start>120){Test->AddError(TEXT("Market range viewport timed out"));return true;}
  auto* V=GEngine?GEngine->GameViewport.Get():nullptr;auto* W=V?V->GetWorld():nullptr;
  auto* C=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  auto* GM=W?W->GetAuthGameMode<AHansaGameMode>():nullptr;auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!Hud||!Host||!Host->IsReady()||HansaWaitForFrontend(Hud))return false;
  if(Stage==0)
  {
   Hud->GetScenarioPresentationModel()->Close();Hud->GetBuildMenuPresentationModel()->SetOpen(false);
   Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
   Hud->GetRootWidget()->SetPreferences({});
   const auto* Map=Host->FindPlacementMap();if(!Map)return false;
   const auto* Market=Host->FindBuildingDefinition(TEXT("Building.Market"));
   const auto* Bakery=Host->FindBuildingDefinition(TEXT("Building.Bakery"));
   if(!Market||!Bakery){Test->AddError(TEXT("Required definitions missing"));return true;}
   bool Placed=false;
   for(const auto& Cell:Map->Cells)
   {
    FHansaPlacementSpec B;B.CityId=Host->GetCityId();B.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery")).Value;
    B.Anchor={Cell.Coordinate.X+2,Cell.Coordinate.Y+1};
    MarketSpec=B;MarketSpec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Market")).Value;
    MarketSpec.Anchor={Cell.Coordinate.X+2,Cell.Coordinate.Y-Market->FootprintHeightCells};
    auto MayPlace=[&](const FHansaPlacementSpec& S){const auto R=Host->ValidatePlacement(S);return R.CanPlace()||(R.GetReasons().Num()==1&&R.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired);};
    if(!MayPlace(B)||!MayPlace(MarketSpec))continue;
    TArray<FHansaPlacementSpec> Roads;bool Valid=true;
    for(int32 X=0;X<8;++X){auto R=B;R.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;R.Anchor={Cell.Coordinate.X+X,Cell.Coordinate.Y};Valid&=Host->ValidatePlacement(R).CanPlace();Roads.Add(R);}
    if(!Valid)continue;
    if(!Test->TestTrue(TEXT("Road placed through gameplay command"),Host->PlaceBuildings(Roads).IsSuccess()))return true;
    Host->AdvanceTicks(5);
    if(!Test->TestTrue(TEXT("Bakery placed through gameplay command"),Host->PlaceBuildings(MakeArrayView(&B,1)).IsSuccess()))return true;
    Host->AdvanceTicks(Bakery->BuildTicks+2);Placed=true;break;
   }
   if(!Test->TestTrue(TEXT("Found valid ordinary construction site"),Placed))return true;
   Stage=1;Ready=FPlatformTime::Seconds();return false;
  }
  AHansaBuildingWorldProjectionActor* Building=nullptr;
  for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Bakery")){Building=*It;break;}
  if(!Building)return false;
  if(Stage==1||Stage==3)
  {
   C->OnWorldSelectionChanged.Broadcast(Building,FHitResult());
   Hud->GetRootWidget()->ActivateSemanticId(TEXT("Session.Help.Hide"));
   if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(Building->GetActorLocation());Camera->AddZoomIntent((Camera->GetZoomDistance()-6500.f)/Camera->ZoomUnitsPerStep);}
   Test->TestEqual(TEXT("Warning agrees with market eligibility"),Building->IsMarketNotInRangeIndicatorVisible(),Stage==1);
   Test->TestEqual(TEXT("Inspector agrees with eligibility"),Hud->GetInspectorPresentationModel()->GetSnapshot().Production.bHasMarketAccess,Stage==3);
   Ready=FPlatformTime::Seconds();++Stage;return false;
  }
  if(FPlatformTime::Seconds()-Ready<1.)return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!Test->TestTrue(TEXT("Real viewport capture"),FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return true;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("MarketRange");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("market-range-%dx%d-%s"),Size.X,Size.Y,Stage==2?TEXT("missing"):TEXT("recovered"));
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("building=%llu\nmarketWarning=%d\n"),Building->GetBuildingId().GetValue(),Building->IsMarketNotInRangeIndicatorVisible());
  for(const auto& N:Hud->GetRootWidget()->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("Inspector.")))Evidence+=N.Id+TEXT("\t")+N.State.Value+TEXT("\n");
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));
  if(Stage==4)return true;
  if(!Test->TestTrue(TEXT("Closer market placed through gameplay command"),Host->PlaceBuildings(MakeArrayView(&MarketSpec,1)).IsSuccess()))return true;
  Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Market"))->BuildTicks+2);
  Stage=3;return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketRangeViewportTest,"Hansa.UI.MarketRange.RealViewport",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaMarketRangeViewportTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FMarketRangeCapture(this));return true;}
#endif
