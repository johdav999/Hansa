#include "UI/HansaAutomationProofScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace Hansa::Automation
{
	namespace
	{
		const FLinearColor BalticNavy = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("152A35")));
		const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("202628")));
		const FLinearColor MutedInk = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("596160")));
		const FLinearColor Linen = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("F2E9D8")));
		const FLinearColor Parchment = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("DFCFAF")));
		const FLinearColor Brass = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C19A52")));
		const FLinearColor Brick = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A44C3F")));
		const FLinearColor WarningAmber = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D09132")));
		const FLinearColor Chalk = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FAF7EF")));

		FHansaSemanticNode MakeNode(
			const TCHAR* Id,
			const EHansaSemanticRole Role,
			const TCHAR* Label,
			const TCHAR* ParentId = TEXT(""),
			TArray<EHansaSemanticAction> Actions = {})
		{
			FHansaSemanticNode Node;
			Node.Id = Id;
			Node.Role = Role;
			Node.Label = Label;
			Node.ParentId = ParentId;
			Node.Actions = MoveTemp(Actions);
			return Node;
		}
	}

	void SHansaAutomationProofScreen::Construct(
		const FArguments& Arguments,
		FHansaSemanticUiRegistry& InRegistry)
	{
		(void)Arguments;
		Registry = &InRegistry;

		ChildSlot
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SAssignNew(PresentationBox, SBox)
			.WidthOverride(1280.0f)
			.HeightOverride(720.0f)
			[
			SAssignNew(ScreenWidget, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(BalticNavy)
			.Padding(FMargin(80.0f, 56.0f))
			[
				SAssignNew(PanelWidget, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(Linen)
				.Padding(FMargin(40.0f, 32.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SAssignNew(TitleWidget, STextBlock)
						.Text(FText::FromString(TEXT("Automation Proof")))
						.ColorAndOpacity(Ink)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 24.0f)
					[
						SAssignNew(StatusWidget, STextBlock)
						.Text(FText::FromString(TEXT("Ready — semantic state observable")))
						.ColorAndOpacity(MutedInk)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 24.0f)
					[
						SNew(SUniformGridPanel).SlotPadding(FMargin(8.0f))
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
							.BorderBackgroundColor(Brick)
							.Padding(2.0f)
							[
								SAssignNew(ActivateButton, SButton)
								.Text(FText::FromString(TEXT("Activate")))
								.ForegroundColor(Chalk)
								.ButtonColorAndOpacity(Brick)
								.ContentPadding(FMargin(24.0f, 12.0f))
								.OnClicked(this, &SHansaAutomationProofScreen::HandleActivate)
							]
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
							.BorderBackgroundColor(this, &SHansaAutomationProofScreen::GetFocusRingColor)
							.Padding(3.0f)
							[
								SAssignNew(FocusButton, SButton)
								.Text(FText::FromString(TEXT("Focus target")))
								.ForegroundColor(Chalk)
								.ButtonColorAndOpacity(Parchment)
								.ContentPadding(FMargin(24.0f, 12.0f))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(WarningWidget, SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
						.BorderBackgroundColor(WarningAmber.CopyWithNewOpacity(0.24f))
						.Padding(FMargin(16.0f, 12.0f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("[!] Reference warning — evidence remains development-only")))
							.ColorAndOpacity(Ink)
						]
					]
				]
			]
			]
		];

		Registry->RegisterNode(MakeNode(TEXT("AutomationProof.Screen"), EHansaSemanticRole::Screen, TEXT("Automation Proof")));
		Registry->RegisterNode(MakeNode(TEXT("AutomationProof.Panel"), EHansaSemanticRole::Panel, TEXT("Working panel"), TEXT("AutomationProof.Screen")));
		Registry->RegisterNode(MakeNode(TEXT("AutomationProof.Title"), EHansaSemanticRole::Heading, TEXT("Automation Proof"), TEXT("AutomationProof.Panel")));
		Registry->RegisterNode(MakeNode(TEXT("AutomationProof.Status"), EHansaSemanticRole::Status, TEXT("Ready — semantic state observable"), TEXT("AutomationProof.Panel")));
		Registry->RegisterNode(
			MakeNode(TEXT("AutomationProof.Activate"), EHansaSemanticRole::Button, TEXT("Activate"), TEXT("AutomationProof.Panel"), { EHansaSemanticAction::Activate, EHansaSemanticAction::Focus }),
			FHansaSemanticActionHandlers {
				[this] { return ActivateFromAutomation(); },
				[this]
				{
					if (!ActivateButton.IsValid() || !FSlateApplication::IsInitialized()) return false;
					FSlateApplication::Get().SetKeyboardFocus(ActivateButton, EFocusCause::SetDirectly);
					SynchronizeSemantics();
					return true;
				}
			});
		Registry->RegisterNode(
			MakeNode(TEXT("AutomationProof.FocusTarget"), EHansaSemanticRole::Button, TEXT("Focus target"), TEXT("AutomationProof.Panel"), { EHansaSemanticAction::Activate, EHansaSemanticAction::Focus }),
			FHansaSemanticActionHandlers {
				[this] { bAutomationFocusRequested = !bAutomationFocusRequested; SynchronizeSemantics(); return true; },
				[this] { return FocusFromAutomation(); }
			});
		Registry->RegisterNode(MakeNode(TEXT("AutomationProof.Warning"), EHansaSemanticRole::Alert, TEXT("Reference warning"), TEXT("AutomationProof.Panel")));
	}

	void SHansaAutomationProofScreen::SetPresentationSize(const FIntPoint& Size)
	{
		if (PresentationBox.IsValid())
		{
			PresentationBox->SetWidthOverride(static_cast<float>(Size.X));
			PresentationBox->SetHeightOverride(static_cast<float>(Size.Y));
		}
	}

	TSharedRef<SWidget> SHansaAutomationProofScreen::GetCaptureWidget() const
	{
		return PresentationBox.ToSharedRef();
	}

	FReply SHansaAutomationProofScreen::HandleActivate()
	{
		ActivateFromAutomation();
		return FReply::Handled();
	}

	bool SHansaAutomationProofScreen::ActivateFromAutomation()
	{
		bActivated = !bActivated;
		if (StatusWidget.IsValid())
		{
			StatusWidget->SetText(FText::FromString(bActivated
				? TEXT("Activated — selected state true")
				: TEXT("Ready — semantic state observable")));
		}
		if (Registry != nullptr)
		{
			Registry->SetLabel(TEXT("AutomationProof.Status"), bActivated
				? TEXT("Activated — selected state true")
				: TEXT("Ready — semantic state observable"));
		}
		SynchronizeSemantics();
		return true;
	}

	bool SHansaAutomationProofScreen::FocusFromAutomation()
	{
		if (!FocusButton.IsValid() || !FSlateApplication::IsInitialized())
		{
			return false;
		}
		bAutomationFocusRequested = true;
		FSlateApplication::Get().SetKeyboardFocus(FocusButton, EFocusCause::SetDirectly);
		SynchronizeSemantics();
		return true;
	}

	FSlateColor SHansaAutomationProofScreen::GetFocusRingColor() const
	{
		return (FocusButton.IsValid() && FocusButton->HasKeyboardFocus())
			? FSlateColor(Brass)
			: FSlateColor(FLinearColor::Transparent);
	}

	void SHansaAutomationProofScreen::UpdateNodeGeometry(
		const FString& Id,
		const TSharedPtr<SWidget>& Widget,
		FHansaSemanticState State)
	{
		if (Registry == nullptr || !Widget.IsValid())
		{
			return;
		}
		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2f Origin = ScreenWidget.IsValid()
			? ScreenWidget->GetCachedGeometry().GetAbsolutePosition()
			: FVector2f::ZeroVector;
		const FVector2f Position = Geometry.GetAbsolutePosition() - Origin;
		const FVector2f Size = Geometry.GetDrawSize();
		State.bVisible = Widget->GetVisibility().IsVisible();
		State.bEnabled = Widget->IsEnabled();
		Registry->UpdateNode(Id, State, {
			FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y),
			FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y) });
	}

	void SHansaAutomationProofScreen::SynchronizeSemantics()
	{
		FHansaSemanticState Default;
		UpdateNodeGeometry(TEXT("AutomationProof.Screen"), ScreenWidget, Default);
		UpdateNodeGeometry(TEXT("AutomationProof.Panel"), PanelWidget, Default);
		UpdateNodeGeometry(TEXT("AutomationProof.Title"), TitleWidget, Default);
		FHansaSemanticState StatusState;
		StatusState.bSelected = bActivated;
		UpdateNodeGeometry(TEXT("AutomationProof.Status"), StatusWidget, StatusState);
		FHansaSemanticState ActivateState;
		ActivateState.bFocused = ActivateButton.IsValid() && ActivateButton->HasKeyboardFocus();
		ActivateState.bSelected = bActivated;
		UpdateNodeGeometry(TEXT("AutomationProof.Activate"), ActivateButton, ActivateState);
		FHansaSemanticState FocusState;
		FocusState.bFocused = FocusButton.IsValid() && FocusButton->HasKeyboardFocus();
		FocusState.bSelected = bAutomationFocusRequested;
		UpdateNodeGeometry(TEXT("AutomationProof.FocusTarget"), FocusButton, FocusState);
		FHansaSemanticState WarningState;
		WarningState.bWarning = true;
		UpdateNodeGeometry(TEXT("AutomationProof.Warning"), WarningWidget, WarningState);
	}

	FHansaAutomationProofScreenHost::FHansaAutomationProofScreenHost(FHansaSemanticUiRegistry& InRegistry)
		: Registry(InRegistry)
	{
	}

	FHansaAutomationProofScreenHost::~FHansaAutomationProofScreenHost()
	{
		if (Window.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
		}
		Registry.Reset();
	}

	bool FHansaAutomationProofScreenHost::EnsureScreen(const FIntPoint& ClientSize)
	{
		if (!FSlateApplication::IsInitialized() || ClientSize.X <= 0 || ClientSize.Y <= 0)
		{
			return false;
		}
		if (!Window.IsValid())
		{
			SAssignNew(Window, SWindow)
				.Title(FText::FromString(TEXT("Hansa Automation Proof")))
				.ClientSize(FVector2D(ClientSize.X, ClientSize.Y))
				.SizingRule(ESizingRule::FixedSize)
				.SupportsMaximize(false)
				.SupportsMinimize(false);
			SAssignNew(Screen, SHansaAutomationProofScreen, Registry);
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

	bool FHansaAutomationProofScreenHost::CaptureNative(const FIntPoint& Size, TArray<FColor>& OutPixels)
	{
		if (!EnsureScreen(Size) || !Screen.IsValid())
		{
			return false;
		}
		FIntVector CapturedSize;
		const bool bCaptured = FSlateApplication::Get().TakeScreenshot(
			Screen->GetCaptureWidget(),
			FIntRect(0, 0, Size.X, Size.Y),
			OutPixels,
			CapturedSize);
		return bCaptured && CapturedSize.X == Size.X && CapturedSize.Y == Size.Y &&
			OutPixels.Num() == Size.X * Size.Y;
	}

	void FHansaAutomationProofScreenHost::SynchronizeSemantics()
	{
		if (Screen.IsValid())
		{
			Screen->SynchronizeSemantics();
		}
	}
}
