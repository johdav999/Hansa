#include "Definitions/HansaEconomicDefinitionCompiler.h"

#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"

namespace Hansa::Game::EconomicCompiler
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

	TArray<Hansa::Simulation::FHansaCompiledGoodAmount> CompileAmounts(const TArray<FHansaGoodAmount>& Amounts)
	{
		TArray<Hansa::Simulation::FHansaCompiledGoodAmount> Result;
		Result.Reserve(Amounts.Num());
		for (const FHansaGoodAmount& Amount : Amounts)
		{
			Hansa::Simulation::FHansaCompiledGoodAmount Compiled;
			Compiled.GoodId = Amount.GoodId;
			Compiled.QuantityMilliUnits = Amount.QuantityMilliUnits;
			Result.Add(MoveTemp(Compiled));
		}
		Result.Sort([](const Hansa::Simulation::FHansaCompiledGoodAmount& Left, const Hansa::Simulation::FHansaCompiledGoodAmount& Right)
		{
			return Left.GoodId.Compare(Right.GoodId, ESearchCase::CaseSensitive) < 0;
		});
		return Result;
	}

	void ValidateProductionGraph(
		const TArray<const UHansaDefinitionBase*>& Definitions,
		const TSet<FString>& GoodIds,
		TArray<FHansaDefinitionValidationIssue>& OutIssues)
	{
		TArray<const UHansaRecipeDefinition*> Recipes;
		TMap<FString, TArray<FString>> ProducersByGood;
		TSet<FString> ProducedGoods;
		for (const UHansaDefinitionBase* Definition : Definitions)
		{
			if (const UHansaRecipeDefinition* Recipe = Cast<UHansaRecipeDefinition>(Definition))
			{
				Recipes.Add(Recipe);
				TSet<FString> InputGoods;
				for (const FHansaGoodAmount& Input : Recipe->Inputs)
				{
					InputGoods.Add(Input.GoodId);
				}
				for (const FHansaGoodAmount& Output : Recipe->Outputs)
				{
					ProducedGoods.Add(Output.GoodId);
					ProducersByGood.FindOrAdd(Output.GoodId).Add(Recipe->StableDefinitionId);
					if (InputGoods.Contains(Output.GoodId))
					{
						AddIssue(OutIssues, TEXT("HSA-REGISTRY-008"), Recipe->StableDefinitionId + TEXT(".Outputs"),
							FText::Format(NSLOCTEXT("HansaEconomicCompiler", "UnconservedRecipeGood", "Recipe consumes and produces {0}, so its net inventory creation or destruction is ambiguous."), FText::FromString(Output.GoodId)),
							NSLOCTEXT("HansaEconomicCompiler", "UnconservedRecipeGoodRemedy", "Split the transformation into explicit recipes or move intentional creation/destruction to a declared source or sink boundary."));
					}
				}
			}
		}

		TMap<FString, TArray<FString>> Edges;
		for (const UHansaRecipeDefinition* Consumer : Recipes)
		{
			for (const FHansaGoodAmount& Input : Consumer->Inputs)
			{
				if (const TArray<FString>* Producers = ProducersByGood.Find(Input.GoodId))
				{
					for (const FString& Producer : *Producers)
					{
						Edges.FindOrAdd(Producer).AddUnique(Consumer->StableDefinitionId);
					}
				}
			}
		}
		for (TPair<FString, TArray<FString>>& Pair : Edges)
		{
			Pair.Value.Sort();
		}

		TMap<FString, uint8> VisitState;
		TSet<FString> CycleReported;
		TFunction<void(const FString&)> Visit = [&](const FString& RecipeId)
		{
			VisitState.Add(RecipeId, 1);
			if (const TArray<FString>* Targets = Edges.Find(RecipeId))
			{
				for (const FString& Target : *Targets)
				{
					const uint8 TargetState = VisitState.FindRef(Target);
					if (TargetState == 0)
					{
						Visit(Target);
					}
					else if (TargetState == 1 && !CycleReported.Contains(Target))
					{
						CycleReported.Add(Target);
						AddIssue(OutIssues, TEXT("HSA-REGISTRY-009"), Target + TEXT(".Inputs"),
							FText::Format(NSLOCTEXT("HansaEconomicCompiler", "RecipeDependencyCycle", "Recipe dependency graph contains a cycle through {0} and {1}."), FText::FromString(RecipeId), FText::FromString(Target)),
							NSLOCTEXT("HansaEconomicCompiler", "RecipeDependencyCycleRemedy", "Break the cycle with an explicit external/source input or redesign the recipes as an acyclic production chain."));
					}
				}
			}
			VisitState.Add(RecipeId, 2);
		};
		for (const UHansaRecipeDefinition* Recipe : Recipes)
		{
			if (VisitState.FindRef(Recipe->StableDefinitionId) == 0)
			{
				Visit(Recipe->StableDefinitionId);
			}
		}

		// Goods without an in-registry producer are deliberate external stock boundaries.
		TSet<FString> ReachableGoods;
		for (const FString& GoodId : GoodIds)
		{
			if (!ProducedGoods.Contains(GoodId))
			{
				ReachableGoods.Add(GoodId);
			}
		}
		TSet<FString> ReachableRecipes;
		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			for (const UHansaRecipeDefinition* Recipe : Recipes)
			{
				if (ReachableRecipes.Contains(Recipe->StableDefinitionId))
				{
					continue;
				}
				const bool bInputsReachable = Recipe->Inputs.ContainsByPredicate([&ReachableGoods](const FHansaGoodAmount& Input)
				{
					return !ReachableGoods.Contains(Input.GoodId);
				}) == false;
				if (bInputsReachable)
				{
					ReachableRecipes.Add(Recipe->StableDefinitionId);
					for (const FHansaGoodAmount& Output : Recipe->Outputs)
					{
						ReachableGoods.Add(Output.GoodId);
					}
					bChanged = true;
				}
			}
		}
		for (const UHansaRecipeDefinition* Recipe : Recipes)
		{
			if (!ReachableRecipes.Contains(Recipe->StableDefinitionId))
			{
				AddIssue(OutIssues, TEXT("HSA-REGISTRY-010"), Recipe->StableDefinitionId + TEXT(".Inputs"),
					NSLOCTEXT("HansaEconomicCompiler", "UnreachableRecipe", "Recipe cannot be reached from any explicit source or externally supplied good in this registry."),
					NSLOCTEXT("HansaEconomicCompiler", "UnreachableRecipeRemedy", "Add a declared source or a good with an external supply boundary, or repair the recipe dependency chain."));
			}
		}
	}
}

FHansaEconomicRegistryCompileResult FHansaEconomicDefinitionCompiler::Compile(
	const TArray<const UHansaDefinitionBase*>& Definitions)
{
	using namespace Hansa::Game::EconomicCompiler;
	FHansaEconomicRegistryCompileResult Result;
	TArray<const UHansaDefinitionBase*> SortedDefinitions = Definitions;
	SortedDefinitions.RemoveAll([](const UHansaDefinitionBase* Definition) { return Definition == nullptr; });
	SortedDefinitions.Sort([](const UHansaDefinitionBase& Left, const UHansaDefinitionBase& Right)
	{
		const int32 IdOrder = Left.StableDefinitionId.Compare(Right.StableDefinitionId, ESearchCase::CaseSensitive);
		return IdOrder != 0 ? IdOrder < 0 : Left.GetClass()->GetPathName() < Right.GetClass()->GetPathName();
	});

	TSet<FString> AllIds;
	TSet<FString> GoodIds;
	TSet<FString> RecipeIds;
	TSet<FString> BuildingIds;
	TSet<FString> NeedIds;
	TSet<FString> PopulationTierIds;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		Definition->ValidateDefinition(Result.Issues);
		if (AllIds.Contains(Definition->StableDefinitionId))
		{
			AddIssue(Result.Issues, TEXT("HSA-REGISTRY-001"), Definition->StableDefinitionId,
				NSLOCTEXT("HansaEconomicCompiler", "DuplicateStableId", "The economic definition set contains a duplicate stable ID."),
				NSLOCTEXT("HansaEconomicCompiler", "DuplicateStableIdRemedy", "Keep one accepted definition per globally unique stable ID."));
		}
		AllIds.Add(Definition->StableDefinitionId);

		if (Definition->IsA<UHansaGoodDefinition>())
		{
			GoodIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaRecipeDefinition>())
		{
			RecipeIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaBuildingDefinition>())
		{
			BuildingIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaNeedDefinition>())
		{
			NeedIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaPopulationTierDefinition>())
		{
			PopulationTierIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaCityMarketProfileDefinition>())
		{
		}
		else
		{
			AddIssue(Result.Issues, TEXT("HSA-REGISTRY-002"), Definition->StableDefinitionId,
				NSLOCTEXT("HansaEconomicCompiler", "UnsupportedDefinition", "The economic compiler received a non-economic definition type."),
				NSLOCTEXT("HansaEconomicCompiler", "UnsupportedDefinitionRemedy", "Compile only good, recipe, building, need, population-tier and city-market definitions in this registry."));
		}
	}

	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		if (const UHansaRecipeDefinition* Recipe = Cast<UHansaRecipeDefinition>(Definition))
		{
			for (const FHansaGoodAmount& Amount : Recipe->Inputs)
			{
				if (!GoodIds.Contains(Amount.GoodId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-003"), Recipe->StableDefinitionId + TEXT(".Inputs"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingInputGood", "Recipe input references missing good {0}."), FText::FromString(Amount.GoodId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingInputGoodRemedy", "Add the referenced Good definition to the same content set or correct the stable reference."));
				}
			}
			for (const FHansaGoodAmount& Amount : Recipe->Outputs)
			{
				if (!GoodIds.Contains(Amount.GoodId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-004"), Recipe->StableDefinitionId + TEXT(".Outputs"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingOutputGood", "Recipe output references missing good {0}."), FText::FromString(Amount.GoodId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingOutputGoodRemedy", "Add the referenced Good definition to the same content set or correct the stable reference."));
				}
			}
		}
		else if (const UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(Definition))
		{
			if (Building->ConstructionCostPfennig < 0 || Building->CancellationRefundBasisPoints < 0 ||
				Building->CancellationRefundBasisPoints > 10000)
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-018"), Building->StableDefinitionId + TEXT(".ConstructionCostPfennig"),
					NSLOCTEXT("HansaEconomicCompiler", "InvalidConstructionRefund", "Building currency cost or cancellation refund is outside the deterministic range."),
					NSLOCTEXT("HansaEconomicCompiler", "InvalidConstructionRefundRemedy", "Use a non-negative Pfennig cost and a cancellation refund from 0 through 10000 basis points."));
			}
			for (const FHansaGoodAmount& Cost : Building->ConstructionCosts)
			{
				if (!GoodIds.Contains(Cost.GoodId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-005"), Building->StableDefinitionId + TEXT(".ConstructionCosts"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingCostGood", "Building construction cost references missing good {0}."), FText::FromString(Cost.GoodId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingCostGoodRemedy", "Add the referenced Good definition or correct the construction-cost stable reference."));
				}
			}
			for (const FString& RecipeId : Building->RecipeIds)
			{
				if (!RecipeIds.Contains(RecipeId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-006"), Building->StableDefinitionId + TEXT(".RecipeIds"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingBuildingRecipe", "Building references missing recipe {0}."), FText::FromString(RecipeId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingBuildingRecipeRemedy", "Add the referenced Recipe definition or correct the stable reference."));
				}
			}
			if (!Building->UpgradeTargetBuildingId.IsEmpty() && !BuildingIds.Contains(Building->UpgradeTargetBuildingId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-007"), Building->StableDefinitionId + TEXT(".UpgradeTargetBuildingId"),
					FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingUpgradeBuilding", "Upgrade target {0} is missing."), FText::FromString(Building->UpgradeTargetBuildingId)),
					NSLOCTEXT("HansaEconomicCompiler", "MissingUpgradeBuildingRemedy", "Add the target Building definition or clear/correct the upgrade reference."));
			}
			if ((Building->ResidenceCapacity > 0 && !PopulationTierIds.Contains(Building->ResidentPopulationTierId)) ||
				(Building->ResidenceCapacity == 0 && !Building->ResidentPopulationTierId.IsEmpty()))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-019"), Building->StableDefinitionId + TEXT(".ResidentPopulationTierId"),
					NSLOCTEXT("HansaEconomicCompiler", "InvalidResidenceTier", "Residence capacity and resident population-tier linkage are inconsistent or reference a missing tier."),
					NSLOCTEXT("HansaEconomicCompiler", "InvalidResidenceTierRemedy", "Assign an existing PopulationTier.* to each positive-capacity residence and clear it on non-residences."));
			}
			if (!Building->UpgradeTargetBuildingId.IsEmpty() && BuildingIds.Contains(Building->UpgradeTargetBuildingId))
			{
				const UHansaBuildingDefinition* Target = nullptr;
				for (const UHansaDefinitionBase* Candidate : SortedDefinitions)
				{
					if (Candidate->StableDefinitionId == Building->UpgradeTargetBuildingId)
					{
						Target = Cast<UHansaBuildingDefinition>(Candidate);
						break;
					}
				}
				const UHansaPopulationTierDefinition* TargetTier = nullptr;
				if (Target != nullptr)
				{
					for (const UHansaDefinitionBase* Candidate : SortedDefinitions)
					{
						if (Candidate->StableDefinitionId == Target->ResidentPopulationTierId)
						{
							TargetTier = Cast<UHansaPopulationTierDefinition>(Candidate);
							break;
						}
					}
				}
				if (Target == nullptr || Building->ResidenceCapacity <= 0 || Target->ResidenceCapacity <= 0 ||
					Building->FootprintWidthCells != Target->FootprintWidthCells ||
					Building->FootprintHeightCells != Target->FootprintHeightCells ||
					TargetTier == nullptr || TargetTier->PreviousTierId != Building->ResidentPopulationTierId)
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-020"), Building->StableDefinitionId + TEXT(".UpgradeTargetBuildingId"),
						NSLOCTEXT("HansaEconomicCompiler", "InvalidResidenceProgression", "A residence upgrade must preserve its footprint and advance to the directly linked population tier."),
						NSLOCTEXT("HansaEconomicCompiler", "InvalidResidenceProgressionRemedy", "Choose a same-footprint residence whose hosted tier names this residence tier as its previous tier."));
				}
			}
		}
		else if (const UHansaNeedDefinition* Need = Cast<UHansaNeedDefinition>(Definition))
		{
			if (Need->Kind == EHansaNeedKind::Good && !GoodIds.Contains(Need->GoodId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-011"), Need->StableDefinitionId + TEXT(".GoodId"),
					FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingNeedGood", "Population need references missing good {0}."), FText::FromString(Need->GoodId)),
					NSLOCTEXT("HansaEconomicCompiler", "MissingNeedGoodRemedy", "Add the referenced Good definition or correct the need's stable reference."));
			}
		}
		else if (const UHansaPopulationTierDefinition* Tier = Cast<UHansaPopulationTierDefinition>(Definition))
		{
			for (const FHansaPopulationTierNeed& Requirement : Tier->Needs)
			{
				const UHansaNeedDefinition* NeedDefinition = nullptr;
				for (const UHansaDefinitionBase* Candidate : SortedDefinitions)
				{
					if (Candidate->StableDefinitionId == Requirement.NeedId)
					{
						NeedDefinition = Cast<UHansaNeedDefinition>(Candidate);
						break;
					}
				}
				if (!NeedIds.Contains(Requirement.NeedId) || NeedDefinition == nullptr)
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-012"), Tier->StableDefinitionId + TEXT(".Needs"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingTierNeed", "Population tier references missing need {0}."), FText::FromString(Requirement.NeedId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingTierNeedRemedy", "Add the referenced Need definition or correct the tier requirement."));
				}
				else if ((NeedDefinition->Kind == EHansaNeedKind::Good && Requirement.ConsumptionMilliUnitsPerResidentPerTick <= 0) ||
					(NeedDefinition->Kind == EHansaNeedKind::Service && Requirement.ConsumptionMilliUnitsPerResidentPerTick != 0))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-013"), Tier->StableDefinitionId + TEXT(".Needs"),
						NSLOCTEXT("HansaEconomicCompiler", "ImpossibleTierConsumption", "Tier consumption is impossible: good needs require positive consumption and service needs require zero inventory consumption."),
						NSLOCTEXT("HansaEconomicCompiler", "ImpossibleTierConsumptionRemedy", "Set a positive per-resident rate for good needs, or zero for service needs."));
				}
			}
			if (!Tier->PreviousTierId.IsEmpty() && !PopulationTierIds.Contains(Tier->PreviousTierId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-014"), Tier->StableDefinitionId + TEXT(".PreviousTierId"),
					FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingPreviousTier", "Population progression references missing tier {0}."), FText::FromString(Tier->PreviousTierId)),
					NSLOCTEXT("HansaEconomicCompiler", "MissingPreviousTierRemedy", "Add the prerequisite tier, correct the reference, or clear it for the one base tier."));
			}
		}
		else if (const UHansaCityMarketProfileDefinition* CityMarket = Cast<UHansaCityMarketProfileDefinition>(Definition))
		{
			for (const FHansaMarketGoodProfile& Profile : CityMarket->Goods)
			{
				if (!GoodIds.Contains(Profile.GoodId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-017"), CityMarket->StableDefinitionId + TEXT(".Goods"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingMarketGood", "City market profile references missing good {0}."), FText::FromString(Profile.GoodId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingMarketGoodRemedy", "Add the referenced Good definition or correct the city market row."));
				}
			}
		}
	}

	TMap<FString, const UHansaPopulationTierDefinition*> TiersById;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		if (const UHansaPopulationTierDefinition* Tier = Cast<UHansaPopulationTierDefinition>(Definition))
		{
			TiersById.Add(Tier->StableDefinitionId, Tier);
		}
	}
	int32 BaseTierCount = 0;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		if (const UHansaPopulationTierDefinition* Tier = Cast<UHansaPopulationTierDefinition>(Definition))
		{
			BaseTierCount += Tier->PreviousTierId.IsEmpty() ? 1 : 0;
			TSet<FString> Visited;
			const UHansaPopulationTierDefinition* Cursor = Tier;
			while (Cursor != nullptr && !Cursor->PreviousTierId.IsEmpty())
			{
				if (Visited.Contains(Cursor->StableDefinitionId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-015"), Tier->StableDefinitionId + TEXT(".PreviousTierId"),
						NSLOCTEXT("HansaEconomicCompiler", "TierCycle", "Population tier progression contains a cycle."),
						NSLOCTEXT("HansaEconomicCompiler", "TierCycleRemedy", "Point each tier toward a lower tier and retain exactly one base tier."));
					break;
				}
				Visited.Add(Cursor->StableDefinitionId);
				const UHansaPopulationTierDefinition* const* Previous = TiersById.Find(Cursor->PreviousTierId);
				Cursor = Previous != nullptr ? *Previous : nullptr;
			}
		}
	}
	if (!PopulationTierIds.IsEmpty() && BaseTierCount != 1)
	{
		AddIssue(Result.Issues, TEXT("HSA-REGISTRY-016"), TEXT("PopulationTiers"),
			NSLOCTEXT("HansaEconomicCompiler", "TierBaseCount", "Population progression requires exactly one base tier."),
			NSLOCTEXT("HansaEconomicCompiler", "TierBaseCountRemedy", "Clear PreviousTierId on exactly one lowest tier and link all others toward it."));
	}
	ValidateProductionGraph(SortedDefinitions, GoodIds, Result.Issues);

	if (!Result.IsValid())
	{
		return Result;
	}

	TArray<Hansa::Simulation::FHansaCompiledGoodDefinition> CompiledGoods;
	TArray<Hansa::Simulation::FHansaCompiledRecipeDefinition> CompiledRecipes;
	TArray<Hansa::Simulation::FHansaCompiledBuildingDefinition> CompiledBuildings;
	TArray<Hansa::Simulation::FHansaCompiledNeedDefinition> CompiledNeeds;
	TArray<Hansa::Simulation::FHansaCompiledPopulationTierDefinition> CompiledPopulationTiers;
	TArray<Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition> CompiledCityMarkets;
	FString RegistryCanonicalData;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		const uint64 ContentHash = Definition->ComputeDeterministicContentHash();
		RegistryCanonicalData += FString::Printf(
			TEXT("%s|%s|%llu\n"),
			*Definition->GetClass()->GetPathName(),
			*Definition->StableDefinitionId,
			static_cast<unsigned long long>(ContentHash));

		if (const UHansaGoodDefinition* Good = Cast<UHansaGoodDefinition>(Definition))
		{
			CompiledGoods.Add({
				Good->StableDefinitionId,
				StaticEnum<EHansaGoodUnit>()->GetNameStringByValue(static_cast<int64>(Good->QuantityUnit)),
				Good->BaseValueMilliMarks,
				Good->PriceElasticityBasisPoints,
				Good->SpoilageBasisPointsPerDay,
				ContentHash
			});
		}
		else if (const UHansaRecipeDefinition* Recipe = Cast<UHansaRecipeDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledRecipeDefinition Compiled;
			Compiled.StableId = Recipe->StableDefinitionId;
			Compiled.Inputs = CompileAmounts(Recipe->Inputs);
			Compiled.Outputs = CompileAmounts(Recipe->Outputs);
			Compiled.CycleTicks = Recipe->CycleTicks;
			Compiled.LaborerWorkforce = Recipe->LaborerWorkforce;
			Compiled.ArtisanWorkforce = Recipe->ArtisanWorkforce;
			Compiled.bDeclaredSource = Recipe->bDeclaredSource;
			Compiled.bDeclaredSink = Recipe->bDeclaredSink;
			Compiled.ContentHash = ContentHash;
			CompiledRecipes.Add(MoveTemp(Compiled));
		}
		else if (const UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledBuildingDefinition Compiled;
			Compiled.StableId = Building->StableDefinitionId;
			Compiled.ConstructionCosts = CompileAmounts(Building->ConstructionCosts);
			Compiled.ConstructionCostPfennig = Building->ConstructionCostPfennig;
			Compiled.CancellationRefundBasisPoints = Building->CancellationRefundBasisPoints;
			Compiled.RecipeIds = Building->RecipeIds;
			Compiled.RecipeIds.Sort();
			Compiled.UpgradeTargetBuildingId = Building->UpgradeTargetBuildingId;
			Compiled.FootprintWidthCells = Building->FootprintWidthCells;
			Compiled.FootprintHeightCells = Building->FootprintHeightCells;
			Compiled.BuildTicks = Building->BuildTicks;
			Compiled.StorageCapacityMilliUnits = Building->StorageCapacityMilliUnits;
			Compiled.ResidenceCapacity = Building->ResidenceCapacity;
			Compiled.ResidentPopulationTierId = Building->ResidentPopulationTierId;
			Compiled.LaborerWorkforce = Building->LaborerWorkforce;
			Compiled.ArtisanWorkforce = Building->ArtisanWorkforce;
			Compiled.bRequiresRoad = Building->bRequiresRoad;
			Compiled.bRequiresShoreline = Building->bRequiresShoreline;
			Compiled.ContentHash = ContentHash;
			CompiledBuildings.Add(MoveTemp(Compiled));
		}
		else if (const UHansaNeedDefinition* Need = Cast<UHansaNeedDefinition>(Definition))
		{
			CompiledNeeds.Add({ Need->StableDefinitionId,
				Need->Kind == EHansaNeedKind::Good
					? Hansa::Simulation::EHansaCompiledNeedKind::Good
					: Hansa::Simulation::EHansaCompiledNeedKind::Service,
				Need->GoodId, ContentHash });
		}
		else if (const UHansaPopulationTierDefinition* Tier = Cast<UHansaPopulationTierDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledPopulationTierDefinition Compiled;
			Compiled.StableId = Tier->StableDefinitionId;
			Compiled.PreviousTierId = Tier->PreviousTierId;
			for (const FHansaPopulationTierNeed& Requirement : Tier->Needs)
			{
				Compiled.Needs.Add({ Requirement.NeedId, Requirement.ConsumptionMilliUnitsPerResidentPerTick,
					Requirement.ImportanceBasisPoints });
			}
			Compiled.Needs.Sort([](const auto& Left, const auto& Right) { return Left.NeedId < Right.NeedId; });
			Compiled.WorkforcePerResidentBasisPoints = Tier->WorkforcePerResidentBasisPoints;
			Compiled.GrowthSatisfactionBasisPoints = Tier->GrowthSatisfactionBasisPoints;
			Compiled.DeclineSatisfactionBasisPoints = Tier->DeclineSatisfactionBasisPoints;
			Compiled.EvaluationTicks = Tier->EvaluationTicks;
			Compiled.GrowthResidentsPerEvaluation = Tier->GrowthResidentsPerEvaluation;
			Compiled.DeclineResidentsPerEvaluation = Tier->DeclineResidentsPerEvaluation;
			Compiled.ContentHash = ContentHash;
			CompiledPopulationTiers.Add(MoveTemp(Compiled));
		}
		else if (const UHansaCityMarketProfileDefinition* CityMarket = Cast<UHansaCityMarketProfileDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition Compiled;
			Compiled.StableId = CityMarket->StableDefinitionId;
			Compiled.UpdateCadenceTicks = CityMarket->UpdateCadenceTicks;
			Compiled.PriceHistoryCapacity = CityMarket->PriceHistoryCapacity;
			Compiled.TargetSmoothingBasisPoints = CityMarket->TargetSmoothingBasisPoints;
			Compiled.MaximumMovementBasisPointsPerUpdate = CityMarket->MaximumMovementBasisPointsPerUpdate;
			Compiled.StaleAfterTicks = CityMarket->StaleAfterTicks;
			for (const FHansaMarketGoodProfile& Profile : CityMarket->Goods)
			{
				Compiled.Goods.Add({ Profile.GoodId, Profile.DesiredReserveMilliUnits,
					Profile.ConfirmedIncomingSupplyMilliUnits, Profile.SeasonModifierBasisPoints,
					Profile.CityModifierBasisPoints, Profile.MinimumPriceMilliMarks,
					Profile.MaximumPriceMilliMarks, Profile.InitialPriceMilliMarks });
			}
			Compiled.Goods.Sort([](const auto& Left, const auto& Right) { return Left.GoodId < Right.GoodId; });
			Compiled.ContentHash = ContentHash;
			CompiledCityMarkets.Add(MoveTemp(Compiled));
		}
	}

	Result.Registry = Hansa::Simulation::FHansaEconomicRegistry(
		MoveTemp(CompiledGoods),
		MoveTemp(CompiledRecipes),
		MoveTemp(CompiledBuildings),
		HashUtf8Fnv1a(RegistryCanonicalData),
		MoveTemp(CompiledNeeds),
		MoveTemp(CompiledPopulationTiers),
		MoveTemp(CompiledCityMarkets));
	return Result;
}
