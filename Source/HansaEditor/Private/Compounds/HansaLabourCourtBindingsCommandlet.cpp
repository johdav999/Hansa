#include "Compounds/HansaLabourCourtBindingsCommandlet.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

UHansaLabourCourtBindingsCommandlet::UHansaLabourCourtBindingsCommandlet()
{ IsClient=false; IsEditor=true; LogToConsole=true; }
int32 UHansaLabourCourtBindingsCommandlet::Main(const FString& Params)
{
 const bool bApply=FParse::Param(*Params,TEXT("Apply"));
 const auto* Source=LoadObject<UHansaBuildingDefinition>(nullptr,TEXT("/Game/Hansa/Core/Buildings/DA_Building_Residence_Laborer.DA_Building_Residence_Laborer"));
 if(!Source){ UE_LOG(LogTemp,Error,TEXT("Legacy source residence missing"));return 1; }
 const TArray<FString> Families={TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")};
 const TArray<FString> Labels={TEXT("Lane court"),TEXT("Shared court"),TEXT("Corner court"),TEXT("Craft court")};
 TArray<TStrongObjectPtr<UHansaBuildingDefinition>> Drafts;
 TArray<const UHansaDefinitionBase*> Definitions;
 UAssetManager& Manager=UAssetManager::Get();
 TArray<FPrimaryAssetTypeInfo> Types;Manager.GetPrimaryAssetTypeInfoList(Types);
 for(const auto& Type:Types){TArray<FPrimaryAssetId> Ids;Manager.GetPrimaryAssetIdList(Type.PrimaryAssetType,Ids);
  for(const auto& Id:Ids)if(auto* D=Cast<UHansaDefinitionBase>(Manager.GetPrimaryAssetPath(Id).TryLoad()))Definitions.Add(D);}
 for(int32 F=0;F<Families.Num();++F)
 {
  const FString Path=TEXT("/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_")+Families[F];
  auto* Compound=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,*Path);
  if(!Compound){UE_LOG(LogTemp,Error,TEXT("Reviewed compound must be explicitly promoted first: %s"),*Path);return 1;}
  for(int32 Stage=1;Stage<=3;++Stage)
  {
   const FString Id=FString::Printf(TEXT("Building.Residence.Laborer.%s.Stage%d"),*Families[F],Stage);
   const FString Package=TEXT("/Game/Hansa/Core/Buildings/LabourCourts/DA_")+Id.Replace(TEXT("."),TEXT("_"));
   if(FPackageName::DoesPackageExist(Package)){UE_LOG(LogTemp,Error,TEXT("Refusing overwrite: %s"),*Package);return 1;}
   FString Error;auto* B=Hansa::Editor::Compounds::CreateBindingDraft(*Source,*Compound,Id,Error);
   if(!B){UE_LOG(LogTemp,Error,TEXT("%s"),*Error);return 1;}
   Drafts.Emplace(B);B->CompoundStage=Stage;B->CompoundDistrictId=TEXT("District.Lubeck.LateMedieval");
   const FString Label=Labels[F]+(Stage==1?TEXT(""):Stage==2?TEXT(" - established"):TEXT(" - developed"));
   B->DisplayName=FText::ChangeKey(TEXT("HansaDefinitions"),Id,FText::FromString(Label));B->LocalizationKey=FName(*Id);
   B->bShowInConstructionMenu=true;B->bUpgradeOnly=Stage>1;B->ConstructionMenuOrder=10+F;
   B->ConstructionPresentationPurpose=FText::FromString(TEXT("Laborer homes with a shared yard; three development stages"));
   // Capacity and costs are explicit per parcel, never inferred from decorative mesh count.
   // Preserve the existing laborer capacity and cost contract in this visual integration.
   B->ResidenceCapacity=Source->ResidenceCapacity;
   B->UpgradeTargetBuildingId=Stage<3?FString::Printf(TEXT("Building.Residence.Laborer.%s.Stage%d"),*Families[F],Stage+1):FString();
   B->RefreshContentHash();Definitions.Add(B);
  }
 }
 const auto Result=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 if(!Result.IsValid()){for(const auto& E:Result.Issues)UE_LOG(LogTemp,Error,TEXT("%s: %s"),*E.PropertyPath,*E.Cause.ToString());return 1;}
 if(bApply)for(auto& D:Drafts)
 {
  const FString Name=TEXT("DA_")+D->StableDefinitionId.Replace(TEXT("."),TEXT("_"));
  const FString Path=TEXT("/Game/Hansa/Core/Buildings/LabourCourts/")+Name;
  UPackage* Package=CreatePackage(*Path);D->Rename(*Name,Package,REN_DontCreateRedirectors);D->SetFlags(RF_Public|RF_Standalone);
  FAssetRegistryModule::AssetCreated(D.Get());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  if(!UPackage::SavePackage(Package,D.Get(),*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args))return 1;
 }
 UE_LOG(LogTemp,Display,TEXT("LABOUR_BINDINGS_VALIDATED 12 new definitions; apply=%d; legacy unchanged"),bApply);return 0;
}
