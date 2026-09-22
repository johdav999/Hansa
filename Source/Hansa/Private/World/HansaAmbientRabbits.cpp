#include "World/HansaAmbientRabbits.h"
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
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
    constexpr float BodyRadius = 65.f; // 116 cm long approved rabbit; includes a safety margin.
    constexpr float WalkSpeed = 16.f; // Authored one-second in-place gait, centimetres/second.
    FVector RootAt(const UAnimSequence* Clip, double Time)
    {
        return Clip->ExtractRootTrackTransform(FAnimExtractContext(Time), nullptr).GetTranslation();
    }
}

AHansaAmbientRabbits::AHansaAmbientRabbits()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AmbientRoot")));
    RabbitMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Rabbit/SK_Rabbit.SK_Rabbit")));
    Walk = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Rabbit/A_Rabbit_Walk.A_Rabbit_Walk")));
    Jump = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Hansa/Animals/Rabbit/A_Rabbit_Jump.A_Rabbit_Jump")));
}

bool AHansaAmbientRabbits::ValidateClips(USkeletalMesh* Mesh, UAnimSequence* W, UAnimSequence* J, FString& Error)
{
    if (!Mesh || !W || !J) { Error = TEXT("Missing promoted rabbit mesh or clips"); return false; }
    if (Mesh->GetSkeleton() != W->GetSkeleton() || W->GetSkeleton() != J->GetSkeleton() ||
        Mesh->GetRefSkeleton().GetNum() != 25 || Mesh->GetRefSkeleton().GetBoneName(0) != TEXT("root"))
    { Error = TEXT("Rabbit canonical skeleton mismatch"); return false; }
    const FVector Travel = RootAt(J, J->GetPlayLength()) - RootAt(J, 0);
    if (W->GetSamplingFrameRate() != FFrameRate(30,1) || J->GetSamplingFrameRate() != FFrameRate(30,1) ||
        W->GetNumberOfSampledKeys() != 31 || J->GetNumberOfSampledKeys() != 31 ||
        !Mesh->GetRefSkeleton().GetRefBonePose()[0].GetScale3D().Equals(FVector::OneVector,.001))
    { Error = TEXT("Rabbit frame rate, range or root bind scale mismatch"); return false; }
    double JumpPeak = 0;
    for (int32 Frame = 0; Frame <= 30; ++Frame)
    {
        const double Time = Frame/30.;
        const FTransform Root = J->ExtractRootTrackTransform(FAnimExtractContext(Time), nullptr);
        if (!Root.GetScale3D().Equals(FVector::OneVector,.001) || !RootAt(W,Time).Equals(RootAt(W,0),.01))
        { Error = TEXT("Rabbit root scale or in-place track mismatch"); return false; }
        JumpPeak = FMath::Max(JumpPeak,Root.GetTranslation().Z-RootAt(J,0).Z);
    }
    if (!FMath::IsNearlyEqual(JumpPeak,23.,1.)) { Error = TEXT("Rabbit jump height mismatch"); return false; }
    if (!FMath::IsNearlyEqual(W->GetPlayLength(), 1.f, .001f) || !FMath::IsNearlyEqual(J->GetPlayLength(), 1.f, .001f) ||
        FVector::Dist(RootAt(W, 0), RootAt(W, W->GetPlayLength())) > .01 ||
        !FMath::IsNearlyEqual(Travel.Size2D(), 45., .5) || FMath::Abs(Travel.Z) > .5 || !J->bForceRootLock)
    { Error = TEXT("Rabbit timing/root contract mismatch"); return false; }
    Error.Reset();
    return true;
}

int32 AHansaAmbientRabbits::GetLiveRabbitCount() const
{
    int32 Count = 0;
    for (const auto& Rabbit : Rabbits) Count += Rabbit.bPlaced ? 1 : 0;
    return Count;
}

TArray<FHansaRabbitObservation> AHansaAmbientRabbits::QueryRabbits() const
{
    TArray<FHansaRabbitObservation> Result;
    for (int32 I = 0; I < Rabbits.Num(); ++I)
    {
        const auto& Rabbit = Rabbits[I];
        FHansaRabbitObservation Observation;
        Observation.StableId = FString::Printf(TEXT("Animal.Rabbit.Ambient.Lubeck.%02d"), I+1);
        Observation.Activity = Rabbit.Activity;
        Observation.Location = Rabbit.Mesh->GetComponentLocation();
        Observation.Destination = Rabbit.Destination;
        Observation.AnimationTime = Rabbit.PoseTime;
        Observation.bVisible = Rabbit.bPlaced;
        Result.Add(Observation);
    }
    return Result;
}

#if WITH_EDITOR
EDataValidationResult AHansaAmbientRabbits::IsDataValid(FDataValidationContext& Context) const
{
    bool bValid = Population >= 5 && Population <= 6 && TownRadius >= 1000 && TownRadius <= 16000;
    for (const FSoftObjectPath Path : {RabbitMesh.ToSoftObjectPath(), Walk.ToSoftObjectPath(), Jump.ToSoftObjectPath()})
        bValid &= Path.ToString().StartsWith(TEXT("/Game/Hansa/Animals/Rabbit/"));
    FString Error;
    bValid &= ValidateClips(RabbitMesh.LoadSynchronous(), Walk.LoadSynchronous(), Jump.LoadSynchronous(), Error);
    if (!bValid) Context.AddError(FText::FromString(TEXT("Rabbit settings require 5-6 animals, a 10-160m radius and validated production assets. ") + Error));
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

void AHansaAmbientRabbits::RefreshObstacles()
{
    Occupied.Reset();
    const auto Projection = Host->BuildProjection();
    if (!Projection) { ValidationError = TEXT("Placement projection unavailable"); return; }
    for (const auto& Building : Projection.Value.GetPlacements())
        if (Building.Spec.CityId == Host->GetCityId())
            for (const auto Cell : Building.OccupiedCells) Occupied.Add(FIntPoint(Cell.X, Cell.Y));
    ValidationError.Reset();
}

bool AHansaAmbientRabbits::SafeGround(const FVector& Candidate, FVector& Ground) const
{
    using namespace Hansa::Simulation;
    if (!Foundation || !Host || !ValidationError.IsEmpty() || FVector::Dist2D(Candidate, TownCenter) > TownRadius) return false;
    const auto* Map = Host->FindPlacementMap();
    if (!Map) return false;
    // Canonical map cells are sorted X then Y. Binary lookup avoids duplicating million-cell maps.
    for (const FVector2D Offset : {FVector2D(0,0), FVector2D(-BodyRadius,-BodyRadius), FVector2D(-BodyRadius,BodyRadius),
        FVector2D(BodyRadius,-BodyRadius), FVector2D(BodyRadius,BodyRadius)})
    {
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
    // Ground-only trace cannot select roofs. A separate body query rejects authored obstacles.
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    GetWorld()->OverlapMultiByObjectType(Hits, Ground + FVector(0,0,48), FQuat::Identity, Objects,
        FCollisionShape::MakeBox(FVector(BodyRadius,BodyRadius,40)), FCollisionQueryParams(SCENE_QUERY_STAT(RabbitBody), false, this));
    if (Candidate.Z > Ground.Z + 1.)
    {
        TArray<FOverlapResult> AirHits;
        GetWorld()->OverlapMultiByObjectType(AirHits, Ground + FVector(0,0,48+FMath::Min(Candidate.Z-Ground.Z,23.)), FQuat::Identity, Objects,
            FCollisionShape::MakeBox(FVector(BodyRadius,BodyRadius,40)), FCollisionQueryParams(SCENE_QUERY_STAT(RabbitAirBody), false, this));
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

bool AHansaAmbientRabbits::SafePath(const FVector& From, const FVector& To) const
{
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(From, To) / 20.));
    FVector Previous = From;
    for (int32 I = 0; I <= Steps; ++I)
    {
        FVector Ground;
        if (!SafeGround(FMath::Lerp(From, To, double(I)/Steps), Ground) || FMath::Abs(Ground.Z - Previous.Z) > 6.) return false;
        Previous = Ground;
    }
    return true;
}

bool AHansaAmbientRabbits::Place(FHansaAmbientRabbit& Rabbit)
{
    for (int32 Attempt = 0; Attempt < 48; ++Attempt)
    {
        const float Angle = Random.FRandRange(0, 2*PI);
        const float Radius = FMath::Sqrt(Random.FRand()) * TownRadius;
        FVector Ground;
        if (!SafeGround(TownCenter + FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0), Ground)) continue;
        bool bClear = true;
        for (const auto& Other : Rabbits)
            if (&Other != &Rabbit && Other.bPlaced && FVector::Dist2D(Other.Mesh->GetComponentLocation(), Ground) < 2*BodyRadius) bClear = false;
        if (!bClear) continue;
        Rabbit.Start = Ground;
        Rabbit.Heading = FRotator(0, Random.FRandRange(-180,180), 0).Quaternion();
        Rabbit.Mesh->SetWorldLocation(Ground);
        Rabbit.bPlaced = true;
        Rabbit.Mesh->SetVisibility(true);
        Stand(Rabbit);
        Sample(Rabbit);
        return true;
    }
    return false; // Retry after streaming/placement changes. Never put a rabbit in an unsafe fallback.
}

void AHansaAmbientRabbits::Stand(FHansaAmbientRabbit& Rabbit)
{
    Rabbit.Activity = EHansaRabbitActivity::Still;
    Rabbit.Elapsed = 0;
    Rabbit.Duration = Random.FRandRange(2, 6);
    // Walk destinations end on complete gait cycles, so resting uses the grounded neutral pose.
    Rabbit.PoseTime = 0;
    Rabbit.Mesh->SetAnimation(WalkClip);
}

void AHansaAmbientRabbits::Sample(FHansaAmbientRabbit& Rabbit)
{
    const FQuat GroundHeading = Rabbit.Activity == EHansaRabbitActivity::Jump ? Rabbit.Heading :
        Hansa::Game::TerrainPlacement::RoadRotation(GetWorld(), Rabbit.Mesh->GetComponentLocation(), Rabbit.Heading);
    Rabbit.Mesh->SetWorldRotation(GroundHeading * MeshFacing);
    Rabbit.Mesh->SetPosition(Rabbit.PoseTime, false);
    Rabbit.Mesh->TickAnimation(0, false);
    Rabbit.Mesh->RefreshBoneTransforms();
    Rabbit.Mesh->UpdateBounds();
}

void AHansaAmbientRabbits::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()->IsGameWorld() || GetNetMode() != NM_Standalone) return;
    if (!Host)
    {
        const auto* Mode = GetWorld()->GetAuthGameMode<AHansaGameMode>();
        if (!Mode) return;
        Host = const_cast<AHansaGameMode*>(Mode)->GetSimulationHost();
    }
    if (!Host || !Host->IsReady() || Host->GetCityId().ToString() != TEXT("City.Lubeck")) return;
    if (!Foundation)
        for (TActorIterator<AHansaLubeckWorldFoundation> It(GetWorld()); It; ++It) { Foundation = *It; break; }
    if (!Foundation) return;
    if (!bInitialized)
    {
        bInitialized = true;
        USkeletalMesh* Mesh = RabbitMesh.LoadSynchronous();
        WalkClip = Walk.LoadSynchronous(); JumpClip = Jump.LoadSynchronous();
        if (!ValidateClips(Mesh, WalkClip, JumpClip, ValidationError))
        { UE_LOG(LogTemp, Error, TEXT("Ambient rabbits disabled: %s"), *ValidationError); return; }
        JumpTravel = RootAt(JumpClip, JumpClip->GetPlayLength()) - RootAt(JumpClip, 0);
        MeshFacing = FQuat::FindBetweenNormals(JumpTravel.GetSafeNormal2D(), FVector::ForwardVector);
        TownCenter = Foundation->GetAutomationStartTransform().GetLocation();
        Random.Initialize(int32(Host->GetCampaignSeed() ^ 0x52414242));
        for (int32 I = 0; I < FMath::Clamp(Population,5,6); ++I)
        {
            auto& Rabbit = Rabbits.AddDefaulted_GetRef();
            Rabbit.Mesh = NewObject<USkeletalMeshComponent>(this, *FString::Printf(TEXT("Rabbit_%02d"), I+1));
            Rabbit.Mesh->SetupAttachment(GetRootComponent());
            Rabbit.Mesh->SetSkeletalMeshAsset(Mesh);
            Rabbit.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Rabbit.Mesh->SetCanEverAffectNavigation(false);
            Rabbit.Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Rabbit.Mesh->SetAnimation(WalkClip);
            Rabbit.Mesh->SetForcedLOD(2);
            Rabbit.Mesh->SetVisibility(false);
            Rabbit.Mesh->RegisterComponent();
            Rabbit.Mesh->SetComponentTickEnabled(false);
        }
    }
    if (Rabbits.IsEmpty()) return;
    RefreshIn -= DeltaSeconds;
    const bool bRefresh = RefreshIn <= 0;
    if (bRefresh) { RefreshObstacles(); RefreshIn = 1.f; }
    if (Host->GetSpeed() == EHansaRuntimeSimulationSpeed::Paused) return;
    const float Step = FMath::Min(DeltaSeconds, .1f); // Cosmetic real-time speed; no fast-forward skating.
    for (auto& Rabbit : Rabbits)
    {
        if (!Rabbit.bPlaced) { if (bRefresh) Place(Rabbit); continue; }
        FVector Ground;
        const FVector Location = Rabbit.Mesh->GetComponentLocation();
        if (!SafeGround(Location, Ground))
        { Rabbit.bPlaced = false; Rabbit.Mesh->SetVisibility(false); continue; }
        Rabbit.Elapsed += Step;
        if (Rabbit.Activity == EHansaRabbitActivity::Still)
        {
            if (Rabbit.Elapsed < Rabbit.Duration) continue;
            bool bMoving = false;
            for (int32 Attempt = 0; Attempt < 12; ++Attempt)
            {
                const bool bJump = Rabbit.WalksSinceJump >= 3;
                const float Angle = Random.FRandRange(-PI, PI);
                const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
                const float Distance = bJump ? JumpTravel.Size2D() : Random.RandRange(7,15)*WalkSpeed;
                FVector Target;
                if (!SafeGround(Location + Direction*Distance, Target) || !SafePath(Ground, Target)) continue;
                // Jumps must land on the authored root-height plane; reject slopes/steps.
                if (bJump && FMath::Abs(Target.Z - Ground.Z) > .5) continue;
                FVector AirGround;
                if (bJump && (!SafeGround(Ground+FVector(0,0,23),AirGround) || !SafeGround(Target+FVector(0,0,23),AirGround))) continue;
                Rabbit.Start = Ground; Rabbit.Destination = Target;
                Rabbit.Heading = Direction.Rotation().Quaternion();
                Rabbit.Activity = bJump ? EHansaRabbitActivity::Jump : EHansaRabbitActivity::Walk;
                Rabbit.Duration = bJump ? JumpClip->GetPlayLength() : Distance/WalkSpeed;
                Rabbit.Elapsed = 0;
                if (bJump) Rabbit.PoseTime = 0;
                Rabbit.Mesh->SetAnimation(bJump ? JumpClip : WalkClip);
                bMoving = true;
                break;
            }
            if (!bMoving) Stand(Rabbit);
        }
        else
        {
            FVector Next;
            if (Rabbit.Activity == EHansaRabbitActivity::Jump)
            {
                Rabbit.PoseTime = FMath::Min(Rabbit.Elapsed, Rabbit.Duration);
                Next = Rabbit.Start + (Rabbit.Heading*MeshFacing).RotateVector(RootAt(JumpClip, Rabbit.PoseTime)-RootAt(JumpClip,0));
            }
            else
            {
                Rabbit.PoseTime = FMath::Fmod(Rabbit.PoseTime + Step, WalkClip->GetPlayLength());
                Next = FMath::Lerp(Rabbit.Start,Rabbit.Destination,FMath::Min(Rabbit.Elapsed/Rabbit.Duration,1.f));
            }
            FVector NextGround;
            if (!SafeGround(Next, NextGround) || !SafePath(Ground, NextGround))
            { Rabbit.PoseTime = 0; Rabbit.Mesh->SetWorldLocation(Ground); Stand(Rabbit); }
            else
            {
                Rabbit.Mesh->SetWorldLocation(Rabbit.Activity == EHansaRabbitActivity::Jump ? Next : NextGround);
                if (Rabbit.Elapsed >= Rabbit.Duration)
                {
                    if (Rabbit.Activity == EHansaRabbitActivity::Jump) { ++CompletedJumps; Rabbit.WalksSinceJump = 0; Rabbit.PoseTime = 0; }
                    else ++Rabbit.WalksSinceJump;
                    Stand(Rabbit);
                }
            }
        }
        Sample(Rabbit);
    }
}
