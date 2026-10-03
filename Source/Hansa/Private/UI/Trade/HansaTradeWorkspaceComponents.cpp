#include "HansaTradeWorkspaceComponents.h"
#include "STradeRouteCanvas.h"
#include "TradeArtwork.h"
#include "STradeStationDetails.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SComboButton.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "UI/SHansaReferenceFrame.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaTradeMap"
namespace Hansa::UI {
void STradeComponent::Initialize(const TSharedRef<FTradeComponentContext>& In) { Context=In; Preferences=In->Preferences;Model=In->Model;
		WorkingBrush=GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush; FloatingBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Floating); OverlayBrush=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		PrimaryButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary); SecondaryButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary); IconButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Icon);
		LightBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,false); LightBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)); LightCaptionStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption,false); LightCaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences)); DarkBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,true); DarkBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)); LightHeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,false); LightHeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences)); HeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1,true); HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
        DetailsAreaStyle=FCoreStyle::Get().GetWidgetStyle<FExpandableAreaStyle>(TEXT("ExpandableArea"));
        DetailsAreaStyle.SetCollapsedImage(*GetGeneratedIconBrush(EUiGlyph::Plus,24)).SetExpandedImage(*GetGeneratedIconBrush(EUiGlyph::Minus,24));
        RouteNameStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
        RouteNameStyle.SetBackgroundImageNormal(WorkingBrush).SetBackgroundImageHovered(WorkingBrush).SetBackgroundImageFocused(GetComponentStyle(EUiSurface::Panel,EUiState::Selected,Preferences).Brush).SetBackgroundImageReadOnly(WorkingBrush);
        RouteNameStyle.SetForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).SetFocusedForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).SetReadOnlyForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink));
        RouteNameStyle.SetPadding(FMargin(10,8));
        LightBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences));
        DarkBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences));
        const auto Navy=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy),Slate=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HarborSlate),Linen=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen);
        DarkRowStyle=IconButtonStyle;DarkRowStyle.SetNormal(FSlateRoundedBoxBrush(Navy*.72f,0.f)).SetHovered(FSlateRoundedBoxBrush(Slate,0.f)).SetPressed(FSlateRoundedBoxBrush(Slate,0.f)).SetNormalPadding(FMargin(8,5)).SetPressedPadding(FMargin(8,5));
        DarkSelectedRowStyle=DarkRowStyle;DarkSelectedRowStyle.SetNormal(FSlateRoundedBoxBrush(Slate,0.f));
        LightRowStyle=SecondaryButtonStyle;LightRowStyle.SetNormal(FSlateRoundedBoxBrush(Linen,0.f)).SetHovered(FSlateRoundedBoxBrush(Linen,0.f)).SetNormalPadding(FMargin(6,6)).SetPressedPadding(FMargin(6,6));
}
TSharedRef<SHansaAction> STradeComponent::MakeControl(const TCHAR* Id,const FText& Label,EHansaUiButtonStyle Kind) {
 auto Button=SNew(SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(Kind).Label(Label).OnClicked(this,&STradeComponent::Invoke,FString(Id));
 Button->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Name=FName(Id)]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(Name);}));
 MapWidget(Id,Button);return Button;
}
TSharedRef<SWidget> STradeComponent::Pair(const TCHAR* A,const FText& AL,const TCHAR* B,const FText& BL) {
 return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[MakeControl(A,AL,EHansaUiButtonStyle::Secondary)]+SHorizontalBox::Slot().FillWidth(1)[MakeControl(B,BL,EHansaUiButtonStyle::Secondary)];
}
void STradeDirectory::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
Initialize(In);
ModeFilterAction=MakeControl(TEXT("TradeMap.Mode.Filter"),LOCTEXT("RoutesFilter","Routes"),EHansaUiButtonStyle::Secondary);
CityFilterAction=MakeControl(TEXT("TradeMap.City.Filter"),LOCTEXT("CitiesFilter","Cities"),EHansaUiButtonStyle::Secondary);
GoodFilterAction=MakeControl(TEXT("TradeMap.Good.Filter"),LOCTEXT("GoodsFilter","Good"),EHansaUiButtonStyle::Secondary);
RoutesView=MakeControl(TEXT("TradeMap.Directory.Routes"),LOCTEXT("DirectoryRoutes","Routes"),EHansaUiButtonStyle::Secondary);
FleetView=MakeControl(TEXT("TradeMap.Directory.Fleet"),LOCTEXT("DirectoryFleet","Fleet"),EHansaUiButtonStyle::Secondary);
StatusFilter=MakeControl(TEXT("TradeMap.Directory.Filter"),LOCTEXT("DirectoryAll","All states"),EHansaUiButtonStyle::Secondary);
ToggleRoute=MakeControl(TEXT("TradeMap.Directory.Toggle"),LOCTEXT("DirectoryPause","Pause / resume"),EHansaUiButtonStyle::Secondary);
CancelRoute=MakeControl(TEXT("TradeMap.Directory.Cancel"),LOCTEXT("DirectoryCancel","Cancel safely"),EHansaUiButtonStyle::Secondary);
Locate=MakeControl(TEXT("TradeMap.Directory.Locate"),LOCTEXT("DirectoryLocate","Locate ship"),EHansaUiButtonStyle::Secondary);
Recovery=MakeControl(TEXT("TradeMap.Directory.Recovery"),LOCTEXT("DirectoryRecovery","Review recovery"),EHansaUiButtonStyle::Secondary);
SAssignNew(DirectoryToolbar,SScrollBox).Orientation(Orient_Vertical);
for(auto W:{StatusFilter,ModeFilterAction,CityFilterAction,GoodFilterAction,ToggleRoute,CancelRoute,Locate,Recovery})DirectoryToolbar->AddSlot().Padding(0,2)[W.ToSharedRef()];
auto More=SAssignNew(MoreMenu,SComboButton).ButtonStyle(&IconButtonStyle).HasDownArrow(false)
 .ButtonContent()[SNew(STextBlock).Text(LOCTEXT("DirectoryMore","Filters / actions")).TextStyle(&DarkBodyStyle)]
 .MenuContent()[SNew(SBox).WidthOverride(280).MaxDesiredHeight(420)[DirectoryToolbar.ToSharedRef()]];
PagingNavigation=Pair(TEXT("TradeMap.Route.Page.Previous"),LOCTEXT("PreviousRoutes","Previous"),TEXT("TradeMap.Route.Page.Next"),LOCTEXT("NextRoutes","Next"));
ChildSlot[SNew(SHansaReferenceFrame).Dark(true).Padding(12)
                    [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SAssignNew(DirectoryTitle,STextBlock).Text(LOCTEXT("Directory","Routes")).TextStyle(&HeadingStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(1)[RoutesView.ToSharedRef()]+SHorizontalBox::Slot().FillWidth(1)[FleetView.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight()[More]

 +SVerticalBox::Slot().AutoHeight()[SAssignNew(EmptyText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]
+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ModeText,STextBlock).TextStyle(&LightCaptionStyle)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(CityModeText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(GoodModeText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(CitySearchInput,SEditableTextBox).Style(&RouteNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)).HintText(LOCTEXT("SearchCities","Search routes, ships or cities")).OnTextChanged_Lambda([this](const FText& T){if(auto* P=Model.Get())P->SetCitySearchIntent(T.ToString());})]
					+SVerticalBox::Slot().FillHeight(.55f)[SAssignNew(RouteList,SListView<TSharedPtr<FTradeRouteRow>>).ListItemsSource(&Rows).SelectionMode(ESelectionMode::None)
 .OnGenerateRow_Lambda([](TSharedPtr<FTradeRouteRow> Row,const TSharedRef<STableViewBase>& Owner){return SNew(STableRow<TSharedPtr<FTradeRouteRow>>,Owner).Padding(FMargin(0,3))[Row->Action.ToSharedRef()];})
 .OnItemScrolledIntoView_Lambda([this](TSharedPtr<FTradeRouteRow> Row,const TSharedPtr<ITableRow>&){if(Model.IsValid()&&Model->GetSnapshot().FocusedSemanticId==FName(*Row->SemanticId))FSlateApplication::Get().SetKeyboardFocus(Row->Action,EFocusCause::Navigation);})]
					+SVerticalBox::Slot().AutoHeight().Padding(0,12,0,6)[SNew(STextBlock).Text(LOCTEXT("Ports","Ports")).TextStyle(&HeadingStyle)]
 +SVerticalBox::Slot().FillHeight(.45f)[SAssignNew(PortScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(PortList,SVerticalBox)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[PagingNavigation.ToSharedRef()]]];
 for(auto W:{RoutesView,FleetView})W->SetButtonStyle(&DarkRowStyle);MapWidget(TEXT("TradeMap.Directory.More"),MoreMenu);
}
void STradeRegionalMap::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
Initialize(In);
OverlayAction=MakeControl(TEXT("TradeMap.Chart.Overlay"),LOCTEXT("MapOverlay","Routes / fleet"),EHansaUiButtonStyle::Secondary);
ThicknessAction=MakeControl(TEXT("TradeMap.Chart.Thickness"),LOCTEXT("MapThickness","Lines: 1×"),EHansaUiButtonStyle::Secondary);
SAssignNew(MapToolbar,SScrollBox).Orientation(Orient_Vertical);
for(auto W:{OverlayAction,ThicknessAction})MapToolbar->AddSlot().Padding(0,2)[W.ToSharedRef()];
for(const auto& Pair:TArray<TPair<FString,FText>>{{TEXT("NextRoute"),LOCTEXT("NextRoute","Next route")},{TEXT("PreviousCity"),LOCTEXT("PreviousCity","Previous city")},{TEXT("Focus"),LOCTEXT("Navigate","Navigate map")},{TEXT("NextCity"),LOCTEXT("NextCity","Next city")}})MapToolbar->AddSlot().Padding(0,2)[MakeControl(*(TEXT("TradeMap.Chart.")+Pair.Key),Pair.Value,EHansaUiButtonStyle::Secondary)];
auto Tools=SNew(SHorizontalBox);
for(const auto& Pair:TArray<TPair<FString,FText>>{{TEXT("ZoomIn"),FText::FromString(TEXT("+"))},{TEXT("ZoomOut"),FText::FromString(TEXT("−"))},{TEXT("Reset"),LOCTEXT("Fit","Fit region")}})Tools->AddSlot().AutoWidth().Padding(2)[MakeControl(*(TEXT("TradeMap.Chart.")+Pair.Key),Pair.Value,EHansaUiButtonStyle::Icon)];
Tools->AddSlot().AutoWidth().Padding(2)[SAssignNew(ToolsMenu,SComboButton).HasDownArrow(false).ButtonStyle(&IconButtonStyle).ButtonContent()[SNew(STextBlock).Text(LOCTEXT("Layers","Map tools")).TextStyle(&DarkBodyStyle)].MenuContent()[SNew(SBox).WidthOverride(260).MaxDesiredHeight(400)[MapToolbar.ToSharedRef()]]];
ChildSlot[SNew(SHansaReferenceFrame).Dark(true).Padding(5)[SNew(SOverlay)
 +SOverlay::Slot()[SAssignNew(CanvasHost,SBox)[SAssignNew(RouteCanvas,STradeRouteCanvas).Preferences(Preferences).Model(Model.Get())]]
 +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(8)[Tools]
 +SOverlay::Slot().VAlign(VAlign_Bottom).Padding(8)[SNew(SBorder).BorderImage(&OverlayBrush).Padding(4)[SAssignNew(MapSummary,STextBlock).TextStyle(&DarkBodyStyle).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]]]];
 MapWidget(TEXT("TradeMap.Chart.Tools"),ToolsMenu);MapWidget(TEXT("TradeMap.Chart.CanvasFocus"),RouteCanvas);
 MapWidget(TEXT("TradeMap.Chart.Summary"),MapSummary);
}
void STradeRouteEditor::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
Initialize(In);
ChildSlot[SNew(SVerticalBox)
+SVerticalBox::Slot().AutoHeight()[SAssignNew(SetupPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Plan","Plan your voyage")).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(LOCTEXT("Name","Route name · up to 48 characters")).TextStyle(&LightCaptionStyle)]
                            +SVerticalBox::Slot().AutoHeight()[SNew(SBox).MinDesiredHeight(48)[SAssignNew(RouteNameInput,SEditableTextBox).Style(&RouteNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences))
                                .OnTextChanged_Lambda([this](const FText& T){if(auto* P=Model.Get())P->SetRouteNameIntent(T.ToString());})]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(NameValidation,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ChooseShipHelp","Select a ship explicitly. Its current assignment and any reassignment are reviewed before departure.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Secondary)
                                .OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Creator.Cog")))
                                [SAssignNew(CogLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]]
+SVerticalBox::Slot().AutoHeight()[SAssignNew(EditPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text(LOCTEXT("Stops","Ordered stops")).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(LOCTEXT("ForeignSettlement","Foreign market loads buy goods; unloads sell them at the current price. Local transfers are free.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight()[SNew(SBox).MaxDesiredHeight(208)[SAssignNew(StopScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(StopList,SVerticalBox)]]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(StopValidation,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Editor.Visit"),LOCTEXT("VisitStop","Visit selected stop"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Pair(TEXT("TradeMap.Creator.City"),LOCTEXT("City","Change city"),TEXT("TradeMap.Creator.Good"),LOCTEXT("Good","Change good"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Editor.Action.Cycle"),LOCTEXT("Action","Next cargo action · home / market / station"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Editor.Quantity.Decrease"),LOCTEXT("QtyMinus","− 5 cargo"),TEXT("TradeMap.Editor.Quantity.Increase"),LOCTEXT("QtyPlus","+ 5 cargo"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Editor.Reserve.Decrease"),LOCTEXT("ReserveMinus","− 5 reserve"),TEXT("TradeMap.Editor.Reserve.Increase"),LOCTEXT("ReservePlus","+ 5 reserve"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Editor.Stop.Up"),LOCTEXT("Up","Move up"),TEXT("TradeMap.Editor.Stop.Down"),LOCTEXT("Down","Move down"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Creator.Add"),LOCTEXT("Add","+ Add stop"),TEXT("TradeMap.Creator.Remove"),LOCTEXT("Remove","Remove stop"))]]
+SVerticalBox::Slot().AutoHeight()[SAssignNew(ReviewPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(STextBlock).Text(LOCTEXT("Ready","Departure review")).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReviewText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]
+SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteDetailsPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(ReserveRisk,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteMetrics,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[MakeControl(TEXT("TradeMap.Editor.Save"),LOCTEXT("Save","Save changes"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Editor.Discard"),LOCTEXT("DiscardEdits","Discard unsaved edits"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(RouteStateCard,SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"))).Padding(10)[SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteStateHeading,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
                                +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(RouteStateDetail,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(ToggleActiveButton,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Editor.ToggleActive")))
                                [SAssignNew(ToggleActiveText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]]
                             +SVerticalBox::Slot().AutoHeight()[SAssignNew(ToggleActiveHint,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[MakeControl(TEXT("TradeMap.Editor.Cancel"),LOCTEXT("CancelRoute","Cancel route"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("CancelHint","Cancel when stopped with an empty hold. To keep cargo moving, resume and wait for unloading.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(EditorStatus,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]];
}
void STradePresence::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
 Initialize(In);
 ChildSlot[SAssignNew(PresencePanel,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(LegacyPresence,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(PresenceArt,SBox).WidthOverride(80).HeightOverride(80)[SNew(SImage).Image(TradeArtwork(TEXT("MerchantOffice"),false))]]
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8,0,0,0)[SAssignNew(PresenceProgressText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(PresenceUpgradeButton,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Presence.Upgrade")))]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(PresenceSourcePicker,SComboButton).ButtonStyle(&SecondaryButtonStyle).HasDownArrow(false).OnGetMenuContent_Lambda([this]{
    TSharedRef<SVerticalBox> List=SNew(SVerticalBox);if(Model.IsValid())for(const auto& C:Model->GetSnapshot().PresenceSources){const FString Id=C.Id;List->AddSlot().AutoHeight().Padding(0,2)[SNew(SBox).MinDesiredHeight(48)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).ToolTipText(C.Detail).OnClicked_Lambda([this,Id]{if(Model.IsValid())Model->SelectPresenceSource(Id);PresenceSourcePicker->SetIsOpen(false);return FReply::Handled();})[SNew(STextBlock).Text(C.Label).TextStyle(&LightBodyStyle).AutoWrapText(true)]]];}
    return SNew(SBox).WidthOverride(320).MaxDesiredHeight(280)[SNew(SScrollBox)+SScrollBox::Slot()[List]];
 }).ButtonContent()[SAssignNew(PresenceSourceLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SAssignNew(PresenceFundingText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(PresenceReviewText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(PresenceCancelButton,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("CancelOfficeReview","Edit review")).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Presence.Cancel")))]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(RequirementList,SVerticalBox)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(PresenceConsequencesText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SExpandableArea).Style(&DetailsAreaStyle).InitiallyCollapsed(true)
 .HeaderContent()[SNew(STextBlock).Text(LOCTEXT("PresenceHistory","Lawful contribution history")).TextStyle(&LightBodyStyle)]
 .BodyContent()[SAssignNew(PresenceHistoryText,STextBlock).TextStyle(&LightBodyStyle).LineHeightPercentage(1.3f).AutoWrapText(true)]]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Establishment,STradeEstablishment,In)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(TradeStationText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Details,STradeStationDetails,In)]];
 TradeStationButton=Establishment->Action;
 Footer=SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(UpgradeFooter,SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(UpgradeFooterConfirm,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Presence.Upgrade")))]
  +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(UpgradeFooterCancel,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("CancelOfficeReview","Edit review")).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Presence.Cancel")))]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(LegacyFooter,SBox)[Establishment->Footer.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight()[Details->Footer.ToSharedRef()];
 UpgradeFooter->SetVisibility(EVisibility::Collapsed);
}
void STradeOrders::ShowEditor(bool bShow) {
 bEditor=bShow;
 if(auto Scroll=PageScroll.Pin())Scroll->ScrollToStart();
 if(ListPanel)ListPanel->SetVisibility(bShow?EVisibility::Collapsed:EVisibility::Visible);
 if(EditorPanel)EditorPanel->SetVisibility(bShow?EVisibility::Visible:EVisibility::Collapsed);
}
void STradeOrders::SetCompact(bool Compact) {
 bCompact=Compact;
 if(ListHeading)ListHeading->SetVisibility(Compact?EVisibility::Collapsed:EVisibility::Visible);
 if(EditorHeading)EditorHeading->SetVisibility(EVisibility::Visible);
 if(OrderPicker)OrderPicker->SetVisibility(Compact?EVisibility::Collapsed:EVisibility::Visible);
}
void STradeOrders::RefreshRows(const FHansaTradeMapSnapshot& Snapshot) {
 if(!RowsPanel||!Model.IsValid())return;
 RowsPanel->ClearChildren();
 const auto& Rows=Model->GetStationOrderRows();
 StationOrderList->SetVisibility(EVisibility::Visible);
 for(const auto& Row:Rows){
  const FString SemanticId=FString::Printf(TEXT("TradeMap.Orders.Row.%llu"),Row.Id);
  const bool Selected=Snapshot.SelectedStationOrderId==int64(Row.Id);
  auto Cell=[this](const FText& Heading,const FText& Value)->TSharedRef<SWidget>{
   return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Heading).TextStyle(&LightCaptionStyle)]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Value).TextStyle(&LightBodyStyle).AutoWrapText(true)];
  };
  const FSlateBrush* GoodArt=Row.GoodId==TEXT("Good.Grain")?TradeIconArtwork(TEXT("GrainSack"),48):GetGeneratedIconBrush(GlyphForGood(Row.GoodId),40);
  auto Content=SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SBox).WidthOverride(40).HeightOverride(40)[SNew(SImage).Image(GoodArt)]]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(Row.Good).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(Row.State).TextStyle(&LightBodyStyle)]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[Cell(LOCTEXT("OrderSideColumn","Side"),Row.Side)]
    +SHorizontalBox::Slot().FillWidth(1)[Cell(LOCTEXT("OrderTargetColumn","Target / reserve"),Row.Target)]
    +SHorizontalBox::Slot().FillWidth(1)[Cell(LOCTEXT("OrderCapColumn","Cap / update"),Row.Cap)]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[Cell(LOCTEXT("OrderBudgetColumn","Remaining budget"),Row.Budget)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SNew(STextBlock)
    .Text(FText::Format(LOCTEXT("OrderReportResult","Report: {0} · Last: {1}"),Row.ReportAge,Row.LastResult))
    .TextStyle(&LightCaptionStyle).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
    .Text(FText::Format(LOCTEXT("OrderRemedy","Next: {0}"),Row.Remedy))
    .TextStyle(&LightCaptionStyle).AutoWrapText(true)];
  if(bCompact)Content=SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SBox).WidthOverride(32).HeightOverride(32)[SNew(SImage).Image(GetGeneratedIconBrush(GlyphForGood(Row.GoodId),32))]]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::Format(LOCTEXT("CompactOrderIdentity","{0} · {1}"),Row.Good,Row.Side)).TextStyle(&LightHeadingStyle)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(Row.State).TextStyle(&LightBodyStyle)]]
   +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[Cell(LOCTEXT("CompactOrderTarget","Target / reserve"),Row.Target)]
    +SHorizontalBox::Slot().FillWidth(1)[Cell(LOCTEXT("CompactOrderCap","Cap / update"),Row.Cap)]
    +SHorizontalBox::Slot().FillWidth(1)[Cell(LOCTEXT("CompactOrderBudget","Budget"),Row.Budget)]]
   +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
    .Text(FText::Format(LOCTEXT("CompactOrderOutcome","Report {0} · Last {1} · Next {2}"),Row.ReportAge,Row.LastResult,Row.Remedy))
    .TextStyle(&LightCaptionStyle).AutoWrapText(true)];
  auto Action=SNew(SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody)
   .Kind(EHansaUiButtonStyle::Secondary).OnClicked_Lambda([this,Id=Row.Id](){
    if(auto* P=Model.Get())if(P->SelectStationOrder(Id))ShowEditor(true);
    return FReply::Handled();
   });
  Action->SetFocusHandler(FSimpleDelegate::CreateLambda([Weak=Model,Name=FName(*SemanticId)](){if(auto* P=Weak.Get())P->SetFocusedSemanticId(Name);}));
  Action->SetButtonStyle(Selected?&SelectedOrderRowStyle:&OrderRowStyle);
  Action->SetState(Selected?EUiState::Selected:EUiState::Default);
  Action->SetContent(Content);
  Action->SetToolTipText(FText::Format(LOCTEXT("OrderRowTooltip","{0}, {1}. {2}. Report {3}. Last result {4}. Next {5}."),Row.Good,Row.Side,Row.State,Row.ReportAge,Row.LastResult,Row.Remedy));
  MapWidget(SemanticId,Action);
  RowsPanel->AddSlot().AutoHeight().Padding(0,0,0,6)[Action];
 }
}
void STradeFeedback::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
 Initialize(In);
 ChildSlot[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(SBox).MaxDesiredHeight(64)[SAssignNew(ValidationScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(ValidationText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(ReviewAction,SBox)[MakeControl(TEXT("TradeMap.Creator.Review"),LOCTEXT("Review","Review voyage"),EHansaUiButtonStyle::Primary)]]
 +SHorizontalBox::Slot().FillWidth(2)[SAssignNew(ConfirmActions,SBox)[Pair(TEXT("TradeMap.Creator.Edit"),LOCTEXT("Edit","Edit stops"),TEXT("TradeMap.Creator.Activate"),LOCTEXT("Activate","Create and activate"))]]
 +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[SAssignNew(DiscardAction,SBox)[MakeControl(TEXT("TradeMap.Creator.Discard"),LOCTEXT("Discard","Discard draft"),EHansaUiButtonStyle::Secondary)]]]
 +SVerticalBox::Slot().AutoHeight()[MakeControl(TEXT("TradeMap.Creator.Keep"),LOCTEXT("KeepDraft","Keep draft"),EHansaUiButtonStyle::Secondary)]];
}
void STradeShell::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In, TSharedRef<SWidget> Directory, TSharedRef<SWidget> RegionalMap, TSharedRef<SWidget> ContextHost, TSharedRef<SWidget> Schedule) {
 Initialize(In);ContentViews={Directory,RegionalMap,ContextHost,Schedule};
 SAssignNew(PageNavigation,SHorizontalBox);
 for(const TCHAR* Name:{TEXT("Map"),TEXT("Routes"),TEXT("Workspace"),TEXT("Schedule")}) {
  const FString Id=TEXT("TradeMap.Page.")+FString(Name);
  auto Button=MakeControl(*Id,FText::FromString(Name),EHansaUiButtonStyle::Secondary);
  PageButtons.Add(Name,Button);PageNavigation->AddSlot().FillWidth(1)[Button];
 }
 FullLayout=SNew(SHansaReferenceFrame).Dark(true).Padding(4)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(RouteTitle,STextBlock).TextStyle(&HeadingStyle).AutoWrapText(true)]
  +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SBox).MinDesiredWidth(140)[MakeControl(TEXT("TradeMap.New"),LOCTEXT("New","New route"),EHansaUiButtonStyle::Secondary)]]
  +SHorizontalBox::Slot().AutoWidth()[MakeControl(TEXT("TradeMap.Close"),LOCTEXT("Close","Close"),EHansaUiButtonStyle::Icon)]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(WorkspaceStatus,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[PageNavigation.ToSharedRef()]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(BodyHost,SBox)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SAssignNew(ScheduleHost,SBox)]];
 ChildSlot[FullLayout.ToSharedRef()];
 RefreshLayout(false,TEXT("Map"));
}
void STradeShell::RefreshLayout(bool Compact,const FString& SelectedPage) {
 if(Page==SelectedPage&&bCompact==Compact)return;
 bCompact=Compact;Page=SelectedPage;
 if(Page==TEXT("WorldStation")){BodyHost->SetContent(SNullWidget::NullWidget);ScheduleHost->SetContent(SNullWidget::NullWidget);ChildSlot[ContentViews[2]];return;}
 ChildSlot[FullLayout.ToSharedRef()];
 PageNavigation->SetVisibility(Compact&&Page!=TEXT("Ledger")&&Page!=TEXT("Specialization")?EVisibility::Visible:EVisibility::Collapsed);
 for(const auto& Entry:PageButtons)Entry.Value->SetState(Entry.Key==Page?EUiState::Selected:EUiState::Default);
 BodyHost->SetContent(SNullWidget::NullWidget);ScheduleHost->SetContent(SNullWidget::NullWidget);
 ScheduleHost->SetVisibility(Compact?EVisibility::Collapsed:EVisibility::Visible);
 if(Page==TEXT("Specialization")&&ContentViews.Num()>5){ScheduleHost->SetVisibility(EVisibility::Collapsed);BodyHost->SetContent(ContentViews[5]);return;}
 if(Page==TEXT("Ledger")&&ContentViews.Num()>4){ScheduleHost->SetVisibility(EVisibility::Collapsed);BodyHost->SetContent(ContentViews[4]);return;}
 if(Compact) {
  const int32 Index=Page==TEXT("Routes")?0:Page==TEXT("Workspace")?2:Page==TEXT("Schedule")?3:1;
  BodyHost->SetContent(ContentViews[Index]);
 } else {
  ScheduleHost->SetVisibility(EVisibility::Collapsed);
  BodyHost->SetContent(SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride_Lambda([this]{return FMath::Clamp(float(GetCachedGeometry().GetLocalSize().X)*.16f,240.f,330.f);})[ContentViews[0]]]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
    +SVerticalBox::Slot().FillHeight(1)[ContentViews[1]]
    +SVerticalBox::Slot().AutoHeight()[ContentViews[3]]]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride_Lambda([this]{const bool bOrders=Model.IsValid()&&Model->GetSnapshot().ActiveSection==TEXT("Orders");return bOrders?FMath::Clamp(float(GetCachedGeometry().GetLocalSize().X)*.35f,470.f,580.f):FMath::Clamp(float(GetCachedGeometry().GetLocalSize().X)*.2352f,350.f,440.f);})[ContentViews[2]]]);
 }
}
FText STradeRegionalMap::AccessibleSummary() const { return RouteCanvas->Summary(); }
void STradeRegionalMap::Refresh(const FTradeRegionalMapView& View) { RouteCanvas->SetSnapshot(View); MapSummary->SetText(RouteCanvas->OverlayLabel()); MapSummary->SetToolTipText(RouteCanvas->Summary()); }
void STradeRegionalMap::CycleOverlay() { RouteCanvas->CycleOverlay(); OverlayAction->SetLabel(RouteCanvas->OverlayLabel()); MapSummary->SetText(RouteCanvas->OverlayLabel()); MapSummary->SetToolTipText(RouteCanvas->Summary()); }
void STradeRegionalMap::CycleThickness() { RouteCanvas->CycleThickness(); ThicknessAction->SetLabel(RouteCanvas->ThicknessLabel()); }
bool STradeRegionalMap::FrameSelection() { return RouteCanvas->FrameSelection(); }
bool STradeRegionalMap::FocusCity(FName Id) { return RouteCanvas->FocusCity(Id); }
bool STradeRegionalMap::NextRoute(int32 Direction) { return RouteCanvas->NextRoute(Direction); }
TSharedRef<SWidget> STradeRegionalMap::FocusWidget() { return RouteCanvas.ToSharedRef(); }
FVector2D STradeRegionalMap::MarkerPosition(FName Id) const { return RouteCanvas->MarkerPosition(Id); }
void STradeRegionalMap::ChangeZoom(float Delta) { RouteCanvas->ChangeZoom(Delta); }
void STradeRegionalMap::ResetView() { RouteCanvas->ResetView(); }
void STradeContext::ShowSection(const FString& Section,bool Creating) {
 if(MoreLabel) {
  const TMap<FString,FText> Names={{TEXT("Orders"),LOCTEXT("MoreOrders","Orders")},{TEXT("Ledger"),LOCTEXT("MoreLedger","Ledger")},{TEXT("Construction"),LOCTEXT("MoreExpansion","Expansion")},{TEXT("Decisions"),LOCTEXT("MoreDecisions","Decisions")},{TEXT("Recovery"),LOCTEXT("MoreRecovery","Recovery")}};
  MoreLabel->SetText(Names.Contains(Section)?Names[Section]:LOCTEXT("MoreSections","More"));
 }
 if(RecoveryView)RecoveryView->SetVisibility(!Creating&&Section==TEXT("Recovery")?EVisibility::Visible:EVisibility::Collapsed);
 if(DecisionsView)DecisionsView->SetVisibility(!Creating&&Section==TEXT("Decisions")?EVisibility::Visible:EVisibility::Collapsed);
 if(ConstructionView)ConstructionView->SetVisibility(!Creating&&Section==TEXT("Construction")?EVisibility::Visible:EVisibility::Collapsed);
 OverviewPrimary->SetVisibility(!Creating&&(Section==TEXT("Overview")||Section==TEXT("Route"))?EVisibility::Visible:EVisibility::Collapsed);
 if(PresenceFooter)PresenceFooter->SetVisibility(!Creating&&(Section==TEXT("Presence")||Section==TEXT("StationUpgrade"))?EVisibility::Visible:EVisibility::Collapsed);
 if(OrdersView)OrdersView->SetVisibility(!Creating&&Section==TEXT("Orders")?EVisibility::Visible:EVisibility::Collapsed);
 const bool WorldStation=Model.IsValid()&&Model->bWorldStationDetail;
 CitySectionTabs->SetVisibility(WorldStation?EVisibility::Collapsed:EVisibility::Visible);
 WorldStationTabs->SetVisibility(WorldStation?EVisibility::Visible:EVisibility::Collapsed);
 WorldDetailsTab->SetState(Section==TEXT("Presence")?EUiState::Selected:EUiState::Default);
 WorldOrdersTab->SetState(Section==TEXT("Orders")?EUiState::Selected:EUiState::Default);
 WorldUpgradeTab->SetState(Section==TEXT("StationUpgrade")?EUiState::Selected:EUiState::Default);
 Navigation->SetVisibility(Creating||(!WorldStation&&Preferences.bLargeText&&Section==TEXT("Recovery"))?EVisibility::Collapsed:EVisibility::Visible);
 CityNavigation->SetVisibility(Creating||(Preferences.bLargeText&&Section==TEXT("Recovery"))?EVisibility::Collapsed:EVisibility::Visible);
 const TArray<TSharedPtr<SScrollBox>> Views=Scrolls();
 const FString VisibleSection=Section==TEXT("StationUpgrade")?TEXT("Presence"):Section==TEXT("Route")?TEXT("Overview"):Section;
 const TArray<FString> Names={TEXT(""),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders"),TEXT("Overview")};
 for(int32 I=0;I<Views.Num();++I)Views[I]->SetVisibility(Names[I]==VisibleSection?EVisibility::Visible:EVisibility::Collapsed);
}

void STradeDirectory::RefreshRows(const FHansaTradeMapSnapshot& S) {
 const auto& Entries=S.Directory;
 EmptyText->SetText(!S.bHasProjection?LOCTEXT("DirectoryLoading","Waiting for an authorized trade report."):S.DirectoryFilter==3?LOCTEXT("DirectoryDraft","Route drafts live in New route. Finish the current draft there."):LOCTEXT("DirectoryEmpty","No matches. Change filters or create a route."));
 EmptyText->SetVisibility(Entries.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);
 RoutesView->SetState(S.bFleetView?EUiState::Default:EUiState::Selected);
 FleetView->SetState(S.bFleetView?EUiState::Selected:EUiState::Default);
 const FText Filters[]={LOCTEXT("DirAll","All states"),LOCTEXT("DirActive","Active"),LOCTEXT("DirPaused","Paused"),LOCTEXT("DirDraft","Draft"),LOCTEXT("DirAttention","Needs attention"),LOCTEXT("DirAvailable","Available fleet"),LOCTEXT("DirPresence","Your presence"),LOCTEXT("DirGood","Selected good")};
 StatusFilter->SetLabel(Filters[FMath::Clamp(S.DirectoryFilter,0,7)]);
 for(auto W:{RoutesView,FleetView,StatusFilter}){W->SetEnabled(S.bCreating||!S.bDirty);W->SetToolTipText(LOCTEXT("DirDraftGuard","Save or discard route edits before changing directory views."));}
 const auto* R=Model.IsValid()?Model->GetSelectedRoutePresentation():nullptr;
 ToggleRoute->SetLabel(R?R->ToggleActionLabel:LOCTEXT("DirSelectRoute","Select a route"));
 ToggleRoute->SetEnabled(R&&R->bCanToggleActive&&!S.bCreating&&!S.bDirty);
 ToggleRoute->SetToolTipText(R?R->ToggleActionHint:LOCTEXT("DirChoose","Select an owned route first."));
 CancelRoute->SetEnabled(R&&R->bCanCancel&&!S.bCreating&&!S.bDirty);
 CancelRoute->SetToolTipText(LOCTEXT("DirCancelReason","Cancel only your route at a stop with an empty hold. Unload cargo first; cancellation never deletes cargo."));
 Locate->SetEnabled(!S.bCreating&&(S.SelectedRouteValue!=0||S.SelectedVehicleValue!=0));
 Recovery->SetEnabled(R&&R->bOwnedByPlayer&&!S.bCreating);
 Recovery->SetToolTipText(LOCTEXT("DirRecoveryHint","Review the selected route's station, preserved cargo and recovery requirements."));
 TArray<TSharedPtr<FTradeRouteRow>> Next;
 for(const auto& E:Entries) {
  const auto* Existing=Rows.FindByPredicate([&](const auto& Row){return Row->SemanticId==E.SemanticId;});
  auto Row=Existing?*Existing:MakeShared<FTradeRouteRow>();
  if(!Existing) {
   Row->Id=E.RouteValue;Row->SemanticId=E.SemanticId;
   Row->Action=MakeControl(*E.SemanticId,FText(),EHansaUiButtonStyle::Icon);Row->Action->SetButtonStyle(&DarkRowStyle);
   Row->Action->SetContent(SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,4,8,0)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(E.bSea?EUiGlyph::Ship:EUiGlyph::Road,24))]]
    +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Row->Text,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]);
  }
  const auto* Route=S.Routes.FindByPredicate([&](const auto& R){return R.RouteValue==E.RouteValue;});
  const FText Status=Route?Route->State:E.bAvailable?LOCTEXT("Available","Available"):LOCTEXT("Unassigned","No route");
  Row->Text->SetText(FText::Format(LOCTEXT("CompactDirectoryRow","{0}\n{1} · {2}"),E.Label,E.bOwned?LOCTEXT("Owned","Your house"):LOCTEXT("Rival","Rival"),Status));
  const bool Selected=S.bFleetView?E.VehicleValue==S.SelectedVehicleValue:E.RouteValue==S.SelectedRouteValue;
  Row->Action->SetButtonStyle(Selected?&DarkSelectedRowStyle:&DarkRowStyle);Row->Action->SetState(Selected?EUiState::Selected:EUiState::Default);
  Row->Action->SetEnabled(E.SemanticId==TEXT("TradeMap.Directory.Draft")||(!S.bCreating&&(!S.bDirty||Selected)));
  Row->Action->SetToolTipText(FText::Format(LOCTEXT("DirectoryFullDetail","{0}\n{1}"),E.Detail,E.Alert));
  Next.Add(Row);
 }
 if(Rows!=Next) {Rows=MoveTemp(Next);RouteList->RequestListRefresh();}
 TArray<FName> CurrentPorts;for(const auto& C:S.Cities)CurrentPorts.Add(C.StableId);
 if(CurrentPorts!=PortIds){PortIds=CurrentPorts;PortList->ClearChildren();PortActions.Reset();for(const auto& C:S.Cities){
  const FString Id=TEXT("TradeMap.Port.")+C.StableId.ToString();auto B=MakeControl(*Id,C.Label,EHansaUiButtonStyle::Icon);B->SetButtonStyle(&DarkRowStyle);
  B->SetContent(SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(EUiGlyph::Harbor,24))]]+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(C.Label).TextStyle(&DarkBodyStyle)]);
  PortActions.Add(C.StableId,B);PortList->AddSlot().AutoHeight()[B];
 }}
 for(const auto& Port:PortActions){Port.Value->SetState(Port.Key==S.SelectedCityStableId?EUiState::Selected:EUiState::Default);Port.Value->SetButtonStyle(Port.Key==S.SelectedCityStableId?&DarkSelectedRowStyle:&DarkRowStyle);}
}
void STradeDirectory::Reveal(const FString& Id) {
 if(Id.StartsWith(TEXT("TradeMap.Directory.")))for(auto W:{RoutesView,FleetView,StatusFilter,ModeFilterAction,CityFilterAction,GoodFilterAction,ToggleRoute,CancelRoute,Locate,Recovery})if(W->HasKeyboardFocus())DirectoryToolbar->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::IntoView);
 for(const auto& Row:Rows)if(Id==Row->SemanticId){RouteList->RequestScrollIntoView(Row);break;}
}
void STradeRouteEditor::RefreshStops(const TArray<FHansaTradeMapStopPresentation>& Stops,int32 Selected) {
 // Stops are bounded by the existing creator contract; preserve controls by stable semantic slot.
 bool StructureChanged=StopActions.Num()!=Stops.Num();
 for(const auto& Stop:Stops) {
  auto& Action=StopActions.FindOrAdd(Stop.Index);
  if(!Action) { const FString Id=FString::Printf(TEXT("TradeMap.Stop.%d"),Stop.Index);Action=MakeControl(*Id,Stop.AccessibleLabel,EHansaUiButtonStyle::Secondary);StructureChanged=true; }
  if(Selected!=RevealedStop&&Stop.Index==Selected&&StopScroll){StopScroll->ScrollDescendantIntoView(Action,false);RevealedStop=Selected;}
  Action->SetLabel(Stop.AccessibleLabel);Action->SetState(Stop.Index==Selected?EUiState::Selected:EUiState::Default);
 }
 if(StructureChanged) {
  StopList->ClearChildren();
  for(auto It=StopActions.CreateIterator();It;++It)if(!Stops.ContainsByPredicate([&](const auto& Stop){return Stop.Index==It.Key();}))It.RemoveCurrent();
  for(const auto& Stop:Stops)StopList->AddSlot().AutoHeight().Padding(0,3)[StopActions[Stop.Index].ToSharedRef()];
 }
}

void STradePresence::Refresh(const FTradePresenceView& S) {
 if(Model.IsValid())Establishment->Refresh(Model->GetSnapshot().Establishment);
 TradeStationText->SetText(FText::Format(LOCTEXT("StationInspectorBody","{0}\n{1}"),S.TradeStationState,S.TradeStationDetail));
 if(!Model.IsValid())return;
 const auto& State=Model->GetSnapshot();const auto& E=State.Establishment;
 const bool Completed=E.bVisible&&E.bComplete;
 Details->SetVisibility(Completed?EVisibility::Visible:EVisibility::Collapsed);
 Details->Footer->SetVisibility(Completed?EVisibility::Visible:EVisibility::Collapsed);
 LegacyFooter->SetVisibility(Completed?EVisibility::Collapsed:EVisibility::Visible);
 if(Completed){
  LegacyPresence->SetVisibility(EVisibility::Collapsed);Establishment->SetVisibility(EVisibility::Collapsed);
  TradeStationText->SetVisibility(EVisibility::Collapsed);UpgradeFooter->SetVisibility(EVisibility::Collapsed);
  Details->Refresh();return;
 }
 const bool CompactUpgrade=Model->bWorldStationDetail&&Model->IsAutomaticMerchantOfficeUpgrade();
 const bool UpgradeReview=Model->IsAutomaticMerchantOfficeUpgrade()&&State.bPresenceReview;
 UpgradeFooter->SetVisibility(UpgradeReview?EVisibility::Visible:EVisibility::Collapsed);
 UpgradeFooterConfirm->SetLabel(State.PresenceUpgradeAction);UpgradeFooterConfirm->SetEnabled(State.bCanPresenceUpgradeAction);
 Establishment->Footer->SetVisibility(UpgradeReview||!E.bVisible?EVisibility::Collapsed:EVisibility::Visible);
 Establishment->SetVisibility(UpgradeReview||!E.bVisible?EVisibility::Collapsed:EVisibility::Visible);
 MapWidget(TEXT("TradeMap.Presence.Upgrade"),UpgradeReview?UpgradeFooterConfirm:PresenceUpgradeButton);
 MapWidget(TEXT("TradeMap.Presence.Cancel"),UpgradeReview?UpgradeFooterCancel:PresenceCancelButton);
 MapWidget(TEXT("TradeMap.Presence.Footer"),UpgradeFooter);
 const bool ProgressMet=!State.PresenceRequirements.ContainsByPredicate([](const auto& R){return R.RequirementId!=TEXT("AvailableMoney")&&!R.RequirementId.StartsWith(TEXT("UpgradeGood."))&&!R.bMet;});
 LegacyPresence->SetVisibility((Model->bWorldStationDetail&&!Model->IsAutomaticMerchantOfficeUpgrade())||(E.bVisible&&!E.bComplete)?EVisibility::Collapsed:EVisibility::Visible);
 TradeStationButton->SetVisibility(E.bReview?EVisibility::Collapsed:EVisibility::Visible);
 TradeStationButton->SetToolTipText(E.Blocker);
 TradeStationText->SetVisibility(Model->GetLedgerPresentation().bAvailable||(E.bVisible&&!E.bComplete)?EVisibility::Collapsed:EVisibility::Visible);
 PresenceArt->SetVisibility(State.bPresenceOfficeVisual&&!State.bCompact&&!Preferences.bLargeText?EVisibility::Visible:EVisibility::Collapsed);PresenceProgressText->SetText(State.PresenceStageSummary.IsEmpty()?S.PresenceProgress:State.PresenceStageSummary);PresenceConsequencesText->SetText(State.PresenceConsequences);
 PresenceFundingText->SetText(State.PresenceFundingDetail);
 PresenceFundingText->SetVisibility(State.PresenceFundingDetail.IsEmpty()||UpgradeReview?EVisibility::Collapsed:EVisibility::Visible);
 PresenceReviewText->SetText(State.PresenceReview);
 PresenceReviewText->SetVisibility(State.bPresenceReview||(!CompactUpgrade&&!State.PresenceReview.IsEmpty())?EVisibility::Visible:EVisibility::Collapsed);
 RequirementList->SetVisibility(CompactUpgrade&&ProgressMet?EVisibility::Collapsed:EVisibility::Visible);
 PresenceConsequencesText->SetVisibility(CompactUpgrade?EVisibility::Collapsed:EVisibility::Visible);
 PresenceCancelButton->SetVisibility(State.bPresenceReview&&!UpgradeReview?EVisibility::Visible:EVisibility::Collapsed);
 PresenceSourcePicker->SetVisibility(State.PresenceSources.IsEmpty()||UpgradeReview?EVisibility::Collapsed:EVisibility::Visible);
 PresenceSourcePicker->SetEnabled(!State.PresenceSources.IsEmpty()&&!State.bPresenceReview);
 const auto* Chosen=State.PresenceSources.FindByPredicate([&](const auto& X){return X.Id==State.PresenceSourceId;});
 PresenceSourceLabel->SetText(Chosen?FText::Format(LOCTEXT("ChosenPresenceSource","Source: {0}"),Chosen->Label):LOCTEXT("ChoosePresenceSource","Choose funding source"));
 PresenceSourcePicker->SetToolTipText(Chosen?Chosen->Detail:LOCTEXT("PresenceSourceHint","Choose an owned station, home, or ship inventory. Review its stock and treasury cost."));
 FString NewKey=State.PresenceStageSummary.ToString()+TEXT("|")+State.PresenceSourceId+TEXT("|")+State.PresenceFundingDetail.ToString();for(const auto& R:State.PresenceRequirements)NewKey+=FString::Printf(TEXT("|%s:%lld:%lld:%d"),*R.RequirementId,R.CurrentValue,R.RequiredValue,R.bMet);
 if(RequirementsKey!=NewKey){
  RequirementsKey=NewKey;RequirementList->ClearChildren();
  for(const auto& R:State.PresenceRequirements){
   const bool Quantity=R.RequirementId==TEXT("LawfulTradeVolume")||R.RequirementId==TEXT("ShortageRelief")||R.RequirementId.StartsWith(TEXT("UpgradeGood."));
   const bool Money=R.RequirementId==TEXT("AvailableMoney")||R.RequirementId==TEXT("TransactionValue")||R.RequirementId==TEXT("QualifyingInvestment");
   const bool Time=R.RequirementId.Contains(TEXT("Operation"));
   const FText Unit=Quantity?LOCTEXT("Units","units"):Money?LOCTEXT("Pfennig","pfennig"):LOCTEXT("Deliveries","deliveries");
   const FText Value=Time?FText::Format(LOCTEXT("CalendarRequirement","{0} · {1}"),PresenceTimeProgress(R.CurrentValue,R.RequiredValue,E.MinutesPerTick),R.bMet?LOCTEXT("Met","Met"):LOCTEXT("Unmet","Unmet")):FText::Format(LOCTEXT("RequirementValue","{0} / {1} {2} · {3}"),FText::AsNumber(double(R.CurrentValue)/(Quantity?1000.:1.)),FText::AsNumber(double(R.RequiredValue)/(Quantity?1000.:1.)),Unit,R.bMet?LOCTEXT("Met","Met"):LOCTEXT("Unmet","Unmet"));
   FText Remedy=LOCTEXT("GenericRemedy","Continue lawful trade and station operation to meet this condition.");
   if(R.RequirementId.StartsWith(TEXT("UpgradeGood.")))Remedy=LOCTEXT("GoodsRemedy","Stock the selected owned funding inventory with this construction material.");
   else if(R.RequirementId==TEXT("AvailableMoney"))Remedy=LOCTEXT("MoneyRemedy","Earn treasury money before funding. The investment is paid at confirmation.");
   else if(R.RequirementId==TEXT("LawfulTradeVolume"))Remedy=LOCTEXT("TradeVolumeSource","Recorded volume from accepted lawful trade. Continue legitimate market and route transactions in this city.");
   else if(R.RequirementId==TEXT("TransactionValue"))Remedy=LOCTEXT("TransactionValueSource","Settled value of lawful transactions in this city. Trade goods through permitted routes and markets.");
   else if(R.RequirementId==TEXT("ShortageRelief"))Remedy=LOCTEXT("ShortageReliefSource","Deliver goods that fulfill a recorded local shortage. Inspect the city market report and route cargo.");
   else if(R.RequirementId==TEXT("QualifyingInvestment"))Remedy=LOCTEXT("InvestmentSource","Qualifying money invested in local commercial presence is recorded here. Review station and office investments.");
   else if(R.RequirementId.Contains(TEXT("Delivery")))Remedy=LOCTEXT("DeliveryRemedy","Complete lawful deliveries into this city.");
   else if(R.RequirementId.Contains(TEXT("Operation")))Remedy=LOCTEXT("OperationRemedy","Keep the local station active and solvent over time.");
   auto Row=SNew(SHorizontalBox).ToolTipText(Remedy)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(R.bMet?EUiGlyph::Check:EUiGlyph::Warning,24))]]
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Time?(R.RequirementId.Contains(TEXT("Reliable"))?LOCTEXT("ReliableCalendar","Reliable operation"):LOCTEXT("SolventCalendar","Solvent operation")):FText::FromString(R.Description.Replace(TEXT("Good."),TEXT("")))).TextStyle(&LightBodyStyle).AutoWrapText(true)]
     +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Value).TextStyle(&LightCaptionStyle).AutoWrapText(true)]];
   if(!R.bMet){const FString LocateId=TEXT("TradeMap.Presence.Locate.")+R.RequirementId;const bool Stock=R.RequirementId.StartsWith(TEXT("UpgradeGood."))||R.RequirementId==TEXT("AvailableMoney");
    TSharedPtr<SButton> Locate;Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4,0,0,0)[SAssignNew(Locate,SButton).ButtonStyle(&SecondaryButtonStyle).ContentPadding(FMargin(5,2)).ToolTipText(Remedy).OnClicked(this,&STradeComponent::Invoke,LocateId)[SNew(STextBlock).Text(Stock?LOCTEXT("ViewStock","Stock"):LOCTEXT("ViewRoutes","Locate")).TextStyle(&LightCaptionStyle)]];MapWidget(LocateId,Locate); }
   RequirementList->AddSlot().AutoHeight().Padding(0,3)[Row];
   MapWidget(TEXT("TradeMap.Presence.Requirement.")+R.RequirementId,Row);
  }
 }
 PresenceHistoryText->SetText(State.PresenceHistory);
 PresenceUpgradeButton->SetLabel(State.PresenceUpgradeAction);PresenceUpgradeButton->SetEnabled(State.bCanPresenceUpgradeAction);
 PresenceUpgradeButton->SetVisibility(UpgradeReview||(E.bVisible&&!E.bComplete)?EVisibility::Collapsed:EVisibility::Visible);
}
void STradeDirectory::Refresh(const FTradeDirectoryView& S) {
 const auto DetailVisibility=EVisibility::Collapsed;
 DirectoryTitle->SetVisibility(EVisibility::Visible);ModeText->SetVisibility(DetailVisibility);CityModeText->SetVisibility(DetailVisibility);GoodModeText->SetVisibility(DetailVisibility);
 PagingNavigation->SetVisibility(EVisibility::Collapsed);
 ModeFilterAction->SetLabel(S.ModeFilter==EHansaTradeMapModeFilter::All?LOCTEXT("All","All routes"):S.ModeFilter==EHansaTradeMapModeFilter::Sea?LOCTEXT("SeaOnly","Sea routes"):LOCTEXT("LandOnly","Land routes"));
 CityFilterAction->SetLabel(S.CityFilter==EHansaTradeMapCityFilter::All?LOCTEXT("AllCities","All cities"):S.CityFilter==EHansaTradeMapCityFilter::Presence?LOCTEXT("PresenceCities","Your presence"):LOCTEXT("RouteCities","Route cities"));
 GoodFilterAction->SetLabel(FText::FromString(S.PreferredGoodStableId.ToString().Replace(TEXT("Good."),TEXT(""))));
		if (CitySearchInput->GetText().ToString() != S.CitySearchText) CitySearchInput->SetText(FText::FromString(S.CitySearchText));

        RouteList->SetEnabled(true);

		ModeText->SetText(S.ModeFilter==EHansaTradeMapModeFilter::All?LOCTEXT("All","All routes"):S.ModeFilter==EHansaTradeMapModeFilter::Sea?LOCTEXT("SeaOnly","Sea routes"):LOCTEXT("LandOnly","Land routes"));

		CityModeText->SetText(FText::Format(LOCTEXT("CityModeSummary","{0} · {1} matching cities"),S.CityFilter==EHansaTradeMapCityFilter::All?LOCTEXT("AllCities","All cities"):S.CityFilter==EHansaTradeMapCityFilter::Presence?LOCTEXT("PresenceCities","Your presence"):LOCTEXT("RouteCities","Route cities"),FText::AsNumber(S.MatchingCityCount)));

		GoodModeText->SetText(FText::Format(LOCTEXT("SelectedGoodSummary","Selected good: {0} · reports retain age"),FText::FromName(S.PreferredGoodStableId)));

}

void STradeFeedback::Refresh(const FTradeFeedbackView& S) {
 SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);
 ReviewAction->SetVisibility(S.bCreating&&!S.bReview?EVisibility::Visible:EVisibility::Collapsed);
 ConfirmActions->SetVisibility(S.bCreating&&S.bReview?EVisibility::Visible:EVisibility::Collapsed);
 DiscardAction->SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);
        ValidationText->SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);

        ValidationText->SetText(S.Validation);
}

void STradeRouteEditor::Refresh(const FTradeRouteEditorView& S, const FHansaTradeMapRoutePresentation* R) {
		RouteMetrics->SetText(R?FText::Format(LOCTEXT("Metrics","{0}\n{1}\nCapacity: {2}\nUpkeep: {3}\nExpected cash result: {4}\n{5}"),R->StopSummary,R->RoundTripTime,R->Capacity,R->Upkeep,R->ExpectedProfitRange,R->Uncertainty):FText());
		ReserveRisk->SetText(S.ReserveRisk);
		RouteStateHeading->SetText(R?R->StateHeading:FText());
		RouteStateDetail->SetText(R?R->StateDetail:FText());
		ToggleActiveText->SetText(R?R->ToggleActionLabel:FText());
		ToggleActiveHint->SetText(R?R->ToggleActionHint:FText());
		ToggleActiveButton->SetEnabled(R&&R->bCanToggleActive);

		// The action keeps one primary style; its label communicates pause/resume.
		const FLinearColor StateColor=!R?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment)
			:R->bTraveling?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticBlue)
			:R->bActive?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal)
			:R->bOwnedByPlayer?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment)
			:UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk);
		RouteStateCard->SetBorderBackgroundColor(StateColor.CopyWithNewOpacity(R&&R->bActive?.24f:.36f));
		EditorStatus->SetText(S.EditorStatus);EditorStatus->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);

        if (RouteNameInput->GetText().ToString() != S.DraftName) RouteNameInput->SetText(FText::FromString(S.DraftName));

        SetupPanel->SetVisibility(S.bCreating&&!S.bReview?EVisibility::Visible:EVisibility::Collapsed);

        ReviewPanel->SetVisibility(S.bCreating&&S.bReview?EVisibility::Visible:EVisibility::Collapsed);

 CogLabel->SetText(S.CogLabel);
 if(auto* P=Model.Get()) {
 const auto& Draft=P->GetSnapshot();
 NameValidation->SetText(Draft.Validation);StopValidation->SetText(Draft.Validation);
 NameValidation->SetVisibility(Draft.bCreating&&!Draft.bCanCreate&&Draft.ValidationTarget==TEXT("TradeMap.Creator.Name")?EVisibility::Visible:EVisibility::Collapsed);
 StopValidation->SetVisibility(Draft.bCreating&&!Draft.bCanCreate&&Draft.ValidationTarget!=TEXT("TradeMap.Creator.Name")?EVisibility::Visible:EVisibility::Collapsed);
 }

        ReviewText->SetText(FText::Format(LOCTEXT("NamedReview","{0}\n{1}\n\n{2}"),FText::FromString(S.DraftName),S.CogLabel,S.CreatorReview));

}

}
#undef LOCTEXT_NAMESPACE
