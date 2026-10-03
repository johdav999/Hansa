#include "Misc/AutomationTest.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "UObject/UnrealType.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Definitions/HansaEconomicImpact.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCargoSchemaTest,"Hansa.TradeRoute.Editor.SlotContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCargoSchemaTest::RunTest(const FString&)
{
 auto* Definition=NewObject<UHansaVehicleDefinition>();
 TestEqual(TEXT("Cog authoring default"),Definition->CargoSlotCount,3);
 const auto* Property=FindFProperty<FIntProperty>(UHansaVehicleDefinition::StaticClass(),TEXT("CargoSlotCount"));
 if(!TestNotNull(TEXT("Slot count exposed to editor schema"),Property))return false;
 TestEqual(TEXT("AI can inspect physical slot contract"),Property->GetMetaData(TEXT("HansaAIAccess")),FString(TEXT("Read")));
 TestEqual(TEXT("Slot changes demand migration"),Property->GetMetaData(TEXT("HansaMigration")),FString(TEXT("RequiresMigration")));
 FHansaEditorSchemaRegistry Registry;
 const auto Schema=Registry.BuildSchemaForClass(UHansaVehicleDefinition::StaticClass());
 TestTrue(TEXT("Vehicle generic authoring schema validates"),Schema.IsValid());
 const FString Json=Registry.ExportJsonSchema(Schema);
 TestTrue(TEXT("Physical slot contract exported to automation schema"),Json.Contains(TEXT("CargoSlotCount"))&&Json.Contains(TEXT("RequiresMigration")));
 Definition->StableDefinitionId=TEXT("Vehicle.Cog");
 TArray<const UHansaDefinitionBase*> Definitions{Definition};
 TestTrue(TEXT("Slot authoring impact includes active allocations and routes"),Hansa::Editor::EconomicDefinitions::DescribeEconomicImpact(Definition->StableDefinitionId,Definitions).Contains(TEXT("Vehicle.Cog.CargoSlots/RouteInstructions (persistent slot identity; active saves require migration review)")));
 Definition->CargoSlotCount=4;TArray<FHansaDefinitionValidationIssue> Issues;Definition->ValidateDefinition(Issues);
 TestTrue(TEXT("Unsupported authored counts rejected"),Issues.ContainsByPredicate([](const auto& I){return I.Code==TEXT("HSA-VEHICLE-004");}));
 return !HasAnyErrors();
}
#endif
