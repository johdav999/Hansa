#include "Definitions/HansaDefinitionBase.h"

#include "Internationalization/Text.h"
#include "Model/HansaIds.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UnrealType.h"

namespace Hansa::Game::Definitions
{
	uint64 HashUtf8Fnv1a(const FString& Text)
	{
		constexpr uint64 OffsetBasis = 14695981039346656037ull;
		constexpr uint64 Prime = 1099511628211ull;
		uint64 Hash = OffsetBasis;
		const FTCHARToUTF8 Utf8(*Text);
		for (int32 Index = 0; Index < Utf8.Length(); ++Index)
		{
			Hash ^= static_cast<uint8>(Utf8.Get()[Index]);
			Hash *= Prime;
		}
		return Hash;
	}

	void AddIssue(
		TArray<FHansaDefinitionValidationIssue>& OutIssues,
		const EHansaDefinitionValidationSeverity Severity,
		const FName Code,
		const TCHAR* PropertyPath,
		const FText& Cause,
		const FText& Remedy)
	{
		OutIssues.Add(FHansaDefinitionValidationIssue { Severity, Code, PropertyPath, Cause, Remedy });
	}
}

UHansaDefinitionBase::UHansaDefinitionBase()
	: DisplayName(FText::FromString(TEXT("Unnamed definition")))
	, LocalizationKey(TEXT("Game.Definition.Unnamed"))
	, DefinitionCategory(TEXT("Uncategorized"))
	, ContentSet(TEXT("Core"))
{
	SetFlags(RF_Transactional);
}

FPrimaryAssetId UHansaDefinitionBase::GetPrimaryAssetId() const
{
	if (StableDefinitionId.IsEmpty())
	{
		return FPrimaryAssetId();
	}

	return FPrimaryAssetId(FPrimaryAssetType(GetClass()->GetFName()), FName(*StableDefinitionId));
}

void UHansaDefinitionBase::PostLoad()
{
	Super::PostLoad();
	RefreshContentHash();
}

void UHansaDefinitionBase::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	using namespace Hansa::Game::Definitions;

	if (!Hansa::Simulation::FHansaDefinitionId::TryParse(StableDefinitionId))
	{
		AddIssue(
			OutIssues,
			EHansaDefinitionValidationSeverity::Error,
			TEXT("HSA-DEF-001"),
			TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaDefinition", "InvalidStableId", "The stable definition ID is empty or is not a registered canonical Domain.Name identity."),
			NSLOCTEXT("HansaDefinition", "InvalidStableIdRemedy", "Choose a registered domain and an explicit PascalCase name; never derive identity from display text or an asset path."));
	}

	if (SchemaVersion < 1)
	{
		AddIssue(
			OutIssues,
			EHansaDefinitionValidationSeverity::Error,
			TEXT("HSA-DEF-002"),
			TEXT("SchemaVersion"),
			NSLOCTEXT("HansaDefinition", "InvalidSchemaVersion", "Schema version must be at least one."),
			NSLOCTEXT("HansaDefinition", "InvalidSchemaVersionRemedy", "Set the current schema version and provide a migration for breaking changes."));
	}

	if (AuthoredRevision < 1)
	{
		AddIssue(
			OutIssues,
			EHansaDefinitionValidationSeverity::Error,
			TEXT("HSA-DEF-003"),
			TEXT("AuthoredRevision"),
			NSLOCTEXT("HansaDefinition", "InvalidAuthoredRevision", "Authored revision must be at least one."),
			NSLOCTEXT("HansaDefinition", "InvalidAuthoredRevisionRemedy", "Restore a positive monotonic revision before accepting patches or saving."));
	}

	if (DisplayName.IsEmptyOrWhitespace() || LocalizationKey.IsNone())
	{
		AddIssue(
			OutIssues,
			EHansaDefinitionValidationSeverity::Error,
			TEXT("HSA-DEF-004"),
			TEXT("DisplayName"),
			NSLOCTEXT("HansaDefinition", "MissingDisplayIdentity", "Display name and localization key are required."),
			NSLOCTEXT("HansaDefinition", "MissingDisplayIdentityRemedy", "Provide readable display text and a stable Game.* localization key."));
	}

	if (DefinitionCategory.IsNone() || ContentSet.IsNone())
	{
		AddIssue(
			OutIssues,
			EHansaDefinitionValidationSeverity::Error,
			TEXT("HSA-DEF-005"),
			TEXT("DefinitionCategory"),
			NSLOCTEXT("HansaDefinition", "MissingOrganization", "Definition category and content set are required."),
			NSLOCTEXT("HansaDefinition", "MissingOrganizationRemedy", "Assign an authoring category and stable content-set name."));
	}

	if (bDeprecated)
	{
		const Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaDefinitionId> Replacement =
			Hansa::Simulation::FHansaDefinitionId::TryParse(ReplacementDefinitionId);
		if (!Replacement || ReplacementDefinitionId == StableDefinitionId)
		{
			AddIssue(
				OutIssues,
				EHansaDefinitionValidationSeverity::Error,
				TEXT("HSA-DEF-006"),
				TEXT("ReplacementDefinitionId"),
				NSLOCTEXT("HansaDefinition", "InvalidReplacement", "A deprecated definition needs a different valid replacement stable ID."),
				NSLOCTEXT("HansaDefinition", "InvalidReplacementRemedy", "Select an existing compatible replacement or clear Deprecated."));
		}
	}
}

uint64 UHansaDefinitionBase::ComputeDeterministicContentHash() const
{
	FString CanonicalData;
	CanonicalData.Reserve(512);
	AppendDefinitionHashData(CanonicalData);
	return Hansa::Game::Definitions::HashUtf8Fnv1a(CanonicalData);
}

void UHansaDefinitionBase::RefreshContentHash()
{
	ContentHash = ComputeDeterministicContentHash();
}

void UHansaDefinitionBase::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	TArray<FString> SortedTags;
	SortedTags.Reserve(Tags.Num());
	for (const FName Tag : Tags)
	{
		SortedTags.Add(Tag.ToString());
	}
	SortedTags.Sort();

	const FString* InspectedSource = FTextInspector::GetSourceString(DisplayName);
	const FString SourceDisplayName = InspectedSource != nullptr ? *InspectedSource : FString();
	const FString DisplayNamespace = FTextInspector::GetNamespace(DisplayName).Get(FString());
	const FString DisplayKey = FTextInspector::GetKey(DisplayName).Get(FString());
	InOutCanonicalData += FString::Printf(
		TEXT("id=%s\nschema=%d\nrevision=%d\ndisplaySource=%s\ndisplayNamespace=%s\ndisplayKey=%s\nlocalization=%s\ncategory=%s\ncontentSet=%s\ndeprecated=%d\nreplacement=%s\n"),
		*StableDefinitionId,
		SchemaVersion,
		AuthoredRevision,
		*SourceDisplayName,
		*DisplayNamespace,
		*DisplayKey,
		*LocalizationKey.ToString(),
		*DefinitionCategory.ToString(),
		*ContentSet.ToString(),
		bDeprecated ? 1 : 0,
		*ReplacementDefinitionId);
	for (const FString& Tag : SortedTags)
	{
		InOutCanonicalData += TEXT("tag=") + Tag + TEXT("\n");
	}
}

#if WITH_EDITOR
void UHansaDefinitionBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshContentHash();
}

void UHansaDefinitionBase::PreSave(FObjectPreSaveContext SaveContext)
{
	RefreshContentHash();
	Super::PreSave(SaveContext);
}
#endif
