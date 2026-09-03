#pragma once

#include "Commandlets/Commandlet.h"

#include "HansaEconomicDefinitionValidateCommandlet.generated.h"

/** Headless production-graph validator. Writes machine-readable diagnostics and returns non-zero on errors. */
UCLASS()
class HANSAEDITOR_API UHansaEconomicDefinitionValidateCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UHansaEconomicDefinitionValidateCommandlet();
	virtual int32 Main(const FString& Params) override;
};
