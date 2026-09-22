#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "World/HansaCompoundPresentation.h"
#include "GameFramework/Actor.h"

#include "Model/HansaIds.h"

namespace Hansa::Game::EconomicDefinitions
{
	void AddIssue(
		TArray<FHansaDefinitionValidationIssue>& OutIssues,
		const FName Code,
		const FString& PropertyPath,
		const FText& Cause,
		const FText& Remedy)
	{
		OutIssues.Add(FHansaDefinitionValidationIssue {
			EHansaDefinitionValidationSeverity::Error,
			Code,
			PropertyPath,
			Cause,
			Remedy
		});
	}

	bool HasValidDomain(const FString& StableId, const FString& ExpectedDomain)
	{
		const Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaDefinitionId> Parsed =
			Hansa::Simulation::FHansaDefinitionId::TryParse(StableId);
		return Parsed && Parsed.Value.GetDomain() == ExpectedDomain;
	}

	void ValidateAmounts(
		const TArray<FHansaGoodAmount>& Amounts,
		const TCHAR* PropertyName,
		TArray<FHansaDefinitionValidationIssue>& OutIssues)
	{
		TSet<FString> SeenGoodIds;
		for (int32 Index = 0; Index < Amounts.Num(); ++Index)
		{
			const FHansaGoodAmount& Amount = Amounts[Index];
			const FString Path = FString::Printf(TEXT("%s[%d]"), PropertyName, Index);
			if (!HasValidDomain(Amount.GoodId, TEXT("Good")))
			{
				AddIssue(
					OutIssues,
					TEXT("HSA-ECO-001"),
					Path + TEXT(".GoodId"),
					NSLOCTEXT("HansaEconomicDefinition", "InvalidGoodReference", "The amount does not reference a canonical Good.* identity."),
					NSLOCTEXT("HansaEconomicDefinition", "InvalidGoodReferenceRemedy", "Select an existing Good.* stable ID from the economic registry."));
			}
			if (Amount.QuantityMilliUnits <= 0)
			{
				AddIssue(
					OutIssues,
					TEXT("HSA-ECO-002"),
					Path + TEXT(".QuantityMilliUnits"),
					NSLOCTEXT("HansaEconomicDefinition", "InvalidGoodQuantity", "Good quantities must be positive."),
					NSLOCTEXT("HansaEconomicDefinition", "InvalidGoodQuantityRemedy", "Enter a quantity of at least one milli-unit."));
			}
			if (SeenGoodIds.Contains(Amount.GoodId))
			{
				AddIssue(
					OutIssues,
					TEXT("HSA-ECO-003"),
					Path + TEXT(".GoodId"),
					NSLOCTEXT("HansaEconomicDefinition", "DuplicateGoodAmount", "A good appears more than once in the same amount list."),
					NSLOCTEXT("HansaEconomicDefinition", "DuplicateGoodAmountRemedy", "Combine duplicate entries into one deterministic quantity."));
			}
			SeenGoodIds.Add(Amount.GoodId);
		}
	}

	void AppendSortedAmounts(FString& InOutCanonicalData, const TCHAR* Prefix, const TArray<FHansaGoodAmount>& Amounts)
	{
		TArray<FHansaGoodAmount> Sorted = Amounts;
		Sorted.Sort([](const FHansaGoodAmount& Left, const FHansaGoodAmount& Right)
		{
			return Left.GoodId.Compare(Right.GoodId, ESearchCase::CaseSensitive) < 0;
		});
		for (const FHansaGoodAmount& Amount : Sorted)
		{
			InOutCanonicalData += FString::Printf(
				TEXT("%s=%s:%lld\n"),
				Prefix,
				*Amount.GoodId,
				static_cast<long long>(Amount.QuantityMilliUnits));
		}
	}
}

UHansaGoodDefinition::UHansaGoodDefinition()
{
	DefinitionCategory = TEXT("Goods");
	LocalizationKey = TEXT("Game.Good.Unnamed");
}

void UHansaGoodDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	using namespace Hansa::Game::EconomicDefinitions;
	if (!HasValidDomain(StableDefinitionId, TEXT("Good")))
	{
		AddIssue(OutIssues, TEXT("HSA-GOOD-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaEconomicDefinition", "GoodWrongDomain", "A good definition requires a Good.* stable ID."),
			NSLOCTEXT("HansaEconomicDefinition", "GoodWrongDomainRemedy", "Assign a canonical Good.* stable identity."));
	}
	if (BaseValueMilliMarks <= 0 || PriceElasticityBasisPoints < 0 || PriceElasticityBasisPoints > 50000 ||
		SpoilageBasisPointsPerDay < 0 || SpoilageBasisPointsPerDay > 10000)
	{
		AddIssue(OutIssues, TEXT("HSA-GOOD-002"), TEXT("BaseValueMilliMarks"),
			NSLOCTEXT("HansaEconomicDefinition", "GoodMarketRange", "One or more market values are outside their deterministic authored range."),
			NSLOCTEXT("HansaEconomicDefinition", "GoodMarketRangeRemedy", "Use a positive base value, 0–50000 elasticity, and 0–10000 spoilage basis points."));
	}
}

void UHansaGoodDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	const FString UnitName = StaticEnum<EHansaGoodUnit>()->GetNameStringByValue(static_cast<int64>(QuantityUnit));
	InOutCanonicalData += FString::Printf(
		TEXT("unit=%s\nbaseValueMilliMarks=%lld\nelasticityBasisPoints=%d\nspoilageBasisPointsPerDay=%d\nicon=%s\n"),
		*UnitName,
		static_cast<long long>(BaseValueMilliMarks),
		PriceElasticityBasisPoints,
		SpoilageBasisPointsPerDay,
		*Icon.ToSoftObjectPath().ToString());
    if (bSpoilageEnabled) InOutCanonicalData += TEXT("|spoilage-enabled=1");
}

UHansaRecipeDefinition::UHansaRecipeDefinition()
{
	DefinitionCategory = TEXT("Recipes");
	LocalizationKey = TEXT("Game.Recipe.Unnamed");
}

void UHansaRecipeDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	using namespace Hansa::Game::EconomicDefinitions;
	if (!HasValidDomain(StableDefinitionId, TEXT("Recipe")))
	{
		AddIssue(OutIssues, TEXT("HSA-RECIPE-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeWrongDomain", "A recipe definition requires a Recipe.* stable ID."),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeWrongDomainRemedy", "Assign a canonical Recipe.* stable identity."));
	}
	ValidateAmounts(Inputs, TEXT("Inputs"), OutIssues);
	ValidateAmounts(Outputs, TEXT("Outputs"), OutIssues);
	if (CycleTicks <= 0 || LaborerWorkforce < 0 || ArtisanWorkforce < 0)
	{
		AddIssue(OutIssues, TEXT("HSA-RECIPE-002"), TEXT("CycleTicks"),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeTimingWorkforce", "Cycle time must be positive and workforce values cannot be negative."),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeTimingWorkforceRemedy", "Set at least one cycle tick and non-negative workforce placeholders."));
	}
	if (Outputs.IsEmpty() && !bDeclaredSink)
	{
		AddIssue(OutIssues, TEXT("HSA-RECIPE-003"), TEXT("Outputs"),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeMissingOutput", "A recipe needs a positive output unless it is an explicit sink."),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeMissingOutputRemedy", "Add an output or deliberately mark the recipe as a sink."));
	}
	if (Inputs.IsEmpty() != bDeclaredSource)
	{
		AddIssue(OutIssues, TEXT("HSA-RECIPE-004"), TEXT("bDeclaredSource"),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeSourceMismatch", "Input-free recipes must be declared sources, and source recipes must not consume stored goods."),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeSourceMismatchRemedy", "Align the source flag with the recipe input list."));
	}
	if (!Outputs.IsEmpty() && bDeclaredSink)
	{
		AddIssue(OutIssues, TEXT("HSA-RECIPE-005"), TEXT("bDeclaredSink"),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeSinkMismatch", "A declared sink cannot also produce stored goods."),
			NSLOCTEXT("HansaEconomicDefinition", "RecipeSinkMismatchRemedy", "Clear the sink flag or remove the outputs."));
	}
}

void UHansaRecipeDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	using namespace Hansa::Game::EconomicDefinitions;
	AppendSortedAmounts(InOutCanonicalData, TEXT("input"), Inputs);
	AppendSortedAmounts(InOutCanonicalData, TEXT("output"), Outputs);
	InOutCanonicalData += FString::Printf(
		TEXT("cycleTicks=%d\nlaborers=%d\nartisans=%d\nsource=%d\nsink=%d\n"),
		CycleTicks,
		LaborerWorkforce,
		ArtisanWorkforce,
		bDeclaredSource ? 1 : 0,
		bDeclaredSink ? 1 : 0);
    if (!InternalCatchRecipeId.IsEmpty()) InOutCanonicalData += TEXT("|internal-catch=") + InternalCatchRecipeId;
}

UHansaBuildingDefinition::UHansaBuildingDefinition()
{
	SchemaVersion = 5;
	DefinitionCategory = TEXT("Buildings");
	LocalizationKey = TEXT("Game.Building.Unnamed");
	PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
}

UHansaResidentialCompoundDefinition* UHansaBuildingDefinition::LoadResidentialCompound() const
{
 const FString Path=ResidentialCompound.ToSoftObjectPath().ToString();
 if(Path.Contains(TEXT("/Developer/"))||Path.Contains(TEXT("/Generated/Staging/")))return nullptr;
 return ResidentialCompound.LoadSynchronous();
}
UClass* UHansaBuildingDefinition::LoadPresentationActorClass() const
{
 if(!ResidentialCompound.IsNull())return AHansaCompoundPresentation::StaticClass();
	const FString Path = PresentationActorClass.ToSoftObjectPath().ToString();
	if (Path.Contains(TEXT("/Generated/Staging/")) || Path.Contains(TEXT("/Developer/"))) return nullptr;
	UClass* Class = PresentationActorClass.LoadSynchronous();
	return Class != nullptr && Class->IsChildOf(AActor::StaticClass()) &&
		!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ? Class : nullptr;
}

void UHansaBuildingDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	using namespace Hansa::Game::EconomicDefinitions;
	if (!HasValidDomain(StableDefinitionId, TEXT("Building")))
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingWrongDomain", "A building definition requires a Building.* stable ID."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingWrongDomainRemedy", "Assign a canonical Building.* stable identity."));
	}
	ValidateAmounts(ConstructionCosts, TEXT("ConstructionCosts"), OutIssues);
	if (ConstructionCostPfennig < 0 || CancellationRefundBasisPoints < 0 ||
		CancellationRefundBasisPoints > 10000)
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-007"), TEXT("ConstructionCostPfennig"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingInvalidConstructionMoney", "Building currency cost must be non-negative and cancellation refund must be between 0 and 10000 basis points."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingInvalidConstructionMoneyRemedy", "Use a non-negative Pfennig cost and a refund percentage from 0% through 100%."));
	}
	if (FootprintWidthCells <= 0 || FootprintHeightCells <= 0 ||
		FootprintWidthCells > 64 || FootprintHeightCells > 64 || BuildTicks <= 0)
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-002"), TEXT("FootprintWidthCells"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingFootprintTiming", "Building footprint dimensions and build time must be positive and within the authored grid range."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingFootprintTimingRemedy", "Use 1–64 grid cells per dimension and at least one build tick."));
	}
	if (StorageCapacityMilliUnits < 0 || ResidenceCapacity < 0 || LaborerWorkforce < 0 || ArtisanWorkforce < 0)
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-003"), TEXT("StorageCapacityMilliUnits"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingNegativeCapacity", "Building capacities and workforce placeholders cannot be negative."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingNegativeCapacityRemedy", "Set capacities and workforce placeholders to zero or a positive value."));
	}
	if ((ResidenceCapacity > 0 && !HasValidDomain(ResidentPopulationTierId, TEXT("PopulationTier"))) ||
		(ResidenceCapacity == 0 && !ResidentPopulationTierId.IsEmpty()))
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-008"), TEXT("ResidentPopulationTierId"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingResidenceTier", "A residence requires a PopulationTier.* identity and a non-residence must not host a population tier."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingResidenceTierRemedy", "Select the tier hosted by a positive-capacity residence, or clear the tier on a non-residence."));
	}

	TSet<FString> SeenRecipeIds;
	for (int32 Index = 0; Index < RecipeIds.Num(); ++Index)
	{
		const FString& RecipeId = RecipeIds[Index];
		if (!HasValidDomain(RecipeId, TEXT("Recipe")) || SeenRecipeIds.Contains(RecipeId))
		{
			AddIssue(OutIssues, TEXT("HSA-BUILDING-004"), FString::Printf(TEXT("RecipeIds[%d]"), Index),
				NSLOCTEXT("HansaEconomicDefinition", "BuildingInvalidRecipe", "A building recipe reference is invalid or duplicated."),
				NSLOCTEXT("HansaEconomicDefinition", "BuildingInvalidRecipeRemedy", "Select each canonical Recipe.* identity once."));
		}
		SeenRecipeIds.Add(RecipeId);
	}
	if (!UpgradeTargetBuildingId.IsEmpty() && !HasValidDomain(UpgradeTargetBuildingId, TEXT("Building")))
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-005"), TEXT("UpgradeTargetBuildingId"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingInvalidUpgrade", "The optional upgrade target is not a canonical Building.* identity."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingInvalidUpgradeRemedy", "Select an existing Building.* identity or leave the target empty."));
	}
	if (!PresentationActorClass.IsNull() && LoadPresentationActorClass() == nullptr)
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-009"), TEXT("PresentationActorClass"),
			NSLOCTEXT("HansaEconomicDefinition", "InvalidPresentationActor", "The presentation actor must be an existing concrete promoted Actor class."),
			NSLOCTEXT("HansaEconomicDefinition", "InvalidPresentationActorRemedy", "Choose a production Actor Blueprint or clear it to use the presentation mesh."));
	}
	if (bShowInConstructionMenu && ConstructionMenuOrder < 0)
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-010"), TEXT("ConstructionMenuOrder"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingConstructionOrder", "A visible construction card requires a non-negative menu order."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingConstructionOrderRemedy", "Assign a stable zero-based order within its category or chain."));
	}
	if (!StaticEnum<EHansaConstructionTier>()->IsValidEnumValue(static_cast<int64>(ConstructionTier)))
    {
        AddIssue(OutIssues, TEXT("HSA-BUILDING-CONSTRUCTION-TIER"), TEXT("ConstructionTier"),
            NSLOCTEXT("HansaEconomicDefinition", "InvalidConstructionTier", "Construction tier is invalid."),
            NSLOCTEXT("HansaEconomicDefinition", "InvalidConstructionTierRemedy", "Select Day Laborers, Craftsmen, Merchants or the legacy compatibility value."));
    }
    const bool bProductionCard = bShowInConstructionMenu && ConstructionMenuCategory == EHansaConstructionMenuCategory::Production;
	const bool bHasChain = !ConstructionChainOutputGoodId.IsEmpty();
	if (bProductionCard != bHasChain || (bHasChain &&
		(!HasValidDomain(ConstructionChainOutputGoodId, TEXT("Good")) || ConstructionChainStage < 1 ||
		 ConstructionChainStageCount < 1 || ConstructionChainStage > ConstructionChainStageCount)) ||
		(!bHasChain && (ConstructionChainStage != 0 || ConstructionChainStageCount != 0)))
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-011"), TEXT("ConstructionChainOutputGoodId"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingConstructionChain", "A visible Production card requires a valid Good.* chain output and a bounded one-based stage; non-production cards must not declare chain stages."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingConstructionChainRemedy", "Assign the final chain output and consistent stage/count metadata, or clear all chain fields outside Production."));
	}
	if (!RequiredConstructionTechnologyId.IsEmpty() &&
		!HasValidDomain(RequiredConstructionTechnologyId, TEXT("Technology")))
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-012"), TEXT("RequiredConstructionTechnologyId"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingConstructionTechnology", "The construction unlock is not a canonical Technology.* identity."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingConstructionTechnologyRemedy", "Reference an existing Technology.* definition or leave the field empty."));
	}
 if (MaximumMarketRoadDistanceCells < 2 || MaximumMarketRoadDistanceCells > 4096)
 {
  AddIssue(OutIssues, TEXT("HSA-BUILDING-MARKET-RANGE"), TEXT("MaximumMarketRoadDistanceCells"),
   NSLOCTEXT("HansaEconomicDefinition", "MarketRange", "Market road distance must be between 2 and 4096 cells."),
   NSLOCTEXT("HansaEconomicDefinition", "MarketRangeRemedy", "Set a bounded road-distance limit, including both entrance steps."));
 }
	if (bProvidesMarketAccess && (!bRequiresRoad || StorageCapacityMilliUnits <= 0))
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-013"), TEXT("bProvidesMarketAccess"),
			NSLOCTEXT("HansaEconomicDefinition", "MarketAccessProviderContract", "A physical market-access provider requires road adjacency and positive storage capacity."),
			NSLOCTEXT("HansaEconomicDefinition", "MarketAccessProviderRemedy", "Enable Requires road and give the market hub positive storage capacity, or disable Provides market access."));
	}
	if (SchemaVersion >= 5 && !RecipeIds.IsEmpty() && !bRequiresRoad)
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-014"), TEXT("bRequiresRoad"),
			NSLOCTEXT("HansaEconomicDefinition", "ProductionRoadAccessContract", "A production building must require road access so its external input and output transfers use a physical market network."),
			NSLOCTEXT("HansaEconomicDefinition", "ProductionRoadAccessRemedy", "Enable Requires road, then connect the completed building to an operational market."));
	}
 if (!ResidentialCompound.IsNull())
 {
  const auto* Compound=LoadResidentialCompound();
  if(!Compound || Compound->FootprintWidthCells!=FootprintWidthCells || Compound->FootprintHeightCells!=FootprintHeightCells ||
    Compound->PopulationTierId!=ResidentPopulationTierId || ResidenceCapacity<=0 || !bRequiresRoad || CompoundStage<1 || CompoundStage>3 || !PresentationActorClass.IsNull())
   AddIssue(OutIssues,TEXT("HSA-BUILDING-COMPOUND"),TEXT("ResidentialCompound"),FText::FromString(TEXT("Compound must resolve, match the logical footprint and resident tier, require a road and replace the optional actor class.")),FText::FromString(TEXT("Author a larger building footprint explicitly; never shrink child meshes or expand occupied legacy parcels silently.")));
  if(Compound)
  {
   Compound->ValidateDefinition(OutIssues);
   if(!Compound->Layouts.ContainsByPredicate([this](const FHansaCompoundLayout& L){return L.DevelopmentStage==CompoundStage&&(L.DistrictIds.IsEmpty()||L.DistrictIds.Contains(CompoundDistrictId));}))
    AddIssue(OutIssues,TEXT("HSA-BUILDING-COMPOUND-STAGE"),TEXT("CompoundStage"),FText::FromString(TEXT("No eligible compound layout for the configured stage/district.")),FText::FromString(TEXT("Supply an eligible reviewed layout.")));
  }
 }
	if (PresentationMesh.IsNull() && PresentationActorClass.IsNull() && ResidentialCompound.IsNull())
	{
		AddIssue(OutIssues, TEXT("HSA-BUILDING-006"), TEXT("PresentationMesh"),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingMissingMesh", "A building needs a promoted or explicit placeholder presentation mesh."),
			NSLOCTEXT("HansaEconomicDefinition", "BuildingMissingMeshRemedy", "Assign a production mesh or the reviewed engine placeholder until art is promoted."));
	}
}

void UHansaBuildingDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	using namespace Hansa::Game::EconomicDefinitions;
	AppendSortedAmounts(InOutCanonicalData, TEXT("cost"), ConstructionCosts);
	TArray<FString> SortedRecipeIds = RecipeIds;
	SortedRecipeIds.Sort();
	for (const FString& RecipeId : SortedRecipeIds)
	{
		InOutCanonicalData += TEXT("recipe=") + RecipeId + TEXT("\n");
	}
	InOutCanonicalData += FString::Printf(
		TEXT("currencyCostPfennig=%lld\ncancellationRefundBasisPoints=%d\nupgrade=%s\nfootprint=%dx%d\nbuildTicks=%d\nstorage=%d\nresidents=%d\nresidentTier=%s\nlaborers=%d\nartisans=%d\nrequiresRoad=%d\nrequiresShoreline=%d\nmesh=%s\n"),
		static_cast<long long>(ConstructionCostPfennig),
		CancellationRefundBasisPoints,
		*UpgradeTargetBuildingId,
		FootprintWidthCells,
		FootprintHeightCells,
		BuildTicks,
		StorageCapacityMilliUnits,
		ResidenceCapacity,
		*ResidentPopulationTierId,
		LaborerWorkforce,
		ArtisanWorkforce,
		bRequiresRoad ? 1 : 0,
		bRequiresShoreline ? 1 : 0,
		*PresentationMesh.ToSoftObjectPath().ToString());
 if (!ResidentialCompound.IsNull())
 {
  const auto* Compound=LoadResidentialCompound();
  InOutCanonicalData+=FString::Printf(TEXT("compound=%s/%llu/%d/%s\n"),Compound?*Compound->StableDefinitionId:*ResidentialCompound.ToSoftObjectPath().ToString(),
   static_cast<unsigned long long>(Compound?Compound->ComputeDeterministicContentHash():0),CompoundStage,*CompoundDistrictId);
 }
	// The additive default preserves historical catalog hashes and save lineages.
    if (ConstructionTier != EHansaConstructionTier::Legacy)
        InOutCanonicalData += TEXT("constructionTier=") + StaticEnum<EHansaConstructionTier>()->GetNameStringByValue(static_cast<int64>(ConstructionTier)) + TEXT("\n");
    // Empty is the compatible default: legacy mesh-only content keeps its existing hash.
	if (!PresentationActorClass.IsNull())
	{
		InOutCanonicalData += TEXT("presentationActorClass=") + PresentationActorClass.ToSoftObjectPath().ToString() + TEXT("\n");
	}
	// Version 2 assets predate the construction presentation model. Keeping their canonical
	// serialization unchanged makes the explicit v2 -> v3 catalog migration reproducible.
	if (SchemaVersion >= 3)
	{
		InOutCanonicalData += FString::Printf(
			TEXT("constructionMenuVisible=%d\nconstructionCategory=%s\nconstructionOrder=%d\nconstructionChain=%s\nconstructionChainStage=%d\nconstructionChainStageCount=%d\nrequiredConstructionTechnology=%s\nupgradeOnly=%d\nconstructionPurpose=%s\n"),
			bShowInConstructionMenu ? 1 : 0,
			*StaticEnum<EHansaConstructionMenuCategory>()->GetNameStringByValue(static_cast<int64>(ConstructionMenuCategory)),
			ConstructionMenuOrder,
			*ConstructionChainOutputGoodId,
			ConstructionChainStage,
			ConstructionChainStageCount,
			*RequiredConstructionTechnologyId,
			bUpgradeOnly ? 1 : 0,
			*ConstructionPresentationPurpose.ToString());
	}
	if (SchemaVersion >= 4)
	{
		InOutCanonicalData += FString::Printf(TEXT("providesMarketAccess=%d\n"), bProvidesMarketAccess ? 1 : 0);
	}
 // Additive default preserves the legacy catalog lineage; authored overrides participate in hashing.
 if (MaximumMarketRoadDistanceCells != 40)
  InOutCanonicalData += FString::Printf(TEXT("maximumMarketRoadDistanceCells=%d\n"), MaximumMarketRoadDistanceCells);
}
