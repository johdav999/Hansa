#include "World/HansaMarketPresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Definitions/HansaEconomicDefinitions.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketRolesTest, "Hansa.World.Market.PresentationRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaMarketRolesTest::RunTest(const FString& Parameters)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MarketRolesWorld"));
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Market = World->SpawnActor<AHansaMarketPresentation>();
	if (!TestNotNull(TEXT("Market spawns"), Market)) return false;
	TArray<UStaticMeshComponent*> Roles; Market->GetComponents(Roles);
	TestEqual(TEXT("Six modular roles"), Roles.Num(), 6);
	TestFalse(TEXT("No cosmetic tick"), Market->PrimaryActorTick.bCanEverTick);
	for (auto* Role : Roles)
	{
		TestTrue(TEXT("Native metre contract has identity scale"), Role->GetRelativeScale3D().Equals(FVector::OneVector));
		TestEqual(TEXT("Selection and traffic stay authoritative"), Role->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("No overlap events"), Role->GetGenerateOverlapEvents());
		TestFalse(TEXT("No navigation mutation"), Role->CanEverAffectNavigation());
		TestNull(TEXT("Native defaults do not import staged assets"), Role->GetStaticMesh());
	}
	for (int32 Repeat=0; Repeat<3; ++Repeat)
	{
		Market->OnConstruction(FTransform::Identity);
		TestEqual(TEXT("Stalls reused without accumulation"), Market->Stalls->GetInstanceCount(), 2);
		TestEqual(TEXT("Awnings reused without accumulation"), Market->Awnings->GetInstanceCount(), 2);
		TestEqual(TEXT("Baskets reused without accumulation"), Market->Baskets->GetInstanceCount(), 2);
	}
	FTransform Instance;
	Market->Stalls->GetInstanceTransform(0, Instance);
	TestTrue(TEXT("Imported Y handedness"), Instance.GetLocation().Equals(FVector(270,310,19)));
	TestTrue(TEXT("Instance scale is one"), Instance.GetScale3D().Equals(FVector::OneVector));
	Market->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestTrue(TEXT("Authored court remains visible"), Market->CourtHall->IsVisible());
	for (auto* Role : Roles) if (Role != Market->CourtHall) TestFalse(TEXT("Trade equipment hidden during construction"), Role->IsVisible());
	for (auto Status : {EHansaBuildingWorldStatus::Ready, EHansaBuildingWorldStatus::Blocked})
	{
		Market->ApplyStatus(Status);
		for (auto* Role : Roles) TestTrue(TEXT("Completed equipment does not claim operating stock or coverage"), Role->IsVisible());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketStagedAssetTest, "Hansa.World.Market.StagedAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::RequiresUser)
bool FHansaMarketStagedAssetTest::RunTest(const FString& Parameters)
{
	UClass* Class = LoadClass<AHansaMarketPresentation>(nullptr,
		TEXT("/Game/Hansa/Generated/Staging/Market_P15/BP_Market_Review.BP_Market_Review_C"));
	if (!TestNotNull(TEXT("Staged review class loads"), Class)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MarketAssetWorld"));
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Market = World->SpawnActor<AHansaMarketPresentation>(Class);
	if (!TestNotNull(TEXT("Review actor spawns"), Market)) return false;
	TArray<UStaticMeshComponent*> Roles; Market->GetComponents(Roles);
	TestEqual(TEXT("Six roles"), Roles.Num(), 6);
	for (auto* Role : Roles)
	{
		const UStaticMesh* Mesh = Role->GetStaticMesh();
		if (!TestNotNull(*Role->GetName(), Mesh)) continue;
		TestFalse(TEXT("No engine primitives"), Mesh->GetPathName().StartsWith(TEXT("/Engine/")));
		TestEqual(TEXT("Three LODs"), Mesh->GetNumLODs(), 3);
		TestTrue(TEXT("Reusable mesh simple collision exists"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount()>0);
		TestEqual(TEXT("Presentation collision disabled"), Role->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		for (const auto& Slot : Mesh->GetStaticMaterials()) TestNotNull(TEXT("PBR slots assigned"), Slot.MaterialInterface.Get());
	}
	const FBox Bounds = Market->CalculateComponentsBoundingBoxInLocalSpace(true,true);
	TestTrue(TEXT("3x3 plot with 20cm inset"), Bounds.Min.X>=-580 && Bounds.Min.Y>=-580 && Bounds.Max.X<=580 && Bounds.Max.Y<=580);
	TestTrue(TEXT("Ground pivot"), FMath::Abs(Bounds.Min.Z)<=2);
	TestTrue(TEXT("Civic hall metre scale"), Bounds.GetSize().Z>600 && Bounds.GetSize().Z<620);
	auto* Definition = Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Market")));
	if (TestNotNull(TEXT("Stable Market definition resolves"), Definition))
	{
		const auto OriginalClass = Definition->PresentationActorClass;
		ON_SCOPE_EXIT { Definition->PresentationActorClass = OriginalClass; };
		TestEqual(TEXT("Footprint width unchanged"), Definition->FootprintWidthCells, 3);
		TestEqual(TEXT("Footprint height unchanged"), Definition->FootprintHeightCells, 3);
		TestTrue(TEXT("Road connection required"), Definition->bRequiresRoad);
		Definition->PresentationActorClass = Class;
		TestNull(TEXT("Production resolver rejects unapproved staging"), Definition->LoadPresentationActorClass());
		Definition->PresentationActorClass = AHansaMarketPresentation::StaticClass();
		TestEqual(TEXT("Native class binding supported"), Definition->LoadPresentationActorClass(), AHansaMarketPresentation::StaticClass());
	}
	return true;
}
#endif
