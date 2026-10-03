#include "HansaTradeWorkspaceComponents.h"
#include "TradeArtwork.h"
#include "UI/SHansaReferenceFrame.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

#define LOCTEXT_NAMESPACE "HansaOrdersReference"
namespace Hansa::UI {
namespace {
const FSlateBrush* CharcoalArtwork(float Scale) {
 static TMap<int32,TSharedPtr<FSlateDynamicImageBrush>> Brushes;
 const int32 Size=Scale>1.1f?160:Scale<.9f?80:112;
 auto& B=Brushes.FindOrAdd(Size);
 if(!B)B=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/FString::Printf(TEXT("Hansa/UI/TradeOrders/charcoal-crate--%d.png"),Size))),FVector2D(Size));
 return B.Get();
}
}
void STradeOrders::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
 Initialize(In);
 const auto C=[](EHansaUiColorToken T){return UHansaUiStyleLibrary::GetColor(T);};
 const auto Linen=C(EHansaUiColorToken::Linen),Parchment=C(EHansaUiColorToken::Parchment),Brass=C(EHansaUiColorToken::Brass),Navy=C(EHansaUiColorToken::BalticNavy);
 CardBrush=FSlateRoundedBoxBrush(FLinearColor(Linen.R,Linen.G,Linen.B,.28f),3.f,FLinearColor(Brass.R,Brass.G,Brass.B,.55f),1.f);
 WarningBrush=FSlateRoundedBoxBrush(FLinearColor(Parchment.R,Parchment.G,Parchment.B,.5f),3.f,C(EHansaUiColorToken::WarningAmber),1.f);
 RuleBrush=FSlateColorBrush(FLinearColor(Brass.R,Brass.G,Brass.B,.4f));
 auto Dark=Navy*.65f;Dark.A=1;FooterBrush=FSlateColorBrush(Dark);
 OrderCaptionStyle=LightBodyStyle;OrderCaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences));
 auto CaptionFont=OrderCaptionStyle.Font;CaptionFont.Size=GetComponentFont(EHansaUiTypographyToken::Caption,Preferences).Size;
 OrderCaptionStyle.SetFont(CaptionFont).SetColorAndOpacity(C(EHansaUiColorToken::MutedInk));
 OrderTitleStyle=LightHeadingStyle;OrderValueStyle=LightHeadingStyle;
 OrderEditorHeadingStyle=OrderTitleStyle;auto EditorFont=OrderTitleStyle.Font;EditorFont.Size+=6;OrderEditorHeadingStyle.SetFont(EditorFont);
 StockBarStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(Linen.R*.8f,Linen.G*.8f,Linen.B*.8f,1),3.f,C(EHansaUiColorToken::MutedInk),1.f)).SetFillImage(FSlateRoundedBoxBrush(Brass,3.f));
 OrderRowStyle=LightRowStyle;OrderRowStyle.SetNormal(FSlateRoundedBoxBrush(Linen,3.f,Brass*.6f,1.f)).SetNormalPadding(FMargin(8,5)).SetPressedPadding(FMargin(8,5));
 SelectedOrderRowStyle=OrderRowStyle;SelectedOrderRowStyle.SetNormal(FSlateRoundedBoxBrush(Parchment,3.f,Brass,1.f));
 OrderSecondaryStyle=SecondaryButtonStyle;OrderSecondaryStyle.SetNormal(FSlateRoundedBoxBrush(Parchment,2.f,Brass*.6f,1.f));
 OrderPrimaryStyle=PrimaryButtonStyle;OrderPrimaryStyle.SetNormal(FSlateRoundedBoxBrush(Dark,2.f,Brass,1.f));
 OrderSelectedStyle=OrderPrimaryStyle;
 auto Text=[this](const FText& T,bool Heading=false)->TSharedRef<STextBlock>{return SNew(STextBlock).Text(T).TextStyle(Heading?&OrderTitleStyle:&LightBodyStyle).AutoWrapText(true);};
 auto Caption=[this](const FText& T)->TSharedRef<STextBlock>{return SNew(STextBlock).Text(T).TextStyle(&OrderCaptionStyle).AutoWrapText(true);};
 auto Icon=[](EUiGlyph G,float Size)->TSharedRef<SWidget>{return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SImage).Image(GetGeneratedIconBrush(G,int32(Size)))];};
 auto Rule=[this]()->TSharedRef<SWidget>{return SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)];};
 auto Numeric=[this](const TCHAR* Field,TSharedPtr<SEditableTextBox>& Input)->TSharedRef<SWidget>{
  const FString Prefix=TEXT("TradeMap.Orders.")+FString(Field);
  auto Minus=MakeControl(*(Prefix+TEXT(".Decrease")),LOCTEXT("Minus","−"),EHansaUiButtonStyle::Secondary);
  auto Plus=MakeControl(*(Prefix+TEXT(".Increase")),LOCTEXT("Plus","+"),EHansaUiButtonStyle::Secondary);
  for(auto Button:{Minus,Plus})Button->SetButtonStyle(&OrderSecondaryStyle);
  Minus->SetContent(SNew(STextBlock).Text(LOCTEXT("Minus","−")).TextStyle(&LightBodyStyle).Justification(ETextJustify::Center));Plus->SetContent(SNew(STextBlock).Text(LOCTEXT("Plus","+")).TextStyle(&LightBodyStyle).Justification(ETextJustify::Center));
  Minus->SetToolTipText(LOCTEXT("Decrease","Decrease this amount"));Plus->SetToolTipText(LOCTEXT("Increase","Increase this amount"));
  SAssignNew(Input,SEditableTextBox).Style(&RouteNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Data,Preferences)).Justification(ETextJustify::Center).MinDesiredWidth(72)
   .OnTextCommitted_Lambda([this,Key=FString(Field)](const FText& Value,ETextCommit::Type Commit){
    if(Commit==ETextCommit::OnCleared)return;
    if(auto* P=Model.Get())if(!P->SetStationOrderNumber(Key,Value.ToString())){
     const auto& S=P->GetSnapshot();const auto Field=Key==TEXT("Target")?TargetInput:Key==TEXT("Cap")?CapInput:BudgetInput;
     Field->SetText(FText::FromString(Key==TEXT("Target")?S.StationOrderTargetInput:Key==TEXT("Cap")?S.StationOrderCapInput:FString::Printf(TEXT("%lld"),S.StationOrderBudgetPfennig)));
     StationOrderFeedback->SetText(LOCTEXT("InvalidNumber","Enter a valid amount within the office's limits."));StationOrderFeedback->SetVisibility(EVisibility::Visible);
    }
   });
  MapWidget(Prefix+TEXT(".Value"),Input);
  const float ControlSize=40.f/FMath::Min(1.f,Preferences.UiScale);
  return SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(ControlSize).MinDesiredHeight(ControlSize)[Minus]]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SBox).MinDesiredHeight(ControlSize)[Input.ToSharedRef()]]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(ControlSize).MinDesiredHeight(ControlSize)[Plus]];
 };
 auto Row=[&,this](EUiGlyph Glyph,const FText& Label,const FText& Hint,const FText& Unit,const TCHAR* Field,TSharedPtr<SEditableTextBox>& Input,TSharedPtr<STextBlock>* LabelOut,TSharedPtr<STextBlock>* HintOut,TSharedPtr<STextBlock>* UnitOut=nullptr)->TSharedRef<SWidget>{
  auto LabelWidget=Text(Label,true),HintWidget=Caption(Hint);
  auto UnitWidget=Caption(Unit);if(UnitOut)*UnitOut=UnitWidget;
  if(LabelOut)*LabelOut=LabelWidget;if(HintOut)*HintOut=HintWidget;
  auto Controls=Numeric(Field,Input);
  return SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[Icon(Glyph,32)]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[LabelWidget]+SVerticalBox::Slot().AutoHeight()[HintWidget]]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride((Preferences.bLargeText?176:152)+80.f/FMath::Min(1.f,Preferences.UiScale)-80.f)[Controls]]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)[SNew(SBox).WidthOverride(Preferences.bLargeText?80:56)[UnitWidget]]]
   +SVerticalBox::Slot().AutoHeight()[Rule()];
 };
 SAssignNew(GoodPicker,SComboButton).ButtonStyle(&OrderRowStyle).HasDownArrow(false)
 .OnGetMenuContent_Lambda([this]()->TSharedRef<SWidget>{
  auto Choices=SNew(SVerticalBox);
  if(auto* P=Model.Get())for(const auto G:P->GetStationOrderGoods()){
   auto B=SNew(SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Secondary).Label(P->GetOrderGoodLabel(G))
    .OnClicked_Lambda([this,G]{if(auto* M=Model.Get())M->SelectStationOrderGood(FName(*G.ToString()));GoodPicker->SetIsOpen(false);return FReply::Handled();});
   MapWidget(TEXT("TradeMap.Orders.Choice.")+G.ToString(),B);
   Choices->AddSlot().AutoHeight().Padding(0,2)[B];
  }
  return SNew(SBox).WidthOverride(280).MaxDesiredHeight(360)[SNew(SScrollBox)+SScrollBox::Slot()[Choices]];
 }).ButtonContent()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(SBox).WidthOverride(112).HeightOverride(112).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)[SAssignNew(CommodityImage,SImage)]]
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[SAssignNew(GoodLabel,STextBlock).TextStyle(&OrderValueStyle).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight()[Caption(LOCTEXT("SelectCommodity","Select commodity"))]
   +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Rule()]
   +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Text(LOCTEXT("Stock","Office stock"))]+SHorizontalBox::Slot().AutoWidth()[SAssignNew(StockLabel,STextBlock).TextStyle(&LightBodyStyle)]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(SBox).HeightOverride(8)[SAssignNew(StockBar,SProgressBar).Style(&StockBarStyle).FillColorAndOpacity(FLinearColor::White)]]]
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)[Icon(EUiGlyph::Down,16)]];
 MapWidget(TEXT("TradeMap.Orders.Good"),GoodPicker);
 BuyAction=MakeControl(TEXT("TradeMap.Orders.Buy"),LOCTEXT("Buy","Buy"),EHansaUiButtonStyle::Primary);
 SellAction=MakeControl(TEXT("TradeMap.Orders.Sell"),LOCTEXT("Sell","Sell"),EHansaUiButtonStyle::Secondary);
 auto TargetRow=Row(EUiGlyph::Storage,LOCTEXT("Target","Target stock"),FText(),LOCTEXT("Units","units"),TEXT("Target"),TargetInput,&TargetLabel,&TargetHint);
 auto RateRow=Row(EUiGlyph::Loading,LOCTEXT("Rate","Maximum purchase rate"),FText(),FText(),TEXT("Cap"),CapInput,&RateLabel,&RateHint,&RateUnit);
 BudgetRow=Row(EUiGlyph::Coin,LOCTEXT("Budget","Purchase budget"),LOCTEXT("BudgetHelp","Total spending limit for this order."),LOCTEXT("Pfennig","pfennig"),TEXT("Budget"),BudgetInput,nullptr,nullptr);
 auto Form=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[GoodPicker.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,4)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[BuyAction.ToSharedRef()]+SHorizontalBox::Slot().FillWidth(1)[SellAction.ToSharedRef()]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SAssignNew(ModeHint,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(10,8))[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Instructions","Order instructions"),true)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Rule()]
   +SVerticalBox::Slot().AutoHeight()[TargetRow]
   +SVerticalBox::Slot().AutoHeight()[RateRow]
   +SVerticalBox::Slot().AutoHeight()[BudgetRow.ToSharedRef()]]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(MarketCard,SBorder).BorderImage(&WarningBrush).Padding(10)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Icon(EUiGlyph::Loading,24)]+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(ReportHeading,STextBlock).TextStyle(&OrderTitleStyle).AutoWrapText(true)]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Rule()]
   +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(2)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Caption(LOCTEXT("Quote","Last reported price"))]+SVerticalBox::Slot().AutoHeight()[SAssignNew(PriceLabel,STextBlock).TextStyle(&OrderValueStyle).AutoWrapText(true)]]
    +SHorizontalBox::Slot().FillWidth(1).Padding(12,0,0,0)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Caption(LOCTEXT("Age","Report age"))]+SVerticalBox::Slot().AutoHeight()[SAssignNew(AgeLabel,STextBlock).TextStyle(&OrderValueStyle).AutoWrapText(true)]]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)[Icon(EUiGlyph::Warning,16)]+SHorizontalBox::Slot().FillWidth(1)[Caption(LOCTEXT("QuoteWarning","Actual transaction prices may differ."))]]
   +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&CardBrush).Padding(6)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[Icon(EUiGlyph::Lock,24)]+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("PriceLimit","Price limit"))]+SVerticalBox::Slot().AutoHeight()[SAssignNew(LimitLabel,STextBlock).TextStyle(&OrderCaptionStyle).AutoWrapText(true)]]]]]]
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&CardBrush).Padding(10)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,0,10,0)[Icon(EUiGlyph::Save,28)]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Factor","Your factor will"),true)]+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(SummaryLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]+SVerticalBox::Slot().AutoHeight()[Caption(LOCTEXT("Transport","Goods stay in this office. Ship transport is arranged separately."))]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBorder).BorderImage(&CardBrush).Padding(10)[SAssignNew(ActivityLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(StationOrderText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];
 SaveAction=MakeControl(TEXT("TradeMap.Orders.Save"),LOCTEXT("Create","Create buy order"),EHansaUiButtonStyle::Primary);SaveAction->SetButtonStyle(&OrderPrimaryStyle);
 PauseAction=MakeControl(TEXT("TradeMap.Orders.Pause"),LOCTEXT("Pause","Pause order"),EHansaUiButtonStyle::Secondary);
 CancelAction=MakeControl(TEXT("TradeMap.Orders.Cancel"),LOCTEXT("Cancel","Cancel order"),EHansaUiButtonStyle::Secondary);
 ExistingActions=SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[PauseAction.ToSharedRef()]+SHorizontalBox::Slot().FillWidth(1)[CancelAction.ToSharedRef()];
 ChildSlot[SAssignNew(StationOrdersPanel,SOverlay)
  +SOverlay::Slot()[SAssignNew(ListPanel,SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ListHeading,STextBlock).Text(LOCTEXT("Orders","Trading orders")).TextStyle(&LightHeadingStyle)]
   +SVerticalBox::Slot().AutoHeight()[SAssignNew(OrderPicker,SComboButton).ButtonStyle(&SecondaryButtonStyle).OnGetMenuContent_Lambda([this]()->TSharedRef<SWidget>{
    auto Choices=SNew(SVerticalBox);if(auto* P=Model.Get())for(const auto& Row:P->GetStationOrderRows())Choices->AddSlot().AutoHeight()[SNew(SHansaAction).Preferences(Preferences).Label(Row.Good).OnClicked_Lambda([this,Id=Row.Id]{if(auto* M=Model.Get())M->SelectStationOrder(Id);OrderPicker->SetIsOpen(false);ShowEditor(true);return FReply::Handled();})];
    return SNew(SBox).MaxDesiredHeight(320)[SNew(SScrollBox)+SScrollBox::Slot()[Choices]];
   }).ButtonContent()[Text(LOCTEXT("ChooseOrder","Choose order to edit"))]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(NewOrderControl,SBox)[MakeControl(TEXT("TradeMap.Orders.New"),LOCTEXT("NewOrder","Create new order"),EHansaUiButtonStyle::Primary)]]
   +SVerticalBox::Slot().FillHeight(1)[SAssignNew(ListScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(StationOrderList,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(RowsPanel,SVerticalBox)]]]]
  +SOverlay::Slot()[SAssignNew(EditorPanel,SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,10)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(EditorHeading,STextBlock).TextStyle(&OrderEditorHeadingStyle).AutoWrapText(true)]+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)[SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(8,4))[SAssignNew(DraftLabel,STextBlock).TextStyle(&OrderCaptionStyle)]]]
   +SVerticalBox::Slot().FillHeight(1)[SAssignNew(EditorScroll,SScrollBox).ScrollBarThickness(FVector2D(4,4))+SScrollBox::Slot()[Form]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(StationOrderFeedback,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&FooterBrush).Padding(10)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(3).Padding(0,0,8,0)[SaveAction.ToSharedRef()]+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[ExistingActions.ToSharedRef()]+SHorizontalBox::Slot().FillWidth(1)[MakeControl(TEXT("TradeMap.Orders.Back"),LOCTEXT("AllOrders","All orders"),EHansaUiButtonStyle::Secondary)]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(FooterHint,STextBlock).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible).TextStyle(&DarkBodyStyle).Justification(ETextJustify::Center).AutoWrapText(true)]]]]];
 MapWidget(TEXT("TradeMap.Orders.Select"),OrderPicker);ShowEditor(false);
}
bool STradeOrders::OpenGoodMenu(){if(!Model.IsValid()||!Model->CanStationOrderAction(TEXT("Good")))return false;GoodPicker->SetIsOpen(true);return true;}
bool STradeOrders::IsGoodMenuOpen() const{return GoodPicker.IsValid()&&GoodPicker->IsOpen();}
void STradeOrders::CloseGoodMenu(){GoodPicker->SetIsOpen(false);}
void STradeOrders::RefreshEditor(const FHansaTradeMapSnapshot& S) {
 if(!Model.IsValid())return;const auto& V=Model->GetStationOrderEditor();
 StationOrderList->SetText(LOCTEXT("MultipleOrders","Buy and sell several products at once. Create a separate order for each product and direction, or select an existing order to edit."));
 EditorHeading->SetText(V.Heading);DraftLabel->SetText(V.State);GoodLabel->SetText(V.GoodLabel);StockLabel->SetText(V.Stock);StockBar->SetPercent(V.StockFraction);StockBar->SetToolTipText(V.Stock);
 CommodityImage->SetImage(V.Good==TEXT("Good.Charcoal")?CharcoalArtwork(Preferences.UiScale):GetGeneratedIconBrush(GlyphForGood(V.Good),112));
 ModeHint->SetText(V.ModeHint);TargetLabel->SetText(V.TargetLabel);TargetHint->SetText(V.TargetHint);RateLabel->SetText(V.RateLabel);RateHint->SetText(V.RateHint);RateUnit->SetText(V.RateUnit);
 BuyAction->SetButtonStyle(V.bBuy?&OrderSelectedStyle:&OrderSecondaryStyle);SellAction->SetButtonStyle(!V.bBuy?&OrderSelectedStyle:&OrderSecondaryStyle);
 BuyAction->SetState(!Model->CanStationOrderAction(TEXT("Buy"))?EUiState::Disabled:V.bBuy?EUiState::Selected:EUiState::Default);SellAction->SetState(!Model->CanStationOrderAction(TEXT("Sell"))?EUiState::Disabled:!V.bBuy?EUiState::Selected:EUiState::Default);
 BuyAction->SetToolTipText(LOCTEXT("BuyDescription","Buy locally into office stock, up to a target."));SellAction->SetToolTipText(LOCTEXT("SellDescription","Sell office stock while preserving the chosen reserve."));
 MarketCard->SetBorderImage(V.bStale||!V.bReportAvailable?&WarningBrush:&CardBrush);ReportHeading->SetText(V.ReportHeading);PriceLabel->SetText(V.Price);AgeLabel->SetText(V.ReportAge);LimitLabel->SetText(V.Limit);
 SummaryLabel->SetText(V.Summary);ActivityLabel->SetText(V.Activity);FooterHint->SetText(V.FooterHint);StationOrderText->SetText(V.Rights);StationOrderText->SetVisibility(V.Rights.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 BudgetRow->SetVisibility(V.bBuy?EVisibility::Visible:EVisibility::Collapsed);ExistingActions->SetVisibility(S.SelectedStationOrderId>0?EVisibility::Visible:EVisibility::Collapsed);
 SaveAction->SetLabel(S.SelectedStationOrderId>0?LOCTEXT("Save","Save changes"):V.bBuy?LOCTEXT("CreateBuy","Create buy order"):LOCTEXT("CreateSell","Create sell order"));
 PauseAction->SetLabel(V.bPaused?LOCTEXT("Resume","Resume order"):LOCTEXT("Pause","Pause order"));CancelAction->SetLabel(S.bConfirmStationOrderCancel?LOCTEXT("ConfirmCancel","Confirm cancellation"):LOCTEXT("Cancel","Cancel order"));
 GoodPicker->SetEnabled(Model->CanStationOrderAction(TEXT("Good")));GoodPicker->SetToolTipText(S.SelectedStationOrderId>0?LOCTEXT("FixedGood","The good and direction of an existing order are fixed. Create a new order to change them."):LOCTEXT("ChooseGood","Choose a commodity for this trading order."));
}
}
#undef LOCTEXT_NAMESPACE
