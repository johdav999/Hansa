#include "World/HansaLandSurveyView.h"
#include "World/HansaTerrainPlacement.h"
#include "GameFramework/PlayerController.h"

namespace Hansa::Game::LandOverlay
{
using namespace Hansa::Simulation;
void FSurveyView::Reset() { Published={}; Pending={}; PendingPage=0; }
bool FSurveyView::AcceptPage(int32 Page,const FHansaLandQueryResult& Reply)
{
    if(Reply.Failure!=EHansaLandQueryFailure::None || Reply.SurveyPages<1 || Reply.SurveyPages>256 ||
        Reply.Cells.Num()>256 || Page!=PendingPage) return false;
    if(Page==0)
    {
        if(IsReady() && Published.StateRevision==Reply.StateRevision && Published.ViewerHouseId==Reply.ViewerHouseId && Published.CityId==Reply.CityId)
            return true;
        // A changed permission snapshot invalidates old colors immediately.
        Published={}; Pending=Reply;
    }
    else
    {
        if(Reply.StateRevision!=Pending.StateRevision || Reply.ViewerHouseId!=Pending.ViewerHouseId ||
            Reply.CityId!=Pending.CityId || Reply.SurveyPages!=Pending.SurveyPages)
        { Reset();return false; }
        Pending.Cells.Append(Reply.Cells);
    }
    if(++PendingPage==Reply.SurveyPages) { Published=MoveTemp(Pending); Pending={};PendingPage=0;return true; }
    return false;
}
FHansaLandQueryResult FSurveyView::Extract(FIntPoint Min,FIntPoint Max,int32 Stride) const
{
    FHansaLandQueryResult R;
    if(!IsReady() || Stride<1 || Stride>128 || Max.X<Min.X || Max.Y<Min.Y || int64(Max.X)-Min.X>=32 || int64(Max.Y)-Min.Y>=32)
    { R.Failure=EHansaLandQueryFailure::InvalidBounds;return R; }
    R.CityId=Published.CityId;R.ViewerHouseId=Published.ViewerHouseId;R.StateRevision=Published.StateRevision;
    R.BoundsMin={Min.X,Min.Y};R.BoundsMax={Max.X,Max.Y};
    const int32 Height=Max.Y-Min.Y+1;
    for(int32 X=Min.X;X<=Max.X;++X) for(int32 Y=Min.Y;Y<=Max.Y;++Y)
    { FHansaLandCellView C;C.Coordinate={X,Y};R.Cells.Add(C); }
    TArray<int32> Counts;Counts.AddZeroed(R.Cells.Num());
    TBitArray<> Mixed(false,R.Cells.Num());
    for(const auto& Run:Published.Cells)
    {
        const int32 X=FMath::FloorToInt(double(Run.Coordinate.X)/Stride);
        if(X<Min.X)continue;
        if(X>Max.X)break;
        for(int32 Y=FMath::Max(Min.Y,FMath::FloorToInt(double(Run.Coordinate.Y)/Stride));
            Y<=FMath::Min(Max.Y,FMath::FloorToInt(double(Run.Coordinate.Y+Run.RunLength-1)/Stride));++Y)
        {
            const int32 I=(X-Min.X)*Height+Y-Min.Y;
            auto& C=R.Cells[I];
            if(Counts[I]==0) { C=Run;C.Coordinate={X,Y};C.RunLength=1; }
            else if(C.bSurveyKnown!=Run.bSurveyKnown || C.RecordedOwnerId!=Run.RecordedOwnerId || C.Terrain!=Run.Terrain ||
                C.Access!=Run.Access || C.bProtected!=Run.bProtected || C.OccupyingBuildingId!=Run.OccupyingBuildingId || C.RegionId!=Run.RegionId) Mixed[I]=true;
            Counts[I]+=FMath::Min((Y+1)*Stride,Run.Coordinate.Y+Run.RunLength)-FMath::Max(Y*Stride,Run.Coordinate.Y);
        }
    }
    for(int32 I=0;I<R.Cells.Num();++I) if(Mixed[I] || Counts[I]!=Stride*Stride)
    { const auto P=R.Cells[I].Coordinate;R.Cells[I]={};R.Cells[I].Coordinate=P; }
    for(auto& C:R.Cells)
        C.bOutsideSurvey=int64(C.Coordinate.X+1)*Stride<=Published.BoundsMin.X || int64(C.Coordinate.Y+1)*Stride<=Published.BoundsMin.Y ||
            int64(C.Coordinate.X)*Stride>Published.BoundsMax.X || int64(C.Coordinate.Y)*Stride>Published.BoundsMax.Y;
    return R;
}
int32 FSurveyView::RegionAt(FIntPoint Cell) const
{
    for(const auto& R:Published.Cells)
        if(R.Coordinate.X==Cell.X && Cell.Y>=R.Coordinate.Y && Cell.Y<R.Coordinate.Y+R.RunLength) return R.RegionId;
    return 0;
}
FBox2D FSurveyView::RegionBounds(int32 Region) const
{
    FBox2D Bounds(ForceInit);
    if(Region>0) for(const auto& R:Published.Cells) if(R.RegionId==Region)
    { Bounds+=FVector2D(R.Coordinate.X,R.Coordinate.Y); Bounds+=FVector2D(R.Coordinate.X+1,R.Coordinate.Y+R.RunLength); }
    return Bounds;
}
TArray<FVector> CameraGroundFootprint(APlayerController* PC,double Height)
{
    TArray<FVector> Points;
    if(!PC)return Points;
    int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<=0 || H<=0)return Points;
    // Edge midpoints make the conservative coverage less sensitive to ridges.
    for(const FVector2D UV:{FVector2D(0,0),FVector2D(.5,0),FVector2D(1,0),FVector2D(1,.5),
        FVector2D(1,1),FVector2D(.5,1),FVector2D(0,1),FVector2D(0,.5)})
    {
        FVector O,D;if(!PC->DeprojectScreenPositionToWorld(UV.X*W,UV.Y*H,O,D))continue;
        const double Distance=D.Z<-.001?FMath::Clamp((Height-O.Z)/D.Z,0.,500000.):500000.;
        FHitResult Hit;
        const FVector End=O+D*Distance;
        Points.Add(TerrainPlacement::Trace(PC->GetWorld(),O,O+D*FMath::Min(Distance+20000.,500000.),Hit)?Hit.ImpactPoint:End);
    }
    return Points;
}
TArray<FIntPoint> VisibleSurveyChunks(TConstArrayView<FVector> Points,const FTransform& Grid,
    FIntPoint SurveyMin,FIntPoint SurveyMax,FIntPoint Focus,int32 Size)
{
    TArray<FIntPoint> Result;FBox2D Bounds(ForceInit);
    if(Size<1 || Points.IsEmpty())return Result;
    for(const auto& P:Points)Bounds+=FVector2D(Grid.InverseTransformPosition(P));
    const int32 MinX=FMath::Max(FMath::FloorToInt(Bounds.Min.X/Size)-1,FMath::FloorToInt(double(SurveyMin.X)/Size));
    const int32 MinY=FMath::Max(FMath::FloorToInt(Bounds.Min.Y/Size)-1,FMath::FloorToInt(double(SurveyMin.Y)/Size));
    const int32 MaxX=FMath::Min(FMath::FloorToInt(Bounds.Max.X/Size)+1,FMath::FloorToInt(double(SurveyMax.X)/Size));
    const int32 MaxY=FMath::Min(FMath::FloorToInt(Bounds.Max.Y/Size)+1,FMath::FloorToInt(double(SurveyMax.Y)/Size));
    // Survey queries constrain each side to 2048; no unbounded world enumeration.
    if(int64(MaxX)-MinX>140 || int64(MaxY)-MinY>140)return Result;
    for(int32 X=MinX;X<=MaxX;++X)for(int32 Y=MinY;Y<=MaxY;++Y)Result.Add({X,Y});
    Result.Sort([&](FIntPoint A,FIntPoint B){return (FVector2D(A*Size)+FVector2D(Size*.5)-FVector2D(Focus)).SizeSquared()<
        (FVector2D(B*Size)+FVector2D(Size*.5)-FVector2D(Focus)).SizeSquared();});
    return Result;
}
FVector2D WorldToLandMap(FVector2D W,FVector2D C,double S){return FVector2D(.5+(W.Y-C.Y)/S,.5-(W.X-C.X)/S);}
FVector2D LandMapToWorld(FVector2D M,FVector2D C,double S){return FVector2D(C.X+(.5-M.Y)*S,C.Y+(M.X-.5)*S);}
}
