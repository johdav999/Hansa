#include "UI/HansaMarketTablePresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"

#define LOCTEXT_NAMESPACE "HansaMarketTablePresentationModel"

namespace
{
	struct FGoodDescriptor
	{
		const TCHAR* StableId;
		const TCHAR* Label;
		const TCHAR* Glyph;
		EHansaMarketGoodCategory Category;
	};

	const FGoodDescriptor CanonicalGoods[] = {
		{ TEXT("Good.Grain"), TEXT("Grain"), TEXT(""), EHansaMarketGoodCategory::Food },
		{ TEXT("Good.Flour"), TEXT("Flour"), TEXT(""), EHansaMarketGoodCategory::Food },
		{ TEXT("Good.Hops"), TEXT("Hops"), TEXT(""), EHansaMarketGoodCategory::Food },
		{ TEXT("Good.Malt"), TEXT("Malt"), TEXT(""), EHansaMarketGoodCategory::Manufactured },
		{ TEXT("Good.Bread"), TEXT("Bread"), TEXT(""), EHansaMarketGoodCategory::Food },
		{ TEXT("Good.Fish"), TEXT("Fish"), TEXT(""), EHansaMarketGoodCategory::Food },
		{ TEXT("Good.Salt"), TEXT("Salt"), TEXT(""), EHansaMarketGoodCategory::Food },
		{ TEXT("Good.Timber"), TEXT("Timber"), TEXT(""), EHansaMarketGoodCategory::Material },
		{ TEXT("Good.Planks"), TEXT("Planks"), TEXT(""), EHansaMarketGoodCategory::Material },
		{ TEXT("Good.Iron"), TEXT("Iron"), TEXT(""), EHansaMarketGoodCategory::Material },
		{ TEXT("Good.Tools"), TEXT("Tools"), TEXT(""), EHansaMarketGoodCategory::Manufactured },
		{ TEXT("Good.Barrels"), TEXT("Barrels"), TEXT(""), EHansaMarketGoodCategory::Manufactured },
		{ TEXT("Good.Beer"), TEXT("Beer"), TEXT(""), EHansaMarketGoodCategory::Manufactured }
	};

	bool TextEqual(const FText& Left, const FText& Right) { return Left.EqualTo(Right); }

	FText Quantity(const int64 MilliUnits)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), static_cast<double>(MilliUnits) / 1000.0));
	}

	FText Money(const int64 MilliMarks, const bool bEstimated)
	{
		const FText Amount = FText::FromString(FString::Printf(TEXT("%.3f mk"), static_cast<double>(MilliMarks) / 1000.0));
		return bEstimated ? FText::Format(LOCTEXT("EstimatedMoney", "≈ {0}"), Amount) : Amount;
	}

	FText SignedContribution(const int32 BasisPoints)
	{
		const TCHAR* Glyph = BasisPoints > 0 ? TEXT("Rising") : BasisPoints < 0 ? TEXT("Falling") : TEXT("Stable");
		return FText::FromString(FString::Printf(TEXT("%s %+.1f%%"), Glyph, static_cast<double>(BasisPoints) / 100.0));
	}

	FText FactorLabel(const Hansa::Simulation::EHansaMarketExplanationFactor Factor)
	{
		using namespace Hansa::Simulation;
		switch (Factor)
		{
		case EHansaMarketExplanationFactor::Scarcity: return LOCTEXT("FactorScarcity", "Stock versus reserve");
		case EHansaMarketExplanationFactor::CitizenDemand: return LOCTEXT("FactorCitizenDemand", "Citizen demand");
		case EHansaMarketExplanationFactor::IndustrialDemand: return LOCTEXT("FactorIndustrialDemand", "Industrial demand");
		case EHansaMarketExplanationFactor::IncomingSupply: return LOCTEXT("FactorIncoming", "Incoming supply");
		case EHansaMarketExplanationFactor::UnmetDemand: return LOCTEXT("FactorUnmet", "Unmet demand");
		case EHansaMarketExplanationFactor::SeasonModifier: return LOCTEXT("FactorSeason", "Season modifier");
		case EHansaMarketExplanationFactor::CityModifier: return LOCTEXT("FactorCity", "City modifier");
		case EHansaMarketExplanationFactor::TargetClamp: return LOCTEXT("FactorClamp", "Price limit");
		default: return LOCTEXT("FactorUnknown", "Other factor");
		}
	}

	FText RelationshipBuildingLabel(const Hansa::Simulation::FHansaSimulationProjection& Projection,
        const Hansa::Simulation::FHansaBuildingId BuildingId)
    {
        const auto* Building = Projection.GetBuildingWorldProjections().FindByPredicate(
            [BuildingId](const auto& Item) { return Item.BuildingId == BuildingId; });
        if (!Building) return LOCTEXT("UnreportedBuilding", "Building details unavailable");
        FString Name = Building->Placement.BuildingDefinitionId.ToString();
        int32 Separator;
        if (Name.FindLastChar(TEXT('.'), Separator)) Name = Name.Mid(Separator + 1);
        return FText::FromString(FName::NameToDisplayString(Name, false));
    }

	FText CategoryLabel(const EHansaMarketGoodCategory Category)
	{
		switch (Category)
		{
		case EHansaMarketGoodCategory::Food: return LOCTEXT("Food", "Food");
		case EHansaMarketGoodCategory::Material: return LOCTEXT("Material", "Material");
		case EHansaMarketGoodCategory::Manufactured: return LOCTEXT("Manufactured", "Manufactured");
		default: return LOCTEXT("AllCategories", "All");
		}
	}

	FText Sparkline(const TArray<Hansa::Simulation::FHansaMarketPriceHistoryEntry>& History)
	{
		if (History.IsEmpty()) return FText::FromString(TEXT("—"));
		int64 Minimum = History[0].PriceMilliMarks;
		int64 Maximum = Minimum;
		for (const auto& Entry : History)
		{
			Minimum = FMath::Min(Minimum, Entry.PriceMilliMarks);
			Maximum = FMath::Max(Maximum, Entry.PriceMilliMarks);
		}
		const TCHAR* Blocks[] = { TEXT("▁"), TEXT("▂"), TEXT("▃"), TEXT("▄"), TEXT("▅"), TEXT("▆"), TEXT("▇"), TEXT("█") };
		FString Result;
		const int32 First = FMath::Max(0, History.Num() - 8);
		for (int32 Index = First; Index < History.Num(); ++Index)
		{
			const int32 Level = Maximum == Minimum ? 3 : static_cast<int32>((History[Index].PriceMilliMarks - Minimum) * 7 / (Maximum - Minimum));
			Result += Blocks[FMath::Clamp(Level, 0, 7)];
		}
		return FText::FromString(Result);
	}

	FText CategoryFilterLabel(const EHansaMarketGoodCategory Value)
	{
		return FText::Format(LOCTEXT("CategoryFilter", "Category: {0}"), CategoryLabel(Value));
	}

	FText TrendFilterLabel(const EHansaMarketTrendFilter Value)
	{
		const FText ValueText = Value == EHansaMarketTrendFilter::Rising ? LOCTEXT("Rising", "Rising") :
			Value == EHansaMarketTrendFilter::Stable ? LOCTEXT("Stable", "Stable") :
			Value == EHansaMarketTrendFilter::Falling ? LOCTEXT("Falling", "Falling") :
			Value == EHansaMarketTrendFilter::Unknown ? LOCTEXT("Unknown", "Unknown") : LOCTEXT("AllTrends", "All");
		return FText::Format(LOCTEXT("TrendFilter", "Trend: {0}"), ValueText);
	}

	FText QuickFilterLabel(const EHansaMarketQuickFilter Value)
	{
		const FText ValueText = Value == EHansaMarketQuickFilter::Shortage ? LOCTEXT("Shortage", "Shortage") :
			Value == EHansaMarketQuickFilter::OwnedStock ? LOCTEXT("OwnedStock", "Owned stock") :
			Value == EHansaMarketQuickFilter::Incoming ? LOCTEXT("Incoming", "Incoming") :
			Value == EHansaMarketQuickFilter::Opportunity ? LOCTEXT("Opportunity", "Opportunity") : LOCTEXT("AllQuick", "All");
		return FText::Format(LOCTEXT("QuickFilter", "Filter: {0}"), ValueText);
	}
}

bool operator==(const FHansaMarketTableRowPresentation& Left, const FHansaMarketTableRowPresentation& Right)
{
	return Left.GoodStableId == Right.GoodStableId && TextEqual(Left.GoodLabel, Right.GoodLabel) &&
		TextEqual(Left.GoodGlyph, Right.GoodGlyph) && TextEqual(Left.CategoryLabel, Right.CategoryLabel) &&
		TextEqual(Left.Stock, Right.Stock) && TextEqual(Left.Reserve, Right.Reserve) && TextEqual(Left.Demand, Right.Demand) &&
		TextEqual(Left.Price, Right.Price) && TextEqual(Left.Trend, Right.Trend) && TextEqual(Left.Sparkline, Right.Sparkline) &&
		TextEqual(Left.Incoming, Right.Incoming) && TextEqual(Left.Status, Right.Status) && TextEqual(Left.ReportAge, Right.ReportAge) &&
		TextEqual(Left.AccessibleLabel, Right.AccessibleLabel) && Left.Category == Right.Category && Left.TrendKind == Right.TrendKind &&
		Left.StockRaw == Right.StockRaw && Left.ReserveRaw == Right.ReserveRaw && Left.DemandRaw == Right.DemandRaw &&
		Left.PriceRaw == Right.PriceRaw && Left.PriceDifferenceRaw == Right.PriceDifferenceRaw &&
		Left.PriceDifferenceBasisPoints == Right.PriceDifferenceBasisPoints && Left.IncomingRaw == Right.IncomingRaw &&
		Left.ReportAgeTicks == Right.ReportAgeTicks && Left.bUnknown == Right.bUnknown && Left.bStale == Right.bStale &&
		Left.bEstimated == Right.bEstimated && Left.bShortage == Right.bShortage && Left.bOpportunity == Right.bOpportunity;
}

bool operator==(const FHansaMarketChartPointPresentation& Left, const FHansaMarketChartPointPresentation& Right)
{
	return Left.Tick == Right.Tick && Left.PriceMilliMarks == Right.PriceMilliMarks &&
		FMath::IsNearlyEqual(Left.NormalizedPrice, Right.NormalizedPrice) && TextEqual(Left.AccessibleLabel, Right.AccessibleLabel);
}

bool operator==(const FHansaMarketFactorPresentation& Left, const FHansaMarketFactorPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.Contribution, Right.Contribution) &&
		TextEqual(Left.AccessibleLabel, Right.AccessibleLabel) && Left.ContributionBasisPoints == Right.ContributionBasisPoints;
}

bool operator==(const FHansaMarketRelationshipPresentation& Left, const FHansaMarketRelationshipPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.Detail, Right.Detail) &&
		TextEqual(Left.Status, Right.Status) && TextEqual(Left.AccessibleLabel, Right.AccessibleLabel) && Left.bWarning == Right.bWarning && Left.BuildingValue == Right.BuildingValue;
}

bool operator==(const FHansaSelectedGoodPresentation& Left, const FHansaSelectedGoodPresentation& Right)
{
	return Left.GoodStableId == Right.GoodStableId && TextEqual(Left.GoodLabel, Right.GoodLabel) && TextEqual(Left.GoodGlyph, Right.GoodGlyph) &&
		TextEqual(Left.Confidence, Right.Confidence) && TextEqual(Left.BaseValue, Right.BaseValue) && TextEqual(Left.LocalPrice, Right.LocalPrice) &&
		TextEqual(Left.RecentAverageDifference, Right.RecentAverageDifference) && TextEqual(Left.StockVersusReserve, Right.StockVersusReserve) &&
		TextEqual(Left.ReserveDays, Right.ReserveDays) && TextEqual(Left.CitizenDemand, Right.CitizenDemand) &&
		TextEqual(Left.IndustrialDemand, Right.IndustrialDemand) && TextEqual(Left.IncomingSupply, Right.IncomingSupply) &&
		TextEqual(Left.Production, Right.Production) && TextEqual(Left.Consumption, Right.Consumption) && TextEqual(Left.SupplyBalance, Right.SupplyBalance) && TextEqual(Left.Explanation, Right.Explanation) && TextEqual(Left.ChartSummary, Right.ChartSummary) &&
		TextEqual(Left.PinActionLabel, Right.PinActionLabel) && TextEqual(Left.PinDisabledReason, Right.PinDisabledReason) &&
		TextEqual(Left.RouteActionLabel, Right.RouteActionLabel) && TextEqual(Left.RouteDisabledReason, Right.RouteDisabledReason) &&
		TextEqual(Left.LastActionResult, Right.LastActionResult) && Left.History == Right.History && Left.Factors == Right.Factors &&
		Left.Consumers == Right.Consumers && Left.Producers == Right.Producers && Left.CurrentPriceMilliMarks == Right.CurrentPriceMilliMarks &&
		Left.RecentAveragePriceMilliMarks == Right.RecentAveragePriceMilliMarks && Left.MinimumHistoryPriceMilliMarks == Right.MinimumHistoryPriceMilliMarks &&
		Left.MaximumHistoryPriceMilliMarks == Right.MaximumHistoryPriceMilliMarks && Left.bHasSelection == Right.bHasSelection &&
		Left.bHasReport == Right.bHasReport && Left.bStale == Right.bStale && Left.bPinned == Right.bPinned &&
		Left.bPinEnabled == Right.bPinEnabled && Left.bRouteEnabled == Right.bRouteEnabled;
}

bool operator==(const FHansaMarketTableSnapshot& Left, const FHansaMarketTableSnapshot& Right)
{
	return Left.AllRows == Right.AllRows && Left.VisibleRows == Right.VisibleRows && TextEqual(Left.SearchText, Right.SearchText) &&
		TextEqual(Left.ResultSummary, Right.ResultSummary) && TextEqual(Left.EmptyTitle, Right.EmptyTitle) && TextEqual(Left.EmptyDetail, Right.EmptyDetail) &&
		Left.SelectedGoodStableId == Right.SelectedGoodStableId && Left.FocusedSemanticId == Right.FocusedSemanticId &&
		Left.CategoryFilter == Right.CategoryFilter && Left.TrendFilter == Right.TrendFilter && Left.QuickFilter == Right.QuickFilter &&
		Left.SortColumn == Right.SortColumn && Left.bSortAscending == Right.bSortAscending && Left.SelectedGood == Right.SelectedGood;
}

void UHansaMarketTablePresentationModel::InitializeDefaults()
{
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot = {};
	DetailByGood.Reset();
	PinnedGoods.Reset();
	Snapshot.EmptyTitle = LOCTEXT("EmptyTitle", "No goods match these filters");
	Snapshot.EmptyDetail = LOCTEXT("EmptyDetail", "Change the search or filters to show market reports.");
	for (const FGoodDescriptor& Descriptor : CanonicalGoods)
	{
		FHansaMarketTableRowPresentation Row;
		Row.GoodStableId = FName(Descriptor.StableId);
		Row.GoodLabel = FText::FromString(Descriptor.Label);
		Row.GoodGlyph = FText::FromString(Descriptor.Glyph);
		Row.Category = Descriptor.Category;
		Row.CategoryLabel = CategoryLabel(Descriptor.Category);
		Row.Stock = Row.Reserve = Row.Demand = Row.Price = Row.Trend = Row.Incoming = FText::FromString(TEXT("—"));
		Row.Sparkline = FText::FromString(TEXT("—"));
		Row.Status = LOCTEXT("NoRecentReport", "? No recent report");
		Row.ReportAge = LOCTEXT("ReportUnavailable", "Report unavailable");
		Row.AccessibleLabel = FText::Format(LOCTEXT("UnknownAccessible", "{0}. No recent report. Stock, reserve, demand, price, trend and incoming supply are unknown."), Row.GoodLabel);
		Snapshot.AllRows.Add(MoveTemp(Row));
	}
	RebuildVisibleRows();
	RebuildSelectedGood();
	PublishIfChanged(Previous);
}

bool UHansaMarketTablePresentationModel::ApplyProjection(
	const Hansa::Simulation::FHansaSimulationProjection& Projection,
	const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const Hansa::Simulation::FHansaCityDefinitionId CityId)
{
	if (!CityId.IsValid()) return false;
	const FHansaMarketTableSnapshot Previous = Snapshot;
	const FText Search = Snapshot.SearchText;
	const FName Selected = Snapshot.SelectedGoodStableId;
	const FName Focused = Snapshot.FocusedSemanticId;
	const EHansaMarketGoodCategory CategoryFilter = Snapshot.CategoryFilter;
	const EHansaMarketTrendFilter TrendFilter = Snapshot.TrendFilter;
	const EHansaMarketQuickFilter QuickFilter = Snapshot.QuickFilter;
	const EHansaMarketSortColumn SortColumn = Snapshot.SortColumn;
	const bool bSortAscending = Snapshot.bSortAscending;
	Snapshot = {};
	Snapshot.SearchText = Search;
	Snapshot.SelectedGoodStableId = Selected;
	Snapshot.FocusedSemanticId = Focused;
	Snapshot.CategoryFilter = CategoryFilter;
	Snapshot.TrendFilter = TrendFilter;
	Snapshot.QuickFilter = QuickFilter;
	Snapshot.SortColumn = SortColumn;
	Snapshot.bSortAscending = bSortAscending;
	Snapshot.EmptyTitle = LOCTEXT("EmptyTitle", "No goods match these filters");
	Snapshot.EmptyDetail = LOCTEXT("EmptyDetail", "Change the search or filters to show market reports.");
	DetailByGood.Reset();

	for (const FGoodDescriptor& Descriptor : CanonicalGoods)
	{
		FHansaMarketTableRowPresentation Row;
		Row.GoodStableId = FName(Descriptor.StableId);
		Row.GoodLabel = FText::FromString(Descriptor.Label);
		Row.GoodGlyph = FText::FromString(Descriptor.Glyph);
		Row.Category = Descriptor.Category;
		Row.CategoryLabel = CategoryLabel(Descriptor.Category);
		FHansaSelectedGoodPresentation Detail;
		Detail.GoodStableId = Row.GoodStableId;
		Detail.GoodLabel = Row.GoodLabel;
		Detail.GoodGlyph = Row.GoodGlyph;
		Detail.PinActionLabel = LOCTEXT("PinAction", "Pin price & stock");
		Detail.RouteActionLabel = LOCTEXT("RouteAction", "Begin route");
		Detail.RouteDisabledReason = LOCTEXT("RouteNeedsReport", "A market report is required before planning a route.");
		const auto ParsedGood = Hansa::Simulation::FHansaGoodId::TryParse(Descriptor.StableId);
		const Hansa::Simulation::FHansaCityMarketProjection* Market = nullptr;
		if (ParsedGood)
		{
			for (const Hansa::Simulation::FHansaCityMarketProjection& Candidate : Projection.GetMarkets())
			{
				if (Candidate.CityId == CityId && Candidate.GoodId == ParsedGood.Value) { Market = &Candidate; break; }
			}
		}
		const Hansa::Simulation::FHansaCompiledGoodDefinition* Definition = Registry.FindGood(Descriptor.StableId);
		Detail.BaseValue = Definition != nullptr ? Money(Definition->BaseValueMilliMarks, false) : FText::FromString(TEXT("—"));
		if (Market == nullptr)
		{
			Row.Stock = Row.Reserve = Row.Demand = Row.Price = Row.Trend = Row.Incoming = FText::FromString(TEXT("—"));
			Row.Sparkline = FText::FromString(TEXT("—"));
			Row.Status = LOCTEXT("NoRecentReport", "? No recent report");
			Row.ReportAge = LOCTEXT("ReportUnavailable", "Report unavailable");
			Row.AccessibleLabel = FText::Format(LOCTEXT("UnknownAccessible", "{0}. No recent report. Stock, reserve, demand, price, trend and incoming supply are unknown."), Row.GoodLabel);
			Detail.Confidence = LOCTEXT("NoDetailReport", "? No recent report");
			Detail.LocalPrice = Detail.RecentAverageDifference = Detail.StockVersusReserve = Detail.ReserveDays =
				Detail.Production = Detail.Consumption = Detail.SupplyBalance = Detail.CitizenDemand = Detail.IndustrialDemand = Detail.IncomingSupply = FText::FromString(TEXT("—"));
			Detail.Explanation = LOCTEXT("NoDetailExplanation", "A current market report is required before this good's price causes can be explained.");
			Detail.ChartSummary = LOCTEXT("NoHistory", "No price history is available.");
			Detail.PinDisabledReason = LOCTEXT("PinNeedsReport", "A market report is required before this watch can be pinned.");
			DetailByGood.Add(Detail.GoodStableId, MoveTemp(Detail));
			Snapshot.AllRows.Add(MoveTemp(Row));
			continue;
		}

		Row.bUnknown = false;
		Row.bStale = Market->bIsStale;
		Row.bEstimated = Market->bIsStale;
		Row.StockRaw = Market->CurrentStock.GetRawValue();
		Row.ReserveRaw = Market->DesiredReserve.GetRawValue();
		Row.DemandRaw = Market->CitizenDemand.GetRawValue() + Market->IndustrialDemand.GetRawValue();
		Row.PriceRaw = Market->CurrentPriceMilliMarks;
		Row.PriceDifferenceRaw = Market->CurrentPriceMilliMarks - Market->RecentAveragePriceMilliMarks;
		Row.PriceDifferenceBasisPoints = Market->RecentAveragePriceMilliMarks != 0
			? static_cast<int32>(Row.PriceDifferenceRaw * 10000 / Market->RecentAveragePriceMilliMarks) : 0;
		Row.IncomingRaw = Market->ExpectedIncomingSupply.GetRawValue();
		Row.ReportAgeTicks = Market->ReportAgeTicks;
		Row.bShortage = Row.StockRaw < Row.ReserveRaw || Market->UnmetDemand.GetRawValue() > 0;
		Row.TrendKind = Row.PriceDifferenceBasisPoints > 50 ? EHansaMarketTrendFilter::Rising :
			(Row.PriceDifferenceBasisPoints < -50 ? EHansaMarketTrendFilter::Falling : EHansaMarketTrendFilter::Stable);
		Row.bOpportunity = Row.bShortage && Row.TrendKind == EHansaMarketTrendFilter::Rising;
		Row.Stock = Quantity(Row.StockRaw);
		Row.Reserve = Quantity(Row.ReserveRaw);
		Row.Demand = Quantity(Row.DemandRaw);
		Row.Price = Money(Row.PriceRaw, Row.bEstimated);
		const TCHAR* TrendGlyph = Row.TrendKind == EHansaMarketTrendFilter::Rising ? TEXT("Rising") :
			(Row.TrendKind == EHansaMarketTrendFilter::Falling ? TEXT("Falling") : TEXT("Stable"));
		Row.Trend = FText::FromString(FString::Printf(TEXT("%s %+.1f%% · %+.3f mk"), TrendGlyph,
			static_cast<double>(Row.PriceDifferenceBasisPoints) / 100.0, static_cast<double>(Row.PriceDifferenceRaw) / 1000.0));
		Row.Sparkline = Sparkline(Market->PriceHistory);
		Row.Incoming = Quantity(Row.IncomingRaw);
		Row.ReportAge = FText::Format(LOCTEXT("ReportAge", "{0} ticks old"), FText::AsNumber(Row.ReportAgeTicks));
		if (Row.bStale)
		{
			Row.Status = FText::Format(LOCTEXT("StaleEstimated", "Stale · estimated · {0} ticks"), FText::AsNumber(Row.ReportAgeTicks));
		}
		else if (Row.bOpportunity) Row.Status = LOCTEXT("ShortageOpportunity", "Shortage · import opportunity");
		else if (Row.bShortage) Row.Status = LOCTEXT("ShortageStatus", "Shortage");
		else if (Row.bOpportunity) Row.Status = LOCTEXT("OpportunityStatus", "Opportunity");
		else Row.Status = LOCTEXT("CurrentStatus", "Current");
		Row.AccessibleLabel = FText::Format(LOCTEXT("RowAccessible", "{0}, {1}. Stock {2}. Reserve target {3}. Demand {4}. Price {5}. Trend {6}, history {7}. Incoming {8}. Report {9}. Age {10}."),
			Row.GoodLabel, Row.CategoryLabel, Row.Stock, Row.Reserve, Row.Demand, Row.Price, Row.Trend, Row.Sparkline, Row.Incoming, Row.Status, Row.ReportAge);

		Detail.bHasReport = true;
		Detail.bStale = Market->bIsStale;
		Detail.bPinned = PinnedGoods.Contains(Detail.GoodStableId);
		Detail.bPinEnabled = true;
		Detail.bRouteEnabled = true;
		Detail.RouteDisabledReason = FText::GetEmpty();
		Detail.Confidence = Market->bIsStale
			? FText::Format(LOCTEXT("StaleDetailReport", "Stale estimate · {0} ticks old"), FText::AsNumber(Market->ReportAgeTicks))
			: LOCTEXT("CurrentDetailReport", "Current report");
		Detail.CurrentPriceMilliMarks = Market->CurrentPriceMilliMarks;
		Detail.RecentAveragePriceMilliMarks = Market->RecentAveragePriceMilliMarks;
		Detail.LocalPrice = Money(Market->CurrentPriceMilliMarks, Market->bIsStale);
		Detail.RecentAverageDifference = Row.Trend;
		Detail.StockVersusReserve = FText::Format(LOCTEXT("StockReserve", "{0} / {1}"), Row.Stock, Row.Reserve);
		Detail.CitizenDemand = Quantity(Market->CitizenDemand.GetRawValue());
		Detail.IndustrialDemand = Quantity(Market->IndustrialDemand.GetRawValue());
		Detail.IncomingSupply = Quantity(Market->ExpectedIncomingSupply.GetRawValue());
        Detail.Production = Quantity(Market->RecentLocalProduction.GetRawValue());
        int64 ConsumedRaw = 0;
        for (const auto& Consumer : Projection.GetMarketConsumers())
            if (Consumer.CityId == CityId && Consumer.GoodId == ParsedGood.Value) ConsumedRaw += Consumer.FulfilledLastTick.GetRawValue();
        Detail.Consumption = Quantity(ConsumedRaw);
        Detail.SupplyBalance = Row.bShortage
            ? FText::Format(LOCTEXT("ShortageBalance", "Shortage: {0} below reserve; unmet demand {1}. {2}"), Quantity(FMath::Max<int64>(0, Row.ReserveRaw-Row.StockRaw)), Quantity(Market->UnmetDemand.GetRawValue()),
                Row.bOpportunity ? LOCTEXT("ImportOpportunity", "Rising price suggests reviewing an import route; profit and destination stock are not yet known.") : LOCTEXT("ReviewSupply", "Review producers, consumers and incoming supply."))
            : FText::Format(LOCTEXT("SurplusBalance", "Surplus above reserve: {0}. Compare destination reports before planning an export route."), Quantity(FMath::Max<int64>(0, Row.StockRaw-Row.ReserveRaw)));

		const Hansa::Simulation::FHansaMarketReserveProjection* Reserve = nullptr;
		for (const auto& Candidate : Projection.GetMarketReserves())
		{
			if (Candidate.CityId == CityId && Candidate.GoodId == ParsedGood.Value) { Reserve = &Candidate; break; }
		}
		Detail.ReserveDays = Reserve != nullptr && Reserve->bHasDemand
			? FText::FromString(FString::Printf(TEXT("%.1f days"), static_cast<double>(Reserve->ReserveMilliDays) / 1000.0))
			: LOCTEXT("ReserveDaysUnknown", "—");

		const Hansa::Simulation::FHansaMarketExplanationProjection* Explanation = nullptr;
		for (const auto& Candidate : Projection.GetMarketExplanations())
		{
			if (Candidate.CityId == CityId && Candidate.GoodId == ParsedGood.Value) { Explanation = &Candidate; break; }
		}
		if (Explanation != nullptr)
		{
			for (int32 FactorIndex = 0; FactorIndex < Explanation->Factors.Num(); ++FactorIndex)
			{
				const auto& Source = Explanation->Factors[FactorIndex];
				FHansaMarketFactorPresentation Factor;
				Factor.StableId = FName(*FString::Printf(TEXT("Factor.%s.%d"), Hansa::Simulation::LexToString(Source.Factor), FactorIndex));
				Factor.Label = FactorLabel(Source.Factor);
				Factor.ContributionBasisPoints = Source.ContributionBasisPoints;
				Factor.Contribution = SignedContribution(Source.ContributionBasisPoints);
				Factor.AccessibleLabel = FText::Format(LOCTEXT("FactorAccessible", "{0}. Contribution {1}."), Factor.Label, Factor.Contribution);
				Detail.Factors.Add(MoveTemp(Factor));
			}
		}

		FText AuthoritativeCause;
		for (const auto& Alert : Projection.GetActiveMarketAlerts())
		{
			if (Alert.CityId == CityId && Alert.GoodId == ParsedGood.Value) { AuthoritativeCause = LOCTEXT("ReserveReview","Review the reserve target and incoming supply."); break; }
		}
		if (AuthoritativeCause.IsEmpty() && !Detail.Factors.IsEmpty()) AuthoritativeCause = Detail.Factors[0].Label;
		Detail.Explanation = FText::Format(LOCTEXT("CausalExplanation", "{0} is {1} versus its recent average. Stock is {2}; reserve coverage is {3}. {4} Incoming supply is {5}."),
			Detail.GoodLabel, Detail.RecentAverageDifference, Detail.StockVersusReserve, Detail.ReserveDays,
			AuthoritativeCause.IsEmpty() ? LOCTEXT("NoDominantCause", "No dominant causal factor was reported.") : AuthoritativeCause, Detail.IncomingSupply);

		for (const auto& Source : Projection.GetMarketConsumers())
		{
			if (Source.CityId != CityId || Source.GoodId != ParsedGood.Value) continue;
			FHansaMarketRelationshipPresentation Consumer;
			const bool bCitizen = Source.Kind == Hansa::Simulation::EHansaMarketConsumerKind::Citizen;
			Consumer.BuildingValue = static_cast<int64>(Source.BuildingId.GetValue());
			Consumer.StableId = FName(*(bCitizen ? Source.PopulationCohortId.ToDebugString() : Source.ProductionId.ToDebugString()));
			Consumer.Label = bCitizen ? FText::Format(LOCTEXT("ResidentConsumer", "Residents · {0}"), RelationshipBuildingLabel(Projection,Source.BuildingId)) : RelationshipBuildingLabel(Projection,Source.BuildingId);
			const int64 DemandRaw = Source.DemandPerTick.GetRawValue();
			const int32 FulfilledBasisPoints = DemandRaw > 0 ? static_cast<int32>(Source.FulfilledLastTick.GetRawValue() * 10000 / DemandRaw) : 10000;
			Consumer.Detail = FText::Format(LOCTEXT("ConsumerDetail", "{0} demand · {1}% fulfilled"), Quantity(DemandRaw), FText::AsNumber(FulfilledBasisPoints / 100));
			Consumer.bWarning = Source.ProductionBlocker != Hansa::Simulation::EHansaProductionBlocker::None || FulfilledBasisPoints < 10000;
			Consumer.Status = Consumer.bWarning
				? FText::FromString(FString::Printf(TEXT("%s"), Source.ProductionBlocker == Hansa::Simulation::EHansaProductionBlocker::None ? TEXT("Unmet demand") : Hansa::Simulation::LexToString(Source.ProductionBlocker)))
				: LOCTEXT("ConsumerCurrent", "Current");
			Consumer.AccessibleLabel = FText::Format(LOCTEXT("ConsumerAccessible", "{0}. {1}. {2}."), Consumer.Label, Consumer.Detail, Consumer.Status);
			Detail.Consumers.Add(MoveTemp(Consumer));
		}

		for (const auto& Source : Projection.GetMarketProducers())
		{
			if (Source.CityId != CityId || Source.GoodId != ParsedGood.Value) continue;
			FHansaMarketRelationshipPresentation Producer;
			const bool bBackground = Source.Kind == Hansa::Simulation::EHansaMarketProducerKind::BackgroundSupply;
			Producer.BuildingValue = static_cast<int64>(Source.BuildingId.GetValue());
			Producer.StableId = FName(*Source.ProductionId.ToDebugString());
			Producer.Label = bBackground ? LOCTEXT("BackgroundSupply", "Background supply") : RelationshipBuildingLabel(Projection,Source.BuildingId);
			Producer.Detail = FText::Format(LOCTEXT("ProducerDetail", "{0} last tick; capacity {1} per cycle"), Quantity(Source.ActualQuantityLastTick.GetRawValue()), Quantity(Source.NominalQuantityPerCycle.GetRawValue()));
			Producer.bWarning = !Source.bActive || Source.Blocker != Hansa::Simulation::EHansaProductionBlocker::None;
			Producer.Status = Producer.bWarning
				? FText::FromString(FString::Printf(TEXT("%s"), Source.Blocker == Hansa::Simulation::EHansaProductionBlocker::None ? TEXT("Paused") : Hansa::Simulation::LexToString(Source.Blocker)))
				: LOCTEXT("ProducerOperating", "Operating");
			Producer.AccessibleLabel = FText::Format(LOCTEXT("ProducerAccessible", "{0}. {1}. {2}."), Producer.Label, Producer.Detail, Producer.Status);
			Detail.Producers.Add(MoveTemp(Producer));
		}

		const int32 HistoryStart = FMath::Max(0, Market->PriceHistory.Num() - 64);
		if (HistoryStart < Market->PriceHistory.Num())
		{
			Detail.MinimumHistoryPriceMilliMarks = Market->PriceHistory[HistoryStart].PriceMilliMarks;
			Detail.MaximumHistoryPriceMilliMarks = Detail.MinimumHistoryPriceMilliMarks;
			for (int32 Index = HistoryStart; Index < Market->PriceHistory.Num(); ++Index)
			{
				Detail.MinimumHistoryPriceMilliMarks = FMath::Min(Detail.MinimumHistoryPriceMilliMarks, Market->PriceHistory[Index].PriceMilliMarks);
				Detail.MaximumHistoryPriceMilliMarks = FMath::Max(Detail.MaximumHistoryPriceMilliMarks, Market->PriceHistory[Index].PriceMilliMarks);
			}
			for (int32 Index = HistoryStart; Index < Market->PriceHistory.Num(); ++Index)
			{
				const auto& Source = Market->PriceHistory[Index];
				FHansaMarketChartPointPresentation Point;
				Point.Tick = Source.Tick.GetValue();
				Point.PriceMilliMarks = Source.PriceMilliMarks;
				Point.NormalizedPrice = Detail.MaximumHistoryPriceMilliMarks == Detail.MinimumHistoryPriceMilliMarks ? 0.5f :
					static_cast<float>(Source.PriceMilliMarks - Detail.MinimumHistoryPriceMilliMarks) /
					static_cast<float>(Detail.MaximumHistoryPriceMilliMarks - Detail.MinimumHistoryPriceMilliMarks);
				Point.AccessibleLabel = FText::Format(LOCTEXT("ChartPointAccessible", "Tick {0}, price {1}."), FText::AsNumber(Point.Tick), Money(Point.PriceMilliMarks, Market->bIsStale));
				Detail.History.Add(MoveTemp(Point));
			}
		}
		Detail.ChartSummary = Detail.History.IsEmpty() ? LOCTEXT("NoPriceHistory", "No price history is available.") :
			FText::Format(LOCTEXT("ChartSummary", "{0} recorded prices. Range {1} to {2}. Current {3}; recent average {4}. {5}"),
				FText::AsNumber(Detail.History.Num()), Money(Detail.MinimumHistoryPriceMilliMarks, false), Money(Detail.MaximumHistoryPriceMilliMarks, false),
				Detail.LocalPrice, Money(Detail.RecentAveragePriceMilliMarks, Market->bIsStale), Detail.Confidence);
		if(!Detail.History.IsEmpty()) Detail.ChartSummary=FText::Format(LOCTEXT("ChartAxes","Ticks {0}–{1}. {2} Price: {3}; recent average: fine dashed line."),
            FText::AsNumber(Detail.History[0].Tick),FText::AsNumber(Detail.History.Last().Tick),Detail.ChartSummary,
            Detail.bStale?LOCTEXT("StaleLine","long dashed line (stale)"):LOCTEXT("CurrentLine","solid line"));
        DetailByGood.Add(Detail.GoodStableId, MoveTemp(Detail));
        Snapshot.AllRows.Add(MoveTemp(Row));
	}
	RebuildVisibleRows();
	RebuildSelectedGood();
	PublishIfChanged(Previous);
	return true;
}

bool UHansaMarketTablePresentationModel::SetSearchTextIntent(FText SearchText)
{
	if (Snapshot.SearchText.EqualTo(SearchText)) return false;
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.SearchText = MoveTemp(SearchText);
	RebuildVisibleRows(); PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::CycleCategoryFilterIntent()
{
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.CategoryFilter = static_cast<EHansaMarketGoodCategory>((static_cast<uint8>(Snapshot.CategoryFilter) + 1) % 4);
	RebuildVisibleRows(); PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::CycleTrendFilterIntent()
{
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.TrendFilter = static_cast<EHansaMarketTrendFilter>((static_cast<uint8>(Snapshot.TrendFilter) + 1) % 5);
	RebuildVisibleRows(); PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::CycleQuickFilterIntent()
{
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.QuickFilter = static_cast<EHansaMarketQuickFilter>((static_cast<uint8>(Snapshot.QuickFilter) + 1) % 5);
	RebuildVisibleRows(); PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::ClearFiltersIntent()
{
	if (Snapshot.SearchText.IsEmpty() && Snapshot.CategoryFilter == EHansaMarketGoodCategory::All &&
		Snapshot.TrendFilter == EHansaMarketTrendFilter::All && Snapshot.QuickFilter == EHansaMarketQuickFilter::All) return false;
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.SearchText = FText(); Snapshot.CategoryFilter = EHansaMarketGoodCategory::All;
	Snapshot.TrendFilter = EHansaMarketTrendFilter::All; Snapshot.QuickFilter = EHansaMarketQuickFilter::All;
	RebuildVisibleRows(); PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::SortByIntent(const EHansaMarketSortColumn Column)
{
	const FHansaMarketTableSnapshot Previous = Snapshot;
	if (Snapshot.SortColumn == Column) Snapshot.bSortAscending = !Snapshot.bSortAscending;
	else { Snapshot.SortColumn = Column; Snapshot.bSortAscending = true; }
	RebuildVisibleRows(); PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::SelectGoodIntent(const FName GoodStableId)
{
	if (Snapshot.SelectedGoodStableId == GoodStableId || FindRow(GoodStableId) == nullptr) return false;
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.SelectedGoodStableId = GoodStableId;
	Snapshot.FocusedSemanticId = FName(*FString::Printf(TEXT("Market.Row.%s"), *GoodStableId.ToString().Replace(TEXT("."), TEXT("_"))));
	RebuildSelectedGood();
	PublishIfChanged(Previous); return true;
}

bool UHansaMarketTablePresentationModel::TogglePinIntent()
{
	if (!Snapshot.SelectedGood.bHasSelection || !Snapshot.SelectedGood.bPinEnabled) return false;
	const FHansaMarketTableSnapshot Previous = Snapshot;
	const FName GoodId = Snapshot.SelectedGood.GoodStableId;
	if (PinnedGoods.Contains(GoodId)) PinnedGoods.Remove(GoodId); else PinnedGoods.Add(GoodId);
	RebuildSelectedGood();
	Snapshot.SelectedGood.LastActionResult = Snapshot.SelectedGood.bPinned
		? FText::Format(LOCTEXT("PinnedResult", "{0} price and stock pinned."), Snapshot.SelectedGood.GoodLabel)
		: FText::Format(LOCTEXT("UnpinnedResult", "{0} price and stock unpinned."), Snapshot.SelectedGood.GoodLabel);
	PublishIfChanged(Previous);
	return true;
}

bool UHansaMarketTablePresentationModel::BeginRouteIntent()
{
	if (!Snapshot.SelectedGood.bHasSelection || !Snapshot.SelectedGood.bRouteEnabled) return false;
	Snapshot.FocusedSemanticId = TEXT("Market.Detail.Action.BeginRoute");
	RouteRequested.Broadcast(Snapshot.SelectedGood.GoodStableId);
	return true;
}

bool UHansaMarketTablePresentationModel::RevealRelationshipIntent(const bool bProducer, const FName StableId)
{
    const auto& Relationships = bProducer ? Snapshot.SelectedGood.Producers : Snapshot.SelectedGood.Consumers;
    const auto* Found = Relationships.FindByPredicate([StableId](const auto& Item) { return Item.StableId == StableId; });
    if (!Snapshot.SelectedGood.bHasSelection || !Found || Found->BuildingValue <= 0) return false;
    const FName SemanticId(*FString::Printf(TEXT("Market.Detail.%s.%s"), bProducer ? TEXT("Producer") : TEXT("Consumer"), *StableId.ToString().Replace(TEXT("."),TEXT("_"))));
    BuildingRequested.Broadcast(SemanticId, Found->BuildingValue);
    return true;
}

void UHansaMarketTablePresentationModel::SetFocusedSemanticId(const FName SemanticId)
{
	if (Snapshot.FocusedSemanticId == SemanticId) return;
	const FHansaMarketTableSnapshot Previous = Snapshot;
	Snapshot.FocusedSemanticId = SemanticId;
	PublishIfChanged(Previous);
}

const FHansaMarketTableRowPresentation* UHansaMarketTablePresentationModel::FindRow(const FName GoodStableId) const
{
	return Snapshot.AllRows.FindByPredicate([GoodStableId](const FHansaMarketTableRowPresentation& Row) { return Row.GoodStableId == GoodStableId; });
}

void UHansaMarketTablePresentationModel::RebuildVisibleRows()
{
	Snapshot.VisibleRows.Reset();
	const FString Search = Snapshot.SearchText.ToString().TrimStartAndEnd();
	for (const FHansaMarketTableRowPresentation& Row : Snapshot.AllRows)
	{
		if (!Search.IsEmpty() && !Row.GoodLabel.ToString().Contains(Search, ESearchCase::IgnoreCase) && !Row.GoodStableId.ToString().Contains(Search, ESearchCase::IgnoreCase)) continue;
		if (Snapshot.CategoryFilter != EHansaMarketGoodCategory::All && Row.Category != Snapshot.CategoryFilter) continue;
		if (Snapshot.TrendFilter != EHansaMarketTrendFilter::All && Row.TrendKind != Snapshot.TrendFilter) continue;
		const bool bQuickMatch = Snapshot.QuickFilter == EHansaMarketQuickFilter::All ||
			(Snapshot.QuickFilter == EHansaMarketQuickFilter::Shortage && Row.bShortage) ||
			(Snapshot.QuickFilter == EHansaMarketQuickFilter::OwnedStock && !Row.bUnknown && Row.StockRaw > 0) ||
			(Snapshot.QuickFilter == EHansaMarketQuickFilter::Incoming && !Row.bUnknown && Row.IncomingRaw > 0) ||
			(Snapshot.QuickFilter == EHansaMarketQuickFilter::Opportunity && Row.bOpportunity);
		if (bQuickMatch) Snapshot.VisibleRows.Add(Row);
	}

	auto CompareNumber = [this](const int64 Left, const int64 Right, const FName LeftId, const FName RightId)
	{
		if (Left != Right) return Snapshot.bSortAscending ? Left < Right : Left > Right;
		return LeftId.LexicalLess(RightId);
	};
	Snapshot.VisibleRows.StableSort([this, &CompareNumber](const FHansaMarketTableRowPresentation& Left, const FHansaMarketTableRowPresentation& Right)
	{
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Good)
		{
			const int32 Compare = Left.GoodLabel.ToString().Compare(Right.GoodLabel.ToString(), ESearchCase::IgnoreCase);
			return Compare == 0 ? Left.GoodStableId.LexicalLess(Right.GoodStableId) : (Snapshot.bSortAscending ? Compare < 0 : Compare > 0);
		}
		if (Left.bUnknown != Right.bUnknown) return !Left.bUnknown;
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Stock) return CompareNumber(Left.StockRaw, Right.StockRaw, Left.GoodStableId, Right.GoodStableId);
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Reserve) return CompareNumber(Left.ReserveRaw, Right.ReserveRaw, Left.GoodStableId, Right.GoodStableId);
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Demand) return CompareNumber(Left.DemandRaw, Right.DemandRaw, Left.GoodStableId, Right.GoodStableId);
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Price) return CompareNumber(Left.PriceRaw, Right.PriceRaw, Left.GoodStableId, Right.GoodStableId);
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Trend) return CompareNumber(Left.PriceDifferenceBasisPoints, Right.PriceDifferenceBasisPoints, Left.GoodStableId, Right.GoodStableId);
		if (Snapshot.SortColumn == EHansaMarketSortColumn::Incoming) return CompareNumber(Left.IncomingRaw, Right.IncomingRaw, Left.GoodStableId, Right.GoodStableId);
		const int64 LeftStatus = Left.bUnknown ? 3 : Left.bStale ? 2 : Left.bShortage ? 1 : 0;
		const int64 RightStatus = Right.bUnknown ? 3 : Right.bStale ? 2 : Right.bShortage ? 1 : 0;
		return CompareNumber(LeftStatus, RightStatus, Left.GoodStableId, Right.GoodStableId);
	});
	Snapshot.ResultSummary = FText::Format(LOCTEXT("ResultSummary", "{0} of {1} goods"), FText::AsNumber(Snapshot.VisibleRows.Num()), FText::AsNumber(Snapshot.AllRows.Num()));
}

void UHansaMarketTablePresentationModel::RebuildSelectedGood()
{
	if (const FHansaSelectedGoodPresentation* Found = DetailByGood.Find(Snapshot.SelectedGoodStableId))
	{
		Snapshot.SelectedGood = *Found;
		Snapshot.SelectedGood.bHasSelection = true;
		Snapshot.SelectedGood.bPinned = PinnedGoods.Contains(Snapshot.SelectedGood.GoodStableId);
		return;
	}
	Snapshot.SelectedGood = {};
	Snapshot.SelectedGood.PinActionLabel = LOCTEXT("PinAction", "Pin price & stock");
	Snapshot.SelectedGood.RouteActionLabel = LOCTEXT("RouteAction", "Begin route");
	Snapshot.SelectedGood.Explanation = LOCTEXT("SelectGood", "Select a good to inspect its price, supply, demand and authoritative causes.");
	Snapshot.SelectedGood.ChartSummary = LOCTEXT("SelectGoodChart", "Select a good to inspect its price history.");
	Snapshot.SelectedGood.PinDisabledReason = LOCTEXT("PinSelectGood", "Select a good with a current market report first.");
	Snapshot.SelectedGood.RouteDisabledReason = LOCTEXT("RouteSelectGood", "Select a reported good before planning a route.");
}

void UHansaMarketTablePresentationModel::PublishIfChanged(const FHansaMarketTableSnapshot& Previous)
{
	if (Snapshot == Previous) return;
	++Revision;
	Changed.Broadcast(Snapshot, Revision);
}

#undef LOCTEXT_NAMESPACE
