#pragma once

#include "CoreMinimal.h"
#include "Research/HansaResearch.h"
#include "HansaResearchPresentationModel.generated.h"

namespace Hansa::Simulation { class FHansaEconomicRegistry; }

UENUM()
enum class EHansaResearchNodePresentationState : uint8
{
	Locked = 0,
	Available,
	Queued,
	Completed
};

USTRUCT()
struct FHansaResearchNodePresentation
{
	GENERATED_BODY()

	FString StableId;
	FText DisplayName;
	Hansa::Simulation::EHansaResearchBranch Branch = Hansa::Simulation::EHansaResearchBranch::Commerce;
	EHansaResearchNodePresentationState State = EHansaResearchNodePresentationState::Locked;
	TArray<FString> PrerequisiteIds;
	TArray<FString> MissingPrerequisiteIds;
	FText UnlockExplanation;
    FText EffectSummary;
    FText AppliedEffectSummary;
    FText LockedReason;
    TArray<Hansa::Simulation::FHansaCompiledResearchEffect> Effects;
    Hansa::Simulation::EHansaResearchQueueError QueueError = Hansa::Simulation::EHansaResearchQueueError::None;
	int32 CostResearchPoints = 0;
	int32 DurationTicks = 1;
	int32 ProgressTicks = 0;
};

USTRUCT()
struct FHansaResearchPresentationSnapshot
{
	GENERATED_BODY()

    bool bOpen = false;
    bool bLoading = true;
    bool bSubmitting = false;
    FText Feedback;
	int32 AvailableResearchPoints = 0;
	FString SelectedTechnologyId;
	FName FocusedSemanticId;
	TArray<FHansaResearchNodePresentation> Nodes;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaResearchPresentationChanged, const FHansaResearchPresentationSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaResearchFocusRestoreRequested, FName);

/** Event-driven read model for the native research screen. Commands are submitted through an injected authoritative intent. */
UCLASS()
class HANSA_API UHansaResearchPresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		const Hansa::Simulation::FHansaHouseResearchState& State);
	void Refresh(const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		const Hansa::Simulation::FHansaHouseResearchState& State);
	void Open(FName OpenerSemanticId);
	void Close();
	bool SelectTechnology(const FString& TechnologyId);
	bool RequestQueueSelected();
	void SetQueueIntent(TFunction<bool(const FString&)> InIntent) { QueueIntent = MoveTemp(InIntent); }
    void SetFocusedSemanticId(FName SemanticId);
    void SetLoading(bool bLoading);
    void SetError(const FText& Message);
    void SetEffectIntent(TFunction<bool(const Hansa::Simulation::FHansaCompiledResearchEffect&)> Intent) { EffectIntent = MoveTemp(Intent); }
    bool RequestEffect(int32 Index);

	[[nodiscard]] const FHansaResearchPresentationSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaResearchPresentationChanged& OnChanged() { return Changed; }
	FHansaResearchFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }

private:
	void Broadcast();
	FHansaResearchPresentationSnapshot Snapshot;
	FName Opener;
	uint64 Revision = 0;
    TFunction<bool(const FString&)> QueueIntent;
    TFunction<bool(const Hansa::Simulation::FHansaCompiledResearchEffect&)> EffectIntent;
	FHansaResearchPresentationChanged Changed;
	FHansaResearchFocusRestoreRequested FocusRestoreRequested;
};

