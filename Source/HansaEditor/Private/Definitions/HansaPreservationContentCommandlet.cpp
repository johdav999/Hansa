#include "Definitions/HansaPreservationContentCommandlet.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Algo/Reverse.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
UHansaPreservationContentCommandlet::UHansaPreservationContentCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UHansaPreservationContentCommandlet::Main(const FString& Params)
{
 const bool Apply=FParse::Param(*Params,TEXT("Apply"));
 auto& Assets=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
 Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")},true);TArray<FAssetData> Rows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"),Rows,true,false);
 TArray<const UHansaDefinitionBase*> Definitions;TMap<FString,UHansaDefinitionBase*> ById;
 for(const auto& Row:Rows)if(auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset())){Definitions.Add(D);ById.Add(D->StableDefinitionId,D);}
 const auto Before=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 if(!Before.IsValid())return 1;
 auto Seeds=Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
 TArray<UHansaDefinitionBase*> Changed;
 for(const auto& Seed:Seeds)
 {
  const FString Id=Seed->StableDefinitionId;
  if(Id!=TEXT("Good.PreservedFish")&&Id!=TEXT("Recipe.SaltedCatch")&&Id!=TEXT("Building.Fishery.SaltingShed"))continue;
  if(ById.Contains(Id))continue;
  const FString Folder=Id.StartsWith(TEXT("Good."))?TEXT("Goods"):Id.StartsWith(TEXT("Recipe."))?TEXT("Recipes"):TEXT("Buildings");
  const FString Name=TEXT("DA_")+Id.Replace(TEXT("."),TEXT("_"));
  auto* Package=CreatePackage(*(TEXT("/Game/Hansa/Core/")+Folder+TEXT("/")+Name));
  auto* D=DuplicateObject<UHansaDefinitionBase>(Seed.Get(),Package,*Name);D->SetFlags(RF_Public|RF_Standalone);D->RefreshContentHash();
  ById.Add(Id,D);Definitions.Add(D);Changed.Add(D);
 }
 const auto Touch=[&](UHansaDefinitionBase* D,uint64 Old){if(D->ComputeDeterministicContentHash()!=Old){++D->AuthoredRevision;D->RefreshContentHash();Changed.AddUnique(D);}};
 auto* Fish=Cast<UHansaGoodDefinition>(ById.FindRef(TEXT("Good.Fish")));
 auto* Fishery=Cast<UHansaBuildingDefinition>(ById.FindRef(TEXT("Building.Fishery")));
 auto* Need=Cast<UHansaNeedDefinition>(ById.FindRef(TEXT("Need.Fish")));
 if(!Fish||!Fishery||!Need)return 1;
 uint64 Old=Fish->ContentHash;if(Fish->DisplayName.ToString()!=TEXT("Fresh fish"))Fish->DisplayName=FText::FromString(TEXT("Fresh fish"));Fish->bSpoilageEnabled=true;Fish->SpoilageBasisPointsPerDay=500;Touch(Fish,Old);
 Old=Fishery->ContentHash;Fishery->UpgradeTargetBuildingId=TEXT("Building.Fishery.SaltingShed");Touch(Fishery,Old);
 Old=Need->ContentHash;
 if(!Need->Alternatives.ContainsByPredicate([](const auto& V){return V.GoodId==TEXT("Good.PreservedFish");}))
 {FHansaNeedAlternative A;A.GoodId=TEXT("Good.PreservedFish");A.FulfillmentBasisPoints=10000;Need->Alternatives.Add(A);}
 Touch(Need,Old);
 for(const auto& Pair:ById)if(auto* City=Cast<UHansaCityMarketProfileDefinition>(Pair.Value))
 {
  Old=City->ContentHash;
  if(!City->Goods.ContainsByPredicate([](const auto& G){return G.GoodId==TEXT("Good.PreservedFish");}))
  {
   const auto* Fresh=City->Goods.FindByPredicate([](const auto& G){return G.GoodId==TEXT("Good.Fish");});if(!Fresh)return 1;
   auto G=*Fresh;G.GoodId=TEXT("Good.PreservedFish");G.InitialStockMilliUnits=City->bMarketOnly?18000:0;G.InitialPriceMilliMarks=3000;G.MinimumPriceMilliMarks=1500;G.MaximumPriceMilliMarks=12000;
   G.BackgroundProductionMilliUnitsPerUpdate=0;G.BackgroundCitizenDemandMilliUnitsPerUpdate=0;G.BackgroundIndustrialDemandMilliUnitsPerUpdate=0;G.ConfirmedIncomingSupplyMilliUnits=0;City->Goods.Add(G);
  }
  if(City->StableDefinitionId.Contains(TEXT("Rostock")))
   for(auto& G:City->Goods)if(G.GoodId==TEXT("Good.Salt"))G.BackgroundProductionMilliUnitsPerUpdate=2000;
  Touch(City,Old);
 }
 const auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Definitions);Algo::Reverse(Definitions);const auto Reverse=FHansaEconomicDefinitionCompiler::Compile(Definitions);
 if(!Compiled.IsValid()||!Reverse.IsValid()||Compiled.Registry.GetRegistryHash()!=Reverse.Registry.GetRegistryHash())
 {for(const auto& Issue:Compiled.Issues)UE_LOG(LogTemp,Error,TEXT("%s: %s"),*Issue.Code.ToString(),*Issue.Cause.ToString());return 1;}
 auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("beforeRegistryHash"),FString::Printf(TEXT("%016llX"),Before.Registry.GetRegistryHash()));Report->SetStringField(TEXT("registryHash"),FString::Printf(TEXT("%016llX"),Compiled.Registry.GetRegistryHash()));Report->SetBoolField(TEXT("reverseOrderVerified"),true);Report->SetBoolField(TEXT("applied"),Apply);
 TArray<TSharedPtr<FJsonValue>> Evidence;
 for(const auto& E:Compiled.DefinitionHashes){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("stableId"),E.StableId);R->SetStringField(TEXT("contentHash"),FString::Printf(TEXT("%016llX"),E.ContentHash));R->SetStringField(TEXT("class"),E.DefinitionClassPath);Evidence.Add(MakeShared<FJsonValueObject>(R));}
 Report->SetArrayField(TEXT("definitions"),Evidence);
 if(Apply)for(auto* D:Changed)
 {
  auto* Package=D->GetOutermost();Package->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
  if(!UPackage::SavePackage(Package,D,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args))return 1;
 }
 FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
 FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("PreservationCatalog.json")));
 UE_LOG(LogTemp,Display,TEXT("Preservation catalog: %d definitions, %d changes, %016llX, apply=%d"),Definitions.Num(),Changed.Num(),Compiled.Registry.GetRegistryHash(),Apply);return 0;
}
