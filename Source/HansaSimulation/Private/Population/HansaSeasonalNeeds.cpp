#include "Population/HansaSeasonalNeeds.h"

namespace Hansa::Simulation
{
	int32 FHansaSeasonalNeeds::Multiplier(const FHansaCompiledNeedDefinition& Need,
		const FHansaSimulationTick Tick, const uint32 MinutesPerTick)
	{
		if (!Need.bSeasonal) return 10000;
		if (Need.SeasonDays <= 0 || Need.SeasonMultipliers.Num() != 4 || MinutesPerTick == 0) return 0;
		// Reduce ticks modulo the year before multiplication to avoid overflow on long saves.
		const int64 YearMinutes = static_cast<int64>(Need.SeasonDays) * 4 * 1440;
		const int64 Minute = ((Tick.GetValue() % YearMinutes) * MinutesPerTick) % YearMinutes;
		const int32 Season = Need.FixedSeason >= 0 ? Need.FixedSeason : static_cast<int32>(Minute / (Need.SeasonDays * 1440LL));
		return Need.SeasonMultipliers.IsValidIndex(Season) ? Need.SeasonMultipliers[Season] : 0;
	}

	int64 FHansaSeasonalNeeds::Demand(const FHansaCompiledNeedDefinition& Need, const int32 Rate,
		const int32 Residents, const FHansaSimulationTick Tick, const uint32 MinutesPerTick)
	{
		if (Rate <= 0 || Residents <= 0) return 0;
		const int64 Raw = static_cast<int64>(Rate) * Residents;
		const int32 Factor = Multiplier(Need, Tick, MinutesPerTick);
		// Split multiplication and distribute fractional milli-units over a deterministic
		// 10000-tick phase. No floating point, persistent rounding loss, or hidden save state.
		const int64 Whole = (Raw / 10000) * Factor;
		const int64 Fraction = (Raw % 10000) * Factor;
		const int64 Remainder = Fraction % 10000;
		const int64 Phase = Tick.GetValue() % 10000;
		return Whole + Fraction / 10000 + ((Phase + 1) * Remainder / 10000 - Phase * Remainder / 10000);
	}
}
