#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaTradeStationPresentation.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SViewport.h"

namespace {
class FMinimapCapture final : public IAutomationLatentCommand {
public:
 explicit FMinimapCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Campaign minimap viewport timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=W->GetFirstPlayerController();auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto* P=Cast<AHansaStrategyCameraPawn>(C->GetPawn());if(!P)return false;
  if(!Prepared){
   if(!Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(W)){Test->AddError(TEXT("Run on L_HansaWorld_WP"));return true;}
   float RequestedScale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),RequestedScale);
   Root->SetPreferences({RequestedScale>1,RequestedScale>1,RequestedScale>1,RequestedScale});
   Test->TestEqual(TEXT("Requested UI scale applied"),Root->GetPreferences().UiScale,RequestedScale);
   H->GetScenarioPresentationModel()->Close();P->bEnableMouseEdgePan=false;P->ClearCameraIntents();
   for(TActorIterator<ALandscapeProxy> It(W);It;++It)Bounds+=It->GetComponentsBoundingBox(true);
   Test->TestTrue(TEXT("Whole campaign landscape is loaded"),Bounds.IsValid&&Bounds.GetSize().X>7000000&&Bounds.GetSize().Y>6000000);
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<3)return false;
  auto Map=Root->ResolveSemanticWidget(TEXT("HUD.Minimap"));if(!Map){Test->AddError(TEXT("Missing minimap"));return true;}
  const auto Geometry=Map->GetCachedGeometry();
  Test->TestTrue(TEXT("Minimap remains square"),FMath::IsNearlyEqual(Geometry.GetLocalSize().X,Geometry.GetLocalSize().Y));
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
  const FString Dir=FPaths::ProjectDir()/TEXT("Docs/Images/UI/Minimap");IFileManager::Get().MakeDirectory(*Dir,true);
  FString Scale;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);if(Scale.IsEmpty())Scale=TEXT("1");
  const FString Base=Dir/FString::Printf(TEXT("northern-europe--ingame--%dx%d--scale%s"),Size.X,Size.Y,*Scale);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved actual viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  const auto Original=P->GetViewState();
  const FVector2D Center(Bounds.GetCenter());const double Span=FMath::Max(Bounds.GetSize().X,Bounds.GetSize().Y)*1.08;
  auto Hover=[&](FVector2D N){
   const FVector2D Position=Geometry.LocalToAbsolute(N*Geometry.GetLocalSize());
   Map->OnMouseMove(Geometry,FPointerEvent(0,Position,Position,{},EKeys::Invalid,0,FModifierKeysState()));
   return StaticCastSharedRef<SToolTip>(Map->GetToolTip()->AsWidget())->GetTextTooltip().ToString();
  };
  const auto* Trade=H->GetTradeMapPresentationModel();
  Test->TestTrue(TEXT("Campaign cities are available independently of trade panel"),Trade&&Trade->GetMapCities().Num()>=30);
  auto Normalized=[&](FVector2D World){return FVector2D(.5)+(World-Center)/Span;};
  Test->TestEqual(TEXT("Lubeck marker tooltip preserves accented city name"),Hover(Normalized(FVector2D(Hansa::Game::LubeckPlacementGrid::CampaignLubeckCenter()))),FString(TEXT("Lübeck")));
  const FVector2D Rostock(AHansaTradeStationPresentation::SiteTransform(W).TransformPosition(FVector(60000,0,100)));
  Test->TestEqual(TEXT("Nearby Rostock marker resolves its own name"),Hover(Normalized(Rostock)),FString(TEXT("Rostock")));
  Test->TestEqual(TEXT("Moving to empty map clears city tooltip"),Hover({.1,.1}),FString(TEXT("Map: click to move camera. Arrow keys pan; + and - zoom the map.")));
  Map->OnMouseLeave(FPointerEvent());
  auto Click=[&](FVector2D N){
   const FVector2D Position=Geometry.LocalToAbsolute(N*Geometry.GetLocalSize());
   Map->OnMouseButtonDown(Geometry,FPointerEvent(0,Position,Position,{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
  };
  Click({.5,.5});Test->TestTrue(TEXT("Default minimap click reaches full landscape centre"),P->GetFocusLocation2D().Equals(Center,1));
  Click({.25,.5});Test->TestTrue(TEXT("West is left in north-up map"),P->GetFocusLocation2D().Equals(Center-FVector2D(Span*.25,0),1));
  Click({.5,.25});Test->TestTrue(TEXT("North is up in north-up map"),P->GetFocusLocation2D().Equals(Center-FVector2D(0,Span*.25),1));
  P->RestoreViewState(Original);
  const FVector Lubeck=Hansa::Game::LubeckPlacementGrid::CampaignLubeckCenter();
  P->FocusWorldLocationIntent(Lubeck);
  for(int32 I=0;I<24;++I)Root->ActivateSemanticId(TEXT("HUD.Minimap.ZoomIn"));
  Test->TestEqual(TEXT("City tooltip remains aligned at city-scale zoom"),Hover({.5,.5}),FString(TEXT("Lübeck")));
  Test->TestTrue(TEXT("Offscreen city does not leave a stale tooltip"),Hover({.95,.95})!=TEXT("Rostock"));
  Map->OnMouseLeave(FPointerEvent());
  Click({.6,.5});const double ZoomedOffset=P->GetFocusLocation2D().X-Lubeck.X;
  Test->TestTrue(TEXT("Campaign can zoom from regional to city scale"),ZoomedOffset>0&&ZoomedOffset<5000);
  P->RestoreViewState(Original);
  for(int32 I=0;I<40;++I)Root->ActivateSemanticId(TEXT("HUD.Minimap.ZoomOut"));
  Click({.5,.5});Test->TestTrue(TEXT("Zoom out returns to whole map even away from home"),P->GetFocusLocation2D().Equals(Center,1));
  P->RestoreViewState(Original);
  FString Evidence=FString::Printf(TEXT("LandscapeBounds=%s\nSpan=%.0f\nNorthUp=true\n"),*Bounds.ToString(),Span);
  for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("HUD.Minimap")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y);
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));return true;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;bool Prepared=false;FBox Bounds=FBox(ForceInit);
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMinimapViewport,"Hansa.UI.Minimap.CampaignRealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaMinimapViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FMinimapCapture(this));return true;}
#endif
