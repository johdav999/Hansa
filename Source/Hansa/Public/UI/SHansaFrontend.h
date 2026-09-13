#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaFrontendPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "UI/SHansaUiPreferences.h"
class SBox;class SScrollBox;class SVerticalBox;
namespace Hansa::UI {
class HANSA_API SHansaFrontend final:public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SHansaFrontend):_Model(nullptr){}
 SLATE_ARGUMENT(UHansaFrontendPresentationModel*,Model)
 SLATE_ARGUMENT(FUiPreferences,Preferences)
 SLATE_EVENT(FUiPreferencesChanged,OnPreferencesChanged)
 SLATE_EVENT(FSimpleDelegate,OnClosed)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args);~SHansaFrontend();
 void SetPresentationSize(FIntPoint Size);
 bool IsOpen()const;bool ActivateSemanticId(const FString& Id);bool FocusSemanticId(const FString& Id);
 TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id)const;
 TArray<FString> GetControllerFocusOrder()const;
 TArray<FHansaHudSemanticNode> GetSemanticSnapshot()const;
 FString GetFocusedId()const{return Focused;}
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&)override;
private:
 void ScheduleRefresh();void Refresh();
 TWeakObjectPtr<UHansaFrontendPresentationModel> Model;FDelegateHandle Changed;
 FSimpleDelegate OnClosed;bool bWasOpen=false;
 FUiPreferences Preferences;FUiPreferencesChanged PreferencesChanged;
 FSlateBrush Background,Panel,Header;
 TSharedPtr<SBox> PanelSize;TSharedPtr<SVerticalBox> Rows;TSharedPtr<SScrollBox> Scroll;
 TSharedPtr<SHansaUiPreferences> Interface;
 TMap<FString,TSharedPtr<SWidget>> Widgets;TMap<FString,TFunction<void()>> Actions;TMap<FString,FText> Labels;
 TArray<FString> Order;FString Focused;FIntPoint Available{1280,720};bool bRefreshQueued=false;
};
}
