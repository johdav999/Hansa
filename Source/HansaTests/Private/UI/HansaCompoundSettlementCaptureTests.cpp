#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "World/HansaCompoundPresentation.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "RenderTimer.h"
#include "RHIStats.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaBuildingWorldProjection.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "Engine/GameInstance.h"
#include "Components/ChildActorComponent.h"
#include "ProceduralMeshComponent.h"

namespace HansaCompoundCapture
{
FString Directory(){return FPaths::ProjectSavedDir()/TEXT("CompoundIntegration/PreviewPerformance");}
bool Capture(const FString& Name)
{
 TArray<FColor> Pixels;FIntVector Size(0);TArray64<uint8> Png;
 if(!FSlateApplication::Get().TakeScreenshot(GEngine->GameViewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size))return false;
 FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
 return FFileHelper::SaveArrayToFile(Png,*(Directory()/Name+TEXT(".png")));
}
class FPreviewPerformance final:public IAutomationLatentCommand
{
public:
 explicit FPreviewPerformance(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Compound preview benchmark timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  UWorld* W=GEngine->GameViewport->GetWorld();auto* C=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;auto* Camera=C?Cast<AHansaStrategyCameraPawn>(C->GetPawn()):nullptr;
  if(!Hud||!Camera||HansaWaitForFrontend(Hud))return false;
  if(Phase==0)
  {
   IFileManager::Get().MakeDirectory(*Directory(),true);
   Hud->GetScenarioPresentationModel()->Close();Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Hud->GetBuildMenuPresentationModel()->SetOpen(false);
   for(TActorIterator<AHansaLubeckWorldFoundation> It(W);It;++It){Foundation=*It;break;}
   if(!Test->TestNotNull(TEXT("Live world foundation"),Foundation))return true;
   Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(Foundation->PlacementCellToWorld(20,20));Camera->AddZoomIntent(-100);
   Csv=TEXT("phase,sample,wall_ms,game_ms,render_ms,draw_calls,primitives,process_bytes,world_actors,compound_actors,instances,batches\n");
   Phase=1;Frame=0;Previous=FPlatformTime::Seconds();return false;
  }
  const double Now=FPlatformTime::Seconds();const double Delta=(Now-Previous)*1000;Previous=Now;
  ++Frame;if(Frame<=180)return false;
  if(Frame<=360)
  {
   int32 Actors=0,Compounds=0,Instances=0,Batches=0;for(TActorIterator<AActor> It(W);It;++It)++Actors;
   for(TActorIterator<AHansaCompoundPresentation> It(W);It;++It){++Compounds;Batches+=It->Batches.Num();for(const auto& B:It->Batches)Instances+=B->GetInstanceCount();}
   Csv+=FString::Printf(TEXT("%s,%d,%.4f,%.4f,%.4f,%d,%d,%llu,%d,%d,%d,%d\n"),Phase==1?TEXT("empty"):TEXT("32_preview_compounds"),Frame-180,Delta,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),GNumDrawCallsRHI[0],GNumPrimitivesDrawnRHI[0],static_cast<unsigned long long>(FPlatformMemory::GetStats().UsedPhysical),Actors,Compounds,Instances,Batches);
   return false;
  }
  Test->TestTrue(TEXT("Native benchmark viewport"),Capture(Phase==1?TEXT("baseline-empty"):TEXT("district-32")));
  FFileHelper::SaveStringToFile(Csv,*(Directory()/TEXT("samples.csv")));
  if(Phase==1)
  {
   const TArray<FString> Families={TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")};
   for(int32 Row=0;Row<4;++Row)for(int32 Col=0;Col<8;++Col)
   {
    const FString Path=TEXT("/Game/Hansa/Generated/Staging/LabourCourts_20260915/R03/DA_Compound_")+Families[(Row*3+Col)%4];
    auto* D=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,*Path);
    if(!Test->TestNotNull(TEXT("Reviewed staged definition (isolated preview only)"),D))return true;
    auto* A=W->SpawnActor<AHansaCompoundPresentation>();
    A->SetActorLocation(Foundation->PlacementCellToWorld(10+Row*5,6+Col*4));A->SetActorRotation(FRotator(0,Row%2?180:0,0));
    if(!Test->TestTrue(TEXT("Full-size dense stage-three composition"),A->ApplyCompound(D,UHansaResidentialCompoundDefinition::ParcelSeed(TEXT("City.Lubeck"),1000+Row*8+Col,1),3,Col==0?FName(TEXT("CornerLeft")):Col==7?FName(TEXT("CornerRight")):FName(TEXT("Straight")),TEXT("District.Lubeck.LateMedieval"))))return true;
    Spawned.Add(A);
   }
   Phase=2;Frame=0;return false;
  }
  FString Info=FString::Printf(TEXT("Native rendered Development game; map=%s; viewport=%dx%d; 180 warm-up + 180 sample frames per phase.\n32 isolated staged presentation Actors on real game terrain; no production promotion, building bindings, authoritative placement or live residents.\nBaseline and compound phases share camera. Process resident memory includes newly loaded meshes/materials/shaders; draw calls include the full scene and UI. No numeric project threshold is defined.\n"),*W->GetMapName(),GEngine->GameViewport->Viewport->GetSizeXY().X,GEngine->GameViewport->Viewport->GetSizeXY().Y);
  FFileHelper::SaveStringToFile(Info,*(Directory()/TEXT("method.txt")));
  for(auto& A:Spawned)if(A.IsValid())A->Destroy();return true;
 }
private:
 FAutomationTestBase* Test;double Start,Previous=0;int32 Phase=0,Frame=0;FString Csv;
 AHansaLubeckWorldFoundation* Foundation=nullptr;TArray<TWeakObjectPtr<AHansaCompoundPresentation>> Spawned;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundPreviewPerformance,"Hansa.Compound.PreviewPerformance",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaCompoundPreviewPerformance::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(HansaCompoundCapture::FPreviewPerformance(this));return true;}

namespace HansaCompoundSettlement
{
using namespace Hansa::Simulation;
class FPlayerFlow final:public IAutomationLatentCommand
{
public:
 explicit FPlayerFlow(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update()override
 {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Compound settlement flow timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  UWorld* W=GEngine->GameViewport->GetWorld();auto* C=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;auto* GM=W?W->GetAuthGameMode<AHansaGameMode>():nullptr;auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!Hud||!Host||HansaWaitForFrontend(Hud))return false;
  const int32 HomeWidth=Host->FindBuildingDefinition(HomeId())->FootprintWidthCells;
  auto* Menu=Hud->GetBuildMenuPresentationModel();auto* Inspector=Hud->GetInspectorPresentationModel();auto* Save=Hud->GetSaveLoadPresentationModel();auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn());
  if(!Prepared)
  {
   Hud->GetScenarioPresentationModel()->Close();Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   if(Camera){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();}
   if(Stage==0)
   {
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("CompoundIntegration/PlayerFlow")),true);
    for(const TCHAR* Family:{TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")})
    {
     const FString CardId=FString::Printf(TEXT("Building.Residence.Laborer.%s.Stage1"),Family);
     if(!Test->TestNotNull(TEXT("Reviewed production compound card available"),Menu->FindCardPresentation(FName(*CardId))))return true;
    }
    Hud->GetGameInstance()->GetSubsystem<UHansaSaveSubsystem>()->UseIsolatedAutomationSlots();
    Menu->SetOpen(true);Menu->SelectCategory(EHansaBuildCategory::Residences);
   }
   else if(Stage==1)
   {
    // Discover a viable clear footprint through read-only placement validation.
    // All mutations below use the real build presenter and normal gateway.
    bool Found=false;
    for(int32 Y=0;Y<60&&!Found;++Y)for(int32 X=2;X<60&&!Found;++X)
    {
     FHansaPlacementSpec S;S.CityId=Host->GetCityId();S.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(HomeId()).Value;S.Anchor={X,Y};
     const auto V=Host->ValidatePlacement(S);if(V.GetReasons().Num()!=1||V.GetPrimaryFailure()!=EHansaPlacementFailure::RoadRequired)continue;
     S.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Market")).Value;S.Anchor={X+HomeWidth+1,Y};const auto M=Host->ValidatePlacement(S);
     if(M.GetReasons().Num()!=1||M.GetPrimaryFailure()!=EHansaPlacementFailure::RoadRequired)continue;
     bool Clear=true;S.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
     for(int32 J=0;J<3;++J){S.Anchor={X+HomeWidth,Y+J};Clear&=bool(Host->ValidatePlacement(S));}
     if(Clear){Anchor={X,Y};Found=true;}
    }
    if(!Test->TestTrue(TEXT("Find viable full-sized parcel and connected market"),Found))return true;
    auto Place=[&](const FString& DefinitionId,int32 X,int32 Y){return Menu->SelectBuilding(FName(*DefinitionId))&&Menu->TargetGridCell(X,Y)&&Menu->GetSnapshot().bCanConfirm&&Menu->ConfirmIntent();};
    for(int32 J=0;J<3;++J)if(!Test->TestTrue(TEXT("Road through build presenter"),Place(TEXT("Building.Road"),Anchor.X+HomeWidth,Anchor.Y+J)))return true;
    if(!Test->TestTrue(TEXT("Market through build presenter"),Place(TEXT("Building.Market"),Anchor.X+HomeWidth+1,Anchor.Y)))return true;
    Menu->BeginCardDrag(FName(*HomeId()));Menu->UpdateCardDragTarget(Anchor.X,Anchor.Y);Menu->RotateIntent();Menu->RotateIntent();Menu->RotateIntent();Menu->RotateIntent();
    if(!Test->TestTrue(TEXT("Rotated footprint remains road-oriented and valid"),Menu->GetSnapshot().bCanConfirm))return true;
    for(TActorIterator<AHansaLubeckWorldFoundation> It(W);It;++It){if(Camera)Camera->FocusWorldLocationIntent(It->PlacementCellToWorld(Anchor.X+2,Anchor.Y+1));break;}
   }
   else if(Stage==2)
   {
    if(!Test->TestTrue(TEXT("Place compound through visible build intent"),Menu->EndCardDrag(true)))return true;
    Menu->SetOpen(false);Host->SynchronizeWorldProjection();
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==HomeId()){Id=It->GetBuildingId();break;}
    if(!Test->TestTrue(TEXT("One authoritative parcel identity"),Id.IsValid()))return true;
   }
   else if(Stage==3)
   {
    if(!Test->TestTrue(TEXT("Complete actual construction"),Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Market"))->BuildTicks)))return true;
    Host->SynchronizeWorldProjection();Select(W,C);auto* A=Find(W);
    if(!Test->TestNotNull(TEXT("Completed compound projection"),A))return true;
    Test->TestTrue(TEXT("Whole compound active"),A->QueryCompound().bActive);
    Test->TestTrue(TEXT("Population and needs inspection"),Inspector->GetSnapshot().Residence.bValid&&!Inspector->GetSnapshot().Residence.Needs.IsEmpty());
   }
   else if(Stage==4||Stage==5)
   {
    for(int32 Tick=0;Tick<30&&!Host->PreviewUpgradeResidence(Id).IsSuccess();++Tick)Host->AdvanceTicks(1);
    Select(W,C);
    if(!Test->TestTrue(TEXT("Upgrade through inspector with real costs and satisfied needs"),Inspector->UpgradeResidenceIntent()))return true;
    Host->SynchronizeWorldProjection();auto* A=Find(W);
    if(!Test->TestNotNull(TEXT("Upgrade preserves entity"),A))return true;
    const FString Expected=FString::Printf(TEXT("Building.Residence.Laborer.NarrowGang.Stage%d"),Stage-2);
    if(!Test->TestEqual(TEXT("Expected development stage"),A->GetBuildingDefinitionId(),Expected))return true;
    if(!Test->TestEqual(TEXT("Inspector shows the current authored stage name immediately"),Inspector->GetSnapshot().Identity.ToString(),Host->FindBuildingDefinition(Expected)->DisplayName))return true;
   }
   else if(Stage==6)
   {
    SignatureBefore=Signature(W);Fingerprint=Host->BuildProjection().Value.GetFingerprint();Save->Open();Save->SetSaveName(TEXT("Compound UAT isolated slot"));Save->RequestSave();
    if(Save->GetSnapshot().Confirmation!=EHansaSaveLoadConfirmation::None)Save->Confirm();
    if(!Test->TestTrue(TEXT("Normal save service succeeds"),Save->GetSnapshot().Status==EHansaSaveLoadStatus::Success))return true;
   }
   else if(Stage==7)
   {
    Save->RequestLoad();Save->Confirm();
    if(!Test->TestTrue(TEXT("Normal load service succeeds"),Save->GetSnapshot().Status==EHansaSaveLoadStatus::Success))return true;
    Save->Close();Host->SynchronizeWorldProjection();Select(W,C);
    Test->TestTrue(TEXT("Authoritative checksum preserved"),Fingerprint==Host->BuildProjection().Value.GetFingerprint());
    Test->TestEqual(TEXT("Exact mesh transforms, layout and parcel identity after load"),Signature(W),SignatureBefore);
   }
   else if(Stage==8)
   {
    Select(W,C);Inspector->RemoveBuildingIntent();Inspector->RemoveBuildingIntent();Host->SynchronizeWorldProjection();
    if(!Test->TestNull(TEXT("Confirmed demolition removes entire parcel"),Find(W)))return true;
    Menu->SelectBuilding(FName(*HomeId()));Menu->TargetGridCell(Anchor.X,Anchor.Y);
    if(!Test->TestTrue(TEXT("Freed footprint accepts replacement"),Menu->ConfirmIntent()))return true;
    Host->SynchronizeWorldProjection();bool NewId=false;for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==HomeId())NewId|=It->GetBuildingId()!=Id;
    Test->TestTrue(TEXT("Replacement receives a new identity"),NewId);Menu->SetOpen(false);
   }
   if(Stage==5 && Camera)Camera->AddZoomIntent(2);
   if(Stage==8)
   {
    Host->AdvanceTicks(Host->FindBuildingDefinition(HomeId())->BuildTicks);Host->SynchronizeWorldProjection();
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==HomeId()){Id=It->GetBuildingId();break;}
    if(Camera){for(TActorIterator<AHansaLubeckWorldFoundation> It(W);It;++It){Camera->FocusWorldLocationIntent(It->PlacementCellToWorld(Anchor.X+4,Anchor.Y+6));break;}Camera->AddZoomIntent(-100);}
   }
   if(Stage==9)
   {
    auto Place=[&](const FString& DefinitionId,int32 X,int32 Y){return Menu->SelectBuilding(FName(*DefinitionId))&&Menu->TargetGridCell(X,Y)&&Menu->GetSnapshot().bCanConfirm&&Menu->ConfirmIntent();};
    for(int32 Y=3;Y<=12;++Y)if(!Test->TestTrue(TEXT("District road through normal build intent"),Place(TEXT("Building.Road"),Anchor.X+HomeWidth,Anchor.Y+Y)))return true;
    for(int32 X=0;X<4;++X)if(!Test->TestTrue(TEXT("District corner cross-street"),Place(TEXT("Building.Road"),Anchor.X+HomeWidth-4+X,Anchor.Y+12)))return true;
    TSet<FString> RandomFamilies;
    for(int32 I=0;I<5;++I)
    {
     const FString Building=TEXT("Building.Residence.Laborer");
     int32 X=Anchor.X;
     int32 Y=Anchor.Y+3*(I<3?I+1:I-2);
     if(!Test->TestTrue(TEXT("Ordinary labour-house button chooses a compound"),Menu->SelectBuilding(FName(*Building))))return true;
     RandomFamilies.Add(Menu->GetSnapshot().SelectedBuildingId.ToString());
     X=Anchor.X+HomeWidth+(I<3?-Host->FindBuildingDefinition(Menu->GetSnapshot().SelectedBuildingId.ToString())->FootprintWidthCells:1);
     if(I==2)Y=Anchor.Y+12-Host->FindBuildingDefinition(Menu->GetSnapshot().SelectedBuildingId.ToString())->FootprintHeightCells;
     if(!Test->TestTrue(TEXT("Random adjacent compound through normal build intent"),Menu->TargetGridCell(X,Y)&&Menu->GetSnapshot().bCanConfirm&&Menu->ConfirmIntent()))return true;
    }
    Test->TestEqual(TEXT("Random ordinary labour houses cover all four families"),RandomFamilies.Num(),4);
    Menu->CancelIntent();Menu->SetOpen(false);Host->AdvanceTicks(Host->FindBuildingDefinition(HomeId())->BuildTicks);Host->SynchronizeWorldProjection();
    int32 Count=0;bool Corner=false;for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->QueryCompound().bActive){++Count;Corner|=It->QueryCompound().LayoutId.Contains(TEXT("Corner"));}
    Test->TestEqual(TEXT("Six authoritative adjacent compound parcels"),Count,6);Test->TestTrue(TEXT("District includes a real road corner layout"),Corner);
    auto DistrictSignature=[&](){TArray<FString> Rows;for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It){const auto O=It->QueryCompound();if(O.bActive)Rows.Add(FString::Printf(TEXT("%llu:"),static_cast<unsigned long long>(It->GetBuildingId().GetValue()))+It->GetBuildingDefinitionId()+TEXT(":")+O.LayoutId);}Rows.Sort();return FString::Join(Rows,TEXT("\n"));};
    const FString RandomDistrictBefore=DistrictSignature();const auto RandomFingerprint=Host->BuildProjection().Value.GetFingerprint();
    Save->Open();Save->SetSaveName(TEXT("Random labour district isolated slot"));Save->RequestSave();
    if(Save->GetSnapshot().Confirmation!=EHansaSaveLoadConfirmation::None)Save->Confirm();
    if(!Test->TestTrue(TEXT("Save randomly chosen labour houses"),Save->GetSnapshot().Status==EHansaSaveLoadStatus::Success))return true;
    Save->RequestLoad();Save->Confirm();
    if(!Test->TestTrue(TEXT("Load randomly chosen labour houses"),Save->GetSnapshot().Status==EHansaSaveLoadStatus::Success))return true;
    Save->Close();Host->SynchronizeWorldProjection();
    Test->TestEqual(TEXT("Random family, parcel identity and layout survive reload"),DistrictSignature(),RandomDistrictBefore);
    Test->TestTrue(TEXT("Random district authoritative fingerprint survives reload"),RandomFingerprint==Host->BuildProjection().Value.GetFingerprint());
    Hud->GetScenarioPresentationModel()->Close();
   }
   if(Stage==10&&Camera){if(auto* A=Find(W)){Camera->FocusWorldLocationIntent(A->GetActorLocation());Camera->AddZoomIntent(4);Select(W,C);}}
   Prepared=true;Ready=FPlatformTime::Seconds();PerformanceFrames=0;PreviousFrame=Ready;return false;
  }
  if(Stage==1)C->RefreshBuildingPlacementPresentation();
  if(Stage==8||Stage==9)
  {
   const double Now=FPlatformTime::Seconds(),Delta=(Now-PreviousFrame)*1000;PreviousFrame=Now;++PerformanceFrames;
   if(PerformanceFrames>180&&PerformanceFrames<=360)
   {
    int32 Actors=0,Parcels=0,Instances=0,Batches=0;for(TActorIterator<AActor> It(W);It;++It)++Actors;
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It){const auto O=It->QueryCompound();if(O.bActive){++Parcels;Instances+=O.InstanceCount;Batches+=O.BatchCount;}}
    Performance+=FString::Printf(TEXT("%d,%d,%.4f,%.4f,%.4f,%d,%llu,%d,%d,%d,%d\n"),Stage,PerformanceFrames-180,Delta,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),GNumDrawCallsRHI[0],static_cast<unsigned long long>(FPlatformMemory::GetStats().UsedPhysical),Actors,Parcels,Instances,Batches);
   }
   if(PerformanceFrames<=360)return false;
   FFileHelper::SaveStringToFile(Performance,*(FPaths::ProjectSavedDir()/TEXT("CompoundIntegration/PlayerFlow/district-performance.csv")));
  }
  if(FPlatformTime::Seconds()-Ready<.75)return false;
  const auto Dir=FPaths::ProjectSavedDir()/TEXT("CompoundIntegration/PlayerFlow");
  const TCHAR* Names[]={TEXT("catalogue"),TEXT("oriented-ghost"),TEXT("construction"),TEXT("stage1-needs"),TEXT("stage2"),TEXT("stage3"),TEXT("save"),TEXT("loaded"),TEXT("replacement"),TEXT("district"),TEXT("close")};
  TArray<FColor> Pixels;FIntVector Size(0);TArray64<uint8> Png;
  if(!Test->TestTrue(TEXT("Real viewport screenshot"),FSlateApplication::Get().TakeScreenshot(GEngine->GameViewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return true;
  FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Dir/Names[Stage]+TEXT(".png")));
  FString Semantic=FString::Printf(TEXT("map=%s;stage=%d;viewport=%dx%d;building=%llu\n"),*W->GetMapName(),Stage,Size.X,Size.Y,static_cast<unsigned long long>(Id.GetValue()));
  for(const auto& N:Hud->GetRootWidget()->GetSemanticSnapshot())if(N.State.bVisible)Semantic+=N.Id+TEXT("\t")+N.State.Value+TEXT("\n");
  FFileHelper::SaveStringToFile(Semantic,*(Dir/Names[Stage]+TEXT(".txt")));++Stage;Prepared=false;return Stage==11;
 }
private:
 static FString HomeId(){return TEXT("Building.Residence.Laborer.NarrowGang.Stage1");}
 AHansaBuildingWorldProjectionActor* Find(UWorld* W){for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingId()==Id)return *It;return nullptr;}
 void Select(UWorld* W,AHansaStrategyPlayerController* C){if(auto* A=Find(W)){for(TActorIterator<AHansaPlacementProjectionManager> It(W);It;++It)It->SelectBuilding(Id);C->OnWorldSelectionChanged.Broadcast(A,FHitResult());}}
 FString Signature(UWorld* W)
 {
  auto* A=Find(W);if(!A)return TEXT("missing");const auto O=A->QueryCompound();FString S=A->GetBuildingDefinitionId()+O.LayoutId;
  TArray<UChildActorComponent*> Children;A->GetComponents(Children);
  for(auto* Child:Children)if(auto* Compound=Cast<AHansaCompoundPresentation>(Child->GetChildActor()))for(const auto& B:Compound->Batches)for(int32 I=0;I<B->GetInstanceCount();++I){FTransform T;B->GetInstanceTransform(I,T,true);S+=B->GetStaticMesh()->GetPathName()+T.ToString();}
  for(auto* Child:Children)if(auto* Compound=Cast<AHansaCompoundPresentation>(Child->GetChildActor()))
  {
   if(const auto* Ground=Compound->GroundCoverage->GetProcMeshSection(0))
    for(const auto& V:Ground->ProcVertexBuffer)S+=V.Position.ToString()+V.Color.ToString();
   S+=FString::FromInt(Compound->GrassExclusionOwners.Num());
  }
  return S;
 }
 FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;FIntPoint Anchor;FHansaBuildingId Id;FHansaDeterminismFingerprint Fingerprint;FString SignatureBefore;int32 PerformanceFrames=0;double PreviousFrame=0;FString Performance=TEXT("stage,sample,wall_ms,game_ms,render_ms,draw_calls,process_bytes,world_actors,parcels,instances,batches\n");
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundPlayerFlow,"Hansa.Compound.PlayerFlow",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaCompoundPlayerFlow::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(HansaCompoundSettlement::FPlayerFlow(this));return true;}
#endif
