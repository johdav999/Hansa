#pragma once

#include "CoreMinimal.h"
#include "Save/HansaSaveSubsystem.h"
#include "UObject/Object.h"
#include "HansaSaveLoadPresentationModel.generated.h"

UENUM(BlueprintType)
enum class EHansaSaveLoadConfirmation : uint8 { None, Overwrite, Load };

UENUM(BlueprintType)
enum class EHansaSaveLoadStatus : uint8 { None, Working, Success, Warning, Error };

USTRUCT(BlueprintType)
struct HANSA_API FHansaSaveLoadPresentationSnapshot final
{
	GENERATED_BODY()
    bool bSavingAllowed=true;
    FString SaveName=TEXT("Manual save");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") bool bOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") EHansaSaveSlotId SelectedSlot = EHansaSaveSlotId::Manual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") TArray<FHansaSaveSlotMetadata> Slots;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") EHansaSaveLoadConfirmation Confirmation = EHansaSaveLoadConfirmation::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") EHansaSaveLoadStatus Status = EHansaSaveLoadStatus::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") FText StatusMessage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") FText StatusRemedy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Save") FName FocusedSemanticId;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaSaveLoadPresentationChanged, const FHansaSaveLoadPresentationSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaSaveLoadFocusRestoreRequested, FName);

/** Intent-only save/load model with explicit overwrite/load confirmation. */
UCLASS(BlueprintType)
class HANSA_API UHansaSaveLoadPresentationModel final : public UObject
{
	GENERATED_BODY()
public:
	void Bind(UHansaSaveSubsystem* InSubsystem);
	void Open(FName RestoreFocus = TEXT("HUD.TopStatus.SaveLoad"));
	void Close();
	void Refresh();
	void SelectSlot(EHansaSaveSlotId SlotId);
	void SetSavingAllowed(bool Allowed){Snapshot.bSavingAllowed=Allowed;Broadcast();}
    void SetSaveName(const FString& Name){Snapshot.SaveName=Name.Left(64).TrimStartAndEnd();}
	void RequestSave();
	void RequestLoad();
    void ReportOperationFailure(FText Error,FText Remedy){Snapshot.Status=EHansaSaveLoadStatus::Error;Snapshot.StatusMessage=MoveTemp(Error);Snapshot.StatusRemedy=MoveTemp(Remedy);Broadcast();}
	void Confirm();
	void CancelConfirmation();
	void SetFocusedSemanticId(FName SemanticId);
	[[nodiscard]] const FHansaSaveLoadPresentationSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaSaveLoadPresentationChanged& OnChanged() { return Changed; }
	FHansaSaveLoadFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
private:
	void Broadcast();
	void PerformSave();
	void PerformLoad();
	TWeakObjectPtr<UHansaSaveSubsystem> Subsystem;
	FDelegateHandle SlotsChangedHandle;
	UPROPERTY(VisibleAnywhere, Category="Hansa|UI|Save") FHansaSaveLoadPresentationSnapshot Snapshot;
	uint64 Revision = 0;
	FName RestoreFocusSemanticId = TEXT("HUD.TopStatus.SaveLoad");
	FHansaSaveLoadPresentationChanged Changed;
	FHansaSaveLoadFocusRestoreRequested FocusRestoreRequested;
};