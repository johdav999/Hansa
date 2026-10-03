#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Queries/HansaSimulationReadOnly.h"
#define LOCTEXT_NAMESPACE "TradeSpecializationIntents"
using namespace Hansa::Simulation;
FString UHansaTradeMapPresentationModel::SpecializationReviewSignature() const {
 return Snapshot.SelectedCityStableId.ToString()+TEXT("|")+Snapshot.Specialization.Key()+TEXT("|")+Snapshot.SelectedPresenceSpecializationId+TEXT("|")+Snapshot.SpecializationSourceId;
}
void UHansaTradeMapPresentationModel::RefreshSpecialization(){
 Snapshot.SpecializationKey=Snapshot.Specialization.Key();
 const auto* O=Snapshot.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.SelectedPresenceSpecializationId;});
 const auto* Source=O?O->Sources.FindByPredicate([&](const auto& X){return X.Id==Snapshot.SpecializationSourceId;}):nullptr;
 if(!SpecializationReviewedKey.IsEmpty()&&SpecializationReviewedKey!=SpecializationReviewSignature()){
  SpecializationReviewedKey.Reset();Snapshot.PresenceSpecializationFeedback=LOCTEXT("Stale","Review changed. Your branch and source are retained. Check current terms and review again; nothing was spent.");
 }
 Snapshot.bSpecializationReview=!SpecializationReviewedKey.IsEmpty();
 Snapshot.bCanPresenceSpecializationAction=O&&O->bAvailable&&Source&&Source->bEligible&&!Snapshot.bSpecializationPending;
 Snapshot.PresenceSpecializationAction=Snapshot.bSpecializationPending?LOCTEXT("Pending","Awaiting authority"):O&&O->bRespec?LOCTEXT("ReviewRespec","Review respec"):LOCTEXT("ReviewSelection","Review selection");
 Snapshot.PresenceSpecializationComparison=Snapshot.Specialization.Options.IsEmpty()?LOCTEXT("Empty","No specialization policy is available for your presence here."):LOCTEXT("Choose","Compare one exclusive Merchant Office branch. Suggestions explain your context; choose a branch yourself.");
 Snapshot.SpecializationReview=O?FText::Format(LOCTEXT("ReviewTerms","{0} {1}\n{2}\n{3}\n{4}"),
 O->bRespec?LOCTEXT("Replace","Replace your current branch with"):LOCTEXT("Establish","Establish"),O->Name,
 Source?Source->Detail:LOCTEXT("ChooseSource","Choose an owned station inventory. House treasury pays the investment; ship and home stock must first reach a station."),
 O->Dimensions.IsValidIndex(6)?O->Dimensions[6]:FText(),O->Dimensions.IsValidIndex(7)?O->Dimensions[7]:FText()):LOCTEXT("ChooseBranch","Select Warehouse, Market, or Harbor to inspect funding.");
}
bool UHansaTradeMapPresentationModel::CanPresenceSpecializationIntent(const FString& A) const{
 if(IsAnyTradeCommandPending())return false;
 if(A.StartsWith(TEXT("Dimension.")))return true;
 if(A==TEXT("Cancel"))return Snapshot.bSpecializationReview;
 if(A==TEXT("Back"))return true;
 const auto* O=Snapshot.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.SelectedPresenceSpecializationId;});
 if(A==TEXT("Source")||A.StartsWith(TEXT("Source.")))return O&&!O->Sources.IsEmpty()&&!Snapshot.bSpecializationReview;
 if(A==TEXT("Apply")||A==TEXT("Review"))return Snapshot.bCanPresenceSpecializationAction&&!Snapshot.bSpecializationReview;
 if(A==TEXT("Confirm"))return Snapshot.bCanPresenceSpecializationAction&&Snapshot.bSpecializationReview&&SpecializationReviewedKey==SpecializationReviewSignature();
 return Snapshot.Specialization.Options.ContainsByPredicate([&](const auto& X){return X.Id==A;})&&!Snapshot.bSpecializationReview;
}
bool UHansaTradeMapPresentationModel::PresenceSpecializationIntent(const FString& A){
 if(!CanPresenceSpecializationIntent(A))return false;
 const auto Before=Snapshot;
 if(A==TEXT("Back")){if(Snapshot.bSpecializationReview)return PresenceSpecializationIntent(TEXT("Cancel"));return SelectSectionIntent(TEXT("Presence"));}
 if(A.StartsWith(TEXT("Dimension.")))return true;
 if(A==TEXT("Cancel")){SpecializationReviewedKey.Reset();RefreshSpecialization();PublishIfChanged(Before);FocusRestoreRequested.Broadcast(FName(*(TEXT("TradeMap.Presence.Specialization.")+Snapshot.SelectedPresenceSpecializationId)));return true;}
 if(A==TEXT("Apply")||A==TEXT("Review")){
  SpecializationReviewedKey=SpecializationReviewSignature();Snapshot.PresenceSpecializationFeedback=FText();RefreshSpecialization();PublishIfChanged(Before);FocusRestoreRequested.Broadcast(TEXT("TradeMap.Presence.Specialization.Confirm"));return true;
 }
 if(A==TEXT("Source")||A.StartsWith(TEXT("Source."))){
  const auto* O=Snapshot.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.SelectedPresenceSpecializationId;});if(!O)return false;
  if(A==TEXT("Source")){const int32 I=O->Sources.IndexOfByPredicate([&](const auto& X){return X.Id==Snapshot.SpecializationSourceId;});Snapshot.SpecializationSourceId=O->Sources[(I+1)%O->Sources.Num()].Id;}
  else {const FString Id=A.RightChop(7);if(!O->Sources.ContainsByPredicate([&](const auto& X){return X.Id==Id;}))return false;Snapshot.SpecializationSourceId=Id;}
 }else if(A!=TEXT("Confirm")){Snapshot.SelectedPresenceSpecializationId=A;}
 else {
  const auto* O=Snapshot.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==Snapshot.SelectedPresenceSpecializationId;});
  const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());const auto Inventory=FHansaInventoryId::TryCreate(FCString::Strtoui64(*Snapshot.SpecializationSourceId,nullptr,10));
  if(!O||!City||!Inventory||(!Runtime.IsValid()&&!NetworkCommandIntent))return false;
  const FHansaApplyPresenceSpecializationCommand Command{City.Value,O->Id,Inventory.Value,O->bRespec?EHansaPresenceSpecializationAction::Respec:EHansaPresenceSpecializationAction::Select,Snapshot.Specialization.Revision};
  bool Sent=false;SpecializationReviewedKey.Reset();Snapshot.bSpecializationReview=false;
  if(NetworkCommandIntent){
   Snapshot.bSpecializationPending=true;SpecializationSequence=SpecializationNonce=0;
   FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::ApplyPresenceSpecialization;I.CityId=City.Value.ToString();I.FundingInventoryId=static_cast<int64>(Inventory.Value.GetValue());I.PresenceSpecializationId=O->Id;I.PresenceSpecializationAction=static_cast<uint8>(Command.Action);I.PresenceSpecializationRevision=Command.ReviewedRevision;
   Sent=NetworkCommandIntent(I);if(!Sent){Snapshot.bSpecializationPending=false;Snapshot.PresenceSpecializationFeedback=LOCTEXT("Rejected","Rejected. Refresh the terms, check materials and capacity, then review again.");}
  }else{
   Sent=Runtime->ApplyPresenceSpecialization(Command).IsSuccess();
   Snapshot.PresenceSpecializationFeedback=Sent?LOCTEXT("Accepted","Specialization applied. Its authoritative effects are active."):LOCTEXT("Rejected","Rejected. Refresh the terms, check materials and capacity, then review again.");
   const auto P=Runtime->BuildProjection();if(P)ApplyProjection(P.Value,*Runtime->GetEconomicRegistry());
  }
  RefreshSpecialization();PublishIfChanged(Before);FocusRestoreRequested.Broadcast(FName(*(TEXT("TradeMap.Presence.Specialization.")+Snapshot.SelectedPresenceSpecializationId)));return Sent;
 }
 Snapshot.PresenceSpecializationFeedback=FText();SpecializationReviewedKey.Reset();RefreshSpecialization();PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::ReceiveSpecializationFeedback(const FHansaClientCommandFeedback& F){
 if(!Snapshot.bSpecializationPending)return false;
 if(F.State==EHansaClientCommandState::Pending){if(SpecializationSequence==0){SpecializationSequence=F.ClientSequence;SpecializationNonce=F.ClientNonce;}return true;}
 if(SpecializationSequence==0||F.ClientSequence!=SpecializationSequence||F.ClientNonce!=SpecializationNonce)return false;
 const auto Before=Snapshot;Snapshot.bSpecializationPending=false;SpecializationSequence=SpecializationNonce=0;SpecializationReviewedKey.Reset();
 Snapshot.PresenceSpecializationFeedback=FText::FromString(F.Message+TEXT(" ")+F.Remedy);RefreshSpecialization();PublishIfChanged(Before);
 FocusRestoreRequested.Broadcast(FName(*(TEXT("TradeMap.Presence.Specialization.")+Snapshot.SelectedPresenceSpecializationId)));return true;
}
#undef LOCTEXT_NAMESPACE
