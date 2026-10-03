#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaBuildMenu.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaShipsMenuState, "Hansa.UI.Ships.StateAndStaleSelection",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaShipsMenuState::RunTest(const FString&)
{
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> M(NewObject<UHansaBuildMenuPresentationModel>());
 M->SetShips({{8,FText::FromString(TEXT("Cog #8"))},{2,FText::FromString(TEXT("Cog #2"))}});
 auto Menu=SNew(Hansa::UI::SHansaBuildMenu).Model(M.Get());
 TestTrue(TEXT("Ships is a persistent semantic category"),Menu->ActivateSemanticId(TEXT("BuildMenu.Ships")));
 TestTrue(TEXT("Ships opens without entering placement"),M->IsShipsOpen()&&!M->GetSnapshot().bOpen);
 TestEqual(TEXT("Stable identity ordering"),M->GetShips()[0].Id,int64(2));
 int64 Centered=0;M->ShipIntent=[&](int64 Id,bool){Centered=Id;return true;};
 TestTrue(TEXT("Single selection succeeds"),M->SelectShip(2,false));
 TestEqual(TEXT("Single click does not move camera"),Centered,int64(0));
 TestTrue(TEXT("Semantic activation is accessible centering alternative"),Menu->ActivateSemanticId(TEXT("BuildMenu.Ship.8")));
 TestEqual(TEXT("Selected ship is passed to camera intent"),Centered,int64(8));
 M->SetShips({{2,FText::FromString(TEXT("Cog #2"))}});
 TestFalse(TEXT("Removed ship cannot be centered from a stale slot"),M->SelectShip(8,true));
 TestEqual(TEXT("Removed selection clears"),M->GetSelectedShip(),int64(0));
 M->ShipIntent=[](int64,bool){return false;};
 TestFalse(TEXT("Unavailable world position fails honestly"),M->SelectShip(2,true));
 TestFalse(TEXT("Unavailable position leaves persistent feedback"),M->GetShipFeedback().IsEmpty());
 M->SetConstructionAllowed(false);M->ToggleShips();M->SetOpen(true);
 TestTrue(TEXT("Unavailable construction cannot silently close Ships"),M->IsShipsOpen());
 M->SetShips({});
 const auto Nodes=Menu->GetSemanticSnapshot();
 const auto* Empty=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("BuildMenu.ShipsHint");});
 TestTrue(TEXT("Empty fleet has a visible explanation"),Empty&&Empty->State.bVisible&&Empty->Label.Contains(TEXT("do not own")));
 M->SetOpen(false);
 TestFalse(TEXT("Closing the tray closes ships"),M->IsShipsOpen());
 TestFalse(TEXT("Closed menu rejects stale activation"),M->SelectShip(2,true));
 return !HasAnyErrors();
}
#endif
