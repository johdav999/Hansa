#include "UI/HansaRootHud.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "Placement/HansaRostockPlacement.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "World/HansaLandOverlayRenderer.h"
void AHansaRootHud::DrawHUD()
{
 Super::DrawHUD();
 if(!Canvas||!PlayerOwner||ViewedCity!=TEXT("City.Rostock")||(TradeMapPresentationModel&&TradeMapPresentationModel->GetSnapshot().bOpen))return;
 for(TActorIterator<AHansaLandOverlayRenderer> It(GetWorld());It;++It)if(It->IsDisplayingCity(ViewedCity.ToString()))return;
 const auto Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
 auto Project=[&](double X,double Y,FVector2D& Screen){return PlayerOwner->ProjectWorldLocationToScreen(Hansa::Simulation::RostockPlacement::CellCenter(0,0,120)+FVector((X-.5)*400,(Y-.5)*400,0),Screen);};
 auto Line=[&](double X1,double Y1,double X2,double Y2,FLinearColor Color,float Width=2.f){FVector2D A,B;if(Project(X1,Y1,A)&&Project(X2,Y2,B))DrawLine(A.X,A.Y,B.X,B.Y,Color,Width);};
 static const auto Survey=Hansa::Simulation::RostockPlacement::CreateMap();
 for(const auto& L:WorldConstruction.Plots)
 {
  const double X=L.BoundsMin.X,Y=L.BoundsMin.Y,R=L.BoundsMax.X+1,T=L.BoundsMax.Y+1;
  Line(X,Y,R,Y,Brass);Line(R,Y,R,T,Brass);Line(R,T,X,T,Brass);Line(X,T,X,Y,Brass);
  // A narrow outside hatch makes the autonomous side explicit without covering the preview.
  for(double I=X;I<FMath::Min(R,X+128);I+=1){Line(I,Y-.4,I+.4,Y,Brass,1);Line(I,T,I+.4,T+.4,Brass,1);}
  for(const auto C:L.OccupiedCells){if(Survey.Cells.ContainsByPredicate([&](const auto& Cell){return Cell.Coordinate.X==C.X&&Cell.Coordinate.Y==C.Y&&Cell.bBlocked;}))continue;Line(C.X,C.Y,C.X+1,C.Y+1,Brass);Line(C.X+1,C.Y,C.X,C.Y+1,Brass);}
  if(!L.bActive)Line(X,Y,R,T,Brass,3);
  FVector2D Label;if(Project(X,T,Label))
  {
   const FString Text=FString::Printf(TEXT("Lease %llu | House %llu | %s | outside: autonomous city"),L.Id,L.OwnerId,L.bActive?TEXT("Active"):TEXT("Suspended"));
   float TextWidth=0,TextHeight=0;GetTextSize(Text,TextWidth,TextHeight,GEngine->GetSmallFont());
   const FVector2D Anchor=Label;
   Label.X=FMath::Clamp(Label.X,324.,FMath::Max(324.,double(Canvas->ClipX)-TextWidth-8.));
   Label.Y=FMath::Clamp(Label.Y,140.,FMath::Max(140.,double(Canvas->ClipY)*.5-TextHeight-8.));
   if(!Label.Equals(Anchor,1.))DrawLine(Anchor.X,Anchor.Y,Label.X,Label.Y,Brass,1.f);
   DrawRect(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy),Label.X-4,Label.Y-3,TextWidth+8,TextHeight+6);
   DrawText(Text,Brass,Label.X,Label.Y,GEngine->GetSmallFont());
  }
 }
}
