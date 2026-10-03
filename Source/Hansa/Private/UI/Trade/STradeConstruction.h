#pragma once
#include "HansaTradeWorkspaceComponents.h"
namespace Hansa::UI
{
 class STradeLeaseDiagram; class STradeCostDossier;
 class STradeConstruction final : public STradeComponent
 {
 public:
  SLATE_BEGIN_ARGS(STradeConstruction){} SLATE_END_ARGS()
  void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
  void Refresh(const FHansaTradeConstruction&);
  TArray<FString> FocusOrder() const;
  TSharedPtr<SScrollBox> Scroll;
 private:
  TSharedPtr<SVerticalBox> Plots,Cards;
  TSharedPtr<STextBlock> Status,Detail,Locked,WorldStatus;
  TSharedPtr<STradeLeaseDiagram> Diagram; TSharedPtr<STradeCostDossier> Dossier;
  TSharedPtr<SHansaAction> Visit,Place;
  TMap<FString,TSharedPtr<SHansaAction>> PlotActions,CardActions;
  FHansaTradeConstruction View;
 };
}
