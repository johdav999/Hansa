#pragma once
#include "CoreMinimal.h"
#include "ConvexVolume.h"

class AHansaCityCentrePresentation;
class AHansaLubeckWorldFoundation;
class UWorld;

namespace Hansa::Game
{
/** One camera-selected city per pool. No ambient state belongs to the simulation. */
struct HANSA_API FAmbientView
{
    TWeakObjectPtr<AHansaCityCentrePresentation> Centre;
    FName City;
    FVector Focus = FVector::ZeroVector;
    FVector HomeCenter = FVector::ZeroVector;
    bool bHasHomeCenter = false;
    bool bEnabled = true;
    bool bHasCamera = false;
    bool bHasFrustum = false;
    FConvexVolume Frustum;
    float PollIn = 0;

    // Discovery is four times/second, independent of the number of animated individuals.
    bool Refresh(UWorld* World, const AHansaLubeckWorldFoundation& Foundation, FName HomeCity, float Delta);
    bool Includes(const FVector& Position) const;
};
}
