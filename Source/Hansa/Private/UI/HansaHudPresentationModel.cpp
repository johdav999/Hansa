#include "UI/HansaHudPresentationModel.h"

#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"

#define LOCTEXT_NAMESPACE "HansaHudPresentationModel"

namespace
{
	bool TextEqual(const FText& Left, const FText& Right)
	{
		return Left.EqualTo(Right);
	}

	FText StableLabel(FString StableId)
	{
		int32 Separator = INDEX_NONE;
		if (StableId.FindLastChar(TEXT('.'), Separator)) StableId.RightChopInline(Separator + 1);
		StableId.ReplaceInline(TEXT("_"), TEXT(" "));
		return FText::FromString(StableId);
	}
}

bool operator==(const FHansaHudPresentationSnapshot& Left, const FHansaHudPresentationSnapshot& Right)
{
	return TextEqual(Left.Money, Right.Money) && TextEqual(Left.MoneyTrend, Right.MoneyTrend) &&
		TextEqual(Left.Population, Right.Population) && TextEqual(Left.Workforce, Right.Workforce) &&
		TextEqual(Left.CityBreadcrumb, Right.CityBreadcrumb) && TextEqual(Left.DateAndSeason, Right.DateAndSeason) &&
		TextEqual(Left.Research, Right.Research) && TextEqual(Left.Connection, Right.Connection) &&
		Left.Speed == Right.Speed && Left.Alerts == Right.Alerts && Left.Notifications == Right.Notifications &&
		TextEqual(Left.SelectionSummary, Right.SelectionSummary) && TextEqual(Left.InspectorTitle, Right.InspectorTitle) &&
		TextEqual(Left.InspectorSummary, Right.InspectorSummary) &&
		Left.bAlertStackExpanded == Right.bAlertStackExpanded && Left.bBottomAreaOpen == Right.bBottomAreaOpen &&
		Left.bInspectorOpen == Right.bInspectorOpen && Left.FocusedSemanticId == Right.FocusedSemanticId;
}

void UHansaHudPresentationModel::InitializeDefaults()
{
	FHansaHudPresentationSnapshot Defaults;
	Defaults.Money = LOCTEXT("DefaultMoney", "2,450 mk");
	Defaults.MoneyTrend = LOCTEXT("DefaultMoneyTrend", "+18 / day");
	Defaults.Population = LOCTEXT("DefaultPopulation", "Population 1,284");
	Defaults.Workforce = LOCTEXT("DefaultWorkforce", "Workforce 612 / 740");
	Defaults.CityBreadcrumb = LOCTEXT("DefaultCityBreadcrumb", "Free City  /  Lübeck");
	Defaults.DateAndSeason = LOCTEXT("DefaultDateSeason", "Spring · 1280");
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
	GrainWatch.Label = LOCTEXT("DefaultAlert", "△ Grain reserves are low"); GrainWatch.AffectedObject = LOCTEXT("BakeryObject", "Lübeck bakery");
	GrainWatch.Age = LOCTEXT("GrainAge", "3 min"); GrainWatch.AffectedBuildingValue = 2; GrainWatch.bWarning = true;
	GrainWatch.Causal.StableCode = TEXT("MissingInput"); GrainWatch.Causal.Problem = LOCTEXT("GrainProblem", "△ Missing grain");
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
		const FText Good = StableLabel(Source.GoodId.ToString());
		const bool bCritical = Source.Severity == EHansaMarketAlertSeverity::Critical;
		Alert.Label = FText::Format(LOCTEXT("MarketAlertLabel", "{0} {1} {2}"),
			bCritical ? LOCTEXT("CriticalMarketGlyph", "!") : LOCTEXT("WarningMarketGlyph", "△"), Good,
			StableLabel(LexToString(Source.Type)));
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
		Alert.Causal.Cause = Source.Cause;
		if (const FHansaCityMarketProjection* Market = Projection.GetMarkets().FindByPredicate([&Source](const auto& Candidate)
		{
			return Candidate.CityId == Source.CityId && Candidate.GoodId == Source.GoodId;
		}))
		{
			Alert.Causal.Evidence = FText::Format(LOCTEXT("MarketAlertEvidence", "Stock {0}; desired reserve {1}; unmet demand {2}; incoming {3}."),
				FText::AsNumber(Market->CurrentStock.GetRawValue()), FText::AsNumber(Market->DesiredReserve.GetRawValue()),
				FText::AsNumber(Market->UnmetDemand.GetRawValue()), FText::AsNumber(Market->ExpectedIncomingSupply.GetRawValue()));
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
