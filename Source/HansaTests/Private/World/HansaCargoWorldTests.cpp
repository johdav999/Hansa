#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaBakeryPresentation.h"
#include "World/HansaSawmillPresentation.h"
#include "World/HansaHarborPresentation.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Model/HansaSimulationState.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Systems/HansaSimulationPipeline.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Save/HansaSaveEnvelope.h"

namespace Hansa::Tests::LocalLogistics
{
    Hansa::Simulation::FHansaSimulationInitialization MakeInitialization(bool,bool,bool,TArray<Hansa::Simulation::FHansaLogisticsRequestInitialization>);
    Hansa::Simulation::FHansaSimulationDefinitionContext MakeDefinitions();
    Hansa::Simulation::FHansaLogisticsRequestInitialization Request(uint64,uint64,uint64,int64,Hansa::Simulation::EHansaLogisticsPriority);
    Hansa::Simulation::FHansaCommandGatewayResult RemoveBuilding(Hansa::Simulation::FHansaSimulationState&,const Hansa::Simulation::FHansaSimulationDefinitionContext&,Hansa::Simulation::FHansaSimulationTransientCache&,uint64);
    Hansa::Simulation::FHansaCommandGatewayResult PlaceRoad(Hansa::Simulation::FHansaSimulationState&,const Hansa::Simulation::FHansaSimulationDefinitionContext&,Hansa::Simulation::FHansaSimulationTransientCache&,uint64,int32,int32);
    bool Step(Hansa::Simulation::FHansaSimulationState&,const Hansa::Simulation::FHansaSimulationDefinitionContext&,Hansa::Simulation::FHansaSimulationTransientCache&);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoWorldRoute,"Hansa.World.CargoProjection.RouteLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoWorldRoute::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    auto* F=W->SpawnActor<AHansaLubeckWorldFoundation>();W->SpawnActor<AHansaPlacementProjectionManager>();auto* M=W->SpawnActor<AHansaCargoProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;if(!TestTrue(TEXT("Runtime"),H->InitializeForLubeck(W,Error)))return false;
    H->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    const FName InitialKey(TEXT("World.Cargo.Vehicle.1.0"));
    if(const auto* Initial=M->FindObservation(InitialKey))
    {
        const FVector Port=Initial->Location;
        auto* Decoration=W->SpawnActor<AHansaHarborPresentation>();H->SynchronizeWorldProjection();
        TestTrue(TEXT("Decorative harbor cannot relocate a simulated city route"),M->FindObservation(InitialKey)->Location.Equals(Port));
        Decoration->Destroy();
    }
    auto* Model=NewObject<UHansaTradeMapPresentationModel>();Model->InitializeDefaults();Model->BindRuntime(H.Get());Model->ApplyProjection(H->BuildProjection().Value,*H->GetEconomicRegistry());Model->Open();
    auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model);
    Screen->ActivateSemanticId(TEXT("TradeMap.New"));Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Cog"));Model->SelectStopIntent(0);Screen->ActivateSemanticId(TEXT("TradeMap.Editor.Reserve.Decrease"));Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Review"));
    if(!TestTrue(TEXT("Ordinary route activation"),Screen->ActivateSemanticId(TEXT("TradeMap.Creator.Activate"))))return false;
    const uint64 RouteValue=Model->GetSnapshot().SelectedRouteValue;
    bool Departed=false,Traveling=false,Arriving=false,Unloaded=false,Paused=false,Restored=false;FName Key;
    for(int I=0;I<50 && !Unloaded;++I)
    {
        const auto P=H->BuildProjection();const auto Hash=P.Value.GetFingerprint().Value;
        const auto* R=P.Value.GetRoutes().FindByPredicate([&](const auto& V){return V.Id.GetValue()==RouteValue;});if(!R)return false;
        for(const auto& O:M->QueryCargo())if(O.RouteId.StartsWith(FString::Printf(TEXT("%llu."),static_cast<unsigned long long>(RouteValue))))
        {
            Key=O.SemanticId;
            TestTrue(TEXT("Exact route progress including arrival"),FMath::IsNearlyEqual(O.Progress,double(R->Progress.GetPartsPerMillion())/FHansaRate::Scale,1e-6));
            const auto* V=P.Value.GetVehicles().FindByPredicate([&](const auto& X){return X.Id==R->VehicleId;});
            TestTrue(TEXT("Observation is the exact vehicle cargo"),V&&V->Cargo.GetRawValue()==O.CargoMilliUnits);
            TestEqual(TEXT("Observation is current simulation tick"),O.SimulationTick,H->GetSimulationTick());
            TestTrue(*FString::Printf(TEXT("Verified vessel available: %s"),*O.PresentationFailure),O.PresentationFailure.IsEmpty());
            auto* Actor=M->FindActor(Key);if(!TestNotNull(TEXT("One native cargo actor"),Actor))return false;
            TestEqual(TEXT("Occupied hold uses actual cargo"),Actor->Cargo->IsVisible(),O.CargoMilliUnits>0);
            Departed|=O.Phase==EHansaCargoWorldPhase::Departing;Traveling|=O.Phase==EHansaCargoWorldPhase::Traveling;
            if(O.Phase==EHansaCargoWorldPhase::Arriving)
            {
                Arriving=true;TestFalse(TEXT("Unloaded city hides cargo and selection"),O.bVisible);
                M->SetRostockVisible(true);TestTrue(TEXT("Streaming reconstructs same actor"),M->FindActor(Key)==Actor);
                TestTrue(TEXT("Stable semantic selection"),M->SelectCargo(Key));M->ClearSelection();TestTrue(TEXT("Selecting another world target clears cargo focus"),M->GetSelectedCargo().IsNone());M->SetRostockVisible(false);
            }
            if(O.Phase==EHansaCargoWorldPhase::Traveling && !Paused)
            {
                const auto Before=Actor->GetActorLocation();H->AdvanceRealTime(.5);M->Sample(H->GetPresentationTickFraction());
                TestTrue(TEXT("Pause freezes position"),Actor->GetActorLocation().Equals(Before,.001));
                H->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);H->AdvanceRealTime(.25);M->Sample(H->GetPresentationTickFraction());
                TestFalse(TEXT("Fractional simulation time moves vessel"),Actor->GetActorLocation().Equals(Before,.001));
                TestEqual(TEXT("Fractional rendering leaves state unchanged"),H->BuildProjection().Value.GetFingerprint().Value,Hash);
                H->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Paused=true;
                TArray<uint8> Bytes;TestTrue(TEXT("Save underway"),!!H->CaptureSaveBytes(Bytes,TEXT("P32"),TEXT("2026-09-09T00:00:00Z")));
                H->AdvanceTicks(2);TestTrue(TEXT("Restore underway"),!!H->RestoreSaveBytes(Bytes));
                TestEqual(TEXT("Save restores authority"),H->BuildProjection().Value.GetFingerprint().Value,Hash);
                TestNotNull(TEXT("Save reconstructs stable actor"),M->FindActor(Key));Restored=true;
            }
            if(R->LastTransfer.Kind==EHansaRouteCargoActionKind::Unload && R->LastTransfer.CityId.ToString()==TEXT("City.Rostock") && R->LastTransfer.AppliedQuantity.GetRawValue()>0)
            {
                Unloaded=true;TestEqual(TEXT("Visible receipt matches actual transfer"),O.TransferMilliUnits,R->LastTransfer.AppliedQuantity.GetRawValue());
                TestEqual(TEXT("Receipt tick matches inventory event"),O.TransferTick,R->LastTransfer.Tick.GetValue());
            }
        }
        if(!Unloaded)TestTrue(TEXT("Tick"),H->AdvanceTicks(1));
    }
    TestTrue(TEXT("Departing, traveling, arriving and unloading observed"),Departed&&Traveling&&Arriving&&Unloaded&&Paused&&Restored);
    const auto* Observation=M->FindObservation(Key);
    if(Observation)
    {
        auto* Inspector=NewObject<UHansaInspectorPresentationModel>();Inspector->InitializeDefaults();Inspector->ShowCargo(*Observation,TEXT("TradeMap.Root"));
        TestTrue(TEXT("Cargo inspector kind"),Inspector->GetSnapshot().Kind==EHansaInspectorObjectKind::Cargo);
        auto Hidden=*Observation;Hidden.bVisible=false;Inspector->ShowCargo(Hidden,TEXT("TradeMap.Root"));
        TestFalse(TEXT("Offscreen frame cannot claim success"),Inspector->FrameIntent());
    }
    auto* Definition=Cast<UHansaVehicleDefinition>(UHansaDefinitionBase::ResolveByStableId(TEXT("Vehicle.Cog")));
    if(Definition)
    {
        const auto Original=Definition->PresentationActorClass;Definition->PresentationActorClass.Reset();
        auto* Missing=W->SpawnActor<AHansaCargoProjectionManager>();Missing->Synchronize(H->BuildProjection().Value,*H,*F);
        const auto* Failure=Missing->FindObservation(Key);
        TestTrue(TEXT("Missing verified class is explicit and hidden"),Failure&&!Failure->PresentationFailure.IsEmpty()&&!Failure->bVisible);
        TestNull(TEXT("Missing art never substitutes a primitive"),Missing->FindActor(Key));Definition->PresentationActorClass=Original;Missing->Destroy();
    }
    TestTrue(TEXT("Route cancellation accepted"),!!H->CancelRoute(FHansaRouteId::TryCreate(RouteValue).Value));
    TestNull(TEXT("Cancelled route removes vessel"),M->FindActor(Key));TestFalse(TEXT("Cancelled semantic target rejects selection"),M->SelectCargo(Key));
    // Reconstruction is idempotent and never creates a cosmetic fleet.
    for(int I=0;I<4;++I)H->SynchronizeWorldProjection();
    int Count=0;for(TActorIterator<AHansaCargoVehiclePresentation> It(W);It;++It)++Count;TestTrue(TEXT("Sea fleet bounded"),Count<=8);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoWorldRoad,"Hansa.World.CargoProjection.RealRoadDelivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoWorldRoad::RunTest(const FString&)
{
    using namespace Hansa::Simulation;using namespace Hansa::Tests::LocalLogistics;
    auto Initial=MakeInitialization(false,false,false,{});const auto Definitions=MakeDefinitions();
    auto Created=FHansaSimulationState::TryCreate(MoveTemp(Initial));if(!Created)return false;
    auto State=MoveTemp(Created.Value);FHansaSimulationTransientCache Cache;
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    auto* F=W->SpawnActor<AHansaLubeckWorldFoundation>();auto* M=W->SpawnActor<AHansaCargoProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());FString Error;H->InitializeForLubeck(nullptr,Error);
    bool Pickup=false,Transit=false,Delivered=false;FName Key;FVector Start;
    for(int I=0;I<16;++I)
    {
        TestTrue(TEXT("Real logistics pipeline"),Step(State,Definitions,Cache));const auto P=State.CreateReadOnlyAccess(Definitions).BuildProjection();if(!P)return false;
        const auto Hash=P.Value.GetFingerprint().Value;M->Synchronize(P.Value,*H,*F);
        TestTrue(TEXT("Every active local delivery has capacity"),M->GetActiveLocalWagonCount()<=P.Value.GetLogisticsJobs().Num());
        for(const auto& J:P.Value.GetLogisticsJobs())
        {
            if(J.Id.GetValue()!=1)continue;
            const auto Observations=M->QueryCargo();const auto* O=Observations.FindByPredicate([](const auto& X){return X.JobId.StartsWith(TEXT("1."));});
            if(!O)continue;Key=O->SemanticId;
            TestTrue(TEXT("Completed road path reconstructed"),O->PresentationFailure.IsEmpty());
            TestEqual(TEXT("Exact in-flight cargo"),O->CargoMilliUnits,J.CargoQuantity.GetRawValue());
            TestEqual(TEXT("Exact request identity"),O->RequestId,FString(TEXT("1.0")));
            TestEqual(TEXT("Exact source building"),O->SourceBuildingId,FString(TEXT("1.0")));
            TestEqual(TEXT("Exact destination building"),O->DestinationBuildingId,FString(TEXT("2.0")));
            TestEqual(TEXT("Exact good identity"),O->GoodId,FName(TEXT("Good.Grain")));
            TestEqual(TEXT("Exact requested job quantity"),O->QuantityMilliUnits,J.Quantity.GetRawValue());
            if(J.Status==EHansaLogisticsJobStatus::AwaitingPickup){Pickup=true;Start=O->Location;TestEqual(TEXT("No invented cargo before pickup"),O->CargoMilliUnits,int64(0));}
            if(J.Status==EHansaLogisticsJobStatus::InTransit)
            {
                Transit=true;auto* Actor=M->FindActor(Key);if(!TestNotNull(TEXT("Verified wagon"),Actor))return false;
                TestTrue(TEXT("Wagon carries real job"),Actor->GetLogisticsJobId()==J.Id);
                M->Sample(.5);const FVector At=Actor->GetActorLocation();M->Sample(.5);TestTrue(TEXT("Idempotent pose"),Actor->GetActorLocation().Equals(At));
                TestTrue(TEXT("Cargo cue matches ledger"),Actor->Cargo->IsVisible());
            }
            if(J.Status==EHansaLogisticsJobStatus::Completed)
            {
                Delivered=true;TestEqual(TEXT("Delivered job has completed progress"),O->Progress,1.);
                auto* Actor=M->FindActor(Key);TestNotNull(TEXT("Delivered wagon remains for authoritative receipt tick"),Actor);
                if(Actor)TestFalse(TEXT("Delivered wagon is visibly empty"),Actor->Cargo->IsVisible());
                TestEqual(TEXT("Delivery receipt"),O->TransferMilliUnits,J.Quantity.GetRawValue());
                auto* Inspector=NewObject<UHansaInspectorPresentationModel>();Inspector->InitializeDefaults();Inspector->ShowCargo(*O,TEXT("World.Root"));
                TestTrue(TEXT("Local wagon inspector names its good"),Inspector->GetSnapshot().Identity.ToString().Contains(TEXT("Grain")));
                TestTrue(TEXT("Local wagon inspector exposes route and endpoints"),Inspector->GetSnapshot().Flows.Num()>=3);
            }
            TArray<FIntPoint> Path;auto Invalid=J;Invalid.RoadDistanceCells+=100;TestFalse(TEXT("Changed dispatch path fails explicitly"),AHansaCargoProjectionManager::BuildRoadPath(P.Value,Invalid,Path));
        }
        TestEqual(TEXT("Presentation cannot mutate inventory"),State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value,Hash);
    }
    TestTrue(TEXT("Actual pickup, travel and delivery observed"),Pickup&&Transit&&Delivered);return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoWorldConcurrentWagons,"Hansa.Integration.CargoProjection.AllConcurrentLocalDeliveriesVisible",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoWorldConcurrentWagons::RunTest(const FString&)
{
    using namespace Hansa::Simulation;using namespace Hansa::Tests::LocalLogistics;
    TArray<FHansaLogisticsRequestInitialization> Requests;
    for(uint64 Id=1;Id<=6;++Id)Requests.Add(Request(Id,1,2,40,EHansaLogisticsPriority::Normal));
    auto Initial=MakeInitialization(false,false,false,MoveTemp(Requests));
    Initial.LocalLogisticsSettings.MaximumConcurrentJobs=8;
    const auto Definitions=MakeDefinitions();
    auto Created=FHansaSimulationState::TryCreate(MoveTemp(Initial));if(!Created)return false;
    auto State=MoveTemp(Created.Value);FHansaSimulationTransientCache Cache;
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    auto* Foundation=W->SpawnActor<AHansaLubeckWorldFoundation>();auto* Manager=W->SpawnActor<AHansaCargoProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;Host->InitializeForLubeck(nullptr,Error);
    TestTrue(TEXT("Six jobs dispatch through the real logistics pipeline"),Step(State,Definitions,Cache));
    auto Projection=State.CreateReadOnlyAccess(Definitions).BuildProjection();if(!Projection)return false;
    TestEqual(TEXT("Six authoritative jobs exist"),Projection.Value.GetLogisticsJobs().Num(),6);
    const uint64 Fingerprint=Projection.Value.GetFingerprint().Value;
    Manager->Synchronize(Projection.Value,*Host,*Foundation);
    TestEqual(TEXT("Every authoritative job has one active wagon"),Manager->GetActiveLocalWagonCount(),6);
    TSet<AHansaCargoVehiclePresentation*> Unique;
    TSet<FIntPoint> DistinctPositions;
    for(const FHansaLogisticsJobProjection& Job:Projection.Value.GetLogisticsJobs())
    {
        const FName Key(*FString::Printf(TEXT("World.Cargo.Delivery.%llu.%u"),
            static_cast<unsigned long long>(Job.Id.GetValue()),Job.Id.GetGeneration()));
        const FHansaCargoWorldObservation* Observation=Manager->FindObservation(Key);
        if(!TestNotNull(TEXT("Each job has an observation"),Observation))continue;
        TestTrue(TEXT("Every job presentation succeeds"),Observation->PresentationFailure.IsEmpty());
        AHansaCargoVehiclePresentation* Actor=Manager->FindActor(Key);
        if(TestNotNull(TEXT("Each job has a distinct approved wagon actor"),Actor))
        {
            Unique.Add(Actor);
            const FVector Location=Actor->GetActorLocation();
            DistinctPositions.Add(FIntPoint(FMath::RoundToInt(Location.X),FMath::RoundToInt(Location.Y)));
        }
    }
    TestEqual(TEXT("Six distinct wagons are rendered"),Unique.Num(),6);
    TestEqual(TEXT("Coincident jobs receive six deterministic visible positions"),DistinctPositions.Num(),6);
    const double ProfileStart=FPlatformTime::Seconds();
    constexpr int32 SynchronizationSamples=250;
    for(int32 Sample=0;Sample<SynchronizationSamples;++Sample)Manager->Synchronize(Projection.Value,*Host,*Foundation);
    const double AverageMilliseconds=(FPlatformTime::Seconds()-ProfileStart)*1000.0/SynchronizationSamples;
    TestEqual(TEXT("Repeated synchronization is idempotent"),Manager->GetActiveLocalWagonCount(),6);
    TestTrue(TEXT("Six-wagon synchronization stays within the representative-city budget"),AverageMilliseconds<5.0);
    TestEqual(TEXT("Presentation never changes authoritative state"),
        State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value,Fingerprint);
    const FString ProfileDirectory=FPaths::ProjectSavedDir()/TEXT("Profiling");
    IFileManager::Get().MakeDirectory(*ProfileDirectory,true);
    const FString Profile=FString::Printf(
        TEXT("{\n  \"scenario\": \"six-concurrent-local-deliveries\",\n  \"activeAuthoritativeJobs\": 6,\n  \"visibleWagons\": %d,\n  \"poolSize\": %d,\n  \"peakVisibleWagons\": %d,\n  \"synchronizationSamples\": %d,\n  \"averageSynchronizationMilliseconds\": %.6f,\n  \"gameThreadBudgetMilliseconds\": 5.0,\n  \"authorityFingerprint\": \"%llu\"\n}\n"),
        Manager->GetActiveLocalWagonCount(),Manager->GetPooledLocalWagonCount(),Manager->GetPeakLocalWagonCount(),
        SynchronizationSamples,AverageMilliseconds,static_cast<unsigned long long>(Fingerprint));
    TestTrue(TEXT("Representative wagon profile evidence written"),FFileHelper::SaveStringToFile(
        Profile,*(ProfileDirectory/TEXT("visible-local-wagons.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    for(int32 Tick=0;Tick<24;++Tick)
    {
        TestTrue(TEXT("Concurrent deliveries advance"),Step(State,Definitions,Cache));
        Projection=State.CreateReadOnlyAccess(Definitions).BuildProjection();if(!Projection)return false;
        Manager->Synchronize(Projection.Value,*Host,*Foundation);
    }
    TestEqual(TEXT("Completed wagons leave the active map"),Manager->GetActiveLocalWagonCount(),0);
    TestEqual(TEXT("Six approved wagon actors are retained for reuse"),Manager->GetPooledLocalWagonCount(),6);
    TestEqual(TEXT("Peak count records all simultaneous deliveries"),Manager->GetPeakLocalWagonCount(),6);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoWorldPausedWagon,"Hansa.Integration.CargoProjection.RoadBreakAndReconnect",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoWorldPausedWagon::RunTest(const FString&)
{
    using namespace Hansa::Simulation;using namespace Hansa::Tests::LocalLogistics;
    auto Created=FHansaSimulationState::TryCreate(MakeInitialization(false,false,false,{
        Request(1,1,2,100,EHansaLogisticsPriority::Normal)}));if(!Created)return false;
    auto State=MoveTemp(Created.Value);const auto Definitions=MakeDefinitions();FHansaSimulationTransientCache Cache;
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    auto* Foundation=W->SpawnActor<AHansaLubeckWorldFoundation>();auto* Manager=W->SpawnActor<AHansaCargoProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;Host->InitializeForLubeck(nullptr,Error);
    TestTrue(TEXT("Dispatch"),Step(State,Definitions,Cache));TestTrue(TEXT("Pickup"),Step(State,Definitions,Cache));
    const FName Key(TEXT("World.Cargo.Delivery.1.0"));
    TestTrue(TEXT("Active route road removal succeeds"),RemoveBuilding(State,Definitions,Cache,12).IsSuccess());
    auto Projection=State.CreateReadOnlyAccess(Definitions).BuildProjection();if(!Projection)return false;
    Manager->Synchronize(Projection.Value,*Host,*Foundation);
    const FHansaCargoWorldObservation* Paused=Manager->FindObservation(Key);
    if(!TestNotNull(TEXT("Paused wagon observation"),Paused))return false;
    TestEqual(TEXT("Loaded road break has a distinct visual phase"),Paused->Phase,EHansaCargoWorldPhase::DeliveryPaused);
    TestFalse(TEXT("Road blocker is typed"),Paused->PauseReason.IsNone());
    auto* Actor=Manager->FindActor(Key);if(!TestNotNull(TEXT("Loaded wagon remains visible while blocked"),Actor))return false;
    TestTrue(TEXT("Blocked wagon preserves its real cargo cue"),Actor->Cargo->IsVisible()&&Paused->CargoMilliUnits==100);
    Manager->Sample(.75);const FVector Frozen=Actor->GetActorLocation();Manager->Sample(.25);
    TestTrue(TEXT("Paused fractional sampling cannot move the wagon"),Actor->GetActorLocation().Equals(Frozen,.001));
    TestTrue(TEXT("Replacement road placement"),PlaceRoad(State,Definitions,Cache,50,2,0).IsSuccess());
    TestTrue(TEXT("Replacement construction"),Step(State,Definitions,Cache));TestTrue(TEXT("Reconnect"),Step(State,Definitions,Cache));
    Projection=State.CreateReadOnlyAccess(Definitions).BuildProjection();Manager->Synchronize(Projection.Value,*Host,*Foundation);
    const FHansaCargoWorldObservation* Resumed=Manager->FindObservation(Key);
    TestTrue(TEXT("Same wagon identity resumes in transit"),Resumed&&Resumed->Phase==EHansaCargoWorldPhase::Traveling&&Manager->FindActor(Key)==Actor);
    TestEqual(TEXT("Reconnect does not duplicate the wagon"),Manager->GetActiveLocalWagonCount(),1);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoWorldLocalSave,"Hansa.Integration.CargoProjection.LocalDeliverySaveLoad",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoWorldLocalSave::RunTest(const FString&)
{
    using namespace Hansa::Simulation;using namespace Hansa::Tests::LocalLogistics;
    auto Created=FHansaSimulationState::TryCreate(MakeInitialization(false,false,false,{
        Request(1,1,2,100,EHansaLogisticsPriority::Normal)}));if(!Created)return false;
    auto State=MoveTemp(Created.Value);const auto Definitions=MakeDefinitions();FHansaSimulationTransientCache Cache;
    TestTrue(TEXT("Dispatch"),Step(State,Definitions,Cache));TestTrue(TEXT("Pickup"),Step(State,Definitions,Cache));TestTrue(TEXT("Travel"),Step(State,Definitions,Cache));
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    auto* Foundation=W->SpawnActor<AHansaLubeckWorldFoundation>();auto* Manager=W->SpawnActor<AHansaCargoProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;Host->InitializeForLubeck(nullptr,Error);
    auto Projection=State.CreateReadOnlyAccess(Definitions).BuildProjection();if(!Projection)return false;
    Manager->Synchronize(Projection.Value,*Host,*Foundation);
    const FName Key(TEXT("World.Cargo.Delivery.1.0"));const auto* Before=Manager->FindObservation(Key);
    if(!TestNotNull(TEXT("In-transit wagon before save"),Before))return false;
    const FHansaCargoWorldObservation Expected=*Before;TestTrue(TEXT("Select stable local wagon"),Manager->SelectCargo(Key));
    FHansaSaveSnapshot Snapshot;Snapshot.State=State;Snapshot.BuildVersion=TEXT("VisibleWagon");
    Snapshot.SavedUtc=TEXT("2026-09-11T00:00:00Z");Snapshot.DisplayName=TEXT("Visible local wagon");
    Snapshot.Players={{7,FHansaHouseId::TryCreate(1).Value}};
    TArray<uint8> Bytes;const FHansaSaveResult Encoded=FHansaSaveEnvelope::Encode(Snapshot,Definitions,Bytes);
    if(!TestTrue(*Encoded.Message,Encoded.IsSuccess()))return false;
    FHansaSaveSnapshot Loaded;const FHansaSaveResult Decoded=FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded);
    if(!TestTrue(*Decoded.Message,Decoded.IsSuccess()))return false;
    Projection=Loaded.State.CreateReadOnlyAccess(Definitions).BuildProjection();if(!Projection)return false;
    Manager->Synchronize(Projection.Value,*Host,*Foundation);
    const auto* Restored=Manager->FindObservation(Key);
    if(!TestNotNull(TEXT("Save/load reconstructs the same wagon identity"),Restored))return false;
    TestEqual(TEXT("Save/load preserves cargo"),Restored->CargoMilliUnits,Expected.CargoMilliUnits);
    TestEqual(TEXT("Save/load preserves good"),Restored->GoodId,Expected.GoodId);
    TestEqual(TEXT("Save/load preserves progress"),Restored->Progress,Expected.Progress);
    TestEqual(TEXT("Save/load preserves source"),Restored->SourceBuildingId,Expected.SourceBuildingId);
    TestEqual(TEXT("Save/load preserves destination"),Restored->DestinationBuildingId,Expected.DestinationBuildingId);
    TestNotNull(TEXT("Save/load reconstructs approved wagon actor"),Manager->FindActor(Key));
    TestEqual(TEXT("Stable selected identity survives reconstruction"),Manager->GetSelectedCargo(),Key);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoClock,"Hansa.World.CargoProjection.SpeedClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoClock::RunTest(const FString&)
{
    TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());FString Error;if(!H->InitializeForLubeck(nullptr,Error))return false;
    const int64 Initial=H->GetSimulationTick();
    H->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);H->AdvanceRealTime(.25);TestEqual(TEXT("Normal fractional tick"),H->GetPresentationTickFraction(),.25);
    H->SetSpeed(EHansaRuntimeSimulationSpeed::Fast);H->AdvanceRealTime(.1875);TestEqual(TEXT("4x tick cadence"),H->GetSimulationTick(),Initial+1);
    H->SetSpeed(EHansaRuntimeSimulationSpeed::Fastest);H->AdvanceRealTime(1./24.);TestEqual(TEXT("12x fractional tick"),H->GetPresentationTickFraction(),.5);
    H->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);H->AdvanceRealTime(.9);TestEqual(TEXT("Pause retains phase"),H->GetPresentationTickFraction(),.5);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoProduction,"Hansa.World.CargoProjection.ProductionRoles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoProduction::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    ON_SCOPE_EXIT{W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
    W->SpawnActor<AHansaLubeckWorldFoundation>();auto* Manager=W->SpawnActor<AHansaPlacementProjectionManager>();
    TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());FString Error;if(!H->InitializeForLubeck(W,Error))return false;
    const auto P=H->BuildProjection();int Found=0;
    for(const auto& Production:P.Value.GetProductions())if(auto* A=Manager->FindProjectionActor(Production.BuildingId))
    {
        ++Found;const auto O=A->QueryProduction();TestTrue(TEXT("Production typed query available"),O.bAvailable);
        TestEqual(TEXT("Real cycle progress"),O.ProgressTicks,Production.ProgressTicks);
        auto Stopped=Production;Stopped.bActive=false;A->ApplyProduction(&Stopped,P.Value.GetInventories());TestFalse(TEXT("Paused production stops work cue"),A->QueryProduction().bWorking);
        if(auto* B=Cast<AHansaBakeryPresentation>(A->BuildingPresentation->GetChildActor()))
        {
            TArray<FHansaInventoryProjection> Inventories;Inventories.Append(P.Value.GetInventories());for(auto& I:Inventories)for(auto& S:I.Stocks)S.Stock={};
            A->ApplyProduction(&Production,Inventories);TestFalse(TEXT("Empty flour does not display stock"),B->FlourSack->IsVisible());TestFalse(TEXT("Empty bread does not display stock"),B->BreadCrate->IsVisible());
        }
    }
    TestTrue(TEXT("Real local chains projected"),Found>=3);TestEqual(TEXT("Role tests never mutate authority"),H->BuildProjection().Value.GetFingerprint().Value,P.Value.GetFingerprint().Value);
    return !HasAnyErrors();
}
#endif
