#pragma once

#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	enum class EHansaDomainEventType : uint8
	{
		TestEntityCreated = 0,
		TestEntityCancelled,
		NoOpCommandAccepted
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaDomainEventType Type);

	/** Immutable event published only after the enclosing tick transaction succeeds. */
	class HANSASIMULATION_API FHansaDomainEvent final
	{
	public:
		[[nodiscard]] EHansaDomainEventType GetType() const { return Type; }
		[[nodiscard]] uint64 GetGlobalSequence() const { return GlobalSequence; }
		[[nodiscard]] FHansaSimulationTick GetTick() const { return Tick; }
		[[nodiscard]] FHansaCommandId GetSourceCommandId() const { return SourceCommandId; }
		[[nodiscard]] FHansaHouseId GetIssuingHouseId() const { return IssuingHouseId; }
		[[nodiscard]] FHansaTestEntityId GetTestEntityId() const { return TestEntityId; }
		[[nodiscard]] int64 GetValue() const { return Value; }
		[[nodiscard]] FString ToDebugString() const;

	private:
		friend class FHansaSimulationPipeline;

		EHansaDomainEventType Type = EHansaDomainEventType::NoOpCommandAccepted;
		uint64 GlobalSequence = 0;
		FHansaSimulationTick Tick;
		FHansaCommandId SourceCommandId;
		FHansaHouseId IssuingHouseId;
		FHansaTestEntityId TestEntityId;
		int64 Value = 0;
	};
}
