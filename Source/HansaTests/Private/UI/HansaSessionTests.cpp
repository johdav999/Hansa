#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/SHansaScenarioScreen.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionLifecycle,"Hansa.UI.Session.LifecycleAndPersistentHelp",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSessionLifecycle::RunTest(const FString&){
 TStrongObjectPtr<UHansaScenarioPresentationModel> M(NewObject<UHansaScenarioPresentationModel>());M->InitializeDefaults();
 M->AcknowledgeBriefing();TestTrue(TEXT("Loading cannot begin"),M->GetSnapshot().bOpen);
 Hansa::Simulation::FHansaScenarioProgress P;P.ScenarioId=TEXT("Scenario.Test");P.DisplayName=TEXT("Test city");P.Outcome=Hansa::Simulation::EHansaScenarioOutcome::Active;
 M->ApplyProgress(P);M->AcknowledgeBriefing();TestFalse(TEXT("Begin returns to city"),M->GetSnapshot().bOpen);TestTrue(TEXT("Camera help offered"),M->GetSnapshot().bCoachVisible);
 const FString File=FPaths::ProjectSavedDir()/TEXT("Automation/Session")/(FGuid::NewGuid().ToString()+TEXT(".ini"));M->LoadHelpPreferences(File);
 for(int32 I=0;I<6;++I){M->OfferHelp(static_cast<EHansaSessionHelpTopic>(I));TestFalse(TEXT("Every topic has instructions"),M->GetSnapshot().HelpBody.IsEmpty());M->DismissHelp();M->OfferHelp(static_cast<EHansaSessionHelpTopic>(I));TestFalse(TEXT("Dismissal suppresses repeated topic"),M->GetSnapshot().bCoachVisible);}
 TStrongObjectPtr<UHansaScenarioPresentationModel> Restored(NewObject<UHansaScenarioPresentationModel>());Restored->InitializeDefaults();Restored->LoadHelpPreferences(File);TestEqual(TEXT("All dismissals persist"),Restored->GetDismissedHelpMask(),uint32(63));
 M->ToggleHelp();Restored->LoadHelpPreferences(File);TestFalse(TEXT("Opt out persists"),Restored->GetSnapshot().bHelpEnabled);M->ResetHelp();TestEqual(TEXT("Replay clears dismissals"),M->GetDismissedHelpMask(),uint32(0));
 FName SessionAction;M->SetSessionIntent([&](FName Action){SessionAction=Action;});
 M->OpenPause();auto Menu=SNew(Hansa::UI::SHansaScenarioScreen).Model(M.Get());
 TestTrue(TEXT("Pause menu exposes Save/load"),Menu->ResolveSemanticWidget(TEXT("Scenario.SaveLoad")).IsValid());
 TestTrue(TEXT("Pause menu exposes return to title"),Menu->ResolveSemanticWidget(TEXT("Scenario.ReturnTitle")).IsValid());
 TestTrue(TEXT("Save/load activates"),Menu->ActivateSemanticId(TEXT("Scenario.SaveLoad")));TestEqual(TEXT("Save/load intent routed"),SessionAction,FName(TEXT("SaveLoad")));
 TestTrue(TEXT("Return to title activates"),Menu->ActivateSemanticId(TEXT("Scenario.ReturnTitle")));TestEqual(TEXT("Return intent routed"),SessionAction,FName(TEXT("ReturnTitle")));
 M->OpenPause();M->OfferHelp(EHansaSessionHelpTopic::Roads);TestFalse(TEXT("Pause suppresses coaching"),M->GetSnapshot().bCoachVisible);M->ReviewProgress();TestFalse(TEXT("Progress leaves pause navigation"),M->GetSnapshot().bPauseMenu);
 M->SessionRestored();TestTrue(TEXT("Restored session waits in pause menu"),M->GetSnapshot().bPauseMenu&&M->GetSnapshot().bLoadedSession);
 for(auto Outcome:{Hansa::Simulation::EHansaScenarioOutcome::Victory,Hansa::Simulation::EHansaScenarioOutcome::Failure}){P.Outcome=Outcome;P.FailureReason=TEXT("Sustained insolvency");M->ApplyProgress(P);TestTrue(TEXT("New outcome opens"),M->GetSnapshot().bOpen);M->Close();M->ApplyProgress(P);TestFalse(TEXT("Dismissed outcome stays closed on refresh"),M->GetSnapshot().bOpen);}
 return !HasAnyErrors();
}
#endif
