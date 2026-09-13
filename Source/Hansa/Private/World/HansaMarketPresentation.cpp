#include "World/HansaMarketPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

AHansaMarketPresentation::AHansaMarketPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MarketRoot")));
	CourtHall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CourtHall"));
	Stalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stalls"));
	Awnings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Awnings"));
	Scales = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Scales"));
	Baskets = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Baskets"));
	Cargo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cargo"));
	for (UStaticMeshComponent* Component : TArray<UStaticMeshComponent*>{CourtHall, Stalls, Awnings, Scales, Baskets, Cargo})
	{
		Component->SetupAttachment(GetRootComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}
	// FBX converts metres to centimetres and mirrors source Y. No auto-fit scale.
	Scales->SetRelativeLocation(FVector(25,310,19));
	Cargo->SetRelativeLocation(FVector(270,-310,19));
	RebuildInstances();
}

void AHansaMarketPresentation::RebuildInstances()
{
	Stalls->ClearInstances(); Awnings->ClearInstances(); Baskets->ClearInstances();
	for (const double Y : {310.0, -310.0})
	{
		const FTransform Transform(FVector(270,Y,19));
		Stalls->AddInstance(Transform); Awnings->AddInstance(Transform);
	}
	Baskets->AddInstance(FTransform(FVector(265,350,117)));
	Baskets->AddInstance(FTransform(FVector(265,275,117)));
}

void AHansaMarketPresentation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildInstances();
}

void AHansaMarketPresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
	const bool bComplete = Status != Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction;
	CourtHall->SetVisibility(true);
	for (UStaticMeshComponent* Component : TArray<UStaticMeshComponent*>{Stalls, Awnings, Scales, Baskets, Cargo})
		Component->SetVisibility(bComplete);
	// Blocked retains equipment: this actor cannot infer stock or service access.
}
