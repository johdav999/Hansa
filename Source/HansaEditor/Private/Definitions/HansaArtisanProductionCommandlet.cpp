#include "Definitions/HansaArtisanProductionCommandlet.h"
#include "Definitions/HansaArtisanProductionDraft.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "Algo/Reverse.h"
#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"

#include "HansaArtisanProductionPromotion.inl"

UHansaArtisanProductionCommandlet::UHansaArtisanProductionCommandlet()
{ IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true; }

int32 UHansaArtisanProductionCommandlet::Main(const FString& Params)
{
 IAssetRegistry& Assets=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 if(FParse::Param(*Params,TEXT("PromoteReviewed")) || FParse::Param(*Params,TEXT("VerifyApproved")))
  return PromoteArtisanProduction(Assets,FParse::Param(*Params,TEXT("VerifyApproved")));
 if(FParse::Param(*Params,TEXT("VerifyStage")))
 {
  Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionV1")},true);
  TArray<FAssetData> StagedRows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionV1"),StagedRows,true,false);
  TArray<const UHansaDefinitionBase*> Loaded;
  auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Entries;
  for(const auto& Row:StagedRows)if(const auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))
  {
   Loaded.Add(D);auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("stableId"),D->StableDefinitionId);
   E->SetStringField(TEXT("hash"),FString::Printf(TEXT("%016llX"),D->ComputeDeterministicContentHash()));
   Entries.Add(MakeShared<FJsonValueObject>(E));
  }
  const auto Checked=FHansaEconomicDefinitionCompiler::Compile(Loaded);
  Report->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),Checked.Registry.GetRegistryHash()));
  Report->SetArrayField(TEXT("definitions"),Entries);FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("Docs/Development/ArtisanProduction/reloaded.json")));
  UE_LOG(LogTemp,Display,TEXT("Reloaded artisan catalog valid=%d count=%d hash=%016llX"),Checked.IsValid(),Loaded.Num(),Checked.Registry.GetRegistryHash());
  for(const auto& I:Checked.Issues)UE_LOG(LogTemp,Warning,TEXT("%s %s"),*I.PropertyPath,*I.Cause.ToString());
  return Checked.IsValid()?0:9;
 }
 Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")},true);
 TArray<FAssetData> Rows; Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"),Rows,true,false);
 TArray<const UHansaDefinitionBase*> Baseline;
 TArray<TStrongObjectPtr<UHansaDefinitionBase>> Draft;
 for(const auto& Row:Rows) if(const auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))
 { Baseline.Add(D); Draft.Emplace(DuplicateObject<UHansaDefinitionBase>(D,GetTransientPackage())); }
 const auto Before=FHansaEconomicDefinitionCompiler::Compile(Baseline);
 if(!Before.IsValid()) return 1;
 FString Error;
 if(!Hansa::Editor::ArtisanProduction::ApplyDraft(Draft,Error))
 { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 2; }
 const bool BindModels=FParse::Param(*Params,TEXT("BindModels"));
 if(BindModels)
 {
  for(const TCHAR* Kind:{TEXT("CharcoalBurner"),TEXT("Smithy"),TEXT("Tannery"),TEXT("Shoemaker")})
  {
   const FString Name=TEXT("SM_")+FString(Kind);
   const FSoftObjectPath MeshPath(TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionModelsV1/")+Name+TEXT(".")+Name);
   if(!Cast<UStaticMesh>(MeshPath.TryLoad())) { UE_LOG(LogTemp,Error,TEXT("Missing staged workshop mesh %s"),*MeshPath.ToString()); return 8; }
   for(const auto& D:Draft) if(D->StableDefinitionId==TEXT("Building.")+FString(Kind))
   {
    D->PresentationMesh=TSoftObjectPtr<UStaticMesh>(MeshPath);
    D->RefreshContentHash();
   }
  }
 }
 TArray<const UHansaDefinitionBase*> Pointers; for(const auto& D:Draft) Pointers.Add(D.Get());
 const auto After=FHansaEconomicDefinitionCompiler::Compile(Pointers);
 for(const auto& I:After.Issues) UE_LOG(LogTemp,Warning,TEXT("%s: %s"),*I.PropertyPath,*I.Cause.ToString());
 if(!After.IsValid()) return 3;
 Algo::Reverse(Pointers);
 const auto Reverse=FHansaEconomicDefinitionCompiler::Compile(Pointers);
 if(!Reverse.IsValid()||Reverse.Registry.GetRegistryHash()!=After.Registry.GetRegistryHash()) return 4;
 const FString OutDir=FPaths::ProjectDir()/TEXT("Docs/Development/ArtisanProduction");
 IFileManager::Get().MakeDirectory(*OutDir,true);
 const bool Stage=FParse::Param(*Params,TEXT("Stage"));
 // Never overwrite a previous review snapshot on an uncertain/repeated run.
 const FString StageRoot=TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionV1");
 if(Stage && IFileManager::Get().DirectoryExists(*(FPaths::ProjectContentDir()/TEXT("Hansa/Generated/Staging/ArtisanProductionV1"))))
 { UE_LOG(LogTemp,Error,TEXT("Staging snapshot already exists; inspect it before creating a revision.")); return 5; }
 auto Manifest=MakeShared<FJsonObject>();
 Manifest->SetStringField(TEXT("status"),Stage?TEXT("staged-economy-draft"):TEXT("validated-transient-economy-draft"));
 Manifest->SetStringField(TEXT("baseRegistryHash"),FString::Printf(TEXT("%016llX"),Before.Registry.GetRegistryHash()));
 Manifest->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),After.Registry.GetRegistryHash()));
 Manifest->SetStringField(TEXT("presentationStatus"),BindModels?TEXT("Staged workshop meshes bound; explicit production approval and in-game UAT required."):TEXT("Inherited meshes are draft stand-ins. Run with -BindModels after importing verified workshops."));
 TArray<TSharedPtr<FJsonValue>> Entries;
 for(const auto& D:Draft)
 {
  auto E=MakeShared<FJsonObject>(); E->SetStringField(TEXT("stableId"),D->StableDefinitionId);
  E->SetStringField(TEXT("hash"),FString::Printf(TEXT("%016llX"),D->ComputeDeterministicContentHash()));
  const auto* Old=Baseline.FindByPredicate([&](const auto* X){return X->StableDefinitionId==D->StableDefinitionId;});
  E->SetStringField(TEXT("change"),!Old?TEXT("added"):(*Old)->ComputeDeterministicContentHash()==D->ComputeDeterministicContentHash()?TEXT("unchanged"):TEXT("modified"));
  TArray<TSharedPtr<FJsonValue>> Changes;
  for(TFieldIterator<FProperty> It(D->GetClass());It;++It)
  {
   const FProperty* P=*It;
   if(P->HasAnyPropertyFlags(CPF_Transient)||!P->HasAnyPropertyFlags(CPF_Edit))continue;
   FString Current,Previous;
   P->ExportText_InContainer(0,Current,D.Get(),D.Get(),D.Get(),PPF_None);
   if(Old) P->ExportText_InContainer(0,Previous,*Old,*Old,const_cast<UHansaDefinitionBase*>(*Old),PPF_None);
   if(Old && Current==Previous)continue;
   auto Change=MakeShared<FJsonObject>();Change->SetStringField(TEXT("property"),P->GetName());
   Change->SetStringField(TEXT("before"),Previous);Change->SetStringField(TEXT("after"),Current);
   Changes.Add(MakeShared<FJsonValueObject>(Change));
  }
  E->SetArrayField(TEXT("propertyChanges"),Changes);
  if(Stage)
  {
   const FString Name=TEXT("DA_")+D->StableDefinitionId.Replace(TEXT("."),TEXT("_"));
   const FString PackagePath=StageRoot+TEXT("/")+Name;
   UPackage* Package=CreatePackage(*PackagePath);
   auto* Copy=DuplicateObject<UHansaDefinitionBase>(D.Get(),Package,*Name); Copy->SetFlags(RF_Public|RF_Standalone);
   FSavePackageArgs Save; Save.TopLevelFlags=RF_Public|RF_Standalone; Save.SaveFlags=SAVE_NoError;
   if(!UPackage::SavePackage(Package,Copy,*FPackageName::LongPackageNameToFilename(PackagePath,FPackageName::GetAssetPackageExtension()),Save)) return 6;
   E->SetStringField(TEXT("asset"),Copy->GetPathName());
  }
  Entries.Add(MakeShared<FJsonValueObject>(E));
 }
 Manifest->SetArrayField(TEXT("definitions"),Entries);
 FString Json; FJsonSerializer::Serialize(Manifest,TJsonWriterFactory<>::Create(&Json));
 if(!FFileHelper::SaveStringToFile(Json,*(OutDir/TEXT("candidate.json")))) return 7;
 UE_LOG(LogTemp,Display,TEXT("Artisan production draft validated: %d definitions, %016llX"),Draft.Num(),After.Registry.GetRegistryHash());
 return 0;
}
