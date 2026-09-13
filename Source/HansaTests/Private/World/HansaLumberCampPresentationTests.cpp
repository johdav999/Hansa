#include "World/HansaLumberCampPresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLumberCampRolesTest, "Hansa.World.LumberCamp.PresentationRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaLumberCampRolesTest::RunTest(const FString& Parameters)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LumberCampRolesWorld"));
	auto* Camp = World->SpawnActor<AHansaLumberCampPresentation>();
	if (!TestNotNull(TEXT("Camp spawns"), Camp)) { World->DestroyWorld(false); return false; }
	TArray<UStaticMeshComponent*> Roles; Camp->GetComponents(Roles);
	TestEqual(TEXT("Bounded five-role assembly"), Roles.Num(), 5);
	TestFalse(TEXT("No cosmetic actor tick"), Camp->PrimaryActorTick.bCanEverTick);
	for (auto* Role : Roles)
	{
		TestTrue(TEXT("Identity scale"), Role->GetRelativeScale3D().Equals(FVector::OneVector));
		TestEqual(TEXT("No art collision changes gameplay"), Role->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("No navigation side effects"), Role->CanEverAffectNavigation());
	}
	Camp->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestTrue(TEXT("Authored hall remains visible"), Camp->WorkBuilding->IsVisible());
	TestFalse(TEXT("No operating stock during construction"), Camp->LogPile->IsVisible() || Camp->CutTimber->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Ready);
	TestTrue(TEXT("Ready production exposes timber cue"), Camp->CutTimber->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestFalse(TEXT("Blocked removes active output cue"), Camp->CutTimber->IsVisible());
	TestTrue(TEXT("Blocked preserves yard structure"), Camp->LogPile->IsVisible() && Camp->ToolShelter->IsVisible());
	auto* Restored = World->SpawnActor<AHansaLumberCampPresentation>();
	Restored->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestEqual(TEXT("Read-only state reconstructs output visibility"), Restored->CutTimber->IsVisible(), Camp->CutTimber->IsVisible());
	Roles.Reset(); Camp->GetComponents(Roles);
	TestEqual(TEXT("Repeated projection creates no roles"), Roles.Num(), 5);
	TestTrue(TEXT("Imported handedness preserved"), Camp->LogPile->GetRelativeLocation().Equals(FVector(340,360,0)));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLumberCampStagedAssetTest, "Hansa.World.LumberCamp.StagedAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::RequiresUser)
bool FHansaLumberCampStagedAssetTest::RunTest(const FString& Parameters)
{
	UClass* Class = LoadClass<AHansaLumberCampPresentation>(nullptr,
		TEXT("/Game/Hansa/Generated/Staging/LumberCamp_P12/BP_LumberCamp_Review.BP_LumberCamp_Review_C"));
	if (!TestNotNull(TEXT("Staged review class loads"), Class)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LumberCampAssetWorld"));
	auto* Camp = World->SpawnActor<AHansaLumberCampPresentation>(Class);
	if (!TestNotNull(TEXT("Reviewed actor spawns"), Camp)) { World->DestroyWorld(false); return false; }
	TArray<UStaticMeshComponent*> Roles; Camp->GetComponents(Roles);
	TestEqual(TEXT("Five modular roles"), Roles.Num(), 5);
	for (auto* Role : Roles)
	{
		const UStaticMesh* Mesh = Role->GetStaticMesh();
		if (!TestNotNull(*Role->GetName(), Mesh)) continue;
		TestFalse(TEXT("No engine primitive role"), Mesh->GetPathName().StartsWith(TEXT("/Engine/")));
		TestEqual(TEXT("Three LODs"), Mesh->GetNumLODs(), 3);
		TestTrue(TEXT("Simple collision authored"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0);
		TestTrue(TEXT("No auto-fit scale"), Role->GetRelativeScale3D().Equals(FVector::OneVector));
		for (const auto& Slot : Mesh->GetStaticMaterials()) TestNotNull(TEXT("Every PBR slot assigned"), Slot.MaterialInterface.Get());
	}
	const FBox Bounds = Camp->CalculateComponentsBoundingBoxInLocalSpace(true, true);
	TestTrue(TEXT("Plot inset on all four edges"), Bounds.Min.X >= -580 && Bounds.Min.Y >= -580 && Bounds.Max.X <= 580 && Bounds.Max.Y <= 580);
	TestTrue(TEXT("Ground pivot within 2cm"), FMath::Abs(Bounds.Min.Z) <= 2);
	TestTrue(TEXT("Real metre-scale hall"), Bounds.GetSize().Z > 490 && Bounds.GetSize().Z < 510);
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	Camp->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestTrue(TEXT("Authored construction hall"), Camp->WorkBuilding->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Ready);
	TestTrue(TEXT("Ready timber cue"), Camp->CutTimber->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestFalse(TEXT("Blocked timber cue hidden"), Camp->CutTimber->IsVisible());
	World->DestroyWorld(false);
	return true;
}
#endif
