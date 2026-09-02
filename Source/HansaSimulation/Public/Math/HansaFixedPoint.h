#pragma once

#include "Containers/UnrealString.h"
#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	enum class EHansaRoundingMode : uint8
	{
		TowardZero = 0,
		Floor,
		Ceiling,
		HalfAwayFromZero
	};

	class HANSASIMULATION_API FHansaCheckedIntegerMath final
	{
	public:
		static THansaValueResult<int64> TryAdd(int64 Left, int64 Right);
		static THansaValueResult<int64> TrySubtract(int64 Left, int64 Right);
		static THansaValueResult<int64> TryMultiplyDivide(
			int64 Multiplicand,
			int64 Multiplier,
			int64 Divisor,
			EHansaRoundingMode RoundingMode);
	};

	class HANSASIMULATION_API FHansaRate final
	{
	public:
		static constexpr int64 Scale = 1'000'000;
		static constexpr uint8 SerializationTag = 62;

		FHansaRate() = default;

		static FHansaRate FromPartsPerMillion(const int64 PartsPerMillion)
		{
			return FHansaRate(PartsPerMillion);
		}

		static THansaValueResult<FHansaRate> TryMakeNormalized(int64 PartsPerMillion);
		static THansaValueResult<FHansaRate> TryRatio(
			int64 Numerator,
			int64 Denominator,
			EHansaRoundingMode RoundingMode = EHansaRoundingMode::HalfAwayFromZero);

		[[nodiscard]] int64 GetPartsPerMillion() const { return Value; }
		[[nodiscard]] bool IsNormalized() const { return Value >= 0 && Value <= Scale; }
		[[nodiscard]] FString ToDebugString() const;

		friend bool operator==(const FHansaRate& Left, const FHansaRate& Right) { return Left.Value == Right.Value; }
		friend bool operator!=(const FHansaRate& Left, const FHansaRate& Right) { return !(Left == Right); }
		friend bool operator<(const FHansaRate& Left, const FHansaRate& Right) { return Left.Value < Right.Value; }

	private:
		explicit FHansaRate(const int64 InValue)
			: Value(InValue)
		{
		}

		int64 Value = 0;
	};

	struct FHansaMoneyTraits final
	{
		static const TCHAR* DebugName() { return TEXT("Money"); }
		static const TCHAR* UnitName() { return TEXT("pfennig"); }
		static constexpr uint8 SerializationTag = 60;
	};

	struct FHansaQuantityTraits final
	{
		static const TCHAR* DebugName() { return TEXT("Quantity"); }
		static const TCHAR* UnitName() { return TEXT("milli-unit"); }
		static constexpr uint8 SerializationTag = 61;
	};

	template <typename TTraits>
	class THansaSignedUnit final
	{
	public:
		THansaSignedUnit() = default;

		static THansaSignedUnit FromRaw(const int64 InValue)
		{
			return THansaSignedUnit(InValue);
		}

		static THansaValueResult<THansaSignedUnit> TryAdd(const THansaSignedUnit Left, const THansaSignedUnit Right)
		{
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryAdd(Left.Value, Right.Value);
			return Result
				? THansaValueResult<THansaSignedUnit>::Success(THansaSignedUnit(Result.Value))
				: THansaValueResult<THansaSignedUnit>::Failure(Result.Error);
		}

		static THansaValueResult<THansaSignedUnit> TrySubtract(const THansaSignedUnit Left, const THansaSignedUnit Right)
		{
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TrySubtract(Left.Value, Right.Value);
			return Result
				? THansaValueResult<THansaSignedUnit>::Success(THansaSignedUnit(Result.Value))
				: THansaValueResult<THansaSignedUnit>::Failure(Result.Error);
		}

		THansaValueResult<THansaSignedUnit> TryScale(
			const FHansaRate Rate,
			const EHansaRoundingMode RoundingMode = EHansaRoundingMode::HalfAwayFromZero) const
		{
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Value,
				Rate.GetPartsPerMillion(),
				FHansaRate::Scale,
				RoundingMode);
			return Result
				? THansaValueResult<THansaSignedUnit>::Success(THansaSignedUnit(Result.Value))
				: THansaValueResult<THansaSignedUnit>::Failure(Result.Error);
		}

		[[nodiscard]] int64 GetRawValue() const { return Value; }
		[[nodiscard]] static constexpr uint8 GetSerializationTag() { return TTraits::SerializationTag; }
		[[nodiscard]] FString ToDebugString() const
		{
			return FString::Printf(TEXT("%s[%s=%lld]"), TTraits::DebugName(), TTraits::UnitName(), static_cast<long long>(Value));
		}

		friend bool operator==(const THansaSignedUnit& Left, const THansaSignedUnit& Right) { return Left.Value == Right.Value; }
		friend bool operator!=(const THansaSignedUnit& Left, const THansaSignedUnit& Right) { return !(Left == Right); }
		friend bool operator<(const THansaSignedUnit& Left, const THansaSignedUnit& Right) { return Left.Value < Right.Value; }

	private:
		explicit THansaSignedUnit(const int64 InValue)
			: Value(InValue)
		{
		}

		int64 Value = 0;
	};

	using FHansaMoney = THansaSignedUnit<FHansaMoneyTraits>;
	using FHansaQuantity = THansaSignedUnit<FHansaQuantityTraits>;
}
