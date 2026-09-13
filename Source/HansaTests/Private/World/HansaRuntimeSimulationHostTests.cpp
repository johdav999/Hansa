#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaBakeryPresentation.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRuntimePlayableShortageScenarioTest,
	"Hansa.UI.RuntimeScenario.PlayableShortageProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRuntimePlayableShortageScenarioTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The default playable shortage scenario initializes"),
		Host->InitializeForLubeck(nullptr, Error)))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("The default runtime scenario is the populated shortage"), Host->GetScenario(),
		EHansaRuntimeScenario::LubeckGrainShortage);
	const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
	if (!TestNotNull(TEXT("The playable scenario exposes the cooked authored registry"), Registry)) return false;
	TestEqual(TEXT("All expanded MVP goods are available"), Registry->GetGoods().Num(), 13);
	TestEqual(TEXT("All expanded MVP recipes are available"), Registry->GetRecipes().Num(), 11);
	TestEqual(TEXT("All expanded MVP buildings are available"), Registry->GetBuildings().Num(), 17);
	TestEqual(TEXT("All MVP needs are available"), Registry->GetNeeds().Num(), 5);
	TestEqual(TEXT("Both MVP population tiers are available"), Registry->GetPopulationTiers().Num(), 2);
	TestEqual(TEXT("All MVP city-market profiles are available"), Registry->GetCityMarkets().Num(), 4);
	TestEqual(TEXT("All MVP research technologies are available"), Registry->GetTechnologies().Num(), 9);

	const auto InitialProjection = Host->BuildProjection();
	if (!TestTrue(TEXT("The populated state produces a projection"), InitialProjection.IsSuccess())) return false;
	TestEqual(TEXT("The player and one merchant rival receive authoritative research state"), InitialProjection.Value.GetResearch().Num(), 2);
	if (!InitialProjection.Value.GetResearch().IsEmpty())
	{
		TestEqual(TEXT("The playable house starts with the deterministic MVP research budget"),
			InitialProjection.Value.GetResearch()[0].AvailableResearchPoints, 1'000);
	}
	TestTrue(TEXT("The city overview receives population rows"), !InitialProjection.Value.GetPopulationCohorts().IsEmpty());
	TestTrue(TEXT("The city overview receives production rows"), !InitialProjection.Value.GetProductions().IsEmpty());
	TestEqual(TEXT("The runtime contains all four city markets and ten goods each"),
		InitialProjection.Value.GetMarkets().Num(), 40);
	TestEqual(TEXT("The runtime contains Lübeck plus three simulated market-only cities"),
		InitialProjection.Value.GetCityCount(), 4);
	TestTrue(TEXT("The playable world starts with visible building placements"),
		!InitialProjection.Value.GetPlacements().IsEmpty());
	TestTrue(TEXT("The playable city has residents"), InitialProjection.Value.GetTotalResidents() > 0);
	const FHansaCompiledBuildingDefinition* MarketDefinition = Registry->FindBuilding(TEXT("Building.Market"));
	TestTrue(TEXT("The authored Market is the physical local endpoint"),
		MarketDefinition != nullptr && MarketDefinition->bProvidesMarketAccess && MarketDefinition->bRequiresRoad);
	const FHansaInventoryProjection* LubeckInventory = InitialProjection.Value.GetInventories().FindByPredicate([](const FHansaInventoryProjection& Inventory)
	{
		return Inventory.OwnerKind == EHansaInventoryOwnerKind::City && Inventory.CityId.ToString() == TEXT("City.Lubeck");
	});
	const FHansaInventoryProjection* RostockInventory = InitialProjection.Value.GetInventories().FindByPredicate([](const FHansaInventoryProjection& Inventory)
	{
		return Inventory.OwnerKind == EHansaInventoryOwnerKind::City && Inventory.CityId.ToString() == TEXT("City.Rostock");
	});
	TestTrue(TEXT("Lübeck stock is bound to the placed physical market endpoint"),
		LubeckInventory != nullptr && LubeckInventory->BuildingId.GetValue() == 14 && LubeckInventory->Stocks.Num() == 10);
	TestTrue(TEXT("Rostock remains a valid remote market stock endpoint"),
		RostockInventory != nullptr && RostockInventory->Stocks.Num() == 10);
	TestTrue(TEXT("The playable bootstrap can afford a road reconnection"),
		Host->QueryConstructionCost(TEXT("Building.Road")).IsAffordable());

	TestTrue(TEXT("The runtime reaches the first market cadence"), Host->AdvanceTicks(5));
	const auto ShortageProjection = Host->BuildProjection();
	if (!TestTrue(TEXT("The shortage state remains projectable"), ShortageProjection.IsSuccess())) return false;
	const auto GrainId = FHansaGoodId::TryParse(TEXT("Good.Grain"));
	if (!TestTrue(TEXT("The grain stable ID parses"), GrainId.IsSuccess())) return false;
	TestEqual(TEXT("Every authored city/good still has a market projection after the first cadence"),
		ShortageProjection.Value.GetMarkets().Num(), 40);
	for (const auto& Market : ShortageProjection.Value.GetMarkets())
	{
		TestTrue(*FString::Printf(TEXT("%s has a fresh runtime report"), *Market.GoodId.ToString()),
			Market.LastUpdateTick >= 0);
	}
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	if (!TestTrue(TEXT("The Lübeck stable ID parses"), CityId.IsSuccess())) return false;
	TStrongObjectPtr<UHansaMarketTablePresentationModel> MarketModel(NewObject<UHansaMarketTablePresentationModel>());
	MarketModel->InitializeDefaults();
	MarketModel->ApplyProjection(ShortageProjection.Value, *Registry, CityId.Value);
	TestEqual(TEXT("The playable Market table presents all ten goods"), MarketModel->GetSnapshot().AllRows.Num(), 10);
	TestFalse(TEXT("No playable Market row falls back to the unknown/dash state"),
		MarketModel->GetSnapshot().AllRows.ContainsByPredicate([](const auto& Row) { return Row.bUnknown; }));
	TestTrue(TEXT("The shortage produces an active grain alert"),
		ShortageProjection.Value.GetActiveMarketAlerts().ContainsByPredicate([&GrainId](const auto& Alert)
		{
			return Alert.GoodId == GrainId.Value;
		}));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRuntimeSimulationHostClockTest,
	"Hansa.Integration.RuntimeSimulationHost.PauseAndSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRuntimeSimulationHostClockTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The runtime simulation host initializes"), Host->InitializeForLubeck(
		nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild)))
	{
		AddError(Error);
		return false;
	}

	Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
	TestTrue(TEXT("Paused frame time is accepted"), Host->AdvanceRealTime(10.0));
	TestEqual(TEXT("Pause advances no authoritative ticks"), Host->GetSimulationTick(), int64(0));

	Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
	TestTrue(TEXT("A partial normal-speed interval is accumulated"), Host->AdvanceRealTime(0.5));
	TestEqual(TEXT("A partial normal-speed interval does not advance early"), Host->GetSimulationTick(), int64(0));
	TestTrue(TEXT("The second partial interval completes a normal-speed tick"), Host->AdvanceRealTime(0.5));
	TestEqual(TEXT("Normal speed advances one tick per second"), Host->GetSimulationTick(), int64(1));

	Host->SetSpeed(EHansaRuntimeSimulationSpeed::Fast);
	TestTrue(TEXT("Fast frame time advances"), Host->AdvanceRealTime(0.25));
	TestEqual(TEXT("Fast speed advances four ticks per second"), Host->GetSimulationTick(), int64(2));

	Host->SetSpeed(EHansaRuntimeSimulationSpeed::Fastest);
	TestTrue(TEXT("Fastest frame time advances"), Host->AdvanceRealTime(0.25));
	TestEqual(TEXT("Fastest speed is capped to one authoritative tick per rendered frame"),
		Host->GetSimulationTick(), int64(3));
	TestTrue(TEXT("A tiny following frame is accepted"), Host->AdvanceRealTime(0.001));
	TestEqual(TEXT("Overload debt was dropped instead of caught up on the following frame"),
		Host->GetSimulationTick(), int64(3));
	TestTrue(TEXT("A long overloaded frame is accepted"), Host->AdvanceRealTime(1.0));
	TestEqual(TEXT("A long frame still advances at most one authoritative tick"),
		Host->GetSimulationTick(), int64(4));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPhysicalMarketConnectivityJourneyTest,
	"Hansa.Integration.RuntimeSimulationHost.PhysicalMarketConnectivityJourney",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPhysicalMarketConnectivityJourneyTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The playable city initializes for the road-connectivity journey"),
		Host->InitializeForLubeck(nullptr, Error)))
	{
		AddError(Error);
		return false;
	}

	const auto FindBuilding = [&Host](const uint64 Value) -> TOptional<FHansaBuildingWorldProjection>
	{
		const auto Projection = Host->BuildProjection();
		if (!Projection) return {};
		const FHansaBuildingWorldProjection* Found = Projection.Value.GetBuildingWorldProjections().FindByPredicate(
			[Value](const FHansaBuildingWorldProjection& Building) { return Building.BuildingId.GetValue() == Value; });
		return Found != nullptr ? TOptional<FHansaBuildingWorldProjection>(*Found) : TOptional<FHansaBuildingWorldProjection>();
	};
	const TOptional<FHansaBuildingWorldProjection> InitialBakery = FindBuilding(3);
	if (!TestTrue(TEXT("The bakery has an authoritative world projection before any delivery request"), InitialBakery.IsSet())) return false;
	TestTrue(TEXT("The disconnected-producer query is populated proactively"), InitialBakery->bRequiresRoad && InitialBakery->bHasMarketAccess);
	TestEqual(TEXT("The bakery selects the authored physical market"), InitialBakery->SelectedMarketBuildingId.GetValue(), uint64(14));
	TestTrue(TEXT("The bakery reports a positive road distance"), InitialBakery->MarketRoadDistanceCells > 0);

	const auto RoadCost = Host->QueryConstructionCost(TEXT("Building.Road"));
	TestTrue(TEXT("The bootstrap can afford the reconnect road"), RoadCost.IsAffordable());
	TestTrue(TEXT("The first normal demolition command removes one side of the road spine"),
		Host->RemoveBuilding(FHansaBuildingId::TryCreate(24).Value).IsSuccess());
	TestTrue(TEXT("The second normal demolition command severs the remaining road spine"),
		Host->RemoveBuilding(FHansaBuildingId::TryCreate(45).Value).IsSuccess());

	const TOptional<FHansaBuildingWorldProjection> DisconnectedBakery = FindBuilding(3);
	if (!TestTrue(TEXT("The bakery remains queryable with no active delivery request"), DisconnectedBakery.IsSet())) return false;
	TestFalse(TEXT("The authoritative projection reports the severed market connection"), DisconnectedBakery->bHasMarketAccess);
	TestEqual(TEXT("The severed network has a typed cause"), DisconnectedBakery->MarketAccessFailure,
		EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket);

	TStrongObjectPtr<UHansaInspectorPresentationModel> Inspector(NewObject<UHansaInspectorPresentationModel>());
	Inspector->BindRuntime(Host.Get());
	Inspector->InitializeDefaults();
	Inspector->ShowWorldBuilding(TEXT("Building.Bakery"), FText::FromString(TEXT("Bakery")), FText(), 3,
		TEXT("Ready"), TEXT("None"), TEXT("World.Building.3"));
	TestEqual(TEXT("Player feedback distinguishes an in-flight delivery blocked by the severed route"),
		Inspector->GetSnapshot().Causal.Problem.ToString(), FString(TEXT("Delivery blocked")));
	TestTrue(TEXT("Player feedback supplies a road reconnection remedy"),
		Inspector->GetSnapshot().Causal.Remedy.ToString().Contains(TEXT("Reconnect the broken road segment")));
	TestTrue(TEXT("The inspector exposes the same typed market-access state"),
		Inspector->GetSnapshot().Flows.ContainsByPredicate([](const FHansaInspectorFlowPresentation& Row)
		{
			return Row.StableId == TEXT("Inspector.Logistics.MarketAccess") && Row.bProblem;
		}));

	FHansaPlacementSpec Reconnect;
	Reconnect.CityId = Host->GetCityId();
	Reconnect.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
	Reconnect.Anchor = { 14, 19 };
	TestTrue(TEXT("The player can place the reconnect segment through the normal command gateway"),
		Host->PlaceBuildings(MakeArrayView(&Reconnect, 1)).IsSuccess());
	const FHansaCompiledBuildingDefinition* RoadDefinition = Host->FindBuildingDefinition(TEXT("Building.Road"));
	if (!TestNotNull(TEXT("The authored road definition is available"), RoadDefinition)) return false;
	TestTrue(TEXT("The player can complete the reconnect segment"), Host->AdvanceTicks(RoadDefinition->BuildTicks));
	TestTrue(TEXT("The paused delivery gets a simulation tick to resume on the restored route"), Host->AdvanceTicks(1));

	const TOptional<FHansaBuildingWorldProjection> ReconnectedBakery = FindBuilding(3);
	if (!TestTrue(TEXT("The bakery remains projected after reconnection"), ReconnectedBakery.IsSet())) return false;
	TestTrue(TEXT("The authoritative path recovers automatically"), ReconnectedBakery->bHasMarketAccess);
	TestEqual(TEXT("The same physical market is selected after recovery"), ReconnectedBakery->SelectedMarketBuildingId.GetValue(), uint64(14));
	Inspector->ShowWorldBuilding(TEXT("Building.Bakery"), FText::FromString(TEXT("Bakery")), FText(), 3,
		TEXT("Ready"), TEXT("None"), TEXT("World.Building.3"));
	TestTrue(TEXT("The recovered inspector reports the selected market and distance"),
		Inspector->GetSnapshot().Flows.ContainsByPredicate([](const FHansaInspectorFlowPresentation& Row)
		{
			return Row.StableId == TEXT("Inspector.Logistics.MarketAccess") && !Row.bProblem &&
				Row.Value.ToString().Contains(TEXT("Market #14")) && Row.Value.ToString().Contains(TEXT("road cells"));
		}));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPhysicalMarketProductionFamilyPolicyTest,
	"Hansa.Integration.PhysicalMarketConnectivity.ProductionFamiliesSharePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPhysicalMarketProductionFamilyPolicyTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The playable city initializes for production-family policy acceptance"),
		Host->InitializeForLubeck(nullptr, Error)))
	{
		AddError(Error);
		return false;
	}
	const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
	if (!TestNotNull(TEXT("The reviewed economic registry is available"), Registry)) return false;

	for (const TCHAR* StableId : {
		TEXT("Building.GrainFarm"), TEXT("Building.Mill"), TEXT("Building.Bakery"),
		TEXT("Building.Fishery"), TEXT("Building.LumberCamp"), TEXT("Building.Sawmill") })
	{
		const FHansaCompiledBuildingDefinition* Definition = Registry->FindBuilding(StableId);
		if (!TestNotNull(*FString::Printf(TEXT("%s is present in the reviewed catalog"), StableId), Definition))
			continue;
		TestTrue(*FString::Printf(TEXT("%s requires the shared road-to-market operating path"), StableId),
			Definition->SchemaVersion >= 5 && Definition->bRequiresRoad);
		TestTrue(*FString::Printf(TEXT("%s has an authored production recipe"), StableId),
			!Definition->RecipeIds.IsEmpty());
	}
	const FHansaCompiledBuildingDefinition* Fishery = Registry->FindBuilding(TEXT("Building.Fishery"));
	TestTrue(TEXT("The Fishery requires shoreline and road access"),
		Fishery != nullptr && Fishery->bRequiresShoreline && Fishery->bRequiresRoad);

	int32 MarketProviderCount = 0;
	for (const FHansaCompiledBuildingDefinition& Definition : Registry->GetBuildings())
	{
		if (!Definition.bProvidesMarketAccess) continue;
		++MarketProviderCount;
		TestEqual(TEXT("Only the authored Market grants local economic access"),
			Definition.StableId, FString(TEXT("Building.Market")));
	}
	TestEqual(TEXT("The reviewed catalog has one physical market provider"), MarketProviderCount, 1);

	const auto Projection = Host->BuildProjection();
	if (!TestTrue(TEXT("The representative city projects its prebuilt bread chain"), Projection.IsSuccess())) return false;
	for (const uint64 BuildingValue : { uint64(1), uint64(2), uint64(3) })
	{
		const FHansaBuildingWorldProjection* Building = Projection.Value.GetBuildingWorldProjections().FindByPredicate(
			[BuildingValue](const FHansaBuildingWorldProjection& Candidate)
			{
				return Candidate.BuildingId.GetValue() == BuildingValue;
			});
		TestTrue(*FString::Printf(TEXT("Bread-chain building %llu uses the connected physical market"),
			static_cast<unsigned long long>(BuildingValue)),
			Building != nullptr && Building->bRequiresRoad && Building->bHasMarketAccess &&
			Building->SelectedMarketBuildingId.GetValue() == 14);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPhysicalMarketConnectivityPerformanceTest,
	"Hansa.Integration.PhysicalMarketConnectivity.RepresentativeMvpCityPerformance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPhysicalMarketConnectivityPerformanceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The representative MVP city initializes for profiling"), Host->InitializeForLubeck(nullptr, Error)))
	{
		AddError(Error);
		return false;
	}

	constexpr int32 ProjectionPasses = 200;
	int64 ProjectionChecksum = 0;
	auto MeasureProjectionPasses = [&Host, &ProjectionChecksum, this](const TCHAR* Phase) -> double
	{
		const double StartSeconds = FPlatformTime::Seconds();
		for (int32 Pass = 0; Pass < ProjectionPasses; ++Pass)
		{
			const auto Projection = Host->BuildProjection();
			if (!Projection.IsSuccess())
			{
				AddError(FString::Printf(TEXT("Projection failed during %s performance sampling."), Phase));
				return -1.0;
			}
			for (const FHansaBuildingWorldProjection& Building : Projection.Value.GetBuildingWorldProjections())
			{
				if (Building.bRequiresRoad)
				{
					ProjectionChecksum += Building.bHasMarketAccess ? Building.MarketRoadDistanceCells + 1 : 1;
				}
			}
		}
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0 / ProjectionPasses;
	};

	const auto InitialProjection = Host->BuildProjection();
	if (!TestTrue(TEXT("The representative city produces an initial projection"), InitialProjection.IsSuccess())) return false;
	int32 RoadDependentBuildings = 0;
	for (const FHansaBuildingWorldProjection& Building : InitialProjection.Value.GetBuildingWorldProjections())
	{
		RoadDependentBuildings += Building.bRequiresRoad ? 1 : 0;
	}
	TestTrue(TEXT("The profile covers the seven road-dependent buildings in the representative opening"),
		RoadDependentBuildings >= 7);
	const double ConnectedProjectionAverageMs = MeasureProjectionPasses(TEXT("connected"));

	TestTrue(TEXT("Normal road removal enters the profiling scenario"),
		Host->RemoveBuilding(FHansaBuildingId::TryCreate(24).Value).IsSuccess() &&
		Host->RemoveBuilding(FHansaBuildingId::TryCreate(45).Value).IsSuccess());
	const double DisconnectedProjectionAverageMs = MeasureProjectionPasses(TEXT("disconnected"));

	FHansaPlacementSpec Reconnect;
	Reconnect.CityId = Host->GetCityId();
	Reconnect.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
	Reconnect.Anchor = { 14, 19 };
	TestTrue(TEXT("Normal road construction restores the profiled topology"),
		Host->PlaceBuildings(MakeArrayView(&Reconnect, 1)).IsSuccess());
	const FHansaCompiledBuildingDefinition* RoadDefinition = Host->FindBuildingDefinition(TEXT("Building.Road"));
	if (!TestNotNull(TEXT("The road definition is available to complete reconnection"), RoadDefinition)) return false;
	TestTrue(TEXT("The profiled road completes"), Host->AdvanceTicks(RoadDefinition->BuildTicks + 1));

	constexpr int32 SimulationTicks = 120;
	const double TickStartSeconds = FPlatformTime::Seconds();
	TestTrue(TEXT("The connected representative economy advances through the profiling window"),
		Host->AdvanceTicks(SimulationTicks));
	const double SimulationAverageMs = (FPlatformTime::Seconds() - TickStartSeconds) * 1000.0 / SimulationTicks;

	const FString EvidenceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling"));
	IFileManager::Get().MakeDirectory(*EvidenceDirectory, true);
	const FString EvidencePath = FPaths::Combine(EvidenceDirectory, TEXT("physical-market-connectivity.json"));
	const FString Evidence = FString::Printf(
		TEXT("{\n  \"scenario\": \"lubeck_grain_shortage_v1\",\n  \"projectionPassesPerState\": %d,\n  \"roadDependentBuildings\": %d,\n  \"connectedProjectionAverageMs\": %.6f,\n  \"disconnectedProjectionAverageMs\": %.6f,\n  \"simulationTicks\": %d,\n  \"simulationAverageMs\": %.6f,\n  \"checksum\": %lld\n}\n"),
		ProjectionPasses, RoadDependentBuildings, ConnectedProjectionAverageMs,
		DisconnectedProjectionAverageMs, SimulationTicks, SimulationAverageMs,
		static_cast<long long>(ProjectionChecksum));
	TestTrue(TEXT("Performance evidence is written"), FFileHelper::SaveStringToFile(Evidence, *EvidencePath));
	AddInfo(FString::Printf(TEXT("Physical market profile: connected %.3f ms/projection, disconnected %.3f ms/projection, simulation %.3f ms/tick. Evidence: %s"),
		ConnectedProjectionAverageMs, DisconnectedProjectionAverageMs, SimulationAverageMs, *EvidencePath));
	TestTrue(TEXT("Connected projection cost stays below the 25 ms material-regression budget"),
		ConnectedProjectionAverageMs >= 0.0 && ConnectedProjectionAverageMs < 25.0);
	TestTrue(TEXT("Disconnected projection cost stays below the 25 ms material-regression budget"),
		DisconnectedProjectionAverageMs >= 0.0 && DisconnectedProjectionAverageMs < 25.0);
	TestTrue(TEXT("Simulation cost stays below the 25 ms per-tick material-regression budget"),
		SimulationAverageMs >= 0.0 && SimulationAverageMs < 25.0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRuntimeSimulationHostConstructionProjectionTest,
	"Hansa.Integration.RuntimeSimulationHost.ConstructionProjectionCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRuntimeSimulationHostConstructionProjectionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaRuntimeSimulationHostTestWorld"));
	if (!TestNotNull(TEXT("A transient gameplay world is created"), World)) return false;
	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaPlacementProjectionManager* Manager = World->SpawnActor<AHansaPlacementProjectionManager>();
	if (!TestNotNull(TEXT("The Lubeck foundation is available"), Foundation) ||
		!TestNotNull(TEXT("The projection manager is available"), Manager))
	{
		World->DestroyWorld(false);
		return false;
	}

	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The runtime host binds to the gameplay world"), Host->InitializeForLubeck(
		World, Error, EHansaRuntimeScenario::EmptyLubeckBuild)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	const auto RoadDefinition = FHansaBuildingTypeId::TryParse(TEXT("Building.Road"));
	const auto BakeryDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery"));
	const FHansaCompiledBuildingDefinition* BakeryDefinition = Host->FindBuildingDefinition(TEXT("Building.Bakery"));
	if (!TestTrue(TEXT("Runtime building identities are valid"),
		RoadDefinition.IsSuccess() && BakeryDefinitionId.IsSuccess() && BakeryDefinition != nullptr))
	{
		World->DestroyWorld(false);
		return false;
	}

	FHansaPlacementSpec Road;
	Road.CityId = Host->GetCityId();
	Road.BuildingDefinitionId = RoadDefinition.Value;
	Road.Anchor = { 18, 16 };
	TestTrue(TEXT("Road placement enters the runtime host"), Host->PlaceBuildings(MakeArrayView(&Road, 1)).IsSuccess());
	FHansaPlacementSpec Bakery;
	Bakery.CityId = Host->GetCityId();
	Bakery.BuildingDefinitionId = BakeryDefinitionId.Value;
	Bakery.Anchor = { 15, 16 };
	TestTrue(TEXT("Bakery placement enters the same runtime host"), Host->PlaceBuildings(MakeArrayView(&Bakery, 1)).IsSuccess());

	const FHansaBuildingId BakeryId = FHansaBuildingId::TryCreate(2).Value;
	AHansaBuildingWorldProjectionActor* BakeryActor = Manager->FindProjectionActor(BakeryId);
	auto* BakeryArt = BakeryActor ? Cast<AHansaBakeryPresentation>(BakeryActor->BuildingPresentation->GetChildActor()) : nullptr;
	TestTrue(TEXT("Placement events create the bakery projection Actor"), BakeryActor != nullptr &&
		BakeryActor->GetWorldStatus() == EHansaBuildingWorldStatus::UnderConstruction &&
		BakeryArt && BakeryArt->Construction->IsVisible() && !BakeryActor->ConstructionPlaceholder->IsVisible());
	TestTrue(TEXT("The authored construction duration completes construction"),
		Host->AdvanceTicks(BakeryDefinition->BuildTicks));
	BakeryActor = Manager->FindProjectionActor(BakeryId);
	TestTrue(TEXT("Completion events update the same projection Actor to its ready visual"), BakeryActor != nullptr &&
		BakeryActor->GetWorldStatus() == EHansaBuildingWorldStatus::Ready &&
		BakeryArt == BakeryActor->BuildingPresentation->GetChildActor() && BakeryArt &&
		BakeryArt->Bakery->IsVisible() && !BakeryArt->Construction->IsVisible() &&
		!BakeryActor->ConstructionPlaceholder->IsVisible());
	Manager->TearDownProjections();
	World->DestroyWorld(false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRuntimeRoadAndMarketAccessIndependenceTest,
	"Hansa.Integration.RuntimeSimulationHost.RoadAndMarketAccessAreIndependent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRuntimeRoadAndMarketAccessIndependenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The empty player city initializes"), Host->InitializeForLubeck(
		nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild)))
	{
		AddError(Error);
		return false;
	}

	FHansaPlacementSpec Road;
	Road.CityId = Host->GetCityId();
	Road.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
	Road.Anchor = { 18, 16 };
	TestTrue(TEXT("The adjacent road is placed"), Host->PlaceBuildings(MakeArrayView(&Road, 1)).IsSuccess());

	FHansaPlacementSpec Bakery;
	Bakery.CityId = Host->GetCityId();
	Bakery.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery")).Value;
	Bakery.Anchor = { 15, 16 };
	TestTrue(TEXT("The road-dependent building is placed"), Host->PlaceBuildings(MakeArrayView(&Bakery, 1)).IsSuccess());

	const FHansaCompiledBuildingDefinition* RoadDefinition = Host->FindBuildingDefinition(TEXT("Building.Road"));
	const FHansaCompiledBuildingDefinition* BakeryDefinition = Host->FindBuildingDefinition(TEXT("Building.Bakery"));
	if (!TestNotNull(TEXT("Road definition exists"), RoadDefinition) ||
		!TestNotNull(TEXT("Bakery definition exists"), BakeryDefinition))
	{
		return false;
	}
	TestTrue(TEXT("Both constructions complete"),
		Host->AdvanceTicks(FMath::Max(RoadDefinition->BuildTicks, BakeryDefinition->BuildTicks)));

	const auto Projection = Host->BuildProjection();
	const FHansaBuildingWorldProjection* CompletedBakery =
		Projection
			? Projection.Value.GetBuildingWorldProjections().FindByPredicate(
				[](const FHansaBuildingWorldProjection& Building)
				{
					return Building.BuildingId.GetValue() == 2;
				})
			: nullptr;
	TestTrue(TEXT("The completed building is projected"), CompletedBakery != nullptr);
	if (CompletedBakery != nullptr)
	{
		TestTrue(TEXT("An adjacent completed road independently grants road access"),
			CompletedBakery->bHasRoadAccess);
		TestEqual(TEXT("The road-only check has no failure"),
			CompletedBakery->RoadAccessFailure,
			EHansaLogisticsRoadPathFailure::None);
		TestFalse(TEXT("No Market independently means no market access"),
			CompletedBakery->bHasMarketAccess);
		TestEqual(TEXT("The market check retains its precise no-Market cause"),
			CompletedBakery->MarketAccessFailure,
			EHansaLogisticsRoadPathFailure::NoOperationalMarket);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaEmptyNewGameResidenceMigrationTest,
	"Hansa.Integration.RuntimeSimulationHost.EmptyNewGameResidenceAttractsLaborers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEmptyNewGameResidenceMigrationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The normal New Game city initializes"),
		Host->InitializeForLubeck(nullptr, Error) && Host->StartNewGame(Error)))
	{
		AddError(Error);
		return false;
	}

	TArray<FHansaPlacementSpec> Specs;
	const auto Add = [&Specs, &Host](const TCHAR* DefinitionId, const int32 X, const int32 Y)
	{
		FHansaPlacementSpec Spec;
		Spec.CityId = Host->GetCityId();
		Spec.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(DefinitionId).Value;
		Spec.Anchor = { X, Y };
		Specs.Add(Spec);
	};
	for (int32 X = 10; X <= 24; ++X) Add(TEXT("Building.Road"), X, 18);
	Add(TEXT("Building.Residence.Laborer"), 10, 19);
	Add(TEXT("Building.Market"), 14, 19);
	Add(TEXT("Building.GrainFarm"), 20, 19);
	if (!TestTrue(TEXT("The player can place a connected residence, Market, and Grain Farm"),
		Host->PlaceBuildings(Specs).IsSuccess()))
	{
		return false;
	}

	const FHansaCompiledBuildingDefinition* Market = Host->FindBuildingDefinition(TEXT("Building.Market"));
	const FHansaCompiledBuildingDefinition* Residence = Host->FindBuildingDefinition(TEXT("Building.Residence.Laborer"));
	const FHansaCompiledPopulationTierDefinition* LaborerTier = Host->GetEconomicRegistry() != nullptr
		? Host->GetEconomicRegistry()->FindPopulationTier(TEXT("PopulationTier.Laborer")) : nullptr;
	if (!TestNotNull(TEXT("The Market definition is available"), Market) ||
		!TestNotNull(TEXT("The residence definition is available"), Residence) ||
		!TestNotNull(TEXT("The laborer tier is available"), LaborerTier))
	{
		return false;
	}
	TestTrue(TEXT("Construction and the first prospective-needs evaluation complete"),
		Host->AdvanceTicks(FMath::Max(Market->BuildTicks, Residence->BuildTicks) + 1));

	auto Projection = Host->BuildProjection();
	const FHansaPopulationCohortProjection* Cohort = Projection
		? Projection.Value.GetPopulationCohorts().FindByPredicate([](const auto& Candidate)
			{ return Candidate.TierId.ToString() == TEXT("PopulationTier.Laborer"); })
		: nullptr;
	if (!TestNotNull(TEXT("The completed residence creates a laborer cohort"), Cohort)) return false;
	TestTrue(TEXT("The connected empty residence has physical Market access"), Cohort->bHasMarketAccess);
	for (const FHansaPopulationNeedState& Need : Cohort->Needs)
	{
		TestTrue(*FString::Printf(TEXT("Prospective need %s supports migration"), *Need.NeedId.ToString()),
			Need.SatisfactionBasisPoints >= LaborerTier->GrowthSatisfactionBasisPoints);
	}

	TestTrue(TEXT("Two satisfied migration intervals advance"),
		Host->AdvanceTicks(LaborerTier->EvaluationTicks * 2));
	Projection = Host->BuildProjection();
	Cohort = Projection
		? Projection.Value.GetPopulationCohorts().FindByPredicate([](const auto& Candidate)
			{ return Candidate.TierId.ToString() == TEXT("PopulationTier.Laborer"); })
		: nullptr;
	TestTrue(TEXT("The connected residence attracts at least two residents"),
		Cohort != nullptr && Cohort->Residents >= 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStarterEconomyBalanceTest,
	"Hansa.Integration.RuntimeSimulationHost.StarterEconomyLongRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaStarterEconomyBalanceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!Host->InitializeForLubeck(nullptr, Error) || !Host->StartNewGame(Error)) { AddError(Error); return false; }
	Host->SetMerchantAIEnabled(false);
	TArray<FHansaPlacementSpec> Specs;
	const auto Add = [&](const TCHAR* Id, int32 X, int32 Y)
	{
		FHansaPlacementSpec Spec; Spec.CityId = Host->GetCityId();
		Spec.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(Id).Value;
		Spec.Anchor = {X,Y}; Specs.Add(Spec);
	};
	for (int32 X=10; X<=28; ++X) Add(TEXT("Building.Road"),X,18);
	for (int32 X=10; X<=28; ++X) Add(TEXT("Building.Road"),X,23);
	for (int32 Y=19; Y<=22; ++Y) Add(TEXT("Building.Road"),27,Y);
	for (int32 X=10; X<=18; X+=2) Add(TEXT("Building.Residence.Laborer"),X,19);
	Add(TEXT("Building.Market"),20,19);
	Add(TEXT("Building.GrainFarm"),23,19);
	Add(TEXT("Building.Mill"),10,24);
	Add(TEXT("Building.Bakery"),13,24);
	for (int32 X=16; X<=22; X+=2) Add(TEXT("Building.Residence.Laborer"),X,24);
	if (!TestTrue(TEXT("Starter chain builds using normal opening resources"), Host->PlaceBuildings(Specs).IsSuccess())) return false;
	const auto Bread = FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
	uint64 BakeryId = 0;
	uint64 CyclesBeforePause = 0;
	int64 BreadAtDay60 = 0;
	for (int32 Day=0; Day<210; ++Day)
	{
		if (!TestTrue(TEXT("Normal economy advances one game day"),Host->AdvanceTicks(24))) return false;
		const auto View = Host->BuildProjection();
		if (!View) return false;
		if (Day==5 || Day==10)
		{
			int32 Residents=0;
			for (const auto& Home : View.Value.GetPopulationCohorts()) if(Home.CityId==Host->GetCityId()) Residents+=Home.Residents;
			TestEqual(Day==5 ? TEXT("No immigration before market completion") : TEXT("Nine founding households arrive"),
				Residents,Day==5 ? 0 : 18);
		}
		for (const auto& Production : View.Value.GetProductions())
			if (Production.RecipeId.ToString()==TEXT("Recipe.BakeBread"))
			{
				BakeryId=Production.Id.GetValue();
				CyclesBeforePause=Production.CompletedCycles;
			}
		int64 Stock = 0;
		for (const auto& Inventory : View.Value.GetInventories())
			if (Inventory.OwnerKind==EHansaInventoryOwnerKind::City && Inventory.CityId==Host->GetCityId())
				for (const auto& Item : Inventory.Stocks) if (Item.GoodId==Bread) Stock+=Item.Available.GetRawValue();
		if (Day==59 || Day==179) BreadAtDay60=Stock;
		if (Day%30==0)
		{
			int32 Residents=0;
			for (const auto& Home : View.Value.GetPopulationCohorts()) if(Home.CityId==Host->GetCityId()) Residents+=Home.Residents;
			AddInfo(FString::Printf(TEXT("Day%d residents=%d bread=%.3f"),Day+1,Residents,Stock/1000.0));
			for (const auto& Production : View.Value.GetProductions())
				AddInfo(FString::Printf(TEXT("%s cycles=%lld"),*Production.RecipeId.ToString(),Production.CompletedCycles));
		}
		if (Day==89 || Day==209)
		{
			TestEqual(TEXT("All nine placed residences have cohorts"), View.Value.GetPopulationCohorts().Num(),9);
			int32 Residents=0; int64 Required=0,Consumed=0;
			for (const auto& Home : View.Value.GetPopulationCohorts()) if(Home.CityId==Host->GetCityId())
			{
				AddInfo(FString::Printf(TEXT("Home %llu residents=%d capacity=%d satisfaction=%d"),Home.ResidenceBuildingId.GetValue(),Home.Residents,Home.ResidenceCapacity,Home.SatisfactionBasisPoints));
				TestTrue(TEXT("Every starter home has market access"), Home.bHasMarketAccess);
				TestTrue(TEXT("Every starter home is operational"), Home.bResidenceOperational);
				Residents+=Home.Residents;
				for (const auto& Total : Home.Consumption.Goods) if(Total.GoodId==Bread) {Required+=Total.Required;Consumed+=Total.Consumed;}
			}
			AddInfo(FString::Printf(TEXT("Day%d residents=%d bread=%.3f prior30DaysBread=%.3f rollingDemand=%.3f rollingConsumed=%.3f"),
				Day+1,Residents,Stock/1000.0,BreadAtDay60/1000.0,Required/1000.0,Consumed/1000.0));
			TestTrue(TEXT("One bread chain retains at least eighteen residents"),Residents>=18);
			TestTrue(TEXT("Bread reserve is positive and not being depleted"),Stock>0 && Stock>=BreadAtDay60);
			TestTrue(TEXT("Final 30-day bread fulfillment exceeds 95 percent"),Required>0 && Consumed*100>=Required*95);
		}
	}
	// A five-day stoppage must recover through normal production, without free goods.
	TestTrue(TEXT("Pause bakery"),Host->SetProductionActive(FHansaProductionId::TryCreate(BakeryId).Value,false).IsSuccess());
	TestTrue(TEXT("Short interruption advances"),Host->AdvanceTicks(120));
	TestTrue(TEXT("Restart bakery"),Host->SetProductionActive(FHansaProductionId::TryCreate(BakeryId).Value,true).IsSuccess());
	TestTrue(TEXT("Recovery advances"),Host->AdvanceTicks(720));
	const auto Recovered=Host->BuildProjection();
	if (!Recovered) return false;
	for (const auto& Production : Recovered.Value.GetProductions())
		if (Production.Id.GetValue()==BakeryId)
			TestTrue(TEXT("Bakery completes new batches after interruption"),Production.CompletedCycles>CyclesBeforePause+10 && Production.bActive);
	int64 RecoveryRequired=0, RecoveryConsumed=0;
	for (const auto& Home : Recovered.Value.GetPopulationCohorts())
		for (const auto& Total : Home.Consumption.Goods) if(Total.GoodId==Bread)
			{RecoveryRequired+=Total.Required; RecoveryConsumed+=Total.Consumed;}
	TestTrue(TEXT("Recovered 30-day bread fulfillment exceeds 95 percent"),
		RecoveryRequired>0 && RecoveryConsumed*100>=RecoveryRequired*95);
	TArray<uint8> Saved;
	const auto SaveResult=Host->CaptureSaveBytes(Saved,TEXT("StarterBalance"),TEXT("2026-09-12T00:00:00Z"));
	TestTrue(TEXT("Rebalanced city saves"),SaveResult.IsSuccess());
	const int64 SavedTick=Host->GetSimulationTick();
	const auto ReloadResult=Host->RestoreSaveBytes(Saved);
	TestTrue(TEXT("Rebalanced city reloads"),ReloadResult.IsSuccess());
	TestEqual(TEXT("Reload preserves complete authoritative state"),ReloadResult.AuthoritativeHash,SaveResult.AuthoritativeHash);
	TestEqual(TEXT("Reload preserves tick"),Host->GetSimulationTick(),SavedTick);
	return !HasAnyErrors();
}
#endif
