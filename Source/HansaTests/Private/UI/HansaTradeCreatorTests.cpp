#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/Input/SEditableTextBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeCreatorDelivery,"Hansa.UI.TradeCreator.CreateDeliverSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeCreatorDelivery::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error; if(!TestTrue(TEXT("Runtime ready"),Host->InitializeForLubeck(nullptr,Error)))return false;
    TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
    Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();
    auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
    const auto Before=Host->BuildProjection();const uint64 Sequence=Host->GetLastProcessedCommandSequence();
    TestTrue(TEXT("New route uses ordinary UI"),Screen->ActivateSemanticId(TEXT("TradeMap.New")));
    TestTrue(TEXT("Creator is distinct from an existing route"),Model->GetSnapshot().bCreating&&Model->GetSnapshot().SelectedRouteValue==0);
    TestTrue(TEXT("Cog selected through normal control"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Cog")));
    auto Input=StaticCastSharedPtr<SEditableTextBox>(Screen->ResolveSemanticWidget(TEXT("TradeMap.Creator.Name")));
    Input->SetText(FText::FromString(TEXT("Lübeck provisions")));
    TestEqual(TEXT("Native name field drives model"),Model->GetSnapshot().DraftName,FString(TEXT("Lübeck provisions")));
    TestTrue(TEXT("Add stop"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Add")));
    TestTrue(TEXT("Remove stop"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Remove")));
    Model->SelectStopIntent(0);Screen->ActivateSemanticId(TEXT("TradeMap.Editor.Reserve.Decrease"));
    TestTrue(TEXT("Review before commit"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Review")));
    TestTrue(TEXT("Review validates"),Model->GetSnapshot().bCanCreate);
    TestEqual(TEXT("Preview is read-only"),Host->GetLastProcessedCommandSequence(),Sequence);
    TestTrue(TEXT("Reassignment is explicit"),Model->GetSnapshot().CreatorReview.ToString().Contains(TEXT("cancels its stopped route")));
    TestTrue(TEXT("Cash result deducts upkeep without fictional sales"),Model->GetSnapshot().CreatorReview.ToString().Contains(TEXT("-240"))&&Model->GetSnapshot().CreatorReview.ToString().Contains(TEXT("no automatic sale revenue")));
    TestTrue(TEXT("Create and activate"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Activate")));
    const uint64 Id=Model->GetSnapshot().SelectedRouteValue;
    TestTrue(TEXT("New identity, not starter edit"),Id>3);
    TestFalse(TEXT("Repeat activation cannot create duplicate"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Activate")));
    const auto Created=Host->BuildProjection();TestEqual(TEXT("Exactly one new route"),Created.Value.GetRoutes().Num(),Before.Value.GetRoutes().Num()+1);
    const auto* Old=Created.Value.GetRoutes().FindByPredicate([](const auto& R){return R.Id.GetValue()==1;});
    TestTrue(TEXT("Old stopped route cancelled atomically"),Old&&Old->Lifecycle==EHansaRouteLifecycleState::Cancelled);
    bool Delivered=false;
    for(int32 I=0;I<80&&!Delivered;++I){Host->AdvanceTicks(1);const auto P=Host->BuildProjection();const auto* R=P.Value.GetRoutes().FindByPredicate([Id](const auto& It){return It.Id.GetValue()==Id;});Delivered=R&&R->LastTransfer.Kind==EHansaRouteCargoActionKind::Unload&&R->LastTransfer.CityId.ToString()==TEXT("City.Rostock")&&R->LastTransfer.AppliedQuantity.GetRawValue()>0;}
    TestTrue(TEXT("New route delivers real cargo to Rostock"),Delivered);
    TArray<uint8> Bytes;TestTrue(TEXT("Named route save encodes"),!!Host->CaptureSaveBytes(Bytes,TEXT("P26"),TEXT("2026-09-09T12:00:00Z")));
    TestTrue(TEXT("Save restores"),!!Host->RestoreSaveBytes(Bytes));
    TestEqual(TEXT("Player route name survives save/load"),Host->GetRouteLabel(Id),FString(TEXT("Lübeck provisions")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeCreatorRecovery,"Hansa.UI.TradeCreator.ValidationAndRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeCreatorRecovery::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
    if(!Host->InitializeForLubeck(nullptr,Error))return false;
    TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
    Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
    Model->Open(TEXT("Market.Detail.Action.BeginRoute"),TEXT("Good.Bread"));
    TestEqual(TEXT("Market good seeds actual draft"),Model->GetDraftStops()[0].Actions[0].GoodId.ToString(),FString(TEXT("Good.Bread")));
    TestTrue(TEXT("Market opens creation"),Model->GetSnapshot().bCreating);
    Model->SetRouteNameIntent(TEXT(""));Model->ReviewCreateIntent();TestFalse(TEXT("Empty name invalid"),Model->GetSnapshot().bCanCreate);
    Model->SetRouteNameIntent(TEXT("Rostock supplies"));Model->CycleStopCityIntent();Model->ReviewCreateIntent();
    TestFalse(TEXT("Duplicate adjacent cities invalid"),Model->GetSnapshot().bCanCreate);
    const uint64 Sequence=Host->GetLastProcessedCommandSequence();TestFalse(TEXT("Invalid cannot commit"),Model->CreateAndActivateIntent());
    TestEqual(TEXT("Failure leaves command history unchanged"),Host->GetLastProcessedCommandSequence(),Sequence);
    Model->CycleStopCityIntent();Model->CloseIntent();Model->Open();TestTrue(TEXT("Close/reopen retains draft"),Model->GetSnapshot().bCreating);
    TestEqual(TEXT("Close/reopen retains name"),Model->GetSnapshot().DraftName,FString(TEXT("Rostock supplies")));
    Model->ReviewCreateIntent();TestTrue(TEXT("Corrected draft can retry"),Model->GetSnapshot().bCanCreate);
    const auto InvalidStops=TArray<FHansaRouteStop>{Model->GetDraftStops()[0],Model->GetDraftStops()[0]};uint64 Id=0;
    TestFalse(TEXT("Backend rejects invalid plan after candidate cancellation"),!!Host->CreateTradeRoute(FHansaVehicleId::TryCreate(1).Value,InvalidStops,TEXT("Invalid"),true,false,Id));
    const auto P=Host->BuildProjection();const auto* Old=P.Value.GetRoutes().FindByPredicate([](const auto& R){return R.Id.GetValue()==1;});
    TestTrue(TEXT("Atomic rollback preserves old route"),Old&&Old->Lifecycle==EHansaRouteLifecycleState::Inactive);
    TestEqual(TEXT("Atomic rollback preserves history"),Host->GetLastProcessedCommandSequence(),Sequence);
    Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true);Host->AdvanceTicks(1);Model->ReviewCreateIntent();
    TestFalse(TEXT("Busy Cog invalidates reviewed draft"),Model->GetSnapshot().bCanCreate);
    TestFalse(TEXT("Busy Cog cannot be stolen"),Model->CreateAndActivateIntent());
    return !HasAnyErrors();
}
#endif
