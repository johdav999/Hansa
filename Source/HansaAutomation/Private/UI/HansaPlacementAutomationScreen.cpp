#include "UI/HansaPlacementAutomationScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Gameplay/HansaPlacementAutomationFixture.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"

namespace Hansa::Automation
{
	namespace PlacementScreen
	{
		const FLinearColor BalticNavy = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("152A35")));
		const FLinearColor HarborBlue = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("2B5364")));
		const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("202628")));
		const FLinearColor Linen = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F2E9D8")));
		const FLinearColor Parchment = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("DFCFAF")));
		const FLinearColor Brass = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C19A52")));
		const FLinearColor Oxblood = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("7A2E2A")));
		const FLinearColor Success = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("3F7353")));
		const FLinearColor Chalk = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FAF7EF")));

		TSharedRef<SWidget> LabeledButton(
			TSharedPtr<SWidget>& OutWidget,
			const FText& Text,
			TFunction<FReply()> OnClicked,
			TAttribute<bool> Enabled = true,
			TAttribute<FSlateColor> Color = FSlateColor(Parchment))
		{
			TSharedPtr<SButton> Button;
			SAssignNew(Button, SButton)
				.Text(Text)
				.ContentPadding(FMargin(18.0f, 12.0f))
				.IsEnabled(Enabled)
				.ButtonColorAndOpacity(Color)
				.ForegroundColor(Chalk)
				.OnClicked_Lambda(MoveTemp(OnClicked));
			OutWidget = Button;
			return Button.ToSharedRef();
		}
	}

	void SHansaPlacementAutomationScreen::Construct(
		const FArguments& Arguments,
		FHansaPlacementAutomationFixture& InFixture,
		FHansaSemanticUiRegistry& InRegistry)
	{
		(void)Arguments;
		Fixture = &InFixture;
		Registry = &InRegistry;
		using namespace PlacementScreen;

		TSharedPtr<SWidget> CameraWidget;
		TSharedPtr<SWidget> MapWidget;
		TSharedPtr<SWidget> RoadTarget;
		TSharedPtr<SWidget> InvalidTarget;
		TSharedPtr<SWidget> ValidTarget;
		TSharedPtr<SWidget> PreviewWidget;
		TSharedPtr<SWidget> ValidationWidget;
		TSharedPtr<SWidget> CauseWidget;
		TSharedPtr<SWidget> RemedyWidget;
		TSharedPtr<SWidget> ToolbarWidget;
		TSharedPtr<SWidget> RoadTool;
		TSharedPtr<SWidget> WarehouseTool;
		TSharedPtr<SWidget> RotateAction;
		TSharedPtr<SWidget> RepeatAction;
		TSharedPtr<SWidget> ConfirmAction;
		TSharedPtr<SWidget> CancelAction;
		TSharedPtr<SWidget> ResultWidget;

		ChildSlot
		[
			SAssignNew(PresentationBox, SBox)
			.WidthOverride(1280.0f)
			.HeightOverride(720.0f)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SBox)
					.WidthOverride(1280.0f)
					.HeightOverride(720.0f)
					[
						SAssignNew(ScreenWidget, SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
						.BorderBackgroundColor(BalticNavy)
						.Padding(0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
								.BorderBackgroundColor(BalticNavy)
								.Padding(FMargin(28.0f, 14.0f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(TEXT("LÜBECK  ·  BUILD MODE")))
										.ColorAndOpacity(Chalk)
										.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))
									]
									+ SHorizontalBox::Slot().FillWidth(1.0f)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SAssignNew(CameraText, STextBlock)
										.Text(FText::FromString(TEXT("CAMERA  Focus −3200,−700   Yaw 35°   Zoom 6500")))
										.ColorAndOpacity(Parchment)
									]
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f).Padding(28.0f, 18.0f, 28.0f, 12.0f)
							[
								SAssignNew(MapWidget, SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
								.BorderBackgroundColor(HarborBlue)
								.Padding(18.0f)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(SBorder)
										.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
										.BorderBackgroundColor(Parchment.CopyWithNewOpacity(0.92f))
										.Padding(26.0f)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("WAREHOUSE DISTRICT  ·  OWNED LAND")))
												.ColorAndOpacity(Ink)
												.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
											]
											+ SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
											[
												SNew(SUniformGridPanel).SlotPadding(FMargin(18.0f))
												+ SUniformGridPanel::Slot(0, 0)
												[
													LabeledButton(RoadTarget, FText::FromString(TEXT("▦  ROAD CELL\n18,16")),
														[this] { return InvokeIntent([this] { return Fixture->TargetRoadCellIntent(); }); },
														TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Map.RoadTarget"))->State.bEnabled; }), Brass)
												]
												+ SUniformGridPanel::Slot(1, 0)
												[
													LabeledButton(InvalidTarget, FText::FromString(TEXT("⊘  DISCONNECTED\n10,10")),
														[this] { return InvokeIntent([this] { return Fixture->TargetInvalidCellIntent(); }); },
														TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Map.InvalidTarget"))->State.bEnabled; }), Oxblood)
												]
												+ SUniformGridPanel::Slot(2, 0)
												[
													LabeledButton(ValidTarget, FText::FromString(TEXT("✓  ROAD-ADJACENT\n16,16")),
														[this] { return InvokeIntent([this] { return Fixture->TargetValidCellIntent(); }); },
														TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Map.ValidTarget"))->State.bEnabled; }), Success)
												]
											]
										]
									]
									+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(22.0f)
									[
										SAssignNew(PreviewWidget, SBorder)
										.Visibility_Lambda([this] { return Registry->FindNode(TEXT("BuildMode.Placement.Preview"))->State.bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
										.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
										.BorderBackgroundColor(Brass.CopyWithNewOpacity(0.94f))
										.Padding(FMargin(14.0f, 9.0f))
										[
											SAssignNew(PreviewText, STextBlock).ColorAndOpacity(Ink)
										]
									]
									+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(22.0f)
									[
										SAssignNew(ValidationWidget, SBorder)
										.Visibility_Lambda([this] { return Registry->FindNode(TEXT("BuildMode.Placement.Validation"))->State.bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
										.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
										.BorderBackgroundColor(Linen)
										.Padding(FMargin(18.0f, 14.0f))
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight()
											[
												SAssignNew(ValidationText, STextBlock)
												.ColorAndOpacity(Oxblood)
												.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
											]
											+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f)
											[
												SAssignNew(CauseText, STextBlock).ColorAndOpacity(Ink)
											]
											+ SVerticalBox::Slot().AutoHeight()
											[
												SAssignNew(RemedyText, STextBlock).ColorAndOpacity(Ink)
											]
										]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(28.0f, 0.0f, 28.0f, 22.0f)
							[
								SAssignNew(ToolbarWidget, SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
								.BorderBackgroundColor(Linen)
								.Padding(12.0f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f)
									[
										LabeledButton(RoadTool, FText::FromString(TEXT("Road")),
											[this] { return InvokeIntent([this] { return Fixture->SelectRoadIntent(); }); }, true,
											TAttribute<FSlateColor>::CreateLambda([this] { return ToolColor(Registry->FindNode(TEXT("BuildMode.Tool.Road"))->State.bSelected); }))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f)
									[
										LabeledButton(WarehouseTool, FText::FromString(TEXT("Warehouse  2×3")),
											[this] { return InvokeIntent([this] { return Fixture->SelectWarehouseIntent(); }); }, true,
											TAttribute<FSlateColor>::CreateLambda([this] { return ToolColor(Registry->FindNode(TEXT("BuildMode.Tool.Warehouse"))->State.bSelected); }))
									]
									+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f)
									[
										SAssignNew(ResultWidget, SBorder)
										.Visibility_Lambda([this] { return Registry->FindNode(TEXT("BuildMode.Result.Building"))->State.bVisible ? EVisibility::Visible : EVisibility::Collapsed; })
										.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
										.BorderBackgroundColor(Success.CopyWithNewOpacity(0.16f))
										.Padding(FMargin(12.0f, 8.0f))
										[
											SAssignNew(ResultText, STextBlock).ColorAndOpacity(Ink)
										]
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f)
									[
										LabeledButton(RotateAction, FText::FromString(TEXT("Rotate")),
											[this] { return InvokeIntent([this] { return Fixture->RotateIntent(); }); },
											TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Action.Rotate"))->State.bEnabled; }))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f)
									[
										LabeledButton(RepeatAction, FText::FromString(TEXT("Repeat")),
											[this] { return InvokeIntent([this] { return Fixture->ToggleRepeatIntent(); }); },
											TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Action.Repeat"))->State.bEnabled; }))
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f)
									[
										LabeledButton(ConfirmAction, FText::FromString(TEXT("Confirm")),
											[this] { return InvokeIntent([this] { return Fixture->ConfirmIntent(); }); },
											TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Action.Confirm"))->State.bEnabled; }), Success)
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f)
									[
										LabeledButton(CancelAction, FText::FromString(TEXT("Cancel")),
											[this] { return InvokeIntent([this] { return Fixture->CancelIntent(); }); },
											TAttribute<bool>::CreateLambda([this] { return Registry->FindNode(TEXT("BuildMode.Action.Cancel"))->State.bEnabled; }), Oxblood)
									]
								]
							]
						]
					]
				]
			]
		];

		CameraWidget = CameraText;
		SemanticWidgets = {
			{ TEXT("BuildMode.Screen"), ScreenWidget }, { TEXT("BuildMode.Camera"), CameraWidget },
			{ TEXT("BuildMode.Map"), MapWidget }, { TEXT("BuildMode.Map.RoadTarget"), RoadTarget },
			{ TEXT("BuildMode.Map.InvalidTarget"), InvalidTarget }, { TEXT("BuildMode.Map.ValidTarget"), ValidTarget },
			{ TEXT("BuildMode.Placement.Preview"), PreviewWidget }, { TEXT("BuildMode.Placement.Validation"), ValidationWidget },
			{ TEXT("BuildMode.Placement.Validation.Cause"), CauseWidget }, { TEXT("BuildMode.Placement.Validation.Remedy"), RemedyWidget },
			{ TEXT("BuildMode.Toolbar"), ToolbarWidget }, { TEXT("BuildMode.Tool.Road"), RoadTool },
			{ TEXT("BuildMode.Tool.Warehouse"), WarehouseTool }, { TEXT("BuildMode.Action.Rotate"), RotateAction },
			{ TEXT("BuildMode.Action.Repeat"), RepeatAction }, { TEXT("BuildMode.Action.Confirm"), ConfirmAction },
			{ TEXT("BuildMode.Action.Cancel"), CancelAction }, { TEXT("BuildMode.Result.Building"), ResultWidget }
		};
		CauseWidget = CauseText;
		RemedyWidget = RemedyText;
		SemanticWidgets[TEXT("BuildMode.Placement.Validation.Cause")] = CauseText;
		SemanticWidgets[TEXT("BuildMode.Placement.Validation.Remedy")] = RemedyText;
		RefreshText();
	}

	FReply SHansaPlacementAutomationScreen::InvokeIntent(TFunction<bool()> Intent)
	{
		if (Intent && Intent())
		{
			RefreshText();
			SynchronizeSemantics();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	void SHansaPlacementAutomationScreen::RefreshText()
	{
		if (CameraText.IsValid())
		{
			if (const FHansaSemanticNode* Camera = Registry->FindNode(TEXT("BuildMode.Camera")))
			{
				FString Display = Camera->State.Value;
				Display.ReplaceInline(TEXT("focus="), TEXT("Focus "));
				Display.ReplaceInline(TEXT(";yawDegrees="), TEXT("   Yaw "));
				Display.ReplaceInline(TEXT(";zoomDistance="), TEXT("°   Zoom "));
				CameraText->SetText(FText::FromString(FString::Printf(TEXT("CAMERA  %s"), *Display)));
			}
		}
		auto SetFromSemantic = [this](const TCHAR* Id, const TSharedPtr<STextBlock>& Text)
		{
			if (Text.IsValid())
			{
				if (const FHansaSemanticNode* Node = Registry->FindNode(Id)) Text->SetText(FText::FromString(Node->Label));
			}
		};
		SetFromSemantic(TEXT("BuildMode.Placement.Preview"), PreviewText);
		SetFromSemantic(TEXT("BuildMode.Placement.Validation"), ValidationText);
		SetFromSemantic(TEXT("BuildMode.Placement.Validation.Cause"), CauseText);
		SetFromSemantic(TEXT("BuildMode.Placement.Validation.Remedy"), RemedyText);
		SetFromSemantic(TEXT("BuildMode.Result.Building"), ResultText);
	}

	FSlateColor SHansaPlacementAutomationScreen::ToolColor(const bool bSelected) const
	{
		return bSelected ? FSlateColor(PlacementScreen::Brass) : FSlateColor(PlacementScreen::Parchment);
	}

	void SHansaPlacementAutomationScreen::SetPresentationSize(const FIntPoint& Size)
	{
		PresentationBox->SetWidthOverride(static_cast<float>(Size.X));
		PresentationBox->SetHeightOverride(static_cast<float>(Size.Y));
	}

	TSharedRef<SWidget> SHansaPlacementAutomationScreen::GetCaptureWidget() const
	{
		return PresentationBox.ToSharedRef();
	}

	void SHansaPlacementAutomationScreen::UpdateGeometry(const FString& SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		const FHansaSemanticNode* Node = Registry->FindNode(SemanticId);
		if (Node == nullptr || !Widget.IsValid()) return;
		FHansaSemanticState State = Node->State;
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2f Origin = ScreenWidget->GetCachedGeometry().GetAbsolutePosition();
		const FVector2f Position = Geometry.GetAbsolutePosition() - Origin;
		const FVector2f Size = Geometry.GetDrawSize();
		Registry->UpdateNode(SemanticId, State, {
			FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y),
			FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y) });
	}

	void SHansaPlacementAutomationScreen::SynchronizeSemantics()
	{
		Fixture->SynchronizeSemantics();
		RefreshText();
		for (const TPair<FString, TSharedPtr<SWidget>>& Pair : SemanticWidgets)
		{
			UpdateGeometry(Pair.Key, Pair.Value);
		}
	}

	FHansaPlacementAutomationScreenHost::FHansaPlacementAutomationScreenHost(
		FHansaPlacementAutomationFixture& InFixture,
		FHansaSemanticUiRegistry& InRegistry)
		: Fixture(InFixture), Registry(InRegistry)
	{
	}

	FHansaPlacementAutomationScreenHost::~FHansaPlacementAutomationScreenHost()
	{
		if (Window.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
		}
	}

	bool FHansaPlacementAutomationScreenHost::EnsureScreen(const FIntPoint& ClientSize)
	{
		if (!Fixture.IsLoaded() || !FSlateApplication::IsInitialized() || ClientSize.X <= 0 || ClientSize.Y <= 0) return false;
		if (!Window.IsValid())
		{
			SAssignNew(Window, SWindow)
				.Title(FText::FromString(TEXT("Hansa Lübeck Build Automation")))
				.ClientSize(FVector2D(ClientSize.X, ClientSize.Y))
				.SizingRule(ESizingRule::FixedSize)
				.SupportsMaximize(false)
				.SupportsMinimize(false);
			SAssignNew(Screen, SHansaPlacementAutomationScreen, Fixture, Registry);
			Window->SetContent(Screen.ToSharedRef());
			FSlateApplication::Get().AddWindow(Window.ToSharedRef(), true);
			CurrentClientSize = ClientSize;
		}
		else if (CurrentClientSize != ClientSize)
		{
			Window->Resize(FVector2D(ClientSize.X, ClientSize.Y));
			CurrentClientSize = ClientSize;
		}
		Screen->SetPresentationSize(ClientSize);
		FSlateApplication::Get().ForceRedrawWindow(Window.ToSharedRef());
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationScreenHost::CaptureNative(const FIntPoint& Size, TArray<FColor>& OutPixels)
	{
		if (!EnsureScreen(Size) || !Screen.IsValid()) return false;
		FIntVector CapturedSize;
		const bool bCaptured = FSlateApplication::Get().TakeScreenshot(
			Screen->GetCaptureWidget(), FIntRect(0, 0, Size.X, Size.Y), OutPixels, CapturedSize);
		return bCaptured && CapturedSize.X == Size.X && CapturedSize.Y == Size.Y &&
			OutPixels.Num() == Size.X * Size.Y;
	}

	void FHansaPlacementAutomationScreenHost::SynchronizeSemantics()
	{
		if (GEngine != nullptr)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World == nullptr || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)) continue;
				for (TActorIterator<AHansaStrategyCameraPawn> Camera(World); Camera; ++Camera)
				{
					Fixture.ObserveCameraState(Camera->GetFocusLocation2D(), Camera->GetCameraYawDegrees(), Camera->GetZoomDistance());
					break;
				}
				AHansaLubeckWorldFoundation* Foundation = nullptr;
				AHansaPlacementProjectionManager* Manager = nullptr;
				for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It) { Foundation = *It; break; }
				for (TActorIterator<AHansaPlacementProjectionManager> It(World); It; ++It) { Manager = *It; break; }
				const auto Projection = Fixture.BuildProjection();
				if (Foundation != nullptr && Manager != nullptr && Projection)
				{
					Manager->Synchronize(Projection.Value, *Foundation);
				}
				break;
			}
		}
		if (Screen.IsValid()) Screen->SynchronizeSemantics();
		else Fixture.SynchronizeSemantics();
	}
}
