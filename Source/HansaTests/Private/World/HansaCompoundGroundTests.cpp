#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "World/HansaCompoundGround.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaCompoundPresentation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundGroundTest,"Hansa.World.CompoundGround.Fitting",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundGroundTest::RunTest(const FString&)
{
 using namespace Hansa::Game;
 const TArray<FVector> OffsetRoad={FVector(1000,600,0)};
 const auto Approach=CompoundGround::RoadApproach(FVector(800,0,0),OffsetRoad);
 TestEqual(TEXT("Offset access follows frontage before crossing"),Approach.Num(),4);
 if(Approach.Num()==4){TestTrue(TEXT("Approach stays inside the plot"),Approach[1].X<800&&Approach[2].X<800);TestTrue(TEXT("Approach reaches actual road cell"),Approach.Last().Y>=420&&Approach.Last().X==1000);}
 TestTrue(TEXT("No road cannot invent an approach"),CompoundGround::RoadApproach(FVector(800,0,0),{}).IsEmpty());
 TestTrue(TEXT("Offset approach reaches the opaque core, not the empty cell corner"),
  !Approach.IsEmpty()&&FMath::Abs(Approach.Last().Y-600)<=60);
 const TArray<FVector> RoadsForward={FVector(1000,-600,0),FVector(1000,600,0)};
 const TArray<FVector> RoadsReverse={RoadsForward[1],RoadsForward[0]};
 TestTrue(TEXT("Equidistant road selection is stable across road insertion order"),
  CompoundGround::RoadApproach(FVector(800,0,0),RoadsForward).Last().Equals(
   CompoundGround::RoadApproach(FVector(800,0,0),RoadsReverse).Last()));
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
 auto* Terrain=W->SpawnActor<AStaticMeshActor>();
 auto* Mesh=Terrain->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
 Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Mesh->SetCollisionProfileName(TEXT("BlockAll"));Terrain->Tags.Add(TEXT("Hansa.Terrain"));
 Terrain->SetActorScale3D(FVector(80,80,1));Terrain->SetActorLocation(FVector(0,0,-50));
 Terrain->SetActorRotation(FRotator(4,0,0));
 auto* D=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,TEXT("/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_SharedCourt.DA_Compound_SharedCourt"));
 if(!TestNotNull(TEXT("Approved compound"),D))return false;
 auto* A=W->SpawnActor<AHansaCompoundPresentation>();
 TestTrue(TEXT("Compose house"),A->ApplyCompound(D,1234,1,TEXT("Straight"),TEXT("District.Lubeck.LateMedieval")));
 TArray<USceneComponent*> BaselineComponents;A->GetComponents(BaselineComponents);
 A->FitGround(true,true);
 TestTrue(TEXT("Full plot sampled"),A->bTerrainComplete&&A->GroundSampleCount>100);
 TestTrue(TEXT("Slope creates local foundations"),A->MaximumFoundationDepth>5&&A->MaximumFoundationDepth<120);
 TestTrue(TEXT("Landscape grass exclusions installed"),A->GrassExclusionOwners.Num()>0);
 TestEqual(TEXT("Single yard surface"),A->GroundCoverage->GetNumSections(),1);
 TestNotNull(TEXT("Production dirt material"),A->GroundCoverage->GetMaterial(0));
 auto* Section=A->GroundCoverage->GetProcMeshSection(0);
 bool Fit=true,Soft=false,Holes=false;
 TArray<FVector> Before;
 if(Section)for(const auto& V:Section->ProcVertexBuffer)
 {
  const FVector P=A->GetActorTransform().TransformPosition(V.Position);FHitResult Hit;
  Fit&=TerrainPlacement::Trace(W,P+FVector(0,0,10000),P-FVector(0,0,10000),Hit)&&FMath::IsNearlyEqual(P.Z-Hit.ImpactPoint.Z,1.5,.1);
  Soft|=V.Color.A>0&&V.Color.A<240;Holes|=V.Color.A==0;Before.Add(V.Position);
 }
 TestTrue(TEXT("Every yard vertex follows terrain"),Fit);
 TestTrue(TEXT("Soft coverage and exposed original ground"),Soft&&Holes);
 for(UHierarchicalInstancedStaticMeshComponent* Batch:A->Batches)for(int32 I=0;I<Batch->GetInstanceCount();++I)
 {
  FTransform T;Batch->GetInstanceTransform(I,T,true);TestTrue(TEXT("Structures remain upright"),T.GetRotation().GetUpVector().Equals(FVector::UpVector,.001));
 }
 const int32 Exclusions=A->GrassExclusionOwners.Num();const int32 Samples=A->GroundSampleCount;
 A->FitGround(true,true);
 TestEqual(TEXT("Repeated projection keeps exclusion count"),A->GrassExclusionOwners.Num(),Exclusions);
 TestEqual(TEXT("Repeated projection uses fitting cache"),A->GroundSampleCount,Samples);
 // Reconstructed actor uses the same stable seed and geometry (save/load projection contract).
 auto* Restored=W->SpawnActor<AHansaCompoundPresentation>();
 Restored->ApplyCompound(D,1234,1,TEXT("Straight"),TEXT("District.Lubeck.LateMedieval"));Restored->FitGround(false,true);
 auto* After=Restored->GroundCoverage->GetProcMeshSection(0);
 TestTrue(TEXT("Reconstruction vertex count"),After&&After->ProcVertexBuffer.Num()==Before.Num());
 if(After&&After->ProcVertexBuffer.Num()==Before.Num())for(int32 I=0;I<Before.Num();++I)
  if(!Before[I].Equals(After->ProcVertexBuffer[I].Position,.001)){AddError(TEXT("Ground changed after reconstruction"));break;}
 TestEqual(TEXT("Ghost never suppresses grass"),Restored->GrassExclusionOwners.Num(),0);
 A->FitGround(true,false);
 TestTrue(TEXT("Removing road removes only external-path exclusions"),A->GrassExclusionOwners.Num()<Exclusions);
 A->FitGround(false,false);
 TArray<USceneComponent*> ClearedComponents;A->GetComponents(ClearedComponents);
 TestEqual(TEXT("Rebuilding exclusions does not leak owned components"),ClearedComponents.Num(),BaselineComponents.Num());
 A->FitGround(true,false);
 A->Destroy();TestEqual(TEXT("Demolition unregisters owned exclusions"),A->GrassExclusionOwners.Num(),0);
 const auto Composition=D->Compose(1234,1,TEXT("Straight"),TEXT("District.Lubeck.LateMedieval"));
 TestTrue(TEXT("Gentle site accepted"),CompoundGround::CanPlace(W,FTransform::Identity,Composition,FBox(D->BoundsMin,D->BoundsMax)));
 Terrain->SetActorRotation(FRotator(10,0,0));
 const auto TallFoundation=CompoundGround::Survey(W,FTransform::Identity,FBox(FVector(-500,-300,0),FVector(500,300,600)));
 TestTrue(TEXT("Moderate slope can exceed foundation limit"),TallFoundation.SlopeDegrees<15&&TallFoundation.Maximum-TallFoundation.Minimum>120&&!TallFoundation.IsBuildable());
 Terrain->SetActorRotation(FRotator(20,0,0));
 TestFalse(TEXT("Steep site rejected"),CompoundGround::CanPlace(W,FTransform::Identity,Composition,FBox(D->BoundsMin,D->BoundsMax)));
 TestTrue(TEXT("Headless world retains planar placement"),CompoundGround::CanPlace(nullptr,FTransform::Identity,Composition,FBox(D->BoundsMin,D->BoundsMax)));
 Terrain->Tags.Reset();Mesh->ComponentTags.Add(TEXT("Hansa.Terrain"));
 Terrain->SetActorLocation(FVector(50000,0,0));
 TestFalse(TEXT("Missing component-tagged terrain is not a headless fallback"),CompoundGround::CanPlace(W,FTransform::Identity,Composition,FBox(D->BoundsMin,D->BoundsMax)));
 Terrain->SetActorLocation(FVector(0,0,-50));
 Terrain->SetActorScale3D(FVector(2,2,1));
 TestFalse(TEXT("Partial terrain coverage rejected"),CompoundGround::CanPlace(W,FTransform::Identity,Composition,FBox(D->BoundsMin,D->BoundsMax)));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompoundGroundAuthorityTest,"Hansa.World.CompoundGround.Authority",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCompoundGroundAuthorityTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
 W->SpawnActor<AHansaLubeckWorldFoundation>();
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
 FString Error;if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(W,Error))){AddError(Error);return false;}
 auto* Terrain=W->SpawnActor<AStaticMeshActor>();
 auto* Mesh=Terrain->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
 Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Mesh->SetCollisionProfileName(TEXT("BlockAll"));Terrain->Tags.Add(TEXT("Hansa.Terrain"));
 Terrain->SetActorScale3D(FVector(500,500,1));Terrain->SetActorRotation(FRotator(22,0,0));
 FHansaPlacementSpec House;House.CityId=Host->GetCityId();House.Anchor={18,16};
 House.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Residence.Laborer.SharedCourt.Stage1")).Value;
 const auto V=Host->ValidatePlacement(House);
 TestTrue(TEXT("Preview includes slope reason"),V.GetReasons().ContainsByPredicate([](const auto& R){return R.Failure==EHansaPlacementFailure::FoundationTooSteep;}));
 auto Road=House;Road.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;Road.Anchor={22,16};
 const TArray<FHansaPlacementSpec> Batch={Road,House};
 const auto Before=Host->BuildProjection();
 const auto Result=Host->PlaceBuildingsForAuthority({Host->GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Batch);
 TestFalse(TEXT("Direct authority batch rejected"),Result.IsSuccess());
 TestEqual(TEXT("Reports house's batch index"),Result.GetFailedCommandIndex(),1);
 TestTrue(TEXT("Rejected batch produces no events"),Result.GetEvents().IsEmpty());
 const auto After=Host->BuildProjection();
 TestTrue(TEXT("No resource, time or placement changes on rejection"),Before&&After&&Before.Value.GetFingerprint()==After.Value.GetFingerprint());
 return !HasAnyErrors();
}
#endif
