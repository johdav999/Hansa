#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "World/HansaCompoundPresentation.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Save/HansaSaveEnvelope.h"
#include "UObject/StrongObjectPtr.h"
#include "Algo/Reverse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests::Compound
{
using namespace Hansa::Simulation;
UHansaResidentialCompoundDefinition* Fixture(int32 Width=3)
{
 auto* D=NewObject<UHansaResidentialCompoundDefinition>();D->StableDefinitionId=TEXT("Compound.LaborerFixture");
 D->FootprintWidthCells=Width;D->FootprintHeightCells=4;D->BoundsMin=FVector(-Width*200,-800,0);D->BoundsMax=FVector(Width*200,800,900);
 FHansaCompoundLayout L;L.LayoutId=TEXT("Straight");
 FHansaCompoundNode Road;Road.NodeId=TEXT("Road");Road.Position=FVector(Width*200,0,0);Road.bRoadEntrance=true;Road.Links={TEXT("Yard")};
 FHansaCompoundNode Yard;Yard.NodeId=TEXT("Yard");Yard.Purpose=TEXT("YardWork");Yard.Position=FVector(300,0,0);Yard.Links={TEXT("Front"),TEXT("Rear")};
 L.Nodes={Road,Yard};
 for(int32 I=0;I<2;++I)
 {
  FHansaCompoundSlot S;S.SlotId=I?TEXT("RearHouse"):TEXT("Principal");S.bPrincipal=I==0;S.EntranceNodeId=I?TEXT("Rear"):TEXT("Front");
  for(const TCHAR* Name:{TEXT("A"),TEXT("B")})
  {
   FHansaCompoundVariant V;V.VariantId=Name;
   V.Mesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("/Game/Mesh/hansa-residences/Meshes_R07/SM_Residence_Laborer_%s.SM_Residence_Laborer_%s"),Name,Name)));
   UStaticMesh* M=V.Mesh.LoadSynchronous();check(M);const FBox B=M->GetBoundingBox();V.BoundsMin=B.Min;V.BoundsMax=B.Max;
   V.LocalPosition=FVector(-150,I?400:-400,-B.Min.Z);S.Variants.Add(V);
  }
  FHansaCompoundNode N;N.NodeId=S.EntranceNodeId;N.Position=FVector(300,I?400:-400,0);L.Nodes.Add(N);L.Slots.Add(S);
 }
 D->Layouts.Add(L);
 for(const FName Context:{FName(TEXT("CornerLeft")),FName(TEXT("CornerRight")),FName(TEXT("Edge"))}){auto C=L;C.LayoutId=Context.ToString();C.Context=Context;D->Layouts.Add(C);}
 auto StageTwo=L;StageTwo.LayoutId=TEXT("EstablishedFixture");StageTwo.DevelopmentStage=2;D->Layouts.Add(StageTwo);
 return D;
}
FString Signature(const FHansaCompoundComposition& C)
{
 FString S=C.LayoutId;for(const auto& I:C.Instances)S+=I.SlotId+I.Mesh.ToString()+I.Transform.ToString();return S;
}
FHansaPlacementInitialization PlacementInit(int32 Turn=0,bool bFront=true)
{
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;const auto Owner=FHansaHouseId::TryCreate(1).Value;
 FHansaPlacementMapInitialization M;M.CityId=City;M.BoundsMin={-5,-5};M.BoundsMax={8,8};M.RoadBuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
 for(int32 X=-5;X<=8;++X)for(int32 Y=-5;Y<=8;++Y)M.Cells.Add({{X,Y},EHansaPlacementTerrain::Land,Owner,false});
 const FHansaGridCoordinate Front[4]={{3,1},{1,3},{-1,1},{1,-1}};const auto R=Front[bFront?Turn:(Turn+2)%4];
 FHansaPlacedBuildingRecord Road;Road.BuildingId=FHansaBuildingId::TryCreate(1).Value;Road.OwnerId=Owner;Road.Spec={City,M.RoadBuildingDefinitionId,R};Road.OccupiedCells={R};
 FHansaPlacementInitialization Init;Init.Maps={M};Init.Placements={Road};Init.Entitlements={{Owner,FHansaBuildingTypeId::TryParse(TEXT("Building.CompoundFixture")).Value}};return Init;
}
FHansaEconomicRegistry Registry(uint64 Hash=0xC04F01)
{
 FHansaCompiledBuildingDefinition Road;Road.StableId=TEXT("Building.Road");Road.FootprintWidthCells=Road.FootprintHeightCells=1;Road.BuildTicks=1;
 FHansaCompiledBuildingDefinition B;B.StableId=TEXT("Building.CompoundFixture");B.FootprintWidthCells=3;B.FootprintHeightCells=4;B.BuildTicks=10;B.bRequiresRoad=true;B.CompoundRoadFrontMask=15;B.CompoundLayoutContextMask=1;B.ResidentialCompoundId=TEXT("Compound.LaborerFixture");
 return FHansaEconomicRegistry({},{},{Road,B},Hash);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundValidationTest,"Hansa.Compound.RealScaleValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundValidationTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Fixture());TArray<FHansaDefinitionValidationIssue> Issues;D->ValidateDefinition(Issues);
 for(const auto& I:Issues)AddError(I.PropertyPath+TEXT(": ")+I.Cause.ToString());
 TestEqual(TEXT("12x16m supports two full approved residences"),Issues.Num(),0);
 auto Valid=[&](){TArray<FHansaDefinitionValidationIssue> E;D->ValidateDefinition(E);return E.IsEmpty();};
 D->FootprintWidthCells=D->FootprintHeightCells=2;D->BoundsMin=FVector(-400,-400,0);D->BoundsMax=FVector(400,400,900);
 TestFalse(TEXT("8x8m cannot accept the full-sized pair"),Valid());
 D.Reset(Fixture());D->Layouts[0].Slots[1].Variants[0].LocalPosition=D->Layouts[0].Slots[0].Variants[0].LocalPosition;TestFalse(TEXT("Any possible overlap is rejected"),Valid());
 D.Reset(Fixture());D->Layouts[0].Slots[0].Variants[0].Scale=FVector(.5);TestFalse(TEXT("Shrinking architecture is rejected"),Valid());
 D.Reset(Fixture());D->Layouts[0].Nodes[1].Position=FVector(-150,-400,0);TestFalse(TEXT("Obstructed yard node is rejected"),Valid());
 D.Reset(Fixture());D->Layouts[0].Nodes[0].Links.Reset();TestFalse(TEXT("Disconnected access graph is rejected"),Valid());
 D.Reset(Fixture());D->Layouts[0].Slots[0].Variants[0].LocalYaw=30;TestFalse(TEXT("Principal cannot face arbitrarily"),Valid());
 D.Reset(Fixture(4));TestTrue(TEXT("16x16m also validates"),Valid());
 FHansaCompoundSlot Passage;Passage.SlotId=TEXT("Passage");Passage.Group=TEXT("YardProp");
 FHansaCompoundVariant V;V.Mesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Mesh/labour-housing-kit/Meshes_R08/SM_LabourKit_CoveredPassage.SM_LabourKit_CoveredPassage")));
 UStaticMesh* Mesh=V.Mesh.LoadSynchronous();if(TestNotNull(TEXT("Approved open passage"),Mesh))
 {
  V.BoundsMin=Mesh->GetBoundingBox().Min;V.BoundsMax=Mesh->GetBoundingBox().Max;V.LocalPosition=FVector(550,0,-V.BoundsMin.Z);V.bUseSimpleCollisionForAccess=true;Passage.Variants={V};D->Layouts[0].Slots.Add(Passage);
  TArray<FHansaDefinitionValidationIssue> PassageIssues;D->ValidateDefinition(PassageIssues);for(const auto& E:PassageIssues)AddError(E.Cause.ToString());TestTrue(TEXT("Authored convex hulls preserve the covered access route"),PassageIssues.IsEmpty());
  D->Layouts[0].Slots.Last().Variants[0].bUseSimpleCollisionForAccess=false;TestFalse(TEXT("Closed obstacle across route is rejected"),Valid());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundDeterminismTest,"Hansa.Compound.DeterministicVariants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundDeterminismTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Fixture());
 TSet<FString> Seen;
 for(uint64 Id=1;Id<=64;++Id)
 {
  const uint64 Seed=D->ParcelSeed(TEXT("City.Lubeck"),Id,2);const auto A=D->Compose(Seed,1,TEXT("Straight"),TEXT(""));
  TestTrue(TEXT("Composition valid"),A.IsValid());Seen.Add(Signature(A));
  Algo::Reverse(D->Layouts);for(auto& L:D->Layouts){Algo::Reverse(L.Slots);for(auto& S:L.Slots)Algo::Reverse(S.Variants);}
  TestEqual(TEXT("Array order does not change random choices"),Signature(A),Signature(D->Compose(Seed,1,TEXT("Straight"),TEXT(""))));
  const auto B=D->Compose(Seed,2,TEXT("Straight"),TEXT(""));TestEqual(TEXT("Upgrade retains slot choices"),A.Instances[0].Mesh.ToString(),B.Instances[0].Mesh.ToString());
 }
 TestTrue(TEXT("Variation exercised"),Seen.Num()>1);
 TestNotEqual(TEXT("Generation is part of persistent identity"),D->ParcelSeed(TEXT("City.Lubeck"),1,1),D->ParcelSeed(TEXT("City.Lubeck"),1,2));
 TestNotEqual(TEXT("City is part of persistent identity"),D->ParcelSeed(TEXT("City.Lubeck"),1,1),D->ParcelSeed(TEXT("City.Rostock"),1,1));
 for(auto& L:D->Layouts)L.DistrictIds={TEXT("Harbor")};TestFalse(TEXT("District exclusion is enforced"),D->Compose(1,1,TEXT("Straight"),TEXT("Uptown")).IsValid());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundPlacementTest,"Hansa.Compound.RoadOrientationAndFootprint",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundPlacementTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;using namespace Hansa::Simulation;
 const auto Owner=FHansaHouseId::TryCreate(1).Value;
 for(int32 Turn=0;Turn<4;++Turn)
 {
  auto Init=PlacementInit(Turn);const auto State=FHansaPlacementState::TryCreate(Init);TestTrue(TEXT("Placement fixture valid"),State.IsSuccess());
  FHansaPlacementSpec Spec{Init.Maps[0].CityId,Init.Entitlements[0].BuildingDefinitionId,{0,0},static_cast<EHansaGridRotation>(Turn)};
  auto V=FHansaPlacementRules::Validate(State.Value,Registry(),Owner,Spec);TestTrue(TEXT("Front road accepted at every rotation"),V.CanPlace());TestEqual(TEXT("Larger footprint owns twelve cells"),V.GetOccupiedCells().Num(),12);
  const auto Wrong=FHansaPlacementState::TryCreate(PlacementInit(Turn,false));TestFalse(TEXT("Rear-only road cannot serve compound entrance"),FHansaPlacementRules::Validate(Wrong.Value,Registry(),Owner,Spec).CanPlace());
  Spec.Anchor={7,7};TestFalse(TEXT("Enlarged occupied cells respect map bounds"),FHansaPlacementRules::Validate(State.Value,Registry(),Owner,Spec).CanPlace());
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundSaveTest,"Hansa.Compound.SaveReconstruction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundSaveTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;using namespace Hansa::Simulation;
 auto ParcelInitialization=PlacementInit();const auto Owner=FHansaHouseId::TryCreate(1).Value;
 FHansaPlacedBuildingRecord P;P.BuildingId=FHansaBuildingId::TryCreate(90,3).Value;P.OwnerId=Owner;P.Spec={ParcelInitialization.Maps[0].CityId,ParcelInitialization.Entitlements[0].BuildingDefinitionId,{0,0}};
 for(int32 X=0;X<3;++X)for(int32 Y=0;Y<4;++Y)P.OccupiedCells.Add({X,Y});ParcelInitialization.Placements.Add(P);
 FHansaSimulationInitialization Init;Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value).Value;
 Init.CampaignSeed=42;Init.Houses={{Owner,FHansaMoney::FromRaw(10000)}};Init.Cities={{P.Spec.CityId,FHansaQuantity()}};Init.Placement=ParcelInitialization;
 for(const auto& R:ParcelInitialization.Placements)Init.Buildings.Add({R.BuildingId,R.Spec.BuildingDefinitionId,R.OwnerId,FHansaRate()});
 auto State=FHansaSimulationState::TryCreate(Init);if(!TestTrue(TEXT("State initialized"),State.IsSuccess()))return false;
 auto Context=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.CompoundFixture")).Value,0xC04F01,Registry(),FHansaPlacementTopology::TryCreate(ParcelInitialization.Maps).Value);if(!TestTrue(TEXT("Context initialized"),Context.IsSuccess()))return false;
 FHansaSaveSnapshot S;S.State=State.Value;S.Players={{1,Owner}};S.BuildVersion=TEXT("CompoundFixture");S.SavedUtc=TEXT("2026-09-14T00:00:00Z");S.DisplayName=TEXT("Compound");
 TArray<uint8> Bytes;const auto Encoded=FHansaSaveEnvelope::Encode(S,Context.Value,Bytes);if(!TestTrue(*Encoded.Message,Encoded.IsSuccess()))return false;
 FHansaSaveSnapshot Loaded;const auto Decoded=FHansaSaveEnvelope::Decode(Bytes,Context.Value,Loaded);if(!TestTrue(*Decoded.Message,Decoded.IsSuccess()))return false;
 const auto Projected=Loaded.State.CreateReadOnlyAccess(Context.Value).BuildProjection();if(!TestTrue(TEXT("Restored projection"),Projected.IsSuccess()))return false;
 const auto Placements=Projected.Value.GetPlacements();const auto* Restored=Placements.FindByPredicate([&](const auto& R){return R.BuildingId==P.BuildingId;});if(!TestNotNull(TEXT("One persistent compound parcel restored"),Restored))return false;
 TestTrue(TEXT("Twelve occupied cells unchanged"),Restored->OccupiedCells==P.OccupiedCells);
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Fixture());
 const auto Seed=[&](const auto& R){return D->ParcelSeed(R.Spec.CityId.ToString(),R.BuildingId.GetValue(),R.BuildingId.GetGeneration());};
 TestEqual(TEXT("Identical child layout after real save-envelope round trip"),Signature(D->Compose(Seed(P),1,TEXT("Straight"),TEXT(""))),Signature(D->Compose(Seed(*Restored),1,TEXT("Straight"),TEXT(""))));
 auto Changed=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.CompoundFixture")).Value,0xC04F02,Registry(0xC04F02),FHansaPlacementTopology::TryCreate(ParcelInitialization.Maps).Value);
 TestFalse(TEXT("Changed footprint/content cannot silently load old saves"),FHansaSaveEnvelope::Decode(Bytes,Changed.Value,Loaded).IsSuccess());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundRendererTest,"Hansa.Compound.InstancingAndFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundRendererTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Fixture());
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CompoundTestWorld"));auto* A=W->SpawnActor<AHansaCompoundPresentation>();
 TestTrue(TEXT("Whole parcel rendered"),A->ApplyCompound(D.Get(),2,1,TEXT("Straight"),TEXT("")));
 int32 Count=0;for(UHierarchicalInstancedStaticMeshComponent* B:A->Batches){Count+=B->GetInstanceCount();TestFalse(TEXT("No cosmetic nav authority"),B->CanEverAffectNavigation());TestEqual(TEXT("No cosmetic collision"),B->GetCollisionEnabled(),ECollisionEnabled::NoCollision);}
 TestEqual(TEXT("Two houses remain two instances"),Count,2);TestFalse(TEXT("No per-frame child actor work"),A->PrimaryActorTick.bCanEverTick);
 auto* Batch=A->Batches[0].Get();A->ApplyCompound(D.Get(),2,1,TEXT("Straight"),TEXT(""));TestTrue(TEXT("No rebuild on identical projection"),A->Batches[0].Get()==Batch);
 TestEqual(TEXT("Selection bounds cover full logical parcel"),A->GetParcelBounds().GetSize().X,1200.);
 TestFalse(TEXT("Missing definition safely clears old instances"),A->ApplyCompound(nullptr,2,1,TEXT("Straight"),TEXT("")));TestEqual(TEXT("No stale art"),A->Batches.Num(),0);TestTrue(TEXT("Fallback diagnosis exposed"),!A->Diagnostics.IsEmpty());
 D->Layouts[0].Slots[0].Variants.Reset();FHansaCompoundVariant Missing;Missing.Mesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Mesh/CompoundMissingTest.CompoundMissingTest")));Missing.BoundsMin=FVector(-361,-324,0);Missing.BoundsMax=FVector(361,324,650);Missing.LocalPosition=FVector(-150,-400,0);D->Layouts[0].Slots[0].Variants.Add(Missing);
 TestFalse(TEXT("Missing required art triggers safe fallback"),A->ApplyCompound(D.Get(),2,1,TEXT("Straight"),TEXT("")));TestEqual(TEXT("Missing art leaves no partial compound"),A->Batches.Num(),0);
 W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundAuthoringTest,"Hansa.Compound.AuthoringParity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundAuthoringTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;using namespace Hansa::Editor::Compounds;
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Fixture());FHansaEditorSchemaRegistry R;R.Refresh();const auto* Schema=R.FindSchema(D->GetClass());
 if(TestNotNull(TEXT("Compound discovered automatically"),Schema)){for(const auto& E:Schema->Diagnostics)AddError(E.Cause);TestTrue(TEXT("Metadata complete"),Schema->IsValid());}
 FString Json,Error;TestTrue(TEXT("Reflected interchange exports"),ExportInterchange(*D,Json));TStrongObjectPtr<UHansaResidentialCompoundDefinition> Copy(ImportDraft(Json,Error));
 if(TestNotNull(*Error,Copy.Get()))TestEqual(TEXT("Interchange round trip preserves every field"),Copy->ComputeDeterministicContentHash(),D->ComputeDeterministicContentHash());
 TStrongObjectPtr<UHansaBuildingDefinition> Old(NewObject<UHansaBuildingDefinition>());Old->StableDefinitionId=TEXT("Building.Residence.Laborer");Old->FootprintWidthCells=Old->FootprintHeightCells=2;Old->ResidenceCapacity=12;const auto Hash=Old->ComputeDeterministicContentHash();
 TestNull(TEXT("Cannot expand legacy identity silently"),CreateBindingDraft(*Old,*D,Old->StableDefinitionId,Error));
 TStrongObjectPtr<UHansaBuildingDefinition> Draft(CreateBindingDraft(*Old,*D,TEXT("Building.CompoundFixture"),Error));
 if(TestNotNull(*Error,Draft.Get())){TestEqual(TEXT("Draft reserves 12m X"),Draft->FootprintWidthCells,3);TestEqual(TEXT("Draft reserves 16m Y"),Draft->FootprintHeightCells,4);TestEqual(TEXT("Capacity remains a single parcel value"),Draft->ResidenceCapacity,12);TestTrue(TEXT("Impact includes binding"),DescribeImpact(*D,{Draft.Get()}).Num()>1);}
 TestEqual(TEXT("Dry run leaves old content hash intact"),Old->ComputeDeterministicContentHash(),Hash);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("CompoundImplementation/Evidence");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("compound-fixture.json")));
 TArray<FString> Files;TestTrue(TEXT("Schema export"),R.ExportAllJsonSchemas(Dir/TEXT("Schemas"),Files,Error));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundProjectionTest,"Hansa.Compound.ProjectionIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundProjectionTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Compound;using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(Fixture());TStrongObjectPtr<UHansaBuildingDefinition> B(NewObject<UHansaBuildingDefinition>(CreatePackage(TEXT("/Temp/HansaCompoundProjectionFixture"))));
 B->StableDefinitionId=TEXT("Building.CompoundProjectionFixture");B->ResidentialCompound=D.Get();B->FootprintWidthCells=3;B->FootprintHeightCells=4;B->ResidenceCapacity=12;B->ResidentPopulationTierId=TEXT("PopulationTier.Laborer");B->bRequiresRoad=true;
 auto& Assets=UAssetManager::Get();const auto AssetId=B->GetPrimaryAssetId();B->SetFlags(RF_Public | RF_Standalone);TestTrue(TEXT("Register fixture definition"),Assets.RegisterSpecificPrimaryAsset(AssetId,FAssetData(B.Get())));
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("CompoundProjectionWorld"));auto* Foundation=W->SpawnActor<AHansaLubeckWorldFoundation>();auto* A=W->SpawnActor<AHansaBuildingWorldProjectionActor>();
 FHansaBuildingWorldProjection P;P.BuildingId=FHansaBuildingId::TryCreate(90,3).Value;P.OwnerId=FHansaHouseId::TryCreate(1).Value;P.Placement={FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value,FHansaBuildingTypeId::TryParse(B->StableDefinitionId).Value,{0,0}};
 P.FootprintWidthCells=3;P.FootprintHeightCells=4;P.Status=EHansaBuildingWorldStatus::Ready;P.ConstructionProgress=FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
 for(int32 X=0;X<3;++X)for(int32 Y=0;Y<4;++Y)P.OccupiedCells.Add({X,Y});
 A->ApplyProjection(P,*Foundation);auto Observation=A->QueryCompound();TestTrue(TEXT("Production projection resolves compound data"),Observation.bActive);TestEqual(TEXT("Two child instances"),Observation.InstanceCount,2);TestEqual(TEXT("Logical click bounds width"),A->BuildingMesh->GetRelativeScale3D().X,12.);
 A->SetSelected(true);TestTrue(TEXT("Selection remains on the one parent parcel"),A->IsSelected());
 P.Placement.Rotation=EHansaGridRotation::East;P.OccupiedCells.Reset();for(int32 X=0;X<4;++X)for(int32 Y=0;Y<3;++Y)P.OccupiedCells.Add({X,Y});P.AdjacentRoadMask=3;
 A->ApplyProjection(P,*Foundation);TestEqual(TEXT("Corner layout selected against rotated roads"),A->QueryCompound().LayoutId,FString(TEXT("CornerLeft")));
 TestTrue(TEXT("Whole parcel rotates without scaling houses"),FMath::IsNearlyEqual(A->GetActorRotation().Yaw,90.));
 P.Status=EHansaBuildingWorldStatus::UnderConstruction;A->ApplyProjection(P,*Foundation);if(TestNotNull(TEXT("Compound child exists"),A->BuildingPresentation->GetChildActor()))TestTrue(TEXT("Construction hides all child art together"),A->BuildingPresentation->GetChildActor()->IsHidden());
 auto* Ghost=W->SpawnActor<AHansaBuildingPlacementGhost>();TArray<FIntPoint> GhostCells;for(const auto& Cell:P.OccupiedCells)GhostCells.Add({Cell.X,Cell.Y});
 Ghost->ApplyPreview(FName(*B->StableDefinitionId),{0,0},1,GhostCells,EHansaPlacementFeedback::Valid,FText(),*Foundation,false,3);
 TArray<UChildActorComponent*> GhostChildren;Ghost->GetComponents(GhostChildren);bool FoundCorner=false;
 for(auto* Child:GhostChildren)if(auto* C=Cast<AHansaCompoundPresentation>(Child->GetChildActor()))FoundCorner|=C->SelectedLayoutId==TEXT("CornerLeft");
 TestTrue(TEXT("Rotated placement ghost previews the same corner context"),FoundCorner);
 B->ResidentialCompound.Reset();A->ApplyProjection(P,*Foundation);TestFalse(TEXT("Legacy/fallback path clears compound"),A->QueryCompound().bActive);
 W->DestroyWorld(false);FAssetRegistryModule::AssetDeleted(B.Get());Assets.RemovePrimaryAssetsForTypeInMountPoint(AssetId.PrimaryAssetType,TEXT("/Temp/HansaCompoundProjectionFixture"));B->ClearFlags(RF_Public | RF_Standalone);return true;
}
#endif
