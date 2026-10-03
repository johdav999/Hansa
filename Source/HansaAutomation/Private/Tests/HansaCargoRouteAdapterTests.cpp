#include "Gameplay/HansaProductionFixtureService.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoRouteAdapterTest,"Hansa.TradeRoute.Automation.SlotPayload",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoRouteAdapterTest::RunTest(const FString&)
{
 Hansa::Automation::FHansaProductionFixtureService Service;FString Error;auto Output=MakeShared<FJsonObject>();
 if(!TestTrue(TEXT("Load production route fixture"),Service.Load(Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId,Output,Error)))return false;
 auto Request=MakeShared<FJsonObject>();Request->SetStringField(TEXT("command"),TEXT("route.edit"));Request->SetNumberField(TEXT("routeId"),1);
 TArray<TSharedPtr<FJsonValue>> Stops;
 TSharedPtr<FJsonObject> FirstAction;
 for(int32 I=0;I<2;++I){auto Stop=MakeShared<FJsonObject>();Stop->SetStringField(TEXT("cityId"),I?TEXT("City.Rostock"):TEXT("City.Lubeck"));auto Action=MakeShared<FJsonObject>();Action->SetStringField(TEXT("goodId"),TEXT("Good.Grain"));Action->SetNumberField(TEXT("kind"),I?0:1);Action->SetNumberField(TEXT("cargoSlotIndex"),2);Action->SetNumberField(TEXT("quantityMilliUnits"),10000);Action->SetNumberField(TEXT("minimumReserveMilliUnits"),0);Stop->SetArrayField(TEXT("actions"),{MakeShared<FJsonValueObject>(Action)});Stops.Add(MakeShared<FJsonValueObject>(Stop));if(!I)FirstAction=Action;}
 Request->SetArrayField(TEXT("stops"),Stops);
 TestTrue(*Error,Service.Command(Request,Output,Error));
 const auto Projection=Service.GetFixture()->BuildProjection();TestTrue(TEXT("JSON slot preserved in runtime plan"),Projection&&Projection.Value.GetRoutes()[0].Stops[0].Actions[0].CargoSlotIndex==2);
 FirstAction->SetNumberField(TEXT("cargoSlotIndex"),3);TestFalse(TEXT("Automation cannot address a fourth loading slot"),Service.Command(Request,Output,Error));
 TestEqual(TEXT("Rejected request preserves previous cell"),Service.GetFixture()->BuildProjection().Value.GetRoutes()[0].Stops[0].Actions[0].CargoSlotIndex,2);
 return !HasAnyErrors();
}
#endif
