#pragma once
#include "CoreMinimal.h"
#include "UI/HansaUiComponents.h"
#include "Widgets/SCompoundWidget.h"

namespace Hansa::UI {
DECLARE_DELEGATE_OneParam(FUiPreferencesChanged,FUiPreferences);

/** Native presentation settings. No game-state or simulation data is changed. */
class HANSA_API SHansaUiPreferences final : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SHansaUiPreferences){} 
  SLATE_ARGUMENT(FUiPreferences,Preferences)
  SLATE_EVENT(FUiPreferencesChanged,OnChanged)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 bool ActivateControl(const FString& Id);
 const TMap<FString,TSharedPtr<SHansaAction>>& GetControls() const { return Controls; }
private:
 FReply Change(int32 Setting);
 FUiPreferences Preferences;
 FUiPreferencesChanged Changed;
 FTextBlockStyle HeadingStyle,BodyStyle;
 TMap<FString,TSharedPtr<SHansaAction>> Controls;
};
}
