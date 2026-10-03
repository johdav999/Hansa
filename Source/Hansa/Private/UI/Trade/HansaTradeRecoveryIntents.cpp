#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
using namespace Hansa::Simulation;
void UHansaTradeMapPresentationModel::RefreshRecovery()
{
 if(bRemoteEstablishment)RefreshRemoteTradeSelection();
 const auto* V=Recoveries.FindByPredicate([&](const auto& X){return X.City==Snapshot.SelectedCityStableId&&X.Station==RecoveryStation;});
 if(!V)for(const auto& X:Recoveries)if(X.City==Snapshot.SelectedCityStableId&&(!V||X.Station>V->Station))V=&X;
 Snapshot.Recovery=V?*V:FHansaTradeRecovery();RecoveryStation=Snapshot.Recovery.Station;
 Snapshot.RecoveryKey=Snapshot.Recovery.ReviewKey;
 if(!Snapshot.Recovery.Items.ContainsByPredicate([&](const auto& X){return X.Id==Snapshot.RecoveryItem;}))Snapshot.RecoveryItem=TEXT("Station");
 if(!RecoveryReviewedKey.IsEmpty()&&RecoveryReviewedKey!=Snapshot.Recovery.ReviewKey){RecoveryReviewedKey.Reset();Snapshot.RecoveryFeedback=TEXT("Dependencies changed. Review the current dossier again; no closure was submitted.");}
 Snapshot.bRecoveryReview=!RecoveryReviewedKey.IsEmpty();
}
bool UHansaTradeMapPresentationModel::OpenRecovery(FName City,int64 Station,FName Origin)
{
 if(Snapshot.bCreating||Snapshot.bRecoveryPending)return false;
 Open(Origin.IsNone()?FName(TEXT("TradeMap.Navigate.Recovery")):Origin,NAME_None,NAME_None,false);
 if(City!=Snapshot.SelectedCityStableId&&!SelectCityIntent(City))return false;
 const auto Before=Snapshot;RecoveryStation=Station;Snapshot.ActiveSection=TEXT("Recovery");Snapshot.WorkspacePage=TEXT("Workspace");RecoveryReviewedKey.Reset();RefreshRecovery();PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::CanRecoveryIntent(const FString& A) const
{
 if(IsAnyTradeCommandPending())return false;
 if(A==TEXT("Back"))return true;
 if(A==TEXT("Cancel"))return Snapshot.bRecoveryReview;
 if(A==TEXT("Review"))return Snapshot.Recovery.bCanClose&&!Snapshot.bRecoveryReview;
 if(A==TEXT("Confirm"))return Snapshot.Recovery.bCanClose&&Snapshot.bRecoveryReview&&RecoveryReviewedKey==Snapshot.Recovery.ReviewKey;
 if(A==TEXT("Inspect"))return !Snapshot.bRecoveryReview&&Snapshot.Recovery.Items.ContainsByPredicate([&](const auto& I){return I.Id==Snapshot.RecoveryItem;});
 return !Snapshot.bRecoveryReview&&Snapshot.Recovery.Items.ContainsByPredicate([&](const auto& I){return I.Id==A;});
}
bool UHansaTradeMapPresentationModel::RecoveryIntent(const FString& A)
{
 if(!CanRecoveryIntent(A))return false;const auto Before=Snapshot;
 if(A==TEXT("Back")){if(FocusOriginSemanticId.ToString().StartsWith(TEXT("HUD.AlertStack.")))return CloseIntent();return SelectSectionIntent(TEXT("Overview"));}
 if(A==TEXT("Cancel"))RecoveryReviewedKey.Reset();
 else if(A==TEXT("Review")){RecoveryReviewedKey=Snapshot.Recovery.ReviewKey;Snapshot.RecoveryFeedback.Reset();}
 else if(A==TEXT("Inspect")){
  const auto I=*Snapshot.Recovery.Items.FindByPredicate([&](const auto& X){return X.Id==Snapshot.RecoveryItem;});
  if(I.Target==TEXT("Route")){if(!SelectRouteIntent(I.Entity))return false;RefreshRecovery();SelectSectionIntent(TEXT("Route"));return true;}
  if(I.Target==TEXT("Ship")){if(!SelectFleetIntent(I.Entity))return false;return SelectSectionIntent(TEXT("Overview"));}
  if(I.Target==TEXT("Orders")){SelectedStationOrder=uint64(I.Entity);RefreshStationOrderText();}
  return SelectSectionIntent(I.Target);
 }else if(A==TEXT("Confirm")){
  const auto Review=Snapshot.Recovery;RecoveryReviewedKey.Reset();Snapshot.bRecoveryReview=false;bool Done=false;
  if(NetworkCommandIntent){Snapshot.bRecoveryPending=true;RecoverySequence=RecoveryNonce=0;FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::CloseTradeStation;I.TradeStationId=Review.Station;I.RecoveryReviewKey=Review.ReviewKey;Done=NetworkCommandIntent(I);if(!Done){Snapshot.bRecoveryPending=false;Snapshot.RecoveryFeedback=TEXT("Submission failed. Reconnect and review current dependencies.");}}
  else if(Runtime.IsValid()){
   const auto Current=Runtime->BuildTradeRecovery(Runtime->GetHouseId());const auto* V=Current.FindByPredicate([&](const auto& X){return X.Station==Review.Station;});
   if(V&&V->bCanClose&&V->ReviewKey==Review.ReviewKey){const auto Result=Runtime->CloseTradeStation(FHansaTradeStationId::TryCreate(uint64(Review.Station)).Value);Done=Result.IsSuccess();Snapshot.RecoveryFeedback=Done?TEXT("Closure accepted. Preserved assets and remaining blockers are listed below."):FString(TEXT("Closure rejected: "))+LexToString(Result.GetError())+TEXT(". State preserved; resolve the blocker and review again.");}
   else Snapshot.RecoveryFeedback=TEXT("Review is stale. Dependencies changed; nothing was closed.");
   const auto P=Runtime->BuildProjection();if(P)ApplyProjection(P.Value,*Runtime->GetEconomicRegistry());
  }
  RefreshRecovery();PublishIfChanged(Before);return Done;
 }else Snapshot.RecoveryItem=A;
 RefreshRecovery();PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::ReceiveRecoveryFeedback(const FHansaClientCommandFeedback& F)
{
 if(!Snapshot.bRecoveryPending)return false;
 if(F.State==EHansaClientCommandState::Pending){if(!RecoverySequence){RecoverySequence=F.ClientSequence;RecoveryNonce=F.ClientNonce;}return true;}
 if(!RecoverySequence||F.ClientSequence!=RecoverySequence||F.ClientNonce!=RecoveryNonce)return false;
 const auto Before=Snapshot;Snapshot.bRecoveryPending=false;RecoverySequence=RecoveryNonce=0;RecoveryReviewedKey.Reset();Snapshot.RecoveryFeedback=F.Message+TEXT(" ")+F.Remedy;RefreshRecovery();PublishIfChanged(Before);return true;
}
