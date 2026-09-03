#pragma once

#include "Definitions/HansaDefinitionBase.h"

#include "HansaEditorSchemaTestDefinitions.generated.h"

/** Negative-test type: abstract so normal registry discovery remains valid. */
UCLASS(Abstract, Transient, meta = (
	DisplayName = "Incomplete metadata test definition",
	HansaSchemaId = "Hansa.IncompleteMetadataTestDefinition",
	HansaSchemaVersion = "1"))
class UHansaIncompleteMetadataTestDefinition : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Test")
	int32 MissingMetadata = 1;
};
