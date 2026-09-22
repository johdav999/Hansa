case EHansaGameplayCommandType::ManageStationOrder:
{
 const auto& P = Command.GetManageStationOrder();
 auto* Station = Candidate.TradeStations.FindByPredicate([&](const auto& V){return V.Id == P.StationId;});
 if (!Station) return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
 if (Station->OwnerId != Header.Authority.IssuingHouseId) return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
 if (!P.OrderId || P.Action > EHansaStationOrderAction::Cancel) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
 auto* Order = Station->Orders.FindByPredicate([&](const auto& V){return V.Id == P.OrderId;});
 const auto* Registry = Definitions.GetEconomicRegistry();
 const auto* Policy = Registry ? Registry->FindCityTradePolicyForCity(Station->CityId.ToString()) : nullptr;
 const auto* Presence = Candidate.ForeignPresences.FindByPredicate([&](const auto& V){return V.HouseId == Station->OwnerId && V.CityId == Station->CityId;});
 if (P.Action == EHansaStationOrderAction::Create || P.Action == EHansaStationOrderAction::Edit || P.Action == EHansaStationOrderAction::Resume)
 {
  if (!Policy || !Presence || Presence->Status != EHansaForeignPresenceStatus::Active || Station->Status != EHansaTradeStationStatus::Active ||
   !Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.StationOrders")) || Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.StationOrders")))
   return MakeFailure(EHansaCommandGatewayError::TradeStationAccessUnavailable, CommandIndex);
 }
 if (P.Action == EHansaStationOrderAction::Create || P.Action == EHansaStationOrderAction::Edit)
 {
  const auto Storage = Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Station->InventoryId);
  if (!P.Terms.GoodId.IsValid() || P.Terms.Side > EHansaStationOrderSide::Release || P.Terms.TargetOrReserveMilliUnits < 0 ||
   P.Terms.CapMilliUnits <= 0 || P.Terms.CapMilliUnits > Policy->MaximumOrderCapMilliUnits ||
   P.Terms.TotalBudgetPfennig < 0 || P.Terms.TotalBudgetPfennig > Policy->MaximumOrderBudgetPfennig ||
   !Storage || P.Terms.TargetOrReserveMilliUnits > Storage->Capacity.GetRawValue() || !Storage->AcceptedGoods.Contains(P.Terms.GoodId) ||
   !Candidate.Markets.ContainsByPredicate([&](const auto& M){return M.CityId == Station->CityId && M.GoodId == P.Terms.GoodId && M.Report.bAvailable;}) || P.Terms.LimitUnitPriceMilliMarks < 0)
   return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
  if(P.Terms.LimitUnitPriceMilliMarks>0)
  {
   if(!Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.MarketSpecialization"))) return MakeFailure(EHansaCommandGatewayError::TradeStationAccessUnavailable,CommandIndex);
   const auto* Market=Candidate.Markets.FindByPredicate([&](const auto& M){return M.CityId==Station->CityId&&M.GoodId==P.Terms.GoodId;});
   const int32 Reduction=FHansaResearchEffectResolver::GetBasisPoints(Candidate.Research,Station->OwnerId,EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints,Station->CityId.ToString());
   const int32 Friction=FMath::Clamp(500-Reduction,0,10000);const bool Buy=P.Terms.Side==EHansaStationOrderSide::Acquire;
   const auto Price=Market?FHansaCheckedIntegerMath::TryMultiplyDivide(Market->CurrentPriceMilliMarks,Buy?10000+Friction:10000-Friction,10000,EHansaRoundingMode::HalfAwayFromZero):THansaValueResult<int64>::Failure(EHansaValueError::OutOfRange);
   if(!Market||!Price||Price.Value<=0||P.Terms.ReviewedMarketUpdateTick!=Market->LastUpdateTick||P.Terms.ReviewedUnitPriceMilliMarks!=Price.Value)
    return MakeFailure(EHansaCommandGatewayError::StationOrderStaleReview,CommandIndex);
  }
  else if(P.Terms.ReviewedMarketUpdateTick!=-1||P.Terms.ReviewedUnitPriceMilliMarks!=0) return MakeFailure(EHansaCommandGatewayError::InvalidPayload,CommandIndex);
  if (P.Action == EHansaStationOrderAction::Create)
  {
   if (Order) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
   int32 ActiveCount = 0; for (const auto& O : Station->Orders) if (!O.bCancelled) ++ActiveCount;
   const bool bMerchantOffice=Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.MerchantOffice"));
   int32 SpecializationSlots=0;for(const auto& B:Policy->Specializations)if(Presence->ActiveSpecializationIds.Contains(B.SpecializationId))SpecializationSlots+=B.AdditionalOrderSlots;
   const int32 EffectiveLimit=FMath::Min(64,Policy->MaximumStationOrders+(bMerchantOffice?Policy->MerchantOfficeAdditionalOrderSlots:0)+SpecializationSlots);
   if (ActiveCount >= EffectiveLimit || Station->Orders.Num() >= 4096) return MakeFailure(EHansaCommandGatewayError::TradeStationStateInvalid, CommandIndex);
   FHansaStationOrderState NewOrder; NewOrder.Id=P.OrderId; NewOrder.Terms=P.Terms; NewOrder.LastCommandId=Header.CommandId;
   NewOrder.NextUpdateTick=TickBefore.GetValue(); Station->Orders.Add(NewOrder);
   Station->Orders.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
   Order=Station->Orders.FindByPredicate([&](const auto& O){return O.Id==P.OrderId;});
  }
  else
  {
   if (!Order || Order->bCancelled || P.Terms.TotalBudgetPfennig < Order->SpentPfennig) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
   // Retain good and direction so history and lifetime budget keep their meaning.
   if (P.Terms.GoodId != Order->Terms.GoodId || P.Terms.Side != Order->Terms.Side) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
   Order->Terms=P.Terms;
  }
 }
 else
 {
  if (!Order || Order->bCancelled) return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
  if(P.Action == EHansaStationOrderAction::Cancel) Order->bCancelled=true;
  else Order->bPaused=P.Action == EHansaStationOrderAction::Pause;
 }
 Order->LastCommandId=Header.CommandId;
 Event.Type=EHansaDomainEventType::StationOrderChanged; Event.TradeStationId=Station->Id; Event.CityId=Station->CityId;
 Event.StationOrderId=Order->Id; Event.GoodId=Order->Terms.GoodId;
 break;
}
