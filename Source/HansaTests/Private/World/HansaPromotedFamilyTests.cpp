#include "Misc/AutomationTest.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Materials/MaterialInterface.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPromotedFamilyTest, "Hansa.World.PromotedFamilies.P12P15",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPromotedFamilyTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient test world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); };
    const TArray<TTuple<FString, FString, int32>> Families = {
        {TEXT("LumberCamp"), TEXT("hansa-lumber-camp"), 5},
        {TEXT("Sawmill"), TEXT("hansa-sawmill"), 5},
        {TEXT("Residence_Laborer"), TEXT("hansa-residences"), 1},
        {TEXT("Residence_Artisan"), TEXT("hansa-artisan-houses"), 1},
        {TEXT("Market"), TEXT("hansa-market"), 6},
        {TEXT("Dock"), TEXT("hansa-harbor"), 7}
    };
    for (const auto& Family : Families)
    {
        const FString Path = TEXT("/Game/Hansa/Core/Buildings/DA_Building_") + Family.Get<0>();
        auto* Definition = LoadObject<UHansaBuildingDefinition>(nullptr, *(Path + TEXT(".DA_Building_") + Family.Get<0>()));
        if (!TestNotNull(*Path, Definition)) continue;
        UClass* Class = Definition->LoadPresentationActorClass();
        if (!TestNotNull(TEXT("Promoted class resolves through production loader"), Class)) continue;
        const FString Root = TEXT("/Game/Mesh/") + Family.Get<1>() + TEXT("/");
        TestTrue(TEXT("Class is canonically promoted"), Class->GetPathName().StartsWith(Root));
        TestNotNull(TEXT("Main mesh resolves"), Definition->LoadPresentationMesh());
        AActor* Actor = World->SpawnActor<AActor>(Class);
        if (!TestNotNull(TEXT("Promoted actor spawns"), Actor)) continue;
        TestFalse(TEXT("Cosmetic actor never ticks"), Actor->PrimaryActorTick.bCanEverTick);
        TArray<UStaticMeshComponent*> Components;
        Actor->GetComponents(Components);
        TestEqual(TEXT("All expected authored components survive promotion"), Components.Num(), Family.Get<2>());
        for (UStaticMeshComponent* Component : Components)
        {
            UStaticMesh* Mesh = Component->GetStaticMesh();
            if (!TestNotNull(TEXT("Authored role has a mesh"), Mesh)) continue;
            TestTrue(TEXT("Mesh has no staging dependency"), Mesh->GetPathName().StartsWith(Root));
            TestTrue(TEXT("Identity component scale"), Component->GetRelativeScale3D().Equals(FVector::OneVector));
            TestEqual(TEXT("Three real LODs retained"), Mesh->GetNumLODs(), 3);
            TestEqual(TEXT("Cosmetic components do not own collision"), Component->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
            TestFalse(TEXT("Cosmetic components do not own navigation"), Component->CanEverAffectNavigation());
            for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
            {
                UMaterialInterface* Material = Component->GetMaterial(Index);
                TestTrue(TEXT("Material is assigned from the promoted family"), Material && Material->GetPathName().StartsWith(Root));
            }
        }
        Actor->Destroy();
    }
    World->DestroyWorld(false);
    return !HasAnyErrors();
}
#endif
