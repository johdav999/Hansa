#include "World/HansaSawmillPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AHansaSawmillPresentation::AHansaSawmillPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SawmillRoot")));
	auto AddRole = [this](const TCHAR* Name, const FVector& Location)
	{
		auto* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(GetRootComponent());
		Component->SetRelativeLocation(Location);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		return Component;
	};
	// Metre-authored geometry; FBX converts to cm and mirrors source Y.
	WorkBuilding = AddRole(TEXT("WorkBuilding"), FVector::ZeroVector);
	LogInput = AddRole(TEXT("LogInput"), FVector(470,365,0));
	PlankOutput = AddRole(TEXT("PlankOutput"), FVector(460,-375,0));
	Rack = AddRole(TEXT("Rack"), FVector(-425,-475,0));
	SawWork = AddRole(TEXT("SawWork"), FVector(210,0,0));
	ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus::Ready);
}

void AHansaSawmillPresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	const bool bComplete = Status != EHansaBuildingWorldStatus::UnderConstruction;
	WorkBuilding->SetVisibility(true);
	LogInput->SetVisibility(bComplete);
	Rack->SetVisibility(bComplete);
	SawWork->SetVisibility(bComplete);
	// Symbolic operability cue, not a claim of current stock or a completed shipment.
	PlankOutput->SetVisibility(Status == EHansaBuildingWorldStatus::Ready);
}
