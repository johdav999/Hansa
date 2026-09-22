#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
namespace Hansa::UI {
// Native rules and fill, with separately generated, un-stretched corner artwork.
class HANSA_API SHansaReferenceFrame final : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SHansaReferenceFrame):_Dark(true),_Padding(8.f){}
  SLATE_ARGUMENT(bool,Dark)
  SLATE_ARGUMENT(FMargin,Padding)
  SLATE_DEFAULT_SLOT(FArguments,Content)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
 bool bDark=true;
};
}

