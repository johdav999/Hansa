#include "World/HansaRostockQuarter.h"
#include "World/HansaTerrainPlacement.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#define LOCTEXT_NAMESPACE "HansaRostock"
AHansaRostockQuarter::AHansaRostockQuarter()
{
    PrimaryActorTick.bCanEverTick=false;bReplicates=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RostockDatum")));
    Tags.Add(TEXT("City.Rostock"));Tags.Add(TEXT("Presentation.RemoteCity"));
    const TCHAR* Roles[]={TEXT("Residences"),TEXT("Market"),TEXT("Bakery"),TEXT("Mill"),TEXT("Quay"),TEXT("Dock"),TEXT("Hoist"),TEXT("WarehouseYard"),TEXT("Mooring"),TEXT("Mill"),TEXT("Street")};
    const TCHAR* Paths[]={TEXT("/Game/Mesh/hansa-residences/Meshes_R06/SM_Residence_Laborer_A"),TEXT("/Game/Mesh/hansa-market/Meshes/SM_HansaMarket"),TEXT("/Game/Mesh/hansa-bakery/P10/Meshes/SM_Bakery_Body"),TEXT("/Game/Mesh/hansa-mill/P09/Meshes/SM_Mill_Body"),TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaQuay_Edge4m"),TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaDock_Deck4m"),TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaHarborHoist"),TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaHarborTransferSkid"),TEXT("/Game/Mesh/hansa-harbor/Meshes/SM_HansaMooring_Post"),TEXT("/Game/Mesh/hansa-mill/P09/Meshes/SM_Mill_Rotor"),TEXT("/Game/Mesh/hansa-dirt-road/Meshes/SM_HansaRoad_Straight")};
    for(int32 I=0;I<UE_ARRAY_COUNT(Roles);++I)
    {
        auto* C=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*FString::Printf(TEXT("%s%d"),Roles[I],I));C->SetupAttachment(RootComponent);
        ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Paths[I]);C->SetStaticMesh(Mesh.Object);
        C->SetCollisionProfileName(TEXT("NoCollision"));C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);C->SetCollisionResponseToAllChannels(ECR_Ignore);C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);C->SetCullDistances(0,22000);
        C->ComponentTags.Add(FName(Roles[I]));Modules.Add(C);
    }
}
double AHansaRostockQuarter::GroundHeight(double Y)
{
    const double T=FMath::Clamp((Y-2300.)/900.,0.,1.);return FMath::Lerp(100.,-450.,T*T*(3.-2.*T));
}
void AHansaRostockQuarter::OnConstruction(const FTransform& T)
{
    Super::OnConstruction(T);RefreshTerrainPlacement();
}
void AHansaRostockQuarter::RefreshTerrainPlacement()
{
    for(auto C:Modules)C->ClearInstances();
    auto Add=[this](int I,double X,double Y,double Z=100,double Yaw=0)
    {
        // Rotor follows its parent mill datum; decks remain on their shared harbor datum.
        const FVector Local(X,Y,Z);
        const FVector World = GetActorTransform().TransformPosition(Local);
        const FVector Anchor = GetActorTransform().TransformPosition(I==9 ? FVector(3600,1500,100) : FVector(X,Y,100));
        FVector Grounded = Hansa::Game::TerrainPlacement::Ground(GetWorld(),Anchor,Anchor.Z);
        const bool bWaterfront = I==4 || I==5 || I==6 || I==8;
        const FVector Result = bWaterfront ? Local : GetActorTransform().InverseTransformPosition(World + Grounded - Anchor);
        const FQuat Heading = FRotator(0,Yaw,0).Quaternion();
        const FQuat Rotation = I==10 ? GetActorQuat().Inverse() * Hansa::Game::TerrainPlacement::RoadRotation(
            GetWorld(),GetActorTransform().TransformPosition(Result),GetActorQuat()*Heading) : Heading;
        Modules[I]->AddInstance(FTransform(Rotation,Result));
    };
    for(double Y:{-3700.,-1900.,-100.})for(double X:{-4300.,-2500.,1800.,3600.})Add(0,X,Y,100,Y<0?0:180);
    for(int Y=-11;Y<6;++Y){Add(10,-1300,Y*400,101,90);Add(10,700,Y*400,101,90);}
    Add(9,3600,1899,1183.5);
    Add(1,-200,-1600);Add(2,-4500,1500);Add(3,3600,1500);
    for(int I=0;I<22;++I){Add(4,-4400+400*I,2300,100,90);if(I%3==0)Add(8,-4400+400*I,2350);}
    for(int I=0;I<3;++I)Add(5,-700,2500+400*I,100);Add(6,-1250,2100);Add(7,-2000,1800);Add(7,-2000,2200);
}
FName AHansaRostockQuarter::RoleFor(const UPrimitiveComponent* C){return C&&!C->ComponentTags.IsEmpty()?C->ComponentTags[0]:NAME_None;}
FText AHansaRostockQuarter::LabelFor(FName R)
{
    if(R==TEXT("Cargo"))return LOCTEXT("Cargo","Cargo vessel");
    if(R==TEXT("Market"))return LOCTEXT("Market","Rostock market");
    if(R==TEXT("WarehouseYard"))return LOCTEXT("Yard","Warehouse handling yard");
    if(R==TEXT("Dock")||R==TEXT("Hoist")||R==TEXT("Quay")||R==TEXT("Mooring"))return LOCTEXT("Harbor","Warnow waterfront");
    if(R==TEXT("Bakery"))return LOCTEXT("Bakery","Bakers' quarter");
    if(R==TEXT("Mill"))return LOCTEXT("Mill","Milling quarter");
    return LOCTEXT("Homes","Merchant residences");
}
#undef LOCTEXT_NAMESPACE
