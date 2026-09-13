#include "Population/HansaConsumptionHistory.h"
#include "Population/HansaPopulation.h"

namespace Hansa::Simulation
{
    namespace
    {
        bool Less(const FHansaConsumptionTotal& A, const FHansaConsumptionTotal& B)
        {
            return A.CityId == B.CityId ? A.GoodId < B.GoodId : A.CityId < B.CityId;
        }

        bool Add(TArray<FHansaConsumptionTotal>& Totals, const FHansaConsumptionTotal& Value)
        {
            if (!Value.CityId.IsValid() || !Value.GoodId.IsValid() || Value.Required < 0 ||
                Value.Consumed < 0 || Value.Consumed > Value.Required) return false;
            auto* Total = Totals.FindByPredicate([&](const auto& T)
                { return T.CityId == Value.CityId && T.GoodId == Value.GoodId; });
            if (!Total) { Totals.Add(Value); return true; }
            if (Total->Required > MAX_int64 - Value.Required || Total->Consumed > MAX_int64 - Value.Consumed) return false;
            Total->Required += Value.Required;
            Total->Consumed += Value.Consumed;
            return true;
        }
    }

    bool FHansaConsumptionHistory::Record(TConstArrayView<FHansaPopulationCohortState> Cohorts,
        const FHansaSimulationClock& Clock)
    {
        const int64 Tick = Clock.GetTick().GetValue();
        if (Tick <= 0 || (!Samples.IsEmpty() && Samples.Last().EndTick + 1 != Tick)) return false;
        FHansaConsumptionSample Sample;
        Sample.EndTick = Tick;
        for (const auto& Cohort : Cohorts)
            for (const auto& Need : Cohort.Needs)
                if (Need.GoodId.IsValid() && !Add(Sample.Goods,
                    {Cohort.CityId, Need.GoodId, Need.RequiredLastTick.GetRawValue(), Need.ConsumedLastTick.GetRawValue()})) return false;
        Sample.Goods.Sort(Less);
        const int32 Capacity = WindowTicks(Clock.GetMinutesPerTick());
        // Check the complete retained sum before mutating, including int64 overflow.
        TArray<FHansaConsumptionTotal> Totals = Sample.Goods;
        const int32 First = FMath::Max(0, Samples.Num() + 1 - Capacity);
        for (int32 I = First; I < Samples.Num(); ++I)
            for (const auto& Good : Samples[I].Goods) if (!Add(Totals, Good)) return false;
        if (First > 0) Samples.RemoveAt(0, First);
        Samples.Add(MoveTemp(Sample));
        return true;
    }

    bool FHansaConsumptionHistory::Validate(const FHansaSimulationClock& Clock) const
    {
        if (Samples.Num() > WindowTicks(Clock.GetMinutesPerTick())) return false;
        if (!Samples.IsEmpty() && Samples.Last().EndTick != Clock.GetTick().GetValue()) return false;
        TArray<FHansaConsumptionTotal> Totals;
        int64 PreviousTick = Samples.IsEmpty() ? 0 : Samples[0].EndTick - 1;
        for (const auto& Sample : Samples)
        {
            if (Sample.EndTick <= 0 || Sample.EndTick != PreviousTick + 1) return false;
            PreviousTick = Sample.EndTick;
            for (int32 I = 0; I < Sample.Goods.Num(); ++I)
                if ((I > 0 && !Less(Sample.Goods[I - 1], Sample.Goods[I])) || !Add(Totals, Sample.Goods[I])) return false;
        }
        return true;
    }

    TArray<FHansaConsumptionTotal> FHansaConsumptionHistory::RecentTotals(const int32 TickCount) const
    {
        TArray<FHansaConsumptionTotal> Totals;
        for (int32 Index = FMath::Max(0, Samples.Num() - FMath::Max(0, TickCount)); Index < Samples.Num(); ++Index)
            for (const auto& Good : Samples[Index].Goods)
                if (!Add(Totals, Good)) return {};
        Totals.Sort(Less);
        return Totals;
    }

    FHansaConsumptionProjection FHansaConsumptionHistory::Project(const FHansaSimulationClock& Clock) const
    {
        FHansaConsumptionProjection Result;
        Result.CoveredMinutes = int64(Samples.Num()) * Clock.GetMinutesPerTick();
        Result.bFullWindow = Samples.Num() == WindowTicks(Clock.GetMinutesPerTick());
        for (const auto& Sample : Samples)
            for (const auto& Good : Sample.Goods)
                if (!Add(Result.Goods, Good)) return {}; // Invalid data must never appear as a known percentage.
        Result.Goods.Sort(Less);
        return Result;
    }
}
