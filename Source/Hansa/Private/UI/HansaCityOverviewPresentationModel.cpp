#include "UI/HansaCityOverviewPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Market/HansaMarket.h"
#include "World/HansaRuntimeSimulationHost.h"
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
        for(int32 I=Result.Len()-1;I>0;--I)if(FChar::IsUpper(Result[I]) && FChar::IsLower(Result[I-1]))Result.InsertAt(I,TEXT(' '));
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
			if (Result == nullptr || Need.SatisfactionBasisPoints < Result->SatisfactionBasisPoints ||
                (Need.SatisfactionBasisPoints==Result->SatisfactionBasisPoints && Need.GoodId.IsValid() &&
                 (!Result->GoodId.IsValid() || Need.ReserveMilliDays<Result->ReserveMilliDays))) Result = &Need;
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
		Summary(TEXT("PopulationTrend"), LOCTEXT("PopulationTrend", "Population trend"), LOCTEXT("NoPopulation", "No residents"), LOCTEXT("Stable", "Stable")),
		Summary(TEXT("TreasuryContribution"), LOCTEXT("TreasuryContribution", "Treasury contribution"), Money(0), LOCTEXT("PerTick", "per simulation tick")),
		Summary(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"), LOCTEXT("NoSatisfaction", "—"), LOCTEXT("NoCohorts", "No cohorts")),
		Summary(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce"), LOCTEXT("NoWorkforce", "0 available"), LOCTEXT("NoAssigned", "0 assigned")),
		Summary(TEXT("StapleReserve"), LOCTEXT("StapleReserve", "Staple reserve"), ReserveDays(0), LOCTEXT("NoDemand", "No current demand")),
		Summary(TEXT("Alerts"), LOCTEXT("Alerts", "Alerts"), LOCTEXT("NoAlerts", "0 active"), LOCTEXT("AllClear", "All clear"))
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
	const int64 TreasuryContributionMilliMarks, const UHansaRuntimeSimulationHost* KnowledgeSource)
{
	using namespace Hansa::Simulation;
	if (!CityId.IsValid()) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	const bool bWasOpen = Snapshot.bOpen;
	const EHansaCityOverviewTab ActiveTab = Snapshot.ActiveTab;
	const FName FocusOrigin = Snapshot.FocusOriginSemanticId;
	const FName Focused = Snapshot.FocusedSemanticId;
	const FName Selected = Snapshot.CityStableId==FName(*CityId.ToString())?Snapshot.SelectedRowStableId:NAME_None;
	Snapshot = {};
	Snapshot.bOpen = bWasOpen;
	Snapshot.ActiveTab = ActiveTab;
	Snapshot.FocusOriginSemanticId = FocusOrigin;
	Snapshot.FocusedSemanticId = Focused;
	Snapshot.SelectedRowStableId = Selected;
	Snapshot.CityStableId = FName(*CityId.ToString());
    const bool Remote = Snapshot.CityStableId != TEXT("City.Lubeck");
    Snapshot.LastActionResult = Previous.LastActionResult;
	Snapshot.CityTitle = FText::Format(LOCTEXT("CityTitleFormat", "{0} City Overview"), CityDisplayName.IsEmpty() ? StableLabel(CityId.ToString()) : CityDisplayName);

	const FHansaCityPopulationProjection* CityPopulation = nullptr;
	for (const FHansaCityPopulationProjection& Item : Projection.GetCityPopulations())
	{
		if (!Remote && Item.CityId == CityId) { CityPopulation = &Item; break; }
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
		const TCHAR* TrendGlyph = CityPopulation->Trend == EHansaPopulationTrend::Growing ? TEXT("") :
			(CityPopulation->Trend == EHansaPopulationTrend::Declining ? TEXT("") : TEXT(""));
		FHansaCityOverviewSummaryPresentation PopulationSummary = Summary(TEXT("PopulationTrend"), LOCTEXT("PopulationTrend", "Population trend"),
			FText::AsNumber(CityPopulation->TotalResidents), FText::FromString(FString::Printf(TEXT("%s %s (%+d)"), TrendGlyph,
				LexToString(CityPopulation->Trend), CityPopulation->ResidentChangeLastTick)));
		PopulationSummary.bWarning = CityPopulation->Trend == EHansaPopulationTrend::Declining;
		FHansaCityOverviewSummaryPresentation SatisfactionSummary = Summary(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"),
			Percent(CityPopulation->SatisfactionBasisPoints), CityPopulation->SatisfactionBasisPoints >= 8000 ? LOCTEXT("Satisfied", "Stable") : LOCTEXT("NeedsAttention", "Needs attention"));
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
				AlertCount == 0 ? LOCTEXT("AllClear", "All clear") : (bCriticalAlert ? LOCTEXT("CriticalAlerts", "! Critical") : LOCTEXT("WarningAlerts", "Warning")))
		};
		Snapshot.HeaderSummaries.Last().bWarning = AlertCount > 0;
		Snapshot.HeaderSummaries.Last().bError = bCriticalAlert;
	}
	else
	{
		Snapshot.HeaderSummaries = {
			Summary(TEXT("PopulationTrend"), LOCTEXT("PopulationTrend", "Population trend"), LOCTEXT("NoPopulation", "No residents"), LOCTEXT("Stable", "Stable")),
			Summary(TEXT("TreasuryContribution"), LOCTEXT("TreasuryContribution", "Treasury contribution"), Money(TreasuryContributionMilliMarks), LOCTEXT("PerTick", "per simulation tick")),
			Summary(TEXT("Satisfaction"), LOCTEXT("Satisfaction", "Satisfaction"), LOCTEXT("NoSatisfaction", "—"), LOCTEXT("NoCohorts", "No cohorts")),
			Summary(TEXT("Workforce"), LOCTEXT("Workforce", "Workforce"), LOCTEXT("NoWorkforce", "0 available"), LOCTEXT("NoAssigned", "0 assigned")),
			Summary(TEXT("StapleReserve"), LOCTEXT("StapleReserve", "Staple reserve"), ReserveDays(0), LOCTEXT("NoDemand", "No current demand")),
			Summary(TEXT("Alerts"), LOCTEXT("Alerts", "Alerts"), FText::Format(LOCTEXT("AlertCount", "{0} active"), FText::AsNumber(AlertCount)), AlertCount == 0 ? LOCTEXT("AllClear", "All clear") : LOCTEXT("WarningAlerts", "Warning"))
		};
		Snapshot.HeaderSummaries.Last().bWarning = AlertCount > 0;
		Snapshot.HeaderSummaries.Last().bError = bCriticalAlert;
	}

    if (!CityPopulation) for(auto& Card:Snapshot.HeaderSummaries) {
        Card.Value=LOCTEXT("Unavailable","Unavailable");
        Card.Detail=Remote?LOCTEXT("RemoteUnavailable","No civic report received"):LOCTEXT("NoCivicProjection","No population report available");
    }
    if (CityPopulation) {
        Snapshot.HeaderSummaries[0].Detail=FText::Format(LOCTEXT("TierTrend","{0} · Laborers {1} / Artisans {2}"),Snapshot.HeaderSummaries[0].Detail,FText::AsNumber(CityPopulation->LaborerResidents),FText::AsNumber(CityPopulation->ArtisanResidents));
        Snapshot.HeaderSummaries[3].Detail=FText::Format(LOCTEXT("TierEmployment","Assigned / supplied · Laborers {0}/{1} · Artisans {2}/{3}"),FText::AsNumber(CityPopulation->LaborerWorkforceAssigned),FText::AsNumber(CityPopulation->LaborerWorkforceSupply),FText::AsNumber(CityPopulation->ArtisanWorkforceAssigned),FText::AsNumber(CityPopulation->ArtisanWorkforceSupply));
        Snapshot.HeaderSummaries[1].Value=LOCTEXT("Unavailable","Unavailable");
        Snapshot.HeaderSummaries[1].Detail=LOCTEXT("NoTreasuryProjection","City contribution is not reported");
    }
	for (const FHansaPopulationCohortProjection& Cohort : Projection.GetPopulationCohorts())
	{
		if (Remote || Cohort.CityId != CityId) continue;
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
			Row.Status = Row.bError ? LOCTEXT("CriticalNeed", "! Critical need") : (Row.bWarning ? LOCTEXT("WarningNeed", "Need warning") : LOCTEXT("StableNeed", "Needs stable"));
			if (Need->GoodId.IsValid()) Row.RelatedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Production.Good.%s"), *SemanticSuffix(Need->GoodId.ToString())));
		}
        else Row.Status = LOCTEXT("NoNeeds", "No configured needs");
        for(const auto& Need:Cohort.Needs) {
            if(&Need==WeakestNeed(Cohort))continue;
            auto Entry=Field(TEXT("Need"),StableLabel(Need.NeedId.ToString()),FText::Format(LOCTEXT("NeedBreakdown", "access {0} · affordability {1} · reliability {2} · consumed {3} · reserve {4}"),Percent(Need.AccessBasisPoints),Percent(Need.AffordabilityBasisPoints),Percent(Need.ReliabilityBasisPoints),Quantity(Need.ConsumedLastTick.GetRawValue()),ReserveDays(Need.ReserveMilliDays)));
            if(!Need.GoodId.IsValid())Entry.Value=FText::Format(LOCTEXT("ServiceNeed","access {0} · affordability {1} · reliability {2} · service need"),Percent(Need.AccessBasisPoints),Percent(Need.AffordabilityBasisPoints),Percent(Need.ReliabilityBasisPoints));
            Entry.StableId=FName(*SemanticSuffix(Need.NeedId.ToString()));Row.Fields.Add(Entry);
        }
        if (KnowledgeSource && Registry.FindNeed(TEXT("Need.Heating")) && KnowledgeSource->QueryHeating().SeasonMultiplier==0)
            Row.Fields.Add(Field(TEXT("Need_Heating"),LOCTEXT("HeatingLabel","Heating"),LOCTEXT("SummerHeating","Not needed this season; stockpile for winter.")));
        Row.Fields.Add(Field(TEXT("GrowthCause"),LOCTEXT("GrowthCause","Growth / decline context"),
            !Cohort.bResidenceOperational?LOCTEXT("ResidenceNotReady","Residence is not operational"):
            !Cohort.bHasMarketAccess?LOCTEXT("NoGrowthAccess","Market access is missing"):
            Cohort.ResidentChangeLastTick<0?LOCTEXT("DeclineContext","Residents declined; inspect the weakest need and its access, affordability and reliability"):
            Cohort.Residents>=Cohort.ResidenceCapacity?LOCTEXT("AtCapacity","Housing capacity reached"):
            Cohort.ResidentChangeLastTick>0?LOCTEXT("GrowingContext","Residents increased; needs and housing permit growth"):
            LOCTEXT("StableContext","No migration this tick; inspect needs and available housing")));

		Row.CausalActionLabel = LOCTEXT("RevealSupplyingChain", "Reveal supplying chain");
		Row.bCausalActionEnabled = !Row.RelatedSemanticId.IsNone();
		if (!Row.bCausalActionEnabled) Row.CausalActionDisabledReason = LOCTEXT("NoSupplyingChain", "No supplying chain is available for this need.");
		Snapshot.PopulationRows.Add(MoveTemp(Row));
	}

	for (const FHansaProductionProjection& Production : Projection.GetProductions())
	{
		if (Remote || Production.CityId != CityId) continue;
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
		Row.Status = !Row.bWarning ? LOCTEXT("ProductionReady", "Operating") : FText::Format(LOCTEXT("ProductionBlocked", "{0}"), FText::FromString(LexToString(Production.Blocker)));
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
		Row.Subtitle = Market.bIsStale ? LOCTEXT("StaleMarket", "Stale local report") : LOCTEXT("CurrentMarket", "Current local report");
		Row.Fields = {
			Field(TEXT("Stock"), LOCTEXT("Stock", "Stock / desired reserve"), FText::Format(LOCTEXT("StockValue", "{0} / {1}"), Quantity(Market.CurrentStock.GetRawValue()), Quantity(Market.DesiredReserve.GetRawValue()))),
			Field(TEXT("Demand"), LOCTEXT("Demand", "Citizen / industrial demand"), FText::Format(LOCTEXT("DemandValue", "{0} / {1}"), Quantity(Market.CitizenDemand.GetRawValue()), Quantity(Market.IndustrialDemand.GetRawValue()))),
			Field(TEXT("Incoming"), LOCTEXT("Incoming", "Confirmed incoming"), Quantity(Market.ExpectedIncomingSupply.GetRawValue())),
			Field(TEXT("Price"), LOCTEXT("Price", "Local price / recent average"), FText::Format(LOCTEXT("PriceValue", "{0} / {1}"), Money(Market.CurrentPriceMilliMarks), Money(Market.RecentAveragePriceMilliMarks)))
		};
		int64 Produced = 0;
        bool bProductionKnown = true;
        for (const auto& Production : Projection.GetProductions())
        {
            if (Production.CityId != CityId || Production.Kind != EHansaProductionKind::BuildingRecipe) continue;
            for (const auto& Output : Production.Outputs)
            {
                if (Output.GoodId != Market.GoodId) continue;
                const int64 PerCycle = Output.NominalQuantityPerCycle.GetRawValue();
                if (PerCycle <= 0) continue;
                if (Production.CompletedCycles > uint64((MAX_int64 - Produced) / PerCycle))
                {
                    bProductionKnown = false;
                    break;
                }
                Produced += int64(Production.CompletedCycles) * PerCycle;
            }
            if (!bProductionKnown) break;
        }
        Row.Fields.Add(Field(TEXT("Produced"), LOCTEXT("Produced", "Produced so far (local buildings)"),
            bProductionKnown ? Quantity(Produced) : LOCTEXT("ProducedUnknown", "Unavailable")));
        Row.Fields.Add(Field(TEXT("StockMeaning"), LOCTEXT("StockMeaning", "Stock accounting"),
            LOCTEXT("StockMeaningValue", "Available now; excludes consumed, exported and reserved goods")));
        const bool bBelowReserve = Market.CurrentStock.GetRawValue() < Market.DesiredReserve.GetRawValue();
		Row.bWarning = bBelowReserve || Market.bIsStale;
		Row.Status = bBelowReserve ? LOCTEXT("LowReserve", "Low reserve") : (Market.bIsStale ? LOCTEXT("Stale", "Stale") : LOCTEXT("MarketStable", "Stable"));
		Row.CausalActionLabel = LOCTEXT("ReviewSupply", "Review supply");
		Row.RelatedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Production.Good.%s"), *SemanticSuffix(Market.GoodId.ToString())));
		Row.bCausalActionEnabled = true;
        if(Remote) {
            const auto Price=KnowledgeSource?KnowledgeSource->QueryKnownMarketPrice(CityId,Market.GoodId):TOptional<FHansaKnownMarketPriceProjection>();
            const auto Supply=KnowledgeSource?KnowledgeSource->QueryKnownMarketSupply(CityId,Market.GoodId):TOptional<FHansaKnownMarketSupplyDemandProjection>();
            const auto Info=Price.IsSet()?Price->InformationState:EHansaMarketInformationState::Unknown;
            Row.Subtitle=FText::Format(LOCTEXT("RemoteReport","Rostock · {0} · report age {1}"),FText::FromString(LexToString(Info)),Price.IsSet() && Price->ReportAgeTicks.IsSet()?FText::Format(LOCTEXT("TicksOld","{0} ticks"),FText::AsNumber(Price->ReportAgeTicks.GetValue())):LOCTEXT("Unavailable","Unavailable"));
            const FText Unknown=LOCTEXT("Unavailable","Unavailable");
            Row.Fields={Field(TEXT("Stock"),LOCTEXT("ReportedStock","Reported stock"),Supply.IsSet() && Supply->Stock.IsSet()?Quantity(Supply->Stock->GetRawValue()):Unknown),
                Field(TEXT("Demand"),LOCTEXT("ReportedDemand","Reported total demand"),Supply.IsSet() && Supply->TotalDemand.IsSet()?Quantity(Supply->TotalDemand->GetRawValue()):Unknown),
                Field(TEXT("Price"),LOCTEXT("ReportedPrice","Reported price"),Price.IsSet() && Price->PriceMilliMarks.IsSet()?Money(Price->PriceMilliMarks.GetValue()):Unknown)};
            Row.Status=Info==EHansaMarketInformationState::Unknown?LOCTEXT("UnknownReport","No recent report"):
                Info==EHansaMarketInformationState::Estimated?LOCTEXT("EstimatedReport","Estimated · indicative values"):
                Info==EHansaMarketInformationState::Stale?LOCTEXT("StaleReport","Stale · historical values"):LOCTEXT("KnownReport","Known report");
            Row.bWarning=Info==EHansaMarketInformationState::Stale || Info==EHansaMarketInformationState::Estimated;
            Row.bCausalActionEnabled=false;Row.RelatedSemanticId=NAME_None;Row.CausalActionDisabledReason=LOCTEXT("RemoteNoChain","Remote production buildings are not reported");
        }
        if (!Remote && KnowledgeSource && Market.GoodId.ToString()==TEXT("Good.Firewood"))
        {
            const auto H=KnowledgeSource->QueryHeating();
            Row.Fields.Add(Field(TEXT("HeatingDemand"),LOCTEXT("HeatingDemand","Fuel demand per day"),FText::Format(
                LOCTEXT("HeatingDemandValue","Households {0}; workshops {1} at nominal capacity; seasonal heating {2}"),Quantity(H.HouseholdDailyRaw),Quantity(H.WorkshopDailyRaw),Percent(H.SeasonMultiplier))));
            Row.Fields.Add(Field(TEXT("HeatingProtection"),LOCTEXT("HeatingProtection","Household fuel protection"),FText::Format(
                LOCTEXT("HeatingProtectionValue","{0} days; target {1}; committed {2}; available surplus {3}; {4}"),FText::AsNumber(H.ReserveDays),Quantity(H.ProtectedRaw),Quantity(H.CommittedRaw),Quantity(H.SurplusRaw),
                H.bOverride?LOCTEXT("HeatingReleased","protection released"):LOCTEXT("HeatingEnabled","protection active"))));
            Row.Fields.Add(Field(TEXT("HeatingWinterTarget"),LOCTEXT("HeatingWinterTarget","Winter stockpile"),FText::Format(
                LOCTEXT("HeatingWinterValue","Target {0}. {1}"),Quantity(H.WinterDailyRaw*H.ReserveDays),
                H.StockRaw-H.CommittedRaw<H.WinterDailyRaw*H.ReserveDays?LOCTEXT("WinterRemedy","Produce or import firewood before winter."):LOCTEXT("WinterReady","Current stock covers this target."))));
        }
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
	if(Previous.LoadState==EHansaCityOverviewLoadState::Ready || Previous.LoadState==EHansaCityOverviewLoadState::Empty)UpdateStateForActiveRows();
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
	if (!Snapshot.bOpen || Snapshot.LoadState!=EHansaCityOverviewLoadState::Ready || FindActiveRow(RowStableId) == nullptr) return false;
	const FHansaCityOverviewSnapshot Previous = Snapshot;
	Snapshot.SelectedRowStableId = RowStableId;
	Snapshot.FocusedSemanticId = FName(*FString::Printf(TEXT("CityOverview.Row.%s"), *SemanticSuffix(RowStableId.ToString())));
	PublishIfChanged(Previous);
	return true;
}

bool UHansaCityOverviewPresentationModel::ActivateCausalIntent(const FName RowStableId)
{
	const FHansaCityOverviewRowPresentation* Row = FindActiveRow(RowStableId);
	if (!Snapshot.bOpen || Snapshot.LoadState!=EHansaCityOverviewLoadState::Ready || Row == nullptr || !Row->bCausalActionEnabled || Row->RelatedSemanticId.IsNone()) return false;
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
	Snapshot.LastActionResult = RelatedId.ToString().StartsWith(TEXT("CityOverview.Production.Good."))?LOCTEXT("SupplySelected","Supplying production selected"):LOCTEXT("BuildingOpened","Building details opened");
	PublishIfChanged(Previous);
	RelatedTargetRequested.Broadcast(RelatedId, BuildingValue);
	return true;
}

bool UHansaCityOverviewPresentationModel::RetryIntent()
{
	if (!Snapshot.bOpen || Snapshot.LoadState != EHansaCityOverviewLoadState::Error) return false;
	SetLoading(Snapshot.CityTitle);
    RefreshRequested.Broadcast();
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
	Snapshot.StateDetail = LOCTEXT("LoadingDetail", "The latest city report is being prepared.");
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
		Snapshot.StateDetail = Snapshot.CityStableId==TEXT("City.Lubeck")?LOCTEXT("EmptyDetail", "Build or connect eligible buildings to populate this view."):LOCTEXT("RemoteEmpty","This civic report is unavailable. Open Market to inspect known trade reports. Rostock does not permit construction.");
	}
	else
	{
		Snapshot.LoadState = EHansaCityOverviewLoadState::Ready;
		Snapshot.StateTitle = FText::GetEmpty();
		Snapshot.StateDetail = FText::GetEmpty();
		if (!Snapshot.SelectedRowStableId.IsNone() && FindActiveRow(Snapshot.SelectedRowStableId) == nullptr) Snapshot.SelectedRowStableId = NAME_None;
	}
}

bool UHansaCityOverviewPresentationModel::SelectCityIntent(FName CityId)
{
    if(!Snapshot.bOpen || (CityId!=TEXT("City.Lubeck") && CityId!=TEXT("City.Rostock")) || CityId==Snapshot.CityStableId)return false;
    const auto Previous=Snapshot;Snapshot.CityStableId=CityId;Snapshot.SelectedRowStableId=NAME_None;
    Snapshot.PopulationRows.Reset();Snapshot.ProductionRows.Reset();Snapshot.MarketRows.Reset();Snapshot.HeaderSummaries.Reset();
    Snapshot.CityTitle=CityId==TEXT("City.Rostock")?LOCTEXT("RostockTitle","Rostock City Overview"):LOCTEXT("DefaultTitle","Lübeck City Overview");
    Snapshot.LoadState=EHansaCityOverviewLoadState::Loading;Snapshot.StateTitle=LOCTEXT("LoadingTitle","Loading city data");
    Snapshot.FocusedSemanticId=CityId==TEXT("City.Rostock")?TEXT("CityOverview.City.Rostock"):TEXT("CityOverview.City.Lubeck");
    PublishIfChanged(Previous);RefreshRequested.Broadcast();return true;
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

bool UHansaCityOverviewPresentationModel::VisitCityIntent(){return Snapshot.bOpen && VisitRequested && VisitRequested(Snapshot.CityStableId);}
void UHansaCityOverviewPresentationModel::SetVisitStatus(FText Status){const auto Previous=Snapshot;Snapshot.LastActionResult=Status;PublishIfChanged(Previous);}
