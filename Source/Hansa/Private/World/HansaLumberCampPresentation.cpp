#include "World/HansaLumberCampPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AHansaLumberCampPresentation::AHansaLumberCampPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LumberCampRoot")));
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
	// Source modules use metre-authored local ground pivots and +X entrance.
	// Verified UE FBX import mirrors Blender Y; offsets use the same handedness conversion.
	WorkBuilding = AddRole(TEXT("WorkBuilding"), FVector::ZeroVector);
	LogPile = AddRole(TEXT("LogPile"), FVector(340,360,0));
	CutTimber = AddRole(TEXT("CutTimber"), FVector(350,60,0));
	ToolShelter = AddRole(TEXT("ToolShelter"), FVector(-300,-325,0));
	StumpSlash = AddRole(TEXT("StumpSlash"), FVector(190,-310,0));
	ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus::Ready);
}

void AHansaLumberCampPresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	const bool bComplete = Status != EHansaBuildingWorldStatus::UnderConstruction;
	WorkBuilding->SetVisibility(true);
	LogPile->SetVisibility(bComplete);
	ToolShelter->SetVisibility(bComplete);
	StumpSlash->SetVisibility(bComplete);
	// This cue indicates operability, never a count of inventory or a delivery.
	CutTimber->SetVisibility(Status == EHansaBuildingWorldStatus::Ready);
}
