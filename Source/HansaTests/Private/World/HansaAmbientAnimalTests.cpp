#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "World/HansaAmbientAnimals.h"
#include "World/HansaAmbientRabbits.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAnimalBehaviorTest, "Hansa.World.Animals.MixedSpecies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaAnimalBehaviorTest::RunTest(const FString&)
{
    const auto Dog = FHansaAmbientAnimalProfile::Dog();
    auto* Mesh = Dog.Mesh.LoadSynchronous();
    auto* Walk = Dog.Walk.LoadSynchronous();
    FString Error;
    if (!TestTrue(TEXT("Promoted dog contract"), AHansaAmbientAnimals::ValidateProfile(Dog,Mesh,Walk,nullptr,Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Dog material assigned"), Mesh->GetMaterials()[0].MaterialInterface->GetPathName().StartsWith(TEXT("/Game/Hansa/Animals/Dog/M_Dog")));
    TestTrue(TEXT("Dog is life sized in centimetres"), Mesh->GetBounds().BoxExtent.GetMax() > 30 && Mesh->GetBounds().BoxExtent.GetMax() < 100);
    auto Invalid = Dog; Invalid.MaximumPopulation = 1;
    TestFalse(TEXT("Inverted population rejected"), AHansaAmbientAnimals::ValidateProfile(Invalid,Mesh,Walk,nullptr,Error));
    Invalid = Dog; Invalid.WalksBetweenJumps = 1;
    TestFalse(TEXT("Jump behaviour requires clip"), AHansaAmbientAnimals::ValidateProfile(Invalid,Mesh,Walk,nullptr,Error));
    Invalid = Dog; Invalid.WalkCycleSeconds = 1;
    TestFalse(TEXT("Wrong gait timing rejected"), AHansaAmbientAnimals::ValidateProfile(Invalid,Mesh,Walk,nullptr,Error));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Terrain = World->SpawnActor<AStaticMeshActor>();
    auto* Floor = Terrain->GetStaticMeshComponent(); Floor->SetMobility(EComponentMobility::Movable);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetCollisionProfileName(TEXT("BlockAll")); Terrain->Tags.Add(TEXT("Hansa.Terrain"));
    Terrain->SetActorScale3D(FVector(240,160,1)); Terrain->SetActorLocation(FVector(0,0,200));
    auto* Manager = World->SpawnActor<AHansaAmbientAnimals>();
    Manager->Foundation = Foundation; Manager->Host = NewObject<UHansaRuntimeSimulationHost>(Manager);
    if (!TestTrue(TEXT("Lubeck ready"), Manager->Host->InitializeForLubeck(World,Error))) { AddError(Error); return false; }
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal); Manager->Tick(.1f);
    TestTrue(TEXT("Both species enabled"), Manager->ValidationError.IsEmpty());
    TestEqual(TEXT("Six rabbits retained"), Manager->GetLiveAnimalCount(TEXT("Rabbit")),6);
    const int32 Dogs = Manager->GetLiveAnimalCount(TEXT("Dog"));
    TestTrue(TEXT("Two or three dogs spawned"), Dogs >= 2 && Dogs <= 3);
    TestEqual(TEXT("Total population"),Manager->GetLiveAnimalCount(),6+Dogs);
    const auto Fingerprint = Manager->Host->BuildProjection().Value.GetFingerprint().Value;
    TSet<FString> Ids;
    for (const auto& O : Manager->QueryAnimals()) { TestFalse(TEXT("Unique diagnostics ID"), Ids.Contains(O.StableId)); Ids.Add(O.StableId); }
    bool bDogWalked = false, bDogRested = false, bRabbitJumped = false, bMeasuredSpeed = false;
    for (int32 Frame = 0; Frame < 1800; ++Frame)
    {
        TArray<FVector> Before;
        for (const auto& A : Manager->Animals) Before.Add(A.Mesh->GetComponentLocation());
        const auto Previous = Manager->Animals;
        Manager->Tick(.1f);
        for (int32 I = 0; I < Manager->Animals.Num(); ++I)
        {
            const auto& A = Manager->Animals[I]; const auto& P = Manager->Species[A.ProfileIndex];
            if (P.SpeciesId == TEXT("Rabbit")) { bRabbitJumped |= A.Activity == EHansaAnimalActivity::Jump; continue; }
            TestTrue(TEXT("Dog never uses rabbit jump"), A.Activity != EHansaAnimalActivity::Jump);
            const double PelvisHeight = A.Mesh->GetBoneLocation(TEXT("pelvis")).Z - A.Mesh->GetComponentLocation().Z;
            TestTrue(TEXT("Evaluated dog pelvis stays at life-size height above ground"), PelvisHeight > 30 && PelvisHeight < 55);
            if (A.Activity == EHansaAnimalActivity::Still || A.bTurning)
            {
                bDogRested = true;
                TestNull(TEXT("Dog rests in reference pose"), A.Mesh->GetSingleNodeInstance()->GetCurrentAsset());
            }
            if (A.Activity == EHansaAnimalActivity::Walk && !A.bTurning)
            {
                bDogWalked = true;
                TestEqual(TEXT("Dog uses its walk clip"), A.Mesh->GetSingleNodeInstance()->GetCurrentAsset(), static_cast<UAnimationAsset*>(Walk));
                if (Previous[I].Activity == EHansaAnimalActivity::Walk && !Previous[I].bTurning && A.Elapsed < A.Duration)
                {
                    const double Travel = FVector::Dist2D(Before[I],A.Mesh->GetComponentLocation());
                    TestTrue(TEXT("Dog moves at authored 26.67 cm/s"), FMath::IsNearlyEqual(Travel,32./1.2*.1,.01)); bMeasuredSpeed = true;
                }
            }
        }
    }
    TestTrue(TEXT("Dogs walk and rest"), bDogWalked && bDogRested && bMeasuredSpeed);
    TestTrue(TEXT("Rabbits still jump with dogs present"), bRabbitJumped);
    TestEqual(TEXT("Dog population stable"),Manager->GetLiveAnimalCount(TEXT("Dog")),Dogs);
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    const auto Paused = Manager->QueryAnimals(); Manager->Tick(.1f);
    const auto After = Manager->QueryAnimals();
    for (int32 I=0; I<Paused.Num(); ++I) { TestEqual(TEXT("Pause freezes all locations"),After[I].Location,Paused[I].Location); TestEqual(TEXT("Pause freezes animation"),After[I].AnimationTime,Paused[I].AnimationTime); }
    TestEqual(TEXT("Wildlife does not change simulation"),Manager->Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
    auto* Legacy = World->SpawnActor<AHansaAmbientRabbits>(); Legacy->Population = 5; Legacy->TownRadius = 4500; Legacy->Tick(0);
    TestEqual(TEXT("Legacy population preserved"),Legacy->Species[0].MinimumPopulation,5);
    TestEqual(TEXT("Legacy radius preserved"),Legacy->Species[0].TownRadius,4500.f);
    TestEqual(TEXT("Legacy actors gain dogs"),Legacy->Species[1].SpeciesId,FName(TEXT("Dog")));
    auto* Extension = World->SpawnActor<AHansaAmbientAnimals>();
    auto Other = Dog; Other.SpeciesId = TEXT("OtherAnimal"); Other.MinimumPopulation = Other.MaximumPopulation = 1;
    Extension->Species = {Other}; Extension->Host = Manager->Host; Extension->Foundation = Foundation;
    Extension->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal); Extension->Tick(.1f);
    TestEqual(TEXT("A third profile requires no manager code"),Extension->GetLiveAnimalCount(TEXT("OtherAnimal")),1);
    AddInfo(FString::Printf(TEXT("Mixed population: 6 rabbits, %d dogs; tested 180 seconds."),Dogs));
    return true;
}
#endif
