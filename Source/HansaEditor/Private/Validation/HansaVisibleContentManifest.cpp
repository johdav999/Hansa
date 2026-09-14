#include "Validation/HansaVisibleContentManifest.h"

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	using namespace Hansa::Editor::VisibleContent;

	void AddIssue(TArray<FIssue>& OutIssues, const TCHAR* Code, const FString& StableId, const FString& Message)
	{
		OutIssues.Add({Code, StableId, Message});
	}

	bool ReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		FString& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.TrimStartAndEnd().IsEmpty())
		{
			OutError = FString::Printf(TEXT("Missing non-empty string field '%s'."), Field);
			return false;
		}
		return true;
	}

	bool ReadRequiredBool(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		bool& OutValue,
		FString& OutError)
	{
		if (!Object.IsValid() || !Object->TryGetBoolField(Field, OutValue))
		{
			OutError = FString::Printf(TEXT("Missing boolean field '%s'."), Field);
			return false;
		}
		return true;
	}

	bool ReadRequiredStringArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FString>& OutValues,
		FString& OutError,
		const bool bAllowEmpty)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || Values == nullptr ||
			(!bAllowEmpty && Values->IsEmpty()))
		{
			OutError = FString::Printf(TEXT("Missing%s string array field '%s'."), bAllowEmpty ? TEXT("") : TEXT(" non-empty"), Field);
			return false;
		}
		OutValues.Reset(Values->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (!Value.IsValid() || !Value->TryGetString(Text) || Text.TrimStartAndEnd().IsEmpty())
			{
				OutError = FString::Printf(TEXT("Array field '%s' contains an empty or non-string value."), Field);
				return false;
			}
			OutValues.Add(MoveTemp(Text));
		}
		return true;
	}

	bool IsNoneReference(const FString& Reference)
	{
		return Reference.Equals(TEXT("none"), ESearchCase::IgnoreCase);
	}

	bool IsForbiddenPath(const FString& Reference)
	{
		return Reference.Contains(TEXT("/Generated/Staging/"), ESearchCase::IgnoreCase) ||
			Reference.Contains(TEXT("/Developer/"), ESearchCase::IgnoreCase);
	}

	bool IsEngineBasicShape(const FString& Reference)
	{
		return Reference.Contains(TEXT("/Engine/BasicShapes/"), ESearchCase::IgnoreCase) ||
			Reference.Contains(TEXT("/Engine/EngineMeshes/Cube"), ESearchCase::IgnoreCase);
	}

	bool IsReleaseReadyStatus(const FString& Status)
	{
		return Status == TEXT("production-ready") || Status == TEXT("native-production");
	}

	const UHansaDefinitionBase* FindDefinition(
		const TConstArrayView<const UHansaDefinitionBase*> Definitions,
		const FString& StableId)
	{
		const UHansaDefinitionBase* const* Found = Definitions.FindByPredicate(
			[&StableId](const UHansaDefinitionBase* Definition)
			{
				return Definition != nullptr && Definition->StableDefinitionId == StableId;
			});
		return Found != nullptr ? *Found : nullptr;
	}

	bool ResolveDefinitionPresentation(
		const UHansaDefinitionBase& Definition,
		const FString& Property,
		FString& OutReference)
	{
		if (Property == TEXT("PresentationMesh"))
		{
			OutReference = Definition.PresentationMesh.ToSoftObjectPath().ToString();
			return true;
		}
		if (Property == TEXT("PresentationMeshOrActor"))
		{
			if (const auto* Vehicle = Cast<UHansaVehicleDefinition>(&Definition))
			{
				OutReference = Vehicle->PresentationActorClass.IsNull()
					? Vehicle->PresentationMesh.ToSoftObjectPath().ToString()
					: Vehicle->PresentationActorClass.ToSoftObjectPath().ToString();
				return true;
			}
			const UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(&Definition);
			if (Building == nullptr)
			{
				return false;
			}
			OutReference = Building->PresentationActorClass.IsNull()
				? Building->PresentationMesh.ToSoftObjectPath().ToString()
				: Building->PresentationActorClass.ToSoftObjectPath().ToString();
			return true;
		}
		if (Property == TEXT("Icon"))
		{
			const UHansaGoodDefinition* Good = Cast<UHansaGoodDefinition>(&Definition);
			if (Good == nullptr)
			{
				return false;
			}
			OutReference = Good->Icon.ToSoftObjectPath().ToString();
			return true;
		}
		return false;
	}
}

namespace Hansa::Editor::VisibleContent
{
	FString FManifestValidator::ProjectManifestPath()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests"), TEXT("Golden"),
			TEXT("enhanced_mvp_visible_content_v1.json"));
	}

	bool FManifestValidator::LoadProjectManifest(FManifest& OutManifest, FString& OutError)
	{
		return LoadManifest(ProjectManifestPath(), OutManifest, OutError);
	}

	bool FManifestValidator::LoadManifest(const FString& Path, FManifest& OutManifest, FString& OutError)
	{
		OutManifest = {};
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			OutError = FString::Printf(TEXT("Unable to read visible-content manifest '%s'."), *Path);
			return false;
		}
		TSharedPtr<FJsonObject> RootObject;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), RootObject) || !RootObject.IsValid())
		{
			OutError = FString::Printf(TEXT("Unable to parse visible-content manifest '%s'."), *Path);
			return false;
		}
		double SchemaVersion = 0.0;
		if (!RootObject->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) ||
			!ReadRequiredString(RootObject, TEXT("manifestId"), OutManifest.ManifestId, OutError) ||
			!ReadRequiredString(RootObject, TEXT("goldenSessionId"), OutManifest.GoldenSessionId, OutError) ||
			!ReadRequiredStringArray(RootObject, TEXT("requiredCategories"), OutManifest.RequiredCategories, OutError, false) ||
			!ReadRequiredStringArray(RootObject, TEXT("goldenPathPresentationDefinitions"),
				OutManifest.GoldenPathPresentationDefinitions, OutError, false) ||
			!ReadRequiredStringArray(RootObject, TEXT("requiredScreenIds"), OutManifest.RequiredScreenIds, OutError, false))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Manifest is missing a numeric schemaVersion.");
			}
			return false;
		}
		OutManifest.SchemaVersion = static_cast<int32>(SchemaVersion);

		const TArray<TSharedPtr<FJsonValue>>* Resolutions = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("requiredNativeResolutions"), Resolutions) || Resolutions == nullptr || Resolutions->IsEmpty())
		{
			OutError = TEXT("Manifest requires a non-empty requiredNativeResolutions array.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Resolutions)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			FNativeResolution Resolution;
			double Width = 0.0;
			double Height = 0.0;
			double UiScale = 0.0;
			if (!ReadRequiredString(Object, TEXT("stableId"), Resolution.StableId, OutError) ||
				!Object->TryGetNumberField(TEXT("width"), Width) ||
				!Object->TryGetNumberField(TEXT("height"), Height) ||
				!Object->TryGetNumberField(TEXT("uiScale"), UiScale) ||
				!ReadRequiredBool(Object, TEXT("nativeCaptureRequired"), Resolution.bNativeCaptureRequired, OutError) ||
				!ReadRequiredBool(Object, TEXT("rasterResamplingAllowed"), Resolution.bRasterResamplingAllowed, OutError))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("Native resolution entry is missing numeric width, height, or uiScale.");
				}
				return false;
			}
			Resolution.Width = static_cast<int32>(Width);
			Resolution.Height = static_cast<int32>(Height);
			Resolution.UiScale = static_cast<float>(UiScale);
			OutManifest.RequiredNativeResolutions.Add(MoveTemp(Resolution));
		}

		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!RootObject->TryGetArrayField(TEXT("entries"), Entries) || Entries == nullptr || Entries->IsEmpty())
		{
			OutError = TEXT("Manifest requires a non-empty entries array.");
			return false;
		}
		for (int32 Index = 0; Index < Entries->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Object = (*Entries)[Index].IsValid() ? (*Entries)[Index]->AsObject() : nullptr;
			FEntry Entry;
			const TSharedPtr<FJsonObject>* Requirements = nullptr;
			if (!ReadRequiredString(Object, TEXT("stableId"), Entry.StableId, OutError) ||
				!ReadRequiredString(Object, TEXT("category"), Entry.Category, OutError) ||
				!ReadRequiredString(Object, TEXT("owningDefinition"), Entry.OwningDefinition, OutError) ||
				!ReadRequiredString(Object, TEXT("presentationRole"), Entry.PresentationRole, OutError) ||
				!ReadRequiredString(Object, TEXT("currentReference"), Entry.CurrentReference, OutError) ||
				!ReadRequiredString(Object, TEXT("status"), Entry.Status, OutError) ||
				!ReadRequiredString(Object, TEXT("intendedCanonicalPath"), Entry.IntendedCanonicalPath, OutError) ||
				!ReadRequiredString(Object, TEXT("deliveryPrompt"), Entry.DeliveryPrompt, OutError) ||
				!ReadRequiredBool(Object, TEXT("goldenPath"), Entry.bGoldenPath, OutError) ||
				!ReadRequiredBool(Object, TEXT("bindsDefinitionPresentation"), Entry.bBindsDefinitionPresentation, OutError) ||
				!ReadRequiredString(Object, TEXT("definitionPresentationProperty"), Entry.DefinitionPresentationProperty, OutError) ||
				!ReadRequiredStringArray(Object, TEXT("proofNeeded"), Entry.ProofNeeded, OutError, false) ||
				!ReadRequiredStringArray(Object, TEXT("requiredUiStates"), Entry.RequiredUiStates, OutError, true) ||
				!ReadRequiredStringArray(Object, TEXT("implementedUiStates"), Entry.ImplementedUiStates, OutError, true) ||
				!Object->TryGetObjectField(TEXT("requirements"), Requirements) || Requirements == nullptr ||
				!ReadRequiredString(*Requirements, TEXT("lod"), Entry.LodRequirement, OutError) ||
				!ReadRequiredString(*Requirements, TEXT("collision"), Entry.CollisionRequirement, OutError) ||
				!ReadRequiredString(*Requirements, TEXT("pivot"), Entry.PivotRequirement, OutError) ||
				!ReadRequiredString(*Requirements, TEXT("footprint"), Entry.FootprintRequirement, OutError))
			{
				OutError = FString::Printf(TEXT("Entry %d is invalid: %s"), Index, *OutError);
				return false;
			}
			OutManifest.Entries.Add(MoveTemp(Entry));
		}
		return true;
	}

	TArray<FIssue> FManifestValidator::Validate(
		const FManifest& Manifest,
		const EValidationMode Mode,
		const TConstArrayView<const UHansaDefinitionBase*> Definitions)
	{
		TArray<FIssue> Issues;
		if (Manifest.SchemaVersion != 1)
		{
			AddIssue(Issues, TEXT("HVCM-SCHEMA-001"), Manifest.ManifestId,
				FString::Printf(TEXT("Unsupported schemaVersion %d; expected 1."), Manifest.SchemaVersion));
		}

		const TSet<FString> AllowedStatuses = {
			TEXT("production-ready"), TEXT("native-production"), TEXT("unverified-production"),
			TEXT("prototype-native"), TEXT("reference-only"), TEXT("staging-only"),
			TEXT("engine-placeholder"), TEXT("missing"), TEXT("not-shown") };
		const TSet<FString> RequiredCategoryContract = {
			TEXT("world"), TEXT("building"), TEXT("construction-stage"), TEXT("road"), TEXT("ship"),
			TEXT("cart"), TEXT("citizen"), TEXT("harbor-equipment"), TEXT("production-prop"),
			TEXT("cargo-prop"), TEXT("vegetation"), TEXT("street-dressing"), TEXT("water-shore"),
			TEXT("effect"), TEXT("icon"), TEXT("cursor"), TEXT("overlay"), TEXT("frontend-screen"),
			TEXT("hud-panel"), TEXT("management-screen"), TEXT("modal-screen"), TEXT("ui-state") };
		TSet<FString> DeclaredCategories;
		DeclaredCategories.Append(Manifest.RequiredCategories);
		for (const FString& RequiredCategory : RequiredCategoryContract)
		{
			if (!DeclaredCategories.Contains(RequiredCategory))
			{
				AddIssue(Issues, TEXT("HVCM-COVERAGE-001"), RequiredCategory,
					TEXT("Required visible-content category is not declared."));
			}
		}

		bool bHas720 = false;
		bool bHas1080 = false;
		TSet<FString> ResolutionIds;
		for (const FNativeResolution& Resolution : Manifest.RequiredNativeResolutions)
		{
			if (ResolutionIds.Contains(Resolution.StableId) || Resolution.Width <= 0 || Resolution.Height <= 0 ||
				Resolution.UiScale <= 0.0f || !Resolution.bNativeCaptureRequired || Resolution.bRasterResamplingAllowed)
			{
				AddIssue(Issues, TEXT("HVCM-RESOLUTION-001"), Resolution.StableId,
					TEXT("Native resolution must be unique, positive, capture-required, and forbid raster resampling."));
			}
			ResolutionIds.Add(Resolution.StableId);
			bHas720 |= Resolution.Width == 1280 && Resolution.Height == 720;
			bHas1080 |= Resolution.Width == 1920 && Resolution.Height == 1080;
		}
		if (!bHas720 || !bHas1080)
		{
			AddIssue(Issues, TEXT("HVCM-RESOLUTION-002"), Manifest.ManifestId,
				TEXT("The enhanced MVP requires native 1280x720 and 1920x1080 capture contracts."));
		}

		TSet<FString> StableIds;
		TSet<FString> CoveredCategories;
		TSet<FString> CoveredDefinitions;
		TSet<FString> CoveredScreens;
		for (const FEntry& Entry : Manifest.Entries)
		{
			if (StableIds.Contains(Entry.StableId))
			{
				AddIssue(Issues, TEXT("HVCM-SCHEMA-002"), Entry.StableId, TEXT("Stable manifest ID is duplicated."));
			}
			StableIds.Add(Entry.StableId);
			CoveredCategories.Add(Entry.Category);
			CoveredScreens.Add(Entry.StableId);
			if (!DeclaredCategories.Contains(Entry.Category))
			{
				AddIssue(Issues, TEXT("HVCM-COVERAGE-002"), Entry.StableId,
					FString::Printf(TEXT("Category '%s' is not declared by requiredCategories."), *Entry.Category));
			}
			if (!AllowedStatuses.Contains(Entry.Status))
			{
				AddIssue(Issues, TEXT("HVCM-SCHEMA-003"), Entry.StableId,
					FString::Printf(TEXT("Unknown explicit status '%s'."), *Entry.Status));
			}
			if (!Entry.bGoldenPath && Entry.Status != TEXT("not-shown"))
			{
				AddIssue(Issues, TEXT("HVCM-SCHEMA-004"), Entry.StableId,
					TEXT("A non-golden entry must use status 'not-shown' so exclusions are unambiguous."));
			}

			if (Entry.bBindsDefinitionPresentation)
			{
				CoveredDefinitions.Add(Entry.OwningDefinition);
				const UHansaDefinitionBase* Definition = FindDefinition(Definitions, Entry.OwningDefinition);
				FString ActualReference;
				if (Definition == nullptr)
				{
					AddIssue(Issues, TEXT("HVCM-DEF-002"), Entry.StableId,
						FString::Printf(TEXT("Owning definition '%s' does not exist in the reviewed catalog."), *Entry.OwningDefinition));
				}
				else if (!ResolveDefinitionPresentation(*Definition, Entry.DefinitionPresentationProperty, ActualReference))
				{
					AddIssue(Issues, TEXT("HVCM-DEF-003"), Entry.StableId,
						FString::Printf(TEXT("Unsupported definition presentation property '%s'."), *Entry.DefinitionPresentationProperty));
				}
				else
				{
					const FString CanonicalActual = ActualReference.IsEmpty() ? TEXT("none") : ActualReference;
					if (CanonicalActual != Entry.CurrentReference)
					{
						AddIssue(Issues, TEXT("HVCM-DEF-004"), Entry.StableId,
							FString::Printf(TEXT("Manifest currentReference '%s' differs from %s.%s '%s'."),
								*Entry.CurrentReference, *Entry.OwningDefinition, *Entry.DefinitionPresentationProperty,
								*CanonicalActual));
					}
				}
			}

			if (Mode == EValidationMode::ReleaseReadiness && Entry.bGoldenPath)
			{
				if (!IsReleaseReadyStatus(Entry.Status))
				{
					AddIssue(Issues, TEXT("HVCM-RELEASE-001"), Entry.StableId,
						FString::Printf(TEXT("Golden presentation is explicitly '%s'; owner %s must deliver %s."),
							*Entry.Status, *Entry.DeliveryPrompt, *Entry.IntendedCanonicalPath));
				}
				if (Entry.bBindsDefinitionPresentation && IsNoneReference(Entry.CurrentReference))
				{
					AddIssue(Issues, TEXT("HVCM-DEF-001"), Entry.StableId,
						FString::Printf(TEXT("Golden definition '%s' has no production presentation."), *Entry.OwningDefinition));
				}
				if (IsForbiddenPath(Entry.CurrentReference))
				{
					AddIssue(Issues, TEXT("HVCM-PATH-001"), Entry.StableId,
						TEXT("Golden presentation points to staging or Developer content."));
				}
				if (IsEngineBasicShape(Entry.CurrentReference))
				{
					AddIssue(Issues, TEXT("HVCM-PATH-002"), Entry.StableId,
						TEXT("Golden presentation uses an Engine basic-shape fallback."));
				}

				TSet<FString> ImplementedStates;
				ImplementedStates.Append(Entry.ImplementedUiStates);
				for (const FString& RequiredState : Entry.RequiredUiStates)
				{
					if (!ImplementedStates.Contains(RequiredState))
					{
						AddIssue(Issues, TEXT("HVCM-UI-001"), Entry.StableId,
							FString::Printf(TEXT("Required UI state '%s' is not implemented."), *RequiredState));
					}
				}
			}
		}

		for (const FString& Category : RequiredCategoryContract)
		{
			if (!CoveredCategories.Contains(Category))
			{
				AddIssue(Issues, TEXT("HVCM-COVERAGE-003"), Category,
					TEXT("Required visible-content category has no manifest entry."));
			}
		}
		for (const FString& DefinitionId : Manifest.GoldenPathPresentationDefinitions)
		{
			if (!CoveredDefinitions.Contains(DefinitionId))
			{
				AddIssue(Issues, TEXT("HVCM-COVERAGE-004"), DefinitionId,
					TEXT("Golden-path presentation definition has no binding manifest entry."));
			}
		}
		for (const FString& ScreenId : Manifest.RequiredScreenIds)
		{
			if (!CoveredScreens.Contains(ScreenId))
			{
				AddIssue(Issues, TEXT("HVCM-COVERAGE-005"), ScreenId,
					TEXT("Required golden-session screen has no manifest entry."));
			}
		}
		return Issues;
	}
}
