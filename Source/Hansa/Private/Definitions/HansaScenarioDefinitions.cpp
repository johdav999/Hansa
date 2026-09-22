#include "Definitions/HansaScenarioDefinitions.h"

namespace
{
	TArray<FHansaAuthoredScenarioSlotRule> MakeEightHouseSlots()
	{
		const TArray<EHansaAuthoredSessionSlotState> AllStates = {
			EHansaAuthoredSessionSlotState::Human, EHansaAuthoredSessionSlotState::AI,
			EHansaAuthoredSessionSlotState::Open, EHansaAuthoredSessionSlotState::Closed,
			EHansaAuthoredSessionSlotState::Reserved};
		TArray<FHansaAuthoredScenarioSlotRule> Slots;
		Slots.Add({TEXT("HouseSlot.Primary"), 1, EHansaAuthoredSessionSlotState::Human, AllStates, 0, false, true});
		for (int32 Index = 2; Index <= 8; ++Index)
		{
			Slots.Add({FString::Printf(TEXT("HouseSlot.Rival%02d"), Index), Index,
				EHansaAuthoredSessionSlotState::AI, AllStates, 0, false, true});
		}
		return Slots;
	}

	void AddIssue(const UHansaDefinitionBase& Definition, TArray<FHansaDefinitionValidationIssue>& OutIssues,
		const FName Code, const TCHAR* Property, const TCHAR* Cause, const TCHAR* Remedy)
	{
		OutIssues.Add({EHansaDefinitionValidationSeverity::Error, Code,
			Definition.StableDefinitionId + TEXT(".") + Property, FText::FromString(Cause), FText::FromString(Remedy)});
	}
}

UHansaScenarioObjectiveDefinition::UHansaScenarioObjectiveDefinition()
{
	DefinitionCategory = TEXT("ScenarioObjective");
}

void UHansaScenarioObjectiveDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (TargetValue <= 0)
	{
		AddIssue(*this, OutIssues, TEXT("HSA-OBJECTIVE-001"), TEXT("TargetValue"),
			TEXT("An objective threshold must be positive."), TEXT("Set a reachable positive target in the metric's declared unit."));
	}
	if (Metric == EHansaAuthoredObjectiveMetric::CitySatisfactionAtLeast && TargetValue > 10'000)
	{
		AddIssue(*this, OutIssues, TEXT("HSA-OBJECTIVE-002"), TEXT("TargetValue"),
			TEXT("City satisfaction cannot exceed 10,000 basis points."), TEXT("Choose a target between 1 and 10,000 basis points."));
	}
	const bool bNeedsCity = Metric == EHansaAuthoredObjectiveMetric::CityPopulationAtLeast ||
		Metric == EHansaAuthoredObjectiveMetric::ArtisanPopulationAtLeast ||
		Metric == EHansaAuthoredObjectiveMetric::CitySatisfactionAtLeast ||
		Metric == EHansaAuthoredObjectiveMetric::MarketStockAtLeast ||
		Metric == EHansaAuthoredObjectiveMetric::MarketPriceAtMost;
	const bool bNeedsGood = Metric == EHansaAuthoredObjectiveMetric::MarketStockAtLeast ||
		Metric == EHansaAuthoredObjectiveMetric::MarketPriceAtMost;
	if (bNeedsCity != !CityId.IsEmpty())
	{
		AddIssue(*this, OutIssues, TEXT("HSA-OBJECTIVE-003"), TEXT("CityId"),
			TEXT("The metric's city reference is missing or unexpectedly populated."), TEXT("Assign City.* only for city, civic or market metrics."));
	}
	if (bNeedsGood != !GoodId.IsEmpty())
	{
		AddIssue(*this, OutIssues, TEXT("HSA-OBJECTIVE-004"), TEXT("GoodId"),
			TEXT("The metric's good reference is missing or unexpectedly populated."), TEXT("Assign Good.* only for market stock or price metrics."));
	}
	if (ProgressUnit.IsEmpty())
	{
		AddIssue(*this, OutIssues, TEXT("HSA-OBJECTIVE-005"), TEXT("ProgressUnit"),
			TEXT("The objective has no visible progress unit."), TEXT("Provide a short localized unit such as pfennig, residents, routes or milli-units."));
	}
}

void UHansaScenarioObjectiveDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("|metric=%d|target=%lld|city=%s|good=%s|unit=%s"),
		static_cast<int32>(Metric), static_cast<long long>(TargetValue), *CityId, *GoodId, *ProgressUnit.ToString());
}

UHansaVictoryDefinition::UHansaVictoryDefinition()
{
	DefinitionCategory = TEXT("Victory");
}

void UHansaVictoryDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (ObjectiveIds.IsEmpty() || SustainTicks <= 0 || EndingPriority < 0 || Summary.IsEmpty())
	{
		AddIssue(*this, OutIssues, TEXT("HSA-VICTORY-001"), TEXT("ObjectiveIds"),
			TEXT("Victory paths need objectives, a positive sustain duration, non-negative priority and visible summary."),
			TEXT("Complete every required field with bounded values."));
	}
	TSet<FString> Seen;
	for (const FString& ObjectiveId : ObjectiveIds)
	{
		if (ObjectiveId.IsEmpty() || Seen.Contains(ObjectiveId))
		{
			AddIssue(*this, OutIssues, TEXT("HSA-VICTORY-002"), TEXT("ObjectiveIds"),
				TEXT("Victory objective references contain an empty or duplicate entry."), TEXT("Use unique ScenarioObjective.* IDs."));
		}
		Seen.Add(ObjectiveId);
	}
}

void UHansaVictoryDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	TArray<FString> Sorted = ObjectiveIds;
	Sorted.Sort();
	InOutCanonicalData += FString::Printf(TEXT("|sustain=%d|priority=%d|summary=%s"), SustainTicks, EndingPriority, *Summary.ToString());
	for (const FString& Id : Sorted) InOutCanonicalData += TEXT("|objective=") + Id;
}

UHansaScenarioDefinition::UHansaScenarioDefinition()
{
	DefinitionCategory = TEXT("Scenario");
	SchemaVersion = 3;
	MultiplayerSlots = MakeEightHouseSlots();
}

void UHansaScenarioDefinition::PostLoad()
{
	Super::PostLoad();
	if (SchemaVersion < 3)
	{
		SchemaVersion = 3;
		// Schema 3 expands the original two-house fixture while preserving houses 1 and 2.
		MultiplayerSlots = MakeEightHouseSlots();
		RefreshContentHash();
	}
}

void UHansaScenarioDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (HomeCityId.IsEmpty() || VictoryIds.Num() < 2 || FailureSustainTicks <= 0 || InsolvencyThresholdPfennig > 0 || Briefing.IsEmpty())
	{
		AddIssue(*this, OutIssues, TEXT("HSA-SCENARIO-001"), TEXT("VictoryIds"),
			TEXT("A scenario needs a home city, several victory paths, a non-positive insolvency threshold, positive failure duration and briefing."),
			TEXT("Complete the bounded scenario start and ending policy."));
	}
	TSet<FString> Seen;
	for (const FString& VictoryId : VictoryIds)
	{
		if (VictoryId.IsEmpty() || Seen.Contains(VictoryId))
		{
			AddIssue(*this, OutIssues, TEXT("HSA-SCENARIO-002"), TEXT("VictoryIds"),
				TEXT("Scenario endings contain an empty or duplicate victory reference."), TEXT("Use unique Victory.* IDs."));
		}
		Seen.Add(VictoryId);
	}
	if (MultiplayerSlots.Num() < 2 || MultiplayerSlots.Num() > 8)
	{
		AddIssue(*this, OutIssues, TEXT("HSA-SCENARIO-003"), TEXT("MultiplayerSlots"),
			TEXT("A multiplayer scenario requires two through eight authored slots."), TEXT("Author a bounded slot for every supported house."));
	}
	TSet<FString> SlotIds; TSet<int64> HouseIds;
	for (const FHansaAuthoredScenarioSlotRule& Slot : MultiplayerSlots)
	{
		TSet<EHansaAuthoredSessionSlotState> Allowed;
		for (const EHansaAuthoredSessionSlotState State : Slot.AllowedStates) Allowed.Add(State);
		if (Slot.SlotId.IsEmpty() || SlotIds.Contains(Slot.SlotId) || Slot.HouseId <= 0 || HouseIds.Contains(Slot.HouseId) ||
			Allowed.Num() != Slot.AllowedStates.Num() || !Allowed.Contains(Slot.DefaultState) ||
			Slot.AuthoredTeamId < 0 || (Slot.bTeamRequired && Slot.AuthoredTeamId == 0))
		{
			AddIssue(*this, OutIssues, TEXT("HSA-SCENARIO-004"), TEXT("MultiplayerSlots"),
				TEXT("A scenario slot has a duplicate/invalid identity, state set, default, or team constraint."),
				TEXT("Use unique positive house/slot IDs, unique allowed states containing the default, and a team ID when required."));
		}
		SlotIds.Add(Slot.SlotId); HouseIds.Add(Slot.HouseId);
	}
}

void UHansaScenarioDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("|home=%s|insolvency=%lld|failure=%d|briefing=%s"),
		*HomeCityId, static_cast<long long>(InsolvencyThresholdPfennig), FailureSustainTicks, *Briefing.ToString());
	for (const FString& Id : VictoryIds) InOutCanonicalData += TEXT("|victory=") + Id;
	TArray<FHansaAuthoredScenarioSlotRule> SortedSlots = MultiplayerSlots;
	SortedSlots.Sort([](const auto& Left, const auto& Right) { return Left.SlotId < Right.SlotId; });
	for (const FHansaAuthoredScenarioSlotRule& Slot : SortedSlots)
	{
		InOutCanonicalData += FString::Printf(TEXT("|slot=%s:%lld:%d:%lld:%d:%d"), *Slot.SlotId,
			static_cast<long long>(Slot.HouseId), static_cast<int32>(Slot.DefaultState),
			static_cast<long long>(Slot.AuthoredTeamId), Slot.bTeamRequired ? 1 : 0, Slot.bAllowHumanTakeover ? 1 : 0);
		TArray<EHansaAuthoredSessionSlotState> States = Slot.AllowedStates;
		States.Sort([](const auto Left, const auto Right) { return static_cast<uint8>(Left) < static_cast<uint8>(Right); });
		for (const auto State : States) InOutCanonicalData += FString::Printf(TEXT(":%d"), static_cast<int32>(State));
	}
}
