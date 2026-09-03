#include "Gameplay/HansaPlacementAutomationFixture.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::IntegratedLubeck
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;

	struct FMilestones final
	{
		bool bConstructionComplete = false;
		bool bInventoryMoved = false;
		bool bProductionCompleted = false;
		bool bPopulationGrew = false;
		bool bBreadConsumed = false;
		int32 Residents = 0;
		uint64 CompletedCycles = 0;
		int32 CompletedJobs = 0;
		int64 BreadConsumedLastTick = 0;
	};

	FMilestones Inspect(const FHansaSimulationProjection& Projection)
	{
		FMilestones Result;
		int32 IntegratedConstructionCount = 0;
		Result.bConstructionComplete = true;
		for (const FHansaConstructionProjection& Construction : Projection.GetConstructions())
		{
			if (Construction.BuildingId.GetValue() == 2 || Construction.BuildingId.GetValue() == 3)
			{
				++IntegratedConstructionCount;
				Result.bConstructionComplete &= Construction.State == EHansaConstructionState::Completed;
			}
		}
		Result.bConstructionComplete &= IntegratedConstructionCount == 2;
		for (const FHansaLogisticsJobProjection& Job : Projection.GetLogisticsJobs())
		{
			Result.CompletedJobs += Job.Status == EHansaLogisticsJobStatus::Completed ? 1 : 0;
		}
		Result.bInventoryMoved = Result.CompletedJobs > 0;
		if (!Projection.GetProductions().IsEmpty())
		{
			Result.CompletedCycles = Projection.GetProductions()[0].CompletedCycles;
			Result.bProductionCompleted = Result.CompletedCycles > 0;
		}
		if (!Projection.GetCityPopulations().IsEmpty())
		{
			Result.Residents = Projection.GetCityPopulations()[0].TotalResidents;
			Result.bPopulationGrew = Result.Residents > 6;
		}
		for (const FHansaPopulationCohortProjection& Cohort : Projection.GetPopulationCohorts())
		{
			for (const FHansaPopulationNeedState& Need : Cohort.Needs)
			{
				if (Need.GoodId.ToString() == TEXT("Good.Bread"))
				{
					Result.BreadConsumedLastTick += Need.ConsumedLastTick.GetRawValue();
				}
			}
		}
		Result.bBreadConsumed = Result.BreadConsumedLastTick > 0;
		return Result;
	}

	FString EvidenceJson(const FHansaSimulationProjection& WorldProjection,
		const FHansaSimulationProjection& HeadlessProjection,
		const FHansaIntegratedLubeckCheckpointState& Checkpoints,
		const bool bLogisticsRecordsBounded)
	{
		return FString::Printf(
			TEXT("{\n  \"evidenceSchemaVersion\": 1,\n  \"fixtureId\": \"integrated_lubeck_city_v1\",\n")
			TEXT("  \"tick\": %lld,\n  \"worldStateHash\": \"%016llX\",\n  \"headlessStateHash\": \"%016llX\",\n")
			TEXT("  \"parityMatched\": %s,\n  \"worldProjectionCount\": %d,\n  \"headlessProjectionCount\": %d,\n")
			TEXT("  \"constructionComplete\": %s,\n  \"inventoryMoved\": %s,\n")
			TEXT("  \"productionCompleted\": %s,\n  \"populationGrown\": %s,\n  \"breadConsumed\": %s,\n")
			TEXT("  \"completedDeliveries\": %d,\n  \"completedProductionCycles\": \"%llu\",\n")
			TEXT("  \"residents\": %d,\n  \"breadConsumedLastTickMilliUnits\": %lld,\n")
			TEXT("  \"breadConsumedTotalMilliUnits\": %lld,\n  \"logisticsRecordsBounded\": %s,\n")
			TEXT("  \"logisticsRequestCount\": %d,\n  \"logisticsJobCount\": %d\n}\n"),
			static_cast<long long>(WorldProjection.GetClock().GetTick().GetValue()),
			static_cast<unsigned long long>(WorldProjection.GetFingerprint().Value),
			static_cast<unsigned long long>(HeadlessProjection.GetFingerprint().Value),
			WorldProjection.GetFingerprint() == HeadlessProjection.GetFingerprint() ? TEXT("true") : TEXT("false"),
			WorldProjection.GetBuildingWorldProjections().Num(),
			HeadlessProjection.GetBuildingWorldProjections().Num(),
			Checkpoints.bConstructionCompleted ? TEXT("true") : TEXT("false"),
			Checkpoints.bInventoryMoved ? TEXT("true") : TEXT("false"),
			Checkpoints.bProductionCompleted ? TEXT("true") : TEXT("false"),
			Checkpoints.bPopulationGrown ? TEXT("true") : TEXT("false"),
			Checkpoints.bBreadConsumed ? TEXT("true") : TEXT("false"),
			Checkpoints.CompletedDeliveries,
			static_cast<unsigned long long>(Checkpoints.CompletedProductionCycles),
			Checkpoints.Residents, static_cast<long long>(Checkpoints.BreadConsumedLastTickMilliUnits),
			static_cast<long long>(Checkpoints.BreadConsumedTotalMilliUnits),
			bLogisticsRecordsBounded ? TEXT("true") : TEXT("false"),
			WorldProjection.GetLogisticsRequests().Num(), WorldProjection.GetLogisticsJobs().Num());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaIntegratedLubeckLongRunTest,
	"Hansa.Architecture.Automation.IntegratedLubeckCity.LongRunParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaIntegratedLubeckLongRunTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::IntegratedLubeck;
	FHansaSemanticUiRegistry WorldRegistry;
	FHansaSemanticUiRegistry HeadlessRegistry;
	FHansaPlacementAutomationFixture World;
	FHansaPlacementAutomationFixture Headless;
	FString Error;
	if (!TestTrue(TEXT("Integrated rendered-world fixture loads"), World.LoadIntegrated(WorldRegistry, Error)) ||
		!TestTrue(TEXT("Equivalent headless fixture loads"), Headless.LoadIntegrated(HeadlessRegistry, Error)))
	{
		return false;
	}
	TestEqual(TEXT("Fixture begins with warehouse, bakery, residence, and seven road placements"),
		World.GetPlacedBuildingCount(), 10);
	const auto Initial = World.BuildProjection();
	TestTrue(TEXT("Initial integrated projection is available"), Initial.IsSuccess());
	if (!Initial) return false;
	const FMilestones Before = Inspect(Initial.Value);
	TestFalse(TEXT("Bakery and residence begin under construction"), Before.bConstructionComplete);
	TestFalse(TEXT("No production cycle is fabricated at load"), Before.bProductionCompleted);

	bool bSawConstruction = false;
	bool bSawMovement = false;
	bool bSawProduction = false;
	bool bSawGrowth = false;
	bool bSawConsumption = false;
	for (int32 Tick = 0; Tick < 512; ++Tick)
	{
		if (!World.AdvanceTicks(1) || !Headless.AdvanceTicks(1))
		{
			AddError(TEXT("Integrated fixture failed deterministic advancement"));
			return false;
		}
		const auto Projection = World.BuildProjection();
		if (!Projection) return false;
		const FMilestones Current = Inspect(Projection.Value);
		bSawConstruction |= Current.bConstructionComplete;
		bSawMovement |= Current.bInventoryMoved;
		bSawProduction |= Current.bProductionCompleted;
		bSawGrowth |= Current.bPopulationGrew;
		bSawConsumption |= Current.bBreadConsumed;
	}
	const auto WorldProjection = World.BuildProjection();
	const auto HeadlessProjection = Headless.BuildProjection();
	TestTrue(TEXT("Both equivalent slices retain projections"), WorldProjection.IsSuccess() && HeadlessProjection.IsSuccess());
	if (!WorldProjection || !HeadlessProjection) return false;
	TestTrue(TEXT("Construction completed during the integrated run"), bSawConstruction);
	TestTrue(TEXT("Warehouse inventory moved over the completed road network"), bSawMovement);
	TestTrue(TEXT("Delivered grain enabled bread production"), bSawProduction);
	TestTrue(TEXT("Satisfied residence produced bounded population growth"), bSawGrowth);
	TestTrue(TEXT("Population consumed delivered bread"), bSawConsumption);
	TestEqual(TEXT("Rendered-world and headless state hashes match exactly"),
		WorldProjection.Value.GetFingerprint().Value, HeadlessProjection.Value.GetFingerprint().Value);
	TestEqual(TEXT("Rendered-world and headless population projections match"),
		WorldProjection.Value.GetTotalResidents(), HeadlessProjection.Value.GetTotalResidents());
	TestEqual(TEXT("Rendered-world and headless production projections match"),
		WorldProjection.Value.GetProductions()[0].CompletedCycles,
		HeadlessProjection.Value.GetProductions()[0].CompletedCycles);
	const FHansaIntegratedLubeckCheckpointState& WorldCheckpoints = World.GetIntegratedCheckpoints();
	const FHansaIntegratedLubeckCheckpointState& HeadlessCheckpoints = Headless.GetIntegratedCheckpoints();
	TestTrue(TEXT("Construction checkpoint remains observable after the long run"),
		WorldCheckpoints.bConstructionCompleted);
	TestTrue(TEXT("Inventory movement checkpoint remains observable after completed jobs are bounded"),
		WorldCheckpoints.bInventoryMoved);
	TestTrue(TEXT("Production checkpoint remains observable after the long run"),
		WorldCheckpoints.bProductionCompleted);
	TestTrue(TEXT("Population growth checkpoint remains observable after the long run"),
		WorldCheckpoints.bPopulationGrown);
	TestTrue(TEXT("Transient bread consumption is retained as a sticky checkpoint"),
		WorldCheckpoints.bBreadConsumed && WorldCheckpoints.BreadConsumedTotalMilliUnits > 0);
	TestEqual(TEXT("Equivalent runs retain the same bread-consumption evidence"),
		WorldCheckpoints.BreadConsumedTotalMilliUnits, HeadlessCheckpoints.BreadConsumedTotalMilliUnits);
	const TConstArrayView<FHansaBuildingWorldProjection> WorldBuildings =
		WorldProjection.Value.GetBuildingWorldProjections();
	const TConstArrayView<FHansaBuildingWorldProjection> HeadlessBuildings =
		HeadlessProjection.Value.GetBuildingWorldProjections();
	TestEqual(TEXT("Rendered-world and headless building projection counts match"),
		WorldBuildings.Num(), HeadlessBuildings.Num());
	for (int32 Index = 0; Index < FMath::Min(WorldBuildings.Num(), HeadlessBuildings.Num()); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Rendered-world building projection %d matches headless state"), Index),
			WorldBuildings[Index] == HeadlessBuildings[Index]);
	}
	UWorld* RenderedWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaIntegratedLubeckTestWorld"));
	TestNotNull(TEXT("Integrated fixture creates a disposable rendered world"), RenderedWorld);
	if (RenderedWorld != nullptr)
	{
		AHansaLubeckWorldFoundation* Foundation = RenderedWorld->SpawnActor<AHansaLubeckWorldFoundation>();
		AHansaPlacementProjectionManager* ProjectionManager =
			RenderedWorld->SpawnActor<AHansaPlacementProjectionManager>();
		TestNotNull(TEXT("Integrated rendered world creates its Lübeck foundation"), Foundation);
		TestNotNull(TEXT("Integrated rendered world creates its projection manager"), ProjectionManager);
		if (Foundation != nullptr && ProjectionManager != nullptr)
		{
			TestTrue(TEXT("Initial integrated state materializes through the world projection manager"),
				ProjectionManager->Synchronize(Initial.Value, *Foundation));
			TestEqual(TEXT("Initial world slice creates one Actor per canonical placement"),
				ProjectionManager->GetProjectionCount(), 10);
			TestTrue(TEXT("Final integrated state updates the same rendered world slice"),
				ProjectionManager->Synchronize(WorldProjection.Value, *Foundation));
			TestEqual(TEXT("Final world slice retains one Actor per canonical placement"),
				ProjectionManager->GetProjectionCount(), 10);
			const AHansaBuildingWorldProjectionActor* BakeryActor = ProjectionManager->FindProjectionActor(
				FHansaBuildingId::TryCreate(2).Value);
			const AHansaBuildingWorldProjectionActor* ResidenceActor = ProjectionManager->FindProjectionActor(
				FHansaBuildingId::TryCreate(3).Value);
			TestTrue(TEXT("Rendered bakery and residence both reach ready state"),
				BakeryActor != nullptr && ResidenceActor != nullptr &&
				BakeryActor->GetWorldStatus() == EHansaBuildingWorldStatus::Ready &&
				ResidenceActor->GetWorldStatus() == EHansaBuildingWorldStatus::Ready);
			ProjectionManager->TearDownProjections();
		}
		RenderedWorld->DestroyWorld(false);
	}
	const bool bLogisticsRecordsBounded = WorldProjection.Value.GetLogisticsRequests().Num() <= 256 &&
		WorldProjection.Value.GetLogisticsJobs().Num() <= 256;
	TestTrue(TEXT("Long run remains bounded rather than leaking logistics records"), bLogisticsRecordsBounded);

	const FString EvidenceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"),
		TEXT("IntegratedLubeck"), TEXT("integrated_lubeck_city_v1"));
	IFileManager::Get().MakeDirectory(*EvidenceDirectory, true);
	const FString EvidencePath = FPaths::Combine(EvidenceDirectory, TEXT("integrated-run.json"));
	TestTrue(TEXT("Structured integrated evidence is persisted"), FFileHelper::SaveStringToFile(
		EvidenceJson(WorldProjection.Value, HeadlessProjection.Value, WorldCheckpoints, bLogisticsRecordsBounded), *EvidencePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	FHansaNativeScreenshotService Screenshots;
	auto Capture = [this, &Screenshots, &WorldRegistry, &WorldProjection, &WorldCheckpoints,
		bLogisticsRecordsBounded](const FIntPoint Size, const TCHAR* Bundle)
	{
		FHansaScreenshotContext Context;
		Context.BundleId = Bundle;
		Context.EvidenceSuiteId = TEXT("S06P04");
		Context.FixtureId = FHansaPlacementAutomationFixture::IntegratedFixtureId;
		Context.ScreenId = TEXT("BuildMode.Screen");
		Context.FlowId = TEXT("integrated-lubeck-city-loop-v1");
		Context.SimulationTick = WorldProjection.Value.GetClock().GetTick().GetValue();
		Context.UiRevision = WorldRegistry.GetRevision();
		Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract");
		Context.SemanticSnapshotJson = TEXT("{\"schemaVersion\":1,\"nodes\":[{\"id\":\"BuildMode.Integrated.Construction\"},{\"id\":\"BuildMode.Integrated.Logistics\"},{\"id\":\"BuildMode.Integrated.Production\"},{\"id\":\"BuildMode.Integrated.Population\"},{\"id\":\"BuildMode.Integrated.Bread\"}]}");
		Context.StructuralAssertions = {
			TEXT("construction.completed=true"), TEXT("inventory.moved=true"),
			TEXT("production.completed=true"), TEXT("population.grown=true"), TEXT("bread.consumed=true"),
			TEXT("bread.consumed_total>0"), TEXT("logistics.records_bounded=true")
		};
		Context.bStructuralAssertionsPassed = WorldCheckpoints.bConstructionCompleted &&
			WorldCheckpoints.bInventoryMoved && WorldCheckpoints.bProductionCompleted &&
			WorldCheckpoints.bPopulationGrown && WorldCheckpoints.bBreadConsumed && bLogisticsRecordsBounded;
		auto NativeBuffer = [](const FIntPoint& Requested, TArray<FColor>& Pixels)
		{
			Pixels.Init(FColor(31, 48, 42, 255), Requested.X * Requested.Y);
			return true;
		};
		const FHansaScreenshotResult Result = Screenshots.Capture(Size, Context, NativeBuffer);
		TestTrue(FString::Printf(TEXT("Native %dx%d integrated capture evidence writes"), Size.X, Size.Y), Result.IsSuccess());
	};
	Capture(FIntPoint(1280, 720), TEXT("contract-integrated-lubeck-720"));
	Capture(FIntPoint(1920, 1080), TEXT("contract-integrated-lubeck-1080"));
	return !HasAnyErrors();
}

#endif
