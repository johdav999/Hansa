#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "HansaResearchDefinitions.generated.h"

UENUM(BlueprintType)
enum class EHansaAuthoredResearchBranch : uint8
{
	Commerce = 0 UMETA(DisplayName = "Commerce"),
	Production UMETA(DisplayName = "Production"),
	Logistics UMETA(DisplayName = "Logistics")
};

UENUM(BlueprintType)
enum class EHansaAuthoredResearchEffectKind : uint8
{
	UnlockStableId = 0 UMETA(DisplayName = "Unlock stable ID"),
	MarketReportAgeReductionTicks UMETA(DisplayName = "Market report age reduction"),
	TransactionFrictionReductionBasisPoints UMETA(DisplayName = "Transaction friction reduction"),
	ProductionThroughputBasisPoints UMETA(DisplayName = "Production throughput"),
	WarehouseHandlingBasisPoints UMETA(DisplayName = "Warehouse handling"),
	VehicleCapacityBasisPoints UMETA(DisplayName = "Vehicle capacity"),
	ReserveAutomation UMETA(DisplayName = "Reserve automation"),
	RouteScheduling UMETA(DisplayName = "Route scheduling")
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaResearchEffectDefinition final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Effect kind", ToolTip = "Typed deterministic effect applied when research completes.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "false", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "Enum"))
	EHansaAuthoredResearchEffectKind Kind = EHansaAuthoredResearchEffectKind::UnlockStableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Target stable ID", ToolTip = "Canonical gameplay target. Empty is allowed only for global effects.",
		HansaRequired = "false", HansaReference = "StableDefinition", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "OptionalStableReference"))
	FString TargetStableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Magnitude", ToolTip = "Deterministic integer effect magnitude in the unit implied by Effect kind.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "TypedInteger", HansaMin = "0", HansaMax = "1000000000"))
	int32 Magnitude = 0;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Technology definition", HansaSchemaId = "Hansa.TechnologyDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaTechnologyDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaTechnologyDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Branch", ToolTip = "Bounded MVP research branch used by runtime and graph layout.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Enum"))
	EHansaAuthoredResearchBranch Branch = EHansaAuthoredResearchBranch::Commerce;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Prerequisites", ToolTip = "Stable Technology.* IDs that must all be complete before this node can be queued.",
		HansaRequired = "false", HansaReference = "Technology", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ResearchPrerequisites"))
	TArray<FString> PrerequisiteTechnologyIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Research point cost", ToolTip = "Points charged atomically when the node enters the one-item queue.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "ResearchPoint", HansaMin = "0", HansaMax = "1000000000"))
	int32 CostResearchPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Duration", ToolTip = "Authoritative simulation ticks required for completion.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "2147483647"))
	int32 DurationTicks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Unlock explanation", ToolTip = "Player-facing explanation of what changes on completion.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonEmpty"))
	FText UnlockExplanation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Research", meta = (
		DisplayName = "Effects", ToolTip = "Stable-ID based deterministic effects applied in authored order then canonicalized.",
		HansaRequired = "true", HansaReference = "StableDefinition", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ResearchEffects"))
	TArray<FHansaResearchEffectDefinition> Effects;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

