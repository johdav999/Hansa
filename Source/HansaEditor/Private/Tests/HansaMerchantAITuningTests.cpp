#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "AI/HansaMerchantAI.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Presence/HansaForeignPresenceInitialization.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Systems/HansaSimulationPipeline.h"

namespace
{
	TArray<const UHansaDefinitionBase*> Raw(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<const UHansaDefinitionBase*> Result;
		for (const auto& Definition : Definitions) Result.Add(Definition.Get());
		return Result;
	}

	UHansaMerchantAITuningDefinition* FindTuning(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		for (const auto& Definition : Definitions)
		{
			if (auto* Tuning = Cast<UHansaMerchantAITuningDefinition>(Definition.Get())) return Tuning;
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantAITuningAuthoringTest,
	"Hansa.Content.AI.TuningSchemaAndReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMerchantAITuningAuthoringTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FHansaEditorSchemaRegistry SchemaRegistry;
	SchemaRegistry.Refresh();
	const FHansaDefinitionClassSchema* Schema = SchemaRegistry.FindSchema(UHansaMerchantAITuningDefinition::StaticClass());
	TestTrue(TEXT("Merchant AI tuning receives generic reflected editor coverage"), Schema != nullptr && Schema->IsValid());

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	UHansaMerchantAITuningDefinition* Tuning = FindTuning(Definitions);
	if (!TestNotNull(TEXT("The MVP contains one merchant tuning definition"), Tuning)) return false;
	const FHansaEconomicRegistryCompileResult Valid = FHansaEconomicDefinitionCompiler::Compile(Raw(Definitions));
	TestTrue(TEXT("The authored tuning compiles into the runtime registry"), Valid.IsValid());
	TestEqual(TEXT("Exactly one merchant tuning is compiled"), Valid.Registry.GetMerchantAITunings().Num(), 1);
	const FString ExportedSchema = SchemaRegistry.ExportJsonSchema(*Schema);
	for (const TCHAR* Field : {TEXT("ProtectedCashReservePfennig"), TEXT("ActionCooldownTicks"),
		TEXT("DirectTradeQuantityMilliUnits"), TEXT("StationOrderTargetMilliUnits"),
		TEXT("StationOrderCapMilliUnits"), TEXT("StationOrderBudgetPfennig"), TEXT("PresenceUtility")})
	{
		TestTrue(*FString::Printf(TEXT("%s is available to generic authoring"), Field), ExportedSchema.Contains(Field));
	}

	Tuning->TradePlans[0].RouteDefinitionId = TEXT("Route.Missing");
	Tuning->RefreshContentHash();
	const FHansaEconomicRegistryCompileResult Invalid = FHansaEconomicDefinitionCompiler::Compile(Raw(Definitions));
	TestFalse(TEXT("A missing AI route reference rejects the registry"), Invalid.IsValid());
	TestTrue(TEXT("The invalid reference has a stable actionable diagnostic"), Invalid.Issues.ContainsByPredicate([](const auto& Issue)
	{
		return Issue.Code == TEXT("HSA-REGISTRY-028");
	}));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantAITradePresenceParityTest,
	"Hansa.Integration.TradePresence.MerchantAICommandParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMerchantAITradePresenceParityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace Hansa::Simulation;
	auto Owned = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	TArray<const UHansaDefinitionBase*> DefinitionsRaw = Raw(Owned);
	auto Compiled = FHansaEconomicDefinitionCompiler::Compile(DefinitionsRaw);
	if (!TestTrue(TEXT("AI parity catalog compiles"), Compiled.IsValid())) return false;
	auto DefinitionsResult = FHansaSimulationDefinitionContext::TryCreate(
		FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value,
		Compiled.Registry.GetRegistryHash(), MoveTemp(Compiled.Registry));
	if (!TestTrue(TEXT("AI parity definitions validate"), DefinitionsResult.IsSuccess())) return false;
	FHansaSimulationDefinitionContext Definitions = MoveTemp(DefinitionsResult.Value);
	const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
	if (!TestNotNull(TEXT("AI parity registry exists"), Registry)) return false;

	const FHansaHouseId House = FHansaHouseId::TryCreate(2).Value;
	const FHansaCityDefinitionId Rostock = FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
	const FHansaCityDefinitionId Lubeck = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
	const FHansaGoodId Timber = FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;
	const FHansaGoodId Planks = FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
	const FHansaGoodId Tools = FHansaGoodId::TryParse(TEXT("Good.Tools")).Value;
	const FHansaVehicleId VehicleId = FHansaVehicleId::TryCreate(1).Value;
	const FHansaInventoryId CargoId = FHansaInventoryId::TryCreate(1).Value;
	const FHansaInventoryId MarketId = FHansaInventoryId::TryCreate(2).Value;

	FHansaSimulationInitialization Initial;
	Initial.Clock = FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value, FHansaSimulationTick()).Value;
	Initial.CampaignSeed = 0x54523131;
	Initial.Houses.Add({House, FHansaMoney::FromRaw(500000)});
	Initial.Cities.Add({Rostock, {}}); Initial.Cities.Add({Lubeck, {}});
	Initial.Research.Add({House, 0});
	FHansaVehicleState Vehicle; Vehicle.Id=VehicleId; Vehicle.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;
	Vehicle.OwnerId=House; Vehicle.CargoInventoryId=CargoId; Vehicle.Mode=EHansaRouteMode::Sea;
	Vehicle.Capacity=FHansaQuantity::FromRaw(12000); Vehicle.CurrentCityId=Rostock; Initial.Vehicles.Add(Vehicle);
	FHansaInventoryInitialization Cargo; Cargo.Id=CargoId; Cargo.OwnerKind=EHansaInventoryOwnerKind::Vehicle; Cargo.VehicleId=VehicleId;
	Cargo.Capacity=Vehicle.Capacity; Cargo.AcceptedGoods={Timber,Planks,Tools};
	Cargo.InitialStock={{Timber,FHansaQuantity::FromRaw(8000)},{Planks,FHansaQuantity::FromRaw(4000)}}; Initial.Inventories.Add(Cargo);
	FHansaInventoryInitialization MarketInventory; MarketInventory.Id=MarketId; MarketInventory.OwnerKind=EHansaInventoryOwnerKind::City;
	MarketInventory.CityId=Rostock; MarketInventory.Capacity=FHansaQuantity::FromRaw(200000); MarketInventory.AcceptedGoods={Planks,Tools};
	MarketInventory.InitialStock={{Planks,FHansaQuantity::FromRaw(50000)},{Tools,FHansaQuantity::FromRaw(50000)}}; Initial.Inventories.Add(MarketInventory);
	Initial.MarketSettings.UpdateCadenceTicks=1; Initial.MarketSettings.PriceHistoryCapacity=8;
	for (const FHansaGoodId Good : {Planks, Tools})
	{
		FHansaCityMarketInitialization Market; Market.CityId=Rostock; Market.GoodId=Good; Market.InventoryIds.Add(MarketId);
		Market.DesiredReserve=FHansaQuantity::FromRaw(10000); Market.MinimumPriceMilliMarks=100; Market.MaximumPriceMilliMarks=5000;
		Market.InitialPriceMilliMarks=1000; Market.InitialLastUpdateTick=0; Market.InitialReportTick=0; Initial.Markets.Add(Market);
	}
	if (!TestTrue(TEXT("Rostock visiting presence seeds"), FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Initial, *Registry))) return false;
	FHansaForeignPresenceState& Presence = Initial.ForeignPresences[0];
	Presence.Contributions.LawfulTradeVolumeMilliUnits=300000; Presence.Contributions.CompletedDeliveryCount=12;
	Presence.Contributions.InvestedPfennig=100000; Presence.Contributions.TransactionValuePfennig=250000;
	Presence.Contributions.FulfilledShortageMilliUnits=50000; Presence.Contributions.ReliableOperatingTicks=12;
	Presence.Contributions.SolventOperatingTicks=8;
	auto StateResult = FHansaSimulationState::TryCreate(MoveTemp(Initial));
	if (!TestTrue(TEXT("AI parity state validates"), StateResult.IsSuccess())) return false;
	FHansaSimulationState State = MoveTemp(StateResult.Value);
	FHansaSimulationTransientCache Cache;
	FHansaCompiledMerchantAITuning Tuning = Registry->GetMerchantAITunings()[0];
	Tuning.DecisionCadenceTicks=1; Tuning.ActionCooldownTicks=0; Tuning.ProtectedCashReservePfennig=1000;
	Tuning.PresenceUtility=10000; Tuning.ResearchUtility=-10000; Tuning.ProductionUtility=-10000;
	Tuning.StationOrderTargetMilliUnits=20000; Tuning.StationOrderCapMilliUnits=20000; Tuning.StationOrderBudgetPfennig=50000;
	Tuning.PreferredResearchTechnologyIds.Reset(); Tuning.TradePlans.Reset();
	Tuning.TradePlans.Add({TEXT("MerchantPlan.RostockPlanks"),TEXT("Route.BalticSea"),TEXT("Vehicle.Cog"),TEXT("City.Rostock"),TEXT("City.Lubeck"),TEXT("Good.Planks"),20000,10000,0});
	Tuning.TradePlans.Add({TEXT("MerchantPlan.RostockTools"),TEXT("Route.BalticSea"),TEXT("Vehicle.Cog"),TEXT("City.Rostock"),TEXT("City.Lubeck"),TEXT("Good.Tools"),20000,10000,0});
	FHansaMerchantAIController Controller;
	uint64 CommandValue = 1;
	auto DecideAndExecute = [&](EHansaGameplayCommandType Expected)
	{
		const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
		FHansaMerchantAIDecision Decision = Controller.Evaluate(View, *Registry, Tuning, House, 2,
			FHansaCommandId::TryCreate(CommandValue++).Value);
		if (!TestTrue(*FString::Printf(TEXT("AI emits %s through the normal command envelope"), LexToString(Expected)), Decision.Command.IsSet())) return false;
		if (!TestEqual(TEXT("AI command type matches the selected option"), Decision.Command->GetType(), Expected)) return false;
		const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions,
			MakeArrayView(&Decision.Command.GetValue(), 1), Cache);
		Controller.RecordGatewayOutcome(Result);
		return TestTrue(TEXT("The ordinary gateway accepts the AI command"), Result.IsSuccess());
	};
	auto AdvanceUntil = [&](auto Predicate, int32 Limit)
	{
		for (int32 Index=0; Index<Limit && !Predicate(); ++Index)
			if (!FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache).IsSuccess()) return false;
		return Predicate();
	};

	TestTrue(TEXT("AI proposes the authored station site"), DecideAndExecute(EHansaGameplayCommandType::ProposeTradeStation));
	TestTrue(TEXT("AI funds the proposal with owned cargo and money"), DecideAndExecute(EHansaGameplayCommandType::FundTradeStation));
	TestTrue(TEXT("Station completes through ordinary construction ticks"), AdvanceUntil([&]()
	{
		const auto Stations=State.CreateReadOnlyAccess(Definitions).GetTradeStations();
		return !Stations.IsEmpty() && Stations[0].Status==EHansaTradeStationStatus::Active;
	}, 5));
	TestTrue(TEXT("AI creates first station order"), DecideAndExecute(EHansaGameplayCommandType::ManageStationOrder));
	TestTrue(TEXT("AI creates second station order"), DecideAndExecute(EHansaGameplayCommandType::ManageStationOrder));
	TestTrue(TEXT("Both orders execute through market settlement"), AdvanceUntil([&]()
	{
		const auto Stations=State.CreateReadOnlyAccess(Definitions).GetTradeStations();
		if (Stations.IsEmpty() || Stations[0].Orders.Num()!=2) return false;
		return Stations[0].Orders[0].History.ContainsByPredicate([](const auto& E){return E.AppliedMilliUnits>0;}) &&
			Stations[0].Orders[1].History.ContainsByPredicate([](const auto& E){return E.AppliedMilliUnits>0;});
	}, 3));
	TestTrue(TEXT("AI requests earned merchant-office progression"), DecideAndExecute(EHansaGameplayCommandType::RequestPresenceUpgrade));
	TestTrue(TEXT("AI funds progression from its station inventory"), DecideAndExecute(EHansaGameplayCommandType::FundPresenceUpgrade));
	TestTrue(TEXT("Merchant office completes normally"), AdvanceUntil([&]()
	{
		const auto P=State.CreateReadOnlyAccess(Definitions).QueryForeignPresence(House,Rostock);
		return P.IsSet() && P->CurrentStageId==TEXT("PresenceStage.MerchantOffice");
	}, 5));
	TestTrue(TEXT("AI chooses one affordable authored branch"), DecideAndExecute(EHansaGameplayCommandType::ApplyPresenceSpecialization));

	const auto Read = State.CreateReadOnlyAccess(Definitions);
	const auto FinalPresence = Read.QueryForeignPresence(House,Rostock);
	TestTrue(TEXT("AI owns an active station and merchant office"), FinalPresence.IsSet() && FinalPresence->StationId.IsValid() &&
		FinalPresence->CurrentStageId==TEXT("PresenceStage.MerchantOffice"));
	int32 SelectedBranchCount = 0;
	if (FinalPresence.IsSet()) for (const auto& Branch : FinalPresence->Specializations) SelectedBranchCount += Branch.bSelected ? 1 : 0;
	TestEqual(TEXT("Exactly one exclusive branch is selected"), SelectedBranchCount, 1);
	TestTrue(TEXT("Station orders moved physical goods"), Read.QueryTradeStation(FinalPresence->StationId)->StorageUsed.GetRawValue()>0);
	TestTrue(TEXT("Money remains above the authored reserve"), Read.GetHouses()[0].Money.GetRawValue()>=Tuning.ProtectedCashReservePfennig);
	const TArray<FHansaMerchantAIExplanationProjection> Explanations = Controller.BuildExplanationProjection();
	TestTrue(TEXT("Allowlisted explanations cover the accepted command journey"), Explanations.Num()>=7 &&
		Explanations.ContainsByPredicate([](const auto& Item){return Item.Action==TEXT("ApplyPresenceSpecialization") && Item.bCommandAccepted;}));
	for (const auto& Item : Explanations)
	{
		TestFalse(TEXT("Explanation omits private inventory identity"), Item.Reason.Contains(TEXT("Inventory")));
		TestFalse(TEXT("Explanation omits private order terms"), Item.Reason.Contains(TEXT("20000")));
	}
	return !HasAnyErrors();
}

#endif
