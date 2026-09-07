#include "UI/SHansaSaveLoadScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaSaveLoadScreen"

namespace Hansa::UI
{
	SHansaSaveLoadScreen::~SHansaSaveLoadScreen()
	{
		if (UHansaSaveLoadPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaSaveLoadScreen::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;
		ScrimBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		PanelBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		InnerBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		HeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, false);
		BodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		DataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, false);
		CaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);
		ChildSlot
		[
			SAssignNew(RootWidget, SBorder).BorderImage(&ScrimBrush).Padding(24.0f)
			[
				SNew(SBox).WidthOverride(1120.0f).MaxDesiredHeight(720.0f)
				[
					SNew(SBorder).BorderImage(&PanelBrush).Padding(24.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(LOCTEXT("Title", "Save and load")).TextStyle(&HeadingStyle)]
								+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Close", "Close")).OnClicked(this, &SHansaSaveLoadScreen::HandleClose)]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[SNew(SBorder).BorderImage(&InnerBrush).Padding(12.0f)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(SlotRows, SVerticalBox)]]]
								+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(12.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SBorder).BorderImage(&InnerBrush).Padding(20.0f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SAssignNew(DetailTitle, STextBlock).TextStyle(&HeadingStyle)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)[SAssignNew(DetailTimestamp, STextBlock).TextStyle(&DataStyle)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(DetailScenario, STextBlock).TextStyle(&BodyStyle)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(DetailVersion, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(DetailHashes, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
										+ SVerticalBox::Slot().FillHeight(1.0f)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(SaveButton, SButton).ButtonStyle(&PrimaryButtonStyle).Text(LOCTEXT("Save", "Save here")).OnClicked(this, &SHansaSaveLoadScreen::HandleSave)]
											+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 0.0f, 0.0f, 0.0f)[SAssignNew(LoadButton, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Load", "Load")).OnClicked(this, &SHansaSaveLoadScreen::HandleLoad)]
										]
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(StatusWidget, SBorder).BorderImage(&InnerBrush).Padding(10.0f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(StatusText, STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(RemedyText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
								]
							]
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SAssignNew(ConfirmationWidget, SBorder).BorderImage(&CriticalBrush).Padding(24.0f)
							[
								SNew(SBox).WidthOverride(480.0f)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ConfirmationText, STextBlock).TextStyle(&HeadingStyle).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 20.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(CancelButton, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Cancel", "Cancel")).OnClicked(this, &SHansaSaveLoadScreen::HandleCancel)]
										+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 0.0f, 0.0f)[SAssignNew(ConfirmButton, SButton).ButtonStyle(&PrimaryButtonStyle).Text(LOCTEXT("Confirm", "Confirm")).OnClicked(this, &SHansaSaveLoadScreen::HandleConfirm)]
									]
								]
							]
						]
					]
				]
			]
		];
		SemanticWidgets.Add(TEXT("SaveLoad.Root"), RootWidget); SemanticWidgets.Add(TEXT("SaveLoad.Close"), CloseButton);
		SemanticWidgets.Add(TEXT("SaveLoad.Action.Save"), SaveButton); SemanticWidgets.Add(TEXT("SaveLoad.Action.Load"), LoadButton);
		SemanticWidgets.Add(TEXT("SaveLoad.Confirmation"), ConfirmationWidget); SemanticWidgets.Add(TEXT("SaveLoad.Confirmation.Confirm"), ConfirmButton);
		SemanticWidgets.Add(TEXT("SaveLoad.Confirmation.Cancel"), CancelButton); SemanticWidgets.Add(TEXT("SaveLoad.Status"), StatusWidget);
		if (UHansaSaveLoadPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaSaveLoadScreen::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
	}

	void SHansaSaveLoadScreen::Refresh(const FHansaSaveLoadPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		PresentedRevision = Revision; SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed); RebuildSlots(Snapshot);
		const FHansaSaveSlotMetadata* Slot = Snapshot.Slots.FindByPredicate([&](const auto& Candidate){return Candidate.SlotId==Snapshot.SelectedSlot;});
		if (Slot)
		{
			DetailTitle->SetText(Slot->DisplayName.IsEmpty() ? Slot->SlotLabel : FText::FromString(Slot->DisplayName));
			DetailTimestamp->SetText(Slot->SavedUtc.IsEmpty() ? LOCTEXT("NoTimestamp", "No save in this slot") : FText::FromString(Slot->SavedUtc));
			DetailScenario->SetText(FText::Format(LOCTEXT("ScenarioFmt", "Scenario: {0} · {1}"), FText::FromString(Slot->ScenarioId), Slot->CompatibilityLabel));
			DetailVersion->SetText(FText::Format(LOCTEXT("VersionFmt", "Build {0} · format {1} · tick {2}"), FText::FromString(Slot->BuildVersion), FText::AsNumber(Slot->FormatVersion), FText::AsNumber(Slot->SimulationTick)));
			DetailHashes->SetText(FText::Format(LOCTEXT("HashFmt", "State {0} · campaign {1}"), FText::FromString(Slot->AuthoritativeHash), FText::FromString(Slot->CampaignHash)));
			SaveButton->SetEnabled(true);
			LoadButton->SetEnabled(Slot->bCanLoad);
		}
		else
		{
			DetailTitle->SetText(LOCTEXT("NoSlotsTitle", "No save slots available"));
			DetailTimestamp->SetText(LOCTEXT("NoSlotsStatus", "Save storage is unavailable in this session."));
			DetailScenario->SetText(LOCTEXT("NoSlotsRemedy", "Return to a playable campaign before saving or loading."));
			DetailVersion->SetText(FText::GetEmpty());
			DetailHashes->SetText(FText::GetEmpty());
			SaveButton->SetEnabled(false);
			LoadButton->SetEnabled(false);
		}
		const bool bConfirm = Snapshot.Confirmation != EHansaSaveLoadConfirmation::None;
		ConfirmationWidget->SetVisibility(bConfirm ? EVisibility::Visible : EVisibility::Collapsed);
		ConfirmationText->SetText(Snapshot.Confirmation == EHansaSaveLoadConfirmation::Overwrite ? LOCTEXT("OverwritePrompt", "Overwrite this save? The previous state in this slot will be replaced.") : LOCTEXT("LoadPrompt", "Load this save? Unsaved progress in the current session will be lost."));
		const bool bStatus = Snapshot.Status != EHansaSaveLoadStatus::None;
		StatusWidget->SetVisibility(bStatus ? EVisibility::Visible : EVisibility::Collapsed); StatusText->SetText(Snapshot.StatusMessage); RemedyText->SetText(Snapshot.StatusRemedy);
	}

	void SHansaSaveLoadScreen::RebuildSlots(const FHansaSaveLoadPresentationSnapshot& Snapshot)
	{
		SlotRows->ClearChildren();
		if (Snapshot.Slots.IsEmpty())
		{
			SlotRows->AddSlot().AutoHeight()
			[
				SNew(SBorder).BorderImage(&InnerBrush).Padding(16.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("NoSlotsList", "No save slots are available.")).TextStyle(&BodyStyle).AutoWrapText(true)
				]
			];
			return;
		}
		for (const FHansaSaveSlotMetadata& Slot : Snapshot.Slots)
		{
			const FString SemanticId = TEXT("SaveLoad.Slot.") + Slot.StableId.ToString(); TSharedPtr<SButton> Button;
			SlotRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SAssignNew(Button, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaSaveLoadScreen::HandleSelectSlot, Slot.SlotId)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(Slot.SlotLabel).TextStyle(&BodyStyle)]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(Slot.CompatibilityLabel).TextStyle(&DataStyle)]]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Slot.SavedUtc.IsEmpty()?LOCTEXT("EmptySlot","Empty slot"):FText::FromString(Slot.SavedUtc)).TextStyle(&CaptionStyle)]
				]
			];
			SemanticWidgets.Add(SemanticId, Button);
		}
	}

	FReply SHansaSaveLoadScreen::HandleClose(){if(auto* P=Model.Get())P->Close();return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleSelectSlot(EHansaSaveSlotId S){if(auto* P=Model.Get())P->SelectSlot(S);return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleSave(){if(auto* P=Model.Get())P->RequestSave();return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleLoad(){if(auto* P=Model.Get())P->RequestLoad();return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleConfirm(){if(auto* P=Model.Get())P->Confirm();return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleCancel(){if(auto* P=Model.Get())P->CancelConfirmation();return FReply::Handled();}

	bool SHansaSaveLoadScreen::ActivateSemanticId(const FString& Id)
	{
		if(Id==TEXT("SaveLoad.Close"))return HandleClose().IsEventHandled();
		if(Id==TEXT("SaveLoad.Action.Save"))return HandleSave().IsEventHandled();
		if(Id==TEXT("SaveLoad.Action.Load"))return HandleLoad().IsEventHandled();
		if(Id==TEXT("SaveLoad.Confirmation.Confirm"))return HandleConfirm().IsEventHandled();
		if(Id==TEXT("SaveLoad.Confirmation.Cancel"))return HandleCancel().IsEventHandled();
		if(Id.StartsWith(TEXT("SaveLoad.Slot."))){if(auto* P=Model.Get())for(const auto& S:P->GetSnapshot().Slots)if(Id==TEXT("SaveLoad.Slot.")+S.StableId.ToString())return HandleSelectSlot(S.SlotId).IsEventHandled();}
		return false;
	}

	bool SHansaSaveLoadScreen::FocusSemanticId(const FString& Id)
	{
		const auto* Found=SemanticWidgets.Find(Id);auto Widget=Found?Found->Pin():nullptr;if(!Widget.IsValid()||!Widget->IsEnabled()||!Widget->GetVisibility().IsVisible())return false;
		if(auto* P=Model.Get())P->SetFocusedSemanticId(FName(*Id));if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);return true;
	}

	FReply SHansaSaveLoadScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		const EHansaUiNavigationIntent Intent = ClassifyNavigationIntent(InKeyEvent);
		if (Intent == EHansaUiNavigationIntent::Back)
		{
			if (const UHansaSaveLoadPresentationModel* Pinned = Model.Get(); Pinned != nullptr && Pinned->GetSnapshot().Confirmation != EHansaSaveLoadConfirmation::None)
			{
				return HandleCancel();
			}
			return HandleClose();
		}
		UHansaSaveLoadPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return FReply::Unhandled();
		if (Intent == EHansaUiNavigationIntent::Activate)
		{
			return ActivateSemanticId(Pinned->GetSnapshot().FocusedSemanticId.ToString()) ? FReply::Handled() : FReply::Unhandled();
		}
		if (Intent == EHansaUiNavigationIntent::Next || Intent == EHansaUiNavigationIntent::Previous)
		{
			const FString Target = FindWrappedFocusTarget(GetControllerFocusOrder(), Pinned->GetSnapshot().FocusedSemanticId.ToString(), Intent == EHansaUiNavigationIntent::Next);
			return !Target.IsEmpty() && FocusSemanticId(Target) ? FReply::Handled() : FReply::Unhandled();
		}
		return FReply::Unhandled();
	}

	TArray<FString> SHansaSaveLoadScreen::GetControllerFocusOrder() const
	{
		TArray<FString> R={TEXT("SaveLoad.Close")};const auto* P=Model.Get();if(!P)return R;
		for(const auto& S:P->GetSnapshot().Slots)R.Add(TEXT("SaveLoad.Slot.")+S.StableId.ToString());
		if(P->GetSnapshot().Confirmation!=EHansaSaveLoadConfirmation::None)R.Append({TEXT("SaveLoad.Confirmation.Cancel"),TEXT("SaveLoad.Confirmation.Confirm")});
		else R.Append({TEXT("SaveLoad.Action.Save"),TEXT("SaveLoad.Action.Load")});return R;
	}

	TArray<FHansaHudSemanticNode> SHansaSaveLoadScreen::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> R;const auto* P=Model.Get();if(!P)return R;const auto& S=P->GetSnapshot();
		auto Add=[&](FString Id,FString Parent,FString Label,EHansaHudSemanticRole Role,bool Activate,bool Focus,FString Type,FString Value,bool Selected=false,bool Warning=false,bool Error=false,bool Visible=true,bool Enabled=true){FHansaHudSemanticNode N;N.Id=MoveTemp(Id);N.ParentId=MoveTemp(Parent);N.Label=MoveTemp(Label);N.Role=Role;N.bCanActivate=Activate;N.bCanFocus=Focus;N.State.bVisible=S.bOpen&&Visible;N.State.bEnabled=Enabled;N.State.bFocused=S.FocusedSemanticId==FName(*N.Id);N.State.bSelected=Selected;N.State.bWarning=Warning;N.State.bError=Error;N.State.ValueType=MoveTemp(Type);N.State.Value=MoveTemp(Value);R.Add(MoveTemp(N));};
		Add(TEXT("SaveLoad.Root"),TEXT("HUD.Root"),TEXT("Save and load"),EHansaHudSemanticRole::Screen,false,false,TEXT("open"),S.bOpen?TEXT("true"):TEXT("false"));
		Add(TEXT("SaveLoad.Close"),TEXT("SaveLoad.Root"),TEXT("Close"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("close"));
		for(const auto& Slot:S.Slots){const FString Id=TEXT("SaveLoad.Slot.")+Slot.StableId.ToString();const FString Value=FString::Printf(TEXT("exists=%s;compatibility=%s;savedUtc=%s;scenario=%s;format=%u;build=%s;tick=%lld;hash=%s;remedy=%s"),Slot.bExists?TEXT("true"):TEXT("false"),*Slot.CompatibilityLabel.ToString(),*Slot.SavedUtc,*Slot.ScenarioId,Slot.FormatVersion,*Slot.BuildVersion,static_cast<long long>(Slot.SimulationTick),*Slot.AuthoritativeHash,*Slot.Remedy.ToString());Add(Id,TEXT("SaveLoad.Root"),Slot.SlotLabel.ToString(),EHansaHudSemanticRole::Button,true,true,TEXT("save-slot"),Value,Slot.SlotId==S.SelectedSlot,Slot.Compatibility==EHansaSaveSlotCompatibility::Incompatible,Slot.Compatibility==EHansaSaveSlotCompatibility::Corrupt,true,true);}
		Add(TEXT("SaveLoad.Action.Save"),TEXT("SaveLoad.Root"),TEXT("Save here"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("save"),false,false,false,true,!S.Slots.IsEmpty());
		const auto* Slot=S.Slots.FindByPredicate([&](const auto& X){return X.SlotId==S.SelectedSlot;});Add(TEXT("SaveLoad.Action.Load"),TEXT("SaveLoad.Root"),TEXT("Load"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("load"),false,false,false,true,Slot&&Slot->bCanLoad);
		const bool C=S.Confirmation!=EHansaSaveLoadConfirmation::None;Add(TEXT("SaveLoad.Confirmation"),TEXT("SaveLoad.Root"),TEXT("Confirmation"),EHansaHudSemanticRole::Panel,false,false,TEXT("kind"),S.Confirmation==EHansaSaveLoadConfirmation::Overwrite?TEXT("overwrite"):TEXT("load"),false,false,false,C);
		Add(TEXT("SaveLoad.Confirmation.Confirm"),TEXT("SaveLoad.Confirmation"),TEXT("Confirm"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("confirm"),false,false,false,C);
		Add(TEXT("SaveLoad.Confirmation.Cancel"),TEXT("SaveLoad.Confirmation"),TEXT("Cancel"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("cancel"),false,false,false,C);
		Add(TEXT("SaveLoad.Status"),TEXT("SaveLoad.Root"),S.StatusMessage.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("operation-status"),S.StatusRemedy.ToString(),S.Status==EHansaSaveLoadStatus::Success,S.Status==EHansaSaveLoadStatus::Warning,S.Status==EHansaSaveLoadStatus::Error,S.Status!=EHansaSaveLoadStatus::None);
		return R;
	}
}

#undef LOCTEXT_NAMESPACE
