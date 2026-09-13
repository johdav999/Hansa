#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaUiComponents.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaScenarioPresentationModel.h"
class STextBlock;
namespace Hansa::UI {
/** Optional, non-modal contextual guidance. Never issues gameplay commands. */
class HANSA_API SHansaSessionCoach final : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SHansaSessionCoach):_Model(nullptr){}
  SLATE_ARGUMENT(UHansaScenarioPresentationModel*,Model)
  SLATE_ARGUMENT(FUiPreferences,Preferences)
  SLATE_EVENT(FSimpleDelegate,OnDismissed)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 ~SHansaSessionCoach();
 bool ActivateSemanticId(const FString& Id);
 bool FocusSemanticId(const FString& Id);
 TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const;
 TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
 bool IsOffered() const;
private:
 void Refresh(const FHansaScenarioPresentationSnapshot& S,uint64);
 TWeakObjectPtr<UHansaScenarioPresentationModel> Model;
 FDelegateHandle Changed;
 FSimpleDelegate OnDismissed;
 FSlateBrush PanelBrush,HeaderBrush;
 TSharedPtr<STextBlock> Title,Body;
 TSharedPtr<SHansaAction> Dismiss,Hide;
};
}
