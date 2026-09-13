#include "Definitions/HansaEconomicDefinitions.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaFisheryPresentationAssetTest,
	"Hansa.Content.Fishery.PresentationAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaFisheryPresentationAssetTest::RunTest(const FString& Parameters)
{
	const UHansaBuildingDefinition* Fishery = LoadObject<UHansaBuildingDefinition>(
		nullptr,
		TEXT("/Game/Hansa/Core/Buildings/DA_Building_Fishery.DA_Building_Fishery"));
	if (!TestNotNull(TEXT("Building.Fishery definition loads"), Fishery))
	{
		return false;
	}

	const FSoftObjectPath ExpectedMeshPath(
		TEXT("/Game/Mesh/hansa-fishery/SM_HansaFishery.SM_HansaFishery"));
	TestEqual(
		TEXT("Fishery uses its promoted presentation mesh"),
		Fishery->PresentationMesh.ToSoftObjectPath(),
		ExpectedMeshPath);

	const UStaticMesh* Mesh = Fishery->PresentationMesh.LoadSynchronous();
	if (!TestNotNull(TEXT("Fishery presentation mesh loads"), Mesh))
	{
		return false;
	}

	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize();
	TestTrue(TEXT("Fishery mesh is grounded at its pivot"), FMath::IsNearlyZero(Bounds.Min.Z, 0.1));
	TestTrue(TEXT("Fishery mesh fits the 3x2-cell presentation envelope"),
		Size.X <= 1200.0 && Size.Y <= 800.0 && Size.Z <= 600.0);
	TestEqual(TEXT("Fishery mesh has three LODs"), Mesh->GetNumLODs(), 3);
	TestEqual(TEXT("Fishery mesh preserves its eight authored material slots"), Mesh->GetStaticMaterials().Num(), 8);

	const UBodySetup* BodySetup = Mesh->GetBodySetup();
	TestTrue(
		TEXT("Fishery mesh has simple collision"),
		BodySetup != nullptr && BodySetup->AggGeom.ConvexElems.Num() > 0);

	using namespace Hansa::Simulation;
	const auto BuildingId = FHansaBuildingId::TryCreate(1);
	const auto OwnerId = FHansaHouseId::TryCreate(1);
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const auto DefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Fishery"));
	const auto CompleteProgress = FHansaRate::TryMakeNormalized(FHansaRate::Scale);
	if (!BuildingId.IsSuccess() || !OwnerId.IsSuccess() || !CityId.IsSuccess() ||
		!DefinitionId.IsSuccess() || !CompleteProgress.IsSuccess())
	{
		AddError(TEXT("Fishery projection fixture identities must be valid"));
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaFisheryPresentationWorld"));
	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaBuildingWorldProjectionActor* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
	FHansaBuildingWorldProjection Projection;
	Projection.BuildingId = BuildingId.Value;
	Projection.OwnerId = OwnerId.Value;
	Projection.Placement.CityId = CityId.Value;
	Projection.Placement.BuildingDefinitionId = DefinitionId.Value;
	Projection.Placement.Anchor = { 0, 0 };
	Projection.FootprintWidthCells = 3;
	Projection.FootprintHeightCells = 2;
	Projection.OccupiedCells = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } };
	Projection.ConstructionProgress = CompleteProgress.Value;
	Projection.Status = EHansaBuildingWorldStatus::Ready;
	Actor->ApplyProjection(Projection, *Foundation);
	TestTrue(TEXT("Completed Fishery projection renders the promoted mesh"), Actor->BuildingMesh->GetStaticMesh().Get() == Mesh);
	TestTrue(TEXT("Completed Fishery projection is visible"), Actor->BuildingMesh->IsVisible());
	TestTrue(TEXT("Completed Fishery projection preserves authored scale"), Actor->BuildingMesh->GetRelativeScale3D().Equals(FVector::OneVector));
	FHansaProductionProjection Production;
	Production.BuildingId = BuildingId.Value;
	Actor->ApplyProduction(&Production, {});
	TestTrue(TEXT("Production refresh preserves the promoted Fishery mesh"), Actor->BuildingMesh->IsVisible());
	TestTrue(TEXT("Promoted mesh-only presentation is accepted as verified art"), Actor->QueryProduction().PresentationFailure.IsEmpty());
	World->DestroyWorld(false);
	return true;
}

#endif
