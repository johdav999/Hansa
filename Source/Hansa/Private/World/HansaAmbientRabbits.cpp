#include "World/HansaAmbientRabbits.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
namespace { FVector RootAt(const UAnimSequence* Clip, double Time) { return Clip->ExtractRootTrackTransform(FAnimExtractContext(Time),nullptr).GetTranslation(); } }
AHansaAmbientRabbits::AHansaAmbientRabbits()
{
    const auto P = FHansaAmbientAnimalProfile::Rabbit();
    Species = {P}; RabbitMesh = P.Mesh; Walk = P.Walk; Jump = P.Jump;
}
void AHansaAmbientRabbits::SyncLegacyProfile()
{
    if (bInitialized) return;
    auto P = FHansaAmbientAnimalProfile::Rabbit();
    P.MinimumPopulation = Population; P.MaximumPopulation = Population; P.TownRadius = TownRadius;
    P.Mesh = RabbitMesh; P.Walk = Walk; P.Jump = Jump;
    // Existing serialized rabbit actors keep their overrides, and gain the dog population.
    Species = {P, FHansaAmbientAnimalProfile::Dog()};
}
void AHansaAmbientRabbits::Tick(float DeltaSeconds) { SyncLegacyProfile(); Super::Tick(DeltaSeconds); }
int32 AHansaAmbientRabbits::GetLiveRabbitCount() const { return GetLiveAnimalCount(TEXT("Rabbit")); }
TArray<FHansaRabbitObservation> AHansaAmbientRabbits::QueryRabbits() const
{
    TArray<FHansaRabbitObservation> Result;
    for (const auto& A : QueryAnimals()) if (A.SpeciesId == TEXT("Rabbit"))
    {
        FHansaRabbitObservation O; O.StableId = A.StableId; O.Activity = static_cast<EHansaRabbitActivity>(A.Activity);
        O.Location = A.Location; O.Destination = A.Destination; O.AnimationTime = A.AnimationTime; O.bVisible = A.bVisible; Result.Add(O);
    }
    return Result;
}
#if WITH_EDITOR
EDataValidationResult AHansaAmbientRabbits::IsDataValid(FDataValidationContext& Context) const
{
    auto P = FHansaAmbientAnimalProfile::Rabbit(); P.MinimumPopulation = Population; P.MaximumPopulation = Population; P.TownRadius = TownRadius;
    FString Error;
    const bool Valid = Population >= 5 && Population <= 6 && ValidateProfile(P,RabbitMesh.LoadSynchronous(),Walk.LoadSynchronous(),Jump.LoadSynchronous(),Error);
    if (!Valid) Context.AddError(FText::FromString(TEXT("Legacy rabbit settings invalid: ")+Error));
    return Valid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
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
