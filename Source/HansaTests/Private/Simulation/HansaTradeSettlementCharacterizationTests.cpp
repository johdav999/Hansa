#include "Misc/AutomationTest.h"

#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests::TradeSettlement
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

	template <typename TId>
	TId Stable(const TCHAR* Value)
	{
		return Require(TId::TryParse(Value));
	}

	struct FSettlementHarness
	{
		FHansaSimulationDefinitionContext Definitions;
		FHansaSimulationState State;
		FHansaSimulationTransientCache Cache;
		FHansaHouseId HouseId = Entity<FHansaHouseId>(1);
		FHansaVehicleId VehicleId = Entity<FHansaVehicleId>(1);
		FHansaInventoryId LubeckInventoryId = Entity<FHansaInventoryId>(1);
		FHansaInventoryId RostockInventoryId = Entity<FHansaInventoryId>(2);
		FHansaInventoryId CargoInventoryId = Entity<FHansaInventoryId>(3);
		FHansaCityDefinitionId Lubeck = Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		FHansaCityDefinitionId Rostock = Stable<FHansaCityDefinitionId>(TEXT("City.Rostock"));
		FHansaGoodId Grain = Stable<FHansaGoodId>(TEXT("Good.Grain"));
		FHansaRouteDefinitionId SeaRoute = Stable<FHansaRouteDefinitionId>(TEXT("Route.TestSea"));
	};

	FSettlementHarness MakeHarness(const int64 LubeckStock, const int64 RostockStock)
	{
		FSettlementHarness Harness;
		FHansaCompiledGoodDefinition Grain;
		Grain.StableId = TEXT("Good.Grain");
		FHansaCompiledVehicleDefinition Cog;
		Cog.StableId = TEXT("Vehicle.Cog");
		Cog.Mode = EHansaRouteMode::Sea;
		Cog.CargoCapacityMilliUnits = 15'000;
		Cog.UpkeepPfennigPerTravelTick = 2;
		FHansaCompiledRouteDefinition Sea;
		Sea.StableId = TEXT("Route.TestSea");
		Sea.Mode = EHansaRouteMode::Sea;
		Sea.Connections.Add({ TEXT("City.Lubeck"), TEXT("City.Rostock"), 3 });
		FHansaCompiledCityMarketProfileDefinition LubeckMarket;
		LubeckMarket.StableId = TEXT("City.Lubeck");
		FHansaCompiledCityMarketProfileDefinition RostockMarket;
		RostockMarket.StableId = TEXT("City.Rostock");
		RostockMarket.bMarketOnly = true;
		FHansaEconomicRegistry Registry(
			{ Grain }, {}, {}, 0x9003ULL, {}, {}, { LubeckMarket, RostockMarket }, { Cog }, { Sea });
		Harness.Definitions = Require(FHansaSimulationDefinitionContext::TryCreate(
			Stable<FHansaScenarioId>(TEXT("Scenario.TradeSettlementCharacterizationV1")),
			0x9003ULL, MoveTemp(Registry)));

		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Require(FHansaSimulationTick::TryCreate(0))));
		Initialization.CampaignSeed = 0x53095003ULL;
		Initialization.Houses.Add({ Harness.HouseId, FHansaMoney::FromRaw(100'000) });
		Initialization.Cities = { { Harness.Lubeck, FHansaQuantity() }, { Harness.Rostock, FHansaQuantity() } };

		FHansaVehicleState Vehicle;
		Vehicle.Id = Harness.VehicleId;
		Vehicle.DefinitionId = Stable<FHansaVehicleDefinitionId>(TEXT("Vehicle.Cog"));
		Vehicle.OwnerId = Harness.HouseId;
		Vehicle.CargoInventoryId = Harness.CargoInventoryId;
		Vehicle.Mode = EHansaRouteMode::Sea;
		Vehicle.Capacity = FHansaQuantity::FromRaw(15'000);
		Vehicle.CurrentCityId = Harness.Lubeck;
		Vehicle.UpkeepPfennigPerTravelTick = 2;
		Initialization.Vehicles.Add(Vehicle);

		const auto AddInventory = [&Initialization, &Harness](const FHansaInventoryId InventoryId,
			const EHansaInventoryOwnerKind OwnerKind, const FHansaCityDefinitionId CityId,
			const int64 Stock)
		{
			FHansaInventoryInitialization Inventory;
			Inventory.Id = InventoryId;
			Inventory.OwnerKind = OwnerKind;
			Inventory.CityId = CityId;
			if (OwnerKind == EHansaInventoryOwnerKind::Vehicle) Inventory.VehicleId = Harness.VehicleId;
			Inventory.Capacity = FHansaQuantity::FromRaw(
				OwnerKind == EHansaInventoryOwnerKind::Vehicle ? 15'000 : 100'000);
			Inventory.AcceptedGoods.Add(Harness.Grain);
			Inventory.InitialStock.Add({ Harness.Grain, FHansaQuantity::FromRaw(Stock) });
			Initialization.Inventories.Add(MoveTemp(Inventory));
		};
		AddInventory(Harness.LubeckInventoryId, EHansaInventoryOwnerKind::City, Harness.Lubeck, LubeckStock);
		AddInventory(Harness.RostockInventoryId, EHansaInventoryOwnerKind::City, Harness.Rostock, RostockStock);
		AddInventory(Harness.CargoInventoryId, EHansaInventoryOwnerKind::Vehicle, FHansaCityDefinitionId(), 0);

		Initialization.MarketSettings.UpdateCadenceTicks = 5;
		Initialization.MarketSettings.PriceHistoryCapacity = 8;
		Initialization.MarketSettings.TargetSmoothingBasisPoints = 2500;
		Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate = 1000;
		Initialization.MarketSettings.StaleAfterTicks = 10;
		FHansaCityMarketInitialization Market;
		Market.CityId = Harness.Rostock;
		Market.GoodId = Harness.Grain;
		Market.InventoryIds.Add(Harness.RostockInventoryId);
		Market.DesiredReserve = FHansaQuantity::FromRaw(10'000);
		Market.MinimumPriceMilliMarks = 100;
		Market.MaximumPriceMilliMarks = 5'000;
		Market.InitialPriceMilliMarks = 1'000;
		Market.bMarketOnly = true;
		Initialization.Markets.Add(MoveTemp(Market));
		Harness.State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
		return Harness;
	}

	FHansaCommandGatewayResult CreateRoute(FSettlementHarness& Harness, TArray<FHansaRouteStop> Stops)
	{
		FHansaCommandHeader Header;
		Header.CommandId = Entity<FHansaCommandId>(1);
		Header.GlobalSequence = 1;
		Header.Authority.IssuingHouseId = Harness.HouseId;
		Header.Authority.PrincipalId = Harness.HouseId.GetValue();
		Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Header.RequestedExecutionTick = Harness.State.CreateReadOnlyAccess(Harness.Definitions).GetClock().GetTick();
		FHansaCreateRouteCommand Payload;
		Payload.RouteId = Entity<FHansaRouteId>(1);
		Payload.VehicleId = Harness.VehicleId;
		Payload.RouteDefinitionId = Harness.SeaRoute;
		Payload.Stops = MoveTemp(Stops);
		Payload.bActivate = true;
		const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(Header, Payload);
		return FHansaGameplayCommandGateway::ExecuteTick(
			Harness.State, Harness.Definitions, MakeArrayView(&Command, 1), Harness.Cache);
	}

	FHansaCommandGatewayResult Step(FSettlementHarness& Harness)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(
			Harness.State, Harness.Definitions, TConstArrayView<FHansaGameplayCommand>(), Harness.Cache);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeForeignRouteSettlementCharacterizationTest,
	"Hansa.Simulation.Trade.ForeignRouteSettlementCharacterization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradeForeignRouteSettlementCharacterizationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::TradeSettlement;

	FSettlementHarness Sale = MakeHarness(50'000, 0);
	FHansaRouteStop SaleSource;
	SaleSource.CityId = Sale.Lubeck;
	SaleSource.Actions.Add({ EHansaRouteCargoActionKind::Load, EHansaRouteCargoCondition::Always,
		Sale.Grain, FHansaQuantity::FromRaw(15'000), FHansaQuantity() });
	FHansaRouteStop SaleDestination;
	SaleDestination.CityId = Sale.Rostock;
	SaleDestination.Actions.Add({ EHansaRouteCargoActionKind::Unload, EHansaRouteCargoCondition::Always,
		Sale.Grain, FHansaQuantity::FromRaw(15'000), FHansaQuantity() });
	TestTrue(TEXT("priced foreign sale route starts"), CreateRoute(
		Sale, { MoveTemp(SaleSource), MoveTemp(SaleDestination) }).IsSuccess());
	TestEqual(TEXT("local load transfers inventory without settling money"),
		Sale.State.CreateReadOnlyAccess(Sale.Definitions).GetHouses()[0].Money.GetRawValue(), int64(100'000));
	FHansaCommandGatewayResult SaleSettlement;
	for (int32 Tick = 0; Tick < 4; ++Tick) SaleSettlement = Step(Sale);
	TestTrue(TEXT("foreign sale tick succeeds"), SaleSettlement.IsSuccess());
	const FHansaSimulationReadOnlyAccess SaleRead = Sale.State.CreateReadOnlyAccess(Sale.Definitions);
	const FHansaRouteTransferRecord& SaleRecord = SaleRead.QueryRoute(Entity<FHansaRouteId>(1))->LastTransfer;
	TestEqual(TEXT("foreign unload is a priced sale"), SaleRecord.Kind, EHansaRouteCargoActionKind::Unload);
	TestEqual(TEXT("sale price includes current transaction friction"), SaleRecord.UnitPriceMilliMarks, int64(950));
	TestEqual(TEXT("sale proceeds round toward zero"), SaleRecord.SettledMoneyRaw, int64(14'250));
	TestEqual(TEXT("sale proceeds and three upkeep ticks change house money"),
		SaleRead.GetHouses()[0].Money.GetRawValue(), int64(114'244));
	TestTrue(TEXT("sale publishes an explicit settlement event"),
		SaleSettlement.GetEvents().ContainsByPredicate([](const FHansaDomainEvent& Event)
		{
			return Event.GetType() == EHansaDomainEventType::RouteTradeSettled &&
				Event.GetValue() == 14'250 && Event.GetRelatedValue() == 950;
		}));

	FSettlementHarness Purchase = MakeHarness(0, 50'000);
	FHansaRouteStop PurchaseOrigin;
	PurchaseOrigin.CityId = Purchase.Lubeck;
	FHansaRouteStop PurchaseSource;
	PurchaseSource.CityId = Purchase.Rostock;
	PurchaseSource.Actions.Add({ EHansaRouteCargoActionKind::Load, EHansaRouteCargoCondition::Always,
		Purchase.Grain, FHansaQuantity::FromRaw(15'000), FHansaQuantity() });
	TestTrue(TEXT("priced foreign purchase route starts"), CreateRoute(
		Purchase, { MoveTemp(PurchaseOrigin), MoveTemp(PurchaseSource) }).IsSuccess());
	FHansaCommandGatewayResult PurchaseSettlement;
	for (int32 Tick = 0; Tick < 4; ++Tick) PurchaseSettlement = Step(Purchase);
	TestTrue(TEXT("foreign purchase tick succeeds"), PurchaseSettlement.IsSuccess());
	const FHansaSimulationReadOnlyAccess PurchaseRead = Purchase.State.CreateReadOnlyAccess(Purchase.Definitions);
	const FHansaRouteTransferRecord& PurchaseRecord = PurchaseRead.QueryRoute(Entity<FHansaRouteId>(1))->LastTransfer;
	TestEqual(TEXT("foreign load is a priced purchase"), PurchaseRecord.Kind, EHansaRouteCargoActionKind::Load);
	TestEqual(TEXT("purchase price includes current transaction friction"), PurchaseRecord.UnitPriceMilliMarks, int64(1'050));
	TestEqual(TEXT("purchase cost rounds upward"), PurchaseRecord.SettledMoneyRaw, int64(-15'750));
	TestEqual(TEXT("purchase cost and three upkeep ticks change house money"),
		PurchaseRead.GetHouses()[0].Money.GetRawValue(), int64(84'244));
	TestEqual(TEXT("purchase moves physical stock into the Cog"),
		PurchaseRead.QueryVehicle(Purchase.VehicleId)->Cargo.GetRawValue(), int64(15'000));
	TestEqual(TEXT("purchase conserves Rostock plus Cog stock"),
		PurchaseRead.GetInventories().QueryStock(Purchase.RostockInventoryId, Purchase.Grain)->Stock.GetRawValue() +
		PurchaseRead.QueryVehicle(Purchase.VehicleId)->Cargo.GetRawValue(), int64(50'000));
	return !HasAnyErrors();
}
#endif
