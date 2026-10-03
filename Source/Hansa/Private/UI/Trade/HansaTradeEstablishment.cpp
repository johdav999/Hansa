#include "UI/HansaTradeEstablishment.h"
#include "UI/HansaTradeCityInspector.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Trade/HansaCargoPlan.h"
#define LOCTEXT_NAMESPACE "HansaTradeEstablishment"
using namespace Hansa::Simulation;
using namespace Hansa::UI;
namespace {
FString Units(int64 Raw) { return FText::AsNumber(double(Raw)/1000.).ToString(); }
FString CityName(const FHansaEconomicRegistry& R,const FString& Id) { return Id==TEXT("City.Lubeck")?TEXT("Lübeck"):Id==TEXT("City.Luneburg")?TEXT("Lüneburg"):Id.Replace(TEXT("City."),TEXT("")); }
}
#include "HansaConstructionDeliveryPresentation.inl"
TArray<FHansaEstablishmentChoice> Hansa::UI::BuildMerchantOfficeSources(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& R,FHansaHouseId Viewer,FName City) {
 TArray<FHansaEstablishmentChoice> Result;
 const auto* Station=FindTradeCityStation(P,City,Viewer);
 const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Viewer&&X.CityId.ToString()==City.ToString();});
 const auto* Stage=R.FindPresenceStage(TEXT("PresenceStage.MerchantOffice"));
 const auto* Policy=R.FindCityTradePolicyForCity(City.ToString());
 const auto* House=P.GetHouses().FindByPredicate([&](const auto& H){return H.Id==Viewer;});
 if(!Station||!Presence||!Stage||!House||!Station->Station.ConstructionSite.bLocalDelivery)return Result;
 auto Delivery=Station->Station;Delivery.SpentGoods=Presence->Upgrade.DeliveredGoods;
 Delivery.FundingInventoryId=Presence->Upgrade.FundingInventoryId;
 for(const auto& Inv:P.GetInventories()){
  const auto* Ship=Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle?P.GetVehicles().FindByPredicate([&](const auto& V){return V.CargoInventoryId==Inv.Id&&V.OwnerId==Viewer&&V.Mode==EHansaRouteMode::Sea;}):nullptr;
  const bool LocalStation=Inv.Id==Station->Station.InventoryId;
  if(!Ship&&!LocalStation)continue;
  const auto* Route=Ship?P.GetRoutes().FindByPredicate([&](const auto& Route){return Route.VehicleId==Ship->Id&&Route.OwnerId==Viewer&&Route.Lifecycle!=EHansaRouteLifecycleState::Cancelled;}):nullptr;
  FHansaEstablishmentChoice C;C.Id=LexToString(Inv.Id.GetValue());C.Label=FText::FromString(LocalStation?TEXT("Trade station storage · ")+CityName(R,City.ToString()):FString::Printf(TEXT("Cog #%llu · automatic delivery"),Ship->Id.GetValue()));
  C.bLocalStationSource=LocalStation;
  C.bEligible=House->Money.GetRawValue()>=Stage->UpgradeCostPfennig&&Presence->Status==EHansaForeignPresenceStatus::Active&&Station->Station.Status==EHansaTradeStationStatus::Active;
  DescribeConstructionDelivery(C,Inv,Delivery,Route,Stage,P,R,P.GetClock().GetMinutesPerTick(),true);
  bool FullLocal=LocalStation,FullMaterials=true;FString Costs;
  for(const auto& G:Stage->UpgradeGoods){const auto* Stock=Inv.Stocks.FindByPredicate([&](const auto& S){return S.GoodId.ToString()==G.GoodId;});const int64 Available=Stock?Stock->Available.GetRawValue():0;FullLocal&=Available>=G.QuantityMilliUnits;
   const auto* LocalInv=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==Station->Station.InventoryId;});
   const auto* LocalStock=LocalInv?LocalInv->Stocks.FindByPredicate([&](const auto& S){return S.GoodId.ToString()==G.GoodId;}):nullptr;
   FullMaterials&=Available+(LocalStation?0:LocalStock?LocalStock->Available.GetRawValue():0)>=G.QuantityMilliUnits;
   Costs+=FString::Printf(TEXT("%s: %s units · available %s\n"),*(R.FindGood(G.GoodId)?R.FindGood(G.GoodId)->DisplayName:G.GoodId),*Units(G.QuantityMilliUnits),*Units(Available));}
  if(LocalStation)C.bEligible&=FullLocal;
  if(Ship&&!C.DeliveryMode){C.bEligible&=FullMaterials&&Ship->CurrentCityId==Presence->CityId&&!Ship->Navigation.IsMoving()&&(!Route||Route->Lifecycle!=EHansaRouteLifecycleState::Traveling);C.Label=FText::FromString(FString::Printf(TEXT("Cog #%llu · materials at destination"),Ship->Id.GetValue()));}
  // The single reviewed upgrade authorizes construction priority for its one-time shipment.
  if(C.DeliveryMode){C.DeliveryMode=2;C.Transfer=C.PriorityTransfer;}
  auto UpgradeMoney=[](int64 Value){return FText::Format(LOCTEXT("UpgradeMoney","{0} pfennig"),FText::AsNumber(Value));};
  C.UpgradeCost=UpgradeMoney(Stage->UpgradeCostPfennig);C.UpgradeTreasury=UpgradeMoney(House->Money.GetRawValue());C.UpgradeRemainder=UpgradeMoney(House->Money.GetRawValue()-Stage->UpgradeCostPfennig);
  C.UpgradeDuration=PresenceDuration(Stage->UpgradeConstructionTicks,P.GetClock().GetMinutesPerTick());
  if(Policy)C.UpgradeBenefit=FText::Format(LOCTEXT("UpgradeBenefits","+{0} storage units · +{1} standing-order slots"),FText::FromString(Units(Policy->MerchantOfficeStorageBonusMilliUnits)),FText::AsNumber(Policy->MerchantOfficeAdditionalOrderSlots));
  for(const auto& G:Stage->UpgradeGoods){FHansaStationMaterial M;M.GoodId=G.GoodId;const auto* Good=R.FindGood(G.GoodId);M.Label=FText::FromString(Good?Good->DisplayName:G.GoodId);M.RequiredMilliUnits=G.QuantityMilliUnits;
   const auto* Stock=Inv.Stocks.FindByPredicate([&](const auto& S){return S.GoodId.ToString()==G.GoodId;});M.AvailableMilliUnits=Stock?Stock->Available.GetRawValue():0;
   if(!LocalStation){const auto* Local=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==Station->Station.InventoryId;});const auto* LocalStock=Local?Local->Stocks.FindByPredicate([&](const auto& S){return S.GoodId.ToString()==G.GoodId;}):nullptr;M.AvailableMilliUnits+=LocalStock?LocalStock->Available.GetRawValue():0;}
   C.Materials.Add(MoveTemp(M));}
  C.Detail=FText::FromString((Policy?FString::Printf(TEXT("Adds %s units storage and %d standing-order slots.\n"),*Units(Policy->MerchantOfficeStorageBonusMilliUnits),Policy->MerchantOfficeAdditionalOrderSlots):FString())+FString::Printf(TEXT("Treasury %lld pfennig · pay %lld · afterward %lld.\n"),House->Money.GetRawValue(),Stage->UpgradeCostPfennig,House->Money.GetRawValue()-Stage->UpgradeCostPfennig)+Costs+
   (LocalStation?TEXT("Use the materials already in this station. "):C.Transfer.ToString()+TEXT("\n")+C.DeliveryStatus.ToString()+TEXT("\n"))+TEXT("Pay once on confirmation. The station stays operational; construction starts automatically after all materials arrive. Ready after ")+PresenceDuration(Stage->UpgradeConstructionTicks,P.GetClock().GetMinutesPerTick()).ToString()+TEXT(" of construction.")+
   (!C.bEligible?TEXT("\nUpgrade needs attention: check funds, rights and delivery source. Choose a Cog with a route through a home city and this destination, or replenish station storage."):TEXT("")));
  C.ReviewKey=C.Id+FString::Printf(TEXT("|Office|%lld|%d|%d|%d|"),Stage->UpgradeCostPfennig,Stage->UpgradeConstructionTicks,C.bEligible,C.DeliveryMode)+(Route?LexToString(Route->Id.GetValue()):TEXT("Local"));
  if(Policy){C.ReviewKey+=FString::Printf(TEXT("|%lld:%d|"),Policy->MerchantOfficeStorageBonusMilliUnits,Policy->MerchantOfficeAdditionalOrderSlots);if(const auto* Site=Policy->TradeStationSites.FindByPredicate([&](const auto& S){return S.SiteId==Station->Station.SiteId;})){C.ReviewKey+=LexToString(Site->CancellationRefundBasisPoints);C.Detail=FText::FromString(C.Detail.ToString()+FString::Printf(TEXT("\nClosing the station cancels the upgrade and refunds %s%% of paid upgrade money and delivered materials. Undelivered cargo stays aboard."),*FText::AsNumber(double(Site->CancellationRefundBasisPoints)/100.).ToString()));}}
  if(Route)for(const auto& Stop:Route->Stops)C.ReviewKey+=Stop.CityId.ToString()+TEXT("|");
  for(const auto& G:Stage->UpgradeGoods)C.ReviewKey+=G.GoodId+TEXT(":")+LexToString(G.QuantityMilliUnits)+TEXT("|");
  C.Action=LOCTEXT("OfficeUpgrade","Upgrade to Merchant Office");Result.Add(MoveTemp(C));
 }
 // Prefer already stocked station storage, then a suitable ship, deterministically.
 Result.Sort([](const auto& A,const auto& B){if(A.bEligible!=B.bEligible)return A.bEligible;if(A.DeliveryMode!=B.DeliveryMode)return A.DeliveryMode<B.DeliveryMode;return FCString::Strtoui64(*A.Id,nullptr,10)<FCString::Strtoui64(*B.Id,nullptr,10);});
 return Result;
}
bool FHansaTradeEstablishment::operator==(const FHansaTradeEstablishment& B) const {
 if(!OperationalLabel.EqualTo(B.OperationalLabel)||!ConstructionPaid.EqualTo(B.ConstructionPaid)||Rights!=B.Rights||CancellationRefundBasisPoints!=B.CancellationRefundBasisPoints||bPendingUpgrade!=B.bPendingUpgrade)return false;
 if(bOfficeBuilt!=B.bOfficeBuilt||!State.EqualTo(B.State)||!SiteName.EqualTo(B.SiteName)||!SiteStatus.EqualTo(B.SiteStatus)||!BuildDuration.EqualTo(B.BuildDuration)||!OperationsSummary.EqualTo(B.OperationsSummary))return false;
 if(bPriorityReview!=B.bPriorityReview||bCanPrioritizeDelivery!=B.bCanPrioritizeDelivery||bLocalDelivery!=B.bLocalDelivery||PlacementAnchor!=B.PlacementAnchor||PlacementRotation!=B.PlacementRotation||StationId!=B.StationId||LeaseBoundsMin!=B.LeaseBoundsMin||LeaseBoundsMax!=B.LeaseBoundsMax||WorldPresentationClass!=B.WorldPresentationClass||!Treasury.EqualTo(B.Treasury)||!Remainder.EqualTo(B.Remainder)||!Cost.EqualTo(B.Cost)||!Storage.EqualTo(B.Storage)||!DailyUpkeep.EqualTo(B.DailyUpkeep)||!ConstructionRemaining.EqualTo(B.ConstructionRemaining)||ConstructionProgress!=B.ConstructionProgress||bAwaitingPickup!=B.bAwaitingPickup||ReviewKey!=B.ReviewKey||SiteId!=B.SiteId||SourceId!=B.SourceId||bReview!=B.bReview||bPending!=B.bPending||bVisible!=B.bVisible||bCanOpenMarket!=B.bCanOpenMarket||MinutesPerTick!=B.MinutesPerTick||!Blocker.EqualTo(B.Blocker)||!Feedback.EqualTo(B.Feedback)||!Action.EqualTo(B.Action)||!Summary.EqualTo(B.Summary)||Sites.Num()!=B.Sites.Num()||Sources.Num()!=B.Sources.Num())return false;
 for(int32 I=0;I<Sites.Num();++I)if(!Sites[I].Detail.EqualTo(B.Sites[I].Detail)||!Sites[I].Label.EqualTo(B.Sites[I].Label)||Sites[I].bEligible!=B.Sites[I].bEligible)return false;
 for(int32 I=0;I<Sources.Num();++I)if(!Sources[I].Detail.EqualTo(B.Sources[I].Detail)||!Sources[I].Label.EqualTo(B.Sources[I].Label)||Sources[I].bEligible!=B.Sources[I].bEligible||Sources[I].DeliveryMode!=B.Sources[I].DeliveryMode||!Sources[I].PriorityTransfer.EqualTo(B.Sources[I].PriorityTransfer)||!Sources[I].DeliveryStatus.EqualTo(B.Sources[I].DeliveryStatus)||Sources[I].bPriorityActive!=B.Sources[I].bPriorityActive||Sources[I].Materials!=B.Sources[I].Materials||!Sources[I].UpgradeBenefit.EqualTo(B.Sources[I].UpgradeBenefit)||!Sources[I].UpgradeDuration.EqualTo(B.Sources[I].UpgradeDuration)||!Sources[I].UpgradeTreasury.EqualTo(B.Sources[I].UpgradeTreasury)||!Sources[I].UpgradeRemainder.EqualTo(B.Sources[I].UpgradeRemainder)||!Sources[I].UpgradeCost.EqualTo(B.Sources[I].UpgradeCost))return false;
 return true;
}
FHansaTradeEstablishment Hansa::UI::BuildTradeEstablishment(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& R,FHansaHouseId Viewer,FName City,const FString& SelectedSite,const FString& SelectedSource) {
 FHansaTradeEstablishment O;O.CityId=City;O.CityLabel=FText::FromString(CityName(R,City.ToString()));O.SiteId=SelectedSite;O.SourceId=SelectedSource;O.Tick=P.GetClock().GetTick().GetValue();
 O.MinutesPerTick=P.GetClock().GetMinutesPerTick();
 const auto* Policy=R.FindCityTradePolicyForCity(City.ToString());
 const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return Viewer.IsValid()&&X.HouseId==Viewer&&X.CityId.ToString()==City.ToString();});
 const auto* Station=FindTradeCityStation(P,City,Viewer);
 if(Station&&Station->Station.Status==EHansaTradeStationStatus::Closed&&!Station->Lease.bOccupied)Station=nullptr;
 O.bVisible=Policy&&(!Policy->TradeStationSites.IsEmpty()||Station);
 O.Action=LOCTEXT("ReviewProposal","Review trade station site reservation");
 if(!O.bVisible)return O;
 if(Station){O.bLocalDelivery=Station->Station.ConstructionSite.bLocalDelivery;O.PlacementAnchor={Station->Station.ConstructionSite.Anchor.X,Station->Station.ConstructionSite.Anchor.Y};O.PlacementRotation=int32(Station->Station.ConstructionSite.Rotation);O.LeaseBoundsMin=FIntPoint(Station->Lease.BoundsMin.X,Station->Lease.BoundsMin.Y);O.LeaseBoundsMax=FIntPoint(Station->Lease.BoundsMax.X,Station->Lease.BoundsMax.Y);O.WorldPresentationClass=Station->PresentationClassPath;O.StationId=int64(Station->Station.Id.GetValue());O.SiteId=Station->Station.SiteId;O.bProposed=Station->Station.Status==EHansaTradeStationStatus::Proposed;O.bConstructing=Station->Station.Status==EHansaTradeStationStatus::UnderConstruction;O.bComplete=Station->Station.Status==EHansaTradeStationStatus::Active||Station->Station.Status==EHansaTradeStationStatus::Suspended;O.bArrears=Station->Station.OperationalState==EHansaTradeStationOperationalState::Underfunded;}
 if(Station&&Station->Station.Status==EHansaTradeStationStatus::Closed){O.bVisible=false;return O;}
 O.bOfficeBuilt=Station&&Presence&&Presence->StationId==Station->Station.Id&&Presence->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.MerchantOffice")&&C.bGranted;});
 if(Station){
  const TCHAR* Operational=TEXT("Active");switch(Station->Station.OperationalState){case EHansaTradeStationOperationalState::Underfunded:Operational=TEXT("Underfunded");break;case EHansaTradeStationOperationalState::StorageBlocked:Operational=TEXT("Storage blocked");break;case EHansaTradeStationOperationalState::OrderSuspended:Operational=TEXT("Orders suspended");break;case EHansaTradeStationOperationalState::RightsSuspended:Operational=TEXT("Rights suspended");break;case EHansaTradeStationOperationalState::VoluntarilyClosed:Operational=TEXT("Voluntarily closed");break;case EHansaTradeStationOperationalState::Revoked:Operational=TEXT("Revoked");break;default:break;}
  O.State=FText::Format(O.bOfficeBuilt?LOCTEXT("OfficeState","{0} Merchant Office · {1}"):LOCTEXT("StationState","{0} station · {1}"),O.CityLabel,FText::FromString(O.bProposed?(O.bLocalDelivery?TEXT("Paid · awaiting materials"):TEXT("Proposed")):O.bConstructing?TEXT("Under construction"):Operational));
  O.OperationalLabel=FText::FromString(O.bProposed?TEXT("Awaiting materials"):O.bConstructing?TEXT("Under construction"):Operational);
  O.ConstructionPaid=FText::Format(LOCTEXT("PaidConstruction","{0} pfennig"),FText::AsNumber(Station->Station.SpentMoneyRaw));
  if(Presence){O.bPendingUpgrade=Presence->Upgrade.Status!=EHansaPresenceUpgradeStatus::None;
   const TArray<FString> CommercialRights={TEXT("PresenceCapability.LocalStorage"),TEXT("PresenceCapability.MarketReports"),TEXT("PresenceCapability.PublicMarketTrade"),TEXT("PresenceCapability.RouteAccess"),TEXT("PresenceCapability.StationOrders"),TEXT("PresenceCapability.TradeStation"),TEXT("PresenceCapability.MerchantOffice"),TEXT("PresenceCapability.HarborSpecialization"),TEXT("PresenceCapability.MarketSpecialization")};
   for(const auto& Id:CommercialRights){const auto* C=Presence->Capabilities.FindByPredicate([&](const auto& Capability){return Capability.CapabilityId==Id;});
    if(!C||(CommercialRights.IndexOfByKey(Id)>=6&&!C->bGranted))continue;
    FHansaStationRight Right;Right.Id=C->CapabilityId;const auto* Definition=R.FindPresenceCapability(C->CapabilityId);Right.Label=FText::FromString(Definition?Definition->DisplayName:C->CapabilityId);Right.bGranted=C->bGranted&&!Policy->DeniedCapabilityIds.Contains(C->CapabilityId)&&Presence->Status==EHansaForeignPresenceStatus::Active;O.Rights.Add(MoveTemp(Right));}}
 }
 O.bAwaitingPickup=Station&&O.bProposed&&Station->Station.FundingInventoryId.IsValid()&&(!O.bLocalDelivery||Station->Station.FundingInventoryId!=Station->Station.InventoryId);
 if(O.bAwaitingPickup&&(!O.bLocalDelivery||O.SourceId.IsEmpty()))O.SourceId=LexToString(Station->Station.FundingInventoryId.GetValue());
 const auto* Stage=Presence?R.GetPresenceStages().FindByPredicate([&](const auto& X){return X.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"))&&R.IsValidPresenceTransition(City.ToString(),Presence->CurrentStageId,X.StableId);}):nullptr;
 if(Station&&!O.bProposed){Stage=nullptr;for(const auto& Candidate:R.GetPresenceStages())if(Policy->AllowedStageIds.Contains(Candidate.StableId)&&Candidate.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"))&&(!Stage||Candidate.Ordinal<Stage->Ordinal))Stage=&Candidate;}
 const auto* House=P.GetHouses().FindByPredicate([&](const auto& X){return Viewer.IsValid()&&X.Id==Viewer;});
 const int64 Money=House?House->Money.GetRawValue():0;
 const int64 Cost=O.bArrears?Station->Station.OutstandingUpkeepPfennig:Stage?Stage->UpgradeCostPfennig:0;
 const auto* Site=Policy->TradeStationSites.FindByPredicate([&](const auto& X){return X.SiteId==O.SiteId;});
 O.CancellationRefundBasisPoints=Site?Site->CancellationRefundBasisPoints:0;
 for(const auto& S:Policy->TradeStationSites){
  FHansaEstablishmentChoice C;C.Id=S.SiteId;
  const bool Occupied=P.GetTradeStations().ContainsByPredicate([&](const auto& X){return X.Station.CityId.ToString()==City.ToString()&&X.Station.SiteId==S.SiteId&&X.Lease.bOccupied&&X.Station.Id.GetValue()!=uint64(O.StationId);});
  C.bEligible=S.PlotCategory==TEXT("Commercial")&&!Occupied;
  C.Label=S.SiteId.Contains(TEXT("Harbor"))?LOCTEXT("HarborSite","Harbor trading site"):LOCTEXT("CommercialSite","Commercial trading site");
  if(Policy->TradeStationSites.Num()>1)C.Label=FText::Format(LOCTEXT("NumberedSite","{0} {1}"),C.Label,FText::AsNumber(O.Sites.Num()+1));
  C.Detail=FText::Format(LOCTEXT("SiteCalendar","{0} · {1} units storage · {2} to build · {3} upkeep\n{4}"),C.Label,FText::FromString(Units(S.StorageCapacityMilliUnits)),PresenceDuration(S.ConstructionTicks,O.MinutesPerTick),PresenceDailyUpkeep(S.UpkeepPfennigPerTick,O.MinutesPerTick),Occupied?LOCTEXT("Occupied","Site occupied. Choose another site or wait for its lease to be released."):C.bEligible?LOCTEXT("SiteAvailable","Commercial lease available; availability is checked when reserving."):LOCTEXT("WrongCategory","This site does not permit a commercial station."));
  O.Sites.Add(C);
 }
 const auto* ChosenSite=O.Sites.FindByPredicate([&](const auto& X){return X.Id==O.SiteId;});
 const auto* Next=Presence&&Stage?Presence->NextStages.FindByPredicate([&](const auto& X){return X.StageId==Stage->StableId;}):nullptr;
 const bool Access=Presence&&Presence->Status==EHansaForeignPresenceStatus::Active&&!Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"));
 O.bHasAccess=Access;
 O.bCanOpenMarket=Presence&&Presence->Status==EHansaForeignPresenceStatus::Active&&!Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.PublicMarketTrade"))&&Presence->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.PublicMarketTrade")&&C.bGranted;});
 O.SiteName=ChosenSite?ChosenSite->Label:LOCTEXT("ChooseSite","Choose a station site");
 if(O.bOfficeBuilt)O.SiteName=LOCTEXT("OfficeName","Merchant Office");
 O.SiteStatus=ChosenSite?(ChosenSite->bEligible?(Station?LOCTEXT("Reserved","Your commercial lease"):LOCTEXT("AvailableLease","Commercial lease available")):LOCTEXT("SiteBlocked","Site unavailable — choose another")):LOCTEXT("NoSite","Select an available commercial lease");
 O.Storage=Station||Site?FText::Format(LOCTEXT("Storage","{0} units storage"),FText::FromString(Units(Station?Station->StorageCapacity.GetRawValue():Site->StorageCapacityMilliUnits))):LOCTEXT("UnknownMetric","—");
 O.BuildDuration=Site?FText::Format(LOCTEXT("BuildDuration","{0} to build"),PresenceDuration(Site->ConstructionTicks,O.MinutesPerTick)):LOCTEXT("UnknownMetric","—");
 O.DailyUpkeep=Site?PresenceDailyUpkeep(Site->UpkeepPfennigPerTick,O.MinutesPerTick):LOCTEXT("UnknownMetric","—");
 if(Station)O.DailyUpkeep=PresenceDailyUpkeep(Station->Station.UpkeepPfennigPerTick,O.MinutesPerTick);
 if(O.bOfficeBuilt)O.BuildDuration=LOCTEXT("OfficeReady","Upgrade complete");
 O.bHasCost=House&&(Stage||O.bArrears);
 auto MoneyText=[](int64 Value){return FText::Format(LOCTEXT("MoneyValue","{0} pfennig"),FText::AsNumber(Value));};
 O.Treasury=House?MoneyText(Money):LOCTEXT("UnknownMoney","Unavailable");O.Cost=O.bHasCost?MoneyText(O.bLocalDelivery||O.bAwaitingPickup||O.bComplete||O.bConstructing?Station->Station.SpentMoneyRaw:Cost):LOCTEXT("UnknownMoney","Unavailable");
 O.Remainder=O.bHasCost?MoneyText(O.bLocalDelivery||O.bAwaitingPickup||O.bComplete||O.bConstructing?Money:Money-Cost):LOCTEXT("UnknownMoney","Unavailable");
 if(Next)for(const auto& Q:Next->Requirements) {
  if(Q.RequirementId.StartsWith(TEXT("UpgradeGood."))||Q.RequirementId==TEXT("AvailableMoney"))continue;
  if(Q.RequirementId==TEXT("QualifyingInvestment")){O.bHasInvestment=true;O.bInvestmentMet=Q.bMet;O.Investment=Q.bMet?LOCTEXT("InvestmentMet","Qualifying investment after funding: requirement met"):FText::Format(LOCTEXT("InvestmentUnmet","Qualifying investment after funding: {0} / {1} pfennig"),FText::AsNumber(Q.CurrentValue),FText::AsNumber(Q.RequiredValue));continue;}
  FHansaEstablishmentRequirement Row;Row.Id=Q.RequirementId;Row.bMet=Q.bMet;
  const bool Time=Q.RequirementId.Contains(TEXT("Operation"));
  const bool Quantity=Q.RequirementId==TEXT("LawfulTradeVolume")||Q.RequirementId==TEXT("ShortageRelief");
  Row.Label=FText::FromString(Q.Description);
  if(Time)Row.Label=Q.RequirementId.Contains(TEXT("Reliable"))?LOCTEXT("Reliable","Reliable operation"):LOCTEXT("Solvent","Solvent operation");
  Row.Value=Time?PresenceTimeProgress(Q.CurrentValue,Q.RequiredValue,O.MinutesPerTick):FText::Format(LOCTEXT("ProgressValue","{0} / {1}{2}"),FText::AsNumber(double(Q.CurrentValue)/(Quantity?1000.:1.)),FText::AsNumber(double(Q.RequiredValue)/(Quantity?1000.:1.)),Quantity?LOCTEXT("UnitsSuffix"," units"):Q.RequirementId==TEXT("TransactionValue")?LOCTEXT("MoneySuffix"," pfennig"):FText());
  Row.Progress=Q.RequiredValue>0?FMath::Clamp(float(double(Q.CurrentValue)/Q.RequiredValue),0.f,1.f):(Q.bMet?1.f:0.f);
  Row.Hint=Time?LOCTEXT("KeepOperating","Keep a selling route active and your house solvent as game time passes."):Q.RequirementId==TEXT("ShortageRelief")?LOCTEXT("SupplyShortages","Sell goods below this city's market reserve through your route."):LOCTEXT("TradeMore","Complete paid route sales in this city.");
  O.UnmetRequirements+=!Q.bMet;O.Requirements.Add(MoveTemp(Row));
 }
 O.bCanPropose=!Station&&Access&&Next&&Next->bProgressRequirementsMet&&ChosenSite&&ChosenSite->bEligible;
 FString Summary=O.bProposed?TEXT("2 / 4 · Site reserved. Choose a funding inventory.\n"):O.bConstructing?TEXT("3 / 4 · Construction funded.\n"):O.bComplete?TEXT("4 / 4 · Station operations available.\n"):TEXT("1 / 4 · Meet trading requirements, then reserve a site for your trade station at no charge.\n");
 Summary+=CityName(R,City.ToString())+TEXT(" · ")+(ChosenSite?ChosenSite->Detail.ToString():TEXT("Choose a site explicitly; no plot is reserved yet."));
 if(Stage){if(O.bComplete||O.bConstructing)Summary+=FString::Printf(TEXT("\nConstruction paid: %lld pfennig · station #%lld."),Station->Station.SpentMoneyRaw,O.StationId);else Summary+=FString::Printf(TEXT("\nTreasury: %lld pfennig · later trade station construction cost %lld · afterward %lld."),Money,Cost,FMath::Max<int64>(0,Money-Cost));
  Summary+=O.bComplete?TEXT("\nStation capabilities: "):TEXT("\nGained: ");for(const auto& Id:Stage->GrantedCapabilityIds){if(!O.bComplete&&Presence&&Presence->Capabilities.ContainsByPredicate([&](const auto& C){return C.CapabilityId==Id&&C.bGranted;}))continue;const auto* Cap=R.FindPresenceCapability(Id);Summary+=(Cap?Cap->DisplayName:Id)+TEXT("; ");}
  for(const auto& Q:O.Requirements)Summary+=FString::Printf(TEXT("\n%s: %s · %s"),*Q.Label.ToString(),*Q.Value.ToString(),Q.bMet?TEXT("met"):TEXT("unmet"));
  if(O.bHasInvestment)Summary+=TEXT("\n")+O.Investment.ToString();
 }else if(!Station)Summary+=TEXT("\nStation access unavailable. Restore presence or review city policy.");
 if(!Access)Summary+=TEXT("\nCity policy or presence status blocks establishment. Restore the required rights before proposing or spending.");
 Summary+=O.bOfficeBuilt?TEXT("\nMerchant Office upgrade complete. The city remains autonomous; specialization and broader construction require separate rights."):TEXT("\nThe city remains autonomous. Office upgrades, specialization and broader construction require separate rights.");
 if(O.bOfficeBuilt){
  Summary=TEXT("Merchant Office upgrade complete.\n")+CityName(R,City.ToString())+TEXT(" · Your commercial lease\n")+O.Storage.ToString()+TEXT(" · ")+O.DailyUpkeep.ToString()+TEXT(" upkeep\nGranted capabilities: ");
  for(const auto& C:Presence->Capabilities)if(C.bGranted){const auto* Capability=R.FindPresenceCapability(C.CapabilityId);Summary+=(Capability?Capability->DisplayName:C.CapabilityId)+TEXT("; ");}
  Summary+=FString::Printf(TEXT("\nMerchant Office adds %d standing-order slots.\nThe city remains autonomous. Specialization and broader construction require separate rights."),Policy->MerchantOfficeAdditionalOrderSlots);
 }
 if(O.bConstructing){const int64 Remaining=FMath::Max<int64>(0,Station->Station.CompletionTick.GetValue()-O.Tick);O.ConstructionRemaining=FText::Format(LOCTEXT("Remaining","{0} remaining"),PresenceDuration(Remaining,O.MinutesPerTick));O.ConstructionProgress=Site&&Site->ConstructionTicks>0?FMath::Clamp(1.f-float(Remaining)/Site->ConstructionTicks,0.f,1.f):0;Summary+=TEXT("\n")+O.ConstructionRemaining.ToString()+TEXT(". Let game time pass to finish construction.");}
 if(O.bArrears)Summary+=FString::Printf(TEXT("\nArrears: %lld pfennig. Reopening consumes treasury money only."),Cost);
 for(const auto& Inv:P.GetInventories()){
  bool Owned=false,Allowed=false;FString Label,Location=CityName(R,Inv.CityId.ToString()),Risk;
  const FHansaRouteProjection* FundingRoute=nullptr;
  if(Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle){const auto* V=P.GetVehicles().FindByPredicate([&](const auto& X){return X.Id==Inv.VehicleId&&Viewer.IsValid()&&X.OwnerId==Viewer;});Owned=V!=nullptr;Allowed=Owned;
   if(V){Location=CityName(R,V->CurrentCityId.ToString());Label=FString::Printf(TEXT("%s #%llu"),V->Mode==EHansaRouteMode::Sea?TEXT("Ship"):TEXT("Vehicle"),V->Id.GetValue());const auto* Route=P.GetRoutes().FindByPredicate([&](const auto& X){return X.VehicleId==V->Id&&X.OwnerId==Viewer&&X.Lifecycle!=EHansaRouteLifecycleState::Cancelled;});if(Route)Risk=TEXT("Assigned route: spending cargo can leave its next delivery short. Review the route before confirming.");if(Route&&Route->RemainingTravelTicks>0){const FString Destination=Route->Stops.IsValidIndex(Route->NextStopIndex)?CityName(R,Route->Stops[Route->NextStopIndex].CityId.ToString()):TEXT("next stop");Location=FString::Printf(TEXT("In transit to %s; last city %s"),*Destination,*Location);}}
  }else if(Inv.OwnerKind==EHansaInventoryOwnerKind::Building||Inv.OwnerKind==EHansaInventoryOwnerKind::Warehouse){const auto* B=P.GetBuildingWorldProjections().FindByPredicate([&](const auto& X){return X.BuildingId==Inv.BuildingId&&Viewer.IsValid()&&X.OwnerId==Viewer;});Owned=B!=nullptr;Allowed=Owned;if(B)Location=CityName(R,B->Placement.CityId.ToString());Label=FString::Printf(TEXT("Building #%llu · inventory #%llu"),Inv.BuildingId.GetValue(),Inv.Id.GetValue());Risk=TEXT("Spending stock may interrupt local production or supply.");
  }else if(Inv.OwnerKind==EHansaInventoryOwnerKind::TradeStation){Owned=P.GetTradeStations().ContainsByPredicate([&](const auto& X){return X.Station.InventoryId==Inv.Id&&Viewer.IsValid()&&X.Station.OwnerId==Viewer;});Label=O.bLocalDelivery&&Station&&Inv.Id==Station->Station.InventoryId?TEXT("Trade house storage"):FString::Printf(TEXT("Station #%llu · inventory #%llu"),Inv.TradeStationId.GetValue(),Inv.Id.GetValue());}
  if(!Owned)continue;
  if(Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle)FundingRoute=P.GetRoutes().FindByPredicate([&](const auto& X){return X.VehicleId==Inv.VehicleId&&X.OwnerId==Viewer&&X.Lifecycle!=EHansaRouteLifecycleState::Cancelled;});
  FHansaEstablishmentChoice C;C.Id=LexToString(Inv.Id.GetValue());C.Label=FText::FromString(Label+TEXT(" · ")+Location);C.bEligible=!O.bAwaitingPickup&&Allowed&&(Stage||O.bArrears)&&Access&&House&&Money>=Cost;
  FString Detail=C.Label.ToString()+TEXT("\n");FString Transfer=C.Label.ToString()+TEXT("\n");
  const auto* Pickup=FundingRoute?FundingRoute->Stops.FindByPredicate([&](const auto& Stop){const auto* City=R.FindCityMarket(Stop.CityId.ToString());return City&&!City->bMarketOnly;}):nullptr;
  bool NeedsPickup=false;
  if(Pickup){Detail+=FString::Printf(TEXT("Collect missing materials at %s automatically. Construction pickup has priority over route loads; cargo space and connected stock are checked at pickup. Free hold now: %s units.\n"),*CityName(R,Pickup->CityId.ToString()),*Units(Inv.FreeCapacity.GetRawValue()));}
  if(FundingRoute){
   Detail+=TEXT("Cargo already aboard is secured immediately; planned route loads do not count as secured materials.\n");
   for(const auto& Stop:FundingRoute->Stops){
    int64 Requested=0;for(const auto& A:Stop.Actions)if(IsRouteLoad(A.Kind))Requested+=FMath::Min(MAX_int64-Requested,A.QuantityLimit.GetRawValue());
    if(Requested>Inv.Capacity.GetRawValue())Detail+=FString::Printf(TEXT("Route requests %s units at %s for a %s-unit hold. Earlier loads use capacity first; reduce loads to leave room for construction materials.\n"),*Units(Requested),*CityName(R,Stop.CityId.ToString()),*Units(Inv.Capacity.GetRawValue()));
   }
  }
  if(!Allowed)Detail+=TEXT("Ineligible: this command cannot spend station inventory. Choose an owned ship or building inventory.\n");
  if(!Access)Detail+=TEXT("City access or presence is suspended. Restore the required rights before spending.\n");
  if(!Stage&&!O.bArrears)Detail+=TEXT("Station funding is unavailable in the current stage.\n");
  if(Money<Cost)Detail+=FString::Printf(TEXT("Treasury shortfall: %lld pfennig. Earn money before funding.\n"),Cost-Money);
  if(Stage&&!O.bArrears)for(const auto& G:Stage->UpgradeGoods){const auto* Stock=Inv.Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==G.GoodId;});const int64 Available=Stock?Stock->Available.GetRawValue():0,Total=Stock?Stock->Stock.GetRawValue():0;const auto* Good=R.FindGood(G.GoodId);
   Detail+=FString::Printf(TEXT("%s: need %s · available %s · reserved %s · stock afterward %s · shortfall %s units\n"),*(Good?Good->DisplayName:G.GoodId),*Units(G.QuantityMilliUnits),*Units(Available),*Units(Total-Available),*Units(FMath::Max<int64>(0,Total-G.QuantityMilliUnits)),*Units(FMath::Max<int64>(0,G.QuantityMilliUnits-Available)));Transfer+=FString::Printf(TEXT("%s: spend %s · afterward %s units\n"),*(Good?Good->DisplayName:G.GoodId),*Units(G.QuantityMilliUnits),*Units(FMath::Max<int64>(0,Total-G.QuantityMilliUnits)));NeedsPickup |= Available<G.QuantityMilliUnits;C.bEligible&=Available>=G.QuantityMilliUnits||Pickup!=nullptr;
   if(Pickup&&Available<G.QuantityMilliUnits){int64 StockAtPickup=0;for(const auto& Local:P.GetInventories())if(Local.OwnerKind==EHansaInventoryOwnerKind::City&&Local.CityId==Pickup->CityId)for(const auto& S:Local.Stocks)if(S.GoodId.ToString()==G.GoodId)StockAtPickup+=FMath::Min(MAX_int64-StockAtPickup,S.Available.GetRawValue());Transfer+=FString::Printf(TEXT("Collect %s %s at %s · city stock now %s units (access and protected reserves checked on arrival).\n"),*Units(G.QuantityMilliUnits-Available),*(Good?Good->DisplayName:G.GoodId),*CityName(R,Pickup->CityId.ToString()),*Units(StockAtPickup));}
   if(FundingRoute)for(const auto& Stop:FundingRoute->Stops)for(const auto& A:Stop.Actions)
    if(!IsRouteLoad(A.Kind)&&A.GoodId.ToString()==G.GoodId)
     Detail+=FString::Printf(TEXT("Route unloads up to %s %s at %s. Confirm construction funding to secure the required materials before route sales. The route can keep running.\n"),*Units(A.QuantityLimit.GetRawValue()),*(Good?Good->DisplayName:G.GoodId),*CityName(R,Stop.CityId.ToString()));
  }
  if(NeedsPickup&&Pickup)Transfer+=TEXT("Start construction automatically when all materials are secured. Keep this route active.\n");
  if(O.bAwaitingPickup){Detail=TEXT("Awaiting pickup. Money has already been paid; secured materials cannot be sold by the route.\n");if(Stage)for(const auto& G:Stage->UpgradeGoods){const auto* Secured=Station->Station.SpentGoods.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==G.GoodId;});const int64 Raw=Secured?Secured->Quantity.GetRawValue():0;const auto* Good=R.FindGood(G.GoodId);Detail+=FString::Printf(TEXT("%s: secured %s / %s units\n"),*(Good?Good->DisplayName:G.GoodId),*Units(Raw),*Units(G.QuantityMilliUnits));}if(!FundingRoute)Detail+=TEXT("No assigned route. Assign this Cog a route with an owned-city stop, or cancel the pickup order.\n");else if(FundingRoute->Lifecycle==EHansaRouteLifecycleState::Inactive)Detail+=TEXT("Route paused. Resume it to collect materials, or cancel the pickup order.\n");if(Inv.FreeCapacity.GetRawValue()==0)Detail+=TEXT("Hold full. Pickup waits until ordinary route unloading makes room.\n");Detail+=TEXT("Keep the assigned route active. Pickup waits for connected city stock, cargo slots, and free hold space. Cancel using Close station safely / recover lease.\n");}
  if(!O.bAwaitingPickup)Detail+=TEXT("Funding is not submitted yet. Review the terms, then confirm payment to secure cargo and start the construction order. No berth restriction applies to authorizing funding.\n")+Risk;
  C.Action=NeedsPickup&&Pickup?FText::Format(LOCTEXT("CollectAtCity","Review collection at {0}"),FText::FromString(CityName(R,Pickup->CityId.ToString()))):Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle?LOCTEXT("FundFromCog","Review funding from this Cog"):LOCTEXT("FundFromStock","Review funding from this inventory");
  // Fixed authorization for this source. Travel, hold space, current cargo and
  // city stock are live information; they do not change the construction order.
  C.ReviewKey=C.Id+FString::Printf(TEXT("|%d|%d|%d|%d|"),Owned,Allowed,C.bEligible,Access)+(Pickup?Pickup->CityId.ToString():FString());
  if(Stage)for(const auto& G:Stage->UpgradeGoods)C.ReviewKey+=G.GoodId+TEXT(":")+LexToString(G.QuantityMilliUnits)+TEXT("|");
  if(O.bLocalDelivery&&O.bProposed){
   C.bEligible=Owned&&Access;
   DescribeConstructionDelivery(C,Inv,Station->Station,FundingRoute,Stage,P,R,O.MinutesPerTick);
   if(Inv.Id==Station->Station.InventoryId&&!C.bEligible)continue;
   Detail=C.Detail.ToString();Transfer=C.Transfer.ToString();
  }
  C.Detail=FText::FromString(Detail);C.Transfer=FText::FromString(Transfer);O.Sources.Add(C);
 }
 O.Sources.Sort([](const auto& A,const auto& B){return FCString::Strtoui64(*A.Id,nullptr,10)<FCString::Strtoui64(*B.Id,nullptr,10);});
 if(O.bLocalDelivery&&O.bProposed&&(O.SourceId.IsEmpty()||(Station&&O.SourceId==LexToString(Station->Station.InventoryId.GetValue())))){
  const auto* Suggested=O.Sources.FindByPredicate([](const auto& C){return C.bEligible&&C.DeliveryMode==1;});
  if(!Suggested)Suggested=O.Sources.FindByPredicate([](const auto& C){return C.bEligible;});
  O.SourceId=Suggested?Suggested->Id:FString();
 }
 const auto* Source=O.Sources.FindByPredicate([&](const auto& X){return X.Id==O.SourceId;});
 O.SourceDetail=Source?Source->Detail:O.SourceId.IsEmpty()&&O.bLocalDelivery?LOCTEXT("ChooseDeliverySource","Choose a Cog with a route through a home city and this destination. Build a Cog or create a route if none is available."):O.SourceId.IsEmpty()?LOCTEXT("ChooseSource","Choose a funding inventory. No source is selected automatically."):LOCTEXT("LostSource","Selected inventory is no longer owned or visible. Choice retained; choose an eligible owned inventory.");
 O.bCanFund=(O.bProposed||O.bArrears)&&Source&&Source->bEligible&&Site&&ChosenSite&&ChosenSite->bEligible;
 O.FundingTerms=FText::FromString(FString::Printf(TEXT("Treasury: spend %lld pfennig · afterward %lld.\nSecure available materials now; collect missing materials at the owned-city stop and start construction automatically. Cancellation refund %d%% of money paid and materials secured."),Cost,FMath::Max<int64>(0,Money-Cost),Site?Site->CancellationRefundBasisPoints/100:0));
 O.Confirmation=FText::Format(LOCTEXT("ExactSpending","{0}\n{1}"),Source?Source->Transfer:O.SourceDetail,O.FundingTerms);
 if(Station)O.OperationsSummary=FText::FromString(FString::Printf(TEXT("Station #%lld · factor #%llu\nStorage %s / %s units · reserved %s · upkeep %s\n%s"),O.StationId,Station->Station.FactorId.GetValue(),*Units(Station->StorageUsed.GetRawValue()),*Units(Station->StorageCapacity.GetRawValue()),*Units(Station->StorageReserved.GetRawValue()),*PresenceDailyUpkeep(Station->Station.UpkeepPfennigPerTick,O.MinutesPerTick).ToString(),*Station->Blocker));
 if(!O.bProposed&&!O.bArrears)O.Confirmation=FText::FromString(TEXT("Reserve the selected site. This proposal spends no money or materials. Funding is a separate reviewed action.\n")+(ChosenSite?ChosenSite->Detail.ToString():TEXT("No site selected.")));
 if(O.bAwaitingPickup)Summary=TEXT("2 / 4 · Awaiting pickup. Construction will start automatically when all materials are secured.\n")+Summary;
 O.Summary=FText::FromString(Summary);
 O.Action=O.bArrears?LOCTEXT("ReviewArrears","Review arrears payment"):O.bAwaitingPickup?LOCTEXT("AwaitingPickup","Awaiting pickup"):O.bProposed?LOCTEXT("ReviewFunding","Fund construction from this inventory"):O.bConstructing?LOCTEXT("Constructing","Construction in progress"):O.bComplete?LOCTEXT("Operations","Inspect station operations"):O.Action;
 if(O.bOfficeBuilt&&O.bComplete&&!O.bArrears)O.Action=LOCTEXT("OfficeOperations","Inspect Merchant Office operations");
 if(O.bProposed&&!O.bAwaitingPickup&&Source)O.Action=Source->Action;
 O.Blocker=!Access?LOCTEXT("RestoreAccess","Restore city trading rights before establishing a station."):
  O.bAwaitingPickup?LOCTEXT("PickupProgress","Materials secured are protected from route sales. Keep the route active to collect the remainder."):O.bConstructing?O.ConstructionRemaining:O.bComplete&&!O.bArrears?LOCTEXT("Ready","Your station is ready for trade."):
  O.bProposed||O.bArrears?(O.bCanFund?LOCTEXT("FundingReady","Not funded yet. Review the terms, then confirm payment. Loading cargo alone does not start construction."):Source?LOCTEXT("SelectedSourceShort","Selected inventory cannot fund construction. Check the shortfalls and route instructions above."):LOCTEXT("NeedSource","Choose an eligible inventory and check its funds and materials.")):
  O.UnmetRequirements>0?FText::Format(LOCTEXT("RequirementsLeft","{0} requirements remain. Settled route sales into this city advance trade and delivery progress; keep an owned Cog operating here for the required time."),FText::AsNumber(O.UnmetRequirements)):
  !ChosenSite?LOCTEXT("SelectSiteFirst","Choose a site before reviewing your proposal."):
  !ChosenSite->bEligible?LOCTEXT("AnotherSite","Choose an available commercial site."):
  !O.bCanPropose?LOCTEXT("CheckRights","Review the city's requirements and qualifying investment."):LOCTEXT("ProposalReady","Reserve this site first. Funding is a separate step.");
 if(O.bOfficeBuilt&&Access&&O.bComplete&&!O.bArrears)O.Blocker=LOCTEXT("OfficeOperational","Your Merchant Office is ready for trade.");
 if(O.bLocalDelivery&&O.bProposed){
  const bool Assigned=Source&&Station->Station.FundingInventoryId.GetValue()==FCString::Strtoui64(*Source->Id,nullptr,10);
  O.Action=Assigned&&Source->bPriorityActive?LOCTEXT("SpareSpaceDelivery","Use spare cargo space · no payment"):Assigned?LOCTEXT("UpdateDelivery","Update delivery · no payment"):LOCTEXT("ArrangeDelivery","Arrange delivery · no payment");
  O.Blocker=Source?Assigned?Source->DeliveryStatus:LOCTEXT("Suggested","Suggested delivery is not yet arranged. Confirm Arrange delivery to use this source."):LOCTEXT("NoDelivery","No delivery arranged. Choose another ship, create a route, or build a Cog.");
  O.bCanPrioritizeDelivery=Source&&Source->bEligible&&Source->DeliveryMode==1&&!Source->bPriorityActive;
  O.FundingTerms=LOCTEXT("DeliveryTerms","Construction is already paid. Arranging delivery charges no money. Cargo remains physical and is protected from sale until delivered. Cancellation keeps undelivered cargo aboard; delivered materials use the lease refund terms.");
  O.Confirmation=FText::Format(LOCTEXT("DeliveryConfirmation","{0}\n{1}"),Source?Source->Transfer:O.SourceDetail,O.FundingTerms);O.Summary=O.Blocker;
 }
 // Only relevant reviewed terms invalidate a confirmation; unrelated market ticks do not.
 if(O.bProposed||O.bArrears){
  // A review authorizes fixed construction costs, not the current treasury balance
  // or earned route progress. Those counters advance while the player reads.
  // Eligibility still changes if funds fall below the cost; source eligibility,
  // policy, ownership, site, stage and refund changes still invalidate the review.
  O.ReviewKey=City.ToString()+TEXT("|")+O.SiteId+TEXT("|")+O.SourceId+FString::Printf(TEXT("|%llu|%lld|%lld|%d|%d|%d|%d|%d|"),Viewer.GetValue(),O.StationId,Cost,O.bCanFund,O.bAwaitingPickup,O.bConstructing,O.bComplete,Access)+(Source?Source->ReviewKey:FString())+(ChosenSite?ChosenSite->Detail.ToString():FString())+(Stage?Stage->StableId:FString())+LexToString(Site?Site->CancellationRefundBasisPoints:0);
 }else{
 O.ReviewKey=City.ToString()+TEXT("|")+O.SiteId+TEXT("|")+O.SourceId+FString::Printf(TEXT("|%llu|%lld|%lld|%lld|%d|%d|%d|%d|"),Viewer.GetValue(),O.StationId,Money,Cost,O.bCanPropose,O.bCanFund,O.bConstructing,O.bComplete)+Summary+O.SourceDetail.ToString();
 }
 return O;
}
#undef LOCTEXT_NAMESPACE
