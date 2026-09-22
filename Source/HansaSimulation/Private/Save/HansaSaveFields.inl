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
	if (FormatVersion >= 12) Value(V.ForeignPresences);
	if (FormatVersion >= 14) { Value(V.TradeStations); Value(V.LeasedPlots); }
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
	if (FormatVersion >= 11) { Value(V.NextRegionalShipmentSequence); Value(V.RemoteIndustries); Value(V.RegionalShipments); }
}

void Value(FHansaRemoteIndustryState& V)
{
	Value(V.CityId); Value(V.ProductionChainId); Value(V.StageKey); Value(V.RecipeId); Value(V.CompletedCycles);
	Value(V.Blocker); Value(V.BlockingGoodId); Value(V.BlockingRequired); Value(V.BlockingAvailable); Value(V.LastProduced); Value(V.LastUpdateTick);
}

void Value(FHansaRegionalShipmentState& V)
{
	Value(V.Sequence); Value(V.RegionId); Value(V.SourceCityId); Value(V.DestinationCityId); Value(V.GoodId);
	Value(V.CommittedQuantity); Value(V.DeliverableQuantity); Value(V.DispatchTick); Value(V.DeliveryTick); Value(V.TransportCostMilliMarks);
}

void Value(FHansaConsumptionHistory& V) { Value(V.Samples); }
void Value(FHansaConsumptionSample& V) { Value(V.EndTick); Value(V.Goods); }
void Value(FHansaConsumptionTotal& V)
{
    Value(V.CityId); Value(V.GoodId); Value(V.Required); Value(V.Consumed);
    if (FormatVersion >= 9) Value(V.SuppliedGoods);
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
	if (FormatVersion >= 8) { Value(V.HeatingReserveDays); Value(V.bReleaseHeatingReserve); }
    if (FormatVersion >= 9) Value(V.bPreservedFishHouseholdAvailable);
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
	if (FormatVersion >= 13) Value(V.LastSpotTrade);
    if (FormatVersion >= 10)
    {
        Value(V.Navigation.CityId); Value(V.Navigation.Home); Value(V.Navigation.Cell);
        Value(V.Navigation.Path); Value(V.Navigation.NextIndex);
    }
}

void Value(FHansaSpotTradeRecord& V)
{
	Value(V.CommandId); Value(V.Tick); Value(V.HouseId); Value(V.VehicleId); Value(V.CityId); Value(V.GoodId);
	Value(V.Side); Value(V.RequestedQuantity); Value(V.AppliedQuantity); Value(V.Outcome); Value(V.Blocker);
	Value(V.MarketUpdateTick); Value(V.UnitPriceMilliMarks); Value(V.SettledMoneyRaw);
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
    if (FormatVersion >= 9) { Value(V.UnitPriceMilliMarks); Value(V.SettledMoneyRaw); }
}

void Value(FHansaForeignPresenceContributions& V)
{
	Value(V.LawfulTradeVolumeMilliUnits); Value(V.CompletedDeliveryCount); Value(V.InvestedPfennig);
	if (FormatVersion >= 17) { Value(V.TransactionValuePfennig); Value(V.FulfilledShortageMilliUnits); Value(V.ReliableOperatingTicks); Value(V.SolventOperatingTicks); }
}
void Value(FHansaPresenceHistoryEntry& V) { Value(V.Kind); Value(V.Tick); Value(V.SourceEventSequence); Value(V.StageId); Value(V.QuantityMilliUnits); Value(V.MoneyPfennig); }
void Value(FHansaCityPrivilegeState& V){Value(V.PrivilegeId);Value(V.GrantedLeaseId);Value(V.Status);Value(V.GrantedTick);Value(V.ExpiryTick);Value(V.SpentMoneyPfennig);}
void Value(FHansaCityProjectState& V){Value(V.ProjectId);Value(V.Status);Value(V.FundedTick);Value(V.CompletionTick);Value(V.SpentMoneyPfennig);Value(V.bSharedEffectApplied);}
void Value(FHansaPresenceUpgradeState& V) { Value(V.Status); Value(V.TargetStageId); Value(V.FundingInventoryId); Value(V.RequestedTick); Value(V.FundedTick); Value(V.CompletionTick); Value(V.SpentMoneyPfennig); }

void Value(FHansaForeignPresenceState& V)
{
	Value(V.HouseId); Value(V.CityId); Value(V.CurrentStageId); Value(V.GrantedCapabilityIds); Value(V.Contributions);
	Value(V.Status); Value(V.EstablishedTick); Value(V.LastUpgradeTick); Value(V.StationId); Value(V.LeasedPlotId);
	if (FormatVersion >= 17) { Value(V.LastAcceptedContributionEventSequence); Value(V.Upgrade); Value(V.History); }
	if (FormatVersion >= 18) { Value(V.ActiveSpecializationIds); Value(V.SpecializationRevision); } if (FormatVersion >= 20) { Value(V.Privileges); Value(V.CityProjects); Value(V.bGovernanceAuthority); Value(V.GovernanceCharterId); Value(V.GovernanceGrantedTick); Value(V.AuthorityRevision); }
}
void Value(FHansaTradeStationSpentGood& V) { Value(V.GoodId); Value(V.Quantity); }
void Value(FHansaTradeStationState& V)
{
	Value(V.Id); Value(V.OwnerId); Value(V.CityId); Value(V.SiteId); Value(V.InventoryId); Value(V.FactorId); Value(V.LeasedPlotId);
	Value(V.Status); Value(V.ProposedTick); Value(V.FundedTick); Value(V.CompletionTick); Value(V.CompletedTick);
	Value(V.UpkeepPfennigPerTick); Value(V.SpentMoneyRaw); Value(V.FundingInventoryId); Value(V.SpentGoods);
 if (FormatVersion >= 15) Value(V.Orders);
 if (FormatVersion >= 21) { Value(V.OperationalState); Value(V.OperationalStateChangedTick); Value(V.OutstandingUpkeepPfennig); }
}
void Value(FHansaLeasedPlotState& V)
{
	Value(V.Id); Value(V.StationId); Value(V.OwnerId); Value(V.CityId); Value(V.SiteId); Value(V.PlotCategory);
	if (FormatVersion >= 19) { Value(V.BoundsMin); Value(V.BoundsMax); Value(V.PermittedBuildingCategories); Value(V.OccupyingBuildingIds); }
	Value(V.bActive); Value(V.bOccupied);
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

void Value(FHansaSpoilageRecord& V) { Value(V.GoodId); Value(V.RemainderNumerator); Value(V.DestroyedMilliUnits); }

void Value(FHansaInventoryLedger& V)
{
	Value(V.bInitialized);
	Value(V.MovementCapacity);
	Value(V.LastMovementSequence);
	Value(V.Inventories);
	Value(V.Reservations);
	Value(V.RecentMovements);
    if (FormatVersion >= 9) Value(V.Spoilage);
}

void Value(FHansaInventoryRecord& V)
{
	Value(V.Id);
	Value(V.OwnerKind);
	Value(V.CityId);
	Value(V.BuildingId);
	Value(V.VehicleId);
	if (FormatVersion >= 14) Value(V.TradeStationId);
	Value(V.Capacity);
	Value(V.AcceptedGoods);
	Value(V.Stocks);
	if (FormatVersion >= 9) Value(V.HouseholdExcludedGoods);
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

void Value(FHansaProductionGoodTotal& V) { Value(V.GoodId); Value(V.QuantityMilliUnits); }

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
    if (FormatVersion >= 9) { Value(V.RequestedRecipeId); Value(V.bFallbackToFresh); Value(V.PendingUpgradeBuildingId); Value(V.OutputTotals); }
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

void Value(FHansaNeedSupply& V)
{ Value(V.GoodId); Value(V.QuantityMilliUnits); Value(V.FulfillmentMilliUnits); }

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
	if (FormatVersion >= 9) Value(V.SuppliedGoods);
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

void Value(FHansaSetHeatingReserveCommand& V) { Value(V.MarketBuildingId); Value(V.ReserveDays); Value(V.bReleaseProtection); }

void Value(FHansaSetProductionModeCommand& V) { Value(V.ProductionId); Value(V.RecipeId); Value(V.bFallbackToFresh); }
void Value(FHansaUpgradeProductionCommand& V) { Value(V.ProductionId); }
void Value(FHansaSetHouseholdAvailabilityCommand& V) { Value(V.MarketBuildingId); Value(V.GoodId); Value(V.bAvailable); }

void Value(FHansaMoveShipCommand& V) { Value(V.VehicleId); Value(V.Target); }
void Value(FHansaSpotTradeCommand& V) { Value(V.VehicleId); Value(V.CityId); Value(V.GoodId); Value(V.Side); Value(V.Quantity); Value(V.ReviewedMarketUpdateTick); Value(V.ReviewedUnitPriceMilliMarks); }
void Value(FHansaProposeTradeStationCommand& V) { Value(V.StationId); Value(V.FactorId); Value(V.LeasedPlotId); Value(V.InventoryId); Value(V.CityId); Value(V.SiteId); }
void Value(FHansaFundTradeStationCommand& V) { Value(V.StationId); Value(V.FundingInventoryId); }
void Value(FHansaCloseTradeStationCommand& V) { Value(V.StationId); }

void Value(FHansaStationOrderTerms& V) { Value(V.GoodId); Value(V.Side); Value(V.TargetOrReserveMilliUnits); Value(V.CapMilliUnits); Value(V.TotalBudgetPfennig); if (FormatVersion >= 18) { Value(V.LimitUnitPriceMilliMarks); Value(V.ReviewedMarketUpdateTick); Value(V.ReviewedUnitPriceMilliMarks); } }
void Value(FHansaStationOrderExecution& V) { Value(V.Tick); Value(V.MarketUpdateTick); Value(V.RequestedMilliUnits); Value(V.AppliedMilliUnits); Value(V.UnitPriceMilliMarks); Value(V.MoneyDelta); Value(V.FirstMovementSequence); Value(V.LastMovementSequence); Value(V.Outcome); Value(V.Blocker); }
void Value(FHansaStationOrderState& V) { Value(V.Id); Value(V.LastCommandId); Value(V.Terms); Value(V.bPaused); Value(V.bCancelled); Value(V.SpentPfennig); Value(V.NextUpdateTick); Value(V.History); }
void Value(FHansaManageStationOrderCommand& V) { Value(V.StationId); Value(V.OrderId); Value(V.Action); Value(V.Terms); }
void Value(FHansaRequestPresenceUpgradeCommand& V) { Value(V.CityId); Value(V.TargetStageId); }
void Value(FHansaFundPresenceUpgradeCommand& V) { Value(V.CityId); Value(V.TargetStageId); Value(V.FundingInventoryId); }
void Value(FHansaApplyPresenceSpecializationCommand& V) { Value(V.CityId); Value(V.SpecializationId); Value(V.FundingInventoryId); Value(V.Action); Value(V.ReviewedRevision); }
void Value(FHansaManageCityPrivilegeCommand& V){Value(V.CityId);Value(V.PrivilegeId);Value(V.FundingInventoryId);Value(V.GrantedLeaseId);Value(V.Action);Value(V.ReviewedRevision);}
void Value(FHansaFundCityProjectCommand& V){Value(V.CityId);Value(V.ProjectId);Value(V.FundingInventoryId);Value(V.ReviewedRevision);}
void Value(FHansaTransitionCityAuthorityCommand& V){Value(V.CityId);Value(V.CharterId);Value(V.ReviewedRevision);}
