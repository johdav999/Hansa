#include "World/HansaCompoundGround.h"
#include "World/HansaTerrainPlacement.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Components/PrimitiveComponent.h"

namespace Hansa::Game::CompoundGround
{
TArray<FVector> RoadApproach(FVector Entrance,TConstArrayView<FVector> RoadCenters)
{
    if(RoadCenters.IsEmpty())return {};
    FVector Target;double Best=TNumericLimits<double>::Max();
    for(FVector Center:RoadCenters)
    {
        // Target the opaque road core; a cell corner can lie outside the road mesh.
        Center.Z=0;Center.Y=FMath::Clamp(Entrance.Y,Center.Y-60,Center.Y+60);
        const double Distance=FVector::DistSquared2D(Entrance,Center);
        if(Distance<Best||(FMath::IsNearlyEqual(Distance,Best)&&Center.Y<Target.Y)){Target=Center;Best=Distance;}
    }
    Entrance.Z=0;
    if(FMath::Abs(Target.Y-Entrance.Y)<40)return {Entrance,Target};
    // Follow the inside of the frontage, then cross into the actual road cell.
    return {Entrance,Entrance-FVector(65,0,0),FVector(Entrance.X-65,Target.Y,0),Target};
}
FSurvey Survey(const UWorld* World, const FTransform& Transform, const FBox& Bounds)
{
    FSurvey Out;
    if (!Bounds.IsValid) return Out;
    const int32 NX = FMath::Clamp(FMath::CeilToInt(Bounds.GetSize().X / SampleSpacing), 1, 256);
    const int32 NY = FMath::Clamp(FMath::CeilToInt(Bounds.GetSize().Y / SampleSpacing), 1, 256);
    for (int32 Y=0; Y<=NY; ++Y) for (int32 X=0; X<=NX; ++X)
    {
        const FVector P = Transform.TransformPosition(FVector(
            FMath::Lerp(Bounds.Min.X, Bounds.Max.X, double(X)/NX),
            FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, double(Y)/NY), Bounds.Min.Z));
        ++Out.Samples;
        FHitResult Hit;
        if (!TerrainPlacement::Trace(World, P+FVector(0,0,1000000), P-FVector(0,0,1000000), Hit)) continue;
        const double Z = Transform.InverseTransformPosition(Hit.ImpactPoint).Z;
        if (Out.Hits++ == 0) Out.Minimum = Out.Maximum = Z;
        else { Out.Minimum = FMath::Min(Out.Minimum,Z); Out.Maximum = FMath::Max(Out.Maximum,Z); }
        Out.SlopeDegrees = FMath::Max(Out.SlopeDegrees, FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(double(Hit.ImpactNormal.Z), -1.0, 1.0))));
    }
    return Out;
}

bool CanPlace(const UWorld* World, const FTransform& Transform, const FHansaCompoundComposition& Composition, const FBox& Plot)
{
    if (!Composition.IsValid()) return false;
    if (!World) return true;
    const FSurvey PlotSurvey = Survey(World, Transform, Plot);
    if (PlotSurvey.Hits == 0)
    {
        // Only legacy/headless worlds without terrain keep their planar datum.
        for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
        {
            if (It->IsA<ALandscapeProxy>() || It->ActorHasTag(TEXT("Hansa.Terrain"))) return false;
            TArray<UPrimitiveComponent*> Components;It->GetComponents(Components);
            for (const auto* Component:Components)
                if (Component->ComponentHasTag(TEXT("Hansa.Terrain"))) return false;
        }
        return true;
    }
    if (!PlotSurvey.IsComplete() || PlotSurvey.SlopeDegrees > MaximumSlopeDegrees) return false;
    for (const auto& I : Composition.Instances)
    {
        if (I.Group != TEXT("Dwelling") && I.Group != TEXT("Workshop")) continue;
        if (!Survey(World, I.Transform * Transform, I.AuthoredBounds).IsBuildable()) return false;
    }
    return true;
}
}

