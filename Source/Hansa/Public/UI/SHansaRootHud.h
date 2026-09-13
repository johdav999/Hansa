#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"

#include "UI/HansaHudLayout.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaUiComponents.h"

class SScrollBox;
class SBorder;
class SBox;
class SButton;
class STextBlock;
class SVerticalBox;
class UHansaFrontendPresentationModel;
namespace Hansa::UI {class SHansaFrontend;}
class UHansaBuildMenuPresentationModel;
class UHansaCityOverviewPresentationModel;
class UHansaInspectorPresentationModel;
class UHansaMarketTablePresentationModel;
class UHansaResearchPresentationModel;
class UHansaScenarioPresentationModel;
class UHansaSaveLoadPresentationModel;
class UHansaTradeMapPresentationModel;
class AHansaStrategyPlayerController;
struct FHansaCityOverviewSnapshot;
struct FHansaScenarioPresentationSnapshot;
struct FHansaSaveLoadPresentationSnapshot;
struct FHansaTradeMapSnapshot;
struct FHansaResearchPresentationSnapshot;
namespace Hansa::UI { class SHansaBuildMenu; }
namespace Hansa::UI { class SHansaCityOverview; }
namespace Hansa::UI { class SHansaContextInspector; }
namespace Hansa::UI { class SHansaResearchScreen; }
namespace Hansa::UI { class SHansaTradeMap; }
namespace Hansa::UI { class SHansaScenarioScreen; class SHansaSessionCoach; }
namespace Hansa::UI { class SHansaSaveLoadScreen; }

namespace Hansa::UI
{
	/** Native, event-refreshed root HUD. The empty center remains hit-test transparent to the city view. */
	class HANSA_API SHansaRootHud final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaRootHud)
			: _FrontendModel(nullptr), _Model(nullptr), _BuildModel(nullptr), _InspectorModel(nullptr), _CityOverviewModel(nullptr), _MarketTableModel(nullptr), _TradeMapModel(nullptr), _ResearchModel(nullptr), _ScenarioModel(nullptr), _SaveLoadModel(nullptr), _PlacementController(nullptr), _InitialViewportSize(FIntPoint(1280, 720)) {}
			SLATE_ARGUMENT(UHansaFrontendPresentationModel*, FrontendModel)
			SLATE_ARGUMENT(UHansaHudPresentationModel*, Model)
			SLATE_ARGUMENT(UHansaBuildMenuPresentationModel*, BuildModel)
			SLATE_ARGUMENT(UHansaInspectorPresentationModel*, InspectorModel)
			SLATE_ARGUMENT(UHansaCityOverviewPresentationModel*, CityOverviewModel)
			SLATE_ARGUMENT(UHansaMarketTablePresentationModel*, MarketTableModel)
			SLATE_ARGUMENT(UHansaTradeMapPresentationModel*, TradeMapModel)
			SLATE_ARGUMENT(UHansaResearchPresentationModel*, ResearchModel)
			SLATE_ARGUMENT(UHansaScenarioPresentationModel*, ScenarioModel)
			SLATE_ARGUMENT(UHansaSaveLoadPresentationModel*, SaveLoadModel)
			SLATE_ARGUMENT(AHansaStrategyPlayerController*, PlacementController)
			SLATE_ARGUMENT(FIntPoint, InitialViewportSize)
			SLATE_ARGUMENT(FUiPreferences, Preferences)
		SLATE_END_ARGS()

		~SHansaRootHud();
		void Construct(const FArguments& Arguments);

		TSharedPtr<SHansaBuildMenu> GetConstructionMenu() const { return BuildMenuWidget; }
		void SetPreferences(FUiPreferences InPreferences);
		FUiPreferences GetPreferences() const { return Preferences; }
		TSharedPtr<SHansaCityOverview> GetCityOverview() const { return CityOverviewWidget; }
        TSharedPtr<SHansaContextInspector> GetInspector() const { return InspectorWidget; }
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const;
		void SetPresentationSize(FIntPoint Size);
		[[nodiscard]] TSharedRef<SWidget> GetCaptureWidget() const;
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
		virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

	private:
		void UnbindModels();
        TWeakObjectPtr<UHansaFrontendPresentationModel> FrontendModel;
        TSharedPtr<SHansaFrontend> FrontendWidget;
        FReply HandleSessionOpen();
        TSharedPtr<SHansaSessionCoach> SessionCoach;
        TSharedPtr<SButton> SessionButton;
		void QueuePreferencesChange(FUiPreferences Value);
		FIntPoint PhysicalViewportSize{1280,720};
		void Refresh(const FHansaHudPresentationSnapshot& Snapshot, uint64 Revision);
		void RefreshInspectorHost(const FHansaInspectorSnapshot& Snapshot, uint64 Revision);
		void RefreshCityOverviewHost(const FHansaCityOverviewSnapshot& Snapshot, uint64 Revision);
		void RefreshTradeMapHost(const FHansaTradeMapSnapshot& Snapshot, uint64 Revision);
		void RefreshResearchHost(const FHansaResearchPresentationSnapshot& Snapshot, uint64 Revision);
		void RefreshScenarioHost(const FHansaScenarioPresentationSnapshot& Snapshot, uint64 Revision);
		void RefreshSaveLoadHost(const FHansaSaveLoadPresentationSnapshot& Snapshot, uint64 Revision);
		void RestoreFocusFromScenario(FName SemanticId);
		void RestoreFocusFromSaveLoad(FName SemanticId);
		void RebuildAlerts(const FHansaHudPresentationSnapshot& Snapshot);
		void RebuildNotifications(const FHansaHudPresentationSnapshot& Snapshot);
		void RecordNativeFocus(FName Id);
		void MapWidget(const TCHAR* SemanticId, const TSharedPtr<SWidget>& Widget);
		FReply HandleAlertToggle();
		FReply HandleAlertAction(FName AlertId, EHansaHudAlertAction Action);
		void RestoreFocusFromInspector(FName SemanticId);
		void RestoreFocusFromCityOverview(FName SemanticId);
		void RestoreFocusFromTradeMap(FName SemanticId);
		void RestoreFocusFromResearch(FName SemanticId);
		FReply HandleCityOverviewOpen();
		FReply HandleTradeMapOpen();
		FReply HandleResearchOpen();
		FReply HandleSaveLoadOpen();
		FReply HandleBottomToggle();
		FReply HandleSpeed(EHansaHudGameSpeed Speed, const TCHAR* SemanticId);
		FSlateColor FocusColor(const TCHAR* SemanticId) const;
		void ApplyResponsiveLayout();
		void UpdateFocusIndicator(FName SemanticId);

		TWeakObjectPtr<UHansaHudPresentationModel> Model;
		TWeakObjectPtr<UHansaBuildMenuPresentationModel> BuildModel;
		TWeakObjectPtr<UHansaInspectorPresentationModel> InspectorModel;
		TWeakObjectPtr<UHansaCityOverviewPresentationModel> CityOverviewModel;
		TWeakObjectPtr<UHansaMarketTablePresentationModel> MarketTableModel;
		TWeakObjectPtr<UHansaTradeMapPresentationModel> TradeMapModel;
		TWeakObjectPtr<UHansaResearchPresentationModel> ResearchModel;
		TWeakObjectPtr<UHansaScenarioPresentationModel> ScenarioModel;
		TWeakObjectPtr<UHansaSaveLoadPresentationModel> SaveLoadModel;
		TWeakObjectPtr<AHansaStrategyPlayerController> PlacementController;
		FDelegateHandle ModelChangedHandle;
		FDelegateHandle InspectorFocusRestoreHandle;
		FDelegateHandle InspectorChangedHandle;
		FDelegateHandle CityOverviewChangedHandle;
		FDelegateHandle CityOverviewFocusRestoreHandle;
		FDelegateHandle TradeMapChangedHandle;
		FDelegateHandle TradeMapFocusRestoreHandle;
		FDelegateHandle ResearchChangedHandle;
		FDelegateHandle ResearchFocusRestoreHandle;
		FDelegateHandle ScenarioChangedHandle;
		FDelegateHandle ScenarioFocusRestoreHandle;
		FDelegateHandle SaveLoadChangedHandle;
		FDelegateHandle SaveLoadFocusRestoreHandle;
		FArguments RebuildArguments;
        FUiPreferences Preferences;
        TArray<FHansaHudAlertPresentation> PresentedAlerts;
        TArray<FHansaHudNotificationPresentation> PresentedNotifications;
        TSharedPtr<SScrollBox> AlertScroll;
        FHansaHudLayoutMetrics Layout;
		uint64 PresentedRevision = 0;

		FSlateBrush WorldOverlayBrush;
		FSlateBrush WorkingBrush;
		FSlateBrush FloatingBrush;
		FButtonStyle IconButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FTextBlockStyle DarkBodyStyle;
		FTextBlockStyle DarkDataStyle;
		FTextBlockStyle DarkCaptionStyle;
		FTextBlockStyle LightBodyStyle;
		FTextBlockStyle LightHeadingStyle;

		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SBox> TopStatusBox;
		TSharedPtr<SBox> AlertHostBox;
		TSharedPtr<SBox> AlertRowsHostBox;
		TSharedPtr<SBox> BottomHostBox;
		TSharedPtr<SBox> BuildMenuHostBox;
		TSharedPtr<SBox> InspectorHostBox;
		TSharedPtr<SBox> NotificationHostBox;
		SOverlay::FOverlaySlot* TopStatusSlot = nullptr;
		SOverlay::FOverlaySlot* AlertSlot = nullptr;
		SOverlay::FOverlaySlot* BottomSlot = nullptr;
		SOverlay::FOverlaySlot* BuildMenuSlot = nullptr;
		SOverlay::FOverlaySlot* InspectorSlot = nullptr;
		SOverlay::FOverlaySlot* NotificationSlot = nullptr;
		SOverlay::FOverlaySlot* FocusSlot = nullptr;
		SOverlay::FOverlaySlot* CityOverviewSlot = nullptr;
		SOverlay::FOverlaySlot* TradeMapSlot = nullptr;
		SOverlay::FOverlaySlot* ResearchSlot = nullptr;
		SOverlay::FOverlaySlot* ScenarioSlot = nullptr;
		SOverlay::FOverlaySlot* SaveLoadSlot = nullptr;
		TSharedPtr<SWidget> ScreenWidget;
		TSharedPtr<SWidget> TopStatusWidget;
        TSharedPtr<SWidget> TopLeftPanel, TopCenterPanel, TopRightPanel;
		TSharedPtr<STextBlock> MoneyText;
		TSharedPtr<STextBlock> MoneyTrendText;
		TSharedPtr<STextBlock> PopulationText;
        TSharedRef<SWidget> BuildTopMenu();
        TSharedPtr<STextBlock> WealthyText;
        TSharedPtr<STextBlock> ProductTexts[3];
        TSharedPtr<SHansaGlyph> ProductGlyphs[3];
        TSharedPtr<SWidget> ProductChips[3];
        TSharedPtr<SWidget> MoneyChip, TrendChip, PopulationChip, LaborerChip, WealthyChip;
		TSharedPtr<STextBlock> WorkforceText;
		TSharedPtr<STextBlock> CityBreadcrumbText;
		TSharedPtr<SButton> CityOverviewButton;
		TSharedPtr<SButton> TradeMapButton;
        TSharedPtr<SButton> ReturnCityButton;
		TSharedPtr<SButton> SaveLoadButton;
		TSharedPtr<STextBlock> DateText;
		TSharedPtr<STextBlock> ResearchText;
		TSharedPtr<SButton> ResearchButton;
		TSharedPtr<STextBlock> ConnectionText;
		TSharedPtr<STextBlock> FpsText;
		double FpsSampleElapsedSeconds = 0.0;
		int32 FpsSampleFrameCount = 0;
		TSharedPtr<SWidget> SpeedGroupWidget;
		TSharedPtr<SButton> PauseButton;
		TSharedPtr<SButton> NormalButton;
		TSharedPtr<SButton> FastButton;
		TSharedPtr<SButton> FastestButton;
		TSharedPtr<SBorder> PauseFocus;
		TSharedPtr<SBorder> NormalFocus;
		TSharedPtr<SBorder> FastFocus;
		TSharedPtr<SBorder> FastestFocus;
		TSharedPtr<SWidget> AlertStackWidget;
		TSharedPtr<SButton> AlertToggleButton;
		TSharedPtr<STextBlock> AlertToggleText;
		TSharedPtr<SVerticalBox> AlertRows;
		TSharedPtr<SWidget> PinnedTrackersWidget;
		TSharedPtr<SVerticalBox> PinnedTrackerRows;
		TSharedPtr<SWidget> BottomAreaWidget;
		TSharedPtr<SButton> BottomToggleButton;
		TSharedPtr<STextBlock> SelectionText;
		TSharedPtr<SWidget> InspectorHostWidget;
		TSharedPtr<SHansaContextInspector> InspectorWidget;
		TSharedPtr<SWidget> CityOverviewHostWidget;
		TSharedPtr<SHansaCityOverview> CityOverviewWidget;
		TSharedPtr<SWidget> TradeMapHostWidget;
		TSharedPtr<SWidget> ResearchHostWidget;
		TSharedPtr<SHansaResearchScreen> ResearchWidget;
		TSharedPtr<SWidget> ScenarioHostWidget;
		TSharedPtr<SWidget> SaveLoadHostWidget;
		TSharedPtr<SHansaScenarioScreen> ScenarioWidget;
		TSharedPtr<SHansaSaveLoadScreen> SaveLoadWidget;
		TSharedPtr<SHansaTradeMap> TradeMapWidget;
		TSharedPtr<SWidget> NotificationLayerWidget;
		TSharedPtr<SVerticalBox> NotificationRows;
		TSharedPtr<SWidget> FocusLayerWidget;
		TSharedPtr<STextBlock> FocusText;
		TSharedPtr<SHansaBuildMenu> BuildMenuWidget;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
		TMap<FString, TWeakPtr<SWidget>> AlertSemanticWidgets;
	};
}
