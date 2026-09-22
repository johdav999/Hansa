#include "Compounds/HansaArtisanPlotsCommandlet.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "Serialization/JsonSerializer.h"
#include "Algo/Reverse.h"

namespace
{
bool SavePlotAsset(UHansaDefinitionBase* Asset,const FString& Path)
{
 const FString Name=FPackageName::GetLongPackageAssetName(Path);
 Asset->Rename(*Name,CreatePackage(*Path),REN_DontCreateRedirectors);
 Asset->SetFlags(RF_Public|RF_Standalone);Asset->RefreshContentHash();
 FAssetRegistryModule::AssetCreated(Asset);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(Asset->GetPackage(),Asset,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
}
}
UHansaArtisanPlotsCommandlet::UHansaArtisanPlotsCommandlet()
{IsClient=false;IsEditor=true;LogToConsole=true;}
int32 UHansaArtisanPlotsCommandlet::Main(const FString& Params)
{
 const FString CompoundPath=TEXT("/Game/Hansa/Core/Compounds/ArtisanPlots/DA_Compound_ArtisanPlot");
 const FString BuildingPath=TEXT("/Game/Hansa/Core/Buildings/ArtisanPlots/DA_Building_Residence_Artisan_Plot");
 const bool Apply=FParse::Param(*Params,TEXT("Apply"));
 if(FPackageName::DoesPackageExist(CompoundPath)||FPackageName::DoesPackageExist(BuildingPath))
 {UE_LOG(LogTemp,Error,TEXT("Artisan plot revision already exists; refusing overwrite."));return 1;}
 TStrongObjectPtr<UHansaResidentialCompoundDefinition> D(NewObject<UHansaResidentialCompoundDefinition>());
 D->StableDefinitionId=TEXT("Compound.Artisan.Plot");D->DisplayName=FText::ChangeKey(TEXT("HansaDefinitions"),D->StableDefinitionId,FText::FromString(TEXT("Artisan street plots")));
 D->LocalizationKey=TEXT("Game.Compound.Artisan.Plot");D->ContentSet=TEXT("Core");D->PopulationTierId=TEXT("PopulationTier.Artisan");
 D->FootprintWidthCells=4;D->FootprintHeightCells=2;D->BoundsMin=FVector(-800,-400,0);D->BoundsMax=FVector(800,400,1000);
 auto Slot=[&](FHansaCompoundLayout& L,const FString& Id,const FString& Path,FVector Position,float Yaw,FName Group,bool Principal=false,const FString& Entry=FString())
 {
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);if(!Mesh)return false;
  FHansaCompoundSlot S;S.SlotId=Id;S.Group=Group;S.bPrincipal=Principal;S.EntranceNodeId=Entry;
  FHansaCompoundVariant V;V.Mesh=Mesh;V.VariantId=TEXT("Original");V.BoundsMin=Mesh->GetBoundingBox().Min;V.BoundsMax=Mesh->GetBoundingBox().Max;
  Position.Z=-V.BoundsMin.Z;V.LocalPosition=Position;V.LocalYaw=Yaw;S.Variants.Add(V);L.Slots.Add(S);return true;
 };
 auto Node=[](FHansaCompoundLayout& L,const TCHAR* Id,FVector P,FName Purpose,TArray<FString> Links,bool Road=false)
 {FHansaCompoundNode N;N.NodeId=Id;N.Position=P;N.Purpose=Purpose;N.Links=MoveTemp(Links);N.bRoadEntrance=Road;L.Nodes.Add(N);};
 const FString Kit=TEXT("/Game/Mesh/labour-housing-kit/Meshes_R08/SM_LabourKit_");
 for(int32 I=0;I<2;++I)
 {
  FHansaCompoundLayout L;L.LayoutId=I?TEXT("WorkYard"):TEXT("Compact");L.Weight=1;
  const FString House=TEXT("/Game/Mesh/lubeck-urban-housing-r01/Meshes/SM_ArtisanBrick_")+FString(I?TEXT("Stepped"):TEXT("Plain"));
  if(!Slot(L,TEXT("Principal"),House,FVector(200,-95,0),0,TEXT("Dwelling"),true,TEXT("FrontDoor")))return 1;
  if(!Slot(L,TEXT("RearBuilding"),Kit+(I?TEXT("YardWorkshop"):TEXT("StorageShed")),I?FVector(-550,-90,0):FVector(-615,-130,0),I?90:0,I?TEXT("Workshop"):TEXT("YardProp"),false,TEXT("RearDoor")))return 1;
  for(int32 J=0;J<2;++J)for(int32 Side:{-1,1})
   if(!Slot(L,FString::Printf(TEXT("SideFence%d_%d"),J,Side),Kit+TEXT("Fence2m"),FVector(-660+J*214,Side*385,0),90,TEXT("Fence")))return 1;
  for(int32 J=0;J<3;++J)
   if(!Slot(L,FString::Printf(TEXT("BackFence%d"),J),Kit+TEXT("Fence2m"),FVector(-785,-278+J*214,0),0,TEXT("Fence")))return 1;
  Node(L,TEXT("Road"),FVector(800,275,0),TEXT("Entrance"),{TEXT("FrontWalk")},true);
  Node(L,TEXT("FrontWalk"),FVector(735,275,0),TEXT("Delivery"),{TEXT("FrontDoor"),TEXT("Yard")});
  Node(L,TEXT("FrontDoor"),FVector(735,-262,0),TEXT("Entrance"),{});
  Node(L,TEXT("Yard"),FVector(-315,275,0),TEXT("YardWork"),{TEXT("RearWalk")});
  if(I)
  {
   Node(L,TEXT("RearWalk"),FVector(-618,275,0),TEXT("Entrance"),{TEXT("RearDoor")});
   Node(L,TEXT("RearDoor"),FVector(-618,225,0),TEXT("Entrance"),{});
  }
  else
  {
   Node(L,TEXT("RearWalk"),FVector(-315,-130,0),TEXT("Entrance"),{TEXT("RearDoor")});
   Node(L,TEXT("RearDoor"),FVector(-375,-130,0),TEXT("Entrance"),{});
  }
  D->Layouts.Add(L);
 }
 TArray<FHansaDefinitionValidationIssue> Issues;D->ValidateDefinition(Issues);
 for(const auto& I:Issues)UE_LOG(LogTemp,Error,TEXT("%s: %s"),*I.PropertyPath,*I.Cause.ToString());
 if(!Issues.IsEmpty())return 1;
 const auto* Source=LoadObject<UHansaBuildingDefinition>(nullptr,TEXT("/Game/Hansa/Core/Buildings/DA_Building_Residence_Artisan"));if(!Source)return 1;
 FString Error;TStrongObjectPtr<UHansaBuildingDefinition> B(Hansa::Editor::Compounds::CreateBindingDraft(*Source,*D,TEXT("Building.Residence.Artisan.Plot"),Error));
 if(!B){UE_LOG(LogTemp,Error,TEXT("%s"),*Error);return 1;}
 B->DisplayName=FText::ChangeKey(TEXT("HansaDefinitions"),B->StableDefinitionId,FText::FromString(TEXT("Artisan house")));
 B->LocalizationKey=FName(*B->StableDefinitionId);B->bUpgradeOnly=false;B->bShowInConstructionMenu=true;
 B->ConstructionPresentationPurpose=FText::FromString(TEXT("Artisan home; randomly chooses a compact yard or workshop yard"));
 B->PresentationMesh=D->Layouts[0].Slots[0].Variants[0].Mesh;B->RefreshContentHash();
 auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 Registry.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")},true);TArray<FAssetData> Assets;Registry.GetAssetsByPath(TEXT("/Game/Hansa/Core"),Assets,true,false);
 TArray<const UHansaDefinitionBase*> Definitions;for(const auto& A:Assets)if(auto* Def=Cast<UHansaDefinitionBase>(A.GetAsset()))Definitions.Add(Def);
 const auto Before=FHansaEconomicDefinitionCompiler::Compile(Definitions);if(!Before.IsValid()||Before.Registry.GetRegistryHash()!=0xA2B48E339BEE2EFAULL){UE_LOG(LogTemp,Error,TEXT("Expected reviewed v23 baseline before additive artisan promotion."));return 1;}
 Definitions.Add(D.Get());Definitions.Add(B.Get());
 const auto After=FHansaEconomicDefinitionCompiler::Compile(Definitions);Algo::Reverse(Definitions);const auto Reverse=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 for(const auto& I:After.Issues)UE_LOG(LogTemp,Error,TEXT("%s: %s"),*I.PropertyPath,*I.Cause.ToString());
 if(!After.IsValid()||!Reverse.IsValid()||After.Registry.GetRegistryHash()!=Reverse.Registry.GetRegistryHash())return 1;
 for(const auto& Old:Before.DefinitionHashes){const auto* New=After.DefinitionHashes.FindByPredicate([&](const auto& R){return R.StableId==Old.StableId;});if(!New||New->ContentHash!=Old.ContentHash)return 1;}
 auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("schemaVersion"),1);Root->SetNumberField(TEXT("catalogVersion"),24);Root->SetNumberField(TEXT("previousCatalogVersion"),23);
 Root->SetStringField(TEXT("previousRegistryHash"),FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(Before.Registry.GetRegistryHash())));
 Root->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(After.Registry.GetRegistryHash())));
 TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& R:After.DefinitionHashes){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stableId"),R.StableId);J->SetStringField(TEXT("classPath"),R.DefinitionClassPath);J->SetStringField(TEXT("contentHash"),FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(R.ContentHash)));Rows.Add(MakeShared<FJsonValueObject>(J));}Root->SetArrayField(TEXT("definitions"),Rows);
 FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
 if(Apply)
 {
  if(!SavePlotAsset(D.Get(),CompoundPath))return 1;
  B->ResidentialCompound=D.Get();B->RefreshContentHash();if(!SavePlotAsset(B.Get(),BuildingPath))return 1;
  if(!FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("Tests/Golden/economic_catalog_v24.json"))))return 1;
  Hansa::Editor::Compounds::ExportInterchange(*D,Json);
  if(!FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("SourceArt/Generated/Buildings/LubeckUrbanHousing_20260916/artisan-plots.json"))))return 1;
 }
 UE_LOG(LogTemp,Display,TEXT("ARTISAN_PLOTS_VALIDATED apply=%d hash=%016llX unchanged=%d"),Apply,static_cast<unsigned long long>(After.Registry.GetRegistryHash()),Before.DefinitionHashes.Num());return 0;
}
