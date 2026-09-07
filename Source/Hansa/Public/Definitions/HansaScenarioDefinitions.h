#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "HansaScenarioDefinitions.generated.h"

UENUM(BlueprintType)
enum class EHansaAuthoredObjectiveMetric : uint8
{
	HouseMoneyAtLeast = 0 UMETA(DisplayName = "House money at least"),
	CityPopulationAtLeast UMETA(DisplayName = "City population at least"),
	ArtisanPopulationAtLeast UMETA(DisplayName = "Artisan population at least"),
	ActiveSeaRoutesAtLeast UMETA(DisplayName = "Active sea routes at least"),
	ActiveLandRoutesAtLeast UMETA(DisplayName = "Active land routes at least"),
	CompletedTradeLegsAtLeast UMETA(DisplayName = "Completed trade legs at least"),
	CompletedTechnologiesAtLeast UMETA(DisplayName = "Completed technologies at least"),
	ActiveProductionsAtLeast UMETA(DisplayName = "Active productions at least"),
	CitySatisfactionAtLeast UMETA(DisplayName = "City satisfaction at least"),
	MarketStockAtLeast UMETA(DisplayName = "Market stock at least"),
	MarketPriceAtMost UMETA(DisplayName = "Market price at most")
};

/** One reusable, typed and inspectable scenario threshold. */
UCLASS(BlueprintType, meta = (
	DisplayName = "Scenario objective definition", HansaSchemaId = "Hansa.ScenarioObjectiveDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaScenarioObjectiveDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaScenarioObjectiveDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Objective", meta = (
		DisplayName = "Metric", ToolTip = "Typed read-only runtime fact evaluated for this objective.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "Enum"))
	EHansaAuthoredObjectiveMetric Metric = EHansaAuthoredObjectiveMetric::HouseMoneyAtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Objective", meta = (
		DisplayName = "Target value", ToolTip = "Explicit integer threshold in the unit implied by Metric.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "ObjectiveThreshold",
		HansaUnit = "TypedInteger", HansaMin = "0", HansaMax = "9223372036854775807"))
	int64 TargetValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Objective", meta = (
		DisplayName = "City ID", ToolTip = "Required City.* reference for city, market and civic metrics.",
		HansaRequired = "false", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ConditionalReference"))
	FString CityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Objective", meta = (
		DisplayName = "Good ID", ToolTip = "Required Good.* reference for market stock and price metrics.",
		HansaRequired = "false", HansaReference = "Good", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ConditionalReference"))
	FString GoodId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Presentation", meta = (
		DisplayName = "Progress unit", ToolTip = "Short localized unit token shown beside current and target values.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Generate",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonEmpty"))
	FText ProgressUnit;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

/** One complete alternate victory path; every referenced objective must be satisfied together. */
UCLASS(BlueprintType, meta = (
	DisplayName = "Victory definition", HansaSchemaId = "Hansa.VictoryDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaVictoryDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaVictoryDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Victory", meta = (
		DisplayName = "Objective IDs", ToolTip = "Unique ScenarioObjective.* definitions that must all remain met.",
		HansaRequired = "true", HansaReference = "ScenarioObjective", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReferenceArray"))
	TArray<FString> ObjectiveIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Victory", meta = (
		DisplayName = "Sustain ticks", ToolTip = "Consecutive evaluation ticks all objectives must remain met before victory.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "1000000"))
	int32 SustainTicks = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Victory", meta = (
		DisplayName = "Ending priority", ToolTip = "Lower unique values win when paths become eligible on the same evaluation.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "Priority", HansaMin = "0", HansaMax = "1000000"))
	int32 EndingPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Presentation", meta = (
		DisplayName = "Summary", ToolTip = "Player-facing explanation of this strategic victory path.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonEmpty"))
	FText Summary;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

/** Scenario start policy, alternate endings and bounded insolvency defeat contract. */
UCLASS(BlueprintType, meta = (
	DisplayName = "Scenario definition", HansaSchemaId = "Hansa.ScenarioDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaScenarioDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaScenarioDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (
		DisplayName = "Home city ID", ToolTip = "City whose population, civic and market objectives define the scenario.",
		HansaRequired = "true", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Read",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString HomeCityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (
		DisplayName = "Victory IDs", ToolTip = "Ordered set of alternate Victory.* endings enabled for this scenario.",
		HansaRequired = "true", HansaReference = "Victory", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ScenarioEndings"))
	TArray<FString> VictoryIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Failure", meta = (
		DisplayName = "Insolvency threshold", ToolTip = "House money at or below this amount starts the defeat sustain timer.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Integer",
		HansaUnit = "Pfennig", HansaMin = "-1000000000", HansaMax = "0"))
	int64 InsolvencyThresholdPfennig = -5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Failure", meta = (
		DisplayName = "Failure sustain ticks", ToolTip = "Consecutive insolvent ticks with no active route or production before defeat.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "1000000"))
	int32 FailureSustainTicks = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario|Presentation", meta = (
		DisplayName = "Briefing", ToolTip = "Player-facing scenario start description.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonEmpty"))
	FText Briefing;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

