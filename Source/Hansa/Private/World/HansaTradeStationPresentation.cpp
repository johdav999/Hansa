#include "World/HansaTradeStationPresentation.h"
#include "World/HansaBuildingSelectionFootprint.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "World/HansaHarborPresentation.h"
#include "Placement/HansaRostockPlacement.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

using namespace Hansa::Simulation;
AHansaTradeStationPresentation::AHansaTradeStationPresentation()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=1.f;bReplicates=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StationSite")));
 Warehouse=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MerchantWarehouse"));
 LoadingSkid=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LoadingSkid"));
 AccessPath=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("QuayAccess"));
 ConstructorHelpers::FObjectFinder<UStaticMesh> House(TEXT("/Game/Mesh/hansa-artisan-houses/Meshes/SM_ArtisanHouse_D"));
 ConstructorHelpers::FObjectFinder<UStaticMesh> Office(TEXT("/Game/Mesh/merchant-office-hausbaumhaus/SM_MerchantOffice_Hausbaumhaus_R2"));
 ConstructorHelpers::FObjectFinder<UStaticMesh> Skid(TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaHarborTransferSkid"));
 ConstructorHelpers::FObjectFinder<UStaticMesh> Road(TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaDock_Deck4m"));
 StationHouseMesh=House.Object;MerchantOfficeMesh=Office.Object;
 Warehouse->SetStaticMesh(StationHouseMesh);LoadingSkid->SetStaticMesh(Skid.Object);AccessPath->SetStaticMesh(Road.Object);
 for(auto* C:TArray<UStaticMeshComponent*>{Warehouse,LoadingSkid,AccessPath}){
  C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Movable);
  C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetGenerateOverlapEvents(false);
 }
 // The approved merchant-house shell supplies the store and factor's rooms.
 // Empty handling equipment never pretends to be simulated stock.
 Warehouse->SetRelativeRotation(FRotator(0,180,0));LoadingSkid->SetRelativeLocation(FVector(430,120,0));
 Selection=CreateDefaultSubobject<UBoxComponent>(TEXT("StationSelection"));Selection->SetupAttachment(RootComponent);
 Selection->SetRelativeLocation(FVector(0,0,450));Selection->SetBoxExtent(FVector(370,330,470));
 Selection->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Selection->SetCollisionResponseToAllChannels(ECR_Ignore);
 Selection->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Selection->SetCanEverAffectNavigation(false);Selection->SetGenerateOverlapEvents(false);
 // Stations use the same visible footprint treatment as ordinary buildings.
 // Custom depth alone writes a mask; it does not draw the selection marker.
 ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 SelectionOutline=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionOutline"));
 SelectionCornerSegments.Reserve(8);
 for(int32 Index=0;Index<8;++Index)
  SelectionCornerSegments.Add(CreateDefaultSubobject<UStaticMeshComponent>(FName(*FString::Printf(TEXT("SelectionCorner_%d"),Index))));
 TArray<UStaticMeshComponent*> Markers;Markers.Add(SelectionOutline);
 for(UStaticMeshComponent* Segment:SelectionCornerSegments)Markers.Add(Segment);
 for(auto* Marker:Markers){
  Marker->SetupAttachment(RootComponent);Marker->SetStaticMesh(Cube.Object);
  Marker->SetMobility(EComponentMobility::Movable);Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Marker->SetCanEverAffectNavigation(false);Marker->SetGenerateOverlapEvents(false);Marker->CastShadow=false;
  Marker->SetVisibility(false);
 }
 SelectionOutline->ComponentTags.Add(TEXT("Hansa.Projection.SelectionOutline"));
 for(UStaticMeshComponent* Segment:SelectionCornerSegments)Segment->ComponentTags.Add(TEXT("Hansa.Projection.SelectionCorner"));
 Dock=CreateDefaultSubobject<UChildActorComponent>(TEXT("HarborDock"));Dock->SetupAttachment(RootComponent);
 StreamingSource=CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(TEXT("StationTerrainStreaming"));
 Tags.Add(TEXT("TradeStation"));Tags.Add(TEXT("City.Rostock"));
}
FVector AHansaTradeStationPresentation::WarehouseLocation(const FHansaLeasedPlotState& Lease)
{
 // Landward commercial plot, at its quay-facing edge. The station and later
 // office shells both fit the 8 x 8 m local construction site.
 const int32 X=Lease.BoundsMin.X+FMath::Min(3,FMath::Max(0,Lease.BoundsMax.X-Lease.BoundsMin.X));
 return RostockPlacement::CellCenter(X,Lease.BoundsMax.Y)-FVector(100,200,0);
}
FTransform AHansaTradeStationPresentation::SiteTransform(const UWorld* World)
{
 if(!Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(World))return FTransform::Identity;
 // Promoted regional Landscape: dry bank of the Warnow estuary at Rostock.
 // Source: terrain manifest / water-surface grid, dry cell (2160,2342).
 // Preserve the saved four-metre lease grid; rotate its quay direction north toward the existing estuary surface.
 const FQuat Rotation=FRotator(0,180,0).Quaternion();
 return FTransform(Rotation,FVector(4017857.142857,4356398.809524,150)-Rotation.RotateVector(FVector(59800,-600,100)));
}
void AHansaTradeStationPresentation::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 // Terrain can stream after the station is restored or the camera arrives.
 if(CurrentStation.IsSet()){const auto Station=CurrentStation.GetValue();ApplyStation(Station,bCurrentOfficeBuilt);}
}
bool AHansaTradeStationPresentation::ApplyStation(const FHansaTradeStationProjection& S,bool bMerchantOfficeBuilt)
{
 if(S.Station.CityId.ToString()!=TEXT("City.Rostock")||
   (S.Station.Status!=EHansaTradeStationStatus::Active&&S.Station.Status!=EHansaTradeStationStatus::Suspended&&!S.Station.ConstructionSite.bLocalDelivery)||S.Station.Status==EHansaTradeStationStatus::Closed)return false;
 CurrentStation=S;
 bCurrentOfficeBuilt=bMerchantOfficeBuilt;
 const bool bOfficeVisualActive=bMerchantOfficeBuilt&&MerchantOfficeMesh!=nullptr;
 UStaticMesh* DesiredMesh=bOfficeVisualActive?MerchantOfficeMesh.Get():StationHouseMesh.Get();
 if(Warehouse->GetStaticMesh()!=DesiredMesh){
  Warehouse->SetStaticMesh(DesiredMesh);
  Selection->SetRelativeLocation(bOfficeVisualActive?FVector(0,0,630):FVector(0,0,450));
  Selection->SetBoxExtent(bOfficeVisualActive?FVector(370,400,650):FVector(370,330,470));
 }
 const FTransform Site=SiteTransform(GetWorld());
 const auto& Placement=S.Station.ConstructionSite;
 const FVector Local=Placement.bLocalDelivery?RostockPlacement::CellCenter(Placement.Anchor.X,Placement.Anchor.Y)+FVector(200,200,0):WarehouseLocation(S.Lease);
 Warehouse->SetRelativeRotation(FRotator(0,Placement.bLocalDelivery?int32(Placement.Rotation)*90:180,0));
 const FVector Nominal=Site.TransformPosition(Local);
 const FVector Grounded=Hansa::Game::TerrainPlacement::Ground(GetWorld(),Nominal,Nominal.Z);
 const bool Reposition=StationId!=int64(S.Station.Id.GetValue())||!GetActorLocation().Equals(Grounded,1.);
 StationId=int64(S.Station.Id.GetValue());
 SetActorLocationAndRotation(Grounded,Site.GetRotation());SetActorHiddenInGame(false);SetActorEnableCollision(true);
 if(DockClassPath!=S.PresentationClassPath){
  UClass* Class=LoadClass<AActor>(nullptr,*S.PresentationClassPath);
  if(!Class||!Class->IsChildOf(AHansaHarborPresentation::StaticClass()))return false;
  Dock->SetChildActorClass(Class);DockClassPath=S.PresentationClassPath;
 }
 // The dock faces out across the Warnow; its shore end meets the existing quay.
 const bool Ready=S.Station.Status==EHansaTradeStationStatus::Active||S.Station.Status==EHansaTradeStationStatus::Suspended;
 Dock->SetVisibility(Ready,true);if(Dock->GetChildActor())Dock->GetChildActor()->SetActorHiddenInGame(!Ready);AccessPath->SetVisibility(Ready);LoadingSkid->SetVisibility(Ready);
 RefreshSelection();
 const FVector Quay(Local.X,2500,100);
 double DeckHeight=Grounded.Z;
 if(auto* Harbor=Cast<AHansaHarborPresentation>(Dock->GetChildActor())){
  const FQuat Heading=Site.GetRotation()*FRotator(0,90,0).Quaternion();
  const FVector Deck=Harbor->GroundDeckLocation(Site.TransformPosition(Quay+FVector(0,800,0)),Heading);
  Dock->SetWorldLocationAndRotation(Deck,Heading);DeckHeight=Deck.Z;
 }
 if(Reposition||!FMath::IsNearlyEqual(LastDeckHeight,DeckHeight,1.)||AccessPath->GetInstanceCount()==0){
  AccessPath->ClearInstances();
  LastDeckHeight=DeckHeight;
  const double End=2500;
  for(double Y=Local.Y+600;Y<=End;Y+=400){
   FVector P=Site.TransformPosition(FVector(Local.X,Y,100));P.Z=DeckHeight;
   AccessPath->AddInstance(FTransform(Site.GetRotation()*FRotator(0,90,0).Quaternion(),P),true);
  }
 }
 return Warehouse->GetStaticMesh()!=nullptr&&Dock->GetChildActor()!=nullptr;
}
void AHansaTradeStationPresentation::SetSelected(bool Selected)
{
 bSelected=Selected;RefreshSelection();
}

void AHansaTradeStationPresentation::RefreshSelection()
{
 const bool Ready=CurrentStation.IsSet()&&(CurrentStation->Station.Status==EHansaTradeStationStatus::Active||CurrentStation->Station.Status==EHansaTradeStationStatus::Suspended);
 Warehouse->SetRenderCustomDepth(bSelected||!Ready);Warehouse->SetCustomDepthStencilValue(bSelected?1:2);
 if(!SelectionMaterial){
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  if(Base){
   SelectionMaterial=UMaterialInstanceDynamic::Create(Base,this);
   SelectionMaterial->SetVectorParameterValue(TEXT("Color"),FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C19A52"))));
   SelectionOutline->SetMaterial(0,SelectionMaterial);
   for(UStaticMeshComponent* Segment:SelectionCornerSegments)Segment->SetMaterial(0,SelectionMaterial);
  }
 }
 // Both shells occupy the same 8 x 8 metre construction site. Refit on refresh
 // because terrain can arrive after save restoration or camera travel.
 if(bSelected)Hansa::Game::BuildingSelection::ConfigureFootprint(GetWorld(),SelectionOutline,SelectionCornerSegments,800,800,5);
 SelectionOutline->SetVisibility(bSelected);
 for(UStaticMeshComponent* Segment:SelectionCornerSegments)Segment->SetVisibility(bSelected);
}

