#include "World/HansaAmbientAnimals.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
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
    FVector RootAt(const UAnimSequence* Clip, double Time)
    { return Clip->ExtractRootTrackTransform(FAnimExtractContext(Time), nullptr).GetTranslation(); }
    FVector ReferencePosition(const USkeletalMesh* Mesh, FName Bone)
    {
        const auto& Ref = Mesh->GetRefSkeleton();
        int32 Index = Ref.FindBoneIndex(Bone);
        FTransform Pose = FTransform::Identity;
        while (Index != INDEX_NONE) { Pose = Pose * Ref.GetRefBonePose()[Index]; Index = Ref.GetParentIndex(Index); }
        return Pose.GetTranslation();
    }
}
FHansaAmbientAnimalProfile FHansaAmbientAnimalProfile::Rabbit()
{
    FHansaAmbientAnimalProfile P;
    P.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Rabbit/SK_Rabbit.SK_Rabbit")));
    P.Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Rabbit/A_Rabbit_Walk.A_Rabbit_Walk")));
    P.Jump = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Rabbit/A_Rabbit_Jump.A_Rabbit_Jump")));
    return P;
}
FHansaAmbientAnimalProfile FHansaAmbientAnimalProfile::Dog()
{
    FHansaAmbientAnimalProfile P;
    P.SpeciesId = TEXT("Dog"); P.MinimumPopulation = 2; P.MaximumPopulation = 3; P.TownRadius = 3000;
    P.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Dog/SK_Dog.SK_Dog")));
    P.Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Dog/A_Dog_Walk.A_Dog_Walk")));
    P.ExpectedBones = 32; P.WalkCycleSeconds = 1.2f; P.WalkCycleDistance = 32;
    P.MinimumWalkCycles = 8; P.MaximumWalkCycles = 18; P.MinimumRestSeconds = 1; P.MaximumRestSeconds = 3;
    P.WalksBetweenJumps = 0; P.bRestInReferencePose = true; P.BodyRadius = 60; P.BodyHeight = 75; P.ForcedLOD = 1;
    return P;
}
AHansaAmbientAnimals::AHansaAmbientAnimals()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.f/30.f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AmbientRoot")));
    Species = {FHansaAmbientAnimalProfile::Rabbit(), FHansaAmbientAnimalProfile::Dog()};
}
bool AHansaAmbientAnimals::ValidateProfile(const FHansaAmbientAnimalProfile& P, USkeletalMesh* Mesh, UAnimSequence* W, UAnimSequence* J, FString& Error)
{
    Error.Reset();
    for (float Value : {P.TownRadius, P.WalkCycleSeconds, P.WalkCycleDistance, P.MinimumRestSeconds, P.MaximumRestSeconds, P.BodyRadius, P.BodyHeight})
        if (!FMath::IsFinite(Value)) { Error = TEXT("Non-finite animal setting"); return false; }
    if (P.SpeciesId.IsNone() || P.MinimumPopulation < 0 || P.MaximumPopulation < P.MinimumPopulation || P.MaximumPopulation > 32 ||
        P.TownRadius < 1000 || P.TownRadius > 16000 || P.WalkCycleSeconds < .1f || P.WalkCycleDistance < 1 ||
        P.MinimumWalkCycles < 1 || P.MaximumWalkCycles < P.MinimumWalkCycles || P.MaximumWalkCycles > 100 ||
        P.MinimumRestSeconds < 0 || P.MaximumRestSeconds < P.MinimumRestSeconds || P.WalksBetweenJumps < 0 || P.BodyRadius < 10 || P.BodyHeight < 10 || P.ForcedLOD < 0)
    { Error = TEXT("Invalid population, movement or clearance ranges"); return false; }
    if (!Mesh || !W || (P.WalksBetweenJumps > 0 && !J)) { Error = TEXT("Missing promoted animal mesh or required clips"); return false; }
    for (const UObject* Asset : {static_cast<const UObject*>(Mesh), static_cast<const UObject*>(W), P.WalksBetweenJumps > 0 ? static_cast<const UObject*>(J) : nullptr})
        if (Asset && !Asset->GetPathName().StartsWith(TEXT("/Game/Hansa/Animals/"))) { Error = TEXT("Only promoted animal assets may be referenced"); return false; }
    const auto& Ref = Mesh->GetRefSkeleton();
    if (!Mesh->GetSkeleton() || Mesh->GetSkeleton() != W->GetSkeleton() || Ref.GetNum() != P.ExpectedBones || Ref.GetBoneName(0) != TEXT("root") ||
        Ref.FindBoneIndex(TEXT("pelvis")) == INDEX_NONE || Ref.FindBoneIndex(TEXT("head")) == INDEX_NONE ||
        !Ref.GetRefBonePose()[0].GetScale3D().Equals(FVector::OneVector,.001) || P.ForcedLOD > Mesh->GetLODNum())
    { Error = TEXT("Animal skeleton, root scale or runtime LOD mismatch"); return false; }
    const int32 Keys = FMath::RoundToInt(P.WalkCycleSeconds*30)+1;
    if (W->GetSamplingFrameRate() != FFrameRate(30,1) || W->GetNumberOfSampledKeys() != Keys || !FMath::IsNearlyEqual(W->GetPlayLength(),P.WalkCycleSeconds,.001f))
    { Error = TEXT("Authored walk duration/sample rate mismatch"); return false; }
    for (int32 Frame = 0; Frame < Keys; ++Frame)
    {
        const FTransform Root = W->ExtractRootTrackTransform(FAnimExtractContext(Frame/30.), nullptr);
        if (!Root.GetTranslation().Equals(RootAt(W,0),.01) || !Root.GetScale3D().Equals(FVector::OneVector,.001))
        { Error = TEXT("Walk must be in place with unit root scale"); return false; }
    }
    if (P.WalksBetweenJumps > 0)
    {
        if (J->GetSkeleton() != Mesh->GetSkeleton() || !J->bForceRootLock || J->GetSamplingFrameRate() != FFrameRate(30,1) || J->GetPlayLength() <= 0)
        { Error = TEXT("Jump requires the same skeleton, 30fps and a locked visual root"); return false; }
        const FVector Travel = RootAt(J,J->GetPlayLength())-RootAt(J,0);
        if (Travel.Size2D() < 1 || FMath::Abs(Travel.Z) > .5) { Error = TEXT("Jump must travel horizontally and recover its ground height"); return false; }
        for (int32 Frame = 0; Frame < J->GetNumberOfSampledKeys(); ++Frame)
            if (!J->ExtractRootTrackTransform(FAnimExtractContext(Frame/30.),nullptr).GetScale3D().Equals(FVector::OneVector,.001))
            { Error = TEXT("Jump root scale mismatch"); return false; }
    }
    return true;
}
int32 AHansaAmbientAnimals::GetLiveAnimalCount(FName Id) const
{
    int32 Count = 0;
    for (const auto& A : Animals) if (A.bPlaced && (Id.IsNone() || Species[A.ProfileIndex].SpeciesId == Id)) ++Count;
    return Count;
}
TArray<FHansaAnimalObservation> AHansaAmbientAnimals::QueryAnimals() const
{
    TArray<FHansaAnimalObservation> Result;
    for (const auto& A : Animals)
    {
        FHansaAnimalObservation O; O.SpeciesId = Species[A.ProfileIndex].SpeciesId;
        const FString City = View.City.ToString().Replace(TEXT("City."),TEXT(""));
        O.StableId = FString::Printf(TEXT("Animal.%s.Ambient.%s.%02d"),*O.SpeciesId.ToString(),*City,A.SpeciesOrdinal);
        O.Activity = A.bTurning ? EHansaAnimalActivity::Still : A.Activity; O.Location = A.Mesh->GetComponentLocation();
        O.Destination = A.Destination; O.AnimationTime = A.PoseTime; O.bVisible = A.bPlaced && A.Mesh->IsVisible(); O.bTurning = A.bTurning; Result.Add(O);
    }
    return Result;
}
#if WITH_EDITOR
EDataValidationResult AHansaAmbientAnimals::IsDataValid(FDataValidationContext& Context) const
{
    bool bValid = Species.Num() > 0; TSet<FName> Ids;
    for (const auto& P : Species)
    {
        FString Error;
        const bool bProfileValid = ValidateProfile(P,P.Mesh.LoadSynchronous(),P.Walk.LoadSynchronous(),P.Jump.LoadSynchronous(),Error);
        if (!bProfileValid || Ids.Contains(P.SpeciesId)) { Context.AddError(FText::FromString(P.SpeciesId.ToString()+TEXT(": ")+Error+TEXT(" (species IDs must be unique)"))); bValid = false; }
        Ids.Add(P.SpeciesId);
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
void AHansaAmbientAnimals::InitializeSpecies()
{
    bInitialized = true;
    Random.Initialize(int32(Host->GetCampaignSeed() ^ 0x52414242)); Loaded.SetNum(Species.Num());
    TSet<FName> Ids;
    for (int32 Index = 0; Index < Species.Num(); ++Index)
    {
        const auto& P = Species[Index]; auto& A = Loaded[Index];
        A.Mesh = P.Mesh.LoadSynchronous(); A.Walk = P.Walk.LoadSynchronous(); A.Jump = P.Jump.LoadSynchronous();
        if (Ids.Contains(P.SpeciesId)) A.Error = TEXT("Duplicate species ID");
        else ValidateProfile(P,A.Mesh,A.Walk,A.Jump,A.Error);
        Ids.Add(P.SpeciesId);
        if (!A.Error.IsEmpty()) { ValidationError += P.SpeciesId.ToString()+TEXT(": ")+A.Error+TEXT("; "); UE_LOG(LogTemp, Warning, TEXT("Ambient %s disabled: %s"),*P.SpeciesId.ToString(),*A.Error); continue; }
        FVector Forward = ReferencePosition(A.Mesh,TEXT("head"))-ReferencePosition(A.Mesh,TEXT("pelvis"));
        if (P.WalksBetweenJumps > 0)
        {
            A.JumpTravel = RootAt(A.Jump,A.Jump->GetPlayLength())-RootAt(A.Jump,0); Forward = A.JumpTravel;
            for (int32 F = 0; F < A.Jump->GetNumberOfSampledKeys(); ++F) A.JumpPeak = FMath::Max(A.JumpPeak,float(RootAt(A.Jump,F/30.).Z-RootAt(A.Jump,0).Z));
        }
        A.MeshFacing = FQuat::FindBetweenNormals(Forward.GetSafeNormal2D(),FVector::ForwardVector);
        const int32 Count = Random.RandRange(P.MinimumPopulation,P.MaximumPopulation);
        for (int32 I = 0; I < Count; ++I)
        {
            auto& Animal = Animals.AddDefaulted_GetRef(); Animal.ProfileIndex = Index; Animal.SpeciesOrdinal = I+1;
            Animal.Mesh = NewObject<USkeletalMeshComponent>(this,*FString::Printf(TEXT("%s_%02d"),*P.SpeciesId.ToString(),I+1));
            Animal.Mesh->SetupAttachment(GetRootComponent()); Animal.Mesh->SetSkeletalMeshAsset(A.Mesh);
            Animal.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Animal.Mesh->SetCanEverAffectNavigation(false);
            Animal.Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode); Animal.Mesh->SetAnimation(A.Walk);
            Animal.Mesh->SetForcedLOD(P.ForcedLOD); Animal.Mesh->SetVisibility(false); Animal.Mesh->RegisterComponent(); Animal.Mesh->SetComponentTickEnabled(false);
        }
    }
}

void AHansaAmbientAnimals::RefreshObstacles()
{
    Occupied.Reset(); MunicipalObstacles.Reset();
    if (const auto* Centre=View.Centre.Get())
    {
        FString Error;
        if (!Centre->ValidateLayout(Error)) { bProjectionReady=false; return; }
        for (const auto& Slot:Centre->Slots)
            if (Slot.bSelectable && Slot.Mesh && Centre->IsSlotActive(Slot))
                MunicipalObstacles.Add(Slot.Mesh->GetBoundingBox().TransformBy(FTransform(FRotator(0,Slot.Yaw,0),Slot.Location)*Centre->GetActorTransform()));
    }
    else
    {
        const auto Projection = Host->BuildProjection();
        if (!Projection) { bProjectionReady = false; return; }
        for (const auto& Building : Projection.Value.GetPlacements())
            if (Building.Spec.CityId == Host->GetCityId())
                for (const auto Cell : Building.OccupiedCells) Occupied.Add(FIntPoint(Cell.X, Cell.Y));
    }
    bProjectionReady = true;
}

bool AHansaAmbientAnimals::SafeGround(const FVector& Candidate, FVector& Ground, const FHansaAmbientAnimalProfile& Profile) const
{
    using namespace Hansa::Simulation;
    if (!Foundation || !Host || !bProjectionReady || FVector::Dist2D(Candidate, TownCenter) > Profile.TownRadius) return false;
    const auto* Centre=View.Centre.Get();
    const auto* Map = Centre ? nullptr : Host->FindPlacementMap();
    if (!Centre && !Map) return false;
    if (MunicipalObstacles.ContainsByPredicate([&](const FBox& B) { return B.ExpandBy(Profile.BodyRadius).IsInsideXY(Candidate); })) return false;
    // Canonical map cells are sorted X then Y. Binary lookup avoids duplicating million-cell maps.
    for (const FVector2D Offset : {FVector2D(0,0), FVector2D(-Profile.BodyRadius,-Profile.BodyRadius), FVector2D(-Profile.BodyRadius,Profile.BodyRadius),
        FVector2D(Profile.BodyRadius,-Profile.BodyRadius), FVector2D(Profile.BodyRadius,Profile.BodyRadius)})
    {
        if (Centre)
        {
            const FVector Point=Candidate+FVector(Offset.X,Offset.Y,0);
            FHitResult Foot;
            if (!Hansa::Game::TerrainPlacement::Trace(GetWorld(),Point+FVector(0,0,100000),Point-FVector(0,0,100000),Foot) ||
                Foot.ImpactNormal.Z<.94 || Foot.ImpactPoint.Z<=Centre->AmbientWaterLevel+5) return false;
            continue;
        }
        int32 X, Y;
        if (!Foundation->WorldToPlacementCell(Candidate + FVector(Offset.X, Offset.Y, 0), X, Y) || Occupied.Contains(FIntPoint(X,Y))) return false;
        int32 Low = 0, High = Map->Cells.Num();
        while (Low < High)
        {
            const int32 Mid = (Low + High) / 2;
            const auto C = Map->Cells[Mid].Coordinate;
            if (C.X < X || (C.X == X && C.Y < Y)) Low = Mid + 1; else High = Mid;
        }
        if (!Map->Cells.IsValidIndex(Low)) return false;
        const auto& Cell = Map->Cells[Low];
        if (Cell.Coordinate.X != X || Cell.Coordinate.Y != Y || Cell.Terrain != EHansaPlacementTerrain::Land || Cell.bBlocked) return false;
    }
    FHitResult Hit;
    if (!Hansa::Game::TerrainPlacement::Trace(GetWorld(), Candidate + FVector(0,0,100000), Candidate - FVector(0,0,100000), Hit) || Hit.ImpactNormal.Z < .94) return false;
    Ground = Hit.ImpactPoint;
    if (Centre && Ground.Z <= Centre->AmbientWaterLevel+5) return false;
    // Ground-only trace cannot select roofs. A separate body query rejects authored obstacles.
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    GetWorld()->OverlapMultiByObjectType(Hits, Ground + FVector(0,0,8+Profile.BodyHeight*.5f), FQuat::Identity, Objects,
        FCollisionShape::MakeBox(FVector(Profile.BodyRadius,Profile.BodyRadius,Profile.BodyHeight*.5f)), FCollisionQueryParams(SCENE_QUERY_STAT(AnimalBody), false, this));
    if (Candidate.Z > Ground.Z + 1.)
    {
        TArray<FOverlapResult> AirHits;
        GetWorld()->OverlapMultiByObjectType(AirHits, Ground + FVector(0,0,8+Profile.BodyHeight*.5f+FMath::Min(Candidate.Z-Ground.Z,100.)), FQuat::Identity, Objects,
            FCollisionShape::MakeBox(FVector(Profile.BodyRadius,Profile.BodyRadius,Profile.BodyHeight*.5f)), FCollisionQueryParams(SCENE_QUERY_STAT(AnimalAirBody), false, this));
        Hits.Append(AirHits);
    }
    for (const auto& Overlap : Hits)
    {
        const auto* Actor = Overlap.GetActor();
        const auto* Component = Overlap.GetComponent();
        if (!Actor || !Component || Actor->IsHidden() || Actor->IsA<ALandscapeProxy>() ||
            Actor->ActorHasTag(TEXT("Hansa.Terrain")) || Component->ComponentHasTag(TEXT("Hansa.Terrain"))) continue;
        return false;
    }
    return true;
}

bool AHansaAmbientAnimals::SafePath(const FVector& From, const FVector& To, const FHansaAmbientAnimalProfile& Profile) const
{
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(From, To) / 20.));
    FVector Previous = From;
    for (int32 I = 0; I <= Steps; ++I)
    {
        FVector Ground;
        if (!SafeGround(FMath::Lerp(From, To, double(I)/Steps), Ground, Profile) || FMath::Abs(Ground.Z - Previous.Z) > 6.) return false;
        Previous = Ground;
    }
    return true;
}

bool AHansaAmbientAnimals::Place(FHansaAmbientAnimal& Animal)
{
    const auto& Profile = Species[Animal.ProfileIndex];
    for (int32 Attempt = 0; Attempt < 48; ++Attempt)
    {
        const float Angle = Random.FRandRange(0, 2*PI);
        const float Radius = FMath::Sqrt(Random.FRand()) * Profile.TownRadius;
        FVector Ground;
        const FVector Candidate=TownCenter + FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        if (!View.Includes(Candidate) || !SafeGround(Candidate, Ground, Profile)) continue;
        bool bClear = true;
        for (const auto& Other : Animals)
            if (&Other != &Animal && Other.bPlaced && FVector::Dist2D(Other.Mesh->GetComponentLocation(), Ground) < Profile.BodyRadius+Species[Other.ProfileIndex].BodyRadius) bClear = false;
        if (!bClear) continue;
        Animal.Start = Ground;
        Animal.Heading = FRotator(0, Random.FRandRange(-180,180), 0).Quaternion();
        Animal.Mesh->SetWorldLocation(Ground);
        Animal.bPlaced = true;
        Animal.Mesh->SetVisibility(true);
        Stand(Animal);
        Sample(Animal);
        return true;
    }
    return false; // Retry after streaming/placement changes. Never place an animal in an unsafe fallback.
}

void AHansaAmbientAnimals::Stand(FHansaAmbientAnimal& Animal)
{
    const auto& Profile = Species[Animal.ProfileIndex];
    Animal.bTurning = false;
    Animal.Activity = EHansaAnimalActivity::Still;
    Animal.Elapsed = 0;
    Animal.Duration = Random.FRandRange(Profile.MinimumRestSeconds, Profile.MaximumRestSeconds);
    // Walk destinations end on complete gait cycles, so resting uses the grounded neutral pose.
    Animal.PoseTime = 0;
    Animal.Mesh->SetAnimation(Profile.bRestInReferencePose ? nullptr : Loaded[Animal.ProfileIndex].Walk.Get());
}

void AHansaAmbientAnimals::BeginTurn(FHansaAmbientAnimal& Animal, const FQuat& Target)
{
    Animal.TurnStart = Animal.Heading;
    Animal.TargetHeading = Target;
    Animal.TurnElapsed = 0;
    // Smoothstep has a peak slope of 1.5: keep peak yaw speed at 120 degrees/sec.
    const float Degrees = FMath::RadiansToDegrees(Animal.Heading.AngularDistance(Target));
    Animal.TurnDuration = FMath::Max(.25f, 1.5f * Degrees / 120.f);
    Animal.bTurning = true;
}

void AHansaAmbientAnimals::AdvanceTurn(FHansaAmbientAnimal& Animal, float Step)
{
    Animal.TurnElapsed = FMath::Min(Animal.TurnElapsed + FMath::Max(0.f, Step), Animal.TurnDuration);
    const float Alpha = Animal.TurnElapsed / Animal.TurnDuration;
    const float Eased = Alpha * Alpha * (3.f - 2.f * Alpha);
    Animal.Heading = FQuat::Slerp(Animal.TurnStart, Animal.TargetHeading, Eased).GetNormalized();
    if (Alpha >= 1.f)
    {
        Animal.Heading = Animal.TargetHeading;
        Animal.bTurning = false;
    }
}

void AHansaAmbientAnimals::Sample(FHansaAmbientAnimal& Animal)
{
    const auto& Profile = Species[Animal.ProfileIndex];
    if (Profile.bRestInReferencePose && (Animal.bTurning || Animal.Activity == EHansaAnimalActivity::Still)) Animal.Mesh->SetAnimation(nullptr);
    const FQuat GroundHeading = Animal.Activity == EHansaAnimalActivity::Jump && !Animal.bTurning ? Animal.Heading :
        Hansa::Game::TerrainPlacement::RoadRotation(GetWorld(), Animal.Mesh->GetComponentLocation(), Animal.Heading);
    Animal.Mesh->SetWorldRotation(GroundHeading * Loaded[Animal.ProfileIndex].MeshFacing);
    Animal.Mesh->SetPosition(Animal.PoseTime, false);
    Animal.Mesh->TickAnimation(0, false);
    Animal.Mesh->RefreshBoneTransforms();
    Animal.Mesh->UpdateBounds();
}


void AHansaAmbientAnimals::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()->IsGameWorld() || GetNetMode() != NM_Standalone) return;
    if (!Host)
    {
        const auto* Mode = GetWorld()->GetAuthGameMode<AHansaGameMode>();
        if (!Mode) return;
        Host = const_cast<AHansaGameMode*>(Mode)->GetSimulationHost();
    }
    if (!Host || !Host->IsReady()) return;
    if (!Foundation)
        for (TActorIterator<AHansaLubeckWorldFoundation> It(GetWorld()); It; ++It) { Foundation = *It; break; }
    if (!Foundation) return;
    if (View.Refresh(GetWorld(),*Foundation,FName(*Host->GetCityId().ToString()),DeltaSeconds))
    {
        RefreshIn=0;
        for (auto& Animal:Animals) { Animal.bPlaced=false; Animal.Mesh->SetVisibility(false); }
    }
    SetActorTickInterval(View.bEnabled?1.f/30.f:.25f);
    if (!View.bEnabled)
    {
        for (auto& Animal:Animals) Animal.Mesh->SetVisibility(false);
        return;
    }
    TownCenter=View.Centre.IsValid()?View.Centre->GetMarketLocation():View.HomeCenter;
    if (!bInitialized) InitializeSpecies();
    if (Animals.IsEmpty()) return;
    RefreshIn -= DeltaSeconds;
    const bool bRefresh = RefreshIn <= 0;
    if (bRefresh) { RefreshObstacles(); RefreshIn = 1.f; }
    for (auto& Animal:Animals) Animal.Mesh->SetVisibility(Animal.bPlaced && View.Includes(Animal.Mesh->GetComponentLocation()));
    if (Host->GetSpeed() == EHansaRuntimeSimulationSpeed::Paused) return;
    const float Step = FMath::Min(DeltaSeconds, .1f); // Cosmetic real-time speed; no fast-forward skating.
    for (auto& Animal : Animals)
    {
        const auto& Profile = Species[Animal.ProfileIndex];
        const auto& Assets = Loaded[Animal.ProfileIndex];
        auto* WalkClip = Assets.Walk.Get();
        auto* JumpClip = Assets.Jump.Get();
        const float WalkSpeed = Profile.WalkCycleDistance/Profile.WalkCycleSeconds;
        const FVector JumpTravel = Assets.JumpTravel;
        const FQuat MeshFacing = Assets.MeshFacing;
        if (!Animal.bPlaced) { if (bRefresh) Place(Animal); continue; }
        const bool bVisible=View.Includes(Animal.Mesh->GetComponentLocation());
        Animal.Mesh->SetVisibility(bVisible);
        if (!bVisible) continue;
        FVector Ground;
        const FVector Location = Animal.Mesh->GetComponentLocation();
        if (!SafeGround(Location, Ground, Profile))
        { Animal.bPlaced = false; Animal.Mesh->SetVisibility(false); continue; }
        if (Animal.bTurning)
        {
            AdvanceTurn(Animal, Step);
            // Hold the neutral pose and position until aligned; never steer airborne root motion.
            if (!Animal.bTurning)
                Animal.Mesh->SetAnimation(Animal.Activity == EHansaAnimalActivity::Jump ? JumpClip : WalkClip);
            Sample(Animal);
            continue;
        }
        Animal.Elapsed += Step;
        if (Animal.Activity == EHansaAnimalActivity::Still)
        {
            if (Animal.Elapsed < Animal.Duration) continue;
            bool bMoving = false;
            for (int32 Attempt = 0; Attempt < 12; ++Attempt)
            {
                const bool bJump = Profile.WalksBetweenJumps > 0 && Animal.WalksSinceJump >= Profile.WalksBetweenJumps;
                const float Angle = Random.FRandRange(-PI, PI);
                const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
                const float Distance = bJump ? JumpTravel.Size2D() : Random.RandRange(Profile.MinimumWalkCycles,Profile.MaximumWalkCycles)*Profile.WalkCycleDistance;
                FVector Target;
                if (!SafeGround(Location + Direction*Distance, Target, Profile) || !SafePath(Ground, Target, Profile)) continue;
                // Jumps must land on the authored root-height plane; reject slopes/steps.
                if (bJump && FMath::Abs(Target.Z - Ground.Z) > .5) continue;
                FVector AirGround;
                if (bJump && (!SafeGround(Ground+FVector(0,0,Assets.JumpPeak),AirGround,Profile) || !SafeGround(Target+FVector(0,0,Assets.JumpPeak),AirGround,Profile))) continue;
                Animal.Start = Ground; Animal.Destination = Target;
                BeginTurn(Animal, Direction.Rotation().Quaternion());
                Animal.Activity = bJump ? EHansaAnimalActivity::Jump : EHansaAnimalActivity::Walk;
                Animal.Duration = bJump ? JumpClip->GetPlayLength() : Distance/WalkSpeed;
                Animal.Elapsed = 0;
                Animal.PoseTime = 0;
                bMoving = true;
                break;
            }
            if (!bMoving) Stand(Animal);
        }
        else
        {
            FVector Next;
            if (Animal.Activity == EHansaAnimalActivity::Jump)
            {
                Animal.PoseTime = FMath::Min(Animal.Elapsed, Animal.Duration);
                Next = Animal.Start + (Animal.Heading*MeshFacing).RotateVector(RootAt(JumpClip, Animal.PoseTime)-RootAt(JumpClip,0));
            }
            else
            {
                Animal.PoseTime = FMath::Fmod(Animal.PoseTime + Step, WalkClip->GetPlayLength());
                Next = FMath::Lerp(Animal.Start,Animal.Destination,FMath::Min(Animal.Elapsed/Animal.Duration,1.f));
            }
            FVector NextGround;
            if (!SafeGround(Next, NextGround, Profile) || !SafePath(Ground, NextGround, Profile))
            { Animal.PoseTime = 0; Animal.Mesh->SetWorldLocation(Ground); Stand(Animal); }
            else
            {
                Animal.Mesh->SetWorldLocation(Animal.Activity == EHansaAnimalActivity::Jump ? Next : NextGround);
                if (Animal.Elapsed >= Animal.Duration)
                {
                    if (Animal.Activity == EHansaAnimalActivity::Jump) { ++CompletedJumps; Animal.WalksSinceJump = 0; Animal.PoseTime = 0; }
                    else ++Animal.WalksSinceJump;
                    Stand(Animal);
                }
            }
        }
        Sample(Animal);
    }
}
