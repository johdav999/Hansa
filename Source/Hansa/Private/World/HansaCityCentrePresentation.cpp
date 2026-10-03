#include "World/HansaCityCentrePresentation.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AHansaCityCentrePresentation::AHansaCityCentrePresentation()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=2.f; bReplicates=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MunicipalCentre")));
 Tags.Add(TEXT("City.Rostock")); Tags.Add(TEXT("Presentation.CityCentre"));
 // Hard references keep approved meshes in cooked builds. Slots remain editable in Details.
 const TCHAR* Paths[]={TEXT("hansa-residences/Meshes_R07/SM_Residence_Laborer_A"),TEXT("hansa-residences/Meshes_R07/SM_Residence_Laborer_B"),TEXT("hansa-market/Meshes/SM_HansaMarket"),TEXT("hansa-grain-farm/Final/SM_HansaGrainFarm"),TEXT("hansa-sawmill/Meshes/SM_HansaSawmill"),TEXT("hansa-artisan-production/SM_Tannery"),TEXT("hansa-fishery/SM_HansaFishery"),TEXT("hansa-dirt-road/Meshes/SM_HansaRoad_Straight"),TEXT("hansa-grain-farm/Final/SM_HansaGrainField_Center_Mature_4m")};
 TArray<UStaticMesh*> Meshes;
 for(const auto* Path:Paths){ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*(FString(TEXT("/Game/Mesh/"))+Path));Meshes.Add(Mesh.Object);}
 auto Add=[&](FString Id,const TCHAR* Label,int32 Mesh,double X,double Y,float Yaw=0,const TCHAR* Chain=TEXT(""),const TCHAR* Stage=TEXT(""),bool Selectable=true){
  FHansaCityCentreSlot S;S.Id=FName(*Id);S.Label=FText::FromString(Label);S.Mesh=Meshes[Mesh];S.Location=FVector(X,Y,0);S.Yaw=Yaw;S.ProductionChain=FName(Chain);S.ProductionStage=FName(Stage);S.bSelectable=Selectable;Slots.Add(S);
 };
 // Canonical Rostock coordinates relative to (60000,0). Leave the lease east of X=-1500 clear.
 Add(TEXT("Market"),TEXT("Rostock market"),2,-4600,-2100);
 int32 House=0;
 for(double Y:{-5500.,-4200.})for(double X:{-6400.,-5100.})Add(FString::Printf(TEXT("Home.%02d"),++House),TEXT("City residences"),House%2,X,Y,180);
 for(double Y:{-5700.,-4400.,-3000.,-1600.})Add(FString::Printf(TEXT("Home.%02d"),++House),TEXT("City residences"),House%2,-2100,Y,90);
 for(double Y:{-1900.,-500.})for(double X:{-6800.,-5700.})Add(FString::Printf(TEXT("Home.%02d"),++House),TEXT("City residences"),House%2,X,Y);
 Add(TEXT("Farm"),TEXT("Grain farm"),3,-6500,-9600,0,TEXT("ProductionChain.Bread"),TEXT("GrowGrain"));
 Add(TEXT("Sawmill"),TEXT("Sawmill"),4,-4500,-7300,0,TEXT("ProductionChain.Planks"),TEXT("SawPlanks"));
 Add(TEXT("Tannery"),TEXT("Tannery"),5,-7100,-7300,0,TEXT("ProductionChain.Shoes"),TEXT("TanLeather"));
 Add(TEXT("Fishery"),TEXT("Fishing quarter"),6,-2500,600,0,TEXT("ProductionChain.FreshFish"),TEXT("CatchFish"));
 int32 Road=0;
 for(double Y=-9800;Y<=-600;Y+=400)Add(FString::Printf(TEXT("Street.%03d"),++Road),TEXT("Street"),7,-3200,Y,90,TEXT(""),TEXT(""),false);
 for(double X=-7200;X<=-3200;X+=400)Add(FString::Printf(TEXT("Street.%03d"),++Road),TEXT("Street"),7,X,-3300,0,TEXT(""),TEXT(""),false);
 for(double X=-6400;X<=-3200;X+=400)Add(FString::Printf(TEXT("Street.%03d"),++Road),TEXT("Street"),7,X,-8700,0,TEXT(""),TEXT(""),false);
 for(double X=-3200;X<=-800;X+=400)Add(FString::Printf(TEXT("Street.%03d"),++Road),TEXT("Waterfront lane"),7,X,-600,0,TEXT(""),TEXT(""),false);
 for(int32 X=0;X<5;++X)for(int32 Y=0;Y<4;++Y)Add(FString::Printf(TEXT("Field.%d.%d"),X,Y),TEXT("Grain field"),8,-8400+400*X,-11200-400*Y,0,TEXT("ProductionChain.Bread"),TEXT("GrowGrain"),false);
}
bool AHansaCityCentrePresentation::IsEnabled(const FHansaCityCentreSlot& S,const Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition* P)
{
 if(S.ProductionChain.IsNone())return true;
 if(!P)return false;
 for(const auto& B:P->IndustryBindings)if(B.bEnabled&&B.ProductionChainId==S.ProductionChain.ToString()&&B.EnabledStageKeys.Contains(S.ProductionStage.ToString()))return true;
 return false;
}
bool AHansaCityCentrePresentation::ValidateLayout(FString& Error) const
{
 if(CityId.IsNone()||LayoutVersion!=1||Slots.Num()>256||AmbientCitizens<0||AmbientCitizens>128||!FMath::IsFinite(AmbientWaterLevel)){Error=TEXT("City, supported layout version (1), at most 256 slots, ambient citizens 0-128 and finite water level are required.");return false;}
 TSet<FName> Ids;TArray<FBox> Buildings;
 for(const auto& S:Slots){
  if(S.Id.IsNone()||Ids.Contains(S.Id)||!S.Mesh||S.Location.ContainsNaN()||!FMath::IsFinite(S.Yaw)||S.ProductionChain.IsNone()!=S.ProductionStage.IsNone()){
   Error=TEXT("Slots require unique IDs, approved meshes, finite transforms, and a complete optional chain/stage pair.");return false;
  }Ids.Add(S.Id);
  if(S.bSelectable){
   const FBox Bounds=S.Mesh->GetBoundingBox().TransformBy(FTransform(FRotator(0,S.Yaw,0),S.Location));
   for(const FBox& Existing:Buildings)if(Bounds.IntersectXY(Existing)){Error=TEXT("Selectable city buildings overlap.");return false;}
   Buildings.Add(Bounds);
  }
 }Error.Reset();return true;
}
#if WITH_EDITOR
EDataValidationResult AHansaCityCentrePresentation::IsDataValid(FDataValidationContext& Context) const
{
 FString Error;if(!ValidateLayout(Error)){Context.AddError(FText::FromString(Error));return EDataValidationResult::Invalid;}
 return EDataValidationResult::Valid;
}
#endif
void AHansaCityCentrePresentation::OnConstruction(const FTransform& T)
{
 Super::OnConstruction(T);
 // Editor preview uses exactly the same accepted public industry definitions as the game.
 Hansa::Simulation::FHansaEconomicRegistry Registry;FString Error;
 bProfileApplied=false;
 if(FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Registry,Error))ApplyCity(Registry);
 else {bProfileApplied=true;EnabledSlots.Reset();for(const auto& S:Slots)if(S.ProductionChain.IsNone())EnabledSlots.Add(S.Id);Rebuild();}
}
void AHansaCityCentrePresentation::ApplyCity(const Hansa::Simulation::FHansaEconomicRegistry& Registry)
{
 TSet<FName> Next;const auto* Profile=Registry.FindCityMarket(CityId.ToString());
 for(const auto& S:Slots)if(IsEnabled(S,Profile))Next.Add(S.Id);
 if(!bProfileApplied||Next.Difference(EnabledSlots).Num()||EnabledSlots.Difference(Next).Num()){
  EnabledSlots=MoveTemp(Next);bProfileApplied=true;Rebuild();
 }
}
void AHansaCityCentrePresentation::Rebuild()
{
 for(auto C:Pieces)if(C)C->DestroyComponent();for(auto C:Selections)if(C)C->DestroyComponent();Pieces.Reset();Selections.Reset();
 FString Error;if(!ValidateLayout(Error)){UE_LOG(LogTemp,Warning,TEXT("City centre: %s"),*Error);return;}
 for(const auto& S:Slots){
  if(bProfileApplied&&!EnabledSlots.Contains(S.Id))continue;
  auto* C=NewObject<UStaticMeshComponent>(this);C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(S.Mesh);
  C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->ComponentTags.Add(S.Id);C->RegisterComponent();Pieces.Add(C);
  if(S.bSelectable){auto* B=NewObject<UBoxComponent>(this);B->SetupAttachment(C);B->SetBoxExtent(S.Mesh->GetBoundingBox().GetExtent());B->SetRelativeLocation(S.Mesh->GetBoundingBox().GetCenter());B->ComponentTags.Add(S.Id);
   B->SetCollisionEnabled(ECollisionEnabled::QueryOnly);B->SetCollisionResponseToAllChannels(ECR_Ignore);B->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);B->SetGenerateOverlapEvents(false);B->SetCanEverAffectNavigation(false);B->RegisterComponent();Selections.Add(B);
  }
 }Tick(0);
}
void AHansaCityCentrePresentation::Tick(float Delta)
{
 Super::Tick(Delta);
 for(auto C:Pieces){const auto* S=FindSlot(C);if(!S)continue;
  const FVector Nominal=GetActorTransform().TransformPosition(S->Location);
  FVector Ground=Hansa::Game::TerrainPlacement::Ground(GetWorld(),Nominal,Nominal.Z);
  const FQuat Heading=GetActorQuat()*FRotator(0,S->Yaw,0).Quaternion();
  const bool Road=S->Id.ToString().StartsWith(TEXT("Street."));if(Road)Ground.Z+=3;
  C->SetWorldLocationAndRotation(Ground,Road?Hansa::Game::TerrainPlacement::RoadRotation(GetWorld(),Ground,Heading):Heading);
 }
}
const FHansaCityCentreSlot* AHansaCityCentrePresentation::FindSlot(const UPrimitiveComponent* C) const
{
 if(!C||C->GetOwner()!=this||C->ComponentTags.IsEmpty())return nullptr;
 return Slots.FindByPredicate([C](const auto& S){return S.Id==C->ComponentTags[0];});
}
FVector AHansaCityCentrePresentation::GetMarketLocation() const
{
 for(auto C:Pieces)if(C->ComponentHasTag(TEXT("Market")))return C->GetComponentLocation();return GetActorLocation();
}
