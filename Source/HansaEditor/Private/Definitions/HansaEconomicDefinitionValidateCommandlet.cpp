#include "Definitions/HansaEconomicDefinitionValidateCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace Hansa::Editor::EconomicValidation
{
	FString EscapeJson(const FString& Value)
	{
		FString Escaped = Value.Replace(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		return Escaped;
	}

	FString Severity(const EHansaDefinitionValidationSeverity Value)
	{
		switch (Value)
		{
		case EHansaDefinitionValidationSeverity::Information: return TEXT("Information");
		case EHansaDefinitionValidationSeverity::Warning: return TEXT("Warning");
		case EHansaDefinitionValidationSeverity::Error: return TEXT("Error");
		default: return TEXT("Unknown");
		}
	}

	FString WriteJson(const FHansaEconomicRegistryCompileResult& Result)
	{
		FString Json = TEXT("{\n");
		Json += TEXT("  \"schemaVersion\": 1,\n");
		Json += FString::Printf(TEXT("  \"valid\": %s,\n"), Result.IsValid() ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("  \"registryHash\": \"%016llX\",\n"), static_cast<unsigned long long>(Result.Registry.GetRegistryHash()));
		Json += FString::Printf(TEXT("  \"goods\": %d, \"recipes\": %d, \"buildings\": %d,\n"),
			Result.Registry.GetGoods().Num(), Result.Registry.GetRecipes().Num(), Result.Registry.GetBuildings().Num());
		Json += TEXT("  \"issues\": [\n");
		for (int32 Index = 0; Index < Result.Issues.Num(); ++Index)
		{
			const FHansaDefinitionValidationIssue& Issue = Result.Issues[Index];
			Json += FString::Printf(
				TEXT("    {\"severity\": \"%s\", \"code\": \"%s\", \"propertyPath\": \"%s\", \"cause\": \"%s\", \"remedy\": \"%s\"}%s\n"),
				*Severity(Issue.Severity), *Issue.Code.ToString(), *EscapeJson(Issue.PropertyPath),
				*EscapeJson(Issue.Cause.ToString()), *EscapeJson(Issue.Remedy.ToString()),
				Index + 1 < Result.Issues.Num() ? TEXT(",") : TEXT(""));
		}
		Json += TEXT("  ]\n}\n");
		return Json;
	}
}

UHansaEconomicDefinitionValidateCommandlet::UHansaEconomicDefinitionValidateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UHansaEconomicDefinitionValidateCommandlet::Main(const FString& Params)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous({ TEXT("/Game/Hansa/Core") }, true);
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(TEXT("/Game/Hansa/Core"), Assets, true, false);
	TArray<const UHansaDefinitionBase*> Definitions;
	for (const FAssetData& Asset : Assets)
	{
		if (const UHansaDefinitionBase* Definition = Cast<UHansaDefinitionBase>(Asset.GetAsset()))
		{
			if (Definition->IsA<UHansaGoodDefinition>() || Definition->IsA<UHansaRecipeDefinition>() || Definition->IsA<UHansaBuildingDefinition>())
			{
				Definitions.Add(Definition);
			}
		}
	}
	const FHansaEconomicRegistryCompileResult Result = FHansaEconomicDefinitionCompiler::Compile(Definitions);
	FString OutputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("Production"), TEXT("economic-validation.json"));
	FParse::Value(*Params, TEXT("Output="), OutputPath);
	OutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	if (!FFileHelper::SaveStringToFile(Hansa::Editor::EconomicValidation::WriteJson(Result), *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write economic validation evidence to %s."), *OutputPath);
		return 2;
	}
	UE_LOG(LogTemp, Display, TEXT("Economic validation: %s (%d issues), evidence %s."), Result.IsValid() ? TEXT("valid") : TEXT("invalid"), Result.Issues.Num(), *OutputPath);
	return Result.IsValid() ? 0 : 1;
}
