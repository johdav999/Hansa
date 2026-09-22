#include "Materials/Material.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Misc/AutomationTest.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Algo/Reverse.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLabourCourtContentTest,"Hansa.Compound.AuthoredCourts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLabourCourtContentTest::RunTest(const FString&)
{
 for(const FString Family:{TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")})
 {
  FString Json,Error;const FString File=FPaths::ProjectDir()/TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R05/definitions")/(Family+TEXT(".json"));
  if(!TestTrue(TEXT("Authored input exists"),FFileHelper::LoadFileToString(Json,*File)))continue;
  TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Hansa::Editor::Compounds::ImportDraft(Json,Error));
  if(!TestNotNull(*Error,D.Get()))continue;
  TestEqual(TEXT("Three stages, two stable alternatives, four road contexts"),D->Layouts.Num(),24);
  TestTrue(TEXT("Real parcel growth reserved from stage one"),D->FootprintWidthCells>=4&&D->FootprintHeightCells>=3);
  for(uint64 Id=1;Id<=64;++Id)for(FName Context:{FName(TEXT("Straight")),FName(TEXT("CornerLeft")),FName(TEXT("CornerRight")),FName(TEXT("Edge"))})
  {
   const uint64 Seed=UHansaResidentialCompoundDefinition::ParcelSeed(TEXT("City.Lubeck"),Id,1);
   FString VariantKey;
   for(int32 Stage=1;Stage<=3;++Stage)
   {
    auto C=D->Compose(Seed,Stage,Context,TEXT("District.Lubeck.LateMedieval"));TestTrue(TEXT("Every selected composition validates"),C.IsValid());
    TArray<FString> Parts;C.LayoutId.ParseIntoArray(Parts,TEXT("."));if(Parts.Num()!=4){AddError(TEXT("Invalid stable layout key"));continue;}
    if(Stage==1)VariantKey=Parts[2];else TestEqual(TEXT("Upgrades retain authored alternative"),Parts[2],VariantKey);
    for(const FName Purpose:{FName(TEXT("Entrance")),FName(TEXT("YardWork")),FName(TEXT("Rest")),FName(TEXT("Delivery"))})TestTrue(TEXT("Required activity present"),C.Nodes.ContainsByPredicate([&](const auto& N){return N.Purpose==Purpose;}));
    Algo::Reverse(D->Layouts);auto Again=D->Compose(Seed,Stage,Context,TEXT("District.Lubeck.LateMedieval"));TestEqual(TEXT("Order independent saved selection"),Again.LayoutId,C.LayoutId);
   }
  }
  TestFalse(TEXT("Late-period courts cannot appear in unreviewed districts"),D->Compose(1,3,TEXT("Straight"),TEXT("District.Early1250")).IsValid());
  // A surface may be dimensioned, but cannot hide a full-size solid obstacle.
  auto& Surface=D->Layouts[0].Slots.Last();
  if(Surface.Group==TEXT("Surface"))
  {
   Surface.Variants[0].BoundsMax.Z=300;TArray<FHansaDefinitionValidationIssue> Issues;D->ValidateLayout(D->Layouts[0],Issues,false);TestFalse(TEXT("Tall surface rejected"),Issues.IsEmpty());
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLabourCourtUpgradeSchemaTest,"Hansa.Compound.UpgradeAuthoring",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLabourCourtUpgradeSchemaTest::RunTest(const FString&)
{
 auto Owned=Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
 TArray<const UHansaDefinitionBase*> Definitions;UHansaBuildingDefinition* Source=nullptr;
 for(const auto& D:Owned){Definitions.Add(D.Get());if(D->StableDefinitionId==TEXT("Building.Residence.Laborer"))Source=Cast<UHansaBuildingDefinition>(D.Get());}
 if(!TestNotNull(TEXT("Legacy residence"),Source))return false;
 FString Json,Error;FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R05/definitions/NarrowGang.json")));
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> Compound(Hansa::Editor::Compounds::ImportDraft(Json,Error));
 if(!TestNotNull(*Error,Compound.Get()))return false;
 Definitions.Add(Compound.Get());TArray<TStrongObjectPtr<UHansaBuildingDefinition>> Stages;
 for(int32 Stage=1;Stage<=3;++Stage)
 {
  auto* B=Hansa::Editor::Compounds::CreateBindingDraft(*Source,*Compound,FString::Printf(TEXT("Building.Test.Court.Stage%d"),Stage),Error);
  if(!TestNotNull(*Error,B))return false;
  Stages.Emplace(B);B->CompoundStage=Stage;B->CompoundDistrictId=TEXT("District.Lubeck.LateMedieval");B->bUpgradeOnly=Stage>1;
  B->UpgradeTargetBuildingId=Stage<3?FString::Printf(TEXT("Building.Test.Court.Stage%d"),Stage+1):FString();B->RefreshContentHash();Definitions.Add(B);
 }
 const auto Good=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 for(const auto& I:Good.Issues)if(I.Severity==EHansaDefinitionValidationSeverity::Error)AddError(I.PropertyPath+TEXT(": ")+I.Cause.ToString());
 TestTrue(TEXT("Same-tier stage chain compiles with all editor validation"),Good.IsValid());
 const auto* Compiled=Good.Registry.FindBuilding(TEXT("Building.Test.Court.Stage2"));
 if(TestNotNull(TEXT("Compiled development metadata"),Compiled)){TestEqual(TEXT("Compiled stage"),Compiled->CompoundStage,2);TestEqual(TEXT("Compiled district"),Compiled->CompoundDistrictId,Stages[1]->CompoundDistrictId);}
 Stages[0]->UpgradeTargetBuildingId=Stages[2]->StableDefinitionId;
 auto Invalid=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 TestTrue(TEXT("Editor rejects skipped development stages"),Invalid.Issues.ContainsByPredicate([](const auto& I){return I.Code==TEXT("HSA-REGISTRY-020");}));
 Stages[0]->UpgradeTargetBuildingId=Stages[1]->StableDefinitionId;Stages[1]->CompoundDistrictId=TEXT("District.Other");
 Invalid=FHansaEconomicDefinitionCompiler::Compile(Definitions);TestFalse(TEXT("Editor rejects mismatched districts"),Invalid.IsValid());
 TestEqual(TEXT("Legacy 8m footprint retained"),Source->FootprintWidthCells,2);
 TestEqual(TEXT("Legacy artisan progression retained"),Source->UpgradeTargetBuildingId,FString(TEXT("Building.Residence.Artisan")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLabourCourtMaterialUsageTest,"Hansa.Compound.HousingMaterialsSupportInstancing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLabourCourtMaterialUsageTest::RunTest(const FString&)
{
 for(const TCHAR* Name:{TEXT("Lime"),TEXT("Oak"),TEXT("Clay"),TEXT("Fieldstone"),TEXT("Iron"),TEXT("DarkWindow")})
 {
  const FString Path=FString::Printf(TEXT("/Game/Mesh/labour-housing-kit/Materials/M_LabourKit_%s"),Name);
  auto* M=LoadObject<UMaterial>(nullptr,*Path);
  if(TestNotNull(TEXT("Approved kit material exists"),M))TestTrue(*FString::Printf(TEXT("%s has persisted instanced shader usage; no game fallback"),Name),M->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCourtDecorationTest,"Hansa.Compound.DecorationVariation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCourtDecorationTest::RunTest(const FString&)
{
 int32 TotalRotated=0;
 for(const FString Family:{TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")})
 {
  auto* D=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,*(TEXT("/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_")+Family));
  if(!TestNotNull(TEXT("Production court"),D))return false;
  int32 Straight=0,Rotated=0;
  for(uint64 Id=1;Id<=256;++Id)
  {
   const uint64 Seed=UHansaResidentialCompoundDefinition::ParcelSeed(TEXT("City.Lubeck"),Id,1);
   const auto C=D->Compose(Seed,1,TEXT("Straight"),TEXT("District.Lubeck.LateMedieval"));
   if(!TestTrue(TEXT("Decorated composition validates"),C.IsValid()))return false;
   TestTrue(TEXT("First-stage courts have fences"),C.Instances.ContainsByPredicate([](const auto& I){return I.Group==TEXT("Fence");}));
   TestTrue(TEXT("First-stage courts have household cargo"),C.Instances.ContainsByPredicate([](const auto& I){return I.SlotId==TEXT("YardCargo0");}));
   const auto* L=D->Layouts.FindByPredicate([&](const auto& Layout){return Layout.LayoutId==C.LayoutId;});
   for(const auto& I:C.Instances)
   {
    if(I.Group!=TEXT("Dwelling"))continue;
    const auto* Slot=L->Slots.FindByPredicate([&](const auto& S){return S.SlotId==I.SlotId;});
    const auto* Original=Slot->Variants.FindByPredicate([](const auto& V){return V.VariantId==TEXT("Original");});
    const double Offset=FMath::Abs(FMath::FindDeltaAngleDegrees(Original->LocalYaw,I.Transform.Rotator().Yaw));
    TestTrue(TEXT("Occasional yaw stays within twenty-five degrees of the authored alignment"),Offset<=25.001);
    TestTrue(TEXT("House scale remains one"),I.Transform.GetScale3D().Equals(FVector::OneVector));
    if(Slot->Variants.Num()==3){if(Offset>.01)++Rotated;else ++Straight;}
   }
   const auto Again=D->Compose(Seed,1,TEXT("Straight"),TEXT("District.Lubeck.LateMedieval"));
   TestEqual(TEXT("Reload seed preserves prop inclusion"),Again.Instances.Num(),C.Instances.Num());
   for(int32 I=0;I<C.Instances.Num();++I)TestTrue(TEXT("Reload seed preserves exact yaw and placement"),Again.Instances[I].Transform.Equals(C.Instances[I].Transform));
  }
  TotalRotated+=Rotated;
  if(Family==TEXT("NarrowGang"))TestEqual(TEXT("Tight courts retain alignment"),Rotated,0);
  else TestTrue(TEXT("Eligible houses turn about 80 percent of the time"),Rotated>0 && double(Rotated)/(Rotated+Straight)>.70 && double(Rotated)/(Rotated+Straight)<.90);
 }
 TestTrue(TEXT("Some houses receive irregularity across the court pack"),TotalRotated>0);
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCourtBoundaryTest,"Hansa.Compound.ContinuousBoundaries",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCourtBoundaryTest::RunTest(const FString&)
{
 for(const FString Family:{TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")})
 {
  auto* D=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,*(TEXT("/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_")+Family));
  if(!TestNotNull(TEXT("Production court"),D))return false;
  for(const auto& L:D->Layouts)
  {
   TArray<FBox> Fences;
   for(const auto& S:L.Slots)if(S.Group==TEXT("Fence"))
   {
    TestTrue(TEXT("Boundary sections cannot randomly disappear"),S.bRequired && S.PresenceBasisPoints==10000);
    for(const auto& V:S.Variants)
    {
     const FBox B=FBox(V.BoundsMin,V.BoundsMax).TransformBy(FTransform(FRotator(0,V.LocalYaw,0),V.LocalPosition,V.Scale));
     TestTrue(TEXT("Only rear and side boundaries are fenced"),B.Max.X<D->BoundsMin.X+30 || B.Max.Y<D->BoundsMin.Y+30 || B.Min.Y>D->BoundsMax.Y-30);
     Fences.Add(B);
    }
   }
   // Sample the three fence centre lines every 5cm, including all panel joints.
   for(int32 Side=0;Side<3;++Side)
   {
    const double Start=Side==0?D->BoundsMin.Y:D->BoundsMin.X,End=Side==0?D->BoundsMax.Y:D->BoundsMax.X;
    for(double Along=Start+1;Along<End;Along+=5)
    {
     const FVector P=Side==0?FVector(D->BoundsMin.X+10,Along,50):FVector(Along,Side==1?D->BoundsMin.Y+10:D->BoundsMax.Y-10,50);
     if(!TestTrue(TEXT("Every non-street boundary is continuously covered"),Fences.ContainsByPredicate([&](const FBox& B){return B.IsInsideOrOn(P);})))return false;
    }
   }
   for(double Y=D->BoundsMin.Y+35;Y<D->BoundsMax.Y-35;Y+=5)
    TestFalse(TEXT("Street frontage stays open"),Fences.ContainsByPredicate([&](const FBox& B){return B.IsInsideOrOn(FVector(D->BoundsMax.X-1,Y,50));}));
  }
  // The end-post exception must never permit stacked duplicate sections.
  auto L=D->Layouts[0];const auto* Fence=L.Slots.FindByPredicate([](const auto& S){return S.Group==TEXT("Fence");});
  auto Duplicate=*Fence;Duplicate.SlotId=TEXT("StackedFence");L.Slots.Add(Duplicate);
  TArray<FHansaDefinitionValidationIssue> Issues;D->ValidateLayout(L,Issues,false);
  TestTrue(TEXT("Stacked fence sections are rejected"),!Issues.IsEmpty());
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompactCourtTest,"Hansa.Compound.CompactCourts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompactCourtTest::RunTest(const FString&)
{
 for(const FString Family:{TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")})
 {
  auto* Current=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,*(TEXT("/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_")+Family));
  if(!TestNotNull(TEXT("Compact production definition"),Current))return false;
  FString Json,Error;FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R07/baseline")/(Family+TEXT(".json"))));
  TStrongObjectPtr<UHansaResidentialCompoundDefinition> Old(Hansa::Editor::Compounds::ImportDraft(Json,Error));if(!TestNotNull(*Error,Old.Get()))return false;
  TestEqual(TEXT("Twenty-five percent less occupied land on the four-metre grid"),Current->FootprintWidthCells*Current->FootprintHeightCells*4,Old->FootprintWidthCells*Old->FootprintHeightCells*3);
  for(const auto& L:Current->Layouts)
  {
   TArray<FHansaDefinitionValidationIssue> Issues;Current->ValidateLayout(L,Issues,true);TestTrue(TEXT("All mesh bounds, variants, doors and access links validate"),Issues.IsEmpty());
   const auto* Before=Old->Layouts.FindByPredicate([&](const auto& X){return X.LayoutId==L.LayoutId;});if(!TestNotNull(TEXT("Stable layout identity"),Before))return false;
   for(const auto& S:Before->Slots)if(S.Group==TEXT("Dwelling")||S.Group==TEXT("Workshop")||S.SlotId==TEXT("Store")||S.SlotId==TEXT("Utility")||S.SlotId==TEXT("Work"))
   {
    const auto* After=L.Slots.FindByPredicate([&](const auto& X){return X.SlotId==S.SlotId;});if(!TestNotNull(TEXT("Existing building retained"),After))return false;
    TestEqual(TEXT("Existing building mesh retained"),After->Variants[0].Mesh.ToSoftObjectPath(),S.Variants[0].Mesh.ToSoftObjectPath());
    for(const auto& V:After->Variants)TestTrue(TEXT("Building stays full size"),V.Scale.Equals(FVector::OneVector));
   }
   if(L.DevelopmentStage<=2)TestTrue(TEXT("Early courts gain an approved lean-to"),L.Slots.ContainsByPredicate([](const auto& S){return S.SlotId==TEXT("CompactLeanTo");}));
  }
 }
 return !HasAnyErrors();
}
#endif
