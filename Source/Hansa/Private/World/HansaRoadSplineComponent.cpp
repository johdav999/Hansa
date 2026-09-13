#include "World/HansaRoadSplineComponent.h"
#include "World/HansaTerrainPlacement.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "VT/RuntimeVirtualTextureVolume.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarRoadRVT(TEXT("hansa.Road.RVT"),1,TEXT("Enable road shoulder RVT writes. Physical terrain conformity remains enabled."));

UHansaRoadSplineComponent::UHansaRoadSplineComponent()
{
    SetMobility(EComponentMobility::Movable);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    SetForwardAxis(ESplineMeshAxis::X, false);
    SetBoundaryMin(-200, false);
    SetBoundaryMax(200, false);
    SetStartAndEnd(FVector(-200,0,0), FVector(400,0,0), FVector(200,0,0), FVector(400,0,0), false);
    VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Always;
}

void UHansaRoadSplineComponent::OnRegister()
{
    Super::OnRegister();
    AddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::LevelChanged);
    RemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &ThisClass::LevelChanged);
}

void UHansaRoadSplineComponent::OnUnregister()
{
    InvalidateRoadRVT();
    for (USplineMeshComponent* Section : Sections) if (Section) Section->DestroyComponent();
    Sections.Reset();
    FWorldDelegates::LevelAddedToWorld.Remove(AddedHandle);
    FWorldDelegates::LevelRemovedFromWorld.Remove(RemovedHandle);
    Super::OnUnregister();
}

void UHansaRoadSplineComponent::OnVisibilityChanged()
{
    Super::OnVisibilityChanged();
    for (int32 I=0;I<Sections.Num();++I) if (Sections[I]) Sections[I]->SetVisibility(IsVisible() && ActiveSections>1 && I<ActiveSections);
    InvalidateRoadRVT();
}

void UHansaRoadSplineComponent::LevelChanged(ULevel* Level, UWorld* World)
{
    if (World != GetWorld() || FittedClearance < 0) return;
    FBox ChangedBounds(ForceInit);
    if (Level) for (AActor* Actor:Level->Actors) if (Actor && Actor->IsA<ALandscapeProxy>()) ChangedBounds+=Actor->GetComponentsBoundingBox(true);
    if (ChangedBounds.IsValid && !ChangedBounds.ExpandBy(250).Intersect(Bounds.GetBox())) return;
    // Collision registration/removal completes after the world delegate returns.
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]()
    { if(IsRegistered()) FitTerrain(FittedClearance,bFittedPreview,true); }));
}

void UHansaRoadSplineComponent::InvalidateRoadRVT()
{
    if (!GetWorld()) return;
    for (TActorIterator<ARuntimeVirtualTextureVolume> It(GetWorld()); It; ++It)
        if (auto* Volume = It->VirtualTextureComponent.Get())
            if (Volume->GetVirtualTexture() && Volume->GetVirtualTexture()->GetName() == TEXT("RVT_HansaRoad")) Volume->Invalidate(Bounds);
}

bool UHansaRoadSplineComponent::FitTerrain(float Clearance, bool bPreview, bool bForce)
{
    if (!GetWorld() || !GetStaticMesh()) return false;
    const FTransform Transform = GetComponentTransform();
    if (!bForce && FittedMesh == GetStaticMesh() && FittedTransform.Equals(Transform, .001)
        && FittedClearance == Clearance && bFittedPreview == bPreview && (GetMaterial(0) == TerrainMaterial || !bTerrainFitted) && FittedRVTMode == CVarRoadRVT.GetValueOnGameThread() && FittedErrorTolerance == SplineErrorTolerance)
    {
        LastTraceCount = 0; LastFitMilliseconds = 0;
        return bTerrainFitted;
    }
    const double Start = FPlatformTime::Seconds();
    InvalidateRoadRVT();
    LastTraceCount = 0;
    bTerrainFitted = false;
    FittedTransform = Transform;
    FittedMesh = GetStaticMesh();
    FittedClearance = Clearance;
    bFittedPreview = bPreview;
    FittedRVTMode = CVarRoadRVT.GetValueOnGameThread();
    FittedErrorTolerance = SplineErrorTolerance;
    // One-vertex apron permits centred derivatives at shared boundaries. Samples use world XY,
    // so neighbouring cells and rotated junctions agree independently of their actor origin Z.
    constexpr int32 Size = 35;
    constexpr float Step = 12.5f;
    constexpr float Half = 212.5f;
    TArray<float> Heights;
    Heights.SetNumUninitialized(Size*Size);
    // A native Landscape exposes its complex collision heightfield directly. Resolve it
    // once, then avoid thousands of broad-phase world traces for each edited road cell.
    FHitResult CentreHit;
    Hansa::Game::TerrainPlacement::Trace(GetWorld(),Transform.GetLocation()+FVector(0,0,1000000),Transform.GetLocation()-FVector(0,0,1000000),CentreHit);
    ++LastTraceCount;
    const auto* Landscape=Cast<ALandscapeProxy>(CentreHit.GetActor());
    if(Landscape && !Landscape->GetActorUpVector().Equals(FVector::UpVector,.0001))Landscape=nullptr;
    const auto Sample=[&](const FVector& P,double& Z)
    {
        if(Landscape)
        {
            const TOptional<float> Height=Landscape->GetHeightAtLocation(P);
            if(Height.IsSet()){Z=Height.GetValue();return true;}
        }
        FHitResult Hit;++LastTraceCount;
        if(!Hansa::Game::TerrainPlacement::Trace(GetWorld(),P+FVector(0,0,1000000),P-FVector(0,0,1000000),Hit))return false;
        Z=Hit.ImpactPoint.Z;return true;
    };
    int32 Found = 0;
    float Min = MAX_flt, Max = -MAX_flt;
    for (int32 Y=0; Y<Size; ++Y)
        for (int32 X=0; X<Size; ++X)
        {
            const FVector P = Transform.TransformPosition(FVector(X*Step-Half,Y*Step-Half,0));
            double Z=0;
            const bool bHit=Sample(P,Z);
            Found += bHit;
            const float H = bHit ? Z-Transform.GetLocation().Z : 0;
            Heights[Y*Size+X] = H;
            Min = FMath::Min(Min,H); Max = FMath::Max(Max,H);
        }
    SampledHeights = Heights;
    if (Found != Size*Size)
    {
        FitDiagnostic = Found == 0 ? TEXT("No terrain collision; legacy road datum retained.")
            : TEXT("Terrain collision is incomplete; road fitting awaits terrain streaming.");
        SetStartAndEnd(FVector(-200,0,0),FVector(400,0,0),FVector(200,0,0),FVector(400,0,0));
        if (TerrainMaterial) TerrainMaterial->SetScalarParameterValue(TEXT("RoadFitEnabled"), 0);
        SetRenderInMainPass(Found == 0);
        for (USplineMeshComponent* Section : Sections) Section->SetVisibility(false);
        RuntimeVirtualTextures.Reset();
        MarkRenderStateDirty();
        LastFitMilliseconds = (FPlatformTime::Seconds()-Start)*1000;
        return false;
    }
    // Probe between source vertices. Conservatively distribute any positive residual to
    // the four surrounding vertices, considering both triangulation diagonals. The apron
    // includes the same neighbouring quads on either side of a shared cell boundary.
    TArray<float> Lift;Lift.Init(0,Size*Size);
    for(int32 Y=0;Y<Size-1;++Y)for(int32 X=0;X<Size-1;++X)
    {
        const FVector P=Transform.TransformPosition(FVector((X+.5f)*Step-Half,(Y+.5f)*Step-Half,0));
        double Z=0;if(!Sample(P,Z))continue;
        const int32 A=Y*Size+X,B=A+1,C=A+Size,D=C+1;
        const float Interpolated=FMath::Min((Heights[A]+Heights[D])*.5f,(Heights[B]+Heights[C])*.5f);
        const float Correction=FMath::Max(0.f,float(Z-Transform.GetLocation().Z)-Interpolated);
        for(int32 I:{A,B,C,D})Lift[I]=FMath::Max(Lift[I],Correction);
    }
    for(int32 I=0;I<Heights.Num();++I){Heights[I]+=Lift[I];Max=FMath::Max(Max,Heights[I]);}
    auto* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain.M_Road_Terrain"));
    if (!Material)
    {
        FitDiagnostic = TEXT("M_Road_Terrain is missing; run the road material authoring commandlet.");
        return false;
    }
    if (!TerrainMaterial) TerrainMaterial = UMaterialInstanceDynamic::Create(Material,this);
    const auto Height = [&](int32 X, int32 Y) { return Heights[Y*Size+X]; };
    SetStartAndEnd(FVector(-200,0,Height(1,17)),FVector(400,0,(Height(2,17)-Height(0,17))*16),
        FVector(200,0,Height(33,17)),FVector(400,0,(Height(34,17)-Height(32,17))*16));
    if (!HeightTexture)
    {
        HeightTexture = UTexture2D::CreateTransient(Size,Size,PF_R32_FLOAT);
        HeightTexture->SRGB = false;
        HeightTexture->Filter = TF_Bilinear;
        HeightTexture->AddressX = TA_Clamp; HeightTexture->AddressY = TA_Clamp;
        HeightTexture->NeverStream = true;
    }
    void* Data = HeightTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, Heights.GetData(), Heights.Num()*sizeof(float));
    HeightTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
    HeightTexture->UpdateResource();
    TerrainMaterial->SetTextureParameterValue(TEXT("RoadHeightField"),HeightTexture);
    const FMatrix Inverse = Transform.ToInverseMatrixWithScale();
    TerrainMaterial->SetVectorParameterValue(TEXT("RoadLocalX"), FLinearColor(Inverse.M[0][0],Inverse.M[1][0],Inverse.M[2][0],Inverse.M[3][0]));
    TerrainMaterial->SetVectorParameterValue(TEXT("RoadLocalY"), FLinearColor(Inverse.M[0][1],Inverse.M[1][1],Inverse.M[2][1],Inverse.M[3][1]));
    const FMatrix ToWorld=Transform.ToMatrixWithScale();
    TerrainMaterial->SetVectorParameterValue(TEXT("RoadWorldX"),FLinearColor(ToWorld.M[0][0],ToWorld.M[1][0],ToWorld.M[3][0],0));
    TerrainMaterial->SetVectorParameterValue(TEXT("RoadWorldY"),FLinearColor(ToWorld.M[0][1],ToWorld.M[1][1],ToWorld.M[3][1],0));
    TerrainMaterial->SetVectorParameterValue(TEXT("RoadSection"),FLinearColor(1,0,0,0));
    TerrainMaterial->SetScalarParameterValue(TEXT("RoadDatumZ"),Transform.GetLocation().Z);
    TerrainMaterial->SetScalarParameterValue(TEXT("RoadClearance"),Clearance);
    TerrainMaterial->SetScalarParameterValue(TEXT("RoadFitEnabled"),1);
    TerrainMaterial->SetScalarParameterValue(TEXT("GroundEnabled"),0);
    SetMaterial(0,TerrainMaterial);
    // Preserve the approved tessellation. Decimated LODs cannot maintain the same terrain fit.
    SetForcedLodModel(1);
    SetBoundsScale(1 + (Max-Min+20)/200);
    RuntimeVirtualTextures.Reset();
    if (!bPreview && CVarRoadRVT.GetValueOnGameThread()!=0)
    {
        bool bHasRoadVolume = false;
        for (TActorIterator<ARuntimeVirtualTextureVolume> It(GetWorld()); It; ++It)
            if (It->VirtualTextureComponent->GetVirtualTexture() && It->VirtualTextureComponent->GetVirtualTexture()->GetName() == TEXT("RVT_HansaRoad")) bHasRoadVolume = true;
        if (!bHasRoadVolume)
        {
            FBox TerrainBounds(ForceInit);
            for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It) TerrainBounds += It->GetComponentsBoundingBox(true);
            if (TerrainBounds.IsValid)
                if (auto* RVT = LoadObject<URuntimeVirtualTexture>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/RVT_HansaRoad.RVT_HansaRoad")))
                {
                    FActorSpawnParameters Parameters;Parameters.ObjectFlags|=RF_Transient;
                    auto* Volume = GetWorld()->SpawnActor<ARuntimeVirtualTextureVolume>(Parameters);
                    Volume->VirtualTextureComponent->SetMobility(EComponentMobility::Movable);
                    TerrainBounds = TerrainBounds.ExpandBy(500);
                    Volume->SetActorTransform(FTransform(FQuat::Identity,TerrainBounds.Min,TerrainBounds.GetSize()));
                    Volume->VirtualTextureComponent->SetVirtualTexture(RVT);
                    Volume->Tags.Add(TEXT("Hansa.Road.RuntimeRVT"));
                }
        }
    }
    if (!bPreview && CVarRoadRVT.GetValueOnGameThread()!=0)
        for (TActorIterator<ARuntimeVirtualTextureVolume> It(GetWorld()); It; ++It)
            if (auto* Volume = It->VirtualTextureComponent.Get())
                if (auto* RVT = Volume->GetVirtualTexture(); RVT && RVT->GetName() == TEXT("RVT_HansaRoad"))
                {
                    RuntimeVirtualTextures.AddUnique(RVT);
                    if(It->ActorHasTag(TEXT("Hansa.Road.RuntimeRVT")))
                    {
                        FBox Extent=Volume->Bounds.GetBox();const FBox Before=Extent;
                        for(TActorIterator<ALandscapeProxy> Land(GetWorld());Land;++Land)Extent+=Land->GetComponentsBoundingBox(true).ExpandBy(500);
                        if(!Extent.Equals(Before,1))It->SetActorTransform(FTransform(FQuat::Identity,Extent.Min,Extent.GetSize()));
                    }
                }
    // Split straight runs only where a cubic section misses the sampled centreline.
    // Every section retains world-space texture density and a shared endpoint derivative.
    const auto Derivative = [&](int32 I) { return (Height(I+1,17)-Height(I-1,17))/(2*Step); };
    ActiveSections = 1;
    if (GetStaticMesh()->GetName().EndsWith(TEXT("_Straight")))
    {
        for (; ActiveSections < 4; ActiveSections *= 2)
        {
            float Error = 0;
            const int32 Intervals = 32/ActiveSections;
            for (int32 S=0; S<ActiveSections; ++S)
            {
                const int32 A=1+S*Intervals, B=A+Intervals;
                const float Length=Intervals*Step;
                for (int32 I=A+1; I<B; ++I)
                {
                    const float T=float(I-A)/Intervals;
                    const float Predicted=FMath::CubicInterp(Height(A,17),Derivative(A)*Length,Height(B,17),Derivative(B)*Length,T);
                    Error=FMath::Max(Error,FMath::Abs(Predicted-Height(I,17)));
                }
            }
            if (Error <= FMath::Clamp(SplineErrorTolerance,.25f,5.f)) break;
        }
    }
    SetRenderInMainPass(ActiveSections == 1);
    SetRenderInDepthPass(ActiveSections == 1);
    SetCastShadow(false); // Thin dirt overlays should not cast floating shoulder shadows.
    for (int32 S=0; S<(ActiveSections>1?ActiveSections:0); ++S)
    {
        if (Sections.Num() <= S)
        {
            auto* Section=NewObject<USplineMeshComponent>(GetOwner());
            Section->SetupAttachment(this);
            Section->SetMobility(EComponentMobility::Movable);
            Section->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Section->SetCanEverAffectNavigation(false);
            Section->SetCastShadow(false);
            Section->SetForwardAxis(ESplineMeshAxis::X,false);
            Section->SetBoundaryMin(-200,false);Section->SetBoundaryMax(200,false);
            Section->VirtualTextureRenderPassType=ERuntimeVirtualTextureMainPassType::Always;
            GetOwner()->AddInstanceComponent(Section);
            Section->RegisterComponent();Sections.Add(Section);
        }
        auto* Section=Sections[S].Get();
        const int32 Intervals=32/ActiveSections,A=1+S*Intervals,B=A+Intervals;
        const float Length=Intervals*Step;
        Section->SetStaticMesh(GetStaticMesh());
        auto* SectionMaterial=Cast<UMaterialInstanceDynamic>(Section->GetMaterial(0));
        if(!SectionMaterial || SectionMaterial==TerrainMaterial || SectionMaterial->Parent!=Material)
            SectionMaterial=UMaterialInstanceDynamic::Create(Material,Section);
        SectionMaterial->CopyParameterOverrides(TerrainMaterial);
        SectionMaterial->SetVectorParameterValue(TEXT("RoadSection"),FLinearColor(Length/400,-200+(S+.5f)*Length,0,0));
        Section->SetMaterial(0,SectionMaterial);
        Section->SetStartAndEnd(FVector(-200+S*Length,0,Height(A,17)),FVector(Length,0,Derivative(A)*Length),
            FVector(-200+(S+1)*Length,0,Height(B,17)),FVector(Length,0,Derivative(B)*Length));
        Section->SetForcedLodModel(1);Section->SetBoundsScale(BoundsScale);
        Section->RuntimeVirtualTextures=RuntimeVirtualTextures;
        Section->MarkRenderStateDirty();Section->SetVisibility(IsVisible());
    }
    for (int32 S=(ActiveSections>1?ActiveSections:0);S<Sections.Num();++S)
    {Sections[S]->RuntimeVirtualTextures.Reset();Sections[S]->MarkRenderStateDirty();Sections[S]->SetVisibility(false);}
    // A split parent is only a section owner; it must not draw a second copy into the RVT.
    if (ActiveSections>1) RuntimeVirtualTextures.Reset();
    MarkRenderStateDirty();
    InvalidateRoadRVT();
    bTerrainFitted = true;
    FitDiagnostic.Reset();
    LastFitMilliseconds = (FPlatformTime::Seconds()-Start)*1000;
    return true;
}

bool UHansaRoadSplineComponent::SampleGroundHeight(const FVector& WorldPosition, double& OutZ) const
{
    if (!bTerrainFitted || SampledHeights.Num()!=35*35) return false;
    const FVector Local=FittedTransform.InverseTransformPosition(WorldPosition);
    const double X=(Local.X+212.5)/12.5,Y=(Local.Y+212.5)/12.5;
    if(X<0 || Y<0 || X>34 || Y>34) return false;
    const int32 IX=FMath::Min(33,FMath::FloorToInt(X)),IY=FMath::Min(33,FMath::FloorToInt(Y));
    OutZ=FittedTransform.GetLocation().Z+FMath::Lerp(
        FMath::Lerp(double(SampledHeights[IY*35+IX]),double(SampledHeights[IY*35+IX+1]),X-IX),
        FMath::Lerp(double(SampledHeights[(IY+1)*35+IX]),double(SampledHeights[(IY+1)*35+IX+1]),X-IX),Y-IY);
    return true;
}
