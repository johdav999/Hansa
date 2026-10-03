#include "Placement/HansaRostockPlacement.h"
namespace Hansa::Simulation::RostockPlacement
{
 FHansaPlacementMapInitialization CreateMap()
 {
  FHansaPlacementMapInitialization M;
  M.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
  M.RoadBuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
  M.BoundsMin={-8,-8};M.BoundsMax={40,40};
  // Municipal land is never player-owned. Access comes exclusively from active leases.
  const auto Municipal=FHansaHouseId::TryCreate(0xfffffffe).Value;
  for(int32 X=-8;X<=40;++X)for(int32 Y=-8;Y<=40;++Y)
  {
   const FVector P=CellCenter(X,Y)-FVector(WorldOffsetX,0,0);
   auto Intersects=[&](double CX,double CY,double HX,double HY){return FMath::Abs(P.X-CX)<HX+200 && FMath::Abs(P.Y-CY)<HY+200;};
   bool Blocked=false;
   for(double HX : {-4300.,-2500.,1800.,3600.})for(double HY : {-3700.,-1900.,-100.})Blocked|=Intersects(HX,HY,600,600);
   Blocked|=Intersects(-200,-1600,600,600)||Intersects(-4500,1500,1000,800)||Intersects(3600,1500,1000,1000);
   Blocked|=Intersects(1900,1750,650,650)||Intersects(-2000,2000,700,600)||Intersects(-1250,2100,450,450);
   Blocked|=Intersects(-200,2300,4600,200)||Intersects(-700,2900,300,800);
   const bool Road=(X==8||X==13)&&Y>=9&&Y<=25;
   if(Road)M.PublicRoadCells.Add({X,Y});
   M.Cells.Add({{X,Y},P.Y<2300?EHansaPlacementTerrain::Land:P.Y<3200?EHansaPlacementTerrain::Shore:EHansaPlacementTerrain::Water,Municipal,Blocked||Road});
  }
  return M;
 }
 bool IsCanonicalMap(const FHansaPlacementMapInitialization& M)
 {
  if(M.CityId.ToString()!=TEXT("City.Rostock"))return false;
  auto A=FHansaPlacementTopology::TryCreate({M}),B=FHansaPlacementTopology::TryCreate({CreateMap()});
  return A&&B&&A.Value.GetTopologyHash()==B.Value.GetTopologyHash();
 }
}
