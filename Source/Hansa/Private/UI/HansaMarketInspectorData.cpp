#include "UI/HansaInspectorPresentationModel.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Population/HansaPopulation.h"
#include "Population/HansaConsumptionHistory.h"

#define LOCTEXT_NAMESPACE "HansaMarketInspectorData"

TArray<FHansaInspectorFlowPresentation> BuildMarketDemandFlows(
    TConstArrayView<Hansa::Simulation::FHansaPopulationCohortProjection> Cohorts,
    const Hansa::Simulation::FHansaEconomicRegistry& Registry, const FString& CityId,
    const Hansa::Simulation::FHansaConsumptionProjection& Consumption)
{
    using namespace Hansa::Simulation;
    TArray<FHansaInspectorFlowPresentation> Rows;
    for (const auto& Cohort : Cohorts)
    {
        if (Cohort.CityId.ToString() != CityId) continue;
        const auto* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
        if (!Tier) continue;
        for (const auto& Authored : Tier->Needs)
        {
            const auto* Definition = Registry.FindNeed(Authored.NeedId);
            if (!Definition || Definition->Kind != EHansaCompiledNeedKind::Good) continue;
            if (Cohort.Residents <= 0) continue;
            const FName GoodId(*Definition->GoodId);
            auto* Row = Rows.FindByPredicate([&](const auto& R) { return R.DemandGoodId == GoodId; });
            if (!Row)
            {
                FHansaInspectorFlowPresentation New;
                New.StableId = GoodId; New.DemandGoodId = GoodId; New.bDemandKnown = true;
                const auto* Good = Registry.FindGood(Definition->GoodId);
                New.Label = FText::FromString(Good ? Good->DisplayName : Definition->GoodId);
                Rows.Add(MoveTemp(New)); Row = &Rows.Last();
            }
        }
    }
    // Keep history even when the contributing residence has changed tier or been removed.
    for (const auto& Total : Consumption.Goods)
    {
        if (Total.CityId.ToString() != CityId) continue;
        const FName GoodId(*Total.GoodId.ToString());
        auto* Row = Rows.FindByPredicate([&](const auto& R) { return R.DemandGoodId == GoodId; });
        if (!Row)
        {
            FHansaInspectorFlowPresentation New;
            New.StableId = GoodId; New.DemandGoodId = GoodId;
            const auto* Good = Registry.FindGood(Total.GoodId.ToString());
            New.Label = FText::FromString(Good ? Good->DisplayName : Total.GoodId.ToString());
            Rows.Add(MoveTemp(New)); Row = &Rows.Last();
        }
        Row->DemandRequired = Total.Required;
        Row->DemandSupplied = Total.Consumed;
    }
    Rows.Sort([](const auto& A, const auto& B) { return A.DemandGoodId.LexicalLess(B.DemandGoodId); });
    FNumberFormattingOptions Amount; Amount.SetMaximumFractionalDigits(3);
    FNumberFormattingOptions Percent; Percent.SetMaximumFractionalDigits(1);
    for (auto& Row : Rows)
    {
        Row.bDemandKnown = Consumption.CoveredMinutes > 0;
        Row.DemandPeriod = FormatMarketConsumptionPeriod(Consumption);
        Row.bProblem = Row.bDemandKnown && Row.DemandSupplied < Row.DemandRequired;
        if (!Row.bDemandKnown)
        {
            Row.Value = LOCTEXT("Pending", "Awaiting population evaluation");
            Row.State = LOCTEXT("UnknownPercent", "—");
        }
        else if (Row.DemandRequired == 0)
        {
            Row.Value = LOCTEXT("NoDemand", "No demand in this period");
            Row.State = LOCTEXT("NoPercent", "—");
        }
        else
        {
            Row.State = FText::AsPercent(double(Row.DemandSupplied) / Row.DemandRequired, &Percent);
            Row.Value = FText::Format(LOCTEXT("ConsumedAmounts", "{0} / {1} units consumed"),
                FText::AsNumber(Row.DemandSupplied / 1000., &Amount), FText::AsNumber(Row.DemandRequired / 1000., &Amount));
        }
    }
    return Rows;
}
FText FormatMarketConsumptionPeriod(const Hansa::Simulation::FHansaConsumptionProjection& Consumption)
{
    if (Consumption.CoveredMinutes == 0) return LOCTEXT("AwaitingHistory", "Awaiting consumption history");
    if (Consumption.bFullWindow) return LOCTEXT("Last30Days", "Last 30 days");
    const int64 Days = Consumption.CoveredMinutes / 1440;
    const int64 Hours = (Consumption.CoveredMinutes % 1440) / 60;
    const int64 Minutes = Consumption.CoveredMinutes % 60;
    if (Days == 0 && Hours == 0)
        return FText::Format(LOCTEXT("FirstMinutes", "First {0} min recorded"), FText::AsNumber(Minutes));
    if (Days == 0 && Minutes == 0)
        return FText::Format(LOCTEXT("FirstHours", "First {0} h recorded"), FText::AsNumber(Hours));
    if (Hours == 0 && Minutes == 0)
        return FText::Format(LOCTEXT("FirstDays", "First {0} days recorded"), FText::AsNumber(Days));
    if (Minutes == 0)
        return FText::Format(LOCTEXT("FirstDaysHours", "First {0} d {1} h recorded"), FText::AsNumber(Days), FText::AsNumber(Hours));
    return FText::Format(LOCTEXT("FirstPartialDays", "First {0} d {1} h {2} min recorded"),
        FText::AsNumber(Days), FText::AsNumber(Hours), FText::AsNumber(Minutes));
}
#undef LOCTEXT_NAMESPACE
