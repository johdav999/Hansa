#include "Queries/HansaSimulationReadOnly.h"

#include "Definitions/HansaEconomicRegistry.h"

namespace Hansa::Simulation
{
	TConstArrayView<FHansaForeignPresenceState> FHansaSimulationReadOnlyAccess::GetForeignPresences() const
	{
		return State ? TConstArrayView<FHansaForeignPresenceState>(State->ForeignPresences) : TConstArrayView<FHansaForeignPresenceState>();
	}

	TOptional<FHansaForeignPresenceProjection> FHansaSimulationReadOnlyAccess::QueryForeignPresence(
		FHansaHouseId HouseId, FHansaCityDefinitionId CityId) const
	{
		if (!State || !Definitions || !HouseId.IsValid() || !CityId.IsValid()) return {};
		const FHansaForeignPresenceState* Presence = State->ForeignPresences.FindByPredicate(
			[&](const auto& Value){ return Value.HouseId == HouseId && Value.CityId == CityId; });
		if (!Presence) return {};
		const FHansaEconomicRegistry* Registry = Definitions->GetEconomicRegistry();
		if (!Registry) return {};
		const auto* Stage = Registry->FindPresenceStage(Presence->CurrentStageId);
		const auto* Policy = Registry->FindCityTradePolicyForCity(CityId.ToString());
		if (!Stage || !Policy || !Policy->AllowedStageIds.Contains(Stage->StableId)) return {};

		FHansaForeignPresenceProjection Result;
		Result.HouseId=Presence->HouseId; Result.CityId=Presence->CityId; Result.CurrentStageId=Presence->CurrentStageId;
		Result.CurrentStageDisplayName=Stage->DisplayName; Result.Status=Presence->Status; Result.Contributions=Presence->Contributions;
		Result.EstablishedTick=Presence->EstablishedTick; Result.LastUpgradeTick=Presence->LastUpgradeTick;
		Result.StationId=Presence->StationId; Result.LeasedPlotId=Presence->LeasedPlotId;
		Result.Upgrade=Presence->Upgrade; Result.SpecializationRevision=Presence->SpecializationRevision; Result.History=Presence->History;
		const FHansaHouseState* ProjectionHouse=State->Houses.FindByPredicate([&](const auto& V){return V.Id==HouseId;});
		const FHansaTradeStationState* ProjectionStation=State->TradeStations.FindByPredicate([&](const auto& V){return V.Id==Presence->StationId;});
		for(const auto& Branch:Policy->Specializations)
		{
			FHansaPresenceSpecializationProjection Option;Option.SpecializationId=Branch.SpecializationId;Option.DisplayName=Branch.DisplayName;Option.ExclusiveGroupId=Branch.ExclusiveGroupId;
			Option.GrantedCapabilityIds=Branch.GrantedCapabilityIds;Option.InvestmentCostPfennig=Branch.InvestmentCostPfennig;Option.InvestmentGoods=Branch.InvestmentGoods;Option.RespecRefundBasisPoints=Branch.RespecRefundBasisPoints;
			Option.StorageCapacityBonusMilliUnits=Branch.StorageCapacityBonusMilliUnits;Option.AdditionalOrderSlots=Branch.AdditionalOrderSlots;Option.StationTransferCapBonusMilliUnits=Branch.StationTransferCapBonusMilliUnits;
			Option.bSelected=Presence->ActiveSpecializationIds.Contains(Branch.SpecializationId);const auto* Required=Registry->FindPresenceStage(Branch.RequiredStageId);
			const bool bStage=Required&&Stage->Ordinal>=Required->Ordinal;const bool bConflict=Policy->Specializations.ContainsByPredicate([&](const auto& Other){return Other.ExclusiveGroupId==Branch.ExclusiveGroupId&&Presence->ActiveSpecializationIds.Contains(Other.SpecializationId)&&Other.SpecializationId!=Branch.SpecializationId;});
			bool bGoods=ProjectionStation!=nullptr;for(const auto& Cost:Branch.InvestmentGoods){const auto Good=FHansaGoodId::TryParse(Cost.GoodId);const auto Stock=Good&&ProjectionStation?State->InventoryLedger.CreateReadOnlyAccess().QueryStock(ProjectionStation->InventoryId,Good.Value):TOptional<FHansaInventoryStockProjection>();bGoods&=Stock.IsSet()&&Stock->Available.GetRawValue()>=Cost.QuantityMilliUnits;}
			const bool bMoney=ProjectionHouse&&ProjectionHouse->Money.GetRawValue()>=Branch.InvestmentCostPfennig;Option.bAvailable=!Option.bSelected&&bStage&&bGoods&&bMoney&&(!bConflict||Branch.bAllowRespec);
			if(Option.bSelected)Option.Blocker=TEXT("Selected");else if(!bStage)Option.Blocker=TEXT("Requires merchant office");else if(bConflict&&!Branch.bAllowRespec)Option.Blocker=TEXT("Exclusive choice already selected");else if(!bMoney)Option.Blocker=TEXT("Insufficient house funds");else if(!bGoods)Option.Blocker=TEXT("Required station materials unavailable");
			Result.Specializations.Add(MoveTemp(Option));

		}
		Result.Specializations.Sort([](const auto& L,const auto& R){return L.SpecializationId<R.SpecializationId;});Result.Privileges=Presence->Privileges;Result.CityProjects=Presence->CityProjects;Result.bGovernanceAuthority=Presence->bGovernanceAuthority;Result.GovernanceCharterId=Presence->GovernanceCharterId;Result.GovernanceGrantedTick=Presence->GovernanceGrantedTick;Result.AuthorityRevision=Presence->AuthorityRevision;
		for (const auto& Capability : Registry->GetPresenceCapabilities())
		{
			const bool bGranted = Presence->Status == EHansaForeignPresenceStatus::Active &&
				Presence->GrantedCapabilityIds.Contains(Capability.StableId) && !Policy->DeniedCapabilityIds.Contains(Capability.StableId);
			FString Reason;
			if (Policy->DeniedCapabilityIds.Contains(Capability.StableId)) Reason=TEXT("Denied by city policy ")+Policy->StableId;
			else if (Presence->Status != EHansaForeignPresenceStatus::Active) Reason=TEXT("Presence is not active");
			else if (bGranted) Reason=TEXT("Granted by stage ")+Stage->StableId;
			else Reason=TEXT("Not granted at current stage ")+Stage->StableId;
			Result.Capabilities.Add({Capability.StableId, Capability.DisplayName, MoveTemp(Reason), bGranted});
		}

		const FHansaHouseState* House = State->Houses.FindByPredicate([&](const auto& Value){ return Value.Id == HouseId; });
		for (const auto& Candidate : Registry->GetPresenceStages())
		{
			if (!Registry->IsValidPresenceTransition(CityId.ToString(), Stage->StableId, Candidate.StableId)) continue;
			FHansaPresenceStageOptionProjection Option;
			Option.StageId=Candidate.StableId; Option.DisplayName=Candidate.DisplayName; Option.Ordinal=Candidate.Ordinal; Option.UpgradeCostPfennig=Candidate.UpgradeCostPfennig;
			auto AddRequirement=[&](const FString& Id,const FString& Description,int64 Current,int64 Required)
			{ Option.Requirements.Add({Id,Description,Current,Required,Current>=Required}); };
			AddRequirement(TEXT("LawfulTradeVolume"),TEXT("Lawful trade volume"),Presence->Contributions.LawfulTradeVolumeMilliUnits,Candidate.RequiredLawfulTradeVolumeMilliUnits);
			AddRequirement(TEXT("CompletedDeliveries"),TEXT("Completed deliveries"),Presence->Contributions.CompletedDeliveryCount,Candidate.RequiredCompletedDeliveries);
			AddRequirement(TEXT("TransactionValue"),TEXT("Lawful transaction value"),Presence->Contributions.TransactionValuePfennig,Candidate.RequiredTransactionValuePfennig);
			AddRequirement(TEXT("ShortageRelief"),TEXT("Shortage relief delivered"),Presence->Contributions.FulfilledShortageMilliUnits,Candidate.RequiredFulfilledShortageMilliUnits);
			AddRequirement(TEXT("ReliableOperation"),TEXT("Reliable operating ticks"),Presence->Contributions.ReliableOperatingTicks,Candidate.RequiredReliableOperatingTicks);
			AddRequirement(TEXT("SolventOperation"),TEXT("Solvent operating ticks"),Presence->Contributions.SolventOperatingTicks,Candidate.RequiredSolventOperatingTicks);
			const int64 PlannedInvestment = Presence->Contributions.InvestedPfennig > MAX_int64 - Candidate.UpgradeCostPfennig ? MAX_int64 : Presence->Contributions.InvestedPfennig + Candidate.UpgradeCostPfennig;
			AddRequirement(TEXT("QualifyingInvestment"),TEXT("Qualifying investment after funding"),PlannedInvestment,Candidate.RequiredInvestedPfennig);
			AddRequirement(TEXT("AvailableMoney"),TEXT("Available house money"),House?House->Money.GetRawValue():0,Candidate.UpgradeCostPfennig);
			for (const auto& Cost : Candidate.UpgradeGoods)
			{
				int64 Available = 0;
				if (Presence->StationId.IsValid()) if (const auto* Station=State->TradeStations.FindByPredicate([&](const auto& V){return V.Id==Presence->StationId;}))
					if (const auto Stock=State->InventoryLedger.CreateReadOnlyAccess().QueryStock(Station->InventoryId,FHansaGoodId::TryParse(Cost.GoodId).Value);Stock.IsSet()) Available=Stock->Available.GetRawValue();
				Option.Requirements.Add({TEXT("UpgradeGood.")+Cost.GoodId,TEXT("Required material ")+Cost.GoodId,Available,Cost.QuantityMilliUnits,Available>=Cost.QuantityMilliUnits});
			}
			Option.bProgressRequirementsMet=!Option.Requirements.ContainsByPredicate([](const auto& Requirement){return Requirement.RequirementId!=TEXT("AvailableMoney")&&!Requirement.RequirementId.StartsWith(TEXT("UpgradeGood."))&&!Requirement.bMet;});
			Option.bFundingAvailable=!Option.Requirements.ContainsByPredicate([](const auto& Requirement){return (Requirement.RequirementId==TEXT("AvailableMoney")||Requirement.RequirementId.StartsWith(TEXT("UpgradeGood.")))&&!Requirement.bMet;});
			Option.bAvailable=Presence->Status==EHansaForeignPresenceStatus::Active && Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::None && Option.bProgressRequirementsMet && Option.bFundingAvailable;
			Result.NextStages.Add(MoveTemp(Option));
		}
		Result.NextStages.Sort([](const auto& Left,const auto& Right){return Left.Ordinal!=Right.Ordinal?Left.Ordinal<Right.Ordinal:Left.StageId<Right.StageId;});
		return Result;
	}

	TArray<FHansaForeignPresenceProjection> FHansaSimulationReadOnlyAccess::BuildForeignPresenceProjection(FHansaHouseId HouseId) const
	{
		TArray<FHansaForeignPresenceProjection> Result;
		if (!State) return Result;
		for (const auto& Presence : State->ForeignPresences)
		{
			if (HouseId.IsValid() && Presence.HouseId != HouseId) continue;
			if (TOptional<FHansaForeignPresenceProjection> Projection = QueryForeignPresence(Presence.HouseId, Presence.CityId)) Result.Add(MoveTemp(Projection.GetValue()));
		}
		return Result;
	}
	FHansaSpotTradeQuoteProjection FHansaSimulationReadOnlyAccess::QuerySpotTradeQuote(FHansaHouseId HouseId,
		FHansaVehicleId VehicleId, FHansaCityDefinitionId CityId, FHansaGoodId GoodId,
		EHansaSpotTradeSide Side, FHansaQuantity Quantity) const
	{
		FHansaSpotTradeQuoteProjection Result;
		Result.HouseId=HouseId; Result.VehicleId=VehicleId; Result.CityId=CityId; Result.GoodId=GoodId;
		Result.Side=Side; Result.RequestedQuantity=Quantity;
		auto Block=[&](const TCHAR* Cause,const TCHAR* Remedy){Result.Cause=Cause;Result.Remedy=Remedy;return Result;};
		if (!State || !Definitions || Quantity.GetRawValue()<=0) return Block(TEXT("Choose a positive quantity."),TEXT("Increase the trade quantity."));
		const auto* Registry=Definitions->GetEconomicRegistry();
		const auto* Vehicle=State->Vehicles.FindByPredicate([&](const auto& V){return V.Id==VehicleId;});
		if (!Vehicle || Vehicle->OwnerId!=HouseId) return Block(TEXT("The selected ship is not owned by this house."),TEXT("Select an owned Cog."));
		const bool bTraveling=State->Routes.ContainsByPredicate([&](const auto& R){return R.VehicleId==VehicleId&&R.Lifecycle==EHansaRouteLifecycleState::Traveling;});
		if (Vehicle->Mode!=EHansaRouteMode::Sea || Vehicle->CurrentCityId!=CityId || bTraveling ||
			(Vehicle->Navigation.CityId.IsValid()&&!Vehicle->Navigation.IsAtHome()))
			return Block(TEXT("The ship is not berthed in this city."),TEXT("Sail the owned Cog to this city's berth."));
		const auto* Policy=Registry?Registry->FindCityTradePolicyForCity(CityId.ToString()):nullptr;
		const auto* Presence=State->ForeignPresences.FindByPredicate([&](const auto& P){return P.HouseId==HouseId&&P.CityId==CityId;});
		if (!Policy||!Policy->bPublicMarketAccess||!Presence||Presence->Status!=EHansaForeignPresenceStatus::Active||
			!Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.PublicMarketTrade"))||Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.PublicMarketTrade")))
			return Block(TEXT("Public market access is unavailable."),TEXT("Establish an active visiting contact permitted by city policy."));
		const auto* Market=State->Markets.FindByPredicate([&](const auto& M){return M.CityId==CityId&&M.GoodId==GoodId;});
		if (!Market||!Market->Report.bAvailable||Market->Report.PriceMilliMarks<=0)
			return Block(TEXT("No lawful market report is available for this good."),TEXT("Wait for or obtain a market report."));
		const int32 Reduction=FHansaResearchEffectResolver::GetBasisPoints(State->Research,HouseId,
			EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints,CityId.ToString());
		const int32 Multiplier=Side==EHansaSpotTradeSide::BuyFromCity?10000+FMath::Max(0,500-Reduction):10000-FMath::Max(0,500-Reduction);
		const auto Price=FHansaCheckedIntegerMath::TryMultiplyDivide(Market->Report.PriceMilliMarks,Multiplier,10000,EHansaRoundingMode::HalfAwayFromZero);
		if (!Price||Price.Value<=0) return Block(TEXT("The reported price cannot be quoted safely."),TEXT("Wait for the next market report."));
		Result.ReviewedMarketUpdateTick=Market->Report.MarketUpdateTick;Result.ReviewedUnitPriceMilliMarks=Price.Value;
		const auto Cargo=State->InventoryLedger.CreateReadOnlyAccess().QueryInventory(Vehicle->CargoInventoryId);
		int64 Estimate=Quantity.GetRawValue();
		if (Side==EHansaSpotTradeSide::BuyFromCity)
		{
			Estimate=FMath::Min(Estimate,Cargo.IsSet()?Cargo->FreeCapacity.GetRawValue():0);
			Estimate=FMath::Min(Estimate,Market->Report.Stock.GetRawValue());
			const auto* House=State->Houses.FindByPredicate([&](const auto& H){return H.Id==HouseId;});
			const auto Affordable=FHansaCheckedIntegerMath::TryMultiplyDivide(House?FMath::Max<int64>(0,House->Money.GetRawValue()):0,1000,Price.Value,EHansaRoundingMode::TowardZero);
			Estimate=FMath::Min(Estimate,Affordable?Affordable.Value:0);
		}
		else
		{
			const auto Stock=State->InventoryLedger.CreateReadOnlyAccess().QueryStock(Vehicle->CargoInventoryId,GoodId);
			Estimate=FMath::Min(Estimate,Stock.IsSet()?Stock->Available.GetRawValue():0);
		}
		Result.EstimatedQuantity=FHansaQuantity::FromRaw(FMath::Max<int64>(0,Estimate));
		const auto Settlement=FHansaCheckedIntegerMath::TryMultiplyDivide(Result.EstimatedQuantity.GetRawValue(),Price.Value,1000,
			Side==EHansaSpotTradeSide::BuyFromCity?EHansaRoundingMode::Ceiling:EHansaRoundingMode::TowardZero);
		Result.EstimatedSettlementMoneyRaw=Settlement?(Side==EHansaSpotTradeSide::BuyFromCity?-Settlement.Value:Settlement.Value):0;
		Result.bCanSubmit=true;
		Result.Cause=TEXT("Estimate uses the last lawful report; final quantity and price are revalidated on confirmation.");
		Result.Remedy=TEXT("Confirm now or refresh after a stale-review rejection.");
		return Result;
	}

	TConstArrayView<FHansaTradeStationState> FHansaSimulationReadOnlyAccess::GetTradeStations() const
	{
		return State ? TConstArrayView<FHansaTradeStationState>(State->TradeStations) : TConstArrayView<FHansaTradeStationState>();
	}

	TConstArrayView<FHansaLeasedPlotState> FHansaSimulationReadOnlyAccess::GetLeasedPlots() const
	{
		return State ? TConstArrayView<FHansaLeasedPlotState>(State->LeasedPlots) : TConstArrayView<FHansaLeasedPlotState>();
	}

	TOptional<FHansaTradeStationProjection> FHansaSimulationReadOnlyAccess::QueryTradeStation(const FHansaTradeStationId StationId) const
	{
		if (!State || !Definitions || !StationId.IsValid()) return {};
		const auto* Station = State->TradeStations.FindByPredicate([&](const auto& Value){ return Value.Id == StationId; });
		if (!Station) return {};
		const auto* Lease = State->LeasedPlots.FindByPredicate([&](const auto& Value){ return Value.Id == Station->LeasedPlotId; });
		const auto* Registry = Definitions->GetEconomicRegistry();
		const auto* Policy = Registry ? Registry->FindCityTradePolicyForCity(Station->CityId.ToString()) : nullptr;
		const auto* Site = Policy ? Policy->TradeStationSites.FindByPredicate([&](const auto& Value){ return Value.SiteId == Station->SiteId; }) : nullptr;
		const auto Inventory = State->InventoryLedger.CreateReadOnlyAccess().QueryInventory(Station->InventoryId);
		if (!Lease || !Inventory.IsSet()) return {};
		FHansaTradeStationProjection Result;
		Result.Station = *Station; Result.Lease = *Lease; Result.StorageUsed = Inventory->UsedCapacity;
		Result.StorageCapacity = Inventory->Capacity; Result.StorageReserved = Inventory->Reserved;
		Result.PresentationClassPath = Site ? Site->PresentationClassPath : FString();
		Result.PreservedAssets = TEXT("Cargo, reservations, orders, routes, factor identity, leased buildings, workers and presence history remain authoritative until explicitly recovered or removed.");
		Result.ContinuingCosts = (Station->OperationalState==EHansaTradeStationOperationalState::Active||Station->OperationalState==EHansaTradeStationOperationalState::StorageBlocked||Station->OperationalState==EHansaTradeStationOperationalState::OrderSuspended)
			? FString::Printf(TEXT("Station upkeep continues at %lld pfennig per tick."),static_cast<long long>(Station->UpkeepPfennigPerTick))
			: TEXT("New station upkeep is paused; any displayed arrears remain payable.");
		if(!Policy)Result.RecoveryDiagnostic=TEXT("Removed city policy: install compatible city policy content before this station can resume.");
		else if(!Site)Result.RecoveryDiagnostic=TEXT("Missing station-site definition: install the original site or an explicit definition migration.");
		else if(Lease->BoundsMin.X!=Site->LeaseBoundsMin.X||Lease->BoundsMin.Y!=Site->LeaseBoundsMin.Y||Lease->BoundsMax.X!=Site->LeaseBoundsMax.X||Lease->BoundsMax.Y!=Site->LeaseBoundsMax.Y)
			Result.RecoveryDiagnostic=TEXT("Changed plot definition: saved bounds are preserved; an explicit plot migration is required.");
		const auto* Presence=State->ForeignPresences.FindByPredicate([&](const auto& V){return V.HouseId==Station->OwnerId&&V.CityId==Station->CityId;});
		if(Result.RecoveryDiagnostic.IsEmpty()&&Station->Status==EHansaTradeStationStatus::Active&&(!Presence||!Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"))))
			Result.RecoveryDiagnostic=TEXT("Incompatible capability set: restore the trade-station capability or migrate the presence stage.");
		if(!Result.RecoveryDiagnostic.IsEmpty()){Result.Blocker=Result.RecoveryDiagnostic;Result.NextStep=TEXT("Do not delete records; restore compatible content or run an explicit migration.");return Result;}
		switch (Station->Status)
		{
		case EHansaTradeStationStatus::Proposed: Result.Blocker = TEXT("Awaiting funding"); Result.NextStep = TEXT("Fund the authored money and material cost"); break;
		case EHansaTradeStationStatus::UnderConstruction: Result.Blocker = FString::Printf(TEXT("Construction completes at tick %lld"), static_cast<long long>(Station->CompletionTick.GetValue())); Result.NextStep = TEXT("Advance simulation time"); break;
		case EHansaTradeStationStatus::Active: Result.Blocker = Inventory->UsedCapacity.GetRawValue() > 0 ? TEXT("Closing will strand and preserve stored cargo") : FString(); Result.NextStep = Inventory->UsedCapacity.GetRawValue() > 0 ? TEXT("Close safely, then recover cargo outbound before final plot release") : TEXT("Station is operational"); break;
		case EHansaTradeStationStatus::Suspended: Result.Blocker = TEXT("Station is suspended"); Result.NextStep = TEXT("Resolve the suspension cause"); break;
		case EHansaTradeStationStatus::Closed: Result.NextStep = TEXT("Station is closed and the plot has been released"); break;
		}
		switch(Station->OperationalState)
		{
		case EHansaTradeStationOperationalState::Underfunded:Result.Blocker=FString::Printf(TEXT("Upkeep payment failed; %lld pfennig is outstanding"),static_cast<long long>(Station->OutstandingUpkeepPfennig));Result.NextStep=TEXT("Pay arrears with Fund station, then manually resume any paused routes");break;
		case EHansaTradeStationOperationalState::StorageBlocked:Result.Blocker=TEXT("Station storage is full; inbound transfers and acquisition orders are blocked");Result.NextStep=TEXT("Load cargo onto a berthed owned Cog or run release orders");break;
		case EHansaTradeStationOperationalState::OrderSuspended:Result.Blocker=TEXT("All live factor orders are paused");Result.NextStep=TEXT("Resume or replace at least one factor order");break;
		case EHansaTradeStationOperationalState::RightsSuspended:Result.Blocker=TEXT("City or presence rights are suspended");Result.NextStep=TEXT("Recover cargo outbound; restore the authored presence rights before resuming trade");break;
		case EHansaTradeStationOperationalState::VoluntarilyClosed:
			if(Inventory->UsedCapacity.GetRawValue()>0||Inventory->Reserved.GetRawValue()>0||!Lease->OccupyingBuildingIds.IsEmpty()){Result.Blocker=TEXT("Closure is preserving stranded station assets");Result.NextStep=TEXT("Recover cargo outbound, release reservations, and remove leased buildings; then close again to release the plot");}
			else Result.NextStep=TEXT("Closure is finalized; the empty plot and presence link were released");break;
		case EHansaTradeStationOperationalState::Revoked:Result.Blocker=TEXT("Station rights were revoked");Result.NextStep=TEXT("Recover cargo outbound; revoked rights cannot be resumed without a new grant");break;
		default:break;
		}
		return Result;
	}

	TArray<FHansaTradeStationProjection> FHansaSimulationReadOnlyAccess::BuildTradeStationProjection(const FHansaHouseId HouseId) const
	{
		TArray<FHansaTradeStationProjection> Result;
		if (!State) return Result;
		for (const auto& Station : State->TradeStations)
			if ((!HouseId.IsValid() || Station.OwnerId == HouseId))
				if (auto Projection = QueryTradeStation(Station.Id); Projection.IsSet()) Result.Add(MoveTemp(Projection.GetValue()));
		return Result;
	}
	TArray<FHansaLeasedPlotOverlayProjection> FHansaSimulationReadOnlyAccess::BuildLeasedPlotOverlayProjection(const FHansaHouseId HouseId, const FHansaCityDefinitionId CityId) const
	{
		TArray<FHansaLeasedPlotOverlayProjection> Result;if(!State)return Result;
		for(const FHansaLeasedPlotState& Lease:State->LeasedPlots)
		{
			if((HouseId.IsValid()&&Lease.OwnerId!=HouseId)||(CityId.IsValid()&&Lease.CityId!=CityId))continue;
			FHansaLeasedPlotOverlayProjection Out;Out.LeaseId=Lease.Id;Out.OwnerId=Lease.OwnerId;Out.CityId=Lease.CityId;Out.BoundsMin=Lease.BoundsMin;Out.BoundsMax=Lease.BoundsMax;
			if(Lease.BoundsMin.X>Lease.BoundsMax.X||Lease.BoundsMin.Y>Lease.BoundsMax.Y){Out.Kind=EHansaLeasedPlotOverlayKind::Invalid;Out.PatternKey=TEXT("error-cross");Out.Label=TEXT("Invalid lease");Out.Reason=TEXT("Authored bounds are invalid");}
			else if(!Lease.bActive){Out.Kind=EHansaLeasedPlotOverlayKind::Warning;Out.PatternKey=TEXT("warning-stripes");Out.Label=TEXT("Rights suspended");Out.Reason=TEXT("Resolve the station or presence suspension before building");}
			else if(Lease.OccupyingBuildingIds.IsEmpty()){Out.Kind=EHansaLeasedPlotOverlayKind::LeasedEmpty;Out.PatternKey=TEXT("diagonal-hatch");Out.Label=TEXT("Leased — empty");Out.Reason=TEXT("Authorized footprint available");}
			else{Out.Kind=EHansaLeasedPlotOverlayKind::Owned;Out.PatternKey=TEXT("solid-brass-outline");Out.Label=TEXT("Player construction");Out.Reason=TEXT("Occupied by player-owned leased construction");}
			Result.Add(MoveTemp(Out));
		}
		Result.Sort([](const auto& A,const auto& B){return A.CityId!=B.CityId?A.CityId<B.CityId:A.LeaseId<B.LeaseId;});return Result;
	}

	TArray<FHansaForeignConstructionOptionProjection> FHansaSimulationReadOnlyAccess::BuildForeignConstructionBrowser(const FHansaHouseId HouseId, const FHansaCityDefinitionId CityId) const
	{
		TArray<FHansaForeignConstructionOptionProjection> Result;if(!State||!Definitions||!HouseId.IsValid()||!CityId.IsValid())return Result;const auto* Registry=Definitions->GetEconomicRegistry();if(!Registry)return Result;
		const auto* Presence=State->ForeignPresences.FindByPredicate([&](const auto& V){return V.HouseId==HouseId&&V.CityId==CityId;});
		for(const TCHAR* StableId:{TEXT("Building.Market"),TEXT("Building.Warehouse"),TEXT("Building.Dock")})
		{
			const auto* Building=Registry->FindBuilding(StableId);if(!Building)continue;FHansaForeignConstructionOptionProjection Out;Out.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(StableId).Value;Out.DisplayName=Building->DisplayName;Out.Category=Building->ConstructionMenuCategory==TEXT("Harbor")?TEXT("Commercial"):Building->ConstructionMenuCategory;
			if(!Presence||Presence->Status!=EHansaForeignPresenceStatus::Active)Out.Reason=TEXT("No active foreign presence");
			else if(!Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.MerchantQuarter")))Out.Reason=TEXT("Requires Merchant quarter");
			else if(!State->LeasedPlots.ContainsByPredicate([&](const auto& Lease){return Lease.OwnerId==HouseId&&Lease.CityId==CityId&&Lease.bActive&&Lease.PermittedBuildingCategories.Contains(Out.Category);}))Out.Reason=TEXT("No active leased plot permits this category");
			else{Out.bPermitted=true;Out.Reason=TEXT("Permitted on the highlighted leased footprint");}
			Result.Add(MoveTemp(Out));
		}
		return Result;
	}
}
