#include "Definitions/HansaTextileProductionCommandlet.h"

#include "Definitions/HansaTextileProductionDraft.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Algo/Reverse.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "HansaTextileProductionPromotion.inl"

namespace
{
constexpr const TCHAR* StageRoot = TEXT("/Game/Hansa/Generated/Staging/TextileProductionV2");
constexpr const TCHAR* ModelRoot = TEXT("/Game/Hansa/Generated/Staging/TextileProductionModelsV1");
constexpr const TCHAR* ReportDirectory = TEXT("Docs/Development/TextileProduction");
}

UHansaTextileProductionCommandlet::UHansaTextileProductionCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UHansaTextileProductionCommandlet::Main(const FString& Params)
{
	IAssetRegistry& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	if (FParse::Param(*Params, TEXT("PromoteReviewed")) || FParse::Param(*Params, TEXT("VerifyApproved"))) return PromoteTextileProduction(Assets, FParse::Param(*Params, TEXT("VerifyApproved")));
	if (FParse::Param(*Params, TEXT("VerifyStage")))
	{
		Assets.ScanPathsSynchronous({StageRoot}, true);
		TArray<FAssetData> Rows;
		Assets.GetAssetsByPath(StageRoot, Rows, true, false);
		TArray<const UHansaDefinitionBase*> Loaded;
		auto Report = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Entries;
		for (const FAssetData& Row : Rows)
		{
			if (const auto* Definition = Cast<UHansaDefinitionBase>(Row.GetAsset()))
			{
				Loaded.Add(Definition);
				auto Entry = MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("stableId"), Definition->StableDefinitionId);
				Entry->SetStringField(TEXT("hash"), FString::Printf(TEXT("%016llX"), Definition->ComputeDeterministicContentHash()));
				Entries.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}
		const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Loaded);
		Report->SetBoolField(TEXT("valid"), Compiled.IsValid());
		Report->SetNumberField(TEXT("definitionCount"), Loaded.Num());
		Report->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), Compiled.Registry.GetRegistryHash()));
		Report->SetArrayField(TEXT("definitions"), Entries);
		FString Json;
		FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectDir() / ReportDirectory / TEXT("reloaded.json")));
		for (const auto& Issue : Compiled.Issues)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *Issue.PropertyPath, *Issue.Cause.ToString());
		}
		UE_LOG(LogTemp, Display, TEXT("Reloaded textile catalog valid=%d count=%d hash=%016llX"), Compiled.IsValid(), Loaded.Num(), Compiled.Registry.GetRegistryHash());
		return Compiled.IsValid() ? 0 : 9;
	}

	const bool bReviseV1 = FParse::Param(*Params, TEXT("ReviseV1"));
	const FString SourceRoot = bReviseV1 ? TEXT("/Game/Hansa/Generated/Staging/TextileProductionV1") : TEXT("/Game/Hansa/Core");
	Assets.ScanPathsSynchronous({SourceRoot}, true);
	TArray<FAssetData> Rows;
	Assets.GetAssetsByPath(*SourceRoot, Rows, true, false);
	TArray<const UHansaDefinitionBase*> Baseline;
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Draft;
	for (const FAssetData& Row : Rows)
	{
		if (const auto* Definition = Cast<UHansaDefinitionBase>(Row.GetAsset()))
		{
			Baseline.Add(Definition);
			Draft.Emplace(DuplicateObject<UHansaDefinitionBase>(Definition, GetTransientPackage()));
		}
	}
	const auto Before = FHansaEconomicDefinitionCompiler::Compile(Baseline);
	if (!Before.IsValid()) return 1;
	FString Error;
	if (!bReviseV1 && !Hansa::Editor::TextileProduction::ApplyDraft(Draft, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		return 2;
	}

	if (bReviseV1)
	{
		for (const auto& Definition : Draft)
		{
			if (auto* Building = Cast<UHansaBuildingDefinition>(Definition.Get()))
			{
				if (Building->StableDefinitionId == TEXT("Building.Tailor") || Building->StableDefinitionId == TEXT("Building.Chandler"))
				{
					Building->FootprintWidthCells = 3;
					Building->FootprintHeightCells = 2;
					++Building->AuthoredRevision;
				}
				if (Building->StableDefinitionId == TEXT("Building.Weaver") || Building->StableDefinitionId == TEXT("Building.Tailor") ||
					Building->StableDefinitionId == TEXT("Building.Chandler") || Building->StableDefinitionId == TEXT("Building.Ropewalk"))
					Building->ConstructionPresentationPurpose = FText::ChangeKey(TEXT("Hansa.TextileProduction"),
						Building->StableDefinitionId + TEXT(".Purpose"), Building->ConstructionPresentationPurpose);
			}
			Definition->RefreshContentHash();
		}
	}
	const bool bBindModels = FParse::Param(*Params, TEXT("BindModels"));
	if (bBindModels)
	{
		for (const TCHAR* Kind : {TEXT("Weaver"), TEXT("Tailor"), TEXT("Chandler"), TEXT("Ropewalk")})
		{
			const FString Name = TEXT("SM_") + FString(Kind);
			const FSoftObjectPath MeshPath(FString(ModelRoot) + TEXT("/") + Name + TEXT(".") + Name);
			const auto* WorkshopMesh = Cast<UStaticMesh>(MeshPath.TryLoad());
            if (!WorkshopMesh)
			{
				UE_LOG(LogTemp, Error, TEXT("Missing staged textile workshop mesh %s"), *MeshPath.ToString());
				return 8;
			}
			for (const auto& Definition : Draft)
			{
				if (Definition->StableDefinitionId == TEXT("Building.") + FString(Kind))
				{
					const auto* Building = CastChecked<UHansaBuildingDefinition>(Definition.Get());
                    const FVector Size = WorkshopMesh->GetBoundingBox().GetSize();
                    if (Size.X > Building->FootprintWidthCells * 400.0 + 0.1 || Size.Y > Building->FootprintHeightCells * 400.0 + 0.1)
                    {
                        UE_LOG(LogTemp, Error, TEXT("%s footprint does not contain imported mesh %.2f x %.2f cm. Increase authored footprint; do not shrink the model."), *Definition->StableDefinitionId, Size.X, Size.Y);
                        return 10;
                    }
                    Definition->PresentationMesh = TSoftObjectPtr<UStaticMesh>(MeshPath);
					Definition->RefreshContentHash();
				}
			}
		}
	}

	TArray<const UHansaDefinitionBase*> Pointers;
	for (const auto& Definition : Draft) Pointers.Add(Definition.Get());
	const auto After = FHansaEconomicDefinitionCompiler::Compile(Pointers);
	for (const auto& Issue : After.Issues)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *Issue.PropertyPath, *Issue.Cause.ToString());
	}
	if (!After.IsValid()) return 3;
	Algo::Reverse(Pointers);
	const auto Reverse = FHansaEconomicDefinitionCompiler::Compile(Pointers);
	if (!Reverse.IsValid() || Reverse.Registry.GetRegistryHash() != After.Registry.GetRegistryHash()) return 4;

	const FString OutputDirectory = FPaths::ProjectDir() / ReportDirectory;
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	const bool bStage = FParse::Param(*Params, TEXT("Stage"));
	if (bStage && IFileManager::Get().DirectoryExists(*(FPaths::ProjectContentDir() / TEXT("Hansa/Generated/Staging/TextileProductionV2"))))
	{
		UE_LOG(LogTemp, Error, TEXT("Textile staging snapshot already exists; inspect it before creating a revision."));
		return 5;
	}

	auto Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("status"), bStage ? TEXT("staged-economy-draft") : TEXT("validated-transient-economy-draft"));
	Manifest->SetStringField(TEXT("baseRegistryHash"), FString::Printf(TEXT("%016llX"), Before.Registry.GetRegistryHash()));
	Manifest->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), After.Registry.GetRegistryHash()));
	Manifest->SetStringField(TEXT("presentationStatus"), bBindModels
		? TEXT("Verified staged workshop meshes bound; explicit production approval remains required.")
		: TEXT("No workshop meshes bound. Import and verify the staged models, then rerun with -BindModels."));
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const auto& Definition : Draft)
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("stableId"), Definition->StableDefinitionId);
		Entry->SetStringField(TEXT("hash"), FString::Printf(TEXT("%016llX"), Definition->ComputeDeterministicContentHash()));
		const auto* Existing = Baseline.FindByPredicate([&](const auto* Value) { return Value->StableDefinitionId == Definition->StableDefinitionId; });
		Entry->SetStringField(TEXT("change"), !Existing ? TEXT("added") :
			(*Existing)->ComputeDeterministicContentHash() == Definition->ComputeDeterministicContentHash() ? TEXT("unchanged") : TEXT("modified"));
		TArray<TSharedPtr<FJsonValue>> Changes;
		for (TFieldIterator<FProperty> It(Definition->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Transient) || !Property->HasAnyPropertyFlags(CPF_Edit)) continue;
			FString Current, Previous;
			Property->ExportText_InContainer(0, Current, Definition.Get(), Definition.Get(), Definition.Get(), PPF_None);
			if (Existing) Property->ExportText_InContainer(0, Previous, *Existing, *Existing, const_cast<UHansaDefinitionBase*>(*Existing), PPF_None);
			if (Existing && Current == Previous) continue;
			auto Change = MakeShared<FJsonObject>();
			Change->SetStringField(TEXT("property"), Property->GetName());
			Change->SetStringField(TEXT("before"), Previous);
			Change->SetStringField(TEXT("after"), Current);
			Changes.Add(MakeShared<FJsonValueObject>(Change));
		}
		Entry->SetArrayField(TEXT("propertyChanges"), Changes);
		if (bStage)
		{
			const FString Name = TEXT("DA_") + Definition->StableDefinitionId.Replace(TEXT("."), TEXT("_"));
			const FString PackagePath = FString(StageRoot) + TEXT("/") + Name;
			UPackage* Package = CreatePackage(*PackagePath);
			auto* Copy = DuplicateObject<UHansaDefinitionBase>(Definition.Get(), Package, *Name);
			Copy->SetFlags(RF_Public | RF_Standalone);
			FSavePackageArgs Save;
			Save.TopLevelFlags = RF_Public | RF_Standalone;
			Save.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, Copy, *FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension()), Save)) return 6;
			Entry->SetStringField(TEXT("asset"), Copy->GetPathName());
		}
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Manifest->SetArrayField(TEXT("definitions"), Entries);
	FString Json;
	FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&Json));
	if (!FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("candidate.json")))) return 7;
	UE_LOG(LogTemp, Display, TEXT("Textile production draft validated: %d definitions, %016llX"), Draft.Num(), After.Registry.GetRegistryHash());
	return 0;
}
