#include "Math/HansaDeterministicRandom.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint64 SplitMixIncrement = 0x9e3779b97f4a7c15ULL;
		constexpr uint64 FnvOffsetBasis = 14695981039346656037ULL;
		constexpr uint64 FnvPrime = 1099511628211ULL;

		bool IsAsciiLetter(const TCHAR Character)
		{
			return (Character >= TEXT('A') && Character <= TEXT('Z')) ||
				(Character >= TEXT('a') && Character <= TEXT('z'));
		}

		bool IsAsciiDigit(const TCHAR Character)
		{
			return Character >= TEXT('0') && Character <= TEXT('9');
		}
	}

	THansaValueResult<FHansaRandomStream> FHansaRandomStream::TryCreate(
		const uint64 CampaignSeed,
		const FString& StreamName)
	{
		if (!IsValidStreamName(StreamName))
		{
			return THansaValueResult<FHansaRandomStream>::Failure(EHansaValueError::InvalidFormat);
		}

		const uint64 InitialState = CampaignSeed ^ HashStreamName(StreamName);
		return THansaValueResult<FHansaRandomStream>::Success(FHansaRandomStream(
			StreamName,
			EHansaRandomAlgorithm::SplitMix64V1,
			InitialState,
			0));
	}

	THansaValueResult<FHansaRandomStream> FHansaRandomStream::TryRestore(
		const FString& StreamName,
		const EHansaRandomAlgorithm Algorithm,
		const uint64 State,
		const uint64 DrawCount)
	{
		if (!IsValidStreamName(StreamName))
		{
			return THansaValueResult<FHansaRandomStream>::Failure(EHansaValueError::InvalidFormat);
		}
		if (Algorithm != EHansaRandomAlgorithm::SplitMix64V1)
		{
			return THansaValueResult<FHansaRandomStream>::Failure(EHansaValueError::UnsupportedVersion);
		}
		return THansaValueResult<FHansaRandomStream>::Success(FHansaRandomStream(StreamName, Algorithm, State, DrawCount));
	}

	uint64 FHansaRandomStream::NextUInt64()
	{
		State += SplitMixIncrement;
		uint64 Value = State;
		Value = (Value ^ (Value >> 30)) * 0xbf58476d1ce4e5b9ULL;
		Value = (Value ^ (Value >> 27)) * 0x94d049bb133111ebULL;
		++DrawCount;
		return Value ^ (Value >> 31);
	}

	uint32 FHansaRandomStream::NextUInt32()
	{
		return static_cast<uint32>(NextUInt64() >> 32);
	}

	THansaValueResult<uint32> FHansaRandomStream::TryNextBounded(const uint32 UpperExclusive)
	{
		if (UpperExclusive == 0)
		{
			return THansaValueResult<uint32>::Failure(EHansaValueError::OutOfRange);
		}

		const uint32 RejectionThreshold = static_cast<uint32>(0U - UpperExclusive) % UpperExclusive;
		uint32 Value = 0;
		do
		{
			Value = NextUInt32();
		}
		while (Value < RejectionThreshold);

		return THansaValueResult<uint32>::Success(Value % UpperExclusive);
	}

	FString FHansaRandomStream::ToDebugString() const
	{
		return FString::Printf(
			TEXT("RandomStream[name=%s;algorithm=%u;draws=%llu;state=%016llX]"),
			*Name,
			static_cast<uint8>(Algorithm),
			static_cast<unsigned long long>(DrawCount),
			static_cast<unsigned long long>(State));
	}

	bool FHansaRandomStream::IsValidStreamName(const FString& StreamName)
	{
		if (StreamName.IsEmpty() || StreamName.StartsWith(TEXT(".")) || StreamName.EndsWith(TEXT(".")) || StreamName.Contains(TEXT("..")))
		{
			return false;
		}

		bool bAtSegmentStart = true;
		for (const TCHAR Character : StreamName)
		{
			if (Character == TEXT('.'))
			{
				bAtSegmentStart = true;
				continue;
			}
			if ((bAtSegmentStart && !IsAsciiLetter(Character)) ||
				(!bAtSegmentStart && !IsAsciiLetter(Character) && !IsAsciiDigit(Character)))
			{
				return false;
			}
			bAtSegmentStart = false;
		}
		return !bAtSegmentStart;
	}

	uint64 FHansaRandomStream::HashStreamName(const FString& StreamName)
	{
		uint64 Hash = FnvOffsetBasis;
		for (const TCHAR Character : StreamName)
		{
			Hash ^= static_cast<uint8>(Character);
			Hash *= FnvPrime;
		}
		return Hash;
	}
}
