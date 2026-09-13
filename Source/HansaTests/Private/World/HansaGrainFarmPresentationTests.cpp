#include "World/HansaGrainFarmPresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGrainFarmRolesTest, "Hansa.World.GrainFarm.PresentationRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaGrainFarmRolesTest::RunTest(const FString& Parameters)
{
	using Hansa::Simulation::EHansaBuildingWorldStatus;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaGrainFarmRolesTestWorld"));
	auto* Farm = World->SpawnActor<AHansaGrainFarmPresentation>();
	if (!TestNotNull(TEXT("Farm assembly spawns"), Farm)) { World->DestroyWorld(false); return false; }
	TArray<UStaticMeshComponent*> Components; Farm->GetComponents(Components);
	TestEqual(TEXT("All authored roles present"), Components.Num(), 9);
	for (auto* Component : Components)
	{
		const UStaticMesh* Mesh = Component->GetStaticMesh();
		TestNotNull(*Component->GetName(), Mesh);
		TestTrue(TEXT("Authored scale one"), Component->GetRelativeScale3D().Equals(FVector::OneVector));
		TestEqual(TEXT("Art cannot claim navigation/clicks"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		if (Component->GetStaticMesh()) TestEqual(TEXT("Three LODs"), Component->GetStaticMesh()->GetNumLODs(), 3);
	}
	const FBox Bounds = Farm->CalculateComponentsBoundingBoxInLocalSpace(true, true);
	TestTrue(TEXT("Whole assembly fits 1560 cm inset"), Bounds.GetSize().X <= 1560 && Bounds.GetSize().Y <= 1560);
	TestTrue(TEXT("Ground pivot within 2 cm"), FMath::Abs(Bounds.Min.Z) <= 2);
	Farm->ApplyStatus(EHansaBuildingWorldStatus::UnderConstruction);
	TestTrue(TEXT("Authored scaffold visible"), Farm->Construction->IsVisible());
	TestFalse(TEXT("Completed building hidden"), Farm->Farm->IsVisible());
	TestFalse(TEXT("Work props hidden"), Farm->WorkProps->IsVisible());
	for (const auto& Field : Farm->Fields) TestFalse(TEXT("No crop during construction"), Field->IsVisible());
	for (const auto& Field : Farm->Furrows) TestTrue(TEXT("Furrows during construction"), Field->IsVisible());
	Farm->ApplyStatus(EHansaBuildingWorldStatus::Ready);
	TestTrue(TEXT("Farm and work cues visible"), Farm->Farm->IsVisible() && Farm->WorkProps->IsVisible());
	TestFalse(TEXT("Scaffold hidden when complete"), Farm->Construction->IsVisible());
	Farm->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestFalse(TEXT("Blocked production hides active work"), Farm->WorkProps->IsVisible());
	for (const auto& Field : Farm->Fields) TestTrue(TEXT("Pause preserves seasonal crop"), Field->IsVisible());
	auto* Restored = World->SpawnActor<AHansaGrainFarmPresentation>();
	Restored->ApplyStatus(EHansaBuildingWorldStatus::Blocked);
	TestEqual(TEXT("Reconstructed visibility matches"), Restored->WorkProps->IsVisible(), Farm->WorkProps->IsVisible());
	World->DestroyWorld(false);
	return true;
}
#endif
