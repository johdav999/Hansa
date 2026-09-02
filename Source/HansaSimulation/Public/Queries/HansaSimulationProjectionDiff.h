#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Simulation
{
	enum class EHansaProjectionDiffField : uint8
	{
		Tick = 0,
		ProcessedCommandCount,
		PublishedDomainEventCount,
		CityCount,
		BuildingCount,
		VehicleCount,
		RouteCount,
		TestEntityCount,
		HouseMoney,
		Fingerprint
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaProjectionDiffField Field);

	struct FHansaProjectionDiffEntry
	{
		EHansaProjectionDiffField Field = EHansaProjectionDiffField::Tick;
		FString StableKey;
		FString BeforeValue;
		FString AfterValue;
	};

	class HANSASIMULATION_API FHansaSimulationProjectionDiff final
	{
	public:
		[[nodiscard]] static FHansaSimulationProjectionDiff Compare(
			const FHansaSimulationProjection& Before,
			const FHansaSimulationProjection& After);

		[[nodiscard]] bool IsEmpty() const { return Entries.IsEmpty(); }
		[[nodiscard]] TConstArrayView<FHansaProjectionDiffEntry> GetEntries() const { return Entries; }
		[[nodiscard]] FString ToCompactDebugString(int32 MaximumEntries = 8) const;

	private:
		TArray<FHansaProjectionDiffEntry> Entries;
	};
}
