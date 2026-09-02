#include "Model/HansaSimulationTime.h"

#include "Math/HansaFixedPoint.h"

namespace Hansa::Simulation
{
	THansaValueResult<FHansaSimulationVersion> FHansaSimulationVersion::TryCreate(const uint32 Value)
	{
		return Value == 0
			? THansaValueResult<FHansaSimulationVersion>::Failure(EHansaValueError::InvalidZero)
			: THansaValueResult<FHansaSimulationVersion>::Success(FHansaSimulationVersion(Value));
	}

	THansaValueResult<FHansaSimulationTick> FHansaSimulationTick::TryCreate(const int64 Value)
	{
		return Value < 0
			? THansaValueResult<FHansaSimulationTick>::Failure(EHansaValueError::NegativeNotAllowed)
			: THansaValueResult<FHansaSimulationTick>::Success(FHansaSimulationTick(Value));
	}

	THansaValueResult<FHansaSimulationDuration> FHansaSimulationDuration::TryCreate(const int64 Ticks)
	{
		return Ticks < 0
			? THansaValueResult<FHansaSimulationDuration>::Failure(EHansaValueError::NegativeNotAllowed)
			: THansaValueResult<FHansaSimulationDuration>::Success(FHansaSimulationDuration(Ticks));
	}

	THansaValueResult<FHansaSimulationClock> FHansaSimulationClock::TryCreate(
		const FHansaSimulationVersion Version,
		const FHansaSimulationTick Tick,
		const uint16 MinutesPerTick)
	{
		if (!Version.IsValid())
		{
			return THansaValueResult<FHansaSimulationClock>::Failure(EHansaValueError::InvalidZero);
		}
		if (Version.GetValue() != CurrentSimulationVersion)
		{
			return THansaValueResult<FHansaSimulationClock>::Failure(EHansaValueError::UnsupportedVersion);
		}
		if (MinutesPerTick == 0 || MinutesPerTick > 24 * 60)
		{
			return THansaValueResult<FHansaSimulationClock>::Failure(EHansaValueError::OutOfRange);
		}
		return THansaValueResult<FHansaSimulationClock>::Success(FHansaSimulationClock(Version, Tick, MinutesPerTick));
	}

	THansaValueResult<FHansaSimulationClock> FHansaSimulationClock::TryAdvance(const FHansaSimulationDuration Duration) const
	{
		const THansaValueResult<int64> Advanced = FHansaCheckedIntegerMath::TryAdd(Tick.GetValue(), Duration.GetTicks());
		if (!Advanced)
		{
			return THansaValueResult<FHansaSimulationClock>::Failure(Advanced.Error);
		}

		const THansaValueResult<FHansaSimulationTick> AdvancedTick = FHansaSimulationTick::TryCreate(Advanced.Value);
		return AdvancedTick
			? THansaValueResult<FHansaSimulationClock>::Success(FHansaSimulationClock(Version, AdvancedTick.Value, MinutesPerTick))
			: THansaValueResult<FHansaSimulationClock>::Failure(AdvancedTick.Error);
	}

	THansaValueResult<FHansaCalendarProjection> FHansaSimulationClock::TryProjectCalendar() const
	{
		const THansaValueResult<int64> TotalMinutes = FHansaCheckedIntegerMath::TryMultiplyDivide(
			Tick.GetValue(),
			MinutesPerTick,
			1,
			EHansaRoundingMode::TowardZero);
		if (!TotalMinutes)
		{
			return THansaValueResult<FHansaCalendarProjection>::Failure(TotalMinutes.Error);
		}

		constexpr int64 MinutesPerDay = 24 * 60;
		FHansaCalendarProjection Projection;
		Projection.ElapsedDays = TotalMinutes.Value / MinutesPerDay;
		const int64 MinuteOfDay = TotalMinutes.Value % MinutesPerDay;
		Projection.HourOfDay = static_cast<uint8>(MinuteOfDay / 60);
		Projection.MinuteOfHour = static_cast<uint8>(MinuteOfDay % 60);
		return THansaValueResult<FHansaCalendarProjection>::Success(Projection);
	}

	FString FHansaSimulationClock::ToDebugString() const
	{
		return FString::Printf(
			TEXT("Clock[simulationVersion=%u;tick=%lld;minutesPerTick=%u]"),
			Version.GetValue(),
			static_cast<long long>(Tick.GetValue()),
			MinutesPerTick);
	}
}
