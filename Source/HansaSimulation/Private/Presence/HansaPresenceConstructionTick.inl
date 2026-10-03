// Shared physical-material escrow for placed stations and later presence upgrades.
// Only the explicitly assigned inventory is eligible. Ships must be at this city;
// no source endpoint, market stock, planned load or cargo at sea creates materials.
auto SecurePresenceMaterials=[&](FHansaHouseId Owner,FHansaCityDefinitionId City,FHansaInventoryId Source,
 TConstArrayView<FHansaCompiledPresenceUpgradeGoodCost> Costs,TArray<FHansaTradeStationSpentGood>& Escrow)
{
 const auto Inventory=Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Source);
 bool Local=false;
 if(Inventory){
  if(Inventory->OwnerKind==EHansaInventoryOwnerKind::Vehicle){
   const auto* V=Candidate.Vehicles.FindByPredicate([&](const auto& V){return V.CargoInventoryId==Source&&V.OwnerId==Owner;});
   Local=V&&V->CurrentCityId==City&&!V->Navigation.IsMoving()&&(!V->Navigation.CityId.IsValid()||V->Navigation.IsAtHome());
   if(Local)for(const auto& R:Candidate.Routes)if(R.VehicleId==V->Id&&R.Lifecycle==EHansaRouteLifecycleState::Traveling)Local=false;
  }else if(Inventory->CityId==City){
   Local=Candidate.TradeStations.ContainsByPredicate([&](const auto& S){return S.InventoryId==Source&&S.OwnerId==Owner;})||
    Candidate.Buildings.ContainsByPredicate([&](const auto& B){return B.Id==Inventory->BuildingId&&B.OwnerId==Owner;});
  }
 }
 bool Complete=true;
 for(const auto& Cost:Costs){
  const auto Good=FHansaGoodId::TryParse(Cost.GoodId);if(!Good){Complete=false;continue;}
  auto* Delivered=Escrow.FindByPredicate([&](const auto& G){return G.GoodId==Good.Value;});
  const int64 Missing=FMath::Max<int64>(0,Cost.QuantityMilliUnits-(Delivered?Delivered->Quantity.GetRawValue():0));
  const auto Stock=Local?Candidate.InventoryLedger.CreateReadOnlyAccess().QueryStock(Source,Good.Value):TOptional<FHansaInventoryStockProjection>();
  const int64 Raw=Stock?FMath::Min(Missing,Stock->Available.GetRawValue()):0;
  if(Raw>0&&Candidate.InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Inventory(Source),FHansaInventoryEndpoint::Sink(TEXT("PresenceConstructionEscrow")),Good.Value,FHansaQuantity::FromRaw(Raw),Candidate.Clock.GetTick(),Candidate.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess()){
   if(Delivered)Delivered->Quantity=FHansaQuantity::FromRaw(Delivered->Quantity.GetRawValue()+Raw);else Escrow.Add({Good.Value,FHansaQuantity::FromRaw(Raw)});
  }
  Delivered=Escrow.FindByPredicate([&](const auto& G){return G.GoodId==Good.Value;});
  Complete&=Delivered&&Delivered->Quantity.GetRawValue()>=Cost.QuantityMilliUnits;
 }Escrow.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;});return Complete;
};
for(auto& S:Candidate.TradeStations){
 if(!S.DeliveryReservations.IsEmpty()){const auto Ledger=Candidate.InventoryLedger.CreateReadOnlyAccess().CaptureSnapshot();S.DeliveryReservations.RemoveAll([&](const auto Id){return !Ledger.GetReservations().ContainsByPredicate([&](const auto& R){return R.Id==Id;});});}
 if(!S.ConstructionSite.bLocalDelivery||S.Status!=EHansaTradeStationStatus::Proposed)continue;
 const auto* P=Candidate.ForeignPresences.FindByPredicate([&](const auto& P){return P.HouseId==S.OwnerId&&P.CityId==S.CityId&&P.Status==EHansaForeignPresenceStatus::Active;});
 const auto* Policy=Registry->FindCityTradePolicyForCity(S.CityId.ToString());
 const auto* Site=Policy?Policy->TradeStationSites.FindByPredicate([&](const auto& V){return V.SiteId==S.SiteId;}):nullptr;
 const auto* Stage=P?Registry->GetPresenceStages().FindByPredicate([&](const auto& V){return V.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"))&&Registry->IsValidPresenceTransition(S.CityId.ToString(),P->CurrentStageId,V.StableId);}):nullptr;
 if(!Site||!Stage)continue;
 if(S.DeliveryMode){
  const auto* V=Candidate.Vehicles.FindByPredicate([&](const auto& V){return V.CargoInventoryId==S.FundingInventoryId&&V.OwnerId==S.OwnerId;});
  const bool Traveling=V&&Candidate.Routes.ContainsByPredicate([&](const auto& R){return R.VehicleId==V->Id&&R.Lifecycle==EHansaRouteLifecycleState::Traveling;});
  if(V&&V->CurrentCityId==S.CityId&&!V->Navigation.IsMoving()&&!Traveling)FHansaConstructionDelivery::Collect(S,Candidate.InventoryLedger,Stage->UpgradeGoods,Candidate.Clock.GetTick(),true,{});
 }
 if(!SecurePresenceMaterials(S.OwnerId,S.CityId,S.FundingInventoryId,Stage->UpgradeGoods,S.SpentGoods))continue;
 const auto End=FHansaSimulationTick::TryCreate(Candidate.Clock.GetTick().GetValue()+Site->ConstructionTicks);if(!End)continue;
 S.DeliveryMode=0;S.Status=EHansaTradeStationStatus::UnderConstruction;S.FundedTick=Candidate.Clock.GetTick();S.CompletionTick=End.Value;
}
for(auto& P:Candidate.ForeignPresences){
 auto& U=P.Upgrade;if(U.Status!=EHansaPresenceUpgradeStatus::AwaitingMaterials||!U.ConstructionSite.bLocalDelivery||P.Status!=EHansaForeignPresenceStatus::Active)continue;
 const auto* Stage=Registry->FindPresenceStage(U.TargetStageId);
 auto* Station=Candidate.TradeStations.FindByPredicate([&](const auto& S){return S.Id==P.StationId&&S.OwnerId==P.HouseId;});
 if(Station&&Station->DeliveryMode&&Stage){
  const auto* V=Candidate.Vehicles.FindByPredicate([&](const auto& V){return V.CargoInventoryId==U.FundingInventoryId&&V.OwnerId==P.HouseId;});
  const bool Traveling=V&&Candidate.Routes.ContainsByPredicate([&](const auto& R){return R.VehicleId==V->Id&&R.Lifecycle==EHansaRouteLifecycleState::Traveling;});
  if(V&&V->CurrentCityId==P.CityId&&!V->Navigation.IsMoving()&&!Traveling)FHansaConstructionDelivery::Collect(U.FundingInventoryId,U.DeliveredGoods,Station->DeliveryReservations,Candidate.InventoryLedger,Stage->UpgradeGoods,Candidate.Clock.GetTick(),true,{});
 }
 if(!Stage||!SecurePresenceMaterials(P.HouseId,P.CityId,U.FundingInventoryId,Stage->UpgradeGoods,U.DeliveredGoods))continue;
 const auto End=FHansaSimulationTick::TryCreate(Candidate.Clock.GetTick().GetValue()+Stage->UpgradeConstructionTicks);if(!End)continue;
 if(Station&&!FHansaConstructionDelivery::Release(*Station,Candidate.InventoryLedger,Candidate.Clock.GetTick()))continue;
 U.Status=EHansaPresenceUpgradeStatus::Funded;U.FundedTick=Candidate.Clock.GetTick();U.CompletionTick=End.Value;
}
