#include "World/HansaWarehousePresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaWarehouseCargoTest, "Hansa.World.Warehouse.CargoProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaWarehouseCargoTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WarehouseCargoWorld"));
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Warehouse = World->SpawnActor<AHansaWarehousePresentation>();
	if (!TestNotNull(TEXT("Warehouse spawns"), Warehouse)) return false;
	TArray<UStaticMeshComponent*> Roles; Warehouse->GetComponents(Roles);
	TestEqual(TEXT("Five storage/transfer roles"), Roles.Num(), 5);
	for (auto* Role : Roles)
	{
		TestNull(TEXT("No unapproved native asset dependency"), Role->GetStaticMesh());
		TestTrue(TEXT("No scale correction"), Role->GetRelativeScale3D().Equals(FVector::OneVector));
		TestEqual(TEXT("Does not steal selection or block carts"), Role->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("No navigation mutation"), Role->CanEverAffectNavigation());
	}
	FHansaInventoryProjection Inventory;
	Inventory.Capacity = FHansaQuantity::FromRaw(200000);
	for (const auto& Case : TArray<TPair<int64,int32>>{{0,0},{1,1},{66666,1},{66667,2},{133333,2},{133334,3},{200000,3},{300000,3},{-1,0}})
	{
		Inventory.UsedCapacity = FHansaQuantity::FromRaw(Case.Key);
		Warehouse->ApplyInventory(&Inventory);
		TestEqual(TEXT("Deterministic occupancy band"), Warehouse->GetCargoGroupCount(), Case.Value);
		TestEqual(TEXT("Bounded instances"), Warehouse->Cargo->GetInstanceCount(), Case.Value);
	}
	Inventory.UsedCapacity = Inventory.Capacity;
	Warehouse->ApplyInventory(&Inventory);
	for (int32 Repeat=0; Repeat<3; ++Repeat)
	{
		Warehouse->OnConstruction(FTransform::Identity);
		TestEqual(TEXT("Two freight doors"), Warehouse->Doors->GetInstanceCount(), 2);
		TestEqual(TEXT("Three reusable skids"), Warehouse->Skids->GetInstanceCount(), 3);
		TestEqual(TEXT("Cargo does not accumulate"), Warehouse->Cargo->GetInstanceCount(), 3);
	}
	Warehouse->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestEqual(TEXT("No cargo during construction"), Warehouse->Cargo->GetInstanceCount(), 0);
	Warehouse->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestEqual(TEXT("Blocked does not erase actual stock"), Warehouse->Cargo->GetInstanceCount(), 3);
	Warehouse->ApplyInventory(nullptr);
	TestEqual(TEXT("Missing inventory clears stale cargo"), Warehouse->Cargo->GetInstanceCount(), 0);
	Inventory.Capacity = FHansaQuantity::FromRaw(MAX_int64);
	Inventory.UsedCapacity = Inventory.Capacity;
	Warehouse->ApplyInventory(&Inventory);
	TestEqual(TEXT("No integer overflow"), Warehouse->GetCargoGroupCount(), 3);
	Inventory.Capacity = FHansaQuantity::FromRaw(0);
	Warehouse->ApplyInventory(&Inventory);
	TestEqual(TEXT("Invalid capacity is unknown"), Warehouse->GetCargoGroupCount(), 0);
	TestFalse(TEXT("No cosmetic tick"), Warehouse->PrimaryActorTick.bCanEverTick);
	return true;
}
#endif
