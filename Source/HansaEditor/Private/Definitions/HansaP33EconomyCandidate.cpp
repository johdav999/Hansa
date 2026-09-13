#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "HAL/FileManager.h"

namespace Hansa::Editor::EconomicDefinitions
{
TArray<TStrongObjectPtr<UHansaDefinitionBase>> CreateP33EconomyCandidate(FString& OutError)
{
	IAssetRegistry& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")}, true);
	TArray<FAssetData> Rows;
	Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Rows, true, false);
	TArray<const UHansaDefinitionBase*> Baseline;
	for (const auto& Row : Rows) if (const auto* D = Cast<UHansaDefinitionBase>(Row.GetAsset())) Baseline.Add(D);
	const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Baseline);
	if (!Compiled.IsValid() || Baseline.Num() != 72 || Compiled.Registry.GetRegistryHash() != FHansaLubeckScenarioInitializer::MvpRegistryHash)
	{
		OutError = TEXT("P33 requires the exact reviewed catalog v8; refresh the proposal after any production catalog change.");
		return {};
	}
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Result;
	for (const auto* Original : Baseline)
	{
		TStrongObjectPtr<UHansaDefinitionBase> Copy(DuplicateObject<UHansaDefinitionBase>(Original, GetTransientPackage()));
		bool bChanged = false;
		if (auto* Home = Cast<UHansaBuildingDefinition>(Copy.Get()); Home && Home->StableDefinitionId == TEXT("Building.Residence.Artisan"))
		{
			Home->ResidenceCapacity = 12; // A full laborer home must be eligible without evicting residents.
			bChanged = true;
		}
		if (auto* Tier = Cast<UHansaPopulationTierDefinition>(Copy.Get()))
		{
			const bool bArtisan = !Tier->PreviousTierId.IsEmpty();
			for (auto& Need : Tier->Needs)
			{
				if (Need.NeedId == TEXT("Need.Bread")) Need.ConsumptionMilliUnitsPerResidentPerTick = bArtisan ? 5 : 3;
				if (Need.NeedId == TEXT("Need.Fish")) Need.ConsumptionMilliUnitsPerResidentPerTick = 2;
				if (Need.NeedId == TEXT("Need.Beer")) Need.ConsumptionMilliUnitsPerResidentPerTick = bArtisan ? 2 : 1;
				if (Need.NeedId == TEXT("Need.Tools")) Need.ConsumptionMilliUnitsPerResidentPerTick = 1;
			}
			bChanged = true;
		}
		if (auto* Recipe = Cast<UHansaRecipeDefinition>(Copy.Get()); Recipe && Recipe->StableDefinitionId == TEXT("Recipe.BakeBread"))
		{
			Recipe->Outputs[0].QuantityMilliUnits = 6000;
			bChanged = true;
		}
		if (auto* Market = Cast<UHansaCityMarketProfileDefinition>(Copy.Get()))
		{
			for (auto& Good : Market->Goods)
			{
				Good.ConfirmedIncomingSupplyMilliUnits = 0; // Runtime derives promises from loaded route cargo.
				if (!Market->bMarketOnly)
				{
					Good.InitialStockMilliUnits = Good.DesiredReserveMilliUnits + 10000;
					if (Good.GoodId == TEXT("Good.Grain")) Good.InitialStockMilliUnits = 16000;
					if (Good.GoodId == TEXT("Good.Bread")) Good.InitialStockMilliUnits = 12000;
				}
				if (Market->StableDefinitionId == TEXT("City.Rostock"))
				{
					if (Good.GoodId == TEXT("Good.Beer")) Good.BackgroundProductionMilliUnitsPerUpdate = 2000;
					if (Good.GoodId == TEXT("Good.Bread")) Good.BackgroundProductionMilliUnitsPerUpdate = 1500;
					if (Good.GoodId == TEXT("Good.Tools")) Good.BackgroundProductionMilliUnitsPerUpdate = 1400;
					if (Good.GoodId == TEXT("Good.Iron") || Good.GoodId == TEXT("Good.Salt")) Good.BackgroundProductionMilliUnitsPerUpdate = 1250;
				}
			}
			bChanged = true;
		}
		if (bChanged) ++Copy->AuthoredRevision;
		Copy->RefreshContentHash();
		Result.Add(MoveTemp(Copy));
	}
	return Result;
}

int32 StageP33EconomyCandidate()
{
	FString Error;
	auto Candidate = CreateP33EconomyCandidate(Error);
	if (Candidate.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return 1; }
	TArray<const UHansaDefinitionBase*> Definitions;
	for (const auto& D : Candidate) Definitions.Add(D.Get());
	const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
	if (!Compiled.IsValid())
	{
		for (const auto& Issue : Compiled.Issues) UE_LOG(LogTemp, Error, TEXT("%s %s: %s"), *Issue.Code.ToString(), *Issue.PropertyPath, *Issue.Cause.ToString());
		return 1;
	}
	auto Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("catalogVersion"), 9);
	Root->SetStringField(TEXT("status"), TEXT("candidate-awaiting-balance-approval"));
	Root->SetStringField(TEXT("baseRegistryHash"), FString::Printf(TEXT("%016llX"), FHansaLubeckScenarioInitializer::MvpRegistryHash));
	Root->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), Compiled.Registry.GetRegistryHash()));
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const auto& D : Candidate)
	{
		const FString Name = TEXT("DA_") + D->StableDefinitionId.Replace(TEXT("."), TEXT("_"));
		const FString PackageName = TEXT("/Game/Hansa/Generated/Staging/EconomyP33/") + Name;
		UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_NoWarn);
		if (!Package) Package = CreatePackage(*PackageName);
		Package->FullyLoad();
		auto* Asset = DuplicateObject<UHansaDefinitionBase>(D.Get(), Package, *Name);
		Asset->SetFlags(RF_Public | RF_Standalone);
		const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Asset, *File, Args)) { UE_LOG(LogTemp, Error, TEXT("Could not save P33 staging asset %s"), *File); return 1; }
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("stableId"), D->StableDefinitionId);
		Row->SetStringField(TEXT("classPath"), D->GetClass()->GetPathName());
		Row->SetStringField(TEXT("contentHash"), FString::Printf(TEXT("%016llX"), D->ComputeDeterministicContentHash()));
		Row->SetStringField(TEXT("stagedAsset"), PackageName);
		const UHansaDefinitionBase* Original = nullptr;
		// Baseline definitions remain loaded at their canonical Core paths.
		TArray<TSharedPtr<FJsonValue>> Changes;
		for (TObjectIterator<UHansaDefinitionBase> It; It; ++It)
			if (It->StableDefinitionId == D->StableDefinitionId && It->GetPathName().StartsWith(TEXT("/Game/Hansa/Core/"))) { Original = *It; break; }
		if (!Original) return 1;
		Row->SetStringField(TEXT("beforeHash"), FString::Printf(TEXT("%016llX"), Original->ComputeDeterministicContentHash()));
		for (TFieldIterator<FProperty> Property(D->GetClass()); Property; ++Property)
		{
			if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->Identical_InContainer(Original, D.Get())) continue;
			FString Before, After;
			Property->ExportText_InContainer(0, Before, Original, nullptr, nullptr, PPF_None);
			Property->ExportText_InContainer(0, After, D.Get(), nullptr, nullptr, PPF_None);
			auto Change = MakeShared<FJsonObject>();
			Change->SetStringField(TEXT("field"), Property->GetName());
			Change->SetStringField(TEXT("before"), Before); Change->SetStringField(TEXT("after"), After);
			Changes.Add(MakeShared<FJsonValueObject>(Change));
		}
		Row->SetArrayField(TEXT("changes"), Changes);
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Root->SetArrayField(TEXT("definitions"), Rows);
	FString Json; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	const FString Output = FPaths::ProjectDir() / TEXT("Docs/Development/EconomyP33/catalog_v9_candidate.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
	return FFileHelper::SaveStringToFile(Json, *Output) ? 0 : 1;
}
}
