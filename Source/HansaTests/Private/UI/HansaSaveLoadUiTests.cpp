#include "Misc/AutomationTest.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/SHansaSaveLoadScreen.h"

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
	TestTrue(TEXT("Save action is keyboard/controller reachable"), FocusOrder.Contains(TEXT("SaveLoad.Action.Save")));
	TestTrue(TEXT("Load action is keyboard/controller reachable"), FocusOrder.Contains(TEXT("SaveLoad.Action.Load")));
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

#endif