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
#include "UI/SHansaContextInspector.h"
#include "UI/SHansaBuildMenu.h"
#include "UI/SHansaResearchScreen.h"
#include "UI/SHansaTradeMap.h"
#include "UI/SHansaScenarioScreen.h"
#include "UI/SHansaSaveLoadScreen.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaRootHud"

namespace Hansa::UI
{
	namespace
	{
		TSharedRef<STextBlock> StatusText(const FTextBlockStyle* Style, TSharedPtr<STextBlock>& Out)
		{
			return SAssignNew(Out, STextBlock).TextStyle(Style).Clipping(EWidgetClipping::ClipToBounds);
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

	SHansaRootHud::~SHansaRootHud()
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
		Model = Arguments._Model;
		BuildModel = Arguments._BuildModel;
		InspectorModel = Arguments._InspectorModel;
		CityOverviewModel = Arguments._CityOverviewModel;
		MarketTableModel = Arguments._MarketTableModel;
		TradeMapModel = Arguments._TradeMapModel;
		ResearchModel = Arguments._ResearchModel;
		ScenarioModel = Arguments._ScenarioModel;
		SaveLoadModel = Arguments._SaveLoadModel;
		Layout = MakeHudLayoutMetrics(Arguments._InitialViewportSize);
		WorldOverlayBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		WorkingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		FloatingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Floating);
		IconButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Icon);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		DarkBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, true);
		DarkDataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, true);
		DarkCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, true);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);

		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FLinearColor Transparent = FLinearColor::Transparent;
		const FMargin Safe(Layout.SafeArea);

		ChildSlot
		[
			SAssignNew(PresentationBox, SBox)
			.WidthOverride(static_cast<float>(Layout.ViewportSize.X))
			.HeightOverride(static_cast<float>(Layout.ViewportSize.Y))
			[
				SAssignNew(ScreenWidget, SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(Transparent).Visibility(EVisibility::HitTestInvisible)
				]
				+ SOverlay::Slot().Expose(TopStatusSlot).HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(Safe)
				[
					SAssignNew(TopStatusBox, SBox).HeightOverride(Layout.TopBarHeight)
					[
						SAssignNew(TopStatusWidget, SBorder).BorderImage(&WorldOverlayBrush).Padding(FMargin(8.0f, 4.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("HouseCrest", "HANSA")).TextStyle(&DarkCaptionStyle)
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[StatusText(&DarkDataStyle, MoneyText)]
								+ SVerticalBox::Slot().AutoHeight()[StatusText(&DarkCaptionStyle, MoneyTrendText)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)[StatusText(&DarkDataStyle, PopulationText)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)[StatusText(&DarkDataStyle, WorkforceText)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f)
							[
								SAssignNew(CityOverviewButton, SButton).ButtonStyle(&IconButtonStyle)
								.ToolTipText(LOCTEXT("CityOverviewTip", "Open the selected city's Population, Production, and Market overview."))
								.OnClicked(this, &SHansaRootHud::HandleCityOverviewOpen)
								[
									StatusText(&DarkBodyStyle, CityBreadcrumbText)
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
							[
								SAssignNew(TradeMapButton, SButton).ButtonStyle(&IconButtonStyle)
								.ToolTipText(LOCTEXT("TradeMapTip", "Open the European trade map and Simple route editor."))
								.OnClicked(this, &SHansaRootHud::HandleTradeMapOpen)
								.Text(LOCTEXT("TradeMap", "Trade map"))
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)[StatusText(&DarkDataStyle, DateText)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
							[
								SAssignNew(SpeedGroupWidget, SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(PauseFocus, SBorder).BorderImage(WhiteBrush).Padding(2.0f)[SAssignNew(PauseButton, SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("Pause", "Ⅱ")).OnClicked(this, &SHansaRootHud::HandleSpeed, EHansaHudGameSpeed::Paused, TEXT("HUD.TopStatus.Speed.Pause"))]]
								+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(NormalFocus, SBorder).BorderImage(WhiteBrush).Padding(2.0f)[SAssignNew(NormalButton, SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("NormalSpeed", "▶")).OnClicked(this, &SHansaRootHud::HandleSpeed, EHansaHudGameSpeed::Normal, TEXT("HUD.TopStatus.Speed.Normal"))]]
								+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(FastFocus, SBorder).BorderImage(WhiteBrush).Padding(2.0f)[SAssignNew(FastButton, SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("FastSpeed", "▶▶")).OnClicked(this, &SHansaRootHud::HandleSpeed, EHansaHudGameSpeed::Fast, TEXT("HUD.TopStatus.Speed.Fast"))]]
								+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(FastestFocus, SBorder).BorderImage(WhiteBrush).Padding(2.0f)[SAssignNew(FastestButton, SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("FastestSpeed", "▶▶▶")).OnClicked(this, &SHansaRootHud::HandleSpeed, EHansaHudGameSpeed::Fastest, TEXT("HUD.TopStatus.Speed.Fastest"))]]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
							[
								SAssignNew(ResearchButton, SButton).ButtonStyle(&IconButtonStyle)
								.ToolTipText(LOCTEXT("ResearchTip", "Open research branches, prerequisites, effects, and queue."))
								.OnClicked(this, &SHansaRootHud::HandleResearchOpen)
								[StatusText(&DarkCaptionStyle, ResearchText)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)[StatusText(&DarkCaptionStyle, ConnectionText)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(SaveLoadButton, SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("SaveLoad", "Save / load")).ToolTipText(LOCTEXT("SaveLoadTip", "Open manual save and autosave slots.")).OnClicked(this, &SHansaRootHud::HandleSaveLoadOpen)]
						]
					]
				]
				+ SOverlay::Slot().Expose(AlertSlot).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.TopBarHeight, 0.0f, 0.0f)
				[
					SAssignNew(AlertHostBox, SBox).WidthOverride(Layout.AlertWidth)
					[
						SAssignNew(AlertStackWidget, SBorder).BorderImage(&WorkingBrush).Padding(12.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(AlertToggleButton, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaRootHud::HandleAlertToggle)
								[
									SAssignNew(AlertToggleText, STextBlock).TextStyle(&LightBodyStyle)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SAssignNew(AlertRowsHostBox, SBox).MaxDesiredHeight(Layout.AlertExpandedHeight)
								[
									SNew(SScrollBox)
									+ SScrollBox::Slot()
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SAssignNew(AlertRows, SVerticalBox)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
										[
											SAssignNew(PinnedTrackersWidget, SBorder).BorderImage(&FloatingBrush).Padding(6.0f)
											[
												SNew(SVerticalBox)
												+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PinnedTrackers", "◆ Pinned tracking")).TextStyle(&DarkCaptionStyle)]
												+ SVerticalBox::Slot().AutoHeight()[SAssignNew(PinnedTrackerRows, SVerticalBox)]
											]
										]
									]
								]
							]
						]
					]
				]
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
				+ SOverlay::Slot().Expose(BuildMenuSlot).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(Layout.SafeArea, 0.0f, Layout.SafeArea, Layout.SafeArea + Layout.BottomHeight + 8.0f)
				[
					SAssignNew(BuildMenuHostBox, SBox)
					.WidthOverride(FMath::Min(1248.0f, static_cast<float>(Layout.ViewportSize.X) - Layout.SafeArea * 2.0f))
					.HeightOverride(Layout.BuildMenuHeight)
					[
						SAssignNew(BuildMenuWidget, SHansaBuildMenu).Model(BuildModel.Get())
					]
				]
				+ SOverlay::Slot().Expose(InspectorSlot).HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(0.0f, Layout.SafeArea * 2.0f + Layout.TopBarHeight, Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.BottomHeight)
				[
					SAssignNew(InspectorHostBox, SBox).WidthOverride(Layout.InspectorWidth)
					[
						SAssignNew(InspectorHostWidget, SBorder).BorderImage(&WorkingBrush).Padding(0.0f)
						[
							SAssignNew(InspectorWidget, SHansaContextInspector).Model(InspectorModel.Get())
						]
					]
				]
				+ SOverlay::Slot().Expose(NotificationSlot).HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.BottomHeight)
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
				+ SOverlay::Slot().Expose(CityOverviewSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(CityOverviewHostWidget, SBox)
					[
						SAssignNew(CityOverviewWidget, SHansaCityOverview)
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
						SAssignNew(TradeMapWidget, SHansaTradeMap)
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
						SAssignNew(ResearchWidget, SHansaResearchScreen).Model(ResearchModel.Get())
					]
				]
				+ SOverlay::Slot().Expose(ScenarioSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(ScenarioHostWidget, SBox)
					[
						SAssignNew(ScenarioWidget, SHansaScenarioScreen)
						.Model(ScenarioModel.Get())
					]
				]
				+ SOverlay::Slot().Expose(SaveLoadSlot).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Layout.SafeArea)
				[
					SAssignNew(SaveLoadHostWidget, SBox)
					[
						SAssignNew(SaveLoadWidget, SHansaSaveLoadScreen).Model(SaveLoadModel.Get())
					]
				]
				]
		];

		MapWidget(TEXT("HUD.Root"), ScreenWidget);
		MapWidget(TEXT("HUD.TopStatus"), TopStatusWidget);
		MapWidget(TEXT("HUD.TopStatus.Money"), MoneyText);
		MapWidget(TEXT("HUD.TopStatus.MoneyTrend"), MoneyTrendText);
		MapWidget(TEXT("HUD.TopStatus.Population"), PopulationText);
		MapWidget(TEXT("HUD.TopStatus.Workforce"), WorkforceText);
		MapWidget(TEXT("HUD.TopStatus.CityBreadcrumb"), CityBreadcrumbText);
		MapWidget(TEXT("HUD.TopStatus.CityOverview"), CityOverviewButton);
		MapWidget(TEXT("HUD.TopStatus.TradeMap"), TradeMapButton);
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

	void SHansaRootHud::SetPresentationSize(const FIntPoint Size)
	{
		Layout = MakeHudLayoutMetrics(Size);
		ApplyResponsiveLayout();
		if (UHansaHudPresentationModel* PinnedModel = Model.Get())
		{
			Refresh(PinnedModel->GetSnapshot(), PinnedModel->GetRevision());
		}
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
		if (AlertRowsHostBox.IsValid()) AlertRowsHostBox->SetMaxDesiredHeight(Layout.AlertExpandedHeight);
		if (BottomHostBox.IsValid())
		{
			BottomHostBox->SetWidthOverride(Layout.BottomWidth);
			BottomHostBox->SetHeightOverride(Layout.BottomHeight);
		}
		if (BuildMenuHostBox.IsValid())
		{
			BuildMenuHostBox->SetWidthOverride(FMath::Min(1248.0f, static_cast<float>(Layout.ViewportSize.X) - Layout.SafeArea * 2.0f));
			BuildMenuHostBox->SetHeightOverride(Layout.BuildMenuHeight);
		}
		if (InspectorHostBox.IsValid()) InspectorHostBox->SetWidthOverride(Layout.InspectorWidth);
		if (NotificationHostBox.IsValid()) NotificationHostBox->SetWidthOverride(Layout.NotificationWidth);
		if (TopStatusSlot != nullptr) TopStatusSlot->SetPadding(FMargin(Layout.SafeArea));
		if (AlertSlot != nullptr) AlertSlot->SetPadding(FMargin(Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.TopBarHeight, 0.0f, 0.0f));
		if (BottomSlot != nullptr) BottomSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Layout.SafeArea));
		if (BuildMenuSlot != nullptr) BuildMenuSlot->SetPadding(FMargin(Layout.SafeArea, 0.0f, Layout.SafeArea, Layout.SafeArea + Layout.BottomHeight + 8.0f));
		if (InspectorSlot != nullptr) InspectorSlot->SetPadding(FMargin(0.0f, Layout.SafeArea * 2.0f + Layout.TopBarHeight, Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.BottomHeight));
		if (NotificationSlot != nullptr) NotificationSlot->SetPadding(FMargin(0.0f, 0.0f, Layout.SafeArea, Layout.SafeArea * 2.0f + Layout.BottomHeight));
		if (FocusSlot != nullptr) FocusSlot->SetPadding(FMargin(Layout.SafeArea, 0.0f, 0.0f, Layout.SafeArea));
		const FMargin ModalPadding(Layout.SafeArea);
		if (CityOverviewSlot != nullptr) CityOverviewSlot->SetPadding(ModalPadding);
		if (TradeMapSlot != nullptr) TradeMapSlot->SetPadding(ModalPadding);
		if (ResearchSlot != nullptr) ResearchSlot->SetPadding(ModalPadding);
		if (ScenarioSlot != nullptr) ScenarioSlot->SetPadding(ModalPadding);
		if (SaveLoadSlot != nullptr) SaveLoadSlot->SetPadding(ModalPadding);

		const EVisibility SecondaryStatusVisibility = Layout.ViewportSize.X < 1500 ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
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
		CityBreadcrumbText->SetText(Snapshot.CityBreadcrumb);
		DateText->SetText(Snapshot.DateAndSeason);
		ResearchText->SetText(Layout.ViewportSize.X < 1500 ? LOCTEXT("ResearchCompact", "Research") : Snapshot.Research);
		ConnectionText->SetText(Snapshot.Connection);
		SelectionText->SetText(Snapshot.SelectionSummary);
		AlertToggleText->SetText(Snapshot.bAlertStackExpanded
			? FText::Format(LOCTEXT("HideAlerts", "Objectives and alerts ({0})  ▲"), FText::AsNumber(Snapshot.Alerts.Num()))
			: FText::Format(LOCTEXT("ShowAlerts", "Objectives and alerts ({0})  ▼"), FText::AsNumber(Snapshot.Alerts.Num())));
		AlertRows->SetVisibility(Snapshot.bAlertStackExpanded ? EVisibility::Visible : EVisibility::Collapsed);
		BottomAreaWidget->SetVisibility(Snapshot.bBottomAreaOpen ? EVisibility::Visible : EVisibility::Collapsed);
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
		PauseFocus->SetBorderBackgroundColor(FocusColor(TEXT("HUD.TopStatus.Speed.Pause")));
		NormalFocus->SetBorderBackgroundColor(FocusColor(TEXT("HUD.TopStatus.Speed.Normal")));
		FastFocus->SetBorderBackgroundColor(FocusColor(TEXT("HUD.TopStatus.Speed.Fast")));
		FastestFocus->SetBorderBackgroundColor(FocusColor(TEXT("HUD.TopStatus.Speed.Fastest")));
		RebuildAlerts(Snapshot);
		RebuildNotifications(Snapshot);
	}

	void SHansaRootHud::RefreshInspectorHost(const FHansaInspectorSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (InspectorHostWidget.IsValid()) InspectorHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
	}

	void SHansaRootHud::RefreshCityOverviewHost(const FHansaCityOverviewSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (CityOverviewHostWidget.IsValid()) CityOverviewHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
	}

	void SHansaRootHud::RefreshTradeMapHost(const FHansaTradeMapSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		if (TradeMapHostWidget.IsValid()) TradeMapHostWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		if (Snapshot.bOpen) UpdateFocusIndicator(Snapshot.FocusedSemanticId);
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
		FocusLayerWidget->SetVisibility(SemanticId.IsNone() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
		FocusText->SetText(SemanticId.IsNone() ? FText::GetEmpty() :
			FText::Format(LOCTEXT("FocusedControl", "Keyboard/controller focus · {0}"), FText::FromName(SemanticId)));
	}

	void SHansaRootHud::RestoreFocusFromSaveLoad(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		FocusSemanticId(TEXT("HUD.TopStatus.SaveLoad"));
	}
	void SHansaRootHud::RestoreFocusFromScenario(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		FocusSemanticId(TEXT("HUD.AlertStack.Toggle"));
	}

	void SHansaRootHud::RestoreFocusFromResearch(const FName SemanticId)
	{
		if (!SemanticId.IsNone() && FocusSemanticId(SemanticId.ToString())) return;
		FocusSemanticId(TEXT("HUD.TopStatus.Research.Open"));
	}

	void SHansaRootHud::RebuildAlerts(const FHansaHudPresentationSnapshot& Snapshot)
	{
		AlertRows->ClearChildren(); PinnedTrackerRows->ClearChildren(); AlertSemanticWidgets.Reset();
		auto SafeId = [](const FName StableId)
		{
			FString Value = StableId.ToString(); Value.ReplaceInline(TEXT("."), TEXT("_")); return Value;
		};
		TArray<FName> Groups;
		for (const FHansaHudAlertPresentation& Alert : Snapshot.Alerts)
		{
			if (!Alert.bSnoozed && !Groups.Contains(Alert.GroupId)) Groups.Add(Alert.GroupId);
			if (Alert.bPinned)
			{
				PinnedTrackerRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(STextBlock).Text(FText::Format(LOCTEXT("PinnedTrackerRow", "◆ {0} · {1}"), Alert.AffectedObject, Alert.Causal.Problem)).TextStyle(&DarkCaptionStyle).AutoWrapText(true)
				]
			;
			}
		}
		PinnedTrackersWidget->SetVisibility(PinnedTrackerRows->NumSlots() > 0 ? EVisibility::Visible : EVisibility::Collapsed);

		int32 ExpandedGroups = 0;
		for (const FName GroupId : Groups)
		{
			if (ExpandedGroups++ >= 3) break;
			TArray<const FHansaHudAlertPresentation*> GroupAlerts;
			for (const FHansaHudAlertPresentation& Alert : Snapshot.Alerts) if (!Alert.bSnoozed && Alert.GroupId == GroupId) GroupAlerts.Add(&Alert);
			if (GroupAlerts.IsEmpty()) continue;
			const FString GroupSemanticId = FString::Printf(TEXT("HUD.AlertStack.Group.%s"), *SafeId(GroupId));
			TSharedPtr<SBorder> GroupWidget;
			AlertRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SAssignNew(GroupWidget, SBorder).BorderImage(&FloatingBrush).Padding(6.0f)
				[
					SNew(STextBlock).Text(FText::Format(LOCTEXT("AlertGroup", "△ {0} · {1}"), FText::FromName(GroupId), FText::AsNumber(GroupAlerts.Num()))).TextStyle(&DarkBodyStyle)
				]
			];
			AlertSemanticWidgets.Add(GroupSemanticId, GroupWidget);

			const FHansaHudAlertPresentation& Alert = *GroupAlerts[0];
			const EHansaUiSeverity SeverityToken = Alert.Causal.Severity == EHansaCausalSeverity::Critical ? EHansaUiSeverity::Critical :
				(Alert.bWarning ? EHansaUiSeverity::Warning : EHansaUiSeverity::Notice);
			const FHansaUiSeverityStyle Severity = UHansaUiStyleLibrary::GetSeverityStyle(SeverityToken);
			const FString AlertSemanticId = FString::Printf(TEXT("HUD.AlertStack.Alert.%s"), *SafeId(Alert.StableId));
			TSharedPtr<SBorder> AlertWidget;
			TSharedPtr<SUniformGridPanel> Actions;
			AlertRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)
			[
				SAssignNew(AlertWidget, SBorder).BorderImage(&WorkingBrush).BorderBackgroundColor(Severity.AccentColor.CopyWithNewOpacity(0.22f)).Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::Format(LOCTEXT("AlertSummary", "{0} {1} · {2}\n{3}\nCause · {4}"),
							Alert.Causal.Severity == EHansaCausalSeverity::Critical ? LOCTEXT("CriticalGlyph", "! Critical ·") :
								(Alert.bWarning ? LOCTEXT("WarningGlyph", "△ Warning ·") : LOCTEXT("NoticeGlyph", "i Notice ·")),
							Alert.AffectedObject, Alert.Age, Alert.Label, Alert.Causal.Cause)).TextStyle(&LightBodyStyle).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[SAssignNew(Actions, SUniformGridPanel).SlotPadding(FMargin(2.0f))]
				]
			];
			AlertSemanticWidgets.Add(AlertSemanticId, AlertWidget);
			int32 ActionIndex = 0;
			auto AddAction = [this, &Actions, &Alert, &SafeId, &ActionIndex](const TCHAR* Suffix, const FText& Label, const EHansaHudAlertAction Action, const FText& ToolTip)
			{
				const FString Id = FString::Printf(TEXT("HUD.AlertStack.Alert.%s.%s"), *SafeId(Alert.StableId), Suffix);
				TSharedPtr<SButton> Button;
				Actions->AddSlot(ActionIndex % 2, ActionIndex / 2)
				[
					SNew(SBox).MinDesiredHeight(UHansaUiStyleLibrary::GetSpacing(EHansaUiSpacingToken::ControllerFocusTarget))
					[
						SAssignNew(Button, SButton).ButtonStyle(&IconButtonStyle).ContentPadding(FMargin(6.0f, 4.0f)).Text(Label).ToolTipText(ToolTip).OnClicked(this, &SHansaRootHud::HandleAlertAction, Alert.StableId, Action)
					]
				];
				++ActionIndex;
				AlertSemanticWidgets.Add(Id, Button);
			};
			AddAction(TEXT("Frame"), LOCTEXT("FrameAlert", "⌖ Frame"), EHansaHudAlertAction::Frame, LOCTEXT("FrameAlertTip", "Frame the affected object [F]."));
			AddAction(TEXT("OpenCause"), LOCTEXT("OpenCauseAlert", "? Cause"), EHansaHudAlertAction::OpenCause,
				FText::Format(LOCTEXT("OpenCauseAlertTip", "Open causal inspector [C].\nCause: {0}\nRemedy: {1}"), Alert.Causal.Cause, Alert.Causal.Remedy));
			AddAction(TEXT("Snooze"), LOCTEXT("SnoozeAlert", "◷ Snooze"), EHansaHudAlertAction::Snooze, LOCTEXT("SnoozeAlertTip", "Snooze this alert for the current presentation interval."));
			AddAction(TEXT("Pin"), Alert.bPinned ? LOCTEXT("PinnedAlert", "◆ Pinned") : LOCTEXT("PinAlert", "◇ Pin"), EHansaHudAlertAction::Pin, LOCTEXT("PinAlertTip", "Toggle persistent pinned tracking."));
		}
	}

	void SHansaRootHud::RebuildNotifications(const FHansaHudPresentationSnapshot& Snapshot)
	{
		NotificationRows->ClearChildren();
		for (const FHansaHudNotificationPresentation& Notification : Snapshot.Notifications)
		{
			NotificationRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SNew(SBorder).BorderImage(&FloatingBrush).Padding(10.0f)[SNew(STextBlock).Text(Notification.Label).TextStyle(&DarkBodyStyle).AutoWrapText(true)]
			];
		}
	}

	void SHansaRootHud::MapWidget(const TCHAR* SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(SemanticId, Widget);
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
		FocusSemanticId(TEXT("HUD.AlertStack.Toggle"));
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
			Pinned->Open(TEXT("HUD.TopStatus.SaveLoad"));
			FocusSemanticId(TEXT("SaveLoad.Close"));
			return FReply::Handled();
		}
		return FReply::Unhandled();
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
		if (SaveLoadWidget.IsValid() && SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen)
		{
			return SaveLoadWidget->ActivateSemanticId(SemanticId);
		}
		if (ScenarioWidget.IsValid() && ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen)
		{
			return ScenarioWidget->ActivateSemanticId(SemanticId);
		}
		if (ResearchWidget.IsValid() && ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen)
		{
			return ResearchWidget->ActivateSemanticId(SemanticId);
		}
		if (SemanticId == TEXT("HUD.TopStatus.SaveLoad")) return HandleSaveLoadOpen().IsEventHandled();
		if (SemanticId == TEXT("HUD.TopStatus.Research.Open")) return HandleResearchOpen().IsEventHandled();
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
		if (BuildMenuWidget.IsValid() && BuildMenuWidget->ActivateSemanticId(SemanticId)) return true;
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
		if (SaveLoadWidget.IsValid() && SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen)
		{
			return SaveLoadWidget->FocusSemanticId(SemanticId);
		}
		if (ScenarioWidget.IsValid() && ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen)
		{
			return ScenarioWidget->FocusSemanticId(SemanticId);
		}
		if (ResearchWidget.IsValid() && ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen)
		{
			return ResearchWidget->FocusSemanticId(SemanticId);
		}
		if (TradeMapWidget.IsValid() && TradeMapWidget->FocusSemanticId(SemanticId)) return true;
		if (CityOverviewWidget.IsValid() && CityOverviewWidget->FocusSemanticId(SemanticId)) return true;
		if (BuildMenuWidget.IsValid() && BuildMenuWidget->FocusSemanticId(SemanticId)) return true;
		if (InspectorWidget.IsValid() && InspectorWidget->FocusSemanticId(SemanticId)) return true;
		const TWeakPtr<SWidget>* Found = AlertSemanticWidgets.Find(SemanticId);
		if (Found == nullptr) Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found != nullptr ? Found->Pin() : nullptr;
		if (!Widget.IsValid()) return false;
		if (UHansaHudPresentationModel* PinnedModel = Model.Get()) PinnedModel->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
		return true;
	}

	TArray<FString> SHansaRootHud::GetControllerFocusOrder() const
	{
		if (SaveLoadWidget.IsValid() && SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen)
		{
			return SaveLoadWidget->GetControllerFocusOrder();
		}
		if (ScenarioWidget.IsValid() && ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen)
		{
			return ScenarioWidget->GetControllerFocusOrder();
		}
		if (ResearchWidget.IsValid() && ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen)
		{
			return ResearchWidget->GetControllerFocusOrder();
		}
		TArray<FString> Result = {
			TEXT("HUD.TopStatus.CityOverview"), TEXT("HUD.TopStatus.TradeMap"), TEXT("HUD.TopStatus.Research.Open"), TEXT("HUD.TopStatus.SaveLoad"), TEXT("HUD.TopStatus.Speed.Pause"), TEXT("HUD.TopStatus.Speed.Normal"),
			TEXT("HUD.TopStatus.Speed.Fast"), TEXT("HUD.TopStatus.Speed.Fastest"), TEXT("HUD.AlertStack.Toggle")
		};
		if (const UHansaHudPresentationModel* Pinned = Model.Get())
		{
			for (const FHansaHudAlertPresentation& Alert : Pinned->GetSnapshot().Alerts)
			{
				if (Alert.bSnoozed || !Pinned->GetSnapshot().bAlertStackExpanded) continue;
				FString SafeAlertId = Alert.StableId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
				const FString Base = FString::Printf(TEXT("HUD.AlertStack.Alert.%s"), *SafeAlertId);
				Result.Append({ Base + TEXT(".Frame"), Base + TEXT(".OpenCause"), Base + TEXT(".Snooze"), Base + TEXT(".Pin") });
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
		if (BuildModel.IsValid() && BuildModel->GetSnapshot().bOpen && BuildMenuWidget.IsValid())
		{
			Result.Append(BuildMenuWidget->GetControllerFocusOrder());
		}
		return Result;
	}

	FReply SHansaRootHud::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		const EHansaUiNavigationIntent Intent = ClassifyNavigationIntent(InKeyEvent);
		if (Intent == EHansaUiNavigationIntent::Back)
		{
			if (SaveLoadModel.IsValid() && SaveLoadModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("SaveLoad.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("Scenario.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("Research.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("TradeMap.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("CityOverview.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (InspectorModel.IsValid() && InspectorModel->GetSnapshot().bOpen) return ActivateSemanticId(TEXT("Inspector.Close")) ? FReply::Handled() : FReply::Unhandled();
			if (BuildModel.IsValid() && BuildModel->GetSnapshot().bOpen) { BuildModel->SetOpen(false); return FReply::Handled(); }
			return FReply::Unhandled();
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
		Add(TEXT("HUD.TopStatus.Money"), TEXT("HUD.TopStatus"), TEXT("Money"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Money.ToString());
		Add(TEXT("HUD.TopStatus.MoneyTrend"), TEXT("HUD.TopStatus.Money"), TEXT("Money trend"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.MoneyTrend.ToString());
		Add(TEXT("HUD.TopStatus.Population"), TEXT("HUD.TopStatus"), TEXT("Population"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Population.ToString());
		Add(TEXT("HUD.TopStatus.Workforce"), TEXT("HUD.TopStatus"), TEXT("Workforce"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), State.Workforce.ToString());
		Add(TEXT("HUD.TopStatus.CityBreadcrumb"), TEXT("HUD.TopStatus"), TEXT("Selected city"), EHansaHudSemanticRole::Text, false, false, TEXT("text"), State.CityBreadcrumb.ToString());
		const bool bCityOverviewOpen = CityOverviewModel.IsValid() && CityOverviewModel->GetSnapshot().bOpen;
		Add(TEXT("HUD.TopStatus.CityOverview"), TEXT("HUD.TopStatus"), TEXT("Open City Overview"), EHansaHudSemanticRole::Button, true, true, TEXT("open"), bCityOverviewOpen ? TEXT("true") : TEXT("false"), bCityOverviewOpen);
		const bool bTradeMapOpen = TradeMapModel.IsValid() && TradeMapModel->GetSnapshot().bOpen;
		Add(TEXT("HUD.TopStatus.TradeMap"), TEXT("HUD.TopStatus"), TEXT("Open European trade map"), EHansaHudSemanticRole::Button, true, true, TEXT("open"), bTradeMapOpen ? TEXT("true") : TEXT("false"), bTradeMapOpen);
		const bool bResearchOpen = ResearchModel.IsValid() && ResearchModel->GetSnapshot().bOpen;
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
			const bool bCritical = Alert.Causal.Severity == EHansaCausalSeverity::Critical;
			const FString Value = FString::Printf(TEXT("severity=%s;age=%s;object=%s;cause=%s;remedy=%s;snoozed=%s;pinned=%s"),
				bCritical ? TEXT("critical") : Alert.bWarning ? TEXT("warning") : TEXT("notice"), *Alert.Age.ToString(), *Alert.AffectedObject.ToString(),
				*Alert.Causal.Cause.ToString(), *Alert.Causal.Remedy.ToString(), Alert.bSnoozed ? TEXT("true") : TEXT("false"), Alert.bPinned ? TEXT("true") : TEXT("false"));
			Add(AlertId, FString::Printf(TEXT("HUD.AlertStack.Group.%s"), *SafeGroupId), Alert.Label.ToString(), EHansaHudSemanticRole::Alert,
				false, true, TEXT("alert"), Value, false, Alert.bWarning, State.bAlertStackExpanded && !Alert.bSnoozed, bCritical);
			for (const TPair<const TCHAR*, EHansaHudAlertAction>& Action : {
				TPair<const TCHAR*, EHansaHudAlertAction>(TEXT("Frame"), EHansaHudAlertAction::Frame),
				{ TEXT("OpenCause"), EHansaHudAlertAction::OpenCause }, { TEXT("Snooze"), EHansaHudAlertAction::Snooze }, { TEXT("Pin"), EHansaHudAlertAction::Pin } })
			{
				Add(AlertId + TEXT(".") + Action.Key, AlertId, Action.Key, EHansaHudSemanticRole::Button, true, true,
					TEXT("alert-action"), Action.Key, Action.Value == EHansaHudAlertAction::Pin && Alert.bPinned, Alert.bWarning,
					State.bAlertStackExpanded && !Alert.bSnoozed, bCritical);
			}
		}
		int32 PinnedCount = 0;
		for (const FHansaHudAlertPresentation& Alert : State.Alerts) PinnedCount += Alert.bPinned ? 1 : 0;
		Add(TEXT("HUD.PinnedTrackers"), TEXT("HUD.AlertStack"), TEXT("Pinned tracking"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(PinnedCount), false, false, PinnedCount > 0);
		for (const FHansaHudAlertPresentation& Alert : State.Alerts) if (Alert.bPinned)
		{
			FString SafeAlertId = Alert.StableId.ToString(); SafeAlertId.ReplaceInline(TEXT("."), TEXT("_"));
			Add(FString::Printf(TEXT("HUD.PinnedTrackers.Tracker.%s"), *SafeAlertId), TEXT("HUD.PinnedTrackers"), Alert.AffectedObject.ToString(), EHansaHudSemanticRole::Status,
				false, false, TEXT("tracker"), Alert.Causal.Problem.ToString(), true, Alert.bWarning);
		}
		Add(TEXT("HUD.BottomArea"), TEXT("HUD.Root"), TEXT("Build and selection"), EHansaHudSemanticRole::Panel, true, false, TEXT("open"), State.bBottomAreaOpen ? TEXT("true") : TEXT("false"), State.bBottomAreaOpen, false, State.bBottomAreaOpen);
		const UHansaInspectorPresentationModel* PinnedInspector = InspectorModel.Get();
		const bool bInspectorOpen = PinnedInspector != nullptr ? PinnedInspector->GetSnapshot().bOpen : State.bInspectorOpen;
		Add(TEXT("HUD.InspectorHost"), TEXT("HUD.Root"), TEXT("Inspector host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bInspectorOpen ? TEXT("true") : TEXT("false"), bInspectorOpen, false, bInspectorOpen);
		Add(TEXT("HUD.NotificationLayer"), TEXT("HUD.Root"), TEXT("Notifications"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(State.Notifications.Num()), false, false, State.Notifications.Num() > 0);
		Add(TEXT("HUD.TooltipLayer"), TEXT("HUD.Root"), TEXT("Tooltips"), EHansaHudSemanticRole::Panel, false, false, TEXT("mode"), TEXT("short-and-expanded"));
		Add(TEXT("HUD.FocusLayer"), TEXT("HUD.Root"), TEXT("Controller focus"), EHansaHudSemanticRole::Status, false, false, TEXT("semantic-id"), State.FocusedSemanticId.ToString(), false, false, !State.FocusedSemanticId.IsNone());
		Add(TEXT("HUD.CityOverviewHost"), TEXT("HUD.Root"), TEXT("City Overview host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bCityOverviewOpen ? TEXT("true") : TEXT("false"), bCityOverviewOpen, false, bCityOverviewOpen);
		Add(TEXT("HUD.TradeMapHost"), TEXT("HUD.Root"), TEXT("European trade map host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bTradeMapOpen ? TEXT("true") : TEXT("false"), bTradeMapOpen, false, bTradeMapOpen);
		Add(TEXT("HUD.ResearchHost"), TEXT("HUD.Root"), TEXT("Research host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bResearchOpen ? TEXT("true") : TEXT("false"), bResearchOpen, false, bResearchOpen);
		const bool bScenarioOpen = ScenarioModel.IsValid() && ScenarioModel->GetSnapshot().bOpen;
		Add(TEXT("HUD.SaveLoadHost"), TEXT("HUD.Root"), TEXT("Save and load host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bSaveLoadOpen ? TEXT("true") : TEXT("false"), bSaveLoadOpen, false, bSaveLoadOpen);
		Add(TEXT("HUD.ScenarioHost"), TEXT("HUD.Root"), TEXT("Scenario and victory host"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), bScenarioOpen ? TEXT("true") : TEXT("false"), bScenarioOpen, false, bScenarioOpen);
		if (BuildMenuWidget.IsValid()) Result.Append(BuildMenuWidget->GetSemanticSnapshot());
		if (InspectorWidget.IsValid()) Result.Append(InspectorWidget->GetSemanticSnapshot());
		if (CityOverviewWidget.IsValid()) Result.Append(CityOverviewWidget->GetSemanticSnapshot());
		if (TradeMapWidget.IsValid()) Result.Append(TradeMapWidget->GetSemanticSnapshot());
		if (ResearchWidget.IsValid()) Result.Append(ResearchWidget->GetSemanticSnapshot());
		if (ScenarioWidget.IsValid()) Result.Append(ScenarioWidget->GetSemanticSnapshot());
		if (SaveLoadWidget.IsValid()) Result.Append(SaveLoadWidget->GetSemanticSnapshot());
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
