#pragma once
#include "CoreMinimal.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaUiComponents.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
class SBorder;
class SImage;
class SComboButton;
class SProgressBar;
class SBox;
class STextBlock;
class SVerticalBox;
class SEditableTextBox;
class SHorizontalBox;
class SOverlay;
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/SListView.h"
namespace Hansa::UI {
class STradeRouteCanvas;
class STradeJourneyTimeline;
// Immutable map input: no order, office, validation, or creator state is retained here.
struct FTradeRegionalMapView {
 TArray<FHansaTradeMapCityPresentation> Cities;
 TArray<FHansaTradeMapStopPresentation> Stops;
 TArray<FHansaTradeMapRoutePresentation> Routes;
 int64 SelectedRouteValue=0; FName GoodId; bool bCreating=false;
 FName SelectedCityStableId; FVector2D ShipPosition; bool bShipInTransit=false;
};
struct FTradeComponentContext {
 FUiPreferences Preferences;
 TWeakObjectPtr<UHansaTradeMapPresentationModel> Model;
 TFunction<FReply(const FString&)> Invoke;
 TFunction<void(const FString&,TSharedPtr<SWidget>)> Register;
};
class STradeComponent : public SCompoundWidget {
public:
 void Initialize(const TSharedRef<FTradeComponentContext>& In);
 FReply Invoke(FString Id) { return Context->Invoke(Id); }
 TSharedRef<SHansaAction> MakeControl(const TCHAR* Id,const FText& Label,EHansaUiButtonStyle Kind);
 TSharedRef<SWidget> Pair(const TCHAR* A,const FText& AL,const TCHAR* B,const FText& BL);
 void MapWidget(const FString& Id,TSharedPtr<SWidget> W) { Context->Register(Id,W); }
protected:
 TSharedPtr<FTradeComponentContext> Context;
 FUiPreferences Preferences; TWeakObjectPtr<UHansaTradeMapPresentationModel> Model;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FButtonStyle IconButtonStyle;
		FSlateBrush WorkingBrush;
		FSlateBrush FloatingBrush;
		FSlateBrush OverlayBrush;
		FTextBlockStyle LightBodyStyle;
		FTextBlockStyle LightCaptionStyle;
		FTextBlockStyle DarkBodyStyle;
		FTextBlockStyle LightHeadingStyle;
		FTextBlockStyle HeadingStyle;
FEditableTextBoxStyle RouteNameStyle;
 FExpandableAreaStyle DetailsAreaStyle;
 FButtonStyle DarkRowStyle, DarkSelectedRowStyle, LightRowStyle;
};
struct FTradeRouteRow {
 FString SemanticId; int64 Id=0; TSharedPtr<SHansaAction> Action; TSharedPtr<STextBlock> Text;
};
struct FTradeDirectoryView {
 FString CitySearchText;
 EHansaTradeMapModeFilter ModeFilter;
 EHansaTradeMapCityFilter CityFilter;
 int32 MatchingCityCount;
 FName PreferredGoodStableId;
 bool bCreating;
 bool bCompact;
 bool bPaging;
};
class STradeDirectory final : public STradeComponent {
public:
 void Refresh(const FTradeDirectoryView& S);
 SLATE_BEGIN_ARGS(STradeDirectory){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
TSharedPtr<STextBlock> EmptyText,DirectoryTitle;
TSharedPtr<SWidget> PagingNavigation;
TSharedPtr<SHansaAction> ModeFilterAction,CityFilterAction,GoodFilterAction;
TSharedPtr<SEditableTextBox> CitySearchInput; TSharedPtr<STextBlock> ModeText,CityModeText,GoodModeText; TSharedPtr<SListView<TSharedPtr<FTradeRouteRow>>> RouteList;
 TArray<TSharedPtr<FTradeRouteRow>> Rows;
 void RefreshRows(const FHansaTradeMapSnapshot& Snapshot);
 TSharedPtr<SHansaAction> StatusFilter,RoutesView,FleetView,ToggleRoute,CancelRoute,Locate,Recovery;
 TSharedPtr<SScrollBox> DirectoryToolbar; TSharedPtr<SComboButton> MoreMenu;
 TSharedPtr<SVerticalBox> PortList; TArray<FName> PortIds; TSharedPtr<SScrollBox> PortScroll; TMap<FName,TSharedPtr<SHansaAction>> PortActions;
 void Reveal(const FString& Id);

};
class STradeRegionalMap final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeRegionalMap){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
TSharedPtr<SBox> CanvasHost; TSharedPtr<STradeRouteCanvas> RouteCanvas;
FText AccessibleSummary() const;
void Refresh(const FTradeRegionalMapView& View); void ChangeZoom(float Delta); void ResetView();
bool FrameSelection();
void CycleOverlay(); void CycleThickness(); bool FocusCity(FName Id); bool NextRoute(int32 Direction);
TSharedRef<SWidget> FocusWidget(); FVector2D MarkerPosition(FName Id) const;
TSharedPtr<SScrollBox> MapToolbar; TSharedPtr<SComboButton> ToolsMenu;
TSharedPtr<STextBlock> MapSummary; TSharedPtr<SHansaAction> OverlayAction,ThicknessAction;
};
struct FTradeRouteEditorView {
 FString DraftName;
 bool bCreating;
 bool bReview;
 FText CogLabel;
 FText CreatorReview;
 FText ReserveRisk;
 FText EditorStatus;
};
class STradeRouteEditor final : public STradeComponent {
public:
 void Refresh(const FTradeRouteEditorView& S, const FHansaTradeMapRoutePresentation* R);
 SLATE_BEGIN_ARGS(STradeRouteEditor){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
TSharedPtr<STextBlock> NameValidation,StopValidation;
TSharedPtr<SScrollBox> StopScroll; int32 RevealedStop=INDEX_NONE;
TSharedPtr<SEditableTextBox> RouteNameInput; TSharedPtr<STextBlock> CogLabel,ReviewText,EditorStatus,ReserveRisk,RouteMetrics,RouteStateHeading,RouteStateDetail,ToggleActiveText,ToggleActiveHint; TSharedPtr<SVerticalBox> SetupPanel,EditPanel,ReviewPanel,RouteDetailsPanel,StopList; TMap<int32,TSharedPtr<SHansaAction>> StopActions;
 void RefreshStops(const TArray<FHansaTradeMapStopPresentation>& Stops,int32 Selected);
 TSharedPtr<SBorder> RouteStateCard; TSharedPtr<SButton> ToggleActiveButton;

};
struct FTradeLedgerItem { FHansaTradeLedgerRow Data; TSharedPtr<SHansaAction> Action; TArray<TSharedPtr<STextBlock>> Cells; };
class STradeLedger final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeLedger){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
 void Refresh(const FHansaTradeLedger&);
 bool Intent(const FString&);
 bool Reveal(const FString&);
 TArray<FString> FocusOrder() const;
 FText AccessibleDetail() const;
 void Scroll(float Direction);
 FTableViewStyle LedgerListStyle;
 FHansaTradeLedger View;
 TArray<TSharedPtr<FTradeLedgerItem>> Rows;
 TSharedPtr<SScrollBox> Horizontal,DetailsScroll;
private:
 void Rebuild();
 TSharedRef<ITableRow> MakeRow(TSharedPtr<FTradeLedgerItem>,const TSharedRef<STableViewBase>&);
 TArray<FText> Cells(const FHansaTradeLedgerRow&) const;
 TSharedPtr<SListView<TSharedPtr<FTradeLedgerItem>>> List;
 TSharedPtr<SWidget> StockPanel,DetailsPanel;
 TSharedPtr<STextBlock> Title,Summary,Detail,Empty;
 TSharedPtr<SVerticalBox> OperationCards;
 TArray<TSharedPtr<STextBlock>> CardValues;
 TSharedPtr<SHansaAction> ColumnsButton;
 TSharedPtr<SHansaAction> FilterButton,OrdersButton,RouteButton;
 TMap<FString,TSharedPtr<SHansaAction>> Tabs;
 FName SelectedGood;int32 Filter=0;FString Page=TEXT("Stock"),PendingFocus;
};
class STradeEstablishment final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeEstablishment){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
 void Refresh(const FHansaTradeEstablishment&);
 bool ToggleTerms();
 TSharedPtr<SWidget> Footer;
 TSharedPtr<SHansaAction> Action;
private:
 TSharedRef<SWidget> ChoiceMenu(bool Site);
 TSharedRef<ITableRow> ChoiceRow(TSharedPtr<FHansaEstablishmentChoice>,const TSharedRef<STableViewBase>&,bool Site);
 TArray<TSharedPtr<FHansaEstablishmentChoice>> Sites,Sources;
 TSharedPtr<STextBlock> Step,Summary,SourceDetail,Review,Feedback,SiteLabel,SourceLabel;
 TSharedPtr<STextBlock> SiteStatus,StorageValue,BuildValue,UpkeepValue,RequirementsTitle,RequirementsStatus;
 TSharedPtr<STextBlock> TreasuryValue,CostValue,RemainderValue,InvestmentText,NoticeText,BlockerText,ActionText,ConstructionText,CostTitle,RemainderTitle;
 TSharedPtr<SWidget> SiteCard,SiteArt,RequirementSection,FundingCard,FundingHint,InvestmentRow,Notice,Autonomy,ConstructionCard,MarketSlot,SourcePanel;
 TSharedPtr<SImage> SiteImage;
 TSharedPtr<SVerticalBox> RequirementRows;
 TSharedPtr<SProgressBar> ConstructionBar;
 TSharedPtr<SHansaGlyph> ActionIcon,InvestmentIcon;
 TSharedPtr<SHansaAction> Market,ShowOnMap;
 TArray<TSharedPtr<SBorder>> StepBadges;
 TArray<TSharedPtr<STextBlock>> StepLabels;
 FSlateBrush CardBrush,NoticeBrush,RuleBrush,CurrentStepBrush,FutureStepBrush;
 FProgressBarStyle RequirementProgressStyle;
 FTextBlockStyle CaptionStyle,DataStyle;
 FString RequirementKey;
 TSharedPtr<SWidget> Terms;
 bool bTermsExpanded=false;
 TSharedPtr<SComboButton> SitePicker,SourcePicker;
 TSharedPtr<SHansaAction> Confirm,Cancel,Close,Priority;
};
struct FTradePresenceView {
 FText TradeStationState;
 FText TradeStationDetail;
 FText TradeStationAction;
 FText PresenceProgress;
 FText PresenceUpgradeAction;
 bool bCanTradeStationAction;
 bool bCanPresenceUpgradeAction;
};
class STradeStationDetails;
class STradePresence final : public STradeComponent {
public:
 void Refresh(const FTradePresenceView& S);
 SLATE_BEGIN_ARGS(STradePresence){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
TSharedPtr<STradeEstablishment> Establishment;
TSharedPtr<STradeStationDetails> Details;
TSharedPtr<SWidget> LegacyFooter;
TSharedPtr<SWidget> Footer;
TSharedPtr<SVerticalBox> UpgradeFooter;
TSharedPtr<SHansaAction> UpgradeFooterConfirm,UpgradeFooterCancel;
TSharedPtr<SVerticalBox> PresencePanel,RequirementList,LegacyPresence; TSharedPtr<SBox> PresenceArt; FString RequirementsKey; TSharedPtr<STextBlock> TradeStationText,PresenceProgressText,PresenceConsequencesText,PresenceFundingText,PresenceHistoryText,PresenceReviewText,PresenceSourceLabel; TSharedPtr<SHansaAction> TradeStationButton,PresenceUpgradeButton,PresenceCancelButton; TSharedPtr<SComboButton> PresenceSourcePicker;

};
class STradeSpecialization final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeSpecialization){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
 void Refresh(const FHansaTradeMapSnapshot&);
 TArray<FString> FocusOrder() const;
 TSharedPtr<SVerticalBox> SpecializationPanel;
 TSharedPtr<SScrollBox> Scroll;
private:
 TSharedPtr<SBox> CardsHost;
 TSharedPtr<SWidget> ReviewPanel;
 TSharedPtr<STextBlock> Heading,Summary,Feedback,Review,SourceLabel;
 TSharedPtr<SComboButton> SourcePicker;
 TSharedPtr<SHansaAction> Apply,Confirm,Cancel,Back;
 TMap<FString,TSharedPtr<SHansaAction>> Choices;
 TMap<FString,TSharedPtr<SWidget>> Cards;
 TMap<FString,TSharedPtr<STextBlock>> Values,States,Recommendations;
 bool bCompactCards=false,bLayoutReady=false;
};

class STradeOrders final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeOrders){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
 void ShowEditor(bool bShow);
 void RefreshEditor(const FHansaTradeMapSnapshot& Snapshot);
 void SetPageScroll(const TSharedPtr<SScrollBox>& Scroll) { PageScroll=Scroll; }
 void SetCompact(bool bCompact);
 void RefreshRows(const FHansaTradeMapSnapshot& Snapshot);
 bool IsEditor() const { return bEditor; }
 bool OpenGoodMenu();
 bool IsGoodMenuOpen() const;
 void CloseGoodMenu();
 TSharedPtr<SScrollBox> ListScroll,EditorScroll;
 TSharedPtr<SOverlay> StationOrdersPanel;
 TSharedPtr<SVerticalBox> ListPanel,EditorPanel,RowsPanel;
 TSharedPtr<SBox> NewOrderControl;
 TSharedPtr<STextBlock> ListHeading,EditorHeading,StationOrderText,StationOrderList,StationOrderFeedback;
 TSharedPtr<SEditableTextBox> TargetInput,CapInput,BudgetInput;
 TSharedPtr<SComboButton> OrderPicker;
private:
 TSharedPtr<STextBlock> GoodLabel,StockLabel,ModeHint,TargetLabel,TargetHint,RateLabel,RateHint,RateUnit,ReportHeading,PriceLabel,AgeLabel,LimitLabel,SummaryLabel,ActivityLabel,FooterHint,DraftLabel;
 TSharedPtr<SImage> CommodityImage;
 TSharedPtr<class SProgressBar> StockBar;
 TSharedPtr<SBorder> MarketCard;
 TSharedPtr<SWidget> BudgetRow,ExistingActions;
 TSharedPtr<SComboButton> GoodPicker;
 TSharedPtr<SHansaAction> BuyAction,SellAction,SaveAction,PauseAction,CancelAction;
 FSlateBrush CardBrush,WarningBrush,RuleBrush,FooterBrush;
 FTextBlockStyle OrderCaptionStyle,OrderTitleStyle,OrderValueStyle,OrderEditorHeadingStyle;
 FProgressBarStyle StockBarStyle;
 FButtonStyle OrderPrimaryStyle,OrderSelectedStyle,OrderSecondaryStyle;
 bool bEditor=false,bCompact=false;
 TWeakPtr<SScrollBox> PageScroll;
 FButtonStyle OrderRowStyle,SelectedOrderRowStyle;
};
struct FTradeFeedbackView {
 bool bCreating;
 bool bReview;
 FText Validation;
};
class STradeFeedback final : public STradeComponent {
public:
 void Refresh(const FTradeFeedbackView& S);
 SLATE_BEGIN_ARGS(STradeFeedback){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
TSharedPtr<STextBlock> ValidationText; TSharedPtr<SScrollBox> ValidationScroll; TSharedPtr<SBox> ReviewAction,ConfirmActions,DiscardAction;

};

struct FTradeScheduleListRow { FHansaTradeScheduleRow Data; TSharedPtr<SHansaAction> Action; TSharedPtr<STextBlock> Text; TArray<TSharedPtr<STextBlock>> Columns; TSharedPtr<SImage> StatusIcon; };
class STradeSchedule final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeSchedule){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In);
 void Refresh(const FHansaTradeSchedulePresentation& S);
 void SetCompact(bool Compact);
 void SetOrdersPreview(bool bOrders);
 bool SelectTab(const FString& Tab);
 bool Reveal(const FString& Id);
 bool bCompactSchedule=false,bOrdersPreview=false;
 TSharedPtr<SWidget> TableHeader;
 FString Group=TEXT("Cargo"),PendingFocus,SelectedRow;
 FHansaTradeSchedulePresentation View;
 TArray<TSharedPtr<FTradeScheduleListRow>> Rows;
 TMap<FString,TSharedPtr<SHansaAction>> TabButtons;
 TMap<FString,float> Offsets;
 TSharedPtr<SListView<TSharedPtr<FTradeScheduleListRow>>> List;
 TSharedPtr<STextBlock> ScheduleText,EmptyText,JourneyHeading,JourneyIdentity; TSharedPtr<SHansaAction> ChooseOwned,OpenJourney;
 FButtonStyle ManifestRowStyle,ManifestSelectedStyle,ManifestTabStyle,ManifestSelectedTabStyle;
 FTextBlockStyle ManifestGoodStyle;
 FProgressBarStyle ProgressStyle;
 FTableViewStyle ScheduleListStyle;
 TSharedPtr<STradeJourneyTimeline> JourneyProgress;
 TSharedPtr<SBox> ScheduleBounds;
};
class STradeContext final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeContext){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In, const TArray<TSharedRef<SWidget>>& Views);
TSharedPtr<STextBlock> SelectedCityText,SelectedRouteText; TSharedPtr<SWidget> CityNavigation; TSharedPtr<SVerticalBox> Navigation; TSharedPtr<SScrollBox> RouteEditorScroll,PresenceScroll,SpecializationScroll,OrdersScroll;
TSharedPtr<SScrollBox> OverviewScroll; TSharedPtr<STextBlock> OverviewInfo,OverviewReport,OverviewAccess; TSharedPtr<SBox> CityArt,OverviewPrimary; TSharedPtr<SWidget> RecoveryView;
 TSharedPtr<SWidget> OrdersView; TSharedPtr<SWidget> ConstructionView; TSharedPtr<SWidget> DecisionsView;
 FName DisplayedCity; TMap<FName,float> CityOverviewOffsets;
 TSharedPtr<SComboButton> SectionMenu;
 TSharedPtr<SWidget> CitySectionTabs,WorldStationTabs;
 TSharedPtr<SHansaAction> WorldDetailsTab,WorldOrdersTab,WorldUpgradeTab;
 TSharedPtr<SScrollBox> SectionMenuScroll;
 TSharedPtr<STextBlock> MoreLabel,WarningTitle,WarningDetail,GoodLabel,PriceValue,StockValue,RoutesValue,ShipsValue,LeaseTitle,SiteSummary,PresenceHint,ConstructionStatus,PresenceActionLabel;
 TSharedPtr<SHansaGlyph> GoodIcon;
 TSharedPtr<SImage> ReportIcon;
 TSharedPtr<SWidget> NoticeCard,PresenceArt,OverviewBody;
 TSharedPtr<SWidget> OverviewFooter;
 TSharedPtr<SWidget> PresenceFooter;
 TSharedPtr<STextBlock> AccessValues[3];
 TSharedPtr<SHansaGlyph> AccessIcons[3];
 FSlateBrush QuoteBrush,NoticeBrush,RuleBrush;
 FTextBlockStyle OverviewCaptionStyle,OverviewValueStyle;
 FButtonStyle OverviewPrimaryStyle;
 TSharedRef<SWidget> BuildOverview();
 void RefreshOverview(const FHansaTradeMapCityPresentation* City);
 void ShowSection(const FString& Section,bool Creating); TArray<TSharedPtr<SScrollBox>> Scrolls() const { return {RouteEditorScroll,PresenceScroll,SpecializationScroll,OrdersScroll,OverviewScroll}; }
};
class STradeShell final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeShell){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In, TSharedRef<SWidget> Directory, TSharedRef<SWidget> RegionalMap, TSharedRef<SWidget> ContextHost, TSharedRef<SWidget> Schedule);
TSharedPtr<STextBlock> RouteTitle,WorkspaceStatus;
 TSharedPtr<SBox> BodyHost,ScheduleHost;
 TSharedPtr<SWidget> FullLayout;
 TSharedPtr<SHorizontalBox> PageNavigation;
 TArray<TSharedRef<SWidget>> ContentViews;
 TMap<FString,TSharedPtr<SHansaAction>> PageButtons;
 bool bCompact=false;
 FString Page;
 void RefreshLayout(bool Compact,const FString& SelectedPage);


};
}
