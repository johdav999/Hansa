#include "World/HansaResidencePresentation.h"
#include "Components/StaticMeshComponent.h"

AHansaResidencePresentation::AHansaResidencePresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	ResidenceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResidenceMesh"));
	SetRootComponent(ResidenceMesh);
	ResidenceMesh->SetMobility(EComponentMobility::Movable);
	ResidenceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResidenceMesh->SetGenerateOverlapEvents(false);
	ResidenceMesh->SetCanEverAffectNavigation(false);
}

int32 AHansaResidencePresentation::VariantForParcel(const int32 X, const int32 Y, const int32 VariantCount)
{
	// Unsigned fixed mixing is stable for negative coordinates, save/load and tier upgrades.
	uint32 Hash = static_cast<uint32>(X) * 0x9e3779b9u ^ static_cast<uint32>(Y) * 0x85ebca6bu;
	Hash ^= Hash >> 16;
	Hash *= 0x7feb352du;
	Hash ^= Hash >> 15;
	return static_cast<int32>(Hash % static_cast<uint32>(VariantCount == 4 ? 4 : 2));
}

void AHansaResidencePresentation::ApplyParcel(const int32 X, const int32 Y)
{
	SelectedVariant = VariantForParcel(X, Y, VariantC && VariantD ? 4 : 2);
	RefreshMesh();
}

void AHansaResidencePresentation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshMesh();
}

void AHansaResidencePresentation::RefreshMesh()
{
	UStaticMesh* Variants[] = { VariantA.Get(), VariantB.Get(), VariantC.Get(), VariantD.Get() };
	UStaticMesh* Selected = Variants[FMath::Clamp(SelectedVariant, 0, 3)];
	// Missing variant is a validation error, never silently substituted or scaled.
	if (ResidenceMesh->GetStaticMesh() != Selected) ResidenceMesh->SetStaticMesh(Selected);
}
