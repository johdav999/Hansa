#pragma once
#include "HansaTradeWorkspaceComponents.h"
namespace Hansa::UI {
class STradeRecovery final : public STradeComponent {
public:
 SLATE_BEGIN_ARGS(STradeRecovery){} SLATE_END_ARGS()
 void Construct(const FArguments&,const TSharedRef<FTradeComponentContext>&);
 void Refresh(const FHansaTradeMapSnapshot&);
 TArray<FString> FocusOrder() const;
 TSharedPtr<SScrollBox> Scroll;
private:
 FHansaTradeMapSnapshot View;
 TSharedPtr<SVerticalBox> Items;
 TSharedPtr<STextBlock> Summary,Detail,Feedback;
 TSharedPtr<SHansaAction> Inspect,Review,Confirm,Cancel,Back;
 TMap<FString,TSharedPtr<SHansaAction>> Rows;
};
}
