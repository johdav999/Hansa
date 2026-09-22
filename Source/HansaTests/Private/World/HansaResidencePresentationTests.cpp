#include "World/HansaResidencePresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResidenceVariantsTest, "Hansa.World.Residence.ParcelVariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaResidenceVariantsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ResidenceVariantsWorld"));
	auto* Residence = World->SpawnActor<AHansaResidencePresentation>();
	if (!TestNotNull(TEXT("Residence spawns"), Residence)) { World->DestroyWorld(false); return false; }
	TestNull(TEXT("No unapproved native asset default"), Residence->VariantA.Get());
	TestNull(TEXT("No second unapproved native default"), Residence->VariantB.Get());
	Residence->VariantA = NewObject<UStaticMesh>();
	Residence->VariantB = NewObject<UStaticMesh>();
	int32 Counts[2] = {0,0};
	for (int32 X = -12; X <= 12; ++X)
	{
		for (int32 Y = -12; Y <= 12; ++Y)
		{
			Residence->ApplyParcel(X,Y);
			const int32 Variant = AHansaResidencePresentation::VariantForParcel(X,Y);
			++Counts[Variant];
			TestTrue(TEXT("Parcel selects assigned mesh"), Residence->ResidenceMesh->GetStaticMesh() ==
				(Variant == 0 ? Residence->VariantA.Get() : Residence->VariantB.Get()));
			Residence->OnConstruction(FTransform::Identity);
			TestTrue(TEXT("Construction refresh retains variant"), Residence->ResidenceMesh->GetStaticMesh() ==
				(Variant == 0 ? Residence->VariantA.Get() : Residence->VariantB.Get()));
		}
	}
	TestTrue(TEXT("Both variants exercised across city parcels"), Counts[0] > 200 && Counts[1] > 200);
	TestTrue(TEXT("No auto-fit scale"), Residence->ResidenceMesh->GetRelativeScale3D().Equals(FVector::OneVector));
	TestFalse(TEXT("No per-frame cosmetic work"), Residence->PrimaryActorTick.bCanEverTick);
	TestEqual(TEXT("No art collision authority"), Residence->ResidenceMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("No art navigation authority"), Residence->ResidenceMesh->CanEverAffectNavigation());

	Residence->VariantC = NewObject<UStaticMesh>();
	Residence->VariantD = NewObject<UStaticMesh>();
	UStaticMesh* FourMeshes[] = {Residence->VariantA.Get(), Residence->VariantB.Get(), Residence->VariantC.Get(), Residence->VariantD.Get()};
	int32 FourCounts[4] = {0,0,0,0};
	for (int32 X = -12; X <= 12; ++X)
		for (int32 Y = -12; Y <= 12; ++Y)
		{
			const int32 Variant = AHansaResidencePresentation::VariantForParcel(X,Y,4);
			++FourCounts[Variant];
			Residence->ApplyParcel(X,Y);
			TestTrue(TEXT("Four-variant parcel selects authored mesh"), Residence->ResidenceMesh->GetStaticMesh() == FourMeshes[Variant]);
			Residence->SetActorRotation(FRotator(0,90,0));
			Residence->OnConstruction(FTransform::Identity);
			TestTrue(TEXT("Rotation and construction preserve four-variant identity"), Residence->ResidenceMesh->GetStaticMesh() == FourMeshes[Variant]);
		}
	for (const int32 Count : FourCounts) TestTrue(TEXT("All four variants appear across negative and positive parcels"), Count > 100);
	Residence->VariantD = nullptr;
	Residence->ApplyParcel(-7,5);
	TestTrue(TEXT("Incomplete optional pair preserves legacy two-variant selection"), Residence->ResidenceMesh->GetStaticMesh() == FourMeshes[AHansaResidencePresentation::VariantForParcel(-7,5)]);
	Residence->VariantC = nullptr;
	Residence->VariantA = nullptr; Residence->VariantB = nullptr; Residence->ApplyParcel(0,0);
	TestNull(TEXT("Missing reviewed mesh does not retain stale art"), Residence->ResidenceMesh->GetStaticMesh());
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaArtisanProductionTest, "Hansa.World.Residence.ArtisanProduction",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaArtisanProductionTest::RunTest(const FString& Parameters)
{
 auto* Definition = LoadObject<UHansaBuildingDefinition>(nullptr, TEXT("/Game/Hansa/Core/Buildings/DA_Building_Residence_Artisan.DA_Building_Residence_Artisan"));
 if (!TestNotNull(TEXT("Artisan definition loads"), Definition)) return false;
 UClass* Class = Definition->LoadPresentationActorClass();
 if (!TestNotNull(TEXT("Artisan class loads"), Class)) return false;
 auto* Defaults = Cast<AHansaResidencePresentation>(Class->GetDefaultObject());
 if (!TestNotNull(TEXT("Artisan uses parcel presentation"), Defaults)) return false;
 TSet<UStaticMesh*> Unique;
 for (UStaticMesh* Mesh : {Defaults->VariantA.Get(), Defaults->VariantB.Get(), Defaults->VariantC.Get(), Defaults->VariantD.Get()})
 {
  if (!TestNotNull(TEXT("Each production variant assigned"), Mesh)) continue;
  Unique.Add(Mesh);
  TestTrue(TEXT("Variant has production path"), Mesh->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-artisan-houses/Meshes/")));
  TestEqual(TEXT("Three LODs per variant"), Mesh->GetNumLODs(), 3);
  TestEqual(TEXT("Seven assigned material families"), Mesh->GetStaticMaterials().Num(), 7);
  const FBox Bounds = Mesh->GetBoundingBox();
  TestTrue(TEXT("Model fits existing parcel without scaling"), Bounds.Min.X >= -400 && Bounds.Max.X <= 400 && Bounds.Min.Y >= -400 && Bounds.Max.Y <= 400);
  TestTrue(TEXT("Model sits on ground"), FMath::Abs(Bounds.Min.Z) <= 2);
  TestTrue(TEXT("Simple collision retained for asset reuse"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0);
  for (const auto& Slot : Mesh->GetStaticMaterials()) TestNotNull(TEXT("Every slot assigned"), Slot.MaterialInterface.Get());
 }
 TestEqual(TEXT("Four distinct production meshes"), Unique.Num(), 4);
 TestEqual(TEXT("Unchanged capacity"), Definition->ResidenceCapacity, 8);
 TestEqual(TEXT("Unchanged parcel width"), Definition->FootprintWidthCells, 2);
 TestEqual(TEXT("Unchanged parcel depth"), Definition->FootprintHeightCells, 2);
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResidenceStagedTest, "Hansa.World.Residence.StagedFamily",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::RequiresUser)
bool FHansaResidenceStagedTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ResidenceStagedWorld"));
	double LaborerHeight = 0;
	for (const FString Tier : {FString(TEXT("Laborer")), FString(TEXT("Artisan"))})
	{
		const FString Path = FString::Printf(TEXT("/Game/Hansa/Generated/Staging/Residences_P14/BP_Residence_%s_Review.BP_Residence_%s_Review_C"), *Tier, *Tier);
		UClass* Class = LoadClass<AHansaResidencePresentation>(nullptr, *Path);
		if (!TestNotNull(TEXT("Review tier class loads"), Class)) continue;
		auto* Actor = World->SpawnActor<AHansaResidencePresentation>(Class);
		if (!TestNotNull(TEXT("Review tier actor spawns"), Actor)) continue;
		TestTrue(TEXT("A and B are distinct reviewed variants"), Actor->VariantA != Actor->VariantB);
		for (UStaticMesh* Mesh : {Actor->VariantA.Get(), Actor->VariantB.Get()})
		{
			if (!TestNotNull(TEXT("Reviewed mesh assigned"), Mesh)) continue;
			TestTrue(TEXT("Role remains in this review folder"), Mesh->GetPathName().StartsWith(TEXT("/Game/Hansa/Generated/Staging/Residences_P14/")));
			TestEqual(TEXT("Three real imported LODs"), Mesh->GetNumLODs(), 3);
			TestTrue(TEXT("Simple collision generated"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0);
			const FBox Bounds = Mesh->GetBoundingBox();
			TestTrue(TEXT("Both tiers fit unchanged 2x2 parcel"), Bounds.Min.X >= -380 && Bounds.Max.X <= 380 && Bounds.Min.Y >= -380 && Bounds.Max.Y <= 380);
			TestTrue(TEXT("Ground contact within 2cm"), FMath::Abs(Bounds.Min.Z) <= 2);
			TestEqual(TEXT("Bounded shared material families"), Mesh->GetStaticMaterials().Num(), 7);
			for (const auto& Slot : Mesh->GetStaticMaterials()) TestNotNull(TEXT("No unassigned material"), Slot.MaterialInterface.Get());
			if (Tier == TEXT("Laborer")) LaborerHeight = FMath::Max(LaborerHeight, Bounds.GetSize().Z);
			else TestTrue(TEXT("Artisan gains a full floor, not scaled geometry"), Bounds.GetSize().Z > LaborerHeight + 200);
		}
		for (int32 X = -4; X <= 4; ++X)
		{
			Actor->ApplyParcel(X, 3);
			const auto* Expected = AHansaResidencePresentation::VariantForParcel(X,3) == 0 ? Actor->VariantA.Get() : Actor->VariantB.Get();
			TestTrue(TEXT("Actual reviewed mesh follows stable parcel selection"), Actor->ResidenceMesh->GetStaticMesh() == Expected);
			Actor->SetActorRotation(FRotator(0, X*90, 0));
			Actor->ApplyParcel(X, 3);
			TestTrue(TEXT("Rotation cannot change selected variant"), Actor->ResidenceMesh->GetStaticMesh() == Expected);
		}
		const FString Id = TEXT("Building.Residence.") + Tier;
		auto* Definition = Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(Id));
		if (TestNotNull(TEXT("Stable tier definition resolves"), Definition))
		{
			TestEqual(TEXT("2-cell width unchanged"), Definition->FootprintWidthCells, 2);
			TestEqual(TEXT("2-cell depth unchanged"), Definition->FootprintHeightCells, 2);
			TestEqual(TEXT("Visual work preserves population capacity"), Definition->ResidenceCapacity, Tier == TEXT("Laborer") ? 12 : 8);
		}
	}
	World->DestroyWorld(false);
	return true;
}
#endif
