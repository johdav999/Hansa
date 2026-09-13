#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
#include "Misc/AutomationTest.h"
#include "Fixtures/HansaProductionFixture.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/SHansaCityOverview.h"
#include "UObject/StrongObjectPtr.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Text/STextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOverviewRemoteKnowledge,"Hansa.UI.CityOverview.P24RemoteKnowledgeAndSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCityOverviewRemoteKnowledge::RunTest(const FString&){
 using namespace Hansa::Simulation;
 auto Created=FHansaProductionFixture::TryCreateGrainShortage();if(!TestTrue(TEXT("Fixture initializes"),Created.IsSuccess()))return false;
 auto Fixture=Created.Value;if(!TestTrue(TEXT("Fixture advances"),Fixture.Step(5).IsSuccess()))return false;auto Projection=Fixture.BuildProjection();if(!Projection)return false;
 const auto* Registry=Fixture.GetDefinitions().GetEconomicRegistry();
 TStrongObjectPtr<UHansaCityOverviewPresentationModel> Model(NewObject<UHansaCityOverviewPresentationModel>());Model->InitializeDefaults();Model->Open();
 const auto Local=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 Model->ApplyProjection(Projection.Value,*Registry,Local,FText::FromString(TEXT("Lübeck")));
 const FName Selected=Model->GetActiveRows()[0].StableId;Model->SelectRowIntent(Selected);
 Model->ApplyProjection(Projection.Value,*Registry,Local,FText::FromString(TEXT("Lübeck")));
 TestEqual(TEXT("Projection refresh preserves stable cohort selection"),Model->GetSnapshot().SelectedRowStableId,Selected);
 Model->ApplyProjection(Projection.Value,*Registry,FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,FText::FromString(TEXT("Rostock")));
 TestTrue(TEXT("Remote population never leaks authoritative civic records"),Model->GetSnapshot().PopulationRows.IsEmpty());
 TestTrue(TEXT("Remote production never leaks authoritative building records"),Model->GetSnapshot().ProductionRows.IsEmpty());
 for(const auto& Summary:Model->GetSnapshot().HeaderSummaries)TestEqual(TEXT("Missing civic data is not zero"),Summary.Value.ToString(),FString(TEXT("Unavailable")));
 Model->SelectTabIntent(EHansaCityOverviewTab::Market);
 for(const auto& Row:Model->GetActiveRows()){
  for(const auto& Field:Row.Fields)TestEqual(TEXT("Without a known report remote values stay unavailable"),Field.Value.ToString(),FString(TEXT("Unavailable")));
  TestFalse(TEXT("No unreported remote building can be activated"),Row.bCausalActionEnabled);
 }
 auto Screen=SNew(Hansa::UI::SHansaCityOverview).Model(Model.Get());
 Model->SetError(FText::FromString(TEXT("Missing report")),FText::FromString(TEXT("Retry")));
 Model->SelectTabIntent(EHansaCityOverviewTab::Population);
 TestEqual(TEXT("Tab selection cannot clear a pending error"),Model->GetSnapshot().LoadState,EHansaCityOverviewLoadState::Error);
 TestFalse(TEXT("Hidden row cannot receive focus"),Screen->FocusSemanticId(TEXT("CityOverview.Row.Market_Good_Bread")));
 Model->CloseIntent();TestFalse(TEXT("Closed screen cannot focus tabs"),Screen->FocusSemanticId(TEXT("CityOverview.Tab.Market")));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOverviewRecoveryReadability,"Hansa.UI.CityOverview.P24RecoveryReadability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCityOverviewRecoveryReadability::RunTest(const FString&){
 using namespace Hansa::UI;
 for(bool Accessible:{false,true}){
  TStrongObjectPtr<UHansaCityOverviewPresentationModel> Model(NewObject<UHansaCityOverviewPresentationModel>());Model->InitializeDefaults();Model->Open();
  FUiPreferences Preferences{Accessible,Accessible,Accessible,.8f};
  auto Screen=SNew(SHansaCityOverview).Model(Model.Get()).Preferences(Preferences);
  for(bool Error:{true,false}){
   if(Error)Model->SetError(FText::FromString(TEXT("Report unavailable")),FText::FromString(TEXT("Try again to request a fresh report.")));
   else Model->SetLoading();
   for(const TCHAR* Id:{TEXT("CityOverview.State.Title"),TEXT("CityOverview.State.Detail")}){
    auto Text=StaticCastSharedPtr<STextBlock>(Screen->ResolveSemanticWidget(Id));
    if(!TestTrue(TEXT("Recovery text is a real mapped native widget"),Text.IsValid()))return false;
    TestTrue(TEXT("Actual recovery text meets body contrast on its current surface"),UHansaUiStyleLibrary::MeetsContrastTarget(Text->GetColorAndOpacity().GetSpecifiedColor(),UHansaUiStyleLibrary::GetColor(Error?EHansaUiColorToken::BalticNavy:EHansaUiColorToken::Linen),EHansaUiContrastTarget::BodyText));
   }
   auto Retry=Screen->ResolveSemanticWidget(TEXT("CityOverview.State.Retry"));
   TestTrue(TEXT("Retry participates in shared target and focus styling"),Retry && Retry->GetType()==TEXT("SHansaAction"));
   TestEqual(TEXT("Retry focus is available only during error recovery"),Screen->GetControllerFocusOrder().Contains(TEXT("CityOverview.State.Retry")),Error);
   if(Error){Retry->SlatePrepass();TestTrue(TEXT("Retry remains a 48 pixel target at 80 percent scale"),Retry->GetDesiredSize().Y*.8f>=47.9f);}
  }
 }
 return !HasAnyErrors();
}

#endif

