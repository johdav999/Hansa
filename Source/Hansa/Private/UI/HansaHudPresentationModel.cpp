#include "UI/HansaHudPresentationModel.h"

#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaPresentationClock.h"

#define LOCTEXT_NAMESPACE "HansaHudPresentationModel"

namespace
{
	bool HudPresentationModelTextEqual(const FText& Left, const FText& Right)
	{
		return Left.EqualTo(Right);
	}

	FText HudPresentationModelStableLabel(FString StableId)
	{
		int32 Separator = INDEX_NONE;
		if (StableId.FindLastChar(TEXT('.'), Separator)) StableId.RightChopInline(Separator + 1);
		StableId.ReplaceInline(TEXT("_"), TEXT(" "));
		FString Label;
		for(int32 I=0;I<StableId.Len();++I){if(I>0&&FChar::IsUpper(StableId[I])&&FChar::IsLower(StableId[I-1]))Label+=TEXT(" ");Label+=StableId[I];}
		return FText::FromString(Label);
	}
}

bool operator==(const FHansaHudPresentationSnapshot& Left, const FHansaHudPresentationSnapshot& Right)
{
	return HudPresentationModelTextEqual(Left.Money, Right.Money) && HudPresentationModelTextEqual(Left.MoneyTrend, Right.MoneyTrend) &&
		HudPresentationModelTextEqual(Left.Population, Right.Population) && HudPresentationModelTextEqual(Left.Workforce, Right.Workforce) &&
		Left.WealthyCitizens.EqualTo(Right.WealthyCitizens) && Left.MoneyTrendTooltip.EqualTo(Right.MoneyTrendTooltip) && Left.TopProducts == Right.TopProducts &&
		Left.bRemoteCityView == Right.bRemoteCityView && HudPresentationModelTextEqual(Left.CityBreadcrumb, Right.CityBreadcrumb) && HudPresentationModelTextEqual(Left.DateAndSeason, Right.DateAndSeason) &&
		HudPresentationModelTextEqual(Left.Research, Right.Research) && HudPresentationModelTextEqual(Left.Connection, Right.Connection) &&
		Left.Speed == Right.Speed && Left.Alerts == Right.Alerts && Left.Notifications == Right.Notifications &&
		HudPresentationModelTextEqual(Left.SelectionSummary, Right.SelectionSummary) && HudPresentationModelTextEqual(Left.InspectorTitle, Right.InspectorTitle) &&
		HudPresentationModelTextEqual(Left.InspectorSummary, Right.InspectorSummary) &&
		Left.bAlertStackExpanded == Right.bAlertStackExpanded && Left.bBottomAreaOpen == Right.bBottomAreaOpen &&
		Left.bInspectorOpen == Right.bInspectorOpen && Left.FocusedSemanticId == Right.FocusedSemanticId;
}

void UHansaHudPresentationModel::ResetCashHistory()
{
    CashHistory.Reset(); CashHistoryHouse={};
    auto Updated=Snapshot;
    Updated.MoneyTrend=LOCTEXT("UnknownMetric","—");
    Updated.MoneyTrendTooltip=LOCTEXT("MonthlyPending","Money change over the last 30 game days, including all income and spending. Waiting for a complete observed month; history restarts after loading a game.");
    ApplySnapshot(Updated);
}

void UHansaHudPresentationModel::InitializeDefaults()
{
	CashHistory.Reset(); CashHistoryHouse = {};
	FHansaHudPresentationSnapshot Defaults;
	Defaults.Money = LOCTEXT("DefaultMoney", "2,450");
	Defaults.MoneyTrend = LOCTEXT("DefaultMoneyTrend", "—");
	Defaults.Population = LOCTEXT("DefaultPopulation", "1,284");
	Defaults.Workforce = LOCTEXT("DefaultWorkforce", "1,000");
	Defaults.WealthyCitizens=FText::AsNumber(284);
    Defaults.TopProducts={BuildBreadBalance(nullptr,0)};
	Defaults.MoneyTrendTooltip=LOCTEXT("MonthlyPending","Money change over the last 30 game days, including all income and spending. Waiting for a complete observed month; history restarts after loading a game.");
	Defaults.CityBreadcrumb = LOCTEXT("DefaultCityBreadcrumb", "Free City  /  Lübeck");
	Defaults.DateAndSeason = LOCTEXT("DefaultDateSeason", "Day 1 · 12:00");
	Defaults.Research = LOCTEXT("DefaultResearch", "Guild influence 14");
	Defaults.Connection = LOCTEXT("DefaultConnection", "Connected");
	Defaults.SelectionSummary = LOCTEXT("DefaultSelection", "Build and selection");
	FHansaHudAlertPresentation Objective;
	Objective.StableId = TEXT("Objectives"); Objective.GroupId = TEXT("Objectives");
	Objective.Label = LOCTEXT("DefaultObjective", "Establish the bread supply chain");
	Objective.AffectedObject = LOCTEXT("LubeckObjective", "Lübeck"); Objective.Age = LOCTEXT("ObjectiveAge", "Current");
	Objective.Causal.StableCode = TEXT("ScenarioObjective"); Objective.Causal.Problem = Objective.Label;
	Objective.Causal.Cause = LOCTEXT("ObjectiveCause", "The bread supply chain is incomplete.");
	Objective.Causal.Evidence = LOCTEXT("ObjectiveEvidence", "The scenario objective is still active.");
	Objective.Causal.Remedy = LOCTEXT("ObjectiveRemedy", "Build and connect grain, milling and bakery production.");
	Objective.Causal.RelatedSemanticId = TEXT("BuildMenu.Category.Production"); Objective.Causal.Severity = EHansaCausalSeverity::Notice;

	FHansaHudAlertPresentation GrainWatch;
	GrainWatch.StableId = TEXT("GrainWatch"); GrainWatch.GroupId = TEXT("Production");
	GrainWatch.Label = LOCTEXT("DefaultAlert", "Grain reserves are low"); GrainWatch.AffectedObject = LOCTEXT("BakeryObject", "Lübeck bakery");
	GrainWatch.Age = LOCTEXT("GrainAge", "3 min"); GrainWatch.AffectedBuildingValue = 2; GrainWatch.bWarning = true;
	GrainWatch.Causal.StableCode = TEXT("MissingInput"); GrainWatch.Causal.Problem = LOCTEXT("GrainProblem", "Missing grain");
	GrainWatch.Causal.Cause = LOCTEXT("GrainCause", "No grain is available in connected storage.");
	GrainWatch.Causal.Evidence = LOCTEXT("GrainEvidence", "Available 0.0; required 0.1 per cycle.");
	GrainWatch.Causal.Remedy = LOCTEXT("GrainRemedy", "Restore Lübeck's grain reserve or establish an incoming route.");
	GrainWatch.Causal.RelatedSemanticId = TEXT("CityOverview.Storage"); GrainWatch.Causal.Severity = EHansaCausalSeverity::Warning;
	Defaults.Alerts = { MoveTemp(Objective), MoveTemp(GrainWatch) };
	ApplySnapshot(Defaults);
}

const FHansaHudAlertPresentation* UHansaHudPresentationModel::FindAlert(const FName AlertId) const
{
	return Snapshot.Alerts.FindByPredicate([AlertId](const FHansaHudAlertPresentation& Alert) { return Alert.StableId == AlertId; });
}

bool UHansaHudPresentationModel::ApplyAlertAction(const FName AlertId, const EHansaHudAlertAction Action)
{
	FHansaHudAlertPresentation* Alert = Snapshot.Alerts.FindByPredicate(
		[AlertId](const FHansaHudAlertPresentation& Candidate) { return Candidate.StableId == AlertId; });
	if (Alert == nullptr) return false;
	if (Action == EHansaHudAlertAction::Snooze) Alert->bSnoozed = !Alert->bSnoozed;
	if (Action == EHansaHudAlertAction::Pin) Alert->bPinned = !Alert->bPinned;
	Snapshot.FocusedSemanticId = FName(*FString::Printf(TEXT("HUD.AlertStack.Alert.%s.%s"), *AlertId.ToString(),
		Action == EHansaHudAlertAction::Frame ? TEXT("Frame") : Action == EHansaHudAlertAction::OpenCause ? TEXT("OpenCause") :
		Action == EHansaHudAlertAction::Snooze ? TEXT("Snooze") : TEXT("Pin")));
	BroadcastChange(); return true;
}

bool UHansaHudPresentationModel::ApplySnapshot(const FHansaHudPresentationSnapshot& NewSnapshot)
{
	if (Snapshot == NewSnapshot)
	{
		return false;
	}
	Snapshot = NewSnapshot;
	BroadcastChange();
	return true;
}

void UHansaHudPresentationModel::ToggleAlertStack()
{
	Snapshot.bAlertStackExpanded = !Snapshot.bAlertStackExpanded;
	BroadcastChange();
}

void UHansaHudPresentationModel::SetBottomAreaOpen(const bool bOpen)
{
	if (Snapshot.bBottomAreaOpen != bOpen)
	{
		Snapshot.bBottomAreaOpen = bOpen;
		BroadcastChange();
	}
}

void UHansaHudPresentationModel::SetInspectorOpen(const bool bOpen)
{
	if (Snapshot.bInspectorOpen != bOpen)
	{
		Snapshot.bInspectorOpen = bOpen;
		BroadcastChange();
	}
}

void UHansaHudPresentationModel::SetSpeed(const EHansaHudGameSpeed NewSpeed)
{
	if (Snapshot.Speed != NewSpeed)
	{
		Snapshot.Speed = NewSpeed;
		BroadcastChange();
	}
}

void UHansaHudPresentationModel::SetFocusedSemanticId(const FName SemanticId)
{
	if (Snapshot.FocusedSemanticId != SemanticId)
	{
		Snapshot.FocusedSemanticId = SemanticId;
		BroadcastChange();
	}
}

void UHansaHudPresentationModel::SetSelection(
	FText SelectionSummary,
	FText InspectorTitle,
	FText InspectorSummary,
	const bool bOpenInspector)
{
	FHansaHudPresentationSnapshot Updated = Snapshot;
	Updated.SelectionSummary = MoveTemp(SelectionSummary);
	Updated.InspectorTitle = MoveTemp(InspectorTitle);
	Updated.InspectorSummary = MoveTemp(InspectorSummary);
	Updated.bInspectorOpen = bOpenInspector;
	ApplySnapshot(Updated);
}

FHansaHudProductSummary UHansaHudPresentationModel::BuildBreadBalance(
 const Hansa::Simulation::FHansaCityMarketProjection* Market,const uint16 MinutesPerTick)
{
 FHansaHudProductSummary Product;
 Product.GoodId=TEXT("Good.Bread");
 Product.Value=LOCTEXT("BreadPendingValue","Bread —");
 Product.Tooltip=LOCTEXT("BreadPendingTip","Bread balance per game day. Waiting for the first city market report.");
 if(!Market || Market->LastUpdateTick<0 || Market->PriceHistory.IsEmpty() || Market->UpdateCadenceTicks<=0 || MinutesPerTick==0)return Product;
 // Average complete reports in the latest game day (or available startup history).
 // Local demand is per tick; market-only city demand is per reporting interval.
 const double TicksPerDay=1440.0/MinutesPerTick;
 const int32 Count=FMath::Min(Market->PriceHistory.Num(),FMath::Max(1,FMath::FloorToInt(TicksPerDay/Market->UpdateCadenceTicks)));
 double Supply=0,Demand=0;
 for(int32 I=Market->PriceHistory.Num()-Count;I<Market->PriceHistory.Num();++I)
 {
     const auto& Entry=Market->PriceHistory[I];
     Supply+=double(Entry.LocalProduction.GetRawValue())/1000.;
     Demand+=(double(Entry.CitizenDemand.GetRawValue())+double(Entry.IndustrialDemand.GetRawValue()))/1000.
         *(Market->bDemandPerReport?1:Market->UpdateCadenceTicks);
 }
 const double WindowTicks=double(Count)*Market->UpdateCadenceTicks;
 Supply*=TicksPerDay/WindowTicks; Demand*=TicksPerDay/WindowTicks;
 // Round before formatting/sign selection so small deficits cannot display negative zero.
 const double Balance=FMath::RoundToDouble((Supply-Demand)*100.)/100.;
 FNumberFormattingOptions Format;Format.SetMaximumFractionalDigits(2);
 const FText Signed=FText::Format(LOCTEXT("BreadSignedBalance","{0}{1}"),Balance>0?FText::FromString(TEXT("+")):FText::GetEmpty(),FText::AsNumber(Balance,&Format));
 Product.Value=FText::Format(LOCTEXT("BreadBalanceValue","Bread {0}"),Signed);
 Product.Tooltip=FText::Format(LOCTEXT("BreadBalanceTip","Bread balance: {0} units per game day.\nLocal supply: {1}/day; citizen and industrial demand: {2}/day.\nAverage of {3} complete market reports ({4} game hours). Positive means surplus; negative means shortage. Demand includes unmet needs. Excludes stored stock and imports."),
     Signed,FText::AsNumber(Supply,&Format),FText::AsNumber(Demand,&Format),FText::AsNumber(Count),FText::AsNumber(WindowTicks*MinutesPerTick/60.,&Format));
 if(Market->bIsStale) Product.Tooltip=FText::Format(LOCTEXT("BreadStaleTip","{0}\nThis report is stale; the balance may have changed."),Product.Tooltip);
 return Product;
}

void UHansaHudPresentationModel::ApplyRuntimeStatus(const Hansa::Simulation::FHansaSimulationProjection& Projection,
 const Hansa::Simulation::FHansaCityDefinitionId CityId,const Hansa::Simulation::FHansaHouseId HouseId)
{
 auto Updated=Snapshot;
 const auto* House=Projection.GetHouses().FindByPredicate([&](const auto& H){return H.Id==HouseId;});
 const auto* City=Projection.GetCityPopulations().FindByPredicate([&](const auto& C){return C.CityId==CityId;});
 FNumberFormattingOptions MoneyFormat;MoneyFormat.SetMaximumFractionalDigits(2);
 Updated.Money=House?FText::AsNumber(double(House->Money.GetRawValue())/1000.,&MoneyFormat):LOCTEXT("UnknownMetric","—");
 Updated.Population=City?FText::AsNumber(City->TotalResidents):LOCTEXT("UnknownMetric","—");
 Updated.Workforce=City?FText::AsNumber(City->LaborerResidents):LOCTEXT("UnknownMetric","—");
 Updated.WealthyCitizens=City?FText::AsNumber(City->ArtisanResidents):LOCTEXT("UnknownMetric","—");
 const auto& Calendar=Projection.GetCalendar();
 const int64 Minute=Calendar.ElapsedDays*1440+Calendar.HourOfDay*60+Calendar.MinuteOfHour;
 constexpr int64 MonthMinutes=30*1440;
 if(CashHistoryHouse!=HouseId || (!CashHistory.IsEmpty() && Minute<CashHistory.Last().Minute)) CashHistory.Reset();
 CashHistoryHouse=HouseId;
 Updated.MoneyTrend=LOCTEXT("UnknownMetric","—");
 Updated.MoneyTrendTooltip=LOCTEXT("MonthlyPending","Money change over the last 30 game days, including all income and spending. Waiting for a complete observed month; history restarts after loading a game.");
 if(House)
 {
     const int64 Cash=House->Money.GetRawValue();
     if(!CashHistory.IsEmpty() && CashHistory.Last().Minute==Minute) CashHistory.Last().MilliMarks=Cash;
     else CashHistory.Add({Minute,Cash});
     // Retain the last observation at or before the boundary, plus the window.
     while(CashHistory.Num()>1 && CashHistory[1].Minute<=Minute-MonthMinutes) CashHistory.RemoveAt(0);
     if(CashHistory[0].Minute<=Minute-MonthMinutes)
     {
         const double Delta=(double(Cash)-double(CashHistory[0].MilliMarks))/1000.;
         Updated.MoneyTrend=FText::Format(LOCTEXT("SignedMonthlyCash","{0}{1}"),Delta>0?FText::FromString(TEXT("+")):FText::GetEmpty(),FText::AsNumber(Delta,&MoneyFormat));
         Updated.MoneyTrendTooltip=FText::Format(LOCTEXT("MonthlyCashTip","Player money change over the last 30 game days: {0} marks. Includes all income and spending across cities."),Updated.MoneyTrend);
     }
 }
 else CashHistory.Reset();
 const auto* BreadMarket=Projection.GetMarkets().FindByPredicate([&](const auto& M){
     return M.CityId==CityId && M.GoodId.ToString()==TEXT("Good.Bread");
 });
 Updated.TopProducts={BuildBreadBalance(BreadMarket,Projection.GetClock().GetMinutesPerTick())};
 const Hansa::Simulation::FHansaCalendarProjection DisplayCalendar=Hansa::Game::PresentationClock::AtMidday(Calendar);
 Updated.DateAndSeason=FText::Format(LOCTEXT("RuntimeCalendar","Day {0} · {1}"),FText::AsNumber(DisplayCalendar.ElapsedDays+1),FText::FromString(FString::Printf(TEXT("%02d:%02d"),DisplayCalendar.HourOfDay,DisplayCalendar.MinuteOfHour)));
 const auto* Research=Projection.GetResearch().FindByPredicate([&](const auto& R){return R.HouseId==HouseId;});
 Updated.Research=Research?FText::Format(LOCTEXT("RuntimeResearch","Research · {0} points available"),FText::AsNumber(Research->AvailableResearchPoints)):LOCTEXT("ResearchUnavailable","Research unavailable");
 ApplySnapshot(Updated);
}

bool UHansaHudPresentationModel::ApplyMarketAlerts(
	const Hansa::Simulation::FHansaSimulationProjection& Projection,
	const Hansa::Simulation::FHansaCityDefinitionId CityId,
	FText CityDisplayName)
{
	using namespace Hansa::Simulation;
	if (!CityId.IsValid()) return false;
	FHansaHudPresentationSnapshot Updated = Snapshot;
	Updated.Alerts.RemoveAll([](const FHansaHudAlertPresentation& Existing)
	{
		return Existing.StableId == TEXT("GrainWatch") || Existing.StableId.ToString().StartsWith(TEXT("Market."));
	});
	for (const FHansaMarketAlertProjection& Source : Projection.GetActiveMarketAlerts())
	{
		if (Source.CityId != CityId) continue;
		FHansaHudAlertPresentation Alert;
		Alert.StableId = FName(*FString::Printf(TEXT("Market.%s.%s"), LexToString(Source.Type), *Source.GoodId.ToString()));
		Alert.GroupId = TEXT("Market");
		const FText Good = HudPresentationModelStableLabel(Source.GoodId.ToString());
		const bool bCritical = Source.Severity == EHansaMarketAlertSeverity::Critical;
		Alert.Label = FText::Format(LOCTEXT("MarketAlertLabel", "{0} {1} {2}"),
			bCritical ? LOCTEXT("CriticalMarketGlyph", "Critical") : LOCTEXT("WarningMarketGlyph", "Warning"), Good,
			HudPresentationModelStableLabel(LexToString(Source.Type)));
		Alert.AffectedObject = FText::Format(LOCTEXT("MarketAffectedObject", "{0} · {1}"), CityDisplayName, Good);
		Alert.Age = FText::Format(LOCTEXT("MarketAlertAge", "{0} ticks"), FText::AsNumber(Source.AgeTicks));
		Alert.bWarning = true;
		if (!Source.ProductionIds.IsEmpty())
		{
			for (const auto& Production : Projection.GetProductions())
			{
				if (Production.Id == Source.ProductionIds[0]) { Alert.AffectedBuildingValue = static_cast<int64>(Production.BuildingId.GetValue()); break; }
			}
		}
		Alert.Causal.StableCode = Source.CauseMessageKey.IsNone() ? FName(LexToString(Source.Type)) : Source.CauseMessageKey;
		Alert.Causal.Problem = Alert.Label;
		Alert.Causal.Cause = FText::Format(LOCTEXT("MarketAlertCause","Review {0} stock and its reserve target."),Good);
		if (const FHansaCityMarketProjection* Market = Projection.GetMarkets().FindByPredicate([&Source](const auto& Candidate)
		{
			return Candidate.CityId == Source.CityId && Candidate.GoodId == Source.GoodId;
		}))
		{
			Alert.Causal.Evidence = FText::Format(LOCTEXT("MarketAlertEvidence", "Stock {0}; desired reserve {1}; unmet demand {2}; incoming {3}."),
				FText::AsNumber(double(Market->CurrentStock.GetRawValue())/1000.), FText::AsNumber(double(Market->DesiredReserve.GetRawValue())/1000.),
				FText::AsNumber(double(Market->UnmetDemand.GetRawValue())/1000.), FText::AsNumber(double(Market->ExpectedIncomingSupply.GetRawValue())/1000.));
		}
		Alert.Causal.Remedy = Source.SuggestedActions.IsEmpty()
			? LOCTEXT("MarketFallbackRemedy", "Review local production and establish incoming supply.")
			: Source.SuggestedActions[0].Message;
		FString GoodSuffix = Source.GoodId.ToString(); GoodSuffix.ReplaceInline(TEXT("."), TEXT("_"));
		Alert.Causal.RelatedSemanticId = FName(*FString::Printf(TEXT("Market.Row.%s"), *GoodSuffix));
		Alert.Causal.Severity = bCritical ? EHansaCausalSeverity::Critical : EHansaCausalSeverity::Warning;
		if (const FHansaHudAlertPresentation* Existing = Snapshot.Alerts.FindByPredicate([&Alert](const auto& Candidate) { return Candidate.StableId == Alert.StableId; }))
		{
			Alert.bSnoozed = Existing->bSnoozed;
			Alert.bPinned = Existing->bPinned;
		}
		Updated.Alerts.Add(MoveTemp(Alert));
	}
	return ApplySnapshot(Updated);
}

void UHansaHudPresentationModel::BroadcastChange()
{
	++Revision;
	PresentationChanged.Broadcast(Snapshot, Revision);
}

#undef LOCTEXT_NAMESPACE
