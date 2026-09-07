#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaHudSemantics.h"

class SButton;
class STextBlock;
class SVerticalBox;
class UHansaSaveLoadPresentationModel;
enum class EHansaSaveSlotId : uint8;
struct FHansaSaveLoadPresentationSnapshot;

namespace Hansa::UI
{
	class HANSA_API SHansaSaveLoadScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaSaveLoadScreen) : _Model(nullptr) {}
			SLATE_ARGUMENT(UHansaSaveLoadPresentationModel*, Model)
		SLATE_END_ARGS()
		~SHansaSaveLoadScreen();
		void Construct(const FArguments& Arguments);
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	private:
		void Refresh(const FHansaSaveLoadPresentationSnapshot& Snapshot, uint64 Revision);
		void RebuildSlots(const FHansaSaveLoadPresentationSnapshot& Snapshot);
		FReply HandleClose();
		FReply HandleSelectSlot(EHansaSaveSlotId SlotId);
		FReply HandleSave();
		FReply HandleLoad();
		FReply HandleConfirm();
		FReply HandleCancel();
		TWeakObjectPtr<UHansaSaveLoadPresentationModel> Model;
		FDelegateHandle ChangedHandle;
		uint64 PresentedRevision = 0;
		FSlateBrush ScrimBrush, PanelBrush, InnerBrush, CriticalBrush;
		FButtonStyle PrimaryButtonStyle, SecondaryButtonStyle;
		FTextBlockStyle HeadingStyle, BodyStyle, DataStyle, CaptionStyle;
		TSharedPtr<SVerticalBox> SlotRows;
		TSharedPtr<STextBlock> DetailTitle, DetailTimestamp, DetailScenario, DetailVersion, DetailHashes;
		TSharedPtr<STextBlock> StatusText, RemedyText, ConfirmationText;
		TSharedPtr<SButton> CloseButton, SaveButton, LoadButton, ConfirmButton, CancelButton;
		TSharedPtr<SWidget> RootWidget, ConfirmationWidget, StatusWidget;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
	};
}
