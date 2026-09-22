#include "Compounds/HansaCompoundAuthoring.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaResearchDefinitions.h"
#include "Definitions/HansaScenarioDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Definitions/HansaEconomicImpact.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaLubeckScenarioInitializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Editor::Tests
{
	struct FReviewedEconomicCatalog final
	{
		int32 CatalogVersion = 0;
		uint64 RegistryHash = 0;
		TArray<FHansaEconomicDefinitionHashEvidence> Definitions;
	};

	bool ParseHash64(const FString& Text, uint64& OutHash)
	{
		if (Text.Len() != 16)
		{
			return false;
		}
		for (const TCHAR Character : Text)
		{
			if (!FChar::IsHexDigit(Character)) return false;
		}
		OutHash = FCString::Strtoui64(*Text, nullptr, 16);
		return true;
	}

	bool LoadReviewedEconomicCatalog(FReviewedEconomicCatalog& OutCatalog, FString& OutError,
		const TCHAR* ManifestName = FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 35 ? TEXT("economic_catalog_v35.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 34 ? TEXT("economic_catalog_v34.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 33 ? TEXT("economic_catalog_v33.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 32 ? TEXT("economic_catalog_v32.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 31 ? TEXT("economic_catalog_v31.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 30 ? TEXT("economic_catalog_v30.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 29 ? TEXT("economic_catalog_v29.json") : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 28 ? TEXT("economic_catalog_v28.json") : TEXT("economic_catalog_v27.json"))
	{
		const FString ManifestPath = FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Tests"), TEXT("Golden"), ManifestName);
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
		{
			OutError = FString::Printf(TEXT("Unable to read reviewed catalog manifest %s"), *ManifestPath);
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("Unable to parse reviewed catalog manifest %s"), *ManifestPath);
			return false;
		}
		double CatalogVersion = 0;
		FString RegistryHash;
		const TArray<TSharedPtr<FJsonValue>>* Definitions = nullptr;
		if (!Root->TryGetNumberField(TEXT("catalogVersion"), CatalogVersion) ||
			!Root->TryGetStringField(TEXT("registryHash"), RegistryHash) ||
			!ParseHash64(RegistryHash, OutCatalog.RegistryHash) ||
			!Root->TryGetArrayField(TEXT("definitions"), Definitions) || Definitions == nullptr)
		{
			OutError = TEXT("Reviewed catalog manifest is missing a valid catalogVersion, registryHash, or definitions array");
			return false;
		}
		OutCatalog.CatalogVersion = static_cast<int32>(CatalogVersion);
		for (const TSharedPtr<FJsonValue>& Value : *Definitions)
		{
			const TSharedPtr<FJsonObject> Definition = Value->AsObject();
			FHansaEconomicDefinitionHashEvidence Evidence;
			FString ContentHash;
			if (!Definition.IsValid() ||
				!Definition->TryGetStringField(TEXT("classPath"), Evidence.DefinitionClassPath) ||
				!Definition->TryGetStringField(TEXT("stableId"), Evidence.StableId) ||
				!Definition->TryGetStringField(TEXT("contentHash"), ContentHash) ||
				!ParseHash64(ContentHash, Evidence.ContentHash))
			{
				OutError = TEXT("Reviewed catalog manifest contains an invalid definition fingerprint");
				return false;
			}
			OutCatalog.Definitions.Add(MoveTemp(Evidence));
		}
		return true;
	}

	bool SameDefinitionHashes(
		const TArray<FHansaEconomicDefinitionHashEvidence>& Left,
		const TArray<FHansaEconomicDefinitionHashEvidence>& Right)
	{
		if (Left.Num() != Right.Num()) return false;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].DefinitionClassPath != Right[Index].DefinitionClassPath ||
				Left[Index].StableId != Right[Index].StableId ||
				Left[Index].ContentHash != Right[Index].ContentHash)
			{
				return false;
			}
		}
		return true;
	}

	TArray<const UHansaDefinitionBase*> RawDefinitions(
		const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<const UHansaDefinitionBase*> Result;
		Result.Reserve(Definitions.Num());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions)
		{
			Result.Add(Definition.Get());
		}
		return Result;
	}

	UHansaDefinitionBase* FindDefinition(
		const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions,
		const FString& StableId)
	{
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions)
		{
			if (Definition->StableDefinitionId == StableId)
			{
				return Definition.Get();
			}
		}
		return nullptr;
	}

	bool ContainsIssueCode(const FHansaEconomicRegistryCompileResult& Result, const FString& Code)
	{
		return Result.Issues.ContainsByPredicate([&Code](const FHansaDefinitionValidationIssue& Issue)
		{
			return Issue.Code.ToString() == Code;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEconomicSchemaCoverageTest,
	"Hansa.Architecture.Authoring.EconomicSchemaCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEconomicSchemaCoverageTest::RunTest(const FString& Parameters)
{
	FHansaEditorSchemaRegistry Registry;
	Registry.Refresh();
	for (const UClass* DefinitionClass : {
		UHansaGoodDefinition::StaticClass(),
		UHansaRecipeDefinition::StaticClass(),
		UHansaBuildingDefinition::StaticClass(),
		UHansaNeedDefinition::StaticClass(),
		UHansaPopulationTierDefinition::StaticClass(),
		UHansaCityMarketProfileDefinition::StaticClass(),
		UHansaVehicleDefinition::StaticClass(),
		UHansaRouteDefinition::StaticClass(),
		UHansaTechnologyDefinition::StaticClass(),
		UHansaMerchantAITuningDefinition::StaticClass(),
		UHansaScenarioObjectiveDefinition::StaticClass(),
		UHansaVictoryDefinition::StaticClass(),
		UHansaScenarioDefinition::StaticClass() })
	{
		const FHansaDefinitionClassSchema* Schema = Registry.FindSchema(DefinitionClass);
		TestNotNull(*FString::Printf(TEXT("%s is discovered by the generic schema registry"), *DefinitionClass->GetName()), Schema);
		if (Schema != nullptr)
		{
			TestTrue(*FString::Printf(TEXT("%s metadata is complete"), *DefinitionClass->GetName()), Schema->IsValid());
			TestTrue(TEXT("Every economic schema exports reflected properties"), !Schema->Properties.IsEmpty());
			const FHansaEditorSchemaProperty* MeshProperty = Schema->Properties.FindByPredicate(
				[](const FHansaEditorSchemaProperty& Property) { return Property.Name == TEXT("PresentationMesh"); });
			TestNotNull(TEXT("Every definition exposes the common presentation mesh field"), MeshProperty);
			if (MeshProperty != nullptr)
			{
				TestEqual(TEXT("The editor provides a typed static mesh reference"), MeshProperty->ReferenceType, FString(TEXT("StaticMesh")));
				TestEqual(TEXT("Mesh assignment remains human controlled"), MeshProperty->AIAccess, FString(TEXT("Never")));
				TestEqual(TEXT("Inherited mesh assignment is a compatible migration"), MeshProperty->Migration, FString(TEXT("Compatible")));
			}

		}
	}
	const auto* VehicleSchema = Registry.FindSchema(UHansaVehicleDefinition::StaticClass());
	const auto* VehicleActor = VehicleSchema ? VehicleSchema->Properties.FindByPredicate(
		[](const FHansaEditorSchemaProperty& Property) { return Property.Name == TEXT("PresentationActorClass"); }) : nullptr;
	if (TestNotNull(TEXT("Vehicle presentation schema exported"), VehicleActor))
	{
		TestEqual(TEXT("Vehicle actor reference"), VehicleActor->ReferenceType, FString(TEXT("ActorClass")));
		TestEqual(TEXT("Vehicle optional extension compatible"), VehicleActor->Migration, FString(TEXT("Compatible")));
		TestEqual(TEXT("Vehicle executable class approval cannot be generated"), VehicleActor->AIAccess, FString(TEXT("Never")));
	}
	const FHansaDefinitionClassSchema* BuildingSchema = Registry.FindSchema(UHansaBuildingDefinition::StaticClass());
	const FHansaEditorSchemaProperty* ActorProperty = BuildingSchema != nullptr ? BuildingSchema->Properties.FindByPredicate(
		[](const FHansaEditorSchemaProperty& Property) { return Property.Name == TEXT("PresentationActorClass"); }) : nullptr;
	if (TestNotNull(TEXT("Building actor reference is exposed to authoring and schema export"), ActorProperty))
	{
		TestEqual(TEXT("Actor picker uses class references"), ActorProperty->ReferenceType, FString(TEXT("ActorClass")));
		TestEqual(TEXT("Actor migration preserves existing mesh content"), ActorProperty->Migration, FString(TEXT("Compatible")));
		TestEqual(TEXT("AI cannot substitute executable Blueprint classes"), ActorProperty->AIAccess, FString(TEXT("Never")));
	}
	const FHansaEditorSchemaProperty* MarketAccessProperty = BuildingSchema != nullptr ? BuildingSchema->Properties.FindByPredicate(
		[](const FHansaEditorSchemaProperty& Property) { return Property.Name == TEXT("bProvidesMarketAccess"); }) : nullptr;
	if (TestNotNull(TEXT("Physical market-access capability is exported"), MarketAccessProperty))
	{
		TestEqual(TEXT("Market-access capability has a player-facing tooltip"),
			MarketAccessProperty->Description,
			FString(TEXT("Marks this completed building as a physical local-market hub. Its bound city inventory and adjacent connected roads become eligible endpoints for local deliveries and citizen access.")));
		TestEqual(TEXT("Market-access capability participates in strict AI patches"),
			MarketAccessProperty->AIAccess, FString(TEXT("Generate")));
		TestEqual(TEXT("Market-access capability requires an explicit content migration"),
			MarketAccessProperty->Migration, FString(TEXT("RequiresMigration")));
		TestEqual(TEXT("Building schema includes the production-road invariant"), BuildingSchema->SchemaVersion, 5);
	}
	const FHansaEditorSchemaProperty* RequiresRoadProperty = BuildingSchema != nullptr ? BuildingSchema->Properties.FindByPredicate(
		[](const FHansaEditorSchemaProperty& Property) { return Property.Name == TEXT("bRequiresRoad"); }) : nullptr;
	if (TestNotNull(TEXT("Road operating eligibility is exported"), RequiresRoadProperty))
	{
		TestEqual(TEXT("Road eligibility changes require explicit content migration"),
			RequiresRoadProperty->Migration, FString(TEXT("RequiresMigration")));
	}
	for (const TCHAR* PropertyName : {
		TEXT("bShowInConstructionMenu"), TEXT("ConstructionMenuCategory"), TEXT("ConstructionMenuOrder"),
		TEXT("ConstructionChainOutputGoodId"), TEXT("ConstructionChainStage"), TEXT("ConstructionChainStageCount"),
		TEXT("RequiredConstructionTechnologyId"), TEXT("bUpgradeOnly"), TEXT("ConstructionPresentationPurpose") })
	{
		const FHansaEditorSchemaProperty* Property = BuildingSchema != nullptr ? BuildingSchema->Properties.FindByPredicate(
			[PropertyName](const FHansaEditorSchemaProperty& Candidate) { return Candidate.Name == PropertyName; }) : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("Construction catalog property %s is exported"), PropertyName), Property))
		{
			TestEqual(*FString::Printf(TEXT("Construction catalog property %s is serialized"), PropertyName),
				Property->Serialization, FString(TEXT("Included")));
			TestFalse(*FString::Printf(TEXT("Construction catalog property %s declares migration handling"), PropertyName),
				Property->Migration.IsEmpty());
		}
	}
	TArray<FString> ExportedSchemas;
	FString ExportError;
	TestTrue(TEXT("Updated schemas export for every definition"), Registry.ExportAllJsonSchemas(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence/Presentation/Schemas")), ExportedSchemas, ExportError));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEconomicRegistryTest,
	"Hansa.Content.Definitions.EconomicRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEconomicRegistryTest::RunTest(const FString& Parameters)
{
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	TArray<const UHansaDefinitionBase*> Forward = Hansa::Editor::Tests::RawDefinitions(Definitions);
	const FHansaEconomicRegistryCompileResult First = FHansaEconomicDefinitionCompiler::Compile(Forward);
	TestTrue(TEXT("Reviewed MVP definition set compiles"), First.IsValid());
	TestEqual(TEXT("Expanded MVP goods count"), First.Registry.GetGoods().Num(), 15);
	TestEqual(TEXT("Expanded MVP recipes count"), First.Registry.GetRecipes().Num(), 13);
	TestEqual(TEXT("Expanded MVP buildings count"), First.Registry.GetBuildings().Num(), 19);
	TestEqual(TEXT("MVP needs count"), First.Registry.GetNeeds().Num(), 6);
	TestEqual(TEXT("MVP population tiers count"), First.Registry.GetPopulationTiers().Num(), 2);
	TestEqual(TEXT("MVP city market profiles count"), First.Registry.GetCityMarkets().Num(), 4);
	TestEqual(TEXT("MVP vehicle definitions count"), First.Registry.GetVehicles().Num(), 2);
	TestEqual(TEXT("MVP route definitions count"), First.Registry.GetRoutes().Num(), 2);
	TestEqual(TEXT("MVP merchant AI tuning count"), First.Registry.GetMerchantAITunings().Num(), 1);
	TestEqual(TEXT("MVP scenario objective count"), First.Registry.GetScenarioObjectives().Num(), 11);
	TestEqual(TEXT("MVP victory path count"), First.Registry.GetVictories().Num(), 3);
	TestEqual(TEXT("MVP scenario count"), First.Registry.GetScenarios().Num(), 1);
	TestNotNull(TEXT("Lübeck shortage scenario is queryable by stable ID"), First.Registry.FindScenario(TEXT("Scenario.LubeckGrainShortageV1")));
	TestNotNull(TEXT("Vehicle.Cog is queryable by stable ID"), First.Registry.FindVehicle(TEXT("Vehicle.Cog")));
	TestNotNull(TEXT("Route.SaltRoad is queryable by stable ID"), First.Registry.FindRoute(TEXT("Route.SaltRoad")));
	TestTrue(TEXT("Registry hash is non-zero"), First.Registry.GetRegistryHash() != 0);
	const auto Quantity = [](const TArray<FHansaGoodAmount>& Amounts, const TCHAR* GoodId)
	{
		const FHansaGoodAmount* Found = Amounts.FindByPredicate(
			[GoodId](const FHansaGoodAmount& Amount) { return Amount.GoodId == GoodId; });
		return Found != nullptr ? Found->QuantityMilliUnits : int64(-1);
	};
	const auto* SeededFarmRecipe = Cast<UHansaRecipeDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.GrowGrain")));
	const auto* SeededMillRecipe = Cast<UHansaRecipeDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.MillFlour")));
	const auto* SeededBakeryRecipe = Cast<UHansaRecipeDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.BakeBread")));
	const auto* SeededFisheryRecipe = Cast<UHansaRecipeDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.CatchFish")));
	if (TestNotNull(TEXT("Fivefold farm recipe exists"), SeededFarmRecipe) &&
		TestNotNull(TEXT("Fivefold mill recipe exists"), SeededMillRecipe) &&
		TestNotNull(TEXT("Fivefold bakery recipe exists"), SeededBakeryRecipe) &&
		TestNotNull(TEXT("Fivefold fishery recipe exists"), SeededFisheryRecipe))
	{
		TestEqual(TEXT("Farm batch is fivefold"), Quantity(SeededFarmRecipe->Outputs, TEXT("Good.Grain")), int64(30'000));
		TestEqual(TEXT("Mill input is fivefold"), Quantity(SeededMillRecipe->Inputs, TEXT("Good.Grain")), int64(20'000));
		TestEqual(TEXT("Mill output is fivefold"), Quantity(SeededMillRecipe->Outputs, TEXT("Good.Flour")), int64(15'000));
		TestEqual(TEXT("Bakery input is fivefold"), Quantity(SeededBakeryRecipe->Inputs, TEXT("Good.Flour")), int64(10'000));
		TestEqual(TEXT("Bakery output is fivefold"), Quantity(SeededBakeryRecipe->Outputs, TEXT("Good.Bread")), int64(15'000));
		TestEqual(TEXT("Fishery batch is fivefold"), Quantity(SeededFisheryRecipe->Outputs, TEXT("Good.Fish")), int64(20'000));
		TestTrue(TEXT("All staple recipes use two laborers"),
			SeededFarmRecipe->LaborerWorkforce == 2 && SeededMillRecipe->LaborerWorkforce == 2 &&
			SeededBakeryRecipe->LaborerWorkforce == 2 && SeededFisheryRecipe->LaborerWorkforce == 2);
	}
    const auto* SeededArtisan = Cast<UHansaBuildingDefinition>(Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Residence.Artisan")));
    if (TestNotNull(TEXT("Artisan seed exists"), SeededArtisan))
    {
        TestEqual(TEXT("Artisan seed revision matches saved asset"), SeededArtisan->AuthoredRevision, 3);
        TestEqual(TEXT("Artisan seed mesh matches production"), SeededArtisan->PresentationMesh.ToSoftObjectPath().ToString(), FString(TEXT("/Game/Mesh/hansa-artisan-houses/Meshes/SM_ArtisanHouse_A.SM_ArtisanHouse_A")));
        TestTrue(TEXT("Artisan seed actor matches production"), SeededArtisan->PresentationActorClass.ToSoftObjectPath().ToString().StartsWith(TEXT("/Game/Mesh/hansa-artisan-houses/")));
    }
	const UHansaBuildingDefinition* SeededBakery = Cast<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Bakery")));
	const UHansaBuildingDefinition* SeededMill = Cast<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Mill")));
	const UHansaBuildingDefinition* SeededFarm = Cast<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.GrainFarm")));
	const UHansaBuildingDefinition* SeededFishery = Cast<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Fishery")));
	const UHansaBuildingDefinition* SeededBrewery = Cast<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Brewery")));
	if (TestNotNull(TEXT("Seed builder contains the approved Bakery revision"), SeededBakery))
	{
		TestEqual(TEXT("Bakery seed carries its accepted authored revision"), SeededBakery->AuthoredRevision, 5);
	}
	if (TestNotNull(TEXT("Seed builder contains the approved Mill revision"), SeededMill))
	{
		TestEqual(TEXT("Mill seed carries its accepted authored revision"), SeededMill->AuthoredRevision, 4);
	}
	if (TestNotNull(TEXT("Seed builder contains the fivefold staple building set"), SeededFarm) &&
		TestNotNull(TEXT("Seed builder contains the fivefold Fishery"), SeededFishery) &&
		SeededMill != nullptr && SeededBakery != nullptr)
	{
		TestTrue(TEXT("All staple buildings use two laborers"),
			SeededFarm->LaborerWorkforce == 2 && SeededMill->LaborerWorkforce == 2 &&
			SeededBakery->LaborerWorkforce == 2 && SeededFishery->LaborerWorkforce == 2);
		TestTrue(TEXT("Staple capital costs are retuned"),
			SeededFarm->ConstructionCostPfennig == 3'000 && SeededMill->ConstructionCostPfennig == 4'000 &&
			SeededBakery->ConstructionCostPfennig == 3'500 && SeededFishery->ConstructionCostPfennig == 3'750);
		TestTrue(TEXT("Staple material costs are doubled"),
			Quantity(SeededFarm->ConstructionCosts, TEXT("Good.Timber")) == 6'000 &&
			Quantity(SeededMill->ConstructionCosts, TEXT("Good.Planks")) == 6'000 &&
			Quantity(SeededBakery->ConstructionCosts, TEXT("Good.Planks")) == 8'000 &&
			Quantity(SeededFishery->ConstructionCosts, TEXT("Good.Timber")) == 10'000);
	}
	if (TestNotNull(TEXT("Seed builder contains the player-buildable Brewery revision"), SeededBrewery))
	{
		TestTrue(TEXT("Brewery seed is visible in the Beer production chain"),
			SeededBrewery->AuthoredRevision == 4 && SeededBrewery->bShowInConstructionMenu &&
			SeededBrewery->ConstructionMenuCategory == EHansaConstructionMenuCategory::Production &&
			SeededBrewery->ConstructionChainOutputGoodId == TEXT("Good.Beer") &&
			SeededBrewery->ConstructionChainStage == 4 && SeededBrewery->ConstructionChainStageCount == 4);
	}
	TestNotNull(TEXT("Good.Grain is queryable by stable ID"), First.Registry.FindGood(TEXT("Good.Grain")));
	TestNotNull(TEXT("Good.Hops is queryable by stable ID"), First.Registry.FindGood(TEXT("Good.Hops")));
	TestNotNull(TEXT("Good.Malt is queryable by stable ID"), First.Registry.FindGood(TEXT("Good.Malt")));
	TestNotNull(TEXT("Good.Barrels is queryable by stable ID"), First.Registry.FindGood(TEXT("Good.Barrels")));
	const auto* BrewBeer = First.Registry.FindRecipe(TEXT("Recipe.BrewBeer"));
	if (TestNotNull(TEXT("Recipe.BrewBeer is queryable by stable ID"), BrewBeer))
	{
		TestEqual(TEXT("Brewery consumes malt, hops, barrels and fuel"), BrewBeer->Inputs.Num(), 4);
	}
	TestNotNull(TEXT("Recipe.GrowHops is queryable by stable ID"), First.Registry.FindRecipe(TEXT("Recipe.GrowHops")));
	TestNotNull(TEXT("Recipe.MaltGrain is queryable by stable ID"), First.Registry.FindRecipe(TEXT("Recipe.MaltGrain")));
	TestNotNull(TEXT("Recipe.MakeBarrels is queryable by stable ID"), First.Registry.FindRecipe(TEXT("Recipe.MakeBarrels")));
	TestNotNull(TEXT("Building.HopFarm is queryable by stable ID"), First.Registry.FindBuilding(TEXT("Building.HopFarm")));
	TestNotNull(TEXT("Building.MaltHouse is queryable by stable ID"), First.Registry.FindBuilding(TEXT("Building.MaltHouse")));
	TestNotNull(TEXT("Building.Cooperage is queryable by stable ID"), First.Registry.FindBuilding(TEXT("Building.Cooperage")));
	TestNotNull(TEXT("Building.Brewery is queryable by stable ID"), First.Registry.FindBuilding(TEXT("Building.Brewery")));
	for (const Hansa::Simulation::FHansaCompiledBuildingDefinition& Building : First.Registry.GetBuildings())
	{
		TestTrue(*FString::Printf(TEXT("%s has an authored positive currency cost"), *Building.StableId),
			Building.ConstructionCostPfennig > 0);
		TestTrue(*FString::Printf(TEXT("%s has a bounded cancellation refund"), *Building.StableId),
			Building.CancellationRefundBasisPoints >= 0 && Building.CancellationRefundBasisPoints <= 10000);
		TestTrue(*FString::Printf(TEXT("%s has a deterministic positive build time"), *Building.StableId),
			Building.BuildTicks > 0);
	}
	TestNotNull(TEXT("Need.Bread is queryable by stable ID"), First.Registry.FindNeed(TEXT("Need.Bread")));
	TestNotNull(TEXT("PopulationTier.Artisan is queryable by stable ID"), First.Registry.FindPopulationTier(TEXT("PopulationTier.Artisan")));
	TestNotNull(TEXT("City.Lubeck market is queryable by stable ID"), First.Registry.FindCityMarket(TEXT("City.Lubeck")));
	int32 MarketOnlyCityCount = 0;
	for (const Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition& City : First.Registry.GetCityMarkets())
	{
		TestEqual(*FString::Printf(TEXT("%s covers all fifteen expanded MVP goods"), *City.StableId), City.Goods.Num(), 15);
		if (!City.bMarketOnly) continue;
		++MarketOnlyCityCount;
		TestTrue(*FString::Printf(TEXT("%s has a delayed deterministic report cadence"), *City.StableId),
			City.ReportCadenceTicks > City.UpdateCadenceTicks);
		TestTrue(*FString::Printf(TEXT("%s has ordered report-age classifications"), *City.StableId),
			City.CurrentReportMaxAgeTicks <= City.RecentReportMaxAgeTicks &&
			City.RecentReportMaxAgeTicks <= City.StaleReportMaxAgeTicks &&
			City.StaleReportMaxAgeTicks <= City.EstimatedReportMaxAgeTicks);
		for (const Hansa::Simulation::FHansaCompiledMarketGoodProfile& Good : City.Goods)
		{
			TestTrue(*FString::Printf(TEXT("%s %s has explicit simulated stock or background flow"),
				*City.StableId, *Good.GoodId), Good.InitialStockMilliUnits > 0 ||
				Good.BackgroundProductionMilliUnitsPerUpdate > 0 ||
				Good.BackgroundCitizenDemandMilliUnitsPerUpdate > 0 ||
				Good.BackgroundIndustrialDemandMilliUnitsPerUpdate > 0);
		}
	}
	TestEqual(TEXT("Hamburg, Lüneburg and Rostock are market-only cities"), MarketOnlyCityCount, 3);

	Algo::Reverse(Forward);
	const FHansaEconomicRegistryCompileResult Reversed = FHansaEconomicDefinitionCompiler::Compile(Forward);
	TestTrue(TEXT("Reordered source definitions compile"), Reversed.IsValid());
	TestEqual(TEXT("Registry hash is independent of asset discovery order"), Reversed.Registry.GetRegistryHash(), First.Registry.GetRegistryHash());
	TestTrue(TEXT("Per-definition evidence is independent of asset discovery order"),
		Hansa::Editor::Tests::SameDefinitionHashes(Reversed.DefinitionHashes, First.DefinitionHashes));

	UHansaGoodDefinition* Grain = Cast<UHansaGoodDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Good.Grain")));
	if (TestNotNull(TEXT("Seed builder contains Good.Grain"), Grain))
	{
		const uint64 StoredHash = Grain->ContentHash;
		Grain->BaseValueMilliMarks += 1;
		const FHansaEconomicRegistryCompileResult Temporary =
			FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestTrue(TEXT("Temporary edited registry compiles"), Temporary.IsValid());
		TestEqual(TEXT("Temporary compile never mutates saved/derived asset state"), Grain->ContentHash, StoredHash);
		TestNotEqual(TEXT("Result-affecting edit changes registry hash"), Temporary.Registry.GetRegistryHash(), First.Registry.GetRegistryHash());
		const FString MismatchReport = Temporary.DescribeRegistryHashMismatch(
			First.Registry.GetRegistryHash(), First.DefinitionHashes);
		TestTrue(TEXT("A future registry mismatch names its changed definition"),
			MismatchReport.Contains(TEXT("changed Good.Grain")));
		const FHansaEconomicDefinitionHashEvidence* GrainEvidence = First.DefinitionHashes.FindByPredicate(
			[](const FHansaEconomicDefinitionHashEvidence& Evidence)
			{
				return Evidence.StableId == TEXT("Good.Grain");
			});
		TestTrue(TEXT("A future registry mismatch includes expected and actual definition hashes"),
			GrainEvidence != nullptr && MismatchReport.Contains(FString::Printf(
				TEXT("expected %016llX, actual"),
				static_cast<unsigned long long>(GrainEvidence->ContentHash))));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCityMarketDefinitionValidationTest,
	"Hansa.Content.Definitions.CityMarketSimulationAndReportingValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityMarketDefinitionValidationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Tests;
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		auto* Rostock = CastChecked<UHansaCityMarketProfileDefinition>(FindDefinition(Definitions, TEXT("City.Rostock")));
		Rostock->RecentReportMaxAgeTicks = Rostock->StaleReportMaxAgeTicks + 1;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(Definitions));
		TestFalse(TEXT("Misordered market information ages fail validation"), Result.IsValid());
		TestTrue(TEXT("Report-policy validation uses a stable actionable code"),
			ContainsIssueCode(Result, TEXT("HSA-MARKET-002")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		auto* Hamburg = CastChecked<UHansaCityMarketProfileDefinition>(FindDefinition(Definitions, TEXT("City.Hamburg")));
		FHansaMarketGoodProfile& Good = Hamburg->Goods[0];
		Good.InitialStockMilliUnits = 0;
		Good.BackgroundProductionMilliUnitsPerUpdate = 0;
		Good.BackgroundCitizenDemandMilliUnitsPerUpdate = 0;
		Good.BackgroundIndustrialDemandMilliUnitsPerUpdate = 0;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(Definitions));
		TestFalse(TEXT("Empty market-only activity fails validation"), Result.IsValid());
		TestTrue(TEXT("Market-only activity validation uses a stable actionable code"),
			ContainsIssueCode(Result, TEXT("HSA-MARKET-005")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBuildingConstructionPolicyValidationTest,
	"Hansa.Content.Definitions.BuildingConstructionPolicyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildingConstructionPolicyValidationTest::RunTest(const FString& Parameters)
{
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Warehouse")));
	if (!TestNotNull(TEXT("Warehouse definition exists"), Building))
	{
		return false;
	}
	Building->ConstructionCostPfennig = -1;
	Building->CancellationRefundBasisPoints = 10001;
	const FHansaEconomicRegistryCompileResult Result =
		FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
	TestFalse(TEXT("Negative currency or over-refund policy fails compilation"), Result.IsValid());
	TestTrue(TEXT("Construction policy validation is actionable"),
		Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-BUILDING-007")) ||
		Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-018")));
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> InvalidMarketDefinitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	UHansaBuildingDefinition* InvalidMarket = CastChecked<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(InvalidMarketDefinitions, TEXT("Building.Market")));
	InvalidMarket->bRequiresRoad = false;
	const FHansaEconomicRegistryCompileResult InvalidMarketResult =
		FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(InvalidMarketDefinitions));
	TestFalse(TEXT("A market-access provider without a road contract fails compilation"), InvalidMarketResult.IsValid());
	TestTrue(TEXT("Market-provider validation has a stable cause and remedy code"),
		Hansa::Editor::Tests::ContainsIssueCode(InvalidMarketResult, TEXT("HSA-BUILDING-013")));
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> InvalidProductionDefinitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	UHansaBuildingDefinition* InvalidFishery = CastChecked<UHansaBuildingDefinition>(
		Hansa::Editor::Tests::FindDefinition(InvalidProductionDefinitions, TEXT("Building.Fishery")));
	InvalidFishery->bRequiresRoad = false;
	const FHansaEconomicRegistryCompileResult InvalidProductionResult =
		FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(InvalidProductionDefinitions));
	TestFalse(TEXT("A production building without road operating eligibility fails compilation"), InvalidProductionResult.IsValid());
	TestTrue(TEXT("Production-road validation has a stable cause and remedy code"),
		Hansa::Editor::Tests::ContainsIssueCode(InvalidProductionResult, TEXT("HSA-BUILDING-014")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaConstructionCatalogDefinitionTest,
	"Hansa.Content.Definitions.ConstructionCatalogModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaConstructionCatalogDefinitionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Tests;
	using namespace Hansa::Simulation;
	(void)Parameters;
	auto FindCard = [](const TArray<FHansaBuildCardPresentation>& Cards, const TCHAR* StableId)
	{
		return Cards.FindByPredicate(
			[StableId](const FHansaBuildCardPresentation& Card) { return Card.StableId == StableId; });
	};

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	FHansaEconomicRegistryCompileResult Compiled = FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(Definitions));
	if (!TestTrue(TEXT("The authored construction catalog compiles"), Compiled.IsValid())) return false;
	TArray<FHansaBuildCardPresentation> Cards;
	TArray<FHansaBuildChainPresentation> Chains;
	FString Error;
	TestTrue(TEXT("The presentation catalog builds solely from compiled definitions"),
		UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(Compiled.Registry, {}, Cards, Chains, Error));
	TArray<FName> BreadOrder;
	for (const FHansaBuildCardPresentation& Card : Cards)
	{
		if (Card.ProductionChainOutputGoodId == TEXT("Good.Bread")) BreadOrder.Add(Card.StableId);
	}
	TestEqual(TEXT("Bread chain contains three authored cards"), BreadOrder.Num(), 3);
	if (BreadOrder.Num() == 3)
	{
		TestEqual(TEXT("Bread stage 1 is Grain Farm"), BreadOrder[0], FName(TEXT("Building.GrainFarm")));
		TestEqual(TEXT("Bread stage 2 is Mill"), BreadOrder[1], FName(TEXT("Building.Mill")));
		TestEqual(TEXT("Bread stage 3 is Bakery"), BreadOrder[2], FName(TEXT("Building.Bakery")));
	}

	UHansaBuildingDefinition* GrainFarm = CastChecked<UHansaBuildingDefinition>(
		FindDefinition(Definitions, TEXT("Building.GrainFarm")));
	GrainFarm->DisplayName = FText::FromString(TEXT("Revised grain farm"));
	GrainFarm->ConstructionCostPfennig += 111;
	GrainFarm->ConstructionMenuOrder = 4;
	Compiled = FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(Definitions));
	TestTrue(TEXT("A hot-reloaded authored building revision compiles without a Slate edit"), Compiled.IsValid());
	Cards.Reset(); Chains.Reset(); Error.Reset();
	TestTrue(TEXT("The revised catalog rematerializes from the new registry"),
		UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(Compiled.Registry, {}, Cards, Chains, Error));
	const FHansaBuildCardPresentation* RevisedGrainFarm = FindCard(Cards, TEXT("Building.GrainFarm"));
	TestTrue(TEXT("Hot reload updates name, cost, and order while preserving semantic identity"),
		RevisedGrainFarm != nullptr && RevisedGrainFarm->StableId == TEXT("Building.GrainFarm") &&
		RevisedGrainFarm->Name.ToString() == TEXT("Revised grain farm") &&
		RevisedGrainFarm->Cost.ToString().Contains(TEXT("3111 pf")) && RevisedGrainFarm->MenuOrder == 4);

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> LockedDefinitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	UHansaBuildingDefinition* Sawmill = CastChecked<UHansaBuildingDefinition>(
		FindDefinition(LockedDefinitions, TEXT("Building.Sawmill")));
	Sawmill->RequiredConstructionTechnologyId = TEXT("Technology.Production.ImprovedSawmilling");
	const FHansaEconomicRegistryCompileResult LockedCompile =
		FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(LockedDefinitions));
	TestTrue(TEXT("A valid authored technology prerequisite compiles"), LockedCompile.IsValid());
	Cards.Reset(); Chains.Reset(); Error.Reset();
	UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(LockedCompile.Registry, {}, Cards, Chains, Error);
	const FHansaBuildCardPresentation* LockedSawmill = FindCard(Cards, TEXT("Building.Sawmill"));
	TestTrue(TEXT("Incomplete research locks content with the causal technology name"), LockedSawmill != nullptr &&
		LockedSawmill->bLocked && LockedSawmill->LockedReason.ToString().Contains(TEXT("Improved sawmilling")));
	Cards.Reset(); Chains.Reset(); Error.Reset();
	UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(
		LockedCompile.Registry, { TEXT("Technology.Production.ImprovedSawmilling") }, Cards, Chains, Error);
	const FHansaBuildCardPresentation* UnlockedSawmill = FindCard(Cards, TEXT("Building.Sawmill"));
	TestTrue(TEXT("Completing the authored prerequisite unlocks the same card"),
		UnlockedSawmill != nullptr && !UnlockedSawmill->bLocked);

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> MissingChain =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	MissingChain.RemoveAll([](const TStrongObjectPtr<UHansaDefinitionBase>& Definition)
	{
		return Definition->StableDefinitionId == TEXT("Building.Mill");
	});
	const FHansaEconomicRegistryCompileResult MissingChainResult =
		FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(MissingChain));
	TestFalse(TEXT("A production chain with a missing member fails validation"), MissingChainResult.IsValid());
	TestTrue(TEXT("Missing chain members use a stable actionable diagnostic"),
		ContainsIssueCode(MissingChainResult, TEXT("HSA-REGISTRY-038")));

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> MissingTechnology =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	CastChecked<UHansaBuildingDefinition>(FindDefinition(MissingTechnology, TEXT("Building.Sawmill")))
		->RequiredConstructionTechnologyId = TEXT("Technology.Production.Missing");
	const FHansaEconomicRegistryCompileResult MissingTechnologyResult =
		FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(MissingTechnology));
	TestFalse(TEXT("A missing construction technology fails validation"), MissingTechnologyResult.IsValid());
	TestTrue(TEXT("Missing construction prerequisites use a stable actionable diagnostic"),
		ContainsIssueCode(MissingTechnologyResult, TEXT("HSA-REGISTRY-037")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEconomicInvalidReferencesTest,
	"Hansa.Content.Definitions.InvalidReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEconomicInvalidReferencesTest::RunTest(const FString& Parameters)
{
	const FString FixturePath = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Tests"), TEXT("Fixtures"), TEXT("economic_invalid_references_v1.json"));
	FString Json;
	if (!TestTrue(TEXT("Invalid-reference fixture is readable"), FFileHelper::LoadFileToString(Json, *FixturePath)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!TestTrue(TEXT("Invalid-reference fixture parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Cases = nullptr;
	if (!TestTrue(TEXT("Invalid-reference fixture declares cases"), Root->TryGetArrayField(TEXT("cases"), Cases) && Cases != nullptr))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& CaseValue : *Cases)
	{
		const TSharedPtr<FJsonObject> Case = CaseValue->AsObject();
		const FString Kind = Case->GetStringField(TEXT("kind"));
		const FString TargetId = Case->GetStringField(TEXT("targetId"));
		const FString InvalidReference = Case->GetStringField(TEXT("invalidReference"));
		const FString ExpectedCode = Case->GetStringField(TEXT("expectedCode"));
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaDefinitionBase* Target = Hansa::Editor::Tests::FindDefinition(Definitions, TargetId);
		if (!TestNotNull(*FString::Printf(TEXT("Fixture target exists: %s"), *TargetId), Target))
		{
			continue;
		}

		if (Kind == TEXT("recipeInput"))
		{
			CastChecked<UHansaRecipeDefinition>(Target)->Inputs[0].GoodId = InvalidReference;
		}
		else if (Kind == TEXT("buildingRecipe"))
		{
			CastChecked<UHansaBuildingDefinition>(Target)->RecipeIds[0] = InvalidReference;
		}
		else if (Kind == TEXT("buildingUpgrade"))
		{
			CastChecked<UHansaBuildingDefinition>(Target)->UpgradeTargetBuildingId = InvalidReference;
		}

		const FHansaEconomicRegistryCompileResult CompileResult =
			FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(*FString::Printf(TEXT("Invalid fixture case fails: %s"), *Kind), CompileResult.IsValid());
		TestTrue(
			*FString::Printf(TEXT("Invalid fixture case reports %s"), *ExpectedCode),
			Hansa::Editor::Tests::ContainsIssueCode(CompileResult, ExpectedCode));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEconomicAssetReloadTest,
	"Hansa.Integration.Authoring.EconomicAssetReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEconomicAssetReloadTest::RunTest(const FString& Parameters)
{
	Hansa::Editor::Tests::FReviewedEconomicCatalog ReviewedCatalog;
	FString ManifestError;
	if (!TestTrue(TEXT("Reviewed economic catalog manifest loads"),
		Hansa::Editor::Tests::LoadReviewedEconomicCatalog(ReviewedCatalog, ManifestError)))
	{
		AddError(ManifestError);
		return false;
	}
	TestEqual(TEXT("Runtime and manifest select the same catalog version"),
		ReviewedCatalog.CatalogVersion, FHansaLubeckScenarioInitializer::MvpCatalogVersion);
	TestEqual(TEXT("Runtime and manifest pin the same catalog hash"),
		ReviewedCatalog.RegistryHash, FHansaLubeckScenarioInitializer::MvpRegistryHash);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous({ TEXT("/Game/Hansa/Core") }, true);
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Assets, true, false);
	TArray<const UHansaDefinitionBase*> LoadedDefinitions;
	for (const FAssetData& AssetData : Assets)
	{
		if (const UHansaDefinitionBase* Definition = Cast<UHansaDefinitionBase>(AssetData.GetAsset()))
		{
			TestTrue(*FString::Printf(TEXT("%s refreshes its derived content hash on reload"), *Definition->StableDefinitionId), Definition->ContentHash != 0);
			LoadedDefinitions.Add(Definition);
		}
	}
	TestEqual(TEXT("All authored expanded-MVP definition assets reload from disk"), LoadedDefinitions.Num(), FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 35 ? 208 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 33 ? 205 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 30 ? 181 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 29 ? 163 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 28 ? 118 : 106);
	const FHansaEconomicRegistryCompileResult CompileResult = FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
	for (const FHansaDefinitionValidationIssue& Issue : CompileResult.Issues)
	{
		AddInfo(FString::Printf(TEXT("Catalog validation %s at %s: %s Remedy: %s"),
			*Issue.Code.ToString(), *Issue.PropertyPath, *Issue.Cause.ToString(), *Issue.Remedy.ToString()));
	}
	TestTrue(TEXT("Reloaded production assets compile"), CompileResult.IsValid());
	TArray<const UHansaDefinitionBase*> ReversedLoaded = LoadedDefinitions;
	Algo::Reverse(ReversedLoaded);
	const auto ReverseCompile = FHansaEconomicDefinitionCompiler::Compile(ReversedLoaded);
	TestTrue(TEXT("Reviewed assets compile in reverse discovery order"), ReverseCompile.IsValid());
	TestEqual(TEXT("Reviewed registry is discovery-order independent"), ReverseCompile.Registry.GetRegistryHash(), CompileResult.Registry.GetRegistryHash());
	TestTrue(TEXT("Reviewed fingerprints are discovery-order independent"),
		Hansa::Editor::Tests::SameDefinitionHashes(ReverseCompile.DefinitionHashes, CompileResult.DefinitionHashes));
	if (CompileResult.Registry.GetRegistryHash() != ReviewedCatalog.RegistryHash ||
		!Hansa::Editor::Tests::SameDefinitionHashes(CompileResult.DefinitionHashes, ReviewedCatalog.Definitions))
	{
		AddError(CompileResult.DescribeRegistryHashMismatch(
			ReviewedCatalog.RegistryHash, ReviewedCatalog.Definitions));
	}
	if (FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 35)
	{
		Hansa::Editor::Tests::FReviewedEconomicCatalog VersionThirtyFour;
		if (!TestTrue(TEXT("Catalog v34 lineage manifest loads"),
			Hansa::Editor::Tests::LoadReviewedEconomicCatalog(
				VersionThirtyFour, ManifestError, TEXT("economic_catalog_v34.json"))))
		{
			AddError(ManifestError);
			return false;
		}
		TestEqual(TEXT("Catalog v35 names v34 as its compatible predecessor"),
			VersionThirtyFour.RegistryHash, FHansaLubeckScenarioInitializer::ImmediatePreviousMvpRegistryHash);

		TMap<FString, uint64> PreviousHashes;
		for (const FHansaEconomicDefinitionHashEvidence& Evidence : VersionThirtyFour.Definitions)
		{
			PreviousHashes.Add(Evidence.StableId, Evidence.ContentHash);
		}
		TArray<FString> Added;
		TArray<FString> Changed;
		for (const FHansaEconomicDefinitionHashEvidence& Evidence : CompileResult.DefinitionHashes)
		{
			if (const uint64* PreviousHash = PreviousHashes.Find(Evidence.StableId))
			{
				if (*PreviousHash != Evidence.ContentHash) Changed.Add(Evidence.StableId);
				PreviousHashes.Remove(Evidence.StableId);
			}
			else
			{
				Added.Add(Evidence.StableId);
			}
		}
		Added.Sort();
		Changed.Sort();
		TArray<FString> Removed;
		PreviousHashes.GetKeys(Removed);
		Removed.Sort();
		TestEqual(TEXT("Catalog v35 adds only the three reviewed specialization capabilities"),
			FString::Join(Added, TEXT(",")),
			FString(TEXT("PresenceCapability.HarborSpecialization,PresenceCapability.MarketSpecialization,PresenceCapability.WarehouseSpecialization")));
		TestEqual(TEXT("Catalog v35 changes only specialization-dependent stages and city policies"),
			FString::Join(Changed, TEXT(",")),
			FString(TEXT("CityTradePolicy.Hamburg,CityTradePolicy.Lubeck,CityTradePolicy.Luneburg,CityTradePolicy.Rostock,PresenceStage.ExceptionalGovernance,PresenceStage.MerchantOffice,PresenceStage.MerchantQuarter,PresenceStage.PrivilegedPresence,PresenceStage.TradeStation,PresenceStage.VisitingContact")));
		TestTrue(TEXT("Catalog v35 removes no v34 definitions"), Removed.IsEmpty());
	}
	TestEqual(TEXT("Reviewed manifest covers every authored definition"), ReviewedCatalog.Definitions.Num(), FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 35 ? 208 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 33 ? 205 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 30 ? 181 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 29 ? 163 : FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 28 ? 118 : 106);
	// V29 is an explicitly incompatible complete snapshot. Its disk manifest and reverse-order
	// compile above are authoritative; historical serialized FText identities are not derivable
	// from hashes, so the older field-by-field lineage proof remains scoped to v28 and earlier.
	if (FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 29) return true;

    TArray<TStrongObjectPtr<UHansaDefinitionBase>> ArtisanBaseline;
    if (FHansaLubeckScenarioInitializer::MvpCatalogVersion >= 28)
    {
    // Reverse the reviewed artisan field diff to prove the entire v27 lineage.
    FString ArtisanDiff;
    if (!FFileHelper::LoadFileToString(ArtisanDiff, *(FPaths::ProjectDir()/TEXT("Docs/Development/ArtisanProduction/candidate.json")))) return false;
    TSharedPtr<FJsonObject> ArtisanRoot;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ArtisanDiff), ArtisanRoot)) return false;
    for (const auto& Entry : ArtisanRoot->GetArrayField(TEXT("definitions")))
    {
        const auto Row=Entry->AsObject(); const FString Id=Row->GetStringField(TEXT("stableId"));
        const FString Change=Row->GetStringField(TEXT("change"));
        if(Change==TEXT("added")) { LoadedDefinitions.RemoveAll([&](const auto* D){return D->StableDefinitionId==Id;}); continue; }
        if(Change!=TEXT("modified")) continue;
        for(auto& Current:LoadedDefinitions) if(Current->StableDefinitionId==Id)
        {
            auto* Old=DuplicateObject<UHansaDefinitionBase>(Current,GetTransientPackage());
            for(const auto& Value:Row->GetArrayField(TEXT("propertyChanges")))
            {
                const auto Field=Value->AsObject();
                FProperty* P=Old->GetClass()->FindPropertyByName(*Field->GetStringField(TEXT("property")));
                if(!P || !P->ImportText_InContainer(*Field->GetStringField(TEXT("before")),Old,Old,PPF_None)) return false;
            }
            Old->RefreshContentHash();Current=Old;ArtisanBaseline.Emplace(Old);
        }
    }
    const auto VersionTwentySeven=FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
    Hansa::Editor::Tests::FReviewedEconomicCatalog BeforeArtisan;
    if(!Hansa::Editor::Tests::LoadReviewedEconomicCatalog(BeforeArtisan,ManifestError,TEXT("economic_catalog_v27.json")))return false;
    TestTrue(TEXT("Pre-artisan catalog validates"),VersionTwentySeven.IsValid());
    TestEqual(TEXT("Exact v27 hash reconstructs"),VersionTwentySeven.Registry.GetRegistryHash(),BeforeArtisan.RegistryHash);
    TestTrue(TEXT("Every v27 fingerprint reconstructs"),Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentySeven.DefinitionHashes,BeforeArtisan.Definitions));

    }

    // Reconstruct the complete pre-compaction catalogue before older lineage checks.
    TArray<TStrongObjectPtr<UHansaResidentialCompoundDefinition>> CompactBaseline;
    TArray<TStrongObjectPtr<UHansaBuildingDefinition>> CompactBindingBaseline;
    for(auto& Current:LoadedDefinitions)if(const auto* Compound=Cast<UHansaResidentialCompoundDefinition>(Current))
    {
        if(!Compound->StableDefinitionId.StartsWith(TEXT("Compound.Laborer.")))continue;
        const FString Family=Compound->StableDefinitionId.Mid(17);FString Json,Error;
        if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R07/baseline")/(Family+TEXT(".json")))))return false;
        TStrongObjectPtr<UHansaResidentialCompoundDefinition> Old(Hansa::Editor::Compounds::ImportDraft(Json,Error));
        if(!TestNotNull(*Error,Old.Get()))return false;
        auto* Previous=DuplicateObject<UHansaResidentialCompoundDefinition>(Compound,GetTransientPackage());
        Previous->Layouts=Old->Layouts;Previous->FootprintWidthCells=Old->FootprintWidthCells;Previous->FootprintHeightCells=Old->FootprintHeightCells;
        Previous->BoundsMin=Old->BoundsMin;Previous->BoundsMax=Old->BoundsMax;Previous->AuthoredRevision=Old->AuthoredRevision;Previous->RefreshContentHash();
        Current=Previous;CompactBaseline.Emplace(Previous);
    }
    for(auto& Current:LoadedDefinitions)if(const auto* B=Cast<UHansaBuildingDefinition>(Current))
    {
        const auto* Compound=B->LoadResidentialCompound();if(!Compound)continue;
        for(const auto& Old:CompactBaseline)if(Old->StableDefinitionId==Compound->StableDefinitionId)
        {
            auto* Previous=DuplicateObject<UHansaBuildingDefinition>(B,GetTransientPackage());
            Previous->ResidentialCompound=Old.Get();Previous->FootprintWidthCells=Old->FootprintWidthCells;Previous->FootprintHeightCells=Old->FootprintHeightCells;
            --Previous->AuthoredRevision;Previous->RefreshContentHash();Current=Previous;CompactBindingBaseline.Emplace(Previous);break;
        }
    }
    const auto VersionTwentySix=FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
    Hansa::Editor::Tests::FReviewedEconomicCatalog BeforeCompactCatalog;
    if(!TestTrue(TEXT("Pre-compaction v26 manifest loads"),Hansa::Editor::Tests::LoadReviewedEconomicCatalog(BeforeCompactCatalog,ManifestError,TEXT("economic_catalog_v26.json"))))return false;
    TestTrue(TEXT("Pre-compaction catalogue validates"),VersionTwentySix.IsValid());
    TestEqual(TEXT("Exact v26 hash reconstructs"),VersionTwentySix.Registry.GetRegistryHash(),BeforeCompactCatalog.RegistryHash);
    TestTrue(TEXT("Every v26 fingerprint reconstructs"),Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentySix.DefinitionHashes,BeforeCompactCatalog.Definitions));

    // Reverse only firewood's additions and nine field changes. Preservation stays intact.
    LoadedDefinitions.RemoveAll([](const UHansaDefinitionBase* D) {
        return D->StableDefinitionId==TEXT("Good.Firewood")||D->StableDefinitionId==TEXT("Recipe.SplitFirewood")||D->StableDefinitionId==TEXT("Building.WoodcutterYard")||D->StableDefinitionId==TEXT("Need.Heating");
    });
    TArray<TStrongObjectPtr<UHansaDefinitionBase>> FirewoodBaseline;
    for(auto& Current:LoadedDefinitions)
    {
        const FString Id=Current->StableDefinitionId;
        if(Id!=TEXT("Recipe.BakeBread")&&Id!=TEXT("Recipe.MaltGrain")&&Id!=TEXT("Recipe.BrewBeer")&&!Current->IsA<UHansaPopulationTierDefinition>()&&!Current->IsA<UHansaCityMarketProfileDefinition>())continue;
        auto* Previous=DuplicateObject<UHansaDefinitionBase>(Current,GetTransientPackage());
        --Previous->AuthoredRevision;
        if(auto* R=Cast<UHansaRecipeDefinition>(Previous))R->Inputs.RemoveAll([](const auto& A){return A.GoodId==TEXT("Good.Firewood");});
        if(auto* T=Cast<UHansaPopulationTierDefinition>(Previous))T->Needs.RemoveAll([](const auto& A){return A.NeedId==TEXT("Need.Heating");});
        if(auto* M=Cast<UHansaCityMarketProfileDefinition>(Previous))M->Goods.RemoveAll([](const auto& A){return A.GoodId==TEXT("Good.Firewood");});
        Previous->RefreshContentHash();Current=Previous;FirewoodBaseline.Emplace(Previous);
    }
    const auto VersionTwentyFive=FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
    Hansa::Editor::Tests::FReviewedEconomicCatalog PreviousFirewoodCatalog;
    if(!TestTrue(TEXT("Reviewed v25 manifest loads"),Hansa::Editor::Tests::LoadReviewedEconomicCatalog(PreviousFirewoodCatalog,ManifestError,TEXT("economic_catalog_v25_preservedfish.json"))))return false;
    TestTrue(TEXT("Previous preservation catalog reconstructs"),VersionTwentyFive.IsValid());
    TestEqual(TEXT("Firewood preserves v25 economics exactly"),VersionTwentyFive.Registry.GetRegistryHash(),PreviousFirewoodCatalog.RegistryHash);
    TestTrue(TEXT("Every previous fingerprint survives"),Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentyFive.DefinitionHashes,PreviousFirewoodCatalog.Definitions));

    // Reverse only preservation's authored changes and prove the exact v24 contract.
    LoadedDefinitions.RemoveAll([](const UHansaDefinitionBase* D) {
        return D->StableDefinitionId == TEXT("Good.PreservedFish") || D->StableDefinitionId == TEXT("Recipe.SaltedCatch") || D->StableDefinitionId == TEXT("Building.Fishery.SaltingShed");
    });
    TArray<TStrongObjectPtr<UHansaDefinitionBase>> PreservationBaseline;
    for (int32 Index = 0; Index < LoadedDefinitions.Num(); ++Index)
    {
        const auto* Current = LoadedDefinitions[Index];
        const FString Id = Current->StableDefinitionId;
        if (Id != TEXT("Good.Fish") && Id != TEXT("Need.Fish") && Id != TEXT("Building.Fishery") && !Cast<UHansaCityMarketProfileDefinition>(Current)) continue;
        TStrongObjectPtr<UHansaDefinitionBase> Previous(DuplicateObject<UHansaDefinitionBase>(Current, GetTransientPackage()));
        --Previous->AuthoredRevision;
        if (auto* Good = Cast<UHansaGoodDefinition>(Previous.Get())) { const auto* OriginalFish = LoadObject<UHansaGoodDefinition>(nullptr, TEXT("/Game/PreservationBaseline/DA_Good_Fish.DA_Good_Fish")); if (!OriginalFish) { AddError(TEXT("Original fish localization fixture missing")); return false; } Good->DisplayName = OriginalFish->DisplayName; Good->bSpoilageEnabled = false; }
        if (auto* Need = Cast<UHansaNeedDefinition>(Previous.Get())) Need->Alternatives.Reset();
        if (auto* Building = Cast<UHansaBuildingDefinition>(Previous.Get())) Building->UpgradeTargetBuildingId.Reset();
        if (auto* City = Cast<UHansaCityMarketProfileDefinition>(Previous.Get()))
        {
            City->Goods.RemoveAll([](const auto& G){return G.GoodId == TEXT("Good.PreservedFish");});
            if (Id.Contains(TEXT("Rostock")))
            {
                --City->AuthoredRevision;
                for (auto& Good : City->Goods) if (Good.GoodId == TEXT("Good.Salt")) Good.BackgroundProductionMilliUnitsPerUpdate = 1000;
            }
        }
        Previous->RefreshContentHash(); LoadedDefinitions[Index] = Previous.Get(); PreservationBaseline.Add(MoveTemp(Previous));
    }
    const auto VersionTwentyFour = FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
    Hansa::Editor::Tests::FReviewedEconomicCatalog PreviousPreservationCatalog;
    if (!TestTrue(TEXT("Full v24 manifest loads"), Hansa::Editor::Tests::LoadReviewedEconomicCatalog(PreviousPreservationCatalog, ManifestError, TEXT("economic_catalog_v24.json")))) return false;
    TestTrue(TEXT("Catalog v24 reconstructs without preservation"), VersionTwentyFour.IsValid());
    TestEqual(TEXT("Preservation has no unrelated catalog changes"), VersionTwentyFour.Registry.GetRegistryHash(), PreviousPreservationCatalog.RegistryHash);
    TestTrue(TEXT("Every v24 fingerprint is preserved"), Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentyFour.DefinitionHashes, PreviousPreservationCatalog.Definitions));

    // Additive artisan plots leave all 97 prior fingerprints and legacy parcels unchanged.
    LoadedDefinitions.RemoveAll([](const UHansaDefinitionBase* D) {
        return D->StableDefinitionId == TEXT("Compound.Artisan.Plot") || D->StableDefinitionId == TEXT("Building.Residence.Artisan.Plot");
    });
    const auto VersionTwentyThree = FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
    Hansa::Editor::Tests::FReviewedEconomicCatalog PreviousPlotCatalog;
    if (!TestTrue(TEXT("Full v23 manifest loads"), Hansa::Editor::Tests::LoadReviewedEconomicCatalog(PreviousPlotCatalog, ManifestError, TEXT("economic_catalog_v23.json")))) return false;
    TestTrue(TEXT("Catalog v23 reconstructs by removing only the two additions"), VersionTwentyThree.IsValid());
    TestEqual(TEXT("Prior registry is unchanged"), VersionTwentyThree.Registry.GetRegistryHash(), PreviousPlotCatalog.RegistryHash);
    TestTrue(TEXT("Every v23 fingerprint is preserved"), Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentyThree.DefinitionHashes, PreviousPlotCatalog.Definitions));

    // Reconstruct the exact prior court content, keeping every gameplay binding unchanged.
    TArray<const UHansaDefinitionBase*> VersionTwentyTwoDefinitions = LoadedDefinitions;
    TArray<TStrongObjectPtr<UHansaResidentialCompoundDefinition>> WeightBaselineCourts;
    for (auto& Definition : VersionTwentyTwoDefinitions)
    {
        if (!Definition->IsA<UHansaResidentialCompoundDefinition>()) continue;
        const FString Family = Definition->StableDefinitionId.RightChop(FString(TEXT("Compound.Laborer.")).Len());
        FString Json, Error;
        const FString Baseline = FPaths::ProjectDir() / TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R06/baseline") / (Family + TEXT(".json"));
        if (!TestTrue(TEXT("Prior court definition preserved"), FFileHelper::LoadFileToString(Json, *Baseline))) return false;
        auto* Previous = Hansa::Editor::Compounds::ImportDraft(Json, Error);
        if (!TestNotNull(*Error, Previous)) return false;
        auto* Preserved=DuplicateObject<UHansaResidentialCompoundDefinition>(CastChecked<UHansaResidentialCompoundDefinition>(Definition),GetTransientPackage());
        Preserved->Layouts=Previous->Layouts;Preserved->AuthoredRevision=Previous->AuthoredRevision;Preserved->RefreshContentHash();
        WeightBaselineCourts.Emplace(Preserved); Definition = Preserved;
    }
    TArray<TStrongObjectPtr<UHansaBuildingDefinition>> WeightBaselineBindings;
    for(auto& Definition:VersionTwentyTwoDefinitions)if(const auto* B=Cast<UHansaBuildingDefinition>(Definition))
    {
        const auto* Bound=B->LoadResidentialCompound();if(!Bound)continue;
        for(const auto& Court:WeightBaselineCourts)if(Court->StableDefinitionId==Bound->StableDefinitionId)
        {
            auto* Copy=DuplicateObject<UHansaBuildingDefinition>(B,GetTransientPackage());Copy->ResidentialCompound=Court.Get();
            WeightBaselineBindings.Emplace(Copy);Definition=Copy;break;
        }
    }
    const auto VersionTwentyTwo = FHansaEconomicDefinitionCompiler::Compile(VersionTwentyTwoDefinitions);
    TestTrue(TEXT("Catalog v22 reconstruction compiles"), VersionTwentyTwo.IsValid());
    TestEqual(TEXT("Court decoration changes only four presentation definitions"), VersionTwentyTwo.Registry.GetRegistryHash(), uint64(0x76FF996D95CBB5EAULL));
    Hansa::Editor::Tests::FReviewedEconomicCatalog WeightBaselineCatalog;
    if (!TestTrue(TEXT("Prior full manifest loads"), Hansa::Editor::Tests::LoadReviewedEconomicCatalog(WeightBaselineCatalog, ManifestError, TEXT("economic_catalog_v22.json")))) return false;
    TestTrue(TEXT("Every prior fingerprint is reconstructed"), Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentyTwo.DefinitionHashes, WeightBaselineCatalog.Definitions));

    LoadedDefinitions = VersionTwentyTwoDefinitions;
    // Reverting only artisan presentation fields must reproduce all 97 v21 fingerprints.
    TStrongObjectPtr<UHansaBuildingDefinition> PreviousArtisan;
    TArray<const UHansaDefinitionBase*> VersionTwentyOneDefinitions = LoadedDefinitions;
    for (auto& Definition : VersionTwentyOneDefinitions)
    {
        if (Definition->StableDefinitionId != TEXT("Building.Residence.Artisan")) continue;
        PreviousArtisan.Reset(DuplicateObject<UHansaBuildingDefinition>(CastChecked<UHansaBuildingDefinition>(Definition), GetTransientPackage()));
        PreviousArtisan->AuthoredRevision = 2;
        PreviousArtisan->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Mesh/hansa-residences/Meshes_R06/SM_Residence_Artisan_A.SM_Residence_Artisan_A")));
        PreviousArtisan->PresentationActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Mesh/hansa-residences/BP_Residence_Artisan_Review.BP_Residence_Artisan_Review_C")));
        PreviousArtisan->RefreshContentHash();
        Definition = PreviousArtisan.Get();
    }
    const auto VersionTwentyOne = FHansaEconomicDefinitionCompiler::Compile(VersionTwentyOneDefinitions);
    TestTrue(TEXT("Previous artisan catalog compiles"), VersionTwentyOne.IsValid());
    TestEqual(TEXT("Only artisan presentation changes in v22"), VersionTwentyOne.Registry.GetRegistryHash(), uint64(0x5E0327141B0AC574ULL));
    Hansa::Editor::Tests::FReviewedEconomicCatalog VersionTwentyOneCatalog;
    if (!TestTrue(TEXT("Full v21 manifest loads"), Hansa::Editor::Tests::LoadReviewedEconomicCatalog(VersionTwentyOneCatalog, ManifestError, TEXT("economic_catalog_v21.json")))) return false;
    TestTrue(TEXT("All v21 fingerprints reconstructed"), Hansa::Editor::Tests::SameDefinitionHashes(VersionTwentyOne.DefinitionHashes, VersionTwentyOneCatalog.Definitions));
    LoadedDefinitions = VersionTwentyOneDefinitions;

    // Reconstruct the exact prior court content, keeping every gameplay binding unchanged.
    {
    TArray<const UHansaDefinitionBase*> VersionTwentyDefinitions = LoadedDefinitions;
    TArray<TStrongObjectPtr<UHansaResidentialCompoundDefinition>> PreviousCourts;
    for (auto& Definition : VersionTwentyDefinitions)
    {
        if (!Definition->IsA<UHansaResidentialCompoundDefinition>()) continue;
        const FString Family = Definition->StableDefinitionId.RightChop(FString(TEXT("Compound.Laborer.")).Len());
        FString Json, Error;
        const FString Baseline = FPaths::ProjectDir() / TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R05/baseline") / (Family + TEXT(".json"));
        if (!TestTrue(TEXT("Prior court definition preserved"), FFileHelper::LoadFileToString(Json, *Baseline))) return false;
        auto* Previous = Hansa::Editor::Compounds::ImportDraft(Json, Error);
        if (!TestNotNull(*Error, Previous)) return false;
        auto* Preserved=DuplicateObject<UHansaResidentialCompoundDefinition>(CastChecked<UHansaResidentialCompoundDefinition>(Definition),GetTransientPackage());
        Preserved->Layouts=Previous->Layouts;Preserved->AuthoredRevision=Previous->AuthoredRevision;Preserved->RefreshContentHash();
        PreviousCourts.Emplace(Preserved); Definition = Preserved;
    }
    TArray<TStrongObjectPtr<UHansaBuildingDefinition>> PreviousBindings;
    for(auto& Definition:VersionTwentyDefinitions)if(const auto* B=Cast<UHansaBuildingDefinition>(Definition))
    {
        const auto* Bound=B->LoadResidentialCompound();if(!Bound)continue;
        for(const auto& Court:PreviousCourts)if(Court->StableDefinitionId==Bound->StableDefinitionId)
        {
            auto* Copy=DuplicateObject<UHansaBuildingDefinition>(B,GetTransientPackage());Copy->ResidentialCompound=Court.Get();
            PreviousBindings.Emplace(Copy);Definition=Copy;break;
        }
    }
    const auto VersionTwenty = FHansaEconomicDefinitionCompiler::Compile(VersionTwentyDefinitions);
    TestTrue(TEXT("Catalog v20 reconstruction compiles"), VersionTwenty.IsValid());
    TestEqual(TEXT("Court decoration changes only four presentation definitions"), VersionTwenty.Registry.GetRegistryHash(), uint64(0x4A86F28719E21627ULL));
    Hansa::Editor::Tests::FReviewedEconomicCatalog PreviousCatalog;
    if (!TestTrue(TEXT("Prior full manifest loads"), Hansa::Editor::Tests::LoadReviewedEconomicCatalog(PreviousCatalog, ManifestError, TEXT("economic_catalog_v20.json")))) return false;
    TestTrue(TEXT("Every prior fingerprint is reconstructed"), Hansa::Editor::Tests::SameDefinitionHashes(VersionTwenty.DefinitionHashes, PreviousCatalog.Definitions));

    }
    TArray<const UHansaDefinitionBase*> VersionNineteenDefinitions = LoadedDefinitions;
    TArray<TStrongObjectPtr<UHansaResidentialCompoundDefinition>> PreviousCourts;
    for (auto& Definition : VersionNineteenDefinitions)
    {
        if (!Definition->IsA<UHansaResidentialCompoundDefinition>()) continue;
        const FString Family = Definition->StableDefinitionId.RightChop(FString(TEXT("Compound.Laborer.")).Len());
        FString Json, Error;
        const FString Baseline = FPaths::ProjectDir() / TEXT("SourceArt/Generated/Compounds/LabourCourts_20260915/R04/baseline") / (Family + TEXT(".json"));
        if (!TestTrue(TEXT("Prior court definition preserved"), FFileHelper::LoadFileToString(Json, *Baseline))) return false;
        auto* Previous = Hansa::Editor::Compounds::ImportDraft(Json, Error);
        if (!TestNotNull(*Error, Previous)) return false;
        auto* Preserved=DuplicateObject<UHansaResidentialCompoundDefinition>(CastChecked<UHansaResidentialCompoundDefinition>(Definition),GetTransientPackage());
        Preserved->Layouts=Previous->Layouts;Preserved->AuthoredRevision=Previous->AuthoredRevision;Preserved->RefreshContentHash();
        PreviousCourts.Emplace(Preserved); Definition = Preserved;
    }
    TArray<TStrongObjectPtr<UHansaBuildingDefinition>> PreviousBindings;
    for(auto& Definition:VersionNineteenDefinitions)if(const auto* B=Cast<UHansaBuildingDefinition>(Definition))
    {
        const auto* Bound=B->LoadResidentialCompound();if(!Bound)continue;
        for(const auto& Court:PreviousCourts)if(Court->StableDefinitionId==Bound->StableDefinitionId)
        {
            auto* Copy=DuplicateObject<UHansaBuildingDefinition>(B,GetTransientPackage());Copy->ResidentialCompound=Court.Get();
            PreviousBindings.Emplace(Copy);Definition=Copy;break;
        }
    }
    const auto VersionNineteen = FHansaEconomicDefinitionCompiler::Compile(VersionNineteenDefinitions);
    TestTrue(TEXT("Catalog v19 reconstruction compiles"), VersionNineteen.IsValid());
    TestEqual(TEXT("Court decoration changes only four presentation definitions"), VersionNineteen.Registry.GetRegistryHash(), uint64(0x31FB425080110FD0ULL));
    Hansa::Editor::Tests::FReviewedEconomicCatalog PreviousCatalog;
    if (!TestTrue(TEXT("Prior full manifest loads"), Hansa::Editor::Tests::LoadReviewedEconomicCatalog(PreviousCatalog, ManifestError, TEXT("economic_catalog_v19.json")))) return false;
    TestTrue(TEXT("Every prior fingerprint is reconstructed"), Hansa::Editor::Tests::SameDefinitionHashes(VersionNineteen.DefinitionHashes, PreviousCatalog.Definitions));

	TArray<const UHansaDefinitionBase*> VersionEighteenDefinitions = LoadedDefinitions.FilterByPredicate([](const UHansaDefinitionBase* D)
    {
        const auto* B=Cast<UHansaBuildingDefinition>(D);
        return !D->IsA<UHansaResidentialCompoundDefinition>() && (!B||B->ResidentialCompound.IsNull());
    });
    const auto VersionEighteen=FHansaEconomicDefinitionCompiler::Compile(VersionEighteenDefinitions);
    TestTrue(TEXT("Catalog v18 reconstruction compiles"),VersionEighteen.IsValid());
    TestEqual(TEXT("Catalog v19 only adds reviewed compounds and new bindings"),VersionEighteen.Registry.GetRegistryHash(),uint64(0x1C2B54191C78E4CAULL));
    TArray<const UHansaDefinitionBase*> VersionSeventeenDefinitions = VersionEighteenDefinitions;
	TArray<TStrongObjectPtr<UHansaBuildingDefinition>> VersionSeventeenPresentations;
	for (int32 Index = 0; Index < VersionSeventeenDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionSeventeenDefinitions[Index]);
		if (!Building || (Building->StableDefinitionId != TEXT("Building.MaltHouse") &&
			Building->StableDefinitionId != TEXT("Building.Cooperage"))) continue;
		TStrongObjectPtr<UHansaBuildingDefinition> Previous(
			DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		Previous->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		Previous->AuthoredRevision = 1;
		Previous->RefreshContentHash();
		VersionSeventeenDefinitions[Index] = Previous.Get();
		VersionSeventeenPresentations.Add(MoveTemp(Previous));
	}
	const auto VersionSeventeen = FHansaEconomicDefinitionCompiler::Compile(VersionSeventeenDefinitions);
	TestTrue(TEXT("Catalog v17 reconstruction compiles"), VersionSeventeen.IsValid());
	TestEqual(TEXT("Catalog v18 changes only the malt house and cooperage presentation meshes"),
		VersionSeventeen.Registry.GetRegistryHash(), 0x968431FAD59A2C51ULL);

	TArray<const UHansaDefinitionBase*> VersionSixteenDefinitions = VersionSeventeenDefinitions;
	TStrongObjectPtr<UHansaBuildingDefinition> VersionSixteenLaborerResidence;
	for (int32 Index = 0; Index < VersionSixteenDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionSixteenDefinitions[Index]);
		if (!Building || Building->StableDefinitionId != TEXT("Building.Residence.Laborer")) continue;
		VersionSixteenLaborerResidence.Reset(
			DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		VersionSixteenLaborerResidence->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Mesh/hansa-residences/Meshes_R06/SM_Residence_Laborer_A.SM_Residence_Laborer_A")));
		VersionSixteenLaborerResidence->RefreshContentHash();
		VersionSixteenDefinitions[Index] = VersionSixteenLaborerResidence.Get();
		break;
	}
	const auto VersionSixteen = FHansaEconomicDefinitionCompiler::Compile(VersionSixteenDefinitions);
	TestTrue(TEXT("Catalog v16 reconstruction compiles"), VersionSixteen.IsValid());
	TestEqual(TEXT("Catalog v17 changes only the reviewed R07 laborer residence presentation"),
		VersionSixteen.Registry.GetRegistryHash(), 0xB65512A7BFAC9E0CULL);

	TArray<const UHansaDefinitionBase*> VersionFifteenDefinitions = VersionSixteenDefinitions;
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> VersionFifteenOwned;
	for (int32 Index = 0; Index < VersionFifteenDefinitions.Num(); ++Index)
	{
		const FString Id = VersionFifteenDefinitions[Index]->StableDefinitionId;
		const bool bFarm = Id == TEXT("Building.GrainFarm") || Id == TEXT("Recipe.GrowGrain");
		const bool bMill = Id == TEXT("Building.Mill") || Id == TEXT("Recipe.MillFlour");
		const bool bBakery = Id == TEXT("Building.Bakery") || Id == TEXT("Recipe.BakeBread");
		const bool bFish = Id == TEXT("Building.Fishery") || Id == TEXT("Recipe.CatchFish");
		if (!bFarm && !bMill && !bBakery && !bFish) continue;
		TStrongObjectPtr<UHansaDefinitionBase> Previous(
			DuplicateObject<UHansaDefinitionBase>(VersionFifteenDefinitions[Index], GetTransientPackage()));
		if (auto* Recipe = Cast<UHansaRecipeDefinition>(Previous.Get()))
		{
			Recipe->Inputs = bMill ? TArray<FHansaGoodAmount>{ { TEXT("Good.Grain"), 4'000 } } :
				bBakery ? TArray<FHansaGoodAmount>{ { TEXT("Good.Flour"), 2'000 } } :
				TArray<FHansaGoodAmount>{};
			Recipe->Outputs = bFarm ? TArray<FHansaGoodAmount>{ { TEXT("Good.Grain"), 6'000 } } :
				bMill ? TArray<FHansaGoodAmount>{ { TEXT("Good.Flour"), 3'000 } } :
				bBakery ? TArray<FHansaGoodAmount>{ { TEXT("Good.Bread"), 3'000 } } :
				TArray<FHansaGoodAmount>{ { TEXT("Good.Fish"), 4'000 } };
			Recipe->LaborerWorkforce = 1;
			Recipe->ArtisanWorkforce = 0;
			Recipe->AuthoredRevision = bBakery ? 3 : 2;
		}
		if (auto* Building = Cast<UHansaBuildingDefinition>(Previous.Get()))
		{
			Building->ConstructionCosts = bFarm
				? TArray<FHansaGoodAmount>{ { TEXT("Good.Timber"), 3'000 }, { TEXT("Good.Tools"), 500 } }
				: bMill
				? TArray<FHansaGoodAmount>{ { TEXT("Good.Timber"), 4'000 }, { TEXT("Good.Planks"), 3'000 },
					{ TEXT("Good.Tools"), 1'000 } }
				: bBakery
				? TArray<FHansaGoodAmount>{ { TEXT("Good.Planks"), 4'000 }, { TEXT("Good.Tools"), 1'000 } }
				: TArray<FHansaGoodAmount>{ { TEXT("Good.Timber"), 5'000 }, { TEXT("Good.Planks"), 2'000 },
					{ TEXT("Good.Tools"), 500 } };
			Building->ConstructionCostPfennig = bFarm ? 1'200 : bMill ? 1'600 : bBakery ? 1'400 : 1'500;
			Building->LaborerWorkforce = 1;
			Building->ArtisanWorkforce = 0;
			Building->AuthoredRevision = bFarm ? 4 : bMill ? 3 : bBakery ? 4 : 4;
		}
		Previous->RefreshContentHash();
		VersionFifteenDefinitions[Index] = Previous.Get();
		VersionFifteenOwned.Add(MoveTemp(Previous));
	}
	const auto VersionFifteen = FHansaEconomicDefinitionCompiler::Compile(VersionFifteenDefinitions);
	TestTrue(TEXT("Catalog v15 reconstruction compiles"), VersionFifteen.IsValid());
	TestEqual(TEXT("Catalog v16 changes only the eight reviewed staple capacity assets"),
		VersionFifteen.Registry.GetRegistryHash(), FHansaLubeckScenarioInitializer::PreviousStarterBalanceMvpRegistryHash);

	TArray<const UHansaDefinitionBase*> VersionFourteenDefinitions = VersionFifteenDefinitions;
	TStrongObjectPtr<UHansaBuildingDefinition> VersionFourteenFishery;
	for (int32 Index = 0; Index < VersionFourteenDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionFourteenDefinitions[Index]);
		if (!Building || Building->StableDefinitionId != TEXT("Building.Fishery")) continue;
		TestEqual(TEXT("Approved Fishery mesh is the runtime presentation"),
			Building->PresentationMesh.ToSoftObjectPath().ToString(),
			FString(TEXT("/Game/Mesh/hansa-fishery/SM_HansaFishery.SM_HansaFishery")));
		TestNotNull(TEXT("Approved Fishery mesh resolves from saved content"), Building->LoadPresentationMesh());
		VersionFourteenFishery.Reset(DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		VersionFourteenFishery->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		VersionFourteenFishery->AuthoredRevision = 3;
		VersionFourteenFishery->RefreshContentHash();
		VersionFourteenDefinitions[Index] = VersionFourteenFishery.Get();
		break;
	}
	const auto VersionFourteen = FHansaEconomicDefinitionCompiler::Compile(VersionFourteenDefinitions);
	TestTrue(TEXT("Catalog v14 reconstruction compiles"), VersionFourteen.IsValid());
	TestEqual(TEXT("Catalog v15 changes only Fishery presentation and authored revision"),
		VersionFourteen.Registry.GetRegistryHash(), FHansaLubeckScenarioInitializer::PreviousFisheryPresentationMvpRegistryHash);

	TArray<const UHansaDefinitionBase*> VersionThirteenDefinitions = VersionFourteenDefinitions;
	TStrongObjectPtr<UHansaBuildingDefinition> VersionThirteenBrewery;
	for (int32 Index = 0; Index < VersionThirteenDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionThirteenDefinitions[Index]);
		if (!Building || Building->StableDefinitionId != TEXT("Building.Brewery")) continue;
		TestEqual(TEXT("Approved brewery mesh is the runtime presentation"),
			Building->PresentationMesh.ToSoftObjectPath().ToString(),
			FString(TEXT("/Game/Mesh/hansa-brewery-huexstrasse128/Production/SM_HansaBrewery_Production.SM_HansaBrewery_Production")));
		TestNotNull(TEXT("Approved brewery mesh resolves from saved content"), Building->LoadPresentationMesh());
		VersionThirteenBrewery.Reset(DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		VersionThirteenBrewery->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		VersionThirteenBrewery->AuthoredRevision = 3;
		VersionThirteenBrewery->FootprintHeightCells = 3;
		VersionThirteenBrewery->RefreshContentHash();
		VersionThirteenDefinitions[Index] = VersionThirteenBrewery.Get();
		break;
	}
	const auto VersionThirteen = FHansaEconomicDefinitionCompiler::Compile(VersionThirteenDefinitions);
	TestTrue(TEXT("Catalog v13 reconstruction compiles"), VersionThirteen.IsValid());
	TestEqual(TEXT("Catalog v14 changes only Brewery presentation, footprint and authored revision"),
		VersionThirteen.Registry.GetRegistryHash(), FHansaLubeckScenarioInitializer::PreviousPresentationMvpRegistryHash);
	TArray<const UHansaDefinitionBase*> VersionTwelveDefinitions = VersionThirteenDefinitions;
	const TSet<FString> VersionThirteenOnlyIds = {
		TEXT("Good.Hops"), TEXT("Good.Malt"), TEXT("Good.Barrels"),
		TEXT("Recipe.GrowHops"), TEXT("Recipe.MaltGrain"), TEXT("Recipe.MakeBarrels"),
		TEXT("Building.HopFarm"), TEXT("Building.MaltHouse"), TEXT("Building.Cooperage")
	};
	VersionTwelveDefinitions.RemoveAll([&VersionThirteenOnlyIds](const UHansaDefinitionBase* Definition)
		{ return VersionThirteenOnlyIds.Contains(Definition->StableDefinitionId); });
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> VersionTwelveOwned;
	for (int32 Index = 0; Index < VersionTwelveDefinitions.Num(); ++Index)
	{
		const FString Id = VersionTwelveDefinitions[Index]->StableDefinitionId;
		if (Id == TEXT("Recipe.BrewBeer"))
		{
			TStrongObjectPtr<UHansaDefinitionBase> Previous(DuplicateObject<UHansaRecipeDefinition>(
				CastChecked<UHansaRecipeDefinition>(VersionTwelveDefinitions[Index]), GetTransientPackage()));
			auto* Recipe = CastChecked<UHansaRecipeDefinition>(Previous.Get());
			Recipe->DisplayName = FText::ChangeKey(
				TEXT(""),
				TEXT("DC839DC3404DDA952C2B8B9A2FB65CD1"),
				FText::FromString(TEXT("Brew beer")));
			FHansaGoodAmount Grain;
			Grain.GoodId = TEXT("Good.Grain");
			Grain.QuantityMilliUnits = 3000;
			Recipe->Inputs = { Grain };
			Recipe->AuthoredRevision = 1;
			Recipe->RefreshContentHash();
			VersionTwelveDefinitions[Index] = Recipe;
			VersionTwelveOwned.Add(MoveTemp(Previous));
		}
		else if (Id == TEXT("Building.Brewery"))
		{
			TStrongObjectPtr<UHansaDefinitionBase> Previous(DuplicateObject<UHansaBuildingDefinition>(
				CastChecked<UHansaBuildingDefinition>(VersionTwelveDefinitions[Index]), GetTransientPackage()));
			auto* Building = CastChecked<UHansaBuildingDefinition>(Previous.Get());
			Building->ConstructionMenuOrder = 0;
			Building->ConstructionPresentationPurpose = FText::GetEmpty();
			Building->ConstructionChainStage = 1;
			Building->ConstructionChainStageCount = 1;
			Building->AuthoredRevision = 2;
			Building->RefreshContentHash();
			VersionTwelveDefinitions[Index] = Building;
			VersionTwelveOwned.Add(MoveTemp(Previous));
		}
		else if (const auto* CurrentCity = Cast<UHansaCityMarketProfileDefinition>(VersionTwelveDefinitions[Index]))
		{
			TStrongObjectPtr<UHansaDefinitionBase> Previous(DuplicateObject<UHansaCityMarketProfileDefinition>(
				CurrentCity, GetTransientPackage()));
			auto* City = CastChecked<UHansaCityMarketProfileDefinition>(Previous.Get());
			City->Goods.RemoveAll([](const FHansaMarketGoodProfile& Profile)
				{ return Profile.GoodId == TEXT("Good.Hops") || Profile.GoodId == TEXT("Good.Malt") || Profile.GoodId == TEXT("Good.Barrels"); });
			City->AuthoredRevision = FMath::Max(1, City->AuthoredRevision - 1);
			City->RefreshContentHash();
			VersionTwelveDefinitions[Index] = City;
			VersionTwelveOwned.Add(MoveTemp(Previous));
		}
	}
	const auto VersionTwelve = FHansaEconomicDefinitionCompiler::Compile(VersionTwelveDefinitions);
	TestTrue(TEXT("Catalog v12 reconstruction compiles"), VersionTwelve.IsValid());
	Hansa::Editor::Tests::FReviewedEconomicCatalog ReviewedVersionTwelve;
	FString VersionTwelveManifestError;
	if (Hansa::Editor::Tests::LoadReviewedEconomicCatalog(
		ReviewedVersionTwelve, VersionTwelveManifestError, TEXT("economic_catalog_v12.json")) &&
		VersionTwelve.Registry.GetRegistryHash() != ReviewedVersionTwelve.RegistryHash)
	{
		AddError(VersionTwelve.DescribeRegistryHashMismatch(
			ReviewedVersionTwelve.RegistryHash, ReviewedVersionTwelve.Definitions));
	}
	TestEqual(TEXT("Catalog v13 is isolated to the expanded Beer chain"),
		VersionTwelve.Registry.GetRegistryHash(), uint64(0xC31520B6DB3A6E09ULL));

	TArray<const UHansaDefinitionBase*> VersionElevenDefinitions = VersionTwelveDefinitions;
	TStrongObjectPtr<UHansaBuildingDefinition> VersionElevenBrewery;
	for (int32 Index = 0; Index < VersionElevenDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionElevenDefinitions[Index]);
		if (!Building || Building->StableDefinitionId != TEXT("Building.Brewery")) continue;
		VersionElevenBrewery.Reset(DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		VersionElevenBrewery->AuthoredRevision = 1;
		VersionElevenBrewery->bShowInConstructionMenu = false;
		VersionElevenBrewery->ConstructionMenuCategory = EHansaConstructionMenuCategory::Production;
		VersionElevenBrewery->ConstructionMenuOrder = 0;
		VersionElevenBrewery->ConstructionChainOutputGoodId.Reset();
		VersionElevenBrewery->ConstructionChainStage = 0;
		VersionElevenBrewery->ConstructionChainStageCount = 0;
		VersionElevenBrewery->RefreshContentHash();
		VersionElevenDefinitions[Index] = VersionElevenBrewery.Get();
		break;
	}
	const auto VersionEleven = FHansaEconomicDefinitionCompiler::Compile(VersionElevenDefinitions);
	TestTrue(TEXT("Catalog v11 reconstruction compiles"), VersionEleven.IsValid());
	TestEqual(TEXT("Only the Brewery construction-card contract changes catalog v11"),
		VersionEleven.Registry.GetRegistryHash(), uint64(0x4170F53E6E9BC675ULL));

	TArray<const UHansaDefinitionBase*> VersionTenDefinitions = VersionElevenDefinitions;
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> VersionTenOwned;
	for (int32 Index = 0; Index < VersionTenDefinitions.Num(); ++Index)
	{
		const FString Id = VersionTenDefinitions[Index]->StableDefinitionId;
		const bool bFarm = Id.EndsWith(TEXT("GrainFarm")) || Id.EndsWith(TEXT("GrowGrain"));
		const bool bMill = Id == TEXT("Building.Mill") || Id == TEXT("Recipe.MillFlour");
		const bool bBakery = Id == TEXT("Building.Bakery") || Id == TEXT("Recipe.BakeBread");
		const bool bFish = Id == TEXT("Building.Fishery") || Id == TEXT("Recipe.CatchFish");
		if (!bFarm && !bMill && !bBakery && !bFish && Id != TEXT("PopulationTier.Laborer")) continue;
		TStrongObjectPtr<UHansaDefinitionBase> Previous(DuplicateObject<UHansaDefinitionBase>(VersionTenDefinitions[Index], GetTransientPackage()));
		--Previous->AuthoredRevision;
		if (auto* Recipe = Cast<UHansaRecipeDefinition>(Previous.Get()))
		{
			Recipe->AuthoredRevision = 1;
			Recipe->CycleTicks = bFarm ? 120 : bMill ? 60 : bBakery ? 45 : 90;
			Recipe->LaborerWorkforce = (bFarm || bFish) ? 8 : 4;
			Recipe->ArtisanWorkforce = bMill ? 1 : bBakery ? 2 : 0;
		}
		if (auto* Building = Cast<UHansaBuildingDefinition>(Previous.Get()))
		{
			Building->LaborerWorkforce = (bFarm || bFish) ? 8 : 4;
			Building->ArtisanWorkforce = bMill ? 1 : bBakery ? 2 : 0;
		}
		if (auto* Tier = Cast<UHansaPopulationTierDefinition>(Previous.Get()))
		{
			for (auto& Need : Tier->Needs)
			{
				Need.ConsumptionMilliUnitsPerResidentPerTick *= 10;
				Need.ImportanceBasisPoints = Need.NeedId == TEXT("Need.Bread") ? 4000 :
					Need.NeedId == TEXT("Need.Fish") ? 2500 : Need.NeedId == TEXT("Need.Beer") ? 1500 : 2000;
			}
			Tier->GrowthSatisfactionBasisPoints = 8000;
			Tier->DeclineSatisfactionBasisPoints = 3500;
			Tier->EvaluationTicks = 60;
		}
		Previous->RefreshContentHash();
		VersionTenDefinitions[Index] = Previous.Get();
		VersionTenOwned.Add(MoveTemp(Previous));
	}
	const auto VersionTen = FHansaEconomicDefinitionCompiler::Compile(VersionTenDefinitions);
	TestTrue(TEXT("Catalog v10 reconstruction compiles"), VersionTen.IsValid());
	TestEqual(TEXT("Only the nine reviewed starter balance assets change v10"), VersionTen.Registry.GetRegistryHash(), uint64(0x547A8E4FB77941CDULL));
	TArray<const UHansaDefinitionBase*> VersionNineDefinitions = VersionTenDefinitions;
	TArray<TStrongObjectPtr<UHansaBuildingDefinition>> VersionNineBuildings;
	for (int32 Index = 0; Index < VersionNineDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionNineDefinitions[Index]);
		if (!Building) continue;
		TStrongObjectPtr<UHansaBuildingDefinition> Previous(DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		Previous->SchemaVersion = 4;
		if (Previous->StableDefinitionId == TEXT("Building.Fishery"))
		{
			Previous->bRequiresRoad = false;
			Previous->AuthoredRevision = 1;
		}
		Previous->RefreshContentHash();
		VersionNineDefinitions[Index] = Previous.Get();
		VersionNineBuildings.Add(MoveTemp(Previous));
	}
	const auto VersionNine = FHansaEconomicDefinitionCompiler::Compile(VersionNineDefinitions);
	TestTrue(TEXT("Catalog v9 reconstruction compiles"), VersionNine.IsValid());
	TestEqual(TEXT("Only schema v5 and the Fishery road contract change catalog v9"), VersionNine.Registry.GetRegistryHash(),
		FHansaLubeckScenarioInitializer::PreviousMvpRegistryHash);

	TArray<const UHansaDefinitionBase*> VersionEightDefinitions = VersionNineDefinitions;
	TArray<TStrongObjectPtr<UHansaBuildingDefinition>> VersionEightBuildings;
	for (int32 Index = 0; Index < VersionEightDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionEightDefinitions[Index]);
		if (!Building) continue;
		TStrongObjectPtr<UHansaBuildingDefinition> Previous(DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		Previous->SchemaVersion = 3;
		Previous->bProvidesMarketAccess = false;
		Previous->RefreshContentHash();
		VersionEightDefinitions[Index] = Previous.Get();
		VersionEightBuildings.Add(MoveTemp(Previous));
	}
	const auto VersionEight = FHansaEconomicDefinitionCompiler::Compile(VersionEightDefinitions);
	TestTrue(TEXT("Catalog v8 reconstruction compiles"), VersionEight.IsValid());
	TestEqual(TEXT("Only the authored physical-market capability changes catalog v8"), VersionEight.Registry.GetRegistryHash(),
		FHansaLubeckScenarioInitializer::LegacyMvpRegistryHash);

	TArray<const UHansaDefinitionBase*> VersionSevenDefinitions = VersionEightDefinitions;
	TArray<TStrongObjectPtr<UHansaVehicleDefinition>> PreviousVehicles;
	for (int32 Index = 0; Index < VersionSevenDefinitions.Num(); ++Index)
	{
		const auto* Vehicle = Cast<UHansaVehicleDefinition>(VersionSevenDefinitions[Index]);
		if (!Vehicle) continue;
		TestNotNull(TEXT("Promoted vehicle actor resolves"), Vehicle->LoadPresentationActorClass());
		TStrongObjectPtr<UHansaVehicleDefinition> Previous(DuplicateObject<UHansaVehicleDefinition>(Vehicle, GetTransientPackage()));
		Previous->AuthoredRevision = 1;
		Previous->PresentationActorClass.Reset();
		Previous->PresentationMesh.Reset();
		VersionSevenDefinitions[Index] = Previous.Get();
		PreviousVehicles.Add(MoveTemp(Previous));
	}
	const auto VersionSeven = FHansaEconomicDefinitionCompiler::Compile(VersionSevenDefinitions);
	TestTrue(TEXT("Catalog v7 reconstruction compiles"), VersionSeven.IsValid());
	TestEqual(TEXT("Only P19 vehicle bindings change catalog v7"), VersionSeven.Registry.GetRegistryHash(),
		FHansaLubeckScenarioInitializer::OlderMvpRegistryHash);
	TArray<const UHansaDefinitionBase*> VersionSixDefinitions = VersionSevenDefinitions;
	TStrongObjectPtr<UHansaBuildingDefinition> VersionSixRoad;
	for (int32 Index = 0; Index < VersionSixDefinitions.Num(); ++Index)
	{
		const auto* Road = Cast<UHansaBuildingDefinition>(VersionSixDefinitions[Index]);
		if (!Road || Road->StableDefinitionId != TEXT("Building.Road")) continue;
		TestNotNull(TEXT("Promoted Road actor resolves"), Road->LoadPresentationActorClass());
		TestNotNull(TEXT("Promoted Road mesh resolves"), Road->LoadPresentationMesh());
		VersionSixRoad.Reset(DuplicateObject<UHansaBuildingDefinition>(Road, GetTransientPackage()));
		VersionSixRoad->AuthoredRevision = 1;
		VersionSixRoad->PresentationActorClass.Reset();
		VersionSixRoad->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		VersionSixDefinitions[Index] = VersionSixRoad.Get();
	}
	const auto VersionSix = FHansaEconomicDefinitionCompiler::Compile(VersionSixDefinitions);
	TestTrue(TEXT("Catalog v6 reconstruction compiles"), VersionSix.IsValid());
	TestEqual(TEXT("Only the P18 Road binding changes catalog v6"), VersionSix.Registry.GetRegistryHash(),
		uint64(0x483D86D8C5549199ULL));
	TArray<const UHansaDefinitionBase*> VersionFiveDefinitions = VersionSixDefinitions;
	TStrongObjectPtr<UHansaBuildingDefinition> VersionFiveDock;
	for (int32 Index = 0; Index < VersionFiveDefinitions.Num(); ++Index)
	{
		const auto* Dock = Cast<UHansaBuildingDefinition>(VersionFiveDefinitions[Index]);
		if (!Dock || Dock->StableDefinitionId != TEXT("Building.Dock")) continue;
		TestNotNull(TEXT("Promoted Dock actor resolves"), Dock->LoadPresentationActorClass());
		TestNotNull(TEXT("Promoted Dock mesh resolves"), Dock->LoadPresentationMesh());
		VersionFiveDock.Reset(DuplicateObject<UHansaBuildingDefinition>(Dock, GetTransientPackage()));
		VersionFiveDock->AuthoredRevision = 1;
		VersionFiveDock->PresentationActorClass.Reset();
		VersionFiveDock->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		VersionFiveDefinitions[Index] = VersionFiveDock.Get();
	}
	const auto VersionFive = FHansaEconomicDefinitionCompiler::Compile(VersionFiveDefinitions);
	TestTrue(TEXT("Catalog v5 reconstruction compiles"), VersionFive.IsValid());
	TestEqual(TEXT("Only the P17 Dock binding changes catalog v5"), VersionFive.Registry.GetRegistryHash(),
		uint64(0xDAA463F7043FD66FULL));
	TArray<const UHansaDefinitionBase*> VersionFourDefinitions = VersionFiveDefinitions;
	TArray<TStrongObjectPtr<UHansaBuildingDefinition>> VersionFourBuildings;
	for (int32 Index = 0; Index < VersionFourDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionFourDefinitions[Index]);
		if (!Building || (Building->StableDefinitionId != TEXT("Building.LumberCamp") &&
			Building->StableDefinitionId != TEXT("Building.Sawmill") &&
			Building->StableDefinitionId != TEXT("Building.Market") &&
			Building->StableDefinitionId != TEXT("Building.Residence.Laborer") &&
			Building->StableDefinitionId != TEXT("Building.Residence.Artisan"))) continue;
		TestNotNull(TEXT("Approved presentation actor resolves"), Building->LoadPresentationActorClass());
		TestNotNull(TEXT("Approved presentation mesh resolves"), Building->LoadPresentationMesh());
		TStrongObjectPtr<UHansaBuildingDefinition> Previous(DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		Previous->AuthoredRevision = 1;
		Previous->PresentationActorClass.Reset();
		Previous->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			Building->StableDefinitionId == TEXT("Building.Residence.Laborer")
			? TEXT("/Game/Mesh/LaborerResidence/Materials_R02/Meshes/SM_LaborerResidence.SM_LaborerResidence")
			: TEXT("/Engine/BasicShapes/Cube.Cube")));
		VersionFourDefinitions[Index] = Previous.Get();
		VersionFourBuildings.Add(MoveTemp(Previous));
	}
	const auto VersionFour = FHansaEconomicDefinitionCompiler::Compile(VersionFourDefinitions);
	TestTrue(TEXT("Catalog v4 reconstruction compiles"), VersionFour.IsValid());
	TestEqual(TEXT("Reversing only P12-P15 presentation changes reproduces v4"),
		VersionFour.Registry.GetRegistryHash(), 0xDA77AC921DFB6DC0ULL);
	TArray<const UHansaDefinitionBase*> VersionThreeDefinitions = VersionFourDefinitions;
	TArray<TStrongObjectPtr<UHansaBuildingDefinition>> VersionThreeBuildings;
	for (int32 Index = 0; Index < VersionThreeDefinitions.Num(); ++Index)
	{
		const auto* Building = Cast<UHansaBuildingDefinition>(VersionThreeDefinitions[Index]);
		if (!Building || (Building->StableDefinitionId != TEXT("Building.GrainFarm") &&
			Building->StableDefinitionId != TEXT("Building.Bakery"))) continue;
		TStrongObjectPtr<UHansaBuildingDefinition> Previous(
			DuplicateObject<UHansaBuildingDefinition>(Building, GetTransientPackage()));
		Previous->PresentationActorClass.Reset();
		if (Building->StableDefinitionId == TEXT("Building.GrainFarm"))
		{
			Previous->AuthoredRevision = 1;
			Previous->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		}
		else
		{
			Previous->AuthoredRevision = 2;
			Previous->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
				TEXT("/Game/Mesh/hansa-bakery/Meshes/SM_HansaBakery.SM_HansaBakery")));
		}
		VersionThreeDefinitions[Index] = Previous.Get();
		VersionThreeBuildings.Add(MoveTemp(Previous));
	}
	const auto VersionThree = FHansaEconomicDefinitionCompiler::Compile(VersionThreeDefinitions);
	TestTrue(TEXT("Catalog v3 reconstruction compiles"), VersionThree.IsValid());
	TestEqual(TEXT("Reversing only approved farm/bakery presentation changes reproduces v3"),
		VersionThree.Registry.GetRegistryHash(), 0xD8DB2585B867453BULL);
	TArray<const UHansaDefinitionBase*> VersionTwoDefinitions = VersionThreeDefinitions;
	TArray<TStrongObjectPtr<UHansaBuildingDefinition>> VersionTwoBuildings;
	for (int32 Index = 0; Index < VersionTwoDefinitions.Num(); ++Index)
	{
		const UHansaBuildingDefinition* AuthoredBuilding = Cast<UHansaBuildingDefinition>(VersionTwoDefinitions[Index]);
		if (AuthoredBuilding == nullptr) continue;
		TStrongObjectPtr<UHansaBuildingDefinition> VersionTwoBuilding(
			DuplicateObject<UHansaBuildingDefinition>(AuthoredBuilding, GetTransientPackage()));
		// Catalog v2's building assets still carried the original serialized schema value 1;
		// the class-level schema metadata was 2 for the compatible actor-reference addition.
		VersionTwoBuilding->SchemaVersion = 1;
		VersionTwoBuilding->bShowInConstructionMenu = false;
		VersionTwoBuilding->ConstructionMenuCategory = EHansaConstructionMenuCategory::Production;
		VersionTwoBuilding->ConstructionMenuOrder = 0;
		VersionTwoBuilding->ConstructionChainOutputGoodId.Reset();
		VersionTwoBuilding->ConstructionChainStage = 0;
		VersionTwoBuilding->ConstructionChainStageCount = 0;
		VersionTwoBuilding->RequiredConstructionTechnologyId.Reset();
		VersionTwoBuilding->bUpgradeOnly = false;
		VersionTwoBuilding->ConstructionPresentationPurpose = FText::GetEmpty();
		VersionTwoDefinitions[Index] = VersionTwoBuilding.Get();
		VersionTwoBuildings.Add(MoveTemp(VersionTwoBuilding));
	}
	const FHansaEconomicRegistryCompileResult VersionTwo =
		FHansaEconomicDefinitionCompiler::Compile(VersionTwoDefinitions);
	TestTrue(TEXT("Catalog v2 shape still compiles for lineage verification"), VersionTwo.IsValid());
	TestEqual(TEXT("Clearing the P03 presentation schema reproduces catalog v2"),
		VersionTwo.Registry.GetRegistryHash(), 0x97F691C37A6A4BFFULL);
	const FString VersionReport = CompileResult.DescribeRegistryHashMismatch(
		VersionTwo.Registry.GetRegistryHash(), VersionTwo.DefinitionHashes);
	TestTrue(TEXT("Catalog lineage report identifies changed building definitions"),
		VersionReport.Contains(TEXT("changed Building.Mill")) && VersionReport.Contains(TEXT("changed Building.Road")));
	TestFalse(TEXT("Catalog lineage report does not implicate unrelated definitions"),
		VersionReport.Contains(TEXT("Good.Grain")));
	TestEqual(TEXT("Reloaded goods count"), CompileResult.Registry.GetGoods().Num(), 20);
	TestEqual(TEXT("Reloaded recipes count"), CompileResult.Registry.GetRecipes().Num(), 16);
	TestEqual(TEXT("Reloaded buildings including compound stages"), CompileResult.Registry.GetBuildings().Num(), 35);
	TestEqual(TEXT("Reloaded technology count"), CompileResult.Registry.GetTechnologies().Num(), 9);
	TestEqual(TEXT("Reloaded merchant AI tuning count"), CompileResult.Registry.GetMerchantAITunings().Num(), 1);
	TestEqual(TEXT("Reloaded scenario objective count"), CompileResult.Registry.GetScenarioObjectives().Num(), 11);
	TestEqual(TEXT("Reloaded victory path count"), CompileResult.Registry.GetVictories().Num(), 3);
	TestEqual(TEXT("Reloaded scenario count"), CompileResult.Registry.GetScenarios().Num(), 1);
	for (const Hansa::Simulation::FHansaCompiledBuildingDefinition& Building : CompileResult.Registry.GetBuildings())
	{
		TestTrue(*FString::Printf(TEXT("Reloaded %s retains its S06 currency cost"), *Building.StableId),
			Building.ConstructionCostPfennig > 0);
		TestEqual(*FString::Printf(TEXT("Reloaded %s retains its bounded refund policy"), *Building.StableId),
			Building.CancellationRefundBasisPoints, 5000);
	}
	const auto* LaborerResidence = CompileResult.Registry.FindBuilding(TEXT("Building.Residence.Laborer"));
	const auto* ArtisanResidence = CompileResult.Registry.FindBuilding(TEXT("Building.Residence.Artisan"));
	const UHansaBuildingDefinition* AuthoredLaborerResidence = nullptr;
	for (const UHansaDefinitionBase* Definition : LoadedDefinitions)
	{
		if (Definition->StableDefinitionId == TEXT("Building.Residence.Laborer"))
		{
			AuthoredLaborerResidence = Cast<UHansaBuildingDefinition>(Definition);
			break;
		}
	}
	TestTrue(TEXT("Reloaded laborer residence retains its hosted tier"), LaborerResidence != nullptr &&
		LaborerResidence->ResidentPopulationTierId == TEXT("PopulationTier.Laborer"));
	TestTrue(TEXT("Reloaded laborer residence retains the approved presentation mesh"),
		AuthoredLaborerResidence != nullptr &&
		AuthoredLaborerResidence->PresentationMesh.ToSoftObjectPath().ToString() ==
			TEXT("/Game/Mesh/hansa-residences/Meshes_R07/SM_Residence_Laborer_A.SM_Residence_Laborer_A"));
	TestTrue(TEXT("Reloaded artisan residence retains its hosted tier"), ArtisanResidence != nullptr &&
		ArtisanResidence->ResidentPopulationTierId == TEXT("PopulationTier.Artisan"));
	TestTrue(TEXT("Reloaded residence progression remains direct and authored"), LaborerResidence != nullptr &&
		LaborerResidence->UpgradeTargetBuildingId == TEXT("Building.Residence.Artisan"));
	TestEqual(TEXT("Reloaded needs count"), CompileResult.Registry.GetNeeds().Num(), 7);
	TestEqual(TEXT("Reloaded population tier count"), CompileResult.Registry.GetPopulationTiers().Num(), 2);
	TestEqual(TEXT("Reloaded city market profile count"), CompileResult.Registry.GetCityMarkets().Num(), 4);
	TestEqual(TEXT("Reloaded vehicle count"), CompileResult.Registry.GetVehicles().Num(), 2);
	TestEqual(TEXT("Reloaded route count"), CompileResult.Registry.GetRoutes().Num(), 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPopulationDefinitionValidationTest,
	"Hansa.Content.Definitions.PopulationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationDefinitionValidationTest::RunTest(const FString& Parameters)
{
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaNeedDefinition* Bread = CastChecked<UHansaNeedDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Need.Bread")));
		Bread->GoodId = TEXT("Good.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Missing need good fails closed"), Result.IsValid());
		TestTrue(TEXT("Missing need good diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-011")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaPopulationTierDefinition* Laborer = CastChecked<UHansaPopulationTierDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("PopulationTier.Laborer")));
		Laborer->Needs[0].NeedId = TEXT("Need.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Missing tier need fails closed"), Result.IsValid());
		TestTrue(TEXT("Missing tier need diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-012")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaPopulationTierDefinition* Laborer = CastChecked<UHansaPopulationTierDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("PopulationTier.Laborer")));
		Laborer->Needs.Last().ConsumptionMilliUnitsPerResidentPerTick = 1;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Service inventory consumption fails closed"), Result.IsValid());
		TestTrue(TEXT("Impossible consumption diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-013")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaPopulationTierDefinition* Laborer = CastChecked<UHansaPopulationTierDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("PopulationTier.Laborer")));
		UHansaPopulationTierDefinition* Artisan = CastChecked<UHansaPopulationTierDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("PopulationTier.Artisan")));
		Laborer->PreviousTierId = Artisan->StableDefinitionId;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Tier progression cycle fails closed"), Result.IsValid());
		TestTrue(TEXT("Tier cycle diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-015")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaBuildingDefinition* LaborerResidence = CastChecked<UHansaBuildingDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Residence.Laborer")));
		LaborerResidence->ResidentPopulationTierId = TEXT("PopulationTier.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Missing residence tier fails closed"), Result.IsValid());
		TestTrue(TEXT("Residence-tier diagnostic is stable"),
			Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-019")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaBuildingDefinition* ArtisanResidence = CastChecked<UHansaBuildingDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Building.Residence.Artisan")));
		ArtisanResidence->FootprintWidthCells = 3;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Residence upgrade footprint mismatch fails closed"), Result.IsValid());
		TestTrue(TEXT("Residence-progression diagnostic is stable"),
			Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-020")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMarketDefinitionValidationTest,
	"Hansa.Content.Definitions.MarketValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketDefinitionValidationTest::RunTest(const FString& Parameters)
{
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaCityMarketProfileDefinition* Lubeck = CastChecked<UHansaCityMarketProfileDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("City.Lubeck")));
		Lubeck->Goods[0].GoodId = TEXT("Good.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Missing city-market good fails closed"), Result.IsValid());
		TestTrue(TEXT("Missing market good diagnostic is stable"),
			Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-017")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaCityMarketProfileDefinition* Hamburg = CastChecked<UHansaCityMarketProfileDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("City.Hamburg")));
		Hamburg->StaleAfterTicks = Hamburg->UpdateCadenceTicks - 1;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Impossible stale-report threshold fails closed"), Result.IsValid());
		TestTrue(TEXT("Market settings diagnostic is stable"), Result.Issues.ContainsByPredicate(
			[](const FHansaDefinitionValidationIssue& Issue) { return Issue.Code.ToString() == TEXT("HSA-MARKET-002"); }));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEconomicProductionGraphValidationTest,
	"Hansa.Content.Definitions.ProductionGraphValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEconomicProductionGraphValidationTest::RunTest(const FString& Parameters)
{
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaRecipeDefinition* Brewery = CastChecked<UHansaRecipeDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.BrewBeer")));
		Brewery->Outputs.Add({ TEXT("Good.Malt"), 1'000 });
		const FHansaEconomicRegistryCompileResult Result = FHansaEconomicDefinitionCompiler::Compile(
			Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Ambiguous same-good recipe fails conservation validation"), Result.IsValid());
		TestTrue(TEXT("Conservation diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-008")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaRecipeDefinition* Grain = CastChecked<UHansaRecipeDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.GrowGrain")));
		Grain->Inputs.Add({ TEXT("Good.Flour"), 1'000 });
		Grain->bDeclaredSource = false;
		const FHansaEconomicRegistryCompileResult Result = FHansaEconomicDefinitionCompiler::Compile(
			Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Closed production cycle fails graph validation"), Result.IsValid());
		TestTrue(TEXT("Cycle diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-009")));
		TestTrue(TEXT("Reachability diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-REGISTRY-010")));
	}
	{
		TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
			Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaRecipeDefinition* Fish = CastChecked<UHansaRecipeDefinition>(
			Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Recipe.CatchFish")));
		Fish->bDeclaredSource = false;
		const FHansaEconomicRegistryCompileResult Result = FHansaEconomicDefinitionCompiler::Compile(
			Hansa::Editor::Tests::RawDefinitions(Definitions));
		TestFalse(TEXT("Implicit source fails boundary validation"), Result.IsValid());
		TestTrue(TEXT("Source/sink diagnostic is stable"), Hansa::Editor::Tests::ContainsIssueCode(Result, TEXT("HSA-RECIPE-004")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEconomicTransactionsTest,
	"Hansa.Integration.Authoring.EconomicTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEconomicTransactionsTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Editor transaction system is available"), GEditor) || GEditor->Trans == nullptr)
	{
		return false;
	}

	TStrongObjectPtr<UHansaGoodDefinition> Good(NewObject<UHansaGoodDefinition>(GetTransientPackage()));
	Good->SetFlags(RF_Transactional);
	const int64 InitialGoodValue = Good->BaseValueMilliMarks;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("HansaEconomicTests", "EditGood", "Edit Hansa good"));
		Good->Modify();
		Good->BaseValueMilliMarks = 2400;
	}
	TestTrue(TEXT("Good edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Good undo restores value"), Good->BaseValueMilliMarks, InitialGoodValue);
	TestTrue(TEXT("Good edit can be redone"), GEditor->RedoTransaction());
	TestEqual(TEXT("Good redo restores edit"), Good->BaseValueMilliMarks, static_cast<int64>(2400));

	TStrongObjectPtr<UHansaRecipeDefinition> Recipe(NewObject<UHansaRecipeDefinition>(GetTransientPackage()));
	Recipe->SetFlags(RF_Transactional);
	const int32 InitialCycle = Recipe->CycleTicks;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("HansaEconomicTests", "EditRecipe", "Edit Hansa recipe"));
		Recipe->Modify();
		Recipe->CycleTicks = 99;
	}
	TestTrue(TEXT("Recipe edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Recipe undo restores cycle"), Recipe->CycleTicks, InitialCycle);
	TestTrue(TEXT("Recipe edit can be redone"), GEditor->RedoTransaction());
	TestEqual(TEXT("Recipe redo restores edit"), Recipe->CycleTicks, 99);

	TStrongObjectPtr<UHansaBuildingDefinition> Building(NewObject<UHansaBuildingDefinition>(GetTransientPackage()));
	Building->SetFlags(RF_Transactional);
	const int32 InitialCapacity = Building->StorageCapacityMilliUnits;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("HansaEconomicTests", "EditBuilding", "Edit Hansa building"));
		Building->Modify();
		Building->StorageCapacityMilliUnits = 125000;
	}
	TestTrue(TEXT("Building edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Building undo restores capacity"), Building->StorageCapacityMilliUnits, InitialCapacity);
	TestTrue(TEXT("Building edit can be redone"), GEditor->RedoTransaction());
	TestEqual(TEXT("Building redo restores edit"), Building->StorageCapacityMilliUnits, 125000);
	Building->bRequiresRoad = true;
	Building->StorageCapacityMilliUnits = 1000;
	const bool InitialMarketAccess = Building->bProvidesMarketAccess;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("HansaEconomicTests", "EditMarketAccess", "Edit Hansa market access"));
		Building->Modify();
		Building->bProvidesMarketAccess = true;
	}
	TestTrue(TEXT("Market-access edit can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Market-access undo restores value"), Building->bProvidesMarketAccess, InitialMarketAccess);
	TestTrue(TEXT("Market-access edit can be redone"), GEditor->RedoTransaction());
	TestTrue(TEXT("Market-access redo restores edit"), Building->bProvidesMarketAccess);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMvpResearchCatalogueTest,
	"Hansa.Content.Research.MvpCatalogueAndGraphValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMvpResearchCatalogueTest::RunTest(const FString& Parameters)
{
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	const FHansaEconomicRegistryCompileResult Compiled = FHansaEconomicDefinitionCompiler::Compile(
		Hansa::Editor::Tests::RawDefinitions(Definitions));
	TestTrue(TEXT("The accepted MVP catalogue compiles"), Compiled.IsValid());
	TestEqual(TEXT("The MVP contains exactly nine technologies"), Compiled.Registry.GetTechnologies().Num(), 9);
	for (const Hansa::Simulation::EHansaResearchBranch Branch : {
		Hansa::Simulation::EHansaResearchBranch::Commerce,
		Hansa::Simulation::EHansaResearchBranch::Production,
		Hansa::Simulation::EHansaResearchBranch::Logistics})
	{
		TestEqual(TEXT("Each branch contains exactly three technologies"),
			Compiled.Registry.GetTechnologies().FilterByPredicate([Branch](const auto& Technology)
			{
				return Technology.Branch == Branch;
			}).Num(), 3);
	}

	UHansaTechnologyDefinition* RouteScheduling = CastChecked<UHansaTechnologyDefinition>(
		Hansa::Editor::Tests::FindDefinition(Definitions, TEXT("Technology.Logistics.RouteScheduling")));
	RouteScheduling->PrerequisiteTechnologyIds = {TEXT("Technology.Missing")};
	const FHansaEconomicRegistryCompileResult Broken = FHansaEconomicDefinitionCompiler::Compile(
		Hansa::Editor::Tests::RawDefinitions(Definitions));
	TestFalse(TEXT("Missing research nodes fail compilation"), Broken.IsValid());
	TestTrue(TEXT("Missing prerequisite diagnostic is stable"),
		Hansa::Editor::Tests::ContainsIssueCode(Broken, TEXT("HSA-REGISTRY-022")));
	return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketRangeAuthoringTest,
 "Hansa.Editor.Definitions.MarketRangeAuthoring",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaMarketRangeAuthoringTest::RunTest(const FString& Parameters)
{
 auto* B=NewObject<UHansaBuildingDefinition>();
 TestEqual(TEXT("Legacy/new assets inherit finite default"),B->MaximumMarketRoadDistanceCells,40);
 FHansaEditorSchemaRegistry Schemas;
 const auto Schema=Schemas.BuildSchemaForClass(UHansaBuildingDefinition::StaticClass());
 TestTrue(TEXT("Range has complete reflected authoring metadata"),Schema.IsValid());
 TestTrue(TEXT("Generated AI schema includes road-distance limit"),Schemas.ExportJsonSchema(Schema).Contains(TEXT("MaximumMarketRoadDistanceCells")));
 const uint64 Before=B->ComputeDeterministicContentHash();
 B->MaximumMarketRoadDistanceCells=39;
 TestNotEqual(TEXT("Authoring a range changes deterministic content"),B->ComputeDeterministicContentHash(),Before);
 B->MaximumMarketRoadDistanceCells=1;
 TArray<FHansaDefinitionValidationIssue> Issues;B->ValidateDefinition(Issues);
 TestTrue(TEXT("Invalid range has actionable diagnostic"),Issues.ContainsByPredicate([](const auto& I){return I.Code==TEXT("HSA-BUILDING-MARKET-RANGE");}));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPreservationAuthoringValidation,
 "Hansa.Content.Definitions.PreservedFish.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPreservationAuthoringValidation::RunTest(const FString&)
{
 using namespace Hansa::Editor::Tests;
 for (int32 Case = 0; Case < 7; ++Case)
 {
  auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
  auto* Need = CastChecked<UHansaNeedDefinition>(FindDefinition(Definitions,TEXT("Need.Fish")));
  auto* Recipe = CastChecked<UHansaRecipeDefinition>(FindDefinition(Definitions,TEXT("Recipe.SaltedCatch")));
  auto* Building = CastChecked<UHansaBuildingDefinition>(FindDefinition(Definitions,TEXT("Building.Fishery.SaltingShed")));
  if (Case == 1) { const FHansaNeedAlternative Duplicate = Need->Alternatives[0]; Need->Alternatives.Add(Duplicate); }
  if (Case == 2) Need->Alternatives[0].FulfillmentBasisPoints = 0;
  if (Case == 3) Need->Alternatives[0].GoodId = TEXT("Good.Missing");
  if (Case == 4) Recipe->Outputs[0].QuantityMilliUnits = 20001;
  if (Case == 5) Recipe->Inputs.RemoveAt(0);
  if (Case == 6) ++Building->FootprintWidthCells;
  const auto Result = FHansaEconomicDefinitionCompiler::Compile(RawDefinitions(Definitions));
  TestEqual(*FString::Printf(TEXT("Preservation authoring case %d validity"),Case),Result.IsValid(),Case==0);
 }
 FHansaEditorSchemaRegistry Schemas;
 for (const auto* Class : {UHansaGoodDefinition::StaticClass(),UHansaRecipeDefinition::StaticClass(),UHansaNeedDefinition::StaticClass()})
 {
  const auto Schema = Schemas.BuildSchemaForClass(Class);
  TestTrue(TEXT("Extended reflected schema is valid"),Schema.IsValid());
  const FString Json = Schemas.ExportJsonSchema(Schema);
  const TCHAR* Field = Class==UHansaGoodDefinition::StaticClass()?TEXT("bSpoilageEnabled"):Class==UHansaRecipeDefinition::StaticClass()?TEXT("InternalCatchRecipeId"):TEXT("Alternatives");
  TestTrue(TEXT("New gameplay field is exported to authoring/AI contract"),Json.Contains(Field));
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPreservationImpactTest,
 "Hansa.Content.Definitions.PreservedFish.ImpactAnalysis", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPreservationImpactTest::RunTest(const FString&)
{
 const auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
 auto Raw = Hansa::Editor::Tests::RawDefinitions(Definitions);
 const auto Fish = Hansa::Editor::EconomicDefinitions::DescribeEconomicImpact(TEXT("Good.PreservedFish"), Raw);
 TestTrue(TEXT("Preserved fish edits expose alternative consumption dependency"), Fish.Contains(TEXT("Need.Fish.Alternatives")));
 TestTrue(TEXT("Preserved fish edits expose salted output dependency"), Fish.Contains(TEXT("Recipe.SaltedCatch.Outputs")));
 const auto Catch = Hansa::Editor::EconomicDefinitions::DescribeEconomicImpact(TEXT("Recipe.CatchFish"), Raw);
 TestTrue(TEXT("Internal catch dependency is visible before editing source output"), Catch.Contains(TEXT("Recipe.SaltedCatch.InternalCatchRecipeId")));
 const auto Shed = Hansa::Editor::EconomicDefinitions::DescribeEconomicImpact(TEXT("Building.Fishery.SaltingShed"), Raw);
 TestTrue(TEXT("Upgrade target exposes source fishery dependency"), Shed.Contains(TEXT("Building.Fishery.UpgradeTargetBuildingId")));
 Algo::Reverse(Raw);
 TestTrue(TEXT("Impact analysis is discovery-order independent"), Fish == Hansa::Editor::EconomicDefinitions::DescribeEconomicImpact(TEXT("Good.PreservedFish"), Raw));
 return !HasAnyErrors();
}

#endif
