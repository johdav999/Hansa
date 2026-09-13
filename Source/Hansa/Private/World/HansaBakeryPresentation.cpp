#include "World/HansaBakeryPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AHansaBakeryPresentation::AHansaBakeryPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BakeryRoot")));
	auto AddRole = [this](const TCHAR* Name, const TCHAR* Part, const FVector& Location)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(GetRootComponent());
		Component->SetRelativeLocation(Location);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		const FString Path = FString(TEXT("/Game/Mesh/hansa-bakery/P10/Meshes/SM_Bakery_")) + Part;
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*Path);
		Component->SetStaticMesh(Mesh.Object);
		return Component;
	};
	Bakery = AddRole(TEXT("CompletedBakery"), TEXT("Body"), FVector::ZeroVector);
	Construction = AddRole(TEXT("ConstructionFrame"), TEXT("Construction"), FVector::ZeroVector);
	FlourSack = AddRole(TEXT("FlourInput"), TEXT("Input"), FVector(-500,350,0));
	BreadCrate = AddRole(TEXT("BreadOutput"), TEXT("Output"), FVector(-290,362.5,103));
	Sign = AddRole(TEXT("PermanentBreadSign"), TEXT("Sign"), FVector(380,411.75,376.5));
	ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus::Ready);
}

void AHansaBakeryPresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	const bool bConstructing = Status == EHansaBuildingWorldStatus::UnderConstruction;
	const bool bWorking = Status == EHansaBuildingWorldStatus::Ready;
	Bakery->SetVisibility(!bConstructing);
	Construction->SetVisibility(bConstructing);
	Sign->SetVisibility(!bConstructing);
	// Ready means production can operate; neither cue asserts a count or delivery.
	FlourSack->SetVisibility(bWorking);
	BreadCrate->SetVisibility(bWorking);
}
