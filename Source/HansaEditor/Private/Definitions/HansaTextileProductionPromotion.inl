// Explicit normal-game integration requested by the user on 2026-09-19.
// Retain the reviewed staging snapshot and back up overwritten packages.
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/UObjectHash.h"
#include "Misc/CommandLine.h"

namespace
{
int32 PromoteTextileProduction(IAssetRegistry& Assets, bool VerifyOnly)
{
 Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core"),TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2"),TEXT("/Game/Hansa/Generated/Staging/TextileProductionModelsV1")},true);
 auto Load=[&](const TCHAR* Root) {
  TArray<FAssetData> Rows;Assets.GetAssetsByPath(FName(Root),Rows,true,false);
  TArray<const UHansaDefinitionBase*> Result;
  for(const auto& Row:Rows)if(auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))Result.Add(D);
  return Result;
 };
 auto Base=Load(TEXT("/Game/Hansa/Core"));
 const auto Before=FHansaEconomicDefinitionCompiler::Compile(Base);
 if(!Before.IsValid())return 20;
 if(VerifyOnly)
 {
  auto Reverse=Base;Algo::Reverse(Reverse);
  if(Base.Num()!=181||FHansaEconomicDefinitionCompiler::Compile(Reverse).Registry.GetRegistryHash()!=Before.Registry.GetRegistryHash())return 21;
  for(const auto* D:Base)
   if(D->PresentationMesh.ToSoftObjectPath().ToString().Contains(TEXT("/Generated/Staging/")))return 22;
  Assets.ScanPathsSynchronous({TEXT("/Game/Mesh/hansa-textile-production")},true);
  TArray<FAssetData> ApprovedMedia;Assets.GetAssetsByPath(TEXT("/Game/Mesh/hansa-textile-production"),ApprovedMedia,true,false);
  if(ApprovedMedia.IsEmpty())return 36;
  for(const auto& Row:ApprovedMedia)
  {
   TArray<FName> Dependencies;Assets.GetDependencies(Row.PackageName,Dependencies);
   for(const FName Dependency:Dependencies)
    if(Dependency.ToString().Contains(TEXT("/Generated/Staging/"))){UE_LOG(LogTemp,Error,TEXT("Staging dependency: %s -> %s"),*Row.PackageName.ToString(),*Dependency.ToString());return 37;}
  }
  auto Root=MakeShared<FJsonObject>();
  Root->SetNumberField(TEXT("schemaVersion"),1);Root->SetNumberField(TEXT("catalogVersion"),30);
  Root->SetNumberField(TEXT("previousCatalogVersion"),29);Root->SetStringField(TEXT("previousRegistryHash"),TEXT("0698066FA49B59A2"));
  Root->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),Before.Registry.GetRegistryHash()));
  Root->SetStringField(TEXT("approval"),TEXT("User explicitly requested: promote these, 2026-09-19, following the textile playable review."));
  Root->SetStringField(TEXT("saveCompatibility"),TEXT("Changed economics require New Game. Existing catalog-v29 saves are preserved and rejected by the ordinary catalog hash guard."));
  TArray<TSharedPtr<FJsonValue>> Entries;
  for(const auto& D:Before.DefinitionHashes){auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("stableId"),D.StableId);E->SetStringField(TEXT("classPath"),D.DefinitionClassPath);E->SetStringField(TEXT("contentHash"),FString::Printf(TEXT("%016llX"),D.ContentHash));Entries.Add(MakeShared<FJsonValueObject>(E));}
  Root->SetArrayField(TEXT("definitions"),Entries);FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
  if(!FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("Tests/Golden/economic_catalog_v30.json"))))return 23;
  UE_LOG(LogTemp,Display,TEXT("TEXTILE_ACCEPTED_RELOAD hash=%016llX definitions=%d"),Before.Registry.GetRegistryHash(),Base.Num());return 0;
 }
 if(Before.Registry.GetRegistryHash()!=0x0698066FA49B59A2ULL)return 24;
 auto Draft=Load(TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2"));
 const auto Reviewed=FHansaEconomicDefinitionCompiler::Compile(Draft);
 if(!Reviewed.IsValid()||Reviewed.Registry.GetRegistryHash()!=0x386F8F6145FE5135ULL){UE_LOG(LogTemp,Error,TEXT("Reviewed valid=%d count=%d hash=%016llX"),Reviewed.IsValid(),Draft.Num(),Reviewed.Registry.GetRegistryHash());for(const auto& I:Reviewed.Issues)UE_LOG(LogTemp,Warning,TEXT("%s %s"),*I.PropertyPath,*I.Cause.ToString());return 25;}
 const FString SourceRoot=TEXT("/Game/Hansa/Generated/Staging/TextileProductionModelsV1");
 const FString TargetRoot=TEXT("/Game/Mesh/hansa-textile-production");
 TArray<FAssetData> Media;Assets.GetAssetsByPath(*SourceRoot,Media,true,false);
 TMap<UObject*,UObject*> Replacements;TArray<UObject*> SaveObjects;
 for(const auto& Row:Media)
 {
  const FString Path=TargetRoot+Row.PackageName.ToString().Mid(SourceRoot.Len());
  if(FPackageName::DoesPackageExist(Path))return 26;
  UObject* Source=Row.GetAsset();if(!Source)return 27;
  UObject* Copy=DuplicateObject(Source,CreatePackage(*Path),Row.AssetName);
  Copy->SetFlags(RF_Public|RF_Standalone);Replacements.Add(Source,Copy);SaveObjects.Add(Copy);
 }
 if(SaveObjects.IsEmpty())return 28;
 for(auto* Copy:SaveObjects)
 {
  TArray<UObject*> Inner;GetObjectsWithOuter(Copy,Inner,EGetObjectsFlags::IncludeNestedObjects);Inner.Add(Copy);
  for(auto* Object:Inner){FArchiveReplaceObjectRef<UObject> Replace(Object,Replacements,EArchiveReplaceObjectFlags::IgnoreOuterRef|EArchiveReplaceObjectFlags::IgnoreArchetypeRef);}
 }
 // Merge into the current accepted regional economy; never replace it with the older review snapshot.
 TArray<TStrongObjectPtr<UHansaDefinitionBase>> Merged;
 for(const auto* D:Base)Merged.Emplace(DuplicateObject<UHansaDefinitionBase>(D,GetTransientPackage()));
 FString MergeError;
 if(!Hansa::Editor::TextileProduction::ApplyDraft(Merged,MergeError)){UE_LOG(LogTemp,Error,TEXT("%s"),*MergeError);return 34;}
 Draft.Reset();
 for(auto& D:Merged)
 {
  const bool bNew=!Base.ContainsByPredicate([&](const auto* B){return B->StableDefinitionId==D->StableDefinitionId;});
  if(bNew)
  {
   auto Review=Load(TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2"));
   const auto* Found=Review.FindByPredicate([&](const auto* R){return R->StableDefinitionId==D->StableDefinitionId;});
   if(!Found)return 35;
   D.Reset(DuplicateObject<UHansaDefinitionBase>(*Found,GetTransientPackage()));
  }
  Draft.Add(D.Get());
 }
 TArray<TStrongObjectPtr<UHansaDefinitionBase>> Prepared;
 TArray<const UHansaDefinitionBase*> Proposed;
 for(const auto* D:Draft)
 {
  auto* Copy=DuplicateObject<UHansaDefinitionBase>(D,GetTransientPackage());Prepared.Emplace(Copy);
  const FString MeshPath=Copy->PresentationMesh.ToSoftObjectPath().ToString();
  if(MeshPath.StartsWith(SourceRoot))Copy->PresentationMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TargetRoot+MeshPath.Mid(SourceRoot.Len())));
  Copy->RefreshContentHash();Proposed.Add(Copy);
 }
 const auto Check=FHansaEconomicDefinitionCompiler::Compile(Proposed);
 if(!Check.IsValid())return 29;
 UE_LOG(LogTemp,Display,TEXT("TEXTILE_PROPOSED hash=%016llX"),Check.Registry.GetRegistryHash());
 if(FParse::Param(FCommandLine::Get(),TEXT("DryRun")))return 0;
 for(auto* Copy:Proposed)
 {
  const auto* Existing=Base.FindByPredicate([&](const auto* D){return D->StableDefinitionId==Copy->StableDefinitionId;});
  if(Existing&&(*Existing)->ComputeDeterministicContentHash()==Copy->ComputeDeterministicContentHash())continue;
  UHansaDefinitionBase* Target=nullptr;
  if(Existing)
  {
   Target=const_cast<UHansaDefinitionBase*>(*Existing);
   for(TFieldIterator<FProperty> It(Copy->GetClass());It;++It)
    if(It->HasAnyPropertyFlags(CPF_Edit)&&!It->HasAnyPropertyFlags(CPF_Transient))
     It->CopyCompleteValue_InContainer(Target,Copy);
  }
  else
  {
   const FString Folder=Copy->StableDefinitionId.StartsWith(TEXT("Good."))?TEXT("Goods"):Copy->StableDefinitionId.StartsWith(TEXT("Recipe."))?TEXT("Recipes"):Copy->StableDefinitionId.StartsWith(TEXT("Building."))?TEXT("Buildings"):TEXT("Needs");
   const FString Name=TEXT("DA_")+Copy->StableDefinitionId.Replace(TEXT("."),TEXT("_"));
   const FString Path=TEXT("/Game/Hansa/Core/")+Folder+TEXT("/")+Name;
   if(FPackageName::DoesPackageExist(Path))return 30;
   Target=DuplicateObject<UHansaDefinitionBase>(Copy,CreatePackage(*Path),*Name);Target->SetFlags(RF_Public|RF_Standalone);
  }
  Target->RefreshContentHash();SaveObjects.Add(Target);
 }
 // Prepare rollback evidence before any package is written.
 const FString BackupRoot=FPaths::ProjectSavedDir()/TEXT("GenerationJobs/TextileProductionV2/promotion-baseline");
 TArray<FString> Files,Backups;
 for(auto* O:SaveObjects)
 {
  const FString File=FPackageName::LongPackageNameToFilename(O->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Backup=BackupRoot/O->GetPackage()->GetName().Mid(6)+TEXT(".uasset");
  Files.Add(File);Backups.Add(Backup);
  if(IFileManager::Get().FileExists(*File))
  {
   if(IFileManager::Get().FileExists(*Backup))continue;
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
   if(IFileManager::Get().Copy(*Backup,*File)!=COPY_OK)return 32;
  }
 }
 for(int32 Index=0;Index<SaveObjects.Num();++Index)
 {
  auto* O=SaveObjects[Index];O->GetPackage()->FullyLoad();ResetLoaders(O->GetPackage());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
  if(!UPackage::SavePackage(O->GetPackage(),O,*Files[Index],Args))
  {
   for(int32 Restore=0;Restore<=Index;++Restore)
    if(IFileManager::Get().FileExists(*Backups[Restore]))IFileManager::Get().Copy(*Files[Restore],*Backups[Restore]);
    else IFileManager::Get().Delete(*Files[Restore]);
   return 33;
  }
 }
 UE_LOG(LogTemp,Display,TEXT("TEXTILE_PROMOTED packages=%d proposedHash=%016llX; verify in a fresh process"),SaveObjects.Num(),Check.Registry.GetRegistryHash());return 0;
}
}
