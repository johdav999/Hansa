#include "Definitions/HansaEconomicRegistry.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/SHansaResearchScreen.h"
#include "UI/SHansaRootHud.h"

using namespace Hansa::Simulation;

namespace
{
	FKeyEvent KeyEvent(const FKey& Key)
	{
		return FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResearchScreenSemanticFocusTest,
	"Hansa.UI.Research.SemanticsAndControllerFocus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaResearchScreenSemanticFocusTest::RunTest(const FString&)
{
	TArray<FHansaCompiledTechnologyDefinition> Technologies={
		{TEXT("Technology.Commerce.MarketReports"),TEXT("Better market reports"),EHansaResearchBranch::Commerce,{},100,4,TEXT("Reports remain current longer."),{{EHansaResearchEffectKind::MarketReportAgeReductionTicks,TEXT("City.Lubeck"),5}},1},
		{TEXT("Technology.Production.ImprovedMilling"),TEXT("Improved milling"),EHansaResearchBranch::Production,{},100,4,TEXT("Mills work faster."),{{EHansaResearchEffectKind::ProductionThroughputBasisPoints,TEXT("Recipe.MillFlour"),1000}},2},
		{TEXT("Technology.Logistics.WarehouseHandling"),TEXT("Warehouse handling"),EHansaResearchBranch::Logistics,{},100,4,TEXT("Warehouses transfer faster."),{{EHansaResearchEffectKind::WarehouseHandlingBasisPoints,TEXT("Building.Warehouse"),1000}},3}};
	FHansaEconomicRegistry Registry({}, {}, {}, 99, {}, {}, {}, {}, {}, Technologies);
	const auto House=FHansaHouseId::TryCreate(1,1).Value;
	FHansaHouseResearchState State{House,500};
	UHansaResearchPresentationModel* Model=NewObject<UHansaResearchPresentationModel>(); Model->AddToRoot();
	Model->Initialize(Registry,State); Model->Open(TEXT("HUD.TopStatus.Research"));
	FString Requested; Model->SetQueueIntent([&Requested](const FString& Id){Requested=Id;return true;});
	TSharedRef<Hansa::UI::SHansaResearchScreen> Screen=SNew(Hansa::UI::SHansaResearchScreen).Model(Model);
	const auto Semantics=Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Root semantic names bounded chart and single queue"),Semantics.ContainsByPredicate([](const auto& N){return N.Id==TEXT("Research.Root")&&N.State.Value.Contains(TEXT("queue=1"));}));
	TestEqual(TEXT("Three technologies produce three semantic nodes"),Semantics.FilterByPredicate([](const auto& N){return N.Id.StartsWith(TEXT("Research.Node."));}).Num(),3);
	const auto Focus=Screen->GetControllerFocusOrder();
	TestEqual(TEXT("Close is the first controller focus stop"),Focus[0],FString(TEXT("Research.Close")));
	TestEqual(TEXT("Queue action is the final controller focus stop"),Focus.Last(),FString(TEXT("Research.Action.Queue")));
	TestTrue(TEXT("Semantic selection works"),Screen->ActivateSemanticId(TEXT("Research.Node.Technology_Commerce_MarketReports")));
	TestTrue(TEXT("Queue semantic submits authoritative intent"),Screen->ActivateSemanticId(TEXT("Research.Action.Queue")));
	TestEqual(TEXT("Queue intent carries stable technology ID"),Requested,FString(TEXT("Technology.Commerce.MarketReports")));

	UHansaHudPresentationModel* HudModel=NewObject<UHansaHudPresentationModel>(); HudModel->AddToRoot(); HudModel->InitializeDefaults();
	Model->Close();
	TSharedRef<Hansa::UI::SHansaRootHud> Root=SNew(Hansa::UI::SHansaRootHud).Model(HudModel).ResearchModel(Model).InitialViewportSize(FIntPoint(1280,720));
	TestTrue(TEXT("Production HUD exposes a reachable research action"),Root->FocusSemanticId(TEXT("HUD.TopStatus.Research.Open")));
	TestTrue(TEXT("Controller A opens research"),Root->OnKeyDown(FGeometry(),KeyEvent(EKeys::Gamepad_FaceButton_Bottom)).IsEventHandled());
	TestTrue(TEXT("Research opens from the production HUD"),Model->GetSnapshot().bOpen);
	TestEqual(TEXT("Opening establishes the first modal focus stop"),Model->GetSnapshot().FocusedSemanticId,FName(TEXT("Research.Close")));
	TestTrue(TEXT("Controller d-pad reaches a research node"),Root->OnKeyDown(FGeometry(),KeyEvent(EKeys::Gamepad_DPad_Down)).IsEventHandled());
	TestEqual(TEXT("Controller focuses the first technology"),Model->GetSnapshot().FocusedSemanticId,FName(TEXT("Research.Node.Technology_Commerce_MarketReports")));
	TestTrue(TEXT("Controller B closes research"),Root->OnKeyDown(FGeometry(),KeyEvent(EKeys::Gamepad_FaceButton_Right)).IsEventHandled());
	TestFalse(TEXT("Research is closed after controller Back"),Model->GetSnapshot().bOpen);
	TestEqual(TEXT("Closing restores focus to the production HUD opener"),HudModel->GetSnapshot().FocusedSemanticId,FName(TEXT("HUD.TopStatus.Research.Open")));
	HudModel->RemoveFromRoot();
	Model->RemoveFromRoot();
	return true;
}
