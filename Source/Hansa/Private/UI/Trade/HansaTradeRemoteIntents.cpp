#include "UI/HansaTradeMapPresentationModel.h"
#include "Queries/HansaSimulationReadOnly.h"
using namespace Hansa::Simulation;

bool UHansaTradeMapPresentationModel::IsAnyTradeCommandPending() const
{
 return Snapshot.bCommandPending||bOrderPending||bEstablishmentPending||bPresencePending||Snapshot.bSpecializationPending||Snapshot.bDecisionPending||Snapshot.bRecoveryPending;
}

FText UHansaTradeMapPresentationModel::RemoteActionReason(const FString& Id) const
{
 if(!bRemoteEstablishment)return FText();
 if(IsAnyTradeCommandPending())return FText::FromString(TEXT("Awaiting the authoritative result. Keep this context until the request completes."));
 if(Id==TEXT("TradeMap.New"))return FText::FromString(TEXT("Creating a new route is unavailable in this remote view: the complete route-planning permission report is not provided. Existing owned routes can be inspected and edited."));
 if(Id==TEXT("TradeMap.Construction.Place")||Id==TEXT("TradeMap.Construction.Visit")||Id==TEXT("TradeMap.Editor.Visit"))return FText::FromString(TEXT("World visit and leased placement are unavailable in this remote view. You can inspect current rights, costs and plots here; the remote world-placement session is not supported."));
 return FText();
}

void UHansaTradeMapPresentationModel::ResetRemoteOwnerState()
{
 // A new viewer must never inherit private drafts, reviews, pending nonces or selections.
 const bool Open=Snapshot.bOpen,Compact=Snapshot.bCompact;Snapshot={};Snapshot.bOpen=Open;Snapshot.bCompact=Compact;Snapshot.Title=FText::FromString(TEXT("Baltic trade"));Snapshot.ActiveSection=TEXT("Overview");Snapshot.FocusedSemanticId=TEXT("TradeMap.Close");
 DraftStops.Reset();AllRoutes.Reset();AllCities.Reset();AvailableGoods.Reset();RemoteRoutes.Reset();RemoteVehicles.Reset();CargoEditor={};bShipDetailOpen=false;RemoteWorkspace={};
 LastProjection.Reset();LastRegistry=nullptr;OrderDraft={};StationOrders.Reset();OrderGoods.Reset();StationOrderRows.Reset();SelectedStationOrder=0;
 PendingSequence=PendingNonce=0;bPendingCreate=false;bPendingRouteEdit=false;ReviewedVoyage.Reset();
 RemoteDraftPlanKey.Reset();
 bOrderPending=false;OrderSequence=OrderNonce=0;
 RecoveryStation=RecoverySequence=RecoveryNonce=0;RecoveryReviewedKey.Reset();Recoveries.Reset();
 DecisionSequence=DecisionNonce=0;DecisionReviewedKey.Reset();SpecializationSequence=SpecializationNonce=0;SpecializationReviewedKey.Reset();
 bPresencePending=false;PresencePendingSequence=PresencePendingNonce=0;PresenceReviewKey.Reset();SelectedPresenceSource.Reset();PresenceFeedback=FText();
 bEstablishmentPending=false;bConstructionPriorityReview=false;EstablishmentSequence=EstablishmentNonce=0;EstablishmentReviewKey.Reset();SelectedFundingSource.Reset();SelectedStationSiteId.Reset();EstablishmentFeedback=FText();
 SelectedConstructionLease=0;SelectedConstructionBuilding=NAME_None;ConstructionReturnFocus=NAME_None;RemoteRevision=0;
}

bool UHansaTradeMapPresentationModel::ReceiveOrderFeedback(const FHansaClientCommandFeedback& F)
{
 if(!bOrderPending)return false;
 if(F.State==EHansaClientCommandState::Pending){if(!OrderSequence){OrderSequence=F.ClientSequence;OrderNonce=F.ClientNonce;}return true;}
 if(!OrderSequence||OrderSequence!=F.ClientSequence||OrderNonce!=F.ClientNonce)return false;
 const auto Before=Snapshot;bOrderPending=false;OrderSequence=OrderNonce=0;
 Snapshot.StationOrderFeedback=FText::FromString(F.Message+TEXT(" ")+F.Remedy);RefreshStationOrderText();PublishIfChanged(Before);return true;
}

void UHansaTradeMapPresentationModel::RefreshRemoteTradeSelection()
{
 if(!bRemoteEstablishment)return;
 AllRoutes=RemoteWorkspace.Routes;
 const auto* Selected=AllRoutes.FindByPredicate([&](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});
 if(Snapshot.SelectedRouteValue&&(!Selected||!Selected->bOwnedByPlayer)&&Snapshot.bDirty){Snapshot.bDirty=false;Snapshot.bReview=false;DraftStops.Reset();Snapshot.EditorStatus=FText::FromString(TEXT("Route ownership or visibility changed. Private draft cleared; request a current authorized report."));}
 if(!Selected&&Snapshot.SelectedRouteValue){Snapshot.SelectedRouteValue=0;Snapshot.SelectedVehicleValue=0;DraftStops.Reset();}
 if(!Snapshot.SelectedRouteValue&&!Snapshot.SelectedVehicleValue&&!Snapshot.bCreating){
  Selected=AllRoutes.FindByPredicate([](const auto& R){return R.bOwnedByPlayer&&!R.bCancelled;});
  if(Selected){Snapshot.SelectedRouteValue=Selected->RouteValue;Snapshot.SelectedVehicleValue=Selected->VehicleValue;}
 }
 if(!Snapshot.bDirty&&!Snapshot.bCreating){
  DraftStops.Reset();
  const auto* Base=RemoteRoutes.FindByPredicate([&](const auto& X){return X.RouteId==Snapshot.SelectedRouteValue&&X.OwnerHouseId==int64(ViewerHouse.GetValue());});RemoteDraftPlanKey=Base?Base->PlanKey:FString();
  if(const auto* R=RemoteRoutes.FindByPredicate([&](const auto& X){return X.RouteId==Snapshot.SelectedRouteValue&&X.OwnerHouseId==int64(ViewerHouse.GetValue());}))
   for(const auto& Stop:R->Stops){const auto City=FHansaCityDefinitionId::TryParse(Stop.CityId);if(!City)continue;FHansaRouteStop D;D.CityId=City.Value;for(const auto& A:Stop.Actions){const auto Good=FHansaGoodId::TryParse(A.GoodId);if(!Good)continue;FHansaRouteCargoAction C;C.CargoSlotIndex=A.CargoSlotIndex;C.Kind=static_cast<EHansaRouteCargoActionKind>(A.Kind);C.GoodId=Good.Value;C.QuantityLimit=FHansaQuantity::FromRaw(A.QuantityMilliUnits);C.MinimumSourceReserve=FHansaQuantity::FromRaw(A.MinimumSourceReserveMilliUnits);D.Actions.Add(C);}DraftStops.Add(D);}
 }
 const auto* Inspector=RemoteWorkspace.Inspectors.FindByPredicate([&](const auto& I){return I.CityId==Snapshot.SelectedCityStableId;});Snapshot.CityInspector=Inspector?*Inspector:FHansaTradeCityInspector();
 Snapshot.bShipInTransit=false;
 RebuildFilteredProjection();RebuildStops();
}

bool UHansaTradeMapPresentationModel::DiscardRouteEditsIntent()
{
 if(IsAnyTradeCommandPending()||Snapshot.bCreating||!Snapshot.bDirty)return false;
 const auto Before=Snapshot;Snapshot.bDirty=false;
 if(bRemoteEstablishment)RefreshRemoteTradeSelection();
 else if(LastProjection&&LastRegistry){const auto P=LastProjection;ApplyProjection(*P,*LastRegistry);}
 Snapshot.EditorStatus=FText::FromString(TEXT("Unsaved edits discarded. Current authoritative route loaded."));PublishIfChanged(Before);return true;
}

void UHansaTradeMapPresentationModel::RebuildRemoteDirectory()
{
 const auto& Source=Snapshot.bFleetView?RemoteWorkspace.FleetDirectory:RemoteWorkspace.RouteDirectory;
 for(const auto& E:Source){
  if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Sea&&!E.bSea)continue;
  if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Land&&E.bSea)continue;
  if(Snapshot.CityFilter==EHansaTradeMapCityFilter::Presence&&!E.bPresence)continue;
  if(Snapshot.CityFilter==EHansaTradeMapCityFilter::Routes&&!E.RouteValue)continue;
  const int32 F=Snapshot.DirectoryFilter;
  if((F==1&&!E.bActive)||(F==2&&(E.bActive||!E.RouteValue))||F==3||(F==4&&!E.bAttention)||(F==5&&!E.bAvailable)||(F==6&&!E.bPresence)||(F==7&&!E.Goods.Contains(Snapshot.PreferredGoodStableId)))continue;
  if(!Snapshot.CitySearchText.IsEmpty()&&!E.Label.ToString().Contains(Snapshot.CitySearchText)&&!E.Detail.ToString().Contains(Snapshot.CitySearchText))continue;
  Snapshot.Directory.Add(E);
 }
}

void UHansaTradeMapPresentationModel::RefreshRemoteCityReports()
{
 // No private report is inferred from public map metadata.
 for(auto& C:AllCities){
  const auto* Market=RemoteMarkets.FindByPredicate([&](const auto& X){return X.CityId==C.StableId.ToString()&&X.GoodId==Snapshot.PreferredGoodStableId.ToString();});
  C.bUnknown=!Market||Market->CurrentPriceMilliMarks<=0;C.bStale=Market&&Market->bStale;C.ReportAgeTicks=Market?Market->ReportAgeTicks:0;
  C.ReportedPriceMilliMarks=C.bUnknown?-1:Market->CurrentPriceMilliMarks;
  C.ReportedStockMilliUnits=C.bUnknown?-1:Market->StockMilliUnits;
  C.Information=FText::FromString(C.bUnknown?TEXT("No authorized report"):FString::Printf(TEXT("%s report · %lld ticks old"),C.bStale?TEXT("Older"):TEXT("Current"),C.ReportAgeTicks));
  C.GoodReport=FText::FromString(C.bUnknown?TEXT("Price unknown · stock unknown"):FString::Printf(TEXT("Price %.3f Mark · stock %s"),double(Market->CurrentPriceMilliMarks)/1000.,Market->StockMilliUnits<0?TEXT("unknown"):*FString::Printf(TEXT("%.3f units"),double(Market->StockMilliUnits)/1000.)));
 }
}
