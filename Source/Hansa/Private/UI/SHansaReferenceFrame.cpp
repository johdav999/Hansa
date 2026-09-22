#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
namespace Hansa::UI {
void SHansaReferenceFrame::Construct(const FArguments& Args) {
 bDark=Args._Dark; ChildSlot.Padding(Args._Padding)[Args._Content.Widget];
 SetVisibility(EVisibility::SelfHitTestInvisible);
}
int32 SHansaReferenceFrame::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const {
 const FVector2D Size=G.GetLocalSize();
 const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 const auto Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
 const auto Fill=bDark?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy)*.65f:UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen);
 FLinearColor Opaque=Fill;Opaque.A=1;
 FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),White,ESlateDrawEffect::None,Opaque);
 auto Rule=[&](float Inset,FLinearColor Color){
  TArray<FVector2D> P={{Inset,Inset},{Size.X-Inset,Inset},{Size.X-Inset,Size.Y-Inset},{Inset,Size.Y-Inset},{Inset,Inset}};
  FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,1.f);
 };
 Rule(1,Brass); Rule(4,Brass*.55f);
 int32 Top=SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer+2,Style,Enabled);
 static TMap<FString,TSharedPtr<FSlateDynamicImageBrush>> Corners;
 const float Side=28.f;
 for(int32 I=0;I<4;++I){
  const int32 Pixels=FMath::CeilToInt(Side*G.GetAccumulatedLayoutTransform().GetScale());
  const int32 Density=Pixels<=28?28:Pixels<=40?40:56;
  FString Key=FString::Printf(TEXT("Corner%d--%d.png"),I,Density);
  auto& Brush=Corners.FindOrAdd(Key);
  if(!Brush)Brush=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/ReferenceHud/")+Key)),FVector2D(Density));
  const FVector2D Pos(I%2?Size.X-Side:0,I>=2?Size.Y-Side:0);
  FSlateDrawElement::MakeBox(Out,Top+1,G.ToPaintGeometry(FVector2D(Side),FSlateLayoutTransform(Pos)),Brush.Get());
 }
 return Top+1;
}
}

