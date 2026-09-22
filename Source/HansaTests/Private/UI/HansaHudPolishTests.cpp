#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaContextInspector.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHudPolishFocus,"Hansa.UI.HUD.P23StableAlertFocus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaHudPolishFocus::RunTest(const FString&){
 TStrongObjectPtr<UHansaHudPresentationModel> Model(NewObject<UHansaHudPresentationModel>());Model->InitializeDefaults();
 auto Hud=SNew(Hansa::UI::SHansaRootHud).Model(Model.Get());
 FString Id;
 for(const auto& Node:Hud->GetSemanticSnapshot())if(Node.Id.EndsWith(TEXT(".OpenCause")) && Node.State.bVisible){Id=Node.Id;break;}
 if(!TestFalse(TEXT("An actual visible alert action exists"),Id.IsEmpty()))return false;
 auto Before=Hud->ResolveSemanticWidget(Id);TestTrue(TEXT("Alert action focuses"),Hud->FocusSemanticId(Id));
 auto Changed=Model->GetSnapshot();Changed.Money=FText::FromString(TEXT("2,451 mk"));Model->ApplySnapshot(Changed);
 TestTrue(TEXT("Unrelated resource updates preserve the actual alert button"),Before==Hud->ResolveSemanticWidget(Id));

 for(int32 Tick=1;Tick<=120;++Tick){
  auto Live=Model->GetSnapshot();
  for(auto& Alert:Live.Alerts){Alert.Age=FText::AsNumber(Tick);Alert.Causal.Evidence=FText::Format(FText::FromString(TEXT("Stock {0}")),FText::AsNumber(Tick));}
  Model->ApplySnapshot(Live);
  TestTrue(TEXT("Age and evidence updates retain the native alert action"),Before==Hud->ResolveSemanticWidget(Id));
 }
 auto Live=Model->GetSnapshot();
 for(auto& Alert:Live.Alerts){Alert.Label=FText::FromString(TEXT("Updated shortage"));Alert.Causal.Cause=FText::FromString(TEXT("Updated cause"));Alert.bWarning=true;Alert.Causal.Severity=EHansaCausalSeverity::Critical;}
 Model->ApplySnapshot(Live);
 TestTrue(TEXT("Severity and text changes retain the native action"),Before==Hud->ResolveSemanticWidget(Id));
 TestTrue(TEXT("Causal tooltip updates without replacement"),Before->GetToolTip().IsValid());
 Model->ToggleAlertStack();TestFalse(TEXT("Hidden alert cannot receive focus"),Hud->FocusSemanticId(Id));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaInspectorPolishFocus,"Hansa.UI.Inspector.P23ActionIdentityAndUnavailableStates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaInspectorPolishFocus::RunTest(const FString&){
 TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());Model->InitializeDefaults();
 Model->ShowWorldBuilding(TEXT("Building.Bakery"),FText::FromString(TEXT("Bakery")),FText::FromString(TEXT("Flour to bread")),42,TEXT("Ready"),TEXT("None"),TEXT("HUD.AlertStack.Toggle"));
 auto Inspector=SNew(Hansa::UI::SHansaContextInspector).Model(Model.Get());
 const FString Pin=TEXT("Inspector.Action.Pin");auto Before=Inspector->ResolveSemanticWidget(Pin);
 TestTrue(TEXT("Cause has a real native focus target"),Inspector->FocusSemanticId(TEXT("Inspector.Action.OpenCause")));
 TestTrue(TEXT("Pin has a real native focus target"),Inspector->FocusSemanticId(Pin));Model->TogglePinIntent();
 TestTrue(TEXT("Pin state update preserves widget identity"),Before==Inspector->ResolveSemanticWidget(Pin));
 for(auto State:{EHansaInspectorDataState::Loading,EHansaInspectorDataState::Error,EHansaInspectorDataState::Empty}){
  Model->ShowStatus(State,FText::FromString(TEXT("Selection unavailable")),FText::FromString(TEXT("Select an object in the city.")),TEXT("HUD.AlertStack.Toggle"));
  TestFalse(TEXT("Unavailable selection cannot activate hidden gameplay actions"),Inspector->ActivateSemanticId(Pin));
  TestFalse(TEXT("Removed gameplay action cannot retain focus"),Inspector->FocusSemanticId(Pin));
  TestTrue(TEXT("Unavailable panel still provides Close"),Inspector->FocusSemanticId(TEXT("Inspector.Close")));
 }
 Inspector->ActivateSemanticId(TEXT("Inspector.Close"));TestFalse(TEXT("Closed inspector cannot receive focus"),Inspector->FocusSemanticId(TEXT("Inspector.Close")));
 return !HasAnyErrors();
}
#endif
