#pragma once

#include "Model/HansaSimulationTime.h"

namespace Hansa::Game::PresentationClock
{
	inline constexpr uint8 LockedHour = 12;
	inline constexpr uint8 LockedMinute = 0;

	inline void LockToMidday(
		Hansa::Simulation::FHansaCalendarProjection& Calendar,
		double* TickFraction = nullptr)
	{
		Calendar.HourOfDay = LockedHour;
		Calendar.MinuteOfHour = LockedMinute;
		if (TickFraction != nullptr) *TickFraction = 0.0;
	}

	inline Hansa::Simulation::FHansaCalendarProjection AtMidday(
		const Hansa::Simulation::FHansaCalendarProjection& Calendar)
	{
		Hansa::Simulation::FHansaCalendarProjection Result = Calendar;
		LockToMidday(Result);
		return Result;
	}
}
