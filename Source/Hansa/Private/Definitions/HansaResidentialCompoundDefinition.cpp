#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

namespace
{
uint64 Mix(uint64 Seed, const FString& Key)
{
 const FTCHARToUTF8 Bytes(*Key);
 for (int32 I=0; I<Bytes.Length(); ++I) { Seed ^= static_cast<uint8>(Bytes.Get()[I]); Seed *= 1099511628211ull; }
 return Seed;
}
void Issue(TArray<FHansaDefinitionValidationIssue>& Out, const FString& Path, const FString& Cause)
{
 Out.Add({EHansaDefinitionValidationSeverity::Error, TEXT("HSA-COMPOUND"), Path, FText::FromString(Cause),
  FText::FromString(TEXT("Correct the named layout at real scale; enlarge the logical footprint explicitly when needed."))});
}
bool Promoted(const FSoftObjectPath& Path)
{
 const FString S=Path.ToString();
 return Path.IsValid() && !S.Contains(TEXT("/Developer/")) && !S.Contains(TEXT("/Generated/Staging/"));
}
bool Finite(const FVector& V) { return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z); }
bool ValidBox(const FVector& A,const FVector& B) { return Finite(A)&&Finite(B)&&A.X<B.X&&A.Y<B.Y&&A.Z<B.Z; }
FBox Envelope(const FHansaCompoundVariant& V)
{
 return FBox(V.BoundsMin,V.BoundsMax).TransformBy(FTransform(FRotator(0,V.LocalYaw,0),V.LocalPosition,V.Scale));
}
bool Overlap(const FBox& A,const FBox& B)
{
 return A.Min.X<B.Max.X-.01 && A.Max.X>B.Min.X+.01 && A.Min.Y<B.Max.Y-.01 && A.Max.Y>B.Min.Y+.01;
}
// Fence modules may share end posts at butt joints/corners, never cross or stack.
bool FenceEndJoint(const FBox& A,const FBox& B)
{
 const FVector Lo=A.Min.ComponentMax(B.Min),Hi=A.Max.ComponentMin(B.Max);
 if(Hi.X-Lo.X>25 || Hi.Y-Lo.Y>25)return false;
 for(const FBox* Box:{&A,&B})
 {
  const FVector Size=Box->GetSize();
  const int32 Along=Size.X>Size.Y?0:1,Across=1-Along;
  if(Size[Across]>25 || Size[Along]<100)return false;
  if(Hi[Along]>Box->Min[Along]+25 && Lo[Along]<Box->Max[Along]-25)return false;
 }
 return true;
}
bool Contains(const FBox& B,const FVector& P,double Radius=0)
{
 return P.X-Radius>=B.Min.X && P.Y-Radius>=B.Min.Y && P.X+Radius<=B.Max.X && P.Y+Radius<=B.Max.Y;
}
// Slab intersection of a pedestrian centre segment against an expanded horizontal obstacle.
bool Blocked(FVector A,FVector B,const FBox& Box,double Radius)
{
 double Lo=0,Hi=1;
 for(int I=0;I<2;++I)
 {
  const double D=B[I]-A[I], Min=Box.Min[I]-Radius, Max=Box.Max[I]+Radius;
  if(FMath::Abs(D)<1.e-8) { if(A[I]<=Min || A[I]>=Max)return false; }
  else { double T0=(Min-A[I])/D,T1=(Max-A[I])/D;if(T0>T1)Swap(T0,T1);Lo=FMath::Max(Lo,T0);Hi=FMath::Min(Hi,T1);if(Lo>=Hi)return false; }
 }
 return true;
}
}
UHansaResidentialCompoundDefinition::UHansaResidentialCompoundDefinition()
{
 DefinitionCategory=TEXT("Residential compounds"); LocalizationKey=TEXT("Game.Compound.Unnamed");
}
uint64 UHansaResidentialCompoundDefinition::ParcelSeed(const FString& CityId,uint64 Value,uint32 Generation)
{
 return Mix(Mix(14695981039346656037ull,CityId),FString::Printf(TEXT("/%llu/%u"),static_cast<unsigned long long>(Value),Generation));
}
void UHansaResidentialCompoundDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& Out) const
{
 Super::ValidateDefinition(Out);
 if(!StableDefinitionId.StartsWith(TEXT("Compound.")) || SchemaVersion!=1)Issue(Out,TEXT("StableDefinitionId"),TEXT("Expected Compound.* identity and schema version 1."));
 if(FootprintWidthCells<1 || FootprintWidthCells>64 || FootprintHeightCells<1 || FootprintHeightCells>64 || AllowedRoadFrontMask<1 || AllowedRoadFrontMask>15)
  Issue(Out,TEXT("Footprint"),TEXT("Footprint must be 1..64 cells per axis and front mask 1..15."));
 if(PopulationTierId!=TEXT("PopulationTier.Laborer") && PopulationTierId!=TEXT("PopulationTier.Artisan"))Issue(Out,TEXT("PopulationTierId"),TEXT("Residential compounds support labourer or artisan households; the owning residence must match."));
 if(!FMath::IsFinite(ClearanceRadius)||ClearanceRadius<40||ClearanceRadius>200)Issue(Out,TEXT("ClearanceRadius"),TEXT("Pedestrian radius must be 40..200 cm."));
 if(!ValidBox(BoundsMin,BoundsMax)||BoundsMin.X!=-FootprintWidthCells*200. || BoundsMax.X!=FootprintWidthCells*200. ||
  BoundsMin.Y!=-FootprintHeightCells*200. || BoundsMax.Y!=FootprintHeightCells*200. || BoundsMin.Z!=0)
  Issue(Out,TEXT("Bounds"),TEXT("Compound bounds must match the centred logical footprint exactly, with ground Z=0."));
 if(Layouts.IsEmpty()||Layouts.Num()>64)Issue(Out,TEXT("Layouts"),TEXT("Supply 1..64 reviewed layout alternatives."));
 TSet<FString> Ids;
 for(const auto& L:Layouts)
 {
  if(L.LayoutId.IsEmpty()||Ids.Contains(L.LayoutId))Issue(Out,TEXT("Layouts"),TEXT("Layout IDs must be unique and nonempty."));
  Ids.Add(L.LayoutId);ValidateLayout(L,Out,true);
 }
}
void UHansaResidentialCompoundDefinition::ValidateLayout(const FHansaCompoundLayout& L,TArray<FHansaDefinitionValidationIssue>& Out,bool bAssets) const
{
 const FString P=TEXT("Layouts.")+L.LayoutId;
 if(L.DevelopmentStage<1||L.DevelopmentStage>3||L.Weight<1||L.Weight>10000 ||
  (L.Context!=TEXT("Straight")&&L.Context!=TEXT("CornerLeft")&&L.Context!=TEXT("CornerRight")&&L.Context!=TEXT("Edge")))Issue(Out,P,TEXT("Invalid stage, context or weight."));
 if(L.Slots.IsEmpty()||L.Slots.Num()>64||L.Nodes.IsEmpty()||L.Nodes.Num()>128) {Issue(Out,P,TEXT("Expected 1..64 slots and 1..128 access nodes."));return;}
 TMap<FString,const FHansaCompoundNode*> Nodes;
 int32 RoadCount=0,PrincipalCount=0;
 const FBox Parcel(BoundsMin,BoundsMax);
 for(const auto& N:L.Nodes)
 {
  if(N.NodeId.IsEmpty()||Nodes.Contains(N.NodeId)||!Finite(N.Position)||FMath::Abs(N.Position.Z)>.01 ||
   !Contains(Parcel,N.Position,N.bRoadEntrance?0:ClearanceRadius))Issue(Out,P+TEXT(".Nodes.")+N.NodeId,TEXT("Duplicate or invalid node, or insufficient parcel-edge clearance."));
  Nodes.Add(N.NodeId,&N);
  if(N.bRoadEntrance) {++RoadCount;if(FMath::Abs(N.Position.X-BoundsMax.X)>.01 || FMath::Abs(N.Position.Y)>BoundsMax.Y-ClearanceRadius)Issue(Out,P,TEXT("Road entrance must meet the +X front edge with side clearance."));}
 }
 if(RoadCount!=1)Issue(Out,P,TEXT("Exactly one road entrance is required."));
 TSet<FString> SlotIds;
 TArray<TArray<FBox>> All;
 TArray<FBox> WalkObstacles;
 for(const auto& S:L.Slots)
 {
  const FString SP=P+TEXT(".Slots.")+S.SlotId;
  if(S.SlotId.IsEmpty()||SlotIds.Contains(S.SlotId))Issue(Out,SP,TEXT("Slot IDs must be unique and nonempty."));SlotIds.Add(S.SlotId);
  if(S.PresenceBasisPoints<0||S.PresenceBasisPoints>10000||(S.bRequired&&S.PresenceBasisPoints!=10000))Issue(Out,SP,TEXT("Invalid inclusion probability for this slot."));
  if(S.bPrincipal) {++PrincipalCount;if(!S.bRequired || S.Group!=TEXT("Dwelling"))Issue(Out,SP,TEXT("Principal must be a required dwelling."));}
  if((S.Group==TEXT("Dwelling")||S.Group==TEXT("Workshop"))&&!Nodes.Contains(S.EntranceNodeId))Issue(Out,SP,TEXT("Dwelling/workshop needs a named accessible entrance node."));
  if(S.Variants.IsEmpty()||S.Variants.Num()>16)Issue(Out,SP,TEXT("Expected 1..16 explicit variants."));
  TArray<FBox> Boxes;TSet<FString> VariantIds;
  for(const auto& V:S.Variants)
  {
   if(V.VariantId.IsEmpty()||VariantIds.Contains(V.VariantId))Issue(Out,SP,TEXT("Variant IDs must be unique and nonempty."));VariantIds.Add(V.VariantId);
   const bool bSurface=S.Group==TEXT("Surface");
   const bool bScaleValid=bSurface ? (Finite(V.Scale)&&V.Scale.X>0&&V.Scale.Y>0&&V.Scale.X<=256&&V.Scale.Y<=256&&V.Scale.Z==1&&V.BoundsMin.Z==0&&V.BoundsMax.Z<=2) : V.Scale.Equals(FVector::OneVector,.000001);
   if(!Finite(V.LocalPosition)||!Finite(V.Scale)||!bScaleValid||!FMath::IsFinite(V.LocalYaw)||FMath::Abs(V.LocalYaw)>180 ||
     (S.bPrincipal&&FMath::Abs(V.LocalYaw)>25)||V.Weight<1||V.Weight>10000||!ValidBox(V.BoundsMin,V.BoundsMax))
    {Issue(Out,SP,TEXT("Invalid explicit transform, bounds or weight; architectural scale must remain one; surfaces must be ground sheets at most 2cm thick."));continue;}
   const FBox B=Envelope(V);if(!bSurface)Boxes.Add(B);
   if(bSurface) { if(V.bUseSimpleCollisionForAccess)Issue(Out,SP,TEXT("Ground surfaces cannot request passage collision.")); }
   else if(V.bUseSimpleCollisionForAccess)
   {
    UStaticMesh* Passage=Promoted(V.Mesh.ToSoftObjectPath())?V.Mesh.LoadSynchronous():nullptr;
    const UBodySetup* Collision=Passage?Passage->GetBodySetup():nullptr;
    if(!Collision||Collision->AggGeom.ConvexElems.IsEmpty()||Collision->AggGeom.BoxElems.Num()||Collision->AggGeom.SphereElems.Num()||Collision->AggGeom.SphylElems.Num())
     Issue(Out,SP,TEXT("Open access requires explicit convex collision; missing/unsupported collision cannot prove a passage."));
    else for(const auto& Hull:Collision->AggGeom.ConvexElems)
    {
     const FBox Obstacle=Hull.ElemBox.TransformBy(Hull.GetTransform()).TransformBy(FTransform(FRotator(0,V.LocalYaw,0),V.LocalPosition));
     if(Obstacle.Min.Z<210&&Obstacle.Max.Z>0)WalkObstacles.Add(Obstacle);
    }
   }
   else WalkObstacles.Add(B);
   if(!Contains(Parcel,B.Min)||!Contains(Parcel,B.Max)||B.Min.Z<-.01||B.Max.Z>BoundsMax.Z || (!bSurface&&FMath::Abs(B.Min.Z)>.01)||(bSurface&&(B.Min.Z<0||B.Max.Z>2)))
    Issue(Out,SP,TEXT("Variant overhangs escape the footprint or the base does not meet ground Z=0."));
   if(!Promoted(V.Mesh.ToSoftObjectPath()))Issue(Out,SP,TEXT("Mesh reference must point to promoted content."));
   if(bAssets)
   {
    UStaticMesh* Mesh=Promoted(V.Mesh.ToSoftObjectPath())?V.Mesh.LoadSynchronous():nullptr;
    if(!Mesh)Issue(Out,SP,TEXT("Missing mesh; required slot will trigger whole-parcel fallback."));
    else
    {
     const FBox Actual=Mesh->GetBoundingBox(),Declared=FBox(V.BoundsMin,V.BoundsMax).ExpandBy(.1);
     if(!Declared.IsInsideOrOn(Actual.Min)||!Declared.IsInsideOrOn(Actual.Max))Issue(Out,SP,TEXT("Declared bounds omit actual mesh geometry/roof overhangs."));
     if(V.Materials.Num()>Mesh->GetStaticMaterials().Num())Issue(Out,SP,TEXT("Material override count exceeds mesh slots."));
    }
    for(const auto& M:V.Materials)if(!Promoted(M.ToSoftObjectPath())||!M.LoadSynchronous())Issue(Out,SP,TEXT("Missing or unpromoted material override."));
   }
   if(const auto* const* Entry=Nodes.Find(S.EntranceNodeId))
   {
    // Entry is a clear approach point next to the authored +X doorway side.
    const FVector Local=FTransform(FRotator(0,V.LocalYaw,0),V.LocalPosition).InverseTransformPosition((*Entry)->Position);
    if(Local.X<V.BoundsMax.X||Local.X>V.BoundsMax.X+200||Local.Y<V.BoundsMin.Y||Local.Y>V.BoundsMax.Y)Issue(Out,SP,TEXT("Entrance approach must lie within 2m of the variant +X doorway side."));
   }
  }
  All.Add(MoveTemp(Boxes));
 }
 if(PrincipalCount!=1)Issue(Out,P,TEXT("Exactly one principal dwelling is required."));
 for(int32 A=0;A<All.Num();++A)for(int32 B=A+1;B<All.Num();++B)for(const FBox& X:All[A])for(const FBox& Y:All[B])
  if(Overlap(X,Y) && !(L.Slots[A].Group==TEXT("Fence") && L.Slots[B].Group==TEXT("Fence") && FenceEndJoint(X,Y)))Issue(Out,P,TEXT("Possible slot variants overlap; separate their complete envelopes."));
 TMap<FString,TArray<FString>> Graph;
 for(const auto& N:L.Nodes)
 {
  for(const FBox& B:WalkObstacles)if(Blocked(N.Position,N.Position,B,ClearanceRadius))Issue(Out,P+TEXT(".Nodes.")+N.NodeId,TEXT("Activity/entrance node is obstructed."));
  for(const FString& Link:N.Links)
  {
   const auto* const* Target=Nodes.Find(Link);
   if(!Target||Link==N.NodeId){Issue(Out,P,TEXT("Invalid access link."));continue;}
   Graph.FindOrAdd(N.NodeId).AddUnique(Link);Graph.FindOrAdd(Link).AddUnique(N.NodeId);
   for(const FBox& B:WalkObstacles)if(Blocked(N.Position,(*Target)->Position,B,ClearanceRadius))Issue(Out,P,TEXT("Access link crosses a possible structure or boundary."));
  }
 }
 TArray<FString> Queue;TSet<FString> Seen;
 for(const auto& N:L.Nodes)if(N.bRoadEntrance){Queue.Add(N.NodeId);Seen.Add(N.NodeId);}
 for(int32 I=0;I<Queue.Num();++I)for(const FString& Next:Graph.FindOrAdd(Queue[I]))if(!Seen.Contains(Next)){Seen.Add(Next);Queue.Add(Next);}
 if(Seen.Num()!=Nodes.Num())Issue(Out,P,TEXT("Every entrance and activity node must connect to the road."));
}
FHansaCompoundComposition UHansaResidentialCompoundDefinition::Compose(uint64 Seed,int32 Stage,FName Context,const FString& District) const
{
 FHansaCompoundComposition R;
 if(FootprintWidthCells<1||FootprintWidthCells>64||FootprintHeightCells<1||FootprintHeightCells>64||AllowedRoadFrontMask<1||AllowedRoadFrontMask>15||
   !ValidBox(BoundsMin,BoundsMax)||BoundsMin.X!=-FootprintWidthCells*200.||BoundsMax.X!=FootprintWidthCells*200.||BoundsMin.Y!=-FootprintHeightCells*200.||BoundsMax.Y!=FootprintHeightCells*200.||BoundsMin.Z!=0||!FMath::IsFinite(ClearanceRadius)||ClearanceRadius<40||ClearanceRadius>200)
 {Issue(R.Issues,TEXT("Footprint"),TEXT("Invalid parcel envelope or clearance; fallback required."));return R;}
 TArray<const FHansaCompoundLayout*> Candidates;
 for(const auto& L:Layouts)if(L.DevelopmentStage==Stage&&L.Context==Context&&(L.DistrictIds.IsEmpty()||L.DistrictIds.Contains(District)))Candidates.Add(&L);
 Candidates.Sort([](const auto& A,const auto& B){return A.LayoutId<B.LayoutId;});
 int64 Total=0;for(const auto* L:Candidates)if(L->Weight>0&&L->Weight<=10000)Total+=L->Weight;
 if(!Total){Issue(R.Issues,TEXT("Layouts"),TEXT("No eligible layout for this stage, road context and district."));return R;}
 uint64 Pick=Mix(Seed,TEXT("layout"))%Total;const FHansaCompoundLayout* Selected=nullptr;
 for(const auto* L:Candidates){if(L->Weight<=0||L->Weight>10000)continue;if(Pick<static_cast<uint64>(L->Weight)){Selected=L;break;}Pick-=L->Weight;}
 if(!Selected)return R;
 ValidateLayout(*Selected,R.Issues,false);if(!R.Issues.IsEmpty())return R;
 R.LayoutId=Selected->LayoutId;R.Nodes=Selected->Nodes;
 TArray<const FHansaCompoundSlot*> Slots;for(const auto& S:Selected->Slots)Slots.Add(&S);
 Slots.Sort([](const auto& A,const auto& B){return A.SlotId<B.SlotId;});
 for(const auto* S:Slots)
 {
  const uint64 SlotSeed=Mix(Seed,S->SlotId);
  if(!S->bRequired&&Mix(SlotSeed,TEXT("presence"))%10000>=static_cast<uint64>(S->PresenceBasisPoints))continue;
  TArray<const FHansaCompoundVariant*> Variants;int64 Sum=0;
  for(const auto& V:S->Variants){Variants.Add(&V);Sum+=V.Weight;}
  Variants.Sort([](const auto& A,const auto& B){return A.VariantId<B.VariantId;});
  uint64 Choice=Mix(SlotSeed,TEXT("variant"))%Sum;
  for(const auto* V:Variants){if(Choice<static_cast<uint64>(V->Weight)){R.Instances.Add({S->SlotId,S->Group,V->Mesh,V->Materials,FTransform(FRotator(0,V->LocalYaw,0),V->LocalPosition,V->Scale),FBox(V->BoundsMin,V->BoundsMax),S->bRequired});break;}Choice-=V->Weight;}
 }
 return R;
}
void UHansaResidentialCompoundDefinition::AppendDefinitionHashData(FString& Data) const
{
 Super::AppendDefinitionHashData(Data);
 // Reflected interchange is also the hash contract: every authored field, including nested paths, participates.
 for(TFieldIterator<FProperty> It(StaticClass(),EFieldIteratorFlags::ExcludeSuper);It;++It)
 {
  FString Text;It->ExportTextItem_Direct(Text,It->ContainerPtrToValuePtr<void>(this),nullptr,nullptr,PPF_None);
  Data+=It->GetName()+TEXT("=")+Text+TEXT("\n");
 }
}
