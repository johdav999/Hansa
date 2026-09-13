#include "World/HansaGrainFarmPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AHansaGrainFarmPresentation::AHansaGrainFarmPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FarmRoot")));
	auto AddRole = [this](const TCHAR* Name, const TCHAR* Asset, const FVector& Location)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(GetRootComponent());
		Component->SetRelativeLocation(Location);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		const FString Path = FString(TEXT("/Game/Mesh/hansa-grain-farm/Final/")) + Asset;
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*Path);
		Component->SetStaticMesh(Mesh.Object);
		return Component;
	};
	// Whole assembly stays within the 1560 cm inset at authored identity scale.
	Farm = AddRole(TEXT("CompletedFarm"), TEXT("SM_HansaGrainFarm"), FVector(0,-195,0));
	Construction = AddRole(TEXT("ConstructionFrame"), TEXT("SM_HansaGrainFarm_Construction"), FVector(0,-195,0));
	WorkProps = AddRole(TEXT("WorkingSheaves"), TEXT("SM_HansaFarmProp_SheavesTools"), FVector(0,300,0));
	const TCHAR* Meshes[] = {TEXT("SM_HansaGrainField_Corner_Mature_4m"), TEXT("SM_HansaGrainField_Center_Mature_4m"), TEXT("SM_HansaGrainField_Edge_Mature_4m")};
	for (int32 Index=0; Index<3; ++Index)
	{
		const FVector Location((Index-1)*400,560,0);
		Fields.Add(AddRole(*FString::Printf(TEXT("MatureField%d"),Index), Meshes[Index], Location));
		// FBX mirrors source Y; turn the edge taper toward the outer field boundary.
		Fields.Last()->SetRelativeRotation(FRotator(0,180,0));
		Furrows.Add(AddRole(*FString::Printf(TEXT("Furrows%d"),Index), TEXT("SM_HansaGrainField_Furrow_4m"), Location));
	}
	ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus::Ready);
}

void AHansaGrainFarmPresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	const bool bConstructing = Status == EHansaBuildingWorldStatus::UnderConstruction;
	const bool bWorking = Status == EHansaBuildingWorldStatus::Ready;
	Farm->SetVisibility(!bConstructing);
	Construction->SetVisibility(bConstructing);
	WorkProps->SetVisibility(bWorking);
	// A production pause does not change the season or erase mature crops.
	for (const auto& Component : Fields) Component->SetVisibility(!bConstructing);
	for (const auto& Component : Furrows) Component->SetVisibility(bConstructing);
}
