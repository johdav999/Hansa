#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"

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
	TestEqual(TEXT("All MVP goods are available"), Registry->GetGoods().Num(), 10);
	TestEqual(TEXT("All MVP recipes are available"), Registry->GetRecipes().Num(), 8);
	TestEqual(TEXT("All MVP buildings are available"), Registry->GetBuildings().Num(), 14);
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
	TestEqual(TEXT("Fastest speed advances twelve ticks per second"), Host->GetSimulationTick(), int64(5));
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
	TestTrue(TEXT("Placement events create the bakery projection Actor"), BakeryActor != nullptr &&
		BakeryActor->GetWorldStatus() == EHansaBuildingWorldStatus::UnderConstruction &&
		BakeryActor->ConstructionPlaceholder->IsVisible());
	TestTrue(TEXT("The authored construction duration completes construction"),
		Host->AdvanceTicks(BakeryDefinition->BuildTicks));
	BakeryActor = Manager->FindProjectionActor(BakeryId);
	TestTrue(TEXT("Completion events update the same projection Actor to its ready visual"), BakeryActor != nullptr &&
		BakeryActor->GetWorldStatus() == EHansaBuildingWorldStatus::Ready &&
		BakeryActor->BuildingMesh->IsVisible() && !BakeryActor->ConstructionPlaceholder->IsVisible());

	Manager->TearDownProjections();
	World->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif
