#include "UI/HansaTradeMapGeometry.h"

namespace Hansa::UI::TradeGeometry
{
FVector2D FCamera::ChartSize(FVector2D Size) const
{
 const double Width = FMath::Max(1., FMath::Min(Size.X - 48., (Size.Y - (Size.Y<240?16.:48.)) * 1.70));
 return FVector2D(Width, Width / 1.70) * Zoom;
}
FVector2D FCamera::Point(FVector2D P, FVector2D Size) const { return (P - FVector2D(.5,.5)) * ChartSize(Size) + Size * .5 + Pan; }
void FCamera::Move(FVector2D Delta, FVector2D Size)
{
 Pan += Delta;
 const auto Limit = ChartSize(Size) * .5;
 Pan.X = FMath::Clamp(Pan.X, -Limit.X, Limit.X); Pan.Y = FMath::Clamp(Pan.Y, -Limit.Y, Limit.Y);
}
void FCamera::ZoomAt(double Delta, FVector2D Anchor, FVector2D Size)
{
 const double Previous = Zoom;
 Zoom = FMath::Clamp(Zoom + Delta, 1., 12.);
 Pan = Anchor - Size * .5 - (Anchor - Size * .5 - Pan) * (Zoom / Previous);
 Move(FVector2D::ZeroVector, Size);
}
void FCamera::Reveal(FVector2D P, FVector2D Size)
{
 const auto Pixel = Point(P,Size);
 if(Pixel.X < 56 || Pixel.Y < 56 || Pixel.X > Size.X - 56 || Pixel.Y > Size.Y - 56)
  Move(Size * .5 - Pixel,Size);
}
TArray<FLabel> PlaceLabels(TArray<FLabelInput> Inputs, FVector2D Size)
{
 Inputs.Sort([](const auto& A,const auto& B){ return A.Priority != B.Priority ? A.Priority > B.Priority : A.Id.LexicalLess(B.Id); });
 TArray<FLabel> Result;
 for(const auto& Input:Inputs)
 {
  if(Input.Point.X < 0 || Input.Point.X > Size.X || Input.Point.Y < 0 || Input.Point.Y > Size.Y) continue;
  const auto S = Input.Size;
  const FVector2D Offsets[] = {{16,-S.Y/2},{-16-S.X,-S.Y/2},{-S.X/2,-16-S.Y},{-S.X/2,16},{16,16},{-16-S.X,-16-S.Y},{16,-16-S.Y},{-16-S.X,16}};
  for(const auto& Offset:Offsets)
  {
   auto P=Input.Point+Offset;
   P.X=FMath::Clamp(P.X,4.,FMath::Max(4.,Size.X-S.X-4)); P.Y=FMath::Clamp(P.Y,4.,FMath::Max(4.,Size.Y-S.Y-4));
   FSlateRect Rect(P.X,P.Y,P.X+S.X,P.Y+S.Y);
   if(Result.ContainsByPredicate([&](const auto& Other){return FSlateRect::DoRectanglesIntersect(Rect,Other.Bounds);}))continue;
   // Never hide another city's functional marker under a label.
   if(Inputs.ContainsByPredicate([&](const auto& Other){return Rect.ContainsPoint(Other.Point);}))continue;
   Result.Add({Input.Id,Rect}); break;
  }
 }
 return Result;
}
FName SpatialNeighbor(const TArray<TPair<FName,FVector2D>>& Points,FName Origin,FVector2D Direction)
{
 const auto* Start=Points.FindByPredicate([&](const auto& P){return P.Key==Origin;});
 if(!Start)return Points.IsEmpty()?NAME_None:Points[0].Key;
 FName Best; double Score=TNumericLimits<double>::Max();
 for(const auto& Candidate:Points)
 {
  const auto D=Candidate.Value-Start->Value; const double Forward=FVector2D::DotProduct(D,Direction);
  if(Forward<=0)continue;
  const double Lateral=FMath::Abs(D.X*Direction.Y-D.Y*Direction.X);
  const double Value=D.Size()+Lateral*2.;
  if(Value<Score||(Value==Score&&Candidate.Key.LexicalLess(Best))){Best=Candidate.Key;Score=Value;}
 }
 return Best;
}
}
