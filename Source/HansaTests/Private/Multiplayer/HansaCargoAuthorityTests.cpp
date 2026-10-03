#include "Misc/AutomationTest.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
using namespace Hansa::Simulation;
using namespace Hansa::Multiplayer;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoAuthorityTest,"Hansa.TradeRoute.Authority.SlotsAndPrivacy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoAuthorityTest::RunTest(const FString&)
{
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!TestTrue(TEXT("Host ready"),Host->InitializeForLubeck(nullptr,Error)))return false;
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);
 FHansaClientInterest Interest;Interest.CityIds={TEXT("City.Lubeck"),TEXT("City.Rostock")};
 if(!Authority.RegisterAdmittedClient({811,FHansaParticipantId::TryCreate(811).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error))return false;
 FHansaClientProjectionSnapshot P;Authority.BuildProjection(811,0,true,P,Error);
 for(const auto& V:P.Vehicles)if(V.OwnerHouseId==int64(Host->GetHouseId().GetValue()))TestEqual(TEXT("Owner sees three physical slots"),V.CargoSlots.Num(),3);else TestTrue(TEXT("Other house slots are private"),V.CargoSlots.IsEmpty());
 const auto* Route=P.Routes.FindByPredicate([&](const auto& R){return R.RouteId==1;});if(!TestNotNull(TEXT("Owned route projected"),Route))return false;
 FHansaClientCommandIntent Edit;Edit.Type=EHansaClientIntentType::EditRoute;Edit.RouteId=Route->RouteId;Edit.RouteStops=Route->Stops;
 for(auto& Stop:Edit.RouteStops)for(auto& Action:Stop.Actions){Action.CargoSlotIndex=0;Action.MinimumSourceReserveMilliUnits=0;}
 auto Submit=[&]{Edit.ClientSequence=Authority.GetExpectedClientSequence(811);Edit.ClientNonce=8000+Edit.ClientSequence;return Authority.SubmitIntent(811,Edit);};
 Edit.RouteStops[0].Actions[0].CargoSlotIndex=3;
 TestFalse(TEXT("Server rejects fourth active slot"),Submit().bAccepted);
 Edit.RouteStops[0].Actions[0].CargoSlotIndex=0;
 const auto Duplicate=Edit.RouteStops[0].Actions[0];Edit.RouteStops[0].Actions.Add(Duplicate);
 TestFalse(TEXT("Server rejects duplicate instruction for same cell"),Submit().bAccepted);
 Edit.RouteStops[0].Actions.Pop();
 auto Opposite=Edit.RouteStops[1].Actions[0];
 Opposite.GoodId=Edit.RouteStops[0].Actions[0].GoodId;
 Opposite.CargoSlotIndex=1;
 Edit.RouteStops[0].Actions.Add(Opposite);
 TestFalse(TEXT("Server rejects load and unload of the same good in a city across slots"),Submit().bAccepted);
 Edit.RouteStops[0].Actions.Pop();
 const auto Accepted=Submit();TestTrue(*(TEXT("Server accepts valid slot plan: ")+Accepted.Message+TEXT(" / ")+Accepted.GatewayError),Accepted.bAccepted);
 FHansaClientProjectionSnapshot After;Authority.BuildProjection(811,0,true,After,Error);
 const auto* Saved=After.Routes.FindByPredicate([&](const auto& R){return R.RouteId==1;});
 TestTrue(TEXT("Authoritative projection preserves slot identity"),Saved&&Saved->Stops[0].Actions[0].CargoSlotIndex==0);
 return !HasAnyErrors();
}
#endif
