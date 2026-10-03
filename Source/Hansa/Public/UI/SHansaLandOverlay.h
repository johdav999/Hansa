#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Queries/HansaLandQuery.h"
#include "Network/HansaLandQueryTransport.h"
#include "UI/HansaUiComponents.h"
#include "World/HansaLandOverlayGeometry.h"
#include "World/HansaLandSurveyView.h"

class AHansaStrategyPlayerController;
class AHansaLandOverlayRenderer;
class STextBlock;

namespace Hansa::UI
{
/** Session-only view state. All land facts come from the bounded viewer query. */
class HANSA_API FHansaLandOverlayUiState final
{
public:
    explicit FHansaLandOverlayUiState(AHansaStrategyPlayerController* InController);
    ~FHansaLandOverlayUiState();
    void ToggleLand();
    void SetMode(Hansa::Game::LandOverlay::EMode NewMode);
    void ClosePanel() { bPanelOpen = false; }
    void CloseSelection();
    bool SelectWorldHit(const FHitResult& Hit);
    void FrameSelection();
    void Update();
    bool IsPanelOpen() const { return bPanelOpen; }
    bool HasSelection() const { return bSelectionOpen; }
    bool CanFrameSelection() const { return bSelectionOpen && Survey.IsReady() && SelectedRegion>0; }
    bool IsActive() const { return Mode != Hansa::Game::LandOverlay::EMode::Off; }
    Hansa::Game::LandOverlay::EMode GetMode() const { return Mode; }
    /** Temporary construction view; the explicit Land mode is never overwritten. */
    bool IsPlacementAssistanceActive() const { return bPlacementAssistanceActive; }
    Hansa::Game::LandOverlay::EMode GetRenderedMode() const
    { return bPlacementAssistanceActive ? Hansa::Game::LandOverlay::EMode::Buildable : Mode; }
    TOptional<FIntPoint> GetPlacementAssistanceCell() const { return PlacementAssistanceCell; }
    const Hansa::Simulation::FHansaLandCellView& GetSelectedView() const { return SelectedView; }
    const FString& GetCityLabel() const { return CityLabel; }
    FText GetOwnerText() const;
    FText GetAccessText() const;
    FText GetReasonText() const;
    FText GetSelectionHelpText() const;
    FText GetAvailabilityText() const { return Availability; }
    bool IsAvailable() const { return bAvailable; }
    const Hansa::Game::LandOverlay::FSurveyView& GetSurvey() const { return Survey; }
    const FTransform& GetGridTransform() const { return SurveyGrid; }
    int32 GetSelectedRegion() const { return bSelectionOpen ? SelectedRegion : 0; }
    const TArray<FVector>& GetCameraFootprint() const { return CameraFootprint; }
    bool bHighContrast = false;
private:
    TWeakObjectPtr<AHansaStrategyPlayerController> Controller;
    TWeakObjectPtr<AHansaLandOverlayRenderer> Renderer;
    Hansa::Game::LandOverlay::EMode Mode = Hansa::Game::LandOverlay::EMode::Off;
    Hansa::Game::LandOverlay::EMode LastMode = Hansa::Game::LandOverlay::EMode::Buildable;
    Hansa::Simulation::FHansaLandCellView SelectedView;
    EHansaLandViewStatus SelectedStatus = EHansaLandViewStatus::Unavailable;
    FIntPoint SelectedCell = FIntPoint::ZeroValue;
    Hansa::Game::LandOverlay::FSurveyView Survey;
    FTransform SurveyGrid;
    TArray<FVector> CameraFootprint;
    int32 SelectedRegion=0;
    double LastSurveyConfirmation=-10;
    uint64 SurveyEpoch=0;
    TSharedPtr<const TArray<Hansa::Game::LandOverlay::FBoundary>> SelectedBoundaries;
    uint64 SelectionRevision=0;
    FString CityLabel;
    FName ActiveCity;
    FText Availability;
    bool bPanelOpen = false;
    bool bSelectionOpen = false;
    bool bAvailable = false;
    bool bPlacementAssistanceActive = false;
    TOptional<FIntPoint> PlacementAssistanceCell;
};

class HANSA_API SHansaLandPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaLandPanel) {}
        SLATE_ARGUMENT(TSharedPtr<FHansaLandOverlayUiState>, State)
        SLATE_ARGUMENT(FUiPreferences, Preferences)
        SLATE_EVENT(FSimpleDelegate, OnClosed)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    TSharedPtr<SWidget> Resolve(const FString& Id) const;
    bool Activate(const FString& Id);
private:
    EActiveTimerReturnType Refresh(double, float);
    TSharedPtr<FHansaLandOverlayUiState> State;
    FSimpleDelegate OnClosed;
    TSharedPtr<SHansaAction> BuildableButton, OwnershipButton, CloseButton;
    TSharedPtr<SWidget> BuildableLegend, OwnershipLegend;
    TSharedPtr<STextBlock> AvailabilityText;
};

class HANSA_API SHansaLandInspector final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaLandInspector) {}
        SLATE_ARGUMENT(TSharedPtr<FHansaLandOverlayUiState>, State)
        SLATE_ARGUMENT(FUiPreferences, Preferences)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    TSharedPtr<SWidget> Resolve(const FString& Id) const;
    bool Activate(const FString& Id);
private:
    TSharedPtr<FHansaLandOverlayUiState> State;
    TSharedPtr<SHansaAction> FrameButton, CloseButton;
    TSharedPtr<STextBlock> TitleText, OwnerText, AccessText, ReasonText, HelpText;
};
}
