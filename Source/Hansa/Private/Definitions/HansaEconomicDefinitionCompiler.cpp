#include "Definitions/HansaEconomicDefinitionCompiler.h"

#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
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
	TSet<FString> VehicleIds;
	TSet<FString> RouteIds;
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
		else if (Definition->IsA<UHansaTechnologyDefinition>())
		{
			TechnologyIds.Add(Definition->StableDefinitionId);
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

	TArray<Hansa::Simulation::FHansaCompiledGoodDefinition> CompiledGoods;
	TArray<Hansa::Simulation::FHansaCompiledRecipeDefinition> CompiledRecipes;
	TArray<Hansa::Simulation::FHansaCompiledBuildingDefinition> CompiledBuildings;
	TArray<Hansa::Simulation::FHansaCompiledNeedDefinition> CompiledNeeds;
	TArray<Hansa::Simulation::FHansaCompiledPopulationTierDefinition> CompiledPopulationTiers;
	TArray<Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition> CompiledCityMarkets;
	TArray<Hansa::Simulation::FHansaCompiledVehicleDefinition> CompiledVehicles;
	TArray<Hansa::Simulation::FHansaCompiledRouteDefinition> CompiledRoutes;
	TArray<Hansa::Simulation::FHansaCompiledTechnologyDefinition> CompiledTechnologies;
	TArray<Hansa::Simulation::FHansaCompiledMerchantAITuning> CompiledMerchantAITunings;
	TArray<Hansa::Simulation::FHansaCompiledScenarioObjective> CompiledScenarioObjectives;
	TArray<Hansa::Simulation::FHansaCompiledVictoryDefinition> CompiledVictories;
	TArray<Hansa::Simulation::FHansaCompiledScenarioDefinition> CompiledScenarios;
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
			Compiled.bMarketOnly = CityMarket->bMarketOnly;
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
		else if (const UHansaVehicleDefinition* Vehicle = Cast<UHansaVehicleDefinition>(Definition))
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
			CompiledScenarios.Add({Scenario->StableDefinitionId, Scenario->DisplayName.ToString(), Scenario->HomeCityId,
				Scenario->VictoryIds, Scenario->InsolvencyThresholdPfennig, Scenario->FailureSustainTicks,
				Scenario->Briefing.ToString(), ContentHash});
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
	return Result;
}
