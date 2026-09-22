#include "World/HansaCompoundPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "World/HansaCompoundGround.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"
#include "LandscapeProxy.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AHansaCompoundPresentation::AHansaCompoundPresentation()
{
 PrimaryActorTick.bCanEverTick=false;bReplicates=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CompoundRoot")));
 RootComponent->SetMobility(EComponentMobility::Movable);
 GroundCoverage=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GroundCoverage"));
 Foundations=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Foundations"));
 for(auto* Mesh:{GroundCoverage.Get(),Foundations.Get()})
 {
  Mesh->SetupAttachment(RootComponent);Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCanEverAffectNavigation(false);
 }
 GroundCoverage->SetCastShadow(false);
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Stone(TEXT("/Game/Mesh/labour-housing-kit/Materials/M_LabourKit_Fieldstone.M_LabourKit_Fieldstone"));
 Foundations->SetMaterial(0,Stone.Object);
}
void AHansaCompoundPresentation::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);RebuildPreview();
}
void AHansaCompoundPresentation::ClearBatches()
{
 ClearGrass();FittedKey.Reset();InstanceBindings.Reset();GroundComposition={};
 GroundCoverage->ClearAllMeshSections();Foundations->ClearAllMeshSections();
 TArray<UHierarchicalInstancedStaticMeshComponent*> Existing;GetComponents(Existing);
 for(auto* Batch:Existing)if(Batch&&Batch->ComponentTags.Contains(TEXT("Hansa.Compound.Batch")))Batch->DestroyComponent();
 Batches.Reset();AccessNodes.Reset();SelectedLayoutId.Reset();
}
void AHansaCompoundPresentation::RebuildPreview()
{
 AppliedKey.Reset();ApplyCompound(Definition,static_cast<uint64>(PreviewSeed),Stage,RoadContext,DistrictId);FitGround(false,false);
}
bool AHansaCompoundPresentation::ApplyCompound(UHansaResidentialCompoundDefinition* D,uint64 Seed,int32 S,FName Context,const FString& District)
{
 const FString Key=FString::Printf(TEXT("%s/%llu/%llu/%d/%s/%s"),D?*D->GetPathName():TEXT(""),
  static_cast<unsigned long long>(D?(D->ContentHash?D->ContentHash:D->ComputeDeterministicContentHash()):0),static_cast<unsigned long long>(Seed),S,*Context.ToString(),*District);
 if(Key==AppliedKey)return !SelectedLayoutId.IsEmpty();
 AppliedKey=Key;ClearBatches();AppliedSeed=Seed;Diagnostics.Reset();Definition=D;Stage=S;RoadContext=Context;DistrictId=District;
 if(!D){Diagnostics.Add(TEXT("Compound definition unavailable; parcel fallback active."));return false;}
 auto Composition=D->Compose(Seed,S,Context,District);
 for(const auto& E:Composition.Issues)Diagnostics.Add(E.PropertyPath+TEXT(": ")+E.Cause.ToString());
 if(!Composition.IsValid())return false;
 TArray<const FHansaCompoundInstance*> Available;
 for(const auto& I:Composition.Instances)
 {
  UStaticMesh* Mesh=I.Mesh.LoadSynchronous();
  bool Valid=Mesh!=nullptr;
  if(Mesh)
  {
   const FBox Actual=Mesh->GetBoundingBox(),Allowed=I.AuthoredBounds.ExpandBy(.1);
   Valid=Allowed.IsInsideOrOn(Actual.Min)&&Allowed.IsInsideOrOn(Actual.Max)&&I.Materials.Num()<=Mesh->GetStaticMaterials().Num();
  }
  for(const auto& M:I.Materials)Valid&=M.LoadSynchronous()!=nullptr;
  if(!Valid){Diagnostics.Add(I.SlotId+TEXT(": unavailable mesh/material or changed mesh envelope; ")+(I.bRequired?TEXT("whole-parcel fallback active."):TEXT("optional slot omitted.")));if(I.bRequired)return false;}
  else Available.Add(&I);
 }
 TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Groups;
 for(const auto* I:Available)
 {
  if(I->Group==TEXT("Surface"))continue; // Replaced by terrain-conforming coverage.
  FString Group=I->Mesh.ToSoftObjectPath().ToString();for(const auto& M:I->Materials)Group+=TEXT("|")+M.ToSoftObjectPath().ToString();
  auto* Batch=Groups.FindRef(Group);
  if(!Batch)
  {
   Batch=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
   Batch->ComponentTags.Add(TEXT("Hansa.Compound.Batch"));
   Batch->SetupAttachment(RootComponent);Batch->SetMobility(EComponentMobility::Movable);
   Batch->SetStaticMesh(I->Mesh.Get());Batch->SetCollisionProfileName(TEXT("NoCollision"));
   Batch->SetGenerateOverlapEvents(false);Batch->SetCanEverAffectNavigation(false);
   Batch->SetComponentTickEnabled(false);Batch->bAutoRebuildTreeOnInstanceChanges=false;
   // Authored preview Actors can participate in the normal HLOD instancing builder.
#if WITH_EDITORONLY_DATA
   Batch->bEnableAutoLODGeneration=true;Batch->HLODBatchingPolicy=EHLODBatchingPolicy::Instancing;
#endif
   for(int32 M=0;M<I->Materials.Num();++M)Batch->SetMaterial(M,I->Materials[M].Get());
   AddInstanceComponent(Batch);Batch->RegisterComponent();Batches.Add(Batch);Groups.Add(Group,Batch);
  }
  const int32 Instance=Batch->AddInstance(I->Transform);
  InstanceBindings.Add({Batches.IndexOfByKey(Batch),Instance,I->Transform,int32(I-Composition.Instances.GetData())});
 }
 for(UHierarchicalInstancedStaticMeshComponent* Batch:Batches){Batch->bAutoRebuildTreeOnInstanceChanges=true;Batch->BuildTreeIfOutdated(false,true);}
 SelectedLayoutId=Composition.LayoutId;AccessNodes=Composition.Nodes;GroundComposition=MoveTemp(Composition);return true;
}
FBox AHansaCompoundPresentation::GetParcelBounds() const
{
 return Definition&&!SelectedLayoutId.IsEmpty()?FBox(Definition->BoundsMin,Definition->BoundsMax):FBox(ForceInit);
}

void AHansaCompoundPresentation::ClearGrass()
{
 for(UObject* ExclusionOwner:GrassExclusionOwners)if(ExclusionOwner)
 {
  ALandscapeProxy::RemoveExclusionBox(FWeakObjectPtr(ExclusionOwner));
  if(auto* Token=Cast<USceneComponent>(ExclusionOwner))Token->DestroyComponent();
 }
 GrassExclusionOwners.Reset();
}
void AHansaCompoundPresentation::EndPlay(const EEndPlayReason::Type Reason)
{
 FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
 FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
 ClearGrass();Super::EndPlay(Reason);
}
void AHansaCompoundPresentation::Destroyed()
{
 ClearGrass();Super::Destroyed();
}
void AHansaCompoundPresentation::FitGround(bool bSuppressGrass,bool bConnectRoad)
{
 using namespace Hansa::Game;
 if(!GroundComposition.IsValid()||!Definition)return;
 const FTransform ActorTransform=GetActorTransform();
 TArray<FVector> RoadCenters;
 bool HasRoadManager=false;
 if(bConnectRoad)for(TActorIterator<AHansaLubeckWorldFoundation> F(GetWorld());F;++F)
  for(TActorIterator<AHansaPlacementProjectionManager> M(GetWorld());M;++M)if(M->IsBoundTo(**F))
  {
   HasRoadManager=true;
   for(FIntPoint Cell:M->GetRoadCells())
   {
    const FVector P=ActorTransform.InverseTransformPosition(F->GetActorTransform().TransformPosition(
     Hansa::Game::LubeckPlacementGrid::GridToWorld({Cell.X,Cell.Y},0)));
    if(FMath::Abs(P.X-Definition->BoundsMax.X-200)<1&&P.Y>=Definition->BoundsMin.Y&&P.Y<=Definition->BoundsMax.Y)
     RoadCenters.Add(FVector(P.X,P.Y,0));
   }
  }
 RoadCenters.Sort([](const FVector& A,const FVector& B){return A.Y<B.Y;});
 FString RoadKey;for(const FVector& P:RoadCenters)RoadKey+=P.ToString();
 const FString Key=AppliedKey+FString::Printf(TEXT("/%d/%d/%d/"),bSuppressGrass,bConnectRoad,HasRoadManager)+RoadKey;
 if(FittedKey==Key&&FittedTransform.Equals(ActorTransform,.01))return;
 FittedKey=Key;FittedTransform=ActorTransform;bLastSuppressGrass=bSuppressGrass;bLastConnectRoad=bConnectRoad;
 ClearGrass();GroundCoverage->ClearAllMeshSections();Foundations->ClearAllMeshSections();
 GroundSampleCount=0;MaximumFoundationDepth=0;bTerrainComplete=true;
 auto GroundPoint=[&](const FVector& Local,double Clearance)
 {
  FVector World=ActorTransform.TransformPosition(Local);FHitResult Hit;++GroundSampleCount;
  if(TerrainPlacement::Trace(GetWorld(),World+FVector(0,0,1000000),World-FVector(0,0,1000000),Hit))
   World.Z=Hit.ImpactPoint.Z+Clearance;
  else {bTerrainComplete=false;World=ActorTransform.TransformPosition(FVector(Local.X,Local.Y,Clearance));}
  return ActorTransform.InverseTransformPosition(World);
 };
 auto Exclude=[&](FBox Box)
 {
  if(!bSuppressGrass)return;
  Box.Min.Z-=10000;Box.Max.Z+=10000;
  UObject* ExclusionOwner=NewObject<USceneComponent>(this);GrassExclusionOwners.Add(ExclusionOwner);
  ALandscapeProxy::AddExclusionBox(FWeakObjectPtr(ExclusionOwner),Box);
 };
 struct FPath {FVector A,B;};
 TArray<FPath> Paths;
 TSet<FString> DrawnLinks;
 AccessNodes=GroundComposition.Nodes;
 for(auto& Node:AccessNodes)Node.Position=GroundPoint(Node.Position,2);
 for(const auto& Node:GroundComposition.Nodes)
 {
  for(const auto& Link:Node.Links)
  {
   const auto* Other=GroundComposition.Nodes.FindByPredicate([&](const FHansaCompoundNode& N){return N.NodeId==Link;});
   if(Other)
   {
    const FString LinkKey=Node.NodeId<Other->NodeId?Node.NodeId+TEXT("/")+Other->NodeId:Other->NodeId+TEXT("/")+Node.NodeId;
    if(!DrawnLinks.Contains(LinkKey)){DrawnLinks.Add(LinkKey);Paths.Add({Node.Position,Other->Position});}
   }
  }
  if(Node.bRoadEntrance&&bConnectRoad)
  {
   // Standalone callers may explicitly supply a road-front assertion without a manager.
   TArray<FVector> Candidates=RoadCenters;
   if(!HasRoadManager)Candidates.Add(Node.Position+FVector(200,0,0));
   const auto Approach=CompoundGround::RoadApproach(Node.Position,Candidates);
   for(int32 P=1;P<Approach.Num();++P)Paths.Add({Approach[P-1],Approach[P]});
  }
 }
 TArray<FVector> StoneVertices,StoneNormals;TArray<int32> StoneTriangles;TArray<FVector2D> StoneUV;
 TArray<FBox> Structures;
 for(const auto& Binding:InstanceBindings)
 {
  const auto& I=GroundComposition.Instances[Binding.CompositionIndex];
  FTransform Fit=Binding.Authored;
  const bool Structure=I.Group==TEXT("Dwelling")||I.Group==TEXT("Workshop");
  if(Structure)
  {
   const auto Survey=CompoundGround::Survey(GetWorld(),I.Transform*ActorTransform,I.AuthoredBounds);
   GroundSampleCount+=Survey.Samples;bTerrainComplete&=Survey.IsComplete();
   if(Survey.Hits>0)
   {
    Fit.AddToTranslation(FVector(0,0,Survey.Maximum-I.AuthoredBounds.Min.Z));
    MaximumFoundationDepth=FMath::Max(MaximumFoundationDepth,Survey.Maximum-Survey.Minimum);
   }
   FBox Foot=I.AuthoredBounds;
   // Roof overhangs remain in collision/selection bounds, not in the stone plinth.
   const FVector Inset(FMath::Min(25.,Foot.GetSize().X*.08),FMath::Min(25.,Foot.GetSize().Y*.08),0);
   Foot.Min+=Inset;Foot.Max-=Inset;
   Structures.Add(Foot.TransformBy(I.Transform));
   Exclude(Foot.TransformBy(Fit*ActorTransform));
   const FVector Corners[]={FVector(Foot.Min.X,Foot.Min.Y,Foot.Min.Z),FVector(Foot.Max.X,Foot.Min.Y,Foot.Min.Z),
    FVector(Foot.Max.X,Foot.Max.Y,Foot.Min.Z),FVector(Foot.Min.X,Foot.Max.Y,Foot.Min.Z)};
   for(int32 Edge=0;Edge<4;++Edge)
   {
    const FVector A=Fit.TransformPosition(Corners[Edge]),B=Fit.TransformPosition(Corners[(Edge+1)%4]);
    const int32 Segments=FMath::Max(1,FMath::CeilToInt(FVector::Distance(A,B)/100));
    for(int32 J=0;J<Segments;++J)
    {
     const FVector TopA=FMath::Lerp(A,B,double(J)/Segments),TopB=FMath::Lerp(A,B,double(J+1)/Segments);
     FVector BottomA=GroundPoint(TopA,-8),BottomB=GroundPoint(TopB,-8);
     BottomA.Z=FMath::Min(BottomA.Z,TopA.Z);BottomB.Z=FMath::Min(BottomB.Z,TopB.Z);
     const int32 K=StoneVertices.Num();
     StoneVertices.Append({TopA,BottomA,BottomB,TopB});
     StoneTriangles.Append({K,K+1,K+2,K,K+2,K+3});
     const FVector Normal=FVector::CrossProduct(BottomA-TopA,BottomB-TopA).GetSafeNormal();
     for(int32 N=0;N<4;++N)StoneNormals.Add(Normal);
     StoneUV.Append({FVector2D(0,TopA.Z/200),FVector2D(0,BottomA.Z/200),
       FVector2D(FVector::Distance(TopA,TopB)/200,BottomB.Z/200),FVector2D(FVector::Distance(TopA,TopB)/200,TopB.Z/200)});
    }
   }
  }
  else
  {
   const FVector Bottom=Fit.TransformPosition(FVector(0,0,I.AuthoredBounds.Min.Z));
   Fit.AddToTranslation(FVector(0,0,GroundPoint(Bottom,0).Z-Bottom.Z));
  }
  Batches[Binding.Batch]->UpdateInstanceTransform(Binding.Instance,Fit,false,true,true);
 }
 for(UHierarchicalInstancedStaticMeshComponent* Batch:Batches)Batch->BuildTreeIfOutdated(false,true);
 if(!StoneVertices.IsEmpty())Foundations->CreateMeshSection(0,StoneVertices,StoneTriangles,StoneNormals,StoneUV,{}, {},false);
 // One continuous surface; no vertical perimeter, overlapping disks, or raised parcel sheets.
 const FBox Plot(Definition->BoundsMin,Definition->BoundsMax);
 // Include the rounded approach tip beyond the road center, avoiding a clipped end.
 FBox Coverage=Plot; if(bConnectRoad)Coverage.Max.X+=300;
 const int32 NX=FMath::Clamp(FMath::CeilToInt(Coverage.GetSize().X/50),1,512);
 const int32 NY=FMath::Clamp(FMath::CeilToInt(Coverage.GetSize().Y/50),1,512);
 TArray<FVector> Vertices,Normals;TArray<int32> Triangles;TArray<FVector2D> UV;TArray<FLinearColor> Colors;
 auto DistanceToPath=[](FVector P,const FPath& Path)
 {
  P.Z=0;FVector A=Path.A,B=Path.B;A.Z=B.Z=0;
  return FVector::Distance(P,FMath::ClosestPointOnSegment(P,A,B));
 };
 const double SeedOffset=double(AppliedSeed%100003);
 for(int32 Y=0;Y<=NY;++Y)for(int32 X=0;X<=NX;++X)
 {
  FVector P(FMath::Lerp(Coverage.Min.X,Coverage.Max.X,double(X)/NX),FMath::Lerp(Coverage.Min.Y,Coverage.Max.Y,double(Y)/NY),0);
  const FVector World=ActorTransform.TransformPosition(P);
  const double Noise=FMath::PerlinNoise2D(FVector2D(World.X/180+SeedOffset,World.Y/180));
  const double Edge=FMath::Min(FMath::Min(P.X-Plot.Min.X,Plot.Max.X-P.X),FMath::Min(P.Y-Plot.Min.Y,Plot.Max.Y-P.Y));
  const double EdgeFade=FMath::Clamp((Edge-25+Noise*55)/140,0.,1.);
  double Wear=0;
  for(const auto& Path:Paths)Wear=FMath::Max(Wear,FMath::Clamp((90+Noise*16-DistanceToPath(P,Path))/40,0.,1.));
  double NearHouse=0;
  for(const FBox& B:Structures)
  {
   const FVector Nearest(FMath::Clamp(P.X,B.Min.X,B.Max.X),FMath::Clamp(P.Y,B.Min.Y,B.Max.Y),0);
   NearHouse=FMath::Max(NearHouse,FMath::Clamp((160-FVector::Distance(P,Nearest))/160,0.,1.)*.7);
  }
  const double Yard=EdgeFade*FMath::Clamp(.24+Noise*.7+NearHouse,0.,.78);
  const double Alpha=FMath::Max(Yard,Wear*.94);
  Vertices.Add(GroundPoint(P,1.5));Normals.Add(FVector::UpVector);
  UV.Add(FVector2D(World.X/300,World.Y/300));Colors.Add(FLinearColor(1,1,1,Alpha));
 }
 for(int32 Y=0;Y<NY;++Y)for(int32 X=0;X<NX;++X)
 {
  const int32 A=Y*(NX+1)+X,B=A+1,C=A+NX+1,D=C+1;
  Triangles.Append({A,D,B,A,C,D});
 }
 GroundCoverage->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UV,Colors,{},false);
 if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Hansa/World/Ground/M_CompoundDirt.M_CompoundDirt")))
  {GroundCoverage->SetMaterial(0,Material);GroundCoverage->SetVisibility(true);}
 else {GroundCoverage->SetVisibility(false);Diagnostics.AddUnique(TEXT("Ground coverage material missing; run HansaCompoundGroundMaterials -Apply."));}
 for(const auto& Path:Paths)
 {
  const int32 N=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Path.A,Path.B)/80));
  for(int32 J=0;J<N;++J)
  {
   const FVector A=ActorTransform.TransformPosition(FMath::Lerp(Path.A,Path.B,double(J)/N));
   const FVector B=ActorTransform.TransformPosition(FMath::Lerp(Path.A,Path.B,double(J+1)/N));
   FBox Box(ForceInit);Box+=A;Box+=B;Exclude(Box.ExpandBy(FVector(48,48,0)));
  }
 }
}


void AHansaCompoundPresentation::BeginPlay()
{
 Super::BeginPlay();
 LevelAddedHandle=FWorldDelegates::LevelAddedToWorld.AddUObject(this,&ThisClass::TerrainLevelChanged);
 LevelRemovedHandle=FWorldDelegates::LevelRemovedFromWorld.AddUObject(this,&ThisClass::TerrainLevelChanged);
}
void AHansaCompoundPresentation::TerrainLevelChanged(ULevel* Level,UWorld* World)
{
 if(World!=GetWorld()||FittedKey.IsEmpty())return;
 FBox Changed(ForceInit);
 if(Level)for(AActor* Actor:Level->Actors)
  if(Actor&&(Actor->IsA<ALandscapeProxy>()||Actor->ActorHasTag(TEXT("Hansa.Terrain"))))Changed+=Actor->GetComponentsBoundingBox(true);
 if(Changed.IsValid&&!Changed.ExpandBy(250).Intersect(GetComponentsBoundingBox(true)))return;
 World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]()
 {
  FittedKey.Reset();FitGround(bLastSuppressGrass,bLastConnectRoad);
 }));
}
