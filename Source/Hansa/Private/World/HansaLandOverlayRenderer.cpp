#include "World/HansaLandOverlayRenderer.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaTerrainPlacement.h"
#include "Placement/HansaRostockPlacement.h"
#include "UI/HansaUiStyle.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

using namespace Hansa::Game::LandOverlay;
using namespace Hansa::Simulation;
namespace
{
uint64 DigestQuery(const FHansaLandQueryResult& Query, const FHansaLandOverlayOptions& Options)
{
    uint64 Hash = 14695981039346656037ULL;
    auto Add = [&](uint64 Value) { for (int32 I=0; I<8; ++I) { Hash ^= (Value >> (I*8)) & 255; Hash *= 1099511628211ULL; } };
    Add(GetTypeHash(Query.CityId.ToString())); Add(Query.ViewerHouseId.GetValue());
    Add(Options.SelectionRevision); Add(Options.SurveyStride); Add(Options.SelectionBoundaries.IsValid()?Options.SelectionBoundaries->Num():0);
    Add(static_cast<uint8>(Options.Mode)); Add(Options.bHighContrast);
    Add(GetTypeHash(Options.LineWidthCm)); Add(Options.SelectedCell.IsSet());
    if (Options.SelectedCell) { Add(Options.SelectedCell->X); Add(Options.SelectedCell->Y); }
    for (const auto& C : Query.Cells)
    {
        Add(C.Coordinate.X); Add(C.Coordinate.Y); Add(C.RecordedOwnerId.GetValue());
        Add(static_cast<uint8>(C.Access)); Add(static_cast<uint8>(C.Terrain));
        Add(C.bOutsideSurvey); Add(C.bSurveyKnown); Add(C.bProtected); Add(C.OccupyingBuildingId.GetValue());
    }
    return Hash;
}
FLinearColor Token(EHansaUiColorToken T, float Alpha = 1.f)
{
    FLinearColor C = UHansaUiStyleLibrary::GetColor(T); C.A = Alpha; return C;
}
struct FMesh
{
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FLinearColor> Colors;
    TArray<FVector2D> UVs, Styles;
    void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FLinearColor Color)
    {
        const int32 N = Vertices.Num();
        Vertices.Append({A,B,C,D}); Colors.Append({Color,Color,Color,Color});
        UVs.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(1,1),FVector2D(0,1)});
        Styles.AddZeroed(4);
        Indices.Append({N,N+1,N+2,N,N+2,N+3});
    }
};
}

AHansaLandOverlayRenderer::AHansaLandOverlayRenderer()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickInterval = 0.f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LandOverlayRoot")));
    FillMaterial = FSoftObjectPath(TEXT("/Game/Hansa/UI/LandOverlay/M_UI_LandOverlay_Fill.M_UI_LandOverlay_Fill"));
    RibbonMaterial = FSoftObjectPath(TEXT("/Game/Hansa/UI/LandOverlay/M_UI_LandOverlay_Ribbon.M_UI_LandOverlay_Ribbon"));
    bReplicates = false;
}
void AHansaLandOverlayRenderer::BeginPlay()
{
    Super::BeginPlay();
    LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &AHansaLandOverlayRenderer::OnLevelChanged);
    LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &AHansaLandOverlayRenderer::OnLevelChanged);
}
void AHansaLandOverlayRenderer::EndPlay(const EEndPlayReason::Type Reason)
{
    FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
    FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
    Super::EndPlay(Reason);
}
void AHansaLandOverlayRenderer::OnLevelChanged(ULevel*, UWorld* World)
{
    if (World == GetWorld()) InvalidateTerrain();
}
void AHansaLandOverlayRenderer::InvalidateTerrain()
{
    for (auto& Pair : Chunks) { Pair.Value.bDirty = true;BuildQueue.AddUnique(Pair.Key); }
    SetActorTickEnabled(!Chunks.IsEmpty());
}
void AHansaLandOverlayRenderer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    while(!BuildQueue.IsEmpty() && LastRebuildFrame!=GFrameCounter)
    {
        const FIntPoint Id=BuildQueue[0];BuildQueue.RemoveAt(0);
        if(auto* Chunk=Chunks.Find(Id);Chunk && Chunk->bDirty)Rebuild(*Chunk);
    }
    SetActorTickEnabled(!BuildQueue.IsEmpty());
}
bool AHansaLandOverlayRenderer::MakeGridTransform(FHansaCityDefinitionId City,
    const AHansaLubeckWorldFoundation& Foundation, FTransform& OutTransform)
{
    if (City.ToString() == TEXT("City.Rostock"))
        OutTransform = FTransform(FQuat::Identity, RostockPlacement::CellCenter(0,0,0)-FVector(200,200,0), FVector(400));
    else if (City.ToString() == TEXT("City.Lubeck"))
        OutTransform = FTransform(FQuat::Identity, Hansa::Game::LubeckPlacementGrid::GridToWorld({0,0},0)-FVector(200,200,0), FVector(400)) * Foundation.GetActorTransform();
    else return false;
    return true;
}
bool AHansaLandOverlayRenderer::ApplyChunk(FIntPoint Id, const FHansaLandQueryResult& Query,
    FIntPoint Min, FIntPoint Max, const FTransform& Transform, const FHansaLandOverlayOptions& Options)
{
    if (!HasGroundMaterial() || Query.Failure != EHansaLandQueryFailure::None || Query.Cells.Num() > 4096 ||
        Max.X < Min.X || Max.Y < Min.Y || int64(Max.X)-Min.X >= 32 || int64(Max.Y)-Min.Y >= 32 ||
        !FMath::IsFinite(Options.LineWidthCm) || Options.LineWidthCm <= 0)
    {
        if (FChunk* Old = Chunks.Find(Id)) { Pool[Old->PoolIndex]->SetVisibility(false); Chunks.Remove(Id); }
        return false;
    }
    FChunk* Existing = Chunks.Find(Id);
    const uint64 Digest = DigestQuery(Query, Options);
    if (Existing && Existing->Digest == Digest && Existing->Min == Min && Existing->Max == Max &&
        Existing->GridToWorld.Equals(Transform)) return true;
    if (!Existing && Chunks.Num() >= 64) return false;
    FChunk& Chunk = Chunks.FindOrAdd(Id);
    if (Chunk.PoolIndex == INDEX_NONE)
    {
        TSet<int32> Used;
        for (const auto& Pair : Chunks) if (Pair.Value.PoolIndex != INDEX_NONE) Used.Add(Pair.Value.PoolIndex);
        for (int32 I=0; I<Pool.Num(); ++I) if (!Used.Contains(I)) { Chunk.PoolIndex=I; break; }
        if (Chunk.PoolIndex == INDEX_NONE)
        {
            auto* Mesh = NewObject<UProceduralMeshComponent>(this);
            Mesh->SetupAttachment(GetRootComponent()); Mesh->SetAbsolute(true,true,true);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetCanEverAffectNavigation(false); Mesh->SetCastShadow(false);
            Mesh->SetTranslucentSortPriority(-10); Mesh->SetCullDistance(150000.f);
            Mesh->RegisterComponent(); Chunk.PoolIndex = Pool.Add(Mesh);
        }
    }
    if(Chunk.Query.StateRevision!=Query.StateRevision) Pool[Chunk.PoolIndex]->SetVisibility(false);
    Chunk.Query = Query; Chunk.Min = Min; Chunk.Max = Max; Chunk.GridToWorld = Transform;
    Chunk.Options = Options; Chunk.Digest = Digest; Chunk.bDirty = true;
    if(LastRebuildFrame!=GFrameCounter) { BuildQueue.Remove(Id);Rebuild(Chunk); }
    if (Chunk.bDirty) { BuildQueue.AddUnique(Id);SetActorTickEnabled(true); }
    return true;
}
void AHansaLandOverlayRenderer::RetainChunks(TConstArrayView<FIntPoint> Ids)
{
    for (auto It = Chunks.CreateIterator(); It; ++It)
        if (!Ids.Contains(It.Key())) { Pool[It.Value().PoolIndex]->SetVisibility(false); It.RemoveCurrent(); }
    BuildQueue.RemoveAll([&](FIntPoint Id){return !Chunks.Contains(Id);});
    if (Chunks.IsEmpty()) SetActorTickEnabled(false);
}
void AHansaLandOverlayRenderer::Clear() { RetainChunks({}); }
bool AHansaLandOverlayRenderer::IsDisplayingCity(const FString& CityId) const
{
    if (IsHidden()) return false;
    for (const auto& Pair : Chunks)
        if (Pair.Value.Query.CityId.ToString()==CityId && Pair.Value.Triangles>0 && Pool[Pair.Value.PoolIndex]->IsVisible()) return true;
    return false;
}
FHansaLandOverlayStats AHansaLandOverlayRenderer::GetStats() const
{
    FHansaLandOverlayStats S; S.ActiveChunks=Chunks.Num(); S.PooledComponents=Pool.Num(); S.Rebuilds=RebuildCount;
    for (const auto& Pair : Chunks) { S.Triangles+=Pair.Value.Triangles; S.MissingTerrainSamples+=Pair.Value.Missing; S.PendingChunks+=Pair.Value.bDirty?1:0; }
    return S;
}

void AHansaLandOverlayRenderer::Rebuild(FChunk& Chunk)
{
    const auto Geometry = Build(Chunk.Query, Chunk.Min, Chunk.Max, Chunk.Options.Mode, Chunk.Options.SelectedCell);
    auto* Component = Pool[Chunk.PoolIndex].Get();
    Component->ClearAllMeshSections();
    Chunk.Triangles=0; Chunk.Missing=0; Chunk.bDirty=false;
    ++RebuildCount;LastRebuildFrame=GFrameCounter;
    if (!Geometry.bValid) { Component->SetVisibility(false); return; }
    // Quantized sub-cell sample cache is shared by fills, hatches and outlines.
    TMap<FIntPoint, TOptional<FVector>> Heights;
    auto Ground = [&](FVector2D Grid, double Lift) -> TOptional<FVector>
    {
        const FIntPoint Key(FMath::RoundToInt(Grid.X*4096), FMath::RoundToInt(Grid.Y*4096));
        if (!Heights.Contains(Key))
        {
            const FVector XY = Chunk.GridToWorld.TransformPosition(FVector(Grid.X,Grid.Y,0));
            FHitResult Hit;
            if (Hansa::Game::TerrainPlacement::Trace(GetWorld(), XY+FVector(0,0,1000000), XY-FVector(0,0,1000000), Hit))
                Heights.Add(Key, FVector(XY.X,XY.Y,Hit.ImpactPoint.Z));
            else { Heights.Add(Key, TOptional<FVector>()); ++Chunk.Missing; }
        }
        const auto& Sample = Heights[Key];
        return Sample ? TOptional<FVector>(Sample.GetValue()+FVector(0,0,Lift)) : TOptional<FVector>();
    };
    FMesh Meshes[4]; // fill, hatch, backed edge, selected edge
    auto Quad = [&](FMesh& Mesh, FVector2D A, FVector2D B, FVector2D C, FVector2D D, FLinearColor Color, double Lift)
    {
        const auto PA=Ground(A,Lift), PB=Ground(B,Lift), PC=Ground(C,Lift), PD=Ground(D,Lift);
        if (PA && PB && PC && PD) Mesh.Quad(*PA,*PB,*PC,*PD,Color);
    };
    const double CellSize = Chunk.GridToWorld.GetScale3D().X;
    const double Width = FMath::Clamp(double(Chunk.Options.LineWidthCm)/FMath::Max(1.,CellSize), .004, .2);
    auto Ribbon = [&](FMesh& Mesh, TConstArrayView<FVector2D> Points, bool bClosed,
        double W, FLinearColor Color, double Lift, int32 Pattern)
    {
        // One cross-section shared by both adjacent strips. The shader draws the
        // dark backing, colored core and continuous pattern in a single surface.
        const auto Sections=BuildRibbon(Points,bClosed,W);
        int32 Previous=INDEX_NONE;
        for (const auto& Section:Sections)
        {
            const auto Left=Ground(Section.Left,Lift),Right=Ground(Section.Right,Lift);
            if (!Left || !Right) { Previous=INDEX_NONE; continue; }
            const int32 N=Mesh.Vertices.Num();
            Mesh.Vertices.Append({*Left,*Right}); Mesh.Colors.Append({Color,Color});
            Mesh.UVs.Append({FVector2D(Section.Phase,0),FVector2D(Section.Phase,1)});
            Mesh.Styles.Append({FVector2D(Pattern,0),FVector2D(Pattern,0)});
            if (Previous!=INDEX_NONE) Mesh.Indices.Append({Previous,N,N+1,Previous,N+1,Previous+1});
            Previous=N;
        }
    };
    TSet<FIntVector> DrawnEdges;
    for (const FRegion& Region : Geometry.Regions)
    {
        const ESurface Surface=Region.Style.Surface;
        FLinearColor Fill=Token(EHansaUiColorToken::ProsperityTeal,.38f);
        FLinearColor Edge=Token(EHansaUiColorToken::Chalk);
        int32 Pattern=0;
        if (Surface==ESurface::Conditional) { Fill=Token(EHansaUiColorToken::WarningAmber,.24f); Edge=Token(EHansaUiColorToken::WarningAmber); Pattern=1; }
        if (Surface==ESurface::Restricted) { Fill=Token(EHansaUiColorToken::Oxblood,.24f); Edge=Token(EHansaUiColorToken::Oxblood); Pattern=1; }
        if (Surface==ESurface::Unknown) { Fill=Token(EHansaUiColorToken::BalticNavy,.15f); Edge=Token(EHansaUiColorToken::Chalk,.65f); Pattern=2; }
        if (Surface==ESurface::Ownership) { Fill=Token(EHansaUiColorToken::HarborSlate,.20f); Edge=Token(EHansaUiColorToken::Linen,.9f); }
        if (Chunk.Options.bHighContrast) { Fill.A*=1.5f; Edge.A=1; }
        for (FIntPoint Cell : Region.Cells)
        {
            const FVector2D P(Cell);
            for (int32 X=0; X<2; ++X) for (int32 Y=0; Y<2; ++Y)
            {
                const FVector2D A=P+FVector2D(X*.5,Y*.5);
                Quad(Meshes[0],A,A+FVector2D(.5,0),A+FVector2D(.5,.5),A+FVector2D(0,.5),Fill,2);
            }
            if (Surface!=ESurface::Permitted && ((Cell.X+Cell.Y)%3==0))
            {
                FLinearColor Hatch=Edge; Hatch.A=.38f;
                const bool Reverse=Surface==ESurface::Ownership && (Region.Style.Owner%2==0);
                const FVector2D Slash[]={P+FVector2D(.2,Reverse?.8:.2),P+FVector2D(.8,Reverse?.2:.8)};
                Ribbon(Meshes[1],Slash,false,Width,Hatch,3,0);
                if (Surface==ESurface::Ownership && Region.Style.Owner%4>=2)
                {
                    const FVector2D Bar[]={P+FVector2D(.2,.5),P+FVector2D(.8,.5)};
                    Ribbon(Meshes[1],Bar,false,Width,Hatch,3,0);
                }
            }
        }
        for (const FBoundary& Boundary : Region.Boundaries)
        {
            TArray<FVector2D> Path;
            auto Flush=[&]
            {
                if (Path.Num()>1) Ribbon(Meshes[2],Path,Path[0].Equals(Path.Last()),Width*2.2,Edge,5,Pattern);
                Path.Reset();
            };
            for (int32 I=1; I<Boundary.Points.Num(); ++I)
            {
                const FIntPoint A=Boundary.Points[I-1],B=Boundary.Points[I];
                const FIntPoint Step(FMath::Sign(B.X-A.X),FMath::Sign(B.Y-A.Y));
                for (FIntPoint P=A;P!=B;P+=Step)
                {
                    const FIntPoint Q=P+Step;
                    const FIntVector Key(FMath::Min(P.X,Q.X),FMath::Min(P.Y,Q.Y),Step.X!=0?0:1);
                    if (DrawnEdges.Contains(Key)) { Flush(); continue; }
                    DrawnEdges.Add(Key);
                    if (Path.IsEmpty()) Path.Add(FVector2D(P));
                    Path.Add(FVector2D(Q));
                }
            }
            Flush();
        }
        // Selection is a stable cell footprint, never a partially highlighted cache chunk.
        if (Chunk.Options.SelectedCell && Region.Cells.Contains(*Chunk.Options.SelectedCell))
        {
            const FVector2D P(*Chunk.Options.SelectedCell);
            const FVector2D Corners[]={P,P+FVector2D(1,0),P+FVector2D(1,1),P+FVector2D(0,1)};
            Ribbon(Meshes[3],Corners,true,Width*2.8,Token(EHansaUiColorToken::Brass),7,0);
        }
    }
    if(Chunk.Options.SelectionBoundaries.IsValid())
    {
        // Clip original legal contours to the visible core; never outline the
        // camera chunk or a coarse summary cell as if it were a parcel boundary.
        const FVector2D Min(Chunk.Min),Max(Chunk.Max+FIntPoint(1,1));
        for(const auto& Boundary:*Chunk.Options.SelectionBoundaries)
        {
            TArray<FVector2D> Path;
            auto Flush=[&]{if(Path.Num()>1)Ribbon(Meshes[3],Path,Path[0].Equals(Path.Last()),Width*2.8,Token(EHansaUiColorToken::Brass),7,0);Path.Reset();};
            for(int32 I=1;I<Boundary.Points.Num();++I)
            {
                const FVector2D A=FVector2D(Boundary.Points[I-1])/Chunk.Options.SurveyStride;
                const FVector2D B=FVector2D(Boundary.Points[I])/Chunk.Options.SurveyStride;
                const FVector2D D=B-A;double T0=0,T1=1;
                bool Visible=true;
                for(int32 Axis=0;Axis<2;++Axis)
                {
                    if(FMath::Abs(D[Axis])<1.e-8) { if(A[Axis]<Min[Axis] || A[Axis]>Max[Axis])Visible=false; }
                    else { const double U=(Min[Axis]-A[Axis])/D[Axis],V=(Max[Axis]-A[Axis])/D[Axis];T0=FMath::Max(T0,FMath::Min(U,V));T1=FMath::Min(T1,FMath::Max(U,V)); }
                }
                if(!Visible || T1<=T0) { Flush();continue; }
                const FVector2D Start=A+D*T0,End=A+D*T1;
                if(!Path.IsEmpty() && !Path.Last().Equals(Start))Flush();
                if(Path.IsEmpty())Path.Add(Start);Path.Add(End);
            }
            Flush();
        }
    }
    for (int32 I=0; I<4; ++I)
    {
        auto& Mesh=Meshes[I]; Chunk.Triangles+=Mesh.Indices.Num()/3;
        if (Mesh.Indices.IsEmpty()) continue;
        Component->CreateMeshSection_LinearColor(I,Mesh.Vertices,Mesh.Indices,{},Mesh.UVs,Mesh.Styles,{}, {},Mesh.Colors,{},false,false);
        Component->SetMaterial(I,I==0?FillMaterial.Get():RibbonMaterial.Get());
    }
    Component->SetVisibility(Chunk.Triangles>0);
    // No repeated mesh rebuild while waiting for terrain: a stream-in event retries the chunk.
}
