#pragma once

#include "CoreMinimal.h"

class UHansaDefinitionBase;

namespace Hansa::Editor::VisibleContent
{
	enum class EValidationMode : uint8
	{
		Completeness,
		ReleaseReadiness
	};

	struct FNativeResolution final
	{
		FString StableId;
		int32 Width = 0;
		int32 Height = 0;
		float UiScale = 1.0f;
		bool bNativeCaptureRequired = false;
		bool bRasterResamplingAllowed = true;
	};

	struct FEntry final
	{
		FString StableId;
		FString Category;
		FString OwningDefinition;
		FString PresentationRole;
		FString CurrentReference;
		FString Status;
		FString IntendedCanonicalPath;
		FString DeliveryPrompt;
		FString LodRequirement;
		FString CollisionRequirement;
		FString PivotRequirement;
		FString FootprintRequirement;
		FString DefinitionPresentationProperty;
		TArray<FString> ProofNeeded;
		TArray<FString> RequiredUiStates;
		TArray<FString> ImplementedUiStates;
		bool bGoldenPath = false;
		bool bBindsDefinitionPresentation = false;
	};

	struct FManifest final
	{
		int32 SchemaVersion = 0;
		FString ManifestId;
		FString GoldenSessionId;
		TArray<FString> RequiredCategories;
		TArray<FString> GoldenPathPresentationDefinitions;
		TArray<FString> RequiredScreenIds;
		TArray<FNativeResolution> RequiredNativeResolutions;
		TArray<FEntry> Entries;
	};

	struct FIssue final
	{
		FString Code;
		FString StableId;
		FString Message;
	};

	class FManifestValidator final
	{
	public:
		static FString ProjectManifestPath();
		static bool LoadProjectManifest(FManifest& OutManifest, FString& OutError);
		static bool LoadManifest(const FString& Path, FManifest& OutManifest, FString& OutError);
		static TArray<FIssue> Validate(
			const FManifest& Manifest,
			EValidationMode Mode,
			TConstArrayView<const UHansaDefinitionBase*> Definitions);
	};
}
