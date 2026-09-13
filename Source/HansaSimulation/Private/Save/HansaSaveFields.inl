// Explicit format-v2 field order. Breaking edits require a format migration.
void Value(FHansaSimulationState& V)
{
	Value(V.bInitialized);
	Value(V.Clock);
	Value(V.CampaignSeed);
	Value(V.ProcessedCommandCount);
	Value(V.LastProcessedCommandSequence);
	Value(V.LastProcessedCommandId);
	Value(V.CommandHistoryFingerprint);
	Value(V.PublishedDomainEventCount);
	Value(V.RandomStreams);
	Value(V.Houses);
	Value(V.Cities);
	Value(V.Buildings);
	Value(V.Vehicles);
	Value(V.Routes);
	Value(V.Research);
	Value(V.TestEntities);
	Value(V.Placement);
	Value(V.InventoryLedger);
	Value(V.NextProductionReservationValue);
	Value(V.Productions);
	Value(V.PopulationCohorts);
	Value(V.MarketSettings);
	Value(V.Markets);
	Value(V.LocalLogisticsSettings);
	Value(V.NextLogisticsJobValue);
	Value(V.NextLogisticsReservationValue);
	Value(V.LocalLogisticsRequests);
	Value(V.LocalLogisticsJobs);
    if (FormatVersion >= 4) Value(V.ConsumptionHistory);
}

void Value(FHansaConsumptionHistory& V) { Value(V.Samples); }
void Value(FHansaConsumptionSample& V) { Value(V.EndTick); Value(V.Goods); }
void Value(FHansaConsumptionTotal& V)
{
    Value(V.CityId); Value(V.GoodId); Value(V.Required); Value(V.Consumed);
}

void Value(FHansaHouseState& V)
{
	Value(V.Id);
	Value(V.Money);
}

void Value(FHansaCityState& V)
{
	Value(V.DefinitionId);
	Value(V.AggregateStock);
}

void Value(FHansaBuildingState& V)
{
	Value(V.Id);
	Value(V.DefinitionId);
	Value(V.OwnerId);
	Value(V.ConstructionProgress);
	Value(V.ConstructionState);
	Value(V.ConstructionStartedTick);
	Value(V.ConstructionElapsedTicks);
}

void Value(FHansaVehicleState& V)
{
	Value(V.Id);
	Value(V.DefinitionId);
	Value(V.OwnerId);
	Value(V.Cargo);
	Value(V.CargoInventoryId);
	Value(V.Mode);
	Value(V.Capacity);
	Value(V.CurrentCityId);
	Value(V.UpkeepPfennigPerTravelTick);
	Value(V.AccruedUpkeepPfennig);
}

void Value(FHansaRouteState& V)
{
	Value(V.Id);
	Value(V.OwnerId);
	Value(V.VehicleId);
	Value(V.Progress);
	Value(V.RouteDefinitionId);
	Value(V.Mode);
	Value(V.Stops);
	Value(V.Lifecycle);
	Value(V.CurrentStopIndex);
	Value(V.NextStopIndex);
	Value(V.RemainingTravelTicks);
	Value(V.TotalTravelTicks);
	Value(V.bPendingStopActions);
	Value(V.CompletedLegCount);
	Value(V.MissedCargoActionCount);
	Value(V.LastTransfer);
}

void Value(FHansaRouteStop& V)
{
	Value(V.CityId);
	Value(V.Actions);
}

void Value(FHansaRouteCargoAction& V)
{
	Value(V.Kind);
	Value(V.Condition);
	Value(V.GoodId);
	Value(V.QuantityLimit);
	Value(V.MinimumSourceReserve);
}

void Value(FHansaRouteTransferRecord& V)
{
	Value(V.Tick);
	Value(V.StopIndex);
	Value(V.ActionIndex);
	Value(V.Kind);
	Value(V.CityId);
	Value(V.GoodId);
	Value(V.RequestedQuantity);
	Value(V.AppliedQuantity);
	Value(V.Outcome);
}

void Value(FHansaHouseResearchState& V)
{
	Value(V.HouseId);
	Value(V.AvailableResearchPoints);
	Value(V.ActiveTechnologyId);
	Value(V.ProgressTicks);
	Value(V.CompletedTechnologyIds);
	Value(V.AppliedEffects);
}

void Value(FHansaAppliedResearchEffect& V)
{
	Value(V.SourceTechnologyId);
	Value(V.Kind);
	Value(V.TargetStableId);
	Value(V.Magnitude);
}

void Value(FHansaTestEntityState& V)
{
	Value(V.Id);
	Value(V.OwnerId);
	Value(V.Value);
}

void Value(FHansaPlacementState& V)
{
	if (FormatVersion < 7)
	{
		if (!bReading) LegacyPlacementMaps.Append(V.GetMaps());
		Value(LegacyPlacementMaps);
	}
	Value(V.Entitlements);
	Value(V.Placements);
}

void Value(FHansaPlacementMapInitialization& V)
{
	Value(V.CityId);
	Value(V.BoundsMin);
	Value(V.BoundsMax);
	Value(V.RoadBuildingDefinitionId);
	Value(V.Cells);
}

void Value(FHansaGridCoordinate& V)
{
	Value(V.X);
	Value(V.Y);
}

void Value(FHansaPlacementGridCell& V)
{
	Value(V.Coordinate);
	Value(V.Terrain);
	Value(V.OwnerId);
	Value(V.bBlocked);
}

void Value(FHansaPlacementEntitlement& V)
{
	Value(V.HouseId);
	Value(V.BuildingDefinitionId);
}

void Value(FHansaPlacedBuildingRecord& V)
{
	Value(V.BuildingId);
	Value(V.OwnerId);
	Value(V.Spec);
	Value(V.OccupiedCells);
}

void Value(FHansaPlacementSpec& V)
{
	Value(V.CityId);
	Value(V.BuildingDefinitionId);
	Value(V.Anchor);
	Value(V.Rotation);
}

void Value(FHansaInventoryLedger& V)
{
	Value(V.bInitialized);
	Value(V.MovementCapacity);
	Value(V.LastMovementSequence);
	Value(V.Inventories);
	Value(V.Reservations);
	Value(V.RecentMovements);
}

void Value(FHansaInventoryRecord& V)
{
	Value(V.Id);
	Value(V.OwnerKind);
	Value(V.CityId);
	Value(V.BuildingId);
	Value(V.VehicleId);
	Value(V.Capacity);
	Value(V.AcceptedGoods);
	Value(V.Stocks);
}

void Value(FHansaInventoryStockRecord& V)
{
	Value(V.GoodId);
	Value(V.Quantity);
	Value(V.Reserved);
}

void Value(FHansaInventoryReservation& V)
{
	Value(V.Id);
	Value(V.InventoryId);
	Value(V.GoodId);
	Value(V.Quantity);
}

void Value(FHansaInventoryMovement& V)
{
	Value(V.Sequence);
	Value(V.Tick);
	Value(V.Kind);
	Value(V.InventoryId);
	Value(V.CounterpartyInventoryId);
	Value(V.ExternalEndpointId);
	Value(V.GoodId);
	Value(V.Quantity);
	Value(V.ReservationId);
}

void Value(FHansaProductionState& V)
{
	Value(V.Id);
	Value(V.Kind);
	Value(V.BuildingId);
	Value(V.CityId);
	Value(V.RecipeId);
	Value(V.SupplyGoodId);
	Value(V.SupplyQuantityPerCycle);
	Value(V.SupplyCycleTicks);
	Value(V.InputInventoryId);
	Value(V.OutputInventoryId);
	Value(V.AllocatedLaborerWorkforce);
	Value(V.AllocatedArtisanWorkforce);
	Value(V.bUsesCityWorkforce);
	Value(V.bActive);
	Value(V.ProgressTicks);
	Value(V.CompletedCycles);
	Value(V.bCompletedCycleLastTick);
	Value(V.Blocker);
	Value(V.BlockingGoodId);
	Value(V.BlockingRequiredQuantity);
	Value(V.BlockingAvailableQuantity);
	Value(V.InputReservations);
}

void Value(FHansaProductionInputReservation& V)
{
	Value(V.GoodId);
	Value(V.ReservationId);
	Value(V.Quantity);
}

void Value(FHansaPopulationCohortState& V)
{
	Value(V.Id);
	Value(V.ResidenceBuildingId);
	Value(V.CityId);
	Value(V.ConsumptionInventoryId);
	Value(V.TierId);
	Value(V.Residents);
	Value(V.ResidenceCapacity);
	Value(V.PurchasingPowerBasisPoints);
	Value(V.ServiceAccessBasisPoints);
	Value(V.ServiceReliabilityBasisPoints);
	Value(V.bResidenceOperational);
	Value(V.bHasMarketAccess);
	Value(V.AccessBasisPoints);
	Value(V.AffordabilityBasisPoints);
	Value(V.ReliabilityBasisPoints);
	Value(V.SatisfactionBasisPoints);
	Value(V.WorkforceSupply);
	Value(V.ConsecutiveGrowthTicks);
	Value(V.ConsecutiveDeclineTicks);
	Value(V.ResidentChangeLastTick);
	Value(V.Needs);
    if (FormatVersion >= 5) Value(V.ConsumptionHistory);
}

void Value(FHansaPopulationNeedState& V)
{
	Value(V.NeedId);
	Value(V.GoodId);
	Value(V.RequiredLastTick);
	Value(V.ConsumedLastTick);
	Value(V.AccessBasisPoints);
	Value(V.AffordabilityBasisPoints);
	Value(V.ReliabilityBasisPoints);
	Value(V.SatisfactionBasisPoints);
	Value(V.ReserveMilliDays);
}

void Value(FHansaMarketSettings& V)
{
	Value(V.UpdateCadenceTicks);
	Value(V.PriceHistoryCapacity);
	Value(V.TargetSmoothingBasisPoints);
	Value(V.MaximumMovementBasisPointsPerUpdate);
	Value(V.StaleAfterTicks);
}

void Value(FHansaCityMarketState& V)
{
	Value(V.CityId);
	Value(V.GoodId);
	Value(V.InventoryIds);
	Value(V.DesiredReserve);
	Value(V.ConfirmedIncomingSupplyPerUpdate);
	Value(V.bMarketOnly);
	Value(V.BackgroundProductionPerUpdate);
	Value(V.BackgroundCitizenDemandPerUpdate);
	Value(V.BackgroundIndustrialDemandPerUpdate);
	Value(V.ReportPolicy);
	Value(V.SeasonModifierBasisPoints);
	Value(V.CityModifierBasisPoints);
	Value(V.MinimumPriceMilliMarks);
	Value(V.MaximumPriceMilliMarks);
	Value(V.CurrentPriceMilliMarks);
	Value(V.LastUpdateTick);
	Value(V.CurrentStock);
	Value(V.CitizenDemand);
	Value(V.IndustrialDemand);
	Value(V.RecentLocalProduction);
	Value(V.AccumulatedLocalProductionSinceUpdate);
	Value(V.ExpectedIncomingSupply);
	Value(V.UnmetDemand);
	Value(V.MinimumConsumerAffordabilityBasisPoints);
	Value(V.ShortageSinceTick);
	Value(V.LowReserveSinceTick);
	Value(V.AffordabilitySinceTick);
	Value(V.Factors);
	Value(V.PriceHistory);
	Value(V.Report);
}

void Value(FHansaMarketReportPolicy& V)
{
	Value(V.ReportCadenceTicks);
	Value(V.CurrentMaxAgeTicks);
	Value(V.RecentMaxAgeTicks);
	Value(V.StaleMaxAgeTicks);
	Value(V.EstimatedMaxAgeTicks);
}

void Value(FHansaMarketPriceFactors& V)
{
	Value(V.ScarcityBasisPoints);
	Value(V.CitizenDemandBasisPoints);
	Value(V.IndustrialDemandBasisPoints);
	Value(V.IncomingSupplyBasisPoints);
	Value(V.UnmetDemandBasisPoints);
	Value(V.SeasonModifierBasisPoints);
	Value(V.CityModifierBasisPoints);
	Value(V.TargetMultiplierBasisPoints);
}

void Value(FHansaMarketPriceHistoryEntry& V)
{
	Value(V.Tick);
	Value(V.Stock);
	Value(V.CitizenDemand);
	Value(V.IndustrialDemand);
	Value(V.LocalProduction);
	Value(V.ExpectedIncomingSupply);
	Value(V.UnmetDemand);
	Value(V.MinimumConsumerAffordabilityBasisPoints);
	Value(V.PriceMilliMarks);
}

void Value(FHansaMarketReportState& V)
{
	Value(V.bAvailable);
	Value(V.ReportTick);
	Value(V.MarketUpdateTick);
	Value(V.Stock);
	Value(V.DesiredReserve);
	Value(V.CitizenDemand);
	Value(V.IndustrialDemand);
	Value(V.RecentLocalProduction);
	Value(V.ExpectedIncomingSupply);
	Value(V.UnmetDemand);
	Value(V.PriceMilliMarks);
	Value(V.Factors);
	Value(V.PriceHistory);
}

void Value(FHansaLocalLogisticsSettings& V)
{
	Value(V.JobCapacity);
	Value(V.PickupDelayTicks);
	Value(V.TicksPerRoadCell);
	Value(V.MaximumConcurrentJobs);
}

void Value(FHansaLogisticsRequestState& V)
{
	Value(V.Id);
	Value(V.SourceInventoryId);
	Value(V.DestinationInventoryId);
	Value(V.GoodId);
	Value(V.RequestedQuantity);
	Value(V.RemainingQuantity);
	Value(V.InFlightQuantity);
	Value(V.Priority);
	Value(V.Status);
	Value(V.Bottleneck);
	Value(V.CreatedTick);
}

void Value(FHansaLogisticsJobState& V)
{
	Value(V.Id);
	Value(V.RequestId);
	Value(V.SourceReservationId);
	Value(V.SourceInventoryId);
	Value(V.DestinationInventoryId);
	Value(V.GoodId);
	Value(V.Quantity);
	Value(V.CargoQuantity);
	Value(V.DispatchTick);
	Value(V.PickupTick);
	Value(V.DeliveryTick);
	Value(V.RoadDistanceCells);
	Value(V.Status);
	if (FormatVersion >= 6)
	{
		Value(V.SelectedMarketBuildingId);
		Value(V.RouteCells);
		Value(V.ElapsedTravelTicks);
		Value(V.RemainingTravelTicks);
		Value(V.PauseReason);
	}
}

void Value(FHansaSavePlayerOwnership& V)
{
	Value(V.PrincipalId);
	Value(V.HouseId);
}

void Value(FHansaSaveVictoryStreak& V)
{
	Value(V.VictoryId);
	Value(V.ConsecutiveSatisfiedTicks);
}

void Value(FHansaSaveScenarioState& V)
{
	Value(V.HouseId);
	Value(V.ScenarioId);
	Value(V.Outcome);
	Value(V.WinningVictoryId);
	Value(V.ConsecutiveFailureTicks);
	Value(V.LastEvaluatedTick);
	Value(V.VictoryStreaks);
}

void Value(FHansaCommandHeader& V)
{
	Value(V.CommandId);
	Value(V.Authority);
	Value(V.RequestedExecutionTick);
	Value(V.GlobalSequence);
	Value(V.SchemaVersion);
}

void Value(FHansaCommandAuthorityContext& V)
{
	Value(V.IssuingHouseId);
	Value(V.PrincipalId);
	Value(V.Origin);
}

void Value(FHansaCreateTestEntityCommand& V)
{
	Value(V.EntityId);
	Value(V.InitialValue);
}

void Value(FHansaCancelTestEntityCommand& V)
{
	Value(V.EntityId);
}

void Value(FHansaNoOpTestCommand& V)
{
	Value(V.CorrelationValue);
}

void Value(FHansaSetProductionActiveCommand& V)
{
	Value(V.ProductionId);
	Value(V.bActive);
}

void Value(FHansaPlaceBuildingCommand& V)
{
	Value(V.BuildingId);
	Value(V.Placement);
}

void Value(FHansaCancelConstructionCommand& V)
{
	Value(V.BuildingId);
}

void Value(FHansaRemoveBuildingCommand& V)
{
	Value(V.BuildingId);
}

void Value(FHansaUpgradeResidenceCommand& V)
{
	Value(V.BuildingId);
}

void Value(FHansaCreateRouteCommand& V)
{
	Value(V.RouteId);
	Value(V.VehicleId);
	Value(V.RouteDefinitionId);
	Value(V.Stops);
	Value(V.bActivate);
}

void Value(FHansaEditRouteCommand& V)
{
	Value(V.RouteId);
	Value(V.Stops);
}

void Value(FHansaSetRouteActiveCommand& V)
{
	Value(V.RouteId);
	Value(V.bActive);
}

void Value(FHansaCancelRouteCommand& V)
{
	Value(V.RouteId);
}

void Value(FHansaQueueResearchCommand& V)
{
	Value(V.TechnologyId);
}
