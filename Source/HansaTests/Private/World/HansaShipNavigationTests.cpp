#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Trade/HansaWaterNavigation.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

using namespace Hansa::Simulation;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaWaterPathTest,"Hansa.ShipNavigation.WaterOnlyPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaWaterPathTest::RunTest(const FString&)
{
    FHansaPlacementMapInitialization Map;Map.BoundsMin={0,0};Map.BoundsMax={99,69};
    for(int32 X=0;X<100;++X)for(int32 Y=0;Y<70;++Y)
    {
        const bool Land=(X>=40 && X<=60 && Y>=18 && Y<=52) || X==90;
        Map.Cells.Add({{X,Y},Land?EHansaPlacementTerrain::Land:EHansaPlacementTerrain::Water,{},false});
    }
    TArray<FHansaGridCoordinate> A,B;
    TestTrue(TEXT("Route bends around island"),FHansaWaterNavigation::FindPath(Map,{10,35},{80,35},A));
    TestTrue(TEXT("Path repeats deterministically"),FHansaWaterNavigation::FindPath(Map,{10,35},{80,35},B)&&A==B);
    TestTrue(TEXT("Island detour is longer than straight crossing"),A.Num()>70);
    FHansaGridCoordinate Last{10,35};
    for(auto C:A){TestTrue(TEXT("Every step keeps beam clear"),FHansaWaterNavigation::IsNavigable(Map,C));TestEqual(TEXT("No corner cutting"),FMath::Abs(C.X-Last.X)+FMath::Abs(C.Y-Last.Y),1);Last=C;}
    TestFalse(TEXT("Land rejected"),FHansaWaterNavigation::FindPath(Map,{10,35},{50,35},B));
    TestFalse(TEXT("Outside map rejected"),FHansaWaterNavigation::FindPath(Map,{10,35},{100,35},B));
    TestFalse(TEXT("Disconnected lake rejected"),FHansaWaterNavigation::FindPath(Map,{10,35},{96,35},B));
    TestFalse(TEXT("Too close to shoreline rejected"),FHansaWaterNavigation::IsNavigable(Map,{39,35}));
    TestTrue(TEXT("Stop at current location is valid"),FHansaWaterNavigation::FindPath(Map,{10,35},{10,35},B)&&B.IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaShipRuntimeTest,"Hansa.ShipNavigation.StartMoveSaveReturn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaShipRuntimeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("ShipNavigation")),CreatePackage(TEXT("/Temp/Lubeck_Terrain_Preview_ShipNavigation")));
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>()),Loaded(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if(!TestTrue(TEXT("New surveyed game initializes"),Host->InitializeForLubeck(World,Error))){AddError(Error);return false;}
    if(!TestTrue(TEXT("Restore host initializes"),Loaded->InitializeForLubeck(World,Error)))return false;
    Host->SetMerchantAIEnabled(false);Loaded->SetMerchantAIEnabled(false);
    const auto P=Host->BuildProjection();
    const auto* V=P.Value.GetVehicles().FindByPredicate([&](const auto& Ship){return Ship.OwnerId==Host->GetHouseId()&&Ship.Mode==EHansaRouteMode::Sea;});
    if(!TestNotNull(TEXT("Player owns starting Cog"),V))return false;
    const auto Id=V->Id;const auto Home=V->Navigation.Home;const auto* Map=Host->FindPlacementMap();
    TestTrue(TEXT("Starts at safe waterfront"),V->Navigation.CityId.IsValid()&&FHansaWaterNavigation::IsNavigable(*Map,Home));
    const auto Start=Hansa::Game::LubeckPlacementGrid::SurveyStartLocation();
    TestTrue(TEXT("Ship near initial camera"),FVector::Dist2D(Start,Hansa::Game::LubeckPlacementGrid::GridToWorld(Home))<15000);
    TArray<FHansaGridCoordinate> Distant;
    for(int32 I=0;I<Map->Cells.Num();I+=37) {
        const auto& C=Map->Cells[I];
        if(C.Terrain==EHansaPlacementTerrain::Water && FMath::Abs(C.Coordinate.X-Home.X)+FMath::Abs(C.Coordinate.Y-Home.Y)>250 && FHansaWaterNavigation::IsNavigable(*Map,C.Coordinate)) Distant.Add(C.Coordinate);
    }
    Distant.Sort([Home](auto A,auto B){return FMath::Abs(A.X-Home.X)+FMath::Abs(A.Y-Home.Y)>FMath::Abs(B.X-Home.X)+FMath::Abs(B.Y-Home.Y);});
    bool FarReachable=false;TArray<FHansaGridCoordinate> FarPath;
    for(auto C:Distant)if(FHansaWaterNavigation::FindPath(*Map,Home,C,FarPath)){FarReachable=true;break;}
    TestTrue(TEXT("Explore over a kilometre beyond starting camera and prototype bounds"),FarReachable);
    TArray<FHansaGridCoordinate> Path;FHansaGridCoordinate Target=Home;bool Found=false;
    for(int32 X=-20;X<=20&&!Found;X+=10)for(int32 Y=-20;Y<=20&&!Found;Y+=10)
    {if(FMath::Abs(X)+FMath::Abs(Y)<20)continue;Target={Home.X+X,Home.Y+Y};Found=FHansaWaterNavigation::FindPath(*Map,Home,Target,Path);}
    if(!TestTrue(TEXT("Connected water can be explored"),Found))return false;
    TestTrue(TEXT("Normal move command accepted"),Host->MoveShip(Id,Target).IsSuccess());
    const auto Before=Host->BuildProjection().Value.GetFingerprint();
    TestFalse(TEXT("Foreign Cog rejected"),Host->MoveShip(FHansaVehicleId::TryCreate(3).Value,Target).IsSuccess());
    TestFalse(TEXT("Land/outside request rejected"),Host->MoveShip(Id,{Map->BoundsMax.X+1,0}).IsSuccess());
    TestTrue(TEXT("Rejected moves preserve authoritative state"),Host->BuildProjection().Value.GetFingerprint()==Before);
    TestFalse(TEXT("Cannot teleport exploring ship onto route"),Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true).IsSuccess());
    TArray<uint8> Bytes;const auto Saved=Host->CaptureSaveBytes(Bytes,TEXT("Ship at sea"),TEXT("2026-09-17T16:00:00Z"));
    if(!TestTrue(*Saved.Message,Saved.IsSuccess()))return false;
    const auto Restored=Loaded->RestoreSaveBytes(Bytes);if(!TestTrue(*Restored.Message,Restored.IsSuccess()))return false;
    TestEqual(TEXT("In-flight ship restores exact hash"),Saved.AuthoritativeHash,Restored.AuthoritativeHash);
    for(int32 I=0;I<Path.Num()+1;++I)
    {
        if(!Host->AdvanceTicks(1)||!Loaded->AdvanceTicks(1))return false;
        TestTrue(TEXT("Saved course continues deterministically"),Host->BuildProjection().Value.GetFingerprint()==Loaded->BuildProjection().Value.GetFingerprint());
    }
    auto At=Host->BuildProjection().Value.GetVehicles()[0];
    TestTrue(TEXT("Arrives exactly at target"),At.Navigation.Cell==Target&&!At.Navigation.IsMoving());
    TestTrue(TEXT("Return to berth command accepted"),Host->MoveShip(Id,Home).IsSuccess());
    Host->AdvanceTicks(Path.Num()+1);At=Host->BuildProjection().Value.GetVehicles()[0];
    TestTrue(TEXT("Returns to home berth"),At.Navigation.IsAtHome());
    TestTrue(TEXT("Trade route can resume from home"),Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true).IsSuccess());
    TestFalse(TEXT("Active trade and manual orders are mutually exclusive"),Host->MoveShip(Id,Target).IsSuccess());
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaShipOrderContinuityTest,"Hansa.ShipNavigation.OrderContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaShipOrderContinuityTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("ShipOrderContinuity")),CreatePackage(TEXT("/Temp/Lubeck_Terrain_Preview_ShipOrderContinuity")));
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Manager=World->SpawnActor<AHansaCargoProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if(!TestTrue(TEXT("Runtime initializes"),Host->InitializeForLubeck(World,Error)))return false;
    Host->SetMerchantAIEnabled(false);
    const auto Projection=Host->BuildProjection();
    const auto* Vehicle=Projection.Value.GetVehicles().FindByPredicate([&](const auto& V){return V.OwnerId==Host->GetHouseId()&&V.Mode==EHansaRouteMode::Sea;});
    if(!TestNotNull(TEXT("Owned ship"),Vehicle))return false;
    const auto Id=Vehicle->Id;
    const auto Home=Vehicle->Navigation.Home;
    const FName Key(*FString::Printf(TEXT("World.Cargo.Vehicle.%llu.%u"),static_cast<unsigned long long>(Id.GetValue()),Id.GetGeneration()));
    auto* Actor=Manager->FindActor(Key);
    if(!TestNotNull(TEXT("Real ship presentation"),Actor))return false;
    const FVector Start=Actor->GetActorLocation();
    Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    Host->AdvanceRealTime(0.4);
    Manager->Sample(Host->GetPresentationTickFraction());
    const FHansaGridCoordinate East{Home.X+2,Home.Y},West{Home.X-2,Home.Y};
    if(!TestTrue(TEXT("First course accepted"),Host->MoveShip(Id,East).IsSuccess()))return false;
    AddInfo(FString::Printf(TEXT("Order start=%s after=%s observed=%s fraction=%.6f"),*Start.ToString(),*Actor->GetActorLocation().ToString(),*Manager->FindObservation(Key)->Location.ToString(),Host->GetPresentationTickFraction()));
    TestTrue(TEXT("Starting midway through a tick does not jump"),Actor->GetActorLocation().Equals(Start,0.001));
    Manager->Sample(Host->GetPresentationTickFraction());
    TestTrue(TEXT("First frame after order does not jump"),Actor->GetActorLocation().Equals(Start,0.001));
    Host->AdvanceRealTime(0.2);Manager->Sample(Host->GetPresentationTickFraction());
    const FVector Underway=Actor->GetActorLocation();
    TestTrue(TEXT("Ship sails between ticks"),FVector::Dist2D(Start,Underway)>1.);
    Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    if(!TestTrue(TEXT("Reverse course accepted while paused"),Host->MoveShip(Id,West).IsSuccess()))return false;
    AddInfo(FString::Printf(TEXT("Reverse before=%s after=%s fraction=%.6f"),*Underway.ToString(),*Actor->GetActorLocation().ToString(),Host->GetPresentationTickFraction()));
    TestTrue(TEXT("Reversing preserves displayed position"),Actor->GetActorLocation().Equals(Underway,0.001));
    Manager->Sample(Host->GetPresentationTickFraction());
    TestTrue(TEXT("Paused resample preserves position"),Actor->GetActorLocation().Equals(Underway,0.001));
    Host->SynchronizeWorldProjection();
    TestTrue(TEXT("Repeated synchronization preserves rebased path"),Actor->GetActorLocation().Equals(Underway,0.001));
    TestTrue(TEXT("Repeated order accepted"),Host->MoveShip(Id,West).IsSuccess());
    TestTrue(TEXT("Repeated order preserves position"),Actor->GetActorLocation().Equals(Underway,0.001));
    TestFalse(TEXT("Invalid order rejected"),Host->MoveShip(Id,{Host->FindPlacementMap()->BoundsMax.X+1,Home.Y}).IsSuccess());
    TestTrue(TEXT("Rejected order preserves position"),Actor->GetActorLocation().Equals(Underway,0.001));
    Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    Host->AdvanceRealTime(0.2);Manager->Sample(Host->GetPresentationTickFraction());
    const FVector Turning=Actor->GetActorLocation();
    TestTrue(TEXT("New course resumes toward west"),Turning.X<Underway.X);
    const auto MovingProjection=Host->BuildProjection();
    const auto* Moving=MovingProjection.Value.GetVehicles().FindByPredicate([&](const auto& V){return V.Id==Id;});
    const auto NextCell=Moving->Navigation.IsMoving()?Moving->Navigation.Path[Moving->Navigation.NextIndex]:Moving->Navigation.Cell;
    const FVector Next=Hansa::Game::LubeckPlacementGrid::GridToWorld(NextCell,Start.Z);
    Manager->Sample(0.999999);
    TestTrue(TEXT("New course reaches next authoritative cell continuously"),Actor->GetActorLocation().Equals(Next,0.01));
    Host->AdvanceRealTime(0.21);Manager->Sample(Host->GetPresentationTickFraction());
    TestTrue(TEXT("Tick boundary remains continuous"),Actor->GetActorLocation().Equals(Next,0.01));
    Host->AdvanceRealTime(0.3);Manager->Sample(Host->GetPresentationTickFraction());
    const FVector BeforeStop=Actor->GetActorLocation();
    TestTrue(TEXT("Stop accepted"),Host->MoveShip(Id,NextCell).IsSuccess());
    TestTrue(TEXT("Stop does not snap back to grid"),Actor->GetActorLocation().Equals(BeforeStop,0.001));
    Manager->Sample(0.999999);
    TestTrue(TEXT("Stop settles at authoritative cell"),Actor->GetActorLocation().Equals(Next,0.01));
    return !HasAnyErrors();
}
#endif
