#pragma once
#include "HansaTradeWorkspaceComponents.h"
namespace Hansa::UI {
class STradeDecisions final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeDecisions){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
 void Refresh(const FHansaTradeMapSnapshot&);
 TArray<FString> FocusOrder() const;
 TSharedPtr<SScrollBox> Scroll;
private:
 TSharedPtr<SVerticalBox> Choices;
 TSharedPtr<SWidget> Dossier;
 TSharedPtr<STextBlock> ContextText,Terms,Status,Feedback,SourceDetail;
 TSharedPtr<SHansaAction> Source,Review,Confirm,Cancel;
 TMap<FString,TSharedPtr<SHansaAction>> Actions;
 FHansaTradeMapSnapshot View;
};
}
