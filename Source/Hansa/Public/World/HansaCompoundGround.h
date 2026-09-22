#pragma once
#include "CoreMinimal.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"

class UWorld;
namespace Hansa::Game::CompoundGround
{
    // Fixed runtime policy, shared by placement validation and presentation.
    constexpr double SampleSpacing = 100.0;
    constexpr double MaximumSlopeDegrees = 15.0;
    constexpr double MaximumFoundationHeight = 120.0;
    struct HANSA_API FSurvey
    {
        int32 Samples = 0;
        int32 Hits = 0;
        double Minimum = 0;
        double Maximum = 0;
        double SlopeDegrees = 0;
        bool IsComplete() const { return Samples > 0 && Hits == Samples; }
        bool IsBuildable() const { return IsComplete() && SlopeDegrees <= MaximumSlopeDegrees && Maximum-Minimum <= MaximumFoundationHeight; }
    };
    HANSA_API TArray<FVector> RoadApproach(FVector Entrance, TConstArrayView<FVector> RoadCenters);
    HANSA_API FSurvey Survey(const UWorld* World, const FTransform& Transform, const FBox& Bounds);
    HANSA_API bool CanPlace(const UWorld* World, const FTransform& Transform, const FHansaCompoundComposition& Composition, const FBox& Plot);
}
