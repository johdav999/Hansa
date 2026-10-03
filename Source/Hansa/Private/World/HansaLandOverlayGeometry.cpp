#include "World/HansaLandOverlayGeometry.h"

namespace Hansa::Game::LandOverlay
{
using namespace Hansa::Simulation;

TArray<FRibbonSection> BuildRibbon(TConstArrayView<FVector2D> Input, bool bClosed,
    double Width, double MaximumStep)
{
    TArray<FRibbonSection> Result;
    if (!FMath::IsFinite(Width) || Width <= 0 || !FMath::IsFinite(MaximumStep) || MaximumStep <= 0 || Input.Num()>8192) return Result;
    TArray<FVector2D> Points;
    for (const auto& P:Input)
    {
        if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y)) return {};
        if (Points.IsEmpty() || !P.Equals(Points.Last(),1.e-8)) Points.Add(P);
    }
    if (bClosed && Points.Num()>1 && Points[0].Equals(Points.Last(),1.e-8)) Points.Pop();
    if (Points.Num() < (bClosed?3:2)) return Result;
    auto Normal=[](FVector2D D){D.Normalize();return FVector2D(-D.Y,D.X);};
    TArray<FVector2D> Offsets;
    for (int32 I=0;I<Points.Num();++I)
    {
        const FVector2D Previous=Points[I]-Points[(I+Points.Num()-1)%Points.Num()];
        const FVector2D Next=Points[(I+1)%Points.Num()]-Points[I];
        FVector2D Offset;
        if (!bClosed && I==0) Offset=Normal(Next)*Width*.5;
        else if (!bClosed && I==Points.Num()-1) Offset=Normal(Previous)*Width*.5;
        else
        {
            const FVector2D A=Normal(Previous), B=Normal(Next);
            const FVector2D Miter=(A+B).GetSafeNormal();
            // Cell contours turn by 90 degrees. The limit also keeps malformed
            // near-reversals finite rather than emitting giant spikes.
            const double Denominator=FVector2D::DotProduct(Miter,B);
            Offset=Denominator>.25 ? Miter*FMath::Min(Width*.5/Denominator,Width) : B*Width*.5;
        }
        Offsets.Add(Offset);
    }
    const int32 Segments=bClosed?Points.Num():Points.Num()-1;
    auto Append=[&](FVector2D P,FVector2D Offset){Result.Add({P-Offset,P+Offset,P.X+P.Y});};
    for (int32 I=0;I<Segments;++I)
    {
        const int32 J=(I+1)%Points.Num();
        const FVector2D Delta=Points[J]-Points[I];
        const double Length=Delta.Size();
        if (Length/MaximumStep>32768 || Result.Num()+Length/MaximumStep>65536) return {};
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(Length/MaximumStep));
        const FVector2D Straight=Normal(Delta)*Width*.5;
        Append(Points[I],Offsets[I]);
        for (int32 Step=1;Step<Steps;++Step) Append(Points[I]+Delta*(double(Step)/Steps),Straight);
    }
    const int32 Last=bClosed?0:Points.Num()-1;
    Append(Points[Last],Offsets[Last]);
    return Result;
}

float RibbonWidthForView(float Distance,float Fov,int32 ViewportWidth,float Pitch,float Pixels)
{
    if (!FMath::IsFinite(Distance) || !FMath::IsFinite(Fov) || !FMath::IsFinite(Pitch) ||
        !FMath::IsFinite(Pixels) || Distance<=0 || Fov<=0 || Fov>=179 || ViewportWidth<=0 || Pixels<=0) return 12.f;
    const float Foreshortening=FMath::Sqrt(FMath::Max(.25f,FMath::Abs(FMath::Sin(FMath::DegreesToRadians(Pitch)))));
    const float CmPerPixel=2.f*Distance*FMath::Tan(FMath::DegreesToRadians(Fov*.5f))/ViewportWidth;
    return FMath::Clamp(FMath::RoundToFloat(CmPerPixel*Pixels/Foreshortening/2.f)*2.f,4.f,80.f);
}

namespace
{
bool Inside(FIntPoint P, FIntPoint Min, FIntPoint Max)
{
    return P.X >= Min.X && P.Y >= Min.Y && P.X <= Max.X && P.Y <= Max.Y;
}
struct FEdge { FIntPoint A, B; int32 Direction; };

TArray<FBoundary> JoinEdges(const TArray<FEdge>& Edges)
{
    TMap<FIntPoint, TArray<int32>> Starts;
    TSet<FIntPoint> Ends;
    for (int32 I = 0; I < Edges.Num(); ++I) { Starts.FindOrAdd(Edges[I].A).Add(I); Ends.Add(Edges[I].B); }
    TSet<int32> Used;
    TArray<FBoundary> Result;
    auto Trace = [&](int32 First)
    {
        FBoundary Path;
        int32 Current = First;
        Path.Points.Add(Edges[First].A);
        while (Current != INDEX_NONE && !Used.Contains(Current))
        {
            Used.Add(Current);
            const FEdge& Edge = Edges[Current];
            Path.Points.Add(Edge.B);
            if (Edge.B == Path.Points[0]) { Path.bClosed = true; break; }
            int32 Next = INDEX_NONE, Best = 5;
            if (const TArray<int32>* Candidates = Starts.Find(Edge.B))
                for (int32 Candidate : *Candidates)
                {
                    if (Used.Contains(Candidate)) continue;
                    const int32 Turn = (Edges[Candidate].Direction - Edge.Direction + 4) % 4;
                    const int32 Rank = Turn == 1 ? 0 : Turn == 0 ? 1 : Turn == 3 ? 2 : 3;
                    if (Rank < Best) { Best = Rank; Next = Candidate; }
                }
            Current = Next;
        }
        // Remove only exactly collinear corners. Holes retain their opposite winding.
        for (int32 I = Path.Points.Num() - 2; I > 0; --I)
        {
            const FIntPoint A = Path.Points[I] - Path.Points[I-1];
            const FIntPoint B = Path.Points[I+1] - Path.Points[I];
            if (int64(A.X)*B.Y == int64(A.Y)*B.X && int64(A.X)*B.X + int64(A.Y)*B.Y > 0)
                Path.Points.RemoveAt(I);
        }
        Result.Add(MoveTemp(Path));
    };
    // Open chains at the core boundary must be followed from their actual start.
    for (int32 I = 0; I < Edges.Num(); ++I)
        if (!Used.Contains(I) && !Ends.Contains(Edges[I].A)) Trace(I);
    for (int32 I = 0; I < Edges.Num(); ++I) if (!Used.Contains(I)) Trace(I);
    return Result;
}
}

TArray<FBoundary> SurveySelectionBoundaries(TConstArrayView<FHansaLandCellView> Runs,int32 RegionId)
{
    if(RegionId<=0)return {};
    TMap<int32,TArray<FIntPoint>> Rows;
    for(const auto& R:Runs) if(R.RegionId==RegionId)
    {
        auto& Row=Rows.FindOrAdd(R.Coordinate.X);
        if(!Row.IsEmpty() && Row.Last().Y==R.Coordinate.Y)Row.Last().Y+=R.RunLength;
        else Row.Add({R.Coordinate.Y,R.Coordinate.Y+R.RunLength});
    }
    TArray<FEdge> Edges;
    for(const auto& Row:Rows) for(const auto& Interval:Row.Value)
    {
        const int32 X=Row.Key, A=Interval.X,B=Interval.Y;
        Edges.Add({{X,A},{X+1,A},0});Edges.Add({{X+1,B},{X,B},2});
        for(int32 Side=0;Side<2;++Side)
        {
            const auto* Adjacent=Rows.Find(X+(Side?1:-1));int32 Cursor=A;
            auto Edge=[&](int32 From,int32 To)
            { if(To>From) Edges.Add(Side?FEdge{{X+1,From},{X+1,To},1}:FEdge{{X,To},{X,From},3}); };
            if(Adjacent)for(const auto& N:*Adjacent)
            {
                if(N.Y<=Cursor)continue;if(N.X>=B)break;
                Edge(Cursor,FMath::Min(B,N.X));Cursor=FMath::Max(Cursor,N.Y);if(Cursor>=B)break;
            }
            Edge(Cursor,B);
        }
    }
    // Stable iteration makes selection output independent of TMap hash order.
    Edges.Sort([](const FEdge& A,const FEdge& B){return A.A.X==B.A.X?(A.A.Y==B.A.Y?A.Direction<B.Direction:A.A.Y<B.A.Y):A.A.X<B.A.X;});
    return JoinEdges(Edges);
}

FStyleKey Classify(const FHansaLandCellView& Cell, EMode Mode)
{
    if (Mode == EMode::Off || Cell.bOutsideSurvey) return {ESurface::Hidden, 0};
    if (!Cell.bSurveyKnown) return {ESurface::Unknown, 0};
    if (Cell.Terrain == EHansaPlacementTerrain::Water || Cell.OccupyingBuildingId.IsValid())
        return {ESurface::Hidden, 0};
    if (Cell.bProtected) return {ESurface::Restricted, 0};
    if (Mode == EMode::Ownership)
        return Cell.RecordedOwnerId.IsValid() ? FStyleKey{ESurface::Ownership, Cell.RecordedOwnerId.GetValue()}
            : FStyleKey{ESurface::Unknown, 0};
    switch (Cell.Access)
    {
    case EHansaLandAccess::Permitted: return {ESurface::Permitted, 0};
    case EHansaLandAccess::Conditional: return {ESurface::Conditional, 0};
    case EHansaLandAccess::Denied: return {ESurface::Restricted, 0};
    default: return {ESurface::Unknown, 0};
    }
}

FGeometry Build(const FHansaLandQueryResult& Query, FIntPoint CoreMin, FIntPoint CoreMax,
    EMode Mode, TOptional<FIntPoint> SelectedCell)
{
    FGeometry Result;
    if (Query.Failure != EHansaLandQueryFailure::None || Query.Cells.Num() > 4096 ||
        CoreMax.X < CoreMin.X || CoreMax.Y < CoreMin.Y ||
        int64(CoreMax.X)-CoreMin.X >= 32 || int64(CoreMax.Y)-CoreMin.Y >= 32) return Result;
    TMap<FIntPoint, FStyleKey> Styles;
    TArray<FIntPoint> Ordered;
    for (const auto& Cell : Query.Cells)
    {
        const FIntPoint P(Cell.Coordinate.X, Cell.Coordinate.Y);
        if (Styles.Contains(P)) return Result;
        Styles.Add(P, Classify(Cell, Mode));
        if (Inside(P, CoreMin, CoreMax)) Ordered.Add(P);
    }
    Ordered.Sort([](FIntPoint A, FIntPoint B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
    TSet<FIntPoint> Visited;
    const FIntPoint Directions[] = {{0,-1},{1,0},{0,1},{-1,0}};
    for (FIntPoint Seed : Ordered)
    {
        if (Visited.Contains(Seed) || Styles[Seed].Surface == ESurface::Hidden) continue;
        FRegion Region;
        Region.Style = Styles[Seed];
        Region.Cells.Add(Seed);
        Visited.Add(Seed);
        TArray<FEdge> Edges;
        for (int32 I = 0; I < Region.Cells.Num(); ++I)
        {
            const FIntPoint P = Region.Cells[I];
            Region.bSelected |= SelectedCell.IsSet() && P == SelectedCell.GetValue();
            const FIntPoint Corners[] = {P, P+FIntPoint(1,0), P+FIntPoint(1,1), P+FIntPoint(0,1)};
            for (int32 D = 0; D < 4; ++D)
            {
                const FIntPoint Neighbor = P + Directions[D];
                const FStyleKey* NeighborStyle = Styles.Find(Neighbor);
                if (NeighborStyle && *NeighborStyle == Region.Style)
                {
                    if (Inside(Neighbor, CoreMin, CoreMax) && !Visited.Contains(Neighbor))
                    { Visited.Add(Neighbor); Region.Cells.Add(Neighbor); }
                }
                // Missing halo is not evidence of a region boundary.
                else if (NeighborStyle)
                    Edges.Add({Corners[D], Corners[(D+1)%4], D});
            }
        }
        Region.Cells.Sort([](FIntPoint A, FIntPoint B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
        Result.UnitEdgeCount += Edges.Num();
        Region.Boundaries = JoinEdges(Edges);
        Result.Regions.Add(MoveTemp(Region));
    }
    Result.bValid = true;
    return Result;
}
}
