#include "Compounds/HansaCompoundPreviewCommandlet.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "World/HansaCompoundPresentation.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
UHansaCompoundPreviewCommandlet::UHansaCompoundPreviewCommandlet(){IsClient=false;IsEditor=true;LogToConsole=true;}
int32 UHansaCompoundPreviewCommandlet::Main(const FString& Params)
{
 FString Source,Revision=TEXT("R01");FParse::Value(*Params,TEXT("Source="),Source);FParse::Value(*Params,TEXT("Revision="),Revision);
 for(TCHAR C:Revision)if(!FChar::IsAlnum(C))return 1;
 FString Json,Error;if(!FFileHelper::LoadFileToString(Json,*Source))return 1;
 const FString Root=TEXT("/Game/Hansa/Developer/CompoundPreview/")+Revision;
 if(FPackageName::DoesPackageExist(Root/TEXT("L_CompoundParcels"))){UE_LOG(LogTemp,Error,TEXT("Preview revision exists; choose a new revision."));return 1;}
 UWorld* World=UEditorLoadingAndSavingUtils::NewBlankMap(false);if(!World)return 1;
 for(int32 Width:{3,4})
 {
  auto* Draft=Hansa::Editor::Compounds::ImportDraft(Json,Error);if(!Draft){UE_LOG(LogTemp,Error,TEXT("%s"),*Error);return 1;}
  Draft->FootprintWidthCells=Width;Draft->BoundsMin.X=-Width*200.;Draft->BoundsMax.X=Width*200.;
  for(auto& Layout:Draft->Layouts)for(auto& Node:Layout.Nodes)if(Node.bRoadEntrance)Node.Position.X=Width*200.;
  Draft->StableDefinitionId=FString::Printf(TEXT("Compound.PromptTwoFixture%dBy16"),Width*4);Draft->RefreshContentHash();
  const FString Name=FString::Printf(TEXT("DA_CompoundFixture_%dx16"),Width*4),PackageName=Root/Name;
  if(FPackageName::DoesPackageExist(PackageName))return 1;
  UPackage* Package=CreatePackage(*PackageName);Draft->Rename(*Name,Package,REN_DontCreateRedirectors);Draft->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Draft);
  const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  if(!UPackage::SavePackage(Package,Draft,*Filename,Args))return 1;
  auto* Preview=World->SpawnActor<AHansaCompoundPresentation>();Preview->SetActorLabel(FString::Printf(TEXT("Compound fixture %d x 16 m"),Width*4));
  Preview->SetActorLocation(FVector((Width-3)*2000.,0,0));Preview->Definition=Draft;Preview->PreviewSeed=1;Preview->RebuildPreview();
  if(!Preview->GetParcelBounds().IsValid)return 1;
 }
 auto* Floor=World->SpawnActor<AStaticMeshActor>();Floor->SetActorLabel(TEXT("Neutral review ground"));Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));Floor->SetActorLocation(FVector(1000,0,-2));Floor->SetActorScale3D(FVector(200));
 auto* Sun=World->SpawnActor<ADirectionalLight>();Sun->SetActorRotation(FRotator(-45,-35,0));Sun->GetLightComponent()->SetIntensity(3.f);
 auto* Sky=World->SpawnActor<ASkyLight>();Sky->GetLightComponent()->SetIntensity(.8f);
 if(!UEditorLoadingAndSavingUtils::SaveMap(World,Root/TEXT("L_CompoundParcels")))return 1;
 UE_LOG(LogTemp,Display,TEXT("COMPOUND_PREVIEW_SAVED %s"),*Root);return 0;
}
