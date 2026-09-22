#include "UI/SHansaRootHud.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaCityOverview.h"
#include "UI/SHansaMarketTable.h"
#include "UI/SHansaContextInspector.h"
#include "UI/SHansaBuildMenu.h"
#include "UI/SHansaResearchScreen.h"
#include "UI/SHansaTradeMap.h"
#include "World/HansaStrategyPlayerController.h"
#include "UI/SHansaScenarioScreen.h"
#include "UI/SHansaSessionCoach.h"
#include "UI/SHansaFrontend.h"
#include "UI/SHansaSaveLoadScreen.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SComboButton.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/SHansaMinimap.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaRootHud"

namespace Hansa::UI
{
	namespace
	{
		TSharedRef<STextBlock> StatusText(const FTextBlockStyle* Style, TSharedPtr<STextBlock>& Out)
		{
			return SAssignNew(Out, STextBlock).TextStyle(Style).OverflowPolicy(ETextOverflowPolicy::Ellipsis).Clipping(EWidgetClipping::ClipToBounds);
		}

        float TopBarHeight(const FHansaHudLayoutMetrics& Metrics,const FUiPreferences& Prefs, bool bRemoteCity=false)
        {
            return Metrics.ViewportSize.X < 1500 || Prefs.bLargeText ? 120.f : 72.f;

        }

		FString SpeedValue(const EHansaHudGameSpeed Speed)
		{
			switch (Speed)
			{
			case EHansaHudGameSpeed::Paused: return TEXT("paused");
			case EHansaHudGameSpeed::Normal: return TEXT("normal");
			case EHansaHudGameSpeed::Fast: return TEXT("fast");
			case EHansaHudGameSpeed::Fastest: return TEXT("fastest");
			default: return TEXT("unknown");
			}
		}
	}

    TSharedRef<SWidget> SHansaRootHud::BuildTopMenu()
    {
        auto Metric=[&](EUiGlyph Glyph,FText Label,TSharedPtr<STextBlock>& Text,TSharedPtr<SWidget>& Chip)->TSharedRef<SWidget>{
            Chip=SNew(SBox).MinDesiredWidth(92).HeightOverride(Label.IsEmpty()?38.f:56.f)
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)[SNew(SHansaGlyph).Glyph(Glyph).Size(32)]
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[StatusText(&DarkDataStyle,Text)]
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&DarkCaptionStyle)]]];
            return Chip.ToSharedRef();
        };
        auto Metrics=SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Metric(EUiGlyph::Coin,FText::GetEmpty(),MoneyText,MoneyChip)]
                +SVerticalBox::Slot().AutoHeight()[SAssignNew(TrendChip,SBox)[StatusText(&DarkCaptionStyle,MoneyTrendText)]]]
            +SHorizontalBox::Slot().AutoWidth()[Metric(EUiGlyph::People,LOCTEXT("Citizens","Citizens"),PopulationText,PopulationChip)]
            +SHorizontalBox::Slot().AutoWidth()[Metric(EUiGlyph::Wealthy,LOCTEXT("Artisans","Artisans"),WealthyText,WealthyChip)]
            +SHorizontalBox::Slot().AutoWidth()[Metric(EUiGlyph::Laborer,LOCTEXT("Laborers","Laborers"),WorkforceText,LaborerChip)];
        // Retain legacy semantic data widgets off-screen; goods remain available in city/market views.
        for(int32 I=0;I<3;++I) ProductChips[I]=SNew(SBox)
            [SNew(SHorizontalBox)+SHorizontalBox::Slot()[SAssignNew(ProductTexts[I],STextBlock)]
             +SHorizontalBox::Slot()[SAssignNew(ProductGlyphs[I],SHansaGlyph)]];
        auto City=SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SAssignNew(CityOverviewButton,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences)
                .OnClicked(this,&SHansaRootHud::HandleCityOverviewOpen)
                [StatusText(&DarkDataStyle,CityBreadcrumbText)]]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SAssignNew(ReturnCityButton,SHansaAction).Compact(true).Preferences(Preferences).Label(LOCTEXT("ReturnCityShort","Return"))
                .OnClicked_Lambda([this]{return ActivateSemanticId(TEXT("HUD.TopStatus.ReturnCity"))?FReply::Handled():FReply::Unhandled();})];
        CityBreadcrumbText->SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
        auto Speed=[&](TSharedPtr<SButton>& Button,EUiGlyph Glyph,EHansaHudGameSpeed Value,const TCHAR* Id)->TSharedRef<SWidget>{
            return SNew(SBox).WidthOverride(48).HeightOverride(56)
                [SAssignNew(Button,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences)
                 .OnClicked(this,&SHansaRootHud::HandleSpeed,Value,Id)[SNew(SHansaGlyph).Glyph(Glyph).Size(28)]];
        };
        auto Speeds=SAssignNew(SpeedGroupWidget,SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[Speed(PauseButton,EUiGlyph::Pause,EHansaHudGameSpeed::Paused,TEXT("HUD.TopStatus.Speed.Pause"))]
            +SHorizontalBox::Slot().AutoWidth()[Speed(NormalButton,EUiGlyph::Play,EHansaHudGameSpeed::Normal,TEXT("HUD.TopStatus.Speed.Normal"))]
            +SHorizontalBox::Slot().AutoWidth()[Speed(FastButton,EUiGlyph::Fast,EHansaHudGameSpeed::Fast,TEXT("HUD.TopStatus.Speed.Fast"))]
            +SHorizontalBox::Slot().AutoWidth()[Speed(FastestButton,EUiGlyph::Fastest,EHansaHudGameSpeed::Fastest,TEXT("HUD.TopStatus.Speed.Fastest"))];
        auto Utilities=SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SAssignNew(SaveLoadButton,SHansaAction).Preferences(Preferences).Label(LOCTEXT("SaveLoad","Save / load")).OnClicked(this,&SHansaRootHud::HandleSaveLoadOpen)]
            +SVerticalBox::Slot().AutoHeight()[SAssignNew(TradeMapButton,SHansaAction).Preferences(Preferences).Label(LOCTEXT("TradeMap","Trade map")).OnClicked(this,&SHansaRootHud::HandleTradeMapOpen)]
            +SVerticalBox::Slot().AutoHeight()[SAssignNew(SessionButton,SHansaAction).Preferences(Preferences).Label(LOCTEXT("SessionMenu","Menu")).OnClicked(this,&SHansaRootHud::HandleSessionOpen)]
            +SVerticalBox::Slot().AutoHeight()[StatusText(&DarkCaptionStyle,ConnectionText)]
            +SVerticalBox::Slot().AutoHeight()[SAssignNew(FpsText,STextBlock).Text(LOCTEXT("FpsPending","FPS —")).TextStyle(&DarkCaptionStyle)];
        auto Right=SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0)
            [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Season).Size(28)]
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[StatusText(&DarkCaptionStyle,DateText)]]
            +SHorizontalBox::Slot().AutoWidth()[Speeds]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0)
            [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SHansaGlyph).Glyph(EUiGlyph::Civic).Size(32)]
             +SHorizontalBox::Slot().AutoWidth()[SAssignNew(InfluenceText,STextBlock).Text(LOCTEXT("InfluenceUnavailable","Influence\n—")).TextStyle(&DarkCaptionStyle)
                .ToolTipText(LOCTEXT("InfluenceUnavailableTip","Influence is not available in this scenario."))]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SAssignNew(ResearchButton,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences).OnClicked(this,&SHansaRootHud::HandleResearchOpen)
                [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SHansaGlyph).Glyph(EUiGlyph::Research).Size(32)]
                 +SHorizontalBox::Slot().AutoWidth()[StatusText(&DarkCaptionStyle,ResearchText)]]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SAssignNew(UtilityMenu,SComboButton).HasDownArrow(false)
                .ButtonContent()[SNew(SHansaGlyph).Glyph(EUiGlyph::Settings).Size(24)]
                .MenuContent()[SNew(SBorder).BorderImage(&WorldOverlayBrush).Padding(12)[Utilities]]];
        TopLeftPanel=Metrics;TopCenterPanel=City;TopRightPanel=Right;
        if(Layout.ViewportSize.X>=1500 && !Preferences.bLargeText)
            return SNew(SHansaReferenceFrame).Dark(true).Padding(FMargin(12,2))
            [SNew(SOverlay)
             +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)[Metrics]
             +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[City]
             +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center)[Right]];
        return SNew(SHansaReferenceFrame).Dark(true).Padding(FMargin(12,2))
            [SNew(SVerticalBox)
             +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Metrics]+SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center)[City]]
             +SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Right)[Right]];

    }

	SHansaRootHud::~SHansaRootHud(){UnbindModels();}

	void SHansaRootHud::UnbindModels()
	{
		if (UHansaHudPresentationModel* PinnedModel = Model.Get())
		{
			PinnedModel->OnChanged().Remove(ModelChangedHandle);
		}
		if (UHansaInspectorPresentationModel* PinnedInspector = InspectorModel.Get())
		{
			PinnedInspector->OnChanged().Remove(InspectorChangedHandle);
			PinnedInspector->OnFocusRestoreRequested().Remove(InspectorFocusRestoreHandle);
		}
		if (UHansaCityOverviewPresentationModel* PinnedCityOverview = CityOverviewModel.Get())
		{
			PinnedCityOverview->OnChanged().Remove(CityOverviewChangedHandle);
			PinnedCityOverview->OnFocusRestoreRequested().Remove(CityOverviewFocusRestoreHandle);
		}
		if (UHansaTradeMapPresentationModel* PinnedTradeMap = TradeMapModel.Get())
		{
			PinnedTradeMap->OnChanged().Remove(TradeMapChangedHandle);
			PinnedTradeMap->OnFocusRestoreRequested().Remove(TradeMapFocusRestoreHandle);
		}
		if (UHansaResearchPresentationModel* PinnedResearch = ResearchModel.Get())
		{
			PinnedResearch->OnChanged().Remove(ResearchChangedHandle);
			PinnedResearch->OnFocusRestoreRequested().Remove(ResearchFocusRestoreHandle);
		}
		if (UHansaScenarioPresentationModel* PinnedScenario = ScenarioModel.Get())
		{
			PinnedScenario->OnChanged().Remove(ScenarioChangedHandle);
			PinnedScenario->OnFocusRestoreRequested().Remove(ScenarioFocusRestoreHandle);
		}
		if (UHansaSaveLoadPresentationModel* PinnedSaveLoad = SaveLoadModel.Get())
		{
			PinnedSaveLoad->OnChanged().Remove(SaveLoadChangedHandle);
			PinnedSaveLoad->OnFocusRestoreRequested().Remove(SaveLoadFocusRestoreHandle);
		}
	}

	void SHansaRootHud::Construct(const FArguments& Arguments)
	{
		SetVisibility(EVisibility::SelfHitTestInvisible);
		SetCanTick(true);
		FpsSampleElapsedSeconds = 0.0;
		FpsSampleFrameCount = 0;
		RebuildArguments=Arguments;Preferences=Arguments._Preferences;PresentedAlerts.Reset();AlertCards.Reset();PinnedAlertTexts.Reset();PresentedNotifications.Reset();SemanticWidgets.Reset();AlertSemanticWidgets.Reset();
		Model = Arguments._Model;
		BuildModel = Arguments._BuildModel;
		InspectorModel = Arguments._InspectorModel;
		CityOverviewModel = Arguments._CityOverviewModel;
		MarketTableModel = Arguments._MarketTableModel;
		TradeMapModel = Arguments._TradeMapModel;
		ResearchModel = Arguments._ResearchModel;
		FrontendModel=Arguments._FrontendModel;
		ScenarioModel = Arguments._ScenarioModel;
		SaveLoadModel = Arguments._SaveLoadModel;
		PlacementController = Arguments._PlacementController;
		PhysicalViewportSize=Arguments._InitialViewportSize;Preferences.UiScale=FMath::Clamp(Preferences.UiScale,.8f,1.4f);
		Layout = MakeHudLayoutMetrics(FIntPoint(FMath::RoundToInt(PhysicalViewportSize.X/Preferences.UiScale),FMath::RoundToInt(PhysicalViewportSize.Y/Preferences.UiScale)));Layout.TopBarHeight=TopBarHeight(Layout,Preferences,Model.IsValid()&&Model->GetSnapshot().bRemoteCityView);
		WorldOverlayBrush = GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		WorkingBrush = GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
		FloatingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Floating);
		IconButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Icon);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		DarkBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, true);
		DarkDataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, true);
		DarkCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, true);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);

		DarkBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        DarkDataStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
        DarkCaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));
        LightBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        LightHeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences));
        const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FLinearColor Transparent = FLinearColor::Transparent;
		const FMargin Safe(Layout.SafeArea);

		ChildSlot
		[
			SNew(SDPIScaler).DPIScale(Preferences.UiScale)[SAssignNew(PresentationBox, SBox)
			.WidthOverride(static_cast<float>(Layout.ViewportSize.X))
			.HeightOverride(static_cast<float>(Layout.ViewportSize.Y))
			[
				SAssignNew(ScreenWidget, SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(Transparent).Visibility(EVisibility::HitTestInvisible)
				]
				+ SOverlay::Slot().Expose(TopStatusSlot).HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(0)
				[
					SAssignNew(TopStatusBox, SBox).HeightOverride(Layout.TopBarHeight)
					[
						SAssignNew(TopStatusWidget, SBox).Visibility(EVisibility::SelfHitTestInvisible)
						[
BuildTopMenu()
						]
					]
				]
				+ SOverlay::Slot().Expose(AlertSlot).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.TopBarHeight, 0.0f, 0.0f)
				[
					SAssignNew(AlertHostBox, SBox).WidthOverride(Layout.AlertWidth)
					[
						SAssignNew(AlertStackWidget, SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder"))).Padding(0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(AlertToggleButton, SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences).Reason(LOCTEXT("AlertsTip","Expand or collapse objectives and causal alerts.")).OnClicked(this, &SHansaRootHud::HandleAlertToggle)
								[
									SAssignNew(AlertToggleText, STextBlock).TextStyle(&DarkDataStyle)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SAssignNew(AlertRowsHostBox, SBox).MaxDesiredHeight(FMath::Max(48.f,FMath::Min(360.f,Layout.ViewportSize.Y-Layout.TopBarHeight-3*Layout.SafeArea-(Layout.ViewportSize.X<1280?160.f:240.f)-96.f)))
								[
									SAssignNew(AlertScroll,SScrollBox)
									+ SScrollBox::Slot()
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SAssignNew(AlertRows, SVerticalBox)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
										[
											SAssignNew(PinnedTrackersWidget, SBorder).BorderImage(&FloatingBrush).Padding(6.0f)
											[
												SNew(SVerticalBox)
												+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PinnedTrackers", "Pinned tracking")).TextStyle(&DarkCaptionStyle)]
												+ SVerticalBox::Slot().AutoHeight()[SAssignNew(PinnedTrackerRows, SVerticalBox)]
											]
										]
									]
								]
							]
						]
					]
				]
                +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(12,0,0,12)
                [SAssignNew(MinimapWidget,SHansaMinimap).Controller(PlacementController.Get()).MapSize(Layout.ViewportSize.X<1280?160.f:240.f)]
				+ SOverlay::Slot().Expose(BottomSlot).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, Layout.SafeArea)
				[
					SAssignNew(BottomHostBox, SBox).WidthOverride(Layout.BottomWidth).HeightOverride(Layout.BottomHeight)
					[
						SAssignNew(BottomAreaWidget, SBorder).BorderImage(&WorldOverlayBrush).Padding(12.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SAssignNew(SelectionText, STextBlock).TextStyle(&DarkBodyStyle)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(BottomToggleButton, SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("ToggleBuild", "Build / selection")).OnClicked(this, &SHansaRootHud::HandleBottomToggle)]
						]
					]
				]
				+ SOverlay::Slot().Expose(BuildMenuSlot).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(Layout.SafeArea, 0.0f, Layout.SafeArea, Layout.SafeArea)
				[
					SAssignNew(BuildMenuHostBox, SBox)
					.WidthOverride(FMath::Min(880.f, static_cast<float>(Layout.ViewportSize.X) - 360.f))
					.MaxDesiredHeight(static_cast<float>(Layout.ViewportSize.Y)-Layout.TopBarHeight-Layout.SafeArea*3.f)
					[
						SAssignNew(BuildMenuWidget, SHansaBuildMenu).Preferences(Preferences).Model(BuildModel.Get()).PlacementController(PlacementController.Get())
					]
				]
				+ SOverlay::Slot().Expose(InspectorSlot).HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(0.0f, Layout.SafeArea * 2.0f + Layout.TopBarHeight, Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.BottomHeight)
				[
					SAssignNew(InspectorHostBox, SBox).WidthOverride(Layout.InspectorWidth)
					[
						SAssignNew(InspectorHostWidget, SBorder).BorderImage(&WorkingBrush).Padding(0.0f)
						[
							SNew(SHansaReferenceFrame).Dark(false).Padding(6)[SAssignNew(InspectorWidget, SHansaContextInspector).Preferences(Preferences).Model(InspectorModel.Get())]
						]
					]
				]
				+ SOverlay::Slot().Expose(NotificationSlot).HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(Layout.SafeArea, 0.0f, 0.0f, Layout.SafeArea * 2.0f + Layout.BottomHeight)
				[
					SAssignNew(NotificationHostBox, SBox).WidthOverride(Layout.NotificationWidth)
					[
						SAssignNew(NotificationLayerWidget, SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(Transparent).Padding(0.0f)
						[
							SAssignNew(NotificationRows, SVerticalBox)
						]
					]
				]
				+ SOverlay::Slot().Expose(FocusSlot).HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(Layout.SafeArea, 0.0f, 0.0f, Layout.SafeArea)
				[
					SAssignNew(FocusLayerWidget, SBorder).BorderImage(&FloatingBrush).Padding(FMargin(10.0f, 6.0f)).Visibility(EVisibility::HitTestInvisible)
					[
						SAssignNew(FocusText, STextBlock).TextStyle(&DarkCaptionStyle)
					]
				]
				+ SOverlay::Slot().Expose(CityOverviewSlot).HAlign(HAlign_Center).VAlign(VAlign_Fill).Padding(Layout.SafeArea)
				[
					SAssignNew(CityOverviewHostWidget, SBox)
					[
						SAssignNew(CityOverviewWidget, SHansaCityOverview).Preferences(Preferences)
						.Model(CityOverviewModel.Get())
						.MarketTableModel(MarketTableModel.Get())
						.InitialViewportSize(FIntPoint(
							FMath::Max(640, Layout.ViewportSize.X - FMath::RoundToInt(Layout.SafeArea * 2.0f)),
							FMath::Max(480, Layout.ViewportSize.Y - FMath::RoundToInt(Layout.SafeArea * 2.0f))))
					]
				]
				+ SOverlay::Slot().Expose(TradeMapSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(TradeMapHostWidget, SBox)
					[
						SAssignNew(TradeMapWidget, SHansaTradeMap).Preferences(Preferences)
						.Model(TradeMapModel.Get())
						.InitialViewportSize(FIntPoint(
							FMath::Max(640, Layout.ViewportSize.X - FMath::RoundToInt(Layout.SafeArea * 2.0f)),
							FMath::Max(480, Layout.ViewportSize.Y - FMath::RoundToInt(Layout.SafeArea * 2.0f))))
					]
				]
				+ SOverlay::Slot().Expose(ResearchSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(ResearchHostWidget, SBox)
					[
						SAssignNew(ResearchWidget, SHansaResearchScreen).Preferences(Preferences).Model(ResearchModel.Get())
					]
				]
                + SOverlay::Slot()
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.32f))
                 .Visibility_Lambda([this]{return ScenarioModel.IsValid()&&ScenarioModel->GetSnapshot().bOpen?EVisibility::Visible:EVisibility::Collapsed;})
                 .OnMouseButtonDown_Lambda([](const FGeometry&,const FPointerEvent&){return FReply::Handled();})]
				+ SOverlay::Slot().Expose(ScenarioSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(ScenarioHostWidget, SBox)
					[
						SAssignNew(ScenarioWidget, SHansaScenarioScreen).Preferences(Preferences)
						.Model(ScenarioModel.Get())
					]
				]
                +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(16,0,0,100)
                [SAssignNew(SessionCoach,SHansaSessionCoach).Model(ScenarioModel.Get()).Preferences(Preferences).OnDismissed_Lambda([this]{const auto Order=GetControllerFocusOrder();if(!Order.IsEmpty())FocusSemanticId(Order[0]);})]
				+ SOverlay::Slot()[SAssignNew(FrontendWidget,SHansaFrontend).Model(FrontendModel.Get()).Preferences(Preferences).OnPreferencesChanged(this,&SHansaRootHud::QueuePreferencesChange).OnClosed_Lambda([this]{const auto Order=GetControllerFocusOrder();if(!Order.IsEmpty())FocusSemanticId(Order[0]);})]
                + SOverlay::Slot().Expose(SaveLoadSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(SaveLoadHostWidget, SBox)
					[
						SAssignNew(SaveLoadWidget, SHansaSaveLoadScreen).Preferences(Preferences).OnPreferencesChanged(this,&SHansaRootHud::QueuePreferencesChange).Model(SaveLoadModel.Get())
					]
				]
				]
		]];

		MapWidget(TEXT("HUD.Root"), ScreenWidget);
        MapWidget(TEXT("HUD.TopStatus.Session"),SessionButton);
		MapWidget(TEXT("HUD.TopStatus"), TopStatusWidget);
        MapWidget(TEXT("HUD.TopStatus.LeftPanel"),TopLeftPanel);
        MapWidget(TEXT("HUD.TopStatus.Influence"),InfluenceText);
        if(MinimapWidget)for(const TCHAR* Id:{TEXT("HUD.Minimap"),TEXT("HUD.Minimap.ZoomIn"),TEXT("HUD.Minimap.ZoomOut"),TEXT("HUD.Minimap.Overlay"),TEXT("HUD.Minimap.Center")})MapWidget(Id,MinimapWidget->Resolve(Id));
        MapWidget(TEXT("HUD.TopStatus.CenterPanel"),TopCenterPanel);
        MapWidget(TEXT("HUD.TopStatus.RightPanel"),TopRightPanel);
		MapWidget(TEXT("HUD.TopStatus.Money"), MoneyChip);
		MapWidget(TEXT("HUD.TopStatus.MoneyTrend"), TrendChip);
		MapWidget(TEXT("HUD.TopStatus.Population"), PopulationChip);
		MapWidget(TEXT("HUD.TopStatus.Workforce"), LaborerChip);
        MapWidget(TEXT("HUD.TopStatus.WealthyCitizens"),WealthyChip);
        for(int32 I=0;I<3;++I)MapWidget(*FString::Printf(TEXT("HUD.TopStatus.Product.%d"),I+1),ProductChips[I]);
		MapWidget(TEXT("HUD.TopStatus.CityBreadcrumb"), CityBreadcrumbText);
		MapWidget(TEXT("HUD.TopStatus.CityOverview"), CityOverviewButton);
		MapWidget(TEXT("HUD.TopStatus.TradeMap"), TradeMapButton);
        MapWidget(TEXT("HUD.TopStatus.ReturnCity"), ReturnCityButton);
		MapWidget(TEXT("HUD.TopStatus.SaveLoad"), SaveLoadButton);
		MapWidget(TEXT("HUD.TopStatus.DateSeason"), DateText);
		MapWidget(TEXT("HUD.TopStatus.Speed"), SpeedGroupWidget);
		MapWidget(TEXT("HUD.TopStatus.Speed.Pause"), PauseButton);
		MapWidget(TEXT("HUD.TopStatus.Speed.Normal"), NormalButton);
		MapWidget(TEXT("HUD.TopStatus.Speed.Fast"), FastButton);
		MapWidget(TEXT("HUD.TopStatus.Speed.Fastest"), FastestButton);
		MapWidget(TEXT("HUD.TopStatus.Research"), ResearchText);
		MapWidget(TEXT("HUD.TopStatus.Research.Open"), ResearchButton);
		MapWidget(TEXT("HUD.TopStatus.Connection"), ConnectionText);
		MapWidget(TEXT("HUD.TopStatus.FPS"), FpsText);
		MapWidget(TEXT("HUD.AlertStack"), AlertStackWidget);
		MapWidget(TEXT("HUD.AlertStack.Toggle"), AlertToggleButton);
		MapWidget(TEXT("HUD.BottomArea"), BottomAreaWidget);
		MapWidget(TEXT("HUD.InspectorHost"), InspectorHostWidget);
		MapWidget(TEXT("HUD.NotificationLayer"), NotificationLayerWidget);
		MapWidget(TEXT("HUD.FocusLayer"), FocusLayerWidget);
		MapWidget(TEXT("HUD.CityOverviewHost"), CityOverviewHostWidget);
		MapWidget(TEXT("HUD.TradeMapHost"), TradeMapHostWidget);
		MapWidget(TEXT("HUD.ResearchHost"), ResearchHostWidget);
		MapWidget(TEXT("HUD.ScenarioHost"), ScenarioHostWidget);
		MapWidget(TEXT("HUD.SaveLoadHost"), SaveLoadHostWidget);
		ApplyResponsiveLayout();
		if (!BuildModel.IsValid() && BuildMenuHostBox.IsValid()) BuildMenuHostBox->SetVisibility(EVisibility::Collapsed);
		if (!InspectorModel.IsValid() && InspectorHostWidget.IsValid()) InspectorHostWidget->SetVisibility(EVisibility::Collapsed);
		if (!CityOverviewModel.IsValid() && CityOverviewHostWidget.IsValid()) CityOverviewHostWidget->SetVisibility(EVisibility::Collapsed);
		if (!TradeMapModel.IsValid() && TradeMapHostWidget.IsValid()) TradeMapHostWidget->SetVisibility(EVisibility::Collapsed);
		if (!ResearchModel.IsValid() && ResearchHostWidget.IsValid()) ResearchHostWidget->SetVisibility(EVisibility::Collapsed);
		if (!ScenarioModel.IsValid() && ScenarioHostWidget.IsValid()) ScenarioHostWidget->SetVisibility(EVisibility::Collapsed);
		if (!SaveLoadModel.IsValid() && SaveLoadHostWidget.IsValid()) SaveLoadHostWidget->SetVisibility(EVisibility::Collapsed);

		if (UHansaHudPresentationModel* PinnedModel = Model.Get())
		{
			ModelChangedHandle = PinnedModel->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::Refresh);
			Refresh(PinnedModel->GetSnapshot(), PinnedModel->GetRevision());
		}
		if (UHansaInspectorPresentationModel* PinnedInspector = InspectorModel.Get())
		{
			InspectorChangedHandle = PinnedInspector->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::RefreshInspectorHost);
			InspectorFocusRestoreHandle = PinnedInspector->OnFocusRestoreRequested().AddSP(SharedThis(this), &SHansaRootHud::RestoreFocusFromInspector);
			RefreshInspectorHost(PinnedInspector->GetSnapshot(), PinnedInspector->GetRevision());
		}
		if (UHansaCityOverviewPresentationModel* PinnedCityOverview = CityOverviewModel.Get())
		{
			CityOverviewChangedHandle = PinnedCityOverview->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::RefreshCityOverviewHost);
			CityOverviewFocusRestoreHandle = PinnedCityOverview->OnFocusRestoreRequested().AddSP(SharedThis(this), &SHansaRootHud::RestoreFocusFromCityOverview);
			RefreshCityOverviewHost(PinnedCityOverview->GetSnapshot(), PinnedCityOverview->GetRevision());
		}
		if (UHansaTradeMapPresentationModel* PinnedTradeMap = TradeMapModel.Get())
		{
			TradeMapChangedHandle = PinnedTradeMap->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::RefreshTradeMapHost);
			TradeMapFocusRestoreHandle = PinnedTradeMap->OnFocusRestoreRequested().AddSP(SharedThis(this), &SHansaRootHud::RestoreFocusFromTradeMap);
			RefreshTradeMapHost(PinnedTradeMap->GetSnapshot(), PinnedTradeMap->GetRevision());
		}
		if (UHansaResearchPresentationModel* PinnedResearch = ResearchModel.Get())
		{
			ResearchChangedHandle = PinnedResearch->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::RefreshResearchHost);
			ResearchFocusRestoreHandle = PinnedResearch->OnFocusRestoreRequested().AddSP(SharedThis(this), &SHansaRootHud::RestoreFocusFromResearch);
			RefreshResearchHost(PinnedResearch->GetSnapshot(), PinnedResearch->GetRevision());
		}
		if (UHansaScenarioPresentationModel* PinnedScenario = ScenarioModel.Get())
		{
			ScenarioChangedHandle = PinnedScenario->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::RefreshScenarioHost);
			ScenarioFocusRestoreHandle = PinnedScenario->OnFocusRestoreRequested().AddSP(SharedThis(this), &SHansaRootHud::RestoreFocusFromScenario);
			RefreshScenarioHost(PinnedScenario->GetSnapshot(), PinnedScenario->GetRevision());
		}
		if (UHansaSaveLoadPresentationModel* PinnedSaveLoad = SaveLoadModel.Get())
		{
			SaveLoadChangedHandle = PinnedSaveLoad->OnChanged().AddSP(SharedThis(this), &SHansaRootHud::RefreshSaveLoadHost);
			SaveLoadFocusRestoreHandle = PinnedSaveLoad->OnFocusRestoreRequested().AddSP(SharedThis(this), &SHansaRootHud::RestoreFocusFromSaveLoad);
			RefreshSaveLoadHost(PinnedSaveLoad->GetSnapshot(), PinnedSaveLoad->GetRevision());
		}
	}

	void SHansaRootHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		if (!FpsText.IsValid() || !FMath::IsFinite(InDeltaTime) || InDeltaTime <= 0.0f)
		{
			return;
		}

		FpsSampleElapsedSeconds += static_cast<double>(InDeltaTime);
		++FpsSampleFrameCount;
		constexpr double FpsUpdateIntervalSeconds = 0.5;
		if (FpsSampleElapsedSeconds < FpsUpdateIntervalSeconds)
		{
			return;
		}

		const int32 FramesPerSecond = FMath::Max(0, FMath::RoundToInt(
			static_cast<double>(FpsSampleFrameCount) / FpsSampleElapsedSeconds));
		FpsText->SetText(FText::Format(
			LOCTEXT("FpsValue", "FPS {0}"), FText::AsNumber(FramesPerSecond)));
		FpsSampleElapsedSeconds = 0.0;
		FpsSampleFrameCount = 0;
	}

    void SHansaRootHud::QueuePreferencesChange(FUiPreferences Value){
        RegisterActiveTimer(0.f,FWidgetActiveTimerDelegate::CreateLambda([Weak=TWeakPtr<SHansaRootHud>(SharedThis(this)),Value](double,float){
            if(auto Self=Weak.Pin()){
                Self->SetPreferences(Value);
                if(GConfig){
                    GConfig->SetFloat(TEXT("Hansa.UI"),TEXT("Scale"),Value.UiScale,GGameUserSettingsIni);
                    GConfig->SetBool(TEXT("Hansa.UI"),TEXT("HighContrast"),Value.bHighContrast,GGameUserSettingsIni);
                    GConfig->SetBool(TEXT("Hansa.UI"),TEXT("LargeText"),Value.bLargeText,GGameUserSettingsIni);
                    GConfig->SetBool(TEXT("Hansa.UI"),TEXT("ReducedMotion"),Value.bReducedMotion,GGameUserSettingsIni);
                    GConfig->Flush(false,GGameUserSettingsIni);
                }
            }
            return EActiveTimerReturnType::Stop;
        }));
    }
    void SHansaRootHud::SetPreferences(FUiPreferences InPreferences){
        FName Focus=Model.IsValid()?Model->GetSnapshot().FocusedSemanticId:FName();
        if(SaveLoadModel.IsValid()&&SaveLoadModel->GetSnapshot().bOpen)Focus=SaveLoadModel->GetSnapshot().FocusedSemanticId;
        else if(FrontendWidget&&FrontendWidget->IsOpen())Focus=FName(*FrontendWidget->GetFocusedId());
        else if(ResearchModel.IsValid()&&ResearchModel->GetSnapshot().bOpen)Focus=ResearchModel->GetSnapshot().FocusedSemanticId;
        else if(TradeMapModel.IsValid()&&TradeMapModel->GetSnapshot().bOpen)Focus=TradeMapModel->GetSnapshot().FocusedSemanticId;
        else if(ScenarioModel.IsValid()&&ScenarioModel->GetSnapshot().bOpen)Focus=ScenarioModel->GetSnapshot().FocusedSemanticId;
        else if(CityOverviewModel.IsValid()&&CityOverviewModel->GetSnapshot().bOpen)Focus=CityOverviewModel->GetSnapshot().FocusedSemanticId;
        else if(InspectorModel.IsValid()&&InspectorModel->GetSnapshot().bOpen)Focus=InspectorModel->GetSnapshot().FocusedSemanticId;
        const bool FullMarket=CityOverviewWidget.IsValid()&&CityOverviewWidget->IsFullMarket();
        auto Args=RebuildArguments;Args._Preferences=InPreferences;Args._InitialViewportSize=PhysicalViewportSize;
        UnbindModels();Construct(Args);if(FullMarket&&CityOverviewWidget)CityOverviewWidget->ActivateSemanticId(TEXT("CityOverview.Market.Details"));if(!Focus.IsNone())FocusSemanticId(Focus.ToString());
    }
    TSharedPtr<SWidget> SHansaRootHud::ResolveSemanticWidget(const FString& Id) const {
        if(Id.StartsWith(TEXT("Session.Help."))&&SessionCoach)return SessionCoach->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("Frontend."))&&FrontendWidget)return FrontendWidget->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("Research."))&&ResearchWidget)return ResearchWidget->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("Scenario."))&&ScenarioWidget)return ScenarioWidget->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("SaveLoad."))&&SaveLoadWidget)return SaveLoadWidget->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("TradeMap."))&&TradeMapWidget)return TradeMapWidget->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("Market."))&&CityOverviewWidget&&CityOverviewWidget->GetMarketTable())return CityOverviewWidget->GetMarketTable()->ResolveSemanticWidget(Id);
        if(Id.StartsWith(TEXT("CityOverview."))&&CityOverviewWidget)return CityOverviewWidget->ResolveSemanticWidget(Id);
        if(MinimapWidget && Id.StartsWith(TEXT("HUD.Minimap")))return MinimapWidget->Resolve(Id);
        const auto* W=AlertSemanticWidgets.Find(Id);if(!W)W=SemanticWidgets.Find(Id);return W?W->Pin():nullptr;
    }

	void SHansaRootHud::SetPresentationSize(const FIntPoint Size)
	{
		const bool WasWide=Layout.ViewportSize.X>=1500;
        const bool NowWide=Size.X/Preferences.UiScale>=1500;
        PhysicalViewportSize=Size;
        if(WasWide!=NowWide || (Layout.ViewportSize.X<1280)!=(Size.X/Preferences.UiScale<1280)){SetPreferences(Preferences);return;}
        Layout = MakeHudLayoutMetrics(FIntPoint(FMath::RoundToInt(Size.X/Preferences.UiScale),FMath::RoundToInt(Size.Y/Preferences.UiScale)));Layout.TopBarHeight=TopBarHeight(Layout,Preferences,Model.IsValid()&&Model->GetSnapshot().bRemoteCityView);
		ApplyResponsiveLayout();
		if (UHansaHudPresentationModel* PinnedModel = Model.Get())
		{
			Refresh(PinnedModel->GetSnapshot(), PinnedModel->GetRevision());
		}
	}

    void SHansaRootHud::LayoutConstructionTray()
    {
        const bool ReserveInspector=Layout.ViewportSize.X>=1600 || (InspectorModel.IsValid() && InspectorModel->GetSnapshot().bOpen);
        const float RightReserve=ReserveInspector?(Preferences.bLargeText?416.f:Layout.InspectorWidth)+24.f:24.f;
        const float Available=FMath::Max(120.f,Layout.ViewportSize.X-340.f-RightReserve);
        const float Width=FMath::Min(880.f,Available);
        if(BuildMenuHostBox){BuildMenuHostBox->SetWidthOverride(Width);BuildMenuHostBox->SetMaxDesiredHeight(Layout.ViewportSize.Y-Layout.TopBarHeight-Layout.SafeArea*3.f);}
        if(BuildMenuSlot){BuildMenuSlot->SetHorizontalAlignment(HAlign_Left);BuildMenuSlot->SetPadding(FMargin(340.f+FMath::Max(0.f,(Available-Width)*.5f),0.f,0.f,Layout.SafeArea));}
    }

	void SHansaRootHud::ApplyResponsiveLayout()
	{
		if (PresentationBox.IsValid())
		{
			PresentationBox->SetWidthOverride(static_cast<float>(Layout.ViewportSize.X));
			PresentationBox->SetHeightOverride(static_cast<float>(Layout.ViewportSize.Y));
		}
		if (TopStatusBox.IsValid()) TopStatusBox->SetHeightOverride(Layout.TopBarHeight);
		if (AlertHostBox.IsValid()) AlertHostBox->SetWidthOverride(Layout.AlertWidth);
		if (AlertRowsHostBox.IsValid()) AlertRowsHostBox->SetMaxDesiredHeight(FMath::Max(48.f,FMath::Min(360.f,Layout.ViewportSize.Y-Layout.TopBarHeight-3*Layout.SafeArea-(Layout.ViewportSize.X<1280?160.f:240.f)-96.f)));
		if (BottomHostBox.IsValid())
		{
			BottomHostBox->SetWidthOverride(Layout.BottomWidth);
			BottomHostBox->SetHeightOverride(Layout.BottomHeight);
		}
        LayoutConstructionTray();
		if (InspectorHostBox.IsValid()) InspectorHostBox->SetWidthOverride(Layout.InspectorWidth);
		if (NotificationHostBox.IsValid()) NotificationHostBox->SetWidthOverride(Layout.NotificationWidth);
		if (TopStatusSlot != nullptr) TopStatusSlot->SetPadding(FMargin(0));
		if (AlertSlot != nullptr) AlertSlot->SetPadding(FMargin(Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.TopBarHeight, 0.0f, 0.0f));
		if (BottomSlot != nullptr) BottomSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Layout.SafeArea));

		if (InspectorSlot != nullptr) InspectorSlot->SetPadding(FMargin(0.0f, Layout.SafeArea * 2.0f + Layout.TopBarHeight, Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.BottomHeight));
        if(InspectorModel.IsValid())RefreshInspectorHost(InspectorModel->GetSnapshot(),InspectorModel->GetRevision());
		if (NotificationSlot != nullptr) NotificationSlot->SetPadding(FMargin(Layout.SafeArea, 0.0f, 0.0f, 280.f));
		if (FocusSlot != nullptr) FocusSlot->SetPadding(FMargin(Layout.SafeArea, 0.0f, 0.0f, Layout.SafeArea));
		const FMargin ModalPadding(Layout.SafeArea);
        const FIntPoint ModalSize(Layout.ViewportSize.X-FMath::RoundToInt(Layout.SafeArea*2),Layout.ViewportSize.Y-FMath::RoundToInt(Layout.SafeArea*2));
        if(ResearchWidget)ResearchWidget->SetPresentationSize(ModalSize);
        if(FrontendWidget)FrontendWidget->SetPresentationSize(ModalSize);
        if(ScenarioWidget)ScenarioWidget->SetPresentationSize(ModalSize);
        if(SaveLoadWidget)SaveLoadWidget->SetPresentationSize(ModalSize);
		if (CityOverviewSlot != nullptr) CityOverviewSlot->SetPadding(ModalPadding);
		if (TradeMapSlot != nullptr) TradeMapSlot->SetPadding(ModalPadding);
		if (ResearchSlot != nullptr) ResearchSlot->SetPadding(ModalPadding);
		if (ScenarioSlot != nullptr) ScenarioSlot->SetPadding(ModalPadding);
		if (SaveLoadSlot != nullptr) SaveLoadSlot->SetPadding(ModalPadding);

		const EVisibility SecondaryStatusVisibility = EVisibility::Visible;
		if (PopulationText.IsValid()) PopulationText->SetVisibility(SecondaryStatusVisibility);
		if (WorkforceText.IsValid()) WorkforceText->SetVisibility(SecondaryStatusVisibility);
		if (ConnectionText.IsValid()) ConnectionText->SetVisibility(SecondaryStatusVisibility);
		if (TradeMapWidget.IsValid())
		{
			TradeMapWidget->SetPresentationSize(FIntPoint(
				FMath::Max(640, Layout.ViewportSize.X - FMath::RoundToInt(Layout.SafeArea * 2.0f)),
				FMath::Max(480, Layout.ViewportSize.Y - FMath::RoundToInt(Layout.SafeArea * 2.0f))));
		}
		if (CityOverviewWidget.IsValid()) CityOverviewWidget->SetPresentationSize(FIntPoint(
			FMath::Max(640, Layout.ViewportSize.X - FMath::RoundToInt(Layout.SafeArea * 2.0f)),
			FMath::Max(480, Layout.ViewportSize.Y - FMath::RoundToInt(Layout.SafeArea * 2.0f))));
	}

	TSharedRef<SWidget> SHansaRootHud::GetCaptureWidget() const
	{
		return PresentationBox.ToSharedRef();
	}

	void SHansaRootHud::Refresh(const FHansaHudPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		PresentedRevision = Revision;
		MoneyText->SetText(Snapshot.Money);
		MoneyTrendText->SetText(Snapshot.MoneyTrend);
		PopulationText->SetText(Snapshot.Population);
		WorkforceText->SetText(Snapshot.Workforce);
        WealthyText->SetText(Snapshot.WealthyCitizens.IsEmpty()?LOCTEXT("NoTier","—"):Snapshot.WealthyCitizens);
        MoneyChip->SetToolTipText(FText::Format(LOCTEXT("PlayerMoneyTip","Total player money across all cities: {0} marks."),Snapshot.Money));
        TrendChip->SetToolTipText(Snapshot.MoneyTrendTooltip);
        PopulationChip->SetToolTipText(FText::Format(LOCTEXT("CurrentPopulationTip","Total residents in the current city: {0}."),Snapshot.Population));
        LaborerChip->SetToolTipText(FText::Format(LOCTEXT("LaborersTip","Laborers living in the current city: {0}. This is the resident count, not available workforce."),Snapshot.Workforce));
        WealthyChip->SetToolTipText(FText::Format(LOCTEXT("WealthyTip","Artisan residents in the current city: {0}. Included in total citizens."),Snapshot.WealthyCitizens));
        for(int32 I=0;I<3;++I){
            const bool Present=Snapshot.TopProducts.IsValidIndex(I);
            ProductChips[I]->SetVisibility(Present?EVisibility::Visible:EVisibility::Collapsed);
            if(!Present)continue;
            ProductTexts[I]->SetText(Snapshot.TopProducts[I].Value);
            ProductChips[I]->SetToolTipText(Snapshot.TopProducts[I].Tooltip);
            const EUiGlyph Glyph=GlyphForGood(Snapshot.TopProducts[I].GoodId);
            ProductGlyphs[I]->SetGlyph(Glyph);
        }
		FString CityTitle=Snapshot.CityBreadcrumb.ToString(); int32 CitySlash; if(CityTitle.FindLastChar(TCHAR(47),CitySlash))CityTitle=CityTitle.Mid(CitySlash+1).TrimStartAndEnd(); CityBreadcrumbText->SetText(FText::FromString(CityTitle));CityBreadcrumbText->SetAutoWrapText(true);CityBreadcrumbText->SetToolTipText(Snapshot.CityBreadcrumb);
		DateText->SetText(Snapshot.DateAndSeason);
		ResearchText->SetText(FText::Format(LOCTEXT("ResearchCompact","Research\n{0}"),Snapshot.ResearchPoints));ResearchButton->SetToolTipText(Snapshot.Research);
		CityOverviewButton->SetToolTipText(FText::Format(LOCTEXT("CityNavigationTip","{0}. Open the city overview."),Snapshot.CityBreadcrumb));
		ConnectionText->SetText(Snapshot.Connection);ConnectionText->SetAutoWrapText(true);
		SelectionText->SetText(Snapshot.SelectionSummary);
        const bool RemoteLayoutChanged=ReturnCityButton && ReturnCityButton->GetVisibility()!=(Snapshot.bRemoteCityView?EVisibility::Visible:EVisibility::Collapsed);
        if(ReturnCityButton)ReturnCityButton->SetVisibility(Snapshot.bRemoteCityView?EVisibility::Visible:EVisibility::Collapsed);
        if(RemoteLayoutChanged)SetPresentationSize(PhysicalViewportSize);
        if(BuildMenuHostBox)BuildMenuHostBox->SetVisibility(Snapshot.bRemoteCityView?EVisibility::Collapsed:EVisibility::Visible);
		AlertToggleText->SetText(Snapshot.bAlertStackExpanded
			? FText::Format(LOCTEXT("HideAlerts", "Alerts ({0}) · Hide"), FText::AsNumber(Snapshot.Alerts.FilterByPredicate([](const auto& Alert){ return !Alert.bSnoozed; }).Num()))
			: FText::Format(LOCTEXT("ShowAlerts", "Alerts ({0}) · Show"), FText::AsNumber(Snapshot.Alerts.FilterByPredicate([](const auto& Alert){ return !Alert.bSnoozed; }).Num())));
		AlertRowsHostBox->SetVisibility(Snapshot.bAlertStackExpanded ? EVisibility::Visible : EVisibility::Collapsed);
		BottomAreaWidget->SetVisibility(!BuildModel.IsValid() && Snapshot.bBottomAreaOpen ? EVisibility::Visible : EVisibility::Collapsed);
		const UHansaInspectorPresentationModel* PinnedInspector = InspectorModel.Get();
		const bool bInspectorOpen = PinnedInspector != nullptr ? PinnedInspector->GetSnapshot().bOpen : Snapshot.bInspectorOpen;
		InspectorHostWidget->SetVisibility(bInspectorOpen ? EVisibility::Visible : EVisibility::Collapsed);
		FName ActiveFocus = Snapshot.FocusedSemanticId;
		if (SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen) ActiveFocus = SaveLoadModel->GetSnapshot().FocusedSemanticId;
		else if (ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen) ActiveFocus = ScenarioModel->GetSnapshot().FocusedSemanticId;
		else if (ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen) ActiveFocus = ResearchModel->GetSnapshot().FocusedSemanticId;
		else if (TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen) ActiveFocus = TradeMapModel->GetSnapshot().FocusedSemanticId;
		else if (CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen) ActiveFocus = CityOverviewModel->GetSnapshot().FocusedSemanticId;
		else if (InspectorModel.IsValid() && InspectorModel->GetSnapshot().bOpen) ActiveFocus = InspectorModel->GetSnapshot().FocusedSemanticId;
		UpdateFocusIndicator(ActiveFocus);
        StaticCastSharedPtr<SHansaAction>(PauseButton)->SetState(Snapshot.Speed==EHansaHudGameSpeed::Paused?EUiState::Selected:EUiState::Default,LOCTEXT("PauseTip","Pause the simulation."));
        StaticCastSharedPtr<SHansaAction>(NormalButton)->SetState(Snapshot.Speed==EHansaHudGameSpeed::Normal?EUiState::Selected:EUiState::Default,LOCTEXT("NormalTip","Normal speed: 1× simulation time."));
        StaticCastSharedPtr<SHansaAction>(FastButton)->SetState(Snapshot.Speed==EHansaHudGameSpeed::Fast?EUiState::Selected:EUiState::Default,LOCTEXT("FastTip","Fast speed: 4× simulation time."));
        StaticCastSharedPtr<SHansaAction>(FastestButton)->SetState(Snapshot.Speed==EHansaHudGameSpeed::Fastest?EUiState::Selected:EUiState::Default,LOCTEXT("FastestTip","Fastest speed: 12× simulation time."));
		RebuildAlerts(Snapshot);
		RebuildNotifications(Snapshot);
	}

	void SHansaRootHud::RefreshInspectorHost(const FHansaInspectorSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (InspectorHostWidget.IsValid()) InspectorHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);

        const bool Market=Snapshot.Kind==EHansaInspectorObjectKind::Market;
        const bool Preservation=!Snapshot.PreservationSummary.IsEmpty();
        const bool Compact=Snapshot.Production.bValid || Snapshot.Residence.bValid || Market;
        if(InspectorSlot)InspectorSlot->SetVerticalAlignment(VAlign_Top);
        if(InspectorHostBox.IsValid())
        {
            InspectorHostBox->SetWidthOverride(Preferences.bLargeText?416.f:Layout.InspectorWidth);
            // The construction tray reserves inspector width, so compact inspectors can
            // use the vertical space below it without overlapping controls.
            const float BottomReserve = Layout.ViewportSize.X < 1280 ? Layout.SafeArea : Layout.BottomHeight + Layout.SafeArea * 2.f;
            const float Available=FMath::Max(120.f,Layout.ViewportSize.Y-Layout.TopBarHeight-Layout.SafeArea*2.f-BottomReserve);
            InspectorHostBox->SetHeightOverride(FOptionalSize(FMath::Min(720.f,Available)));
        }
        LayoutConstructionTray();
        if(InspectorHostWidget.IsValid())StaticCastSharedPtr<SBorder>(InspectorHostWidget)->SetBorderImage(Compact?FCoreStyle::Get().GetBrush(TEXT("NoBorder")):&WorkingBrush);
		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
        if(Snapshot.bOpen&&Snapshot.Kind!=EHansaInspectorObjectKind::Cargo&&ScenarioModel.IsValid())ScenarioModel->OfferHelp(EHansaSessionHelpTopic::Inspection);
	}

	void SHansaRootHud::RefreshCityOverviewHost(const FHansaCityOverviewSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (CityOverviewHostWidget.IsValid()) CityOverviewHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);

		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
        if(Snapshot.bOpen&&Snapshot.ActiveTab==EHansaCityOverviewTab::Market&&ScenarioModel.IsValid())ScenarioModel->OfferHelp(EHansaSessionHelpTopic::Market);
	}

	void SHansaRootHud::RefreshTradeMapHost(const FHansaTradeMapSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (TradeMapHostWidget.IsValid()) TradeMapHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);

		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
        if(Snapshot.bOpen&&ScenarioModel.IsValid())ScenarioModel->OfferHelp(EHansaSessionHelpTopic::Routes);
	}

	void SHansaRootHud::RefreshResearchHost(const FHansaResearchPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (ResearchHostWidget.IsValid()) ResearchHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);

		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
	}

	void SHansaRootHud::RefreshScenarioHost(const FHansaScenarioPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (ScenarioHostWidget.IsValid()) ScenarioHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);

		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
	}

	void SHansaRootHud::RefreshSaveLoadHost(const FHansaSaveLoadPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (SaveLoadHostWidget.IsValid()) SaveLoadHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);

		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
	}

	void SHansaRootHud::UpdateFocusIndicator(const FName SemanticId)
	{
		if (!FocusLayerWidget.IsValid() || !FocusText.IsValid()) return;
		// Focus remains in the semantic tree; native controls draw their own focus rings.
		FocusLayerWidget->SetVisibility(EVisibility::Collapsed);
		(void)SemanticId;
		FocusText->SetText(FText::GetEmpty());
	}

	void SHansaRootHud::RestoreFocusFromSaveLoad(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		const auto Order=GetControllerFocusOrder();if(!Order.IsEmpty())FocusSemanticId(Order[0]);
	}
	void SHansaRootHud::RestoreFocusFromScenario(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
        const auto Order=GetControllerFocusOrder();if(!Order.IsEmpty())FocusSemanticId(Order[0]);
	}

	void SHansaRootHud::RestoreFocusFromResearch(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		FocusSemanticId(TEXT("HUD.TopStatus.Research.Open"));
	}

    void SHansaRootHud::RefreshAlertContent(const FHansaHudPresentationSnapshot& Snapshot)
    {
        for (const auto& Alert : Snapshot.Alerts)
        {
            FString Safe = Alert.StableId.ToString(); Safe.ReplaceInline(TEXT("."), TEXT("_"));
            const FString Base = TEXT("HUD.AlertStack.Alert.") + Safe;
            const FText Reason = FText::Format(LOCTEXT("AlertCausalTip", "Cause: {0}\nEvidence: {1}\nNext step: {2}"), Alert.Causal.Cause, Alert.Causal.Evidence, Alert.Causal.Remedy);
            if (const auto* Tracker = PinnedAlertTexts.Find(Alert.StableId))
                (*Tracker)->SetText(FText::Format(LOCTEXT("PinnedTrackerRow", "{0} · {1}"), Alert.AffectedObject, Alert.Causal.Problem));
            if (Alert.bSnoozed)
                if (const auto* Widget = AlertSemanticWidgets.Find(Base + TEXT(".Snooze")); Widget && Widget->IsValid())
                    StaticCastSharedPtr<SHansaAction>(Widget->Pin())->SetLabel(FText::Format(LOCTEXT("RestoreAlert","Restore: {0}"),Alert.AffectedObject));
            if (auto* Card = AlertCards.Find(Alert.StableId))
            {
                const bool Critical = Alert.Causal.Severity == EHansaCausalSeverity::Critical;
                const bool Warning = Alert.bWarning || Alert.Causal.Severity == EHansaCausalSeverity::Warning;
                Card->Identity->SetText(Alert.AffectedObject);
                Card->Age->SetText(Alert.Age); Card->Age->SetVisibility(EVisibility::Collapsed);
                Card->Title->SetText(Alert.Label);
                Card->Cause->SetText(Alert.Causal.Cause);
                Card->Cause->SetVisibility(EVisibility::Collapsed);
                Card->Icon->SetGlyph(Critical ? EUiGlyph::Error : Warning ? EUiGlyph::Warning : EUiGlyph::Information);
                Card->Accent->SetBorderBackgroundColor(UHansaUiStyleLibrary::GetSeverityStyle(Critical ? EHansaUiSeverity::Critical : Warning ? EHansaUiSeverity::Warning : EHansaUiSeverity::Notice).AccentColor);
            }
            if (const auto* Widget = AlertSemanticWidgets.Find(Base + TEXT(".OpenCause")); Widget && Widget->IsValid())
            {
                const auto Button = StaticCastSharedPtr<SHansaAction>(Widget->Pin());
                Button->SetToolTipText(Reason);
                if (!AlertCards.Contains(Alert.StableId)) Button->SetLabel(FText::Format(LOCTEXT("CompactAlertDetails", "{0} · Details"), Alert.Label));
            }
            if (const auto* Widget = AlertSemanticWidgets.Find(Base); Widget && Widget->IsValid()) Widget->Pin()->SetToolTipText(Reason);
        }
    }

    void SHansaRootHud::RebuildAlerts(const FHansaHudPresentationSnapshot& Snapshot)
    {
        if (PresentedAlerts == Snapshot.Alerts && AlertRows->NumSlots() > 0) return;
        // Age and economic evidence change every simulation tick. They must never
        // replace native buttons or restart layout, hover, tooltips and focus.
        bool SameStructure = PresentedAlerts.Num() == Snapshot.Alerts.Num() && AlertRows->NumSlots() > 0;
        for (int32 I = 0; SameStructure && I < Snapshot.Alerts.Num(); ++I)
        {
            const auto& A = PresentedAlerts[I]; const auto& B = Snapshot.Alerts[I];
            SameStructure = A.StableId == B.StableId && A.GroupId == B.GroupId && A.bSnoozed == B.bSnoozed && A.bPinned == B.bPinned;
        }
        PresentedAlerts = Snapshot.Alerts;
        if (SameStructure) { RefreshAlertContent(Snapshot); return; }
        const float ScrollOffset = AlertScroll->GetScrollOffset();
        FString Restore;
        if (FSlateApplication::IsInitialized()) for (const auto& Pair : AlertSemanticWidgets)
            if (Pair.Value.Pin() == FSlateApplication::Get().GetKeyboardFocusedWidget()) Restore = Pair.Key;
        AlertRows->ClearChildren(); PinnedTrackerRows->ClearChildren(); AlertSemanticWidgets.Reset(); AlertCards.Reset(); PinnedAlertTexts.Reset(); AlertActionHosts.Reset();
        auto SafeId = [](FName Id) { FString Value = Id.ToString(); Value.ReplaceInline(TEXT("."), TEXT("_")); return Value; };
        auto RegisterAction = [this](const FString& Id, TSharedPtr<SHansaAction> Button)
        {
            AlertSemanticWidgets.Add(Id, Button);
            Button->SetFocusHandler(FSimpleDelegate::CreateSP(this, &SHansaRootHud::RecordNativeFocus, FName(*Id)));
        };
        TArray<FName> Groups;
        for (const auto& Alert : Snapshot.Alerts)
        {
            if (!Alert.bSnoozed) Groups.AddUnique(Alert.GroupId);
            if (Alert.bPinned)
            {
                TSharedPtr<STextBlock> Tracker;
                PinnedTrackerRows->AddSlot().AutoHeight().Padding(0, 4)
                    [SAssignNew(Tracker,STextBlock).TextStyle(&DarkCaptionStyle).AutoWrapText(true)];
                PinnedAlertTexts.Add(Alert.StableId,Tracker);
            }
        }
        PinnedTrackersWidget->SetVisibility(PinnedTrackerRows->NumSlots() ? EVisibility::Visible : EVisibility::Collapsed);
        for (const auto& Alert : Snapshot.Alerts)
        {
            if(Alert.bSnoozed)continue;
            const FString Base=TEXT("HUD.AlertStack.Alert.")+SafeId(Alert.StableId);
            auto& Card=AlertCards.Add(Alert.StableId);
            TSharedPtr<SHansaAction> Main;
            TSharedPtr<SVerticalBox> Details;
            TSharedPtr<SBox> DetailBox;
            auto DetailContent=SAssignNew(DetailBox,SBox).Visibility(EVisibility::Collapsed)
                [SAssignNew(Details,SVerticalBox)];
            TSharedPtr<SHansaAction> Expand;
            auto Tablet=SNew(SHansaReferenceFrame).Padding(4)
                [SNew(SVerticalBox)
                 +SVerticalBox::Slot().AutoHeight()
                 [SNew(SHorizontalBox)
                  +SHorizontalBox::Slot().FillWidth(1)
                  [SAssignNew(Main,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences)
                   .OnClicked(this,&SHansaRootHud::HandleAlertAction,Alert.StableId,EHansaHudAlertAction::OpenCause)
                   [SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
                    [SAssignNew(Card.Accent,SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).Padding(6)
                     [SAssignNew(Card.Icon,SHansaGlyph).Size(32).OnDark(true)]]
                    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [SNew(SVerticalBox)
                     +SVerticalBox::Slot().AutoHeight()[SAssignNew(Card.Title,STextBlock).TextStyle(&DarkDataStyle).AutoWrapText(true)]
                     +SVerticalBox::Slot().AutoHeight()[SAssignNew(Card.Identity,STextBlock).TextStyle(&DarkCaptionStyle).AutoWrapText(true)]]]]
                  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                  [SAssignNew(Expand,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences)
                   .Reason(LOCTEXT("AlertOptions","Show alert actions"))
                   .OnClicked_Lambda([DetailBox]{DetailBox->SetVisibility(DetailBox->GetVisibility()==EVisibility::Collapsed?EVisibility::Visible:EVisibility::Collapsed);return FReply::Handled();})
                   [SNew(SHansaGlyph).Glyph(EUiGlyph::Down).Size(20)]]]
                 +SVerticalBox::Slot().AutoHeight()[DetailContent]];
            Card.Age=SNew(STextBlock);Card.Cause=SNew(STextBlock);
            AlertRows->AddSlot().AutoHeight().Padding(0,0,0,8)[Tablet];
            RegisterAction(Base+TEXT(".OpenCause"),Main);
            RegisterAction(Base+TEXT(".Options"),Expand); AlertActionHosts.Add(Base+TEXT(".Options"),DetailBox);
            AlertSemanticWidgets.Add(Base,Tablet);
            AlertSemanticWidgets.Add(TEXT("HUD.AlertStack.Group.")+SafeId(Alert.GroupId),Tablet);
            for(const auto& Action : {TPair<const TCHAR*,EHansaHudAlertAction>(TEXT("Frame"),EHansaHudAlertAction::Frame),
                {TEXT("Snooze"),EHansaHudAlertAction::Snooze},{TEXT("Pin"),EHansaHudAlertAction::Pin}})
            {
                TSharedPtr<SHansaAction> Button;
                const FText Label=Action.Value==EHansaHudAlertAction::Frame?LOCTEXT("AlertLocate","Locate"):
                    Action.Value==EHansaHudAlertAction::Snooze?LOCTEXT("SnoozeAlert","Snooze"):LOCTEXT("PinAlert","Pin");
                Details->AddSlot().AutoHeight()
                    [SAssignNew(Button,SHansaAction).Compact(true).Preferences(Preferences).Label(Label)
                     .OnClicked(this,&SHansaRootHud::HandleAlertAction,Alert.StableId,Action.Value)];
                RegisterAction(Base+TEXT(".")+Action.Key,Button);
                AlertActionHosts.Add(Base+TEXT(".")+Action.Key,DetailBox);
            }
        }
        for (const auto& Alert : Snapshot.Alerts) if (Alert.bSnoozed)
        {
            TSharedPtr<SHansaAction> Button;
            AlertRows->AddSlot().AutoHeight().Padding(0,4)
            [SAssignNew(Button,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences)
                .Label(FText::Format(LOCTEXT("RestoreAlert","Restore: {0}"),Alert.AffectedObject)).Reason(LOCTEXT("RestoreAlertTip","Show this snoozed alert again."))
                .OnClicked(this,&SHansaRootHud::HandleAlertAction,Alert.StableId,EHansaHudAlertAction::Snooze)];
            RegisterAction(TEXT("HUD.AlertStack.Alert.") + SafeId(Alert.StableId) + TEXT(".Snooze"),Button);
        }
        if (Snapshot.Alerts.IsEmpty()) AlertRows->AddSlot().AutoHeight().Padding(8)
            [SNew(STextBlock).Text(LOCTEXT("NoAlerts","No current alerts.")).TextStyle(&DarkBodyStyle).AutoWrapText(true)];
        RefreshAlertContent(Snapshot);
        AlertScroll->SetScrollOffset(ScrollOffset);
        if (!Restore.IsEmpty())
        {
            const auto* Target = AlertSemanticWidgets.Find(Restore);
            if (Target && Target->IsValid()) FSlateApplication::Get().SetKeyboardFocus(Target->Pin(),EFocusCause::SetDirectly);
            else FocusSemanticId(TEXT("HUD.AlertStack.Toggle"));
        }
    }

	void SHansaRootHud::RebuildNotifications(const FHansaHudPresentationSnapshot& Snapshot)
	{
		if(PresentedNotifications==Snapshot.Notifications)return;
		PresentedNotifications=Snapshot.Notifications;
		NotificationRows->ClearChildren();
		for (const FHansaHudNotificationPresentation& Notification : Snapshot.Notifications)
		{
			NotificationRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SNew(SHansaSurface).Surface(EUiSurface::Notification).Preferences(Preferences).State(EUiState::Default).Reason(Notification.Label)
			];
		}
	}

	void SHansaRootHud::RecordNativeFocus(FName Id){if(Model.IsValid())Model->SetFocusedSemanticId(Id);}

	void SHansaRootHud::MapWidget(const TCHAR* SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(SemanticId, Widget);
        if(Widget.IsValid() && Widget->GetType()==TEXT("SHansaAction"))StaticCastSharedPtr<SHansaAction>(Widget)->SetFocusHandler(FSimpleDelegate::CreateSP(this,&SHansaRootHud::RecordNativeFocus,FName(SemanticId)));
	}

	FReply SHansaRootHud::HandleAlertToggle()
	{
		if (UHansaHudPresentationModel* PinnedModel = Model.Get()) PinnedModel->ToggleAlertStack();
		return FReply::Handled();
	}

	FReply SHansaRootHud::HandleAlertAction(const FName AlertId, const EHansaHudAlertAction Action)
	{
		UHansaHudPresentationModel* PinnedModel = Model.Get(); UHansaInspectorPresentationModel* PinnedInspector = InspectorModel.Get();
		if (PinnedModel == nullptr) return FReply::Unhandled();
		const FHansaHudAlertPresentation* Existing = PinnedModel->FindAlert(AlertId); if (Existing == nullptr) return FReply::Unhandled();
		const FHansaHudAlertPresentation Alert = *Existing;
		const FString Suffix = Action == EHansaHudAlertAction::Frame ? TEXT("Frame") : Action == EHansaHudAlertAction::OpenCause ? TEXT("OpenCause") :
			Action == EHansaHudAlertAction::Snooze ? TEXT("Snooze") : TEXT("Pin");
		FString SafeAlertId = AlertId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
		const FName Origin(*FString::Printf(TEXT("HUD.AlertStack.Alert.%s.%s"), *SafeAlertId, *Suffix));
		if (!PinnedModel->ApplyAlertAction(AlertId, Action)) return FReply::Unhandled();
		if (AlertId == FName(TEXT("Objectives")) && Action == EHansaHudAlertAction::OpenCause)
		{
			if (UHansaScenarioPresentationModel* PinnedScenario = ScenarioModel.Get())
			{
				PinnedScenario->Open(Origin);
				return FReply::Handled();
			}
		}
		if ((Action == EHansaHudAlertAction::OpenCause || Action == EHansaHudAlertAction::Frame) && PinnedInspector != nullptr)
		{
			PinnedInspector->OpenFromAlert(Alert.StableId, Alert.AffectedObject, Alert.Age, Alert.AffectedBuildingValue, Alert.Causal, Origin);
			if (Action == EHansaHudAlertAction::OpenCause) PinnedInspector->OpenCauseIntent(); else PinnedInspector->FrameIntent();
		}
		return FReply::Handled();
	}

	void SHansaRootHud::RestoreFocusFromInspector(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
        const auto Order=GetControllerFocusOrder();if(!Order.IsEmpty())FocusSemanticId(Order[0]);
	}

	void SHansaRootHud::RestoreFocusFromCityOverview(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		FocusSemanticId(TEXT("HUD.TopStatus.CityOverview"));
	}

	void SHansaRootHud::RestoreFocusFromTradeMap(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		FocusSemanticId(TEXT("HUD.TopStatus.TradeMap"));
	}

	FReply SHansaRootHud::HandleCityOverviewOpen()
	{
		if (UHansaCityOverviewPresentationModel* Pinned = CityOverviewModel.Get())
		{
			if (!Pinned->Open(TEXT("HUD.TopStatus.CityOverview"))) return FReply::Unhandled();
			FocusSemanticId(TEXT("CityOverview.Close"));
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	FReply SHansaRootHud::HandleTradeMapOpen()
	{
		if (UHansaTradeMapPresentationModel* Pinned = TradeMapModel.Get())
		{
			if (UHansaCityOverviewPresentationModel* City = CityOverviewModel.Get(); City != nullptr && City->GetSnapshot().bOpen) City->CloseIntent();
			if (!Pinned->Open(TEXT("HUD.TopStatus.TradeMap"))) return FReply::Unhandled();
			FocusSemanticId(TEXT("TradeMap.Close"));
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	FReply SHansaRootHud::HandleSaveLoadOpen()
	{
		if (UHansaSaveLoadPresentationModel* Pinned = SaveLoadModel.Get())
		{
			if (UHansaCityOverviewPresentationModel* City = CityOverviewModel.Get(); City != nullptr && City->GetSnapshot().bOpen) City->CloseIntent();
			if (UHansaTradeMapPresentationModel* Trade = TradeMapModel.Get(); Trade != nullptr && Trade->GetSnapshot().bOpen) Trade->CloseIntent();
			if(ScenarioModel.IsValid()&&!ScenarioModel->GetSnapshot().bOpen)ScenarioModel->OpenPause();
            Pinned->Open(TEXT("HUD.TopStatus.SaveLoad"));
			FocusSemanticId(TEXT("SaveLoad.Close"));
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

    FReply SHansaRootHud::HandleSessionOpen()
    {
        if(FrontendWidget&&FrontendWidget->IsOpen())return FReply::Handled();
        if(!ScenarioModel.IsValid())return FReply::Unhandled();
        if(SaveLoadModel.IsValid()&&SaveLoadModel->GetSnapshot().bOpen)return FReply::Handled();
        if(ScenarioModel->GetSnapshot().bOpen&&ScenarioModel->GetSnapshot().bPauseMenu){ScenarioModel->Close();return FReply::Handled();}
        ScenarioModel->OpenPause();FocusSemanticId(ScenarioModel->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing?TEXT("Scenario.Begin"):TEXT("Scenario.Resume"));
        return FReply::Handled();
    }

	FReply SHansaRootHud::HandleResearchOpen()
	{
		if (UHansaResearchPresentationModel* Pinned = ResearchModel.Get())
		{
			if (UHansaCityOverviewPresentationModel* City = CityOverviewModel.Get(); City != nullptr && City->GetSnapshot().bOpen) City->CloseIntent();
			if (UHansaTradeMapPresentationModel* Trade = TradeMapModel.Get(); Trade != nullptr && Trade->GetSnapshot().bOpen) Trade->CloseIntent();
			Pinned->Open(TEXT("HUD.TopStatus.Research.Open"));
			FocusSemanticId(TEXT("Research.Close"));
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	FReply SHansaRootHud::HandleBottomToggle()
	{
		if (UHansaBuildMenuPresentationModel* PinnedBuildModel = BuildModel.Get())
		{
			PinnedBuildModel->SetOpen(!PinnedBuildModel->GetSnapshot().bOpen);
		}
		return FReply::Handled();
	}

	FReply SHansaRootHud::HandleSpeed(const EHansaHudGameSpeed Speed, const TCHAR* SemanticId)
	{
		if (UHansaHudPresentationModel* PinnedModel = Model.Get())
		{
			PinnedModel->SetSpeed(Speed);
			PinnedModel->SetFocusedSemanticId(FName(SemanticId));
		}
		return FReply::Handled();
	}

	FSlateColor SHansaRootHud::FocusColor(const TCHAR* SemanticId) const
	{
		const UHansaHudPresentationModel* PinnedModel = Model.Get();
		return PinnedModel != nullptr && PinnedModel->GetSnapshot().FocusedSemanticId == FName(SemanticId)
			? FSlateColor(UHansaUiStyleLibrary::GetFocusStyle().Color)
			: FSlateColor(FLinearColor::Transparent);
	}

	bool SHansaRootHud::ActivateSemanticId(const FString& SemanticId)
	{
        if(MinimapWidget && SemanticId.StartsWith(TEXT("HUD.Minimap")))return MinimapWidget->Activate(SemanticId);
        if(SemanticId==TEXT("HUD.TopStatus.Session"))return HandleSessionOpen().IsEventHandled();
        if(SemanticId.StartsWith(TEXT("Session.Help."))&&SessionCoach){const bool Done=SessionCoach->ActivateSemanticId(SemanticId);if(Done){const auto Order=GetControllerFocusOrder();if(!Order.IsEmpty())FocusSemanticId(Order[0]);}return Done;}

		if(SemanticId.EndsWith(TEXT(".Options"))){if(auto* Host=AlertActionHosts.Find(SemanticId)){(*Host)->SetVisibility((*Host)->GetVisibility()==EVisibility::Collapsed?EVisibility::Visible:EVisibility::Collapsed);return true;}}
        if(SemanticId.StartsWith(TEXT("HUD.AlertStack.Alert.")) && (!Model.IsValid() || !Model->GetSnapshot().bAlertStackExpanded || !AlertSemanticWidgets.Contains(SemanticId)))return false;
		if (SaveLoadWidget.IsValid() && SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen)
		{
			return SaveLoadWidget->ActivateSemanticId(SemanticId);
		}
		if(FrontendWidget&&FrontendWidget->IsOpen())return FrontendWidget->ActivateSemanticId(SemanticId);
        if (ScenarioWidget.IsValid() && ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen)
		{
			return ScenarioWidget->ActivateSemanticId(SemanticId);
		}
		if (ResearchWidget.IsValid() && ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen)
		{
			return ResearchWidget->ActivateSemanticId(SemanticId);
		}
		if(CityOverviewWidget.IsValid() && CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen)return CityOverviewWidget->ActivateSemanticId(SemanticId);
		if (SemanticId == TEXT("HUD.TopStatus.SaveLoad")) return HandleSaveLoadOpen().IsEventHandled();
		if (SemanticId == TEXT("HUD.TopStatus.Research.Open")) return HandleResearchOpen().IsEventHandled();
		if (SemanticId == TEXT("HUD.TopStatus.ReturnCity"))return Model.IsValid()&&Model->GetSnapshot().bRemoteCityView&&CityOverviewModel.IsValid()&&CityOverviewModel->VisitRequested&&CityOverviewModel->VisitRequested(TEXT("City.Lubeck"));
        if (SemanticId == TEXT("HUD.TopStatus.TradeMap")) return HandleTradeMapOpen().IsEventHandled();
		if (TradeMapWidget.IsValid() && TradeMapWidget->ActivateSemanticId(SemanticId)) return true;
		if (SemanticId == TEXT("HUD.TopStatus.CityOverview")) return HandleCityOverviewOpen().IsEventHandled();
		if (CityOverviewWidget.IsValid() && CityOverviewWidget->ActivateSemanticId(SemanticId)) return true;
		if (SemanticId == TEXT("HUD.AlertStack.Toggle")) return HandleAlertToggle().IsEventHandled();
		if (UHansaHudPresentationModel* PinnedModel = Model.Get())
		{
			for (const FHansaHudAlertPresentation& Alert : PinnedModel->GetSnapshot().Alerts)
			{
				FString SafeAlertId = Alert.StableId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
				const FString Base = FString::Printf(TEXT("HUD.AlertStack.Alert.%s"), *SafeAlertId);
				if (SemanticId == Base + TEXT(".Frame")) return HandleAlertAction(Alert.StableId, EHansaHudAlertAction::Frame).IsEventHandled();
				if (SemanticId == Base + TEXT(".OpenCause")) return HandleAlertAction(Alert.StableId, EHansaHudAlertAction::OpenCause).IsEventHandled();
				if (SemanticId == Base + TEXT(".Snooze")) return HandleAlertAction(Alert.StableId, EHansaHudAlertAction::Snooze).IsEventHandled();
				if (SemanticId == Base + TEXT(".Pin")) return HandleAlertAction(Alert.StableId, EHansaHudAlertAction::Pin).IsEventHandled();
			}
		}
		if (SemanticId == TEXT("HUD.BottomArea"))
		{
			HandleBottomToggle();
			return true;
		}
		if (SemanticId == TEXT("BuildMenu.Root")) { HandleBottomToggle(); return true; }
		if (BuildModel.IsValid() && BuildModel->IsConstructionAllowed() && BuildMenuWidget.IsValid() && BuildMenuWidget->ActivateSemanticId(SemanticId)) return true;
		if (InspectorWidget.IsValid() && InspectorWidget->ActivateSemanticId(SemanticId)) return true;
		if (SemanticId == TEXT("Inspector.Close") && !InspectorModel.IsValid())
		{
			if (UHansaHudPresentationModel* PinnedModel = Model.Get()) { PinnedModel->SetInspectorOpen(false); return true; }
		}
		if (SemanticId == TEXT("HUD.TopStatus.Speed.Pause")) return HandleSpeed(EHansaHudGameSpeed::Paused, *SemanticId).IsEventHandled();
		if (SemanticId == TEXT("HUD.TopStatus.Speed.Normal")) return HandleSpeed(EHansaHudGameSpeed::Normal, *SemanticId).IsEventHandled();
		if (SemanticId == TEXT("HUD.TopStatus.Speed.Fast")) return HandleSpeed(EHansaHudGameSpeed::Fast, *SemanticId).IsEventHandled();
		if (SemanticId == TEXT("HUD.TopStatus.Speed.Fastest")) return HandleSpeed(EHansaHudGameSpeed::Fastest, *SemanticId).IsEventHandled();
		return false;
	}

	bool SHansaRootHud::FocusSemanticId(const FString& SemanticId)
	{
        if(SemanticId.StartsWith(TEXT("Session.Help."))&&SessionCoach)return SessionCoach->FocusSemanticId(SemanticId);
		if (SaveLoadWidget.IsValid() && SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen)
		{
			return SaveLoadWidget->FocusSemanticId(SemanticId);
		}
		if(FrontendWidget&&FrontendWidget->IsOpen())return FrontendWidget->FocusSemanticId(SemanticId);
        if (ScenarioWidget.IsValid() && ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen)
		{
			return ScenarioWidget->FocusSemanticId(SemanticId);
		}
		if (ResearchWidget.IsValid() && ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen)
		{
			return ResearchWidget->FocusSemanticId(SemanticId);
		}
		if (TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen && TradeMapWidget.IsValid() && TradeMapWidget->FocusSemanticId(SemanticId)) return true;
		if (CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen && CityOverviewWidget.IsValid()) return CityOverviewWidget->FocusSemanticId(SemanticId);
		if (BuildModel.IsValid() && BuildModel->IsConstructionAllowed() && BuildMenuWidget.IsValid() && BuildMenuWidget->FocusSemanticId(SemanticId)) return true;
		if (InspectorWidget.IsValid() && InspectorWidget->FocusSemanticId(SemanticId)) return true;
		const TWeakPtr<SWidget>* Found = AlertSemanticWidgets.Find(SemanticId);
		if (Found == nullptr) Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found != nullptr ? Found->Pin() : nullptr;
		if (!Widget.IsValid() || !Widget->IsEnabled() || !GetControllerFocusOrder().Contains(SemanticId)) return false;
		if(!SemanticId.EndsWith(TEXT(".Options")))if(auto* Host=AlertActionHosts.Find(SemanticId))(*Host)->SetVisibility(EVisibility::Visible);
        if(UtilityMenu && (SemanticId==TEXT("HUD.TopStatus.Session")||SemanticId==TEXT("HUD.TopStatus.SaveLoad")||SemanticId==TEXT("HUD.TopStatus.TradeMap")))UtilityMenu->SetIsOpen(true);
        if(AlertSemanticWidgets.Contains(SemanticId))AlertScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);
		if (UHansaHudPresentationModel* PinnedModel = Model.Get()) PinnedModel->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
		return true;
	}

	TArray<FString> SHansaRootHud::GetControllerFocusOrder() const
	{
        auto WithCoach=[this](TArray<FString> Order){if(SessionCoach&&SessionCoach->IsOffered()){Order.Add(TEXT("Session.Help.Dismiss"));Order.Add(TEXT("Session.Help.Hide"));}return Order;};
		if (SaveLoadWidget.IsValid() && SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen)
		{
			return SaveLoadWidget->GetControllerFocusOrder();
		}
		if(FrontendWidget&&FrontendWidget->IsOpen())return FrontendWidget->GetControllerFocusOrder();
        if (ScenarioWidget.IsValid() && ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen)
		{
			return ScenarioWidget->GetControllerFocusOrder();
		}
		if (ResearchWidget.IsValid() && ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen)
		{
			return WithCoach(ResearchWidget->GetControllerFocusOrder());
		}
		if(CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen && CityOverviewWidget.IsValid())return WithCoach(CityOverviewWidget->GetControllerFocusOrder());
		TArray<FString> Result = {
			TEXT("HUD.TopStatus.Session"), TEXT("HUD.TopStatus.CityOverview"), TEXT("HUD.TopStatus.TradeMap"), TEXT("HUD.TopStatus.Research.Open"), TEXT("HUD.TopStatus.SaveLoad"), TEXT("HUD.TopStatus.Speed.Pause"), TEXT("HUD.TopStatus.Speed.Normal"),
			TEXT("HUD.TopStatus.Speed.Fast"), TEXT("HUD.TopStatus.Speed.Fastest"), TEXT("HUD.AlertStack.Toggle")
		};
        if(Model.IsValid()&&Model->GetSnapshot().bRemoteCityView)Result.Insert(TEXT("HUD.TopStatus.ReturnCity"),3);
		if (const UHansaHudPresentationModel* Pinned = Model.Get())
		{
			for (const FHansaHudAlertPresentation& Alert : Pinned->GetSnapshot().Alerts)
			{
				if (!Pinned->GetSnapshot().bAlertStackExpanded) continue;
				FString SafeAlertId = Alert.StableId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
				const FString Base = FString::Printf(TEXT("HUD.AlertStack.Alert.%s"), *SafeAlertId);
				for(const FString& Id:{Base+TEXT(".OpenCause"),Base+TEXT(".Options"),Base+TEXT(".Frame"),Base+TEXT(".Snooze"),Base+TEXT(".Pin")})if(AlertSemanticWidgets.Contains(Id))Result.Add(Id);
			}
		}
		if (CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen && CityOverviewWidget.IsValid())
		{
			Result.Append(CityOverviewWidget->GetControllerFocusOrder());
		}
		if (TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen && TradeMapWidget.IsValid())
		{
			Result.Append(TradeMapWidget->GetControllerFocusOrder());
		}
		if (InspectorModel.IsValid() && InspectorModel->GetSnapshot().bOpen && InspectorWidget.IsValid())
		{
			Result.Append(InspectorWidget->GetControllerFocusOrder());
		}
		if (BuildModel.IsValid() && BuildModel->IsConstructionAllowed() && BuildMenuWidget.IsValid())
		{
			Result.Append(BuildMenuWidget->GetControllerFocusOrder());
		}
		if(MinimapWidget)for(const TCHAR* Id:{TEXT("HUD.Minimap"),TEXT("HUD.Minimap.ZoomIn"),TEXT("HUD.Minimap.ZoomOut"),TEXT("HUD.Minimap.Overlay"),TEXT("HUD.Minimap.Center")})Result.Add(Id);
        return WithCoach(Result);
	}

	FReply SHansaRootHud::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
        if(FrontendWidget&&FrontendWidget->IsOpen()&&!(SaveLoadModel.IsValid()&&SaveLoadModel->GetSnapshot().bOpen))return FrontendWidget->OnKeyDown(MyGeometry,InKeyEvent);
        if(InKeyEvent.GetKey()==EKeys::Gamepad_Special_Right)return HandleSessionOpen();
        if(InKeyEvent.GetKey()==EKeys::F1||InKeyEvent.GetKey()==EKeys::Gamepad_Special_Left)return ActivateSemanticId(TEXT("Session.Help.Dismiss"))?FReply::Handled():FReply::Unhandled();
		const EHansaUiNavigationIntent Intent = ClassifyNavigationIntent(InKeyEvent);
		if (Intent == EHansaUiNavigationIntent::Back)
		{
			if (SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("SaveLoad.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("Scenario.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("Research.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("TradeMap.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("CityOverview.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (InspectorModel.IsValid() && InspectorModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("Inspector.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (BuildModel.IsValid() && BuildModel->GetSnapshot().bDemolitionMode) { BuildModel->CancelIntent(); return FReply::Handled(); }
			if (BuildModel.IsValid() && BuildModel->GetSnapshot().bOpen) { BuildModel->SetOpen(false); return FReply::Handled(); }
			return HandleSessionOpen();
		}
		FString Current;
		if (SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen) Current = SaveLoadModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen) Current = ScenarioModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen) Current = ResearchModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen) Current = TradeMapModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen) Current = CityOverviewModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (InspectorModel.IsValid() && InspectorModel->GetSnapshot().bOpen) Current = InspectorModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (BuildModel.IsValid() && BuildModel->GetSnapshot().bOpen) Current = BuildModel->GetSnapshot().FocusedSemanticId.ToString();
		else if (Model.IsValid()) Current = Model->GetSnapshot().FocusedSemanticId.ToString();
		if(FSlateApplication::IsInitialized()){
            const auto Focused=FSlateApplication::Get().GetKeyboardFocusedWidget();
            for(const auto& Id:GetControllerFocusOrder()){
                auto Widget=ResolveSemanticWidget(Id);if(!Widget && InspectorWidget)Widget=InspectorWidget->ResolveSemanticWidget(Id);if(!Widget && BuildMenuWidget)Widget=BuildMenuWidget->ResolveSemanticWidget(Id);
                if(Widget.IsValid() && Widget==Focused){Current=Id;break;}
            }
        }
        if (Intent == EHansaUiNavigationIntent::Activate)
		{
			return !Current.IsEmpty() && ActivateSemanticId(Current) ? FReply::Handled() : FReply::Unhandled();
		}
		if (Intent == EHansaUiNavigationIntent::Next || Intent == EHansaUiNavigationIntent::Previous)
		{
			const FString Target = FindWrappedFocusTarget(GetControllerFocusOrder(), Current, Intent == EHansaUiNavigationIntent::Next);
			return !Target.IsEmpty() && FocusSemanticId(Target) ? FReply::Handled() : FReply::Unhandled();
		}
		return FReply::Unhandled();
	}

	TArray<FHansaHudSemanticNode> SHansaRootHud::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Result;
		const UHansaHudPresentationModel* PinnedModel = Model.Get();
		if (PinnedModel == nullptr) return Result;
		const FHansaHudPresentationSnapshot& State = PinnedModel->GetSnapshot();
		auto Add = [this, &Result, &State](const FString& Id, const FString& Parent, const FString& Label,
			const EHansaHudSemanticRole Role, const bool bActivate = false, const bool bFocus = false,
			const FString& ValueType = FString(), const FString& Value = FString(), const bool bSelected = false,
			const bool bWarning = false, const TOptional<bool> VisibleOverride = {}, const bool bError = false)
		{
			FHansaHudSemanticNode Node;
			Node.Id = Id; Node.ParentId = Parent; Node.Label = Label; Node.Role = Role;
			Node.bCanActivate = bActivate; Node.bCanFocus = bFocus;
			Node.State.ValueType = ValueType; Node.State.Value = Value; Node.State.bSelected = bSelected;
			Node.State.bWarning = bWarning; Node.State.bError = bError; Node.State.bFocused = State.FocusedSemanticId == FName(*Id);
			const TWeakPtr<SWidget>* Found = AlertSemanticWidgets.Find(Id);
			if (Found == nullptr) Found = SemanticWidgets.Find(Id);
			if (Found != nullptr)
			{
				if (const TSharedPtr<SWidget> Widget = Found->Pin())
				{
					const FGeometry& Geometry = Widget->GetCachedGeometry();
					const FVector2f Origin = ScreenWidget->GetCachedGeometry().GetAbsolutePosition();
					const FVector2f Position = Geometry.GetAbsolutePosition() - Origin;
					const FVector2f Size = Geometry.GetDrawSize();
					Node.Bounds = FIntRect(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y),
						FMath::RoundToInt(Position.X + Size.X), FMath::RoundToInt(Position.Y + Size.Y));
					Node.State.bVisible = Widget->GetVisibility().IsVisible();
					Node.State.bEnabled = Widget->IsEnabled();
				}
			}
			if (VisibleOverride.IsSet()) Node.State.bVisible = VisibleOverride.GetValue();
			Result.Add(MoveTemp(Node));
		};

		Add(TEXT("HUD.Root"), TEXT(""), TEXT("Main HUD"), EHansaHudSemanticRole::Screen);
		Add(TEXT("HUD.TopStatus"), TEXT("HUD.Root"), TEXT("Top status"), EHansaHudSemanticRole::Panel);
        Add(TEXT("HUD.TopStatus.Influence"),TEXT("HUD.TopStatus"),TEXT("Influence"),EHansaHudSemanticRole::Status,false,false,TEXT("availability"),TEXT("unavailable"));
        if(MinimapWidget)for(const TCHAR* Id:{TEXT("HUD.Minimap"),TEXT("HUD.Minimap.ZoomIn"),TEXT("HUD.Minimap.ZoomOut"),TEXT("HUD.Minimap.Overlay"),TEXT("HUD.Minimap.Center")})Add(Id,TEXT("HUD.Root"),Id,EHansaHudSemanticRole::Button,true,true);
        Add(TEXT("HUD.TopStatus.LeftPanel"),TEXT("HUD.TopStatus"),TEXT("Player and city"),EHansaHudSemanticRole::Panel);
        Add(TEXT("HUD.TopStatus.CenterPanel"),TEXT("HUD.TopStatus"),TEXT("City production and citizens"),EHansaHudSemanticRole::Panel);
        Add(TEXT("HUD.TopStatus.RightPanel"),TEXT("HUD.TopStatus"),TEXT("Speed and navigation"),EHansaHudSemanticRole::Panel);
		Add(TEXT("HUD.TopStatus.Money"), TEXT("HUD.TopStatus"), TEXT("Money"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Money.ToString());
		Add(TEXT("HUD.TopStatus.MoneyTrend"), TEXT("HUD.TopStatus.Money"), TEXT("Money trend"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.MoneyTrend.ToString());
		Add(TEXT("HUD.TopStatus.Population"), TEXT("HUD.TopStatus"), TEXT("Population"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Population.ToString());
		Add(TEXT("HUD.TopStatus.Workforce"), TEXT("HUD.TopStatus"), TEXT("Laborers"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Workforce.ToString());
        Add(TEXT("HUD.TopStatus.WealthyCitizens"),TEXT("HUD.TopStatus"),TEXT("Artisans"),EHansaHudSemanticRole::Status,false,false,TEXT("text"),State.WealthyCitizens.ToString());
        for(int32 I=0;I<FMath::Min(3,State.TopProducts.Num());++I)Add(FString::Printf(TEXT("HUD.TopStatus.Product.%d"),I+1),TEXT("HUD.TopStatus"),State.TopProducts[I].GoodId.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("text"),State.TopProducts[I].Value.ToString(),false,false,false);
		Add(TEXT("HUD.TopStatus.CityBreadcrumb"), TEXT("HUD.TopStatus"), TEXT("Selected city"), EHansaHudSemanticRole::Text, false, false, TEXT("text"), State.CityBreadcrumb.ToString());
		const bool bCityOverviewOpen = CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen;
		Add(TEXT("HUD.TopStatus.CityOverview"), TEXT("HUD.TopStatus"), TEXT("Open City Overview"), EHansaHudSemanticRole::Button, true, true, TEXT("open"), bCityOverviewOpen ? TEXT("true") : TEXT("false"), bCityOverviewOpen);
		const bool bTradeMapOpen = TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen;
        if(Model.IsValid()&&Model->GetSnapshot().bRemoteCityView)Add(TEXT("HUD.TopStatus.ReturnCity"),TEXT("HUD.TopStatus"),TEXT("Return to Lübeck"),EHansaHudSemanticRole::Button);
		Add(TEXT("HUD.TopStatus.TradeMap"), TEXT("HUD.TopStatus"), TEXT("Open European trade map"), EHansaHudSemanticRole::Button, true, true, TEXT("open"), bTradeMapOpen ? TEXT("true") : TEXT("false"), bTradeMapOpen);
		const bool bResearchOpen = ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen;
        Add(TEXT("HUD.TopStatus.Session"),TEXT("HUD.TopStatus"),LOCTEXT("SessionMenu","Menu").ToString(),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("pause-menu"));
		Add(TEXT("HUD.TopStatus.Research.Open"), TEXT("HUD.TopStatus"), TEXT("Open research"), EHansaHudSemanticRole::Button, true, true, TEXT("open"), bResearchOpen ? TEXT("true") : TEXT("false"), bResearchOpen);
		const bool bSaveLoadOpen = SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen;
		Add(TEXT("HUD.TopStatus.SaveLoad"), TEXT("HUD.TopStatus"), TEXT("Open save and load"), EHansaHudSemanticRole::Button, true, true, TEXT("open"), bSaveLoadOpen ? TEXT("true") : TEXT("false"), bSaveLoadOpen);
		Add(TEXT("HUD.TopStatus.DateSeason"), TEXT("HUD.TopStatus"), TEXT("Date and season"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.DateAndSeason.ToString());
		Add(TEXT("HUD.TopStatus.Speed"), TEXT("HUD.TopStatus"), TEXT("Time speed"), EHansaHudSemanticRole::Panel, false, false, TEXT("game-speed"), SpeedValue(State.Speed));
		Add(TEXT("HUD.TopStatus.Speed.Pause"), TEXT("HUD.TopStatus.Speed"), TEXT("Pause"), EHansaHudSemanticRole::Button, true, true, TEXT("game-speed"), TEXT("paused"), State.Speed == EHansaHudGameSpeed::Paused);
		Add(TEXT("HUD.TopStatus.Speed.Normal"), TEXT("HUD.TopStatus.Speed"), TEXT("Normal speed"), EHansaHudSemanticRole::Button, true, true, TEXT("game-speed"), TEXT("normal"), State.Speed == EHansaHudGameSpeed::Normal);
		Add(TEXT("HUD.TopStatus.Speed.Fast"), TEXT("HUD.TopStatus.Speed"), TEXT("Fast speed"), EHansaHudSemanticRole::Button, true, true, TEXT("game-speed"), TEXT("fast"), State.Speed == EHansaHudGameSpeed::Fast);
		Add(TEXT("HUD.TopStatus.Speed.Fastest"), TEXT("HUD.TopStatus.Speed"), TEXT("Fastest speed"), EHansaHudSemanticRole::Button, true, true, TEXT("game-speed"), TEXT("fastest"), State.Speed == EHansaHudGameSpeed::Fastest);
		Add(TEXT("HUD.TopStatus.Research"), TEXT("HUD.TopStatus"), TEXT("Research"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Research.ToString());
		Add(TEXT("HUD.TopStatus.Connection"), TEXT("HUD.TopStatus"), TEXT("Connection"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Connection.ToString());
		Add(TEXT("HUD.TopStatus.FPS"), TEXT("HUD.TopStatus"), TEXT("Frames per second"), EHansaHudSemanticRole::Status,
			false, false, TEXT("frames-per-second"), FpsText.IsValid() ? FpsText->GetText().ToString() : TEXT("FPS —"));
		const bool bAnyWarning = State.Alerts.ContainsByPredicate([](const FHansaHudAlertPresentation& Alert) { return Alert.bWarning; });
		Add(TEXT("HUD.AlertStack"), TEXT("HUD.Root"), TEXT("Objectives and alerts"), EHansaHudSemanticRole::Alert, false, false, TEXT("expanded"), State.bAlertStackExpanded ? TEXT("true") : TEXT("false"), State.bAlertStackExpanded, bAnyWarning);
		Add(TEXT("HUD.AlertStack.Toggle"), TEXT("HUD.AlertStack"), TEXT("Toggle alerts"), EHansaHudSemanticRole::Button, true, true, TEXT("expanded"), State.bAlertStackExpanded ? TEXT("true") : TEXT("false"), State.bAlertStackExpanded);
		TArray<FName> SeenGroups;
		for (const FHansaHudAlertPresentation& Alert : State.Alerts)
		{
			FString SafeAlertId = Alert.StableId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
			FString SafeGroupId = Alert.GroupId.ToString(); SafeGroupId.ReplaceInline(TEXT("."), TEXT("_"));
			if (!SeenGroups.Contains(Alert.GroupId))
			{
				SeenGroups.Add(Alert.GroupId);
				int32 Count = 0;
				for (const FHansaHudAlertPresentation& Candidate : State.Alerts) Count += Candidate.GroupId == Alert.GroupId && !Candidate.bSnoozed ? 1 : 0;
				Add(FString::Printf(TEXT("HUD.AlertStack.Group.%s"), *SafeGroupId), TEXT("HUD.AlertStack"), Alert.GroupId.ToString(), EHansaHudSemanticRole::Panel,
					false, false, TEXT("count"), FString::FromInt(Count), false, Alert.bWarning, State.bAlertStackExpanded && Count > 0);
			}
			const FString AlertId = FString::Printf(TEXT("HUD.AlertStack.Alert.%s"), *SafeAlertId);
			Add(AlertId+TEXT(".Options"),AlertId,TEXT("Alert actions"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("expand"),false,false,State.bAlertStackExpanded&&!Alert.bSnoozed);
            const bool bCritical = Alert.Causal.Severity == EHansaCausalSeverity::Critical;
			const FString Value = FString::Printf(TEXT("severity=%s;age=%s;object=%s;cause=%s;remedy=%s;snoozed=%s;pinned=%s"),
				bCritical ? TEXT("critical") : Alert.bWarning ? TEXT("warning") : TEXT("notice"), *Alert.Age.ToString(), *Alert.AffectedObject.ToString(),
				*Alert.Causal.Cause.ToString(), *Alert.Causal.Remedy.ToString(), Alert.bSnoozed ? TEXT("true") : TEXT("false"), Alert.bPinned ? TEXT("true") : TEXT("false"));
			Add(AlertId, FString::Printf(TEXT("HUD.AlertStack.Group.%s"), *SafeGroupId), Alert.Label.ToString(), EHansaHudSemanticRole::Alert,
				false, true, TEXT("alert"), Value, false, Alert.bWarning, State.bAlertStackExpanded && !Alert.bSnoozed && AlertSemanticWidgets.Contains(AlertId), bCritical);
			for (const TPair<const TCHAR*, EHansaHudAlertAction>& Action : {
				TPair<const TCHAR*, EHansaHudAlertAction>(TEXT("Frame"), EHansaHudAlertAction::Frame),
				{ TEXT("OpenCause"), EHansaHudAlertAction::OpenCause }, { TEXT("Snooze"), EHansaHudAlertAction::Snooze }, { TEXT("Pin"), EHansaHudAlertAction::Pin } })
			{
				Add(AlertId + TEXT(".") + Action.Key, Alert.bSnoozed?FString(TEXT("HUD.AlertStack")):AlertId, Alert.bSnoozed?TEXT("Restore alert"):Action.Key, EHansaHudSemanticRole::Button, true, true,
					TEXT("alert-action"), Action.Key, Action.Value == EHansaHudAlertAction::Pin && Alert.bPinned, Alert.bWarning,
					State.bAlertStackExpanded && AlertSemanticWidgets.Contains(AlertId+TEXT(".")+Action.Key) && (!AlertActionHosts.Contains(AlertId+TEXT(".")+Action.Key) || AlertActionHosts[AlertId+TEXT(".")+Action.Key]->GetVisibility().IsVisible()), bCritical);
			}
		}
		int32 PinnedCount = 0;
		for (const FHansaHudAlertPresentation& Alert : State.Alerts) PinnedCount += Alert.bPinned ? 1 : 0;
		Add(TEXT("HUD.PinnedTrackers"), TEXT("HUD.AlertStack"), TEXT("Pinned tracking"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(PinnedCount), false, false, State.bAlertStackExpanded && PinnedCount > 0);
		for (const FHansaHudAlertPresentation& Alert : State.Alerts) if (Alert.bPinned)
		{
			FString SafeAlertId = Alert.StableId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
			Add(FString::Printf(TEXT("HUD.PinnedTrackers.Tracker.%s"), *SafeAlertId), TEXT("HUD.PinnedTrackers"), Alert.AffectedObject.ToString(), EHansaHudSemanticRole::Status,
				false, false, TEXT("tracker"), Alert.Causal.Problem.ToString(), true, Alert.bWarning);
		}
		Add(TEXT("HUD.BottomArea"), TEXT("HUD.Root"), TEXT("Build and selection"), EHansaHudSemanticRole::Panel, true, false, TEXT("open"), State.bBottomAreaOpen ? TEXT("true") : TEXT("false"), State.bBottomAreaOpen, false, !BuildModel.IsValid() && State.bBottomAreaOpen);
		const UHansaInspectorPresentationModel* PinnedInspector = InspectorModel.Get();
		const bool bInspectorOpen = PinnedInspector != nullptr ? PinnedInspector->GetSnapshot().bOpen : State.bInspectorOpen;
		Add(TEXT("HUD.InspectorHost"), TEXT("HUD.Root"), TEXT("Inspector host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bInspectorOpen ? TEXT("true") : TEXT("false"), bInspectorOpen, false, bInspectorOpen);
		Add(TEXT("HUD.NotificationLayer"), TEXT("HUD.Root"), TEXT("Notifications"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(State.Notifications.Num()), false, false, State.Notifications.Num() > 0);
		Add(TEXT("HUD.TooltipLayer"), TEXT("HUD.Root"), TEXT("Tooltips"), EHansaHudSemanticRole::Panel, false, false, TEXT("mode"), TEXT("short-and-expanded"));
		Add(TEXT("HUD.FocusLayer"), TEXT("HUD.Root"), TEXT("Controller focus"), EHansaHudSemanticRole::Status, false, false, TEXT("semantic-id"), State.FocusedSemanticId.ToString(), false, false, false);
		Add(TEXT("HUD.CityOverviewHost"), TEXT("HUD.Root"), TEXT("City Overview host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bCityOverviewOpen ? TEXT("true") : TEXT("false"), bCityOverviewOpen, false, bCityOverviewOpen);
		Add(TEXT("HUD.TradeMapHost"), TEXT("HUD.Root"), TEXT("European trade map host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bTradeMapOpen ? TEXT("true") : TEXT("false"), bTradeMapOpen, false, bTradeMapOpen);
		Add(TEXT("HUD.ResearchHost"), TEXT("HUD.Root"), TEXT("Research host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bResearchOpen ? TEXT("true") : TEXT("false"), bResearchOpen, false, bResearchOpen);
		const bool bScenarioOpen = ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen;
		Add(TEXT("HUD.SaveLoadHost"), TEXT("HUD.Root"), TEXT("Save and load host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bSaveLoadOpen ? TEXT("true") : TEXT("false"), bSaveLoadOpen, false, bSaveLoadOpen);
		Add(TEXT("HUD.ScenarioHost"), TEXT("HUD.Root"), TEXT("Scenario and victory host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bScenarioOpen ? TEXT("true") : TEXT("false"), bScenarioOpen, false, bScenarioOpen);
		if (BuildMenuWidget.IsValid() && BuildModel.IsValid() && BuildModel->IsConstructionAllowed())
		{
			auto Nodes=BuildMenuWidget->GetSemanticSnapshot();
			const FVector2D Delta=BuildMenuWidget->GetCachedGeometry().GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition();
			const FIntPoint Offset(FMath::RoundToInt(Delta.X),FMath::RoundToInt(Delta.Y));
			for(auto& Node:Nodes) { Node.Bounds.Min+=Offset;Node.Bounds.Max+=Offset; }
			Result.Append(Nodes);
		}
		if (InspectorWidget.IsValid()) {
            auto Nodes=InspectorWidget->GetSemanticSnapshot();const auto Delta=InspectorWidget->GetCachedGeometry().GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition();
            const FIntPoint Offset(FMath::RoundToInt(Delta.X),FMath::RoundToInt(Delta.Y));for(auto& N:Nodes){N.Bounds.Min+=Offset;N.Bounds.Max+=Offset;}Result.Append(Nodes);
        }
		if (CityOverviewWidget.IsValid()) {
            auto Nodes=CityOverviewWidget->GetSemanticSnapshot();const auto Delta=CityOverviewWidget->GetCachedGeometry().GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition();
            const FIntPoint Offset(FMath::RoundToInt(Delta.X),FMath::RoundToInt(Delta.Y));for(auto& N:Nodes){N.Bounds.Min+=Offset;N.Bounds.Max+=Offset;}Result.Append(Nodes);
        }
		if (TradeMapWidget.IsValid()) Result.Append(TradeMapWidget->GetSemanticSnapshot());
		if (ResearchWidget.IsValid()) Result.Append(ResearchWidget->GetSemanticSnapshot());
		if (ScenarioWidget.IsValid()) Result.Append(ScenarioWidget->GetSemanticSnapshot());
        if(SessionCoach)Result.Append(SessionCoach->GetSemanticSnapshot());
		if (SaveLoadWidget.IsValid()) Result.Append(SaveLoadWidget->GetSemanticSnapshot());
        if(FrontendWidget)Result.Append(FrontendWidget->GetSemanticSnapshot());
        const bool SaveModal=SaveLoadModel.IsValid()&&SaveLoadModel->GetSnapshot().bOpen;
        const bool SessionModal=ScenarioModel.IsValid()&&ScenarioModel->GetSnapshot().bOpen;
        const bool FrontModal=FrontendWidget&&FrontendWidget->IsOpen();
        if(SaveModal||FrontModal||SessionModal)for(auto& N:Result)if(!N.Id.StartsWith(SaveModal?TEXT("SaveLoad."):FrontModal?TEXT("Frontend."):TEXT("Scenario."))){N.State.bVisible=false;N.State.bEnabled=false;N.bCanActivate=false;N.bCanFocus=false;}

        if(CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen)for(auto& Node:Result)
            if(!Node.Id.StartsWith(TEXT("CityOverview.")) && !Node.Id.StartsWith(TEXT("Market.")) && !Node.Id.StartsWith(TEXT("Session.Help")) && !Node.Id.StartsWith(TEXT("Scenario.")) && !Node.Id.StartsWith(TEXT("SaveLoad.")) && !Node.Id.StartsWith(TEXT("Frontend."))){Node.State.bVisible=false;Node.State.bEnabled=false;}
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
