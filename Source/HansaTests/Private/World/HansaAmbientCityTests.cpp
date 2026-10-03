#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "World/HansaAmbientPeople.h"
#include "World/HansaAmbientAnimals.h"
#include "World/HansaCityCentrePresentation.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAmbientCityTest,"Hansa.World.AmbientCities.SwitchAndSuspend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaAmbientCityTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation=World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Terrain=World->SpawnActor<AStaticMeshActor>();
    auto* Floor=Terrain->GetStaticMeshComponent(); Floor->SetMobility(EComponentMobility::Movable);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetCollisionProfileName(TEXT("BlockAll")); Terrain->Tags.Add(TEXT("Hansa.Terrain"));
    Terrain->SetActorScale3D(FVector(2000,1000,1)); Terrain->SetActorLocation(FVector(40000,0,200));
    auto* Centre=World->SpawnActor<AHansaCityCentrePresentation>(); Centre->SetActorLocation(FVector(60000,0,250)); Centre->Tick(0);
    Hansa::Game::FAmbientStreetNetwork Streets; Streets.BuildMunicipal(*Centre);
    TestTrue(TEXT("Municipal streets and landmarks become a reusable graph"),Streets.Roads.Num()>20 && Streets.Visits.Num()>5);
    bool Connected=false;
    for (const auto& A:Streets.Visits) for (const auto& B:Streets.Visits)
    {
        TArray<FIntPoint> Route;
        if (A.Road!=B.Road && Streets.Route(A.Road,B.Road,Route)) Connected=true;
    }
    TestTrue(TEXT("Municipal citizens can reach another landmark"),Connected);
    auto* Host=NewObject<UHansaRuntimeSimulationHost>(World); FString Error;
    if (!TestTrue(TEXT("Simulation ready"),Host->InitializeForLubeck(World,Error))) { AddError(Error); return false; }
    Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    const auto Fingerprint=Host->BuildProjection().Value.GetFingerprint().Value;
    auto* People=World->SpawnActor<AHansaAmbientPeople>(); People->Host=Host; People->Foundation=Foundation;
    auto* Animals=World->SpawnActor<AHansaAmbientAnimals>(); Animals->Host=Host; Animals->Foundation=Foundation;
    auto* Controller=World->SpawnActor<APlayerController>(); auto* Camera=World->SpawnActor<AHansaStrategyCameraPawn>();
    World->AddController(Controller); Controller->Possess(Camera);
    if (!TestTrue(TEXT("Fixture camera is possessed"),World->GetFirstPlayerController()==Controller && Controller->GetPawn()==Camera)) return false;
    Camera->FocusWorldLocationIntent(Centre->GetMarketLocation());
    bool Walked=false,Visited=false;
    for (int32 Frame=0;Frame<900;++Frame)
    {
        People->Tick(.1f); Animals->Tick(.1f);
        for (const auto& P:People->QueryPeople())
        {
            Walked|=P.bVisible && P.Activity==EHansaPersonActivity::Walking;
            Visited|=P.bVisible && P.Activity==EHansaPersonActivity::Visiting;
            if (P.DestinationBuilding!=0) { AddError(TEXT("Municipal landmark acquired an authoritative building ID")); return false; }
        }
    }
    TestEqual(TEXT("Camera selects Rostock"),People->GetAmbientCity(),FName(TEXT("City.Rostock")));
    TestTrue(TEXT("Municipal citizens walk and visit"),Walked && Visited);
    TestEqual(TEXT("Authored crowd target"),People->TargetPopulation,Centre->AmbientCitizens);
    TestEqual(TEXT("Rostock rabbits"),Animals->GetLiveAnimalCount(TEXT("Rabbit")),6);
    TestTrue(TEXT("Rostock dogs"),Animals->GetLiveAnimalCount(TEXT("Dog"))>=2);
    for (const auto& A:Animals->QueryAnimals()) TestTrue(TEXT("Diagnostic animal city is correct"),A.StableId.Contains(TEXT(".Rostock.")));
    TArray<USkeletalMeshComponent*> BeforePeople,BeforeAnimals;
    People->GetComponents(BeforePeople); Animals->GetComponents(BeforeAnimals);
    for (auto* C:BeforePeople) TestFalse(TEXT("No independent skeletal component ticks"),C->IsComponentTickEnabled());
    Camera->FocusWorldLocationIntent(FVector(1000000,1000000,0));
    People->Tick(.3f); Animals->Tick(.3f);
    const auto SleepingPeople=People->QueryPeople(); const auto SleepingAnimals=Animals->QueryAnimals();
    for (int32 Frame=0;Frame<30;++Frame) { People->Tick(.25f); Animals->Tick(.25f); }
    const auto AfterPeople=People->QueryPeople(); const auto AfterAnimals=Animals->QueryAnimals();
    for (int32 I=0;I<AfterPeople.Num();++I)
    {
        TestFalse(TEXT("Distant citizens hidden"),AfterPeople[I].bVisible);
        TestEqual(TEXT("Distant citizens do not move"),AfterPeople[I].Location,SleepingPeople[I].Location);
    }
    for (int32 I=0;I<AfterAnimals.Num();++I)
    {
        TestFalse(TEXT("Distant animals hidden"),AfterAnimals[I].bVisible);
        TestEqual(TEXT("Distant animal pose frozen"),AfterAnimals[I].AnimationTime,SleepingAnimals[I].AnimationTime);
        TestEqual(TEXT("Distant animals do not move"),AfterAnimals[I].Location,SleepingAnimals[I].Location);
    }
    TestEqual(TEXT("Dormant manager polls at 4 Hz"),People->GetActorTickInterval(),.25f);
    Camera->FocusWorldLocationIntent(Foundation->GetAutomationStartTransform().GetLocation());
    People->Tick(.3f); Animals->Tick(.3f);
    TestEqual(TEXT("Return to home city"),People->GetAmbientCity(),FName(TEXT("City.Lubeck")));
    Camera->FocusWorldLocationIntent(Centre->GetMarketLocation());
    for (int32 Frame=0;Frame<30;++Frame) { People->Tick(.1f); Animals->Tick(.1f); }
    TArray<USkeletalMeshComponent*> ReturnedPeople,ReturnedAnimals;
    People->GetComponents(ReturnedPeople); Animals->GetComponents(ReturnedAnimals);
    TestTrue(TEXT("People components reused between cities"),ReturnedPeople==BeforePeople);
    TestTrue(TEXT("Animal components reused between cities"),ReturnedAnimals==BeforeAnimals);
    TestTrue(TEXT("Returning crowd visible"),People->QueryPeople().ContainsByPredicate([](const auto& P){return P.bVisible;}));
    const auto Paused=Animals->QueryAnimals(); Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused); Animals->Tick(.1f);
    const auto PausedAfter=Animals->QueryAnimals();
    for(int32 I=0;I<Paused.Num();++I) TestEqual(TEXT("Pause preserved"),PausedAfter[I].AnimationTime,Paused[I].AnimationTime);
    TestEqual(TEXT("Ambient life cannot modify simulation"),Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
    // Same layout at a rotated third city uses no Rostock-specific coordinate branch.
    Centre->CityId=TEXT("City.Hamburg"); Centre->SetActorRotation(FRotator(0,90,0)); Centre->Tick(0);
    Camera->FocusWorldLocationIntent(Centre->GetMarketLocation()); Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    for (int32 Frame=0;Frame<30;++Frame) { People->Tick(.1f); Animals->Tick(.1f); }
    TestEqual(TEXT("Third city reusable"),People->GetAmbientCity(),FName(TEXT("City.Hamburg")));
    TestTrue(TEXT("Rotated third-city crowd"),People->QueryPeople().ContainsByPredicate([](const auto& P){return P.bVisible && P.CityId==TEXT("City.Hamburg");}));
    Centre->AmbientWaterLevel=1000; Animals->RefreshObstacles();
    FVector Ground;
    TestFalse(TEXT("Submerged municipal ground rejected"),Animals->SafeGround(Centre->GetMarketLocation(),Ground,Animals->Species[0]));
    return !HasAnyErrors();
}
#endif
