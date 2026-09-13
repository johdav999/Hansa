#include "World/HansaLubeckWorldArt.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace Hansa::Game::LubeckWorldArt
{
    namespace
    {
        double Distance(const FVector2D& P, const LubeckPlacementGrid::FHansaSurfaceBox& Box)
        {
            const FVector V = Box.Rotation.UnrotateVector(FVector(P.X-Box.Location.X,P.Y-Box.Location.Y,0));
            const FVector2D Q(FMath::Abs(V.X)-Box.Scale.X*50,FMath::Abs(V.Y)-Box.Scale.Y*50);
            return FVector2D(FMath::Max(Q.X,0.),FMath::Max(Q.Y,0.)).Length()+FMath::Min(FMath::Max(Q.X,Q.Y),0.);
        }
    }
    double GroundHeight(const FVector2D& P)
    {
        double Land=1.e9,Shore=1.e9;
        for(const auto& S:LubeckPlacementGrid::GetLandSurfaces())Land=FMath::Min(Land,Distance(P,S));
        for(const auto& S:LubeckPlacementGrid::GetShoreSurfaces())Shore=FMath::Min(Shore,Distance(P,S));
        // Exact datum across every permitted cell. Only the non-buildable bank falls to the river bed.
        const double D=FMath::Min(Land,Shore);
        const double Bank=FMath::Clamp(D/650.,0.,1.);
        const double Terrace=Shore<=0?85.:75.;
        return FMath::Lerp(Terrace,-450.,Bank*Bank*(3.-2.*Bank));
    }
    bool IsDressingLocation(const FVector2D& P,double Radius)
    {
        // Decorative solids may not claim any of the authoritative buildable land/shore.
        for(const auto& S:LubeckPlacementGrid::GetLandSurfaces())if(Distance(P,S)<=Radius+200.)return false;
        for(const auto& S:LubeckPlacementGrid::GetShoreSurfaces())if(Distance(P,S)<=Radius+200.)return false;
        return true;
    }
}
AHansaLubeckWorldArt::AHansaLubeckWorldArt()
{
    PrimaryActorTick.bCanEverTick=false;
    bReplicates=false;
#if WITH_EDITOR
    SetIsSpatiallyLoaded(false);
#endif
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WorldArtRoot")));
    Tags.Add(TEXT("Presentation.Environment.Lubeck"));
    Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));Sun->SetupAttachment(RootComponent);Sun->SetMobility(EComponentMobility::Movable);Sun->bAtmosphereSunLight=true;
    Sky=CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));Sky->SetupAttachment(RootComponent);Sky->SetMobility(EComponentMobility::Movable);Sky->bRealTimeCapture=true;
    Atmosphere=CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));Atmosphere->SetupAttachment(RootComponent);
    Fog=CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("RiverHaze"));Fog->SetupAttachment(RootComponent);Fog->SetFogDensity(.007f);Fog->SetFogHeightFalloff(.25f);Fog->SetStartDistance(8000.f);
    Exposure=CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));Exposure->SetupAttachment(RootComponent);Exposure->bUnbound=true;
    Exposure->Settings.bOverride_AutoExposureMinBrightness=true;Exposure->Settings.bOverride_AutoExposureMaxBrightness=true;
    Exposure->Settings.AutoExposureMinBrightness=13;Exposure->Settings.AutoExposureMaxBrightness=13;
    Exposure->Settings.bOverride_MotionBlurAmount=true;Exposure->Settings.MotionBlurAmount=0;
    Quay=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("QuayEdge"));
    Moorings=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Moorings"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Q(TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaQuay_Edge4m.SM_HansaQuay_Edge4m"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> M(TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaMooring_Post.SM_HansaMooring_Post"));
    Quay->SetStaticMesh(Q.Object);Moorings->SetStaticMesh(M.Object);
    for(auto* C:{Quay.Get(),Moorings.Get()}){C->SetupAttachment(RootComponent);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetCullDistances(0,18000);C->ComponentTags.Add(TEXT("Presentation.Environment.Harbor"));}
}
void AHansaLubeckWorldArt::OnConstruction(const FTransform& T)
{
    Super::OnConstruction(T);SetLightingPreset(false);Quay->ClearInstances();Moorings->ClearInstances();
    // Shore outside the playable central strip. No decorative pier or road suggests unowned connectivity.
    for(int I=0;I<8;++I){const FVector P(290,-1550+I*400,85);Quay->AddInstance(FTransform(FRotator::ZeroRotator,P));if(I%2==0)Moorings->AddInstance(FTransform(FVector(300,P.Y,85)));}
}
void AHansaLubeckWorldArt::SetLightingPreset(bool bEvening)
{
    Sun->SetRelativeRotation(FRotator(bEvening?-24:-48,-35,0));Sun->SetIntensity(bEvening?14000:32000);
    Sun->SetLightColor(bEvening?FLinearColor(1,.79f,.58f):FLinearColor(1,.95f,.87f));
    Sky->SetIntensity(bEvening?.8f:1.f);
}
