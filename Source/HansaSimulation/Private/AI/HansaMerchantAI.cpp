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

		const FHansaHouseState* FindHouse(const FHansaSimulationReadOnlyAccess& View, const FHansaHouseId HouseId)
		{
			return View.GetHouses().FindByPredicate([HouseId](const FHansaHouseState& Value) { return Value.Id == HouseId; });
		}

		bool OwnsInventory(const FHansaSimulationReadOnlyAccess& View, const FHansaHouseId HouseId,
			const FHansaInventoryProjection& Inventory)
		{
			if (Inventory.OwnerKind == EHansaInventoryOwnerKind::Vehicle)
				return View.GetVehicles().ContainsByPredicate([&](const FHansaVehicleState& Value)
					{ return Value.Id == Inventory.VehicleId && Value.OwnerId == HouseId; });
			if (Inventory.OwnerKind == EHansaInventoryOwnerKind::Building || Inventory.OwnerKind == EHansaInventoryOwnerKind::Warehouse)
				return View.GetBuildings().ContainsByPredicate([&](const FHansaBuildingState& Value)
					{ return Value.Id == Inventory.BuildingId && Value.OwnerId == HouseId; });
			if (Inventory.OwnerKind == EHansaInventoryOwnerKind::TradeStation)
				return View.GetTradeStations().ContainsByPredicate([&](const FHansaTradeStationState& Value)
					{ return Value.Id == Inventory.TradeStationId && Value.OwnerId == HouseId; });
			return false;
		}

		TOptional<FHansaInventoryId> FindFundingInventory(const FHansaSimulationReadOnlyAccess& View,
			const FHansaHouseId HouseId, TConstArrayView<FHansaCompiledPresenceUpgradeGoodCost> Costs)
		{
			TArray<FHansaInventoryProjection> Inventories = View.GetInventories().BuildProjection();
			Inventories.Sort([](const auto& Left, const auto& Right) { return Left.Id < Right.Id; });
			for (const FHansaInventoryProjection& Inventory : Inventories)
			{
				if (!OwnsInventory(View, HouseId, Inventory) || Inventory.OwnerKind == EHansaInventoryOwnerKind::TradeStation) continue;
				bool bHasCosts = true;
				for (const FHansaCompiledPresenceUpgradeGoodCost& Cost : Costs)
				{
					const auto Good = FHansaGoodId::TryParse(Cost.GoodId);
					const auto Stock = Good ? View.GetInventories().QueryStock(Inventory.Id, Good.Value) : TOptional<FHansaInventoryStockProjection>();
					bHasCosts &= Stock.IsSet() && Stock->Available.GetRawValue() >= Cost.QuantityMilliUnits;
				}
				if (bHasCosts) return Inventory.Id;
			}
			return {};
		}
	}

	const TCHAR* LexToString(const EHansaMerchantAIGoal Goal)
	{
		switch (Goal)
		{
		case EHansaMerchantAIGoal::RecoverShortage: return TEXT("RecoverShortage");
		case EHansaMerchantAIGoal::OperateTradeRoute: return TEXT("OperateTradeRoute");
		case EHansaMerchantAIGoal::DirectTrade: return TEXT("DirectTrade");
		case EHansaMerchantAIGoal::EstablishTradeStation: return TEXT("EstablishTradeStation");
		case EHansaMerchantAIGoal::OperateTradeStation: return TEXT("OperateTradeStation");
		case EHansaMerchantAIGoal::AdvancePresence: return TEXT("AdvancePresence");
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
		case EHansaMerchantAIOptionKind::SpotTrade: return TEXT("SpotTrade");
		case EHansaMerchantAIOptionKind::ProposeTradeStation: return TEXT("ProposeTradeStation");
		case EHansaMerchantAIOptionKind::FundTradeStation: return TEXT("FundTradeStation");
		case EHansaMerchantAIOptionKind::CreateStationOrder: return TEXT("CreateStationOrder");
		case EHansaMerchantAIOptionKind::RequestPresenceUpgrade: return TEXT("RequestPresenceUpgrade");
		case EHansaMerchantAIOptionKind::FundPresenceUpgrade: return TEXT("FundPresenceUpgrade");
		case EHansaMerchantAIOptionKind::ApplyPresenceSpecialization: return TEXT("ApplyPresenceSpecialization");
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
				View.CompareMarketOpportunity(SourceCity, DestinationCity, GoodId, HouseId);

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

			for (const FHansaVehicleState& Vehicle : View.GetVehicles())
			{
				if (Vehicle.OwnerId != HouseId || Vehicle.Mode != EHansaRouteMode::Sea) continue;
				const bool bAtSource = Vehicle.CurrentCityId == SourceCity;
				const bool bAtDestination = Vehicle.CurrentCityId == DestinationCity;
				if (!bAtSource && !bAtDestination) continue;
				const EHansaSpotTradeSide Side = bAtSource ? EHansaSpotTradeSide::BuyFromCity : EHansaSpotTradeSide::SellToCity;
				const int64 Quantity = FMath::Max<int64>(1, FMath::Min(Plan.QuantityLimitMilliUnits, Tuning.DirectTradeQuantityMilliUnits));
				const FHansaSpotTradeQuoteProjection Quote = View.QuerySpotTradeQuote(HouseId, Vehicle.Id,
					Vehicle.CurrentCityId, GoodId, Side, FHansaQuantity::FromRaw(Quantity));
				FCandidate Trade;
				Trade.Option.Kind = EHansaMerchantAIOptionKind::SpotTrade;
				Trade.Option.StableOptionId = FString::Printf(TEXT("%s.Spot.%llu.%s"), *Plan.StablePlanId,
					static_cast<unsigned long long>(Vehicle.Id.GetValue()), bAtSource ? TEXT("Buy") : TEXT("Sell"));
				Trade.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Trade.Option.StableOptionId);
				Trade.Option.Utility = AddClamped(Plan.UtilityBias, Tuning.PresenceUtility / 2);
				Trade.Goal = EHansaMerchantAIGoal::DirectTrade;
				const FHansaHouseState* House = FindHouse(View, HouseId);
				const bool bReserveSafe = House && (Quote.EstimatedSettlementMoneyRaw >= 0 ||
					House->Money.GetRawValue() + Quote.EstimatedSettlementMoneyRaw >= Tuning.ProtectedCashReservePfennig);
				Trade.Option.bEligible = Quote.bCanSubmit && Quote.EstimatedQuantity.GetRawValue() > 0 && bReserveSafe &&
					(bAtDestination || (Comparison.IsSet() && Comparison->bComparable && Comparison->GrossMarginMilliMarks.Get(0) >= Tuning.MinimumGrossMarginMilliMarks));
				Trade.Option.Reason = Trade.Option.bEligible ? TEXT("Lawful berth, known report, cargo/funds and protected reserve permit direct trade.") :
					(!bReserveSafe ? TEXT("Direct trade would breach the authored protected cash reserve.") : Quote.Cause);
				if (Trade.Option.bEligible)
				{
					Trade.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
						FHansaSpotTradeCommand{Vehicle.Id, Vehicle.CurrentCityId, GoodId, Side, Quote.EstimatedQuantity,
							Quote.ReviewedMarketUpdateTick, Quote.ReviewedUnitPriceMilliMarks});
				}
				Candidates.Add(MoveTemp(Trade));
			}
		}

		const FHansaHouseState* House = FindHouse(View, HouseId);
		for (const FHansaForeignPresenceProjection& Presence : View.BuildForeignPresenceProjection(HouseId))
		{
			const FHansaCompiledCityTradePolicyDefinition* Policy = Registry.FindCityTradePolicyForCity(Presence.CityId.ToString());
			const FHansaCompiledForeignPresenceStageDefinition* CurrentStage = Registry.FindPresenceStage(Presence.CurrentStageId);
			if (!Policy || !CurrentStage) continue;
			const FHansaCompiledForeignPresenceStageDefinition* StationStage = Registry.GetPresenceStages().FindByPredicate([&](const auto& Value)
				{ return Value.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation")) && Registry.IsValidPresenceTransition(Policy->CityId, Presence.CurrentStageId, Value.StableId); });

			for (const auto& Site : Policy->TradeStationSites)
			{
				FCandidate Candidate;
				Candidate.Option.Kind = EHansaMerchantAIOptionKind::ProposeTradeStation;
				Candidate.Option.StableOptionId = FString::Printf(TEXT("Presence.%s.Propose.%s"), *Policy->CityId, *Site.SiteId);
				Candidate.Option.Utility = Tuning.PresenceUtility;
				Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
				Candidate.Goal = EHansaMerchantAIGoal::EstablishTradeStation;
				const bool bOccupied = View.GetLeasedPlots().ContainsByPredicate([&](const auto& Value)
					{ return Value.CityId == Presence.CityId && Value.SiteId == Site.SiteId && Value.bOccupied; });
				const auto Funding = StationStage ? FindFundingInventory(View, HouseId, StationStage->UpgradeGoods) : TOptional<FHansaInventoryId>();
				const bool bProgress = StationStage && Presence.Contributions.LawfulTradeVolumeMilliUnits >= StationStage->RequiredLawfulTradeVolumeMilliUnits &&
					Presence.Contributions.CompletedDeliveryCount >= StationStage->RequiredCompletedDeliveries && Presence.Contributions.InvestedPfennig >= StationStage->RequiredInvestedPfennig;
				const bool bBudget = House && StationStage && House->Money.GetRawValue() - StationStage->UpgradeCostPfennig >= Tuning.ProtectedCashReservePfennig;
				Candidate.Option.bEligible = Presence.Status == EHansaForeignPresenceStatus::Active && !Presence.StationId.IsValid() &&
					!bOccupied && bProgress && bBudget && Funding.IsSet();
				Candidate.Option.Reason = Candidate.Option.bEligible ? TEXT("Authored site, progression, materials and protected budget permit a normal station proposal.") :
					(bOccupied ? TEXT("The authored station site is already occupied.") : !bProgress ? TEXT("Station progression requirements are not met.") :
					!bBudget ? TEXT("Station investment would breach the protected cash reserve.") : TEXT("Owned cargo lacks the station construction materials."));
				if (Candidate.Option.bEligible)
				{
					uint64 StationValue = 1, FactorValue = 1, LeaseValue = 1, InventoryValue = 1;
					for (const auto& Value : View.GetTradeStations()) { StationValue = FMath::Max(StationValue, Value.Id.GetValue() + 1); FactorValue = FMath::Max(FactorValue, Value.FactorId.GetValue() + 1); }
					for (const auto& Value : View.GetLeasedPlots()) LeaseValue = FMath::Max(LeaseValue, Value.Id.GetValue() + 1);
					for (const auto& Value : View.GetInventories().BuildProjection()) InventoryValue = FMath::Max(InventoryValue, Value.Id.GetValue() + 1);
					Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
						FHansaProposeTradeStationCommand{FHansaTradeStationId::TryCreate(StationValue).Value, FHansaFactorId::TryCreate(FactorValue).Value,
							FHansaLeasedPlotId::TryCreate(LeaseValue).Value, FHansaInventoryId::TryCreate(InventoryValue).Value, Presence.CityId, Site.SiteId});
				}
				Candidates.Add(MoveTemp(Candidate));
			}

			const FHansaTradeStationState* Station = View.GetTradeStations().FindByPredicate([&](const auto& Value) { return Value.Id == Presence.StationId; });
			if (Station && Station->Status == EHansaTradeStationStatus::Proposed && StationStage)
			{
				FCandidate Candidate;
				Candidate.Option.Kind = EHansaMerchantAIOptionKind::FundTradeStation;
				Candidate.Option.StableOptionId = FString::Printf(TEXT("Presence.%s.FundStation"), *Policy->CityId);
				Candidate.Option.Utility = Tuning.PresenceUtility + 1;
				Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
				Candidate.Goal = EHansaMerchantAIGoal::EstablishTradeStation;
				const auto Funding = FindFundingInventory(View, HouseId, StationStage->UpgradeGoods);
				Candidate.Option.bEligible = Funding.IsSet() && House && House->Money.GetRawValue() - StationStage->UpgradeCostPfennig >= Tuning.ProtectedCashReservePfennig;
				Candidate.Option.Reason = Candidate.Option.bEligible ? TEXT("Owned funding cargo and protected budget permit normal station funding.") : TEXT("Station funding lacks owned materials or protected cash.");
				if (Candidate.Option.bEligible) Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId), FHansaFundTradeStationCommand{Station->Id, Funding.GetValue()});
				Candidates.Add(MoveTemp(Candidate));
			}

			if (Station && Station->Status == EHansaTradeStationStatus::Active)
			{
				const TOptional<FHansaInventoryProjection> StationStorage = View.GetInventories().QueryInventory(Station->InventoryId);
				for (const FHansaCompiledMerchantAITradePlan& Plan : Tuning.TradePlans)
				{
					if (Plan.SourceCityId != Policy->CityId && Plan.DestinationCityId != Policy->CityId) continue;
					const auto Good = FHansaGoodId::TryParse(Plan.GoodId); if (!Good) continue;
					FCandidate Candidate;
					Candidate.Option.Kind = EHansaMerchantAIOptionKind::CreateStationOrder;
					Candidate.Option.StableOptionId = FString::Printf(TEXT("Presence.%s.Order.%s"), *Policy->CityId, *Plan.GoodId);
					Candidate.Option.Utility = Tuning.PresenceUtility - 1;
					Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
					Candidate.Goal = EHansaMerchantAIGoal::OperateTradeStation;
					const bool bExisting = Station->Orders.ContainsByPredicate([&](const auto& Value) { return !Value.bCancelled && Value.Terms.GoodId == Good.Value; });
					const auto Known = View.QueryKnownMarketSupplyDemand(Presence.CityId, Good.Value, HouseId);
					Candidate.Option.bEligible = StationStorage.IsSet() && !bExisting && Known.IsSet() && Known->InformationState != EHansaMarketInformationState::Unknown &&
						House && House->Money.GetRawValue() - Tuning.StationOrderBudgetPfennig >= Tuning.ProtectedCashReservePfennig;
					Candidate.Option.Reason = Candidate.Option.bEligible ? TEXT("Known public report, station capacity and protected order budget permit an unattended order.") :
						(bExisting ? TEXT("A live station order already covers this good.") : TEXT("Order report or protected budget is unavailable."));
					if (Candidate.Option.bEligible)
					{
						uint64 OrderId = 1; for (const auto& Value : Station->Orders) OrderId = FMath::Max(OrderId, Value.Id + 1);
						FHansaStationOrderTerms Terms; Terms.GoodId = Good.Value; Terms.Side = EHansaStationOrderSide::Acquire;
						Terms.TargetOrReserveMilliUnits = FMath::Min(Tuning.StationOrderTargetMilliUnits, StationStorage->Capacity.GetRawValue());
						Terms.CapMilliUnits = FMath::Min(Tuning.StationOrderCapMilliUnits, Policy->MaximumOrderCapMilliUnits);
						Terms.TotalBudgetPfennig = FMath::Min(Tuning.StationOrderBudgetPfennig, Policy->MaximumOrderBudgetPfennig);
						Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
							FHansaManageStationOrderCommand{Station->Id, OrderId, EHansaStationOrderAction::Create, Terms});
					}
					Candidates.Add(MoveTemp(Candidate));
				}
			}

			for (const FHansaPresenceStageOptionProjection& Option : Presence.NextStages)
			{
				FCandidate Candidate;
				const bool bFunding = Presence.Upgrade.Status == EHansaPresenceUpgradeStatus::Requested && Presence.Upgrade.TargetStageId == Option.StageId;
				Candidate.Option.Kind = bFunding ? EHansaMerchantAIOptionKind::FundPresenceUpgrade : EHansaMerchantAIOptionKind::RequestPresenceUpgrade;
				Candidate.Option.StableOptionId = FString::Printf(TEXT("Presence.%s.%s.%s"), *Policy->CityId, bFunding ? TEXT("Fund") : TEXT("Request"), *Option.StageId);
				Candidate.Option.Utility = Tuning.PresenceUtility - 2 + Option.Ordinal;
				Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
				Candidate.Goal = EHansaMerchantAIGoal::AdvancePresence;
				const bool bBudgetSafe = House && House->Money.GetRawValue() - Option.UpgradeCostPfennig >= Tuning.ProtectedCashReservePfennig;
				Candidate.Option.bEligible = bBudgetSafe && (bFunding ? Option.bFundingAvailable : (Presence.Upgrade.Status == EHansaPresenceUpgradeStatus::None && Option.bProgressRequirementsMet && Option.bFundingAvailable));
				Candidate.Option.Reason = Candidate.Option.bEligible ? TEXT("Owned progression, station materials and budget satisfy the authored next stage.") : TEXT("Progression requirements, materials or protected budget are not yet satisfied.");
				if (Candidate.Option.bEligible)
				{
					Candidate.Command = bFunding ? FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId), FHansaFundPresenceUpgradeCommand{Presence.CityId, Option.StageId, Station ? Station->InventoryId : FHansaInventoryId()}) :
						FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId), FHansaRequestPresenceUpgradeCommand{Presence.CityId, Option.StageId});
				}
				Candidates.Add(MoveTemp(Candidate));
				break;
			}

			for (const FHansaPresenceSpecializationProjection& Branch : Presence.Specializations)
			{
				FCandidate Candidate;
				Candidate.Option.Kind = EHansaMerchantAIOptionKind::ApplyPresenceSpecialization;
				Candidate.Option.StableOptionId = FString::Printf(TEXT("Presence.%s.Branch.%s"), *Policy->CityId, *Branch.SpecializationId);
				Candidate.Option.Utility = Tuning.PresenceUtility - 3;
				Candidate.Option.SeededTieBreak = SeededTieBreak(View.GetCampaignSeed(), Window, Candidate.Option.StableOptionId);
				Candidate.Goal = EHansaMerchantAIGoal::AdvancePresence;
				Candidate.Option.bEligible = Branch.bAvailable && Station != nullptr && House &&
					House->Money.GetRawValue() - Branch.InvestmentCostPfennig >= Tuning.ProtectedCashReservePfennig;
				Candidate.Option.Reason = Candidate.Option.bEligible ? TEXT("The authored branch is affordable, compatible and available at the current stage.") : Branch.Blocker;
				if (Candidate.Option.bEligible) Candidate.Command = FHansaGameplayCommand::Create(HeaderFor(View, HouseId, PrincipalId, CommandId),
					FHansaApplyPresenceSpecializationCommand{Presence.CityId, Branch.SpecializationId, Station->InventoryId, EHansaPresenceSpecializationAction::Select, Presence.SpecializationRevision});
				Candidates.Add(MoveTemp(Candidate));
			}
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
		const bool bCoolingDown = LastAcceptedActionTick != MIN_int64 &&
			Decision.Trace.DecisionTick - LastAcceptedActionTick < Tuning.ActionCooldownTicks;
		if (bCoolingDown)
		{
			for (FCandidate& Candidate : Candidates)
			{
				Candidate.Option.bEligible = false;
				Candidate.Option.Reason = TEXT("The authored action cooldown is still active.");
				Candidate.Command.Reset();
			}
		}
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
		if (Result.IsSuccess() && History.Last().SubmittedCommandType.IsSet()) LastAcceptedActionTick = History.Last().DecisionTick;
	}

	const FHansaMerchantAIDecisionTrace* FHansaMerchantAIController::GetLastDecision() const
	{
		return History.IsEmpty() ? nullptr : &History.Last();
	}

	TArray<FHansaMerchantAIExplanationProjection> FHansaMerchantAIController::BuildExplanationProjection() const
	{
		TArray<FHansaMerchantAIExplanationProjection> Result;
		Result.Reserve(History.Num());
		for (const FHansaMerchantAIDecisionTrace& Trace : History)
		{
			FHansaMerchantAIExplanationProjection Item;
			Item.DecisionTick = Trace.DecisionTick;
			Item.Goal = LexToString(Trace.SelectedGoal);
			Item.Action = Trace.SubmittedCommandType.IsSet() ? LexToString(Trace.SubmittedCommandType.GetValue()) : TEXT("None");
			Item.Reason = Trace.Reason;
			Item.bCommandAccepted = Trace.bCommandAccepted;
			Item.GatewayOutcome = Trace.bCommandAccepted ? TEXT("Accepted") : LexToString(Trace.GatewayError);
			Result.Add(MoveTemp(Item));
		}
		return Result;
	}
}
