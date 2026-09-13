#include "World/HansaWarehousePresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

AHansaWarehousePresentation::AHansaWarehousePresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WarehouseRoot")));
	Storehouse = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Storehouse"));
	Hoist = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hoist"));
	Doors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Doors"));
	Skids = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Skids"));
	Cargo = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Cargo"));
	for (auto* Component : TArray<UStaticMeshComponent*>{Storehouse, Hoist, Doors, Skids, Cargo})
	{
		Component->SetupAttachment(GetRootComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}
}

FVector AHansaWarehousePresentation::StagingLocation(const int32 Index)
{
	// Leave the central freight lane clear on both ends of the storehouse.
	// FBX's Y-handedness conversion mirrors the source review layout.
	const FVector Locations[] = {FVector(670,310,0), FVector(670,-310,0), FVector(-670,310,0)};
	return Locations[FMath::Clamp(Index,0,2)];
}

void AHansaWarehousePresentation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Doors->ClearInstances(); Skids->ClearInstances();
	// Source metres become UE centimetres. +X is the street freight entrance.
	Doors->AddInstance(FTransform(FVector(550,0,10)));
	Doors->AddInstance(FTransform(FRotator(0,180,0), FVector(-550,0,10)));
	for (int32 Index=0; Index<3; ++Index) Skids->AddInstance(FTransform(StagingLocation(Index)));
	RefreshCargo();
}

void AHansaWarehousePresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
	bComplete = Status != Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction;
	Storehouse->SetVisibility(true);
	Doors->SetVisibility(bComplete); Hoist->SetVisibility(bComplete); Skids->SetVisibility(bComplete);
	RefreshCargo();
}

void AHansaWarehousePresentation::ApplyInventory(const Hansa::Simulation::FHansaInventoryProjection* Inventory)
{
	int32 Next = 0;
	if (Inventory && Inventory->Capacity.GetRawValue() > 0 && Inventory->UsedCapacity.GetRawValue() > 0)
	{
		const int64 Capacity = Inventory->Capacity.GetRawValue();
		const int64 Used = FMath::Min(Inventory->UsedCapacity.GetRawValue(), Capacity);
		// Overflow-safe ceil(3 * Used / Capacity), including small and int64 capacities.
		const int64 Third = Capacity / 3;
		const int64 TwoThirds = Third * 2 + (Capacity % 3) * 2 / 3;
		Next = Used <= Third ? 1 : Used <= TwoThirds ? 2 : 3;
	}
	if (Next != CargoGroupCount) { CargoGroupCount = Next; RefreshCargo(); }
}

void AHansaWarehousePresentation::RefreshCargo()
{
	const int32 Count = bComplete ? CargoGroupCount : 0;
	// No per-tick clear/re-add or unbounded per-stock components.
	if (Cargo->GetInstanceCount() != Count)
	{
		Cargo->ClearInstances();
		for (int32 Index=0; Index<Count; ++Index)
			Cargo->AddInstance(FTransform(StagingLocation(Index)+FVector(0,0,23)));
	}
	Cargo->SetVisibility(Count > 0);
}
