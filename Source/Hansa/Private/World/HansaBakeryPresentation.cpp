#include "World/HansaBakeryPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AHansaBakeryPresentation::AHansaBakeryPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BakeryRoot")));
	auto AddRole = [this](const TCHAR* Name, const TCHAR* Part, const FVector& Location, bool bProp = false)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(GetRootComponent());
		Component->SetRelativeLocation(Location);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		const FString Path = FString(bProp ? TEXT("/Game/Mesh/hansa-bakery/Props/Meshes/SM_Bakery_")
			: TEXT("/Game/Mesh/hansa-bakery/P10/Meshes/SM_Bakery_")) + Part;
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*Path);
		Component->SetStaticMesh(Mesh.Object);
		return Component;
	};
	Bakery = AddRole(TEXT("CompletedBakery"), TEXT("Body"), FVector::ZeroVector);
	Construction = AddRole(TEXT("ConstructionFrame"), TEXT("Construction"), FVector::ZeroVector);
	FlourSack = AddRole(TEXT("FlourInput"), TEXT("Input"), FVector(-500,350,0));
	BreadCrate = AddRole(TEXT("BreadOutput"), TEXT("Output"), FVector(-290,362.5,103));
	Sign = AddRole(TEXT("PermanentBreadSign"), TEXT("Sign"), FVector(380,411.75,376.5));
	// Side-yard dressing stays inside the 12 x 8 metre parcel, leaving the front door clear.
	// Assets are authored in centimetres with grounded pivots; never squeeze them to fit.
	BreadRack = AddRole(TEXT("BreadCoolingRack"), TEXT("BreadRack"), FVector(540,255,0), true);
	FirewoodBasket = AddRole(TEXT("OvenFirewoodBasket"), TEXT("FirewoodBasket"), FVector(-540,-250,0), true);
	Handcart = AddRole(TEXT("FlourDeliveryHandcart"), TEXT("Handcart"), FVector(-530,60,0), true);
	Millstone = AddRole(TEXT("BakeryHandMill"), TEXT("Millstone"), FVector(540,-250,0), true);
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
	BreadRack->SetVisibility(bWorking);
	Handcart->SetVisibility(bWorking); // The supplied cart includes flour sacks.
	FirewoodBasket->SetVisibility(!bConstructing);
	Millstone->SetVisibility(!bConstructing);
}
