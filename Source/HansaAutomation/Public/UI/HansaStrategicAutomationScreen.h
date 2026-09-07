#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class STextBlock;
class SWindow;

namespace Hansa::Automation
{
	class FHansaSemanticUiRegistry;
	class FHansaStrategicAutomationFixture;

	/** Native, development-only checkpoint surface for the S10-P04 strategic automation fixture. */
	class HANSAAUTOMATION_API SHansaStrategicAutomationScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaStrategicAutomationScreen) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&, FHansaStrategicAutomationFixture& InFixture, FHansaSemanticUiRegistry& InRegistry);
		void SynchronizeSemantics();
		void SetPresentationSize(const FIntPoint& Size);
		TSharedRef<SWidget> GetCaptureWidget() const;

	private:
		bool BuildRoadIntent();
		bool DiagnoseShortageIntent();
		bool StartReliefRoutesIntent();
		bool QueueResearchIntent();
		void RegisterSemantics();
		void UpdateGeometry(const FString& Id, const TSharedPtr<SWidget>& Widget);

		FHansaStrategicAutomationFixture* Fixture = nullptr;
		FHansaSemanticUiRegistry* Registry = nullptr;
		bool bShortageDiagnosed = false;
		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SWidget> ScreenWidget;
		TSharedPtr<STextBlock> SummaryText;
		TSharedPtr<STextBlock> EvidenceText;
		TMap<FString, TSharedPtr<SWidget>> SemanticWidgets;
	};

	class HANSAAUTOMATION_API FHansaStrategicAutomationScreenHost final
	{
	public:
		FHansaStrategicAutomationScreenHost(FHansaStrategicAutomationFixture& InFixture, FHansaSemanticUiRegistry& InRegistry);
		~FHansaStrategicAutomationScreenHost();
		bool EnsureScreen(const FIntPoint& ClientSize = FIntPoint(1280, 720));
		bool CaptureNative(const FIntPoint& Size, TArray<FColor>& OutPixels);
		void SynchronizeSemantics();

	private:
		FHansaStrategicAutomationFixture& Fixture;
		FHansaSemanticUiRegistry& Registry;
		TSharedPtr<SWindow> Window;
		TSharedPtr<SHansaStrategicAutomationScreen> Screen;
		FIntPoint CurrentClientSize = FIntPoint::ZeroValue;
	};
}
