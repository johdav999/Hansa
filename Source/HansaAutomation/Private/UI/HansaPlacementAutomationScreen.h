#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class STextBlock;
class SWindow;

namespace Hansa::Automation
{
	class FHansaPlacementAutomationFixture;
	class FHansaSemanticUiRegistry;

	/** Native Slate build-mode surface used for semantic and exact-size screenshot evidence. */
	class SHansaPlacementAutomationScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaPlacementAutomationScreen) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& Arguments, FHansaPlacementAutomationFixture& InFixture, FHansaSemanticUiRegistry& InRegistry);
		void SynchronizeSemantics();
		void SetPresentationSize(const FIntPoint& Size);
		[[nodiscard]] TSharedRef<SWidget> GetCaptureWidget() const;

	private:
		FReply InvokeIntent(TFunction<bool()> Intent);
		void RefreshText();
		FSlateColor ToolColor(bool bSelected) const;
		void UpdateGeometry(const FString& SemanticId, const TSharedPtr<SWidget>& Widget);

		FHansaPlacementAutomationFixture* Fixture = nullptr;
		FHansaSemanticUiRegistry* Registry = nullptr;
		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SWidget> ScreenWidget;
		TMap<FString, TSharedPtr<SWidget>> SemanticWidgets;
		TSharedPtr<STextBlock> CameraText;
		TSharedPtr<STextBlock> PreviewText;
		TSharedPtr<STextBlock> ValidationText;
		TSharedPtr<STextBlock> CauseText;
		TSharedPtr<STextBlock> RemedyText;
		TSharedPtr<STextBlock> ResultText;
	};

	class FHansaPlacementAutomationScreenHost final
	{
	public:
		FHansaPlacementAutomationScreenHost(FHansaPlacementAutomationFixture& InFixture, FHansaSemanticUiRegistry& InRegistry);
		~FHansaPlacementAutomationScreenHost();

		bool EnsureScreen(const FIntPoint& ClientSize = FIntPoint(1280, 720));
		bool CaptureNative(const FIntPoint& Size, TArray<FColor>& OutPixels);
		void SynchronizeSemantics();

	private:
		FHansaPlacementAutomationFixture& Fixture;
		FHansaSemanticUiRegistry& Registry;
		TSharedPtr<SWindow> Window;
		TSharedPtr<SHansaPlacementAutomationScreen> Screen;
		FIntPoint CurrentClientSize = FIntPoint::ZeroValue;
	};
}
