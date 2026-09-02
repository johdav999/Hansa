#pragma once

#include "Diagnostics/HansaDeterminismTrace.h"
#include "Fixtures/HansaDeterministicFixture.h"

namespace Hansa::Simulation
{
	class HANSASIMULATION_API FHansaDeterminismEvidenceWriter final
	{
	public:
		static constexpr uint32 CurrentEvidenceSchemaVersion = 1;

		[[nodiscard]] static FString WriteRunJson(const FHansaFixtureRunResult& Run);
		[[nodiscard]] static FString WriteComparisonJson(
			const FString& FixtureId,
			const FHansaDeterminismComparison& Comparison);
	};
}
