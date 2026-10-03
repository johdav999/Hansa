#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "World/HansaAmbientPeople.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Game;
using namespace Hansa::Simulation;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPeopleNetworkTest,"Hansa.World.People.StreetNetwork",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPeopleNetworkTest::RunTest(const FString&)
{
    const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    TArray<FHansaBuildingWorldProjection> Buildings;
    auto Add=[&](int32 Id,const TCHAR* Definition,int32 X,int32 Y,EHansaBuildingWorldStatus Status=EHansaBuildingWorldStatus::Ready)
    {
        auto& B=Buildings.AddDefaulted_GetRef(); B.BuildingId=FHansaBuildingId::TryCreate(Id).Value;
        B.Placement.CityId=City; B.Placement.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(Definition).Value;
        B.Status=Status; B.OccupiedCells={{X,Y}};
    };
    Add(1,TEXT("Building.Road"),0,0); Add(2,TEXT("Building.Road"),1,0); Add(3,TEXT("Building.Road"),1,1);
    Add(4,TEXT("Building.Road"),8,8); Add(5,TEXT("Building.Road"),2,1,EHansaBuildingWorldStatus::UnderConstruction);
    Add(6,TEXT("Building.GrainFarm"),0,-1); Add(7,TEXT("Building.Smithy"),1,2);
    Add(8,TEXT("Building.Bakery"),8,9,EHansaBuildingWorldStatus::UnderConstruction);
    auto Foreign=Buildings[0]; Foreign.Placement.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value; Foreign.OccupiedCells={{30,30}}; Buildings.Add(Foreign);
    FAmbientStreetNetwork Streets; Streets.Build(Buildings,City);
    TestEqual(TEXT("Only completed local roads"),Streets.Roads.Num(),4);
    TestEqual(TEXT("Only completed adjacent destinations"),Streets.Visits.Num(),2);
    TArray<FIntPoint> Route;
    TestTrue(TEXT("Street corner connected"),Streets.Route({0,0},{1,1},Route));
    TestEqual(TEXT("Corner is traversed, never diagonally cut"),Route.Num(),2);
    TestEqual(TEXT("Corner first step"),Route[0],FIntPoint(1,0));
    TestFalse(TEXT("Disconnected district has no shortcut"),Streets.Route({0,0},{8,8},Route));
    Streets.Roads.Remove({1,0});
    TestFalse(TEXT("Demolition breaks route"),Streets.Route({0,0},{1,1},Route));
    TestEqual(TEXT("Empty city has no people"),FAmbientStreetNetwork::DesiredCount(0,8,80,10,2),0);
    TestEqual(TEXT("Roads required"),FAmbientStreetNetwork::DesiredCount(100,8,80,0,2),0);
    TestEqual(TEXT("Buildings required"),FAmbientStreetNetwork::DesiredCount(100,8,80,10,0),0);
    TestEqual(TEXT("Population scales upward"),FAmbientStreetNetwork::DesiredCount(65,8,80,10,2),9);
    TestEqual(TEXT("Population scales down"),FAmbientStreetNetwork::DesiredCount(8,8,80,10,2),1);
    TestEqual(TEXT("Density cap prevents tiny-road crowds"),FAmbientStreetNetwork::DesiredCount(MAX_int32,1,80,1,2),2);
    TestEqual(TEXT("Maximum remains bounded without overflow"),FAmbientStreetNetwork::DesiredCount(MAX_int32,1,80,MAX_int32,2),80);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPeopleRuntimeTest,"Hansa.World.People.Runtime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPeopleRuntimeTest::RunTest(const FString&)
{
    FString Error; const auto Profile=FHansaAmbientPersonProfile::Laborer();
    if (!TestTrue(TEXT("Character contract"),AHansaAmbientPeople::ValidateProfile(Profile,Error))) { AddError(Error); return false; }
    auto Invalid=Profile; Invalid.WalkSpeed=0;
    TestFalse(TEXT("Zero gait rejected"),AHansaAmbientPeople::ValidateProfile(Invalid,Error));
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation=World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Terrain=World->SpawnActor<AStaticMeshActor>();
    auto* Floor=Terrain->GetStaticMeshComponent(); Floor->SetMobility(EComponentMobility::Movable);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetCollisionProfileName(TEXT("BlockAll")); Terrain->Tags.Add(TEXT("Hansa.Terrain"));
    Terrain->SetActorScale3D(FVector(240,160,1)); Terrain->SetActorLocation(FVector(0,0,200));
    auto* Manager=World->SpawnActor<AHansaAmbientPeople>(); Manager->Foundation=Foundation;
    Manager->Host=NewObject<UHansaRuntimeSimulationHost>(Manager);
    if (!TestTrue(TEXT("Host ready"),Manager->Host->InitializeForLubeck(World,Error))) { AddError(Error); return false; }
    const auto Fingerprint=Manager->Host->BuildProjection().Value.GetFingerprint().Value;
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    bool Walked=false,Visited=false,Visible=false;
    FVector First=FVector::ZeroVector;
    for (int32 Frame=0;Frame<1500;++Frame)
    {
        Manager->Tick(.1f);
        for (const auto& P:Manager->QueryPeople())
        {
            if (!P.bVisible) continue;
            if (!Visible) First=P.Location;
            Visible=true; Walked|=P.Activity==EHansaPersonActivity::Walking && FVector::Dist2D(P.Location,First)>100;
            Visited|=P.Activity==EHansaPersonActivity::Visiting;
            TestTrue(TEXT("Grounded adult"),P.Location.Z>249 && P.Location.Z<275);
        }
    }
    TestTrue(TEXT("Residents produce visible pedestrians"),Visible);
    TestTrue(TEXT("Pedestrians walk"),Walked);
    TestTrue(TEXT("Pedestrians visit buildings"),Visited && Manager->CompletedVisits>0);
    TestTrue(TEXT("No profile failures"),Manager->ValidationError.IsEmpty());
    TestEqual(TEXT("Cosmetic people cannot change economy"),Manager->Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    const auto Before=Manager->QueryPeople(); Manager->Tick(.1f); const auto After=Manager->QueryPeople();
    for (int32 I=0;I<Before.Num();++I) TestEqual(TEXT("Pause holds location"),Before[I].Location,After[I].Location);
    Manager->MaximumPeople=0; Manager->Tick(1.f);
    for (const auto& P:Manager->QueryPeople()) TestFalse(TEXT("Population cap removes visible people"),P.bVisible);
    TArray<USkeletalMeshComponent*> Components; Manager->GetComponents(Components);
    for (auto* Component:Components)
    {
        TestTrue(TEXT("Unit actor and component scale"),Component->GetComponentScale().Equals(FVector::OneVector));
        const double Head=Component->GetBoneLocation(TEXT("head")).Z-Component->GetComponentLocation().Z;
        TestTrue(TEXT("Evaluated skeleton stays adult sized"),Head>100 && Head<200);
    }
    TArray<uint8> Save;
    TestTrue(TEXT("Capture existing city before cosmetic rebuild"),Manager->Host->CaptureSaveBytes(Save,TEXT("Ambient people test"),TEXT("2026-09-23T17:00:00Z")).IsSuccess());
    TestTrue(TEXT("New game resets authoritative population"),Manager->Host->StartNewGame(Error));
    Manager->MaximumPeople=80; Manager->Tick(1.f);
    TestEqual(TEXT("Empty new city spawns nobody"),Manager->TargetPopulation,0);
    for (const auto& P:Manager->QueryPeople()) TestFalse(TEXT("Old crowd hidden after new game"),P.bVisible);
    // Saves require the matching scenario registry; StartNewGame selects the empty opening.
    Manager->Host=NewObject<UHansaRuntimeSimulationHost>(Manager);
    TestTrue(TEXT("Initialize matching saved scenario"),Manager->Host->InitializeForLubeck(World,Error));
    const auto Restored=Manager->Host->RestoreSaveBytes(Save);
    TestTrue(FString::Printf(TEXT("Restore existing city: %s"),*Restored.Message),Restored.IsSuccess());
    Manager->Tick(1.f);
    TestTrue(TEXT("Restored population reconstructs the crowd"),Manager->TargetPopulation>0);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPeopleAnimationTest,"Hansa.World.People.AnimationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPeopleAnimationTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Actor=World->SpawnActor<AActor>();
    auto* Component=NewObject<USkeletalMeshComponent>(Actor);
    const auto Profile=FHansaAmbientPersonProfile::Laborer();
    auto* Mesh=Profile.Mesh.LoadSynchronous();
    if (!TestNotNull(TEXT("Imported character"),Mesh)) return false;
    TestEqual(TEXT("Supplied Unreal bone hierarchy"),Mesh->GetRefSkeleton().GetNum(),61);
    TestEqual(TEXT("Three native mesh LODs"),Mesh->GetLODNum(),3);
    Component->SetSkeletalMeshAsset(Mesh); Component->RegisterComponent();
    Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    for (const TCHAR* Name:{TEXT("Walk"),TEXT("Idle"),TEXT("LookAround"),TEXT("Dig"),TEXT("Shovel"),TEXT("StandingRelax"),TEXT("WaveGoodbye")})
    {
        const FString Path=FString::Printf(TEXT("/Game/Hansa/Characters/Laborer01/A_Laborer01_%s.A_Laborer01_%s"),Name,Name);
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*Path);
        if (!TestNotNull(Name,Clip)) continue;
        TestTrue(TEXT("Shared character skeleton"),Clip->GetSkeleton()==Mesh->GetSkeleton());
        if (FString(Name)!=TEXT("Walk")) continue;
        Component->SetAnimation(Clip);
        FVector FirstPelvis,LastPelvis;
        double MaximumDrift=0;
        for (int32 Frame=0;Frame<=56;++Frame)
        {
            Component->SetPosition(Clip->GetPlayLength()*Frame/56.f,false);
            Component->TickAnimation(0,false); Component->RefreshBoneTransforms(); Component->UpdateComponentToWorld();
            const FVector Pelvis=Component->GetBoneLocation(TEXT("pelvis"));
            if (Frame==0) FirstPelvis=Pelvis;
            LastPelvis=Pelvis;
            MaximumDrift=FMath::Max(MaximumDrift,FVector::Dist2D(Pelvis,FirstPelvis));
            TestTrue(TEXT("Adult pelvis remains correctly scaled"),Pelvis.Z>70 && Pelvis.Z<110);
        }
        TestTrue(FString::Printf(TEXT("In-place pelvis drift is bounded: %.2fcm"),MaximumDrift),MaximumDrift<25);
        TestTrue(TEXT("Walk loop closes without teleporting"),FVector::Dist(FirstPelvis,LastPelvis)<.5);
    }
    return !HasAnyErrors();
}
#endif
