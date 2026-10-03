#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaUiComponents.h"
class AHansaStrategyPlayerController;
class ASceneCapture2D;
namespace Hansa::UI {
class FHansaLandOverlayUiState;
class SHansaLandPanel;
class SHansaAction;
class HANSA_API SHansaMinimap final : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SHansaMinimap):_Controller(nullptr),_MapSize(240.f){}
  SLATE_ARGUMENT(AHansaStrategyPlayerController*,Controller)
  SLATE_ARGUMENT(float,MapSize)
  SLATE_ARGUMENT(TSharedPtr<FHansaLandOverlayUiState>,LandState)
  SLATE_ARGUMENT(FUiPreferences,Preferences)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 ~SHansaMinimap();
 bool Activate(const FString& Id);
 TSharedPtr<SWidget> Resolve(const FString& Id) const;
 TSharedPtr<FHansaLandOverlayUiState> GetLandState() const { return LandState; }
private:
 TSharedPtr<FHansaLandOverlayUiState> LandState;
 TSharedPtr<SHansaLandPanel> LandPanel;
 TSharedPtr<SHansaAction> LandButton;
 TMap<FString,TSharedPtr<SWidget>> Targets;
 TMap<FString,TFunction<void()>> Actions;
};
}

