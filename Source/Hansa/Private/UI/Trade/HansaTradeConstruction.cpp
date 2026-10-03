#include "UI/HansaTradeConstruction.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
#define LOCTEXT_NAMESPACE "TradeConstruction"
using namespace Hansa::Simulation;

FString FHansaTradeConstruction::Key() const
{
 FString K=City.ToString()+TEXT("|")+LexToString(SelectedLease)+TEXT("|")+SelectedBuilding.ToString()+TEXT("|")+Status.ToString()+TEXT("|")+LockedCategories.ToString();
 for(const auto& P:Plots)
 {
  K+=FString::Printf(TEXT("|plot:%llu:%llu:%llu:%d:%d:%d:%d:%d|"),P.Id,P.StationId,P.OwnerId,P.BoundsMin.X,P.BoundsMin.Y,P.BoundsMax.X,P.BoundsMax.Y,P.bActive);
  K+=P.Summary.ToString()+TEXT("|")+FString::Join(P.PermittedCategories,TEXT(","));
  for(auto C:P.OccupiedCells)K+=TEXT("|")+C.ToString();
 }
 for(const auto& O:Options)K+=TEXT("|option:")+O.Id.ToString()+TEXT("|")+O.Name.ToString()+TEXT("|")+O.Detail.ToString()+TEXT("|")+O.Reason.ToString()+TEXT("|")+LexToString(O.bPermitted)+LexToString(O.bAffordable);
 return K;
}
FHansaTradeConstruction Hansa::UI::BuildTradeConstruction(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& R,FHansaHouseId Viewer,FName City,uint64 SelectedLease,FName SelectedBuilding)
{
 FHansaTradeConstruction V;V.City=City;
 V.Status=LOCTEXT("NoLease","No owned lease in this city. Establish a station and earn Merchant quarter rights before construction.");
 if(!Viewer.IsValid()) {V.Status=LOCTEXT("NoViewer","Construction information unavailable. Wait for an authorized house report.");return V;}
 const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Viewer&&X.CityId.ToString()==City.ToString();});
 const auto* House=P.GetHouses().FindByPredicate([&](const auto& X){return X.Id==Viewer;});
 const auto* Policy=R.FindCityTradePolicyForCity(City.ToString());
 for(const auto& Lease:P.GetLeasedPlots())
 {
  if(Lease.OwnerId!=Viewer||Lease.CityId.ToString()!=City.ToString()||!Lease.Id.IsValid())continue;
  const auto* StationReport=P.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Lease.StationId&&S.Station.OwnerId==Viewer&&S.Station.CityId==Lease.CityId;});
  auto& Plot=V.Plots.AddDefaulted_GetRef();
  Plot.Id=Lease.Id.GetValue();Plot.StationId=Lease.StationId.GetValue();Plot.OwnerId=Viewer.GetValue();
  Plot.BoundsMin={Lease.BoundsMin.X,Lease.BoundsMin.Y};Plot.BoundsMax={Lease.BoundsMax.X,Lease.BoundsMax.Y};Plot.PermittedCategories=Lease.PermittedBuildingCategories;
  Plot.bActive=Lease.bActive&&StationReport&&StationReport->Station.Status==EHansaTradeStationStatus::Active&&Presence&&Presence->Status==EHansaForeignPresenceStatus::Active;
  TSet<FIntPoint> Occupied;
  for(const auto Cell:P.GetProtectedCells(City.ToString()))if(Cell.X>=Lease.BoundsMin.X&&Cell.X<=Lease.BoundsMax.X&&Cell.Y>=Lease.BoundsMin.Y&&Cell.Y<=Lease.BoundsMax.Y)Occupied.Add({Cell.X,Cell.Y});
  for(const auto& B:P.GetPlacements())if(B.Spec.CityId==Lease.CityId)for(const auto Cell:B.OccupiedCells)
   if(Cell.X>=Lease.BoundsMin.X&&Cell.X<=Lease.BoundsMax.X&&Cell.Y>=Lease.BoundsMin.Y&&Cell.Y<=Lease.BoundsMax.Y)Occupied.Add({Cell.X,Cell.Y});
  Plot.OccupiedCells=Occupied.Array();Plot.OccupiedCells.Sort([](auto A,auto B){return A.X==B.X?A.Y<B.Y:A.X<B.X;});
  const int64 Width=int64(Lease.BoundsMax.X)-Lease.BoundsMin.X+1,Height=int64(Lease.BoundsMax.Y)-Lease.BoundsMin.Y+1;
  const bool ValidBounds=Width>0&&Height>0&&Width<=MAX_int32&&Height<=MAX_int32;
  Plot.bActive&=ValidBounds;
  const int64 Area=ValidBounds?Width*Height:0;
  Plot.Summary=FText::Format(LOCTEXT("Plot","Lease #{0} · House #{1} · {2}\n{3}\nBounds {4},{5} to {6},{7} (inclusive)\n{8} occupied or protected / {9} cells; {10} unused cells. Space may be fragmented.\nPermitted categories: {11}"),FText::AsNumber(Lease.Id.GetValue()),FText::AsNumber(Viewer.GetValue()),LOCTEXT("LeasedPlotLabel","Leased commercial plot"),Plot.bActive?LOCTEXT("Active","Active lease"):LOCTEXT("Inactive","Inactive or invalid rights — restore station access before building"),FText::AsNumber(Lease.BoundsMin.X),FText::AsNumber(Lease.BoundsMin.Y),FText::AsNumber(Lease.BoundsMax.X),FText::AsNumber(Lease.BoundsMax.Y),FText::AsNumber(Occupied.Num()),FText::AsNumber(Area),FText::AsNumber(FMath::Max<int64>(0,Area-Occupied.Num())),FText::FromString(FString::Join(Lease.PermittedBuildingCategories,TEXT(", "))));
 }
 V.Plots.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
 const auto* Plot=V.Plots.FindByPredicate([&](const auto& X){return X.Id==SelectedLease;});
 if(!Plot&&SelectedLease==0&&!V.Plots.IsEmpty())Plot=&V.Plots[0];
 if(!Plot&&SelectedLease!=0){V.Status=LOCTEXT("LostLease","Selected lease is no longer available. Choose a current owned plot; no replacement is selected automatically.");return V;}
 if(!Plot)return V;
 V.SelectedLease=Plot->Id;
 V.Status=LOCTEXT("Autonomy","Only the entire footprint inside an active lease may be built. The city's streets, buildings, stock and land outside it remain autonomous.");
 const auto* Station=P.GetTradeStations().FindByPredicate([&](const auto& X){return X.Station.Id.GetValue()==Plot->StationId&&X.Station.OwnerId==Viewer;});
 const auto* Inventory=Station?P.GetInventories().FindByPredicate([&](const auto& X){return X.Id==Station->Station.InventoryId;}):nullptr;
 const bool Quarter=Presence&&Presence->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.MerchantQuarter")&&C.bGranted;})&&Policy&&!Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.MerchantQuarter"));
 const auto* Stage=Presence?R.FindPresenceStage(Presence->CurrentStageId):nullptr;
 for(const TCHAR* Id:{TEXT("Building.Market"),TEXT("Building.Warehouse"),TEXT("Building.Dock")})
 {
  const auto* B=R.FindBuilding(Id);if(!B)continue;
  const FString Category=B->ConstructionMenuCategory==TEXT("Harbor")?TEXT("Commercial"):B->ConstructionMenuCategory;
  const bool CategoryAllowed=Plot->PermittedCategories.Contains(Category);
  if(!CategoryAllowed){V.LockedCategories=FText::Format(LOCTEXT("LockedCategory","{0}\n{1}: lease does not permit {2}. Review city policy and presence rights."),V.LockedCategories,FText::FromString(B->DisplayName),FText::FromString(Category));continue;}
  auto& O=V.Options.AddDefaulted_GetRef();O.Id=Id;O.Name=FText::FromString(B->DisplayName);const bool StageAllows=Stage&&Stage->PermittedBuildingCategories.Contains(Category);O.bPermitted=Plot->bActive&&Quarter&&StageAllows;
  O.bAffordable=House&&Inventory&&House->Money.GetRawValue()>=B->ConstructionCostPfennig;
  O.Detail=FText::Format(LOCTEXT("Cost","{0} pfennig from house treasury; {1} × {2} cells.\nWorkforce: {3} laborers / {4} artisans. Roads, shore and logistics remain normal placement requirements.\nStation materials (available excludes reservations):"),FText::AsNumber(B->ConstructionCostPfennig),FText::AsNumber(B->FootprintWidthCells),FText::AsNumber(B->FootprintHeightCells),FText::AsNumber(B->LaborerWorkforce),FText::AsNumber(B->ArtisanWorkforce));
  for(const auto& Cost:B->ConstructionCosts){const auto* Stock=Inventory?Inventory->Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==Cost.GoodId;}):nullptr;const auto* Good=R.FindGood(Cost.GoodId);const int64 Available=Stock?Stock->Available.GetRawValue():0;O.bAffordable&=Inventory&&Available>=Cost.QuantityMilliUnits;
   O.Detail=FText::Format(LOCTEXT("Material","{0}\n{1}: {2} required / {3} available units"),O.Detail,FText::FromString(Good?Good->DisplayName:Cost.GoodId),FText::AsNumber(double(Cost.QuantityMilliUnits)/1000.),Inventory?FText::AsNumber(double(Available)/1000.):LOCTEXT("Unknown","Unavailable"));}
  O.Reason=!Plot->bActive?LOCTEXT("Suspended","Rights inactive. Restore the station and lease before placement."):!Quarter?LOCTEXT("Quarter","Requires active Merchant quarter construction rights. Review Presence."):!StageAllows?LOCTEXT("StageCategory","The current presence stage does not permit this building category. Review presence rights."):!O.bAffordable?LOCTEXT("Resources","Resources unavailable or insufficient. Deliver unreserved materials to this station and replenish the treasury."):LOCTEXT("Review","Resources available. Placement still requires a valid, unoccupied footprint wholly within this lease.");
 }
 if(V.Options.ContainsByPredicate([&](const auto& O){return O.Id==SelectedBuilding;}))V.SelectedBuilding=SelectedBuilding;
 return V;
}
#undef LOCTEXT_NAMESPACE
