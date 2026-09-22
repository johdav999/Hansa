// Inside MarketClearing, before prices/reports observe this update's physical stock.
if (Candidate.MarketSettings.UpdateCadenceTicks > 0 && Candidate.Clock.GetTick().GetValue() % Candidate.MarketSettings.UpdateCadenceTicks == 0)
{
 const int64 Tick = Candidate.Clock.GetTick().GetValue();
 Candidate.TradeStations.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
 for (auto& Station : Candidate.TradeStations)
 {
  Station.Orders.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
  auto* House=Candidate.Houses.FindByPredicate([&](const auto& H){return H.Id==Station.OwnerId;});
  const auto* Presence=Candidate.ForeignPresences.FindByPredicate([&](const auto& P){return P.HouseId==Station.OwnerId&&P.CityId==Station.CityId;});
  const auto* Policy=Registry->FindCityTradePolicyForCity(Station.CityId.ToString());
  for (auto& O : Station.Orders)
  {
   if (O.bCancelled || Tick < O.NextUpdateTick) continue;
   const auto Next=FHansaCheckedIntegerMath::TryAdd(Tick,Candidate.MarketSettings.UpdateCadenceTicks);
   O.NextUpdateTick=Next?Next.Value:MAX_int64;
   FHansaStationOrderExecution E; E.Tick=Tick;
   const auto Record=[&]() {
    if(O.History.Num()>=16) O.History.RemoveAt(0);
    O.History.Add(E);
    FHansaDomainEvent Result; Result.Type=EHansaDomainEventType::StationOrderExecuted;
    Result.GlobalSequence=++Candidate.PublishedDomainEventCount; Result.Tick=Candidate.Clock.GetTick();
    Result.SourceCommandId=O.LastCommandId; Result.IssuingHouseId=Station.OwnerId; Result.TradeStationId=Station.Id;
    Result.StationOrderId=O.Id; Result.CityId=Station.CityId; Result.GoodId=O.Terms.GoodId; Result.Value=E.AppliedMilliUnits; Result.RelatedValue=E.MoneyDelta;
    PendingEvents.Add(MoveTemp(Result));
   };
   if(O.bPaused) { E.Outcome=EHansaStationOrderOutcome::Paused; Record(); continue; }
   if(!House || !Policy || !Presence || Station.Status!=EHansaTradeStationStatus::Active || Presence->Status!=EHansaForeignPresenceStatus::Active ||
    !Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.StationOrders")) || Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.StationOrders")))
   { E.Outcome=EHansaStationOrderOutcome::Suspended; E.Blocker=EHansaStationOrderBlocker::Access; Record(); continue; }
   const auto* Market=Candidate.Markets.FindByPredicate([&](const auto& M){return M.CityId==Station.CityId && M.GoodId==O.Terms.GoodId;});
   if(!Market || !Market->Report.bAvailable || Market->CurrentPriceMilliMarks<=0) { E.Outcome=EHansaStationOrderOutcome::Blocked; E.Blocker=EHansaStationOrderBlocker::MarketUnavailable; Record(); continue; }
   E.MarketUpdateTick=Market->LastUpdateTick;
   const bool Buy=O.Terms.Side==EHansaStationOrderSide::Acquire;
   const int32 Reduction=FHansaResearchEffectResolver::GetBasisPoints(Candidate.Research,Station.OwnerId,EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints,Station.CityId.ToString());
   const int32 Friction=FMath::Clamp(500-Reduction,0,10000);
   const auto Price=FHansaCheckedIntegerMath::TryMultiplyDivide(Market->CurrentPriceMilliMarks,Buy?10000+Friction:10000-Friction,10000,EHansaRoundingMode::HalfAwayFromZero);
   if(!Price || Price.Value<=0) { E.Outcome=EHansaStationOrderOutcome::Blocked; E.Blocker=EHansaStationOrderBlocker::Arithmetic; Record(); continue; }
   E.UnitPriceMilliMarks=Price.Value;
   if(O.Terms.LimitUnitPriceMilliMarks>0&&((Buy&&Price.Value>O.Terms.LimitUnitPriceMilliMarks)||(!Buy&&Price.Value<O.Terms.LimitUnitPriceMilliMarks))){E.Outcome=EHansaStationOrderOutcome::Blocked;E.Blocker=EHansaStationOrderBlocker::PriceLimit;Record();continue;}
   const auto Access=Candidate.InventoryLedger.CreateReadOnlyAccess();
   const auto Storage=Access.QueryInventory(Station.InventoryId); const auto Stock=Access.QueryStock(Station.InventoryId,O.Terms.GoodId);
   if(!Storage) return MakeFailure(EHansaCommandGatewayError::TradeStationStateInvalid);
   const int64 Raw=Stock?Stock->Stock.GetRawValue():0;
   int64 Reserve=O.Terms.TargetOrReserveMilliUnits;
   // Every live sale reserve protects this good, including paused orders.
   for(const auto& Other:Station.Orders) if(!Other.bCancelled && Other.Terms.Side==EHansaStationOrderSide::Release && Other.Terms.GoodId==O.Terms.GoodId)
    Reserve=FMath::Max(Reserve,Other.Terms.TargetOrReserveMilliUnits);
   E.RequestedMilliUnits=FMath::Min(O.Terms.CapMilliUnits,Buy?FMath::Max<int64>(0,O.Terms.TargetOrReserveMilliUnits-Raw):FMath::Max<int64>(0,Raw-Reserve));
   if(!E.RequestedMilliUnits) { E.Outcome=EHansaStationOrderOutcome::Completed; Record(); continue; }
   int64 Amount=E.RequestedMilliUnits;
   const auto Bound=[&](int64 Limit,EHansaStationOrderBlocker Why){if(Limit<Amount){Amount=FMath::Max<int64>(0,Limit);E.Blocker=Why;}};
   if(Buy) Bound(Storage->FreeCapacity.GetRawValue(),EHansaStationOrderBlocker::StationCapacity);
   else Bound(Stock?FMath::Max<int64>(0,Stock->Available.GetRawValue()-FMath::Max(Reserve,Access.QueryProtectedRaw(Station.InventoryId,O.Terms.GoodId))):0,EHansaStationOrderBlocker::ReservedStock);
   TArray<FHansaInventoryId> MarketInventories=Market->InventoryIds; MarketInventories.Sort();
   int64 Available=0;
   for(const auto Id:MarketInventories) {
    const auto Inv=Access.QueryInventory(Id); const auto S=Access.QueryStock(Id,O.Terms.GoodId);
    if(!Inv || !Inv->AcceptedGoods.Contains(O.Terms.GoodId) || Id==Station.InventoryId) continue;
    const int64 Value=Buy?(S?FMath::Max<int64>(0,S->Available.GetRawValue()-Access.QueryProtectedRaw(Id,O.Terms.GoodId)):0):Inv->FreeCapacity.GetRawValue();
    Available+=FMath::Min(FMath::Max<int64>(0,Value),Amount-Available); if(Available>=Amount) break;
   }
   Bound(Available,Buy?EHansaStationOrderBlocker::MarketStock:EHansaStationOrderBlocker::MarketCapacity);
   if(Buy) {
    const auto Affordable=FHansaCheckedIntegerMath::TryMultiplyDivide(FMath::Max<int64>(0,House->Money.GetRawValue()),1000,Price.Value,EHansaRoundingMode::TowardZero);
    const auto Budget=FHansaCheckedIntegerMath::TryMultiplyDivide(O.Terms.TotalBudgetPfennig-O.SpentPfennig,1000,Price.Value,EHansaRoundingMode::TowardZero);
    if(!Affordable||!Budget){E.Outcome=EHansaStationOrderOutcome::Blocked;E.Blocker=EHansaStationOrderBlocker::Arithmetic;Record();continue;}
    Bound(Affordable.Value,EHansaStationOrderBlocker::Funds); Bound(Budget.Value,EHansaStationOrderBlocker::Budget);
   }
   const auto Money=FHansaCheckedIntegerMath::TryMultiplyDivide(Amount,Price.Value,1000,Buy?EHansaRoundingMode::Ceiling:EHansaRoundingMode::TowardZero);
   const auto After=Money?FHansaCheckedIntegerMath::TryAdd(House->Money.GetRawValue(),Buy?-Money.Value:Money.Value):THansaValueResult<int64>::Failure(EHansaValueError::OutOfRange);
   if(!Money||!After||After.Value<0){E.Outcome=EHansaStationOrderOutcome::Blocked;E.Blocker=EHansaStationOrderBlocker::Arithmetic;Record();continue;}
   // Candidate ledger is committed only after every transfer succeeds.
   auto Ledger=Candidate.InventoryLedger; int64 Applied=0;
   const uint64 OpeningSequence=Ledger.CreateReadOnlyAccess().GetLastMovementSequence();
   bool bValid=true;
   for(const auto Id:MarketInventories) {
    if(Applied>=Amount) break;
    const auto A=Ledger.CreateReadOnlyAccess(); const auto I=A.QueryInventory(Id); const auto S=A.QueryStock(Id,O.Terms.GoodId);
    if(!I||!I->AcceptedGoods.Contains(O.Terms.GoodId)||Id==Station.InventoryId)continue;
    const int64 AvailableRaw=Buy?(S?FMath::Max<int64>(0,S->Available.GetRawValue()-A.QueryProtectedRaw(Id,O.Terms.GoodId)):0):I->FreeCapacity.GetRawValue();
    const int64 Q=FMath::Min(Amount-Applied,AvailableRaw); if(Q<=0)continue;
    const auto Tx=Ledger.TryTransfer(FHansaInventoryEndpoint::Inventory(Buy?Id:Station.InventoryId),FHansaInventoryEndpoint::Inventory(Buy?Station.InventoryId:Id),O.Terms.GoodId,FHansaQuantity::FromRaw(Q),Candidate.Clock.GetTick(),A.GetLastMovementSequence()+1);
    if(!Tx.IsSuccess()){bValid=false;break;} Applied+=Tx.AppliedQuantity.GetRawValue();
   }
   if(!bValid||Applied!=Amount){E.Outcome=EHansaStationOrderOutcome::Blocked;E.Blocker=EHansaStationOrderBlocker::ReservedStock;Record();continue;}
   Candidate.InventoryLedger=MoveTemp(Ledger); House->Money=FHansaMoney::FromRaw(After.Value);
   if(Buy) O.SpentPfennig+=Money.Value;
   E.AppliedMilliUnits=Amount; E.MoneyDelta=Buy?-Money.Value:Money.Value;
   if(Amount){E.FirstMovementSequence=OpeningSequence+1;E.LastMovementSequence=Candidate.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence();}
   E.Outcome=Amount==E.RequestedMilliUnits?EHansaStationOrderOutcome::Filled:Amount?EHansaStationOrderOutcome::Partial:EHansaStationOrderOutcome::Blocked;
   Record();
  }
 }
}
