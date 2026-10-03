#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeEditorSafety,"Hansa.UI.TradeCreator.ReviewAndAcknowledgement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeEditorSafety::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Session"),Host->InitializeForLubeck(nullptr,Error)))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
 Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();Model->BeginCreateIntent();
 TestEqual(TEXT("No silent ship choice"),Model->GetSnapshot().CogValue,int64(0));
 Model->ReviewCreateIntent();TestFalse(TEXT("Ship choice required"),Model->GetSnapshot().bCanCreate);
 Model->EditCreateIntent();Model->CycleCogIntent();Model->SelectStopIntent(0);Model->AdjustMinimumReserveIntent(-5000);Model->ReviewCreateIntent();
 TestTrue(TEXT("Explicit ship gives valid review"),Model->GetSnapshot().bCanCreate);
 TArray<uint8> BeforeChange;Host->CaptureSaveBytes(BeforeChange,TEXT("TG05"),TEXT("2026-09-23T00:00:00Z"));
 Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true);
 TestFalse(TEXT("Changed assignment requires explicit fresh review"),Model->CreateAndActivateIntent());
 TestTrue(TEXT("Stale review retains draft"),Model->GetSnapshot().bCreating&&Model->GetSnapshot().bDirty&&!Model->GetSnapshot().bReview);
 Host->RestoreSaveBytes(BeforeChange);Model->ReviewCreateIntent();
 Model->DiscardCreateIntent();TestTrue(TEXT("Discard first asks"),Model->GetSnapshot().bDiscardConfirmation&&Model->GetSnapshot().bCreating);
 Model->KeepDraftIntent();TestTrue(TEXT("Keep restores draft"),Model->GetSnapshot().bCreating&&!Model->GetSnapshot().bDiscardConfirmation);
 Model->ReviewCreateIntent();
 FHansaClientCommandIntent Sent;
 Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Intent){Sent=Intent;FHansaClientCommandFeedback Pending;Pending.ClientSequence=11;Pending.ClientNonce=91;Model->ReceiveCommandFeedback(Pending);return true;});
 TestTrue(TEXT("Submit remote create"),Model->CreateAndActivateIntent());
 TestEqual(TEXT("Typed create"),Sent.Type,EHansaClientIntentType::CreateRoute);
 TestTrue(TEXT("Pending preserves draft"),Model->GetSnapshot().bCreating&&Model->GetSnapshot().bDirty&&Model->GetSnapshot().bCommandPending);
 TestFalse(TEXT("No duplicate while pending"),Model->CreateAndActivateIntent());
 TestFalse(TEXT("No editing submitted draft"),Model->AdjustQuantityIntent(5000));
 FHansaClientCommandFeedback Rejected;Rejected.ClientSequence=12;Rejected.ClientNonce=91;Rejected.State=EHansaClientCommandState::Rejected;Rejected.Message=TEXT("Ship unavailable.");Rejected.Remedy=TEXT("Choose another ship.");
 Model->ReceiveCommandFeedback(Rejected);TestTrue(TEXT("Unrelated feedback ignored"),Model->GetSnapshot().bCommandPending);
 Rejected.ClientSequence=11;Model->ReceiveCommandFeedback(Rejected);
 TestTrue(TEXT("Rejection preserves editable draft"),Model->GetSnapshot().bCreating&&Model->GetSnapshot().bDirty&&!Model->GetSnapshot().bCommandPending);
 TestTrue(TEXT("Rejection names remedy"),Model->GetSnapshot().Validation.ToString().Contains(TEXT("Choose another ship")));
 Model->ReviewCreateIntent();TestTrue(TEXT("Retry"),Model->CreateAndActivateIntent());
 Rejected.bAccepted=true;Rejected.State=EHansaClientCommandState::Accepted;Model->ReceiveCommandFeedback(Rejected);
 TestFalse(TEXT("Acknowledged creation leaves draft"),Model->GetSnapshot().bCreating);
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeEditorRemote,"Hansa.UI.TradeCreator.RemoteProjectionDraft",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeEditorRemote::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error))return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
 Model->InitializeDefaults();Model->SetViewerHouse(Host->GetHouseId());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();
 TArray<EHansaClientIntentType> Sent;
 Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& I){Sent.Add(I.Type);return false;});
 Model->SelectRouteIntent(1);Model->AdjustQuantityIntent(5000);Model->CommitIntent();
 TestTrue(TEXT("Rejected transport preserves edit"),Model->GetSnapshot().bDirty);
 TestTrue(TEXT("Remote edit intent"),Sent.Contains(EHansaClientIntentType::EditRoute));
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Land(NewObject<UHansaTradeMapPresentationModel>());
 Land->InitializeDefaults();Land->SetViewerHouse(Host->GetHouseId());Land->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Land->SelectRouteIntent(2);
 TestTrue(TEXT("Existing land route city is editable"),Land->CycleStopCityIntent());
 TestFalse(TEXT("Land city choice does not inject a Baltic sea stop"),Land->GetDraftStops()[0].CityId.ToString()==TEXT("City.Rostock"));
 // Separate model avoids discarding an existing edit to create a new route.
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Creator(NewObject<UHansaTradeMapPresentationModel>());
 Creator->InitializeDefaults();Creator->SetViewerHouse(Host->GetHouseId());Creator->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Creator->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& I){Sent.Add(I.Type);return false;});
 Creator->BeginCreateIntent();TestTrue(TEXT("Remote explicit ship selection"),Creator->CycleCogIntent());Creator->ReviewCreateIntent();
 TestTrue(TEXT("Remote preliminary review can submit"),Creator->GetSnapshot().bCanCreate);
 Creator->CreateAndActivateIntent();TestTrue(TEXT("Remote create typed intent"),Sent.Contains(EHansaClientIntentType::CreateRoute));
 TestTrue(TEXT("Failed send preserves creator"),Creator->GetSnapshot().bCreating&&!Creator->GetSnapshot().bCommandPending);
 return !HasAnyErrors();
}
#endif
