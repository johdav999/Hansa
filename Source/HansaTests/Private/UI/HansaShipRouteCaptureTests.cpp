#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/HansaCargoProjectionManager.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Input/SEditableTextBox.h"
namespace
{
class FShipRouteCapture final : public IAutomationLatentCommand
{
public:
 explicit FShipRouteCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Ship route viewport timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto* M=H->GetTradeMapPresentationModel();
  if(H->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  if(!Prepared)
  {
   if(Stage==0)
   {
    FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
    bool FoundShip=false;
    for(TActorIterator<AHansaCargoProjectionManager> It(W);It&&!FoundShip;++It)for(const auto& Observation:It->QueryCargo())if(Observation.JobId.IsEmpty()&&FCString::Atoi64(*Observation.VehicleId)==1){H->GetInspectorPresentationModel()->ShowCargo(Observation,Observation.SemanticId);FoundShip=true;break;}
    if(!FoundShip)return false;
    Test->TestTrue(TEXT("World ship inspector Route opens that ship’s detail"),Root->ActivateSemanticId(TEXT("Inspector.Ship.Route")));
    Test->TestTrue(TEXT("Correct ship-owned route tab opens"),M->bShipDetailOpen&&M->ShipDetailTab==TEXT("Route")&&M->GetSnapshot().SelectedVehicleValue==1);
   }
   if(Stage==1)
   {
    Test->TestTrue(TEXT("Load cell receives keyboard focus"),Root->FocusSemanticId(TEXT("TradeMap.Cargo.1.0.Load")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Right arrow follows physical cargo columns"),M->GetSnapshot().FocusedSemanticId,FName(TEXT("TradeMap.Cargo.1.1.Load")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_DPad_Up,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_DPad_Up,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Controller cannot focus disabled empty unload"),M->GetSnapshot().FocusedSemanticId,FName(TEXT("TradeMap.Cargo.1.1.Load")));
    Root->FocusSemanticId(TEXT("TradeMap.Cargo.1.0.Load"));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestTrue(TEXT("Keyboard opens load selector"),M->CargoEditor.bOpen&&M->CargoEditor.bLoad);
   }
   if(Stage==2)
   {
    Test->TestTrue(TEXT("Quantity receives keyboard focus"),Root->FocusSemanticId(TEXT("TradeMap.Cargo.Quantity")));
    auto Input=StaticCastSharedPtr<SEditableTextBox>(Root->ResolveSemanticWidget(TEXT("TradeMap.Cargo.Quantity")));Input->SelectAllText();
    FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(TEXT('7'),FModifierKeysState(),0,false));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Native numeric entry edits popup quantity"),M->CargoEditor.Draft.QuantityLimit.GetRawValue(),int64(7000));
    M->CancelCargoCell();
    Test->TestTrue(TEXT("Unload receives controller focus"),Root->FocusSemanticId(TEXT("TradeMap.Cargo.0.0.Unload")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    Test->TestTrue(TEXT("Controller opens unload selector"),M->CargoEditor.bOpen&&!M->CargoEditor.bLoad);
   }
   if(Stage==3){M->CancelCargoCell();Test->TestTrue(TEXT("Cancel restores initiating cell focus"),FSlateApplication::Get().GetKeyboardFocusedWidget()==Root->ResolveSemanticWidget(TEXT("TradeMap.Cargo.0.0.Unload")));}
   if(Stage==4){
    auto Button=Root->ResolveSemanticWidget(TEXT("TradeMap.Cargo.0.1.Unload"));
    auto& Slate=FSlateApplication::Get();const auto G=Button->GetCachedGeometry();const auto Pos=G.GetAbsolutePosition()+G.GetAbsoluteSize()*.5;
    Slate.SetCursorPos(Pos);Slate.ProcessMouseMoveEvent(FPointerEvent(0,Pos,Pos,{},EKeys::Invalid,0,FModifierKeysState()));
    const auto Hit=Slate.LocateWindowUnderMouse(Pos,Slate.GetInteractiveTopLevelWindows());
    Test->TestTrue(TEXT("Real hit-test path reaches the matrix behind the disabled cell"),Hit.IsValid()&&Hit.ContainsWidget(Root.Get()));
    if(Hit.IsValid())for(int32 Index=0;Index<Hit.Widgets.Num();++Index)Test->AddInfo(FString::Printf(TEXT("Cargo pointer path: %s"),*Hit.Widgets[Index].Widget->GetTypeAsString()));
    Slate.ProcessMouseButtonDownEvent(nullptr,FPointerEvent(0,Pos,Pos,{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
    Slate.ProcessMouseButtonUpEvent(FPointerEvent(0,Pos,Pos,{},EKeys::LeftMouseButton,0,FModifierKeysState()));
    Test->TestFalse(TEXT("Empty physical slot unload is disabled"),Button->IsEnabled());
    Test->TestFalse(TEXT("Mouse cannot open an empty unload slot"),M->CargoEditor.bOpen);
    Test->TestFalse(TEXT("Automation cannot open an empty unload slot"),Root->ActivateSemanticId(TEXT("TradeMap.Cargo.0.1.Unload")));
   }
   if(Stage==5)
   {
    auto* GameMode=Cast<AHansaGameMode>(W->GetAuthGameMode());
    auto* Host=GameMode?GameMode->GetSimulationHost():nullptr;
    if(!Host){Test->AddError(TEXT("Runtime host unavailable"));return true;}
    auto Stops=Host->BuildProjection().Value.GetRoutes()[0].Stops;
    for(auto& Stop:Stops)for(auto& Action:Stop.Actions)Action.MinimumSourceReserve=Hansa::Simulation::FHansaQuantity();
    Test->TestTrue(TEXT("Prepare ungated cargo plan"),Host->EditRoute(Hansa::Simulation::FHansaRouteId::TryCreate(1).Value,Stops).IsSuccess());
    M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
    Test->TestTrue(TEXT("Start active route for save journey"),M->ToggleActiveIntent());
    Test->TestTrue(TEXT("Begin current voyage"),Host->AdvanceTicks(1));
    M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
    Test->TestTrue(TEXT("Open plank load while moving"),Root->ActivateSemanticId(TEXT("TradeMap.Cargo.0.1.Load")));
    Test->TestTrue(TEXT("Select planks in real screen"),Root->ActivateSemanticId(TEXT("TradeMap.Cargo.Product.Good.Planks")));
    Test->TestTrue(TEXT("Set twenty planks"),M->SetCargoQuantity(20000));
    Test->TestTrue(TEXT("Confirm real load selector"),Root->ActivateSemanticId(TEXT("TradeMap.Cargo.Confirm")));
    Test->TestTrue(TEXT("Open planned plank unload"),Root->ActivateSemanticId(TEXT("TradeMap.Cargo.1.1.Unload")));
    Test->TestTrue(TEXT("Set thirty plank unload limit"),M->SetCargoQuantity(30000));
    Test->TestTrue(TEXT("Confirm real unload selector"),Root->ActivateSemanticId(TEXT("TradeMap.Cargo.Confirm")));
   }
   if(Stage==6)
   {
    auto* GameMode=Cast<AHansaGameMode>(W->GetAuthGameMode());
    auto* Host=GameMode?GameMode->GetSimulationHost():nullptr;
    if(!Host){Test->AddError(TEXT("Runtime host unavailable"));return true;}
    const auto Before=Host->BuildProjection().Value.GetRoutes()[0];
    auto Save=Root->ResolveSemanticWidget(TEXT("TradeMap.Ship.Save"));
    auto& Slate=FSlateApplication::Get();const auto Geometry=Save->GetCachedGeometry();const auto Pos=Geometry.GetAbsolutePosition()+Geometry.GetAbsoluteSize()*.5;
    Slate.SetCursorPos(Pos);Slate.ProcessMouseMoveEvent(FPointerEvent(0,Pos,Pos,{},EKeys::Invalid,0,FModifierKeysState()));
    const auto Hit=Slate.LocateWindowUnderMouse(Pos,Slate.GetInteractiveTopLevelWindows());
    Test->TestTrue(TEXT("Pointer reaches Save changes"),Hit.IsValid()&&Hit.ContainsWidget(Save.Get()));
    Slate.ProcessMouseButtonDownEvent(nullptr,FPointerEvent(0,Pos,Pos,{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
    Slate.ProcessMouseButtonUpEvent(FPointerEvent(0,Pos,Pos,{},EKeys::LeftMouseButton,0,FModifierKeysState()));
    Test->TestFalse(TEXT("Real Save click commits moving route"),M->GetSnapshot().bDirty);
    Test->TestTrue(TEXT("Save confirms next-stop activation"),M->GetSnapshot().EditorStatus.ToString().Contains(TEXT("next stop")));
    const auto After=Host->BuildProjection().Value.GetRoutes()[0];
    Test->TestEqual(TEXT("Real save preserves voyage"),After.RemainingTravelTicks,Before.RemainingTravelTicks-1);
    Test->TestTrue(TEXT("Real save stores plank load"),After.Stops[0].Actions.ContainsByPredicate([](const auto& A){return A.CargoSlotIndex==1&&A.GoodId.ToString()==TEXT("Good.Planks");}));
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<1)return false;
  if(Stage==3){++Stage;Prepared=false;return false;}
  const auto Nodes=Root->GetSemanticSnapshot();const FString Primary=Stage==0||Stage==4||Stage==5||Stage==6?TEXT("TradeMap.Ship.Save"):TEXT("TradeMap.Cargo.Confirm");const auto* PrimaryNode=Nodes.FindByPredicate([&](const auto& N){return N.Id==Primary;});Test->TestTrue(TEXT("Primary action remains visible without clipping"),PrimaryNode&&PrimaryNode->State.bVisible&&!PrimaryNode->State.bClipped);
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Screenshot failed"));return true;}
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("ShipRouteEditor");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("actual-%dx%d-scale-%.1f-%s"),Size.X,Size.Y,Scale,Stage==0?TEXT("ship-matrix"):Stage==1?TEXT("load"):Stage==4?TEXT("empty-slot-unload"):Stage==5?TEXT("moving-route-draft"):Stage==6?TEXT("moving-route-saved"):TEXT("unload"));
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Write actual game capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("TradeMap.Cargo."))||N.Id.StartsWith(TEXT("TradeMap.Ship.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));if(Stage==6)return true;if(Stage==4)M->CancelCargoCell();++Stage;Prepared=false;return false;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;float Scale=1;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteViewport,"Hansa.TradeRoute.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FShipRouteViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FShipRouteCapture(this));return true;}
#endif
