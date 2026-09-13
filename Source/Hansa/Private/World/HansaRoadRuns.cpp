#include "World/HansaRoadRuns.h"

namespace Hansa::Game
{
namespace
{
bool Less(FIntPoint A,FIntPoint B){return A.X!=B.X?A.X<B.X:A.Y<B.Y;}
struct FEdge
{
    FIntPoint A,B;
    FEdge(FIntPoint X,FIntPoint Y):A(Less(X,Y)?X:Y),B(Less(X,Y)?Y:X){}
    bool operator==(const FEdge& Other)const{return A==Other.A && B==Other.B;}
    friend uint32 GetTypeHash(const FEdge& E){return HashCombine(GetTypeHash(E.A),GetTypeHash(E.B));}
};
const FIntPoint Offsets[]={{1,0},{0,1},{-1,0},{0,-1}};
}
TArray<TArray<FIntPoint>> BuildRoadRuns(const TSet<FIntPoint>& Cells)
{
    TArray<TArray<FIntPoint>> Runs;
    TArray<FIntPoint> Sorted=Cells.Array();Sorted.Sort(Less);
    TMap<FIntPoint,TArray<FIntPoint>> Neighbors;
    for(FIntPoint C:Sorted)for(FIntPoint D:Offsets)if(Cells.Contains(C+D))Neighbors.FindOrAdd(C).Add(C+D);
    TSet<FEdge> Visited;
    auto Walk=[&](FIntPoint Start,FIntPoint Next)
    {
        TArray<FIntPoint> Run={Start};FIntPoint Previous=Start,Current=Next;
        while(!Visited.Contains(FEdge(Previous,Current)))
        {
            Visited.Add(FEdge(Previous,Current));Run.Add(Current);
            const auto& Adjacent=Neighbors.FindChecked(Current);
            if(Current==Start || Adjacent.Num()!=2)break;
            const FIntPoint Following=Adjacent[0]==Previous?Adjacent[1]:Adjacent[0];
            Previous=Current;Current=Following;
        }
        Runs.Add(MoveTemp(Run));
    };
    for(FIntPoint C:Sorted)
    {
        const auto* Adjacent=Neighbors.Find(C);
        if(!Adjacent){Runs.Add({C});continue;}
        if(Adjacent->Num()==2)continue;
        for(FIntPoint Next:*Adjacent)if(!Visited.Contains(FEdge(C,Next)))Walk(C,Next);
    }
    // Remaining edges are isolated cycles without endpoints or junctions.
    for(FIntPoint C:Sorted)if(const auto* Adjacent=Neighbors.Find(C))
        for(FIntPoint Next:*Adjacent)if(!Visited.Contains(FEdge(C,Next)))Walk(C,Next);
    return Runs;
}
TMap<FIntPoint,uint8> RoadRunNeighborMasks(const TArray<TArray<FIntPoint>>& Runs)
{
    TMap<FIntPoint,uint8> Result;
    for(const auto& Run:Runs)
    {
        for(FIntPoint Cell:Run)Result.FindOrAdd(Cell);
        for(int32 I=1;I<Run.Num();++I)
            for(int32 Bit=0;Bit<4;++Bit)if(Run[I]-Run[I-1]==Offsets[Bit])
            {
                Result.FindOrAdd(Run[I-1])|=1<<Bit;
                Result.FindOrAdd(Run[I])|=1<<((Bit+2)%4);
            }
    }
    return Result;
}
}
