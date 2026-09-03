#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicRegistry.h"

struct HANSA_API FHansaEconomicRegistryCompileResult final
{
	Hansa::Simulation::FHansaEconomicRegistry Registry;
	TArray<FHansaDefinitionValidationIssue> Issues;

	[[nodiscard]] bool IsValid() const
	{
		return !Issues.ContainsByPredicate([](const FHansaDefinitionValidationIssue& Issue)
		{
			return Issue.Severity == EHansaDefinitionValidationSeverity::Error;
		});
	}
};

/** Pure compiler from accepted Unreal definition assets to an immutable simulation registry. */
class HANSA_API FHansaEconomicDefinitionCompiler final
{
public:
	[[nodiscard]] static FHansaEconomicRegistryCompileResult Compile(
		const TArray<const UHansaDefinitionBase*>& Definitions);
};
