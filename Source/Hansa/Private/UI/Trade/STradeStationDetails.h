#pragma once
#include "HansaTradeWorkspaceComponents.h"

namespace Hansa::UI {
/** Reference-faithful shared inspector for completed stations and offices. */
class STradeStationDetails final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeStationDetails){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
 void Refresh();
 void CloseTerms() {bLeaseOpen=false;}
 bool ToggleTerms();
 bool IsLeaseOpen() const {return bLeaseOpen;}
 TSharedPtr<SWidget> Footer;
private:
 TSharedRef<SWidget> Text(const FText&,bool Heading=false);
 TSharedRef<SWidget> Rule();
 TSharedRef<SWidget> Metric(const FText&,EUiGlyph,TSharedPtr<STextBlock>&);
 TSharedRef<SWidget> LedgerValue(const FText&,TSharedPtr<STextBlock>&,bool Strong=false);
 TSharedRef<SHansaAction> Control(const TCHAR*,const FText&,EUiGlyph,EHansaUiButtonStyle=EHansaUiButtonStyle::Secondary);
 TSharedRef<SWidget> SourceMenu();
 void RefreshMaterials(const FHansaEstablishmentChoice*);
 void RefreshRights(const FHansaTradeEstablishment&);
 void RefreshOverview();
 FSlateBrush CardBrush,RuleBrush,DarkBrush;
 FTextBlockStyle CaptionStyle,ValueStyle;
 TSharedPtr<SVerticalBox> Main,Lease,Upgrade,Materials,Rights,Conditions;
 TSharedPtr<SVerticalBox> Overview,Goods,TransportRows,ActivityRows;
 TSharedPtr<SWidget> LocalTabs,TradingWarning;
 TSharedPtr<STextBlock> TradingSummary,TradingBlocker,UpgradeSummary,UpkeepSummary;
 TSharedPtr<SHansaAction> OverviewTab,UpgradeTab,Market,Stock,OrdersLink;
 TArray<FName> GoodsIds;
 TArray<TSharedPtr<STextBlock>> GoodsAvailable,GoodsReserved;
 float StorageFraction=0;
 TSharedPtr<SWidget> Banner,RefundNotice,UpgradeReviewNotice,UpgradeHost,SourceHost,OfficeCard,ReviewFooter,NormalFooter,MoneyHost;
 TSharedPtr<STextBlock> UpgradeHeading,ReviewTerms;
 TSharedPtr<SHansaAction> ReviewConfirm,ReviewEdit;
 TSharedPtr<STextBlock> SiteName,Storage,Upkeep,Paid,UpgradeBenefit,Duration,Treasury,Cost,Remainder,SourceLabel,Delivery,Feedback,LeaseState,Refund;
 TSharedPtr<SComboButton> SourcePicker;
 TSharedPtr<SHansaAction> UpgradeAction,EditReview,Terms,Closure,Operations,ShowMap,Return;
 TArray<FString> MaterialIds,RightIds;
 TArray<TSharedPtr<STextBlock>> MaterialRequired,MaterialAvailable,RightStates;
 TArray<TSharedPtr<SHansaGlyph>> MaterialStatus,RightIcons;
 int64 DisplayedStation=0;
 bool bLeaseOpen=false;
};
}
