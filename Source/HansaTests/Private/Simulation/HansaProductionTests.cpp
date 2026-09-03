#include "Algo/Reverse.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaDeterminismTrace.h"
#include "Fixtures/HansaProductionFixture.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Production
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue Require(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TId>
	TId Entity(const uint64 Value)
	{
		return Require(TId::TryCreate(Value));
	}

	FHansaGoodId Good(const TCHAR* Value)
	{
		return Require(FHansaGoodId::TryParse(Value));
	}

	FHansaRecipeId Recipe(const TCHAR* Value)
	{
		return Require(FHansaRecipeId::TryParse(Value));
	}

	FHansaBuildingTypeId BuildingType(const TCHAR* Value)
	{
		return Require(FHansaBuildingTypeId::TryParse(Value));
	}

	FHansaSimulationTick Tick(const int64 Value)
	{
		return Require(FHansaSimulationTick::TryCreate(Value));
	}

	FHansaCompiledGoodAmount Amount(const TCHAR* GoodId, const int64 Quantity)
	{
		return { GoodId, Quantity };
	}

	FHansaCompiledRecipeDefinition RecipeDefinition(
		const TCHAR* StableId,
		TArray<FHansaCompiledGoodAmount> Inputs,
		TArray<FHansaCompiledGoodAmount> Outputs,
		const int32 CycleTicks,
		const int32 Laborers,
		const int32 Artisans)
	{
		FHansaCompiledRecipeDefinition Result;
		Result.StableId = StableId;
		Result.Inputs = MoveTemp(Inputs);
		Result.Outputs = MoveTemp(Outputs);
		Result.CycleTicks = CycleTicks;
		Result.LaborerWorkforce = Laborers;
		Result.ArtisanWorkforce = Artisans;
		Result.bDeclaredSource = Result.Inputs.IsEmpty();
		return Result;
	}

	FHansaCompiledBuildingDefinition BuildingDefinition(
		const TCHAR* StableId,
		const TCHAR* RecipeId,
		const int32 Laborers,
		const int32 Artisans)
	{
		FHansaCompiledBuildingDefinition Result;
		Result.StableId = StableId;
		Result.RecipeIds.Add(RecipeId);
		Result.LaborerWorkforce = Laborers;
		Result.ArtisanWorkforce = Artisans;
		return Result;
	}

	FHansaEconomicRegistry MakeRegistry()
	{
		TArray<FHansaCompiledGoodDefinition> Goods;
		for (const TCHAR* StableId : {
			TEXT("Good.Beer"), TEXT("Good.Bread"), TEXT("Good.Fish"), TEXT("Good.Flour"), TEXT("Good.Grain"),
			TEXT("Good.Iron"), TEXT("Good.Planks"), TEXT("Good.Salt"), TEXT("Good.Timber"), TEXT("Good.Tools") })
		{
			FHansaCompiledGoodDefinition Definition;
			Definition.StableId = StableId;
			Definition.Unit = TEXT("milli-unit");
			Goods.Add(MoveTemp(Definition));
		}

		TArray<FHansaCompiledRecipeDefinition> Recipes = {
			RecipeDefinition(TEXT("Recipe.BakeBread"), { Amount(TEXT("Good.Flour"), 2'000) }, { Amount(TEXT("Good.Bread"), 3'000) }, 2, 4, 2),
			RecipeDefinition(TEXT("Recipe.BrewBeer"), { Amount(TEXT("Good.Grain"), 3'000) }, { Amount(TEXT("Good.Beer"), 5'000) }, 2, 4, 2),
			RecipeDefinition(TEXT("Recipe.CatchFish"), {}, { Amount(TEXT("Good.Fish"), 4'000) }, 2, 8, 0),
			RecipeDefinition(TEXT("Recipe.FellTimber"), {}, { Amount(TEXT("Good.Timber"), 6'000) }, 2, 8, 0),
			RecipeDefinition(TEXT("Recipe.GrowGrain"), {}, { Amount(TEXT("Good.Grain"), 6'000) }, 3, 8, 0),
			RecipeDefinition(TEXT("Recipe.MillFlour"), { Amount(TEXT("Good.Grain"), 4'000) }, { Amount(TEXT("Good.Flour"), 3'000) }, 2, 4, 1),
			RecipeDefinition(TEXT("Recipe.SawPlanks"), { Amount(TEXT("Good.Timber"), 5'000) }, { Amount(TEXT("Good.Planks"), 3'500) }, 2, 6, 1),
			RecipeDefinition(TEXT("Recipe.SmithTools"), { Amount(TEXT("Good.Iron"), 3'000) }, { Amount(TEXT("Good.Tools"), 1'000) }, 2, 4, 4)
		};

		TArray<FHansaCompiledBuildingDefinition> Buildings = {
			BuildingDefinition(TEXT("Building.Bakery"), TEXT("Recipe.BakeBread"), 4, 2),
			BuildingDefinition(TEXT("Building.Brewery"), TEXT("Recipe.BrewBeer"), 4, 2),
			BuildingDefinition(TEXT("Building.Fishery"), TEXT("Recipe.CatchFish"), 8, 0),
			BuildingDefinition(TEXT("Building.GrainFarm"), TEXT("Recipe.GrowGrain"), 8, 0),
			BuildingDefinition(TEXT("Building.LumberCamp"), TEXT("Recipe.FellTimber"), 8, 0),
			BuildingDefinition(TEXT("Building.Mill"), TEXT("Recipe.MillFlour"), 4, 1),
			BuildingDefinition(TEXT("Building.Sawmill"), TEXT("Recipe.SawPlanks"), 6, 1),
			BuildingDefinition(TEXT("Building.Smithy"), TEXT("Recipe.SmithTools"), 4, 4)
		};
		return FHansaEconomicRegistry(MoveTemp(Goods), MoveTemp(Recipes), MoveTemp(Buildings), 0x3122334455667788ULL);
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		const FHansaScenarioId Scenario = Require(
			FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")));
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Scenario, 0x3122334455667788ULL, MakeRegistry()));
	}

	FHansaInventoryStockInitialization Stock(const TCHAR* GoodId, const int64 Quantity)
	{
		return { Good(GoodId), FHansaQuantity::FromRaw(Quantity) };
	}

	FHansaProductionInitialization BuildingProduction(
		const uint64 ProductionValue,
		const uint64 BuildingValue,
		const TCHAR* RecipeId,
		const int32 Laborers,
		const int32 Artisans,
		const bool bActive = true)
	{
		FHansaProductionInitialization Result;
		Result.Id = Entity<FHansaProductionId>(ProductionValue);
		Result.Kind = EHansaProductionKind::BuildingRecipe;
		Result.BuildingId = Entity<FHansaBuildingId>(BuildingValue);
		Result.RecipeId = Recipe(RecipeId);
		Result.InputInventoryId = Entity<FHansaInventoryId>(1);
		Result.OutputInventoryId = Entity<FHansaInventoryId>(1);
		Result.AllocatedLaborerWorkforce = Laborers;
		Result.AllocatedArtisanWorkforce = Artisans;
		Result.bActive = bActive;
		return Result;
	}

	FHansaSimulationState MakeFullState(const bool bReverseDiscovery = false)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Tick(0)));
		Initialization.CampaignSeed = 0x33445566;
		Initialization.Houses.Add({ Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(100'000) });
		const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		Initialization.Cities.Add({ City, FHansaQuantity() });

		const TCHAR* BuildingIds[] = {
			TEXT("Building.GrainFarm"), TEXT("Building.Mill"), TEXT("Building.Bakery"), TEXT("Building.LumberCamp"),
			TEXT("Building.Sawmill"), TEXT("Building.Smithy"), TEXT("Building.Brewery"), TEXT("Building.Fishery")
		};
		for (uint64 Index = 0; Index < UE_ARRAY_COUNT(BuildingIds); ++Index)
		{
			Initialization.Buildings.Add({
				Entity<FHansaBuildingId>(Index + 1), BuildingType(BuildingIds[Index]), Entity<FHansaHouseId>(1),
				FHansaRate::FromPartsPerMillion(FHansaRate::Scale) });
		}

		FHansaInventoryInitialization Inventory;
		Inventory.Id = Entity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(2'000'000);
		Inventory.AcceptedGoods = {
			Good(TEXT("Good.Grain")), Good(TEXT("Good.Flour")), Good(TEXT("Good.Bread")), Good(TEXT("Good.Fish")),
			Good(TEXT("Good.Salt")), Good(TEXT("Good.Timber")), Good(TEXT("Good.Planks")), Good(TEXT("Good.Iron")),
			Good(TEXT("Good.Tools")), Good(TEXT("Good.Beer")) };
		Inventory.InitialStock = { Stock(TEXT("Good.Iron"), 60'000) };
		Initialization.Inventories.Add(MoveTemp(Inventory));

		Initialization.Productions = {
			BuildingProduction(1, 1, TEXT("Recipe.GrowGrain"), 8, 0),
			BuildingProduction(2, 2, TEXT("Recipe.MillFlour"), 4, 1),
			BuildingProduction(3, 3, TEXT("Recipe.BakeBread"), 4, 2),
			BuildingProduction(4, 4, TEXT("Recipe.FellTimber"), 8, 0),
			BuildingProduction(5, 5, TEXT("Recipe.SawPlanks"), 6, 1),
			BuildingProduction(6, 6, TEXT("Recipe.SmithTools"), 4, 4),
			BuildingProduction(7, 7, TEXT("Recipe.BrewBeer"), 4, 2),
			BuildingProduction(8, 8, TEXT("Recipe.CatchFish"), 8, 0)
		};
		FHansaProductionInitialization Salt;
		Salt.Id = Entity<FHansaProductionId>(9);
		Salt.Kind = EHansaProductionKind::BackgroundSupply;
		Salt.CityId = City;
		Salt.SupplyGoodId = Good(TEXT("Good.Salt"));
		Salt.SupplyQuantityPerCycle = FHansaQuantity::FromRaw(2'000);
		Salt.SupplyCycleTicks = 3;
		Salt.OutputInventoryId = Entity<FHansaInventoryId>(1);
		Initialization.Productions.Add(Salt);

		if (bReverseDiscovery)
		{
			Algo::Reverse(Initialization.Buildings);
			Algo::Reverse(Initialization.Productions);
			Algo::Reverse(Initialization.Inventories[0].AcceptedGoods);
		}
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}

	FHansaCommandGatewayResult Step(
		FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationTransientCache& Cache)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache);
	}

	int64 StockQuantity(
		const FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions,
		const TCHAR* GoodId)
	{
		const TOptional<FHansaInventoryStockProjection> StockProjection =
			State.CreateReadOnlyAccess(Definitions).GetInventories().QueryStock(
				Entity<FHansaInventoryId>(1), Good(GoodId));
		return StockProjection.IsSet() ? StockProjection->Stock.GetRawValue() : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionFixedTickTest,
	"Hansa.Simulation.Production.FixedTicksAndCausalThroughput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionFixedTickTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Production;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeFullState();
	FHansaSimulationTransientCache Cache;

	const FHansaCommandGatewayResult First = Step(State, Definitions, Cache);
	TestTrue(TEXT("First production tick succeeds"), First.IsSuccess());
	const TOptional<FHansaProductionProjection> MillAfterOne =
		State.CreateReadOnlyAccess(Definitions).QueryProduction(Entity<FHansaProductionId>(2));
	TestTrue(TEXT("Typed production query resolves mill"), MillAfterOne.IsSet());
	if (MillAfterOne.IsSet())
	{
		TestEqual(TEXT("Mill waits for grain on the first tick"), MillAfterOne->Blocker, EHansaProductionBlocker::MissingInput);
		TestEqual(TEXT("Causal projection identifies missing grain"), MillAfterOne->BlockingGoodId, Good(TEXT("Good.Grain")));
		TestEqual(TEXT("Nominal flour output is projected"), MillAfterOne->Outputs[0].NominalQuantityPerCycle.GetRawValue(), int64(3'000));
		TestEqual(TEXT("Blocked mill has zero actual output"), MillAfterOne->Outputs[0].ActualQuantityLastTick.GetRawValue(), int64(0));
	}

	Step(State, Definitions, Cache);
	const FHansaCommandGatewayResult Third = Step(State, Definitions, Cache);
	TestTrue(TEXT("Third production tick succeeds"), Third.IsSuccess());
	bool bFoundGrainCompletion = false;
	for (const FHansaDomainEvent& Event : Third.GetEvents())
	{
		bFoundGrainCompletion |= Event.GetType() == EHansaDomainEventType::ProductionCycleCompleted &&
			Event.GetProductionId() == Entity<FHansaProductionId>(1);
	}
	TestTrue(TEXT("Grain source emits a typed completion event"), bFoundGrainCompletion);
	const TOptional<FHansaProductionProjection> GrainFarm =
		State.CreateReadOnlyAccess(Definitions).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Grain farm projection remains available"), GrainFarm.IsSet());
	if (GrainFarm.IsSet())
	{
		TestEqual(TEXT("Completed source reports nominal actual output"), GrainFarm->Outputs[0].ActualQuantityLastTick.GetRawValue(), int64(6'000));
		TestEqual(TEXT("Completed source resets fixed-tick progress"), GrainFarm->ProgressTicks, 0);
	}
	TestEqual(TEXT("Owning simulation snapshots include canonical production state"),
		State.CreateReadOnlyAccess(Definitions).CaptureSnapshot().GetProductions().GetProductions().Num(), 9);
	TestEqual(TEXT("Mill reserves its exact grain input when it can start"),
		State.CreateReadOnlyAccess(Definitions).GetInventories().QueryStock(
			Entity<FHansaInventoryId>(1), Good(TEXT("Good.Grain")))->Reserved.GetRawValue(), int64(4'000));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionChainsLongRunTest,
	"Hansa.Simulation.Production.MvpChainsLongRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionChainsLongRunTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Production;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState Forward = MakeFullState(false);
	FHansaSimulationState Reversed = MakeFullState(true);
	FHansaSimulationTransientCache ForwardCache;
	FHansaSimulationTransientCache ReversedCache;
	for (int32 Index = 0; Index < 1'000; ++Index)
	{
		const FHansaCommandGatewayResult Left = Step(Forward, Definitions, ForwardCache);
		const FHansaCommandGatewayResult Right = Step(Reversed, Definitions, ReversedCache);
		TestTrue(TEXT("Long-run forward tick succeeds"), Left.IsSuccess());
		TestTrue(TEXT("Long-run reversed tick succeeds"), Right.IsSuccess());
		TestEqual(TEXT("Equivalent discovery orders emit equal event counts"), Left.GetEvents().Num(), Right.GetEvents().Num());
		TestEqual(TEXT("Equivalent discovery orders emit identical typed event order"),
			FHansaDeterminismDiagnostics::ComputeDomainEventOrderHash(Left.GetEvents()),
			FHansaDeterminismDiagnostics::ComputeDomainEventOrderHash(Right.GetEvents()));
		TestEqual(TEXT("Equivalent discovery orders retain identical hashes"),
			Left.GetFingerprintAfter().Value, Right.GetFingerprintAfter().Value);
	}
	for (const TCHAR* ProducedGood : {
		TEXT("Good.Bread"), TEXT("Good.Planks"), TEXT("Good.Tools"), TEXT("Good.Beer"), TEXT("Good.Fish"), TEXT("Good.Salt") })
	{
		TestTrue(FString::Printf(TEXT("MVP production produces %s"), ProducedGood),
			StockQuantity(Forward, Definitions, ProducedGood) > 0);
	}
	const FHansaSubsystemStateHash* ProductionHash =
		Forward.CreateReadOnlyAccess(Definitions).BuildStateHashReport().Find(EHansaStateHashSubsystem::Productions);
	TestNotNull(TEXT("Production has a dedicated authoritative subsystem hash"), ProductionHash);
	TestEqual(TEXT("Canonical production projections survive reordered discovery"),
		Forward.CreateReadOnlyAccess(Definitions).BuildProductionProjection().Num(), 9);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionBlockersTest,
	"Hansa.Simulation.Production.BlockersAndAtomicStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionBlockersTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Production;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeFullState();
	FHansaSimulationTransientCache Cache;
	for (int32 Index = 0; Index < 2'000; ++Index)
	{
		TestTrue(TEXT("Capacity-boundary setup tick succeeds"), Step(State, Definitions, Cache).IsSuccess());
	}
	const TArray<FHansaProductionProjection> Projections =
		State.CreateReadOnlyAccess(Definitions).BuildProductionProjection();
	TestEqual(TEXT("Every production keeps a causal projection"), Projections.Num(), 9);
	TestTrue(TEXT("Finite storage eventually produces an explicit storage blocker"), Projections.ContainsByPredicate(
		[](const FHansaProductionProjection& Projection)
		{
			return Projection.Blocker == EHansaProductionBlocker::StorageBlocked;
		}));
	for (const FHansaInventoryProjection& Inventory : State.CreateReadOnlyAccess(Definitions).GetInventories().BuildProjection())
	{
		TestTrue(TEXT("Production never exceeds inventory capacity"),
			Inventory.UsedCapacity.GetRawValue() <= Inventory.Capacity.GetRawValue());
		for (const FHansaInventoryStockProjection& StockProjection : Inventory.Stocks)
		{
			TestTrue(TEXT("Production never creates negative stock"), StockProjection.Stock.GetRawValue() >= 0);
			TestTrue(TEXT("Production reservations never exceed stock"),
				StockProjection.Reserved.GetRawValue() <= StockProjection.Stock.GetRawValue());
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionWorkforceAndInactiveTest,
	"Hansa.Simulation.Production.WorkforceInactiveAndConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionWorkforceAndInactiveTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Production;
	FHansaSimulationInitialization Initialization;
	Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
		Require(FHansaSimulationVersion::TryCreate(1)), Tick(0)));
	Initialization.Houses.Add({ Entity<FHansaHouseId>(1), FHansaMoney() });
	const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
	Initialization.Cities.Add({ City, FHansaQuantity() });
	Initialization.Buildings = {
		{ Entity<FHansaBuildingId>(1), BuildingType(TEXT("Building.GrainFarm")), Entity<FHansaHouseId>(1), FHansaRate() },
		{ Entity<FHansaBuildingId>(2), BuildingType(TEXT("Building.Mill")), Entity<FHansaHouseId>(1), FHansaRate::FromPartsPerMillion(FHansaRate::Scale) },
		{ Entity<FHansaBuildingId>(3), BuildingType(TEXT("Building.Fishery")), Entity<FHansaHouseId>(1), FHansaRate::FromPartsPerMillion(FHansaRate::Scale) }
	};
	FHansaInventoryInitialization Inventory;
	Inventory.Id = Entity<FHansaInventoryId>(1);
	Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
	Inventory.CityId = City;
	Inventory.Capacity = FHansaQuantity::FromRaw(100'000);
	Inventory.AcceptedGoods = { Good(TEXT("Good.Grain")), Good(TEXT("Good.Flour")), Good(TEXT("Good.Fish")) };
	Inventory.InitialStock = { Stock(TEXT("Good.Grain"), 10'000) };
	Initialization.Inventories.Add(Inventory);
	Initialization.Productions = {
		BuildingProduction(1, 1, TEXT("Recipe.GrowGrain"), 8, 0),
		BuildingProduction(2, 2, TEXT("Recipe.MillFlour"), 0, 0),
		BuildingProduction(3, 3, TEXT("Recipe.CatchFish"), 8, 0, false)
	};
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	const FHansaCommandGatewayResult Result = Step(State, Definitions, Cache);
	TestTrue(TEXT("Blocker tick succeeds"), Result.IsSuccess());
	const FHansaSimulationReadOnlyAccess Access = State.CreateReadOnlyAccess(Definitions);
	TestEqual(TEXT("Incomplete construction blocks production"),
		Access.QueryProduction(Entity<FHansaProductionId>(1))->Blocker, EHansaProductionBlocker::ConstructionIncomplete);
	TestEqual(TEXT("Insufficient workforce is explicit"),
		Access.QueryProduction(Entity<FHansaProductionId>(2))->Blocker, EHansaProductionBlocker::InsufficientLaborerWorkforce);
	TestEqual(TEXT("Inactive production is explicit"),
		Access.QueryProduction(Entity<FHansaProductionId>(3))->Blocker, EHansaProductionBlocker::Inactive);
	TestEqual(TEXT("Each first blocker transition emits one typed event"), Result.GetEvents().Num(), 3);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionContextValidationTest,
	"Hansa.Simulation.Production.DefinitionContextValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionContextValidationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Production;
	const FHansaScenarioId Scenario = Require(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")));
	TestFalse(TEXT("Economic registry hash must match the immutable definition context hash"),
		FHansaSimulationDefinitionContext::TryCreate(Scenario, 1, MakeRegistry()).IsSuccess());
	FHansaSimulationInitialization Invalid;
	Invalid.Clock = Require(FHansaSimulationClock::TryCreate(
		Require(FHansaSimulationVersion::TryCreate(1)), Tick(0)));
	Invalid.Productions.Add(BuildingProduction(1, 99, TEXT("Recipe.MillFlour"), 4, 1));
	TestFalse(TEXT("Production cannot reference a missing building or inventory"),
		FHansaSimulationState::TryCreate(MoveTemp(Invalid)).IsSuccess());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionBoundaryAtomicityTest,
	"Hansa.Simulation.Production.BoundaryAndMultiInputAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionBoundaryAtomicityTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Production;
	const auto MakeMultiInputRegistry = []()
	{
		TArray<FHansaCompiledGoodDefinition> Goods;
		for (const TCHAR* StableId : { TEXT("Good.Bread"), TEXT("Good.Grain"), TEXT("Good.Salt") })
		{
			FHansaCompiledGoodDefinition Definition;
			Definition.StableId = StableId;
			Definition.Unit = TEXT("milli-unit");
			Goods.Add(Definition);
		}
		FHansaCompiledRecipeDefinition RecipeValue = RecipeDefinition(
			TEXT("Recipe.BakeBread"),
			{ Amount(TEXT("Good.Grain"), 1'000), Amount(TEXT("Good.Salt"), 500) },
			{ Amount(TEXT("Good.Bread"), 1'600) },
			1, 1, 0);
		FHansaCompiledBuildingDefinition BuildingValue = BuildingDefinition(
			TEXT("Building.Bakery"), TEXT("Recipe.BakeBread"), 1, 0);
		return FHansaEconomicRegistry(
			MoveTemp(Goods), { MoveTemp(RecipeValue) }, { MoveTemp(BuildingValue) }, 0x445566778899AABBULL);
	};
	const FHansaScenarioId Scenario = Require(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")));
	const FHansaSimulationDefinitionContext Definitions = Require(FHansaSimulationDefinitionContext::TryCreate(
		Scenario, 0x445566778899AABBULL, MakeMultiInputRegistry()));

	const auto MakeState = [](const int64 SaltQuantity, const int64 Capacity)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Tick(0)));
		Initialization.Houses.Add({ Entity<FHansaHouseId>(1), FHansaMoney() });
		const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		Initialization.Cities.Add({ City, FHansaQuantity() });
		Initialization.Buildings.Add({
			Entity<FHansaBuildingId>(1), BuildingType(TEXT("Building.Bakery")), Entity<FHansaHouseId>(1),
			FHansaRate::FromPartsPerMillion(FHansaRate::Scale) });
		FHansaInventoryInitialization Inventory;
		Inventory.Id = Entity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(Capacity);
		Inventory.AcceptedGoods = { Good(TEXT("Good.Bread")), Good(TEXT("Good.Grain")), Good(TEXT("Good.Salt")) };
		Inventory.InitialStock = { Stock(TEXT("Good.Grain"), 1'000), Stock(TEXT("Good.Salt"), SaltQuantity) };
		Initialization.Inventories.Add(Inventory);
		Initialization.Productions.Add(BuildingProduction(1, 1, TEXT("Recipe.BakeBread"), 1, 0));
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	};

	FHansaSimulationState MissingSecondInput = MakeState(499, 1'500);
	FHansaSimulationTransientCache MissingCache;
	TestTrue(TEXT("Missing-input boundary tick succeeds"), Step(MissingSecondInput, Definitions, MissingCache).IsSuccess());
	const FHansaSimulationReadOnlyAccess MissingAccess = MissingSecondInput.CreateReadOnlyAccess(Definitions);
	TestEqual(TEXT("Second missing input is reported causally"),
		MissingAccess.QueryProduction(Entity<FHansaProductionId>(1))->BlockingGoodId, Good(TEXT("Good.Salt")));
	TestEqual(TEXT("Failed multi-input reservation leaves the first input unreserved"),
		MissingAccess.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Good(TEXT("Good.Grain")))->Reserved.GetRawValue(),
		int64(0));

	FHansaSimulationState BlockedOutput = MakeState(500, 1'500);
	FHansaSimulationTransientCache BlockedCache;
	TestTrue(TEXT("Storage-blocked boundary tick succeeds"), Step(BlockedOutput, Definitions, BlockedCache).IsSuccess());
	const FHansaSimulationReadOnlyAccess BlockedAccess = BlockedOutput.CreateReadOnlyAccess(Definitions);
	TestEqual(TEXT("Output one unit beyond effective capacity is blocked"),
		BlockedAccess.QueryProduction(Entity<FHansaProductionId>(1))->Blocker, EHansaProductionBlocker::StorageBlocked);
	TestEqual(TEXT("Storage failure consumes no grain"), StockQuantity(BlockedOutput, Definitions, TEXT("Good.Grain")), int64(1'000));
	TestEqual(TEXT("Storage failure consumes no salt"), StockQuantity(BlockedOutput, Definitions, TEXT("Good.Salt")), int64(500));
	TestEqual(TEXT("Storage failure creates no partial output"), StockQuantity(BlockedOutput, Definitions, TEXT("Good.Bread")), int64(0));
	TestEqual(TEXT("Storage-blocked recipe retains all input reservations for retry"),
		BlockedAccess.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Good(TEXT("Good.Grain")))->Reserved.GetRawValue() +
		BlockedAccess.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Good(TEXT("Good.Salt")))->Reserved.GetRawValue(),
		int64(1'500));

	FHansaSimulationState ExactBoundary = MakeState(500, 1'600);
	FHansaSimulationTransientCache ExactCache;
	const FHansaCommandGatewayResult ExactResult = Step(ExactBoundary, Definitions, ExactCache);
	TestTrue(TEXT("Exact input/output capacity boundary succeeds atomically"), ExactResult.IsSuccess());
	TestEqual(TEXT("All grain input is consumed"), StockQuantity(ExactBoundary, Definitions, TEXT("Good.Grain")), int64(0));
	TestEqual(TEXT("All salt input is consumed"), StockQuantity(ExactBoundary, Definitions, TEXT("Good.Salt")), int64(0));
	TestEqual(TEXT("Exact-capacity bread output is committed"), StockQuantity(ExactBoundary, Definitions, TEXT("Good.Bread")), int64(1'600));
	const FHansaInventoryProjection BoundaryInventory =
		ExactBoundary.CreateReadOnlyAccess(Definitions).GetInventories().BuildProjection()[0];
	TestEqual(TEXT("Exact-capacity recipe finishes at the capacity boundary"),
		BoundaryInventory.UsedCapacity.GetRawValue(), BoundaryInventory.Capacity.GetRawValue());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProductionFixtureGoldenEvidenceTest,
	"Hansa.Integration.Production.HeadlessFixtureGoldenEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProductionFixtureGoldenEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	const THansaValueResult<FHansaProductionFixture> FirstCreated = FHansaProductionFixture::TryCreate();
	const THansaValueResult<FHansaProductionFixture> SecondCreated = FHansaProductionFixture::TryCreate();
	if (!TestTrue(TEXT("Named production fixture initializes without a world"), FirstCreated.IsSuccess() && SecondCreated.IsSuccess()))
	{
		return false;
	}
	FHansaProductionFixture First = FirstCreated.Value;
	FHansaProductionFixture Second = SecondCreated.Value;
	const FHansaStateHashReport Initial = First.BuildStateHashes();
	TestEqual(TEXT("Fixture versions share the reviewed registry hash"), First.GetRegistryHash(), FHansaProductionFixture::RegistryHash);
	const FHansaEconomicRegistry* Registry = First.GetDefinitions().GetEconomicRegistry();
	if (TestNotNull(TEXT("Fixture exposes the complete compiled economic registry"), Registry))
	{
		TestEqual(TEXT("Fixture integrates all MVP building definitions"), Registry->GetBuildings().Num(), 14);
		for (const FHansaCompiledBuildingDefinition& Building : Registry->GetBuildings())
		{
			TestTrue(*FString::Printf(TEXT("%s has a resource construction cost"), *Building.StableId),
				!Building.ConstructionCosts.IsEmpty());
			TestTrue(*FString::Printf(TEXT("%s has a positive currency construction cost"), *Building.StableId),
				Building.ConstructionCostPfennig > 0);
			TestTrue(*FString::Printf(TEXT("%s has positive deterministic build time"), *Building.StableId),
				Building.BuildTicks > 0);
			TestEqual(*FString::Printf(TEXT("%s uses the bounded cancellation refund policy"), *Building.StableId),
				Building.CancellationRefundBasisPoints, 5000);
		}
	}
	TestEqual(TEXT("Fresh fixture state is deterministic"), Second.BuildStateHashes().GetOverallHash(), Initial.GetOverallHash());
	TestTrue(TEXT("First exact fixture run succeeds"), First.Step(30).IsSuccess());
	TestTrue(TEXT("Second exact fixture run succeeds"), Second.Step(30).IsSuccess());
	const FHansaStateHashReport Final = First.BuildStateHashes();
	TestEqual(TEXT("Exact fixture runs produce the same final hash"), Second.BuildStateHashes().GetOverallHash(), Final.GetOverallHash());
	TestEqual(TEXT("Exact fixture runs produce the same ordered event count"), Second.GetEvents().Num(), First.GetEvents().Num());

	const FString Evidence = FHansaProductionEvidenceWriter::WriteJson(First, Initial, 30);
	TSharedPtr<FJsonObject> ParsedEvidence;
	TestTrue(TEXT("Production evidence is valid JSON"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Evidence), ParsedEvidence) && ParsedEvidence.IsValid());
	if (ParsedEvidence.IsValid())
	{
		TestEqual(TEXT("Evidence identifies the exact named fixture"), ParsedEvidence->GetStringField(TEXT("fixtureId")), FString(FHansaProductionFixture::StableFixtureId));
		TestTrue(TEXT("Evidence contains causal projections"), ParsedEvidence->GetArrayField(TEXT("causalProjections")).Num() == 9);
		TestTrue(TEXT("Evidence contains typed production events"), ParsedEvidence->GetArrayField(TEXT("events")).Num() > 0);
	}
	const FString EvidencePath = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("Production"), TEXT("mvp_production_chains_v1.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(EvidencePath), true);
	TestTrue(TEXT("Golden production evidence bundle is persisted"),
		FFileHelper::SaveStringToFile(Evidence, *EvidencePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FString GoldenText;
	const FString GoldenPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests"), TEXT("Golden"), TEXT("mvp_production_chains_v1.json"));
	if (!TestTrue(TEXT("Checked-in production golden is readable"), FFileHelper::LoadFileToString(GoldenText, *GoldenPath)))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Golden;
	if (!TestTrue(TEXT("Checked-in production golden parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GoldenText), Golden) && Golden.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("Golden registry hash"), ParsedEvidence->GetStringField(TEXT("registryHash")), Golden->GetStringField(TEXT("registryHash")));
	TestEqual(TEXT("Golden initial state hash"), ParsedEvidence->GetStringField(TEXT("initialStateHash")), Golden->GetStringField(TEXT("initialStateHash")));
	TestEqual(TEXT("Golden final state hash"), ParsedEvidence->GetStringField(TEXT("finalStateHash")), Golden->GetStringField(TEXT("finalStateHash")));
	TestEqual(TEXT("Golden event count"), ParsedEvidence->GetArrayField(TEXT("events")).Num(), static_cast<int32>(Golden->GetNumberField(TEXT("eventCount"))));
	return !HasAnyErrors();
}

#endif
