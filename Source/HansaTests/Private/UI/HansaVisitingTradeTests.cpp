#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaVisitingTradeTestSupport.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/SHansaMarketTable.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisitingTradeJourney,"Hansa.UI.VisitingTrade.Journey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVisitingTradeJourney::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 for(bool Remote:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareVisitingTrade(*Host,Error)){AddError(Error);return false;}
  TStrongObjectPtr<UHansaMarketTablePresentationModel> M(NewObject<UHansaMarketTablePresentationModel>());M->InitializeDefaults();if(!Remote)M->BindRuntime(Host.Get());
  FHansaMultiplayerAuthority Authority;FHansaClientCommandIntent Pending;bool Delay=false;int64 Nonce=15000;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds.AddUnique(TEXT("City.Rostock"));if(!Authority.RegisterAdmittedClient({915,FHansaParticipantId::TryCreate(1915).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   M->SetNetworkCommandIntent([&](const auto& I){Pending=I;Pending.ClientSequence=Authority.GetExpectedClientSequence(915);Pending.ClientNonce=++Nonce;FHansaClientCommandFeedback Ack;Ack.State=EHansaClientCommandState::Pending;Ack.ClientSequence=Pending.ClientSequence;Ack.ClientNonce=Pending.ClientNonce;M->ReceiveCommandFeedback(Ack);if(!Delay)M->ReceiveCommandFeedback(Authority.SubmitIntent(915,Pending));return true;});
  }
  auto Refresh=[&]{if(Remote){FHansaClientProjectionSnapshot P;if(!Authority.BuildProjection(915,0,true,P,Error)){AddError(Error);return;}TestFalse(TEXT("No rival quote or receipt"),P.VisitingTrade.ContainsByPredicate([](const auto& O){return O.Vehicle==3;}));TArray<uint8> Bytes;FMemoryWriter W(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(W,&P,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader R(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(R,&Copy,nullptr);TestFalse(TEXT("Wire round trip"),R.IsError());M->ApplyRemoteVisiting(Copy,TEXT("City.Rostock"));}else M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry(),City);};
  Refresh();M->SelectGoodIntent(TEXT("Good.Timber"));auto UI=SNew(Hansa::UI::SHansaMarketTable).Model(M.Get());
  TestTrue(TEXT("Foreign spot controls visible"),M->GetSnapshot().SelectedGood.bSpotTradeVisible);
  TestEqual(TEXT("Multiple ships require explicit choice"),M->GetSnapshot().SelectedGood.SpotTradeVehicleValue,int64(0));TestFalse(TEXT("No implicit purchase"),M->ConfirmSpotTradeIntent());
  TestTrue(TEXT("Choose owned Cog"),UI->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Ship")));TestEqual(TEXT("First explicit ship"),M->GetSnapshot().SelectedGood.SpotTradeVehicleValue,int64(1));
  Refresh();TestEqual(TEXT("Refresh retains ship"),M->GetSnapshot().SelectedGood.SpotTradeVehicleValue,int64(1));
  const auto Widget=UI->ResolveSemanticWidget(TEXT("Market.Detail.SpotTrade.Confirm"));Refresh();TestTrue(TEXT("Live updates preserve native identity"),Widget==UI->ResolveSemanticWidget(TEXT("Market.Detail.SpotTrade.Confirm")));
  auto Hash=[&]{return Host->BuildProjection().Value.GetFingerprint().Value;};auto Money=[&]{const auto P=Host->BuildProjection().Value;return P.GetHouses().FindByPredicate([&](const auto& H){return H.Id==Host->GetHouseId();})->Money.GetRawValue();};auto Receipt=[&]{const auto P=Host->BuildProjection().Value;return P.GetVehicles()[0].LastSpotTrade;};
  const auto Before=Hash();const int64 Cash=Money();TestTrue(TEXT("Estimate has uncertainty"),M->GetSnapshot().SelectedGood.SpotTradeQuote.ToString().Contains(TEXT("ESTIMATE")));TestEqual(TEXT("Review read only"),Before,Hash());
  TestTrue(TEXT("Buy confirmed"),UI->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Confirm")));Refresh();const auto Bought=Receipt();
  const auto AfterBuy=Host->BuildProjection().Value;const auto* CityStock=AfterBuy.GetInventories().FindByPredicate([&](const auto& I){return I.OwnerKind==EHansaInventoryOwnerKind::City&&I.CityId==City;});TestEqual(TEXT("Actual city debit matches receipt"),CityStock->Stocks[0].Stock.GetRawValue(),int64(15000));TestTrue(TEXT("Visible cargo matches receipt"),M->GetSnapshot().SelectedGood.SpotTradeVehicle.ToString().Contains(TEXT("Cargo of this good 5")));TestEqual(TEXT("Bought request"),Bought.AppliedQuantity.GetRawValue(),int64(5000));TestEqual(TEXT("Exact buy debit"),Cash+Bought.SettledMoneyRaw,Money());TestTrue(TEXT("Visible receipt identity"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("Executed receipt")));
  UI->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Side"));M->AdjustSpotTradeQuantityIntent(5000);TestTrue(TEXT("Partial sale"),M->ConfirmSpotTradeIntent());Refresh();TestEqual(TEXT("Partial outcome"),Receipt().Outcome,EHansaSpotTradeOutcome::Partial);TestEqual(TEXT("Sale bounded by own stock"),Receipt().AppliedQuantity.GetRawValue(),int64(5000));TestTrue(TEXT("Partial reason shown"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("ship stock limited")));
  TestTrue(TEXT("Owned manifest reachable"),UI->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Manifest")));TestTrue(TEXT("Manifest content visible"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("manifest")));
  bool Linked=false;M->VisitingLinkRequested=[&](FName C,FName G,int64 Ship,FName Action){Linked=C==TEXT("City.Rostock")&&G==TEXT("Good.Timber")&&Ship==1&&Action==TEXT("Station");};TestTrue(TEXT("Station link"),M->SpotTradeLinkIntent(TEXT("Station")));TestTrue(TEXT("City good ship preserved"),Linked);
  TArray<uint8> Save;const auto SaveHash=Hash();TestTrue(TEXT("Save receipts"),Host->CaptureSaveBytes(Save,TEXT("TG15"),TEXT("2026-09-24T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore receipts"),Host->RestoreSaveBytes(Save).IsSuccess());TestEqual(TEXT("Save fingerprint"),Hash(),SaveHash);Refresh();
  if(Remote){M->CycleSpotTradeSideIntent();Delay=true;TestTrue(TEXT("Queue remote purchase"),M->ConfirmSpotTradeIntent());TestFalse(TEXT("Pending duplicate disabled"),M->ConfirmSpotTradeIntent());auto Stale=Pending;Stale.ReviewedUnitPriceMilliMarks+=1;const auto H=Hash();M->ReceiveCommandFeedback(Authority.SubmitIntent(915,Stale));TestEqual(TEXT("Stale rejection rollback"),H,Hash());Refresh();TestTrue(TEXT("Stale keeps selected ship"),M->GetSnapshot().SelectedGood.SpotTradeVehicleValue==1);TestFalse(TEXT("Rejection feedback"),M->GetSnapshot().SelectedGood.SpotTradeResult.IsEmpty());
   const auto Again=Hash();Authority.SubmitIntent(915,Stale);TestEqual(TEXT("Replay no mutation"),Again,Hash());
  }
 }
 return !HasAnyErrors();
}
#endif
