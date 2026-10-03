#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeLedgerTestSupport.h"
#include "HansaVisitingTradeTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "UI/SHansaMarketTable.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeRemoteParity,"Hansa.UI.TradeMap.Remote.TwoClientParity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeRemoteParity::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,EHansaTradeStationOperationalState::Active,true)){AddError(Error);return false;}
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds={TEXT("City.Lubeck"),TEXT("City.Rostock")};
 auto Admit=[&](uint64 Principal,FHansaHouseId House){return Authority.RegisterAdmittedClient({Principal,FHansaParticipantId::TryCreate(Principal+1000).Value,House,EHansaAdmissionMode::LanOffline},Interest,Error);};
 if(!Admit(917,Host->GetHouseId())||!Admit(918,Host->GetRivalHouseId())){AddError(Error);return false;}
 auto Wire=[&](uint64 Principal){FHansaClientProjectionSnapshot P,Copy;if(!Authority.BuildProjection(Principal,0,true,P,Error)){AddError(Error);return Copy;}TArray<uint8> Bytes;FMemoryWriter W(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(W,&P,nullptr);FMemoryReader R(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(R,&Copy,nullptr);TestFalse(TEXT("Reflected wire round trip"),R.IsError());return Copy;};
 auto Hash=[&]{return Host->BuildProjection().Value.GetFingerprint().Value;};
 auto A=Wire(917),B=Wire(918);
 {TArray<uint8> Bytes;FMemoryWriter W(Bytes,true);bool Success=false;TestTrue(TEXT("Bounded owner report net encode"),A.NetSerialize(W,nullptr,Success)&&Success);TestTrue(TEXT("Compressed property fits actor update"),Bytes.Num()<60*1024);FHansaClientProjectionSnapshot Decoded;FMemoryReader R(Bytes,true);TestTrue(TEXT("Net decode"),Decoded.NetSerialize(R,nullptr,Success)&&Success);TestEqual(TEXT("Names retain stable identity"),Decoded.TradeWorkspace.Cities[0].StableId,A.TradeWorkspace.Cities[0].StableId);TestEqual(TEXT("Owner net identity"),Decoded.OwnerHouseId,A.OwnerHouseId);TestEqual(TEXT("Net route plan"),Decoded.Routes[0].PlanKey,A.Routes[0].PlanKey);
  TArray<uint8> Invalid;FMemoryWriter IW(Invalid);uint32 Oversize=9*1024*1024,Size=1;IW.SerializeIntPacked(Oversize);IW.SerializeIntPacked(Size);FMemoryReader IR(Invalid);TestFalse(TEXT("Oversized report rejected before allocation"),Decoded.NetSerialize(IR,nullptr,Success));TestEqual(TEXT("Invalid report clears private owner"),Decoded.OwnerHouseId,int64(0));}

 TestEqual(TEXT("Both scopes report the same authority"),A.AuthoritativeHash,B.AuthoritativeHash);
 TestTrue(TEXT("Owner receives ordinary route plan"),A.Routes.ContainsByPredicate([](const auto& R){return R.RouteId==100&&!R.Stops.IsEmpty()&&!R.PlanKey.IsEmpty();}));
 TestTrue(TEXT("Rival has no station orders or recovery dossiers"),B.StationOrders.IsEmpty()&&B.Recoveries.IsEmpty());
 for(const auto& R:B.Routes)if(R.OwnerHouseId!=B.OwnerHouseId){TestTrue(TEXT("Rival plan stripped"),R.Stops.IsEmpty()&&R.PlanKey.IsEmpty());TestFalse(TEXT("Rival cargo hidden"),R.bCargoVisible);TestEqual(TEXT("Rival exact progress hidden"),R.ProgressPartsPerMillion,int64(0));}
 TestFalse(TEXT("Rival has no owner schedule"),B.TradeWorkspace.Schedules.ContainsByPredicate([](const auto& S){return S.VehicleId==1||S.VehicleId==2;}));
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->ApplyRemoteEstablishment(A);M->Open();M->SelectCityIntent(TEXT("City.Rostock"));M->SelectRouteIntent(100);
 auto UI=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
 TestEqual(TEXT("Ordinary route available without recovery navigation"),M->GetSnapshot().SelectedRouteValue,int64(100));
 TestFalse(TEXT("Remote route creation explicitly unsupported"),UI->ActivateSemanticId(TEXT("TradeMap.New")));
 TestFalse(TEXT("Unsupported action has reason"),M->RemoteActionReason(TEXT("TradeMap.New")).IsEmpty());
 M->DirectoryIntent(TEXT("Fleet"));TestTrue(TEXT("Unassigned owned Cog visible"),M->GetSnapshot().Directory.ContainsByPredicate([](const auto& D){return D.VehicleValue==2&&D.bOwned;}));M->SelectRouteIntent(100);
 const auto Schedule=M->GetSchedulePresentation();TestEqual(TEXT("Owner manifest survived serialization"),Schedule.VehicleId,int64(1));TestTrue(TEXT("Optional quantities restored"),Schedule.Rows.ContainsByPredicate([](const auto& R){return R.Carried.IsSet()||R.Prepared.IsSet();}));
 FHansaClientCommandIntent Pending;int64 Nonce=17000;bool Delay=true;
 M->SetNetworkCommandIntent([&](const auto& Intent){Pending=Intent;Pending.ClientSequence=Authority.GetExpectedClientSequence(917);Pending.ClientNonce=++Nonce;FHansaClientCommandFeedback F;F.State=EHansaClientCommandState::Pending;F.ClientSequence=Pending.ClientSequence;F.ClientNonce=Pending.ClientNonce;M->ReceiveCommandFeedback(F);if(!Delay)M->ReceiveCommandFeedback(Authority.SubmitIntent(917,Pending));return true;});
 TestTrue(TEXT("Remote draft quantity"),M->AdjustQuantityIntent(1000));TestTrue(TEXT("Remove research-gated reserve"),M->AdjustMinimumReserveIntent(-1000));
 TestTrue(TEXT("Submit route edit"),M->CommitIntent());TestTrue(TEXT("Pending observable"),M->GetSnapshot().bAnyCommandPending);TestFalse(TEXT("Cross-workflow order blocked while pending"),M->StationOrderIntent(TEXT("New")));TestFalse(TEXT("Duplicate edit blocked"),M->CommitIntent());
 FHansaClientCommandFeedback Wrong;Wrong.ClientSequence=Pending.ClientSequence;Wrong.ClientNonce=Pending.ClientNonce+1;Wrong.bAccepted=true;M->ReceiveCommandFeedback(Wrong);TestTrue(TEXT("Unrelated ack cannot clear pending"),M->IsAnyTradeCommandPending());
 const auto Accepted=Authority.SubmitIntent(917,Pending);TestTrue(FString(TEXT("Owner edit accepted: "))+Accepted.Message,Accepted.bAccepted);M->ReceiveCommandFeedback(Accepted);A=Wire(917);B=Wire(918);M->ApplyRemoteEstablishment(A);TestFalse(TEXT("Matching ack releases pending"),M->IsAnyTradeCommandPending());TestEqual(TEXT("Both clients converge after edit"),A.AuthoritativeHash,B.AuthoritativeHash);
 TestEqual(TEXT("Server applied quantity"),Host->BuildProjection().Value.GetRoutes()[0].Stops[0].Actions[0].QuantityLimit.GetRawValue(),int64(2000));
 auto Forged=Pending;Forged.ClientSequence=Authority.GetExpectedClientSequence(918);Forged.ClientNonce=++Nonce;const auto BeforeForgery=Hash();TestFalse(TEXT("Rival cannot edit owner route"),Authority.SubmitIntent(918,Forged).bAccepted);TestEqual(TEXT("Rejected edit conserves state"),Hash(),BeforeForgery);
 M->AdjustQuantityIntent(1000);auto Concurrent=Host->BuildProjection().Value.GetRoutes()[0].Stops;Concurrent[0].Actions[0].QuantityLimit=FHansaQuantity::FromRaw(4000);TestTrue(TEXT("Concurrent owner edit"),Host->EditRoute(FHansaRouteId::TryCreate(100).Value,Concurrent).IsSuccess());const auto BeforeStale=Hash();
 Delay=false;TestTrue(TEXT("Stale draft sent normally"),M->CommitIntent());TestEqual(TEXT("Same-tick stale draft cannot overwrite"),Hash(),BeforeStale);TestTrue(TEXT("Stale edit explains remedy"),M->GetSnapshot().EditorStatus.ToString().Contains(TEXT("Discard edits")));TestTrue(TEXT("Rejected edit preserves draft"),M->GetSnapshot().bDirty);
 M->ApplyRemoteEstablishment(Wire(917));TestTrue(TEXT("Discard reloads current plan"),UI->ActivateSemanticId(TEXT("TradeMap.Editor.Discard")));TestFalse(TEXT("Discard clears dirty flag"),M->GetSnapshot().bDirty);
 const FName Focus(TEXT("TradeMap.Editor.Quantity.Increase"));M->SetFocusedSemanticId(Focus);const auto Widget=UI->ResolveSemanticWidget(Focus.ToString());M->ApplyRemoteEstablishment(Wire(917));TestEqual(TEXT("Live update preserves semantic focus"),M->GetSnapshot().FocusedSemanticId,Focus);TestTrue(TEXT("Live update preserves widget identity"),Widget==UI->ResolveSemanticWidget(Focus.ToString()));
 const auto Recent=Wire(917);M->ApplyRemoteEstablishment(Recent);auto Old=A;Old.TradeWorkspace.Routes.Reset();M->ApplyRemoteEstablishment(Old);TestTrue(TEXT("Reordered snapshot ignored"),M->GetSnapshot().Routes.ContainsByPredicate([](const auto& R){return R.RouteValue==100;}));
 M->AdjustQuantityIntent(1000);M->ApplyRemoteEstablishment(Wire(918));TestFalse(TEXT("Owner change clears private draft"),M->GetSnapshot().bDirty);TestEqual(TEXT("Owner change clears station selection"),M->GetSnapshot().SelectedStationOrderId,int64(0));TestFalse(TEXT("New owner cannot edit old route"),M->AdjustQuantityIntent(1000));
 Authority.UnregisterClient(917);TestTrue(TEXT("Reconnect admission"),Admit(919,Host->GetHouseId()));TStrongObjectPtr<UHansaTradeMapPresentationModel> Rejoined(NewObject<UHansaTradeMapPresentationModel>());Rejoined->InitializeDefaults();Rejoined->ApplyRemoteEstablishment(Wire(919));TestTrue(TEXT("Late join full snapshot rebuilds ordinary directory"),Rejoined->GetSnapshot().Routes.ContainsByPredicate([](const auto& R){return R.RouteValue==100&&R.bOwnedByPlayer;}));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeRemoteSpotScopes,"Hansa.UI.VisitingTrade.TwoClientScopes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeRemoteSpotScopes::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareVisitingTrade(*Host,Error)){AddError(Error);return false;}
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds.Add(TEXT("City.Rostock"));
 for(int32 I=0;I<2;++I)if(!Authority.RegisterAdmittedClient({uint64(920+I),FHansaParticipantId::TryCreate(1920+I).Value,I?Host->GetRivalHouseId():Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
 int64 Nonce=18000;
 for(int32 I=0;I<2;++I){const uint64 Principal=920+I;FHansaClientProjectionSnapshot P;Authority.BuildProjection(Principal,0,true,P,Error);
  TStrongObjectPtr<UHansaMarketTablePresentationModel> M(NewObject<UHansaMarketTablePresentationModel>());M->InitializeDefaults();M->ApplyRemoteVisiting(P,TEXT("City.Rostock"));M->SelectGoodIntent(TEXT("Good.Timber"));if(!I)M->CycleSpotTradeVehicleIntent();else M->CycleSpotTradeSideIntent();
  auto UI=SNew(Hansa::UI::SHansaMarketTable).Model(M.Get());TestFalse(TEXT("Remote new-route link disabled with explanation"),M->GetSnapshot().SelectedGood.bRouteEnabled);TestFalse(TEXT("Disabled route reason"),M->GetSnapshot().SelectedGood.RouteDisabledReason.IsEmpty());
  M->SetNetworkCommandIntent([&](auto Intent){Intent.ClientSequence=Authority.GetExpectedClientSequence(Principal);Intent.ClientNonce=++Nonce;FHansaClientCommandFeedback F;F.State=EHansaClientCommandState::Pending;F.ClientSequence=Intent.ClientSequence;F.ClientNonce=Intent.ClientNonce;M->ReceiveCommandFeedback(F);F=Authority.SubmitIntent(Principal,Intent);TestTrue(FString(TEXT("Each client can trade its own cargo: "))+F.Message,F.bAccepted);M->ReceiveCommandFeedback(F);return true;});
  TestTrue(TEXT("Native semantic spot confirmation"),UI->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Confirm")));
  FHansaClientProjectionSnapshot After;Authority.BuildProjection(Principal,0,true,After,Error);M->ApplyRemoteVisiting(After,TEXT("City.Rostock"));TestTrue(TEXT("Executed receipt projected"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("receipt")));
  M->ApplyRemoteVisiting({},TEXT("City.Rostock"));TestTrue(TEXT("Invalid owner clears private market rows"),M->GetSnapshot().AllRows.IsEmpty());TestFalse(TEXT("Invalid owner cannot submit"),M->GetSnapshot().SelectedGood.bSpotTradeCanSubmit);
 }
 FHansaClientProjectionSnapshot A,B;Authority.BuildProjection(920,0,true,A,Error);Authority.BuildProjection(921,0,true,B,Error);TestEqual(TEXT("Two successful traders converge"),A.AuthoritativeHash,B.AuthoritativeHash);for(const auto& O:A.VisitingTrade)TestTrue(TEXT("A only receives its own ships"),O.Vehicle!=3);for(const auto& O:B.VisitingTrade)TestEqual(TEXT("B only receives its own ship"),O.Vehicle,int64(3));
 return !HasAnyErrors();
}
#endif
