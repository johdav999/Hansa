#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Queries/HansaSimulationReadOnly.h"
using namespace Hansa::Simulation;
FString UHansaTradeMapPresentationModel::DecisionSignature() const
{
 const auto* O=Snapshot.Decisions.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionId;});
 if(!O)return {};
 const auto* S=O->Sources.FindByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionSource;});
 return Snapshot.Decisions.City.ToString()+LexToString(Snapshot.Decisions.Revision)+O->Id+LexToString(O->Kind)+O->Terms.ToString()+O->Status.ToString()+LexToString(O->bAvailable)+Snapshot.DecisionSource+(S?S->Detail.ToString():TEXT(""));
}
void UHansaTradeMapPresentationModel::RefreshDecisions()
{
 Snapshot.DecisionsKey=Snapshot.Decisions.Key();
 if(!DecisionReviewedKey.IsEmpty()&&DecisionReviewedKey!=DecisionSignature()){
  DecisionReviewedKey.Reset();Snapshot.DecisionFeedback=TEXT("Review changed. Selection and source retained; check updated terms and review again.");
 }
 Snapshot.bDecisionReview=!DecisionReviewedKey.IsEmpty();
}
bool UHansaTradeMapPresentationModel::CanDecisionIntent(const FString& A) const
{
 if(IsAnyTradeCommandPending())return false;
 if(A==TEXT("Terms"))return false;
 if(A==TEXT("Cancel"))return Snapshot.bDecisionReview;
 const auto* O=Snapshot.Decisions.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionId;});
 if(A==TEXT("Source"))return O&&!O->Sources.IsEmpty()&&!Snapshot.bDecisionReview;
 if(A==TEXT("Review")||A==TEXT("Confirm")){
  const auto* S=O?O->Sources.FindByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionSource;}):nullptr;
  const bool Ready=O&&O->bAvailable&&((O->Kind==1||O->Kind==3)||(S&&S->bEligible));
  return Ready&&(A==TEXT("Review")?!Snapshot.bDecisionReview:Snapshot.bDecisionReview&&DecisionReviewedKey==DecisionSignature());
 }
 return !Snapshot.bDecisionReview&&Snapshot.Decisions.Options.ContainsByPredicate([&](const auto& X){return X.Id==A;});
}
bool UHansaTradeMapPresentationModel::DecisionIntent(const FString& A)
{
 if(!CanDecisionIntent(A))return false;
 const auto Before=Snapshot;
 if(A==TEXT("Cancel"))DecisionReviewedKey.Reset();
 else if(A==TEXT("Review")){DecisionReviewedKey=DecisionSignature();Snapshot.DecisionFeedback.Reset();}
 else if(A==TEXT("Source")){
  const auto& O=*Snapshot.Decisions.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionId;});
  const int32 I=O.Sources.IndexOfByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionSource;});Snapshot.DecisionSource=O.Sources[(I+1)%O.Sources.Num()].Id;
 }else if(A==TEXT("Confirm")){
  // Copy before host publication: local execution may synchronously refresh the projection.
  const auto O=*Snapshot.Decisions.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.DecisionId;});
  const auto City=FHansaCityDefinitionId::TryParse(Snapshot.Decisions.City.ToString());if(!City||(!Runtime.IsValid()&&!NetworkCommandIntent))return false;
  const auto Inventory=FHansaInventoryId::TryCreate(FCString::Strtoui64(*Snapshot.DecisionSource,nullptr,10));
  const int64 ReviewedRevision=Snapshot.Decisions.Revision;DecisionReviewedKey.Reset();Snapshot.bDecisionReview=false;
  bool Sent=false;
  if(NetworkCommandIntent){
   Snapshot.bDecisionPending=true;DecisionSequence=DecisionNonce=0;
   FHansaClientCommandIntent I;I.Type=O.Kind==3?EHansaClientIntentType::TransitionCityAuthority:O.Kind==2?EHansaClientIntentType::FundCityProject:EHansaClientIntentType::ManageCityPrivilege;
   I.CityId=City.Value.ToString();I.DecisionId=O.DefinitionId;I.DecisionAction=O.Kind==1?1:0;I.AuthorityRevision=ReviewedRevision;I.FundingInventoryId=Inventory?int64(Inventory.Value.GetValue()):0;
   Sent=NetworkCommandIntent(I);if(!Sent){Snapshot.bDecisionPending=false;Snapshot.DecisionFeedback=TEXT("Submission failed. Reconnect and review current terms before retrying.");}
  }else{
   FHansaCommandGatewayResult Result;
   if(O.Kind==3)Result=Runtime->TransitionCityAuthority({City.Value,O.DefinitionId,ReviewedRevision});
   else if(O.Kind==2)Result=Runtime->FundCityProject({City.Value,O.DefinitionId,Inventory.Value,ReviewedRevision});
   else Result=Runtime->ManageCityPrivilege({City.Value,O.DefinitionId,Inventory.Value,{},O.Kind==1?EHansaCityPrivilegeAction::Revoke:EHansaCityPrivilegeAction::Acquire,ReviewedRevision});
   Sent=Result.IsSuccess();Snapshot.DecisionFeedback=Sent?TEXT("Accepted. Authoritative effects and progress are shown below."):FString(TEXT("Rejected: "))+LexToString(Result.GetError())+TEXT(". Check updated rights, funding and scenario requirements, then review again. Nothing was spent.");
   const auto P=Runtime->BuildProjection();if(P)ApplyProjection(P.Value,*Runtime->GetEconomicRegistry());
  }
  RefreshDecisions();PublishIfChanged(Before);FocusRestoreRequested.Broadcast(FName(*(TEXT("TradeMap.Decisions.")+Snapshot.DecisionId)));return Sent;
 }else{Snapshot.DecisionId=A;Snapshot.DecisionFeedback.Reset();}
 RefreshDecisions();PublishIfChanged(Before);
 if(A==TEXT("Review"))FocusRestoreRequested.Broadcast(TEXT("TradeMap.Decisions.Confirm"));
 if(A==TEXT("Cancel"))FocusRestoreRequested.Broadcast(FName(*(TEXT("TradeMap.Decisions.")+Snapshot.DecisionId)));
 return true;
}
bool UHansaTradeMapPresentationModel::ReceiveDecisionFeedback(const FHansaClientCommandFeedback& F)
{
 if(!Snapshot.bDecisionPending)return false;
 if(F.State==EHansaClientCommandState::Pending){if(!DecisionSequence){DecisionSequence=F.ClientSequence;DecisionNonce=F.ClientNonce;}return true;}
 if(!DecisionSequence||F.ClientSequence!=DecisionSequence||F.ClientNonce!=DecisionNonce)return false;
 const auto Before=Snapshot;Snapshot.bDecisionPending=false;DecisionSequence=DecisionNonce=0;DecisionReviewedKey.Reset();
 Snapshot.DecisionFeedback=F.Message+TEXT(" ")+F.Remedy;RefreshDecisions();PublishIfChanged(Before);
 FocusRestoreRequested.Broadcast(FName(*(TEXT("TradeMap.Decisions.")+Snapshot.DecisionId)));return true;
}
