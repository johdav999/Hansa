#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicRegistry.h"

/** One canonical registry row, retained so a hash mismatch can identify the responsible definition. */
struct HANSA_API FHansaEconomicDefinitionHashEvidence final
{
	FString DefinitionClassPath;
	FString StableId;
	uint64 ContentHash = 0;
};

struct HANSA_API FHansaEconomicRegistryCompileResult final
{
	Hansa::Simulation::FHansaEconomicRegistry Registry;
	TArray<FHansaDefinitionValidationIssue> Issues;
	TArray<FHansaEconomicDefinitionHashEvidence> DefinitionHashes;

	[[nodiscard]] bool IsValid() const
	{
		return !Issues.ContainsByPredicate([](const FHansaDefinitionValidationIssue& Issue)
		{
			return Issue.Severity == EHansaDefinitionValidationSeverity::Error;
		});
	}

	/** Compare against reviewed evidence and explain whether definitions changed, disappeared, or appeared. */
	[[nodiscard]] FString DescribeRegistryHashMismatch(
		uint64 ExpectedRegistryHash,
		const TArray<FHansaEconomicDefinitionHashEvidence>& ExpectedDefinitions) const;
};

/** Pure compiler from accepted Unreal definition assets to an immutable simulation registry. */
class HANSA_API FHansaEconomicDefinitionCompiler final
{
public:
	[[nodiscard]] static FHansaEconomicRegistryCompileResult Compile(
		const TArray<const UHansaDefinitionBase*>& Definitions);
};
