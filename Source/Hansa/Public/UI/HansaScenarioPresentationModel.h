#pragma once

#include "CoreMinimal.h"
#include "Scenario/HansaScenario.h"
#include "UObject/Object.h"
#include "HansaScenarioPresentationModel.generated.h"

UENUM(BlueprintType)
enum class EHansaScenarioPresentationPhase : uint8
{
	Briefing = 0,
	Active,
	Victory,
	Failure,
	Error
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaScenarioObjectivePresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FName StableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Progress;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Status;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") float Ratio = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") bool bMet = false;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaVictoryPathPresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FName StableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Summary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText SustainProgress;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") float SustainRatio = 0.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") bool bAllObjectivesMet = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") bool bVictorious = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") TArray<FHansaScenarioObjectivePresentation> Objectives;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaScenarioPresentationSnapshot final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FName ScenarioId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Title;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText Briefing;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText StateLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FText OutcomeExplanation;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") EHansaScenarioPresentationPhase Phase = EHansaScenarioPresentationPhase::Briefing;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") TArray<FHansaVictoryPathPresentation> Paths;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FName SelectedVictoryId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Scenario") bool bOpen = true;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaScenarioPresentationChanged, const FHansaScenarioPresentationSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaScenarioFocusRestoreRequested, FName);

/** Event-refreshed UI model for authored scenario progress; it never mutates gameplay. */
UCLASS(BlueprintType)
class HANSA_API UHansaScenarioPresentationModel final : public UObject
{
	GENERATED_BODY()
public:
	void InitializeDefaults();
	bool ApplyProgress(const Hansa::Simulation::FHansaScenarioProgress& Progress);
	void Open(FName RestoreFocus = TEXT("HUD.AlertStack.Toggle"));
	void AcknowledgeBriefing();
	void Close();
	void SelectPath(FName VictoryId);
	void SetFocusedSemanticId(FName SemanticId);
	[[nodiscard]] const FHansaScenarioPresentationSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaScenarioPresentationChanged& OnChanged() { return Changed; }
	FHansaScenarioFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
private:
	void Broadcast();
	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|Scenario") FHansaScenarioPresentationSnapshot Snapshot;
	uint64 Revision = 0;
	FName RestoreFocusSemanticId = TEXT("HUD.AlertStack.Toggle");
	FHansaScenarioPresentationChanged Changed;
	FHansaScenarioFocusRestoreRequested FocusRestoreRequested;
};

