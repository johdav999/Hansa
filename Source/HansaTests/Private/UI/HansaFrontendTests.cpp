#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "UI/HansaFrontendPresentationModel.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrontendState,"Hansa.UI.Frontend.SessionAndSettingsContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFrontendState::RunTest(const FString&){
 TStrongObjectPtr<UHansaFrontendPresentationModel> M(NewObject<UHansaFrontendPresentationModel>());M->Initialize(true);FName Action;M->SetIntent([&](FName A){Action=A;});
 TestFalse(TEXT("Continue rejects empty slots"),M->Request(TEXT("Continue")));
 FHansaSaveSlotMetadata Good;Good.bCanLoad=true;Good.SavedUtc=TEXT("2026-09-09T00:00:00Z");Good.SlotId=EHansaSaveSlotId::Autosave;FHansaSaveSlotMetadata Bad;Bad.SavedUtc=TEXT("2026-09-10T00:00:00Z");M->RefreshSlots({Good,Bad});TestTrue(TEXT("Valid autosave selected over newer corrupt save"),M->GetSnapshot().ContinueSlot==EHansaSaveSlotId::Autosave);
 M->Request(TEXT("Continue"));TestTrue(TEXT("Continue exposes loading"),M->GetSnapshot().Page==EHansaFrontendPage::Loading);TestFalse(TEXT("Loading prevents reentry"),M->Request(TEXT("NewGame")));M->Complete(false,FText::FromString(TEXT("Damaged file")));TestTrue(TEXT("Error exposes recovery"),M->GetSnapshot().Message.ToString().Contains(TEXT("another compatible save")));M->Back();
 M->SessionStarted();M->Request(TEXT("NewGame"));TestTrue(TEXT("New game requires confirmation when playing"),M->GetSnapshot().Page==EHansaFrontendPage::Confirmation);M->Back();TestTrue(TEXT("Cancel keeps session"),M->GetSnapshot().bHasSession);M->Request(TEXT("ReturnTitle"));M->Confirm();TestEqual(TEXT("Confirmed intent routed"),Action,FName(TEXT("ReturnTitle")));
 M->ReturnToTitle();M->OpenSettings();M->LoadPreferences(FPaths::ProjectSavedDir()/TEXT("Automation/Frontend")/(FGuid::NewGuid().ToString()+TEXT(".ini")));M->ChangeSetting(TEXT("VolumeDown"));TestTrue(TEXT("Volume changes real setting intent"),Action==TEXT("SettingsChanged")&&M->GetSnapshot().Volume<1);M->ChangeSetting(TEXT("Window"));TestEqual(TEXT("Display preview routed"),Action,FName(TEXT("PreviewDisplay")));M->Back();TestEqual(TEXT("Display cancellation reverts"),Action,FName(TEXT("RevertDisplay")));M->Back();TestTrue(TEXT("Back still returns to title after display preview"),M->GetSnapshot().Page==EHansaFrontendPage::Title);return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrontendAuthority,"Hansa.Integration.Save.FrontendNewGameAndRejectedRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFrontendAuthority::RunTest(const FString&){
 TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());FString Reason;if(!TestTrue(TEXT("Initialize production profile"),H->InitializeForLubeck(nullptr,Reason)))return false;H->AdvanceTicks(9);TestTrue(TEXT("New game succeeds"),H->StartNewGame(Reason));TestEqual(TEXT("New game clears tick"),H->GetSimulationTick(),int64(0));const auto Fresh=H->BuildProjection();const auto Initial=Fresh.Value.GetFingerprint();
 TestTrue(TEXT("New game has no buildings or roads"),Fresh.Value.GetBuildingWorldProjections().IsEmpty());
 TestTrue(TEXT("New game has no residents or production units"),Fresh.Value.GetPopulationCohorts().IsEmpty()&&Fresh.Value.GetProductions().IsEmpty());
 TestTrue(TEXT("Construction supplies and markets are retained"),!Fresh.Value.GetInventories().IsEmpty());
 TestTrue(TEXT("New-game tick advances without rebuilding starter buildings"),H->AdvanceTicks(1));
 TestTrue(TEXT("Empty city remains empty"),H->BuildProjection().Value.GetBuildingWorldProjections().IsEmpty());
 TestTrue(TEXT("Repeated New Game resets deterministically"),H->StartNewGame(Reason)&&H->BuildProjection().Value.GetFingerprint()==Initial);
 TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());TStrongObjectPtr<UHansaSaveSubsystem> Saves(NewObject<UHansaSaveSubsystem>(Instance.Get()));Saves->UseIsolatedAutomationSlots();Saves->BindRuntime(H.Get());FText Error,Remedy;TestTrue(TEXT("Named manual save"),Saves->Save(EHansaSaveSlotId::Manual,TEXT("Baltic venture"),Error,Remedy));TestEqual(TEXT("Name persists"),Saves->FindSlot(EHansaSaveSlotId::Manual)->DisplayName,FString(TEXT("Baltic venture")));
 const uint8 Corrupt[]={1,2,3};TestTrue(TEXT("Inject only isolated slot"),Saves->WriteAutomationSlot(EHansaSaveSlotId::Autosave,Corrupt));Saves->Refresh();TestTrue(TEXT("Damaged slot classified"),Saves->FindSlot(EHansaSaveSlotId::Autosave)->Compatibility==EHansaSaveSlotCompatibility::Corrupt);TestFalse(TEXT("Damaged restore rejected"),Saves->Load(EHansaSaveSlotId::Autosave,Error,Remedy));TestFalse(TEXT("Damaged slot provides remedy"),Remedy.IsEmpty());TestTrue(TEXT("Failed restore preserves authority"),H->BuildProjection().Value.GetFingerprint()==Initial);return !HasAnyErrors();
}
#endif
