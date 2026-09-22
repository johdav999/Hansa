#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRoadConnectionIndicatorTest,
	"Hansa.Integration.RoadConnectionIndicator.VisibilityAndRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRoadConnectionIndicatorTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaBuildingWorldProjectionActor* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
	if (!TestNotNull(TEXT("Foundation exists"), Foundation) ||
		!TestNotNull(TEXT("Building projection exists"), Actor))
	{
		return false;
	}

	FHansaBuildingWorldProjection Projection;
	Projection.BuildingId = FHansaBuildingId::TryCreate(700).Value;
	Projection.OwnerId = FHansaHouseId::TryCreate(1).Value;
	Projection.Placement.CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
	Projection.Placement.BuildingDefinitionId =
		FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery")).Value;
	Projection.Placement.Anchor = { 18, 16 };
	Projection.OccupiedCells = { { 18, 16 } };
	Projection.FootprintWidthCells = 1;
	Projection.FootprintHeightCells = 1;
	Projection.ConstructionProgress = FHansaRate::TryMakeNormalized(FHansaRate::Scale).Value;
	Projection.Status = EHansaBuildingWorldStatus::Ready;
	Projection.bRequiresRoad = true;
	Projection.bHasRoadAccess = false;
	Projection.bHasMarketAccess = false;
	Projection.MarketAccessFailure = EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket;

	Actor->ApplyProjection(Projection, *Foundation);
	TestTrue(TEXT("Disconnected completed building exposes the marker"),
		Actor->IsRoadDisconnectedIndicatorVisible());
	TestTrue(TEXT("Disconnected marker component is visible"), Actor->RoadDisconnectedMarker->IsVisible());
	TestTrue(TEXT("Only the imported production marker mesh is used"),
		Actor->RoadDisconnectedMarker->GetStaticMesh() != nullptr &&
		Actor->RoadDisconnectedMarker->GetStaticMesh()->GetPathName().StartsWith(
			TEXT("/Game/Mesh/hansa-road-disconnected-symbol/SM_HansaRoadDisconnectedSymbol")));
	TestTrue(TEXT("Marker is above the generic status marker"),
		Actor->RoadDisconnectedMarker->GetRelativeLocation().Z >
		Actor->StatusMarker->GetRelativeLocation().Z);
	TestEqual(TEXT("Marker is scaled for the city camera"),
		Actor->RoadDisconnectedMarker->GetRelativeScale3D(), FVector(2.25));
	TestEqual(TEXT("Marker has no gameplay collision"),
		Actor->RoadDisconnectedMarker->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Marker never affects navigation"),
		Actor->RoadDisconnectedMarker->CanEverAffectNavigation());
	TestTrue(TEXT("Only visible markers enable actor ticking"), Actor->IsActorTickEnabled());

	const double BeforeYaw = Actor->RoadDisconnectedMarker->GetRelativeRotation().Yaw;
	Actor->Tick(1.0f);
	const double RotationDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
		BeforeYaw, Actor->RoadDisconnectedMarker->GetRelativeRotation().Yaw));
	TestTrue(TEXT("Marker rotates around its local vertical axis"), FMath::IsNearlyEqual(RotationDelta, 45.0, 0.1));

	Projection.bHasRoadAccess = true;
	Projection.MarketAccessFailure = EHansaLogisticsRoadPathFailure::NoOperationalMarket;
	Actor->ApplyProjection(Projection, *Foundation);
	TestFalse(TEXT("A road-connected building does not show the road marker when only its market is missing"),
		Actor->IsRoadDisconnectedIndicatorVisible());
	TestTrue(TEXT("Missing market activates its separate rotating warning"), Actor->IsMarketNotInRangeIndicatorVisible() && Actor->IsActorTickEnabled());

	Projection.bHasMarketAccess = true;
	Projection.MarketAccessFailure = EHansaLogisticsRoadPathFailure::None;
	Actor->ApplyProjection(Projection, *Foundation);
	TestFalse(TEXT("Market access does not re-enable a hidden road marker"),
		Actor->IsRoadDisconnectedIndicatorVisible());
	TestFalse(TEXT("Hidden marker stops ticking"), Actor->IsActorTickEnabled());

	Projection.bHasRoadAccess = false;
	Projection.bHasMarketAccess = false;
	Projection.Status = EHansaBuildingWorldStatus::UnderConstruction;
	Actor->ApplyProjection(Projection, *Foundation);
	TestFalse(TEXT("Incomplete construction does not claim a road outage"),
		Actor->IsRoadDisconnectedIndicatorVisible());

	Projection.Status = EHansaBuildingWorldStatus::Ready;
	Projection.bRequiresRoad = false;
	Actor->ApplyProjection(Projection, *Foundation);
	TestFalse(TEXT("Buildings that do not require roads never show the marker"),
		Actor->IsRoadDisconnectedIndicatorVisible());

	return !HasAnyErrors();
}

#endif
