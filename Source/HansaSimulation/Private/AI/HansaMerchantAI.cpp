#include "AI/HansaMerchantAI.h"

#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint64 FnvOffset = 14695981039346656037ULL;
		constexpr uint64 FnvPrime = 1099511628211ULL;

		uint64 SeededTieBreak(const uint64 Seed, const int64 DecisionWindow, const FString& StableOptionId)
		{
			uint64 Hash = FnvOffset;
			auto AddByte = [&Hash](const uint8 Value) { Hash ^= Value; Hash *= FnvPrime; };
			for (uint32 Shift = 0; Shift < 64; Shift += 8) AddByte(static_cast<uint8>(Seed >> Shift));
			for (uint32 Shift = 0; Shift < 64; Shift += 8) AddByte(static_cast<uint8>(static_cast<uint64>(DecisionWindow) >> Shift));
			const FTCHARToUTF8 Utf8(*StableOptionId);
			for (int32 Index = 0; Index < Utf8.Length(); ++Index) AddByte(static_cast<uint8>(Utf8.Get()[Index]));
			return Hash;
		}

		int64 AddClamped(const int64 Left, const int64 Right)
		{
			if (Right > 0 && Left > TNumericLimits<int64>::Max() - Right) return TNumericLimits<int64>::Max();
			if (Right < 0 && Left < TNumericLimits<int64>::Lowest() - Right) return TNumericLimits<int64>::Lowest();
			return Left + Right;
		}

		FHansaCommandHeader HeaderFor(const FHansaSimulationReadOnlyAccess& View, const FHansaHouseId HouseId,
			const uint64 PrincipalId, const FHansaCommandId CommandId)
		{
			FHansaCommandHeader Header;
			Header.CommandId = CommandId;
			Header.Authority.IssuingHouseId = HouseId;
			Header.Authority.PrincipalId = PrincipalId;
			Header.Authority.Origin = EHansaCommandOrigin::ArtificialIntelligence;
			Header.RequestedExecutionTick = View.GetClock().GetTick();
			Header.GlobalSequence = View.GetLastProcessedCommandSequence() + 1;
			return Header;
		}

		const FHansaHouseResearchState* FindResearch(const FHansaSimulationReadOnlyAccess& View, const FHansaHouseId HouseId)
		{
			for (const FHansaHouseResearchState& State : View.GetResearch())
			{
				if (State.HouseId == HouseId) return &State;
			}
			return nullptr;
		}
	}

	const TCHAR* LexToString(const EHansaMerchantAIGoal Goal)
	{
		switch (Goal)
		{
		case EHansaMerchantAIGoal::RecoverShortage: return TEXT("RecoverShortage");
		case EHansaMerchantAIGoal::OperateTradeRoute: return TEXT("OperateTradeRoute");
		case EHansaMerchantAIGoal::Research: return TEXT("Research");
		case EHansaMerchantAIGoal::TradeObjectiveComplete: return TEXT("TradeObjectiveComplete");
		default: return TEXT("None");
		}
	}

	const TCHAR* LexToString(const EHansaMerchantAIOptionKind Kind)
	{
		switch (Kind)
		{
		case EHansaMerchantAIOptionKind::ActivateProduction: return TEXT("ActivateProduction");
		case EHansaMerchantAIOptionKind::ActivateRoute: return TEXT("ActivateRoute");
		case EHansaMerchantAIOptionKind::QueueResearch: return TEXT("QueueResearch");
		default: return TEXT("Unknown");
		}
	}

	bool FHansaMerchantAIController::IsDue(const FHansaSimulationReadOnlyAccess& View,
		const FHansaCompiledMerchantAITuning& Tuning) const
	{
		return Tuning.DecisionCadenceTicks > 0 &&
			View.GetClock().GetTick().GetValue() % Tuning.DecisionCadenceTicks == 0;
	}

	FHansaMerchantAIDecision FHansaMerchantAIController::Evaluate(
		const FHansaSimulationReadOnlyAccess& View,
		const FHansaEconomicRegistry& Registry,
		const FHansaCompiledMerchantAITuning& Tuning,
		const FHansaHouseId HouseId,
		const uint64 PrincipalId,
		const FHansaCommandId CommandId)
	{
		FHansaMerchantAIDecision Decision;
		Decision.Trace.DecisionTick = View.GetClock().GetTick().GetValue();
		Decision.Trace.DecisionOrdinal = ++DecisionOrdinal;
		Decision.Trace.TuningStableId = Tuning.StableId;
		const int64 Window = Decision.Trace.DecisionTick / FMath::Max(1, Tuning.DecisionCadenceTicks);

		struct FCandidate
		{
			FHansaMerchantAIConsideredOption Option;
			TOptional<FHansaGameplayCommand> Command;
			EHansaMerchantAIGoal Goal = EHansaMerchantAIGoal::None;
		};
		TArray<FCandidate> Candidates;
		bool bTradeObjectiveComplete = false;

		for (const FHansaCompiledMerchantAITradePlan& Plan : Tuning.TradePlans)
		{
			FHansaCityDefinitionId SourceCity;
			FHansaCityDefinitionId DestinationCity;
			FHansaGoodId GoodId;
			if (!FHansaCityDefinitionId::TryParse(Plan.SourceCityId) ||
				!FHansaCityDefinitionId::TryParse(Plan.DestinationCityId) || !FHansaGoodId::TryParse(Plan.GoodId)) continue;
			SourceCity = FHansaCityDefinitionId::TryParse(Plan.SourceCityId).Value;
			DestinationCity = FHansaCityDefinitionId::TryParse(Plan.DestinationCityId).Value;
			GoodId = FHansaGoodId::TryParse(Plan.GoodId).Value;
			const TOptional<FHansaMarketOpportunityComparisonProjection> Comparison =
				View.CompareMarketOpportunity(SourceCity, DestinationCity, GoodId);

			FCandidate Candidate;
			Candidate.Option.Kind = EHansaMerchantAIOptionKind::ActivateRoute;
			Candidate.Option.StableOptionId = Plan.StablePlanId;
			Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Plan.StablePlanId);
			Candidate.Goal = EHansaMerchantAIGoal::OperateTradeRoute;
			const FHansaRouteState* Route = nullptr;
			for (const FHansaRouteState& Value : View.GetRoutes())
			{
				if (Value.OwnerId == HouseId && Value.RouteDefinitionId.ToString() == Plan.RouteDefinitionId)
				{
					Route = &Value;
					break;
				}
			}
			if (Comparison.IsSet())
			{
				Decision.Trace.KnownFacts.Add({Plan.StablePlanId + TEXT(".Margin"), Plan.GoodId,
					Comparison->DestinationInformationState, Comparison->GrossMarginMilliMarks.Get(0), TEXT("MilliMark")});
				Decision.Trace.KnownFacts.Add({Plan.StablePlanId + TEXT(".DemandGap"), Plan.GoodId,
					Comparison->DestinationInformationState, Comparison->DestinationDemandGap.IsSet()
						? Comparison->DestinationDemandGap->GetRawValue() : 0, TEXT("MilliUnit")});
			}
			if (Route == nullptr)
			{
				Candidate.Option.Reason = TEXT("No owned route matches the authored opportunity.");
			}
			else if (Route->CompletedLegCount >= Tuning.TargetCompletedTradeLegs)
			{
				bTradeObjectiveComplete = true;
				Candidate.Option.Reason = TEXT("The authored trade objective is already complete.");
			}
			else if (Route->Lifecycle != EHansaRouteLifecycleState::Inactive)
			{
				Candidate.Option.Reason = TEXT("The route is already operating.");
			}
			else if (!Comparison.IsSet() || !Comparison->bComparable || !Comparison->SourceAvailableAboveReserve.IsSet() ||
				!Comparison->DestinationDemandGap.IsSet() || !Comparison->GrossMarginMilliMarks.IsSet())
			{
				Candidate.Option.Reason = TEXT("Allowed market reports are unknown or too stale to compare.");
			}
			else
			{
				const int64 SourceAvailable = Comparison->SourceAvailableAboveReserve->GetRawValue();
				const int64 DemandGap = Comparison->DestinationDemandGap->GetRawValue();
				const int64 Margin = *Comparison->GrossMarginMilliMarks;
				const bool bShortage = DemandGap >= Tuning.MinimumDestinationDemandGapMilliUnits;
				const bool bOpportunity = Margin >= Tuning.MinimumGrossMarginMilliMarks;
				Candidate.Option.bEligible = SourceAvailable > 0 && (bShortage || bOpportunity);
				Candidate.Option.Utility = AddClamped(Plan.UtilityBias,
					AddClamped(FMath::Max<int64>(0, DemandGap / 1000) * Tuning.ShortageUtilityPerUnit,
						FMath::Max<int64>(0, Margin) * Tuning.MarginUtilityPerMilliMark));
				Candidate.Option.Reason = Candidate.Option.bEligible
					? (bShortage ? TEXT("Known destination shortage justifies the route.") : TEXT("Known price margin justifies the route."))
					: TEXT("Known supply, shortage and margin do not clear authored thresholds.");
				if (Candidate.Option.bEligible)
				{
					Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
						FHansaSetRouteActiveCommand{Route->Id, true});
					Candidate.Goal = bShortage ? EHansaMerchantAIGoal::RecoverShortage : EHansaMerchantAIGoal::OperateTradeRoute;
				}
			}
			Candidates.Add(MoveTemp(Candidate));
		}

		const FHansaHouseResearchState* Research = FindResearch(View, HouseId);
		for (int32 Priority = 0; Priority < Tuning.PreferredResearchTechnologyIds.Num(); ++Priority)
		{
			const FString& TechnologyId = Tuning.PreferredResearchTechnologyIds[Priority];
			FCandidate Candidate;
			Candidate.Option.Kind = EHansaMerchantAIOptionKind::QueueResearch;
			Candidate.Option.StableOptionId = TEXT("Research.") + TechnologyId;
			Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
			Candidate.Option.Utility = Tuning.ResearchUtility - Priority;
			Candidate.Goal = EHansaMerchantAIGoal::Research;
			const FHansaCompiledTechnologyDefinition* Technology = Registry.FindTechnology(TechnologyId);
			if (Research == nullptr || Technology == nullptr)
			{
				Candidate.Option.Reason = TEXT("Research state or authored technology is unavailable.");
			}
			else if (!Research->ActiveTechnologyId.IsEmpty())
			{
				Candidate.Option.Reason = TEXT("The one-item research queue is occupied.");
			}
			else if (Research->IsCompleted(TechnologyId))
			{
				Candidate.Option.Reason = TEXT("Technology is already complete.");
			}
			else if (Research->AvailableResearchPoints < Technology->CostResearchPoints)
			{
				Candidate.Option.Reason = TEXT("Research point budget is below the authored cost.");
			}
			else
			{
				const FString* Missing = Technology->PrerequisiteTechnologyIds.FindByPredicate([Research](const FString& Id)
				{
					return !Research->IsCompleted(Id);
				});
				Candidate.Option.bEligible = Missing == nullptr;
				Candidate.Option.Reason = Missing == nullptr ? TEXT("Research budget and prerequisites are satisfied.")
					: TEXT("An authored research prerequisite is incomplete.");
				if (Candidate.Option.bEligible)
				{
					Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
						FHansaQueueResearchCommand{TechnologyId});
				}
			}
			Candidates.Add(MoveTemp(Candidate));
		}

		for (const FHansaProductionProjection& Production : View.BuildProductionProjection())
		{
			const FHansaBuildingState* Building = nullptr;
			for (const FHansaBuildingState& Value : View.GetBuildings())
			{
				if (Value.Id == Production.BuildingId)
				{
					Building = &Value;
					break;
				}
			}
			if (Building == nullptr || Building->OwnerId != HouseId || Production.bActive) continue;
			FCandidate Candidate;
			Candidate.Option.Kind = EHansaMerchantAIOptionKind::ActivateProduction;
			Candidate.Option.StableOptionId = FString::Printf(TEXT("Production.%llu"),
				static_cast<unsigned long long>(Production.Id.GetValue()));
			Candidate.Option.Utility = Tuning.ProductionUtility;
			Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
			Candidate.Option.bEligible = true;
			Candidate.Option.Reason = TEXT("Owned production is idle and may strengthen the merchant economy.");
			Candidate.Goal = EHansaMerchantAIGoal::RecoverShortage;
			Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
				FHansaSetProductionActiveCommand{Production.Id, true});
			Candidates.Add(MoveTemp(Candidate));
		}

		Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
		{
			if (Left.Option.bEligible != Right.Option.bEligible) return Left.Option.bEligible;
			if (Left.Option.Utility != Right.Option.Utility) return Left.Option.Utility > Right.Option.Utility;
			if (Left.Option.SeededTieBreak != Right.Option.SeededTieBreak) return Left.Option.SeededTieBreak < Right.Option.SeededTieBreak;
			return Left.Option.StableOptionId < Right.Option.StableOptionId;
		});
		for (const FCandidate& Candidate : Candidates) Decision.Trace.ConsideredOptions.Add(Candidate.Option);
		const FCandidate* Chosen = Candidates.FindByPredicate([](const FCandidate& Candidate) { return Candidate.Option.bEligible; });
		if (Chosen != nullptr)
		{
			Decision.Command = Chosen->Command;
			Decision.Trace.SelectedGoal = Chosen->Goal;
			Decision.Trace.ChosenOptionId = Chosen->Option.StableOptionId;
			Decision.Trace.Reason = Chosen->Option.Reason;
			if (Decision.Command.IsSet()) Decision.Trace.SubmittedCommandType = Decision.Command->GetType();
		}
		else
		{
			Decision.Trace.SelectedGoal = bTradeObjectiveComplete ? EHansaMerchantAIGoal::TradeObjectiveComplete : EHansaMerchantAIGoal::None;
			Decision.Trace.Reason = bTradeObjectiveComplete
				? TEXT("The configured trade-leg objective has been completed through normal route transfers.")
				: TEXT("No bounded option is eligible from currently known facts.");
		}

		History.Add(Decision.Trace);
		if (History.Num() > FMath::Max(1, Tuning.DecisionHistoryCapacity)) History.RemoveAt(0, History.Num() - Tuning.DecisionHistoryCapacity);
		return Decision;
	}

	void FHansaMerchantAIController::RecordGatewayOutcome(const FHansaCommandGatewayResult& Result)
	{
		if (History.IsEmpty()) return;
		History.Last().bCommandAccepted = Result.IsSuccess();
		History.Last().GatewayError = Result.GetError();
	}

	const FHansaMerchantAIDecisionTrace* FHansaMerchantAIController::GetLastDecision() const
	{
		return History.IsEmpty() ? nullptr : &History.Last();
	}
}
