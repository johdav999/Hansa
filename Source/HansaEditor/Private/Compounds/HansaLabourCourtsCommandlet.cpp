#include "Compounds/HansaLabourCourtsCommandlet.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "World/HansaCompoundPresentation.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

UHansaLabourCourtsCommandlet::UHansaLabourCourtsCommandlet()
{
 IsClient=false;IsEditor=true;LogToConsole=true;
}
int32 UHansaLabourCourtsCommandlet::Main(const FString& Params)
{
 FString Source,Revision=TEXT("R01");FParse::Value(*Params,TEXT("SourceDir="),Source);FParse::Value(*Params,TEXT("Revision="),Revision);
 if(Source.IsEmpty()||Revision.IsEmpty())return 1;
 for(TCHAR C:Revision)if(!FChar::IsAlnum(C))return 1;
 const FString DraftRoot=TEXT("/Game/Hansa/Generated/Staging/LabourCourts_20260915/")+Revision;
 const FString MapRoot=TEXT("/Game/Hansa/Developer/CompoundPreview/LabourCourts_")+Revision;
 const FString LevelPath=MapRoot/TEXT("L_LabourCourts");
 if(FPackageName::DoesPackageExist(LevelPath))return 1;
 const TArray<FString> Names={TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")};
 TArray<TStrongObjectPtr<UHansaResidentialCompoundDefinition>> Drafts;
 for(const FString& Name:Names)
 {
  if(FPackageName::DoesPackageExist(DraftRoot/(TEXT("DA_Compound_")+Name)))return 1;
  FString Json,Error;if(!FFileHelper::LoadFileToString(Json,*(Source/(Name+TEXT(".json")))))return 1;
  auto* D=Hansa::Editor::Compounds::ImportDraft(Json,Error);
  if(!D){UE_LOG(LogTemp,Error,TEXT("%s: %s"),*Name,*Error);return 1;}
  Drafts.Emplace(D);
  // All eligible stages/contexts and seeds, before any persistent mutation.
  for(int32 Stage=1;Stage<=3;++Stage)for(FName Context:{FName(TEXT("Straight")),FName(TEXT("CornerLeft")),FName(TEXT("CornerRight")),FName(TEXT("Edge"))})
   for(uint64 Seed=1;Seed<=64;++Seed)if(!D->Compose(Seed,Stage,Context,TEXT("District.Lubeck.LateMedieval")).IsValid())return 1;
 }
 UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);if(!World)return 1;
 for(int32 I=0;I<Drafts.Num();++I)
 {
  auto* D=Drafts[I].Get();const FString Name=TEXT("DA_Compound_")+Names[I],PackageName=DraftRoot/Name;
  UPackage* Package=CreatePackage(*PackageName);D->Rename(*Name,Package,REN_DontCreateRedirectors);D->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(D);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  if(!UPackage::SavePackage(Package,D,*FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension()),Args))return 1;
 }
 auto Spawn=[&](int32 Family,int32 Stage,uint64 Id,FVector Position,float Yaw,FName Context,const FString& Label)
 {
  auto* A=World->SpawnActor<AHansaCompoundPresentation>();A->SetActorLabel(Label);A->SetActorLocation(Position);A->SetActorRotation(FRotator(0,Yaw,0));
  A->Definition=Drafts[Family].Get();A->Stage=Stage;A->RoadContext=Context;A->DistrictId=TEXT("District.Lubeck.LateMedieval");
  A->PreviewSeed=static_cast<int64>(UHansaResidentialCompoundDefinition::ParcelSeed(TEXT("City.Lubeck"),Id,1));A->RebuildPreview();return A->GetParcelBounds().IsValid!=0;
 };
 for(int32 Stage=1;Stage<=3;++Stage)for(int32 Family=0;Family<4;++Family)
  if(!Spawn(Family,Stage,101+Family,FVector((Stage-1)*2200,Family*1800,0),0,TEXT("Straight"),FString::Printf(TEXT("Stages_%s_S%d"),*Names[Family],Stage)))return 1;
 // Two facing blocks with varied family, identity and density, plus four genuine corners.
 for(int32 Row=0;Row<2;++Row)for(int32 Col=0;Col<12;++Col)
 {
  const int32 Family=(Col*3+Row)%4,Stage=1+(Col+Row*2)%3;
  const FName Context=Col==0?FName(Row?TEXT("CornerRight"):TEXT("CornerLeft")):Col==11?FName(Row?TEXT("CornerLeft"):TEXT("CornerRight")):FName(TEXT("Straight"));
  if(!Spawn(Family,Stage,1000+Row*12+Col,FVector(9000+Row*2000,Col*1600,0),Row?180:0,Context,FString::Printf(TEXT("Block_%d_%02d_%s_S%d"),Row,Col,*Names[Family],Stage)))return 1;
 }
 UStaticMesh* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
 UMaterialInterface* Earth=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/M_Road_Earth.M_Road_Earth"));
 auto Ground=[&](const FString& Name,FVector Position,FVector Scale)
 {
  auto* A=World->SpawnActor<AStaticMeshActor>();A->SetActorLabel(Name);A->GetStaticMeshComponent()->SetStaticMesh(Plane);A->GetStaticMeshComponent()->SetMaterial(0,Earth);A->SetActorLocation(Position);A->SetActorScale3D(Scale);
 };
 Ground(TEXT("Review terrain"),FVector(6000,8000,-2),FVector(250,280,1));
 Ground(TEXT("Block street 4m"),FVector(10000,8800,-.1),FVector(4,194,1));
 Ground(TEXT("South cross street"),FVector(10000,-1000,-.1),FVector(40,4,1));
 Ground(TEXT("North cross street"),FVector(10000,18600,-.1),FVector(40,4,1));
 for(int32 I=0;I<3;++I)Ground(TEXT("Stage comparison street"),FVector(I*2200+1000,2600,-.1),FVector(4,76,1));
 auto* Sun=World->SpawnActor<ADirectionalLight>();Sun->SetActorRotation(FRotator(-45,145,0));Sun->GetLightComponent()->SetIntensity(3.f);Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->ForwardShadingPriority=1;
 auto* Fill=World->SpawnActor<ADirectionalLight>();Fill->SetActorRotation(FRotator(-60,-35,0));Fill->GetLightComponent()->SetIntensity(.7f);Fill->GetLightComponent()->SetCastShadows(false);
 if(!UEditorLoadingAndSavingUtils::SaveMap(World,LevelPath))return 1;
 UE_LOG(LogTemp,Display,TEXT("LABOUR_COURTS_VALIDATED 4 definitions, 96 layouts, 3072 compositions; saved %s"),*LevelPath);return 0;
}
