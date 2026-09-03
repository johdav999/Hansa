#pragma once

#include "Commandlets/Commandlet.h"

#include "HansaEconomicDefinitionSeedCommandlet.generated.h"

/** Explicit one-shot authoring commandlet; never runs as part of normal CI or game startup. */
UCLASS()
class HANSAEDITOR_API UHansaEconomicDefinitionSeedCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UHansaEconomicDefinitionSeedCommandlet();
	virtual int32 Main(const FString& Params) override;
};
