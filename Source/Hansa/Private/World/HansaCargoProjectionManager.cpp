#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRostockQuarter.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

using namespace Hansa::Simulation;
namespace
{
    template<typename T> FString Identity(T Id)
    {
        return Id.IsValid() ? FString::Printf(TEXT("%llu.%u"), static_cast<unsigned long long>(Id.GetValue()), Id.GetGeneration()) : FString();
    }
    FName City(const FHansaCityDefinitionId& Id) { return FName(*Id.ToString()); }
    FName LogisticsStatus(const EHansaLogisticsJobStatus Status) { return FName(LexToString(Status)); }
    FName RoadFailure(const EHansaLogisticsRoadPathFailure Failure) { return FName(LexToString(Failure)); }
    FHansaCityDefinitionId InventoryCity(const FHansaSimulationProjection& P, const FHansaInventoryProjection& I)
    {
        if(I.OwnerKind==EHansaInventoryOwnerKind::City)return I.CityId;
        for(const auto& B:P.GetBuildingWorldProjections())if(B.BuildingId==I.BuildingId)return B.Placement.CityId;
        return {};
    }
    FHansaBuildingId EndpointBuilding(const FHansaInventoryProjection& Inventory,
        const FHansaLogisticsJobProjection& Job)
    {
        if (Inventory.BuildingId.IsValid()) return Inventory.BuildingId;
        return Inventory.OwnerKind == EHansaInventoryOwnerKind::City
            ? Job.SelectedMarketBuildingId : FHansaBuildingId();
    }
    const FHansaBuildingWorldProjection* FindBuilding(const FHansaSimulationProjection& Projection,
        const FHansaBuildingId BuildingId)
    {
        return BuildingId.IsValid() ? Projection.GetBuildingWorldProjections().FindByPredicate(
            [BuildingId](const FHansaBuildingWorldProjection& Value) { return Value.BuildingId == BuildingId; }) : nullptr;
    }
}
AHansaCargoProjectionManager::AHansaCargoProjectionManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
}
void AHansaCargoProjectionManager::ResetActors()
{
    for (auto& Pair : Actors) if (IsValid(Pair.Value)) Pair.Value->Destroy();
    for (auto& Actor : SeaPool) if (IsValid(Actor)) Actor->Destroy();
    for (auto& Actor : LocalWagonPool) if (IsValid(Actor)) Actor->Destroy();
    Actors.Reset(); SeaPool.Reset(); LocalWagonPool.Reset(); Entries.Reset(); Selected = NAME_None; PeakLocalWagons = 0;
}
void AHansaCargoProjectionManager::ReleaseActor(const FName SemanticId)
{
    TObjectPtr<AHansaCargoVehiclePresentation>* Found = Actors.Find(SemanticId);
    if (!Found) return;
    AHansaCargoVehiclePresentation* Actor = Found->Get();
    Actors.Remove(SemanticId);
    if (!IsValid(Actor)) return;
    Actor->SemanticId = NAME_None;
    Actor->ClearProjection();
    (Actor->bSeaVehicle ? SeaPool : LocalWagonPool).Add(Actor);
}
void AHansaCargoProjectionManager::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetActors(); Super::EndPlay(Reason);
}

bool AHansaCargoProjectionManager::BuildRoadPath(const FHansaSimulationProjection& P,
    const FHansaLogisticsJobProjection& Job, TArray<FIntPoint>& OutPath)
{
    OutPath.Reset();
    (void)P;
    if (Job.RouteCells.IsEmpty() || Job.RoadDistanceCells != Job.RouteCells.Num() + 1) return false;
    for (int32 Index = 0; Index < Job.RouteCells.Num(); ++Index)
    {
        const FHansaGridCoordinate Cell = Job.RouteCells[Index];
        if (Index > 0 && FMath::Abs(Cell.X - Job.RouteCells[Index - 1].X) +
            FMath::Abs(Cell.Y - Job.RouteCells[Index - 1].Y) != 1)
        {
            OutPath.Reset(); return false;
        }
        OutPath.Add(FIntPoint(Cell.X, Cell.Y));
    }
    return true;
}
FVector AHansaCargoProjectionManager::SamplePath(TConstArrayView<FVector> Path, double Progress, double& OutDistance, FRotator& OutHeading)
{
    OutDistance=0; OutHeading=FRotator::ZeroRotator;
    if (Path.IsEmpty()) return FVector::ZeroVector;
    double Length=0; for(int32 I=1; I<Path.Num(); ++I) Length+=FVector::Distance(Path[I-1],Path[I]);
    OutDistance=Length*FMath::Clamp(Progress,0.,1.);
    auto AtDistance = [&](double Distance)
    {
        double Remaining=FMath::Clamp(Distance,0.,Length);
        for(int32 I=1; I<Path.Num(); ++I)
        {
            const FVector Segment=Path[I]-Path[I-1]; const double Size=Segment.Size();
            if(Size<=UE_SMALL_NUMBER) continue;
            if(Remaining<=Size) return Path[I-1]+Segment*(Remaining/Size);
            Remaining-=Size;
        }
        return Path.Last();
    };
    const FVector Location=AtDistance(OutDistance);
    const FVector Before=AtDistance(OutDistance-50.0);
    const FVector After=AtDistance(OutDistance+50.0);
    if(!Before.Equals(After,0.01)) OutHeading=(After-Before).Rotation();
    return Location;
}
void AHansaCargoProjectionManager::Synchronize(const FHansaSimulationProjection& P,
    UHansaRuntimeSimulationHost& Host, AHansaLubeckWorldFoundation& Foundation)
{
    RuntimeHost=&Host; Entries.Reset();
    // These local actors must never expose the server's unfiltered state to a network client.
    if (GetNetMode()!=NM_Standalone) {ResetActors(); return;}
    const int64 Tick=P.GetClock().GetTick().GetValue();
    auto Berth = [&](FName CityId, FVector& Location, FVector& Outward)
    {
        if (CityId==TEXT("City.Rostock")) {Location=AHansaRostockQuarter::VisitOffset()+FVector(-700,3900,-125); Outward=FVector(1,0,0); return true;}
        if (CityId!=TEXT("City.Lubeck")) return false;
        // City-level routes have no simulated dock assignment. Keep the authored city port
        // stable; an unrelated or unfinished harbor actor must not relocate a vessel.
        const FTransform StarterBerth=Foundation.GetCargoBerthTransform();
        Location=StarterBerth.GetLocation(); Outward=StarterBerth.GetRotation().GetForwardVector(); return true;
    };
    auto Acquire = [&](FEntry& E, FName DefinitionId, bool bSea)
    {
        auto* Existing=FindActor(E.Observation.SemanticId);
        if(Existing) return Existing;
        TArray<TObjectPtr<AHansaCargoVehiclePresentation>>& Pool=bSea?SeaPool:LocalWagonPool;
        while(!Pool.IsEmpty())
        {
            TObjectPtr<AHansaCargoVehiclePresentation> Reused=Pool.Pop(EAllowShrinking::No);
            if(!IsValid(Reused))continue;
            Actors.Add(E.Observation.SemanticId,Reused);
            Reused->SemanticId=E.Observation.SemanticId;
            return Reused.Get();
        }
        int32 Count=0;
        for (const auto& Pair : Actors) if(IsValid(Pair.Value) && Pair.Value->bSeaVehicle==bSea) ++Count;
        const int32 Guard=bSea?MaximumSeaVehicleActors:MaximumLocalWagonActors;
        if(Count >= Guard)
        {
            E.Observation.PresentationFailure=FString::Printf(TEXT("Vehicle presentation safety guard reached (%d)"),Guard);
            return static_cast<AHansaCargoVehiclePresentation*>(nullptr);
        }
        const auto* Definition=Cast<UHansaVehicleDefinition>(UHansaDefinitionBase::ResolveByStableId(DefinitionId.ToString()));
        UClass* Class=Definition?Definition->LoadPresentationActorClass():nullptr;
        auto* Actor=Class?GetWorld()->SpawnActor<AHansaCargoVehiclePresentation>(Class):nullptr;
        if(!Actor || Actor->bSeaVehicle!=bSea || !Actor->Body || !Actor->Body->GetStaticMesh())
        {if(Actor)Actor->Destroy(); E.Observation.PresentationFailure=TEXT("Verified vehicle presentation unavailable"); return static_cast<AHansaCargoVehiclePresentation*>(nullptr);}
        Actors.Add(E.Observation.SemanticId,Actor); Actor->SemanticId=E.Observation.SemanticId; return Actor;
    };
    // Free stale identities before capacity checks, including save rollback and cancelled routes.
    TSet<FName> Live;
    for(const auto& V:P.GetVehicles()) if(V.OwnerId==Host.GetHouseId() && V.Mode==EHansaRouteMode::Sea)
        Live.Add(FName(*(TEXT("World.Cargo.Vehicle.")+Identity(V.Id))));
    for(const auto& J:P.GetLogisticsJobs()) if(J.Status!=EHansaLogisticsJobStatus::Completed)
        Live.Add(FName(*(TEXT("World.Cargo.Delivery.")+Identity(J.Id))));
    TArray<FName> Stale;
    for(const auto& Pair:Actors)if(!Live.Contains(Pair.Key))Stale.Add(Pair.Key);
    for(const FName Id:Stale)ReleaseActor(Id);
    for(const auto& V:P.GetVehicles())
    {
        if(V.OwnerId!=Host.GetHouseId() || V.Mode!=EHansaRouteMode::Sea) continue;
        FEntry E; auto& O=E.Observation;
        O.SemanticId=FName(*(TEXT("World.Cargo.Vehicle.")+Identity(V.Id))); O.VehicleId=Identity(V.Id);
        O.CargoInventoryId=Identity(V.CargoInventoryId); O.CargoMilliUnits=V.Cargo.GetRawValue(); O.SimulationTick=Tick;
        O.CityId=City(V.CurrentCityId);
        const FHansaRouteProjection* R=nullptr;
        for(const auto& Route:P.GetRoutes()) if(Route.VehicleId==V.Id && Route.OwnerId==V.OwnerId)
            if(!R || R->Lifecycle==EHansaRouteLifecycleState::Cancelled) R=&Route;
        bool Traveling=false; bool Arriving=false;
        if(R)
        {
            O.RouteId=Identity(R->Id);
            E.StartProgress=double(R->Progress.GetPartsPerMillion())/FHansaRate::Scale;
            O.TransferTick=R->LastTransfer.Outcome==EHansaRouteTransferOutcome::None?-1:R->LastTransfer.Tick.GetValue();
            O.TransferMilliUnits=R->LastTransfer.AppliedQuantity.GetRawValue(); O.GoodId=FName(*R->LastTransfer.GoodId.ToString());
            if(R->Lifecycle==EHansaRouteLifecycleState::Cancelled) O.Phase=EHansaCargoWorldPhase::Cancelled;
            else if(R->Lifecycle==EHansaRouteLifecycleState::Traveling && R->TotalTravelTicks>0 && R->Stops.IsValidIndex(R->CurrentStopIndex) && R->Stops.IsValidIndex(R->NextStopIndex))
            {
                Traveling=true; E.ProgressPerTick=1./R->TotalTravelTicks;
                Arriving=R->RemainingTravelTicks<=FMath::Max(1,R->TotalTravelTicks/4);
                O.CityId=City(R->Stops[Arriving?R->NextStopIndex:R->CurrentStopIndex].CityId);
                O.Phase=Arriving?EHansaCargoWorldPhase::Arriving:(E.StartProgress<0.25?EHansaCargoWorldPhase::Departing:EHansaCargoWorldPhase::Traveling);
                // Port lanes are a compressed display of a voyage, not navigable geography.
                E.LaneStart=Arriving?1.-double(FMath::Max(1,R->TotalTravelTicks/4))/R->TotalTravelTicks:0.;
                E.LaneScale=Arriving?1.-E.LaneStart:0.75;
            }
            else if(O.TransferTick==Tick && O.TransferMilliUnits>0)
                O.Phase=R->LastTransfer.Kind==EHansaRouteCargoActionKind::Load?EHansaCargoWorldPhase::Loading:EHansaCargoWorldPhase::Unloading;
        }
        if(R && O.Phase!=EHansaCargoWorldPhase::Cancelled && O.TransferTick==Tick && O.TransferMilliUnits>0)
        {
            O.Phase=R->LastTransfer.Kind==EHansaRouteCargoActionKind::Load?EHansaCargoWorldPhase::Loading:EHansaCargoWorldPhase::Unloading;
            E.ProgressPerTick=0; // A recorded atomic transfer is held at the port for this displayed tick.
        }
        const auto* Inventory=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==V.CargoInventoryId && I.VehicleId==V.Id;});
        int64 StockTotal=0; if(Inventory) for(const auto& Stock:Inventory->Stocks)StockTotal+=Stock.Stock.GetRawValue();
        if(!Inventory || StockTotal!=O.CargoMilliUnits) O.PresentationFailure=TEXT("Cargo inventory projection mismatch");
        FVector Dock, Outward;
        if(!Berth(O.CityId,Dock,Outward)) O.PresentationFailure=TEXT("City berth presentation unavailable");
        else E.Path=Traveling?(Arriving?TArray<FVector>{Dock+Outward*6500,Dock}:TArray<FVector>{Dock,Dock+Outward*6500}):TArray<FVector>{Dock};
        if(O.Phase!=EHansaCargoWorldPhase::Cancelled && O.PresentationFailure.IsEmpty())
            if(auto* Actor=Acquire(E,TEXT("Vehicle.Cog"),true))
            {FHansaRouteProjection SkinRoute; const FHansaRouteProjection* Applied=R;
                if(R && (O.Phase==EHansaCargoWorldPhase::Loading || O.Phase==EHansaCargoWorldPhase::Unloading)) {SkinRoute=*R; SkinRoute.Lifecycle=EHansaRouteLifecycleState::AtStop; Applied=&SkinRoute;}
                if(!Actor->ApplyVehicle(V,Applied))O.PresentationFailure=TEXT("Vehicle projection rejected"); else Actor->SetActorRotation((-Outward).Rotation());}
        Entries.Add(MoveTemp(E));
    }
    for(const auto& J:P.GetLogisticsJobs())
    {
        const auto* Source=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==J.SourceInventoryId;});
        const auto* Destination=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==J.DestinationInventoryId;});
        if(!Source || !Destination || InventoryCity(P,*Source)!=Host.GetCityId()) continue;
        bool bOtherOwner=false;
        for(const auto& B:P.GetBuildingWorldProjections()) if(B.BuildingId==Source->BuildingId && B.OwnerId!=Host.GetHouseId()) bOtherOwner=true;
        if(bOtherOwner)continue;
        // Completed receipts belong in the simulation query/history, not an unbounded actor ledger.
        if(J.Status==EHansaLogisticsJobStatus::Completed && J.DeliveryTick.GetValue()!=Tick) continue;
        FEntry E; auto& O=E.Observation;
        O.SemanticId=FName(*(TEXT("World.Cargo.Delivery.")+Identity(J.Id))); O.JobId=Identity(J.Id); O.RequestId=Identity(J.RequestId); O.CityId=City(InventoryCity(P,*Source));
        O.SourceInventoryId=Identity(J.SourceInventoryId); O.DestinationInventoryId=Identity(J.DestinationInventoryId);
        const FHansaBuildingId SourceBuildingId=EndpointBuilding(*Source,J);
        const FHansaBuildingId DestinationBuildingId=EndpointBuilding(*Destination,J);
        O.SourceBuildingId=Identity(SourceBuildingId); O.DestinationBuildingId=Identity(DestinationBuildingId);
        O.GoodId=FName(*J.GoodId.ToString()); O.QuantityMilliUnits=J.Quantity.GetRawValue(); O.CargoMilliUnits=J.CargoQuantity.GetRawValue(); O.SimulationTick=Tick;
        O.RoadDistanceCells=J.RoadDistanceCells; O.ElapsedTravelTicks=J.ElapsedTravelTicks; O.RemainingTravelTicks=J.RemainingTravelTicks;
        O.LogisticsStatus=LogisticsStatus(J.Status); O.PauseReason=RoadFailure(J.PauseReason);
        O.Phase=J.Status==EHansaLogisticsJobStatus::Completed?EHansaCargoWorldPhase::Delivered:
            J.Status==EHansaLogisticsJobStatus::InTransit?EHansaCargoWorldPhase::Traveling:
            J.Status==EHansaLogisticsJobStatus::PausedInTransit?EHansaCargoWorldPhase::DeliveryPaused:
            J.Status==EHansaLogisticsJobStatus::PausedAwaitingPickup?EHansaCargoWorldPhase::PickupPaused:
            EHansaCargoWorldPhase::AwaitingPickup;
        if(J.Status==EHansaLogisticsJobStatus::Completed){E.StartProgress=1; O.TransferTick=J.DeliveryTick.GetValue(); O.TransferMilliUnits=J.Quantity.GetRawValue();}
        const FHansaBuildingWorldProjection* SourceBuilding=FindBuilding(P,SourceBuildingId);
        const FHansaBuildingWorldProjection* DestinationBuilding=FindBuilding(P,DestinationBuildingId);
        TArray<FIntPoint> Path;
        if(!SourceBuilding || !DestinationBuilding) O.PresentationFailure=TEXT("Delivery endpoint building presentation unavailable");
        else
        {
            O.SourceBuildingDefinitionId=FName(*SourceBuilding->Placement.BuildingDefinitionId.ToString());
            O.DestinationBuildingDefinitionId=FName(*DestinationBuilding->Placement.BuildingDefinitionId.ToString());
            E.Path.Add(Foundation.PlacementCellToWorld(SourceBuilding->Placement.Anchor.X,SourceBuilding->Placement.Anchor.Y,100));
            if(BuildRoadPath(P,J,Path)) for(const auto& C:Path) E.Path.Add(Foundation.PlacementCellToWorld(C.X,C.Y,100));
            else if(J.Status!=EHansaLogisticsJobStatus::PausedAwaitingPickup) O.PresentationFailure=TEXT("Dispatched road path unavailable");
            E.Path.Add(Foundation.PlacementCellToWorld(DestinationBuilding->Placement.Anchor.X,DestinationBuilding->Placement.Anchor.Y,100));
        }
        const int32 Duration=J.ElapsedTravelTicks+J.RemainingTravelTicks;
        if((J.Status==EHansaLogisticsJobStatus::InTransit || J.Status==EHansaLogisticsJobStatus::PausedInTransit) && Duration>0)
        {E.StartProgress=FMath::Clamp(double(J.ElapsedTravelTicks)/Duration,0.,1.); E.ProgressPerTick=J.Status==EHansaLogisticsJobStatus::InTransit?1./Duration:0.;}
        // Jobs may share the same dispatch tick and road. Deterministic lane/convoy slots keep every
        // authoritative wagon individually visible without changing job progress or inventory timing.
        const uint64 VisualSlot=J.Id.GetValue()>0?J.Id.GetValue()-1:0;
        E.LateralOffsetCentimetres=(static_cast<int32>(VisualSlot%5)-2)*115.0;
        E.LongitudinalOffsetCentimetres=-static_cast<int32>((VisualSlot/5)%5)*280.0;
        if(O.PresentationFailure.IsEmpty())
            if(auto* Actor=Acquire(E,TEXT("Vehicle.Wagon"),false)) if(!Actor->ApplyLocalDelivery(J))O.PresentationFailure=TEXT("Delivery projection rejected");
        Entries.Add(MoveTemp(E));
    }
    Entries.Sort([](const FEntry& A,const FEntry& B){return A.Observation.SemanticId.LexicalLess(B.Observation.SemanticId);});
    TSet<FName> Retained;
    for(const auto& E:Entries) if(E.Observation.PresentationFailure.IsEmpty() && E.Observation.Phase!=EHansaCargoWorldPhase::Cancelled) Retained.Add(E.Observation.SemanticId);
    Stale.Reset();
    for(const auto& Pair:Actors)if(!Retained.Contains(Pair.Key))Stale.Add(Pair.Key);
    for(const FName Id:Stale)ReleaseActor(Id);
    PeakLocalWagons=FMath::Max(PeakLocalWagons,GetActiveLocalWagonCount());
    if(!Retained.Contains(Selected))Selected=NAME_None;
    Sample(0);
}
int32 AHansaCargoProjectionManager::GetActiveLocalWagonCount() const
{
    int32 Count=0;
    for(const auto& Pair:Actors)if(IsValid(Pair.Value)&&!Pair.Value->bSeaVehicle)++Count;
    return Count;
}
int32 AHansaCargoProjectionManager::GetPooledLocalWagonCount() const
{
    int32 Count=0;
    for(const auto& Actor:LocalWagonPool)if(IsValid(Actor))++Count;
    return Count;
}
void AHansaCargoProjectionManager::Sample(double Fraction)
{
    Fraction=FMath::IsFinite(Fraction)?FMath::Clamp(Fraction,0.,0.999999):0.;
    for(auto& E:Entries)
    {
        auto& O=E.Observation;
        O.Progress=FMath::Clamp(E.StartProgress+Fraction*E.ProgressPerTick,0.,1.);
        const double LaneProgress=FMath::Clamp((O.Progress-E.LaneStart)/E.LaneScale,0.,1.);
        double Distance; FRotator Heading;
        O.Location=SamplePath(E.Path,LaneProgress,Distance,Heading);
        if(E.Path.Num()>1)
            O.Location+=Heading.Quaternion().RotateVector(
                FVector(E.LongitudinalOffsetCentimetres,E.LateralOffsetCentimetres,0));
        auto* Actor=FindActor(O.SemanticId);
        O.bVisible=Actor && O.PresentationFailure.IsEmpty() && (O.CityId!=TEXT("City.Rostock") || bRostockVisible);
        if(Actor)
        {
            Actor->Tags.Remove(TEXT("City.Lubeck")); Actor->Tags.Remove(TEXT("City.Rostock")); Actor->Tags.AddUnique(O.CityId);
            Actor->SetActorHiddenInGame(!O.bVisible); Actor->SetActorEnableCollision(O.bVisible);
            Actor->SetActorLocation(O.Location);
            if(E.Path.Num()>1)Actor->SetActorRotation(Heading);
            Actor->SetWheelTravelDistance(Distance);
            Actor->SetSelected(O.SemanticId==Selected);
        }
    }
}
void AHansaCargoProjectionManager::SetRostockVisible(bool bVisible)
{
    bRostockVisible=bVisible; Sample(RuntimeHost.IsValid()?RuntimeHost->GetPresentationTickFraction():0.);
}
TArray<FHansaCargoWorldObservation> AHansaCargoProjectionManager::QueryCargo() const
{
    TArray<FHansaCargoWorldObservation> Result; for(const auto& E:Entries)Result.Add(E.Observation); return Result;
}
const FHansaCargoWorldObservation* AHansaCargoProjectionManager::FindObservation(FName Id) const
{
    for(const auto& E:Entries)if(E.Observation.SemanticId==Id)return &E.Observation; return nullptr;
}
AHansaCargoVehiclePresentation* AHansaCargoProjectionManager::FindActor(FName Id) const
{
    const auto* Found=Actors.Find(Id); return Found && IsValid(*Found)?Found->Get():nullptr;
}
bool AHansaCargoProjectionManager::SelectCargo(FName Id)
{
    const auto* O=FindObservation(Id); if(!O || !O->bVisible)return false;
    Selected=Id; Sample(RuntimeHost.IsValid()?RuntimeHost->GetPresentationTickFraction():0.); return true;
}

void AHansaCargoProjectionManager::ClearSelection()
{
    Selected=NAME_None;
    for(const auto& Pair:Actors)if(IsValid(Pair.Value))Pair.Value->SetSelected(false);
}
