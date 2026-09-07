#include "UI/HansaCityOverviewPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Market/HansaMarket.h"
#include "Population/HansaPopulation.h"
#include "Production/HansaProduction.h"
#include "Queries/HansaSimulationReadOnly.h"

#define LOCTEXT_NAMESPACE "HansaCityOverviewPresentationModel"

namespace
{
	bool TextEqual(const FText& Left, const FText& Right) { return Left.EqualTo(Right); }

	FText StableLabel(const FString& StableId)
	{
		FString Result = StableId;
		int32 Separator = INDEX_NONE;
		if (Result.FindLastChar(TEXT('.'), Separator)) Result.RightChopInline(Separator + 1);
		Result.ReplaceInline(TEXT("_"), TEXT(" "));
		return FText::FromString(Result);
	}

	FString SemanticSuffix(const FString& StableId)
	{
		FString Result = StableId;
		Result.ReplaceInline(TEXT("."), TEXT("_"));
		Result.ReplaceInline(TEXT("#"), TEXT("_"));
		Result.ReplaceInline(TEXT("@"), TEXT("_"));
		return Result;
	}

	FText Quantity(const int64 MilliUnits)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), static_cast<double>(MilliUnits) / 1000.0));
	}

	FText Money(const int64 MilliMarks)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f mk"), static_cast<double>(MilliMarks) / 1000.0));
	}

	FText Percent(const int32 BasisPoints)
	{
		return FText::FromString(FString::Printf(TEXT("%.0f%%"), static_cast<double>(BasisPoints) / 100.0));
	}

	FText ReserveDays(const int64 MilliDays)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f days"), static_cast<double>(MilliDays) / 1000.0));
	}

	FHansaCityOverviewSummaryPresentation Summary(const TCHAR* Id, const FText& Label, const FText& Value, const FText& Detail = FText())
	{
		FHansaCityOverviewSummaryPresentation Result;
		Result.StableId = Id;
		Result.Label = Label;
		Result.Value = Value;
		Result.Detail = Detail;
		return Result;
	}

	FHansaCityOverviewFieldPresentation Field(const TCHAR* Id, const FText& Label, const FText& Value)
	{
		FHansaCityOverviewFieldPresentation Result;
		Result.StableId = Id;
		Result.Label = Label;
		Result.Value = Value;
		return Result;
	}

	FName PopulationRowId(const Hansa::Simulation::FHansaPopulationCohortProjection& Cohort)
	{
		return FName(*FString::Printf(TEXT("Population.%llu"), static_cast<unsigned long long>(Cohort.Id.GetValue())));
	}

	FName ProductionRowId(const Hansa::Simulation::FHansaProductionProjection& Production)
	{
		return FName(*FString::Printf(TEXT("Production.%llu"), static_cast<unsigned long long>(Production.Id.GetValue())));
	}

	FName MarketRowId(const Hansa::Simulation::FHansaCityMarketProjection& Market)
	{
		return FName(*FString::Printf(TEXT("Market.%s"), *SemanticSuffix(Market.GoodId.ToString())));
	}

	const Hansa::Simulation::FHansaPopulationNeedState* WeakestNeed(
		const Hansa::Simulation::FHansaPopulationCohortProjection& Cohort)
	{
		const Hansa::Simulation::FHansaPopulationNeedState* Result = nullptr;
		for (const Hansa::Simulation::FHansaPopulationNeedState& Need : Cohort.Needs)
		{
			if (Result == nullptr || Need.SatisfactionBasisPoints < Result->SatisfactionBasisPoints) Result = &Need;
		}
		return Result;
	}
}

bool operator==(const FHansaCityOverviewSummaryPresentation& Left, const FHansaCityOverviewSummaryPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.Value, Right.Value) &&
		TextEqual(Left.Detail, Right.Detail) && Left.bWarning == Right.bWarning && Left.bError == Right.bError;
}

bool operator==(const FHansaCityOverviewFieldPresentation& Left, const FHansaCityOverviewFieldPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.Value, Right.Value);
}

bool operator==(const FHansaCityOverviewRowPresentation& Left, const FHansaCityOverviewRowPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Title, Right.Title) && TextEqual(Left.Subtitle, Right.Subtitle) &&
		Left.Fields == Right.Fields && TextEqual(Left.Status, Right.Status) &&
		TextEqual(Left.CausalActionLabel, Right.CausalActionLabel) &&
		TextEqual(Left.CausalActionDisabledReason, Right.CausalActionDisabledReason) &&
		Left.RelatedSemanticId == Right.RelatedSemanticId && Left.GoodStableId == Right.GoodStableId && Left.Kind == Right.Kind &&
		Left.BuildingValue == Right.BuildingValue && Left.bCausalActionEnabled == Right.bCausalActionEnabled &&
		Left.bWarning == Right.bWarning && Left.bError == Right.bError;
}

bool operator==(const FHansaCityOverviewSnapshot& Left, const FHansaCityOverviewSnapshot& Right)
{
	return Left.CityStableId == Right.CityStableId && TextEqual(Left.CityTitle, Right.CityTitle) &&
		Left.HeaderSummaries == Right.HeaderSummaries && Left.PopulationRows == Right.PopulationRows &&
		Left.ProductionRows == Right.ProductionRows && Left.MarketRows == Right.MarketRows &&
		TextEqual(Left.StateTitle, Right.StateTitle) && TextEqual(Left.StateDetail, Right.StateDetail) &&
		TextEqual(Left.LastActionResult, Right.LastActionResult) && Left.FocusOriginSemanticId == Right.FocusOriginSemanticId &&
		Left.FocusedSemanticId == Right.FocusedSemanticId && Left.SelectedRowStableId == Right.SelectedRowStableId &&
		Left.ActiveTab == Right.ActiveTab && Left.LoadState == Right.LoadState && Left.bOpen == Right.bOpen;
}

void UHansaCityOverviewPresentationModel::InitializeDefaults()
{
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot = {};
	Snapshot.CityStableId = TEXT("City.Lubeck");
	Snapshot.CityTitle = LOCTEXT("DefaultTitle", "Lübeck City Overview");
	Snapshot.HeaderSummaries = {
		Summary(TEXT("PopulationTrend"), LOCTEXT("PopulationTrend", "Population trend"), LOCTEXT("NoPopulation", "No residents"), LOCTEXT("Stable", "→ Stable")),
		Summary(TEXT("TreasuryContribution"), LOCTEXT("TreasuryContribution", "Treasury contribution"), Money(0), LOCTEXT("PerTick", "per simulation tick")),
		Summary(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"), LOCTEXT("NoSatisfaction", "—"), LOCTEXT("NoCohorts", "No cohorts")),
		Summary(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce"), LOCTEXT("NoWorkforce", "0 available"), LOCTEXT("NoAssigned", "0 assigned")),
		Summary(TEXT("StapleReserve"), LOCTEXT("StapleReserve", "Staple reserve"), ReserveDays(0), LOCTEXT("NoDemand", "No current demand")),
		Summary(TEXT("Alerts"), LOCTEXT("Alerts", "Alerts"), LOCTEXT("NoAlerts", "0 active"), LOCTEXT("AllClear", "✓ All clear"))
	};
	Snapshot.StateTitle = LOCTEXT("EmptyTitle", "No city records yet");
	Snapshot.StateDetail = LOCTEXT("EmptyDetail", "Build or connect eligible buildings to populate this view.");
	Snapshot.LoadState = EHansaCityOverviewLoadState::Empty;
	PublishIfChanged(Previous);
}

bool UHansaCityOverviewPresentationModel::ApplyProjection(
	const Hansa::Simulation::FHansaSimulationProjection& Projection,
	const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const Hansa::Simulation::FHansaCityDefinitionId CityId,
	FText CityDisplayName,
	const int64 TreasuryContributionMilliMarks)
{
	using namespace Hansa::Simulation;
	if (!CityId.IsValid()) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	const bool bWasOpen = Snapshot.bOpen;
	const EHansaCityOverviewTab ActiveTab = Snapshot.ActiveTab;
	const FName FocusOrigin = Snapshot.FocusOriginSemanticId;
	const FName Focused = Snapshot.FocusedSemanticId;
	const FName Selected = Snapshot.SelectedRowStableId;
	Snapshot = {};
	Snapshot.bOpen = bWasOpen;
	Snapshot.ActiveTab = ActiveTab;
	Snapshot.FocusOriginSemanticId = FocusOrigin;
	Snapshot.FocusedSemanticId = Focused;
	Snapshot.SelectedRowStableId = Selected;
	Snapshot.CityStableId = FName(*CityId.ToString());
	Snapshot.CityTitle = FText::Format(LOCTEXT("CityTitleFormat", "{0} City Overview"), CityDisplayName.IsEmpty() ? StableLabel(CityId.ToString()) : CityDisplayName);

	const FHansaCityPopulationProjection* CityPopulation = nullptr;
	for (const FHansaCityPopulationProjection& Item : Projection.GetCityPopulations())
	{
		if (Item.CityId == CityId) { CityPopulation = &Item; break; }
	}
	int32 AlertCount = 0;
	bool bCriticalAlert = false;
	for (const FHansaMarketAlertProjection& Alert : Projection.GetActiveMarketAlerts())
	{
		if (Alert.CityId != CityId) continue;
		++AlertCount;
		bCriticalAlert |= Alert.Severity == EHansaMarketAlertSeverity::Critical;
	}

	if (CityPopulation != nullptr)
	{
		const TCHAR* TrendGlyph = CityPopulation->Trend == EHansaPopulationTrend::Growing ? TEXT("↑") :
			(CityPopulation->Trend == EHansaPopulationTrend::Declining ? TEXT("↓") : TEXT("→"));
		FHansaCityOverviewSummaryPresentation PopulationSummary = Summary(TEXT("PopulationTrend"), LOCTEXT("PopulationTrend", "Population trend"),
			FText::AsNumber(CityPopulation->TotalResidents), FText::FromString(FString::Printf(TEXT("%s %s (%+d)"), TrendGlyph,
				LexToString(CityPopulation->Trend), CityPopulation->ResidentChangeLastTick)));
		PopulationSummary.bWarning = CityPopulation->Trend == EHansaPopulationTrend::Declining;
		FHansaCityOverviewSummaryPresentation SatisfactionSummary = Summary(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"),
			Percent(CityPopulation->SatisfactionBasisPoints), CityPopulation->SatisfactionBasisPoints >= 8000 ? LOCTEXT("Satisfied", "✓ Stable") : LOCTEXT("NeedsAttention", "△ Needs attention"));
		SatisfactionSummary.bWarning = CityPopulation->SatisfactionBasisPoints < 8000;
		FHansaCityOverviewSummaryPresentation ReserveSummary = Summary(TEXT("StapleReserve"), LOCTEXT("StapleReserve", "Staple reserve"),
			ReserveDays(CityPopulation->StapleReserveMilliDays), LOCTEXT("StapleDetail", "bread and fish safety"));
		ReserveSummary.bWarning = CityPopulation->StapleReserveMilliDays < 5000;
		Snapshot.HeaderSummaries = {
			MoveTemp(PopulationSummary),
			Summary(TEXT("TreasuryContribution"), LOCTEXT("TreasuryContribution", "Treasury contribution"), Money(TreasuryContributionMilliMarks), LOCTEXT("PerTick", "per simulation tick")),
			MoveTemp(SatisfactionSummary),
			Summary(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce"),
				FText::Format(LOCTEXT("WorkforceAvailable", "{0} available"), FText::AsNumber(CityPopulation->LaborerWorkforceAvailable + CityPopulation->ArtisanWorkforceAvailable)),
				FText::Format(LOCTEXT("WorkforceAssigned", "{0} assigned"), FText::AsNumber(CityPopulation->LaborerWorkforceAssigned + CityPopulation->ArtisanWorkforceAssigned))),
			MoveTemp(ReserveSummary),
			Summary(TEXT("Alerts"), LOCTEXT("Alerts", "Alerts"), FText::Format(LOCTEXT("AlertCount", "{0} active"), FText::AsNumber(AlertCount)),
				AlertCount == 0 ? LOCTEXT("AllClear", "✓ All clear") : (bCriticalAlert ? LOCTEXT("CriticalAlerts", "! Critical") : LOCTEXT("WarningAlerts", "△ Warning")))
		};
		Snapshot.HeaderSummaries.Last().bWarning = AlertCount > 0;
		Snapshot.HeaderSummaries.Last().bError = bCriticalAlert;
	}
	else
	{
		Snapshot.HeaderSummaries = {
			Summary(TEXT("PopulationTrend"), LOCTEXT("PopulationTrend", "Population trend"), LOCTEXT("NoPopulation", "No residents"), LOCTEXT("Stable", "→ Stable")),
			Summary(TEXT("TreasuryContribution"), LOCTEXT("TreasuryContribution", "Treasury contribution"), Money(TreasuryContributionMilliMarks), LOCTEXT("PerTick", "per simulation tick")),
			Summary(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"), LOCTEXT("NoSatisfaction", "—"), LOCTEXT("NoCohorts", "No cohorts")),
			Summary(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce"), LOCTEXT("NoWorkforce", "0 available"), LOCTEXT("NoAssigned", "0 assigned")),
			Summary(TEXT("StapleReserve"), LOCTEXT("StapleReserve", "Staple reserve"), ReserveDays(0), LOCTEXT("NoDemand", "No current demand")),
			Summary(TEXT("Alerts"), LOCTEXT("Alerts", "Alerts"), FText::Format(LOCTEXT("AlertCount", "{0} active"), FText::AsNumber(AlertCount)), AlertCount == 0 ? LOCTEXT("AllClear", "✓ All clear") : LOCTEXT("WarningAlerts", "△ Warning"))
		};
		Snapshot.HeaderSummaries.Last().bWarning = AlertCount > 0;
		Snapshot.HeaderSummaries.Last().bError = bCriticalAlert;
	}

	for (const FHansaPopulationCohortProjection& Cohort : Projection.GetPopulationCohorts())
	{
		if (Cohort.CityId != CityId) continue;
		FHansaCityOverviewRowPresentation Row;
		Row.StableId = PopulationRowId(Cohort);
		Row.Kind = EHansaCityOverviewRowKind::Population;
		Row.BuildingValue = static_cast<int64>(Cohort.ResidenceBuildingId.GetValue());
		Row.Title = StableLabel(Cohort.TierId.ToString());
		Row.Subtitle = FText::Format(LOCTEXT("PopulationSubtitle", "Residence {0} · {1}"), FText::AsNumber(Row.BuildingValue),
			Cohort.bHasMarketAccess ? LOCTEXT("MarketAccess", "market access") : LOCTEXT("NoMarketAccess", "no market access"));
		Row.Fields = {
			Field(TEXT("Residents"), LOCTEXT("Residents", "Residents / capacity"), FText::Format(LOCTEXT("ResidentsValue", "{0} / {1}"), FText::AsNumber(Cohort.Residents), FText::AsNumber(Cohort.ResidenceCapacity))),
			Field(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce supplied"), FText::AsNumber(Cohort.WorkforceSupply)),
			Field(TEXT("Migration"), LOCTEXT("Migration", "Migration this tick"), FText::AsNumber(Cohort.ResidentChangeLastTick)),
			Field(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"), Percent(Cohort.SatisfactionBasisPoints))
		};
		if (const FHansaPopulationNeedState* Need = WeakestNeed(Cohort))
		{
			Row.GoodStableId = FName(*Need->GoodId.ToString());
			Row.Fields.Add(Field(TEXT("Need"), StableLabel(Need->NeedId.ToString()), FText::Format(
				LOCTEXT("NeedBreakdown", "access {0} · affordability {1} · reliability {2} · consumed {3} · reserve {4}"),
				Percent(Need->AccessBasisPoints), Percent(Need->AffordabilityBasisPoints), Percent(Need->ReliabilityBasisPoints),
				Quantity(Need->ConsumedLastTick.GetRawValue()), ReserveDays(Need->ReserveMilliDays))));
			Row.bWarning = Need->SatisfactionBasisPoints < 8000;
			Row.bError = Need->SatisfactionBasisPoints < 4000;
			Row.Status = Row.bError ? LOCTEXT("CriticalNeed", "! Critical need") : (Row.bWarning ? LOCTEXT("WarningNeed", "△ Need warning") : LOCTEXT("StableNeed", "✓ Needs stable"));
			if (Need->GoodId.IsValid()) Row.RelatedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Production.Good.%s"), *SemanticSuffix(Need->GoodId.ToString())));
		}
		else Row.Status = LOCTEXT("NoNeeds", "No configured needs");
		Row.CausalActionLabel = LOCTEXT("RevealSupplyingChain", "Reveal supplying chain");
		Row.bCausalActionEnabled = !Row.RelatedSemanticId.IsNone();
		if (!Row.bCausalActionEnabled) Row.CausalActionDisabledReason = LOCTEXT("NoSupplyingChain", "No supplying chain is available for this need.");
		Snapshot.PopulationRows.Add(MoveTemp(Row));
	}

	for (const FHansaProductionProjection& Production : Projection.GetProductions())
	{
		if (Production.CityId != CityId) continue;
		FHansaCityOverviewRowPresentation Row;
		Row.StableId = ProductionRowId(Production);
		Row.Kind = EHansaCityOverviewRowKind::Production;
		Row.BuildingValue = static_cast<int64>(Production.BuildingId.GetValue());
		Row.Title = Production.RecipeId.IsValid() ? StableLabel(Production.RecipeId.ToString()) : StableLabel(Production.Outputs.IsEmpty() ? TEXT("Background supply") : Production.Outputs[0].GoodId.ToString());
		Row.Subtitle = FText::Format(LOCTEXT("ProductionSubtitle", "Building {0} · {1}"), FText::AsNumber(Row.BuildingValue),
			Production.bActive ? LOCTEXT("Operating", "operating") : LOCTEXT("Paused", "paused"));
		if (!Production.Outputs.IsEmpty())
		{
			const FHansaProductionThroughputProjection& Output = Production.Outputs[0];
			Row.GoodStableId = FName(*Output.GoodId.ToString());
			const int64 Nominal = Output.NominalQuantityPerCycle.GetRawValue();
			const int32 Utilization = Nominal > 0 ? static_cast<int32>(FMath::Clamp<int64>(Output.ActualQuantityLastTick.GetRawValue() * 10000 / Nominal, 0, 10000)) : 0;
			Row.Fields.Add(Field(TEXT("Throughput"), LOCTEXT("Throughput", "Actual / nominal throughput"),
				FText::Format(LOCTEXT("ThroughputValue", "{0} / {1} per cycle"), Quantity(Output.ActualQuantityLastTick.GetRawValue()), Quantity(Nominal))));
			Row.Fields.Add(Field(TEXT("Utilization"), LOCTEXT("Utilization", "Utilization"), Percent(Utilization)));
			Row.RelatedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Market.Good.%s"), *SemanticSuffix(Output.GoodId.ToString())));
		}
		Row.Fields.Add(Field(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce assigned / required"),
			FText::Format(LOCTEXT("ProductionWorkforce", "Laborers {0}/{1} · Artisans {2}/{3}"),
				FText::AsNumber(Production.AllocatedLaborerWorkforce), FText::AsNumber(Production.RequiredLaborerWorkforce),
				FText::AsNumber(Production.AllocatedArtisanWorkforce), FText::AsNumber(Production.RequiredArtisanWorkforce))));
		Row.bWarning = Production.Blocker != EHansaProductionBlocker::None;
		Row.bError = Production.Blocker == EHansaProductionBlocker::InventoryTransactionFailed;
		Row.Status = !Row.bWarning ? LOCTEXT("ProductionReady", "✓ Operating") : FText::Format(LOCTEXT("ProductionBlocked", "△ {0}"), FText::FromString(LexToString(Production.Blocker)));
		Row.CausalActionLabel = LOCTEXT("RevealBuildings", "Reveal buildings");
		Row.bCausalActionEnabled = Production.BuildingId.IsValid();
		Row.RelatedSemanticId = Row.bCausalActionEnabled ? FName(*FString::Printf(TEXT("Inspector.Building.%lld"), Row.BuildingValue)) : Row.RelatedSemanticId;
		if (!Row.bCausalActionEnabled) Row.CausalActionDisabledReason = LOCTEXT("NoProductionBuilding", "Background supply has no city building to reveal.");
		Snapshot.ProductionRows.Add(MoveTemp(Row));
	}

	for (const FHansaCityMarketProjection& Market : Projection.GetMarkets())
	{
		if (Market.CityId != CityId) continue;
		FHansaCityOverviewRowPresentation Row;
		Row.StableId = MarketRowId(Market);
		Row.Kind = EHansaCityOverviewRowKind::Market;
		Row.GoodStableId = FName(*Market.GoodId.ToString());
		Row.Title = StableLabel(Market.GoodId.ToString());
		Row.Subtitle = Market.bIsStale ? LOCTEXT("StaleMarket", "◷ Stale local report") : LOCTEXT("CurrentMarket", "● Current local report");
		Row.Fields = {
			Field(TEXT("Stock"), LOCTEXT("Stock", "Stock / desired reserve"), FText::Format(LOCTEXT("StockValue", "{0} / {1}"), Quantity(Market.CurrentStock.GetRawValue()), Quantity(Market.DesiredReserve.GetRawValue()))),
			Field(TEXT("Demand"), LOCTEXT("Demand", "Citizen / industrial demand"), FText::Format(LOCTEXT("DemandValue", "{0} / {1}"), Quantity(Market.CitizenDemand.GetRawValue()), Quantity(Market.IndustrialDemand.GetRawValue()))),
			Field(TEXT("Incoming"), LOCTEXT("Incoming", "Confirmed incoming"), Quantity(Market.ExpectedIncomingSupply.GetRawValue())),
			Field(TEXT("Price"), LOCTEXT("Price", "Local price / recent average"), FText::Format(LOCTEXT("PriceValue", "{0} / {1}"), Money(Market.CurrentPriceMilliMarks), Money(Market.RecentAveragePriceMilliMarks)))
		};
		const bool bBelowReserve = Market.CurrentStock.GetRawValue() < Market.DesiredReserve.GetRawValue();
		Row.bWarning = bBelowReserve || Market.bIsStale;
		Row.Status = bBelowReserve ? LOCTEXT("LowReserve", "△ Low reserve") : (Market.bIsStale ? LOCTEXT("Stale", "◷ Stale") : LOCTEXT("MarketStable", "✓ Stable"));
		Row.CausalActionLabel = LOCTEXT("ReviewSupply", "Review supply");
		Row.RelatedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Production.Good.%s"), *SemanticSuffix(Market.GoodId.ToString())));
		Row.bCausalActionEnabled = true;
		Snapshot.MarketRows.Add(MoveTemp(Row));
	}

	UpdateStateForActiveRows();
	PublishIfChanged(Previous);
	return true;
}

TConstArrayView<FHansaCityOverviewRowPresentation> UHansaCityOverviewPresentationModel::GetActiveRows() const
{
	switch (Snapshot.ActiveTab)
	{
	case EHansaCityOverviewTab::Production: return Snapshot.ProductionRows;
	case EHansaCityOverviewTab::Market: return Snapshot.MarketRows;
	default: return Snapshot.PopulationRows;
	}
}

const FHansaCityOverviewRowPresentation* UHansaCityOverviewPresentationModel::FindActiveRow(const FName StableId) const
{
	for (const FHansaCityOverviewRowPresentation& Row : GetActiveRows()) if (Row.StableId == StableId) return &Row;
	return nullptr;
}

bool UHansaCityOverviewPresentationModel::Open(const FName FocusOriginSemanticId)
{
	if (Snapshot.bOpen) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.bOpen = true;
	Snapshot.FocusOriginSemanticId = FocusOriginSemanticId;
	Snapshot.FocusedSemanticId = TEXT("CityOverview.Close");
	PublishIfChanged(Previous);
	return true;
}

bool UHansaCityOverviewPresentationModel::CloseIntent()
{
	if (!Snapshot.bOpen) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.bOpen = false;
	Snapshot.FocusedSemanticId = Snapshot.FocusOriginSemanticId;
	PublishIfChanged(Previous);
	FocusRestoreRequested.Broadcast(Snapshot.FocusOriginSemanticId);
	return true;
}

bool UHansaCityOverviewPresentationModel::SelectTabIntent(const EHansaCityOverviewTab Tab)
{
	if (!Snapshot.bOpen || Snapshot.ActiveTab == Tab) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.ActiveTab = Tab;
	Snapshot.SelectedRowStableId = NAME_None;
	Snapshot.FocusedSemanticId = FName(Tab == EHansaCityOverviewTab::Population ? TEXT("CityOverview.Tab.Population") :
		(Tab == EHansaCityOverviewTab::Production ? TEXT("CityOverview.Tab.Production") : TEXT("CityOverview.Tab.Market")));
	UpdateStateForActiveRows();
	PublishIfChanged(Previous);
	return true;
}

bool UHansaCityOverviewPresentationModel::CycleTabIntent(const int32 Direction)
{
	if (!Snapshot.bOpen || Direction == 0) return false;
	const int32 Current = static_cast<int32>(Snapshot.ActiveTab);
	const int32 Next = (Current + (Direction > 0 ? 1 : 2)) % 3;
	return SelectTabIntent(static_cast<EHansaCityOverviewTab>(Next));
}

bool UHansaCityOverviewPresentationModel::SelectRowIntent(const FName RowStableId)
{
	if (!Snapshot.bOpen || FindActiveRow(RowStableId) == nullptr) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.SelectedRowStableId = RowStableId;
	Snapshot.FocusedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Row.%s"), *SemanticSuffix(RowStableId.ToString())));
	PublishIfChanged(Previous);
	return true;
}

bool UHansaCityOverviewPresentationModel::ActivateCausalIntent(const FName RowStableId)
{
	const FHansaCityOverviewRowPresentation* Row = FindActiveRow(RowStableId);
	if (!Snapshot.bOpen || Row == nullptr || !Row->bCausalActionEnabled || Row->RelatedSemanticId.IsNone()) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.SelectedRowStableId = RowStableId;
	const FName RelatedId = Row->RelatedSemanticId;
	const FName GoodStableId = Row->GoodStableId;
	const int64 BuildingValue = Row->BuildingValue;
	if (RelatedId.ToString().StartsWith(TEXT("CityOverview.Production.Good.")))
	{
		Snapshot.ActiveTab = EHansaCityOverviewTab::Production;
		const FHansaCityOverviewRowPresentation* SupplyingProduction = Snapshot.ProductionRows.FindByPredicate(
			[GoodStableId](const FHansaCityOverviewRowPresentation& Candidate) { return Candidate.GoodStableId == GoodStableId; });
		Snapshot.SelectedRowStableId = SupplyingProduction != nullptr ? SupplyingProduction->StableId : NAME_None;
		Snapshot.FocusedSemanticId = SupplyingProduction != nullptr
			? FName(*FString::Printf(TEXT("CityOverview.Row.%s"), *SemanticSuffix(SupplyingProduction->StableId.ToString())))
			: FName(TEXT("CityOverview.List"));
		UpdateStateForActiveRows();
	}
	else
	{
		Snapshot.FocusedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Row.%s.Reveal"), *SemanticSuffix(RowStableId.ToString())));
	}
	Snapshot.LastActionResult = FText::Format(LOCTEXT("OpenedRelated", "Opened related view · {0}"), FText::FromName(RelatedId));
	PublishIfChanged(Previous);
	RelatedTargetRequested.Broadcast(RelatedId, BuildingValue);
	return true;
}

bool UHansaCityOverviewPresentationModel::RetryIntent()
{
	if (!Snapshot.bOpen || Snapshot.LoadState != EHansaCityOverviewLoadState::Error) return false;
	SetLoading(Snapshot.CityTitle);
	return true;
}

void UHansaCityOverviewPresentationModel::SetFocusedSemanticId(const FName SemanticId)
{
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.FocusedSemanticId = SemanticId;
	PublishIfChanged(Previous);
}

void UHansaCityOverviewPresentationModel::SetLoading(FText CityDisplayName)
{
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	if (!CityDisplayName.IsEmpty()) Snapshot.CityTitle = MoveTemp(CityDisplayName);
	Snapshot.LoadState = EHansaCityOverviewLoadState::Loading;
	Snapshot.StateTitle = LOCTEXT("LoadingTitle", "Loading city data");
	Snapshot.StateDetail = LOCTEXT("LoadingDetail", "The latest Lübeck report is being prepared.");
	PublishIfChanged(Previous);
}

void UHansaCityOverviewPresentationModel::SetError(FText Cause, FText Remedy)
{
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.LoadState = EHansaCityOverviewLoadState::Error;
	Snapshot.StateTitle = LOCTEXT("ErrorTitle", "! City report unavailable");
	Snapshot.StateDetail = FText::Format(LOCTEXT("ErrorDetail", "Cause: {0}\nRemedy: {1}"), Cause, Remedy);
	PublishIfChanged(Previous);
}

void UHansaCityOverviewPresentationModel::UpdateStateForActiveRows()
{
	if (GetActiveRows().IsEmpty())
	{
		Snapshot.LoadState = EHansaCityOverviewLoadState::Empty;
		Snapshot.StateTitle = LOCTEXT("EmptyTitle", "No city records yet");
		Snapshot.StateDetail = LOCTEXT("EmptyDetail", "Build or connect eligible buildings to populate this view.");
	}
	else
	{
		Snapshot.LoadState = EHansaCityOverviewLoadState::Ready;
		Snapshot.StateTitle = FText::GetEmpty();
		Snapshot.StateDetail = FText::GetEmpty();
		if (!Snapshot.SelectedRowStableId.IsNone() && FindActiveRow(Snapshot.SelectedRowStableId) == nullptr) Snapshot.SelectedRowStableId = NAME_None;
	}
}

void UHansaCityOverviewPresentationModel::PublishIfChanged(const FHansaCityOverviewSnapshot& Previous)
{
	if (!(Previous == Snapshot))
	{
		++Revision;
		Changed.Broadcast(Snapshot, Revision);
	}
}

#undef LOCTEXT_NAMESPACE
