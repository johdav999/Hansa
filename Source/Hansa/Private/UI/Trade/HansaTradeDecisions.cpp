#include "UI/HansaTradeDecisions.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
using namespace Hansa::Simulation;

FString FHansaTradeDecisions::Key() const
{
 FString K=City.ToString()+LexToString(Revision)+Context.ToString();
 for(const auto& O:Options){K+=O.Id+O.DefinitionId+LexToString(O.Kind)+O.Title.ToString()+O.Commitment.ToString()+O.Terms.ToString()+O.Status.ToString()+LexToString(O.bAvailable);for(const auto& S:O.Sources)K+=S.Id+S.Detail.ToString()+LexToString(S.bEligible);}
 return K;
}
FHansaTradeDecisions Hansa::UI::BuildTradeDecisions(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& R,FHansaHouseId Viewer,FName City,const FString& Scenario)
{
 FHansaTradeDecisions V;V.City=City;
 const FString CityName=City.ToString().Replace(TEXT("City."),TEXT(""));
 auto StageName=[&](const FString& Id){const auto* D=R.FindPresenceStage(Id);return D?D->DisplayName:Id;};
 auto GoodName=[&](const FString& Id){const auto* D=R.FindGood(Id);return D?D->DisplayName:Id;};
 const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Viewer&&X.CityId.ToString()==City.ToString();});
 const auto* Policy=R.FindCityTradePolicyForCity(City.ToString());
 const auto* House=P.GetHouses().FindByPredicate([&](const auto& X){return X.Id==Viewer;});
 V.Context=FText::FromString(CityName+TEXT(" · Autonomous city. Choose a scoped privilege, shared project or exceptional charter."));
 if(!Presence||!Policy||!House){V.Context=FText::FromString(TEXT("Decisions unavailable. Establish commercial presence in this city first."));return V;}
 V.Revision=Presence->AuthorityRevision;
 const auto* Stage=R.FindPresenceStage(Presence->CurrentStageId);
 if(Presence->bGovernanceAuthority)V.Context=FText::FromString(CityName+TEXT(" · Chartered authority. Existing city property remains autonomous."));
 auto Eligible=[&](const FString& Required,const TCHAR* Capability){const auto* S=R.FindPresenceStage(Required);return Stage&&S&&Stage->Ordinal>=S->Ordinal&&Presence->Capabilities.ContainsByPredicate([&](const auto& C){return C.CapabilityId==Capability&&C.bGranted;});};
 auto Funding=[&](FHansaTradeDecisionOption& O,int64 Money,const TArray<FHansaCompiledPresenceUpgradeGoodCost>& Goods){
  O.Commitment=FText::FromString(FString::Printf(TEXT("%lld pfennig"),Money));
  for(const auto& G:Goods)O.Commitment=FText::FromString(O.Commitment.ToString()+FString::Printf(TEXT(" + %.3f units %s"),double(G.QuantityMilliUnits)/1000.,*GoodName(G.GoodId)));
  O.Terms=FText::FromString(O.Terms.ToString()+FString::Printf(TEXT("\nCost: %lld pfennig from house treasury."),Money));
  for(const auto& G:Goods){const auto* D=R.FindGood(G.GoodId);O.Terms=FText::FromString(O.Terms.ToString()+FString::Printf(TEXT("\nMaterial cost: %.3f units %s."),double(G.QuantityMilliUnits)/1000.,*(D?D->DisplayName:G.GoodId)));}
  for(const auto& S:P.GetTradeStations())if(S.Station.OwnerId==Viewer){
   const auto* I=P.GetInventories().FindByPredicate([&](const auto& X){return X.Id==S.Station.InventoryId;});if(!I)continue;
   FHansaEstablishmentChoice C;C.Id=LexToString(I->Id.GetValue());C.Label=FText::FromString(S.Station.CityId.ToString().Replace(TEXT("City."),TEXT(""))+TEXT(" station #")+C.Id);C.bEligible=O.bAvailable&&House->Money.GetRawValue()>=Money;
   FString Detail=FString::Printf(TEXT("%s\nTreasury: %lld; spend %lld pfennig. Materials are consumed from this station on confirmation. Normal tick upkeep continues separately."),*C.Label.ToString(),House->Money.GetRawValue(),Money);
   for(const auto& G:Goods){const auto* Stock=I->Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==G.GoodId;});const int64 Available=Stock?Stock->Available.GetRawValue():0;C.bEligible&=Available>=G.QuantityMilliUnits;Detail+=FString::Printf(TEXT("\n%s: spend %.3f / available %.3f units."),*GoodName(G.GoodId),double(G.QuantityMilliUnits)/1000.,double(Available)/1000.);}
   if(!C.bEligible)Detail+=TEXT("\nBlocked: meet prerequisites and replenish treasury or station materials, then review again.");
   C.Detail=FText::FromString(Detail);C.Transfer=C.Detail;O.Sources.Add(MoveTemp(C));
  }
  O.Sources.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
 };
 for(const auto& D:Policy->Privileges){
  auto& O=V.Options.AddDefaulted_GetRef();O.Id=TEXT("Privilege.")+D.PrivilegeId;O.DefinitionId=D.PrivilegeId;O.Title=FText::FromString(D.DisplayName);
  const auto* Existing=Presence->Privileges.FindByPredicate([&](const auto& X){return X.PrivilegeId==D.PrivilegeId&&X.Status!=EHansaCityPrivilegeStatus::Revoked;});
  O.bAvailable=Eligible(D.RequiredStageId,TEXT("PresenceCapability.Privileges"));
  O.Terms=FText::FromString(FString::Printf(TEXT("Scoped privilege · issuer %s · recipient house %llu\nRequires %s and privilege rights.\nBounded lease: (%d,%d)–(%d,%d); categories: %s.\nDuration: %s. No city stock, buildings, roads, workforce, taxes or ownership transfer. Revocation has no refund; an occupied lease cannot be revoked."),*CityName,Viewer.GetValue(),*StageName(D.RequiredStageId),D.LeaseBoundsMin.X,D.LeaseBoundsMin.Y,D.LeaseBoundsMax.X,D.LeaseBoundsMax.Y,*FString::Join(D.PermittedBuildingCategories,TEXT(", ")),D.DurationTicks?*FString::Printf(TEXT("%lld ticks"),D.DurationTicks):TEXT("permanent until revoked")));
  if(Existing){
   O.Commitment=FText::FromString(TEXT("No additional cost; no refund."));O.Kind=1;const auto* L=P.GetLeasedPlots().FindByPredicate([&](const auto& X){return X.Id==Existing->GrantedLeaseId;});O.bAvailable&=D.bReversible&&L&&L->OccupyingBuildingIds.IsEmpty();
   O.Status=FText::FromString(FString::Printf(TEXT("%s · paid %lld pfennig. %s"),Existing->Status==EHansaCityPrivilegeStatus::Active?TEXT("Active"):TEXT("Suspended"),Existing->SpentMoneyPfennig,O.bAvailable?TEXT("Review revocation; no additional cost."):TEXT("Revocation unavailable: irreversible policy or occupied lease.")));
  }else{O.Status=FText::FromString(O.bAvailable?TEXT("Available · choose a source"):TEXT("Locked · advance presence and obtain privilege rights"));Funding(O,D.CostPfennig,D.CostGoods);}
 }
 for(const auto& D:Policy->CityProjects){
  auto& O=V.Options.AddDefaulted_GetRef();O.Id=TEXT("Project.")+D.ProjectId;O.DefinitionId=D.ProjectId;O.Kind=2;O.Title=FText::FromString(D.DisplayName);
  const auto* E=Presence->CityProjects.FindByPredicate([&](const auto& X){return X.ProjectId==D.ProjectId;});
  O.bAvailable=!E&&Eligible(D.RequiredStageId,TEXT("PresenceCapability.CityProjects"));
  O.Terms=FText::FromString(FString::Printf(TEXT("City partnership project · %s\nRequires %s and city-project rights. Full funding only; partial contributions are unsupported.\nAfter %d ticks: shared desired reserve for %s increases by %.3f units, once. This creates no goods and grants no ownership or governance.\nOther contributors/supporters are not publicly reported. Cancellation and refunds are unsupported. The investment is irreversible."),*CityName,*StageName(D.RequiredStageId),D.ConstructionTicks,*GoodName(D.SharedReserveGoodId),double(D.SharedReserveBonusMilliUnits)/1000.));
  Funding(O,D.CostPfennig,D.CostGoods);
  if(E){O.Sources.Reset();const int64 Remaining=FMath::Max<int64>(0,E->CompletionTick.GetValue()-P.GetClock().GetTick().GetValue());O.Status=FText::FromString(FString::Printf(TEXT("%s · committed %lld pfennig and all listed materials. Remaining funding: 0. %lld ticks remaining. Shared effect %s."),E->Status==EHansaCityProjectStatus::Completed?TEXT("Completed"):TEXT("Funded / in progress"),E->SpentMoneyPfennig,Remaining,E->bSharedEffectApplied?TEXT("applied"):TEXT("pending")));}
  else O.Status=FText::FromString(O.bAvailable?TEXT("Unfunded · committed 0; entire listed cost remains"):TEXT("Locked · advance presence and obtain city-project rights"));
 }
 auto& O=V.Options.AddDefaulted_GetRef();O.Id=TEXT("Charter");O.Kind=3;O.Commitment=FText::FromString(TEXT("No money, goods or property transfer."));O.DefinitionId=Policy->GovernanceCharterId;O.Title=FText::FromString(TEXT("Exceptional charter"));
 O.bAvailable=!Presence->bGovernanceAuthority&&Presence->CurrentStageId==TEXT("PresenceStage.PrivilegedPresence")&&Policy->bExceptionalGovernanceAllowed&&Policy->GovernanceScenarioIds.Contains(Scenario)&&R.FindPresenceStage(TEXT("PresenceStage.ExceptionalGovernance"));
 FString Effects; if(const auto* Target=R.FindPresenceStage(TEXT("PresenceStage.ExceptionalGovernance")))for(const auto& Id:Target->GrantedCapabilityIds){const auto* C=R.FindPresenceCapability(Id);Effects+=(C?C->DisplayName:Id)+TEXT("; ");}
 O.Terms=FText::FromString(TEXT("Requires Privileged Presence, city permission, exact charter and an allowed scenario. Trade volume alone never grants authority.\nCharter: ")+Policy->GovernanceCharterId+TEXT("\nAllowed scenarios: ")+FString::Join(Policy->GovernanceScenarioIds,TEXT(", "))+TEXT("\nCurrent scenario: ")+Scenario+TEXT("\nCost: 0; no funding inventory. Permanent; reversal unsupported. Stage becomes Exceptional Governance, replacing stage capabilities with: ")+Effects+TEXT("\nExisting assets, market inventory, buildings, roads, plots, routes, debts, policies and station inventory do not transfer. Construction still requires ordinary lease permission. No coalition or supporter vote is implemented."));
 O.Status=FText::FromString(Presence->bGovernanceAuthority?TEXT("Granted / completed"):O.bAvailable?TEXT("Eligible · review charter"):TEXT("Locked · city policy, scenario or required stage does not permit this charter"));
 return V;
}
