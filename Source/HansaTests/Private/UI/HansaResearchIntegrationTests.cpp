#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/HansaResearchPresentationModel.h"
#include "UI/SHansaResearchScreen.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResearchProgressSave,"Hansa.UI.Research.ProgressUnlockSaveLoad",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FResearchProgressSave::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;if(!TestTrue(TEXT("Runtime initialized"),Host->InitializeForLubeck(nullptr,Error)))return false;
    TStrongObjectPtr<UHansaResearchPresentationModel> Model(NewObject<UHansaResearchPresentationModel>());
    auto Refresh=[&]{const auto P=Host->BuildProjection();Model->Refresh(*Host->GetEconomicRegistry(),P.Value.GetResearch()[0]);};
    Refresh();Model->Open(TEXT("HUD.TopStatus.Research.Open"));
    Model->SetQueueIntent([&](const FString& Id){const bool Ok=!!Host->QueueResearch(Id);Refresh();return Ok;});
    auto Screen=SNew(Hansa::UI::SHansaResearchScreen).Model(Model.Get());
    const FString RootId=TEXT("Technology.Commerce.MarketReports"),ChildId=TEXT("Technology.Commerce.TransactionFriction");
    Model->SelectTechnology(ChildId);
    TestFalse(TEXT("Prerequisite blocks normal UI"),Model->RequestQueueSelected());
    TestTrue(TEXT("Required technology link is navigable"),Screen->ActivateSemanticId(TEXT("Research.Prerequisite.Technology_Commerce_MarketReports")));
    TestTrue(TEXT("Queue via ordinary screen intent"),Screen->ActivateSemanticId(TEXT("Research.Action.Queue")));
    const auto Active=Host->BuildProjection().Value.GetResearch()[0];
    TestEqual(TEXT("Stable active research"),Active.ActiveTechnologyId,RootId);
    TestTrue(TEXT("Progress advances through command tick"),Active.ProgressTicks>0);
    TArray<uint8> InProgress;
    TestTrue(TEXT("Save active progress"),!!Host->CaptureSaveBytes(InProgress,TEXT("P27 progress"),TEXT("2026-09-09T13:00:00Z")));
    Host->AdvanceTicks(100);
    TestTrue(TEXT("Restore active research"),!!Host->RestoreSaveBytes(InProgress));
    const auto Restored=Host->BuildProjection().Value.GetResearch()[0];
    TestEqual(TEXT("Progress survives real codec"),Restored.ProgressTicks,Active.ProgressTicks);
    TestEqual(TEXT("Charged balance survives codec"),Restored.AvailableResearchPoints,Active.AvailableResearchPoints);
    TestEqual(TEXT("Active identity survives codec"),Restored.ActiveTechnologyId,Active.ActiveTechnologyId);
    Host->AdvanceTicks(100);Refresh();
    const auto Completed=Host->BuildProjection().Value.GetResearch()[0];
    TestTrue(TEXT("Research completes"),Completed.IsCompleted(RootId));
    const auto* Node=Model->GetSnapshot().Nodes.FindByPredicate([&](const auto& N){return N.StableId==RootId;});
    TestTrue(TEXT("Applied effects use authoritative records"),Node&&!Node->AppliedEffectSummary.IsEmpty());
    const auto Semantics=Screen->GetSemanticSnapshot();
    const auto* Action=Semantics.FindByPredicate([](const auto& N){return N.Id==TEXT("Research.Action.Queue");});
    TestTrue(TEXT("Completed action is disabled in the actual shared component"),Action&&!Action->State.bEnabled&&!Action->bCanActivate);
    TestFalse(TEXT("Disabled action is excluded from controller navigation"),Screen->GetControllerFocusOrder().Contains(TEXT("Research.Action.Queue")));

    TArray<uint8> Unlock;
    TestTrue(TEXT("Save completed effects"),!!Host->CaptureSaveBytes(Unlock,TEXT("P27 unlock"),TEXT("2026-09-09T13:00:00Z")));
    TestTrue(TEXT("Restore completed effects"),!!Host->RestoreSaveBytes(Unlock));Refresh();
    TestEqual(TEXT("Applied effects survive exactly once"),Host->BuildProjection().Value.GetResearch()[0].AppliedEffects,Completed.AppliedEffects);
    Model->SelectTechnology(ChildId);
    TestTrue(TEXT("Restored completion unlocks dependent research"),Model->RequestQueueSelected());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResearchFeedback,"Hansa.UI.Research.EligibilityFeedbackAndCausalTargets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FResearchFeedback::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
    if(!Host->InitializeForLubeck(nullptr,Error))return false;
    TStrongObjectPtr<UHansaResearchPresentationModel> Model(NewObject<UHansaResearchPresentationModel>());
    const auto State=Host->BuildProjection().Value.GetResearch()[0];const auto& Registry=*Host->GetEconomicRegistry();
    Model->Open(TEXT("HUD.TopStatus.Research.Open"));
    TestTrue(TEXT("Initial loading is explicit"),Model->GetSnapshot().bLoading);
    Model->Refresh(Registry,State);
    for(const auto& Node:Model->GetSnapshot().Nodes)TestEqual(TEXT("Presentation delegates eligibility to executor"),Node.QueueError,FHansaResearchExecutor::CanQueue(State,Node.StableId,Registry.GetTechnologies()).Error);
    Model->SelectTechnology(TEXT("Technology.Production.ImprovedMilling"));
    Model->SetQueueIntent([](const FString&){return false;});
    TestFalse(TEXT("Rejected submission"),Model->RequestQueueSelected());
    TestFalse(TEXT("Failure leaves no indefinite busy state"),Model->GetSnapshot().bSubmitting);
    TestFalse(TEXT("Failure is visible"),Model->GetSnapshot().Feedback.IsEmpty());
    FString Target;Model->SetEffectIntent([&](const auto& E){Target=E.TargetStableId;return true;});
    TestTrue(TEXT("Effect intent resolves stable target"),Model->RequestEffect(0));
    TestEqual(TEXT("Recipe identity, not display text"),Target,FString(TEXT("Recipe.MillFlour")));
    Model->SetEffectIntent([](const auto&){return false;});
    TestFalse(TEXT("Absent target is honest"),Model->RequestEffect(0));
    TestTrue(TEXT("Absent target explains recovery"),Model->GetSnapshot().Feedback.ToString().Contains(TEXT("No affected")));
    auto Poor=State;Poor.AvailableResearchPoints=0;Model->Refresh(Registry,Poor);
    const auto* N=Model->GetSnapshot().Nodes.FindByPredicate([](const auto& It){return It.StableId==TEXT("Technology.Production.ImprovedMilling");});
    TestTrue(TEXT("Insufficient points are explained"),N&&N->LockedReason.ToString().Contains(TEXT("You have 0")));
    Model->Refresh(Registry,State);TestTrue(TEXT("Fresh authoritative snapshot clears stale error"),Model->GetSnapshot().Feedback.IsEmpty());
    return !HasAnyErrors();
}
#endif
