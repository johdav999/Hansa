#include "World/HansaSawmillPresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Definitions/HansaEconomicDefinitions.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSawmillRolesTest, "Hansa.World.Sawmill.PresentationRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSawmillRolesTest::RunTest(const FString& Parameters)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SawmillRolesWorld"));
	auto* Camp = World->SpawnActor<AHansaSawmillPresentation>();
	if (!TestNotNull(TEXT("Camp spawns"), Camp)) { World->DestroyWorld(false); return false; }
	TArray<UStaticMeshComponent*> Roles; Camp->GetComponents(Roles);
	TestEqual(TEXT("Bounded five-role assembly"), Roles.Num(), 5);
	TestFalse(TEXT("No cosmetic actor tick"), Camp->PrimaryActorTick.bCanEverTick);
	for (auto* Role : Roles)
	{
		TestTrue(TEXT("Identity scale"), Role->GetRelativeScale3D().Equals(FVector::OneVector));
		TestEqual(TEXT("No art collision changes gameplay"), Role->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("No navigation side effects"), Role->CanEverAffectNavigation());
		TestNull(TEXT("Native defaults never reference unapproved media"), Role->GetStaticMesh());
	}
	Camp->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestTrue(TEXT("Authored hall remains visible"), Camp->WorkBuilding->IsVisible());
	TestFalse(TEXT("No operating stock during construction"), Camp->LogInput->IsVisible() || Camp->PlankOutput->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Ready);
	TestTrue(TEXT("Ready production exposes timber cue"), Camp->PlankOutput->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestFalse(TEXT("Blocked removes active output cue"), Camp->PlankOutput->IsVisible());
	TestTrue(TEXT("Blocked preserves yard structure"), Camp->LogInput->IsVisible() && Camp->Rack->IsVisible());
	auto* Restored = World->SpawnActor<AHansaSawmillPresentation>();
	Restored->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestEqual(TEXT("Read-only state reconstructs output visibility"), Restored->PlankOutput->IsVisible(), Camp->PlankOutput->IsVisible());
	Roles.Reset(); Camp->GetComponents(Roles);
	TestEqual(TEXT("Repeated projection creates no roles"), Roles.Num(), 5);
	TestTrue(TEXT("Imported handedness preserved"), Camp->LogInput->GetRelativeLocation().Equals(FVector(470,365,0)));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSawmillStagedAssetTest, "Hansa.World.Sawmill.StagedAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::RequiresUser)
bool FHansaSawmillStagedAssetTest::RunTest(const FString& Parameters)
{
	UClass* Class = LoadClass<AHansaSawmillPresentation>(nullptr,
		TEXT("/Game/Hansa/Generated/Staging/Sawmill_P13/BP_Sawmill_Review.BP_Sawmill_Review_C"));
	if (!TestNotNull(TEXT("Staged review class loads"), Class)) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SawmillAssetWorld"));
	auto* Camp = World->SpawnActor<AHansaSawmillPresentation>(Class);
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
	TestTrue(TEXT("Plot inset on all four edges"), Bounds.Min.X >= -780 && Bounds.Min.Y >= -580 && Bounds.Max.X <= 780 && Bounds.Max.Y <= 580);
	TestTrue(TEXT("Ground pivot within 2cm"), FMath::Abs(Bounds.Min.Z) <= 2);
	TestTrue(TEXT("Real metre-scale hall"), Bounds.GetSize().Z > 550 && Bounds.GetSize().Z < 590);
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	Camp->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestTrue(TEXT("Authored construction hall"), Camp->WorkBuilding->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Ready);
	TestTrue(TEXT("Ready timber cue"), Camp->PlankOutput->IsVisible());
	Camp->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestFalse(TEXT("Blocked timber cue hidden"), Camp->PlankOutput->IsVisible());
	// Test real stable-ID resolution with an in-memory review binding only.
	// Scope-exit restoration never marks/saves the production definition package.
	auto* Definition = Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Sawmill")));
	if (TestNotNull(TEXT("Stable Sawmill definition resolves"), Definition))
	{
		const auto OriginalClass = Definition->PresentationActorClass;
		ON_SCOPE_EXIT { Definition->PresentationActorClass = OriginalClass; };
		Definition->PresentationActorClass = Class;
		TestEqual(TEXT("Authoritative 4x3 footprint width"), Definition->FootprintWidthCells, 4);
		TestEqual(TEXT("Authoritative 4x3 footprint height"), Definition->FootprintHeightCells, 3);
		TestTrue(TEXT("Road contract retained"), Definition->bRequiresRoad);
		TestFalse(TEXT("Manual power needs no shoreline"), Definition->bRequiresShoreline);
		TestNull(TEXT("Production loader rejects the staged Blueprint even with an in-memory binding"), Definition->LoadPresentationActorClass());
		Definition->PresentationActorClass = AHansaSawmillPresentation::StaticClass();
		TestEqual(TEXT("Native presentation type resolves without a staged path"), Definition->LoadPresentationActorClass(), AHansaSawmillPresentation::StaticClass());
	}
	World->DestroyWorld(false);
	return true;
}
#endif
