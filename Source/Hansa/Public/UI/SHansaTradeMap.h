#pragma once

#include "CoreMinimal.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaUiComponents.h"

class SBorder;
class SBox;
class SScrollBox;
class SButton;
class STextBlock;
class SVerticalBox;
class SEditableTextBox;

namespace Hansa::UI
{
    class STradeRecovery;
    class STradeDecisions;
    class STradeConstruction;
    class STradeDirectory;
    class STradeRegionalMap;
    class STradeShipDetail;
class STradeRouteEditor;
    class STradePresence;
    class STradeLedger;
    class STradeSpecialization;
    class STradeOrders;
    class STradeFeedback;
    class STradeSchedule;
    class STradeContext;
    class STradeShell;
	class HANSA_API SHansaTradeMap final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaTradeMap) : _Model(nullptr), _InitialViewportSize(FIntPoint(1280,720)) {}
			SLATE_ARGUMENT(FUiPreferences, Preferences)
			SLATE_ARGUMENT(UHansaTradeMapPresentationModel*, Model)
			SLATE_ARGUMENT(FIntPoint, InitialViewportSize)
		SLATE_END_ARGS()
		~SHansaTradeMap();
		void Construct(const FArguments& Arguments);
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const { const auto* W=SemanticWidgets.Find(Id); return W?W->Pin():nullptr; }
		void SetPresentationSize(FIntPoint Size);
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		virtual bool SupportsKeyboardFocus() const override { return true; }
        virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	private:
		bool DispatchIntent(const FString& Id);
        FString FeedbackKind;
        FText WorkspaceFeedback;
        void RefreshWorkspaceStatus();
		void Refresh(const FHansaTradeMapSnapshot& Snapshot, uint64 Revision);
		void RebuildRoutes(const FHansaTradeMapSnapshot& Snapshot);
		void RebuildStops(const FHansaTradeMapSnapshot& Snapshot);
		void MapWidget(const FString& Id, const TSharedPtr<SWidget>& Widget);
		void ApplySectionVisibility();
        FReply Invoke(const FString Id);
        TArray<TSharedPtr<SScrollBox>> GetScrollRegions() const;
        TWeakObjectPtr<UHansaTradeMapPresentationModel> Model;
        FDelegateHandle ChangedHandle;
        FUiPreferences Preferences;
        FIntPoint PresentationSize{1280,720};
        TSharedPtr<SBox> PresentationBox;
        TMap<FString,TWeakPtr<SWidget>> SemanticWidgets;
        TSharedPtr<STradeRecovery> Recovery;
        TSharedPtr<STradeDecisions> Decisions;
        TSharedPtr<STradeConstruction> Construction;
        TSharedPtr<STradeDirectory> Directory;
        TSharedPtr<STradeRegionalMap> RegionalMap;
        TSharedPtr<STradeRouteEditor> RouteEditor;
    TSharedPtr<STradeShipDetail> ShipDetail;
        TSharedPtr<STradePresence> Presence;
        TSharedPtr<STradeLedger> Ledger;
        TSharedPtr<STradeSpecialization> Specialization;
        TSharedPtr<STradeOrders> Orders;
        TSharedPtr<STradeFeedback> Feedback;
        TSharedPtr<STradeSchedule> Schedule;
        TSharedPtr<STradeContext> ContextHost;
        TSharedPtr<STradeShell> Shell;

	};
}
