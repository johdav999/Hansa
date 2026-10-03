#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeDecisionTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeDecisionsJourney,"Hansa.UI.TradeMap.Decisions.Journey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeDecisionsJourney::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareDecisions(*Host,Error)){AddError(Error);return false;}
  TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(Host->GetHouseId());if(!Remote)M->BindRuntime(Host.Get());
  FHansaMultiplayerAuthority Authority;int64 Nonce=14000;bool Delay=false;FHansaClientCommandIntent Pending;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({914,FHansaParticipantId::TryCreate(1914).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   M->SetNetworkCommandIntent([&](const auto& I){Pending=I;Pending.ClientSequence=Authority.GetExpectedClientSequence(914);Pending.ClientNonce=++Nonce;FHansaClientCommandFeedback Ack;Ack.State=EHansaClientCommandState::Pending;Ack.ClientSequence=Pending.ClientSequence;Ack.ClientNonce=Pending.ClientNonce;M->ReceiveCommandFeedback(Ack);if(!Delay)M->ReceiveCommandFeedback(Authority.SubmitIntent(914,Pending));return true;});
  }
  auto Refresh=[&]{if(Remote){FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(914,0,true,Wire,Error)){AddError(Error);return;}
   TestEqual(TEXT("Only owner dossiers replicated"),Wire.Presences.Num(),2);
   TArray<uint8> Bytes;FMemoryWriter W(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(W,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader R(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(R,&Copy,nullptr);TestFalse(TEXT("Dossier wire round trip"),R.IsError());M->ApplyRemoteEstablishment(Copy);
  }else M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};
  Refresh();M->Open();M->SelectCityIntent(TEXT("City.Rostock"));auto UI=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
  TestTrue(TEXT("Decision tab"),UI->ActivateSemanticId(TEXT("TradeMap.Navigate.Decisions")));
  auto Act=[&](const FString& A){return UI->ActivateSemanticId(TEXT("TradeMap.Decisions.")+A);};
  TestTrue(TEXT("Choose privilege"),Act(TEXT("Privilege.AdditionalCommercialPlot")));TestFalse(TEXT("Source required"),Act(TEXT("Review")));TestTrue(TEXT("Choose station"),Act(TEXT("Source")));
  const auto Widget=UI->ResolveSemanticWidget(TEXT("TradeMap.Decisions.Privilege.AdditionalCommercialPlot"));Refresh();TestTrue(TEXT("Identity retained"),Widget==UI->ResolveSemanticWidget(TEXT("TradeMap.Decisions.Privilege.AdditionalCommercialPlot")));
  auto Money=[&]{const auto P=Host->BuildProjection().Value;return P.GetHouses().FindByPredicate([&](const auto& H){return H.Id==Host->GetHouseId();})->Money.GetRawValue();};
  auto Planks=[&]{const auto P=Host->BuildProjection().Value;const auto* I=P.GetInventories().FindByPredicate([](const auto& X){return X.Id.GetValue()==20;});return I->Stocks.FindByPredicate([](const auto& X){return X.GoodId.ToString()==TEXT("Good.Planks");})->Stock.GetRawValue();};
  auto Hash=[&]{return Host->BuildProjection().Value.GetFingerprint().Value;};uint64 Before=Hash();
  TestTrue(TEXT("Review"),Act(TEXT("Review")));TestEqual(TEXT("Review no mutation"),Before,Hash());TestTrue(TEXT("Edit"),Act(TEXT("Cancel")));TestFalse(TEXT("No unreviewed confirmation"),Act(TEXT("Confirm")));
  Act(TEXT("Review"));Host->AdvanceTicks(1);Refresh();TestFalse(TEXT("Changed treasury invalidates review"),M->GetSnapshot().bDecisionReview);TestEqual(TEXT("Source retained"),M->GetSnapshot().DecisionSource,FString(TEXT("20")));
  const int64 MoneyBeforePrivilege=Money(),PlanksBeforePrivilege=Planks();Act(TEXT("Review"));TestTrue(TEXT("Acquire"),Act(TEXT("Confirm")));Refresh();TestEqual(TEXT("Privilege revision"),M->GetSnapshot().Decisions.Revision,int64(1));
  TestEqual(TEXT("Privilege debit plus normal 25-pfennig tick upkeep"),MoneyBeforePrivilege-Money(),int64(120025));TestEqual(TEXT("Exact privilege material sink"),PlanksBeforePrivilege-Planks(),int64(8000));
  TestEqual(TEXT("Lease allocated by host"),Host->BuildProjection().Value.GetLeasedPlots().Num(),2);
  const int64 MoneyBeforeRevoke=Money(),PlanksBeforeRevoke=Planks();TestTrue(TEXT("Revocation review"),Act(TEXT("Review")));TestTrue(TEXT("Revoke empty lease"),Act(TEXT("Confirm")));Refresh();
  TestEqual(TEXT("Revocation charges only normal 25-pfennig tick upkeep"),Money(),MoneyBeforeRevoke-25);TestEqual(TEXT("Revocation refunds no goods"),Planks(),PlanksBeforeRevoke);
  const int64 MoneyBeforeProject=Money(),PlanksBeforeProject=Planks();TestTrue(TEXT("Project choice"),Act(TEXT("Project.PublicGranary")));Act(TEXT("Review"));TestTrue(TEXT("Fund project"),Act(TEXT("Confirm")));Refresh();
  TestEqual(TEXT("Project debit plus normal 25-pfennig tick upkeep"),MoneyBeforeProject-Money(),int64(90025));TestEqual(TEXT("Exact project material sink"),PlanksBeforeProject-Planks(),int64(6000));
  const auto* Project=M->GetSnapshot().Decisions.Options.FindByPredicate([](const auto& O){return O.Kind==2;});TestTrue(TEXT("Exact committed state"),Project&&Project->Status.ToString().Contains(TEXT("committed 90000")));
  TestFalse(TEXT("Cannot fund twice"),Act(TEXT("Review")));TestTrue(TEXT("Delayed ticks"),Host->AdvanceTicks(3));Refresh();
  Project=M->GetSnapshot().Decisions.Options.FindByPredicate([](const auto& O){return O.Kind==2;});TestTrue(TEXT("Project completed"),Project&&Project->Status.ToString().Contains(TEXT("Completed")));
  if(Remote){Before=Hash();Authority.SubmitIntent(914,Pending);TestEqual(TEXT("Duplicate project intent cannot debit again"),Before,Hash());}
  Act(TEXT("Charter"));TestFalse(TEXT("Rostock locked"),Act(TEXT("Review")));Before=Hash();
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
  TestFalse(TEXT("Forged charter rejected"),Host->TransitionCityAuthority({City,TEXT("Charter.Hamburg.FoundingCouncil"),M->GetSnapshot().Decisions.Revision}).IsSuccess());TestEqual(TEXT("Forged charter rollback"),Before,Hash());
  const auto Hamburg=FHansaCityDefinitionId::TryParse(TEXT("City.Hamburg")).Value;
  TestTrue(TEXT("Select charter city"),M->SelectCityIntent(TEXT("City.Hamburg")));Refresh();UI->ActivateSemanticId(TEXT("TradeMap.Navigate.Decisions"));Act(TEXT("Charter"));
  TestTrue(TEXT("Allowed charter review"),Act(TEXT("Review")));TestTrue(TEXT("Allowed charter confirmation"),Act(TEXT("Confirm")));Refresh();
  TestTrue(TEXT("Charter completed"),M->GetSnapshot().Decisions.Options.ContainsByPredicate([](const auto& O){return O.Kind==3&&O.Status.ToString().Contains(TEXT("completed"));}));
  M->SelectCityIntent(TEXT("City.Rostock"));Refresh();UI->ActivateSemanticId(TEXT("TradeMap.Navigate.Decisions"));
  TArray<uint8> Save;Before=Hash();TestTrue(TEXT("Save decisions"),Host->CaptureSaveBytes(Save,TEXT("TG14"),TEXT("2026-09-24T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore decisions"),Host->RestoreSaveBytes(Save).IsSuccess());TestEqual(TEXT("Save preserves fingerprint"),Before,Hash());Refresh();
  if(Remote){Act(TEXT("Privilege.AdditionalCommercialPlot"));Act(TEXT("Review"));Delay=true;TestTrue(TEXT("Queue confirmation"),Act(TEXT("Confirm")));TestFalse(TEXT("No pending double-submit"),Act(TEXT("Confirm")));
   TestTrue(TEXT("Competing grant"),Host->ManageCityPrivilege({City,TEXT("AdditionalCommercialPlot"),FHansaInventoryId::TryCreate(20).Value,{},EHansaCityPrivilegeAction::Acquire,M->GetSnapshot().Decisions.Revision}).IsSuccess());Before=Hash();M->ReceiveCommandFeedback(Authority.SubmitIntent(914,Pending));Refresh();TestEqual(TEXT("Stale remote rollback"),Before,Hash());TestFalse(TEXT("Pending cleared after rejection"),M->GetSnapshot().bDecisionPending);TestFalse(TEXT("Rejected feedback visible"),M->GetSnapshot().DecisionFeedback.IsEmpty());
  }
 }
 return !HasAnyErrors();
}
#endif
