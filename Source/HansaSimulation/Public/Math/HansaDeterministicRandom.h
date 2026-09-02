#pragma once

#include "Containers/UnrealString.h"
#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	enum class EHansaRandomAlgorithm : uint8
	{
		SplitMix64V1 = 1
	};

	class HANSASIMULATION_API FHansaRandomStream final
	{
	public:
		static constexpr uint8 SerializationTag = 71;

		FHansaRandomStream() = default;

		static THansaValueResult<FHansaRandomStream> TryCreate(uint64 CampaignSeed, const FString& StreamName);
		static THansaValueResult<FHansaRandomStream> TryRestore(
			const FString& StreamName,
			EHansaRandomAlgorithm Algorithm,
			uint64 State,
			uint64 DrawCount);

		uint64 NextUInt64();
		uint32 NextUInt32();
		THansaValueResult<uint32> TryNextBounded(uint32 UpperExclusive);

		[[nodiscard]] const FString& GetName() const { return Name; }
		[[nodiscard]] EHansaRandomAlgorithm GetAlgorithm() const { return Algorithm; }
		[[nodiscard]] uint64 GetState() const { return State; }
		[[nodiscard]] uint64 GetDrawCount() const { return DrawCount; }
		[[nodiscard]] FString ToDebugString() const;

		friend bool operator==(const FHansaRandomStream& Left, const FHansaRandomStream& Right)
		{
			return Left.Name == Right.Name &&
				Left.Algorithm == Right.Algorithm &&
				Left.State == Right.State &&
				Left.DrawCount == Right.DrawCount;
		}

	private:
		FHansaRandomStream(
			const FString& InName,
			const EHansaRandomAlgorithm InAlgorithm,
			const uint64 InState,
			const uint64 InDrawCount)
			: Name(InName)
			, Algorithm(InAlgorithm)
			, State(InState)
			, DrawCount(InDrawCount)
		{
		}

		static bool IsValidStreamName(const FString& StreamName);
		static uint64 HashStreamName(const FString& StreamName);

		FString Name;
		EHansaRandomAlgorithm Algorithm = EHansaRandomAlgorithm::SplitMix64V1;
		uint64 State = 0;
		uint64 DrawCount = 0;
	};
}
