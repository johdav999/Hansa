#pragma once

#include "Definitions/HansaDefinitionBase.h"

#include "HansaFoundationSampleDefinition.generated.h"

/** Editor-only transient definition used to prove generic reflected coverage. */
UCLASS(Transient, NotBlueprintable, meta = (
	DisplayName = "Foundation sample definition",
	HansaSchemaId = "Hansa.FoundationSampleDefinition",
	HansaSchemaVersion = "1"))
class HANSAEDITOR_API UHansaFoundationSampleDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaFoundationSampleDefinition();

	UPROPERTY(EditAnywhere, Category = "Sample", meta = (
		DisplayName = "Sample value",
		ToolTip = "Small bounded property used to prove automatic Details and JSON Schema discovery without a handwritten form.",
		ClampMin = "0",
		ClampMax = "100",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Range",
		HansaUnit = "Percent",
		HansaMin = "0",
		HansaMax = "100"))
	int32 SampleValue = 42;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
