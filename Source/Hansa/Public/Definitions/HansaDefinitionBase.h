#pragma once

#include "Engine/DataAsset.h"

#include "HansaDefinitionBase.generated.h"

UENUM()
enum class EHansaDefinitionValidationSeverity : uint8
{
	Information,
	Warning,
	Error
};

struct HANSA_API FHansaDefinitionValidationIssue final
{
	EHansaDefinitionValidationSeverity Severity = EHansaDefinitionValidationSeverity::Error;
	FName Code;
	FString PropertyPath;
	FText Cause;
	FText Remedy;
};

/**
 * Authoritative accepted-content asset contract. JSON schemas and editor fields
 * are derived from this reflected model; JSON is never a second source of truth.
 */
UCLASS(Abstract, BlueprintType)
class HANSA_API UHansaDefinitionBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UHansaDefinitionBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Identity", meta = (
		DisplayName = "Stable definition ID",
		ToolTip = "Immutable canonical Domain.Name identity used by runtime registries, saves, references and authoring tools.",
		HansaRequired = "true",
		HansaReference = "Definition",
		HansaBulkEditable = "false",
		HansaAIAccess = "Never",
		HansaMigration = "Identity",
		HansaSerialization = "Included",
		HansaValidation = "StableId"))
	FString StableDefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Definition|Versioning", meta = (
		DisplayName = "Schema version",
		ToolTip = "Version of this reflected definition schema. Breaking changes require an explicit migration.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "false",
		HansaAIAccess = "Never",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "Range",
		HansaUnit = "Version",
		HansaMin = "1",
		HansaMax = "2147483647"))
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Versioning", meta = (
		DisplayName = "Authored revision",
		ToolTip = "Monotonic human-authored revision used for reviewed patch and stale-draft checks.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "false",
		HansaAIAccess = "Read",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Range",
		HansaUnit = "Revision",
		HansaMin = "1",
		HansaMax = "2147483647"))
	int32 AuthoredRevision = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Presentation", meta = (
		DisplayName = "Display name",
		ToolTip = "Localized display text shown to authors and players; it never defines stable identity.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonEmpty"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Presentation", meta = (
		DisplayName = "Localization key",
		ToolTip = "Stable localization identity for the display name.",
		HansaRequired = "true",
		HansaReference = "LocalizationKey",
		HansaBulkEditable = "false",
		HansaAIAccess = "Read",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "NonEmpty"))
	FName LocalizationKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Organization", meta = (
		DisplayName = "Category",
		ToolTip = "Authoring category used for browsing and filtering; it does not define gameplay identity.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonEmpty"))
	FName DefinitionCategory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Organization", meta = (
		DisplayName = "Tags",
		ToolTip = "Deterministically sorted authoring and content-selection tags.",
		HansaRequired = "false",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Optional"))
	TArray<FName> Tags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Organization", meta = (
		DisplayName = "Content set",
		ToolTip = "Stable content-set name controlling registry inclusion and fixture availability.",
		HansaRequired = "true",
		HansaReference = "ContentSet",
		HansaBulkEditable = "true",
		HansaAIAccess = "Read",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "NonEmpty"))
	FName ContentSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Deprecation", meta = (
		DisplayName = "Deprecated",
		ToolTip = "Marks this definition unavailable for new content while preserving compatibility for existing references.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Never",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "Boolean"))
	bool bDeprecated = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition|Deprecation", meta = (
		DisplayName = "Replacement definition ID",
		ToolTip = "Canonical stable ID that replaces this definition when Deprecated is enabled.",
		HansaRequired = "false",
		HansaReference = "Definition",
		HansaBulkEditable = "false",
		HansaAIAccess = "Never",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "ConditionalReference"))
	FString ReplacementDefinitionId;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Definition|Derived", meta = (
		DisplayName = "Content hash",
		ToolTip = "Derived deterministic hash of accepted definition content; this value is never authored or proposed.",
		HansaRequired = "false",
		HansaReference = "None",
		HansaBulkEditable = "false",
		HansaAIAccess = "Never",
		HansaMigration = "Derived",
		HansaSerialization = "Derived",
		HansaValidation = "Derived",
		HansaUnit = "Hash64",
		HansaMin = "0",
		HansaMax = "18446744073709551615"))
	uint64 ContentHash = 0;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;
	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const;
	[[nodiscard]] virtual uint64 ComputeDeterministicContentHash() const;
	void RefreshContentHash();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const;
};
