#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeJourneySupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "HAL/FileManager.h"
using namespace Hansa::Simulation;
using namespace Hansa::Tests::TradeJourney;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeJourneyDelivery,"Hansa.UI.TradeJourney.DeterministicImport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeJourneyDelivery::RunTest(const FString&)
{
    auto J=Fixture();if(!TestTrue(TEXT("Versioned journey fixture available"),J.IsValid()))return false;
    uint64 FirstHash=0;
    for(int Repeat=0;Repeat<2;++Repeat)
    {
        TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>()),Control(NewObject<UHansaRuntimeSimulationHost>());
        FString Error;if(!Host->InitializeForLubeck(nullptr,Error)||!Control->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
        if(!TestTrue(TEXT("Reserve automation research completed for both deterministic fixtures"),UnlockReserveAutomation(Host.Get())&&UnlockReserveAutomation(Control.Get())))return false;
        TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
        M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
        M->Open(TEXT("Market.Detail.Action.BeginRoute"),FName(*J->GetStringField(TEXT("good"))),FName(*J->GetStringField(TEXT("source"))));
        auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
        TestEqual(TEXT("Import begins with home unload"),M->GetDraftStops()[0].Actions[0].Kind,EHansaRouteCargoActionKind::Unload);
        TestEqual(TEXT("Source reserve is protected"),M->GetDraftStops()[1].Actions[0].MinimumSourceReserve.GetRawValue(),int64(J->GetNumberField(TEXT("reserveMilliUnits"))));
        for(const auto& Action:J->GetArrayField(TEXT("actions")))TestTrue(*Action->AsString(),Screen->ActivateSemanticId(Action->AsString()));
        const uint64 Route=M->GetSnapshot().SelectedRouteValue;
        TestTrue(TEXT("Created a new circuit"),Route>3);
        int64 Total=0,ControlTotal=0;bool Loaded=false,Delivered=false,LocalPriceResponded=false,RemotePriceResponded=false;FString Log;
        for(int Tick=0;Tick<J->GetIntegerField(TEXT("waitLimitTicks"));++Tick)
        {
            const auto Before=Host->BuildProjection().Value;
            TestTrue(TEXT("Deterministic observable wait"),Host->AdvanceTicks(1)&&Control->AdvanceTicks(1));
            const auto P=Host->BuildProjection().Value,C=Control->BuildProjection().Value;
            Total+=Consumed(P);ControlTotal+=Consumed(C);
            const auto* R=P.GetRoutes().FindByPredicate([&](const auto& X){return X.Id.GetValue()==Route;});
            if(!R){AddError(TEXT("Created route disappeared"));return false;}
            if(R->LastTransfer.Tick==P.GetClock().GetTick()&&R->LastTransfer.AppliedQuantity.GetRawValue()>0)
            {
                if(R->LastTransfer.Kind==EHansaRouteCargoActionKind::Load)
                {
                    Loaded=true;TestTrue(TEXT("Source reserve survived real load"),Stock(P,4)>=5000);
                    TestEqual(TEXT("Loaded quantity enters real hold"),Stock(P,1001)-Stock(Before,1001),R->LastTransfer.AppliedQuantity.GetRawValue());
                }
                else
                {
                    Delivered=true;TestEqual(TEXT("Unloaded quantity leaves real hold"),Stock(Before,1001)-Stock(P,1001),R->LastTransfer.AppliedQuantity.GetRawValue());
                }
                Log+=Evidence(Host.Get(),Route)+TEXT("\n");
            }
            LocalPriceResponded |= Price(P,TEXT("City.Lubeck"))!=Price(C,TEXT("City.Lubeck"));
            RemotePriceResponded |= Price(P,TEXT("City.Rostock"))!=Price(C,TEXT("City.Rostock"));
        }
        TestTrue(TEXT("Real loading and return delivery"),Loaded&&Delivered);
        TestTrue(TEXT("Imported Bread satisfies more actual needs than no-trade control"),Total>ControlTotal);
        TestTrue(TEXT("Both cities prices respond to changed stock/incoming supply"),LocalPriceResponded&&RemotePriceResponded);
        TestTrue(TEXT("Source inventory records exported stock"),Stock(Host->BuildProjection().Value,4)<Stock(Control->BuildProjection().Value,4));
        TArray<uint8> Bytes;const auto Hash=Host->BuildProjection().Value.GetFingerprint().Value;
        TestTrue(TEXT("Save captures trade state"),!!Host->CaptureSaveBytes(Bytes,TEXT("P34"),TEXT("2026-09-09T00:00:00Z")));
        Host->AdvanceTicks(3);TestTrue(TEXT("Restore trade state"),!!Host->RestoreSaveBytes(Bytes));
        TestEqual(TEXT("Save restores exact state"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);
        if(Repeat==0)FirstHash=Hash;else TestEqual(TEXT("Repeated UI fixture is deterministic"),Hash,FirstHash);
        Log+=FString::Printf(TEXT("totalBreadConsumed=%lld\ncontrolBreadConsumed=%lld\n"),Total,ControlTotal);
        FFileHelper::SaveStringToFile(Log,*(FPaths::ProjectSavedDir()/FString::Printf(TEXT("P34/import-repeat-%d.txt"),Repeat)));
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeJourneyRecovery,"Hansa.UI.TradeJourney.Recovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeJourneyRecovery::RunTest(const FString&)
{
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;if(!Host->InitializeForLubeck(nullptr,Error))return false;
    if(!TestTrue(TEXT("Reserve automation research completed for recovery fixture"),UnlockReserveAutomation(Host.Get())))return false;
    TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->BindRuntime(Host.Get());
    auto Refresh=[&]{M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};Refresh();M->Open();
    auto S=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
    S->ActivateSemanticId(TEXT("TradeMap.New"));M->SelectStopIntent(1);M->CycleCargoActionIntent();M->ReviewCreateIntent();TestFalse(TEXT("All-load route rejected"),M->GetSnapshot().bCanCreate);
    M->EditCreateIntent();M->CycleCargoActionIntent();M->SelectStopIntent(0);M->AdjustQuantityIntent(100000);M->AdjustMinimumReserveIntent(1000000);M->ReviewCreateIntent();
    TestTrue(TEXT("Oversized cargo is explicitly limited"),M->GetSnapshot().CreatorReview.ToString().Contains(TEXT("limited by free capacity")));
    TestTrue(TEXT("Insufficient stock warns without promising supply"),M->GetSnapshot().ReserveRisk.ToString().Contains(TEXT("exceeds reported stock")));
    TestFalse(TEXT("Quantity above vessel capacity is rejected"),M->GetSnapshot().bCanCreate);M->EditCreateIntent();M->AdjustQuantityIntent(-100000);M->ReviewCreateIntent();TestTrue(TEXT("Safe zero-load reserve plan is permitted"),M->GetSnapshot().bCanCreate);S->ActivateSemanticId(TEXT("TradeMap.Creator.Activate"));const auto Id=M->GetSnapshot().SelectedRouteValue;
    Host->AdvanceTicks(1);Refresh();TestEqual(TEXT("Reserve prevents cargo creation"),Stock(Host->BuildProjection().Value,1001,TEXT("Good.Grain")),int64(0));
    TestFalse(TEXT("Cannot cancel traveling vessel"),S->ActivateSemanticId(TEXT("TradeMap.Editor.Cancel")));
    M->AdjustQuantityIntent(-5000);TestFalse(TEXT("Moving route cannot edit"),S->ActivateSemanticId(TEXT("TradeMap.Editor.Save")));
    TestTrue(TEXT("Edit error explains recovery and retains draft"),M->GetSnapshot().bDirty&&M->GetSnapshot().EditorStatus.ToString().Contains(TEXT("pause the route")));
    for(int T=0;T<80;++T){Host->AdvanceTicks(1);Refresh();const auto P=Host->BuildProjection().Value;const auto* R=P.GetRoutes().FindByPredicate([&](const auto& X){return X.Id.GetValue()==uint64(Id);});if(R&&R->Lifecycle==EHansaRouteLifecycleState::AtStop&&R->CurrentStopIndex==0)break;}
    TestTrue(TEXT("Dirty draft does not block pause"),S->ActivateSemanticId(TEXT("TradeMap.Editor.ToggleActive")));Refresh();
    M->SelectStopIntent(0);M->AdjustMinimumReserveIntent(-1000000);M->AdjustQuantityIntent(5000);
    TestTrue(TEXT("Stopped empty route accepts repaired draft"),S->ActivateSemanticId(TEXT("TradeMap.Editor.Save")));Refresh();
    TestTrue(TEXT("Cancellation is reachable in controller order"),S->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.Cancel")));
    TestTrue(TEXT("Cancel through ordinary UI"),S->ActivateSemanticId(TEXT("TradeMap.Editor.Cancel")));TestFalse(TEXT("Cancelled route cannot cancel twice"),S->ActivateSemanticId(TEXT("TradeMap.Editor.Cancel")));
    M->SelectRouteIntent(3);TestFalse(TEXT("Rival route cannot be cancelled"),S->ActivateSemanticId(TEXT("TradeMap.Editor.Cancel")));
    Host->AdvanceTicks(300);
    M->Open();M->BeginCreateIntent(TEXT("Good.Bread"),TEXT("City.Rostock"));M->ReviewCreateIntent();
    const auto Report=Host->QueryKnownMarketSupply(FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,FHansaGoodId::TryParse(TEXT("Good.Bread")).Value);
    TestTrue(TEXT("Unvisited report ages rather than revealing current stock"),Report.IsSet()&&Report->InformationState!=EHansaMarketInformationState::Current);
    TestTrue(TEXT("Older reports remain explicit in voyage review"),M->GetSnapshot().CreatorReview.ToString().Contains(TEXT("older stock reports"))||M->GetSnapshot().CreatorReview.ToString().Contains(TEXT("unavailable")));
    return !HasAnyErrors();
}
#endif
