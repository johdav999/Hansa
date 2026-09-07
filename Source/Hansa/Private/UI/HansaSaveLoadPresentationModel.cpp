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
	Snapshot.SelectedSlot = SlotId; Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Snapshot.Status = EHansaSaveLoadStatus::None; Broadcast();
}

void UHansaSaveLoadPresentationModel::RequestSave()
{
	const FHansaSaveSlotMetadata* Slot = Snapshot.Slots.FindByPredicate([this](const auto& Candidate) { return Candidate.SlotId == Snapshot.SelectedSlot; });
	if (Slot != nullptr && Slot->bExists) { Snapshot.Confirmation = EHansaSaveLoadConfirmation::Overwrite; Broadcast(); return; }
	PerformSave();
}

void UHansaSaveLoadPresentationModel::RequestLoad()
{
	const FHansaSaveSlotMetadata* Slot = Snapshot.Slots.FindByPredicate([this](const auto& Candidate) { return Candidate.SlotId == Snapshot.SelectedSlot; });
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

void UHansaSaveLoadPresentationModel::PerformSave()
{
	Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Snapshot.Status = EHansaSaveLoadStatus::Working;
	Snapshot.StatusMessage = LOCTEXT("Saving", "Saving…"); Snapshot.StatusRemedy = FText::GetEmpty(); Broadcast();
	FText Error, Remedy;
	const bool bSaved = Subsystem.IsValid() && Subsystem->Save(Snapshot.SelectedSlot,
		Snapshot.SelectedSlot == EHansaSaveSlotId::Manual ? TEXT("Manual save") : TEXT("Autosave"), Error, Remedy);
	Snapshot.Status = bSaved ? EHansaSaveLoadStatus::Success : EHansaSaveLoadStatus::Error;
	Snapshot.StatusMessage = bSaved ? LOCTEXT("Saved", "Game saved.") : Error;
	Snapshot.StatusRemedy = bSaved ? FText::GetEmpty() : Remedy; Refresh();
}

void UHansaSaveLoadPresentationModel::PerformLoad()
{
	Snapshot.Confirmation = EHansaSaveLoadConfirmation::None; Snapshot.Status = EHansaSaveLoadStatus::Working;
	Snapshot.StatusMessage = LOCTEXT("Loading", "Loading…"); Snapshot.StatusRemedy = FText::GetEmpty(); Broadcast();
	FText Error, Remedy; const bool bLoaded = Subsystem.IsValid() && Subsystem->Load(Snapshot.SelectedSlot, Error, Remedy);
	Snapshot.Status = bLoaded ? EHansaSaveLoadStatus::Success : EHansaSaveLoadStatus::Error;
	Snapshot.StatusMessage = bLoaded ? LOCTEXT("Loaded", "Game loaded. Simulation is paused.") : Error;
	Snapshot.StatusRemedy = bLoaded ? LOCTEXT("Resume", "Review the restored state, then resume time when ready.") : Remedy; Refresh();
}

void UHansaSaveLoadPresentationModel::SetFocusedSemanticId(const FName SemanticId) { Snapshot.FocusedSemanticId = SemanticId; Broadcast(); }
void UHansaSaveLoadPresentationModel::Broadcast() { ++Revision; Changed.Broadcast(Snapshot, Revision); }

#undef LOCTEXT_NAMESPACE