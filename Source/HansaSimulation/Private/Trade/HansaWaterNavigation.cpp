#include "Trade/HansaWaterNavigation.h"
#include "Containers/Map.h"

namespace Hansa::Simulation
{
    namespace
    {
        const FHansaPlacementGridCell* CellAt(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate Cell)
        {
            if (Cell.X < Map.BoundsMin.X || Cell.X > Map.BoundsMax.X || Cell.Y < Map.BoundsMin.Y || Cell.Y > Map.BoundsMax.Y) return nullptr;
            // Dense surveys support constant-time lookups; sparse authored maps retain binary lookup.
            const int64 Height=int64(Map.BoundsMax.Y)-Map.BoundsMin.Y+1;
            const int64 Width=int64(Map.BoundsMax.X)-Map.BoundsMin.X+1;
            if (Width*Height==Map.Cells.Num())
            {
                const int64 Index=(int64(Cell.X)-Map.BoundsMin.X)*Height+Cell.Y-Map.BoundsMin.Y;
                return &Map.Cells[int32(Index)];
            }
            // Canonical topology order is X, then Y.
            int32 Low = 0, High = Map.Cells.Num();
            while (Low < High) { const int32 Mid = Low + (High-Low)/2;
                if (Map.Cells[Mid].Coordinate < Cell) Low=Mid+1; else High=Mid; }
            return Map.Cells.IsValidIndex(Low) && Map.Cells[Low].Coordinate == Cell ? &Map.Cells[Low] : nullptr;
        }
        int64 Key(FHansaGridCoordinate Cell) { return int64((uint64(uint32(Cell.X)) << 32) | uint32(Cell.Y)); }
    }
    bool FHansaWaterNavigation::IsNavigable(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate Cell)
    {
        for (int32 X=-1; X<=1; ++X) for (int32 Y=-1; Y<=1; ++Y)
        {
            if (X*X+Y*Y > 1) continue;
            const auto* C=CellAt(Map,{Cell.X+X,Cell.Y+Y});
            if (!C || C->Terrain!=EHansaPlacementTerrain::Water || C->bBlocked) return false;
        }
        return true;
    }
    bool FHansaWaterNavigation::FindStart(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate Near, FHansaGridCoordinate& Out)
    {
        int64 Best=MAX_int64; bool Found=false;
        // Prefer turning room for the full hull at the initial berth, rather than spawning broadside at a bank.
        for (const auto& C:Map.Cells)
        {
            const int64 X=int64(C.Coordinate.X)-Near.X, Y=int64(C.Coordinate.Y)-Near.Y;
            const int64 Distance=X*X+Y*Y;
            if (C.Terrain==EHansaPlacementTerrain::Water && Distance<Best && IsNavigable(Map,C.Coordinate))
            {
                bool Room=true;
                for(int32 DX=-3;DX<=3&&Room;++DX)for(int32 DY=-3;DY<=3&&Room;++DY)
                {if(DX*DX+DY*DY>9)continue;const auto* Nearby=CellAt(Map,{C.Coordinate.X+DX,C.Coordinate.Y+DY});Room=Nearby&&Nearby->Terrain==EHansaPlacementTerrain::Water&&!Nearby->bBlocked;}
                if(Room){Best=Distance;Out=C.Coordinate;Found=true;}
            }
        }
        return Found;
    }
    bool FHansaWaterNavigation::FindPath(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate From,
        FHansaGridCoordinate To, TArray<FHansaGridCoordinate>& Out)
    {
        Out.Reset();
        if (!IsNavigable(Map,From) || !IsNavigable(Map,To)) return false;
        if (From==To) return true;
        TArray<FHansaGridCoordinate> Queue{From}; TArray<int32> Parent{INDEX_NONE};
        TMap<int64,int32> Seen; Seen.Add(Key(From),0);
        const FHansaGridCoordinate Steps[]={{1,0},{0,1},{-1,0},{0,-1}};
        for (int32 I=0; I<Queue.Num(); ++I)
        {
            for (auto Step:Steps)
            {
                const FHansaGridCoordinate Next{Queue[I].X+Step.X,Queue[I].Y+Step.Y};
                if (Seen.Contains(Key(Next))) continue;
                if (!IsNavigable(Map,Next)) { Seen.Add(Key(Next),INDEX_NONE);continue; }
                const int32 N=Queue.Add(Next); Parent.Add(I); Seen.Add(Key(Next),N);
                if (Next==To)
                {
                    for (int32 P=N;P>0;P=Parent[P]) Out.Add(Queue[P]);
                    for (int32 A=0,B=Out.Num()-1;A<B;++A,--B) Swap(Out[A],Out[B]);
                    return true;
                }
            }
        }
        return false;
    }
    bool FHansaWaterNavigation::Validate(const FHansaVehicleState& V, const FHansaPlacementState& Placement)
    {
        const auto& N=V.Navigation;
        if (!N.CityId.IsValid()) return N.Path.IsEmpty() && N.NextIndex==0;
        const auto* Map=Placement.GetMaps().FindByPredicate([&](const auto& M){return M.CityId==N.CityId;});
        if (!Map || V.Mode!=EHansaRouteMode::Sea || N.NextIndex<0 || N.NextIndex>N.Path.Num() ||
            !IsNavigable(*Map,N.Home) || !IsNavigable(*Map,N.Cell)) return false;
        if (N.NextIndex>0 && N.Path[N.NextIndex-1]!=N.Cell) return false;
        auto Last=N.Path.IsEmpty()?N.Cell:N.Path[0];
        for (int32 I=0;I<N.Path.Num();++I)
        {
            const auto C=N.Path[I];
            if ((I>0 && FMath::Abs(C.X-Last.X)+FMath::Abs(C.Y-Last.Y)!=1) || !IsNavigable(*Map,C)) return false;
            if (I==N.NextIndex && FMath::Abs(C.X-N.Cell.X)+FMath::Abs(C.Y-N.Cell.Y)!=1) return false;
            Last=C;
        }
        return true;
    }
}
