#include "UI/HansaInspectorPresentationModel.h"
#include "World/HansaCargoProjectionManager.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Construction/HansaConstruction.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Events/HansaDomainEvent.h"
#include "Population/HansaPopulation.h"
#include "Production/HansaProduction.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"

#define LOCTEXT_NAMESPACE "HansaInspectorPresentationModel"

namespace
{
	bool InspectorPresentationModelTextEqual(const FText& Left, const FText& Right) { return Left.EqualTo(Right); }

	FText InspectorPresentationModelStableLabel(const FString& StableId)
	{
		FString Result = StableId;
		int32 Separator = INDEX_NONE;
		if (Result.FindLastChar(TEXT('.'), Separator)) Result.RightChopInline(Separator + 1);
		Result.ReplaceInline(TEXT("_"), TEXT(" "));
		return FText::FromString(Result);
	}

	FText InspectorPresentationModelQuantity(const int64 MilliUnits)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), static_cast<double>(MilliUnits) / 1000.0));
	}

	FText InspectorPresentationModelPercent(const int32 BasisPoints)
	{
		return FText::FromString(FString::Printf(TEXT("%.0f%%"), static_cast<double>(BasisPoints) / 100.0));
	}

	FHansaInspectorActionPresentation Action(const TCHAR* Id, const FText& Label, const FText& ToolTip)
	{
		FHansaInspectorActionPresentation Result;
		Result.StableId = Id; Result.Label = Label; Result.ToolTip = ToolTip;
		return Result;
	}

	FText InspectorPresentationModelMoney(const int64 Pfennig)
	{
		return FText::Format(LOCTEXT("MoneyPfennig", "{0} pfennig"), FText::AsNumber(Pfennig));
	}

	FText CostSummary(const Hansa::Simulation::FHansaConstructionCostProjection& Cost, const bool bMissingOnly)
	{
		using namespace Hansa::Simulation;
		TArray<FText> Parts;
		const int64 Currency = bMissingOnly ? Cost.MissingCurrency.GetRawValue() : Cost.RequiredCurrency.GetRawValue();
		if (Currency > 0) Parts.Add(bMissingOnly
			? FText::Format(LOCTEXT("MissingMoney", "missing {0}"), InspectorPresentationModelMoney(Currency)) : InspectorPresentationModelMoney(Currency));
		for (const FHansaConstructionResourceCostProjection& Resource : Cost.Resources)
		{
			const int64 Amount = bMissingOnly ? Resource.Missing.GetRawValue() : Resource.Required.GetRawValue();
			if (Amount > 0) Parts.Add(FText::Format(bMissingOnly
				? LOCTEXT("MissingResource", "missing {0} {1}") : LOCTEXT("RequiredResource", "{0} {1}"),
				InspectorPresentationModelQuantity(Amount), InspectorPresentationModelStableLabel(Resource.GoodId.ToString())));
		}
		return Parts.IsEmpty() ? LOCTEXT("NoCost", "no cost") : FText::Join(LOCTEXT("CostSeparator", ", "), Parts);
	}

	FText GatewayFailure(const Hansa::Simulation::FHansaCommandGatewayResult& Result)
	{
		using namespace Hansa::Simulation;
		if (Result.GetError() == EHansaCommandGatewayError::ConstructionCostUnavailable && Result.GetConstructionCost().IsSet())
		{
			return FText::Format(LOCTEXT("CostUnavailable", "Insufficient upgrade resources: {0}."),
				CostSummary(Result.GetConstructionCost().GetValue(), true));
		}
		switch (Result.GetError())
		{
		case EHansaCommandGatewayError::ConstructionStateInvalid:
			return LOCTEXT("ActionConstructionState", "This action is not valid in the building's current construction state.");
		case EHansaCommandGatewayError::TargetHasDependents:
			return LOCTEXT("ActionHasDependents", "This completed building still owns production, residents, or inventory and cannot be demolished safely.");
		case EHansaCommandGatewayError::ResidenceProgressionUnavailable:
			return LOCTEXT("ActionProgressionUnavailable", "Upgrade prerequisites are not met. Check construction, satisfaction, resident capacity, and the next authored tier.");
		case EHansaCommandGatewayError::NotAuthorized:
			return LOCTEXT("ActionNotAuthorized", "Only the owning house may perform this action.");
		case EHansaCommandGatewayError::TargetNotFound:
			return LOCTEXT("ActionTargetMissing", "The building no longer exists.");
		default:
			return FText::Format(LOCTEXT("ActionRejected", "Action rejected: {0}."), FText::FromString(LexToString(Result.GetError())));
		}
	}
}

bool operator==(const FHansaCausalPresentation& Left, const FHansaCausalPresentation& Right)
{
	return Left.StableCode == Right.StableCode && InspectorPresentationModelTextEqual(Left.Problem, Right.Problem) &&
		InspectorPresentationModelTextEqual(Left.Cause, Right.Cause) && InspectorPresentationModelTextEqual(Left.Evidence, Right.Evidence) &&
		InspectorPresentationModelTextEqual(Left.Remedy, Right.Remedy) && Left.RelatedSemanticId == Right.RelatedSemanticId &&
		Left.Severity == Right.Severity;
}

bool operator==(const FHansaInspectorFlowPresentation& Left, const FHansaInspectorFlowPresentation& Right)
{
	return Left.DemandGoodId == Right.DemandGoodId && Left.DemandRequired == Right.DemandRequired &&
        Left.DemandSupplied == Right.DemandSupplied && Left.bDemandKnown == Right.bDemandKnown &&
        Left.StableId == Right.StableId && InspectorPresentationModelTextEqual(Left.Label, Right.Label) && InspectorPresentationModelTextEqual(Left.Value, Right.Value) &&
		InspectorPresentationModelTextEqual(Left.State, Right.State) && Left.bProblem == Right.bProblem;
}

bool operator==(const FHansaInspectorActionPresentation& Left, const FHansaInspectorActionPresentation& Right)
{
	return Left.StableId == Right.StableId && InspectorPresentationModelTextEqual(Left.Label, Right.Label) && InspectorPresentationModelTextEqual(Left.ToolTip, Right.ToolTip) &&
		InspectorPresentationModelTextEqual(Left.DisabledReason, Right.DisabledReason) && Left.bEnabled == Right.bEnabled && Left.bSelected == Right.bSelected;
}

bool operator==(const FHansaInspectorHistoryPresentation& Left, const FHansaInspectorHistoryPresentation& Right)
{
	return Left.StableId == Right.StableId && InspectorPresentationModelTextEqual(Left.Label, Right.Label) && InspectorPresentationModelTextEqual(Left.Age, Right.Age);
}

bool operator==(const FHansaInspectorSnapshot& Left, const FHansaInspectorSnapshot& Right)
{
	return Left.Residence == Right.Residence && Left.Production == Right.Production && Left.DataState == Right.DataState && Left.ObjectStableId == Right.ObjectStableId && InspectorPresentationModelTextEqual(Left.Identity, Right.Identity) &&
		InspectorPresentationModelTextEqual(Left.State, Right.State) && InspectorPresentationModelTextEqual(Left.PrimaryResult, Right.PrimaryResult) &&
		Left.Flows == Right.Flows && Left.Causal == Right.Causal && Left.Actions == Right.Actions &&
		Left.History == Right.History && InspectorPresentationModelTextEqual(Left.LastActionResult, Right.LastActionResult) &&
		Left.FocusOriginSemanticId == Right.FocusOriginSemanticId && Left.FocusedSemanticId == Right.FocusedSemanticId &&
		Left.PendingConfirmationAction == Right.PendingConfirmationAction &&
		Left.Kind == Right.Kind && Left.BuildingValue == Right.BuildingValue && Left.bOpen == Right.bOpen &&
		Left.bPinned == Right.bPinned && Left.bCauseExpanded == Right.bCauseExpanded;
}

void UHansaInspectorPresentationModel::BindRuntime(UHansaRuntimeSimulationHost* InRuntimeHost)
{
	RuntimeHost = InRuntimeHost;
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
		Result.Evidence = LOCTEXT("ProductionInactiveEvidence", "This building is paused.");
		Result.Remedy = LOCTEXT("ProductionInactiveRemedy", "Resume this production to restore local supply.");
		Result.Severity = EHansaCausalSeverity::Warning;
		return Result;
	}
	switch (Production.Blocker)
	{
	case EHansaProductionBlocker::None:
		Result.Problem = LOCTEXT("ProductionHealthy", "Production is operating");
		Result.Cause = LOCTEXT("ProductionHealthyCause", "Inputs and workforce are available");
		Result.Evidence = LOCTEXT("ProductionHealthyEvidence", "This building can continue its production cycle.");
		Result.Remedy = LOCTEXT("ProductionHealthyRemedy", "No action required.");
		Result.Severity = EHansaCausalSeverity::None;
		break;
	case EHansaProductionBlocker::MissingInput:
		Result.Problem = LOCTEXT("MissingInputProblem", "Missing production input");
		Result.Cause = FText::Format(LOCTEXT("MissingInputCause", "{0} is unavailable"), InspectorPresentationModelStableLabel(Production.BlockingGoodId.ToString()));
		Result.Evidence = FText::Format(LOCTEXT("MissingInputEvidence", "Available {0}; required {1}."),
			InspectorPresentationModelQuantity(Production.BlockingAvailableQuantity.GetRawValue()), InspectorPresentationModelQuantity(Production.BlockingRequiredQuantity.GetRawValue()));
		Result.Remedy = LOCTEXT("MissingInputRemedy", "Restore the missing good in connected storage or establish an incoming route.");
		Result.RelatedSemanticId = TEXT("CityOverview.Storage");
		Result.Severity = EHansaCausalSeverity::Warning;
		break;
	case EHansaProductionBlocker::StorageBlocked:
		Result.StableCode = TEXT("OutputFull");
		Result.Problem = LOCTEXT("OutputFullProblem", "Output storage is full");
		Result.Cause = LOCTEXT("OutputFullCause", "The completed batch has no free output capacity.");
		Result.Evidence = LOCTEXT("OutputFullEvidence", "Production is preserving its inputs until the output can be stored.");
		Result.Remedy = LOCTEXT("OutputFullRemedy", "Connect storage to the market road network or free output capacity.");
		Result.RelatedSemanticId = TEXT("CityOverview.Storage");
		Result.Severity = EHansaCausalSeverity::Warning;
		break;
	case EHansaProductionBlocker::InsufficientLaborerWorkforce:
	case EHansaProductionBlocker::InsufficientArtisanWorkforce:
		Result.Problem = LOCTEXT("WorkforceProblem", "Workforce shortage");
		Result.Cause = FText::FromString(LexToString(Production.Blocker));
		Result.Evidence = FText::Format(LOCTEXT("WorkforceEvidence", "Laborers {0}/{1}; artisans {2}/{3}."),
			FText::AsNumber(Production.AllocatedLaborerWorkforce), FText::AsNumber(Production.RequiredLaborerWorkforce),
			FText::AsNumber(Production.AllocatedArtisanWorkforce), FText::AsNumber(Production.RequiredArtisanWorkforce));
		Result.Remedy = LOCTEXT("WorkforceRemedy", "Add suitable residences or pause lower-priority production.");
		Result.RelatedSemanticId = TEXT("CityOverview.Population");
		Result.Severity = EHansaCausalSeverity::Warning;
		break;
	case EHansaProductionBlocker::ConstructionIncomplete:
		Result.Problem = LOCTEXT("ConstructionProblem", "Construction incomplete");
		Result.Cause = LOCTEXT("ConstructionCause", "The building is not ready to operate.");
		Result.Evidence = LOCTEXT("ConstructionEvidence", "Construction is still in progress.");
		Result.Remedy = LOCTEXT("ConstructionRemedy", "Wait for construction materials and completion.");
		Result.Severity = EHansaCausalSeverity::Notice;
		break;
	default:
		Result.Problem = LOCTEXT("BlockedProblem", "! Production blocked");
		Result.Cause = FText::FromString(LexToString(Production.Blocker));
		Result.Evidence = LOCTEXT("BlockedEvidence", "Production is waiting for the condition shown above.");
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
		Result.Evidence = LOCTEXT("ResidenceInactiveEvidence", "This residence is not ready to house residents.");
		Result.Remedy = LOCTEXT("ResidenceInactiveRemedy", "Complete or reconnect the residence."); Result.Severity = EHansaCausalSeverity::Critical;
	}
	else if (!Residence.bHasMarketAccess)
	{
		Result.StableCode = TEXT("ResidenceNoMarketAccess");
		Result.Problem = LOCTEXT("ResidenceNoMarketProblem", "No Market access");
		Result.Cause = LOCTEXT("ResidenceNoMarketCause", "This residence has no completed road route to a completed Market.");
		Result.Evidence = LOCTEXT("ResidenceNoMarketEvidence", "Bread, fish, and beer cannot reach this house, so no residents will move in.");
		Result.Remedy = LOCTEXT("ResidenceNoMarketRemedy", "Connect the residence and a completed Market to the same completed road network.");
		Result.Severity = EHansaCausalSeverity::Critical;
	}
	else if (Weakest != nullptr && Weakest->SatisfactionBasisPoints < 8000)
	{
		Result.StableCode = Weakest->NeedId.IsValid() ? FName(*Weakest->NeedId.ToString()) : TEXT("NeedLow");
		Result.Problem = FText::Format(LOCTEXT("NeedProblem", "{0} need is not fulfilled"), InspectorPresentationModelStableLabel(Weakest->NeedId.ToString()));
		Result.Cause = FText::Format(LOCTEXT("NeedCause", "{0} satisfaction is low"), InspectorPresentationModelStableLabel(Weakest->NeedId.ToString()));
		Result.Evidence = FText::Format(LOCTEXT("NeedEvidence", "Access {0}; affordability {1}; reliability {2}."),
			InspectorPresentationModelPercent(Weakest->AccessBasisPoints), InspectorPresentationModelPercent(Weakest->AffordabilityBasisPoints), InspectorPresentationModelPercent(Weakest->ReliabilityBasisPoints));
		Result.Remedy = LOCTEXT("NeedRemedy", "Improve access, affordability, or supply reliability for this need.");
		if (Weakest->GoodId.IsValid() && Weakest->AccessBasisPoints == 0)
		{
			Result.Cause = FText::Format(LOCTEXT("NeedNoAvailableStock",
				"The house has Market access, but no unreserved {0} is available in its supply inventory."),
				InspectorPresentationModelStableLabel(Weakest->GoodId.ToString()));
			Result.Remedy = Weakest->GoodId.ToString() == TEXT("Good.Bread")
				? LOCTEXT("BreadStockRecovery", "Supply bread from a Bakery or imports. A Grain Farm and Mill produce grain and flour; a Bakery is needed to make bread.")
				: LOCTEXT("GoodsStockRecovery", "Replenish this good through production or imports. Check current available stock and reservations in the Market view; the 30-day bars show past consumption.");
			Result.RelatedSemanticId = TEXT("CityOverview.Market");
		}
		else if (Weakest->GoodId.IsValid() && Weakest->AffordabilityBasisPoints < 8000)
		{
			Result.Cause = LOCTEXT("NeedUnaffordable", "Purchasing power limits how much of this good the residents can consume.");
		}
		else if (Weakest->GoodId.IsValid() && Weakest->ReliabilityBasisPoints < 8000)
		{
			Result.Cause = LOCTEXT("NeedInsufficientShare", "The available supply does not fulfill this household's demand.");
			Result.Remedy = LOCTEXT("NeedIncreaseSupply", "Increase production or imports. Scarce stock is shared between connected households.");
		}
		Result.Severity = Weakest->SatisfactionBasisPoints < 4000 ? EHansaCausalSeverity::Critical : EHansaCausalSeverity::Warning;
	}
	else
	{
		Result.StableCode = TEXT("ResidenceHealthy");
		Result.Problem = Residence.Residents == 0
			? LOCTEXT("ResidenceReadyForMigration", "Ready for migration")
			: LOCTEXT("ResidenceHealthy", "Needs are stable");
		Result.Cause = LOCTEXT("ResidenceHealthyCause", "No active residence problem");
		Result.Evidence = Residence.Residents == 0
			? LOCTEXT("ResidenceMigrationEvidence", "Market access and prospective needs currently meet the migration threshold.")
			: LOCTEXT("ResidenceHealthyEvidence", "Residents can meet their current needs.");
		Result.Remedy = LOCTEXT("ResidenceHealthyRemedy", "No action required."); Result.Severity = EHansaCausalSeverity::None;
	}
	return Result;
}

void UHansaInspectorPresentationModel::InitializeDefaults()
{
	FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot = {};
	SelectedBuildingDefinitionId.Reset();
	PublishIfChanged(Previous);
}

void UHansaInspectorPresentationModel::ShowStatus(EHansaInspectorDataState State, FText Detail, FText Remedy, FName FocusOrigin)
{
    if(State==EHansaInspectorDataState::Ready)return;
    const auto Previous=Snapshot;Snapshot={};SelectedBuildingDefinitionId.Reset();Snapshot.DataState=State;
    Snapshot.bOpen=true;Snapshot.FocusOriginSemanticId=FocusOrigin;
    Snapshot.Identity=State==EHansaInspectorDataState::Loading?LOCTEXT("LoadingSelection","Loading selection"):
        State==EHansaInspectorDataState::Error?LOCTEXT("UnavailableSelection","Unable to inspect selection"):LOCTEXT("EmptySelection","No object selected");
    Snapshot.State=Snapshot.Identity;Snapshot.PrimaryResult=Detail;Snapshot.Causal.Cause=MoveTemp(Detail);Snapshot.Causal.Remedy=MoveTemp(Remedy);
    Snapshot.Causal.Severity=State==EHansaInspectorDataState::Error?EHansaCausalSeverity::Critical:EHansaCausalSeverity::Notice;
    PublishIfChanged(Previous);
}

void UHansaInspectorPresentationModel::SetCommonActions()
{
	Snapshot.Actions = {
		Action(TEXT("Inspector.Action.Frame"), LOCTEXT("Frame", "Frame object [F]"), LOCTEXT("FrameTip", "Frame the affected object and preserve inspector focus.")),
		Action(TEXT("Inspector.Action.OpenRelated"), LOCTEXT("OpenRelated", "Open related view [C]"),
			FText::Format(LOCTEXT("OpenRelatedTip", "State: Available\nCause: {0}\nRemedy: {1}"), Snapshot.Causal.Cause, Snapshot.Causal.Remedy)),
		Action(TEXT("Inspector.Action.Pin"), Snapshot.bPinned ? LOCTEXT("Unpin", "Unpin tracker [P]") : LOCTEXT("Pin", "Pin tracker [P]"),
			LOCTEXT("PinTip", "Keep this object and its health visible in the HUD."))
	};
	Snapshot.Actions[1].bEnabled = !Snapshot.Causal.RelatedSemanticId.IsNone();
	if (!Snapshot.Actions[1].bEnabled) Snapshot.Actions[1].DisabledReason = LOCTEXT("NoRelatedTarget", "No related screen is available for this cause.");
	Snapshot.Actions[2].bSelected = Snapshot.bPinned;
    if (Snapshot.Production.bValid && !Snapshot.Production.bConstruction)
    {
        Snapshot.Actions.Add(Action(TEXT("Inspector.Action.ViewStorage"), LOCTEXT("ViewStorage", "View storage"),
            LOCTEXT("ViewStorageTip", "Open city stock and warehouse availability.")));
        Snapshot.Causal.RelatedSemanticId = TEXT("CityOverview.Production");
        Snapshot.Actions[1].bEnabled = true;
    }
	if (!SelectedBuildingDefinitionId.IsEmpty()) AppendWorldActions();
}

void UHansaInspectorPresentationModel::AppendWorldActions()
{
	using namespace Hansa::Simulation;
	UHansaRuntimeSimulationHost* Host = RuntimeHost.Get();
	const auto BuildingId = Snapshot.BuildingValue > 0
		? FHansaBuildingId::TryCreate(static_cast<uint64>(Snapshot.BuildingValue))
		: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
	const auto Projection = Host != nullptr ? Host->BuildProjection()
		: THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
	if (Host == nullptr || !BuildingId || !Projection) return;

	const FHansaConstructionProjection* Construction = Projection.Value.GetConstructions().FindByPredicate(
		[&BuildingId](const FHansaConstructionProjection& Value) { return Value.BuildingId == BuildingId.Value; });
	const FHansaProductionProjection* Production = Projection.Value.GetProductions().FindByPredicate(
		[&BuildingId](const FHansaProductionProjection& Value) { return Value.BuildingId == BuildingId.Value; });
	const FHansaPopulationCohortProjection* Residence = Projection.Value.GetPopulationCohorts().FindByPredicate(
		[&BuildingId](const FHansaPopulationCohortProjection& Value) { return Value.ResidenceBuildingId == BuildingId.Value; });

	if (Production != nullptr)
	{
		FHansaInspectorActionPresentation Toggle = Action(TEXT("Inspector.Action.ToggleProduction"),
			Production->bActive ? LOCTEXT("PauseProduction", "Pause production [O]") : LOCTEXT("ResumeProduction", "Resume production [O]"),
			Production->bActive ? LOCTEXT("PauseProductionTip", "Pause this building. Its production will stop until you resume it.")
			: LOCTEXT("ResumeProductionTip", "Resume production in this building."));
		Toggle.bSelected = false;
		Snapshot.Actions.Add(MoveTemp(Toggle));
	}

	const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
	const FHansaCompiledBuildingDefinition* Definition = Registry != nullptr
		? Registry->FindBuilding(SelectedBuildingDefinitionId) : nullptr;
	if (Residence != nullptr && Definition != nullptr && !Definition->UpgradeTargetBuildingId.IsEmpty())
	{
		const FHansaCommandGatewayResult Preview = Host->PreviewUpgradeResidence(BuildingId.Value);
		FHansaInspectorActionPresentation Upgrade = Action(TEXT("Inspector.Action.UpgradeResidence"),
			FText::Format(LOCTEXT("UpgradeResidence", "Upgrade to {0} [U]"), InspectorPresentationModelStableLabel(Definition->UpgradeTargetBuildingId)),
			LOCTEXT("UpgradeResidenceTip", "Move this residence and its population to the next authored tier."));
		Upgrade.bEnabled = Preview.IsSuccess();
		if (!Upgrade.bEnabled)
		{
			Upgrade.DisabledReason = GatewayFailure(Preview);
			if (Preview.GetError() == EHansaCommandGatewayError::ResidenceProgressionUnavailable)
			{
				const FHansaCompiledBuildingDefinition* Target = Registry->FindBuilding(Definition->UpgradeTargetBuildingId);
				const FHansaCompiledPopulationTierDefinition* SourceTier = Registry->FindPopulationTier(Definition->ResidentPopulationTierId);
				if (Target == nullptr || SourceTier == nullptr)
				{
					Upgrade.DisabledReason = LOCTEXT("UpgradeDefinitionMissing", "The authored next residence tier is unavailable.");
				}
				else if (Construction == nullptr || Construction->State != EHansaConstructionState::Completed)
				{
					Upgrade.DisabledReason = LOCTEXT("UpgradeResidenceIncomplete", "Complete construction before upgrading this residence.");
				}
				else if (Residence->Residents > Target->ResidenceCapacity)
				{
					Upgrade.DisabledReason = FText::Format(LOCTEXT("UpgradeCapacityFailure", "{0} residents cannot fit in the target capacity of {1}. Population must fall to {1} or fewer."),
						FText::AsNumber(Residence->Residents), FText::AsNumber(Target->ResidenceCapacity));
				}
				else if (Residence->SatisfactionBasisPoints < SourceTier->GrowthSatisfactionBasisPoints)
				{
					Upgrade.DisabledReason = FText::Format(LOCTEXT("UpgradeSatisfactionFailure", "Needs satisfaction is {0}; the upgrade requires {1}."),
						InspectorPresentationModelPercent(Residence->SatisfactionBasisPoints), InspectorPresentationModelPercent(SourceTier->GrowthSatisfactionBasisPoints));
				}
			}
		}
		const FHansaConstructionCostProjection Cost = Host->QueryConstructionCost(Definition->UpgradeTargetBuildingId);
		Upgrade.ToolTip = FText::Format(LOCTEXT("UpgradeCostTip", "Cost: {0}. Requires completed construction, qualifying satisfaction, and no more residents than the target capacity."), CostSummary(Cost, false));
		Snapshot.Actions.Add(MoveTemp(Upgrade));
	}

	if (Construction != nullptr && Construction->State == EHansaConstructionState::UnderConstruction)
	{
		const FHansaCommandGatewayResult Preview = Host->PreviewCancelConstruction(BuildingId.Value);
		const bool bArmed = Snapshot.PendingConfirmationAction == TEXT("Inspector.Action.CancelConstruction");
		FHansaInspectorActionPresentation Cancel = Action(TEXT("Inspector.Action.CancelConstruction"),
			bArmed ? LOCTEXT("ConfirmCancelConstruction", "Confirm cancellation [X]") : LOCTEXT("CancelConstruction", "Cancel construction [X]"),
			FText::Format(LOCTEXT("CancelConstructionTip", "Cancel this site and refund {0} plus the authored resource refund."), InspectorPresentationModelMoney(Construction->CancellationCurrencyRefund.GetRawValue())));
		Cancel.bEnabled = Preview.IsSuccess(); Cancel.bSelected = bArmed;
		if (!Cancel.bEnabled) Cancel.DisabledReason = GatewayFailure(Preview);
		Snapshot.Actions.Add(MoveTemp(Cancel));
	}
	else if (Construction != nullptr)
	{
		const FHansaCommandGatewayResult Preview = Host->PreviewRemoveBuilding(BuildingId.Value);
		const bool bArmed = Snapshot.PendingConfirmationAction == TEXT("Inspector.Action.RemoveBuilding");
		FHansaInspectorActionPresentation Remove = Action(TEXT("Inspector.Action.RemoveBuilding"),
			bArmed ? LOCTEXT("ConfirmRemoveBuilding", "Confirm demolition [X]") : LOCTEXT("RemoveBuilding", "Demolish building [X]"),
			LOCTEXT("RemoveBuildingTip", "Permanently remove this completed, dependent-free building. Completed demolition has no refund."));
		Remove.bEnabled = Preview.IsSuccess(); Remove.bSelected = bArmed;
		if (!Remove.bEnabled) Remove.DisabledReason = GatewayFailure(Preview);
		Snapshot.Actions.Add(MoveTemp(Remove));
	}
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
	SelectedBuildingDefinitionId.Reset();
	Snapshot = {};
	Snapshot.Kind = EHansaInspectorObjectKind::ProductionBuilding;
	Snapshot.ObjectStableId = FName(*Production.BuildingId.ToDebugString());
	Snapshot.BuildingValue = static_cast<int64>(Production.BuildingId.GetValue());
	Snapshot.Identity = InspectorPresentationModelStableLabel(Production.RecipeId.ToString());
	Snapshot.State = Production.bActive ? LOCTEXT("Operating", "Operating") : LOCTEXT("Paused", "Ⅱ Paused");
	Snapshot.PrimaryResult = Production.Outputs.IsEmpty()
		? LOCTEXT("NoOutput", "No output")
		: FText::Format(LOCTEXT("OutputResult", "{0} · {1} actual last tick\nCapacity: {2} / cycle"),
			InspectorPresentationModelStableLabel(Production.Outputs[0].GoodId.ToString()), InspectorPresentationModelQuantity(Production.Outputs[0].ActualQuantityLastTick.GetRawValue()),
			InspectorPresentationModelQuantity(Production.Outputs[0].NominalQuantityPerCycle.GetRawValue()));
	for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
	{
		FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Input.GoodId); Row.Label = InspectorPresentationModelStableLabel(Input.GoodId);
		Row.Value = FText::Format(LOCTEXT("InputRequired", "Required {0}"), InspectorPresentationModelQuantity(Input.QuantityMilliUnits));
		Row.bProblem = Production.Blocker == EHansaProductionBlocker::MissingInput && Production.BlockingGoodId.ToString() == Input.GoodId;
		Row.State = Row.bProblem ? LOCTEXT("Missing", "Missing") : LOCTEXT("Input", "Input"); Snapshot.Flows.Add(MoveTemp(Row));
	}
	for (const FHansaProductionThroughputProjection& Output : Production.Outputs)
	{
		FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Output.GoodId.ToString()); Row.Label = InspectorPresentationModelStableLabel(Output.GoodId.ToString());
		Row.Value = FText::Format(LOCTEXT("OutputActual", "{0} last tick · {1} / cycle"), InspectorPresentationModelQuantity(Output.ActualQuantityLastTick.GetRawValue()), InspectorPresentationModelQuantity(Output.NominalQuantityPerCycle.GetRawValue()));
		Row.State = LOCTEXT("Output", "Output"); Snapshot.Flows.Add(MoveTemp(Row));
	}
	Snapshot.Causal = MakeProductionCausalPresentation(Production);
	BuildProductionDetail(Production, Registry);
	ApplyProductionConnectivity();
	int32 HistoryIndex = 0;
	for (int32 Index = Events.Num() - 1; Index >= 0 && Snapshot.History.Num() < 3; --Index)
	{
		const FHansaDomainEvent& Event = Events[Index];
		if (Event.GetProductionId() != Production.Id) continue;
		FHansaInspectorHistoryPresentation Row; Row.StableId = FName(*FString::Printf(TEXT("ProductionHistory.%d"), HistoryIndex++));
		Row.Label = InspectorPresentationModelStableLabel(LexToString(Event.GetType())); Row.Age = FText::Format(LOCTEXT("TickAge", "Tick {0}"), FText::AsNumber(Event.GetTick().GetValue()));
		Snapshot.History.Add(MoveTemp(Row));
	}
	if (Snapshot.History.IsEmpty()) Snapshot.History.Add({ TEXT("ProductionHistory.Empty"), LOCTEXT("NoHistory", "No recent state changes"), FText::GetEmpty() });
	Snapshot.FocusOriginSemanticId = FocusOriginSemanticId; Snapshot.bOpen = true; Snapshot.bCauseExpanded = Previous.bOpen && Previous.Production.bValid && Previous.BuildingValue == Snapshot.BuildingValue && Previous.bCauseExpanded;
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

static FHansaInspectorResidenceData BuildResidenceDetail(const Hansa::Simulation::FHansaPopulationCohortProjection& P,const Hansa::Simulation::FHansaEconomicRegistry& Registry)
{
 using namespace Hansa::Simulation;
 FHansaInspectorResidenceData D;D.bValid=true;D.Residents=P.Residents;D.Capacity=P.ResidenceCapacity;D.Workforce=P.WorkforceSupply;
 D.ConsumptionPeriod=FormatMarketConsumptionPeriod(P.Consumption);D.CoveredMinutes=P.Consumption.CoveredMinutes;D.bFullWindow=P.Consumption.bFullWindow;
 FNumberFormattingOptions AmountFormat;AmountFormat.SetMaximumFractionalDigits(3);
 FNumberFormattingOptions PercentFormat;PercentFormat.SetMaximumFractionalDigits(1);
 auto Add=[&](const FString& Id){
  FHansaInspectorNeedData N;N.NeedId=FName(*Id);N.Label=InspectorPresentationModelStableLabel(Id);
  if(Id==TEXT("Need.BasicServices"))N.Label=LOCTEXT("NeedBasicServicesLabel","Basic services");
  else if(Id==TEXT("Need.Bread"))N.Label=LOCTEXT("NeedBreadLabel","Bread");
  else if(Id==TEXT("Need.Fish"))N.Label=LOCTEXT("NeedFishLabel","Fish");
  else if(Id==TEXT("Need.Beer"))N.Label=LOCTEXT("NeedBeerLabel","Beer");
  else if(Id==TEXT("Need.Tools"))N.Label=LOCTEXT("NeedToolsLabel","Tools");
  const auto* Def=Registry.FindNeed(Id);
  if(Def){N.GoodId=FName(*Def->GoodId);N.bService=Def->Kind==EHansaCompiledNeedKind::Service;}
  if(const auto* Value=P.Needs.FindByPredicate([&](const auto& V){return V.NeedId.ToString()==Id;})){
   N.bKnown=true;N.Fulfillment=Value->SatisfactionBasisPoints;N.Access=Value->AccessBasisPoints;N.Affordability=Value->AffordabilityBasisPoints;N.Reliability=Value->ReliabilityBasisPoints;
  }
  N.Percent=N.bKnown?FText::AsPercent(N.Fulfillment/10000.,&PercentFormat):LOCTEXT("ResidenceUnknownPercent","—");
  if(N.bService) N.Amount=LOCTEXT("ResidenceCurrentService","Current service level");
  else
  {
   N.bKnown=P.Consumption.CoveredMinutes>0;N.Fulfillment=0;
   if(const auto* Total=P.Consumption.Goods.FindByPredicate([&](const auto& T){return T.CityId==P.CityId && FName(*T.GoodId.ToString())==N.GoodId;}))
   {N.Required=Total->Required;N.Consumed=Total->Consumed;}
   N.Percent=LOCTEXT("ResidenceUnknownPercent","—");
   if(!N.bKnown) N.Amount=LOCTEXT("ResidencePendingConsumption","Awaiting consumption history");
   else if(N.Required==0) N.Amount=LOCTEXT("ResidenceNoDemand","No demand in this period");
   else
   {
    const double Ratio=double(N.Consumed)/N.Required;
    N.Fulfillment=FMath::RoundToInt(Ratio*10000);
    N.Percent=FText::AsPercent(Ratio,&PercentFormat);
    N.Amount=FText::Format(LOCTEXT("ResidenceConsumptionAmounts","{0} / {1} units consumed"),
     FText::AsNumber(N.Consumed/1000.,&AmountFormat),FText::AsNumber(N.Required/1000.,&AmountFormat));
   }
  }
  D.Needs.Add(MoveTemp(N));
 };
 if(const auto* Tier=Registry.FindPopulationTier(P.TierId.ToString()))for(const auto& N:Tier->Needs)Add(N.NeedId);
 for(const auto& N:P.Needs)if(!D.Needs.ContainsByPredicate([&](const auto& V){return V.NeedId==FName(*N.NeedId.ToString());}))Add(N.NeedId.ToString());
 return D;
}

bool UHansaInspectorPresentationModel::ShowResidence(
	const Hansa::Simulation::FHansaPopulationCohortProjection& Residence,
	const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const FName FocusOriginSemanticId)
{
	using namespace Hansa::Simulation;
	if (!Residence.Id.IsValid() || !Residence.ResidenceBuildingId.IsValid() || Registry.FindPopulationTier(Residence.TierId.ToString()) == nullptr) return false;
	const FHansaInspectorSnapshot Previous = Snapshot;
	SelectedBuildingDefinitionId.Reset();
	Snapshot = {};
	Snapshot.Residence=BuildResidenceDetail(Residence,Registry);
	Snapshot.Kind = EHansaInspectorObjectKind::Residence; Snapshot.ObjectStableId = FName(*Residence.ResidenceBuildingId.ToDebugString());
	Snapshot.BuildingValue = static_cast<int64>(Residence.ResidenceBuildingId.GetValue());
	Snapshot.Identity = FText::Format(LOCTEXT("ResidenceIdentity", "{0} residence"), InspectorPresentationModelStableLabel(Residence.TierId.ToString()));
	Snapshot.State = Residence.bResidenceOperational ? LOCTEXT("ResidenceOperating", "Occupied") : LOCTEXT("ResidenceUnavailable", "! Unavailable");
	Snapshot.PrimaryResult = FText::Format(LOCTEXT("ResidenceResult", "{0}/{1} residents · satisfaction {2}"),
		FText::AsNumber(Residence.Residents), FText::AsNumber(Residence.ResidenceCapacity), InspectorPresentationModelPercent(Residence.SatisfactionBasisPoints));
	for (const FHansaPopulationNeedState& Need : Residence.Needs)
	{
		FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Need.NeedId.ToString()); Row.Label = InspectorPresentationModelStableLabel(Need.NeedId.ToString());
		Row.Value = FText::Format(LOCTEXT("NeedValue", "access {0} · affordability {1} · reliability {2}"),
			InspectorPresentationModelPercent(Need.AccessBasisPoints), InspectorPresentationModelPercent(Need.AffordabilityBasisPoints), InspectorPresentationModelPercent(Need.ReliabilityBasisPoints));
		Row.State = FText::Format(LOCTEXT("NeedSatisfaction", "{0} satisfied"), InspectorPresentationModelPercent(Need.SatisfactionBasisPoints));
		Row.bProblem = Need.SatisfactionBasisPoints < 8000; Snapshot.Flows.Add(MoveTemp(Row));
	}
	Snapshot.Causal = MakeResidenceCausalPresentation(Residence);
	Snapshot.History = { { TEXT("ResidenceHistory.Current"), FText::Format(LOCTEXT("ResidenceTrend", "Resident change: {0}"), FText::AsNumber(Residence.ResidentChangeLastTick)), LOCTEXT("Current", "Current") } };
	Snapshot.FocusOriginSemanticId = FocusOriginSemanticId; Snapshot.bOpen = true; Snapshot.bCauseExpanded = Previous.bOpen && Previous.ObjectStableId==Snapshot.ObjectStableId && Previous.bCauseExpanded;
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
	using namespace Hansa::Simulation;
	const FHansaInspectorSnapshot Previous = Snapshot;
	const bool bRefreshingSameBuilding = Snapshot.bOpen && Snapshot.BuildingValue == BuildingValue;
	const bool bWasPinned = Snapshot.bPinned;
	const bool bWasCauseExpanded = Snapshot.bCauseExpanded;
	const FName PreviousFocusOrigin = Snapshot.FocusOriginSemanticId;
	const FName PreviousFocus = Snapshot.FocusedSemanticId;
	const FText PreviousActionResult = Snapshot.LastActionResult;
	const FName PreviousConfirmation = Snapshot.PendingConfirmationAction;
	Snapshot = {};
	SelectedBuildingDefinitionId = BuildingDefinitionId;
	Snapshot.Kind = BuildingDefinitionId.Contains(TEXT("Residence")) ? EHansaInspectorObjectKind::Residence : EHansaInspectorObjectKind::ProductionBuilding;
	Snapshot.ObjectStableId = FName(*FString::Printf(TEXT("Building.%lld"), BuildingValue)); Snapshot.BuildingValue = BuildingValue;
	Snapshot.Identity = DisplayName.IsEmpty() ? InspectorPresentationModelStableLabel(BuildingDefinitionId) : MoveTemp(DisplayName);
	Snapshot.State = FText::FromString(WorldState);
	Snapshot.PrimaryResult = WorldState == TEXT("UnderConstruction") ? LOCTEXT("ConstructionResult", "Construction in progress") : LOCTEXT("ReadyResult", "Ready for operation");
	bool bAppliedDetailedProjection = false;
	UHansaRuntimeSimulationHost* Host = RuntimeHost.Get();
	const auto TypedBuildingId = BuildingValue > 0
		? FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue))
		: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
	const auto Projection = Host != nullptr ? Host->BuildProjection()
		: THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
	if (TypedBuildingId && Projection)
	{
		const FHansaConstructionProjection* Construction = Projection.Value.GetConstructions().FindByPredicate(
			[&TypedBuildingId](const FHansaConstructionProjection& Value) { return Value.BuildingId == TypedBuildingId.Value; });
		const FHansaProductionProjection* Production = Projection.Value.GetProductions().FindByPredicate(
			[&TypedBuildingId](const FHansaProductionProjection& Value) { return Value.BuildingId == TypedBuildingId.Value; });
		const FHansaPopulationCohortProjection* Residence = Projection.Value.GetPopulationCohorts().FindByPredicate(
			[&TypedBuildingId](const FHansaPopulationCohortProjection& Value) { return Value.ResidenceBuildingId == TypedBuildingId.Value; });
		if (Construction != nullptr && Construction->State == EHansaConstructionState::UnderConstruction)
		{
			auto& Detail = Snapshot.Production;
            Detail.bValid = true;
            Detail.bConstruction = true;
            Detail.bActive = true;
            Detail.bCanProgress = true;
            Detail.bSimulationPaused = Host->GetSpeed() == EHansaRuntimeSimulationSpeed::Paused;
            Detail.ProgressTicks = Construction->ElapsedTicks;
            Detail.CycleTicks = Construction->TotalTicks;
            const int32 ProgressPercent = static_cast<int32>(Construction->Progress.GetPartsPerMillion() / 10000);
			Snapshot.State = FText::Format(LOCTEXT("ConstructionState", "Under construction · {0}%"), FText::AsNumber(ProgressPercent));
			Snapshot.PrimaryResult = FText::Format(LOCTEXT("ConstructionProgress", "Build progress {0}/{1} ticks · cancel refund {2}"),
				FText::AsNumber(Construction->ElapsedTicks), FText::AsNumber(Construction->TotalTicks),
				InspectorPresentationModelMoney(Construction->CancellationCurrencyRefund.GetRawValue()));
			FHansaInspectorFlowPresentation Progress;
			Progress.StableId = TEXT("Construction.Progress"); Progress.Label = LOCTEXT("ConstructionProgressLabel", "Construction");
			Progress.Value = FText::Format(LOCTEXT("ConstructionProgressValue", "{0}% complete"), FText::AsNumber(ProgressPercent));
			Progress.State = LOCTEXT("ConstructionWorking", "Building"); Snapshot.Flows.Add(MoveTemp(Progress));
			for (const FHansaConstructionResourceCostProjection& Refund : Construction->CancellationResourceRefunds)
			{
				FHansaInspectorFlowPresentation Row; Row.StableId = FName(*FString::Printf(TEXT("Refund.%s"), *Refund.GoodId.ToString()));
				Row.Label = InspectorPresentationModelStableLabel(Refund.GoodId.ToString()); Row.Value = InspectorPresentationModelQuantity(Refund.Required.GetRawValue());
				Row.State = LOCTEXT("CancellationRefund", "Cancellation refund"); Snapshot.Flows.Add(MoveTemp(Row));
			}
			Snapshot.Causal.StableCode = TEXT("ConstructionIncomplete");
			Snapshot.Causal.Problem = LOCTEXT("ConstructionActive", "Construction is in progress");
			Snapshot.Causal.Cause = LOCTEXT("ConstructionActiveCause", "Construction has not finished.");
			Snapshot.Causal.Evidence = FText::Format(LOCTEXT("ConstructionActiveEvidence", "{0} of {1} construction ticks have elapsed."), FText::AsNumber(Construction->ElapsedTicks), FText::AsNumber(Construction->TotalTicks));
			Snapshot.Causal.Remedy = LOCTEXT("ConstructionActiveRemedy", "Wait for completion, or cancel the site to recover the displayed refund.");
			Snapshot.Causal.Severity = EHansaCausalSeverity::Notice;
			bAppliedDetailedProjection = true;
		}
		else if (BuildingDefinitionId == TEXT("Building.Market"))
        {
            Snapshot.Kind = EHansaInspectorObjectKind::Market;
            const auto* Building = Projection.Value.GetBuildingWorldProjections().FindByPredicate(
                [&](const auto& B) { return B.BuildingId == TypedBuildingId.Value; });
            const auto* Registry = Host->GetEconomicRegistry();
            if (Building && Registry)
            {
                Snapshot.Flows = BuildMarketDemandFlows(Projection.Value.GetPopulationCohorts(), *Registry, Building->Placement.CityId.ToString(), Projection.Value.GetCitizenConsumption());
                Snapshot.PrimaryResult = FText::Format(LOCTEXT("MarketRollingScope", "City-wide citizen supply\n{0}"), FormatMarketConsumptionPeriod(Projection.Value.GetCitizenConsumption()));
                Snapshot.Causal.Problem = LOCTEXT("MarketDemandExplanation", "Supply fulfillment");
                Snapshot.Causal.Cause = LOCTEXT("MarketDemandCause", "Each bar compares total goods consumed with total citizen demand over the recorded period, up to 30 game days. Stock and incoming shipments count only when consumed. Access and affordability can limit fulfillment.");
                Snapshot.Causal.Remedy = LOCTEXT("MarketDemandRemedy", "For shortages, check production, deliveries, market access and affordability.");
                Snapshot.Causal.RelatedSemanticId = TEXT("CityOverview.Market");
            }
            else
            {
                Snapshot.PrimaryResult = LOCTEXT("MarketUnavailable", "Citizen demand unavailable");
                Snapshot.Causal.Problem = Snapshot.PrimaryResult;
                Snapshot.Causal.Remedy = LOCTEXT("MarketRetry", "Select the market again when city data is available.");
            }
            bAppliedDetailedProjection = true;
        }
		else if (Residence != nullptr)
		{
			if(const auto* Registry=Host->GetEconomicRegistry())Snapshot.Residence=BuildResidenceDetail(*Residence,*Registry);
			Snapshot.Kind = EHansaInspectorObjectKind::Residence;
			Snapshot.State = Residence->bResidenceOperational ? LOCTEXT("WorldResidenceOccupied", "Occupied") : LOCTEXT("WorldResidenceUnavailable", "! Unavailable");
			Snapshot.PrimaryResult = FText::Format(LOCTEXT("WorldResidenceResult", "{0}/{1} residents · satisfaction {2} · workforce {3}"),
				FText::AsNumber(Residence->Residents), FText::AsNumber(Residence->ResidenceCapacity), InspectorPresentationModelPercent(Residence->SatisfactionBasisPoints), FText::AsNumber(Residence->WorkforceSupply));
			for (const FHansaPopulationNeedState& Need : Residence->Needs)
			{
				FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Need.NeedId.ToString()); Row.Label = InspectorPresentationModelStableLabel(Need.NeedId.ToString());
				Row.Value = FText::Format(LOCTEXT("WorldNeedValue", "access {0} · affordability {1} · reliability {2}"),
					InspectorPresentationModelPercent(Need.AccessBasisPoints), InspectorPresentationModelPercent(Need.AffordabilityBasisPoints), InspectorPresentationModelPercent(Need.ReliabilityBasisPoints));
				Row.State = FText::Format(LOCTEXT("WorldNeedState", "{0} satisfied"), InspectorPresentationModelPercent(Need.SatisfactionBasisPoints));
				Row.bProblem = Need.SatisfactionBasisPoints < 8000; Snapshot.Flows.Add(MoveTemp(Row));
			}
			if (Residence->Needs.IsEmpty())
			{
				const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
				const FHansaCompiledPopulationTierDefinition* Tier = Registry != nullptr
					? Registry->FindPopulationTier(Residence->TierId.ToString()) : nullptr;
				if (Tier != nullptr)
				{
					for (const FHansaCompiledPopulationTierNeed& Need : Tier->Needs)
					{
						FHansaInspectorFlowPresentation Row;
						Row.StableId = FName(*Need.NeedId);
						Row.Label = InspectorPresentationModelStableLabel(Need.NeedId);
						Row.Value = FText::Format(
							LOCTEXT("WorldAuthoredNeedValue", "Requires {0} per resident each tick"),
							InspectorPresentationModelQuantity(Need.ConsumptionMilliUnitsPerResidentPerTick));
						Row.State = LOCTEXT("WorldNeedPending", "Awaiting first simulation tick");
						Snapshot.Flows.Add(MoveTemp(Row));
					}
				}
			}
			Snapshot.Causal = MakeResidenceCausalPresentation(*Residence);
			bAppliedDetailedProjection = true;
		}
		else if (Production != nullptr)
		{
			Snapshot.Kind = EHansaInspectorObjectKind::ProductionBuilding;
			Snapshot.State = Production->bActive ? LOCTEXT("WorldOperating", "Operating") : LOCTEXT("WorldPaused", "Ⅱ Paused");
			Snapshot.PrimaryResult = Production->Outputs.IsEmpty() ? LOCTEXT("WorldNoOutput", "No output this cycle")
				: FText::Format(LOCTEXT("WorldOutput", "{0} · {1} actual last tick\nCapacity: {2} / cycle"), InspectorPresentationModelStableLabel(Production->Outputs[0].GoodId.ToString()),
					InspectorPresentationModelQuantity(Production->Outputs[0].ActualQuantityLastTick.GetRawValue()), InspectorPresentationModelQuantity(Production->Outputs[0].NominalQuantityPerCycle.GetRawValue()));
			const FHansaEconomicRegistry* Registry = Host != nullptr ? Host->GetEconomicRegistry() : nullptr;
			const FHansaCompiledRecipeDefinition* Recipe = Registry != nullptr ? Registry->FindRecipe(Production->RecipeId.ToString()) : nullptr;
			if (Recipe != nullptr) for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
			{
				FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Input.GoodId); Row.Label = InspectorPresentationModelStableLabel(Input.GoodId);
				Row.Value = FText::Format(LOCTEXT("WorldInputRequired", "Required {0}"), InspectorPresentationModelQuantity(Input.QuantityMilliUnits)); Row.State = LOCTEXT("WorldInput", "Input");
				Row.bProblem = Production->Blocker == EHansaProductionBlocker::MissingInput && Production->BlockingGoodId.ToString() == Input.GoodId; Snapshot.Flows.Add(MoveTemp(Row));
			}
			for (const FHansaProductionThroughputProjection& Output : Production->Outputs)
			{
				FHansaInspectorFlowPresentation Row; Row.StableId = FName(*Output.GoodId.ToString()); Row.Label = InspectorPresentationModelStableLabel(Output.GoodId.ToString());
				Row.Value = FText::Format(LOCTEXT("WorldOutputActual", "{0} last tick · {1} / cycle"), InspectorPresentationModelQuantity(Output.ActualQuantityLastTick.GetRawValue()), InspectorPresentationModelQuantity(Output.NominalQuantityPerCycle.GetRawValue()));
				Row.State = LOCTEXT("WorldOutputState", "Output"); Snapshot.Flows.Add(MoveTemp(Row));
			}
			Snapshot.Causal = MakeProductionCausalPresentation(*Production);
			if (Registry != nullptr)
			{
				BuildProductionDetail(*Production, *Registry);
				ApplyProductionConnectivity();
			}
			bAppliedDetailedProjection = true;
		}
	}
	if (!bAppliedDetailedProjection && !DefinitionFlow.IsEmpty())
	{
		FHansaInspectorFlowPresentation Flow;
		Flow.StableId = TEXT("DefinitionFlow");
		Flow.Label = Snapshot.Kind == EHansaInspectorObjectKind::Residence ? LOCTEXT("ResidenceNeeds", "Needs and workforce") : LOCTEXT("ProductionFlow", "Production flow");
		Flow.Value = MoveTemp(DefinitionFlow);
		Flow.State = LOCTEXT("DefinitionValue", "Definition");
		Snapshot.Flows.Add(MoveTemp(Flow));
	}
	if (!bAppliedDetailedProjection)
	{
		Snapshot.Causal.StableCode = FName(*ProductionBlocker);
		Snapshot.Causal.Problem = ProductionBlocker == TEXT("None") ? LOCTEXT("WorldHealthy", "No active problem") : FText::Format(LOCTEXT("WorldProblem", "{0}"), FText::FromString(ProductionBlocker));
		Snapshot.Causal.Cause = ProductionBlocker == TEXT("None") ? LOCTEXT("WorldHealthyCause", "Inputs and workforce are available") : FText::FromString(ProductionBlocker);
		Snapshot.Causal.Evidence = LOCTEXT("WorldEvidence", "Current building status.");
		Snapshot.Causal.Remedy = ProductionBlocker == TEXT("None") ? LOCTEXT("WorldNoRemedy", "No action required.") : LOCTEXT("WorldRemedy", "Open the related city system for the complete causal breakdown.");
		Snapshot.Causal.RelatedSemanticId = TEXT("CityOverview.Production");
		Snapshot.Causal.Severity = ProductionBlocker == TEXT("None") ? EHansaCausalSeverity::None : EHansaCausalSeverity::Warning;
	}
	if(Host && TypedBuildingId){
        const auto Events=Host->GetEventHistory();
        for(int32 Index=Events.Num()-1;Index>=0 && Snapshot.History.Num()<3;--Index){
            const auto& Event=Events[Index];if(Event.GetBuildingId()!=TypedBuildingId.Value)continue;
            FHansaInspectorHistoryPresentation Row;Row.StableId=FName(*FString::Printf(TEXT("WorldHistory.%d"),Index));
            Row.Label=InspectorPresentationModelStableLabel(LexToString(Event.GetType()));Row.Age=FText::Format(LOCTEXT("WorldHistoryTick","Tick {0}"),FText::AsNumber(Event.GetTick().GetValue()));Snapshot.History.Add(MoveTemp(Row));
        }
    }
    if(Snapshot.History.IsEmpty())Snapshot.History={{TEXT("WorldHistory.Empty"),LOCTEXT("WorldNoHistory","No recent state changes recorded."),FText()}};
	Snapshot.FocusOriginSemanticId = bRefreshingSameBuilding ? PreviousFocusOrigin : FocusOriginSemanticId;
	Snapshot.FocusedSemanticId = bRefreshingSameBuilding ? PreviousFocus : FName();
	Snapshot.LastActionResult = bRefreshingSameBuilding ? PreviousActionResult : FText();
	Snapshot.PendingConfirmationAction = bRefreshingSameBuilding ? PreviousConfirmation : FName();
	Snapshot.bPinned = bRefreshingSameBuilding && bWasPinned;
	Snapshot.bCauseExpanded = bRefreshingSameBuilding ? bWasCauseExpanded : !Snapshot.Production.bValid && !Snapshot.Residence.bValid && Snapshot.Kind != EHansaInspectorObjectKind::Market && Snapshot.Causal.Severity != EHansaCausalSeverity::None;
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
	SelectedBuildingDefinitionId.Reset();
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
	Snapshot.LastActionResult = Snapshot.bPinned ? LOCTEXT("PinnedResult", "Pinned tracker added") : LOCTEXT("UnpinnedResult", "Pinned tracker removed");
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::FrameIntent()
{
    if(Snapshot.Kind==EHansaInspectorObjectKind::Cargo && !IsActionEnabled(TEXT("Inspector.Action.Frame")))return false;
	if (!Snapshot.bOpen || (Snapshot.BuildingValue <= 0 && Snapshot.Kind != EHansaInspectorObjectKind::Cargo)) return false;
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.LastActionResult = LOCTEXT("FramedResult", "Object framed");
	PublishIfChanged(Previous); FrameRequested.Broadcast(Snapshot.BuildingValue); return true;
}

bool UHansaInspectorPresentationModel::OpenCauseIntent()
{
	if (!Snapshot.bOpen) return false;
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.bCauseExpanded = (Snapshot.Production.bValid || Snapshot.Residence.bValid || Snapshot.Kind == EHansaInspectorObjectKind::Market) ? !Snapshot.bCauseExpanded : true; Snapshot.FocusedSemanticId = (Snapshot.Production.bValid || Snapshot.Residence.bValid || Snapshot.Kind == EHansaInspectorObjectKind::Market) ? TEXT("Inspector.Action.OpenCause") : TEXT("Inspector.Problem.Cause");
	Snapshot.LastActionResult = LOCTEXT("CauseOpened", "Cause and remedy opened"); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::OpenRelatedIntent()
{
	if (!Snapshot.bOpen || Snapshot.Causal.RelatedSemanticId.IsNone()) return false;
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot.LastActionResult = FText::Format(LOCTEXT("RelatedOpened", "Opened related view · {0}"), FText::FromName(Snapshot.Causal.RelatedSemanticId));
	PublishIfChanged(Previous); RelatedTargetRequested.Broadcast(Snapshot.Causal.RelatedSemanticId); return true;
}

bool UHansaInspectorPresentationModel::IsActionEnabled(const FName SemanticId) const
{
	const FHansaInspectorActionPresentation* Found = Snapshot.Actions.FindByPredicate(
		[SemanticId](const FHansaInspectorActionPresentation& ActionValue) { return ActionValue.StableId == SemanticId; });
	return Snapshot.bOpen && Found != nullptr && Found->bEnabled;
}

bool UHansaInspectorPresentationModel::ArmDestructiveAction(const FName SemanticId, const FText& Confirmation)
{
	if (!IsActionEnabled(SemanticId)) return false;
	if (Snapshot.PendingConfirmationAction == SemanticId) return true;
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot.PendingConfirmationAction = SemanticId;
	Snapshot.LastActionResult = Confirmation;
	SetCommonActions();
	PublishIfChanged(Previous);
	return false;
}

bool UHansaInspectorPresentationModel::ToggleProductionIntent()
{
	using namespace Hansa::Simulation;
	if (!IsActionEnabled(TEXT("Inspector.Action.ToggleProduction"))) return false;
	UHansaRuntimeSimulationHost* Host = RuntimeHost.Get();
	const auto BuildingId = Snapshot.BuildingValue > 0 ? FHansaBuildingId::TryCreate(static_cast<uint64>(Snapshot.BuildingValue))
		: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
	const auto Projection = Host != nullptr ? Host->BuildProjection()
		: THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
	if (!BuildingId || !Projection) return false;
	const FHansaProductionProjection* Production = Projection.Value.GetProductions().FindByPredicate(
		[&BuildingId](const FHansaProductionProjection& Value) { return Value.BuildingId == BuildingId.Value; });
	if (Production == nullptr) return false;
	const bool bActivate = !Production->bActive;
	const FHansaCommandGatewayResult Result = Host->SetProductionActive(Production->Id, bActivate);
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot.PendingConfirmationAction = NAME_None;
	Snapshot.LastActionResult = Result.IsSuccess()
		? (bActivate ? LOCTEXT("ProductionResumedResult", "Production resumed") : LOCTEXT("ProductionPausedResult", "Ⅱ Production paused"))
		: GatewayFailure(Result);
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::UpgradeResidenceIntent()
{
	using namespace Hansa::Simulation;
	if (!IsActionEnabled(TEXT("Inspector.Action.UpgradeResidence"))) return false;
	UHansaRuntimeSimulationHost* Host = RuntimeHost.Get();
	const auto BuildingId = Snapshot.BuildingValue > 0 ? FHansaBuildingId::TryCreate(static_cast<uint64>(Snapshot.BuildingValue))
		: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
	if (Host == nullptr || !BuildingId) return false;
	const FHansaCommandGatewayResult Result = Host->UpgradeResidence(BuildingId.Value);
	const FHansaInspectorSnapshot Previous = Snapshot;
	Snapshot.PendingConfirmationAction = NAME_None;
	Snapshot.LastActionResult = Result.IsSuccess() ? LOCTEXT("ResidenceUpgradedResult", "Residence upgraded; population and building appearance moved to the next tier") : GatewayFailure(Result);
	SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::CancelConstructionIntent()
{
	using namespace Hansa::Simulation;
	const FName ActionId(TEXT("Inspector.Action.CancelConstruction"));
	if (!IsActionEnabled(ActionId)) return false;
	if (!ArmDestructiveAction(ActionId, LOCTEXT("CancelArmed", "Activate Cancel construction again to confirm the refund and removal."))) return true;
	UHansaRuntimeSimulationHost* Host = RuntimeHost.Get();
	const auto BuildingId = Snapshot.BuildingValue > 0 ? FHansaBuildingId::TryCreate(static_cast<uint64>(Snapshot.BuildingValue))
		: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
	if (Host == nullptr || !BuildingId) return false;
	const FHansaCommandGatewayResult Result = Host->CancelConstruction(BuildingId.Value);
	if (Result.IsSuccess())
	{
		if (Snapshot.bOpen) CloseIntent();
		return true;
	}
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.PendingConfirmationAction = NAME_None;
	Snapshot.LastActionResult = GatewayFailure(Result); SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::RemoveBuildingIntent()
{
	using namespace Hansa::Simulation;
	const FName ActionId(TEXT("Inspector.Action.RemoveBuilding"));
	if (!IsActionEnabled(ActionId)) return false;
	if (!ArmDestructiveAction(ActionId, LOCTEXT("RemoveArmed", "Activate Demolish building again to confirm permanent removal without a refund."))) return true;
	UHansaRuntimeSimulationHost* Host = RuntimeHost.Get();
	const auto BuildingId = Snapshot.BuildingValue > 0 ? FHansaBuildingId::TryCreate(static_cast<uint64>(Snapshot.BuildingValue))
		: THansaValueResult<FHansaBuildingId>::Failure(EHansaValueError::InvalidZero);
	if (Host == nullptr || !BuildingId) return false;
	const FHansaCommandGatewayResult Result = Host->RemoveBuilding(BuildingId.Value);
	if (Result.IsSuccess())
	{
		if (Snapshot.bOpen) CloseIntent();
		return true;
	}
	const FHansaInspectorSnapshot Previous = Snapshot; Snapshot.PendingConfirmationAction = NAME_None;
	Snapshot.LastActionResult = GatewayFailure(Result); SetCommonActions(); PublishIfChanged(Previous); return true;
}

bool UHansaInspectorPresentationModel::ActivateAction(const FName SemanticId)
{
	if(Snapshot.DataState!=EHansaInspectorDataState::Ready && SemanticId!=TEXT("Inspector.Close"))return false;
	if (SemanticId == TEXT("Inspector.Close")) return CloseIntent();
	if (SemanticId == TEXT("Inspector.Action.Frame")) return FrameIntent();
	if (SemanticId == TEXT("Inspector.Action.Pin")) return TogglePinIntent();
	if (SemanticId == TEXT("Inspector.Action.OpenCause")) return OpenCauseIntent();
    if (SemanticId == TEXT("Inspector.Action.ViewStorage"))
    {
        if (!IsActionEnabled(SemanticId)) return false;
        RelatedTargetRequested.Broadcast(TEXT("CityOverview.Market")); return true;
    }
	if (SemanticId == TEXT("Inspector.Action.OpenRelated")) return OpenRelatedIntent();
	if (SemanticId == TEXT("Inspector.Action.ToggleProduction")) return ToggleProductionIntent();
	if (SemanticId == TEXT("Inspector.Action.UpgradeResidence")) return UpgradeResidenceIntent();
	if (SemanticId == TEXT("Inspector.Action.CancelConstruction")) return CancelConstructionIntent();
	if (SemanticId == TEXT("Inspector.Action.RemoveBuilding")) return RemoveBuildingIntent();
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

void UHansaInspectorPresentationModel::ShowCargo(const FHansaCargoWorldObservation& O, FName FocusOrigin)
{
    const auto Previous=Snapshot;
    const bool Same=Snapshot.Kind==EHansaInspectorObjectKind::Cargo && Snapshot.ObjectStableId==O.SemanticId;
    Snapshot={}; SelectedBuildingDefinitionId.Reset();
    Snapshot.Kind=EHansaInspectorObjectKind::Cargo; Snapshot.ObjectStableId=O.SemanticId; Snapshot.bOpen=true;
    Snapshot.FocusOriginSemanticId=Same?Previous.FocusOriginSemanticId:FocusOrigin;
    Snapshot.FocusedSemanticId=Same?Previous.FocusedSemanticId:TEXT("Inspector.Close");
    Snapshot.bPinned=Same&&Previous.bPinned; Snapshot.bCauseExpanded=Same&&Previous.bCauseExpanded;
    const FText GoodLabel=InspectorPresentationModelStableLabel(O.GoodId.ToString());
    const FText SourceLabel=InspectorPresentationModelStableLabel(
        O.SourceBuildingDefinitionId.IsNone()?O.SourceBuildingId:O.SourceBuildingDefinitionId.ToString());
    const FText DestinationLabel=InspectorPresentationModelStableLabel(
        O.DestinationBuildingDefinitionId.IsNone()?O.DestinationBuildingId:O.DestinationBuildingDefinitionId.ToString());
    Snapshot.Identity=O.JobId.IsEmpty()?LOCTEXT("CargoCog","Cargo cog"):
        FText::Format(LOCTEXT("CargoWagon","{0} delivery wagon"),GoodLabel);
    switch(O.Phase)
    {
        case EHansaCargoWorldPhase::Loading: Snapshot.State=LOCTEXT("CargoLoading","Loading recorded");break;
        case EHansaCargoWorldPhase::Departing: Snapshot.State=LOCTEXT("CargoDeparting","Departing");break;
        case EHansaCargoWorldPhase::Traveling: Snapshot.State=LOCTEXT("CargoTraveling","In transit");break;
        case EHansaCargoWorldPhase::Arriving: Snapshot.State=LOCTEXT("CargoArriving","Approaching berth");break;
        case EHansaCargoWorldPhase::Unloading: Snapshot.State=LOCTEXT("CargoUnloading","Unload recorded");break;
        case EHansaCargoWorldPhase::AwaitingPickup: Snapshot.State=LOCTEXT("CargoPickup","Awaiting pickup");break;
        case EHansaCargoWorldPhase::PickupPaused: Snapshot.State=LOCTEXT("CargoPickupPaused","Paused before pickup");break;
        case EHansaCargoWorldPhase::DeliveryPaused: Snapshot.State=LOCTEXT("CargoDeliveryPaused","Paused: road connection lost");break;
        case EHansaCargoWorldPhase::Delivered: Snapshot.State=LOCTEXT("CargoDelivered","Delivered");break;
        case EHansaCargoWorldPhase::Cancelled: Snapshot.State=LOCTEXT("CargoCancelled","Route cancelled");break;
        default: Snapshot.State=LOCTEXT("CargoBerthed","At berth");break;
    }
    Snapshot.PrimaryResult=O.JobId.IsEmpty()
        ? FText::Format(LOCTEXT("CargoQuantity","{0} units aboard"),InspectorPresentationModelQuantity(O.CargoMilliUnits))
        : FText::Format(LOCTEXT("LocalCargoQuantity","{0} {1} aboard · {2} to {3}"),
            InspectorPresentationModelQuantity(O.CargoMilliUnits),GoodLabel,SourceLabel,DestinationLabel);
    FHansaInspectorFlowPresentation Flow;
    Flow.StableId=TEXT("Inspector.Cargo.Progress"); Flow.Label=LOCTEXT("CargoProgress","Journey");
    Flow.Value=InspectorPresentationModelPercent(FMath::RoundToInt(O.Progress*10000));
    Flow.State=O.bVisible?LOCTEXT("CargoVisible","In view"):LOCTEXT("CargoOffscreen","Outside the loaded view"); Snapshot.Flows.Add(Flow);
    if(!O.JobId.IsEmpty())
    {
        FHansaInspectorFlowPresentation Route;
        Route.StableId=TEXT("Inspector.Cargo.Route");
        Route.Label=LOCTEXT("CargoRoute","Road journey");
        Route.Value=FText::Format(LOCTEXT("CargoRouteDistance","{0} road cells"),FText::AsNumber(O.RoadDistanceCells));
        Route.State=FText::Format(LOCTEXT("CargoRouteTicks","{0} elapsed · {1} remaining"),
            FText::AsNumber(O.ElapsedTravelTicks),FText::AsNumber(O.RemainingTravelTicks));
        Snapshot.Flows.Add(Route);
        FHansaInspectorFlowPresentation Endpoints;
        Endpoints.StableId=TEXT("Inspector.Cargo.Endpoints");
        Endpoints.Label=LOCTEXT("CargoEndpoints","Source and destination");
        Endpoints.Value=FText::Format(LOCTEXT("CargoEndpointNames","{0} to {1}"),SourceLabel,DestinationLabel);
        Endpoints.State=FText::Format(LOCTEXT("CargoJobIdentity","Job {0} · request {1}"),
            FText::FromString(O.JobId),FText::FromString(O.RequestId));
        Snapshot.Flows.Add(Endpoints);
    }
    Snapshot.Causal.Problem=Snapshot.State;
    Snapshot.Causal.Cause=!O.PresentationFailure.IsEmpty()?FText::FromString(O.PresentationFailure):
        !O.PauseReason.IsNone()?FText::Format(LOCTEXT("CargoPauseReason","Road access: {0}"),FText::FromName(O.PauseReason)):
        LOCTEXT("CargoTruth","Cargo changes only when pickup, loading or delivery is recorded.");
    Snapshot.Causal.Evidence=FText::Format(LOCTEXT("CargoEvidence","Observed at tick {0}. {1}"),FText::AsNumber(O.SimulationTick),O.JobId.IsEmpty()?LOCTEXT("CargoSeaScale","Port lanes show compressed voyage progress."):LOCTEXT("CargoRoadScale","Wagon position follows the dispatched road connection."));
    Snapshot.Causal.Remedy=!O.PresentationFailure.IsEmpty()?LOCTEXT("CargoMissingRemedy","The simulation continues. Restore the missing world presentation to see this cargo."):
        O.Phase==EHansaCargoWorldPhase::PickupPaused||O.Phase==EHansaCargoWorldPhase::DeliveryPaused
            ? LOCTEXT("CargoRoadRemedy","Reconnect the source, destination and market road network.")
            : O.JobId.IsEmpty()?LOCTEXT("CargoRemedy","Inspect trade routes for orders and cargo history.")
                : LOCTEXT("CargoLocalRemedy","Inspect the source, destination or road connection.");
    Snapshot.Causal.RelatedSemanticId=TEXT("TradeMap.Root");
    Snapshot.Causal.Severity=O.PresentationFailure.IsEmpty()?EHansaCausalSeverity::None:EHansaCausalSeverity::Warning;
    if(O.TransferTick>=0)
    {
        FHansaInspectorHistoryPresentation Receipt; Receipt.StableId=TEXT("Inspector.Cargo.Transfer");
        Receipt.Label=FText::Format(LOCTEXT("CargoReceipt","Last transfer: {0} {1}"),InspectorPresentationModelQuantity(O.TransferMilliUnits),InspectorPresentationModelStableLabel(O.GoodId.ToString()));
        Receipt.Age=FText::Format(LOCTEXT("CargoReceiptTick","Tick {0}"),FText::AsNumber(O.TransferTick)); Snapshot.History.Add(Receipt);
    }
    SetCommonActions(); Snapshot.Actions[0].bEnabled=O.bVisible;
    if(!O.bVisible)Snapshot.Actions[0].DisabledReason=LOCTEXT("CargoFrameUnavailable","The vehicle is outside the loaded city view.");
    PublishIfChanged(Previous);
}
#undef LOCTEXT_NAMESPACE
