// Validate restore structure through the shared initialization validators; do not replace live records.
static bool ValidateState(const FHansaSimulationState& S, const FHansaSimulationDefinitionContext& D)
{
	if (!S.bInitialized || !S.InventoryLedger.bInitialized ||
		S.Placement.GetTopologyHash() != D.GetPlacementTopologyHash()) return false;
    if (!S.ConsumptionHistory.Validate(S.Clock)) return false;
    for (const auto& Sample : S.ConsumptionHistory.Samples)
        for (const auto& Good : Sample.Goods)
            if (!S.Cities.ContainsByPredicate([&](const auto& City) { return City.DefinitionId == Good.CityId; }) ||
                !D.GetEconomicRegistry() || !D.GetEconomicRegistry()->FindGood(Good.GoodId.ToString())) return false;
	FHansaSimulationInitialization I;
	I.Clock = S.Clock;
	I.CampaignSeed = S.CampaignSeed;
	I.ProcessedCommandCount = S.ProcessedCommandCount;
	I.LastProcessedCommandSequence = S.LastProcessedCommandSequence;
	I.LastProcessedCommandId = S.LastProcessedCommandId;
	I.CommandHistoryFingerprint = S.CommandHistoryFingerprint;
	I.PublishedDomainEventCount = S.PublishedDomainEventCount;
	I.RandomStreams = S.RandomStreams;
	I.Houses = S.Houses;
	I.Cities = S.Cities;
	I.Buildings = S.Buildings;
	I.Vehicles = S.Vehicles;
	I.Routes = S.Routes;
	I.TestEntities = S.TestEntities;
	I.MarketSettings = S.MarketSettings;
	I.LocalLogisticsSettings = S.LocalLogisticsSettings;
	I.Placement = {{}, S.Placement.Entitlements, S.Placement.Placements};
	I.InventoryMovementHistoryCapacity = S.InventoryLedger.MovementCapacity;
	for (const auto& V : S.InventoryLedger.Inventories)
	{
		FHansaInventoryInitialization R;
		R.Id = V.Id;
		R.OwnerKind = V.OwnerKind;
		R.CityId = V.CityId;
		R.BuildingId = V.BuildingId;
		R.VehicleId = V.VehicleId;
		R.Capacity = V.Capacity;
		R.AcceptedGoods = V.AcceptedGoods;
		for (const auto& Stock : V.Stocks) R.InitialStock.Add({Stock.GoodId, Stock.Quantity});
		I.Inventories.Add(MoveTemp(R));
	}
	for (const auto& V : S.Productions)
	{
		FHansaProductionInitialization R;
		R.Id = V.Id;
		R.Kind = V.Kind;
		R.BuildingId = V.BuildingId;
		R.CityId = V.CityId;
		R.RecipeId = V.RecipeId;
		R.SupplyGoodId = V.SupplyGoodId;
		R.SupplyQuantityPerCycle = V.SupplyQuantityPerCycle;
		R.SupplyCycleTicks = V.SupplyCycleTicks;
		R.InputInventoryId = V.InputInventoryId;
		R.OutputInventoryId = V.OutputInventoryId;
		R.AllocatedLaborerWorkforce = V.AllocatedLaborerWorkforce;
		R.AllocatedArtisanWorkforce = V.AllocatedArtisanWorkforce;
		R.bUsesCityWorkforce = V.bUsesCityWorkforce;
		R.bActive = V.bActive;
		I.Productions.Add(MoveTemp(R));
	}
	for (const auto& V : S.PopulationCohorts)
	{
        if (!V.ConsumptionHistory.Validate(S.Clock)) return false;
        for (const auto& Sample : V.ConsumptionHistory.Samples)
            for (const auto& Good : Sample.Goods)
                if (Good.CityId != V.CityId || !D.GetEconomicRegistry() ||
                    !D.GetEconomicRegistry()->FindGood(Good.GoodId.ToString())) return false;
		FHansaPopulationCohortInitialization R;
		R.Id = V.Id;
		R.ResidenceBuildingId = V.ResidenceBuildingId;
		R.CityId = V.CityId;
		R.ConsumptionInventoryId = V.ConsumptionInventoryId;
		R.TierId = V.TierId;
		R.Residents = V.Residents;
		R.ResidenceCapacity = V.ResidenceCapacity;
		R.PurchasingPowerBasisPoints = V.PurchasingPowerBasisPoints;
		R.ServiceAccessBasisPoints = V.ServiceAccessBasisPoints;
		R.ServiceReliabilityBasisPoints = V.ServiceReliabilityBasisPoints;
		I.PopulationCohorts.Add(MoveTemp(R));
	}
	for (const auto& V : S.Markets)
	{
		FHansaCityMarketInitialization R;
		R.CityId = V.CityId;
		R.GoodId = V.GoodId;
		R.InventoryIds = V.InventoryIds;
		R.DesiredReserve = V.DesiredReserve;
		R.ConfirmedIncomingSupplyPerUpdate = V.ConfirmedIncomingSupplyPerUpdate;
		R.bMarketOnly = V.bMarketOnly;
		R.BackgroundProductionPerUpdate = V.BackgroundProductionPerUpdate;
		R.BackgroundCitizenDemandPerUpdate = V.BackgroundCitizenDemandPerUpdate;
		R.BackgroundIndustrialDemandPerUpdate = V.BackgroundIndustrialDemandPerUpdate;
		R.ReportPolicy = V.ReportPolicy;
		R.SeasonModifierBasisPoints = V.SeasonModifierBasisPoints;
		R.CityModifierBasisPoints = V.CityModifierBasisPoints;
		R.MinimumPriceMilliMarks = V.MinimumPriceMilliMarks;
		R.MaximumPriceMilliMarks = V.MaximumPriceMilliMarks;
		R.InitialPriceMilliMarks = V.CurrentPriceMilliMarks;
		R.InitialLastUpdateTick = V.LastUpdateTick;
		R.InitialReportTick = V.Report.ReportTick;
		I.Markets.Add(MoveTemp(R));
	}
	for (const auto& V : S.LocalLogisticsRequests)
	{
		FHansaLogisticsRequestInitialization R;
		R.Id = V.Id;
		R.SourceInventoryId = V.SourceInventoryId;
		R.DestinationInventoryId = V.DestinationInventoryId;
		R.GoodId = V.GoodId;
		R.Priority = V.Priority;
		R.Quantity = V.RequestedQuantity;
		I.LocalLogisticsRequests.Add(MoveTemp(R));
	}
	for (const auto& V : S.Research)
	{
		FHansaHouseResearchInitialization R;
		R.HouseId = V.HouseId;
		R.AvailableResearchPoints = V.AvailableResearchPoints;
		R.ActiveTechnologyId = V.ActiveTechnologyId;
		R.ProgressTicks = V.ProgressTicks;
		R.CompletedTechnologyIds = V.CompletedTechnologyIds;
		R.AppliedEffects = V.AppliedEffects;
		I.Research.Add(MoveTemp(R));
	}
	if (!FHansaSimulationState::TryCreate(MoveTemp(I), D.GetPlacementTopologyShared()).IsSuccess()) return false;
	if (!S.NextProductionReservationValue || !S.NextLogisticsJobValue ||
		S.NextLogisticsReservationValue < 0x8000000000000000ULL ||
		S.InventoryLedger.RecentMovements.Num() > S.InventoryLedger.MovementCapacity) return false;
	TSet<FHansaReservationId> ReservationIds;
	for (const auto& R : S.InventoryLedger.Reservations)
	{
		if (!R.Id.IsValid() || ReservationIds.Contains(R.Id) || R.Quantity.GetRawValue() <= 0) return false;
		ReservationIds.Add(R.Id);
		const auto* Inventory = S.InventoryLedger.Inventories.FindByPredicate([&](const auto& V) { return V.Id == R.InventoryId; });
		if (!Inventory || !Inventory->Stocks.ContainsByPredicate([&](const auto& V) { return V.GoodId == R.GoodId; })) return false;
	}
	for (const auto& Inventory : S.InventoryLedger.Inventories)
		for (const auto& Stock : Inventory.Stocks)
		{
			int64 Reserved = 0;
			for (const auto& R : S.InventoryLedger.Reservations)
				if (R.InventoryId == Inventory.Id && R.GoodId == Stock.GoodId)
				{
					const auto Sum = FHansaCheckedIntegerMath::TryAdd(Reserved, R.Quantity.GetRawValue());
					if (!Sum) return false;
					Reserved = Sum.Value;
				}
			if (Stock.Reserved.GetRawValue() != Reserved || Reserved > Stock.Quantity.GetRawValue()) return false;
		}
	for (const auto& P : S.Productions)
	{
		if (P.ProgressTicks < 0) return false;
		for (const auto& R : P.InputReservations)
			if (!S.InventoryLedger.Reservations.ContainsByPredicate([&](const auto& V)
				{ return V.Id == R.ReservationId && V.InventoryId == P.InputInventoryId && V.GoodId == R.GoodId && V.Quantity == R.Quantity; })) return false;
	}
	for (const auto& R : S.LocalLogisticsRequests)
		if (R.RemainingQuantity.GetRawValue() < 0 || R.RequestedQuantity < R.RemainingQuantity ||
			R.InFlightQuantity.GetRawValue() < 0 || R.RemainingQuantity < R.InFlightQuantity) return false;
	TSet<FHansaLogisticsJobId> Jobs;
	TMap<FHansaLogisticsRequestId, int64> InFlightByRequest;
	for (const auto& J : S.LocalLogisticsJobs)
	{
		if (!J.Id.IsValid() || Jobs.Contains(J.Id) || J.Id.GetValue() >= S.NextLogisticsJobValue ||
			J.CargoQuantity.GetRawValue() < 0 || J.Quantity < J.CargoQuantity ||
			J.ElapsedTravelTicks < 0 || J.RemainingTravelTicks < 0 ||
			J.RoadDistanceCells <= 0 || J.RouteCells.Num() > 4096 ||
			J.Status > EHansaLogisticsJobStatus::PausedInTransit ||
			J.PauseReason > EHansaLogisticsRoadPathFailure::EndpointsDisconnected ||
			!S.LocalLogisticsRequests.ContainsByPredicate([&](const auto& R) { return R.Id == J.RequestId; })) return false;
		for (int32 CellIndex = 1; CellIndex < J.RouteCells.Num(); ++CellIndex)
			if (FMath::Abs(J.RouteCells[CellIndex].X - J.RouteCells[CellIndex - 1].X) +
				FMath::Abs(J.RouteCells[CellIndex].Y - J.RouteCells[CellIndex - 1].Y) != 1) return false;
		const bool bBeforePickup = J.Status == EHansaLogisticsJobStatus::AwaitingPickup ||
			J.Status == EHansaLogisticsJobStatus::PausedAwaitingPickup;
		const bool bCarriesCargo = J.Status == EHansaLogisticsJobStatus::InTransit ||
			J.Status == EHansaLogisticsJobStatus::PausedInTransit;
		if ((bBeforePickup && J.CargoQuantity.GetRawValue() != 0) ||
			(bCarriesCargo && J.CargoQuantity != J.Quantity) ||
			(J.Status == EHansaLogisticsJobStatus::Completed &&
				(J.CargoQuantity.GetRawValue() != 0 || J.RemainingTravelTicks != 0)) ||
			(J.Status == EHansaLogisticsJobStatus::AwaitingPickup && !J.SourceReservationId.IsValid()) ||
			(J.Status != EHansaLogisticsJobStatus::AwaitingPickup && J.SourceReservationId.IsValid())) return false;
		if (J.SourceReservationId.IsValid() && !S.InventoryLedger.Reservations.ContainsByPredicate(
			[&](const auto& Reservation)
			{
				return Reservation.Id == J.SourceReservationId &&
					Reservation.InventoryId == J.SourceInventoryId &&
					Reservation.GoodId == J.GoodId && Reservation.Quantity == J.Quantity;
			})) return false;
		if (J.Status != EHansaLogisticsJobStatus::Completed)
		{
			int64& InFlight = InFlightByRequest.FindOrAdd(J.RequestId);
			const auto Sum = FHansaCheckedIntegerMath::TryAdd(InFlight, J.Quantity.GetRawValue());
			if (!Sum) return false;
			InFlight = Sum.Value;
		}
		Jobs.Add(J.Id);
	}
	for (const auto& R : S.LocalLogisticsRequests)
		if (R.InFlightQuantity.GetRawValue() != InFlightByRequest.FindRef(R.Id)) return false;
	for (const auto& M : S.Markets)
		if (M.PriceHistory.Num() > S.MarketSettings.PriceHistoryCapacity ||
			M.Report.PriceHistory.Num() > S.MarketSettings.PriceHistoryCapacity) return false;
	return true;
}
