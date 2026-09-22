#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaBuildMenu.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "Engine/World.h"
#include "Components/ChildActorComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Algo/Reverse.h"

namespace
{
constexpr const TCHAR* ArtisanPlotId=TEXT("Building.Residence.Artisan.Plot");
UHansaResidentialCompoundDefinition* ArtisanPlots(){return LoadObject<UHansaResidentialCompoundDefinition>(nullptr,TEXT("/Game/Hansa/Core/Compounds/ArtisanPlots/DA_Compound_ArtisanPlot"));}
struct FPlotSetting
{
 bool Previous=true;bool Existed=false;
 FPlotSetting(){Existed=GConfig->GetBool(TEXT("Hansa.Housing"),TEXT("UseArtisanPlots"),Previous,GEngineIni);GConfig->SetBool(TEXT("Hansa.Housing"),TEXT("UseArtisanPlots"),true,GEngineIni);}
 ~FPlotSetting(){if(Existed)GConfig->SetBool(TEXT("Hansa.Housing"),TEXT("UseArtisanPlots"),Previous,GEngineIni);else GConfig->RemoveKey(TEXT("Hansa.Housing"),TEXT("UseArtisanPlots"),GEngineIni);}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaArtisanPlotsValidation,"Hansa.Compound.Artisan.LayoutsAndAuthoring",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaArtisanPlotsValidation::RunTest(const FString&)
{
 auto* D=ArtisanPlots();if(!TestNotNull(TEXT("Authored plots load"),D))return false;
 TArray<FHansaDefinitionValidationIssue> Issues;D->ValidateDefinition(Issues);for(const auto& I:Issues)AddError(I.PropertyPath+TEXT(": ")+I.Cause.ToString());
 TestEqual(TEXT("Exactly two alternatives"),D->Layouts.Num(),2);TestEqual(TEXT("Full plot depth"),D->FootprintWidthCells,4);TestEqual(TEXT("Full plot frontage"),D->FootprintHeightCells,2);
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> Reordered(DuplicateObject<UHansaResidentialCompoundDefinition>(D,GetTransientPackage()));Algo::Reverse(Reordered->Layouts);
 TMap<FString,int32> Counts;
 for(uint64 Id=1;Id<=256;++Id)
 {
  const auto Seed=D->ParcelSeed(TEXT("City.Lubeck"),Id,1);auto C=D->Compose(Seed,1,TEXT("Straight"),TEXT(""));
  if(!TestTrue(TEXT("Complete real-scale layout and access graph"),C.IsValid()))return false;
  ++Counts.FindOrAdd(C.LayoutId);TestEqual(TEXT("Array order cannot reroll a saved plot"),C.LayoutId,Reordered->Compose(Seed,1,TEXT("Straight"),TEXT("")).LayoutId);
  for(const auto& I:C.Instances)TestTrue(TEXT("All structures remain full scale"),I.Transform.GetScale3D().Equals(FVector::OneVector));
 }
 TestEqual(TEXT("Both choices occur"),Counts.Num(),2);for(const auto& C:Counts)TestTrue(TEXT("Equal weights produce a balanced sample"),C.Value>90&&C.Value<166);
 FString Json,Error;Hansa::Editor::Compounds::ExportInterchange(*D,Json);TStrongObjectPtr<UHansaResidentialCompoundDefinition> Copy(Hansa::Editor::Compounds::ImportDraft(Json,Error));
 if(TestNotNull(*Error,Copy.Get()))TestEqual(TEXT("Authoring interchange preserves the saved layout contract"),Copy->ComputeDeterministicContentHash(),D->ComputeDeterministicContentHash());
 Reordered->PopulationTierId=TEXT("PopulationTier.Merchant");Issues.Reset();Reordered->ValidateDefinition(Issues);TestTrue(TEXT("Unsupported household tiers are rejected"),!Issues.IsEmpty());
 auto* B=LoadObject<UHansaBuildingDefinition>(nullptr,TEXT("/Game/Hansa/Core/Buildings/ArtisanPlots/DA_Building_Residence_Artisan_Plot"));
 auto* Legacy=LoadObject<UHansaBuildingDefinition>(nullptr,TEXT("/Game/Hansa/Core/Buildings/DA_Building_Residence_Artisan"));
 if(TestNotNull(TEXT("New plot definition"),B)&&TestNotNull(TEXT("Legacy definition"),Legacy))
 {
  TestEqual(TEXT("One household capacity, irrespective of outbuildings"),B->ResidenceCapacity,Legacy->ResidenceCapacity);
  TestEqual(TEXT("Legacy width preserved"),Legacy->FootprintWidthCells,2);TestEqual(TEXT("Legacy depth preserved"),Legacy->FootprintHeightCells,2);
  TestTrue(TEXT("No incompatible upgrade inherited"),B->UpgradeTargetBuildingId.IsEmpty());
  TestNull(TEXT("Authoring refuses a larger plot under the old identity"),Hansa::Editor::Compounds::CreateBindingDraft(*Legacy,*D,Legacy->StableDefinitionId,Error));
  TestTrue(TEXT("Impact lists the bound artisan residence"),Hansa::Editor::Compounds::DescribeImpact(*D,{B}).Num()>1);
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaArtisanPlotsConstruction,"Hansa.Compound.Artisan.ConstructionSaveAndSwitch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaArtisanPlotsConstruction::RunTest(const FString&)
{
 using namespace Hansa::Simulation;FPlotSetting Setting;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>()),Loaded(NewObject<UHansaRuntimeSimulationHost>());
 FString Error;if(!TestTrue(*Error,Host->InitializeForLubeck(nullptr,Error,EHansaRuntimeScenario::EmptyLubeckBuild)))return false;
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Menu(NewObject<UHansaBuildMenuPresentationModel>());
 if(!TestTrue(*Error,Menu->InitializeForLubeck(nullptr,Host.Get(),Error)))return false;
 Menu->SetOpen(true);auto Slate=SNew(Hansa::UI::SHansaBuildMenu).Model(Menu.Get());Slate->ActivateSemanticId(TEXT("BuildMenu.Category.Residences"));
 TestTrue(TEXT("Artisan card activates through normal UI"),Slate->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Residence_Artisan_Plot")));
 TestFalse(TEXT("Disconnected plot cannot build"),Menu->TargetGridCell(10,10)&&Menu->GetSnapshot().bCanConfirm);
 Menu->SelectBuilding(TEXT("Building.Road"));Menu->TargetGridCell(18,16);if(!TestTrue(TEXT("Road constructs"),Menu->ConfirmIntent()))return false;
 for(int32 I=0;I<4;++I)
 {
  const int32 Y=16+I*3;
  if(I>0){Menu->SelectBuilding(TEXT("Building.Road"));Menu->TargetGridCell(18,Y);if(!TestTrue(TEXT("Additional street access constructs"),Menu->ConfirmIntent()))return false;}
  if(!TestTrue(TEXT("Artisan plot selected"),Menu->SelectBuilding(ArtisanPlotId))||!TestTrue(TEXT("Road-adjacent plot targeted"),Menu->TargetGridCell(14,Y)))return false;
  if(!TestTrue(*FString::Printf(TEXT("Plot %d valid: %s"),I,*Menu->GetSnapshot().ValidationCause.ToString()),Menu->GetSnapshot().bCanConfirm)||!TestTrue(TEXT("Authoritative construction accepted"),Menu->ConfirmIntent()))return false;
 }
 if(!TestTrue(TEXT("Construction completes"),Host->AdvanceTicks(160)))return false;
 TMap<uint64,FString> Choices;auto* D=ArtisanPlots();const auto Before=Host->BuildProjection();if(!Before)return false;
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ArtisanPlotTestWorld"));auto* Foundation=W->SpawnActor<AHansaLubeckWorldFoundation>();auto* Actor=W->SpawnActor<AHansaBuildingWorldProjectionActor>();
 for(const auto& P:Before.Value.GetBuildingWorldProjections())if(P.Placement.BuildingDefinitionId.ToString()==ArtisanPlotId)
 {
  TestEqual(TEXT("Entire 16x8 parcel occupied"),P.OccupiedCells.Num(),8);Actor->ApplyProjection(P,*Foundation);
  const auto O=Actor->QueryCompound();TestTrue(TEXT("Normal world projection renders plot"),O.bActive);TestTrue(TEXT("No presentation diagnostics"),O.Diagnostics.IsEmpty());
  TestTrue(TEXT("Completed compound visible"),Actor->BuildingPresentation->GetChildActor()&&!Actor->BuildingPresentation->GetChildActor()->IsHidden());
  Choices.Add(P.BuildingId.GetValue(),O.LayoutId);
 }
 W->DestroyWorld(false);TestEqual(TEXT("Four logical artisan plots"),Choices.Num(),4);
 TArray<uint8> Bytes;auto Saved=Host->CaptureSaveBytes(Bytes,TEXT("Artisan plots"),TEXT("2026-09-16T12:00:00Z"));if(!TestTrue(*Saved.Message,Saved.IsSuccess()))return false;
 if(!TestTrue(*Error,Loaded->InitializeForLubeck(nullptr,Error,EHansaRuntimeScenario::EmptyLubeckBuild)))return false;
 auto Restored=Loaded->RestoreSaveBytes(Bytes);if(!TestTrue(*Restored.Message,Restored.IsSuccess()))return false;
 TestEqual(TEXT("Exact authoritative save state preserved"),Saved.AuthoritativeHash,Restored.AuthoritativeHash);
 const auto After=Loaded->BuildProjection();for(const auto& P:After.Value.GetBuildingWorldProjections())if(P.Placement.BuildingDefinitionId.ToString()==ArtisanPlotId)
 {const auto Seed=D->ParcelSeed(P.Placement.CityId.ToString(),P.BuildingId.GetValue(),P.BuildingId.GetGeneration());TestEqual(TEXT("Save/load retains chosen layout"),D->Compose(Seed,1,TEXT("Straight"),TEXT("")).LayoutId,Choices.FindChecked(P.BuildingId.GetValue()));}
 GConfig->SetBool(TEXT("Hansa.Housing"),TEXT("UseArtisanPlots"),false,GEngineIni);TestTrue(TEXT("Switch back reloads menu"),Menu->ReloadCatalog(Error));
 TestFalse(TEXT("New plots hidden after reverting"),Menu->SelectBuilding(ArtisanPlotId));TestEqual(TEXT("Switch leaves existing parcels intact"),Host->GetPlacedBuildingCount(),8);
 TestTrue(TEXT("Legacy card restored"),Menu->GetSnapshot().Cards.ContainsByPredicate([](const auto& C){return C.StableId==TEXT("Building.Residence.Artisan");}));
 return !HasAnyErrors();
}
#endif
