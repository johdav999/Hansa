#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class STextBlock;
class SWindow;

namespace Hansa::Automation
{
	class FHansaProductionFixtureService;
	class FHansaSemanticUiRegistry;

	/** Native route/market proof surface backed by route_delivery_v1 authoritative state. */
	class SHansaRouteDeliveryAutomationScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaRouteDeliveryAutomationScreen) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&, FHansaProductionFixtureService& InService, FHansaSemanticUiRegistry& InRegistry);
		void SynchronizeSemantics();
		void SetPresentationSize(const FIntPoint& Size);
		TSharedRef<SWidget> GetCaptureWidget() const;

	private:
		bool ShowRouteIntent();
		bool ShowMarketIntent();
		bool SaveRouteIntent();
		bool StartRouteIntent();
		bool CancelRouteIntent();
		void RegisterSemantics();
		void RefreshText();
		void UpdateGeometry(const FString& Id, const TSharedPtr<SWidget>& Widget);

		FHansaProductionFixtureService* Service = nullptr;
		FHansaSemanticUiRegistry* Registry = nullptr;
		bool bShowingMarket = false;
		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SWidget> ScreenWidget;
		TSharedPtr<SWidget> RoutePanel;
		TSharedPtr<SWidget> MarketPanel;
		TSharedPtr<STextBlock> RouteStatusText;
		TSharedPtr<STextBlock> CargoText;
		TSharedPtr<STextBlock> MarketText;
		TSharedPtr<STextBlock> HashText;
		TMap<FString, TSharedPtr<SWidget>> SemanticWidgets;
	};

	class FHansaRouteDeliveryAutomationScreenHost final
	{
	public:
		FHansaRouteDeliveryAutomationScreenHost(FHansaProductionFixtureService& InService, FHansaSemanticUiRegistry& InRegistry);
		~FHansaRouteDeliveryAutomationScreenHost();
		bool EnsureScreen(const FIntPoint& ClientSize = FIntPoint(1280, 720));
		bool CaptureNative(const FIntPoint& Size, TArray<FColor>& OutPixels);
		void SynchronizeSemantics();

	private:
		FHansaProductionFixtureService& Service;
		FHansaSemanticUiRegistry& Registry;
		TSharedPtr<SWindow> Window;
		TSharedPtr<SHansaRouteDeliveryAutomationScreen> Screen;
		FIntPoint CurrentClientSize = FIntPoint::ZeroValue;
	};
}
