#include "Misc/AutomationTest.h"

#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests::Trade
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue Require(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TId>
	TId TradeTestsEntity(const uint64 Value)
	{
		return Require(TId::TryCreate(Value));
	}

	template <typename TId>
	TId Stable(const TCHAR* Value)
	{
		return Require(TId::TryParse(Value));
	}

	struct FTradeHarness
	{
		FHansaSimulationDefinitionContext Definitions;
		FHansaSimulationState State;
		FHansaSimulationTransientCache Cache;
		FHansaHouseId Owner = TradeTestsEntity<FHansaHouseId>(1);
		FHansaHouseId OtherHouse = TradeTestsEntity<FHansaHouseId>(2);
		FHansaVehicleId VehicleId = TradeTestsEntity<FHansaVehicleId>(1);
		FHansaInventoryId SourceInventoryId = TradeTestsEntity<FHansaInventoryId>(1);
		FHansaInventoryId DestinationInventoryId = TradeTestsEntity<FHansaInventoryId>(2);
		FHansaInventoryId CargoInventoryId = TradeTestsEntity<FHansaInventoryId>(3);
		FHansaCityDefinitionId Lubeck = Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		FHansaCityDefinitionId Rostock = Stable<FHansaCityDefinitionId>(TEXT("City.Rostock"));
		FHansaGoodId Grain = Stable<FHansaGoodId>(TEXT("Good.Grain"));
		FHansaRouteDefinitionId SeaRoute = Stable<FHansaRouteDefinitionId>(TEXT("Route.TestSea"));
	};

	FTradeHarness MakeHarness(const int64 SourceStock = 50'000, const int64 VehicleCapacity = 15'000,
		const bool bPhysicalLubeck = false, const bool bConnectDock = true)
	{
		FTradeHarness Harness;
		FHansaCompiledGoodDefinition Grain;
		Grain.StableId = TEXT("Good.Grain");
		FHansaCompiledVehicleDefinition Cog;
		Cog.StableId = TEXT("Vehicle.Cog");
		Cog.Mode = EHansaRouteMode::Sea;
		Cog.CargoCapacityMilliUnits = VehicleCapacity;
		Cog.UpkeepPfennigPerTravelTick = 2;
		FHansaCompiledRouteDefinition Sea;
		Sea.StableId = TEXT("Route.TestSea");
		Sea.Mode = EHansaRouteMode::Sea;
		Sea.Connections.Add({ TEXT("City.Lubeck"), TEXT("City.Rostock"), 3 });
		FHansaEconomicRegistry Registry(
			{ Grain }, {}, {}, 0x9002ULL, {}, {}, {}, { Cog }, { Sea });
		Harness.Definitions = Require(FHansaSimulationDefinitionContext::TryCreate(
			Stable<FHansaScenarioId>(TEXT("Scenario.TradeInvariantV1")), 0x9002ULL, MoveTemp(Registry)));

		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Require(FHansaSimulationTick::TryCreate(0))));
		Initialization.CampaignSeed = 0x53095002ULL;
		Initialization.Houses = {
			{ Harness.Owner, FHansaMoney::FromRaw(100'000) },
			{ Harness.OtherHouse, FHansaMoney::FromRaw(100'000) }
		};
		Initialization.Cities = { { Harness.Lubeck, FHansaQuantity() }, { Harness.Rostock, FHansaQuantity() } };
		FHansaVehicleState Vehicle;
		Vehicle.Id = Harness.VehicleId;
		Vehicle.DefinitionId = Stable<FHansaVehicleDefinitionId>(TEXT("Vehicle.Cog"));
		Vehicle.OwnerId = Harness.Owner;
		Vehicle.CargoInventoryId = Harness.CargoInventoryId;
		Vehicle.Mode = EHansaRouteMode::Sea;
		Vehicle.Capacity = FHansaQuantity::FromRaw(VehicleCapacity);
		Vehicle.CurrentCityId = Harness.Lubeck;
		Vehicle.UpkeepPfennigPerTravelTick = 2;
		Initialization.Vehicles.Add(Vehicle);
		if (bPhysicalLubeck)
		{
			const FHansaHouseId Owner = Harness.Owner;
			const auto AddBuilding = [&](const uint64 Value, const TCHAR* DefinitionId)
			{
				FHansaBuildingState Building;
				Building.Id = TradeTestsEntity<FHansaBuildingId>(Value);
				Building.DefinitionId = Stable<FHansaBuildingTypeId>(DefinitionId);
				Building.OwnerId = Owner;
				Building.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
				Building.ConstructionState = EHansaConstructionState::Completed;
				Initialization.Buildings.Add(Building);
			};
			AddBuilding(10, TEXT("Building.Market"));
			AddBuilding(11, TEXT("Building.Dock"));
			AddBuilding(12, TEXT("Building.Road"));
			if (bConnectDock) AddBuilding(13, TEXT("Building.Road"));
			AddBuilding(14, TEXT("Building.Road"));
			AddBuilding(15, TEXT("Building.Road"));
			FHansaPlacementMapInitialization Map;
			Map.CityId = Harness.Lubeck;
			Map.BoundsMin = { 0, 0 };
			Map.BoundsMax = { 3, 1 };
			Map.RoadBuildingDefinitionId = Stable<FHansaBuildingTypeId>(TEXT("Building.Road"));
			for (int32 X = 0; X <= 3; ++X)
				for (int32 Y = 0; Y <= 1; ++Y)
					Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, Owner, false });
			Initialization.Placement.Maps.Add(MoveTemp(Map));
			const auto AddPlacement = [&](const uint64 Value, const TCHAR* DefinitionId, const int32 X, const int32 Y)
			{
				Initialization.Placement.Placements.Add({ TradeTestsEntity<FHansaBuildingId>(Value), Owner,
					{ Harness.Lubeck, Stable<FHansaBuildingTypeId>(DefinitionId), { X, Y }, EHansaGridRotation::North },
					{ { X, Y } } });
			};
			AddPlacement(10, TEXT("Building.Market"), 0, 1);
			AddPlacement(11, TEXT("Building.Dock"), 3, 1);
			AddPlacement(12, TEXT("Building.Road"), 0, 0);
			if (bConnectDock) AddPlacement(13, TEXT("Building.Road"), 1, 0);
			AddPlacement(14, TEXT("Building.Road"), 2, 0);
			AddPlacement(15, TEXT("Building.Road"), 3, 0);
		}

		FHansaInventoryInitialization Source;
		Source.Id = Harness.SourceInventoryId;
		Source.OwnerKind = EHansaInventoryOwnerKind::City;
		Source.CityId = Harness.Lubeck;
		if (bPhysicalLubeck) Source.BuildingId = TradeTestsEntity<FHansaBuildingId>(10);
		Source.Capacity = FHansaQuantity::FromRaw(100'000);
		Source.AcceptedGoods.Add(Harness.Grain);
		Source.InitialStock.Add({ Harness.Grain, FHansaQuantity::FromRaw(SourceStock) });
		FHansaInventoryInitialization Destination;
		Destination.Id = Harness.DestinationInventoryId;
		Destination.OwnerKind = EHansaInventoryOwnerKind::City;
		Destination.CityId = Harness.Rostock;
		Destination.Capacity = FHansaQuantity::FromRaw(100'000);
		Destination.AcceptedGoods.Add(Harness.Grain);
		FHansaInventoryInitialization Cargo;
		Cargo.Id = Harness.CargoInventoryId;
		Cargo.OwnerKind = EHansaInventoryOwnerKind::Vehicle;
		Cargo.VehicleId = Harness.VehicleId;
		Cargo.Capacity = FHansaQuantity::FromRaw(VehicleCapacity);
		Cargo.AcceptedGoods.Add(Harness.Grain);
		Initialization.Inventories = { MoveTemp(Source), MoveTemp(Destination), MoveTemp(Cargo) };
		Harness.State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
		return Harness;
	}

	FHansaCommandHeader Header(const FTradeHarness& Harness, const uint64 Sequence,
		const FHansaHouseId Issuer = FHansaHouseId())
	{
		FHansaCommandHeader Result;
		Result.CommandId = TradeTestsEntity<FHansaCommandId>(Sequence);
		Result.Authority.IssuingHouseId = Issuer.IsValid() ? Issuer : Harness.Owner;
		Result.Authority.PrincipalId = Result.Authority.IssuingHouseId.GetValue();
		Result.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Result.RequestedExecutionTick = Harness.State.CreateReadOnlyAccess(Harness.Definitions).GetClock().GetTick();
		Result.GlobalSequence = Sequence;
		return Result;
	}

	TArray<FHansaRouteStop> Stops(const FTradeHarness& Harness, const int64 Reserve,
		const bool bSplitLoad = false)
	{
		FHansaRouteStop Source;
		Source.CityId = Harness.Lubeck;
		Source.Actions.Add({ EHansaRouteCargoActionKind::Load, EHansaRouteCargoCondition::Always,
			Harness.Grain, FHansaQuantity::FromRaw(bSplitLoad ? 10'000 : 15'000), FHansaQuantity::FromRaw(Reserve) });
		if (bSplitLoad)
		{
			Source.Actions.Add({ EHansaRouteCargoActionKind::Load, EHansaRouteCargoCondition::Always,
				Harness.Grain, FHansaQuantity::FromRaw(10'000), FHansaQuantity::FromRaw(Reserve) });
		}
		FHansaRouteStop Destination;
		Destination.CityId = Harness.Rostock;
		Destination.Actions.Add({ EHansaRouteCargoActionKind::Unload, EHansaRouteCargoCondition::Always,
			Harness.Grain, FHansaQuantity::FromRaw(15'000), FHansaQuantity() });
		return { MoveTemp(Source), MoveTemp(Destination) };
	}

	FHansaCommandGatewayResult CreateActiveRoute(FTradeHarness& Harness, const uint64 Sequence,
		TArray<FHansaRouteStop> RouteStops, const FHansaHouseId Issuer = FHansaHouseId(),
		const FHansaRouteDefinitionId Definition = FHansaRouteDefinitionId())
	{
		FHansaCreateRouteCommand Payload;
		Payload.RouteId = TradeTestsEntity<FHansaRouteId>(1);
		Payload.VehicleId = Harness.VehicleId;
		Payload.RouteDefinitionId = Definition.IsValid() ? Definition : Harness.SeaRoute;
		Payload.Stops = MoveTemp(RouteStops);
		Payload.bActivate = true;
		const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(Header(Harness, Sequence, Issuer), Payload);
		return FHansaGameplayCommandGateway::ExecuteTick(
			Harness.State, Harness.Definitions, MakeArrayView(&Command, 1), Harness.Cache);
	}

	FHansaCommandGatewayResult Step(FTradeHarness& Harness)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(
			Harness.State, Harness.Definitions, TConstArrayView<FHansaGameplayCommand>(), Harness.Cache);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeCapacityReserveArrivalReplayTest,
	"Hansa.Simulation.Trade.CapacityReserveArrivalReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradeCapacityReserveArrivalReplayTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Trade;
	FTradeHarness First = MakeHarness();
	FTradeHarness Replay = MakeHarness();
	FHansaCommandGatewayResult FirstCreate = CreateActiveRoute(First, 1, Stops(First, 30'000, true));
	FHansaCommandGatewayResult ReplayCreate = CreateActiveRoute(Replay, 1, Stops(Replay, 30'000, true));
	TestTrue(TEXT("typed route creation succeeds"), FirstCreate.IsSuccess() && ReplayCreate.IsSuccess());
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		TestTrue(TEXT("route step succeeds"), Step(First).IsSuccess());
		TestTrue(TEXT("replay step succeeds"), Step(Replay).IsSuccess());
	}
	const FHansaSimulationReadOnlyAccess Read = First.State.CreateReadOnlyAccess(First.Definitions);
	const TOptional<FHansaInventoryStockProjection> Source = Read.GetInventories().QueryStock(
		First.SourceInventoryId, First.Grain);
	const TOptional<FHansaInventoryStockProjection> Destination = Read.GetInventories().QueryStock(
		First.DestinationInventoryId, First.Grain);
	const TOptional<FHansaVehicleProjection> Vehicle = Read.QueryVehicle(First.VehicleId);
	const TOptional<FHansaRouteProjection> Route = Read.QueryRoute(TradeTestsEntity<FHansaRouteId>(1));
	TestEqual(TEXT("minimum reserve is protected"), Source->Stock.GetRawValue(), int64(35'000));
	TestEqual(TEXT("capacity-limited cargo is delivered"), Destination->Stock.GetRawValue(), int64(15'000));
	TestEqual(TEXT("cargo empties at destination"), Vehicle->Cargo.GetRawValue(), int64(0));
	TestEqual(TEXT("vehicle capacity is projected"), Vehicle->Capacity.GetRawValue(), int64(15'000));
	TestEqual(TEXT("three travel ticks charge deterministic upkeep"), Vehicle->AccruedUpkeepPfennig, int64(6));
	TestEqual(TEXT("first leg completes at the authored arrival tick"), Route->CompletedLegCount, int64(1));
	TestEqual(TEXT("one capacity-limited action is recorded"), Route->MissedCargoActionCount, int64(1));
	TestEqual(TEXT("identical replay fingerprints match"), Read.GetFingerprint().Value,
		Replay.State.CreateReadOnlyAccess(Replay.Definitions).GetFingerprint().Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeValidationCancellationMissedCargoTest,
	"Hansa.Simulation.Trade.ValidationCancellationMissedCargo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradeValidationCancellationMissedCargoTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Trade;
	FTradeHarness Validation = MakeHarness();
	const uint64 InitialHash = Validation.State.CreateReadOnlyAccess(Validation.Definitions).GetFingerprint().Value;
	const FHansaCommandGatewayResult WrongOwner = CreateActiveRoute(
		Validation, 1, Stops(Validation, 0), Validation.OtherHouse);
	TestEqual(TEXT("foreign vehicle edit is rejected"), WrongOwner.GetError(), EHansaCommandGatewayError::NotAuthorized);
	TestEqual(TEXT("rejection is transactional"),
		Validation.State.CreateReadOnlyAccess(Validation.Definitions).GetFingerprint().Value, InitialHash);

	FHansaRouteDefinitionId Unknown = Stable<FHansaRouteDefinitionId>(TEXT("Route.Unknown"));
	const FHansaCommandGatewayResult UnknownRoute = CreateActiveRoute(
		Validation, 2, Stops(Validation, 0), FHansaHouseId(), Unknown);
	TestEqual(TEXT("unknown route definition is rejected"), UnknownRoute.GetError(), EHansaCommandGatewayError::RouteRejected);
	TestEqual(TEXT("typed validation cause is exposed"), UnknownRoute.GetRoutePlanError(), EHansaRoutePlanError::DefinitionNotFound);

	FTradeHarness Cancel = MakeHarness();
	TestTrue(TEXT("route starts with cargo"), CreateActiveRoute(Cancel, 1, Stops(Cancel, 0)).IsSuccess());
	const FHansaCancelRouteCommand Payload { TradeTestsEntity<FHansaRouteId>(1) };
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(Header(Cancel, 2), Payload);
	const FHansaCommandGatewayResult Cancelled = FHansaGameplayCommandGateway::ExecuteTick(
		Cancel.State, Cancel.Definitions, MakeArrayView(&Command, 1), Cancel.Cache);
	const FHansaSimulationReadOnlyAccess CancelRead = Cancel.State.CreateReadOnlyAccess(Cancel.Definitions);
	TestTrue(TEXT("in-transit cancellation succeeds"), Cancelled.IsSuccess());
	TestEqual(TEXT("cancelled route stops"), CancelRead.QueryRoute(TradeTestsEntity<FHansaRouteId>(1))->Lifecycle,
		EHansaRouteLifecycleState::Cancelled);
	TestEqual(TEXT("cancellation preserves loaded cargo"), CancelRead.QueryVehicle(Cancel.VehicleId)->Cargo.GetRawValue(), int64(15'000));

	FTradeHarness Missed = MakeHarness();
	const FHansaCommandGatewayResult MissedResult = CreateActiveRoute(Missed, 1, Stops(Missed, 50'000));
	const TOptional<FHansaRouteProjection> MissedRoute =
		Missed.State.CreateReadOnlyAccess(Missed.Definitions).QueryRoute(TradeTestsEntity<FHansaRouteId>(1));
	TestTrue(TEXT("reserve-blocked route still departs deterministically"), MissedResult.IsSuccess());
	TestEqual(TEXT("missed cargo is recorded"), MissedRoute->MissedCargoActionCount, int64(1));
	TestEqual(TEXT("reserve-blocked cargo remains empty"),
		Missed.State.CreateReadOnlyAccess(Missed.Definitions).QueryVehicle(Missed.VehicleId)->Cargo.GetRawValue(), int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradePhysicalHarborHandoffTest,
	"Hansa.Simulation.Trade.PhysicalHarborHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradePhysicalHarborHandoffTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::Trade;
	FTradeHarness Disconnected = MakeHarness(50'000, 15'000, true, false);
	TestTrue(TEXT("A route with a disconnected harbor remains a valid plan"),
		CreateActiveRoute(Disconnected, 1, Stops(Disconnected, 0)).IsSuccess());
	const FHansaSimulationReadOnlyAccess DisconnectedRead =
		Disconnected.State.CreateReadOnlyAccess(Disconnected.Definitions);
	TestEqual(TEXT("A disconnected harbor cannot load city stock"),
		DisconnectedRead.QueryVehicle(Disconnected.VehicleId)->Cargo.GetRawValue(), int64(0));
	TestEqual(TEXT("A missed harbor handoff conserves source stock"),
		DisconnectedRead.GetInventories().QueryStock(Disconnected.SourceInventoryId, Disconnected.Grain)->Stock.GetRawValue(),
		int64(50'000));

	FTradeHarness Connected = MakeHarness(50'000, 15'000, true, true);
	TestTrue(TEXT("A connected harbor route starts"),
		CreateActiveRoute(Connected, 1, Stops(Connected, 0)).IsSuccess());
	const FHansaSimulationReadOnlyAccess ConnectedRead = Connected.State.CreateReadOnlyAccess(Connected.Definitions);
	TestEqual(TEXT("Reconnecting the harbor permits the bounded load"),
		ConnectedRead.QueryVehicle(Connected.VehicleId)->Cargo.GetRawValue(), int64(15'000));
	TestEqual(TEXT("The connected load conserves city plus cargo stock"),
		ConnectedRead.GetInventories().QueryStock(Connected.SourceInventoryId, Connected.Grain)->Stock.GetRawValue() +
		ConnectedRead.QueryVehicle(Connected.VehicleId)->Cargo.GetRawValue(), int64(50'000));
	return !HasAnyErrors();
}
#endif
