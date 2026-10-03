#include "UI/HansaSaveLoadPresentationModel.h"

#define LOCTEXT_NAMESPACE "HansaSaveLoadPresentationModel"

void UHansaSaveLoadPresentationModel::Bind(UHansaSaveSubsystem* InSubsystem)
{
	if (Subsystem.IsValid() && SlotsChangedHandle.IsValid()) Subsystem->OnChanged().Remove(SlotsChangedHandle);
	Subsystem = InSubsystem;
	if (InSubsystem != nullptr) SlotsChangedHandle = InSubsystem->OnChanged().AddUObject(this, &UHansaSaveLoadPresentationModel::Refresh);
	Refresh();
}

void UHansaSaveLoadPresentationModel::Open(const FName RestoreFocus)
{
	RestoreFocusSemanticId = RestoreFocus;
	Snapshot.bOpen = true;
	Snapshot.Confirmation = EHansaSaveLoadConfirmation::None;
	if (Subsystem.IsValid())
	{
		Subsystem->Refresh();
	}
	else
	{
		Refresh();
	}
}

void UHansaSaveLoadPresentationModel::Close()
{
	Snapshot.bOpen = false; Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Broadcast(); FocusRestoreRequested.Broadcast(RestoreFocusSemanticId);
}

void UHansaSaveLoadPresentationModel::Refresh()
{
	Snapshot.Slots = Subsystem.IsValid() ? Subsystem->GetSlots() : TArray<FHansaSaveSlotMetadata>();
	Broadcast();
}

void UHansaSaveLoadPresentationModel::SelectSlot(const EHansaSaveSlotId SlotId)
{
	SelectSave(SlotId == EHansaSaveSlotId::Manual ? TEXT("manual") : TEXT("autosave"));
}

void UHansaSaveLoadPresentationModel::SelectSave(const FName StableId)
{
	if (Snapshot.Confirmation != EHansaSaveLoadConfirmation::None) return;
	const auto* Slot = Snapshot.Slots.FindByPredicate([StableId](const auto& Candidate) { return Candidate.StableId == StableId; });
	if (!Slot) return;
	Snapshot.SelectedSaveId = StableId; Snapshot.SelectedSlot = Slot->SlotId;
	Snapshot.Status = EHansaSaveLoadStatus::None; Broadcast();
}

void UHansaSaveLoadPresentationModel::RequestNewSave()
{
	if (Snapshot.Confirmation != EHansaSaveLoadConfirmation::None) return;
	if (!Snapshot.bSavingAllowed || Snapshot.Slots.IsEmpty())
	{
		ReportOperationFailure(LOCTEXT("NewSaveUnavailable", "Start or continue a game before saving."), LOCTEXT("NewSaveRemedy", "Return to a playable campaign and try again.")); return;
	}
	PerformSave(true);
}

void UHansaSaveLoadPresentationModel::RequestSave()
{
	if (Snapshot.Confirmation != EHansaSaveLoadConfirmation::None) return;
    if(!Snapshot.bSavingAllowed){Snapshot.Status=EHansaSaveLoadStatus::Error;Snapshot.StatusMessage=LOCTEXT("StartBeforeSave","Start or continue a game before saving.");Snapshot.StatusRemedy=LOCTEXT("StartRemedy","Return to the title screen and choose New game or Continue.");Broadcast();return;}
	const FHansaSaveSlotMetadata* Slot = Snapshot.Slots.FindByPredicate([this](const auto& Candidate) { return Candidate.StableId == Snapshot.SelectedSaveId; });
	if (Slot != nullptr && Slot->bExists) { Snapshot.Confirmation = EHansaSaveLoadConfirmation::Overwrite; Broadcast(); return; }
	PerformSave();
}

void UHansaSaveLoadPresentationModel::RequestLoad()
{
	if (Snapshot.Confirmation != EHansaSaveLoadConfirmation::None) return;
	const FHansaSaveSlotMetadata* Slot = Snapshot.Slots.FindByPredicate([this](const auto& Candidate) { return Candidate.StableId == Snapshot.SelectedSaveId; });
	if (Slot == nullptr || !Slot->bCanLoad)
	{
		Snapshot.Status = EHansaSaveLoadStatus::Error;
		Snapshot.StatusMessage = Slot && !Slot->Error.IsEmpty() ? Slot->Error : LOCTEXT("CannotLoad", "This slot cannot be loaded.");
		Snapshot.StatusRemedy = Slot && !Slot->Remedy.IsEmpty() ? Slot->Remedy : LOCTEXT("SelectCompatible", "Select a populated compatible slot.");
		Broadcast(); return;
	}
	Snapshot.Confirmation = EHansaSaveLoadConfirmation::Load; Broadcast();
}

void UHansaSaveLoadPresentationModel::Confirm()
{
	if (Snapshot.Confirmation == EHansaSaveLoadConfirmation::Overwrite) PerformSave();
	else if (Snapshot.Confirmation == EHansaSaveLoadConfirmation::Load) PerformLoad();
}

void UHansaSaveLoadPresentationModel::CancelConfirmation() { Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Broadcast(); }

void UHansaSaveLoadPresentationModel::PerformSave(const bool bCreateNew)
{
	if (!Snapshot.bSavingAllowed) return;
	Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Snapshot.Status = EHansaSaveLoadStatus::Working;
	Snapshot.StatusMessage = LOCTEXT("Saving", "Saving…"); Snapshot.StatusRemedy = FText::GetEmpty(); Broadcast();
	FText Error, Remedy;
	FName NewId;
	const FString Name = Snapshot.SaveName.IsEmpty() ? TEXT("Manual save") : Snapshot.SaveName;
	const bool bSaved = Subsystem.IsValid() && (bCreateNew
		? Subsystem->CreateManualSave(Name, NewId, Error, Remedy)
		: Subsystem->SaveById(Snapshot.SelectedSaveId, Snapshot.SelectedSlot == EHansaSaveSlotId::Manual ? Name : TEXT("Autosave"), Error, Remedy));
	if (bSaved && bCreateNew) { Snapshot.SelectedSaveId = NewId; Snapshot.SelectedSlot = EHansaSaveSlotId::Manual; }
	Snapshot.Status = bSaved ? EHansaSaveLoadStatus::Success : EHansaSaveLoadStatus::Error;
	Snapshot.StatusMessage = bSaved ? LOCTEXT("Saved", "Game saved.") : Error;
	Snapshot.StatusRemedy = bSaved ? FText::GetEmpty() : Remedy; Refresh();
}

void UHansaSaveLoadPresentationModel::PerformLoad()
{
	Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Snapshot.Status = EHansaSaveLoadStatus::Working;
	Snapshot.StatusMessage = LOCTEXT("Loading", "Loading…"); Snapshot.StatusRemedy = FText::GetEmpty(); Broadcast();
	FText Error, Remedy; const bool bLoaded = Subsystem.IsValid() && Subsystem->LoadById(Snapshot.SelectedSaveId, Error, Remedy);
	Snapshot.Status = bLoaded ? EHansaSaveLoadStatus::Success : EHansaSaveLoadStatus::Error;
	Snapshot.StatusMessage = bLoaded ? LOCTEXT("Loaded", "Game loaded. Simulation is paused.") : Error;
	Snapshot.StatusRemedy = bLoaded ? LOCTEXT("Resume", "Review the restored state, then resume time when ready.") : Remedy; Refresh();
	if (bLoaded && Snapshot.bOpen) Close();
}

void UHansaSaveLoadPresentationModel::SetFocusedSemanticId(const FName SemanticId) { Snapshot.FocusedSemanticId = SemanticId; Broadcast(); }
void UHansaSaveLoadPresentationModel::Broadcast() { ++Revision; Changed.Broadcast(Snapshot, Revision); }

#undef LOCTEXT_NAMESPACE
