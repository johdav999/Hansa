#include "Misc/AutomationTest.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/SHansaSaveLoadScreen.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/GameInstance.h"
#include "UI/HansaFrontendPresentationModel.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaLandOverlay.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveLoadUiSemanticTest,
	"Hansa.UI.SaveLoad.SemanticsFocusAndActionableError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSaveLoadUiSemanticTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UHansaSaveLoadPresentationModel* Model = NewObject<UHansaSaveLoadPresentationModel>();
	Model->Bind(nullptr);
	Model->Open();
	TSharedRef<Hansa::UI::SHansaSaveLoadScreen> Screen = SNew(Hansa::UI::SHansaSaveLoadScreen).Model(Model);
	const TArray<FString> FocusOrder = Screen->GetControllerFocusOrder();
	TestTrue(TEXT("Close is keyboard/controller reachable"), FocusOrder.Contains(TEXT("SaveLoad.Close")));
	TestFalse(TEXT("Unavailable save is excluded from keyboard/controller navigation"), FocusOrder.Contains(TEXT("SaveLoad.Action.Save")));
	TestFalse(TEXT("Unavailable new save is excluded from navigation"), FocusOrder.Contains(TEXT("SaveLoad.Action.NewSave")));
	TestFalse(TEXT("Unavailable load is excluded from keyboard/controller navigation"), FocusOrder.Contains(TEXT("SaveLoad.Action.Load")));
	Model->RequestLoad();
	TestEqual(TEXT("Unavailable slot becomes an explicit error"), Model->GetSnapshot().Status, EHansaSaveLoadStatus::Error);
	TestFalse(TEXT("Unavailable slot supplies an actionable remedy"), Model->GetSnapshot().StatusRemedy.IsEmpty());
	const TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Screen->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Status = Nodes.FindByPredicate([](const Hansa::UI::FHansaHudSemanticNode& Node){ return Node.Id == TEXT("SaveLoad.Status"); });
	TestTrue(TEXT("SaveLoad.Status is semantically exposed as an error"), Status != nullptr && Status->State.bError && Status->State.bVisible);
	TestTrue(TEXT("Semantic Close follows the normal presenter intent"), Screen->ActivateSemanticId(TEXT("SaveLoad.Close")));
	TestFalse(TEXT("Close intent collapses the screen"), Model->GetSnapshot().bOpen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaNewManualSaveTest,
	"Hansa.UI.SaveLoad.NewManualSavesPreserveExistingSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaNewManualSaveTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UHansaSaveSubsystem> Saves(NewObject<UHansaSaveSubsystem>(Instance.Get()));
	TStrongObjectPtr<UHansaSaveLoadPresentationModel> Model(NewObject<UHansaSaveLoadPresentationModel>());
	FString InitError;
	if (!TestTrue(TEXT("Runtime initializes"), Host->InitializeForLubeck(nullptr, InitError))) { AddError(InitError); return false; }
	Saves->UseIsolatedAutomationSlots(); Saves->BindRuntime(Host.Get());
	FText Error, Remedy;
	if (!TestTrue(TEXT("Legacy manual save exists"), Saves->Save(EHansaSaveSlotId::Manual, TEXT("Original"), Error, Remedy)) ||
		!TestTrue(TEXT("Autosave exists"), Saves->Save(EHansaSaveSlotId::Autosave, TEXT("Autosave"), Error, Remedy))) return false;
	const FString OriginalHash = Saves->FindSave(TEXT("manual"))->AuthoritativeHash;
	const FString AutosaveDate = Saves->FindSave(TEXT("autosave"))->SavedUtc;
	Model->Bind(Saves.Get()); Model->Open();
	TSharedRef<Hansa::UI::SHansaSaveLoadScreen> Screen = SNew(Hansa::UI::SHansaSaveLoadScreen).Model(Model.Get());
	Host->AdvanceTicks(3); const int64 FirstTick = Host->GetSimulationTick();
	Model->SetSaveName(TEXT("Checkpoint"));
	TestTrue(TEXT("New save uses normal semantic action"), Screen->ActivateSemanticId(TEXT("SaveLoad.Action.NewSave")));
	const FName FirstId = Model->GetSnapshot().SelectedSaveId;
	TestTrue(TEXT("New save has separate identity"), FirstId != TEXT("manual") && FirstId != TEXT("autosave"));
	TestEqual(TEXT("New save needs no overwrite confirmation"), Model->GetSnapshot().Confirmation, EHansaSaveLoadConfirmation::None);
	Host->AdvanceTicks(4);
	TestTrue(TEXT("Same display name creates another save"), Screen->ActivateSemanticId(TEXT("SaveLoad.Action.NewSave")));
	const FName SecondId = Model->GetSnapshot().SelectedSaveId;
	TestTrue(TEXT("Duplicate names have independent identities"), FirstId != SecondId);
	TestEqual(TEXT("Legacy manual save preserved"), Saves->FindSave(TEXT("manual"))->AuthoritativeHash, OriginalHash);
	TestEqual(TEXT("Autosave preserved"), Saves->FindSave(TEXT("autosave"))->SavedUtc, AutosaveDate);
	Saves->Refresh();
	TestEqual(TEXT("Both new saves rediscovered from disk"), Saves->GetSlots().Num(), 4);
	TStrongObjectPtr<UHansaFrontendPresentationModel> Frontend(NewObject<UHansaFrontendPresentationModel>());
	Frontend->Initialize(true);
	auto ContinueSlots = Saves->GetSlots();
	for (auto& Slot : ContinueSlots) if (Slot.StableId == SecondId) Slot.SavedUtc = TEXT("2099-01-01T00:00:00Z");
	Frontend->RefreshSlots(ContinueSlots);
	TestEqual(TEXT("Continue identifies latest named save"), Frontend->GetSnapshot().ContinueSaveId, SecondId);
	TestTrue(TEXT("First save selected by unique semantic ID"), Screen->ActivateSemanticId(TEXT("SaveLoad.Slot.") + FirstId.ToString()));
	TestEqual(TEXT("Selection distinguishes manual saves"), Model->GetSnapshot().SelectedSaveId, FirstId);
	Model->RequestLoad(); Model->Confirm();
	TestEqual(TEXT("Loads the selected checkpoint"), Host->GetSimulationTick(), FirstTick);
	TestFalse(TEXT("Successful load closes the save/load screen"), Model->GetSnapshot().bOpen);
	Model->Open();
	Model->RequestSave();
	TestEqual(TEXT("Overwrite still requires confirmation"), Model->GetSnapshot().Confirmation, EHansaSaveLoadConfirmation::Overwrite);
	TestFalse(TEXT("New save blocked during confirmation"), Screen->ActivateSemanticId(TEXT("SaveLoad.Action.NewSave")));
	Model->CancelConfirmation(); Model->SetSavingAllowed(false);
	TestFalse(TEXT("New save disabled outside gameplay"), Screen->ActivateSemanticId(TEXT("SaveLoad.Action.NewSave")));
	TestFalse(TEXT("Path traversal rejected"), Saves->SaveById(TEXT("../escape"), TEXT("Invalid"), Error, Remedy));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLoadedMenusTest,
	"Hansa.UI.SaveLoad.SuccessClosesAllMenus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLoadedMenusTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UHansaSaveSubsystem> Saves(NewObject<UHansaSaveSubsystem>(Instance.Get()));
	TStrongObjectPtr<UHansaSaveLoadPresentationModel> SaveLoad(NewObject<UHansaSaveLoadPresentationModel>());
	TStrongObjectPtr<UHansaHudPresentationModel> HudModel(NewObject<UHansaHudPresentationModel>());
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Build(NewObject<UHansaBuildMenuPresentationModel>());
	TStrongObjectPtr<UHansaInspectorPresentationModel> Inspector(NewObject<UHansaInspectorPresentationModel>());
	TStrongObjectPtr<UHansaCityOverviewPresentationModel> City(NewObject<UHansaCityOverviewPresentationModel>());
	TStrongObjectPtr<UHansaTradeMapPresentationModel> Trade(NewObject<UHansaTradeMapPresentationModel>());
	TStrongObjectPtr<UHansaResearchPresentationModel> Research(NewObject<UHansaResearchPresentationModel>());
	TStrongObjectPtr<UHansaScenarioPresentationModel> Scenario(NewObject<UHansaScenarioPresentationModel>());
	TStrongObjectPtr<UHansaFrontendPresentationModel> Frontend(NewObject<UHansaFrontendPresentationModel>());
	FString InitError;
	if (!TestTrue(TEXT("Runtime initializes"), Host->InitializeForLubeck(nullptr, InitError))) return false;
	Saves->UseIsolatedAutomationSlots(); Saves->BindRuntime(Host.Get()); SaveLoad->Bind(Saves.Get());
	FText Error, Remedy;
	if (!TestTrue(TEXT("Isolated checkpoint saved"), Saves->Save(EHansaSaveSlotId::Manual, TEXT("Menu closure"), Error, Remedy))) return false;
	HudModel->InitializeDefaults(); Inspector->InitializeDefaults(); City->InitializeDefaults();
	Trade->InitializeDefaults(); Scenario->InitializeDefaults(); Frontend->Initialize(true);
	if (!TestTrue(TEXT("Construction initializes"), Build->InitializeForLubeck(nullptr, Host.Get(), InitError))) return false;
	TSharedRef<Hansa::UI::SHansaRootHud> Hud = SNew(Hansa::UI::SHansaRootHud)
		.Model(HudModel.Get()).BuildModel(Build.Get()).InspectorModel(Inspector.Get())
		.CityOverviewModel(City.Get()).TradeMapModel(Trade.Get()).ResearchModel(Research.Get())
		.ScenarioModel(Scenario.Get()).SaveLoadModel(SaveLoad.Get()).FrontendModel(Frontend.Get());
	Saves->OnLoaded().AddLambda([&]
	{
		Frontend->SessionStarted(); Scenario->SessionRestored(); Hud->CloseAllMenus();
		HudModel->SetSpeed(EHansaHudGameSpeed::Paused);
	});
	const auto OpenMenus = [&]
	{
		City->Open(); Trade->Open(); Research->Open(TEXT("HUD.TopStatus.Research.Open"));
		Inspector->ShowStatus(EHansaInspectorDataState::Loading, FText(), FText(), TEXT("HUD.TopStatus.Session"));
		Build->SetOpen(true); Hud->GetLandState()->ToggleLand();
		Scenario->OpenPause(); Frontend->OpenSettings(); SaveLoad->Open();
	};
	OpenMenus(); SaveLoad->RequestLoad(); SaveLoad->Confirm();
	TestEqual(TEXT("Load succeeds"), SaveLoad->GetSnapshot().Status, EHansaSaveLoadStatus::Success);
	const auto CheckClosed = [&]
	{
		TestFalse(TEXT("Save/load closed"), SaveLoad->GetSnapshot().bOpen);
		TestFalse(TEXT("Pause menu closed"), Scenario->GetSnapshot().bOpen);
		TestFalse(TEXT("City overview closed"), City->GetSnapshot().bOpen);
		TestFalse(TEXT("Trade closed"), Trade->GetSnapshot().bOpen);
		TestFalse(TEXT("Research closed"), Research->GetSnapshot().bOpen);
		TestFalse(TEXT("Inspector closed"), Inspector->GetSnapshot().bOpen);
		TestFalse(TEXT("Construction closed"), Build->GetSnapshot().bOpen);
		TestFalse(TEXT("Fleet closed"), Build->IsShipsOpen());
		TestFalse(TEXT("Land panel closed"), Hud->GetLandState()->IsPanelOpen());
		TestEqual(TEXT("Frontend hidden"), Frontend->GetSnapshot().Page, EHansaFrontendPage::Hidden);
		TestEqual(TEXT("Focus returns to pause control"), HudModel->GetSnapshot().FocusedSemanticId, FName(TEXT("HUD.TopStatus.Speed.Pause")));
		TestEqual(TEXT("Loaded runtime stays paused"), Host->GetSpeed(), EHansaRuntimeSimulationSpeed::Paused);
	};
	CheckClosed();
	OpenMenus();
	TestFalse(TEXT("Unavailable load rejected"), Saves->LoadById(TEXT("autosave"), Error, Remedy));
	TestTrue(TEXT("Rejected load retains save/load"), SaveLoad->GetSnapshot().bOpen);
	TestTrue(TEXT("Rejected load retains trade"), Trade->GetSnapshot().bOpen);
	TestTrue(TEXT("Rejected load retains pause menu"), Scenario->GetSnapshot().bOpen);
	TestTrue(TEXT("Direct Continue load succeeds"), Saves->LoadById(TEXT("manual"), Error, Remedy));
	CheckClosed();
	return !HasAnyErrors();
}
#endif
