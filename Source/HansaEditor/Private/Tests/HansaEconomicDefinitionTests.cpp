#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Editor.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Editor::Tests
{
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
		UHansaCityMarketProfileDefinition::StaticClass() })
	{
		const FHansaDefinitionClassSchema* Schema = Registry.FindSchema(DefinitionClass);
		TestNotNull(*FString::Printf(TEXT("%s is discovered by the generic schema registry"), *DefinitionClass->GetName()), Schema);
		if (Schema != nullptr)
		{
			TestTrue(*FString::Printf(TEXT("%s metadata is complete"), *DefinitionClass->GetName()), Schema->IsValid());
			TestTrue(TEXT("Every economic schema exports reflected properties"), !Schema->Properties.IsEmpty());
		}
	}
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
	TestEqual(TEXT("MVP goods count"), First.Registry.GetGoods().Num(), 10);
	TestEqual(TEXT("MVP recipes count"), First.Registry.GetRecipes().Num(), 8);
	TestEqual(TEXT("MVP buildings count"), First.Registry.GetBuildings().Num(), 14);
	TestEqual(TEXT("MVP needs count"), First.Registry.GetNeeds().Num(), 5);
	TestEqual(TEXT("MVP population tiers count"), First.Registry.GetPopulationTiers().Num(), 2);
	TestEqual(TEXT("MVP city market profiles count"), First.Registry.GetCityMarkets().Num(), 4);
	TestTrue(TEXT("Registry hash is non-zero"), First.Registry.GetRegistryHash() != 0);
	TestNotNull(TEXT("Good.Grain is queryable by stable ID"), First.Registry.FindGood(TEXT("Good.Grain")));
	TestNotNull(TEXT("Recipe.BrewBeer is queryable by stable ID"), First.Registry.FindRecipe(TEXT("Recipe.BrewBeer")));
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

	Algo::Reverse(Forward);
	const FHansaEconomicRegistryCompileResult Reversed = FHansaEconomicDefinitionCompiler::Compile(Forward);
	TestTrue(TEXT("Reordered source definitions compile"), Reversed.IsValid());
	TestEqual(TEXT("Registry hash is independent of asset discovery order"), Reversed.Registry.GetRegistryHash(), First.Registry.GetRegistryHash());

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
	TestEqual(TEXT("All authored MVP definition assets reload from disk"), LoadedDefinitions.Num(), 43);
	const FHansaEconomicRegistryCompileResult CompileResult = FHansaEconomicDefinitionCompiler::Compile(LoadedDefinitions);
	TestTrue(TEXT("Reloaded production assets compile"), CompileResult.IsValid());
	TestEqual(TEXT("Headless fixture version pins the authored on-disk economic registry"),
		CompileResult.Registry.GetRegistryHash(), Hansa::Simulation::FHansaProductionFixture::RegistryHash);
	TestEqual(TEXT("Reloaded goods count"), CompileResult.Registry.GetGoods().Num(), 10);
	TestEqual(TEXT("Reloaded recipes count"), CompileResult.Registry.GetRecipes().Num(), 8);
	TestEqual(TEXT("Reloaded buildings count"), CompileResult.Registry.GetBuildings().Num(), 14);
	for (const Hansa::Simulation::FHansaCompiledBuildingDefinition& Building : CompileResult.Registry.GetBuildings())
	{
		TestTrue(*FString::Printf(TEXT("Reloaded %s retains its S06 currency cost"), *Building.StableId),
			Building.ConstructionCostPfennig > 0);
		TestEqual(*FString::Printf(TEXT("Reloaded %s retains its bounded refund policy"), *Building.StableId),
			Building.CancellationRefundBasisPoints, 5000);
	}
	const auto* LaborerResidence = CompileResult.Registry.FindBuilding(TEXT("Building.Residence.Laborer"));
	const auto* ArtisanResidence = CompileResult.Registry.FindBuilding(TEXT("Building.Residence.Artisan"));
	TestTrue(TEXT("Reloaded laborer residence retains its hosted tier"), LaborerResidence != nullptr &&
		LaborerResidence->ResidentPopulationTierId == TEXT("PopulationTier.Laborer"));
	TestTrue(TEXT("Reloaded artisan residence retains its hosted tier"), ArtisanResidence != nullptr &&
		ArtisanResidence->ResidentPopulationTierId == TEXT("PopulationTier.Artisan"));
	TestTrue(TEXT("Reloaded residence progression remains direct and authored"), LaborerResidence != nullptr &&
		LaborerResidence->UpgradeTargetBuildingId == TEXT("Building.Residence.Artisan"));
	TestEqual(TEXT("Reloaded needs count"), CompileResult.Registry.GetNeeds().Num(), 5);
	TestEqual(TEXT("Reloaded population tier count"), CompileResult.Registry.GetPopulationTiers().Num(), 2);
	TestEqual(TEXT("Reloaded city market profile count"), CompileResult.Registry.GetCityMarkets().Num(), 4);
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
		Brewery->Outputs.Add({ TEXT("Good.Grain"), 1'000 });
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
	return !HasAnyErrors();
}

#endif
