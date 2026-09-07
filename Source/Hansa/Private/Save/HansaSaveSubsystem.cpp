#include "Save/HansaSaveSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/HansaRuntimeSimulationHost.h"

#define LOCTEXT_NAMESPACE "HansaSaveSubsystem"
using namespace Hansa::Simulation;

namespace
{
	FName SlotStableId(const EHansaSaveSlotId Slot) { return Slot == EHansaSaveSlotId::Manual ? TEXT("manual") : TEXT("autosave"); }
	FText SlotLabel(const EHansaSaveSlotId Slot) { return Slot == EHansaSaveSlotId::Manual ? LOCTEXT("Manual", "Manual save") : LOCTEXT("Autosave", "Autosave"); }
	FString Hex64(const uint64 Value) { return FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Value)); }
	void DescribeError(const EHansaSaveError Error, const FString& Detail, FText& OutError, FText& OutRemedy)
	{
		OutError = Detail.IsEmpty() ? LOCTEXT("UnknownSaveError", "The save operation failed.") : FText::FromString(Detail);
		switch (Error)
		{
		case EHansaSaveError::IncompatibleContent: OutRemedy = LOCTEXT("ContentRemedy", "Restore the matching content build, then try this slot again."); break;
		case EHansaSaveError::IncompatibleSimulation: OutRemedy = LOCTEXT("SimulationRemedy", "Update Hansa or load this save with the build that created it."); break;
		case EHansaSaveError::IncompatibleScenario: OutRemedy = LOCTEXT("ScenarioRemedy", "Start the scenario named by the save, then try again."); break;
		case EHansaSaveError::UnsupportedFormat: OutRemedy = LOCTEXT("FormatRemedy", "Use a supported save version or restore a compatible backup."); break;
		case EHansaSaveError::CorruptData: OutRemedy = LOCTEXT("CorruptRemedy", "Choose another slot. This file is damaged or incomplete."); break;
		default: OutRemedy = LOCTEXT("RetryRemedy", "Retry the operation. If it persists, choose another slot."); break;
		}
	}
}

void UHansaSaveSubsystem::BindRuntime(UHansaRuntimeSimulationHost* InHost) { Host = InHost; Refresh(); }

FString UHansaSaveSubsystem::SlotPath(const EHansaSaveSlotId SlotId) const
{
	const TCHAR* File = SlotId == EHansaSaveSlotId::Manual ? TEXT("manual.hansa") : TEXT("autosave.hansa");
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Hansa"), File);
}

const FHansaSaveSlotMetadata* UHansaSaveSubsystem::FindSlot(const EHansaSaveSlotId SlotId) const
{
	return Slots.FindByPredicate([SlotId](const FHansaSaveSlotMetadata& Slot) { return Slot.SlotId == SlotId; });
}

FHansaSaveSlotMetadata UHansaSaveSubsystem::Inspect(const EHansaSaveSlotId SlotId) const
{
	FHansaSaveSlotMetadata Metadata; Metadata.SlotId = SlotId; Metadata.StableId = SlotStableId(SlotId); Metadata.SlotLabel = SlotLabel(SlotId);
	const FString Path = SlotPath(SlotId);
	Metadata.bExists = IFileManager::Get().FileExists(*Path);
	if (!Metadata.bExists)
	{
		Metadata.Compatibility = EHansaSaveSlotCompatibility::Empty; Metadata.CompatibilityLabel = LOCTEXT("Empty", "Empty slot"); return Metadata;
	}
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || !Host.IsValid())
	{
		Metadata.Compatibility = EHansaSaveSlotCompatibility::Corrupt; Metadata.CompatibilityLabel = LOCTEXT("Unreadable", "Unreadable");
		Metadata.Error = LOCTEXT("ReadFailure", "The save file could not be read."); Metadata.Remedy = LOCTEXT("ReadFailureRemedy", "Choose another slot or create a new save."); return Metadata;
	}
	FHansaSaveMetadata HeaderMetadata;
	FHansaSaveEnvelope::InspectMetadata(Bytes, HeaderMetadata);
	Metadata.FormatVersion = static_cast<int32>(HeaderMetadata.FormatVersion);
	Metadata.DisplayName = HeaderMetadata.DisplayName; Metadata.SavedUtc = HeaderMetadata.SavedUtc;
	Metadata.ScenarioId = HeaderMetadata.ScenarioId; Metadata.BuildVersion = HeaderMetadata.BuildVersion;
	Metadata.AuthoritativeHash = Hex64(HeaderMetadata.AuthoritativeHash); Metadata.CampaignHash = Hex64(HeaderMetadata.CampaignHash);
	FHansaSaveSnapshot Snapshot;
	int64 DecodedTick = 0;
	const FHansaSaveResult Result = Host->InspectSaveBytes(Bytes, Snapshot, DecodedTick);
	if (Metadata.FormatVersion == 0) Metadata.FormatVersion = static_cast<int32>(Result.SourceFormatVersion);
	if (!Result)
	{
		Metadata.Compatibility = Result.Error == EHansaSaveError::CorruptData ? EHansaSaveSlotCompatibility::Corrupt : EHansaSaveSlotCompatibility::Incompatible;
		Metadata.CompatibilityLabel = Metadata.Compatibility == EHansaSaveSlotCompatibility::Corrupt ? LOCTEXT("Corrupt", "Corrupt") : LOCTEXT("Incompatible", "Incompatible");
		DescribeError(Result.Error, Result.Message, Metadata.Error, Metadata.Remedy); return Metadata;
	}
	Metadata.DisplayName = Snapshot.DisplayName; Metadata.SavedUtc = Snapshot.SavedUtc; Metadata.ScenarioId = Snapshot.Scenario.ScenarioId;
	Metadata.BuildVersion = Snapshot.BuildVersion; Metadata.SimulationTick = DecodedTick;
	Metadata.Compatibility = EHansaSaveSlotCompatibility::Compatible; Metadata.CompatibilityLabel = LOCTEXT("Compatible", "Compatible"); Metadata.bCanLoad = true;
	return Metadata;
}

void UHansaSaveSubsystem::Refresh()
{
	Slots = { Inspect(EHansaSaveSlotId::Manual), Inspect(EHansaSaveSlotId::Autosave) }; Changed.Broadcast();
}

bool UHansaSaveSubsystem::Save(const EHansaSaveSlotId SlotId, const FString& DisplayName, FText& OutError, FText& OutRemedy)
{
	OutError = FText::GetEmpty(); OutRemedy = FText::GetEmpty();
	if (!Host.IsValid()) { OutError = LOCTEXT("NoRuntime", "The current game is not ready to save."); OutRemedy = LOCTEXT("NoRuntimeRemedy", "Return to the running scenario and try again."); return false; }
	TArray<uint8> Bytes;
	const FHansaSaveResult Captured = Host->CaptureSaveBytes(Bytes, DisplayName, FDateTime::UtcNow().ToIso8601());
	if (!Captured) { DescribeError(Captured.Error, Captured.Message, OutError, OutRemedy); return false; }
	const FString Path = SlotPath(SlotId); const FString TempPath = Path + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true); IFileManager::Get().Delete(*TempPath, false, true);
	if (!FFileHelper::SaveArrayToFile(Bytes, *TempPath) || !IFileManager::Get().Move(*Path, *TempPath, true, true, false, true))
	{
		IFileManager::Get().Delete(*TempPath, false, true);
		OutError = LOCTEXT("WriteFailure", "The save could not be written atomically."); OutRemedy = LOCTEXT("WriteFailureRemedy", "Check available disk space and retry."); return false;
	}
	Refresh(); return true;
}

bool UHansaSaveSubsystem::Load(const EHansaSaveSlotId SlotId, FText& OutError, FText& OutRemedy)
{
	OutError = FText::GetEmpty(); OutRemedy = FText::GetEmpty();
	const FHansaSaveSlotMetadata* Slot = FindSlot(SlotId);
	if (!Slot || !Slot->bCanLoad || !Host.IsValid())
	{
		OutError = Slot && !Slot->Error.IsEmpty() ? Slot->Error : LOCTEXT("SlotUnavailable", "This slot cannot be loaded.");
		OutRemedy = Slot && !Slot->Remedy.IsEmpty() ? Slot->Remedy : LOCTEXT("SlotUnavailableRemedy", "Select a compatible populated slot."); return false;
	}
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SlotPath(SlotId)))
	{
		OutError = LOCTEXT("LoadReadFailure", "The save disappeared before it could be loaded."); OutRemedy = LOCTEXT("LoadReadRemedy", "Refresh the slot list and try again."); Refresh(); return false;
	}
	const FHansaSaveResult Restored = Host->RestoreSaveBytes(Bytes);
	if (!Restored) { DescribeError(Restored.Error, Restored.Message, OutError, OutRemedy); Refresh(); return false; }
	Refresh(); return true;
}

#undef LOCTEXT_NAMESPACE