#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaConstructionTestSettings.h"
#include "UI/SHansaBuildMenu.h"
#include "InputCoreTypes.h"
#include "UObject/StrongObjectPtr.h"

using namespace Hansa::UI;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionTrayIdentity,"Hansa.UI.BuildMenu.StableCardFocusAndChainEdges",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaConstructionTrayIdentity::RunTest(const FString&)
{
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
 FString Error;if(!TestTrue(TEXT("Catalog initializes"),Model->InitializeForLubeck(nullptr,Error)))return false;
 auto Menu=SNew(SHansaBuildMenu).Model(Model.Get());
 Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production"));
 TestFalse(TEXT("Category opens selectors before any chain"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_GrainFarm")).IsValid());
 Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread"));
 auto Card=Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_GrainFarm"));
 TestTrue(TEXT("Bread exposes two native directed connectors"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Connector.0")).IsValid() && Menu->ResolveSemanticWidget(TEXT("BuildMenu.Connector.1")).IsValid());
 TestTrue(TEXT("Card receives focus"),Menu->FocusSemanticId(TEXT("BuildMenu.Card.Building_GrainFarm")));
 Model->BeginCardDrag(TEXT("Building.GrainFarm"));Model->UpdateCardDragTarget(10,10);Model->UpdateCardDragTarget(11,10);
 TestTrue(TEXT("Pointer updates preserve the actual card widget and drag source"),Card==Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_GrainFarm")));
 Model->CancelIntent();Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Fish"));
 TestFalse(TEXT("Old chain card is removed from semantic resolution"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_GrainFarm")).IsValid());
 TestFalse(TEXT("Cannot focus a removed card"),Menu->FocusSemanticId(TEXT("BuildMenu.Card.Building_GrainFarm")));
 TestFalse(TEXT("Single Fishery card has no stale connector"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Connector.0")).IsValid());
 Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Planks"));
 TestTrue(TEXT("Planks has one connector"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Connector.0")).IsValid());
 TestFalse(TEXT("Planks has no second connector"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Connector.1")).IsValid());
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionTrayCompact,"Hansa.UI.BuildMenu.CompactTrayAndControllerNavigation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaConstructionTrayCompact::RunTest(const FString&)
{
 FScopedHansaArtisanConstructionOverride Override(false);
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
 FString Error;if(!TestTrue(TEXT("Catalog initializes"),Model->InitializeForLubeck(nullptr,Error)))return false;
 Model->SetOpen(false);auto Menu=SNew(SHansaBuildMenu).Model(Model.Get());
 auto Nodes=Menu->GetSemanticSnapshot();
 const auto* Category=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("BuildMenu.Category.Production");});
 TestTrue(TEXT("Categories remain visible while expansion is closed"),Category && Category->State.bVisible);
 TestFalse(TEXT("Closed tray cannot activate hidden cards"),Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Road")));
 TestTrue(TEXT("Category is an accessible open action"),Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production")) && Model->GetSnapshot().bOpen);
 Menu->FocusSemanticId(TEXT("BuildMenu.Category.Production"));
 TestTrue(TEXT("Icon category retains a name tooltip"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Category.Production"))->GetToolTip().IsValid());
 const auto Before=Model->GetSnapshot().FocusedSemanticId;
 Menu->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Gamepad_DPad_Right,FModifierKeysState(),0,false,0,0));
 TestNotEqual(TEXT("Controller D-pad moves actual semantic focus"),Model->GetSnapshot().FocusedSemanticId,Before);
 Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Residences"));
 Menu->ActivateSemanticId(TEXT("BuildMenu.Tier.Craftsmen"));
 auto Locked=Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_Residence_Artisan"));
 TestTrue(TEXT("Upgrade-only card is visibly present but disabled"),Locked.IsValid() && !Locked->IsEnabled());
 TestFalse(TEXT("Locked card rejects activation"),Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Residence_Artisan")));
 Menu->SetPreferences({true,true,true});
 TestFalse(TEXT("Preferences do not unlock a card"),Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_Residence_Artisan"))->IsEnabled());
 return !HasAnyErrors();
}
#endif
