#include "UI/HansaInspectorPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Events/HansaDomainEvent.h"
#include "Population/HansaPopulation.h"
#include "Production/HansaProduction.h"

#define LOCTEXT_NAMESPACE "HansaInspectorPresentationModel"

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

	FText Quantity(const int64 MilliUnits)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), static_cast<double>(MilliUnits) / 1000.0));
	}

	FText Percent(const int32 BasisPoints)
	{
		return FText::FromString(FString::Printf(TEXT("%.0f%%"), static_cast<double>(BasisPoints) / 100.0));
	}

	FHansaInspectorActionPresentation Action(const TCHAR* Id, const FText& Label, const FText& ToolTip)
	{
		FHansaInspectorActionPresentation Result;
		Result.StableId = Id; Result.Label = Label; Result.ToolTip = ToolTip;
		return Result;
	}
}

bool operator==(const FHansaCausalPresentation& Left, const FHansaCausalPresentation& Right)
{
	return Left.StableCode == Right.StableCode && TextEqual(Left.Problem, Right.Problem) &&
		TextEqual(Left.Cause, Right.Cause) && TextEqual(Left.Evidence, Right.Evidence) &&
		TextEqual(Left.Remedy, Right.Remedy) && Left.RelatedSemanticId == Right.RelatedSemanticId &&
		Left.Severity == Right.Severity;
}

bool operator==(const FHansaInspectorFlowPresentation& Left, const FHansaInspectorFlowPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.Value, Right.Value) &&
		TextEqual(Left.State, Right.State) && Left.bProblem == Right.bProblem;
}

bool operator==(const FHansaInspectorActionPresentation& Left, const FHansaInspectorActionPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.ToolTip, Right.ToolTip) &&
		TextEqual(Left.DisabledReason, Right.DisabledReason) && Left.bEnabled == Right.bEnabled && Left.bSelected == Right.bSelected;
}

bool operator==(const FHansaInspectorHistoryPresentation& Left, const FHansaInspectorHistoryPresentation& Right)
{
	return Left.StableId == Right.StableId && TextEqual(Left.Label, Right.Label) && TextEqual(Left.Age, Right.Age);
}

bool operator==(const FHansaInspectorSnapshot& Left, const FHansaInspectorSnapshot& Right)
{
	return Left.ObjectStableId == Right.ObjectStableId && TextEqual(Left.Identity, Right.Identity) &&
		TextEqual(Left.State, Right.State) && TextEqual(Left.PrimaryResult, Right.PrimaryResult) &&
		Left.Flows == Right.Flows && Left.Causal == Right.Causal && Left.Actions == Right.Actions &&
		Left.History == Right.History && TextEqual(Left.LastActionResult, Right.LastActionResult) &&
		Left.FocusOriginSemanticId == Right.FocusOriginSemanticId && Left.FocusedSemanticId == Right.FocusedSemanticId &&
		Left.Kind == Right.Kind && Left.BuildingValue == Right.BuildingValue && Left.bOpen == Right.bOpen &&
		Left.bPinned == Right.bPinned && Left.bCauseExpanded == Right.bCauseExpanded;
}

FHansaCausalPresentation MakeProductionCausalPresentation(
	const Hansa::Simulation::FHansaProductionProjection& Production)
{
	using namespace Hansa::Simulation;
	FHansaCausalPresentation Result;
	Result.StableCode = FName(LexToString(Production.Blocker));
	Result.RelatedSemanticId = TEXT("CityOverview.Production");
	if (!Production.bActive)
	{
		Result.StableCode = TEXT("ProductionInactive");
		Result.Problem = LOCTEXT("ProductionInactive", "Ⅱ Production is paused");
		Result.Cause = LOCTEXT("ProductionInactiveCause", "The production line is not active");
		Result.Evidence = LOCTEXT("ProductionInactiveEvidence", "The authoritative production projection reports this source as inactive.");
		Result.Remedy = LOCTEXT("ProductionInactiveRemedy", "Resume this production to restore local supply.");
		Result.Severity = EHansaCausalSeverity::Warning;
		return Result;
	}
	switch (Production.Blocker)
	{
	case EHansaProductionBlocker::None:
		Result.Problem = LOCTEXT("ProductionHealthy", "✓ Production is operating");
		Result.Cause = LOCTEXT("ProductionHealthyCause", "No active production blocker");
		Result.Evidence = LOCTEXT("ProductionHealthyEvidence", "The authoritative production projection reports no blocker.");
		Result.Remedy = LOCTEXT("ProductionHealthyRemedy", "No action required.");
		Result.Severity = EHansaCausalSeverity::None;
		break;
	case EHansaProductionBlocker::MissingInput:
		Result.Problem = LOCTEXT("MissingInputProblem", "△ Missing production input");
		Result.Cause = FText::Format(LOCTEXT("MissingInputCause", "{0} is unavailable"), StableLabel(Production.BlockingGoodId.ToString()));
		Result.Evidence = FText::Format(LOCTEXT("MissingInputEvidence", "Available {0}; required {1}."),
			Quantity(Production.BlockingAvailableQuantity.GetRawValue()), Quantity(Production.BlockingRequiredQuantity.GetRawValue()));
		Result.Remedy = LOCTEXT("MissingInputRemedy", "Restore the missing good in connected storage or establish an incoming route.");
		Result.RelatedSemanticId = TEXT("CityOverview.Storage");
		Result.Severity = EHansaCausalSeverity::Warning;
		break;
	case EHansaProductionBlocker::InsufficientLaborerWorkforce:
	case EHansaProductionBlocker::InsufficientArtisanWorkforce:
		Result.Problem = LOCTEXT("WorkforceProblem", "△ Workforce shortage");
		Result.Cause = FText::FromString(LexToString(Production.Blocker));
		Result.Evidence = FText::Format(LOCTEXT("WorkforceEvidence", "Laborers {0}/{1}; artisans {2}/{3}."),
			FText::AsNumber(Production.AllocatedLaborerWorkforce), FText::AsNumber(Production.RequiredLaborerWorkforce),
			FText::AsNumber(Production.AllocatedArtisanWorkforce), FText::AsNumber(Production.RequiredArtisanWorkforce));
		Result.Remedy = LOCTEXT("WorkforceRemedy", "Add suitable residences or pause lower-priority production.");
		Result.RelatedSemanticId = TEXT("CityOverview.Population");
		Result.Severity = EHansaCausalSeverity::Warning;
		break;
	case EHansaProductionBlocker::ConstructionIncomplete:
		Result.Problem = LOCTEXT("ConstructionProblem", "◷ Construction incomplete");
		Result.Cause = LOCTEXT("ConstructionCause", "The building is not ready to operate.");
		Result.Evidence = LOCTEXT("ConstructionEvidence", "The authoritative building state remains under construction.");
		Result.Remedy = LOCTEXT("ConstructionRemedy", "Wait for construction materials and completion.");
		Result.Severity = EHansaCausalSeverity::Notice;
		break;
	default:
		Result.Problem = LOCTEXT("BlockedProblem", "! Production blocked");
		Result.Cause = FText::FromString(LexToString(Production.Blocker));
		Result.Evidence = LOCTEXT("BlockedEvidence", "The authoritative production projection reports this blocker.");
		Result.Remedy = LOCTEXT("BlockedRemedy", "Open the related system and resolve the reported blocker.");
		Result.Severity = Production.Blocker == EHansaProductionBlocker::InventoryTransactionFailed
			? EHansaCausalSeverity::Critical : EHansaCausalSeverity::Warning;
		break;
	}
	return Result;
}

FHansaCausalPresentation MakeResidenceCausalPresentation(
	const Hansa::Simulation::FHansaPopulationCohortProjection& Residence)
{
	using namespace Hansa::Simulation;
	FHansaCausalPresentation Result;
	Result.RelatedSemanticId = TEXT("CityOverview.Population");
	const FHansaPopulationNeedState* Weakest = Residence.Needs.IsEmpty() ? nullptr : &Residence.Needs[0];
	for (const FHansaPopulationNeedState& Need : Residence.Needs)
	{
		if (Weakest == nullptr || Need.SatisfactionBasisPoints < Weakest->SatisfactionBasisPoints) Weakest = &Need;
	}
	if (!Residence.bResidenceOperational)
	{
		Result.StableCode = TEXT("ResidenceNotOperational"); Result.Problem = LOCTEXT("ResidenceInactive", "! Residence unavailable");
		Result.Cause = LOCTEXT("ResidenceInactiveCause", "The residence is not operational.");
		Result.Evidence = LOCTEXT("ResidenceInactiveEvidence", "The authoritative cohort projection reports the residence unavailable.");
		Result.Remedy = LOCTEXT("ResidenceInactiveRemedy", "Complete or reconnect the residence."); Result.Severity = EHansaCausalSeverity::Critical;
	}
	else if (Weakest != nullptr && Weakest->SatisfactionBasisPoints < 8000)
	{
		Result.StableCode = Weakest->NeedId.IsValid() ? FName(*Weakest->NeedId.ToString()) : TEXT("NeedLow");
		Result.Problem = FText::Format(LOCTEXT("NeedProblem", "△ {0} need is not fulfilled"), StableLabel(Weakest->NeedId.ToString()));
		Result.Cause = FText::Format(LOCTEXT("NeedCause", "{0} satisfaction is low"), StableLabel(Weakest->NeedId.ToString()));
		Result.Evidence = FText::Format(LOCTEXT("NeedEvidence", "Access {0}; affordability {1}; reliability {2}."),
			Percent(Weakest->AccessBasisPoints), Percent(Weakest->AffordabilityBasisPoints), Percent(Weakest->ReliabilityBasisPoints));
		Result.Remedy = LOCTEXT("NeedRemedy", "Improve access, affordability, or supply reliability for this need.");
		Result.Severity = Weakest->SatisfactionBasisPoints < 4000 ? EHansaCausalSeverity::Critical : EHansaCausalSeverity::Warning;
	}
	else
	{
		Result.StableCode = TEXT("ResidenceHealthy"); Result.Problem = LOCTEXT("ResidenceHealthy", "✓ Needs are stable");
		Result.Cause = LOCTEXT("ResidenceHealthyCause", "No active residence problem");
		Result.Evidence = LOCTEXT("ResidenceHealthyEvidence", "The authoritative cohort projection reports stable need satisfaction.");
		Result.Remedy = LOCTEXT("ResidenceHealthyRemedy", "No action required."); Result.Severity = EHansaCausalSeverity::None;
	}
	return Result;
}

void UHansaInspectorPresentationModel::InitializeDefaults()
{
	FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot = {};
	PublishIfChanged(Previous);
}

void UHansaInspectorPresentationModel::SetCommonActions()
{
	Snapshot.Actions = {
		Action(TEXT("Inspector.Action.Frame"), LOCTEXT("Frame", "Frame object [F]"), LOCTEXT("FrameTip", "Frame the affected object and preserve inspector focus.")),
		Action(TEXT("Inspector.Action.OpenRelated"), LOCTEXT("OpenRelated", "Open related cause [C]"),
			FText::Format(LOCTEXT("OpenRelatedTip", "State: Available\nCause: {0}\nRemedy: {1}"), Snapshot.Causal.Cause, Snapshot.Causal.Remedy)),
		Action(TEXT("Inspector.Action.Pin"), Snapshot.bPinned ? LOCTEXT("Unpin", "Unpin tracker [P]") : LOCTEXT("Pin", "Pin tracker [P]"),
			LOCTEXT("PinTip", "Keep this object and its health visible in the HUD."))
	};
	Snapshot.Actions[1].bEnabled = !Snapshot.Causal.RelatedSemanticId.IsNone();
	if (!Snapshot.Actions[1].bEnabled) Snapshot.Actions[1].DisabledReason = LOCTEXT("NoRelatedTarget", "No related screen is available for this cause.");
	Snapshot.Actions[2].bSelected = Snapshot.bPinned;
}

bool UHansaInspectorPresentationModel::ShowProduction(
	const Hansa::Simulation::FHansaProductionProjection& Production,
	const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const TConstArrayView<Hansa::Simulation::FHansaDomainEvent> Events,
	const FName FocusOriginSemanticId)
{
	using namespace Hansa::Simulation;
	const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production.RecipeId.ToString());
	if (!Production.Id.IsValid() || !Production.BuildingId.IsValid() || Recipe == nullptr) return false;
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot = {};
	Snapshot.Kind = EHansaInspectorObjectKind::ProductionBuilding;
	Snapshot.ObjectStableId = FName(*Production.BuildingId.ToDebugString());
	Snapshot.BuildingValue = static_cast<int64>(Production.BuildingId.GetValue());
	Snapshot.Identity = StableLabel(Production.RecipeId.ToString());
	Snapshot.State = Production.bActive ? LOCTEXT("Operating", "● Operating") : LOCTEXT("Paused", "Ⅱ Paused");
	Snapshot.PrimaryResult = Production.Outputs.IsEmpty()
		? LOCTEXT("NoOutput", "No output")
		: FText::Format(LOCTEXT("OutputResult", "{0}: {1} actual / {2} nominal per cycle"),
			StableLabel(Production.Outputs[0].GoodId.ToString()), Quantity(Production.Outputs[0].ActualQuantityLastTick.GetRawValue()),
			Quantity(Production.Outputs[0].NominalQuantityPerCycle.GetRawValue()));
	for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
	{
		FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Input.GoodId); Row.Label = StableLabel(Input.GoodId);
		Row.Value = FText::Format(LOCTEXT("InputRequired", "Required {0}"), Quantity(Input.QuantityMilliUnits));
		Row.bProblem = Production.Blocker == EHansaProductionBlocker::MissingInput && Production.BlockingGoodId.ToString() == Input.GoodId;
		Row.State = Row.bProblem ? LOCTEXT("Missing", "Missing") : LOCTEXT("Input", "Input"); Snapshot.Flows.Add(MoveTemp(Row));
	}
	for (const FHansaProductionThroughputProjection& Output : Production.Outputs)
	{
		FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Output.GoodId.ToString()); Row.Label = StableLabel(Output.GoodId.ToString());
		Row.Value = FText::Format(LOCTEXT("OutputActual", "{0} actual / {1} nominal"), Quantity(Output.ActualQuantityLastTick.GetRawValue()), Quantity(Output.NominalQuantityPerCycle.GetRawValue()));
		Row.State = LOCTEXT("Output", "Output"); Snapshot.Flows.Add(MoveTemp(Row));
	}
	Snapshot.Causal = MakeProductionCausalPresentation(Production);
	int32 HistoryIndex = 0;
	for (int32 Index = Events.Num() - 1; Index >= 0 && Snapshot.History.Num() < 3; --Index)
	{
		const FHansaDomainEvent& Event = Events[Index];
		if (Event.GetProductionId() != Production.Id) continue;
		FHansaInspectorHistoryPresentation Row; Row.StableId = FName(*FString::Printf(TEXT("ProductionHistory.%d"), HistoryIndex++));
		Row.Label = StableLabel(LexToString(Event.GetType())); Row.Age = FText::Format(LOCTEXT("TickAge", "Tick {0}"), FText::AsNumber(Event.GetTick().GetValue()));
		Snapshot.History.Add(MoveTemp(Row));
	}
	if (Snapshot.History.IsEmpty()) Snapshot.History.Add({ TEXT("ProductionHistory.Empty"), LOCTEXT("NoHistory", "No recent state changes"), FText::GetEmpty() });
	Snapshot.FocusOriginSemanticId = FocusOriginSemanticId; Snapshot.bOpen = true; Snapshot.bCauseExpanded = true;
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::ShowResidence(
	const Hansa::Simulation::FHansaPopulationCohortProjection& Residence,
	const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const FName FocusOriginSemanticId)
{
	using namespace Hansa::Simulation;
	if (!Residence.Id.IsValid() || !Residence.ResidenceBuildingId.IsValid() || Registry.FindPopulationTier(Residence.TierId.ToString()) == nullptr) return false;
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot = {};
	Snapshot.Kind = EHansaInspectorObjectKind::Residence; Snapshot.ObjectStableId = FName(*Residence.ResidenceBuildingId.ToDebugString());
	Snapshot.BuildingValue = static_cast<int64>(Residence.ResidenceBuildingId.GetValue());
	Snapshot.Identity = FText::Format(LOCTEXT("ResidenceIdentity", "{0} residence"), StableLabel(Residence.TierId.ToString()));
	Snapshot.State = Residence.bResidenceOperational ? LOCTEXT("ResidenceOperating", "● Occupied") : LOCTEXT("ResidenceUnavailable", "! Unavailable");
	Snapshot.PrimaryResult = FText::Format(LOCTEXT("ResidenceResult", "{0}/{1} residents · satisfaction {2}"),
		FText::AsNumber(Residence.Residents), FText::AsNumber(Residence.ResidenceCapacity), Percent(Residence.SatisfactionBasisPoints));
	for (const FHansaPopulationNeedState& Need : Residence.Needs)
	{
		FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Need.NeedId.ToString()); Row.Label = StableLabel(Need.NeedId.ToString());
		Row.Value = FText::Format(LOCTEXT("NeedValue", "access {0} · affordability {1} · reliability {2}"),
			Percent(Need.AccessBasisPoints), Percent(Need.AffordabilityBasisPoints), Percent(Need.ReliabilityBasisPoints));
		Row.State = FText::Format(LOCTEXT("NeedSatisfaction", "{0} satisfied"), Percent(Need.SatisfactionBasisPoints));
		Row.bProblem = Need.SatisfactionBasisPoints < 8000; Snapshot.Flows.Add(MoveTemp(Row));
	}
	Snapshot.Causal = MakeResidenceCausalPresentation(Residence);
	Snapshot.History = { { TEXT("ResidenceHistory.Current"), FText::Format(LOCTEXT("ResidenceTrend", "Resident change: {0}"), FText::AsNumber(Residence.ResidentChangeLastTick)), LOCTEXT("Current", "Current") } };
	Snapshot.FocusOriginSemanticId = FocusOriginSemanticId; Snapshot.bOpen = true; Snapshot.bCauseExpanded = true;
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

void UHansaInspectorPresentationModel::ShowWorldBuilding(
	const FString& BuildingDefinitionId,
	FText DisplayName,
	FText DefinitionFlow,
	const int64 BuildingValue,
	const FString& WorldState,
	const FString& ProductionBlocker,
	const FName FocusOriginSemanticId)
{
	const FHansaInspectorSnapshot Previous = Snapshot;
	const bool bRefreshingSameBuilding = Snapshot.bOpen && Snapshot.BuildingValue == BuildingValue;
	const bool bWasPinned = Snapshot.bPinned;
	const bool bWasCauseExpanded = Snapshot.bCauseExpanded;
	const FName PreviousFocusOrigin = Snapshot.FocusOriginSemanticId;
	const FName PreviousFocus = Snapshot.FocusedSemanticId;
	const FText PreviousActionResult = Snapshot.LastActionResult;
	Snapshot = {};
	Snapshot.Kind = BuildingDefinitionId.Contains(TEXT("Residence")) ? EHansaInspectorObjectKind::Residence : EHansaInspectorObjectKind::ProductionBuilding;
	Snapshot.ObjectStableId = FName(*FString::Printf(TEXT("Building.%lld"), BuildingValue)); Snapshot.BuildingValue = BuildingValue;
	Snapshot.Identity = DisplayName.IsEmpty() ? StableLabel(BuildingDefinitionId) : MoveTemp(DisplayName);
	Snapshot.State = FText::FromString(WorldState);
	Snapshot.PrimaryResult = WorldState == TEXT("UnderConstruction") ? LOCTEXT("ConstructionResult", "Construction in progress") : LOCTEXT("ReadyResult", "Ready for operation");
	if (!DefinitionFlow.IsEmpty())
	{
		FHansaInspectorFlowPresentation Flow;
		Flow.StableId = TEXT("DefinitionFlow");
		Flow.Label = Snapshot.Kind == EHansaInspectorObjectKind::Residence ? LOCTEXT("ResidenceNeeds", "Needs and workforce") : LOCTEXT("ProductionFlow", "Production flow");
		Flow.Value = MoveTemp(DefinitionFlow);
		Flow.State = LOCTEXT("DefinitionValue", "Definition");
		Snapshot.Flows.Add(MoveTemp(Flow));
	}
	Snapshot.Causal.StableCode = FName(*ProductionBlocker);
	Snapshot.Causal.Problem = ProductionBlocker == TEXT("None") ? LOCTEXT("WorldHealthy", "✓ No active problem") : FText::Format(LOCTEXT("WorldProblem", "△ {0}"), FText::FromString(ProductionBlocker));
	Snapshot.Causal.Cause = ProductionBlocker == TEXT("None") ? LOCTEXT("WorldHealthyCause", "No active production blocker") : FText::FromString(ProductionBlocker);
	Snapshot.Causal.Evidence = LOCTEXT("WorldEvidence", "This status comes from the authoritative building world projection.");
	Snapshot.Causal.Remedy = ProductionBlocker == TEXT("None") ? LOCTEXT("WorldNoRemedy", "No action required.") : LOCTEXT("WorldRemedy", "Open the related city system for the complete causal breakdown.");
	Snapshot.Causal.RelatedSemanticId = TEXT("CityOverview.Production");
	Snapshot.Causal.Severity = ProductionBlocker == TEXT("None") ? EHansaCausalSeverity::None : EHansaCausalSeverity::Warning;
	Snapshot.History = { { TEXT("WorldHistory.Selected"), LOCTEXT("SelectedHistory", "Object selected"), LOCTEXT("Now", "Now") } };
	Snapshot.FocusOriginSemanticId = bRefreshingSameBuilding ? PreviousFocusOrigin : FocusOriginSemanticId;
	Snapshot.FocusedSemanticId = bRefreshingSameBuilding ? PreviousFocus : FName();
	Snapshot.LastActionResult = bRefreshingSameBuilding ? PreviousActionResult : FText();
	Snapshot.bPinned = bRefreshingSameBuilding && bWasPinned;
	Snapshot.bCauseExpanded = bRefreshingSameBuilding ? bWasCauseExpanded : Snapshot.bCauseExpanded;
	Snapshot.bOpen = true;
	SetCommonActions();
	PublishIfChanged(Previous);
}

void UHansaInspectorPresentationModel::OpenFromAlert(
	const FName AlertId,
	FText AffectedObject,
	FText Age,
	const int64 BuildingValue,
	const FHansaCausalPresentation& Causal,
	const FName FocusOriginSemanticId)
{
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot = {};
	Snapshot.ObjectStableId = AlertId; Snapshot.Identity = MoveTemp(AffectedObject); Snapshot.State = FText::Format(LOCTEXT("AlertAge", "Alert active · {0}"), Age);
	Snapshot.PrimaryResult = Causal.Problem; Snapshot.Causal = Causal; Snapshot.BuildingValue = BuildingValue;
	Snapshot.Kind = EHansaInspectorObjectKind::ProductionBuilding;
	Snapshot.History = { { TEXT("AlertHistory.Active"), LOCTEXT("AlertActive", "Alert became active"), MoveTemp(Age) } };
	Snapshot.FocusOriginSemanticId = FocusOriginSemanticId; Snapshot.bOpen = true; SetCommonActions(); PublishIfChanged(Previous);
}

bool UHansaInspectorPresentationModel::CloseIntent()
{
	if (!Snapshot.bOpen) return false;
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.bOpen = false; Snapshot.FocusedSemanticId = Snapshot.FocusOriginSemanticId;
	PublishIfChanged(Previous); FocusRestoreRequested.Broadcast(Snapshot.FocusOriginSemanticId); return true;
}

bool UHansaInspectorPresentationModel::TogglePinIntent()
{
	if (!Snapshot.bOpen) return false;
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.bPinned = !Snapshot.bPinned;
	Snapshot.LastActionResult = Snapshot.bPinned ? LOCTEXT("PinnedResult", "◆ Pinned tracker added") : LOCTEXT("UnpinnedResult", "◇ Pinned tracker removed");
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::FrameIntent()
{
	if (!Snapshot.bOpen || Snapshot.BuildingValue <= 0) return false;
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.LastActionResult = LOCTEXT("FramedResult", "⌖ Object framed");
	PublishIfChanged(Previous); FrameRequested.Broadcast(Snapshot.BuildingValue); return true;
}

bool UHansaInspectorPresentationModel::OpenCauseIntent()
{
	if (!Snapshot.bOpen) return false;
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.bCauseExpanded = true; Snapshot.FocusedSemanticId = TEXT("Inspector.Problem.Cause");
	Snapshot.LastActionResult = LOCTEXT("CauseOpened", "Cause and remedy opened"); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::OpenRelatedIntent()
{
	if (!Snapshot.bOpen || Snapshot.Causal.RelatedSemanticId.IsNone()) return false;
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot.LastActionResult = FText::Format(LOCTEXT("RelatedOpened", "Opened related view · {0}"), FText::FromName(Snapshot.Causal.RelatedSemanticId));
	PublishIfChanged(Previous); RelatedTargetRequested.Broadcast(Snapshot.Causal.RelatedSemanticId); return true;
}

bool UHansaInspectorPresentationModel::ActivateAction(const FName SemanticId)
{
	if (SemanticId == TEXT("Inspector.Close")) return CloseIntent();
	if (SemanticId == TEXT("Inspector.Action.Frame")) return FrameIntent();
	if (SemanticId == TEXT("Inspector.Action.Pin")) return TogglePinIntent();
	if (SemanticId == TEXT("Inspector.Action.OpenCause")) return OpenCauseIntent();
	if (SemanticId == TEXT("Inspector.Action.OpenRelated")) return OpenRelatedIntent();
	return false;
}

void UHansaInspectorPresentationModel::SetFocusedSemanticId(const FName SemanticId)
{
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.FocusedSemanticId = SemanticId; PublishIfChanged(Previous);
}

void UHansaInspectorPresentationModel::PublishIfChanged(const FHansaInspectorSnapshot& Previous)
{
	if (!(Previous == Snapshot)) { ++Revision; Changed.Broadcast(Snapshot, Revision); }
}

#undef LOCTEXT_NAMESPACE
