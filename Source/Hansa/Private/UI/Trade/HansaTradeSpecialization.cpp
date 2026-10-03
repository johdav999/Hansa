#include "UI/HansaTradeSpecialization.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
#define LOCTEXT_NAMESPACE "TradeSpecialization"
using namespace Hansa::Simulation;
namespace {
FText Units(int64 V){return FText::AsNumber(double(V)/1000.);}
}
FString FHansaTradeSpecialization::Key() const {
 FString K=City.ToString()+TEXT("|")+LexToString(Revision);
 for(const auto& O:Options){K+=O.Id+O.Name.ToString()+O.Blocker.ToString()+O.Recommendation.ToString()+FString::Printf(TEXT("%d%d%d"),O.bCurrent,O.bAvailable,O.bRespec);
 for(const auto& D:O.Dimensions)K+=TEXT("|")+D.ToString();
 for(const auto& C:O.Sources)K+=TEXT("|")+C.Id+C.Label.ToString()+C.Detail.ToString()+LexToString(C.bEligible);}
 return K;
}
TArray<FText> Hansa::UI::SpecializationDimensionLabels(){
 return {LOCTEXT("Investment","Investment"),LOCTEXT("Storage","Storage"),LOCTEXT("Orders","Order slots"),LOCTEXT("Handling","Route handling"),LOCTEXT("Prices","Price capabilities"),LOCTEXT("Prerequisites","Prerequisites"),LOCTEXT("Exclusive","Opportunity cost"),LOCTEXT("Respec","Respec consequences")};
}
FHansaTradeSpecialization Hansa::UI::BuildTradeSpecialization(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& R,FHansaHouseId Viewer,FName City){
 FHansaTradeSpecialization V;V.City=City;
 const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Viewer&&X.CityId.ToString()==City.ToString();});
 const auto* Policy=R.FindCityTradePolicyForCity(City.ToString());
 const auto* House=P.GetHouses().FindByPredicate([&](const auto& X){return X.Id==Viewer;});
 if(!Presence||!Policy||!House)return V;
 V.Revision=Presence->SpecializationRevision;
 const auto* Stage=R.FindPresenceStage(Presence->CurrentStageId);
 for(const auto& O:Presence->Specializations){
  const auto* Def=Policy->Specializations.FindByPredicate([&](const auto& X){return X.SpecializationId==O.SpecializationId;});if(!Def)continue;
  const auto* Required=R.FindPresenceStage(Def->RequiredStageId);
  const auto* Prior=Policy->Specializations.FindByPredicate([&](const auto& X){return X.ExclusiveGroupId==Def->ExclusiveGroupId&&Presence->Specializations.ContainsByPredicate([&](const auto& Selected){return Selected.bSelected&&Selected.SpecializationId==X.SpecializationId;});});
  auto& C=V.Options.AddDefaulted_GetRef();C.Id=O.SpecializationId;C.Name=FText::FromString(O.DisplayName);C.bCurrent=O.bSelected;C.bRespec=Prior!=nullptr;
  const int64 Refund=Prior?Prior->InvestmentCostPfennig*Prior->RespecRefundBasisPoints/10000:0;
  const int64 Delta=O.StorageCapacityBonusMilliUnits-(Prior?Prior->StorageCapacityBonusMilliUnits:0);
  C.bAvailable=!C.bCurrent&&Presence->Status==EHansaForeignPresenceStatus::Active&&Stage&&Required&&Stage->Ordinal>=Required->Ordinal&&(!Prior||Prior->bAllowRespec);
  C.Blocker=C.bCurrent?LOCTEXT("Current","Current specialization"):!C.bAvailable?LOCTEXT("Unavailable","Unavailable: requires active office rights and a reversible current branch."):FText();
  FText Cost=FText::Format(LOCTEXT("Cost","{0} pfennig"),FText::AsNumber(O.InvestmentCostPfennig));
  for(const auto& G:O.InvestmentGoods){const auto* Good=R.FindGood(G.GoodId);Cost=FText::Format(LOCTEXT("CostGood","{0}\n{1} {2}"),Cost,Units(G.QuantityMilliUnits),FText::FromString(Good?Good->DisplayName:G.GoodId));}
  FText Other;for(const auto& B:Policy->Specializations)if(B.ExclusiveGroupId==Def->ExclusiveGroupId&&B.SpecializationId!=C.Id)Other=Other.IsEmpty()?FText::FromString(B.DisplayName):FText::Format(LOCTEXT("And","{0} and {1}"),Other,FText::FromString(B.DisplayName));
  C.Dimensions={Cost,FText::Format(LOCTEXT("StorageBonus","+{0} units capacity"),Units(O.StorageCapacityBonusMilliUnits)),FText::Format(LOCTEXT("SlotBonus","+{0} active orders"),FText::AsNumber(O.AdditionalOrderSlots)),FText::Format(LOCTEXT("HandlingBonus","+{0} units per station transfer"),Units(O.StationTransferCapBonusMilliUnits)),O.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.MarketSpecialization"))&&!Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.MarketSpecialization"))?LOCTEXT("Limits","Buy ceilings and sell floors for station orders; no price discount."):LOCTEXT("NoLimits","No additional price controls or discount."),FText::Format(LOCTEXT("Requires","{0}; active rights; owned station materials and treasury."),FText::FromString(Required?Required->DisplayName:Def->RequiredStageId)),FText::Format(LOCTEXT("Locks","Excludes {0}. One branch in this group can be active."),Other),Def->bAllowRespec?FText::Format(LOCTEXT("Refund","Changing away returns {0}% of this branch's money cost. Materials are never refunded. Removed storage must fit remaining stock."),FText::AsNumber(double(Def->RespecRefundBasisPoints)/100.)):LOCTEXT("Irreversible","Irreversible: this branch does not permit respec.")};
  for(const auto& S:P.GetTradeStations())if(S.Station.OwnerId==Viewer){
   const auto* Inv=P.GetInventories().FindByPredicate([&](const auto& X){return X.Id==S.Station.InventoryId;});if(!Inv)continue;
   FHansaEstablishmentChoice Source;Source.Id=LexToString(Inv->Id.GetValue());Source.Label=FText::Format(LOCTEXT("StationSource","{0} station · inventory #{1}"),FText::FromString(S.Station.CityId.ToString().Replace(TEXT("City."),TEXT(""))),FText::AsNumber(Inv->Id.GetValue()));
   Source.bEligible=C.bAvailable&&House->Money.GetRawValue()<=MAX_int64-Refund&&House->Money.GetRawValue()+Refund>=O.InvestmentCostPfennig;
   Source.Detail=FText::Format(LOCTEXT("ExactMoney","Treasury: {0} pfennig. Refund: {1}. Cost: {2}. After: {3}.\nMaterials from {4}."),FText::AsNumber(House->Money.GetRawValue()),FText::AsNumber(Refund),FText::AsNumber(O.InvestmentCostPfennig),FText::AsNumber(House->Money.GetRawValue()-O.InvestmentCostPfennig+Refund),Source.Label);
   int64 Consumed=0;for(const auto& G:O.InvestmentGoods){const auto* Stock=Inv->Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==G.GoodId;});const int64 Available=Stock?Stock->Available.GetRawValue():0;Source.bEligible&=Available>=G.QuantityMilliUnits;Consumed+=G.QuantityMilliUnits;const auto* Good=R.FindGood(G.GoodId);
    Source.Detail=FText::Format(LOCTEXT("ExactGoods","{0}\n{1}: spend {2}; available {3} units."),Source.Detail,FText::FromString(Good?Good->DisplayName:G.GoodId),Units(G.QuantityMilliUnits),Units(Available));}
   const int64 Capacity=S.StorageCapacity.GetRawValue()+Delta,Remaining=S.StorageUsed.GetRawValue()-Consumed;
   Source.bEligible&=Capacity>=Remaining&&Capacity>=0;
   Source.Detail=FText::Format(LOCTEXT("ExactCapacity","{0}\nSource capacity after: {1} units; stock after: {2} units.\n{3}"),Source.Detail,Units(Capacity),Units(FMath::Max<int64>(0,Remaining)),Source.bEligible?LOCTEXT("FundReady","Ready for review. Payment occurs only on confirmation."):LOCTEXT("FundBlocked","Unavailable: restore rights, replenish treasury/materials, or clear stock before reducing capacity."));
   Source.Transfer=Source.Detail;C.Sources.Add(MoveTemp(Source));
   if(S.Station.CityId==Presence->CityId&&S.StorageCapacity.GetRawValue()>0&&S.StorageUsed.GetRawValue()>S.StorageCapacity.GetRawValue()*7/10&&O.StorageCapacityBonusMilliUnits>0)
    C.Recommendation=LOCTEXT("StorageRecommendation","Suggested: this station is over 70% full; added capacity provides reserve room.");
  }
  if(C.bAvailable&&!C.Sources.ContainsByPredicate([](const auto& Source){return Source.bEligible;})){C.bAvailable=false;C.Blocker=LOCTEXT("NoFunds","Unavailable: no owned station can cover the treasury, materials, and remaining storage requirements.");}
  C.Sources.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
  if(C.Recommendation.IsEmpty()&&O.StationTransferCapBonusMilliUnits>0&&P.GetRoutes().ContainsByPredicate([&](const auto& X){return X.OwnerId==Viewer&&X.Stops.ContainsByPredicate([&](const auto& S){return S.CityId==Presence->CityId;});}))
   C.Recommendation=LOCTEXT("RouteRecommendation","Suggested: your routes visit this city; added station handling supports larger transfers.");
 }
 return V;
}
#undef LOCTEXT_NAMESPACE
