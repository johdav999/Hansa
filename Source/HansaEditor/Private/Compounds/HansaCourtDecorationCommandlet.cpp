#include "Compounds/HansaCourtDecorationCommandlet.h"
#include "Compounds/HansaCompoundAuthoring.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonSerializer.h"
#include "Algo/Reverse.h"

UHansaCourtDecorationCommandlet::UHansaCourtDecorationCommandlet()
{ IsClient=false;IsEditor=true;LogToConsole=true; }
int32 UHansaCourtDecorationCommandlet::Main(const FString& Params)
{
 FString Source;FParse::Value(*Params,TEXT("SourceDir="),Source);if(Source.IsEmpty())return 1;
 const bool Apply=FParse::Param(*Params,TEXT("Apply"));
 const bool Compact=FParse::Param(*Params,TEXT("Compact"));
 TArray<TStrongObjectPtr<UHansaResidentialCompoundDefinition>> Drafts;
 TArray<UHansaResidentialCompoundDefinition*> Targets;
 for(const FString Family:{TEXT("NarrowGang"),TEXT("SharedCourt"),TEXT("CornerCourt"),TEXT("CraftCourt")})
 {
  FString Json,Error;if(!FFileHelper::LoadFileToString(Json,*(Source/TEXT("definitions")/(Family+TEXT(".json")))))return 1;
  auto* Draft=Hansa::Editor::Compounds::ImportDraft(Json,Error);
  if(!Draft){UE_LOG(LogTemp,Error,TEXT("%s: %s"),*Family,*Error);return 1;}
  auto* Target=LoadObject<UHansaResidentialCompoundDefinition>(nullptr,*(TEXT("/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_")+Family));
  if(!Target||Target->StableDefinitionId!=Draft->StableDefinitionId)return 1;
  if(Compact) { if(Draft->FootprintWidthCells>Target->FootprintWidthCells||Draft->FootprintHeightCells>Target->FootprintHeightCells)return 1; }
  else if(Target->FootprintWidthCells!=Draft->FootprintWidthCells||Target->FootprintHeightCells!=Draft->FootprintHeightCells)return 1;
  // Preserve native localization identity; this revision changes layouts and revision only.
  auto* Candidate=DuplicateObject<UHansaResidentialCompoundDefinition>(Target,GetTransientPackage());
  if(Compact) { Candidate->FootprintWidthCells=Draft->FootprintWidthCells;Candidate->FootprintHeightCells=Draft->FootprintHeightCells;Candidate->BoundsMin=Draft->BoundsMin;Candidate->BoundsMax=Draft->BoundsMax; }
  Candidate->Layouts=Draft->Layouts;Candidate->AuthoredRevision=Draft->AuthoredRevision;Candidate->RefreshContentHash();
  Draft=Candidate;Drafts.Emplace(Draft);Targets.Add(Target);
  const FString Baseline=Source/TEXT("baseline")/(Family+TEXT(".json"));
  if(!FPaths::FileExists(Baseline))
  {
   IFileManager::Get().MakeDirectory(*(Source/TEXT("baseline")),true);
   if(!Hansa::Editor::Compounds::ExportInterchange(*Target,Json)||!FFileHelper::SaveStringToFile(Json,*Baseline))return 1;
  }
  for(const auto& L:Draft->Layouts)for(uint64 Id=1;Id<=64;++Id)
   if(!Draft->Compose(UHansaResidentialCompoundDefinition::ParcelSeed(TEXT("City.Lubeck"),Id,1),L.DevelopmentStage,L.Context,TEXT("District.Lubeck.LateMedieval")).IsValid())return 1;
 }
 auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 Registry.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")},true);
 TArray<FAssetData> Assets;Registry.GetAssetsByPath(TEXT("/Game/Hansa/Core"),Assets,true,false);
 TArray<const UHansaDefinitionBase*> Definitions;
 for(const auto& Asset:Assets)if(const auto* D=Cast<UHansaDefinitionBase>(Asset.GetAsset()))Definitions.Add(D);
 const auto Before=FHansaEconomicDefinitionCompiler::Compile(Definitions);if(!Before.IsValid())return 1;
 for(auto& D:Definitions)for(int32 I=0;I<Targets.Num();++I)if(D==Targets[I])D=Drafts[I].Get();
 TArray<TStrongObjectPtr<UHansaBuildingDefinition>> Bindings;
 TArray<UHansaBuildingDefinition*> BindingTargets;
 for(auto& D:Definitions)if(const auto* B=Cast<UHansaBuildingDefinition>(D))
  for(int32 I=0;I<Targets.Num();++I)if(B->LoadResidentialCompound()==Targets[I])
  {
   auto* Copy=DuplicateObject<UHansaBuildingDefinition>(B,GetTransientPackage());Copy->ResidentialCompound=Drafts[I].Get();
   if(Compact && (Copy->FootprintWidthCells!=Drafts[I]->FootprintWidthCells||Copy->FootprintHeightCells!=Drafts[I]->FootprintHeightCells))
   { Copy->FootprintWidthCells=Drafts[I]->FootprintWidthCells;Copy->FootprintHeightCells=Drafts[I]->FootprintHeightCells;++Copy->AuthoredRevision; }
   Copy->RefreshContentHash();BindingTargets.Add(const_cast<UHansaBuildingDefinition*>(B));Bindings.Emplace(Copy);D=Copy;break;
  }
 const auto After=FHansaEconomicDefinitionCompiler::Compile(Definitions);Algo::Reverse(Definitions);
 const auto Reverse=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 if(!After.IsValid()||!Reverse.IsValid()||After.Registry.GetRegistryHash()!=Reverse.Registry.GetRegistryHash())return 1;
 int32 Changed=0;
 for(const auto& Row:After.DefinitionHashes)
 {
  const auto* Old=Before.DefinitionHashes.FindByPredicate([&](const auto& R){return R.StableId==Row.StableId;});
  if(!Old)return 1;
  if(Old->ContentHash!=Row.ContentHash){++Changed;if(!Row.StableId.StartsWith(TEXT("Compound.Laborer."))&&!Row.StableId.StartsWith(TEXT("Building.Residence.Laborer.")))return 1;}
 }
 if(Changed!=16&&Changed!=0)return 1;
 auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("schemaVersion"),1);Root->SetNumberField(TEXT("catalogVersion"),Compact?27:21);Root->SetNumberField(TEXT("previousCatalogVersion"),Compact?26:20);
 Root->SetStringField(TEXT("previousRegistryHash"),Compact?TEXT("0ECFB6BA46CD1344"):TEXT("4A86F28719E21627"));
 Root->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(After.Registry.GetRegistryHash())));
 TArray<TSharedPtr<FJsonValue>> Rows;
 for(const auto& Row:After.DefinitionHashes)
 {
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("classPath"),Row.DefinitionClassPath);J->SetStringField(TEXT("stableId"),Row.StableId);
  J->SetStringField(TEXT("contentHash"),FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(Row.ContentHash)));Rows.Add(MakeShared<FJsonValueObject>(J));
 }
 Root->SetArrayField(TEXT("definitions"),Rows);FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
 if(!FFileHelper::SaveStringToFile(Json,*(Source/(Compact?TEXT("catalog-v27.json"):TEXT("catalog-v21.json")))))return 1;
 if(Apply)for(int32 I=0;I<Targets.Num();++I)
 {
  auto* Target=Targets[I];Target->Modify();
  for(TFieldIterator<FProperty> P(Target->GetClass());P;++P)if(!P->HasAnyPropertyFlags(CPF_Transient))P->CopyCompleteValue_InContainer(Target,Drafts[I].Get());
  Target->RefreshContentHash();Target->MarkPackageDirty();
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  if(!UPackage::SavePackage(Target->GetPackage(),Target,*FPackageName::LongPackageNameToFilename(Target->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),Args))return 1;
 }
 if(Apply && Compact)for(int32 I=0;I<Bindings.Num();++I)
 {
  auto* Target=BindingTargets[I];Target->Modify();
  Target->FootprintWidthCells=Bindings[I]->FootprintWidthCells;Target->FootprintHeightCells=Bindings[I]->FootprintHeightCells;Target->AuthoredRevision=Bindings[I]->AuthoredRevision;
  Target->RefreshContentHash();Target->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  if(!UPackage::SavePackage(Target->GetPackage(),Target,*FPackageName::LongPackageNameToFilename(Target->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),Args))return 1;
 }
 UE_LOG(LogTemp,Display,TEXT("COURT_DECORATION_VALIDATED changed=%d apply=%d hash=%016llX"),Changed,Apply,static_cast<unsigned long long>(After.Registry.GetRegistryHash()));return 0;
}