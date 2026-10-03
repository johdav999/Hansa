#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeSpecializationTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Framework/Application/SlateApplication.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeSpecializationJourney,"Hansa.UI.TradeMap.Specialization.Journey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeSpecializationJourney::RunTest(const FString&){
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareSpecialization(*Host,Error)){AddError(Error);return false;}
  TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(Host->GetHouseId());if(!Remote)M->BindRuntime(Host.Get());
  FHansaMultiplayerAuthority Authority;int64 Nonce=10000;bool Delay=false;FHansaClientCommandIntent Pending;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({912,FHansaParticipantId::TryCreate(1912).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   M->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& I){Pending=I;Pending.ClientSequence=Authority.GetExpectedClientSequence(912);Pending.ClientNonce=++Nonce;FHansaClientCommandFeedback Ack;Ack.State=EHansaClientCommandState::Pending;Ack.ClientSequence=Pending.ClientSequence;Ack.ClientNonce=Pending.ClientNonce;M->ReceiveCommandFeedback(Ack);if(!Delay)M->ReceiveCommandFeedback(Authority.SubmitIntent(912,Pending));return true;});
  }
  auto Refresh=[&]{if(Remote){FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(912,0,true,Wire,Error)){AddError(Error);return;}
   TestEqual(TEXT("Only owner office replicated"),Wire.Presences.Num(),1);
   TestTrue(TEXT("Completed office visual state replicated"),Wire.Presences.Num()==1&&Wire.Presences[0].bOfficeBuilt);
   TArray<uint8> Bytes;FMemoryWriter W(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(W,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader R(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(R,&Copy,nullptr);TestFalse(TEXT("Comparison survives wire encoding"),R.IsError());M->ApplyRemoteEstablishment(Copy);
   TestTrue(TEXT("Completed office visual state survives wire encoding"),Copy.Presences.Num()==1&&Copy.Presences[0].bOfficeBuilt);
  }else M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};
  Refresh();M->Open();M->SelectCityIntent(TEXT("City.Rostock"));auto Ui=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
  Ui->ActivateSemanticId(TEXT("TradeMap.Navigate.Specialization"));
  auto Act=[&](const FString& S){return Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Specialization.")+S);};
  TestEqual(TEXT("Three policy options"),M->GetSnapshot().Specialization.Options.Num(),3);
  TestTrue(TEXT("No automatic branch or source"),M->GetSnapshot().SelectedPresenceSpecializationId.IsEmpty()&&M->GetSnapshot().SpecializationSourceId.IsEmpty());
  for(const auto& O:M->GetSnapshot().Specialization.Options){TestEqual(TEXT("All eight comparison dimensions"),O.Dimensions.Num(),8);for(const auto& S:O.Sources)TestEqual(TEXT("Only owned station funds specialization"),S.Id,FString(TEXT("20")));for(int32 D=0;D<8;++D)TestTrue(TEXT("Each dimension is controller reachable"),Ui->GetControllerFocusOrder().Contains(FString::Printf(TEXT("TradeMap.Presence.Specialization.Dimension.%s.%d"),*O.Id,D)));}
  TestTrue(TEXT("Choose Warehouse"),Act(TEXT("Warehouse")));TestTrue(TEXT("Choose exact source"),Act(TEXT("Source")));
  const auto Stable=Ui->ResolveSemanticWidget(TEXT("TradeMap.Presence.Specialization.Dimension.Warehouse.1"));Refresh();TestTrue(TEXT("Refresh preserves dimension widget"),Stable==Ui->ResolveSemanticWidget(TEXT("TradeMap.Presence.Specialization.Dimension.Warehouse.1")));
  const uint64 Before=Host->BuildProjection().Value.GetFingerprint().Value;
  TestTrue(TEXT("Review opens separately"),Act(TEXT("Apply")));TestEqual(TEXT("Review spends nothing"),Before,Host->BuildProjection().Value.GetFingerprint().Value);
  TestTrue(TEXT("Exact funding source disclosed"),M->GetSnapshot().SpecializationReview.ToString().Contains(TEXT("20")));
  TestTrue(TEXT("Cancel review"),Act(TEXT("Cancel")));TestFalse(TEXT("Unreviewed confirm refused"),Act(TEXT("Confirm")));
  Act(TEXT("Apply"));Host->AdvanceTicks(1);Refresh();
  TestFalse(TEXT("Changed treasury invalidates review"),M->GetSnapshot().bSpecializationReview);TestFalse(TEXT("Stale confirmation cannot spend"),Act(TEXT("Confirm")));
  TestEqual(TEXT("Source survives stale recovery"),M->GetSnapshot().SpecializationSourceId,FString(TEXT("20")));
  Act(TEXT("Apply"));TestTrue(TEXT("Confirm investment"),Act(TEXT("Confirm")));Refresh();
  TestTrue(TEXT("Warehouse becomes current"),M->GetSnapshot().Specialization.Options.ContainsByPredicate([](const auto& O){return O.Id==TEXT("Warehouse")&&O.bCurrent;}));
  TestEqual(TEXT("Focus returns to chosen branch"),M->GetSnapshot().FocusedSemanticId,FName(TEXT("TradeMap.Presence.Specialization.Warehouse")));
  TestTrue(TEXT("Compare Market"),Act(TEXT("Market")));TestTrue(TEXT("Review respec"),Act(TEXT("Apply")));
  TestTrue(TEXT("Materials are non-refundable"),M->GetSnapshot().SpecializationReview.ToString().Contains(TEXT("never refunded")));
  TestTrue(TEXT("Confirm replacement"),Act(TEXT("Confirm")));Refresh();
  TestTrue(TEXT("Market replaces Warehouse"),M->GetSnapshot().Specialization.Options.ContainsByPredicate([](const auto& O){return O.Id==TEXT("Market")&&O.bCurrent;}));
  TArray<uint8> Save;TestTrue(TEXT("Save selected specialization"),Host->CaptureSaveBytes(Save,TEXT("TG12"),TEXT("2026-09-23T00:00:00Z")).IsSuccess());TestTrue(TEXT("Reload selected specialization"),Host->RestoreSaveBytes(Save).IsSuccess());Refresh();TestEqual(TEXT("Revision restored"),M->GetSnapshot().Specialization.Revision,int64(2));
  if(Remote){Act(TEXT("Harbor"));Act(TEXT("Apply"));Delay=true;TestTrue(TEXT("Remote submit queued"),Act(TEXT("Confirm")));TestFalse(TEXT("Pending prevents double submit"),Act(TEXT("Confirm")));
   // Another authoritative selection wins before the queued command arrives.
   const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
   TestTrue(TEXT("Competing authoritative change accepted"),Host->ApplyPresenceSpecialization({City,TEXT("Warehouse"),FHansaInventoryId::TryCreate(20).Value,EHansaPresenceSpecializationAction::Respec,2}).IsSuccess());
   M->ReceiveCommandFeedback(Authority.SubmitIntent(912,Pending));Refresh();TestFalse(TEXT("Rejected remote review cleared"),M->GetSnapshot().bSpecializationReview);TestFalse(TEXT("Rejected remote leaves pending"),M->GetSnapshot().bSpecializationPending);TestTrue(TEXT("Rejection explains recovery"),!M->GetSnapshot().PresenceSpecializationFeedback.IsEmpty());
  }
  for(FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1920,1080)}){Ui->SetPresentationSize(Size);Ui->ActivateSemanticId(TEXT("TradeMap.Navigate.Specialization"));TestTrue(TEXT("Every option reachable after reflow"),Ui->GetControllerFocusOrder().Contains(TEXT("TradeMap.Presence.Specialization.Harbor")));}
 }
 return !HasAnyErrors();
}
#endif
