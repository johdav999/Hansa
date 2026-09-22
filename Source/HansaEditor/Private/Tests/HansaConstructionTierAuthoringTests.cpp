#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Schema/HansaEditorSchemaRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionTierAuthoringTest,
 "Hansa.Editor.Definitions.ConstructionTierAuthoring",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaConstructionTierAuthoringTest::RunTest(const FString&)
{
 auto* Building = NewObject<UHansaBuildingDefinition>();
 FHansaEditorSchemaRegistry Schemas;
 const auto Schema = Schemas.BuildSchemaForClass(UHansaBuildingDefinition::StaticClass());
 TestTrue(TEXT("Reflected metadata is complete"), Schema.IsValid());
 TestTrue(TEXT("Tier is available to authoring and AI schema"), Schemas.ExportJsonSchema(Schema).Contains(TEXT("ConstructionTier")));
 const uint64 LegacyHash = Building->ComputeDeterministicContentHash();
 Building->ConstructionTier = EHansaConstructionTier::Craftsmen;
 TestNotEqual(TEXT("Explicit tier participates in content hash"), Building->ComputeDeterministicContentHash(), LegacyHash);
 Building->ConstructionTier = EHansaConstructionTier::Legacy;
 TestEqual(TEXT("Restoring legacy preserves historical content hash"), Building->ComputeDeterministicContentHash(), LegacyHash);
 Building->ConstructionTier = static_cast<EHansaConstructionTier>(255);
 TArray<FHansaDefinitionValidationIssue> Issues; Building->ValidateDefinition(Issues);
 TestTrue(TEXT("Invalid tier rejected with actionable diagnostic"), Issues.ContainsByPredicate([](const auto& I) { return I.Code == TEXT("HSA-BUILDING-CONSTRUCTION-TIER"); }));
 return !HasAnyErrors();
}
#endif
