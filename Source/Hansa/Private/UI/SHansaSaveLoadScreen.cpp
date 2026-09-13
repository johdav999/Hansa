#include "UI/SHansaSaveLoadScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Styling/CoreStyle.h"
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

	void SHansaSaveLoadScreen::SetPresentationSize(FIntPoint Size){if(PanelSize){PanelSize->SetWidthOverride(FMath::Min(1120.f,float(Size.X)));PanelSize->SetHeightOverride(FMath::Min(680.f,float(Size.Y)));}}
	void SHansaSaveLoadScreen::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model; Preferences = Arguments._Preferences;
        HeaderBrush=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		ScrimBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		PanelBrush = GetComponentStyle(EUiSurface::Modal,EUiState::Default,Preferences).Brush;
		InnerBrush = GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
        SaveNameStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
        SaveNameStyle.SetBackgroundImageNormal(InnerBrush).SetBackgroundImageHovered(InnerBrush).SetBackgroundImageFocused(GetComponentStyle(EUiSurface::Panel,EUiState::Selected,Preferences).Brush).SetBackgroundImageReadOnly(InnerBrush).SetForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).SetFocusedForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).SetReadOnlyForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink));

		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		HeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, false); HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
		BodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false); BodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
		DataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, false); DataStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
		CaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false); CaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));
		ChildSlot
		[
			SAssignNew(RootWidget, SBorder).BorderImage(&ScrimBrush).Padding(0.0f)
			[
				SAssignNew(PanelSize,SBox).WidthOverride(1120.0f).MaxDesiredHeight(720.0f)
				[
					SNew(SBorder).BorderImage(&PanelBrush).Padding(16.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(MainPanel,SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBorder).BorderImage(&HeaderBrush).Padding(8)[SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(LOCTEXT("Title", "Save and load")).TextStyle(&HeadingStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk))]
								+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Icon).Label(LOCTEXT("Close", "Close")).OnClicked(this, &SHansaSaveLoadScreen::HandleClose)]]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
								[SNew(SBorder).BorderImage(&InnerBrush).Padding(12.0f)[SAssignNew(SlotScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(SlotRows, SVerticalBox)]]]
								+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(12.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SBorder).BorderImage(&InnerBrush).Padding(12.0f)
									[
										SAssignNew(DetailScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()[SAssignNew(DetailTitle, STextBlock).TextStyle(&HeadingStyle).AutoWrapText(true)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)[SAssignNew(DetailTimestamp, STextBlock).TextStyle(&DataStyle).AutoWrapText(true)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(DetailScenario, STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(DetailVersion, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(DetailHashes, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true).Visibility(EVisibility::Collapsed)]
										+ SVerticalBox::Slot().FillHeight(1.0f)

									]]
								]
							]
										 + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                            [SNew(SBox).MinDesiredHeight(48.f/FMath::Min(1.f,Preferences.UiScale))[SAssignNew(SaveName,SEditableTextBox).Style(&SaveNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)).HintText(LOCTEXT("SaveName","Name your manual save"))
                             .OnTextChanged_Lambda([this](const FText& Text){if(auto* P=Model.Get())P->SetSaveName(Text.ToString());})]]
                            + SVerticalBox::Slot().AutoHeight()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(SaveButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).Label(LOCTEXT("Save", "Save here")).OnClicked(this, &SHansaSaveLoadScreen::HandleSave)]
											+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 0.0f, 0.0f, 0.0f)[SAssignNew(LoadButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("Load", "Load")).OnClicked(this, &SHansaSaveLoadScreen::HandleLoad)]
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
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ConfirmationText, STextBlock).TextStyle(&HeadingStyle).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk)).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 20.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(CancelButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("Cancel", "Cancel")).OnClicked(this, &SHansaSaveLoadScreen::HandleCancel)]
										+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 0.0f, 0.0f)[SAssignNew(ConfirmButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).Label(LOCTEXT("Confirm", "Confirm")).OnClicked(this, &SHansaSaveLoadScreen::HandleConfirm)]
									]
								]
							]
						]
					]
				]
			]
		];
		SemanticWidgets.Add(TEXT("SaveLoad.Name"),SaveName);SemanticWidgets.Add(TEXT("SaveLoad.Content"),DetailScroll);SemanticWidgets.Add(TEXT("SaveLoad.Root"), RootWidget); SemanticWidgets.Add(TEXT("SaveLoad.Close"), CloseButton);
		SemanticWidgets.Add(TEXT("SaveLoad.Action.Save"), SaveButton); SemanticWidgets.Add(TEXT("SaveLoad.Action.Load"), LoadButton);
		SemanticWidgets.Add(TEXT("SaveLoad.Confirmation"), ConfirmationWidget); SemanticWidgets.Add(TEXT("SaveLoad.Confirmation.Confirm"), ConfirmButton);
		SemanticWidgets.Add(TEXT("SaveLoad.Confirmation.Cancel"), CancelButton); SemanticWidgets.Add(TEXT("SaveLoad.Status"), StatusWidget);
        if(PreferencesWidget)for(const auto& Pair:PreferencesWidget->GetControls()){
            SemanticWidgets.Add(Pair.Key,Pair.Value);
            Pair.Value->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Id=FName(*Pair.Key)]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(Id);}));
        }
		if (UHansaSaveLoadPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaSaveLoadScreen::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
	}

	void SHansaSaveLoadScreen::Refresh(const FHansaSaveLoadPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		if(!SaveName->HasKeyboardFocus()&&SaveName->GetText().ToString()!=Snapshot.SaveName)SaveName->SetText(FText::FromString(Snapshot.SaveName));
        PresentedRevision = Revision; SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed); RebuildSlots(Snapshot);
		const FHansaSaveSlotMetadata* Slot = Snapshot.Slots.FindByPredicate([&](const auto& Candidate){return Candidate.SlotId==Snapshot.SelectedSlot;});
		if (Slot)
		{
			DetailTitle->SetText(Slot->DisplayName.IsEmpty() ? Slot->SlotLabel : FText::FromString(Slot->DisplayName));
			FDateTime SavedDate;DetailTimestamp->SetText(Slot->SavedUtc.IsEmpty()?(Slot->bExists?LOCTEXT("UnreadableTimestamp","Save date unavailable"):LOCTEXT("NoTimestamp","No save in this slot")):FDateTime::ParseIso8601(*Slot->SavedUtc,SavedDate)?FText::AsDateTime(SavedDate):FText::FromString(Slot->SavedUtc));
			DetailScenario->SetVisibility(Slot->bExists?EVisibility::Visible:EVisibility::Collapsed);DetailVersion->SetVisibility(Slot->bExists?EVisibility::Visible:EVisibility::Collapsed);
			DetailScenario->SetText(Slot->Error.IsEmpty()?Slot->CompatibilityLabel:FText::Format(LOCTEXT("SlotError","{0}\n{1}"),Slot->Error,Slot->Remedy));
			DetailVersion->SetText(Slot->SlotId==EHansaSaveSlotId::Autosave?LOCTEXT("AutosavePolicy","Rotates every five minutes while playing."):LOCTEXT("ManualPolicy","Manual saves are replaced only after confirmation."));
			DetailHashes->SetText(FText::Format(LOCTEXT("HashFmt", "State {0} · campaign {1}"), FText::FromString(Slot->AuthoritativeHash), FText::FromString(Slot->CampaignHash)));
			StaticCastSharedPtr<SHansaAction>(SaveButton)->SetState(Snapshot.bSavingAllowed?EUiState::Default:EUiState::Disabled);
			StaticCastSharedPtr<SHansaAction>(LoadButton)->SetState(Slot->bCanLoad?EUiState::Default:EUiState::Disabled);
		}
		else
		{
			DetailTitle->SetText(LOCTEXT("NoSlotsTitle", "No save slots available"));
			DetailTimestamp->SetText(LOCTEXT("NoSlotsStatus", "Save storage is unavailable in this session."));
			DetailScenario->SetText(LOCTEXT("NoSlotsRemedy", "Return to a playable campaign before saving or loading."));
			DetailVersion->SetText(FText::GetEmpty());
			DetailHashes->SetText(FText::GetEmpty());
			StaticCastSharedPtr<SHansaAction>(SaveButton)->SetState(EUiState::Disabled);
			StaticCastSharedPtr<SHansaAction>(LoadButton)->SetState(EUiState::Disabled);
		}
		SaveName->SetVisibility(Snapshot.bSavingAllowed?EVisibility::Visible:EVisibility::Collapsed);
        SaveName->SetEnabled(Snapshot.bSavingAllowed&&Snapshot.SelectedSlot==EHansaSaveSlotId::Manual);
        const bool bConfirm = Snapshot.Confirmation != EHansaSaveLoadConfirmation::None;
		MainPanel->SetEnabled(!bConfirm);
		ConfirmationWidget->SetVisibility(bConfirm ? EVisibility::Visible : EVisibility::Collapsed);
		ConfirmationText->SetText(Snapshot.Confirmation == EHansaSaveLoadConfirmation::Overwrite ? LOCTEXT("OverwritePrompt", "Overwrite this save? The previous state in this slot will be replaced.") : LOCTEXT("LoadPrompt", "Load this save? Unsaved progress in the current session will be lost."));
		const bool bStatus = Snapshot.Status != EHansaSaveLoadStatus::None;
		StatusWidget->SetVisibility(bStatus ? EVisibility::Visible : EVisibility::Collapsed); StatusText->SetText(Snapshot.StatusMessage); RemedyText->SetText(Snapshot.StatusRemedy);
	}

	void SHansaSaveLoadScreen::RebuildSlots(const FHansaSaveLoadPresentationSnapshot& Snapshot)
	{
		FString ContentKey=FString::FromInt(int32(Snapshot.SelectedSlot));
		for(const auto& S:Snapshot.Slots)ContentKey+=S.StableId.ToString()+S.SlotLabel.ToString()+S.SavedUtc+S.CompatibilityLabel.ToString();
		if(ContentKey==PresentedContentKey)return;
		PresentedContentKey=MoveTemp(ContentKey);
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
				SAssignNew(Button,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).State(Slot.SlotId==Snapshot.SelectedSlot?EUiState::Selected:EUiState::Default).OnClicked(this, &SHansaSaveLoadScreen::HandleSelectSlot, Slot.SlotId)
				[
					SNew(SVerticalBox)
					 + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Slot.DisplayName.IsEmpty()?Slot.SlotLabel:FText::FromString(Slot.DisplayName)).TextStyle(&BodyStyle).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Slot.CompatibilityLabel).TextStyle(&BodyStyle).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Slot.SavedUtc.IsEmpty()?(Slot.bExists?LOCTEXT("UnreadableSlotDate","Save date unavailable"):LOCTEXT("EmptySlot","Empty slot")):FText::FromString(Slot.SavedUtc)).TextStyle(&CaptionStyle).AutoWrapText(true)]
				]
			];
			SemanticWidgets.Add(SemanticId, Button);
			StaticCastSharedPtr<SHansaAction>(Button)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,SemanticId]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*SemanticId));}));
		}
	}

	FReply SHansaSaveLoadScreen::HandleClose(){if(auto* P=Model.Get())P->Close();return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleSelectSlot(EHansaSaveSlotId S){if(auto* P=Model.Get())P->SelectSlot(S);FocusSemanticId(S==EHansaSaveSlotId::Manual?TEXT("SaveLoad.Slot.manual"):TEXT("SaveLoad.Slot.autosave"));return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleSave(){if(auto* P=Model.Get()){P->RequestSave();FocusSemanticId(P->GetSnapshot().Confirmation==EHansaSaveLoadConfirmation::None?TEXT("SaveLoad.Action.Save"):TEXT("SaveLoad.Confirmation.Cancel"));}return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleLoad(){if(auto* P=Model.Get()){P->RequestLoad();FocusSemanticId(P->GetSnapshot().Confirmation==EHansaSaveLoadConfirmation::None?TEXT("SaveLoad.Action.Load"):TEXT("SaveLoad.Confirmation.Cancel"));}return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleConfirm(){if(auto* P=Model.Get())P->Confirm();FocusSemanticId(TEXT("SaveLoad.Close"));return FReply::Handled();}
	FReply SHansaSaveLoadScreen::HandleCancel(){if(auto* P=Model.Get())P->CancelConfirmation();FocusSemanticId(TEXT("SaveLoad.Close"));return FReply::Handled();}

	bool SHansaSaveLoadScreen::ActivateSemanticId(const FString& Id)
	{
        if(Model.IsValid()&&Model->GetSnapshot().Confirmation!=EHansaSaveLoadConfirmation::None && !Id.StartsWith(TEXT("SaveLoad.Confirmation.")))return false;
		if(Id.StartsWith(TEXT("SaveLoad.Preferences.")))return PreferencesWidget.IsValid()&&PreferencesWidget->ActivateControl(Id);
		if(Id==TEXT("SaveLoad.Close"))return HandleClose().IsEventHandled();
		if(Id==TEXT("SaveLoad.Action.Save")&&!SaveButton->IsEnabled())return false;
        if(Id==TEXT("SaveLoad.Action.Load")&&!LoadButton->IsEnabled())return false;
        if(Id==TEXT("SaveLoad.Action.Save"))return HandleSave().IsEventHandled();
		if(Id==TEXT("SaveLoad.Action.Load"))return HandleLoad().IsEventHandled();
		if(Id==TEXT("SaveLoad.Confirmation.Confirm"))return HandleConfirm().IsEventHandled();
		if(Id==TEXT("SaveLoad.Confirmation.Cancel"))return HandleCancel().IsEventHandled();
		if(Id.StartsWith(TEXT("SaveLoad.Slot."))){if(auto* P=Model.Get())for(const auto& S:P->GetSnapshot().Slots)if(Id==TEXT("SaveLoad.Slot.")+S.StableId.ToString())return HandleSelectSlot(S.SlotId).IsEventHandled();}
		return false;
	}

	bool SHansaSaveLoadScreen::FocusSemanticId(const FString& Id)
	{
        if(Model.IsValid()&&Model->GetSnapshot().Confirmation!=EHansaSaveLoadConfirmation::None && !Id.StartsWith(TEXT("SaveLoad.Confirmation.")))return false;
		const auto* Found=SemanticWidgets.Find(Id);auto Widget=Found?Found->Pin():nullptr;if(!Widget.IsValid()||!Widget->IsEnabled()||!Widget->GetVisibility().IsVisible())return false;
		if(auto* P=Model.Get())P->SetFocusedSemanticId(FName(*Id));Found=SemanticWidgets.Find(Id);Widget=Found?Found->Pin():nullptr;if(!Widget.IsValid())return false;if(SlotScroll && Id.StartsWith(TEXT("SaveLoad.Slot.")))SlotScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);return true;
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
        const auto* P=Model.Get();if(!P)return {};
        if(P->GetSnapshot().Confirmation!=EHansaSaveLoadConfirmation::None)return {TEXT("SaveLoad.Confirmation.Cancel"),TEXT("SaveLoad.Confirmation.Confirm")};
        TArray<FString> R={TEXT("SaveLoad.Close")};
        for(const auto& Slot:P->GetSnapshot().Slots)R.Add(TEXT("SaveLoad.Slot.")+Slot.StableId.ToString());
        if(SaveName->IsEnabled())R.Add(TEXT("SaveLoad.Name"));
        if(SaveButton->IsEnabled())R.Add(TEXT("SaveLoad.Action.Save"));
        if(LoadButton->IsEnabled())R.Add(TEXT("SaveLoad.Action.Load"));
        if(PreferencesWidget)for(const FString& Id:{TEXT("SaveLoad.Preferences.Contrast"),TEXT("SaveLoad.Preferences.Text"),TEXT("SaveLoad.Preferences.Motion"),TEXT("SaveLoad.Preferences.ScaleDown"),TEXT("SaveLoad.Preferences.ScaleUp"),TEXT("SaveLoad.Preferences.Reset")}){const auto* B=PreferencesWidget->GetControls().Find(Id);if(B&&(*B)->IsEnabled())R.Add(Id);}
        return R;
    }

	TArray<FHansaHudSemanticNode> SHansaSaveLoadScreen::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> R;const auto* P=Model.Get();if(!P)return R;const auto& S=P->GetSnapshot();
		auto Add=[&](FString Id,FString Parent,FString Label,EHansaHudSemanticRole Role,bool Activate,bool Focus,FString Type,FString Value,bool Selected=false,bool Warning=false,bool Error=false,bool Visible=true,bool Enabled=true){FHansaHudSemanticNode N;N.Id=MoveTemp(Id);N.ParentId=MoveTemp(Parent);N.Label=MoveTemp(Label);N.Role=Role;N.bCanActivate=Activate;N.bCanFocus=Focus;N.State.bVisible=S.bOpen&&Visible;N.State.bEnabled=Enabled;N.State.bFocused=S.FocusedSemanticId==FName(*N.Id);N.State.bSelected=Selected;N.State.bWarning=Warning;N.State.bError=Error;N.State.ValueType=MoveTemp(Type);N.State.Value=MoveTemp(Value);R.Add(MoveTemp(N));};
		Add(TEXT("SaveLoad.Root"),TEXT("HUD.Root"),TEXT("Save and load"),EHansaHudSemanticRole::Screen,false,false,TEXT("open"),S.bOpen?TEXT("true"):TEXT("false"));
		Add(TEXT("SaveLoad.Close"),TEXT("SaveLoad.Root"),TEXT("Close"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("close"));
		for(const auto& Slot:S.Slots){const FString Id=TEXT("SaveLoad.Slot.")+Slot.StableId.ToString();const FString Value=FString::Printf(TEXT("exists=%s;compatibility=%s;savedUtc=%s;scenario=%s;format=%u;build=%s;tick=%lld;hash=%s;remedy=%s"),Slot.bExists?TEXT("true"):TEXT("false"),*Slot.CompatibilityLabel.ToString(),*Slot.SavedUtc,*Slot.ScenarioId,Slot.FormatVersion,*Slot.BuildVersion,static_cast<long long>(Slot.SimulationTick),*Slot.AuthoritativeHash,*Slot.Remedy.ToString());Add(Id,TEXT("SaveLoad.Root"),Slot.SlotLabel.ToString(),EHansaHudSemanticRole::Button,true,true,TEXT("save-slot"),Value,Slot.SlotId==S.SelectedSlot,Slot.Compatibility==EHansaSaveSlotCompatibility::Incompatible,Slot.Compatibility==EHansaSaveSlotCompatibility::Corrupt,true,true);}
		Add(TEXT("SaveLoad.Name"),TEXT("SaveLoad.Root"),TEXT("Manual save name"),EHansaHudSemanticRole::Button,false,true,TEXT("text"),S.SaveName,false,false,false,true,S.bSavingAllowed);
        Add(TEXT("SaveLoad.Action.Save"),TEXT("SaveLoad.Root"),TEXT("Save here"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("save"),false,false,false,true,!S.Slots.IsEmpty()&&S.bSavingAllowed);
		const auto* Slot=S.Slots.FindByPredicate([&](const auto& X){return X.SlotId==S.SelectedSlot;});Add(TEXT("SaveLoad.Action.Load"),TEXT("SaveLoad.Root"),TEXT("Load"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("load"),false,false,false,true,Slot&&Slot->bCanLoad);
		const bool C=S.Confirmation!=EHansaSaveLoadConfirmation::None;Add(TEXT("SaveLoad.Confirmation"),TEXT("SaveLoad.Root"),TEXT("Confirmation"),EHansaHudSemanticRole::Panel,false,false,TEXT("kind"),S.Confirmation==EHansaSaveLoadConfirmation::Overwrite?TEXT("overwrite"):TEXT("load"),false,false,false,C);
		Add(TEXT("SaveLoad.Confirmation.Confirm"),TEXT("SaveLoad.Confirmation"),TEXT("Confirm"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("confirm"),false,false,false,C);
		Add(TEXT("SaveLoad.Confirmation.Cancel"),TEXT("SaveLoad.Confirmation"),TEXT("Cancel"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("cancel"),false,false,false,C);
		Add(TEXT("SaveLoad.Status"),TEXT("SaveLoad.Root"),S.StatusMessage.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("operation-status"),S.StatusRemedy.ToString(),S.Status==EHansaSaveLoadStatus::Success,S.Status==EHansaSaveLoadStatus::Warning,S.Status==EHansaSaveLoadStatus::Error,S.Status!=EHansaSaveLoadStatus::None);
        if(PreferencesWidget)if(PreferencesWidget)for(const auto& Pair:PreferencesWidget->GetControls())Add(Pair.Key,TEXT("SaveLoad.Root"),Pair.Key.RightChop(21),EHansaHudSemanticRole::Button,true,true,TEXT("ui-preference"),FString::Printf(TEXT("scale=%.1f;contrast=%d;largeText=%d;reducedMotion=%d"),Preferences.UiScale,Preferences.bHighContrast,Preferences.bLargeText,Preferences.bReducedMotion),false,false,false,!C,Pair.Value->IsEnabled());
        for(auto& N:R){
            if(auto W=ResolveSemanticWidget(N.Id)){const auto G=W->GetCachedGeometry();auto A=G.GetAbsolutePosition(),B=A+G.GetAbsoluteSize();if(N.Id.StartsWith(TEXT("SaveLoad.Slot."))){const auto Clip=SlotScroll->GetCachedGeometry();const auto Min=Clip.GetAbsolutePosition(),Max=Min+Clip.GetAbsoluteSize();A.X=FMath::Max(A.X,Min.X);A.Y=FMath::Max(A.Y,Min.Y);B.X=FMath::Min(B.X,Max.X);B.Y=FMath::Min(B.Y,Max.Y);}if(N.State.bVisible&&B.X>A.X&&B.Y>A.Y)N.Bounds=FIntRect(FMath::RoundToInt(A.X),FMath::RoundToInt(A.Y),FMath::RoundToInt(B.X),FMath::RoundToInt(B.Y));}

            if(C && !N.Id.StartsWith(TEXT("SaveLoad.Confirmation"))){N.State.bEnabled=false;N.bCanActivate=false;N.bCanFocus=false;}
            if(!N.State.bEnabled){N.bCanActivate=false;N.bCanFocus=false;}
        }
		return R;
	}
}

#undef LOCTEXT_NAMESPACE
