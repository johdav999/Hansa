#include "STradeShipDetail.h"
#include "TradeArtwork.h"
#include "Trade/HansaCargoPlan.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace Hansa::UI
{
using namespace Hansa::Simulation;
namespace
{
 class SCargoCellButton final : public SButton
 {
 public:
  void SetResetAction(FOnClicked Action) { ResetAction = MoveTemp(Action); }
  virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
  {
   if (Event.GetEffectingButton() == EKeys::RightMouseButton)
   {
    if (IsEnabled() && ResetAction.IsBound()) ResetAction.Execute();
    return FReply::Handled();
   }
   return SButton::OnMouseButtonDown(Geometry, Event);
  }
  virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
  {
   if (Event.GetEffectingButton() == EKeys::RightMouseButton) return FReply::Handled();
   return SButton::OnMouseButtonUp(Geometry, Event);
  }
 private:
  FOnClicked ResetAction;
 };
 FText T(const TCHAR* Value) { return FText::FromString(Value); }
 const FSlateBrush* EmptyCargo(bool Load)
 {
  static TMap<FString,TSharedPtr<FSlateDynamicImageBrush>> Art;
  const FString Name = Load ? TEXT("load") : TEXT("unload"); auto& B = Art.FindOrAdd(Name);
  if (!B) B = MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeRouteEditor")/(Name+TEXT("--80.png")))), FVector2D(80));
  return B.Get();
 }
 const FSlateBrush* CogPortrait()
 {
  static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeRouteEditor/cog-portrait--144.png"))),FVector2D(144));return &Brush;
 }
 const FSlateBrush* NavyTexture()
 {
  static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeRouteEditor/navy-linen--1254.png"))),FVector2D(1254));Brush.Tiling=ESlateBrushTileType::Both;return &Brush;
 }
 const FSlateBrush* TownPortrait(const FString& City)
 {
  static FSlateDynamicImageBrush Lubeck(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeRouteEditor/lubeck--384x256.png"))),FVector2D(384,256));
  static FSlateDynamicImageBrush Rostock(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeWorkspace/Rostock.png"))),FVector2D(1536,1024));
  return City==TEXT("City.Lubeck")?&Lubeck:City==TEXT("City.Rostock")?&Rostock:nullptr;
 }
}
TSharedRef<SWidget> STradeShipDetail::Text(const FText& Value, int32 Size, bool Dark)
{
 auto Font = GetComponentFont(EHansaUiTypographyToken::SerifBody, Preferences); Font.Size = FMath::RoundToInt(Size * .8f);
 return SNew(STextBlock).Text(Value).Font(Font).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(Dark ? EHansaUiColorToken::Linen : EHansaUiColorToken::Ink)).AutoWrapText(true);
}
TSharedRef<SWidget> STradeShipDetail::Button(const FString& Id, const FText& Label, TFunction<FReply()> Click)
{
 Actions.Add(Id,Click);
 auto W = SNew(SButton).ButtonStyle(&PrimaryButtonStyle).ContentPadding(FMargin(12,6)).HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked_Lambda(MoveTemp(Click))[Text(Label,20,true)];
 MapWidget(Id,W); Cells.Add(Id,W); Order.Add(Id); return SNew(SBox).MinDesiredHeight(48)[SNew(SHansaReferenceFrame).Dark(true).Surface(NavyTexture()).Padding(4)[W]];
}
void STradeShipDetail::Construct(const FArguments&, const TSharedRef<FTradeComponentContext>& In)
{
 Initialize(In);
 PrimaryButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
 PrimaryButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,0.f));OverlayBrush=*NavyTexture();OverlayBrush.TintColor=FLinearColor(.65f,.65f,.65f,1);
 SecondaryButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
 CellStyle=SecondaryButtonStyle;
 const auto Edge=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oak);
 HeaderBrush=FSlateRoundedBoxBrush(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment),0.f,Edge*.5f,1.f);
 CellStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.08f),2.f,Edge*.5f,1.f)).SetHovered(FSlateRoundedBoxBrush(FLinearColor(.7f,.85f,.9f,.5f),2.f,Edge,1.f));
 SelectedCellStyle=CellStyle;SelectedCellStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(.55f,.75f,.85f,.8f),2.f,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass),2.f));
 ChildSlot[SAssignNew(Layers,SOverlay)
  +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(0,0,0,.45f))]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(16)[SAssignNew(Body,SBox).WidthOverride(1168).HeightOverride(888).MaxDesiredHeight(888)]
  +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(0,0,0,.45f)).Visibility_Lambda([this]{return Model.IsValid()&&(Model->CargoEditor.bOpen||bCloseReview)?EVisibility::Visible:EVisibility::Collapsed;})]
  +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(16)[SAssignNew(Popup,SBox).HeightOverride(768).MaxDesiredHeight(768)]];
 if (Model.IsValid()) FocusHandle = Model->OnFocusRestoreRequested().AddSPLambda(this,[this](FName Id){Model->SetFocusedSemanticId(Id);if (auto* Entry = Cells.Find(Id.ToString())) if (auto W = Entry->Pin()) FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::SetDirectly);});
 Refresh();
}
void STradeShipDetail::SetViewportSize(FIntPoint Size)
{
 if(ViewportSize==Size)return;ViewportSize=Size;bCompactLayout=Size.Y<800;
 Body->SetHeightOverride(FMath::Min(888,Size.Y-32));Popup->SetRenderTransform(FSlateRenderTransform(FVector2D(0,bCompactLayout?0:48)));
 CacheKey.Reset();Refresh();
}
void STradeShipDetail::Refresh()
{
 if (!Model.IsValid()) return;
 auto* M = Model.Get(); const auto& S = M->GetSnapshot();
 SetVisibility(M->bShipDetailOpen ? EVisibility::Visible : EVisibility::Collapsed);
 if (!M->bShipDetailOpen) { CacheKey.Reset(); return; }
 // While the selector is open, background market and route updates must not replace
 // its Slate widgets in the middle of a pointer gesture or reset its scroll position.
 FString Key = M->GetShipIdentity().ToString()+M->ShipDetailTab+Search+::LexToString(bCompactLayout);
 if (!M->CargoEditor.bOpen) {
  Key+=S.EditorStatus.ToString()+S.ReserveRisk.ToString()+S.DraftName+::LexToString(S.bDirty)+::LexToString(S.bCreating)+::LexToString(S.bCommandPending)+::LexToString(M->CanEditStops());
  for (const auto& Stop : M->GetSlotDraft()) { Key+=Stop.CityId.ToString(); for (const auto& A : Stop.Actions) Key+=FString::Printf(TEXT("%s:%d:%d:%lld:%lld"),*A.GoodId.ToString(),int32(A.Kind),A.CargoSlotIndex,A.QuantityLimit.GetRawValue(),A.MinimumSourceReserve.GetRawValue()); }
 }
 const auto& E=M->CargoEditor;Key+=FString::Printf(TEXT("%d:%d:%d:%d:%s:%lld:%lld"),E.bOpen,E.bLoad,E.Stop,E.Slot,*E.Draft.GoodId.ToString(),E.Draft.QuantityLimit.GetRawValue(),E.Draft.MinimumSourceReserve.GetRawValue())+E.Error.ToString();
 for(const auto& Product:E.Products)Key+=Product.Detail.ToString()+::LexToString(Product.bEnabled);
 Key+=::LexToString(int32(E.Draft.Kind))+E.SourceLabel.ToString()+::LexToString(S.bReview)+S.Validation.ToString()+S.CreatorReview.ToString();
 if (!E.bOpen) for(const auto& Cargo:M->GetShipCargo())Key+=Cargo.GoodId.ToString()+::LexToString(Cargo.Quantity.GetRawValue());
 Key+=::LexToString(bCloseReview)+::LexToString(bAdvanced);if(CacheKey==Key)return;
 if (E.bOpen && Popup->GetVisibility().IsVisible() && !bRefreshFromTimer) {
  if (!bRefreshQueued) {
   bRefreshQueued = true;
   RegisterActiveTimer(0.f,FWidgetActiveTimerDelegate::CreateSP(this,&STradeShipDetail::ApplyDeferredRefresh));
  }
  return;
 }
 CacheKey=Key;
 TArray<float> PriorScrollOffsets;
 if (E.bOpen && Popup->GetVisibility().IsVisible()) for (const auto& Scroll : Scrolls) PriorScrollOffsets.Add(Scroll.IsValid()?Scroll->GetScrollOffset():0.f);
 FString PriorFocus; const auto Focused=FSlateApplication::Get().GetKeyboardFocusedWidget(); for(const auto& Cell:Cells)if(Cell.Value.Pin()==Focused)PriorFocus=Cell.Key;
 Cells.Reset();Order.Reset();Actions.Reset();Scrolls.Reset();
 auto Tabs = SNew(SHorizontalBox);
 for (const TCHAR* Tab : {TEXT("Overview"),TEXT("Cargo"),TEXT("Route")})
 {
  const FString Name(Tab); Tabs->AddSlot().FillWidth(1).Padding(2)[Button(TEXT("TradeMap.Ship.")+Name,T(Tab),[this,Name]{Model->ShipDetailTab=Name;Refresh();return FReply::Handled();})];
 }
 auto Content = SNew(SVerticalBox);
 if(bCompactLayout)Content->AddSlot().AutoHeight()[SNew(SBorder).BorderImage(&OverlayBrush).Padding(8)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(FText::FromString(M->GetShipIdentity().ToString()+TEXT(" · Cog · 3 cargo slots")),24,true)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("TradeMap.Ship.Close"),T(TEXT("×")),[this]{Dispatch(TEXT("TradeMap.Ship.Close"));return FReply::Handled();})]]
  +SVerticalBox::Slot().AutoHeight()[Tabs]]];
 else Content->AddSlot().AutoHeight()[SNew(SBorder).BorderImage(&OverlayBrush).Padding(FMargin(16,8))[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().Padding(0,0,24,0)[SNew(SBox).WidthOverride(144).HeightOverride(144)[SNew(SImage).Image(CogPortrait())]]
  +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[Text(M->GetShipIdentity(),44,true)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,8)[Text(T(TEXT("Cog · 3 cargo slots")),18,true)]
   +SVerticalBox::Slot().AutoHeight()[Tabs]]
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[Button(TEXT("TradeMap.Ship.Close"),T(TEXT("×")),[this]{return Dispatch(TEXT("TradeMap.Ship.Close"))?FReply::Handled():FReply::Unhandled();})]]];
 if (M->ShipDetailTab == TEXT("Route")) Content->AddSlot().FillHeight(1)[Matrix()];
 else if(M->ShipDetailTab==TEXT("Cargo")){
  auto Cargo=SNew(SVerticalBox);const auto Slots=M->GetShipCargo();
  for(int32 I=0;I<Slots.Num();++I){const auto& Slot=Slots[I];Cargo->AddSlot().AutoHeight().Padding(12)[Text(FText::FromString(FString::Printf(TEXT("%s %d · %s · %g current"),I<3?TEXT("Slot"):TEXT("Recovery cargo"),I+1,Slot.GoodId.IsValid()?*M->GetOrderGoodLabel(Slot.GoodId).ToString():TEXT("Empty"),Slot.Quantity.GetRawValue()/1000.)),24)];}
  if(Slots.Num()>3){Cargo->AddSlot().AutoHeight().Padding(12)[Text(T(TEXT("Legacy overflow is preserved and can only be unloaded. Open the current town’s market, select this ship and sell or transfer its recovery goods before assigning a new slot plan.")),18)];Cargo->AddSlot().AutoHeight().Padding(12)[Button(TEXT("TradeMap.Ship.RecoveryMarket"),T(TEXT("Open market for cargo recovery")),[this]{const auto City=Model->GetShipTown();if(Model->MarketRequested&&!City.IsNone()&&!Model->GetSnapshot().bDirty)Model->MarketRequested(City,NAME_None);return FReply::Handled();})];}
  Cargo->AddSlot().AutoHeight().Padding(12)[Text(FText::FromString(FString::Printf(TEXT("Shared cargo capacity: %g"),M->GetShipCapacity()/1000.)),20)];Content->AddSlot().FillHeight(1).Padding(24)[SNew(SScrollBox)+SScrollBox::Slot()[Cargo]];
 }
 else Content->AddSlot().AutoHeight().Padding(32)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(FString::Printf(TEXT("Shared cargo capacity: %g"),M->GetShipCapacity()/1000.)),24)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,24)[Text(S.EditorStatus)]
  +SVerticalBox::Slot().AutoHeight()[Button(TEXT("TradeMap.Ship.Route"),T(TEXT("Route")),[this]{Model->ShipDetailTab=TEXT("Route");Refresh();return FReply::Handled();})]];
 Body->SetContent(SNew(SHansaReferenceFrame).Dark(false).Surface(TradeArtwork(TEXT("Linen"),true)).Padding(8)[Content]);
 Body->SetVisibility(M->CargoEditor.bOpen||bCloseReview?EVisibility::HitTestInvisible:EVisibility::Visible);
 Popup->SetVisibility(M->CargoEditor.bOpen||bCloseReview?EVisibility::Visible:EVisibility::Collapsed);
 if (M->CargoEditor.bOpen) { Popup->SetHeightOverride(FMath::Min(768,ViewportSize.Y-32));Popup->SetRenderTransform(FSlateRenderTransform(FVector2D(0,bCompactLayout?0:48)));Popup->SetWidthOverride(M->CargoEditor.bLoad?736:600);Popup->SetContent(Selector());for(int32 I=0;I<FMath::Min(PriorScrollOffsets.Num(),Scrolls.Num());++I)if(Scrolls[I].IsValid())Scrolls[I]->SetScrollOffset(PriorScrollOffsets[I]); }
 else if(bCloseReview){Popup->SetWidthOverride(600);Popup->SetHeightOverride(240);Popup->SetContent(SNew(SHansaReferenceFrame).Dark(false).Padding(24)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[Text(T(TEXT("Discard unsaved route changes?")),26)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,24)[Button(TEXT("TradeMap.Ship.CloseReview.Keep"),T(TEXT("Keep editing")),[this]{bCloseReview=false;Refresh();return FReply::Handled();})]
  +SVerticalBox::Slot().AutoHeight()[Button(TEXT("TradeMap.Ship.CloseReview.Discard"),T(TEXT("Discard changes")),[this]{if(Model->GetSnapshot().bCreating){if(!Model->GetSnapshot().bDiscardConfirmation)Model->DiscardCreateIntent();Model->DiscardCreateIntent();}else Model->DiscardRouteEditsIntent();bCloseReview=false;if(!Model->GetSnapshot().bDirty){Model->bShipDetailOpen=false;Model->OnChanged().Broadcast(Model->GetSnapshot(),Model->GetRevision());}Refresh();return FReply::Handled();})]]);}
 if(const auto* Entry=Cells.Find(PriorFocus))if(auto W=Entry->Pin())FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::SetDirectly);
}
EActiveTimerReturnType STradeShipDetail::ApplyDeferredRefresh(double, float)
{
 if (!FSlateApplication::Get().GetPressedMouseButtons().IsEmpty()) return EActiveTimerReturnType::Continue;
 bRefreshQueued = false;
 bRefreshFromTimer = true;
 Refresh();
 bRefreshFromTimer = false;
 return EActiveTimerReturnType::Stop;
}
TSharedRef<SWidget> STradeShipDetail::Matrix()
{
 auto* M = Model.Get(); const auto& S = M->GetSnapshot();
 const auto Stops = M->GetSlotDraft();
 auto Panel = SNew(SVerticalBox);
 if(S.bCreating&&S.bReview){
  auto Review=SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(T(TEXT("Review this ship’s route")),26)]+SVerticalBox::Slot().AutoHeight().Padding(0,16)[Text(S.CreatorReview,18)]+SVerticalBox::Slot().AutoHeight()[Text(S.Validation,18)]];Scrolls.Add(Review);
  return SNew(SVerticalBox)+SVerticalBox::Slot().FillHeight(1).Padding(24)[Review]+SVerticalBox::Slot().AutoHeight().Padding(24)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(4)[Button(TEXT("TradeMap.Ship.EditReview"),T(TEXT("Edit route")),[this]{Model->EditCreateIntent();return FReply::Handled();})]+SHorizontalBox::Slot().FillWidth(1).Padding(4)[Button(TEXT("TradeMap.Ship.ConfirmCreate"),T(TEXT("Create and start route")),[this]{Model->CreateAndActivateIntent();return FReply::Handled();})]];
 }
 if(Stops.IsEmpty())return SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight().Padding(32)[Text(T(TEXT("This ship has no route.")),24)]+SVerticalBox::Slot().AutoHeight().Padding(32)[Button(TEXT("TradeMap.Ship.NewRoute"),T(TEXT("Create route for this ship")),[this]{Model->BeginCreateIntent(NAME_None,Model->GetSnapshot().SelectedCityStableId);return FReply::Handled();})];
 Panel->AddSlot().AutoHeight().Padding(24,bCompactLayout?4:12)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(S.bCreating?FText::FromString(S.DraftName):M->GetSelectedRoutePresentation()?M->GetSelectedRoutePresentation()->Label:T(TEXT("Trade route")),26)]
  +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("TradeMap.Ship.AddStop"),T(TEXT("+ Add stop")),[this]{Model->AddStopIntent();return FReply::Handled();})]];
 if(S.bCreating){auto Name=SNew(SEditableTextBox).Style(&RouteNameStyle).Text(FText::FromString(S.DraftName)).OnTextCommitted_Lambda([this](const FText& V,ETextCommit::Type){Model->SetRouteNameIntent(V.ToString());});MapWidget(TEXT("TradeMap.Ship.RouteName"),Name);Cells.Add(TEXT("TradeMap.Ship.RouteName"),Name);Order.Add(TEXT("TradeMap.Ship.RouteName"));Panel->AddSlot().AutoHeight().Padding(24,0)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Name]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("TradeMap.Ship.ChooseCog"),T(TEXT("Choose ship")),[this]{Model->CycleCogIntent();return FReply::Handled();})]];}
 auto Headers = SNew(SHorizontalBox);
 const TCHAR* Names[] = {TEXT("Stop"),TEXT("Action"),TEXT("Slot 1"),TEXT("Slot 2"),TEXT("Slot 3")};
 for (int32 I=0;I<5;++I) Headers->AddSlot().FillWidth(I==0?1.25f:I==1?.85f:1.f)[SNew(SBorder).BorderImage(&HeaderBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)[Text(T(Names[I]),20)]];
 Panel->AddSlot().AutoHeight().Padding(24,0)[SNew(SBox).HeightOverride(bCompactLayout?32:48)[Headers]];
 auto Groups = SNew(SVerticalBox);
 for (int32 I=0;I<Stops.Num();++I)
 {
  const auto& Stop = Stops[I]; auto Group=SNew(SHorizontalBox);
  const FString CityName = S.Stops.IsValidIndex(I)?S.Stops[I].CityLabel.ToString():Stop.CityId.ToString();
  auto Town=SNew(SVerticalBox);
  auto TownPicker=SNew(SComboButton).ButtonStyle(&CellStyle).IsEnabled(M->CanEditStops()).OnGetMenuContent_Lambda([this,I]{
    auto Choices=SNew(SVerticalBox);
    for(const auto& City:Model->GetSnapshot().Cities){const FName Id=City.StableId;Choices->AddSlot().AutoHeight()[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked_Lambda([this,I,Id]{Model->SetStopTown(I,Id);FSlateApplication::Get().DismissAllMenus();return FReply::Handled();})[Text(City.Label,20)]];}
    return SNew(SBox).WidthOverride(240).MaxDesiredHeight(360)[SNew(SScrollBox)+SScrollBox::Slot()[Choices]];
   }).ButtonContent()[Text(FText::FromString(CityName),24)];
  const FString TownId=FString::Printf(TEXT("TradeMap.Ship.Town.%d"),I);MapWidget(TownId,TownPicker);Cells.Add(TownId,TownPicker);Order.Add(TownId);
  Town->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(40).HeightOverride(40)[SNew(SBorder).BorderImage(&OverlayBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)[Text(FText::AsNumber(I+1),26,true)]]]+SHorizontalBox::Slot().FillWidth(1)[TownPicker]];Town->AddSlot().FillHeight(1)[SNullWidget::NullWidget];
  auto Controls=SNew(SHorizontalBox);
  for (int32 Direction : {-1,1,0}) Controls->AddSlot().AutoWidth().Padding(2)[Button(FString::Printf(TEXT("TradeMap.Ship.Stop.%d.%d"),I,Direction),T(Direction<0?TEXT("▲"):Direction>0?TEXT("▼"):TEXT("×")),[this,I,Direction]{Model->SelectStopIntent(I);if(Direction)Model->MoveStopIntent(Direction);else Model->RemoveStopIntent();return FReply::Handled();})];
  Town->AddSlot().AutoHeight()[Controls];
  Group->AddSlot().FillWidth(1.25f).Padding(2)[SNew(SOverlay)
   +SOverlay::Slot().VAlign(VAlign_Bottom)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(TownPortrait(Stop.CityId.ToString())).ColorAndOpacity(FLinearColor(1,1,1,.8f))]]
   +SOverlay::Slot().Padding(10)[Town]];
  auto Labels=SNew(SVerticalBox);
  for (const TCHAR* Label : {TEXT("Unload"),TEXT("Load")}) Labels->AddSlot().FillHeight(1).VAlign(VAlign_Center).HAlign(HAlign_Center)[Text(T(Label),20)];
  Group->AddSlot().FillWidth(.85f)[Labels];
  for (int32 Slot=0;Slot<3;++Slot)
  {
   auto Column=SNew(SVerticalBox);
   for (bool Load : {false,true})
   {
    const auto* A=Stop.Actions.FindByPredicate([&](const auto& V){return V.CargoSlotIndex==Slot&&IsRouteLoad(V.Kind)==Load;});
    const FString Id=FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"),I,Slot,Load?TEXT("Load"):TEXT("Unload"));
    auto Contents=SNew(SHorizontalBox);
    Contents->AddSlot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(80).HeightOverride(80)[SNew(SImage).Image(A?GetGeneratedIconBrush(GlyphForGood(FName(*A->GoodId.ToString())),80):EmptyCargo(Load))]];
    if(A)Contents->AddSlot().FillWidth(1).VAlign(VAlign_Center)[Text(FText::FromString(FString::Printf(TEXT("%s%g"),Load?TEXT("+"):TEXT("−"),A->QuantityLimit.GetRawValue()/1000.)),28)];
    const bool bCanOpen = M->CanOpenCargoCell(I,Slot,Load);
    auto Cell=SNew(SCargoCellButton).ButtonStyle(&CellStyle).ContentPadding(4).IsEnabled(bCanOpen)
     .ToolTipText(FText::FromString(!Load&&!bCanOpen&&M->CanEditStops()?TEXT("This slot has no current or planned cargo to unload. Select its Load row to load a product."):FString::Printf(TEXT("%s · %s · Slot %d · %s\nLeft-click to edit. Right-click to clear."),*CityName,Load?TEXT("Load"):TEXT("Unload"),Slot+1,A?*A->GoodId.ToString():TEXT("No instruction"))))
     .OnClicked_Lambda([this,I,Slot,Load]{Search.Reset();Model->OpenCargoCell(I,Slot,Load);return FReply::Handled();})[Contents];
    Cell->SetResetAction(FOnClicked::CreateLambda([this,I,Slot,Load]{if(Model->OpenCargoCell(I,Slot,Load))Model->ConfirmCargoCell(true);return FReply::Handled();}));
    MapWidget(Id,Cell);Cells.Add(Id,Cell);Order.Add(Id);
    Column->AddSlot().FillHeight(1).Padding(3)[Cell];
   }
   Group->AddSlot().FillWidth(1)[Column];
  }
  Groups->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(bCompactLayout?176:208)[SNew(SBorder).BorderImage(&CellStyle.Normal).Padding(0)[Group]]];
 }
 auto MatrixScroll=SNew(SScrollBox)+SScrollBox::Slot()[Groups];Scrolls.Add(MatrixScroll);
 Panel->AddSlot().FillHeight(1).Padding(24,0)[MatrixScroll];
 Panel->AddSlot().AutoHeight().Padding(24,bCompactLayout?2:12)[SNew(SBox).MaxDesiredHeight(bCompactLayout?24:48)[SNew(SScrollBox)+SScrollBox::Slot()[Text((S.EditorStatus.IsEmpty()||S.EditorStatus.ToString()==TEXT("Select a route to edit its stops and cargo rules."))?T(TEXT("Select a slot to choose product and quantity.")):S.EditorStatus,16)]]];
 auto Footer=SNew(SHorizontalBox);
 if(!bCompactLayout)Footer->AddSlot().FillWidth(1)[Text(T(TEXT("Shared hold · unload before load")),14)];
 Footer->AddSlot().FillWidth(1).Padding(8)[Button(TEXT("TradeMap.Ship.Save"),T(S.bCreating?TEXT("Review route"):TEXT("Save changes")),[this]{Model->CommitIntent();return FReply::Handled();})];
 Footer->AddSlot().FillWidth(1).Padding(8)[Button(TEXT("TradeMap.Ship.Start"),M->GetSelectedRoutePresentation()?M->GetSelectedRoutePresentation()->ToggleActionLabel:T(TEXT("Start route")),[this]{if(Model->GetSnapshot().bCreating)Model->ReviewCreateIntent();else Model->ToggleActiveIntent();return FReply::Handled();})];
 Panel->AddSlot().AutoHeight().Padding(24,0,24,12)[Footer];
 return Panel;
}
TSharedRef<SWidget> STradeShipDetail::Selector()
{
 auto* M=Model.Get();const auto& E=M->CargoEditor; const auto* Town=M->GetSnapshot().Cities.FindByPredicate([&](const auto& C){return C.StableId.ToString()==E.City.ToString();});const FString TownName=Town?Town->Label.ToString():E.City.ToString().Replace(TEXT("City."),TEXT(""));
 auto Panel=SNew(SVerticalBox);
 auto TitleRow=SNew(SHorizontalBox);
 if(E.bLoad&&!bCompactLayout)TitleRow->AddSlot().AutoWidth().Padding(8,0,16,0)[SNew(SBox).WidthOverride(96).HeightOverride(96)[SNew(SImage).Image(E.Draft.GoodId.IsValid()?GetGeneratedIconBrush(GlyphForGood(FName(*E.Draft.GoodId.ToString())),96):EmptyCargo(true))]];
 TitleRow->AddSlot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[Text(T(E.bLoad?TEXT("Load product"):TEXT("Unload product")),bCompactLayout?26:42,true)]
  +SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(FString::Printf(TEXT("%s · %s · Slot %d"),*M->GetShipIdentity().ToString(),*TownName,E.Slot+1)),20,true)]];
 TitleRow->AddSlot().AutoWidth().VAlign(VAlign_Top)[Button(TEXT("TradeMap.Cargo.Close"),T(TEXT("×")),[this]{Model->CancelCargoCell();return FReply::Handled();})];
 Panel->AddSlot().AutoHeight()[SNew(SBorder).BorderImage(&OverlayBrush).Padding(8)[TitleRow]];
 auto Fields=SNew(SVerticalBox);
 if(E.bLoad){auto SearchInput=SNew(SEditableTextBox).Style(&RouteNameStyle).HintText(T(TEXT("Search products…"))).Text(FText::FromString(Search)).OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type){Search=Value.ToString();Refresh();});MapWidget(TEXT("TradeMap.Cargo.Search"),SearchInput);Cells.Add(TEXT("TradeMap.Cargo.Search"),SearchInput);Order.Add(TEXT("TradeMap.Cargo.Search"));Fields->AddSlot().AutoHeight().Padding(0,8)[SearchInput];}
 if(!bCompactLayout)Fields->AddSlot().AutoHeight().Padding(0,8)[Text(E.bLoad?FText::FromString(TEXT("Available at ")+TownName):T(TEXT("Cargo in this slot")),22)];
 if(E.bLoad)Fields->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(2)[Text(T(TEXT("Product")),18)]+SHorizontalBox::Slot().FillWidth(1)[Text(T(TEXT("Reported stock")),16)]+SHorizontalBox::Slot().FillWidth(.8f)[Text(T(TEXT("Price (pf)")),16)]];
 auto Products=SNew(SVerticalBox);
 if(!E.bLoad && E.Products.IsEmpty())Products->AddSlot().AutoHeight().Padding(0,8)[Text(T(TEXT("This slot has no cargo to unload. To load a product, close this popup and select this slot in the Load row.")),18)];
 for(const auto& Choice:E.Products)
 {
  if(!Search.IsEmpty()&&!Choice.Name.ToString().Contains(Search))continue;
  const FString Good=Choice.GoodId.ToString();
  auto Row=SNew(SButton).ButtonStyle(Choice.GoodId==E.Draft.GoodId?&SelectedCellStyle:&CellStyle).ContentPadding(FMargin(4,2)).IsEnabled(Choice.bEnabled).ToolTipText(Choice.Detail)
   .OnClicked_Lambda([this,Good]{Model->SetCargoProduct(Good);return FReply::Handled();})
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(E.bLoad?48:64).HeightOverride(E.bLoad?48:64)[SNew(SImage).Image(GetGeneratedIconBrush(GlyphForGood(FName(*Good)),64))]]
    +SHorizontalBox::Slot().FillWidth(1).Padding(12,0).VAlign(VAlign_Center)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[Text(Choice.Name,18)]
     +SVerticalBox::Slot().AutoHeight()[Text(E.bLoad?FText():Choice.Detail,14)]]
    +SHorizontalBox::Slot().FillWidth(E.bLoad?.6f:.05f).VAlign(VAlign_Center)[Text(E.bLoad?FText::FromString(Choice.Reported<0?TEXT("—"):FString::Printf(TEXT("%g"),Choice.Reported/1000.)):FText(),18)]
    +SHorizontalBox::Slot().FillWidth(E.bLoad?.6f:.05f).VAlign(VAlign_Center)[Text(E.bLoad?FText::FromString(Choice.PricePf<0?TEXT("—"):FString::Printf(TEXT("%g"),Choice.PricePf/1000.)):FText(),18)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(T(Choice.GoodId==E.Draft.GoodId?TEXT("✓"):TEXT("")),20)]];
  MapWidget(TEXT("TradeMap.Cargo.Product.")+Good,Row);Cells.Add(TEXT("TradeMap.Cargo.Product.")+Good,Row);Order.Add(TEXT("TradeMap.Cargo.Product.")+Good);Products->AddSlot().AutoHeight().Padding(0,2)[Row];
 }
 auto ProductScroll=SNew(SScrollBox)+SScrollBox::Slot()[Products];Scrolls.Add(ProductScroll);
 Fields->AddSlot().AutoHeight()[SNew(SBox).MaxDesiredHeight(bCompactLayout?72:212)[ProductScroll]];
 if(const auto* Selected=Cells.Find(TEXT("TradeMap.Cargo.Product.")+E.Draft.GoodId.ToString()))if(auto W=Selected->Pin())ProductScroll->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::Center);
 Fields->AddSlot().AutoHeight().Padding(0,8,0,4)[Text(T(TEXT("Quantity")),22)];
 auto Quantity=SNew(SHorizontalBox);
 Quantity->AddSlot().AutoWidth()[Button(TEXT("TradeMap.Cargo.Minus"),T(TEXT("−")),[this]{Model->SetCargoQuantity(FMath::Max<int64>(1,Model->CargoEditor.Draft.QuantityLimit.GetRawValue()-1000));return FReply::Handled();})];
 auto Input=SNew(SEditableTextBox).Style(&RouteNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences)).Justification(ETextJustify::Center).Text(FText::AsNumber(E.Draft.QuantityLimit.GetRawValue()/1000.))
  .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type){double Number=0;if(LexTryParseString(Number,*Value.ToString())&&FMath::IsFinite(Number)&&Number>=0&&Number<=1e9)Model->SetCargoQuantity(FMath::RoundToInt64(Number*1000));else Model->SetCargoQuantity(-1);});
 MapWidget(TEXT("TradeMap.Cargo.Quantity"),Input);Cells.Add(TEXT("TradeMap.Cargo.Quantity"),Input);Order.Add(TEXT("TradeMap.Cargo.Quantity"));
 Quantity->AddSlot().AutoWidth().Padding(6,0)[SNew(SBox).WidthOverride(120)[Input]];
 Quantity->AddSlot().AutoWidth()[Button(TEXT("TradeMap.Cargo.Plus"),T(TEXT("+")),[this]{Model->SetCargoQuantity(Model->CargoEditor.Draft.QuantityLimit.GetRawValue()+1000);return FReply::Handled();})];
 Quantity->AddSlot().FillWidth(1).Padding(16,0)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(bCompactLayout?48:64).HeightOverride(bCompactLayout?48:64)[SNew(SImage).Image(E.Draft.GoodId.IsValid()?GetGeneratedIconBrush(GlyphForGood(FName(*E.Draft.GoodId.ToString())),64):EmptyCargo(E.bLoad))]]+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(FString::Printf(TEXT("%s%g"),E.bLoad?TEXT("+"):TEXT("−"),E.Draft.QuantityLimit.GetRawValue()/1000.)),32)]+SVerticalBox::Slot().AutoHeight()[Text(M->GetOrderGoodLabel(E.Draft.GoodId),18)]]];
 Fields->AddSlot().AutoHeight()[Quantity];
 Fields->AddSlot().AutoHeight().Padding(0,8)[Text(E.SourceLabel,16)];
 if(!E.bLoad){auto Planned=FHansaCargoPlan::Before(M->GetSlotDraft(),E.Stop,false,M->GetShipCapacity());FHansaCargoPlan::Apply(Planned,E.Draft,M->GetShipCapacity());const int64 Remaining=Planned.IsValidIndex(E.Slot)?Planned[E.Slot].Quantity.GetRawValue():0;Fields->AddSlot().AutoHeight()[Text(FText::FromString(FString::Printf(TEXT("To: %s · planned remaining in slot: %g"),*TownName,Remaining/1000.)),16)];}
 Fields->AddSlot().AutoHeight().Padding(0,8)[Button(TEXT("TradeMap.Cargo.Advanced"),T(bAdvanced?TEXT("− Source and reserve"):TEXT("+ Source and reserve")),[this]{bAdvanced=!bAdvanced;Refresh();return FReply::Handled();})];
 auto Advanced=SNew(SVerticalBox);
 Advanced->AddSlot().AutoHeight()[Text(T(TEXT("Minimum source reserve")),18)];
 auto ReserveInput=SNew(SEditableTextBox).Style(&RouteNameStyle).Text(FText::AsNumber(E.Draft.MinimumSourceReserve.GetRawValue()/1000.)).OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type){double Number=0;if(LexTryParseString(Number,*Value.ToString())&&FMath::IsFinite(Number)&&Number>=0&&Number<=1e9)Model->SetCargoQuantity(FMath::RoundToInt64(Number*1000),true);else Model->SetCargoQuantity(-1,true);});MapWidget(TEXT("TradeMap.Cargo.Reserve"),ReserveInput);Cells.Add(TEXT("TradeMap.Cargo.Reserve"),ReserveInput);Order.Add(TEXT("TradeMap.Cargo.Reserve"));Advanced->AddSlot().AutoHeight()[ReserveInput];
 Advanced->AddSlot().AutoHeight().Padding(0,4,0,8)[Text(T(TEXT("Leave this many units at the source when the route runs. If stock does not exceed this value, nothing loads; 0 leaves no extra reserve.")),16)];
 for(uint8 Mode=0;Mode<3;++Mode){const uint8 Kind=Mode*2+(E.bLoad?0:1);const TCHAR* Source=Mode==0?(E.bTownMarketOnly?TEXT("Market purchase / sale"):TEXT("City stock transfer")):Mode==1?TEXT("Station transfer"):TEXT("Home transfer");const FString Label=FString::Printf(TEXT("%s%s"),uint8(E.Draft.Kind)==Kind?TEXT("✓ "):TEXT(""),Source);Advanced->AddSlot().AutoHeight()[Button(FString::Printf(TEXT("TradeMap.Cargo.Source.%d"),Kind),FText::FromString(Label),[this,Kind]{Model->SetCargoSource(Kind);return FReply::Handled();})];}
 if(bAdvanced)Fields->AddSlot().AutoHeight().Padding(0,12)[Advanced];
 
 auto FieldScroll=SNew(SScrollBox)+SScrollBox::Slot()[Fields];Scrolls.Add(FieldScroll);
 Panel->AddSlot().FillHeight(1).Padding(24,8)[FieldScroll];
 if (!E.Error.IsEmpty()) Panel->AddSlot().AutoHeight().Padding(24,0)[Text(E.Error,16)];
 Panel->AddSlot().AutoHeight().Padding(24,12)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)[Button(TEXT("TradeMap.Cargo.Cancel"),T(TEXT("Cancel")),[this]{Model->CancelCargoCell();return FReply::Handled();})]
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)[Button(TEXT("TradeMap.Cargo.Remove"),T(E.bLoad?TEXT("Clear load"):TEXT("Clear unload")),[this]{Model->ConfirmCargoCell(true);return FReply::Handled();})]
  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("TradeMap.Cargo.Confirm"),T(E.bLoad?TEXT("Set load"):TEXT("Set unload")),[this]{Model->ConfirmCargoCell();return FReply::Handled();})]];
 
 return SNew(SHansaReferenceFrame).Dark(false).Surface(TradeArtwork(TEXT("Linen"),true)).Padding(8)[Panel];
}
bool STradeShipDetail::Dispatch(const FString& Id)
{
 if(!Model.IsValid())return false;
 if(Id==TEXT("TradeMap.Ship.Close")){if(Model->CargoEditor.bOpen)Model->CancelCargoCell();else if(bCloseReview){bCloseReview=false;Refresh();}else if(Model->GetSnapshot().bDirty){bCloseReview=true;Refresh();}else{Model->bShipDetailOpen=false;Model->OnChanged().Broadcast(Model->GetSnapshot(),Model->GetRevision());Refresh();}return true;}
 if(const auto* Action=Actions.Find(Id)){auto Invoke=*Action;return Invoke().IsEventHandled();}
 if(Id==TEXT("TradeMap.Cargo.Cancel")){Model->CancelCargoCell();return true;}
 if(Id==TEXT("TradeMap.Cargo.Confirm"))return Model->ConfirmCargoCell();
 if(Id==TEXT("TradeMap.Cargo.Remove"))return Model->ConfirmCargoCell(true);
 if(Id.StartsWith(TEXT("TradeMap.Cargo.Product.")))return Model->SetCargoProduct(Id.RightChop(23));
 TArray<FString> Parts;Id.ParseIntoArray(Parts,TEXT("."));
 if(Parts.Num()==5&&Parts[1]==TEXT("Cargo")&&Parts[2].IsNumeric())return Model->OpenCargoCell(FCString::Atoi(*Parts[2]),FCString::Atoi(*Parts[3]),Parts[4]==TEXT("Load"));
 return false;
}
TArray<FString> STradeShipDetail::FocusOrder() const
{
 TArray<FString> Result; const bool PopupOpen=Model.IsValid()&&Model->CargoEditor.bOpen;
 if(bCloseReview){Result={TEXT("TradeMap.Ship.CloseReview.Keep"),TEXT("TradeMap.Ship.CloseReview.Discard")};return Result;}
 for(const auto& Id:Order){ if(PopupOpen && !Id.StartsWith(TEXT("TradeMap.Cargo.")))continue; if(!PopupOpen && Id.StartsWith(TEXT("TradeMap.Cargo.Product.")))continue;const auto* Entry=Cells.Find(Id);auto W=Entry?Entry->Pin():nullptr;if(!W||!W->IsEnabled())continue;bool Visible=true;for(auto Parent=W;Parent;Parent=Parent->GetParentWidget())if(!Parent->GetVisibility().IsVisible()||!Parent->IsEnabled()){Visible=false;break;}if(Visible)Result.Add(Id); }
 return Result;
}
FReply STradeShipDetail::OnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 if(E.GetKey()==EKeys::Escape||E.GetKey()==EKeys::Gamepad_FaceButton_Right){Dispatch(TEXT("TradeMap.Ship.Close"));return FReply::Handled();}
 return STradeComponent::OnKeyDown(G,E);
}
}
