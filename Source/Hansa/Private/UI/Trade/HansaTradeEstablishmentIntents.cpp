#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "../HansaTradeCityLocations.inl"
#define LOCTEXT_NAMESPACE "HansaTradeEstablishmentIntents"
using namespace Hansa::Simulation;
void UHansaTradeMapPresentationModel::RefreshRemoteStationOrders() {
 StationOrders.Reset();OrderGoods.Reset();StationOrderCapacity=0;bStationOrdersWritable=false;
 const auto* View=RemoteStationOrders.FindByPredicate([&](const auto& X){return X.City==Snapshot.SelectedCityStableId;});
 if(!View){Snapshot.TradeStationValue=0;RefreshStationOrderText();return;}
 bStationOrdersWritable=View->bOperational;Snapshot.TradeStationValue=View->StationId;StationOrderCapacity=View->CapacityMilliUnits;
 StationOrderMaxCap=View->MaximumCapMilliUnits>0?View->MaximumCapMilliUnits:50000;
 StationOrderMaxBudget=View->MaximumBudgetPfennig>0?View->MaximumBudgetPfennig:1000000;
 for(const auto& Id:View->GoodIds){const auto Good=FHansaGoodId::TryParse(Id);if(Good&&!OrderGoods.Contains(Good.Value))OrderGoods.Add(Good.Value);}
 for(const auto& Market:RemoteMarkets)if(Market.CityId==Snapshot.SelectedCityStableId.ToString()){
  const auto Good=FHansaGoodId::TryParse(Market.GoodId);if(Good&&!OrderGoods.Contains(Good.Value))OrderGoods.Add(Good.Value);
 }
 for(const auto& Item:View->Orders) {
  const auto Good=FHansaGoodId::TryParse(Item.GoodId);if(!Good)continue;
  FHansaStationOrderState O;O.Id=uint64(Item.Id);O.Terms.GoodId=Good.Value;
  O.Terms.Side=static_cast<EHansaStationOrderSide>(Item.Side);
  O.Terms.TargetOrReserveMilliUnits=Item.TargetMilliUnits;O.Terms.CapMilliUnits=Item.CapMilliUnits;
  O.Terms.TotalBudgetPfennig=Item.BudgetPfennig;O.Terms.LimitUnitPriceMilliMarks=Item.LimitUnitPriceMilliMarks;
  O.Terms.ReviewedMarketUpdateTick=Item.ReviewedMarketUpdateTick;
  O.Terms.ReviewedUnitPriceMilliMarks=Item.ReviewedUnitPriceMilliMarks;
  O.SpentPfennig=Item.SpentPfennig;O.NextUpdateTick=Item.NextUpdateTick;
  O.bPaused=Item.bPaused;O.bCancelled=Item.bCancelled;
  for(const auto& Event:Item.History){
   FHansaStationOrderExecution E;E.Tick=Event.Tick;E.RequestedMilliUnits=Event.RequestedMilliUnits;
   E.AppliedMilliUnits=Event.AppliedMilliUnits;E.MoneyDelta=Event.MoneyDelta;
   E.Outcome=static_cast<EHansaStationOrderOutcome>(Event.Outcome);
   E.Blocker=static_cast<EHansaStationOrderBlocker>(Event.Blocker);O.History.Add(E);
  }
  StationOrders.Add(MoveTemp(O));
  if(!OrderGoods.Contains(Good.Value))OrderGoods.Add(Good.Value);
 }
 OrderGoods.Sort();
 if(SelectedStationOrder&&!StationOrders.ContainsByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;}))SelectedStationOrder=0;
 if(!SelectedStationOrder&&!OrderDraft.GoodId.IsValid()&&!OrderGoods.IsEmpty()){
  OrderDraft.GoodId=OrderGoods[0];OrderDraft.TargetOrReserveMilliUnits=FMath::Min<int64>(10000,StationOrderCapacity);
  OrderDraft.TotalBudgetPfennig=FMath::Min<int64>(10000,StationOrderMaxBudget);
 }
 RefreshStationOrderText();
}
void UHansaTradeMapPresentationModel::RefreshEstablishment() {
 if(bRemoteEstablishment){
  const auto* V=RemoteEstablishments.FindByPredicate([&](const auto& X){return X.CityId==Snapshot.SelectedCityStableId&&(X.StationId||X.SiteId==SelectedStationSiteId);});
  Snapshot.Establishment=V?*V:FHansaTradeEstablishment();
  auto& E=Snapshot.Establishment;if((!E.bLocalDelivery&&!E.bAwaitingPickup)||!SelectedFundingSource.IsEmpty())E.SourceId=SelectedFundingSource;
  const auto* Source=E.Sources.FindByPredicate([&](const auto& X){return X.Id==E.SourceId;});
  E.SourceDetail=Source?Source->Detail:E.SourceId.IsEmpty()?LOCTEXT("RemoteChoose","Choose a funding inventory. No source is selected automatically."):LOCTEXT("RemoteLost","Selected inventory is no longer owned or visible. Choose an eligible inventory.");
  if(E.bProposed&&(!E.bAwaitingPickup||E.bLocalDelivery)&&Source)E.Action=Source->Action;
  E.bCanFund=(E.bProposed||E.bArrears)&&Source&&Source->bEligible&&!E.SiteId.IsEmpty();
  if(!E.bAwaitingPickup&&(E.bProposed||E.bArrears))E.Blocker=E.bCanFund?LOCTEXT("RemoteFundingReady","Not funded yet. Review the terms, then confirm payment. Loading cargo alone does not start construction."):Source?LOCTEXT("RemoteSelectedSourceShort","Selected inventory cannot fund construction. Check the shortfalls and route instructions above."):LOCTEXT("RemoteFundingMissing","Choose an eligible inventory and check its funds and materials.");
  if(E.bProposed||E.bArrears)E.Confirmation=FText::Format(LOCTEXT("RemoteTransfer","Confirm exact spending\n{0}\n{1}"),Source?Source->Transfer:E.SourceDetail,E.FundingTerms);
  E.ReviewKey+=TEXT("|")+E.SourceId+(E.bProposed||E.bArrears?(Source?Source->ReviewKey:FString()):E.SourceDetail.ToString());
  Snapshot.CityInspector.CityId=E.CityId;Snapshot.CityInspector.Identity=LOCTEXT("RemoteAutonomy","Autonomous city");Snapshot.CityInspector.Presence=E.Action;Snapshot.CityInspector.Overview=E.Summary;Snapshot.CityInspector.MarketAccess=LOCTEXT("RemoteMarket","Market report unavailable in this station review.");Snapshot.CityInspector.PrimaryAction=LOCTEXT("RemoteOpen","Review station establishment");Snapshot.CityInspector.PrimarySection=TEXT("Presence");
  Snapshot.TradeStationValue=E.StationId;Snapshot.TradeStationState=E.Action;Snapshot.TradeStationDetail=E.OperationsSummary;RefreshRemoteStationOrders();
 }else{
  if(!LastProjection||!LastRegistry)return;
  Snapshot.Establishment=Hansa::UI::BuildTradeEstablishment(*LastProjection,*LastRegistry,Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse,Snapshot.SelectedCityStableId,SelectedStationSiteId,SelectedFundingSource);
 }
 auto& E=Snapshot.Establishment;
 E.bPriorityReview=bConstructionPriorityReview;
 if(E.bLocalDelivery&&E.bProposed){
  const auto* Source=E.Sources.FindByPredicate([&](const auto& C){return C.Id==E.SourceId;});
  E.bCanPrioritizeDelivery=Source&&Source->bEligible&&Source->DeliveryMode==1&&!Source->bPriorityActive;
  if(bRemoteEstablishment){E.Blocker=Source?E.bAwaitingPickup?Source->DeliveryStatus:LOCTEXT("RemoteSuggested","Suggested delivery is not yet arranged. Choose Arrange delivery to use this source."):LOCTEXT("RemoteNoDelivery","No delivery arranged. Choose another ship, create a route, or build a Cog.");E.Summary=E.Blocker;}
  if(bConstructionPriorityReview&&Source)E.Confirmation=FText::Format(LOCTEXT("PriorityTerms","{0}\n{1}"),Source->PriorityTransfer,E.FundingTerms);
  E.ReviewKey+=bConstructionPriorityReview?TEXT("|Priority"):TEXT("|Wait");
 }
 if(!EstablishmentReviewKey.IsEmpty()&&EstablishmentReviewKey!=E.ReviewKey&&!bEstablishmentPending){EstablishmentReviewKey.Reset();EstablishmentFeedback=LOCTEXT("Stale","Review changed: stock, treasury, site or access differs. Your choices are retained. Review the current terms again; nothing was spent.");}
 E.bReview=!EstablishmentReviewKey.IsEmpty();E.bPending=bEstablishmentPending;E.Feedback=EstablishmentFeedback;
 if(E.bVisible){if(E.StationId)Snapshot.TradeStationState=E.State;Snapshot.TradeStationAction=E.Action;Snapshot.bCanTradeStationAction=CanEstablishmentIntent(TEXT("Review"));}
}
void UHansaTradeMapPresentationModel::ApplyRemoteEstablishment(const FHansaClientProjectionSnapshot& P) {
 if(Runtime.IsValid())return;
 if(P.SchemaVersion!=FHansaClientProjectionSnapshot::CurrentSchemaVersion||P.OwnerHouseId<=0){const auto Before=Snapshot;ResetRemoteOwnerState();RemoteEstablishments.Reset();RemotePresences.Reset();RemoteLedgers.Reset();RemoteStationOrders.Reset();RemoteMarkets.Reset();ViewerHouse={};PublishIfChanged(Before);return;}
 const bool OwnerChanged=ViewerHouse.GetValue()!=uint64(P.OwnerHouseId);
 if(!OwnerChanged&&P.Revision>0&&P.Revision<=RemoteRevision)return;
 const auto Before=Snapshot;
 if(OwnerChanged)ResetRemoteOwnerState();
 bRemoteEstablishment=true;Snapshot.bHasProjection=true;RemoteRevision=P.Revision;
 Recoveries=P.Recoveries;RemoteWorkspace=P.TradeWorkspace;RemoteRoutes=P.Routes;RemoteVehicles=P.Vehicles;
 ViewerHouse=FHansaHouseId::TryCreate(uint64(P.OwnerHouseId)).Value;RemoteEstablishments=P.StationEstablishments;RemotePresences=P.Presences;RemoteLedgers=P.StationLedgers;RemoteStationOrders=P.StationOrders;RemoteMarkets=P.Markets;
 AllCities=RemoteWorkspace.Cities;AvailableGoods.Reset();for(const auto& G:RemoteWorkspace.GoodIds)AvailableGoods.Add(FName(*G));
 if(Snapshot.PreferredGoodStableId.IsNone()&&!AvailableGoods.IsEmpty())Snapshot.PreferredGoodStableId=AvailableGoods.Contains(TEXT("Good.Grain"))?FName(TEXT("Good.Grain")):AvailableGoods[0];
 RefreshRemoteCityReports();
 if(!AllCities.IsEmpty()&&!AllCities.ContainsByPredicate([&](const auto& X){return X.StableId==Snapshot.SelectedCityStableId;}))Snapshot.SelectedCityStableId=AllCities[0].StableId;
 RefreshEstablishment();RefreshRemotePresence();RefreshStationOrderEditor();Snapshot.LedgerKey=GetLedgerPresentation().Key();Snapshot.ConstructionKey=GetConstructionPresentation().Key();PublishIfChanged(Before);
}
void UHansaTradeMapPresentationModel::RefreshRemotePresence()
{
 RefreshRecovery();
 const auto* V=RemotePresences.FindByPredicate([&](const auto& X){return X.City==Snapshot.SelectedCityStableId;});
 Snapshot.Decisions=V?V->Decisions:FHansaTradeDecisions();RefreshDecisions();
 Snapshot.Specialization=V?V->Specialization:FHansaTradeSpecialization();RefreshSpecialization();
 const FText PriorStatus=Snapshot.PresenceStatus;
 Snapshot.bPresenceOfficeVisual=false;Snapshot.PresenceRequirements.Reset();Snapshot.PresenceRequirementsKey.Reset();Snapshot.PresenceSources.Reset();Snapshot.PresenceStageSummary=FText();Snapshot.PresenceConsequences=FText();Snapshot.PresenceFundingDetail=FText();Snapshot.PresenceHistory=FText();Snapshot.PresenceReview=FText();Snapshot.PresenceProgress=FText();Snapshot.PresenceStatus=FText();
 Snapshot.bCanPresenceUpgradeAction=false;Snapshot.PresenceUpgradeAction=LOCTEXT("NoRemoteOffice","No office upgrade available");PresenceUpgradeStageId.Reset();bPresenceUpgradeFunding=false;PresenceFundingInventoryId=FHansaInventoryId();
 if(!V){Snapshot.bPresenceReview=false;return;}
 Snapshot.bPresenceOfficeVisual=V->bOfficeVisual;PresenceUpgradeStageId=V->NextStageId;bPresenceUpgradeFunding=V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::Requested);
 const FString State=V->Status==TEXT("Active")?(V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::Funded)?TEXT("Under construction"):bPresenceUpgradeFunding?TEXT("Awaiting funding"):V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::AwaitingMaterials)?TEXT("Awaiting materials"):TEXT("Active")):V->Status;
 Snapshot.PresenceStatus=PriorStatus.ToString()==TEXT("Suspended")&&State==TEXT("Active")?LOCTEXT("PresenceRecovered","Recovered"):FText::FromString(State);
 FString Summary=V->CurrentStage+TEXT(" · ")+State+TEXT("\n");
 if(!V->NextStage.IsEmpty())Summary+=TEXT("Next: ")+V->NextStage;
 else Summary+=TEXT("Current commercial stage is active");
 Snapshot.PresenceStageSummary=FText::FromString(Summary);Snapshot.PresenceProgress=Snapshot.PresenceStageSummary;
 Snapshot.PresenceConsequences=FText::FromString(TEXT("Progress: lawful trade, deliveries, qualifying investment, and reliable operation.\nOn completion: ")+(V->Unlocks.IsEmpty()?FString(TEXT("No further stage defined.")):V->Unlocks)+TEXT("\nCommercial: only the listed trading rights. Physical: only owned facilities or separately leased plots. Operations: only house facilities. Civic: no city authority; a separate charter is required."));
 Snapshot.PresenceHistory=FText::FromString(V->History.IsEmpty()?TEXT("No recorded contributions yet."):V->History);
 for(const auto& R:V->Requirements){FHansaPresenceRequirementProjection Q;Q.RequirementId=R.Id;Q.Description=R.Description;Q.CurrentValue=R.Current;Q.RequiredValue=R.Required;Q.bMet=R.bMet;Snapshot.PresenceRequirementsKey+=FString::Printf(TEXT("|%s:%lld:%lld:%d"),*Q.RequirementId,Q.CurrentValue,Q.RequiredValue,Q.bMet);Snapshot.PresenceRequirements.Add(MoveTemp(Q));}
 if(V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::Funded)){Snapshot.PresenceUpgradeAction=LOCTEXT("OfficeBuilding","Under construction");Snapshot.PresenceFundingDetail=FText::Format(LOCTEXT("RemoteOfficeTimeRemaining","Funded. All required materials received. Ready in {0}."),Hansa::UI::PresenceDuration(FMath::Max<int64>(0,V->CompletionTick-Snapshot.Establishment.Tick),Snapshot.Establishment.MinutesPerTick));}
 else if(V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::AwaitingMaterials)){Snapshot.PresenceUpgradeAction=LOCTEXT("RemoteUpgradeAwaiting","Upgrade paid · awaiting materials");Snapshot.PresenceFundingDetail=FText::FromString(V->ConstructionDelivery);}
 else if(!V->NextStageId.IsEmpty()){
  Snapshot.PresenceSources=V->Sources;const auto* Chosen=V->Sources.FindByPredicate([&](const auto& X){return X.Id==SelectedPresenceSource;});
  if(Chosen){PresenceFundingInventoryId=FHansaInventoryId::TryCreate(FCString::Strtoui64(*Chosen->Id,nullptr,10)).Value;Snapshot.PresenceFundingDetail=Chosen->Detail;}
  else if(bPresenceUpgradeFunding)Snapshot.PresenceFundingDetail=LOCTEXT("ChooseOfficeSource","Choose an owned station, home, or ship inventory. No source is selected automatically.");
  Snapshot.bCanPresenceUpgradeAction=!bPresencePending&&V->Status==TEXT("Active")&&(bPresenceUpgradeFunding?Chosen&&Chosen->bEligible:V->bProgressMet);
  Snapshot.PresenceUpgradeAction=bPresenceUpgradeFunding?LOCTEXT("ReviewOfficeFunding","Review office funding"):LOCTEXT("ReviewOfficeProgress","Review office conditions");
  Snapshot.PresenceReview=FText::FromString(bPresenceUpgradeFunding?FString::Printf(TEXT("Fund %s using %s.\n%s\nConstruction begins after accepted payment. The city remains autonomous."),*V->NextStage,Chosen?*Chosen->Label.ToString():TEXT("no selected inventory"),*Snapshot.PresenceFundingDetail.ToString()):FString::Printf(TEXT("Request review for %s. This step spends no money or materials. Funding follows as a separate choice."),*V->NextStage));
 }
 Snapshot.PresenceSourceId=SelectedPresenceSource;
 if(IsAutomaticMerchantOfficeUpgrade()&&(V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::None)||V->UpgradeStatus==static_cast<uint8>(EHansaPresenceUpgradeStatus::Requested)))ConfigureMerchantOfficeUpgrade(V->bProgressMet);
 const FString Key=PresenceReviewSignature();
 if(!PresenceReviewKey.IsEmpty()&&PresenceReviewKey!=Key){PresenceReviewKey.Reset();PresenceFeedback=LOCTEXT("OfficeStale","Review changed. Check current progress, treasury, and source stock; nothing was spent.");}
 Snapshot.bPresenceReview=!PresenceReviewKey.IsEmpty();
 if(Snapshot.bPresenceReview)Snapshot.PresenceUpgradeAction=IsAutomaticMerchantOfficeUpgrade()?LOCTEXT("ConfirmDirectOffice","Confirm Merchant Office upgrade"):bPresenceUpgradeFunding?LOCTEXT("ConfirmOfficeFunding","Confirm office funding"):LOCTEXT("ConfirmOfficeRequest","Submit office review");
 if(!PresenceFeedback.IsEmpty())Snapshot.PresenceReview=FText::Format(LOCTEXT("OfficeReviewFeedback","{0}\n{1}"),Snapshot.PresenceReview,PresenceFeedback);
}
bool UHansaTradeMapPresentationModel::SelectEstablishmentSite(const FString& Id) {
 if(bEstablishmentPending||Snapshot.Establishment.StationId||!Snapshot.Establishment.Sites.ContainsByPredicate([&](const auto& X){return X.Id==Id;}))return false;
 const auto Before=Snapshot;SelectedStationSiteId=Id;EstablishmentReviewKey.Reset();EstablishmentFeedback=FText();RefreshEstablishment();PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::SelectEstablishmentSource(const FString& Id) {
 if(bEstablishmentPending||(Snapshot.Establishment.bAwaitingPickup&&!Snapshot.Establishment.bLocalDelivery)||!Snapshot.Establishment.Sources.ContainsByPredicate([&](const auto& X){return X.Id==Id;}))return false;
 const auto Before=Snapshot;SelectedFundingSource=Id;bConstructionPriorityReview=false;EstablishmentReviewKey.Reset();EstablishmentFeedback=FText();RefreshEstablishment();PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::CanEstablishmentIntent(const FString& A) const {
 const auto& E=Snapshot.Establishment;if(!E.bVisible||IsAnyTradeCommandPending())return false;
 if(A==TEXT("Priority"))return E.bLocalDelivery&&E.bProposed&&E.bCanPrioritizeDelivery&&!E.bReview;
 if(A==TEXT("Arrange"))return E.bLocalDelivery&&E.bProposed&&E.bCanFund&&!E.bReview;
 if(A==TEXT("ShowOnMap"))return (E.bComplete||E.bLocalDelivery)&&E.StationId>0&&!E.bReview&&bool(StationMapRequested);
 if(A==TEXT("Terms"))return !E.bReview;
 if(A==TEXT("Site"))return !E.bReview&&!E.StationId&&!E.Sites.IsEmpty();
 if(A==TEXT("Source"))return (!E.bAwaitingPickup||E.bLocalDelivery)&&!E.bReview&&(E.bProposed||E.bArrears)&&!E.Sources.IsEmpty();
 if(A==TEXT("Cancel"))return E.bReview;
 if(A==TEXT("Close"))return E.StationId>0&&!E.bReview;
 if(A==TEXT("Confirm"))return E.bReview&&(E.bProposed||E.bArrears?E.bCanFund:E.bCanPropose);
 if(A==TEXT("Review"))return !E.bReview&&(E.bComplete&&!E.bArrears?true:(E.bProposed||E.bArrears?E.bCanFund:E.bCanPropose));
 return false;
}
bool UHansaTradeMapPresentationModel::EstablishmentIntent(const FString& A) {
 if(A==TEXT("Arrange")){
  if(!CanEstablishmentIntent(A))return false;bConstructionPriorityReview=false;RefreshEstablishment();
  if(!EstablishmentIntent(TEXT("Review")))return false;
  return EstablishmentIntent(TEXT("Confirm"));
 }
 if(A==TEXT("Priority")){
  if(!CanEstablishmentIntent(A))return false;bConstructionPriorityReview=true;RefreshEstablishment();
  return EstablishmentIntent(TEXT("Review"));
 }
 if(A==TEXT("ShowOnMap"))return CanEstablishmentIntent(A)&&StationMapRequested(Snapshot.SelectedCityStableId,Snapshot.Establishment.StationId);
 if(A==TEXT("Close"))return OpenRecovery(Snapshot.SelectedCityStableId,Snapshot.TradeStationValue,TEXT("TradeMap.Station.Close"));
 if(!CanEstablishmentIntent(A))return false;
 const auto Before=Snapshot;
 if(A==TEXT("Site")||A==TEXT("Source")){
  const auto& Items=A==TEXT("Site")?Snapshot.Establishment.Sites:Snapshot.Establishment.Sources;
  const FString Current=A==TEXT("Site")?SelectedStationSiteId:SelectedFundingSource;
  const int32 I=Items.IndexOfByPredicate([&](const auto& X){return X.Id==Current;});const FString Id=Items[(I+1)%Items.Num()].Id;
  return A==TEXT("Site")?SelectEstablishmentSite(Id):SelectEstablishmentSource(Id);
 }
 if(A==TEXT("Cancel")){bConstructionPriorityReview=false;EstablishmentReviewKey.Reset();EstablishmentFeedback=LOCTEXT("Cancelled","Review cancelled. Nothing was submitted or spent.");RefreshEstablishment();PublishIfChanged(Before);FocusRestoreRequested.Broadcast(TEXT("TradeMap.Station.Action"));return true;}
 if(A==TEXT("Review")&&Snapshot.Establishment.bComplete&&!Snapshot.Establishment.bArrears){bWorldStationDetail=false;Snapshot.ActiveSection=TEXT("Ledger");Snapshot.FocusedSemanticId=TEXT("TradeMap.Ledger.Stock");PublishIfChanged(Before);FocusRestoreRequested.Broadcast(Snapshot.FocusedSemanticId);return true;}
 // Local confirmation must compare a fresh authoritative projection, even if no UI event has arrived.
 if(Runtime.IsValid()){const auto P=Runtime->BuildProjection();if(P)ApplyProjection(P.Value,*Runtime->GetEconomicRegistry());}
 if(!CanEstablishmentIntent(A)){PublishIfChanged(Before);return false;}
 if(A==TEXT("Review")){EstablishmentReviewKey=Snapshot.Establishment.ReviewKey;EstablishmentFeedback=FText();RefreshEstablishment();PublishIfChanged(Before);FocusRestoreRequested.Broadcast(TEXT("TradeMap.Station.Confirm"));return true;}
 if(A!=TEXT("Confirm")&&A!=TEXT("Close"))return false;
 const auto E=Snapshot.Establishment;
 FHansaClientCommandIntent Intent;Intent.CityId=Snapshot.SelectedCityStableId.ToString();Intent.TradeStationSiteId=E.SiteId;Intent.TradeStationId=E.StationId;Intent.FundingInventoryId=FCString::Atoi64(*E.SourceId);
 const auto* DeliverySource=E.Sources.FindByPredicate([&](const auto& C){return C.Id==E.SourceId;});
 Intent.ConstructionDeliveryMode=E.bLocalDelivery&&E.bProposed&&DeliverySource?(bConstructionPriorityReview?2:DeliverySource->DeliveryMode):0;
 Intent.Type=A==TEXT("Close")?EHansaClientIntentType::CloseTradeStation:E.bProposed||E.bArrears?EHansaClientIntentType::FundTradeStation:EHansaClientIntentType::ProposeTradeStation;
 bool Sent=false;
 if(NetworkCommandIntent){
  // Existing optimistic-concurrency contract rejects a stale remote tick atomically.
  if(E.Tick<=0){EstablishmentFeedback=LOCTEXT("InitialUpdate","Wait for the first game update, then review again.");EstablishmentReviewKey.Reset();RefreshEstablishment();PublishIfChanged(Before);return false;}
  Intent.ExpectedServerTick=E.Tick;bEstablishmentPending=true;EstablishmentSequence=EstablishmentNonce=0;
  Sent=NetworkCommandIntent(Intent);
  if(!Sent){bEstablishmentPending=false;EstablishmentFeedback=LOCTEXT("NotSent","The station command was not sent. Reconnect and review again; choices are retained.");EstablishmentReviewKey.Reset();}
  else if(bEstablishmentPending)EstablishmentFeedback=LOCTEXT("Pending","Awaiting authority. No success is assumed until the command acknowledgement and station projection arrive.");
 }else if(Runtime.IsValid()){
  FHansaCommandGatewayResult Result;
  if(Intent.Type==EHansaClientIntentType::ProposeTradeStation){FHansaTradeStationId Id;Result=Runtime->ProposeTradeStation(FHansaCityDefinitionId::TryParse(Intent.CityId).Value,Intent.TradeStationSiteId,Id);}
  else if(Intent.Type==EHansaClientIntentType::FundTradeStation)Result=Runtime->FundTradeStation(FHansaTradeStationId::TryCreate(uint64(Intent.TradeStationId)).Value,FHansaInventoryId::TryCreate(uint64(Intent.FundingInventoryId)).Value,Intent.ConstructionDeliveryMode);
  else Result=Runtime->CloseTradeStation(FHansaTradeStationId::TryCreate(uint64(Intent.TradeStationId)).Value);
  Sent=Result.IsSuccess();EstablishmentReviewKey.Reset();
  EstablishmentFeedback=Sent?LOCTEXT("Accepted","Command accepted. The station below shows authoritative progress."):FText::Format(LOCTEXT("Rejected","Rejected: {0}. No resources changed. Choices retained; review current access, site, ownership and stock before retrying."),FText::FromString(LexToString(Result.GetError())));
  const auto P=Runtime->BuildProjection();if(P)ApplyProjection(P.Value,*Runtime->GetEconomicRegistry());
 }
 RefreshEstablishment();PublishIfChanged(Before);return Sent;
}
bool UHansaTradeMapPresentationModel::ReceiveEstablishmentFeedback(const FHansaClientCommandFeedback& F) {
 if(!bEstablishmentPending)return false;
 if(F.State==EHansaClientCommandState::Pending){if(EstablishmentSequence==0){EstablishmentSequence=F.ClientSequence;EstablishmentNonce=F.ClientNonce;}return F.ClientSequence==EstablishmentSequence&&F.ClientNonce==EstablishmentNonce;}
 if(!EstablishmentSequence||F.ClientSequence!=EstablishmentSequence||F.ClientNonce!=EstablishmentNonce)return false;
 const auto Before=Snapshot;bEstablishmentPending=false;EstablishmentReviewKey.Reset();
 EstablishmentFeedback=FText::FromString(F.Message+TEXT(" ")+F.Remedy+(F.bAccepted?TEXT(" Awaiting the authoritative station update."):TEXT(" No resources changed; site and inventory choices retained. Review again.")));
 RefreshEstablishment();PublishIfChanged(Before);return true;
}
#undef LOCTEXT_NAMESPACE

FHansaTradeLedger UHansaTradeMapPresentationModel::GetLedgerPresentation() const {
 if(bRemoteEstablishment){const auto* L=RemoteLedgers.FindByPredicate([&](const auto& X){return X.City==Snapshot.SelectedCityStableId;});return L?*L:FHansaTradeLedger();}
 return LastProjection&&LastRegistry?Hansa::UI::BuildTradeLedger(*LastProjection,*LastRegistry,Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse,Snapshot.SelectedCityStableId):FHansaTradeLedger();
}
bool UHansaTradeMapPresentationModel::LedgerRelatedIntent(FName Good,bool Order){
 const auto L=GetLedgerPresentation();const auto* R=L.Rows.FindByPredicate([&](const auto& X){return X.Good==Good;});if(!R)return false;
 if(Order){if(bRemoteEstablishment)return false;const auto Before=Snapshot;SelectedStationOrder=R->OrderId;OrderDraft.GoodId=FHansaGoodId::TryParse(Good.ToString()).Value;for(const auto& O:StationOrders)if(O.Id==SelectedStationOrder)OrderDraft=O.Terms;RefreshStationOrderText();Snapshot.ActiveSection=TEXT("Orders");PublishIfChanged(Before);return true;}
 return R->RouteId&&SelectRouteIntent(R->RouteId)&&SelectSectionIntent(TEXT("Route"));
}


bool UHansaTradeMapPresentationModel::OpenWorldStation(FName City,int64 StationId) {
 if(StationId<=0||Snapshot.bCreating||Snapshot.bDirty||IsAnyTradeCommandPending())return false;
 if(!SelectCityIntent(City)||Snapshot.Establishment.StationId!=StationId||(!Snapshot.Establishment.bComplete&&!Snapshot.Establishment.bLocalDelivery))return false;
 Open(TEXT("World.Selection.TradeStation"),NAME_None,City,false);
 bShipDetailOpen=false;bWorldStationDetail=true;Snapshot.ActiveSection=TEXT("Presence");Snapshot.WorkspacePage=TEXT("Workspace");
 Snapshot.FocusedSemanticId=TEXT("TradeMap.City.Close");Changed.Broadcast(Snapshot,++Revision);return true;
}
