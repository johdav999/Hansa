#include "World/HansaTerrainPlacement.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "LandscapeProxy.h"

namespace Hansa::Game::TerrainPlacement
{
    bool Trace(const UWorld* World, const FVector& Start, const FVector& End, FHitResult& Hit)
    {
        if (!World) return false;
        TArray<FHitResult> Hits;
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        World->LineTraceMultiByObjectType(Hits, Start, End, Objects,
            FCollisionQueryParams(SCENE_QUERY_STAT(HansaTerrainPlacement), true));
        bool Found = false;
        for (const FHitResult& Candidate : Hits)
        {
            const auto* Actor = Candidate.GetActor();
            const auto* Component = Candidate.GetComponent();
            if (!Actor || !Component || Actor->IsHidden()) continue;
            if (!Actor->IsA<ALandscapeProxy>() && !Actor->ActorHasTag(TEXT("Hansa.Terrain")) &&
                !Component->ComponentHasTag(TEXT("Hansa.Terrain"))) continue;
            if (!Found || Candidate.Distance < Hit.Distance) { Hit = Candidate; Found = true; }
        }
        return Found;
    }

    FQuat RoadRotation(const UWorld* World, const FVector& Position, const FQuat& Heading)
    {
        FHitResult Hit;
        if (!Trace(World, Position + FVector(0,0,1000000), Position - FVector(0,0,1000000), Hit)) return Heading;
        const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
        const FVector Forward = FVector::VectorPlaneProject(Heading.GetForwardVector(), Normal).GetSafeNormal();
        return Forward.IsNearlyZero() ? Heading : FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat();
    }

    FVector Ground(const UWorld* World, const FVector& Position, double DatumZ)
    {
        FHitResult Hit;
        const FVector Datum(Position.X, Position.Y, DatumZ);
        if (!Trace(World, Datum + FVector(0,0,1000000), Datum - FVector(0,0,1000000), Hit)) return Position;
        return FVector(Position.X, Position.Y, Hit.ImpactPoint.Z + Position.Z - DatumZ);
    }
}
