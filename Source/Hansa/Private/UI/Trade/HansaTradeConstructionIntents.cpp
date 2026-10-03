#include "UI/HansaTradeMapPresentationModel.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"

FHansaTradeConstruction UHansaTradeMapPresentationModel::GetConstructionPresentation() const
{
 if(bRemoteEstablishment)
 {
  const auto* Presence=RemotePresences.FindByPredicate([&](const auto& V){return V.City==Snapshot.SelectedCityStableId;});
  const auto* Report=Presence?Presence->ConstructionReports.FindByPredicate([&](const auto& V){return V.City==Snapshot.SelectedCityStableId&&(SelectedConstructionLease==0||V.SelectedLease==SelectedConstructionLease);}):nullptr;
  if(Report)
  {
   auto V=*Report;
   V.Plots=Presence->ConstructionReports[0].Plots;
   V.SelectedBuilding=V.Options.ContainsByPredicate([&](const auto& O){return O.Id==SelectedConstructionBuilding;})?SelectedConstructionBuilding:NAME_None;
   return V;
  }
  FHansaTradeConstruction V;V.City=Snapshot.SelectedCityStableId;
  if(Presence&&!Presence->ConstructionReports.IsEmpty())V.Plots=Presence->ConstructionReports[0].Plots;
  V.Status=NSLOCTEXT("TradeConstruction","RemoteLostLease","Construction report or selected lease is no longer available. Choose a current owned plot or wait for a fresh report.");return V;
 }
 if(!LastProjection||!LastRegistry){FHansaTradeConstruction V;V.City=Snapshot.SelectedCityStableId;V.Status=NSLOCTEXT("TradeConstruction","ProjectionUnavailable","Leased construction report unavailable. No construction permission is inferred from a city marker.");return V;}
 return Hansa::UI::BuildTradeConstruction(*LastProjection,*LastRegistry,Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse,Snapshot.SelectedCityStableId,SelectedConstructionLease,SelectedConstructionBuilding);
}
bool UHansaTradeMapPresentationModel::ConstructionIntent(const FString& Action)
{
 if(IsAnyTradeCommandPending()||!Snapshot.bOpen||Snapshot.bCreating)return false;
 if(bRemoteEstablishment&&(Action==TEXT("Place")||Action==TEXT("Visit"))){const auto Before=Snapshot;Snapshot.EditorStatus=RemoteActionReason(TEXT("TradeMap.Construction.")+Action);PublishIfChanged(Before);return false;}
 const auto View=GetConstructionPresentation();const auto Previous=Snapshot;
 if(Action.StartsWith(TEXT("Plot."))){uint64 Id=0;if(!LexTryParseString(Id,*Action.RightChop(5))||!View.Plots.ContainsByPredicate([&](const auto& P){return P.Id==Id;}))return false;SelectedConstructionLease=Id;SelectedConstructionBuilding=NAME_None;}
 else if(Action.StartsWith(TEXT("Building."))){const FName Id(*Action);if(!View.Options.ContainsByPredicate([&](const auto& O){return O.Id==Id;}))return false;SelectedConstructionBuilding=Id;}
 else if(Action==TEXT("Place"))
 {
  const auto* O=View.Options.FindByPredicate([&](const auto& V){return V.Id==View.SelectedBuilding;});
  if(!O||!O->bPermitted||!O->bAffordable||!ConstructionRequested||!ConstructionRequested(View.City,View.SelectedLease,View.SelectedBuilding))return false;
  ConstructionReturnFocus=FName(*(TEXT("TradeMap.Construction.")+View.SelectedBuilding.ToString()));return true;
 }
 else if(Action==TEXT("Visit"))
 {
  if(!VisitRequested||!VisitRequested(Snapshot.SelectedCityStableId))return false;
  ConstructionReturnFocus=View.SelectedBuilding.IsNone()?FName(TEXT("TradeMap.Construction.Visit")):FName(*(TEXT("TradeMap.Construction.")+View.SelectedBuilding.ToString()));
  return true;
 }
 else if(Action==TEXT("Presence"))return SelectSectionIntent(TEXT("Presence"));
 else if(Action==TEXT("Ledger"))return SelectSectionIntent(TEXT("Ledger"));
 else return false;
 Snapshot.ConstructionKey=GetConstructionPresentation().Key();PublishIfChanged(Previous);return true;
}
