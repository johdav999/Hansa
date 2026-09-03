#pragma once

#include "CoreMinimal.h"
#include "Definitions/HansaDefinitionBase.h"

#include "HansaPopulationDefinitions.generated.h"

UENUM(BlueprintType)
enum class EHansaNeedKind : uint8
{
	Good = 0,
	Service
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Need definition",
	HansaSchemaId = "Hansa.NeedDefinition",
	HansaSchemaVersion = "1"))
class HANSA_API UHansaNeedDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaNeedDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need", meta = (
		DisplayName = "Need kind", ToolTip = "Whether this need consumes an inventoried good or represents a locally supplied service.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "Enum"))
	EHansaNeedKind Kind = EHansaNeedKind::Good;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need", meta = (
		DisplayName = "Good ID", ToolTip = "Good.* consumed by this need. Required for good needs and empty for service needs.",
		HansaRequired = "false", HansaReference = "Good", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ConditionalStableReference"))
	FString GoodId;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaPopulationTierNeed
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need", meta = (
		DisplayName = "Need ID", ToolTip = "Stable Need.* identity required by this population tier.",
		HansaRequired = "true", HansaReference = "Need", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString NeedId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need", meta = (
		DisplayName = "Consumption per resident per tick", ToolTip = "Required good quantity per resident and tick in milli-units; service needs must use zero.",
		ClampMin = "0", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NeedConsumption",
		HansaUnit = "MilliUnitPerResidentTick", HansaMin = "0", HansaMax = "2147483647"))
	int32 ConsumptionMilliUnitsPerResidentPerTick = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Need", meta = (
		DisplayName = "Importance", ToolTip = "Relative contribution of this need to tier satisfaction in basis points.",
		ClampMin = "1", ClampMax = "10000", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "1", HansaMax = "10000"))
	int32 ImportanceBasisPoints = 10000;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Population tier definition",
	HansaSchemaId = "Hansa.PopulationTierDefinition",
	HansaSchemaVersion = "1"))
class HANSA_API UHansaPopulationTierDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaPopulationTierDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Progression", meta = (
		DisplayName = "Previous tier ID", ToolTip = "Optional PopulationTier.* prerequisite; empty identifies the base tier.",
		HansaRequired = "false", HansaReference = "PopulationTier", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "OptionalStableReference"))
	FString PreviousTierId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Needs", meta = (
		DisplayName = "Needs", ToolTip = "Deterministic consumption and service expectations for this tier.",
		HansaRequired = "true", HansaReference = "Need", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "PopulationNeeds"))
	TArray<FHansaPopulationTierNeed> Needs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Workforce", meta = (
		DisplayName = "Workforce share", ToolTip = "Residents available as this tier's workforce in basis points.",
		ClampMin = "0", ClampMax = "10000", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "0", HansaMax = "10000"))
	int32 WorkforcePerResidentBasisPoints = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Migration", meta = (
		DisplayName = "Growth satisfaction", ToolTip = "Minimum bounded satisfaction required to accumulate growth ticks.",
		ClampMin = "0", ClampMax = "10000", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "0", HansaMax = "10000"))
	int32 GrowthSatisfactionBasisPoints = 8000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Migration", meta = (
		DisplayName = "Decline satisfaction", ToolTip = "Maximum bounded satisfaction that accumulates decline ticks.",
		ClampMin = "0", ClampMax = "10000", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "0", HansaMax = "10000"))
	int32 DeclineSatisfactionBasisPoints = 3500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Migration", meta = (
		DisplayName = "Evaluation ticks", ToolTip = "Consecutive qualifying ticks required before a bounded population change.",
		ClampMin = "1", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "2147483647"))
	int32 EvaluationTicks = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Migration", meta = (
		DisplayName = "Growth residents", ToolTip = "Residents added after one successful evaluation, bounded by residence capacity.",
		ClampMin = "1", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "Resident", HansaMin = "1", HansaMax = "2147483647"))
	int32 GrowthResidentsPerEvaluation = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population|Migration", meta = (
		DisplayName = "Decline residents", ToolTip = "Residents removed after one failed evaluation, never below zero.",
		ClampMin = "1", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "Resident", HansaMin = "1", HansaMax = "2147483647"))
	int32 DeclineResidentsPerEvaluation = 1;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
