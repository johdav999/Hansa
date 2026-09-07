#pragma once

#include "CoreMinimal.h"
#include "Save/HansaSaveEnvelope.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HansaSaveSubsystem.generated.h"

class UHansaRuntimeSimulationHost;

UENUM(BlueprintType)
enum class EHansaSaveSlotId : uint8 { Manual, Autosave };

UENUM(BlueprintType)
enum class EHansaSaveSlotCompatibility : uint8 { Empty, Compatible, Incompatible, Corrupt };

USTRUCT(BlueprintType)
struct HANSA_API FHansaSaveSlotMetadata final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") EHansaSaveSlotId SlotId = EHansaSaveSlotId::Manual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FName StableId = TEXT("manual");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FText SlotLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FString DisplayName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FString SavedUtc;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FString ScenarioId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FString BuildVersion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") int32 FormatVersion = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") int64 SimulationTick = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FString AuthoritativeHash;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FString CampaignHash;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") EHansaSaveSlotCompatibility Compatibility = EHansaSaveSlotCompatibility::Empty;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FText CompatibilityLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FText Error;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") FText Remedy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") bool bExists = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Save") bool bCanLoad = false;
};

DECLARE_MULTICAST_DELEGATE(FHansaSaveSlotsChanged);

/** Owns the two allowlisted MVP slots. Callers never provide a path or filename. */
UCLASS()
class HANSA_API UHansaSaveSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void BindRuntime(UHansaRuntimeSimulationHost* InHost);
	void Refresh();
	bool Save(EHansaSaveSlotId SlotId, const FString& DisplayName, FText& OutError, FText& OutRemedy);
	bool Load(EHansaSaveSlotId SlotId, FText& OutError, FText& OutRemedy);
	[[nodiscard]] const TArray<FHansaSaveSlotMetadata>& GetSlots() const { return Slots; }
	[[nodiscard]] const FHansaSaveSlotMetadata* FindSlot(EHansaSaveSlotId SlotId) const;
	FHansaSaveSlotsChanged& OnChanged() { return Changed; }
private:
	[[nodiscard]] FString SlotPath(EHansaSaveSlotId SlotId) const;
	FHansaSaveSlotMetadata Inspect(EHansaSaveSlotId SlotId) const;
	TWeakObjectPtr<UHansaRuntimeSimulationHost> Host;
	UPROPERTY(Transient) TArray<FHansaSaveSlotMetadata> Slots;
	FHansaSaveSlotsChanged Changed;
};