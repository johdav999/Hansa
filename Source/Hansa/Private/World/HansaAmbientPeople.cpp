#include "World/HansaAmbientPeople.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/HansaGameMode.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaCityCentrePresentation.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
    const FIntPoint Neighbors[] = {{1,0},{0,1},{-1,0},{0,-1}};
}
using namespace Hansa::Game;
using namespace Hansa::Simulation;

void FAmbientStreetNetwork::Build(TConstArrayView<FHansaBuildingWorldProjection> Buildings, FHansaCityDefinitionId City)
{
    Roads.Reset(); Visits.Reset(); MunicipalPositions.Reset(); MunicipalLinks.Reset();
    for (const auto& B : Buildings)
        if (B.Placement.CityId == City && B.Status != EHansaBuildingWorldStatus::UnderConstruction && B.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Road"))
            for (const auto C : B.OccupiedCells) Roads.Add({C.X,C.Y});
    for (const auto& B : Buildings)
    {
        if (B.Placement.CityId != City || B.Status == EHansaBuildingWorldStatus::UnderConstruction || B.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Road")) continue;
        // One frontage per building, with stable cell ordering from the authoritative projection.
        bool bFound = false;
        for (const auto C : B.OccupiedCells)
        {
            for (const auto D : Neighbors)
            {
                const FIntPoint Road = FIntPoint(C.X,C.Y)+D;
                if (Roads.Contains(Road)) { Visits.Add({B.BuildingId,Road,{C.X,C.Y}}); bFound = true; break; }
            }
            if (bFound) break;
        }
    }
}
void FAmbientStreetNetwork::BuildMunicipal(const AHansaCityCentrePresentation& Centre)
{
    Roads.Reset(); Visits.Reset(); MunicipalPositions.Reset(); MunicipalLinks.Reset();
    // Keep exact authored road positions: municipal streets need not share the construction grid.
    FString Error;
    if (!Centre.ValidateLayout(Error)) return;
    TArray<FBox> Obstacles;
    for (const auto& S : Centre.Slots)
        if (Centre.IsSlotActive(S) && S.bSelectable && S.Mesh)
            Obstacles.Add(S.Mesh->GetBoundingBox().TransformBy(FTransform(FRotator(0,S.Yaw,0),S.Location)*Centre.GetActorTransform()));
    for (const auto& S : Centre.Slots)
    {
        if (!Centre.IsSlotActive(S) || !S.Id.ToString().StartsWith(TEXT("Street."))) continue;
        const FVector P = Centre.GetActorTransform().TransformPosition(S.Location);
        if (Obstacles.ContainsByPredicate([&](const FBox& B) { return B.ExpandBy(80).IsInsideXY(P); })) continue;
        const FIntPoint Key(MunicipalPositions.Num(),0);
        Roads.Add(Key); MunicipalPositions.Add(Key,P); MunicipalLinks.Add(Key);
    }
    for (const auto& A : MunicipalPositions)
        for (const auto& B : MunicipalPositions)
        {
            if (A.Key == B.Key || FVector::DistSquared2D(A.Value,B.Value) > FMath::Square(450.)) continue;
            bool Clear = true;
            for (int32 Step=0; Step<=8 && Clear; ++Step)
            {
                const FVector P=FMath::Lerp(A.Value,B.Value,Step/8.);
                Clear=!Obstacles.ContainsByPredicate([&](const FBox& Box) { return Box.ExpandBy(80).IsInsideXY(P); });
            }
            if (Clear) MunicipalLinks[A.Key].Add(B.Key);
        }
    // Visit the street opposite a landmark, never invent a path through a municipal building.
    for (const auto& S : Centre.Slots)
    {
        if (!Centre.IsSlotActive(S) || !S.bSelectable) continue;
        const FVector P=Centre.GetActorTransform().TransformPosition(S.Location);
        double Best=FMath::Square(1800.); FIntPoint Key; bool Found=false;
        for (const auto& Road : MunicipalPositions)
        {
            const double Distance=FVector::DistSquared2D(P,Road.Value);
            if (!MunicipalLinks[Road.Key].IsEmpty() && Distance<Best) { Best=Distance; Key=Road.Key; Found=true; }
        }
        if (Found) { FAmbientVisit V; V.Road=Key; V.Inside=Key; V.Landmark=S.Id; Visits.Add(V); }
    }
}
bool FAmbientStreetNetwork::Route(FIntPoint From, FIntPoint To, TArray<FIntPoint>& Out) const
{
    Out.Reset();
    if (!Roads.Contains(From) || !Roads.Contains(To)) return false;
    TArray<FIntPoint> Queue{From}; TMap<FIntPoint,FIntPoint> Parent; Parent.Add(From,From);
    for (int32 Head=0; Head<Queue.Num() && Head<32768; ++Head)
    {
        const auto Cell = Queue[Head];
        if (Cell == To)
        {
            for (auto C=To; C!=From; C=Parent[C]) Out.Add(C);
            for (int32 I=0,J=Out.Num()-1; I<J; ++I,--J) Swap(Out[I],Out[J]);
            return true;
        }
        TArray<FIntPoint, TInlineAllocator<4>> Adjacent;
        if (!MunicipalPositions.IsEmpty())
        {
            if (const auto* Links=MunicipalLinks.Find(Cell)) Adjacent.Append(*Links);
        }
        else for (const auto D : Neighbors) Adjacent.Add(Cell+D);
        for (const auto N : Adjacent)
        {
            if (Roads.Contains(N) && !Parent.Contains(N)) { Parent.Add(N,Cell); Queue.Add(N); }
        }
    }
    return false;
}
int32 FAmbientStreetNetwork::DesiredCount(int32 Residents, int32 Ratio, int32 Maximum, int32 RoadsCount, int32 VisitsCount)
{
    if (Residents<=0 || Ratio<=0 || RoadsCount<=0 || VisitsCount<=0) return 0;
    return int32(FMath::Min3((int64(Residents)+Ratio-1)/Ratio, int64(FMath::Clamp(Maximum,0,128)), int64(RoadsCount)*2));
}
FHansaAmbientPersonProfile FHansaAmbientPersonProfile::Laborer()
{
    FHansaAmbientPersonProfile P;
    const FString Base = TEXT("/Game/Hansa/Characters/Laborer01/");
    P.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(Base+TEXT("SK_Laborer01.SK_Laborer01")));
    P.Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(Base+TEXT("A_Laborer01_Walk.A_Laborer01_Walk")));
    P.Idle = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(Base+TEXT("A_Laborer01_Idle.A_Laborer01_Idle")));
    P.LookAround = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(Base+TEXT("A_Laborer01_LookAround.A_Laborer01_LookAround")));
    return P;
}
AHansaAmbientPeople::AHansaAmbientPeople()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.f/30.f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PeopleRoot")));
    Characters = {FHansaAmbientPersonProfile::Laborer()};
}
bool AHansaAmbientPeople::ValidateProfile(const FHansaAmbientPersonProfile& P, FString& Error)
{
    Error.Reset();
    if (P.CharacterId.IsNone() || P.Weight<1 || P.Weight>100 || !FMath::IsFinite(P.WalkSpeed) || P.WalkSpeed<20 || P.WalkSpeed>250 || !FMath::IsFinite(P.MeshYaw) || !FMath::IsFinite(P.GroundOffset) || FMath::Abs(P.GroundOffset)>20)
    { Error=TEXT("Invalid character ID, weight, gait speed or mesh alignment"); return false; }
    auto* Mesh=P.Mesh.LoadSynchronous(); auto* Walk=P.Walk.LoadSynchronous(); auto* Idle=P.Idle.LoadSynchronous();
    if (!Mesh || !Walk || !Idle || !Mesh->GetSkeleton()) { Error=TEXT("Mesh, walk and idle are required"); return false; }
    const auto& Ref=Mesh->GetRefSkeleton();
    if (Ref.GetNum()<1 || Ref.GetBoneName(0)!=TEXT("root") || Ref.FindBoneIndex(TEXT("pelvis"))==INDEX_NONE || Ref.FindBoneIndex(TEXT("foot_l"))==INDEX_NONE || Ref.FindBoneIndex(TEXT("foot_r"))==INDEX_NONE)
    { Error=TEXT("Human root/pelvis/feet skeleton contract missing"); return false; }
    if (Mesh->GetBounds().BoxExtent.Z<65 || Mesh->GetBounds().BoxExtent.Z>110)
    { Error=TEXT("Import an adult-sized mesh (130–220 cm); do not scale pedestrian actors"); return false; }
    TArray<UAnimSequence*> Clips{Walk,Idle};
    if (!P.LookAround.IsNull()) Clips.Add(P.LookAround.LoadSynchronous());
    for (auto* Clip : Clips)
        if (!Clip || Clip->GetSkeleton()!=Mesh->GetSkeleton() || Clip->GetPlayLength()<=0 || !Clip->bForceRootLock || Clip->GetPathName().Contains(TEXT("/Staging/")))
        { Error=TEXT("All clips need the same skeleton, positive duration, locked root and production paths"); return false; }
    if (Mesh->GetPathName().Contains(TEXT("/Staging/"))) { Error=TEXT("Staging mesh cannot enter gameplay"); return false; }
    return true;
}
#if WITH_EDITOR
EDataValidationResult AHansaAmbientPeople::IsDataValid(FDataValidationContext& Context) const
{
    bool Valid=Characters.Num()>0 && ResidentsPerPerson>0 && ResidentsPerPerson<=1000 && MaximumPeople>=0 && MaximumPeople<=128;
    TSet<FName> Ids;
    for (const auto& P:Characters)
    {
        FString Error;
        if (!ValidateProfile(P,Error) || Ids.Contains(P.CharacterId)) { Context.AddError(FText::FromString(P.CharacterId.ToString()+TEXT(": ")+Error+TEXT("; IDs must be unique"))); Valid=false; }
        Ids.Add(P.CharacterId);
    }
    if (!Valid) Context.AddError(FText::FromString(TEXT("Ambient people require valid profiles, density 1–1000 and cap 0–128.")));
    return Valid?EDataValidationResult::Valid:EDataValidationResult::Invalid;
}
#endif
bool AHansaAmbientPeople::Ground(const FVector& Candidate, FVector& Out) const
{
    FHitResult Hit;
    if (!Hansa::Game::TerrainPlacement::Trace(GetWorld(),Candidate+FVector(0,0,100000),Candidate-FVector(0,0,100000),Hit) || Hit.ImpactNormal.Z<.94) return false;
    Out=Hit.ImpactPoint; return true;
}
FVector AHansaAmbientPeople::StreetPosition(FIntPoint Cell) const
{
    if (const auto* P=Streets.MunicipalPositions.Find(Cell)) return *P;
    return Foundation->PlacementCellToWorld(Cell.X,Cell.Y);
}
bool AHansaAmbientPeople::Activate(FHansaLivePerson& P)
{
    if (Streets.Visits.IsEmpty()) return false;
    const auto& Visit=Streets.Visits[Random.RandRange(0,Streets.Visits.Num()-1)];
    FVector Position;
    P.Lane=Foundation->GetActorQuat().RotateVector(FVector(Random.FRandRange(-65.f,65.f),Random.FRandRange(-65.f,65.f),0));
    if (!Ground(StreetPosition(Visit.Road)+P.Lane,Position) || !View.Includes(Position)) return false;
    for (const auto& Other:People)
        if (&Other!=&P && Other.bActive && FVector::Dist2D(Position,Other.Mesh->GetComponentLocation())<65) return false;
    P.Cell=Visit.Road; P.Route.Reset(); P.bAtFrontage=false; P.bReturning=false; P.Visit=Visit;
    P.Mesh->SetWorldLocation(Position+FVector(0,0,RuntimeCharacters[P.Profile].GroundOffset));
    P.Mesh->SetVisibility(true); P.bActive=true; P.Activity=EHansaPersonActivity::Idle; P.Wait=Random.FRandRange(.1f,2.f); P.PoseTime=Random.FRandRange(0,P.Idle->GetPlayLength());
    Sample(P,0); return true;
}
bool AHansaAmbientPeople::ChooseVisit(FHansaLivePerson& P)
{
    for (int32 Attempt=0;Attempt<32;++Attempt)
    {
        const auto& Visit=Attempt==31?P.Visit:Streets.Visits[Random.RandRange(0,Streets.Visits.Num()-1)];
        if (Attempt!=31 && Streets.Visits.Num()>1 && Visit.Building==P.Visit.Building && Visit.Landmark==P.Visit.Landmark) continue;
        if (!Streets.Route(P.Cell,Visit.Road,P.Route)) continue;
        P.Visit=Visit; P.Waypoint=0; P.Activity=EHansaPersonActivity::Walking; P.PoseTime=0;
        const FVector Center=StreetPosition(Visit.Road);
        const FVector Inside=StreetPosition(Visit.Inside);
        // Stop 60cm outside the building footprint; do not guess a doorway through a wall.
        P.Frontage=Center+(Inside-Center).GetSafeNormal2D()*140.;
        return true;
    }
    P.Wait=Random.FRandRange(1.f,3.f); return false;
}
void AHansaAmbientPeople::RefreshCity()
{
    FAmbientStreetNetwork Next;
    int32 Residents=0, Ratio=ResidentsPerPerson;
    if (const auto* Centre=View.Centre.Get())
    {
        Next.BuildMunicipal(*Centre); Residents=Centre->AmbientCitizens; Ratio=1;
    }
    else
    {
        const auto Projection=Host->BuildProjection();
        if (!Projection) { TargetPopulation=0; for (auto& P:People) { P.bActive=false; P.Mesh->SetVisibility(false); } return; }
        Next.Build(Projection.Value.GetBuildingWorldProjections(),Host->GetCityId());
        for (const auto& City:Projection.Value.GetCityPopulations()) if (City.CityId==Host->GetCityId()) Residents=City.TotalResidents;
    }
    Streets=MoveTemp(Next);
    TargetPopulation=ValidProfiles.IsEmpty()?0:FAmbientStreetNetwork::DesiredCount(Residents,Ratio,MaximumPeople,Streets.Roads.Num(),Streets.Visits.Num());
    for (int32 I=0;I<People.Num();++I)
    {
        auto& P=People[I];
        bool Valid=Streets.Roads.Contains(P.Cell) && Streets.Visits.ContainsByPredicate([&](const auto& V) {
            return V.Building==P.Visit.Building && V.Landmark==P.Visit.Landmark && V.Road==P.Visit.Road && V.Inside==P.Visit.Inside;
        });
        for (int32 Step=P.Waypoint;Step<P.Route.Num() && Valid;++Step) Valid=Streets.Roads.Contains(P.Route[Step]);
        if (!Valid || I>=TargetPopulation) { P.bActive=false; P.Mesh->SetVisibility(false); }
    }
    // Grow a reusable component pool; a bounded population prevents an unbounded skeletal cost.
    const int32 GrowthLimit=FMath::Min(TargetPopulation,People.Num()+8);
    while (People.Num()<GrowthLimit)
    {
        int32 TotalWeight=0; for (int32 Index:ValidProfiles) TotalWeight+=RuntimeCharacters[Index].Weight;
        int32 Pick=Random.RandRange(1,TotalWeight), Index=ValidProfiles[0];
        for (int32 Candidate:ValidProfiles) { Pick-=RuntimeCharacters[Candidate].Weight; if (Pick<=0) { Index=Candidate; break; } }
        auto& P=People.AddDefaulted_GetRef(); P.Profile=Index; const auto& Profile=RuntimeCharacters[Index];
        P.Walk=Profile.Walk.LoadSynchronous(); P.Idle=Profile.Idle.LoadSynchronous(); P.LookAround=Profile.LookAround.LoadSynchronous();
        P.Mesh=NewObject<USkeletalMeshComponent>(this); P.Mesh->SetupAttachment(GetRootComponent()); P.Mesh->SetSkeletalMeshAsset(Profile.Mesh.LoadSynchronous());
        P.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); P.Mesh->SetCanEverAffectNavigation(false);
        P.Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode); P.Mesh->SetAnimation(P.Idle);
        P.Mesh->SetVisibility(false); P.Mesh->RegisterComponent(); P.Mesh->SetComponentTickEnabled(false);
    }
    for (int32 I=0;I<FMath::Min(TargetPopulation,People.Num());++I) if (!People[I].bActive) Activate(People[I]);
}
void AHansaAmbientPeople::Sample(FHansaLivePerson& P,float Delta)
{
    UAnimSequence* Clip=P.Activity==EHansaPersonActivity::Walking?P.Walk.Get():
        (P.Activity==EHansaPersonActivity::Visiting && P.LookAround?P.LookAround.Get():P.Idle.Get());
    P.PoseTime=FMath::Fmod(P.PoseTime+Delta,Clip->GetPlayLength());
    P.Mesh->SetAnimation(Clip); P.Mesh->SetPosition(P.PoseTime,false);
    P.Mesh->TickAnimation(0,false); P.Mesh->RefreshBoneTransforms(); P.Mesh->UpdateComponentToWorld();
}
void AHansaAmbientPeople::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()->IsGameWorld() || GetNetMode()!=NM_Standalone) return;
    if (!Host) if (auto* Mode=GetWorld()->GetAuthGameMode<AHansaGameMode>()) Host=Mode->GetSimulationHost();
    if (!Host || !Host->IsReady()) return;
    if (!Foundation) for (TActorIterator<AHansaLubeckWorldFoundation> It(GetWorld());It;++It) { Foundation=*It; break; }
    if (!Foundation) return;
    if (View.Refresh(GetWorld(),*Foundation,FName(*Host->GetCityId().ToString()),DeltaSeconds))
    {
        RefreshIn=0;
        for (auto& P:People) { P.bActive=false; P.Mesh->SetVisibility(false); }
    }
    SetActorTickInterval(View.bEnabled?1.f/30.f:.25f);
    if (!View.bEnabled)
    {
        for (auto& P:People) P.Mesh->SetVisibility(false);
        return; // No asset loading, projections, routes, ground traces or skeletal evaluation.
    }
    if (!bInitialized)
    {
        bInitialized=true; RuntimeCharacters=Characters; Random.Initialize(int32(Host->GetCampaignSeed()^0x50454f50)); TSet<FName> Ids;
        for (int32 I=0;I<RuntimeCharacters.Num();++I)
        {
            FString Error;
            if (ValidateProfile(RuntimeCharacters[I],Error) && !Ids.Contains(RuntimeCharacters[I].CharacterId)) ValidProfiles.Add(I);
            else ValidationError+=RuntimeCharacters[I].CharacterId.ToString()+TEXT(": ")+Error+TEXT("; ");
            Ids.Add(RuntimeCharacters[I].CharacterId);
        }
        if (!ValidationError.IsEmpty()) UE_LOG(LogTemp,Warning,TEXT("Ambient people: %s"),*ValidationError);
    }
    RefreshIn-=DeltaSeconds;
    if (RefreshIn<=0) { RefreshCity(); RefreshIn=1.f; }
    for (auto& P:People) P.Mesh->SetVisibility(P.bActive && View.Includes(P.Mesh->GetComponentLocation()));
    if (Host->GetSpeed()==EHansaRuntimeSimulationSpeed::Paused) return;
    const float Step=FMath::Clamp(DeltaSeconds,0.f,.1f);
    for (auto& P:People)
    {
        if (!P.bActive) continue;
        const bool bVisible=View.Includes(P.Mesh->GetComponentLocation());
        P.Mesh->SetVisibility(bVisible);
        if (!bVisible) continue;
        if (P.Activity!=EHansaPersonActivity::Walking)
        {
            P.Wait-=Step;
            if (P.Wait<=0)
            {
                if (P.bAtFrontage) { P.bReturning=true; P.Activity=EHansaPersonActivity::Walking; P.PoseTime=0; }
                else ChooseVisit(P);
            }
        }
        if (P.Activity==EHansaPersonActivity::Walking)
        {
            FVector Target=P.Frontage;
            if (P.bReturning) Target=StreetPosition(P.Cell)+P.Lane;
            else if (P.Route.IsValidIndex(P.Waypoint)) Target=StreetPosition(P.Route[P.Waypoint])+P.Lane;
            const FVector Current=P.Mesh->GetComponentLocation();
            const FVector Direction=(Target-Current).GetSafeNormal2D(); const float Distance=FVector::Dist2D(Current,Target);
            const float Travel=FMath::Min(Distance,RuntimeCharacters[P.Profile].WalkSpeed*Step);
            FVector Position;
            if (!Ground(Current+Direction*Travel,Position) || FMath::Abs(Position.Z+RuntimeCharacters[P.Profile].GroundOffset-Current.Z)>35)
            { P.bActive=false; P.Mesh->SetVisibility(false); continue; }
            P.Mesh->SetWorldLocation(Position+FVector(0,0,RuntimeCharacters[P.Profile].GroundOffset));
            if (!Direction.IsNearlyZero()) P.Mesh->SetWorldRotation(FMath::RInterpTo(P.Mesh->GetComponentRotation(),FRotator(0,Direction.Rotation().Yaw+RuntimeCharacters[P.Profile].MeshYaw,0),Step,8));
            if (Distance<=Travel+1)
            {
                if (P.bReturning) { P.bReturning=false; P.bAtFrontage=false; P.Activity=EHansaPersonActivity::Idle; P.Wait=.2f; }
                else if (P.Route.IsValidIndex(P.Waypoint)) P.Cell=P.Route[P.Waypoint++];
                else { P.bAtFrontage=true; P.Activity=EHansaPersonActivity::Visiting; P.Wait=Random.FRandRange(4.f,12.f); P.PoseTime=0; ++CompletedVisits; }
            }
        }
        Sample(P,Step);
    }
}
TArray<FHansaPersonObservation> AHansaAmbientPeople::QueryPeople() const
{
    TArray<FHansaPersonObservation> Result;
    for (int32 I=0;I<People.Num();++I)
    {
        const auto& P=People[I]; FHansaPersonObservation O;
        O.PersonId=I+1; O.CharacterId=RuntimeCharacters[P.Profile].CharacterId; O.Activity=P.Activity;
        O.Location=P.Mesh->GetComponentLocation(); O.DestinationBuilding=int64(P.Visit.Building.GetValue()); O.bVisible=P.bActive && P.Mesh->IsVisible();
        O.CityId=View.City; O.DestinationLandmark=P.Visit.Landmark;
        Result.Add(O);
    }
    return Result;
}
