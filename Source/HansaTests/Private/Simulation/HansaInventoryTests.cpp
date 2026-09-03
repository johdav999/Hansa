#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Inventory/HansaInventory.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Inventory
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue Require(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TEntityId>
	TEntityId Entity(const uint64 Value)
	{
		const THansaValueResult<TEntityId> Result = TEntityId::TryCreate(Value);
		return Require(Result);
	}

	FHansaGoodId Good(const TCHAR* Id)
	{
		return Require(FHansaGoodId::TryParse(Id));
	}

	FHansaSimulationTick Tick(const int64 Value)
	{
		const THansaValueResult<FHansaSimulationTick> Result = FHansaSimulationTick::TryCreate(Value);
		return Require(Result);
	}

	FHansaInventoryInitialization CityInventory(
		const uint64 InventoryValue,
		const TCHAR* CityId,
		const int64 Capacity,
		TArray<FHansaGoodId> Accepted,
		TArray<FHansaInventoryStockInitialization> Stock = {})
	{
		FHansaInventoryInitialization Result;
		Result.Id = Entity<FHansaInventoryId>(InventoryValue);
		Result.OwnerKind = EHansaInventoryOwnerKind::City;
		Result.CityId = Require(FHansaCityDefinitionId::TryParse(CityId));
		Result.Capacity = FHansaQuantity::FromRaw(Capacity);
		Result.AcceptedGoods = MoveTemp(Accepted);
		Result.InitialStock = MoveTemp(Stock);
		return Result;
	}

	FHansaInventoryInitialization BuildingInventory(
		const uint64 InventoryValue,
		const uint64 BuildingValue,
		const EHansaInventoryOwnerKind Kind,
		const int64 Capacity,
		TArray<FHansaGoodId> Accepted,
		TArray<FHansaInventoryStockInitialization> Stock = {})
	{
		FHansaInventoryInitialization Result;
		Result.Id = Entity<FHansaInventoryId>(InventoryValue);
		Result.OwnerKind = Kind;
		Result.BuildingId = Entity<FHansaBuildingId>(BuildingValue);
		Result.Capacity = FHansaQuantity::FromRaw(Capacity);
		Result.AcceptedGoods = MoveTemp(Accepted);
		Result.InitialStock = MoveTemp(Stock);
		return Result;
	}

	FHansaInventoryStockInitialization Stock(const FHansaGoodId GoodId, const int64 Quantity)
	{
		return { GoodId, FHansaQuantity::FromRaw(Quantity) };
	}

	FHansaInventoryLedger MakeLedger(const bool bReverse = false, const int32 MovementCapacity = 64)
	{
		const FHansaGoodId Grain = Good(TEXT("Good.Grain"));
		const FHansaGoodId Bread = Good(TEXT("Good.Bread"));
		const FHansaGoodId Fish = Good(TEXT("Good.Fish"));
		FHansaInventoryInitialization City = CityInventory(
			1,
			TEXT("City.Lubeck"),
			10'000,
			{ Fish, Bread, Grain },
			{ Stock(Grain, 5'000), Stock(Bread, 1'000) });
		FHansaInventoryInitialization Warehouse = BuildingInventory(
			2,
			10,
			EHansaInventoryOwnerKind::Warehouse,
			6'000,
			{ Bread, Grain });
		TArray<FHansaInventoryInitialization> Inventories = bReverse
			? TArray<FHansaInventoryInitialization> { Warehouse, City }
			: TArray<FHansaInventoryInitialization> { City, Warehouse };
		return Require(FHansaInventoryLedger::TryCreate(MoveTemp(Inventories), MovementCapacity));
	}

	int64 TotalStock(const FHansaInventoryReadOnlyAccess& Access)
	{
		int64 Total = 0;
		for (const FHansaInventoryProjection& Inventory : Access.BuildProjection())
		{
			Total += Inventory.UsedCapacity.GetRawValue();
		}
		return Total;
	}

	FString Canonical(const FHansaInventoryReadOnlyAccess& Access)
	{
		FString Result;
		for (const FHansaInventoryProjection& Inventory : Access.BuildProjection())
		{
			Result += FString::Printf(
				TEXT("inventory=%llu;capacity=%lld;used=%lld;reserved=%lld\n"),
				static_cast<unsigned long long>(Inventory.Id.GetValue()),
				static_cast<long long>(Inventory.Capacity.GetRawValue()),
				static_cast<long long>(Inventory.UsedCapacity.GetRawValue()),
				static_cast<long long>(Inventory.Reserved.GetRawValue()));
			for (const FHansaInventoryStockProjection& StockProjection : Inventory.Stocks)
			{
				Result += FString::Printf(
					TEXT("stock=%s:%lld:%lld\n"),
					*StockProjection.GoodId.ToString(),
					static_cast<long long>(StockProjection.Stock.GetRawValue()),
					static_cast<long long>(StockProjection.Reserved.GetRawValue()));
			}
			for (const FHansaInventoryMovement& Movement : Access.QueryRecentMovements(Inventory.Id, 64))
			{
				Result += FString::Printf(
					TEXT("movement=%llu:%d:%s:%lld\n"),
					static_cast<unsigned long long>(Movement.Sequence),
					static_cast<int32>(Movement.Kind),
					*Movement.GoodId.ToString(),
					static_cast<long long>(Movement.Quantity.GetRawValue()));
			}
		}
		return Result;
	}

	void TestProjectionInvariants(FAutomationTestBase& Test, const FHansaInventoryReadOnlyAccess& Access)
	{
		for (const FHansaInventoryProjection& Inventory : Access.BuildProjection())
		{
			Test.TestTrue(TEXT("Used inventory capacity is non-negative"), Inventory.UsedCapacity.GetRawValue() >= 0);
			Test.TestTrue(TEXT("Used inventory capacity never exceeds capacity"), Inventory.UsedCapacity.GetRawValue() <= Inventory.Capacity.GetRawValue());
			Test.TestEqual(
				TEXT("Used plus free capacity equals total capacity"),
				Inventory.UsedCapacity.GetRawValue() + Inventory.FreeCapacity.GetRawValue(),
				Inventory.Capacity.GetRawValue());
			for (const FHansaInventoryStockProjection& StockProjection : Inventory.Stocks)
			{
				Test.TestTrue(TEXT("Stock is non-negative"), StockProjection.Stock.GetRawValue() >= 0);
				Test.TestTrue(TEXT("Reserved stock is non-negative"), StockProjection.Reserved.GetRawValue() >= 0);
				Test.TestTrue(TEXT("Reserved stock never exceeds stock"), StockProjection.Reserved.GetRawValue() <= StockProjection.Stock.GetRawValue());
				Test.TestEqual(
					TEXT("Available stock excludes reservations"),
					StockProjection.Available.GetRawValue(),
					StockProjection.Stock.GetRawValue() - StockProjection.Reserved.GetRawValue());
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInventoryInitializationAndQueriesTest,
	"Hansa.Simulation.Inventory.InitializationAndQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInventoryInitializationAndQueriesTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Inventory;
	FHansaInventoryLedger Ledger = MakeLedger(true);
	const FHansaInventoryReadOnlyAccess Access = Ledger.CreateReadOnlyAccess();
	TestEqual(TEXT("City and warehouse inventory records are present"), Access.GetInventoryCount(), 2);
	const TArray<FHansaInventoryProjection> Projection = Access.BuildProjection();
	TestEqual(TEXT("Discovery order is canonicalized by inventory ID"), Projection[0].Id.GetValue(), static_cast<uint64>(1));
	TestEqual(TEXT("City used capacity is projected"), Projection[0].UsedCapacity.GetRawValue(), static_cast<int64>(6'000));
	TestEqual(TEXT("City free capacity is projected"), Projection[0].FreeCapacity.GetRawValue(), static_cast<int64>(4'000));
	const TOptional<FHansaQuantity> Capacity = Access.QueryCapacity(Entity<FHansaInventoryId>(1));
	TestTrue(TEXT("Typed capacity query resolves an inventory"), Capacity.IsSet());
	if (Capacity.IsSet())
	{
		TestEqual(TEXT("Typed capacity query returns fixed-point capacity"), Capacity->GetRawValue(), static_cast<int64>(10'000));
	}
	const TOptional<FHansaQuantity> Reserved = Access.QueryReservedAmount(Entity<FHansaInventoryId>(1));
	TestTrue(TEXT("Typed reserved-amount query resolves an inventory"), Reserved.IsSet());
	if (Reserved.IsSet())
	{
		TestEqual(TEXT("Typed reserved-amount query starts at zero"), Reserved->GetRawValue(), static_cast<int64>(0));
	}

	const TOptional<FHansaInventoryStockProjection> Grain = Access.QueryStock(
		Entity<FHansaInventoryId>(1), Good(TEXT("Good.Grain")));
	TestTrue(TEXT("Typed stock query resolves accepted stock"), Grain.IsSet());
	if (Grain.IsSet())
	{
		TestEqual(TEXT("Typed stock query returns raw milli-units"), Grain->Stock.GetRawValue(), static_cast<int64>(5'000));
		TestEqual(TEXT("Initial reserved amount is zero"), Grain->Reserved.GetRawValue(), static_cast<int64>(0));
	}
	const TOptional<FHansaInventoryStockProjection> AcceptedButEmpty = Access.QueryStock(
		Entity<FHansaInventoryId>(2), Good(TEXT("Good.Bread")));
	TestTrue(TEXT("Accepted goods without stock return a zero-valued projection"), AcceptedButEmpty.IsSet());
	const TOptional<FHansaInventoryStockProjection> NotAccepted = Access.QueryStock(
		Entity<FHansaInventoryId>(2), Good(TEXT("Good.Fish")));
	TestFalse(TEXT("Goods not accepted by an inventory do not resolve as stock"), NotAccepted.IsSet());
	TestProjectionInvariants(*this, Access);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInventoryInvalidInitializationTest,
	"Hansa.Simulation.Inventory.InvalidInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInventoryInvalidInitializationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Inventory;
	const FHansaGoodId Grain = Good(TEXT("Good.Grain"));

	const FHansaInventoryInitialization First = CityInventory(
		1, TEXT("City.Lubeck"), 1'000, { Grain }, { Stock(Grain, 500) });
	FHansaInventoryInitialization DuplicateId = BuildingInventory(
		1, 10, EHansaInventoryOwnerKind::Warehouse, 1'000, { Grain });
	TestFalse(
		TEXT("Duplicate inventory IDs are rejected"),
		FHansaInventoryLedger::TryCreate({ First, DuplicateId }, 16).IsSuccess());

	const FHansaInventoryInitialization DuplicateOwner = CityInventory(
		2, TEXT("City.Lubeck"), 1'000, { Grain });
	TestFalse(
		TEXT("A city cannot own duplicate inventories"),
		FHansaInventoryLedger::TryCreate({ First, DuplicateOwner }, 16).IsSuccess());

	const FHansaInventoryInitialization NegativeStock = CityInventory(
		3, TEXT("City.Riga"), 1'000, { Grain }, { Stock(Grain, -1) });
	TestFalse(
		TEXT("Negative initial stock is rejected"),
		FHansaInventoryLedger::TryCreate({ NegativeStock }, 16).IsSuccess());

	const FHansaInventoryInitialization OverCapacity = CityInventory(
		4, TEXT("City.Reval"), 1'000, { Grain }, { Stock(Grain, 1'001) });
	TestFalse(
		TEXT("Initial stock above capacity is rejected"),
		FHansaInventoryLedger::TryCreate({ OverCapacity }, 16).IsSuccess());

	const FHansaInventoryInitialization DuplicateAcceptedGood = CityInventory(
		5, TEXT("City.Visby"), 1'000, { Grain, Grain });
	TestFalse(
		TEXT("Duplicate accepted goods are rejected"),
		FHansaInventoryLedger::TryCreate({ DuplicateAcceptedGood }, 16).IsSuccess());

	FHansaInventoryInitialization InvalidOwner = CityInventory(
		6, TEXT("City.Bergen"), 1'000, { Grain });
	InvalidOwner.OwnerKind = static_cast<EHansaInventoryOwnerKind>(255);
	TestFalse(
		TEXT("Unknown inventory owner kinds are rejected"),
		FHansaInventoryLedger::TryCreate({ InvalidOwner }, 16).IsSuccess());

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInventoryAtomicTransfersTest,
	"Hansa.Simulation.Inventory.AtomicTransfers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInventoryAtomicTransfersTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Inventory;
	FHansaInventoryLedger Ledger = MakeLedger();
	const FHansaInventoryId City = Entity<FHansaInventoryId>(1);
	const FHansaInventoryId Warehouse = Entity<FHansaInventoryId>(2);
	const FHansaGoodId Grain = Good(TEXT("Good.Grain"));
	const FHansaGoodId Fish = Good(TEXT("Good.Fish"));

	const int64 TotalBefore = TotalStock(Ledger.CreateReadOnlyAccess());
	const FHansaInventoryTransactionResult Transfer = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Inventory(City),
		FHansaInventoryEndpoint::Inventory(Warehouse),
		Grain,
		FHansaQuantity::FromRaw(2'000),
		Tick(10),
		1);
	TestTrue(TEXT("Inventory-to-inventory transfer succeeds"), Transfer.IsSuccess());
	TestEqual(TEXT("Ordinary transfer conserves total stock"), TotalStock(Ledger.CreateReadOnlyAccess()), TotalBefore);
	TestEqual(TEXT("Ordinary transfer creates no stock"), Transfer.ExplicitlyCreatedQuantity.GetRawValue(), static_cast<int64>(0));
	TestEqual(TEXT("Ordinary transfer destroys no stock"), Transfer.ExplicitlyDestroyedQuantity.GetRawValue(), static_cast<int64>(0));

	const FString BeforeCapacityFailure = Canonical(Ledger.CreateReadOnlyAccess());
	const FHansaInventoryTransactionResult CapacityFailure = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Source(TEXT("Source.CapacityProbe")),
		FHansaInventoryEndpoint::Inventory(Warehouse),
		Grain,
		FHansaQuantity::FromRaw(5'000),
		Tick(11),
		2);
	TestEqual(TEXT("Destination capacity failure is explicit"), CapacityFailure.Error, EHansaInventoryTransactionError::CapacityExceeded);
	TestEqual(TEXT("Failed transfer applies zero"), CapacityFailure.AppliedQuantity.GetRawValue(), static_cast<int64>(0));
	TestEqual(TEXT("Capacity failure changes neither endpoint nor history"), Canonical(Ledger.CreateReadOnlyAccess()), BeforeCapacityFailure);

	const FHansaInventoryTransactionResult RejectedGood = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Inventory(City),
		FHansaInventoryEndpoint::Inventory(Warehouse),
		Fish,
		FHansaQuantity::FromRaw(100),
		Tick(12),
		2);
	TestEqual(TEXT("Destination accepted-goods policy is enforced"), RejectedGood.Error, EHansaInventoryTransactionError::GoodNotAccepted);

	const FHansaInventoryTransactionResult SourceDeposit = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Source(TEXT("Source.BackgroundTrade")),
		FHansaInventoryEndpoint::Inventory(Warehouse),
		Grain,
		FHansaQuantity::FromRaw(500),
		Tick(13),
		2);
	TestTrue(TEXT("Explicit source deposit succeeds"), SourceDeposit.IsSuccess());
	TestEqual(TEXT("Explicit source reports created quantity"), SourceDeposit.ExplicitlyCreatedQuantity.GetRawValue(), static_cast<int64>(500));

	const FHansaInventoryTransactionResult SinkWithdrawal = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Inventory(Warehouse),
		FHansaInventoryEndpoint::Sink(TEXT("Sink.CitizenConsumption")),
		Grain,
		FHansaQuantity::FromRaw(250),
		Tick(14),
		3);
	TestTrue(TEXT("Explicit sink withdrawal succeeds"), SinkWithdrawal.IsSuccess());
	TestEqual(TEXT("Explicit sink reports destroyed quantity"), SinkWithdrawal.ExplicitlyDestroyedQuantity.GetRawValue(), static_cast<int64>(250));

	const FHansaInventoryTransactionResult InvalidExternal = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Source(TEXT("Source.Invalid")),
		FHansaInventoryEndpoint::Sink(TEXT("Sink.Invalid")),
		Grain,
		FHansaQuantity::FromRaw(1),
		Tick(15),
		4);
	TestEqual(TEXT("Source-to-sink bypass is rejected"), InvalidExternal.Error, EHansaInventoryTransactionError::InvalidEndpoint);
	TestProjectionInvariants(*this, Ledger.CreateReadOnlyAccess());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInventoryCompetingReservationsTest,
	"Hansa.Simulation.Inventory.CompetingReservations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInventoryCompetingReservationsTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Inventory;
	FHansaInventoryLedger Ledger = MakeLedger();
	const FHansaInventoryId City = Entity<FHansaInventoryId>(1);
	const FHansaInventoryId Warehouse = Entity<FHansaInventoryId>(2);
	const FHansaGoodId Grain = Good(TEXT("Good.Grain"));
	const FHansaReservationId FirstReservation = Entity<FHansaReservationId>(1);
	const FHansaReservationId SecondReservation = Entity<FHansaReservationId>(2);

	TestTrue(TEXT("First reservation claims stock"), Ledger.TryReserve(
		City, FirstReservation, Grain, FHansaQuantity::FromRaw(3'000), Tick(20), 1).IsSuccess());
	const FHansaInventoryTransactionResult CompetingFailure = Ledger.TryReserve(
		City, SecondReservation, Grain, FHansaQuantity::FromRaw(2'500), Tick(20), 2);
	TestEqual(TEXT("Competing reservation cannot over-claim stock"), CompetingFailure.Error, EHansaInventoryTransactionError::InsufficientUnreservedStock);
	TestTrue(TEXT("Second reservation may claim the exact remainder"), Ledger.TryReserve(
		City, SecondReservation, Grain, FHansaQuantity::FromRaw(2'000), Tick(20), 2).IsSuccess());

	const FHansaInventoryTransactionResult UnreservedWithdrawal = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Inventory(City),
		FHansaInventoryEndpoint::Inventory(Warehouse),
		Grain,
		FHansaQuantity::FromRaw(1),
		Tick(21),
		3);
	TestEqual(TEXT("Unreserved transfer cannot consume reserved stock"), UnreservedWithdrawal.Error, EHansaInventoryTransactionError::InsufficientUnreservedStock);

	const FHansaInventoryTransactionResult ReservedTransfer = Ledger.TryTransfer(
		FHansaInventoryEndpoint::Inventory(City),
		FHansaInventoryEndpoint::Inventory(Warehouse),
		Grain,
		FHansaQuantity::FromRaw(1'500),
		Tick(21),
		3,
		FirstReservation);
	TestTrue(TEXT("Matching reservation can be consumed atomically"), ReservedTransfer.IsSuccess());
	TestTrue(TEXT("Competing reservation can be released"), Ledger.TryReleaseReservation(
		SecondReservation, Tick(22), 4).IsSuccess());

	const TOptional<FHansaInventoryStockProjection> StockProjection =
		Ledger.CreateReadOnlyAccess().QueryStock(City, Grain);
	TestTrue(TEXT("Reserved projection remains queryable"), StockProjection.IsSet());
	if (StockProjection.IsSet())
	{
		TestEqual(TEXT("Partial reservation consumption leaves exact remainder"), StockProjection->Reserved.GetRawValue(), static_cast<int64>(1'500));
		TestEqual(TEXT("Available stock excludes remaining reservation"), StockProjection->Available.GetRawValue(), static_cast<int64>(2'000));
	}
	const TArray<FHansaInventoryMovement> Movements = Ledger.CreateReadOnlyAccess().QueryRecentMovements(City, 8);
	TestTrue(TEXT("Recent movements are newest-first"), Movements.Num() >= 3 && Movements[0].Sequence > Movements[1].Sequence);
	TestProjectionInvariants(*this, Ledger.CreateReadOnlyAccess());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInventoryPropertyInvariantTest,
	"Hansa.Simulation.Inventory.PropertyInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInventoryPropertyInvariantTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Inventory;
	FHansaInventoryLedger Forward = MakeLedger(false, 16);
	FHansaInventoryLedger Reversed = MakeLedger(true, 16);
	const FHansaInventoryId City = Entity<FHansaInventoryId>(1);
	const FHansaInventoryId Warehouse = Entity<FHansaInventoryId>(2);
	const FHansaGoodId Grain = Good(TEXT("Good.Grain"));
	uint64 RandomState = 0x9e3779b97f4a7c15ULL;
	int64 ExplicitlyCreated = 0;
	int64 ExplicitlyDestroyed = 0;
	const int64 InitialTotal = TotalStock(Forward.CreateReadOnlyAccess());

	for (uint64 Sequence = 1; Sequence <= 300; ++Sequence)
	{
		RandomState = RandomState * 6364136223846793005ULL + 1442695040888963407ULL;
		const int64 Quantity = static_cast<int64>((RandomState >> 32) % 700) + 1;
		const uint64 Operation = RandomState % 3;
		FHansaInventoryEndpoint Source;
		FHansaInventoryEndpoint Destination;
		if (Operation == 0)
		{
			Source = FHansaInventoryEndpoint::Source(TEXT("Source.PropertyFixture"));
			Destination = FHansaInventoryEndpoint::Inventory(City);
		}
		else if (Operation == 1)
		{
			Source = FHansaInventoryEndpoint::Inventory(City);
			Destination = FHansaInventoryEndpoint::Inventory(Warehouse);
		}
		else
		{
			Source = FHansaInventoryEndpoint::Inventory(Warehouse);
			Destination = FHansaInventoryEndpoint::Sink(TEXT("Sink.PropertyFixture"));
		}

		const int64 Before = TotalStock(Forward.CreateReadOnlyAccess());
		const FHansaInventoryTransactionResult First = Forward.TryTransfer(
			Source, Destination, Grain, FHansaQuantity::FromRaw(Quantity), Tick(static_cast<int64>(Sequence)), Sequence);
		const FHansaInventoryTransactionResult Second = Reversed.TryTransfer(
			Source, Destination, Grain, FHansaQuantity::FromRaw(Quantity), Tick(static_cast<int64>(Sequence)), Sequence);
		TestEqual(TEXT("Equivalent ledgers return the same transaction error"), First.Error, Second.Error);
		TestEqual(TEXT("Equivalent ledgers apply the same quantity"), First.AppliedQuantity.GetRawValue(), Second.AppliedQuantity.GetRawValue());
		const int64 After = TotalStock(Forward.CreateReadOnlyAccess());
		if (First.IsSuccess())
		{
			ExplicitlyCreated += First.ExplicitlyCreatedQuantity.GetRawValue();
			ExplicitlyDestroyed += First.ExplicitlyDestroyedQuantity.GetRawValue();
			TestEqual(
				TEXT("Successful transaction conserves stock except explicit source/sink delta"),
				After,
				Before + First.ExplicitlyCreatedQuantity.GetRawValue() - First.ExplicitlyDestroyedQuantity.GetRawValue());
		}
		else
		{
			TestEqual(TEXT("Failed transaction is atomic"), After, Before);
			TestEqual(TEXT("Failed transaction applies zero quantity"), First.AppliedQuantity.GetRawValue(), static_cast<int64>(0));
		}
		TestProjectionInvariants(*this, Forward.CreateReadOnlyAccess());
	}

	TestEqual(
		TEXT("Long-run stock equals initial plus explicit sources minus explicit sinks"),
		TotalStock(Forward.CreateReadOnlyAccess()),
		InitialTotal + ExplicitlyCreated - ExplicitlyDestroyed);
	TestEqual(TEXT("Initial discovery order cannot affect final ledger state"), Canonical(Forward.CreateReadOnlyAccess()), Canonical(Reversed.CreateReadOnlyAccess()));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInventorySimulationIntegrationTest,
	"Hansa.Integration.Inventory.StateProjectionAndHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInventorySimulationIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Inventory;
	const auto MakeState = [](const bool bReverse)
	{
		FHansaSimulationInitialization Initialization;
		const THansaValueResult<FHansaSimulationVersion> VersionResult = FHansaSimulationVersion::TryCreate(1);
		const FHansaSimulationVersion Version = Require(VersionResult);
		const THansaValueResult<FHansaSimulationClock> ClockResult =
			FHansaSimulationClock::TryCreate(Version, Tick(0));
		Initialization.Clock = Require(ClockResult);
		Initialization.CampaignSeed = 77;
		Initialization.Houses.Add({ Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(100'000) });
		Initialization.Cities.Add({
			Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))),
			FHansaQuantity::FromRaw(6'000) });
		Initialization.Buildings.Add({
			Entity<FHansaBuildingId>(10),
			Require(FHansaBuildingTypeId::TryParse(TEXT("Building.Warehouse"))),
			Entity<FHansaHouseId>(1),
			FHansaRate::FromPartsPerMillion(FHansaRate::Scale) });
		const FHansaGoodId Grain = Good(TEXT("Good.Grain"));
		FHansaInventoryInitialization City = CityInventory(
			1, TEXT("City.Lubeck"), 10'000, { Grain }, { Stock(Grain, 5'000) });
		FHansaInventoryInitialization Warehouse = BuildingInventory(
			2, 10, EHansaInventoryOwnerKind::Warehouse, 8'000, { Grain }, { Stock(Grain, 1'000) });
		Initialization.Inventories = bReverse
			? TArray<FHansaInventoryInitialization> { Warehouse, City }
			: TArray<FHansaInventoryInitialization> { City, Warehouse };
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	};

	const FHansaScenarioId ScenarioId = Require(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")));
	const FHansaSimulationDefinitionContext Definitions = Require(
		FHansaSimulationDefinitionContext::TryCreate(ScenarioId, 0x123456789abcdef0ULL));
	const FHansaSimulationState Forward = MakeState(false);
	const FHansaSimulationState Reversed = MakeState(true);
	const FHansaSimulationReadOnlyAccess ForwardAccess = Forward.CreateReadOnlyAccess(Definitions);
	const FHansaSimulationReadOnlyAccess ReversedAccess = Reversed.CreateReadOnlyAccess(Definitions);
	TestEqual(
		TEXT("Inventory discovery order does not affect full simulation fingerprint"),
		ForwardAccess.GetFingerprint().Value,
		ReversedAccess.GetFingerprint().Value);
	const FHansaSubsystemStateHash* InventoryHash = ForwardAccess.BuildStateHashReport().Find(EHansaStateHashSubsystem::Inventories);
	TestNotNull(TEXT("Inventory receives a dedicated state-hash subsystem"), InventoryHash);
	if (InventoryHash != nullptr)
	{
		TestTrue(TEXT("Inventory subsystem hash is non-zero"), InventoryHash->Value != 0);
	}
	const FHansaSimulationSnapshot Snapshot = ForwardAccess.CaptureSnapshot();
	TestEqual(TEXT("Snapshot owns inventory records"), Snapshot.GetInventories().GetInventories().Num(), 2);
	const THansaValueResult<FHansaSimulationProjection> Projection = ForwardAccess.BuildProjection();
	TestTrue(TEXT("Simulation projection succeeds with inventories"), Projection.IsSuccess());
	if (Projection)
	{
		TestEqual(TEXT("Simulation projection exposes typed inventory projections"), Projection.Value.GetInventories().Num(), 2);
	}
	return !HasAnyErrors();
}

#endif
