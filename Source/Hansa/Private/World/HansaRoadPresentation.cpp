#include "World/HansaRoadPresentation.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaRoadSplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckPlacementGrid.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AHansaRoadPresentation::AHansaRoadPresentation()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RoadCellDatum")));
    Surface = CreateDefaultSubobject<UHansaRoadSplineComponent>(TEXT("RoadSurface"));
    Surface->SetupAttachment(GetRootComponent());
    Surface->SetMobility(EComponentMobility::Movable);
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetGenerateOverlapEvents(false);
    Surface->SetCanEverAffectNavigation(false);
}

void AHansaRoadPresentation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyNeighbors(NeighborMask);
    if (GroundFoundation) ApplyGround(*GroundFoundation);
}

UStaticMesh* AHansaRoadPresentation::MeshForMask(uint8 Mask) const
{
    using namespace Hansa::Game::RoadTopology;
    switch (Resolve(Mask).Tile)
    {
    case ETile::End: return End;
    case ETile::Straight: return Straight;
    case ETile::Corner: return Corner;
    case ETile::TJunction: return TJunction;
    case ETile::Crossroads: return Crossroads;
    default: return Isolated;
    }
}

void AHansaRoadPresentation::ApplyNeighbors(uint8 Mask)
{
    NeighborMask = Mask & 15;
    Surface->SetStaticMesh(MeshForMask(NeighborMask));
    SetWetness(Wetness);
    Surface->SetRelativeTransform(FTransform(FRotator(0,
        Hansa::Game::RoadTopology::Resolve(NeighborMask).QuarterTurns * 90.0, 0)));
    Surface->ComponentTags.Reset();
    Surface->ComponentTags.Add(FName(*FString::Printf(TEXT("Hansa.RoadTopology.Mask.%u"), NeighborMask)));
}

void AHansaRoadPresentation::SetWetness(float Value)
{
    const bool Changed=Wetness!=FMath::Clamp(Value,0.f,1.f);
    Wetness = FMath::Clamp(Value, 0.f, 1.f);
    if (!WetMaterial && Surface->GetStaticMesh())
        WetMaterial = UMaterialInstanceDynamic::Create(Surface->GetStaticMesh()->GetMaterial(0), this);
    if (WetMaterial)
    {
        WetMaterial->SetScalarParameterValue(TEXT("Wetness"), Wetness);
        if (auto* Spline = Cast<UHansaRoadSplineComponent>(Surface); Spline && Spline->bTerrainFitted)
        {
            if (auto* Material = Cast<UMaterialInstanceDynamic>(Surface->GetMaterial(0))) Material->SetScalarParameterValue(TEXT("Wetness"), Wetness);
        }
        else Surface->SetMaterial(0, WetMaterial);
        TArray<UStaticMeshComponent*> RoadSections;GetComponents(RoadSections);
        for(auto* Section:RoadSections)if(auto* Material=Cast<UMaterialInstanceDynamic>(Section->GetMaterial(0)))Material->SetScalarParameterValue(TEXT("Wetness"),Wetness);
        if(Changed) if(auto* Spline=Cast<UHansaRoadSplineComponent>(Surface)) Spline->InvalidateRoadRVT();
    }
}

double AHansaRoadPresentation::GroundBaseHeight()
{
    const auto& Land=Hansa::Game::LubeckPlacementGrid::GetLandSurfaces()[0];
    return Land.Location.Z + Land.Scale.Z*50.;
}

void AHansaRoadPresentation::ConfigureGroundMaterial(UMaterialInstanceDynamic& Material, const AHansaLubeckWorldFoundation& Foundation, const FVector* SamplePosition)
{
    const FMatrix Inverse=Foundation.GetActorTransform().ToInverseMatrixWithScale();
    Material.SetVectorParameterValue(TEXT("GroundX"),FLinearColor(Inverse.M[0][0],Inverse.M[1][0],Inverse.M[2][0],Inverse.M[3][0]));
    Material.SetVectorParameterValue(TEXT("GroundY"),FLinearColor(Inverse.M[0][1],Inverse.M[1][1],Inverse.M[2][1],Inverse.M[3][1]));
    const FVector Up=Foundation.GetActorTransform().TransformVector(FVector::UpVector);
    Material.SetVectorParameterValue(TEXT("GroundUp"),FLinearColor(Up.X,Up.Y,Up.Z,0));
    const auto Shores=Hansa::Game::LubeckPlacementGrid::GetShoreSurfaces();
    for (int32 Index=0;Index<Shores.Num();++Index)
    {
        const auto& Shore=Shores[Index];
        Material.SetVectorParameterValue(*FString::Printf(TEXT("Shore%d"),Index),FLinearColor(Shore.Location.X,Shore.Location.Y,Shore.Scale.X*50,Shore.Scale.Y*50));
        const double Angle=FMath::DegreesToRadians(-Shore.Rotation.Yaw);
        Material.SetVectorParameterValue(*FString::Printf(TEXT("ShoreRotation%d"),Index),FLinearColor(FMath::Cos(Angle),FMath::Sin(Angle),Shore.Location.Z+Shore.Scale.Z*50-GroundBaseHeight(),0));
    }
    // The legacy shore shader must never add a second height displacement on real terrain.
    FHitResult TerrainHit;
    const FVector Position = SamplePosition ? *SamplePosition : Foundation.GetActorLocation();
    const bool bTerrain = Foundation.bUseAuthoredWorld || Hansa::Game::TerrainPlacement::Trace(
        Foundation.GetWorld(), Position + FVector(0,0,1000000), Position - FVector(0,0,1000000), TerrainHit);
    Material.SetScalarParameterValue(TEXT("GroundEnabled"), bTerrain ? 0 : 1);
}

void AHansaRoadPresentation::ApplyGround(const AHansaLubeckWorldFoundation& Foundation)
{
    GroundFoundation=const_cast<AHansaLubeckWorldFoundation*>(&Foundation);
    const FVector Position = GetActorLocation();
    if (WetMaterial) ConfigureGroundMaterial(*WetMaterial,Foundation,&Position);
    if (auto* Spline = Cast<UHansaRoadSplineComponent>(Surface))
    {
        Spline->FitTerrain(Spline->SurfaceClearance);
        SetWetness(Wetness);
    }
}

#if WITH_EDITOR
EDataValidationResult AHansaRoadPresentation::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult Parent = Super::IsDataValid(Context);
    bool bValid = Parent != EDataValidationResult::Invalid;
    if(const auto* Spline=Cast<UHansaRoadSplineComponent>(Surface))
        if(!FMath::IsFinite(Spline->SurfaceClearance) || Spline->SurfaceClearance<.5f || Spline->SurfaceClearance>10.f ||
            !FMath::IsFinite(Spline->SplineErrorTolerance) || Spline->SplineErrorTolerance<.25f || Spline->SplineErrorTolerance>5.f)
        {Context.AddError(FText::FromString(TEXT("Road terrain clearance must be 0.5-10 cm and spline tolerance 0.25-5 cm.")));bValid=false;}
    const bool bProduction = GetPathName().StartsWith(TEXT("/Game/Mesh/"));
    for (uint8 Mask = 0; Mask < 16; ++Mask)
    {
        const UStaticMesh* Mesh = MeshForMask(Mask);
        if (!Mesh || Mesh->GetNumLODs() < 3 || Mesh->GetStaticMaterials().Num() != 1 ||
            (bProduction && Mesh->GetPathName().Contains(TEXT("/Staging/"))))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Road mask %u requires a reviewed mesh with three LODs and one material; production cannot reference staging."), Mask)));
            bValid = false;
            continue;
        }
        const FBox Bounds = Mesh->GetBoundingBox();
        if (Bounds.Min.X < -200.1 || Bounds.Min.Y < -200.1 || Bounds.Max.X > 200.1 || Bounds.Max.Y > 200.1 ||
            Bounds.GetSize().X < 200 || Bounds.GetSize().Y < 200 || Bounds.GetSize().Z > 10)
        {
            Context.AddError(FText::FromString(TEXT("Road mesh must retain its native 4m cell datum and low-profile surface; runtime terrain deformation must preserve XY cell identity.")));
            bValid = false;
        }
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
