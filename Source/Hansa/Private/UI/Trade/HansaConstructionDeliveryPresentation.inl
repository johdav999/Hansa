// Included by the existing establishment presenter; all values come from owner-scoped state.
void DescribeConstructionDelivery(FHansaEstablishmentChoice& C,const FHansaInventoryProjection& Inv,
 const FHansaTradeStationState& Station,const FHansaRouteProjection* Route,
 const FHansaCompiledForeignPresenceStageDefinition* Stage,const FHansaSimulationProjection& P,
 const FHansaEconomicRegistry& Registry,int32 MinutesPerTick,bool PreviewPriority=false)
{
 const auto* Ship=P.GetVehicles().FindByPredicate([&](const auto& V){return V.CargoInventoryId==Inv.Id&&V.OwnerId==Station.OwnerId&&V.Mode==EHansaRouteMode::Sea;});
 const auto* Home=Route?Route->Stops.FindByPredicate([&](const auto& Stop){const auto* City=Registry.FindCityMarket(Stop.CityId.ToString());return City&&!City->bMarketOnly;}):nullptr;
 const bool Automatic=Ship&&Home&&Route->Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==Station.CityId;});
 bool HasCargo=false;FString Materials;int32 MissingKinds=0;
 if(Stage)for(const auto& G:Stage->UpgradeGoods){
  const auto* D=Station.SpentGoods.FindByPredicate([&](const auto& G2){return G2.GoodId.ToString()==G.GoodId;});
  const auto* S=Inv.Stocks.FindByPredicate([&](const auto& G2){return G2.GoodId.ToString()==G.GoodId;});
  const int64 Missing=FMath::Max<int64>(0,G.QuantityMilliUnits-(D?D->Quantity.GetRawValue():0));
  const int64 Aboard=S?FMath::Min(Missing,S->Stock.GetRawValue()):0;HasCargo|=Aboard>0;MissingKinds+=Missing>Aboard;
  const auto* Good=Registry.FindGood(G.GoodId);
  Materials+=FString::Printf(TEXT("%s: delivered %s / %s · aboard %s units\n"),*(Good?Good->DisplayName:G.GoodId),*Units(D?D->Quantity.GetRawValue():0),*Units(G.QuantityMilliUnits),*Units(Aboard));
 }
 C.DeliveryMode=Automatic?1:0;
 C.bEligible=C.bEligible&&(Automatic||(Ship&&HasCargo)||(Inv.CityId==Station.CityId&&HasCargo));
 const bool Assigned=Station.FundingInventoryId==Inv.Id;
 const bool Prioritized=(Assigned&&Station.DeliveryMode==2)||PreviewPriority;
 C.bPriorityActive=Assigned&&Station.DeliveryMode==2;
 FString Detail=C.Label.ToString()+(Assigned?TEXT("\nThis inventory is assigned to the paid site. "):TEXT("\nThis inventory is selected for review only. Arrange delivery to use it. "))+TEXT("Construction begins after all materials arrive at the trade house.\n")+Materials;
 FString Transfer=TEXT("Deliver the required materials already aboard this source at the destination. No additional payment.");
 FString Status=TEXT("Deliver the assigned materials in the destination city.");
 if(Automatic){
  const FString City=CityName(Registry,Home->CityId.ToString());
  Transfer=FString::Printf(TEXT("Ship #%llu will collect only missing construction materials at %s and deliver them to %s. Ordinary trade loads run first. Existing cargo and route instructions are kept. Several trips are allowed. No additional construction payment.\n%s"),Ship->Id.GetValue(),*City,*CityName(Registry,Station.CityId.ToString()),*Materials);
  C.PriorityTransfer=FText::FromString(FString::Printf(TEXT("Prioritize construction using Ship #%llu at %s. Materials load before ordinary trade goods. Some usual loads may be smaller or skipped until delivery finishes. Existing cargo is kept; normal trading resumes automatically. Route instructions are unchanged.\n%s"),Ship->Id.GetValue(),*City,*Materials));
  Detail+=TEXT("Automatic one-time pickup; no product, quantity or unload instructions needed.\n");
  if(C.bPriorityActive)Detail+=TEXT("Construction priority active. Ordinary loads resume automatically after the materials arrive.\n");
  const bool Access=P.GetInventories().ContainsByPredicate([&](const auto& I){return I.OwnerKind==EHansaInventoryOwnerKind::City&&I.CityId==Home->CityId&&I.bSeaTradeAccess;});
  if(!Access&&MissingKinds>0){Status=TEXT("Delivery needs attention: connect a Market to the source dock by road.");Detail+=Status+TEXT("\n");}
  else if(Route->Lifecycle==EHansaRouteLifecycleState::Inactive){Status=TEXT("Delivery needs attention: resume the paused route or choose another ship.");Detail+=Status+TEXT("\n");}
  else if(HasCargo){
   Status=TEXT("Materials aboard · waiting for arrival at ")+CityName(Registry,Station.CityId.ToString())+TEXT(".");
   if(Route->Lifecycle==EHansaRouteLifecycleState::Traveling&&Route->Stops.IsValidIndex(Route->NextStopIndex)&&Route->Stops[Route->NextStopIndex].CityId==Station.CityId)
    Detail+=TEXT("Sailing to ")+CityName(Registry,Station.CityId.ToString())+TEXT(" · arrival in ")+PresenceDuration(Route->RemainingTravelTicks,MinutesPerTick).ToString()+TEXT(".\n");
   else Detail+=TEXT("Materials aboard; delivery occurs on the next destination visit.\n");
  }else{
   TArray<FHansaCargoSlot> Slots=Inv.CargoSlots;bool Opportunity=false;int32 Compatible=0;
   // Conditional forecast from actual cargo; planned sale success is never assumed as fact.
   for(int32 Visit=0;Visit<Route->Stops.Num()*2;++Visit){
    const auto& Stop=Route->Stops[(Route->NextStopIndex+Visit)%Route->Stops.Num()];
    for(bool Load:{false,true})for(const auto& A:Stop.Actions)if(IsRouteLoad(A.Kind)==Load&&!(Prioritized&&Load&&Stop.CityId==Home->CityId)){
     if(A.CargoSlotIndex!=INDEX_NONE)FHansaCargoPlan::Apply(Slots,A,Inv.Capacity.GetRawValue());
     else {
      // Legacy pooled actions select a matching slot, then an empty slot for loading.
      auto Planned=A;Planned.CargoSlotIndex=Slots.IndexOfByPredicate([&](const auto& Slot){return Slot.GoodId==A.GoodId;});
      if(Load&&Planned.CargoSlotIndex==INDEX_NONE)Planned.CargoSlotIndex=Slots.IndexOfByPredicate([](const auto& Slot){return !Slot.GoodId.IsValid();});
      if(Planned.CargoSlotIndex!=INDEX_NONE)FHansaCargoPlan::Apply(Slots,Planned,Inv.Capacity.GetRawValue());
     }
    }
    if(Stop.CityId!=Home->CityId)continue;
    int64 Used=0;for(const auto& Slot:Slots)Used+=Slot.Quantity.GetRawValue();if(Used>=Inv.Capacity.GetRawValue())continue;
    Compatible=0;for(const auto& Slot:Slots)if(!Slot.GoodId.IsValid()||(Stage&&Stage->UpgradeGoods.ContainsByPredicate([&](const auto& G){return G.GoodId==Slot.GoodId.ToString();})))++Compatible;
    Opportunity|=Compatible>0;
   }
   if(Opportunity){Status=Prioritized?TEXT("Queued · priority pickup on the next home visit."):TEXT("Queued · waiting for space after ordinary trade loads.");Detail+=Prioritized?TEXT("Delivery queued with construction priority. Space depends on planned unloading succeeding.\n"):TEXT("Delivery queued: waiting for cargo space after ordinary trade loads. Space depends on planned unloading succeeding.\n");}
   else {Status=Prioritized?TEXT("Delivery needs attention: unload existing cargo or choose another ship."):TEXT("Delivery needs attention: no suitable slots or capacity. Choose another ship or review priority.");Detail+=Status+TEXT(" Waiting alone will not clear this planned blocker.\n");}
   if(Compatible==1&&MissingKinds>1)Detail+=TEXT("One suitable slot: materials will be delivered over multiple trips.\n");
  }
  if(Stage)for(const auto& G:Stage->UpgradeGoods){
   int64 Available=0;for(const auto& I:P.GetInventories())if(I.OwnerKind==EHansaInventoryOwnerKind::City&&I.CityId==Home->CityId)for(const auto& Stock:I.Stocks)if(Stock.GoodId.ToString()==G.GoodId)Available+=Stock.Available.GetRawValue();
   const auto* D=Station.SpentGoods.FindByPredicate([&](const auto& Cost){return Cost.GoodId.ToString()==G.GoodId;});const auto* A=Inv.Stocks.FindByPredicate([&](const auto& Stock){return Stock.GoodId.ToString()==G.GoodId;});
   const int64 Remaining=FMath::Max<int64>(0,G.QuantityMilliUnits-(D?D->Quantity.GetRawValue():0)-(A?A->Stock.GetRawValue():0));
   if(Available<Remaining){const FString Waiting=TEXT("Waiting for ")+(Registry.FindGood(G.GoodId)?Registry.FindGood(G.GoodId)->DisplayName:G.GoodId)+TEXT(" at ")+City+TEXT(".");Detail+=Waiting+TEXT("\n");if(Access&&Route->Lifecycle!=EHansaRouteLifecycleState::Inactive)Status=Waiting;}
  }
 }
 if(!C.bEligible)Detail+=TEXT("Choose a Cog with a route through a home city and the destination, or a local source with materials. Create a route or build a Cog if none is suitable.\n");
 C.Detail=FText::FromString(Detail);C.Transfer=FText::FromString(Transfer);C.DeliveryStatus=FText::FromString(Status);
 C.Action=LOCTEXT("ArrangeDelivery","Arrange delivery · no payment");C.ReviewKey+=TEXT("|Delivery|")+LexToString(C.DeliveryMode)+LexToString(C.bEligible);
}
