#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"

#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaPresenceDefinitions.h"
#include "Definitions/HansaResearchDefinitions.h"
#include "Definitions/HansaScenarioDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"

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

FString FHansaEconomicRegistryCompileResult::DescribeRegistryHashMismatch(
	const uint64 ExpectedRegistryHash,
	const TArray<FHansaEconomicDefinitionHashEvidence>& ExpectedDefinitions) const
{
	const auto Key = [](const FHansaEconomicDefinitionHashEvidence& Evidence)
	{
		return Evidence.DefinitionClassPath + TEXT("|") + Evidence.StableId;
	};
	TMap<FString, const FHansaEconomicDefinitionHashEvidence*> ExpectedByKey;
	TMap<FString, const FHansaEconomicDefinitionHashEvidence*> ActualByKey;
	for (const FHansaEconomicDefinitionHashEvidence& Evidence : ExpectedDefinitions)
	{
		ExpectedByKey.Add(Key(Evidence), &Evidence);
	}
	for (const FHansaEconomicDefinitionHashEvidence& Evidence : DefinitionHashes)
	{
		ActualByKey.Add(Key(Evidence), &Evidence);
	}

	TArray<FString> Differences;
	for (const FHansaEconomicDefinitionHashEvidence& Expected : ExpectedDefinitions)
	{
		const FHansaEconomicDefinitionHashEvidence* const* Actual = ActualByKey.Find(Key(Expected));
		if (Actual == nullptr)
		{
			Differences.Add(FString::Printf(TEXT("missing %s (%s), expected %016llX"),
				*Expected.StableId, *Expected.DefinitionClassPath,
				static_cast<unsigned long long>(Expected.ContentHash)));
		}
		else if ((*Actual)->ContentHash != Expected.ContentHash)
		{
			Differences.Add(FString::Printf(TEXT("changed %s (%s): expected %016llX, actual %016llX"),
				*Expected.StableId, *Expected.DefinitionClassPath,
				static_cast<unsigned long long>(Expected.ContentHash),
				static_cast<unsigned long long>((*Actual)->ContentHash)));
		}
	}
	for (const FHansaEconomicDefinitionHashEvidence& Actual : DefinitionHashes)
	{
		if (!ExpectedByKey.Contains(Key(Actual)))
		{
			Differences.Add(FString::Printf(TEXT("unexpected %s (%s), actual %016llX"),
				*Actual.StableId, *Actual.DefinitionClassPath,
				static_cast<unsigned long long>(Actual.ContentHash)));
		}
	}
	Differences.Sort();
	if (Differences.IsEmpty())
	{
		Differences.Add(TEXT("no per-definition fingerprint differs; registry row serialization or aggregation changed"));
	}
	return FString::Printf(TEXT("Registry hash mismatch: expected %016llX, actual %016llX. %s."),
		static_cast<unsigned long long>(ExpectedRegistryHash),
		static_cast<unsigned long long>(Registry.GetRegistryHash()),
		*FString::Join(Differences, TEXT("; ")));
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
	TSet<FString> CityIds;
	TSet<FString> ProductionChainIds;
	TSet<FString> RegionIds;
	TSet<FString> VehicleIds;
	TSet<FString> RouteIds;
	TSet<FString> PresenceCapabilityIds;
	TSet<FString> PresenceStageIds;
	TSet<FString> CityTradePolicyIds;
	TSet<FString> TechnologyIds;
	TSet<FString> ScenarioObjectiveIds;
	TSet<FString> VictoryIds;
	TSet<FString> ScenarioIds;
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
		else if (Definition->IsA<UHansaProductionChainDefinition>())
		{
			ProductionChainIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaRegionEconomicProfileDefinition>())
		{
			RegionIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaCityMarketProfileDefinition>())
		{
			CityIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaVehicleDefinition>())
		{
			VehicleIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaRouteDefinition>())
		{
			RouteIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaPresenceCapabilityDefinition>())
		{
			PresenceCapabilityIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaForeignPresenceStageDefinition>())
		{
			PresenceStageIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaCityTradePolicyDefinition>())
		{
			CityTradePolicyIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaTechnologyDefinition>())
		{
			TechnologyIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaResidentialCompoundDefinition>())
        {
        }
        else if (Definition->IsA<UHansaMerchantAITuningDefinition>())
		{
		}
		else if (Definition->IsA<UHansaScenarioObjectiveDefinition>())
		{
			ScenarioObjectiveIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaVictoryDefinition>())
		{
			VictoryIds.Add(Definition->StableDefinitionId);
		}
		else if (Definition->IsA<UHansaScenarioDefinition>())
		{
			ScenarioIds.Add(Definition->StableDefinitionId);
		}
		else
		{
			AddIssue(Result.Issues, TEXT("HSA-REGISTRY-002"), Definition->StableDefinitionId,
				NSLOCTEXT("HansaEconomicCompiler", "UnsupportedDefinition", "The economic compiler received a non-economic definition type."),
				NSLOCTEXT("HansaEconomicCompiler", "UnsupportedDefinitionRemedy", "Compile only supported economic, population, market, vehicle, and route definitions in this registry."));
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
			if (!Building->ConstructionChainOutputGoodId.IsEmpty() && !GoodIds.Contains(Building->ConstructionChainOutputGoodId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-036"), Building->StableDefinitionId + TEXT(".ConstructionChainOutputGoodId"),
					FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingConstructionChainGood", "Construction chain references missing final output {0}."), FText::FromString(Building->ConstructionChainOutputGoodId)),
					NSLOCTEXT("HansaEconomicCompiler", "MissingConstructionChainGoodRemedy", "Add the referenced Good definition or correct the construction-chain output."));
			}
			if (!Building->RequiredConstructionTechnologyId.IsEmpty() &&
				!TechnologyIds.Contains(Building->RequiredConstructionTechnologyId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-037"), Building->StableDefinitionId + TEXT(".RequiredConstructionTechnologyId"),
					FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingConstructionTechnology", "Construction card references missing technology {0}."), FText::FromString(Building->RequiredConstructionTechnologyId)),
					NSLOCTEXT("HansaEconomicCompiler", "MissingConstructionTechnologyRemedy", "Add the referenced Technology definition or clear the construction unlock."));
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
				const bool bCompoundDevelopment = Target != nullptr && !Building->ResidentialCompound.IsNull() &&
                    Building->ResidentialCompound == Target->ResidentialCompound &&
                    Building->CompoundDistrictId == Target->CompoundDistrictId &&
                    Building->ResidentPopulationTierId == Target->ResidentPopulationTierId &&
                    Target->CompoundStage == Building->CompoundStage + 1;
                const bool bProductionUpgrade = Target && Building->ResidenceCapacity == 0 && Target->ResidenceCapacity == 0 &&
                    !Building->RecipeIds.IsEmpty() && Target->bRequiresShoreline == Building->bRequiresShoreline &&
                    Target->bRequiresRoad == Building->bRequiresRoad &&
                    Building->FootprintWidthCells == Target->FootprintWidthCells && Building->FootprintHeightCells == Target->FootprintHeightCells &&
                    !Building->RecipeIds.ContainsByPredicate([&](const FString& Id) { return !Target->RecipeIds.Contains(Id); });
                if (!bProductionUpgrade && (Target == nullptr || Building->ResidenceCapacity <= 0 || Target->ResidenceCapacity <= 0 ||
					Building->FootprintWidthCells != Target->FootprintWidthCells ||
					Building->FootprintHeightCells != Target->FootprintHeightCells ||
					TargetTier == nullptr || (!bCompoundDevelopment && TargetTier->PreviousTierId != Building->ResidentPopulationTierId)))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-020"), Building->StableDefinitionId + TEXT(".UpgradeTargetBuildingId"),
						NSLOCTEXT("HansaEconomicCompiler", "InvalidResidenceProgression", "A residence upgrade must preserve its footprint and advance to the directly linked population tier or the next stage of the same compound and district."),
						NSLOCTEXT("HansaEconomicCompiler", "InvalidResidenceProgressionRemedy", "Choose a same-footprint next-tier residence, or the next stage of the same compound, district and population tier."));
				}
			}
		}
		else if (const UHansaNeedDefinition* Need = Cast<UHansaNeedDefinition>(Definition))
		{
   for (const auto& Alternative : Need->Alternatives)
    if (!GoodIds.Contains(Alternative.GoodId))
     AddIssue(Result.Issues, TEXT("HSA-REGISTRY-NEED-ALT"), Need->StableDefinitionId + TEXT(".Alternatives"),
      NSLOCTEXT("HansaEconomicCompiler", "MissingAlternative", "A need alternative references a missing good."),
      NSLOCTEXT("HansaEconomicCompiler", "MissingAlternativeRemedy", "Select an existing Good.* definition."));
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
			if(!CityMarket->RegionId.IsEmpty() && !RegionIds.Contains(CityMarket->RegionId))
				AddIssue(Result.Issues,TEXT("HSA-REGISTRY-REGION-001"),CityMarket->StableDefinitionId+TEXT(".RegionId"),NSLOCTEXT("HansaEconomicCompiler","MissingRegion","A city references a missing regional economic profile."),NSLOCTEXT("HansaEconomicCompiler","MissingRegionFix","Add the Region.* definition or correct the city reference."));
			for(const auto& Binding:CityMarket->IndustryBindings) if(!ProductionChainIds.Contains(Binding.ProductionChainId))
				AddIssue(Result.Issues,TEXT("HSA-REGISTRY-REGION-002"),CityMarket->StableDefinitionId+TEXT(".IndustryBindings"),NSLOCTEXT("HansaEconomicCompiler","MissingChain","A city industry binding references a missing production chain."),NSLOCTEXT("HansaEconomicCompiler","MissingChainFix","Add the ProductionChain.* definition or correct the binding."));
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
		else if(const UHansaProductionChainDefinition* Chain=Cast<UHansaProductionChainDefinition>(Definition))
		{
			for(const auto& Stage:Chain->Stages) if(!RecipeIds.Contains(Stage.RecipeId)) AddIssue(Result.Issues,TEXT("HSA-REGISTRY-REGION-003"),Chain->StableDefinitionId+TEXT(".Stages"),NSLOCTEXT("HansaEconomicCompiler","MissingChainRecipe","A production-chain stage references a missing recipe."),NSLOCTEXT("HansaEconomicCompiler","MissingChainRecipeFix","Add the Recipe.* definition or correct the stage."));
		}
		else if(const UHansaRegionEconomicProfileDefinition* Region=Cast<UHansaRegionEconomicProfileDefinition>(Definition))
		{
			for(const auto& Permit:Region->PermittedStages) if(!ProductionChainIds.Contains(Permit.ProductionChainId)) AddIssue(Result.Issues,TEXT("HSA-REGISTRY-REGION-004"),Region->StableDefinitionId+TEXT(".PermittedStages"),NSLOCTEXT("HansaEconomicCompiler","MissingRegionChain","A regional portfolio references a missing production chain."),NSLOCTEXT("HansaEconomicCompiler","MissingRegionChainFix","Add the ProductionChain.* definition or correct the portfolio."));
			for(const auto& Resource:Region->ResourceEndowments) if(!GoodIds.Contains(Resource.GoodId)) AddIssue(Result.Issues,TEXT("HSA-REGISTRY-REGION-005"),Region->StableDefinitionId+TEXT(".ResourceEndowments"),NSLOCTEXT("HansaEconomicCompiler","MissingRegionGood","A regional endowment references a missing good."),NSLOCTEXT("HansaEconomicCompiler","MissingRegionGoodFix","Add the Good.* definition or correct the endowment."));
		}
		else if (const UHansaRouteDefinition* Route = Cast<UHansaRouteDefinition>(Definition))
		{
			for (const FHansaRouteConnectionDefinition& Connection : Route->Connections)
			{
				if (!CityIds.Contains(Connection.SourceCityId) || !CityIds.Contains(Connection.DestinationCityId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-021"), Route->StableDefinitionId + TEXT(".Connections"),
						NSLOCTEXT("HansaEconomicCompiler", "MissingRouteCity", "Route connection references a city without a city-market definition in this registry."),
						NSLOCTEXT("HansaEconomicCompiler", "MissingRouteCityRemedy", "Add both referenced City.* market definitions or correct the route connection."));
				}
			}
		}
		else if (const UHansaTechnologyDefinition* Technology = Cast<UHansaTechnologyDefinition>(Definition))
		{
			for (const FString& PrerequisiteId : Technology->PrerequisiteTechnologyIds)
			{
				if (!TechnologyIds.Contains(PrerequisiteId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-022"), Technology->StableDefinitionId + TEXT(".PrerequisiteTechnologyIds"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingTechnologyPrerequisite", "Technology prerequisite {0} is missing."), FText::FromString(PrerequisiteId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingTechnologyPrerequisiteRemedy", "Add the referenced technology or correct the stable prerequisite ID."));
				}
			}
			for (const FHansaResearchEffectDefinition& Effect : Technology->Effects)
			{
				if (!Effect.TargetStableId.IsEmpty() && !AllIds.Contains(Effect.TargetStableId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-023"), Technology->StableDefinitionId + TEXT(".Effects"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingTechnologyEffectTarget", "Technology effect target {0} is missing."), FText::FromString(Effect.TargetStableId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingTechnologyEffectTargetRemedy", "Choose a stable ID from the same accepted content set."));
				}
			}
		}
		else if (const UHansaMerchantAITuningDefinition* Tuning = Cast<UHansaMerchantAITuningDefinition>(Definition))
		{
			for (const FString& TechnologyId : Tuning->PreferredResearchTechnologyIds)
			{
				if (!TechnologyIds.Contains(TechnologyId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-027"), Tuning->StableDefinitionId + TEXT(".PreferredResearchTechnologyIds"),
						FText::Format(NSLOCTEXT("HansaEconomicCompiler", "MissingAIResearch", "Merchant AI tuning references missing technology {0}."), FText::FromString(TechnologyId)),
						NSLOCTEXT("HansaEconomicCompiler", "MissingAIResearchRemedy", "Add the referenced technology or remove it from the priority list."));
				}
			}
			for (const FHansaMerchantAITradePlanDefinition& Plan : Tuning->TradePlans)
			{
				if (!RouteIds.Contains(Plan.RouteDefinitionId) || !VehicleIds.Contains(Plan.VehicleDefinitionId) ||
					!CityIds.Contains(Plan.SourceCityId) || !CityIds.Contains(Plan.DestinationCityId) || !GoodIds.Contains(Plan.GoodId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-028"), Tuning->StableDefinitionId + TEXT(".TradePlans"),
						NSLOCTEXT("HansaEconomicCompiler", "MissingAIPlanReference", "Merchant AI trade plan contains a missing route, vehicle, city, or good stable reference."),
						NSLOCTEXT("HansaEconomicCompiler", "MissingAIPlanReferenceRemedy", "Choose stable IDs from the same accepted registry content set."));
				}
				const UHansaRouteDefinition* RouteDefinition = nullptr;
				const UHansaVehicleDefinition* VehicleDefinition = nullptr;
				for (const UHansaDefinitionBase* Candidate : SortedDefinitions)
				{
					if (Candidate->StableDefinitionId == Plan.RouteDefinitionId) RouteDefinition = Cast<UHansaRouteDefinition>(Candidate);
					if (Candidate->StableDefinitionId == Plan.VehicleDefinitionId) VehicleDefinition = Cast<UHansaVehicleDefinition>(Candidate);
				}
				if (RouteDefinition != nullptr && VehicleDefinition != nullptr && RouteDefinition->Mode != VehicleDefinition->Mode)
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-029"), Tuning->StableDefinitionId + TEXT(".TradePlans"),
						NSLOCTEXT("HansaEconomicCompiler", "AIPlanModeMismatch", "Merchant AI route and vehicle definitions use different travel modes."),
						NSLOCTEXT("HansaEconomicCompiler", "AIPlanModeMismatchRemedy", "Pair a sea route with a sea vehicle or a land route with a land vehicle."));
				}
			}
		}
		else if (const UHansaScenarioObjectiveDefinition* Objective = Cast<UHansaScenarioObjectiveDefinition>(Definition))
		{
			if (!Objective->CityId.IsEmpty() && !CityIds.Contains(Objective->CityId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-030"), Objective->StableDefinitionId + TEXT(".CityId"),
					NSLOCTEXT("HansaEconomicCompiler", "MissingObjectiveCity", "Scenario objective references a missing city."),
					NSLOCTEXT("HansaEconomicCompiler", "MissingObjectiveCityRemedy", "Choose a City.* definition from the accepted content set."));
			}
			if (!Objective->GoodId.IsEmpty() && !GoodIds.Contains(Objective->GoodId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-031"), Objective->StableDefinitionId + TEXT(".GoodId"),
					NSLOCTEXT("HansaEconomicCompiler", "MissingObjectiveGood", "Scenario objective references a missing good."),
					NSLOCTEXT("HansaEconomicCompiler", "MissingObjectiveGoodRemedy", "Choose a Good.* definition from the accepted content set."));
			}
		}
		else if (const UHansaVictoryDefinition* Victory = Cast<UHansaVictoryDefinition>(Definition))
		{
			for (const FString& ObjectiveId : Victory->ObjectiveIds)
			{
				if (!ScenarioObjectiveIds.Contains(ObjectiveId))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-032"), Victory->StableDefinitionId + TEXT(".ObjectiveIds"),
						NSLOCTEXT("HansaEconomicCompiler", "MissingVictoryObjective", "Victory path references a missing scenario objective."),
						NSLOCTEXT("HansaEconomicCompiler", "MissingVictoryObjectiveRemedy", "Choose a ScenarioObjective.* definition from the accepted content set."));
				}
			}
		}
		else if (const UHansaScenarioDefinition* Scenario = Cast<UHansaScenarioDefinition>(Definition))
		{
			if (!CityIds.Contains(Scenario->HomeCityId))
			{
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-033"), Scenario->StableDefinitionId + TEXT(".HomeCityId"),
					NSLOCTEXT("HansaEconomicCompiler", "MissingScenarioCity", "Scenario references a missing home city."),
					NSLOCTEXT("HansaEconomicCompiler", "MissingScenarioCityRemedy", "Choose a City.* definition from the accepted content set."));
			}
			TSet<int32> Priorities;
			for (const FString& VictoryId : Scenario->VictoryIds)
			{
				const UHansaVictoryDefinition* ResolvedVictory = nullptr;
				for (const UHansaDefinitionBase* Candidate : SortedDefinitions)
					if (Candidate->StableDefinitionId == VictoryId) { ResolvedVictory = Cast<UHansaVictoryDefinition>(Candidate); break; }
				if (ResolvedVictory == nullptr)
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-034"), Scenario->StableDefinitionId + TEXT(".VictoryIds"),
						NSLOCTEXT("HansaEconomicCompiler", "MissingScenarioVictory", "Scenario references a missing victory path."),
						NSLOCTEXT("HansaEconomicCompiler", "MissingScenarioVictoryRemedy", "Choose a Victory.* definition from the accepted content set."));
				}
				else if (Priorities.Contains(ResolvedVictory->EndingPriority))
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-035"), Scenario->StableDefinitionId + TEXT(".VictoryIds"),
						NSLOCTEXT("HansaEconomicCompiler", "AmbiguousScenarioEnding", "Scenario victory paths share an ending priority and are ambiguous."),
						NSLOCTEXT("HansaEconomicCompiler", "AmbiguousScenarioEndingRemedy", "Assign a unique ending priority to every enabled victory path."));
				}
				else Priorities.Add(ResolvedVictory->EndingPriority);
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
	TMap<FString, TArray<const UHansaBuildingDefinition*>> ConstructionChains;
	TSet<FString> UpgradeTargets;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		if (const UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(Definition))
		{
			if (!Building->ConstructionChainOutputGoodId.IsEmpty())
			{
				ConstructionChains.FindOrAdd(Building->ConstructionChainOutputGoodId).Add(Building);
			}
			if (!Building->UpgradeTargetBuildingId.IsEmpty()) UpgradeTargets.Add(Building->UpgradeTargetBuildingId);
		}
	}
	for (const TPair<FString, TArray<const UHansaBuildingDefinition*>>& Pair : ConstructionChains)
	{
		const int32 ExpectedCount = Pair.Value[0]->ConstructionChainStageCount;
		TSet<int32> Stages;
		bool bConsistentCount = ExpectedCount > 0;
		for (const UHansaBuildingDefinition* Building : Pair.Value)
		{
			bConsistentCount &= Building->ConstructionChainStageCount == ExpectedCount;
			Stages.Add(Building->ConstructionChainStage);
		}
		bool bComplete = bConsistentCount && Pair.Value.Num() == ExpectedCount && Stages.Num() == ExpectedCount;
		for (int32 Stage = 1; Stage <= ExpectedCount && bComplete; ++Stage) bComplete &= Stages.Contains(Stage);
		if (!bComplete)
		{
			AddIssue(Result.Issues, TEXT("HSA-REGISTRY-038"), Pair.Key + TEXT(".ConstructionChain"),
				FText::Format(NSLOCTEXT("HansaEconomicCompiler", "IncompleteConstructionChain", "Construction chain {0} is missing a member, duplicates a stage, or disagrees about its expected size."), FText::FromString(Pair.Key)),
				NSLOCTEXT("HansaEconomicCompiler", "IncompleteConstructionChainRemedy", "Provide exactly one visible building for every authored stage from one through the shared stage count."));
		}
	}
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		const UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(Definition);
		if (Building != nullptr && Building->bUpgradeOnly && !UpgradeTargets.Contains(Building->StableDefinitionId))
		{
			AddIssue(Result.Issues, TEXT("HSA-REGISTRY-039"), Building->StableDefinitionId + TEXT(".bUpgradeOnly"),
				NSLOCTEXT("HansaEconomicCompiler", "UnreachableUpgradeOnlyBuilding", "An upgrade-only construction card is not the target of another building definition."),
				NSLOCTEXT("HansaEconomicCompiler", "UnreachableUpgradeOnlyBuildingRemedy", "Add the authored upgrade reference or allow direct construction."));
		}
	}
	if (!TechnologyIds.IsEmpty())
	{
		TArray<Hansa::Simulation::FHansaCompiledTechnologyDefinition> GraphTechnologies;
		for (const UHansaDefinitionBase* Definition : SortedDefinitions)
		{
			if (const UHansaTechnologyDefinition* Technology = Cast<UHansaTechnologyDefinition>(Definition))
			{
				Hansa::Simulation::FHansaCompiledTechnologyDefinition Node;
				Node.StableId = Technology->StableDefinitionId;
				Node.PrerequisiteTechnologyIds = Technology->PrerequisiteTechnologyIds;
				GraphTechnologies.Add(MoveTemp(Node));
			}
		}
		const TArray<FString> Roots = {TEXT("Technology.Commerce.MarketReports"), TEXT("Technology.Production.ImprovedMilling"), TEXT("Technology.Logistics.WarehouseHandling")};
		for (const Hansa::Simulation::FHansaResearchGraphDiagnostic& Diagnostic :
			Hansa::Simulation::FHansaResearchGraphValidator::Validate(GraphTechnologies, Roots, AllIds))
		{
			if (Diagnostic.Issue == Hansa::Simulation::EHansaResearchGraphIssue::InvalidEffectReference ||
				Diagnostic.Issue == Hansa::Simulation::EHansaResearchGraphIssue::MissingNode) continue;
			AddIssue(Result.Issues,
				Diagnostic.Issue == Hansa::Simulation::EHansaResearchGraphIssue::Cycle ? TEXT("HSA-REGISTRY-024") :
				Diagnostic.Issue == Hansa::Simulation::EHansaResearchGraphIssue::Unreachable ? TEXT("HSA-REGISTRY-025") : TEXT("HSA-REGISTRY-026"),
				Diagnostic.TechnologyId + TEXT(".PrerequisiteTechnologyIds"), FText::FromString(Diagnostic.Cause), FText::FromString(Diagnostic.Remedy));
		}
	}

	if (!Result.IsValid())
	{
		return Result;
	}

    for (const auto* Definition : SortedDefinitions)
    {
        const auto* Recipe = Cast<UHansaRecipeDefinition>(Definition);
        if (!Recipe || Recipe->InternalCatchRecipeId.IsEmpty()) continue;
        const UHansaRecipeDefinition* Source = nullptr;
        for (const auto* Candidate : SortedDefinitions)
            if (Candidate->StableDefinitionId == Recipe->InternalCatchRecipeId) Source = Cast<UHansaRecipeDefinition>(Candidate);
        bool bValid = Recipe->Inputs.Num()==2 && Recipe->Inputs.ContainsByPredicate([](const auto& I){return I.GoodId==TEXT("Good.Salt");}) &&
            Recipe->Inputs.ContainsByPredicate([](const auto& I){return I.GoodId==TEXT("Good.Barrels");}) &&
            Source && Source->bDeclaredSource && Source->Inputs.IsEmpty() && Source->InternalCatchRecipeId.IsEmpty() &&
            Recipe->CycleTicks >= Source->CycleTicks && !Recipe->bDeclaredSource && !Recipe->Inputs.IsEmpty() &&
            Source->Outputs.Num() == 1 && Recipe->Outputs.Num() == 1 &&
            Source->Outputs[0].GoodId == TEXT("Good.Fish") && Recipe->Outputs[0].GoodId == TEXT("Good.PreservedFish") &&
            Recipe->Outputs[0].QuantityMilliUnits <= Source->Outputs[0].QuantityMilliUnits &&
            !Recipe->Inputs.ContainsByPredicate([](const auto& Input){return Input.GoodId==TEXT("Good.Fish");});
        for (const auto* Candidate : SortedDefinitions)
            if (const auto* Building = Cast<UHansaBuildingDefinition>(Candidate))
                if (Building->RecipeIds.Contains(Recipe->StableDefinitionId))
                    bValid &= Building->bRequiresShoreline && Building->RecipeIds.Contains(Recipe->InternalCatchRecipeId);
        if (!bValid) AddIssue(Result.Issues, TEXT("HSA-RECIPE-INTERNAL-CATCH"), Recipe->StableDefinitionId,
            NSLOCTEXT("HansaEconomicCompiler","InternalCatchInvalid","Internal catch must preserve the fresh source's edible output and timing in a shoreline fishery."),
            NSLOCTEXT("HansaEconomicCompiler","InternalCatchFix","Use a valid fresh-catch source, salt and barrel inputs, and no purchased fresh fish."));
    }
	// Cross-definition presence validation: typed references, one policy per city, unique order, and acyclic prerequisites.
	TMap<FString, const UHansaForeignPresenceStageDefinition*> PresenceStagesById;
	TSet<int32> PresenceOrdinals;
	TSet<FString> PresencePolicyCities;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		if (const auto* Stage = Cast<UHansaForeignPresenceStageDefinition>(Definition))
		{
			if (PresenceOrdinals.Contains(Stage->Ordinal))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-001"), Stage->StableDefinitionId + TEXT(".Ordinal"), NSLOCTEXT("HansaEconomicCompiler", "DuplicatePresenceOrdinal", "Presence-stage ordinals must be unique."), NSLOCTEXT("HansaEconomicCompiler", "DuplicatePresenceOrdinalFix", "Assign one deterministic ordinal to each stage."));
			PresenceOrdinals.Add(Stage->Ordinal); PresenceStagesById.Add(Stage->StableDefinitionId, Stage);
			for (const FString& CapabilityId : Stage->GrantedCapabilityIds) if (!PresenceCapabilityIds.Contains(CapabilityId))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-002"), Stage->StableDefinitionId + TEXT(".GrantedCapabilityIds"), NSLOCTEXT("HansaEconomicCompiler", "MissingPresenceCapability", "A stage references an unknown capability."), NSLOCTEXT("HansaEconomicCompiler", "MissingPresenceCapabilityFix", "Add the PresenceCapability.* definition or correct the reference."));
			for (const auto& Cost : Stage->UpgradeGoods) if (!GoodIds.Contains(Cost.GoodId))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-003"), Stage->StableDefinitionId + TEXT(".UpgradeGoods"), NSLOCTEXT("HansaEconomicCompiler", "MissingPresenceCostGood", "A stage cost references an unknown good."), NSLOCTEXT("HansaEconomicCompiler", "MissingPresenceCostGoodFix", "Add the Good.* definition or correct the material cost."));
		}
		else if (const auto* Policy = Cast<UHansaCityTradePolicyDefinition>(Definition))
		{
			if (!CityIds.Contains(Policy->CityId) || PresencePolicyCities.Contains(Policy->CityId))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-004"), Policy->StableDefinitionId + TEXT(".CityId"), NSLOCTEXT("HansaEconomicCompiler", "InvalidPresencePolicyCity", "A city policy targets a missing city or duplicates another policy."), NSLOCTEXT("HansaEconomicCompiler", "InvalidPresencePolicyCityFix", "Target one compiled City.* exactly once."));
			PresencePolicyCities.Add(Policy->CityId);
			for (const FString& StageId : Policy->AllowedStageIds) if (!PresenceStageIds.Contains(StageId))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-005"), Policy->StableDefinitionId + TEXT(".AllowedStageIds"), NSLOCTEXT("HansaEconomicCompiler", "MissingPolicyStage", "A city policy references an unknown stage."), NSLOCTEXT("HansaEconomicCompiler", "MissingPolicyStageFix", "Add the PresenceStage.* definition or correct the policy."));
			for (const FString& CapabilityId : Policy->DeniedCapabilityIds) if (!PresenceCapabilityIds.Contains(CapabilityId))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-006"), Policy->StableDefinitionId + TEXT(".DeniedCapabilityIds"), NSLOCTEXT("HansaEconomicCompiler", "MissingPolicyCapability", "A city policy denies an unknown capability."), NSLOCTEXT("HansaEconomicCompiler", "MissingPolicyCapabilityFix", "Add the PresenceCapability.* definition or correct the policy."));
			if (!Policy->bExceptionalGovernanceAllowed && Policy->AllowedStageIds.Contains(TEXT("PresenceStage.ExceptionalGovernance")))
				AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-007"), Policy->StableDefinitionId, NSLOCTEXT("HansaEconomicCompiler", "GovernanceContradiction", "Policy disallows exceptional governance but includes its stage."), NSLOCTEXT("HansaEconomicCompiler", "GovernanceContradictionFix", "Remove the stage or explicitly allow scenario-gated governance."));
			if (!Policy->TradeStationSites.IsEmpty())
			{
				const auto* City = SortedDefinitions.FindByPredicate([&](const UHansaDefinitionBase* Candidate)
				{
					const auto* Market = Cast<UHansaCityMarketProfileDefinition>(Candidate);
					return Market != nullptr && Market->StableDefinitionId == Policy->CityId;
				});
				const auto* Market = City != nullptr ? Cast<UHansaCityMarketProfileDefinition>(*City) : nullptr;
				const EHansaCityPresentationClass Presentation = Market == nullptr ? EHansaCityPresentationClass::Unspecified
					: Market->PresentationClass != EHansaCityPresentationClass::Unspecified ? Market->PresentationClass
					: Market->StableDefinitionId == TEXT("City.Lubeck") ? EHansaCityPresentationClass::RenderedBuildable
					: Market->StableDefinitionId == TEXT("City.Rostock") ? EHansaCityPresentationClass::RenderedVisitable
					: EHansaCityPresentationClass::MarketOnly;
				const bool bHasRoute = SortedDefinitions.ContainsByPredicate([&](const UHansaDefinitionBase* Candidate)
				{
					const auto* Route = Cast<UHansaRouteDefinition>(Candidate);
					return Route != nullptr && Route->Connections.ContainsByPredicate([&](const FHansaRouteConnectionDefinition& Connection)
					{
						return Connection.SourceCityId == Policy->CityId || Connection.DestinationCityId == Policy->CityId;
					});
				});
				const bool bAllowsStation = Policy->AllowedStageIds.ContainsByPredicate([&](const FString& StageId)
				{
					return SortedDefinitions.ContainsByPredicate([&](const UHansaDefinitionBase* Candidate)
					{
						const auto* Stage = Cast<UHansaForeignPresenceStageDefinition>(Candidate);
						return Stage != nullptr && Stage->StableDefinitionId == StageId &&
							Stage->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"));
					});
				});
				if (Market == nullptr || Market->Goods.IsEmpty() || !Policy->bPublicMarketAccess || !bHasRoute || !bAllowsStation ||
					Presentation == EHansaCityPresentationClass::MarketOnly)
				{
					AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-012"), Policy->StableDefinitionId + TEXT(".TradeStationSites"),
						NSLOCTEXT("HansaEconomicCompiler", "StationCityContract", "A station-capable city requires a market, public access, route access, a station stage, and a rendered presentation classification."),
						NSLOCTEXT("HansaEconomicCompiler", "StationCityContractFix", "Add the missing market/route/stage contract or remove the station site from an abstract market-only city."));
				}
			}
		}
	}
	for (const auto& Pair : PresenceStagesById)
		for (const FString& PrerequisiteId : Pair.Value->PrerequisiteStageIds) if (!PresenceStagesById.Contains(PrerequisiteId))
			AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-008"), Pair.Key + TEXT(".PrerequisiteStageIds"), NSLOCTEXT("HansaEconomicCompiler", "MissingPresencePrerequisite", "A presence prerequisite is missing."), NSLOCTEXT("HansaEconomicCompiler", "MissingPresencePrerequisiteFix", "Add the stage or correct the prerequisite."));
	for (const auto& Start : PresenceStagesById)
	{
		TSet<FString> Visiting; TSet<FString> Visited;
		TFunction<bool(const FString&)> Visit = [&](const FString& Id)
		{
			if (Visiting.Contains(Id)) return false; if (Visited.Contains(Id)) return true;
			Visiting.Add(Id); const auto* const* Stage = PresenceStagesById.Find(Id);
			if (Stage) for (const FString& Parent : (*Stage)->PrerequisiteStageIds) if (!Visit(Parent)) return false;
			Visiting.Remove(Id); Visited.Add(Id); return true;
		};
		if (!Visit(Start.Key)) AddIssue(Result.Issues, TEXT("HSA-REGISTRY-PRESENCE-009"), Start.Key + TEXT(".PrerequisiteStageIds"), NSLOCTEXT("HansaEconomicCompiler", "PresenceCycle", "Presence-stage prerequisites contain a cycle."), NSLOCTEXT("HansaEconomicCompiler", "PresenceCycleFix", "Make the authored stage ladder acyclic."));
	}
	TArray<Hansa::Simulation::FHansaCompiledGoodDefinition> CompiledGoods;
	TArray<Hansa::Simulation::FHansaCompiledRecipeDefinition> CompiledRecipes;
	TArray<Hansa::Simulation::FHansaCompiledBuildingDefinition> CompiledBuildings;
	TArray<Hansa::Simulation::FHansaCompiledNeedDefinition> CompiledNeeds;
	TArray<Hansa::Simulation::FHansaCompiledPopulationTierDefinition> CompiledPopulationTiers;
	TArray<Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition> CompiledCityMarkets;
	TArray<Hansa::Simulation::FHansaCompiledProductionChainDefinition> CompiledProductionChains;
	TArray<Hansa::Simulation::FHansaCompiledRegionEconomicProfileDefinition> CompiledRegions;
	TArray<Hansa::Simulation::FHansaCompiledVehicleDefinition> CompiledVehicles;
	TArray<Hansa::Simulation::FHansaCompiledRouteDefinition> CompiledRoutes;
	TArray<Hansa::Simulation::FHansaCompiledPresenceCapabilityDefinition> CompiledPresenceCapabilities;
	TArray<Hansa::Simulation::FHansaCompiledForeignPresenceStageDefinition> CompiledPresenceStages;
	TArray<Hansa::Simulation::FHansaCompiledCityTradePolicyDefinition> CompiledCityTradePolicies;
	TArray<Hansa::Simulation::FHansaCompiledTechnologyDefinition> CompiledTechnologies;
	TArray<Hansa::Simulation::FHansaCompiledMerchantAITuning> CompiledMerchantAITunings;
	TArray<Hansa::Simulation::FHansaCompiledScenarioObjective> CompiledScenarioObjectives;
	TArray<Hansa::Simulation::FHansaCompiledVictoryDefinition> CompiledVictories;
	TArray<Hansa::Simulation::FHansaCompiledScenarioDefinition> CompiledScenarios;
	FString RegistryCanonicalData;
	for (const UHansaDefinitionBase* Definition : SortedDefinitions)
	{
		const uint64 ContentHash = Definition->ComputeDeterministicContentHash();
		Result.DefinitionHashes.Add({
			Definition->GetClass()->GetPathName(),
			Definition->StableDefinitionId,
			ContentHash
		});
		RegistryCanonicalData += FString::Printf(
			TEXT("%s|%s|%llu\n"),
			*Definition->GetClass()->GetPathName(),
			*Definition->StableDefinitionId,
			static_cast<unsigned long long>(ContentHash));

		if (const UHansaGoodDefinition* Good = Cast<UHansaGoodDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledGoodDefinition Compiled;
			Compiled.StableId = Good->StableDefinitionId;
			Compiled.Unit = StaticEnum<EHansaGoodUnit>()->GetNameStringByValue(static_cast<int64>(Good->QuantityUnit));
			Compiled.BaseValueMilliMarks = Good->BaseValueMilliMarks;
			Compiled.PriceElasticityBasisPoints = Good->PriceElasticityBasisPoints;
			Compiled.SpoilageBasisPointsPerDay = Good->SpoilageBasisPointsPerDay;
			Compiled.ContentHash = ContentHash;
			Compiled.DisplayName = Good->DisplayName.ToString();
			CompiledGoods.Add(MoveTemp(Compiled));
            CompiledGoods.Last().bSpoilageEnabled = Good->bSpoilageEnabled;
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
            CompiledRecipes.Last().InternalCatchRecipeId = Recipe->InternalCatchRecipeId;
		}
		else if (const UHansaBuildingDefinition* Building = Cast<UHansaBuildingDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledBuildingDefinition Compiled;
			Compiled.StableId = Building->StableDefinitionId;
			Compiled.SchemaVersion = Building->SchemaVersion;
			Compiled.MaximumMarketRoadDistanceCells = Building->MaximumMarketRoadDistanceCells;
			Compiled.DisplayName = Building->DisplayName.ToString();
			Compiled.ConstructionCosts = CompileAmounts(Building->ConstructionCosts);
			Compiled.ConstructionCostPfennig = Building->ConstructionCostPfennig;
			Compiled.CancellationRefundBasisPoints = Building->CancellationRefundBasisPoints;
			Compiled.RecipeIds = Building->RecipeIds;
			Compiled.RecipeIds.Sort();
			Compiled.UpgradeTargetBuildingId = Building->UpgradeTargetBuildingId;
			Compiled.FootprintWidthCells = Building->FootprintWidthCells;
            if(const auto* Compound=Building->LoadResidentialCompound())
            {
             Compiled.ResidentialCompoundId=Compound->StableDefinitionId;
             Compiled.CompoundStage=Building->CompoundStage;
             Compiled.CompoundDistrictId=Building->CompoundDistrictId;
             Compiled.CompoundRoadFrontMask=static_cast<uint8>(Compound->AllowedRoadFrontMask);
             for(const auto& L:Compound->Layouts)if(L.DevelopmentStage==Building->CompoundStage&&(L.DistrictIds.IsEmpty()||L.DistrictIds.Contains(Building->CompoundDistrictId)))
              Compiled.CompoundLayoutContextMask|=L.Context==TEXT("Straight")?1:L.Context==TEXT("CornerLeft")?2:L.Context==TEXT("CornerRight")?4:L.Context==TEXT("Edge")?8:0;
            }
			Compiled.FootprintHeightCells = Building->FootprintHeightCells;
			Compiled.BuildTicks = Building->BuildTicks;
			Compiled.StorageCapacityMilliUnits = Building->StorageCapacityMilliUnits;
			Compiled.ResidenceCapacity = Building->ResidenceCapacity;
			Compiled.ResidentPopulationTierId = Building->ResidentPopulationTierId;
			Compiled.LaborerWorkforce = Building->LaborerWorkforce;
			Compiled.ArtisanWorkforce = Building->ArtisanWorkforce;
			Compiled.bRequiresRoad = Building->bRequiresRoad;
			// Schema v3 used the canonical Market identity before the capability became authored.
			Compiled.bProvidesMarketAccess = Building->SchemaVersion >= 4
				? Building->bProvidesMarketAccess
				: Building->StableDefinitionId == TEXT("Building.Market");
			Compiled.bRequiresShoreline = Building->bRequiresShoreline;
			Compiled.bShowInConstructionMenu = Building->bShowInConstructionMenu;
			Compiled.ConstructionMenuCategory = StaticEnum<EHansaConstructionMenuCategory>()->GetNameStringByValue(
				static_cast<int64>(Building->ConstructionMenuCategory));
			Compiled.ConstructionTier = Building->ConstructionTier == EHansaConstructionTier::Legacy ? FString() :
                StaticEnum<EHansaConstructionTier>()->GetNameStringByValue(static_cast<int64>(Building->ConstructionTier));
            Compiled.ConstructionMenuOrder = Building->ConstructionMenuOrder;
			Compiled.ConstructionChainOutputGoodId = Building->ConstructionChainOutputGoodId;
			Compiled.ConstructionChainStage = Building->ConstructionChainStage;
			Compiled.ConstructionChainStageCount = Building->ConstructionChainStageCount;
			Compiled.RequiredConstructionTechnologyId = Building->RequiredConstructionTechnologyId;
			Compiled.bUpgradeOnly = Building->bUpgradeOnly;
			Compiled.ConstructionPresentationPurpose = Building->ConstructionPresentationPurpose.ToString();
			Compiled.ContentHash = ContentHash;
			CompiledBuildings.Add(MoveTemp(Compiled));
		}
		else if (const UHansaNeedDefinition* Need = Cast<UHansaNeedDefinition>(Definition))
		{
			CompiledNeeds.Add({ Need->StableDefinitionId,
				Need->Kind == EHansaNeedKind::Good
					? Hansa::Simulation::EHansaCompiledNeedKind::Good
					: Hansa::Simulation::EHansaCompiledNeedKind::Service,
				Need->GoodId, ContentHash, Need->bSeasonal, Need->SeasonDays, Need->FixedSeason, Need->DefaultReserveDays, Need->SeasonMultipliers });
   for (const auto& Alternative : Need->Alternatives)
    CompiledNeeds.Last().Alternatives.Add({Alternative.GoodId, Alternative.FulfillmentBasisPoints});
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
		else if (const UHansaProductionChainDefinition* Chain = Cast<UHansaProductionChainDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledProductionChainDefinition Compiled;
			Compiled.StableId=Chain->StableDefinitionId; Compiled.ContentHash=ContentHash;
			for(const auto& Stage:Chain->Stages)
			{
				Hansa::Simulation::FHansaCompiledProductionChainStage Out;
				Out.StageKey=Stage.StageKey; Out.RecipeId=Stage.RecipeId; Out.PrerequisiteStageKeys=Stage.PrerequisiteStageKeys;
				Out.PrerequisiteStageKeys.Sort(); Out.Role=static_cast<uint8>(Stage.Role); Out.IntendedConstructionTier=Stage.IntendedConstructionTier;
				Compiled.Stages.Add(MoveTemp(Out));
			}
			CompiledProductionChains.Add(MoveTemp(Compiled));
		}
		else if (const UHansaRegionEconomicProfileDefinition* Region = Cast<UHansaRegionEconomicProfileDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledRegionEconomicProfileDefinition Compiled;
			Compiled.StableId=Region->StableDefinitionId; Compiled.MemberCityIds=Region->MemberCityIds; Compiled.MemberCityIds.Sort();
			for(const auto& Permit:Region->PermittedStages){Hansa::Simulation::FHansaCompiledRegionPermittedStage Out;Out.ProductionChainId=Permit.ProductionChainId;Out.StageKeys=Permit.StageKeys;Out.StageKeys.Sort();Compiled.PermittedStages.Add(MoveTemp(Out));}
			Compiled.PermittedStages.Sort([](const auto& L,const auto& R){return L.ProductionChainId<R.ProductionChainId;});
			for(const auto& Resource:Region->ResourceEndowments)Compiled.ResourceEndowments.Add({Resource.GoodId,static_cast<uint8>(Resource.Endowment),Resource.SourceCapacityMilliUnitsPerUpdate});
			Compiled.ResourceEndowments.Sort([](const auto& L,const auto& R){return L.GoodId<R.GoodId;});
			Compiled.ExchangeCapacityMilliUnitsPerUpdate=Region->ExchangeCapacityMilliUnitsPerUpdate; Compiled.ExchangeDelayUpdates=Region->ExchangeDelayUpdates;
			Compiled.TransportLossBasisPoints=Region->TransportLossBasisPoints; Compiled.TransportCostMilliMarksPerUnit=Region->TransportCostMilliMarksPerUnit; Compiled.ContentHash=ContentHash;
			CompiledRegions.Add(MoveTemp(Compiled));
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
			Compiled.bMarketOnly = CityMarket->bMarketOnly;
			Compiled.PresentationClass = static_cast<uint8>(CityMarket->PresentationClass);
			Compiled.MapLongitudeMilliDegrees = CityMarket->MapLongitudeMilliDegrees;
			Compiled.MapLatitudeMilliDegrees = CityMarket->MapLatitudeMilliDegrees;
			// Old accepted assets predate explicit presentation metadata. Keep them truthful without
			// changing save identity: only the historical MVP cities receive their documented roles.
			if (CityMarket->PresentationClass == EHansaCityPresentationClass::Unspecified)
			{
				Compiled.PresentationClass = static_cast<uint8>(CityMarket->StableDefinitionId == TEXT("City.Lubeck")
					? EHansaCityPresentationClass::RenderedBuildable
					: CityMarket->StableDefinitionId == TEXT("City.Rostock")
						? EHansaCityPresentationClass::RenderedVisitable
						: EHansaCityPresentationClass::MarketOnly);
			}
			Compiled.RegionId = CityMarket->RegionId;
			for(const auto& Binding:CityMarket->IndustryBindings)
			{
				Hansa::Simulation::FHansaCompiledCityIndustryBinding Out;
				Out.ProductionChainId=Binding.ProductionChainId;Out.EnabledStageKeys=Binding.EnabledStageKeys;Out.EnabledStageKeys.Sort();Out.CyclesPerMarketUpdate=Binding.CyclesPerMarketUpdate;Out.EfficiencyBasisPoints=Binding.EfficiencyBasisPoints;Out.InputReserveMilliUnits=Binding.InputReserveMilliUnits;Out.OutputReserveMilliUnits=Binding.OutputReserveMilliUnits;Out.bEnabled=Binding.bEnabled;Out.SignatureRank=Binding.SignatureRank;
				Compiled.IndustryBindings.Add(MoveTemp(Out));
			}
			Compiled.IndustryBindings.Sort([](const auto& L,const auto& R){return L.ProductionChainId<R.ProductionChainId;});
			Compiled.ReportCadenceTicks = CityMarket->ReportCadenceTicks;
			Compiled.CurrentReportMaxAgeTicks = CityMarket->CurrentReportMaxAgeTicks;
			Compiled.RecentReportMaxAgeTicks = CityMarket->RecentReportMaxAgeTicks;
			Compiled.StaleReportMaxAgeTicks = CityMarket->StaleReportMaxAgeTicks;
			Compiled.EstimatedReportMaxAgeTicks = CityMarket->EstimatedReportMaxAgeTicks;
			for (const FHansaMarketGoodProfile& Profile : CityMarket->Goods)
			{
				Compiled.Goods.Add({ Profile.GoodId, Profile.DesiredReserveMilliUnits,
					Profile.ConfirmedIncomingSupplyMilliUnits, Profile.InitialStockMilliUnits,
					Profile.BackgroundProductionMilliUnitsPerUpdate,
					Profile.BackgroundCitizenDemandMilliUnitsPerUpdate,
					Profile.BackgroundIndustrialDemandMilliUnitsPerUpdate, Profile.SeasonModifierBasisPoints,
					Profile.CityModifierBasisPoints, Profile.MinimumPriceMilliMarks,
					Profile.MaximumPriceMilliMarks, Profile.InitialPriceMilliMarks });
			}
			Compiled.Goods.Sort([](const auto& Left, const auto& Right) { return Left.GoodId < Right.GoodId; });
			Compiled.ContentHash = ContentHash;
			CompiledCityMarkets.Add(MoveTemp(Compiled));
		}
		else if (const UHansaPresenceCapabilityDefinition* Capability = Cast<UHansaPresenceCapabilityDefinition>(Definition))
		{
			CompiledPresenceCapabilities.Add({Capability->StableDefinitionId, Capability->DisplayName.ToString(), Capability->Semantics.ToString(), Capability->CapabilitySchemaVersion, ContentHash});
		}
		else if (const UHansaForeignPresenceStageDefinition* Stage = Cast<UHansaForeignPresenceStageDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledForeignPresenceStageDefinition Compiled;
			Compiled.StableId=Stage->StableDefinitionId; Compiled.DisplayName=Stage->DisplayName.ToString(); Compiled.Ordinal=Stage->Ordinal;
			Compiled.PrerequisiteStageIds=Stage->PrerequisiteStageIds; Compiled.PrerequisiteStageIds.Sort(); Compiled.GrantedCapabilityIds=Stage->GrantedCapabilityIds; Compiled.GrantedCapabilityIds.Sort();
			for(FName Value:Stage->PermittedPlotCategories)Compiled.PermittedPlotCategories.Add(Value.ToString()); for(FName Value:Stage->PermittedBuildingCategories)Compiled.PermittedBuildingCategories.Add(Value.ToString()); Compiled.PermittedPlotCategories.Sort(); Compiled.PermittedBuildingCategories.Sort();
			Compiled.UpgradeCostPfennig=Stage->UpgradeCostPfennig; for(const auto& Cost:Stage->UpgradeGoods)Compiled.UpgradeGoods.Add({Cost.GoodId,Cost.QuantityMilliUnits}); Compiled.UpgradeGoods.Sort([](const auto& L,const auto& R){return L.GoodId<R.GoodId;});
			Compiled.RequiredLawfulTradeVolumeMilliUnits=Stage->RequiredLawfulTradeVolumeMilliUnits; Compiled.RequiredCompletedDeliveries=Stage->RequiredCompletedDeliveries; Compiled.RequiredInvestedPfennig=Stage->RequiredInvestedPfennig;
			Compiled.RequiredTransactionValuePfennig=Stage->RequiredTransactionValuePfennig; Compiled.RequiredFulfilledShortageMilliUnits=Stage->RequiredFulfilledShortageMilliUnits;
			Compiled.RequiredReliableOperatingTicks=Stage->RequiredReliableOperatingTicks; Compiled.RequiredSolventOperatingTicks=Stage->RequiredSolventOperatingTicks; Compiled.UpgradeConstructionTicks=Stage->UpgradeConstructionTicks;
			Compiled.ContentHash=ContentHash; CompiledPresenceStages.Add(MoveTemp(Compiled));
		}
		else if (const UHansaCityTradePolicyDefinition* Policy = Cast<UHansaCityTradePolicyDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledCityTradePolicyDefinition Compiled;
			Compiled.StableId=Policy->StableDefinitionId; Compiled.CityId=Policy->CityId; Compiled.InitialStageId=Policy->InitialStageId; Compiled.AllowedStageIds=Policy->AllowedStageIds; Compiled.AllowedStageIds.Sort(); Compiled.DeniedCapabilityIds=Policy->DeniedCapabilityIds; Compiled.DeniedCapabilityIds.Sort();
			for(FName Value:Policy->AllowedPlotCategories)Compiled.AllowedPlotCategories.Add(Value.ToString()); for(FName Value:Policy->AllowedBuildingCategories)Compiled.AllowedBuildingCategories.Add(Value.ToString()); Compiled.AllowedPlotCategories.Sort(); Compiled.AllowedBuildingCategories.Sort(); Compiled.bPublicMarketAccess=Policy->bPublicMarketAccess; Compiled.bExceptionalGovernanceAllowed=Policy->bExceptionalGovernanceAllowed; Compiled.ContentHash=ContentHash;
			Compiled.MaximumStationOrders=Policy->MaximumStationOrders; Compiled.MaximumOrderCapMilliUnits=Policy->MaximumOrderCapMilliUnits; Compiled.MaximumOrderBudgetPfennig=Policy->MaximumOrderBudgetPfennig;
			Compiled.MerchantOfficeStorageBonusMilliUnits=Policy->MerchantOfficeStorageBonusMilliUnits; Compiled.MerchantOfficeAdditionalOrderSlots=Policy->MerchantOfficeAdditionalOrderSlots;
			for(const auto& Branch:Policy->Specializations)
			{
				Hansa::Simulation::FHansaCompiledCityTradePolicyDefinition::FSpecialization Out;
				Out.SpecializationId=Branch.SpecializationId.ToString();Out.DisplayName=Branch.DisplayName.ToString();Out.RequiredStageId=Branch.RequiredStageId;Out.ExclusiveGroupId=Branch.ExclusiveGroupId.ToString();
				Out.GrantedCapabilityIds=Branch.GrantedCapabilityIds;Out.GrantedCapabilityIds.Sort();Out.InvestmentCostPfennig=Branch.InvestmentCostPfennig;
				for(const auto& Cost:Branch.InvestmentGoods)Out.InvestmentGoods.Add({Cost.GoodId,Cost.QuantityMilliUnits});Out.InvestmentGoods.Sort([](const auto& L,const auto& R){return L.GoodId<R.GoodId;});
				Out.bAllowRespec=Branch.bAllowRespec;Out.RespecRefundBasisPoints=Branch.RespecRefundBasisPoints;Out.StorageCapacityBonusMilliUnits=Branch.StorageCapacityBonusMilliUnits;Out.AdditionalOrderSlots=Branch.AdditionalOrderSlots;Out.StationTransferCapBonusMilliUnits=Branch.StationTransferCapBonusMilliUnits;
				Compiled.Specializations.Add(MoveTemp(Out));
			}
			Compiled.Specializations.Sort([](const auto& L,const auto& R){return L.SpecializationId<R.SpecializationId;});
 for(const auto& Privilege:Policy->Privileges){Hansa::Simulation::FHansaCompiledCityTradePolicyDefinition::FPrivilege Out;Out.PrivilegeId=Privilege.PrivilegeId.ToString();Out.DisplayName=Privilege.DisplayName.ToString();Out.RequiredStageId=Privilege.RequiredStageId;Out.CostPfennig=Privilege.CostPfennig;for(const auto& Cost:Privilege.CostGoods)Out.CostGoods.Add({Cost.GoodId,Cost.QuantityMilliUnits});Out.DurationTicks=Privilege.DurationTicks;Out.bReversible=Privilege.bReversible;Out.LeaseBoundsMin={Privilege.LeaseBoundsMin.X,Privilege.LeaseBoundsMin.Y};Out.LeaseBoundsMax={Privilege.LeaseBoundsMax.X,Privilege.LeaseBoundsMax.Y};for(FName C:Privilege.PermittedBuildingCategories)Out.PermittedBuildingCategories.Add(C.ToString());Out.PermittedBuildingCategories.Sort();Compiled.Privileges.Add(MoveTemp(Out));}Compiled.Privileges.Sort([](const auto& L,const auto& R){return L.PrivilegeId<R.PrivilegeId;});
 for(const auto& Project:Policy->CityProjects){Hansa::Simulation::FHansaCompiledCityTradePolicyDefinition::FCityProject Out;Out.ProjectId=Project.ProjectId.ToString();Out.DisplayName=Project.DisplayName.ToString();Out.RequiredStageId=Project.RequiredStageId;Out.CostPfennig=Project.CostPfennig;for(const auto& Cost:Project.CostGoods)Out.CostGoods.Add({Cost.GoodId,Cost.QuantityMilliUnits});Out.ConstructionTicks=Project.ConstructionTicks;Out.SharedReserveGoodId=Project.SharedReserveGoodId;Out.SharedReserveBonusMilliUnits=Project.SharedReserveBonusMilliUnits;Compiled.CityProjects.Add(MoveTemp(Out));}Compiled.CityProjects.Sort([](const auto& L,const auto& R){return L.ProjectId<R.ProjectId;});
 Compiled.GovernanceScenarioIds=Policy->GovernanceScenarioIds;Compiled.GovernanceScenarioIds.Sort();Compiled.GovernanceCharterId=Policy->GovernanceCharterId.ToString();
			for(const auto& Site:Policy->TradeStationSites) { TArray<FString> Categories; for (FName Category : Site.PermittedBuildingCategories) Categories.Add(Category.ToString()); Categories.Sort(); Compiled.TradeStationSites.Add({Site.SiteId.ToString(),Site.PlotCategory.ToString(),Site.StorageCapacityMilliUnits,Site.ConstructionTicks,Site.UpkeepPfennigPerTick,Site.CancellationRefundBasisPoints,Site.PresentationClass.ToString(),{Site.LeaseBoundsMin.X,Site.LeaseBoundsMin.Y},{Site.LeaseBoundsMax.X,Site.LeaseBoundsMax.Y},MoveTemp(Categories)}); }
			Compiled.TradeStationSites.Sort([](const auto& L,const auto& R){return L.SiteId<R.SiteId;});
			CompiledCityTradePolicies.Add(MoveTemp(Compiled));
		}		else if (const UHansaVehicleDefinition* Vehicle = Cast<UHansaVehicleDefinition>(Definition))
		{
			CompiledVehicles.Add({ Vehicle->StableDefinitionId,
				Vehicle->Mode == EHansaAuthoredRouteMode::Sea
					? Hansa::Simulation::EHansaRouteMode::Sea : Hansa::Simulation::EHansaRouteMode::Land,
				Vehicle->CargoCapacityMilliUnits, Vehicle->UpkeepPfennigPerTravelTick, ContentHash });
		}
		else if (const UHansaRouteDefinition* Route = Cast<UHansaRouteDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledRouteDefinition Compiled;
			Compiled.StableId = Route->StableDefinitionId;
			Compiled.Mode = Route->Mode == EHansaAuthoredRouteMode::Sea
				? Hansa::Simulation::EHansaRouteMode::Sea : Hansa::Simulation::EHansaRouteMode::Land;
			Compiled.CargoRuleSchemaVersion = Route->CargoRuleSchemaVersion;
			for (const FHansaRouteConnectionDefinition& Connection : Route->Connections)
			{
				Compiled.Connections.Add({ Connection.SourceCityId, Connection.DestinationCityId,
					Connection.TravelTicks });
			}
			Compiled.Connections.Sort([](const auto& Left, const auto& Right)
			{
				const FString LeftKey = Left.SourceCityId + TEXT("|") + Left.DestinationCityId;
				const FString RightKey = Right.SourceCityId + TEXT("|") + Right.DestinationCityId;
				return LeftKey < RightKey;
			});
			Compiled.ContentHash = ContentHash;
			CompiledRoutes.Add(MoveTemp(Compiled));
		}
		else if (const UHansaTechnologyDefinition* Technology = Cast<UHansaTechnologyDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledTechnologyDefinition Compiled;
			Compiled.StableId = Technology->StableDefinitionId;
			Compiled.DisplayName = Technology->DisplayName.ToString();
			Compiled.Branch = static_cast<Hansa::Simulation::EHansaResearchBranch>(Technology->Branch);
			Compiled.PrerequisiteTechnologyIds = Technology->PrerequisiteTechnologyIds;
			Compiled.PrerequisiteTechnologyIds.Sort();
			Compiled.CostResearchPoints = Technology->CostResearchPoints;
			Compiled.DurationTicks = Technology->DurationTicks;
			Compiled.UnlockExplanation = Technology->UnlockExplanation.ToString();
			for (const FHansaResearchEffectDefinition& Effect : Technology->Effects)
			{
				Compiled.Effects.Add({static_cast<Hansa::Simulation::EHansaResearchEffectKind>(Effect.Kind), Effect.TargetStableId, Effect.Magnitude});
			}
			Compiled.Effects.Sort([](const auto& Left, const auto& Right)
			{
				if (Left.Kind != Right.Kind) return static_cast<uint8>(Left.Kind) < static_cast<uint8>(Right.Kind);
				if (Left.TargetStableId != Right.TargetStableId) return Left.TargetStableId < Right.TargetStableId;
				return Left.Magnitude < Right.Magnitude;
			});
			Compiled.ContentHash = ContentHash;
			CompiledTechnologies.Add(MoveTemp(Compiled));
		}
		else if (const UHansaMerchantAITuningDefinition* Tuning = Cast<UHansaMerchantAITuningDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledMerchantAITuning Compiled;
			Compiled.StableId = Tuning->StableDefinitionId;
			Compiled.DecisionCadenceTicks = Tuning->DecisionCadenceTicks;
			Compiled.DecisionHistoryCapacity = Tuning->DecisionHistoryCapacity;
			Compiled.MinimumDestinationDemandGapMilliUnits = Tuning->MinimumDestinationDemandGapMilliUnits;
			Compiled.MinimumGrossMarginMilliMarks = Tuning->MinimumGrossMarginMilliMarks;
			Compiled.ShortageUtilityPerUnit = Tuning->ShortageUtilityPerUnit;
			Compiled.MarginUtilityPerMilliMark = Tuning->MarginUtilityPerMilliMark;
			Compiled.ResearchUtility = Tuning->ResearchUtility;
			Compiled.ProductionUtility = Tuning->ProductionUtility;
			Compiled.ProtectedCashReservePfennig = Tuning->ProtectedCashReservePfennig;
			Compiled.ActionCooldownTicks = Tuning->ActionCooldownTicks;
			Compiled.DirectTradeQuantityMilliUnits = Tuning->DirectTradeQuantityMilliUnits;
			Compiled.StationOrderTargetMilliUnits = Tuning->StationOrderTargetMilliUnits;
			Compiled.StationOrderCapMilliUnits = Tuning->StationOrderCapMilliUnits;
			Compiled.StationOrderBudgetPfennig = Tuning->StationOrderBudgetPfennig;
			Compiled.PresenceUtility = Tuning->PresenceUtility;
			Compiled.TargetCompletedTradeLegs = Tuning->TargetCompletedTradeLegs;
			Compiled.PreferredResearchTechnologyIds = Tuning->PreferredResearchTechnologyIds;
			for (const FHansaMerchantAITradePlanDefinition& Plan : Tuning->TradePlans)
			{
				Compiled.TradePlans.Add({Plan.StablePlanId, Plan.RouteDefinitionId, Plan.VehicleDefinitionId,
					Plan.SourceCityId, Plan.DestinationCityId, Plan.GoodId, Plan.QuantityLimitMilliUnits,
					Plan.MinimumSourceReserveMilliUnits, Plan.UtilityBias});
			}
			Compiled.TradePlans.Sort([](const auto& Left, const auto& Right) { return Left.StablePlanId < Right.StablePlanId; });
			Compiled.ContentHash = ContentHash;
			CompiledMerchantAITunings.Add(MoveTemp(Compiled));
		}
		else if (const UHansaScenarioObjectiveDefinition* Objective = Cast<UHansaScenarioObjectiveDefinition>(Definition))
		{
			CompiledScenarioObjectives.Add({Objective->StableDefinitionId, Objective->DisplayName.ToString(),
				static_cast<Hansa::Simulation::EHansaScenarioObjectiveMetric>(Objective->Metric), Objective->TargetValue,
				Objective->CityId, Objective->GoodId, Objective->ProgressUnit.ToString(), ContentHash});
		}
		else if (const UHansaVictoryDefinition* Victory = Cast<UHansaVictoryDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledVictoryDefinition Compiled;
			Compiled.StableId = Victory->StableDefinitionId;
			Compiled.DisplayName = Victory->DisplayName.ToString();
			Compiled.ObjectiveIds = Victory->ObjectiveIds;
			Compiled.ObjectiveIds.Sort();
			Compiled.SustainTicks = Victory->SustainTicks;
			Compiled.EndingPriority = Victory->EndingPriority;
			Compiled.Summary = Victory->Summary.ToString();
			Compiled.ContentHash = ContentHash;
			CompiledVictories.Add(MoveTemp(Compiled));
		}
		else if (const UHansaScenarioDefinition* Scenario = Cast<UHansaScenarioDefinition>(Definition))
		{
			Hansa::Simulation::FHansaCompiledScenarioDefinition Compiled;
			Compiled.StableId = Scenario->StableDefinitionId;
			Compiled.DisplayName = Scenario->DisplayName.ToString();
			Compiled.HomeCityId = Scenario->HomeCityId;
			Compiled.VictoryIds = Scenario->VictoryIds;
			Compiled.InsolvencyThresholdPfennig = Scenario->InsolvencyThresholdPfennig;
			Compiled.FailureSustainTicks = Scenario->FailureSustainTicks;
			Compiled.Briefing = Scenario->Briefing.ToString();
			for (const FHansaAuthoredScenarioSlotRule& Authored : Scenario->MultiplayerSlots)
			{
				Hansa::Simulation::FHansaScenarioSlotRule Rule;
				Rule.SlotId = Authored.SlotId;
				Rule.HouseId = Hansa::Simulation::FHansaHouseId::TryCreate(static_cast<uint64>(Authored.HouseId)).Value;
				Rule.DefaultState = static_cast<Hansa::Simulation::EHansaSessionSlotState>(Authored.DefaultState);
				for (const EHansaAuthoredSessionSlotState State : Authored.AllowedStates)
					Rule.AllowedStateMask |= 1u << static_cast<uint8>(State);
				if (Authored.AuthoredTeamId > 0)
					Rule.AuthoredTeamId = Hansa::Simulation::FHansaTeamId::TryCreate(static_cast<uint64>(Authored.AuthoredTeamId)).Value;
				Rule.bTeamRequired = Authored.bTeamRequired;
				Rule.bAllowHumanTakeover = Authored.bAllowHumanTakeover;
				Compiled.MultiplayerSlots.Add(MoveTemp(Rule));
			}
			Compiled.MultiplayerSlots.Sort([](const auto& Left, const auto& Right) { return Left.SlotId < Right.SlotId; });
			Compiled.ContentHash = ContentHash;
			CompiledScenarios.Add(MoveTemp(Compiled));
		}
	}

	Result.Registry = Hansa::Simulation::FHansaEconomicRegistry(
		MoveTemp(CompiledGoods),
		MoveTemp(CompiledRecipes),
		MoveTemp(CompiledBuildings),
		HashUtf8Fnv1a(RegistryCanonicalData),
		MoveTemp(CompiledNeeds),
		MoveTemp(CompiledPopulationTiers),
		MoveTemp(CompiledCityMarkets),
		MoveTemp(CompiledVehicles),
		MoveTemp(CompiledRoutes),
		MoveTemp(CompiledTechnologies),
		MoveTemp(CompiledMerchantAITunings),
		MoveTemp(CompiledScenarioObjectives),
		MoveTemp(CompiledVictories),
		MoveTemp(CompiledScenarios));
	Result.Registry.SetRegionalEconomy(MoveTemp(CompiledProductionChains),MoveTemp(CompiledRegions));
	Result.Registry.SetPresenceDefinitions(MoveTemp(CompiledPresenceCapabilities), MoveTemp(CompiledPresenceStages), MoveTemp(CompiledCityTradePolicies));
	return Result;
}
