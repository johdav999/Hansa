#pragma once

#include "Containers/UnrealString.h"
#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	class HANSASIMULATION_API FHansaSimulationVersion final
	{
	public:
		static constexpr uint8 SerializationTag = 67;

		FHansaSimulationVersion() = default;

		static THansaValueResult<FHansaSimulationVersion> TryCreate(uint32 Value);

		[[nodiscard]] bool IsValid() const { return Value != 0; }
		[[nodiscard]] uint32 GetValue() const { return Value; }

		friend bool operator==(const FHansaSimulationVersion& Left, const FHansaSimulationVersion& Right) { return Left.Value == Right.Value; }
		friend bool operator!=(const FHansaSimulationVersion& Left, const FHansaSimulationVersion& Right) { return !(Left == Right); }
		friend bool operator<(const FHansaSimulationVersion& Left, const FHansaSimulationVersion& Right) { return Left.Value < Right.Value; }

	private:
		explicit FHansaSimulationVersion(const uint32 InValue)
			: Value(InValue)
		{
		}

		uint32 Value = 0;
	};

	class HANSASIMULATION_API FHansaSimulationTick final
	{
	public:
		static constexpr uint8 SerializationTag = 68;

		FHansaSimulationTick() = default;

		static THansaValueResult<FHansaSimulationTick> TryCreate(int64 Value);

		[[nodiscard]] int64 GetValue() const { return Value; }

		friend bool operator==(const FHansaSimulationTick& Left, const FHansaSimulationTick& Right) { return Left.Value == Right.Value; }
		friend bool operator!=(const FHansaSimulationTick& Left, const FHansaSimulationTick& Right) { return !(Left == Right); }
		friend bool operator<(const FHansaSimulationTick& Left, const FHansaSimulationTick& Right) { return Left.Value < Right.Value; }

	private:
		explicit FHansaSimulationTick(const int64 InValue)
			: Value(InValue)
		{
		}

		int64 Value = 0;
	};

	class HANSASIMULATION_API FHansaSimulationDuration final
	{
	public:
		static constexpr uint8 SerializationTag = 69;

		FHansaSimulationDuration() = default;

		static THansaValueResult<FHansaSimulationDuration> TryCreate(int64 Ticks);

		[[nodiscard]] int64 GetTicks() const { return Ticks; }

		friend bool operator==(const FHansaSimulationDuration& Left, const FHansaSimulationDuration& Right) { return Left.Ticks == Right.Ticks; }
		friend bool operator!=(const FHansaSimulationDuration& Left, const FHansaSimulationDuration& Right) { return !(Left == Right); }
		friend bool operator<(const FHansaSimulationDuration& Left, const FHansaSimulationDuration& Right) { return Left.Ticks < Right.Ticks; }

	private:
		explicit FHansaSimulationDuration(const int64 InTicks)
			: Ticks(InTicks)
		{
		}

		int64 Ticks = 0;
	};

	struct FHansaCalendarProjection
	{
		int64 ElapsedDays = 0;
		uint8 HourOfDay = 0;
		uint8 MinuteOfHour = 0;

		friend bool operator==(const FHansaCalendarProjection& Left, const FHansaCalendarProjection& Right)
		{
			return Left.ElapsedDays == Right.ElapsedDays &&
				Left.HourOfDay == Right.HourOfDay &&
				Left.MinuteOfHour == Right.MinuteOfHour;
		}
	};

	class HANSASIMULATION_API FHansaSimulationClock final
	{
	public:
		static constexpr uint32 CurrentSimulationVersion = 1;
		static constexpr uint16 DefaultMinutesPerTick = 60;
		static constexpr uint8 SerializationTag = 70;

		FHansaSimulationClock() = default;

		static THansaValueResult<FHansaSimulationClock> TryCreate(
			FHansaSimulationVersion Version,
			FHansaSimulationTick Tick,
			uint16 MinutesPerTick = DefaultMinutesPerTick);

		THansaValueResult<FHansaSimulationClock> TryAdvance(FHansaSimulationDuration Duration) const;
		THansaValueResult<FHansaCalendarProjection> TryProjectCalendar() const;

		[[nodiscard]] const FHansaSimulationVersion& GetVersion() const { return Version; }
		[[nodiscard]] const FHansaSimulationTick& GetTick() const { return Tick; }
		[[nodiscard]] uint16 GetMinutesPerTick() const { return MinutesPerTick; }
		[[nodiscard]] FString ToDebugString() const;

		friend bool operator==(const FHansaSimulationClock& Left, const FHansaSimulationClock& Right)
		{
			return Left.Version == Right.Version && Left.Tick == Right.Tick && Left.MinutesPerTick == Right.MinutesPerTick;
		}

	private:
		FHansaSimulationClock(
			const FHansaSimulationVersion InVersion,
			const FHansaSimulationTick InTick,
			const uint16 InMinutesPerTick)
			: Version(InVersion)
			, Tick(InTick)
			, MinutesPerTick(InMinutesPerTick)
		{
		}

		FHansaSimulationVersion Version;
		FHansaSimulationTick Tick;
		uint16 MinutesPerTick = DefaultMinutesPerTick;
	};
}
