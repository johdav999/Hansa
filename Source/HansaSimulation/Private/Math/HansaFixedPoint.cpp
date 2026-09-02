#include "Math/HansaFixedPoint.h"

#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		struct FUnsignedProduct128
		{
			uint64 High = 0;
			uint64 Low = 0;
		};

		uint64 UnsignedMagnitude(const int64 Value)
		{
			return Value < 0
				? static_cast<uint64>(-(Value + 1)) + 1
				: static_cast<uint64>(Value);
		}

		FUnsignedProduct128 MultiplyUnsigned64(const uint64 Left, const uint64 Right)
		{
			constexpr uint64 Mask32 = 0xffffffffULL;
			const uint64 LeftLow = Left & Mask32;
			const uint64 LeftHigh = Left >> 32;
			const uint64 RightLow = Right & Mask32;
			const uint64 RightHigh = Right >> 32;

			const uint64 InitialLow = LeftLow * RightLow;
			const uint64 MiddleA = LeftHigh * RightLow + (InitialLow >> 32);
			const uint64 MiddleLow = MiddleA & Mask32;
			const uint64 MiddleHigh = MiddleA >> 32;
			const uint64 MiddleB = LeftLow * RightHigh + MiddleLow;

			FUnsignedProduct128 Product;
			Product.High = LeftHigh * RightHigh + MiddleHigh + (MiddleB >> 32);
			Product.Low = (MiddleB << 32) | (InitialLow & Mask32);
			return Product;
		}

		bool DivideUnsigned128By64(
			const FUnsignedProduct128 Dividend,
			const uint64 Divisor,
			uint64& OutQuotient,
			uint64& OutRemainder)
		{
			OutQuotient = 0;
			OutRemainder = 0;
			bool bQuotientOverflow = false;

			for (int32 BitIndex = 127; BitIndex >= 0; --BitIndex)
			{
				const uint64 InputBit = BitIndex >= 64
					? ((Dividend.High >> (BitIndex - 64)) & 1ULL)
					: ((Dividend.Low >> BitIndex) & 1ULL);
				const bool bRemainderHighBit = (OutRemainder & (1ULL << 63)) != 0;
				OutRemainder = (OutRemainder << 1) | InputBit;

				if (bRemainderHighBit || OutRemainder >= Divisor)
				{
					OutRemainder -= Divisor;
					if (BitIndex >= 64)
					{
						bQuotientOverflow = true;
					}
					else
					{
						OutQuotient |= 1ULL << BitIndex;
					}
				}
			}

			return !bQuotientOverflow;
		}

		bool ShouldRoundMagnitudeUp(
			const bool bNegative,
			const uint64 Remainder,
			const uint64 Divisor,
			const EHansaRoundingMode RoundingMode)
		{
			if (Remainder == 0)
			{
				return false;
			}

			switch (RoundingMode)
			{
			case EHansaRoundingMode::TowardZero:
				return false;
			case EHansaRoundingMode::Floor:
				return bNegative;
			case EHansaRoundingMode::Ceiling:
				return !bNegative;
			case EHansaRoundingMode::HalfAwayFromZero:
				return Remainder >= Divisor - Remainder;
			default:
				return false;
			}
		}
	}

	THansaValueResult<int64> FHansaCheckedIntegerMath::TryAdd(const int64 Left, const int64 Right)
	{
		if ((Right > 0 && Left > TNumericLimits<int64>::Max() - Right) ||
			(Right < 0 && Left < TNumericLimits<int64>::Lowest() - Right))
		{
			return THansaValueResult<int64>::Failure(EHansaValueError::Overflow);
		}
		return THansaValueResult<int64>::Success(Left + Right);
	}

	THansaValueResult<int64> FHansaCheckedIntegerMath::TrySubtract(const int64 Left, const int64 Right)
	{
		if ((Right < 0 && Left > TNumericLimits<int64>::Max() + Right) ||
			(Right > 0 && Left < TNumericLimits<int64>::Lowest() + Right))
		{
			return THansaValueResult<int64>::Failure(EHansaValueError::Overflow);
		}
		return THansaValueResult<int64>::Success(Left - Right);
	}

	THansaValueResult<int64> FHansaCheckedIntegerMath::TryMultiplyDivide(
		const int64 Multiplicand,
		const int64 Multiplier,
		const int64 Divisor,
		const EHansaRoundingMode RoundingMode)
	{
		if (Divisor == 0)
		{
			return THansaValueResult<int64>::Failure(EHansaValueError::DivisionByZero);
		}

		const bool bNegative = (Multiplicand < 0) ^ (Multiplier < 0) ^ (Divisor < 0);
		const FUnsignedProduct128 Product = MultiplyUnsigned64(
			UnsignedMagnitude(Multiplicand),
			UnsignedMagnitude(Multiplier));
		const uint64 DivisorMagnitude = UnsignedMagnitude(Divisor);

		uint64 Quotient = 0;
		uint64 Remainder = 0;
		if (!DivideUnsigned128By64(Product, DivisorMagnitude, Quotient, Remainder))
		{
			return THansaValueResult<int64>::Failure(EHansaValueError::Overflow);
		}

		if (ShouldRoundMagnitudeUp(bNegative, Remainder, DivisorMagnitude, RoundingMode))
		{
			if (Quotient == TNumericLimits<uint64>::Max())
			{
				return THansaValueResult<int64>::Failure(EHansaValueError::Overflow);
			}
			++Quotient;
		}

		constexpr uint64 NegativeLimit = 1ULL << 63;
		const uint64 Limit = bNegative ? NegativeLimit : static_cast<uint64>(TNumericLimits<int64>::Max());
		if (Quotient > Limit)
		{
			return THansaValueResult<int64>::Failure(EHansaValueError::Overflow);
		}

		if (bNegative)
		{
			return THansaValueResult<int64>::Success(
				Quotient == NegativeLimit
					? TNumericLimits<int64>::Lowest()
					: -static_cast<int64>(Quotient));
		}
		return THansaValueResult<int64>::Success(static_cast<int64>(Quotient));
	}

	THansaValueResult<FHansaRate> FHansaRate::TryMakeNormalized(const int64 PartsPerMillion)
	{
		if (PartsPerMillion < 0 || PartsPerMillion > Scale)
		{
			return THansaValueResult<FHansaRate>::Failure(EHansaValueError::OutOfRange);
		}
		return THansaValueResult<FHansaRate>::Success(FHansaRate(PartsPerMillion));
	}

	THansaValueResult<FHansaRate> FHansaRate::TryRatio(
		const int64 Numerator,
		const int64 Denominator,
		const EHansaRoundingMode RoundingMode)
	{
		const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryMultiplyDivide(
			Numerator,
			Scale,
			Denominator,
			RoundingMode);
		return Result
			? THansaValueResult<FHansaRate>::Success(FHansaRate(Result.Value))
			: THansaValueResult<FHansaRate>::Failure(Result.Error);
	}

	FString FHansaRate::ToDebugString() const
	{
		return FString::Printf(TEXT("Rate[ppm=%lld]"), static_cast<long long>(Value));
	}
}
