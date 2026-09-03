#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SBox;
class STextBlock;
class SWindow;

namespace Hansa::Automation
{
	class FHansaSemanticUiRegistry;
	struct FHansaSemanticState;

	class SHansaAutomationProofScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaAutomationProofScreen) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& Arguments, FHansaSemanticUiRegistry& InRegistry);
		void SynchronizeSemantics();
		void SetPresentationSize(const FIntPoint& Size);
		[[nodiscard]] TSharedRef<SWidget> GetCaptureWidget() const;

	private:
		FReply HandleActivate();
		bool ActivateFromAutomation();
		bool FocusFromAutomation();
		FSlateColor GetFocusRingColor() const;
		void UpdateNodeGeometry(const FString& Id, const TSharedPtr<SWidget>& Widget, FHansaSemanticState State);

		FHansaSemanticUiRegistry* Registry = nullptr;
		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SWidget> ScreenWidget;
		TSharedPtr<SWidget> PanelWidget;
		TSharedPtr<SWidget> TitleWidget;
		TSharedPtr<STextBlock> StatusWidget;
		TSharedPtr<SButton> ActivateButton;
		TSharedPtr<SButton> FocusButton;
		TSharedPtr<SWidget> WarningWidget;
		bool bActivated = false;
		bool bAutomationFocusRequested = false;
	};

	/** Lazily creates the explicitly enabled automation-only native Slate surface. */
	class FHansaAutomationProofScreenHost final
	{
	public:
		explicit FHansaAutomationProofScreenHost(FHansaSemanticUiRegistry& InRegistry);
		~FHansaAutomationProofScreenHost();

		bool EnsureScreen(const FIntPoint& ClientSize = FIntPoint(1280, 720));
		bool CaptureNative(const FIntPoint& Size, TArray<FColor>& OutPixels);
		void SynchronizeSemantics();

	private:
		FHansaSemanticUiRegistry& Registry;
		TSharedPtr<SWindow> Window;
		TSharedPtr<SHansaAutomationProofScreen> Screen;
		FIntPoint CurrentClientSize = FIntPoint::ZeroValue;
	};
}
