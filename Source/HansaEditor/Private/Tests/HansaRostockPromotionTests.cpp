#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "HAL/FileManager.h"

// Explicit offline promotion only; ordinary automation never writes production assets.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRostockPromotion,"Hansa.World.Rostock.PromoteReviewedQuarter",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRostockPromotion::RunTest(const FString&)
{
 const bool Promote=FParse::Param(FCommandLine::Get(),TEXT("PromoteRostockQuarter"));
 if(!Promote&&!FParse::Param(FCommandLine::Get(),TEXT("ReviewRostockPromotion"))){AddInfo(TEXT("Explicit review or promotion flag required; no assets changed."));return true;}
 const FString Source=TEXT("/Game/Hansa/Generated/Staging/Rostock_P31"),Destination=TEXT("/Game/Hansa/World/Cities/Rostock");
 TArray<UPackage*> Dirty;UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);
 if(!Dirty.IsEmpty()||GEditor->PlayWorld){AddError(TEXT("Promotion requires a clean offline editor."));return false;}
 auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);
 TArray<FName> Packages;TArray<FAssetData> Roots;Registry.GetAssetsByPath(FName(*Source),Roots,true);
 for(const auto& A:Roots)Packages.AddUnique(A.PackageName);
 if(Packages.Num()!=5){AddError(TEXT("Expected exactly five reviewed root packages."));return false;}
 for(int32 Index=0;Index<Packages.Num();++Index)
 {
  TArray<FName> Dependencies;Registry.GetDependencies(Packages[Index],Dependencies,UE::AssetRegistry::EDependencyCategory::Package);
  for(const auto D:Dependencies)if(D.ToString().StartsWith(TEXT("/Game/Hansa/Generated/Staging/")))Packages.AddUnique(D);
 }
 Packages.Sort(FNameLexicalLess());
 FString Review=TEXT("source\tdestination\n");
 for(const auto Package:Packages)
 {
  const FString Name=Package.ToString();
  const FString Target=Name.StartsWith(Source+TEXT("/"))?Name.Replace(*Source,*Destination):Destination/TEXT("Dependencies")/Name.RightChop(FString(TEXT("/Game/Hansa/Generated/Staging/")).Len());
  if(FPackageName::DoesPackageExist(Target)){AddError(TEXT("Refusing to overwrite production package: ")+Target);return false;}
  Review+=Name+TEXT("\t")+Target+TEXT("\n");
 }
 const FString ReviewDir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG13");IFileManager::Get().MakeDirectory(*ReviewDir,true);
 if(!FFileHelper::SaveStringToFile(Review,*(ReviewDir/TEXT("production-promotion-review.tsv"))))return false;
 if(!Promote){AddInfo(TEXT("Dependency review only; no assets changed."));return true;}
 TMap<UObject*,UObject*> Replacements;TArray<UObject*> Copies;FString Manifest=TEXT("source\tdestination\n");
 for(const auto Package:Packages)
 {
  const FString Name=Package.ToString();
  const FString Target=Name.StartsWith(Source+TEXT("/"))?Name.Replace(*Source,*Destination):Destination/TEXT("Dependencies")/Name.RightChop(FString(TEXT("/Game/Hansa/Generated/Staging/")).Len());
  if(FPackageName::DoesPackageExist(Target)){AddError(TEXT("Refusing to overwrite production package: ")+Target);return false;}
  TArray<FAssetData> Assets;Registry.GetAssetsByPackageName(Package,Assets);
  if(Assets.Num()!=1){AddError(TEXT("Expected one top-level asset in ")+Name);return false;}
  UObject* Original=Assets[0].GetAsset();if(!Original)return false;
  UObject* Copy=StaticDuplicateObject(Original,CreatePackage(*Target),Original->GetFName());if(!Copy)return false;
  Copy->SetFlags(RF_Public|RF_Standalone);Copies.Add(Copy);Replacements.Add(Original,Copy);Manifest+=Name+TEXT("\t")+Target+TEXT("\n");
 }
 // Duplicate first, then replace references across the complete closed staging dependency graph.
 for(auto* Copy:Copies)
 {
  FArchiveReplaceObjectRef<UObject> Replace(Copy,Replacements,EArchiveReplaceObjectFlags::IgnoreOuterRef|EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
  TArray<UObject*> Children;GetObjectsWithOuter(Copy->GetPackage(),Children,EGetObjectsFlags::IncludeNestedObjects);
  for(auto* Child:Children){FArchiveReplaceObjectRef<UObject> Nested(Child,Replacements,EArchiveReplaceObjectFlags::IgnoreOuterRef|EArchiveReplaceObjectFlags::IgnoreArchetypeRef);}
 }
 for(auto* Copy:Copies)
 {
  const FString File=FPackageName::LongPackageNameToFilename(Copy->GetPackage()->GetName(),Copy->IsA<UWorld>()?FPackageName::GetMapPackageExtension():FPackageName::GetAssetPackageExtension());
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
  if(!UPackage::SavePackage(Copy->GetPackage(),Copy,*File,Args)){AddError(TEXT("Failed saving ")+File);return false;}
 }
 const FString Evidence=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG13");IFileManager::Get().MakeDirectory(*Evidence,true);
 return FFileHelper::SaveStringToFile(Manifest,*(Evidence/TEXT("production-promotion.tsv")));
}
#endif
