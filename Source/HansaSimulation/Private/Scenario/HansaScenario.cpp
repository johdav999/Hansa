#include "Scenario/HansaScenario.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Population/HansaPopulation.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Research/HansaResearch.h"
#include "Trade/HansaTrade.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaScenarioOutcome Outcome)
	{
		switch (Outcome)
		{
		case EHansaScenarioOutcome::Active: return TEXT("Active");
		case EHansaScenarioOutcome::Victory: return TEXT("Victory");
		case EHansaScenarioOutcome::Failure: return TEXT("Failure");
		default: return TEXT("UnknownScenarioOutcome");
		}
	}

	const TCHAR* LexToString(const EHansaScenarioObjectiveMetric Metric)
	{
		switch (Metric)
		{
		case EHansaScenarioObjectiveMetric::HouseMoneyAtLeast: return TEXT("HouseMoneyAtLeast");
		case EHansaScenarioObjectiveMetric::CityPopulationAtLeast: return TEXT("CityPopulationAtLeast");
		case EHansaScenarioObjectiveMetric::ArtisanPopulationAtLeast: return TEXT("ArtisanPopulationAtLeast");
		case EHansaScenarioObjectiveMetric::ActiveSeaRoutesAtLeast: return TEXT("ActiveSeaRoutesAtLeast");
		case EHansaScenarioObjectiveMetric::ActiveLandRoutesAtLeast: return TEXT("ActiveLandRoutesAtLeast");
		case EHansaScenarioObjectiveMetric::CompletedTradeLegsAtLeast: return TEXT("CompletedTradeLegsAtLeast");
		case EHansaScenarioObjectiveMetric::CompletedTechnologiesAtLeast: return TEXT("CompletedTechnologiesAtLeast");
		case EHansaScenarioObjectiveMetric::ActiveProductionsAtLeast: return TEXT("ActiveProductionsAtLeast");
		case EHansaScenarioObjectiveMetric::CitySatisfactionAtLeast: return TEXT("CitySatisfactionAtLeast");
		case EHansaScenarioObjectiveMetric::MarketStockAtLeast: return TEXT("MarketStockAtLeast");
		case EHansaScenarioObjectiveMetric::MarketPriceAtMost: return TEXT("MarketPriceAtMost");
		default: return TEXT("UnknownScenarioObjectiveMetric");
		}
	}

	namespace
	{
		int64 Measure(const FHansaCompiledScenarioObjective& Objective,
			const FHansaSimulationReadOnlyAccess& ReadOnly, const FHansaHouseId HouseId)
		{
			switch (Objective.Metric)
			{
			case EHansaScenarioObjectiveMetric::HouseMoneyAtLeast:
				for (const FHansaHouseState& House : ReadOnly.GetHouses()) if (House.Id == HouseId) return House.Money.GetRawValue();
				return 0;
			case EHansaScenarioObjectiveMetric::ActiveSeaRoutesAtLeast:
			case EHansaScenarioObjectiveMetric::ActiveLandRoutesAtLeast:
			{
				const EHansaRouteMode Mode = Objective.Metric == EHansaScenarioObjectiveMetric::ActiveSeaRoutesAtLeast
					? EHansaRouteMode::Sea : EHansaRouteMode::Land;
				int64 Count = 0;
				for (const FHansaRouteState& Route : ReadOnly.GetRoutes())
					Count += Route.OwnerId == HouseId && Route.Mode == Mode &&
						Route.Lifecycle != EHansaRouteLifecycleState::Inactive && Route.Lifecycle != EHansaRouteLifecycleState::Cancelled ? 1 : 0;
				return Count;
			}
			case EHansaScenarioObjectiveMetric::CompletedTradeLegsAtLeast:
			{
				int64 Count = 0;
				for (const FHansaRouteState& Route : ReadOnly.GetRoutes()) if (Route.OwnerId == HouseId) Count += Route.CompletedLegCount;
				return Count;
			}
			case EHansaScenarioObjectiveMetric::CompletedTechnologiesAtLeast:
				for (const FHansaHouseResearchState& Research : ReadOnly.GetResearch())
					if (Research.HouseId == HouseId) return Research.CompletedTechnologyIds.Num();
				return 0;
			case EHansaScenarioObjectiveMetric::ActiveProductionsAtLeast:
			{
				int64 Count = 0;
				for (const FHansaProductionProjection& Production : ReadOnly.BuildProductionProjection())
				{
					if (!Production.bActive) continue;
					for (const FHansaBuildingState& Building : ReadOnly.GetBuildings())
					{
						if (Building.Id == Production.BuildingId && Building.OwnerId == HouseId) { ++Count; break; }
					}
				}
				return Count;
			}
			case EHansaScenarioObjectiveMetric::CityPopulationAtLeast:
			case EHansaScenarioObjectiveMetric::ArtisanPopulationAtLeast:
			case EHansaScenarioObjectiveMetric::CitySatisfactionAtLeast:
			{
				const auto CityId = FHansaCityDefinitionId::TryParse(Objective.CityId);
				if (!CityId) return 0;
				const TOptional<FHansaCityPopulationProjection> Population = ReadOnly.QueryCityPopulation(CityId.Value);
				if (!Population.IsSet()) return 0;
				if (Objective.Metric == EHansaScenarioObjectiveMetric::CityPopulationAtLeast) return Population->TotalResidents;
				if (Objective.Metric == EHansaScenarioObjectiveMetric::ArtisanPopulationAtLeast) return Population->ArtisanResidents;
				return Population->SatisfactionBasisPoints;
			}
			case EHansaScenarioObjectiveMetric::MarketStockAtLeast:
			case EHansaScenarioObjectiveMetric::MarketPriceAtMost:
			{
				const auto CityId = FHansaCityDefinitionId::TryParse(Objective.CityId);
				const auto GoodId = FHansaGoodId::TryParse(Objective.GoodId);
				if (!CityId || !GoodId) return 0;
				const TOptional<FHansaCityMarketProjection> Market = ReadOnly.QueryMarket(CityId.Value, GoodId.Value);
				if (!Market.IsSet()) return 0;
				return Objective.Metric == EHansaScenarioObjectiveMetric::MarketStockAtLeast
					? Market->CurrentStock.GetRawValue() : Market->CurrentPriceMilliMarks;
			}
			default: return 0;
			}
		}

		bool IsMet(const FHansaCompiledScenarioObjective& Objective, const int64 Current)
		{
			return Objective.Metric == EHansaScenarioObjectiveMetric::MarketPriceAtMost
				? Current <= Objective.TargetValue : Current >= Objective.TargetValue;
		}
	}

	bool FHansaScenarioEvaluator::Initialize(
		const FHansaEconomicRegistry& Registry, const FString& ScenarioId, const FHansaHouseId InHouseId)
	{
		const FHansaCompiledScenarioDefinition* Scenario = Registry.FindScenario(ScenarioId);
		if (Scenario == nullptr || !InHouseId.IsValid()) return false;
		HouseId = InHouseId;
		Progress = {};
		Progress.ScenarioId = Scenario->StableId;
		Progress.DisplayName = Scenario->DisplayName;
		Progress.Briefing = Scenario->Briefing;
		Progress.RequiredFailureTicks = Scenario->FailureSustainTicks;
		for (const FString& VictoryId : Scenario->VictoryIds)
		{
			const FHansaCompiledVictoryDefinition* Victory = Registry.FindVictory(VictoryId);
			if (Victory == nullptr) return false;
			FHansaVictoryPathProgress Path;
			Path.VictoryId = Victory->StableId;
			Path.DisplayName = Victory->DisplayName;
			Path.Summary = Victory->Summary;
			Path.EndingPriority = Victory->EndingPriority;
			Path.RequiredSustainTicks = Victory->SustainTicks;
			for (const FString& ObjectiveId : Victory->ObjectiveIds)
			{
				const FHansaCompiledScenarioObjective* Objective = Registry.FindScenarioObjective(ObjectiveId);
				if (Objective == nullptr) return false;
				Path.Objectives.Add({Objective->StableId, Objective->DisplayName, Objective->Metric,
					0, Objective->TargetValue, Objective->ProgressUnit, false});
			}
			Progress.VictoryPaths.Add(MoveTemp(Path));
		}
		return true;
	}

	bool FHansaScenarioEvaluator::Evaluate(
		const FHansaSimulationReadOnlyAccess& ReadOnly, const FHansaEconomicRegistry& Registry)
	{
		if (!IsInitialized() || Progress.Outcome != EHansaScenarioOutcome::Active) return IsInitialized();
		const FHansaCompiledScenarioDefinition* Scenario = Registry.FindScenario(Progress.ScenarioId);
		if (Scenario == nullptr) return false;
		Progress.LastEvaluatedTick = ReadOnly.GetClock().GetTick();
		bool bInsolvent = false;
		for (const FHansaHouseState& House : ReadOnly.GetHouses())
			if (House.Id == HouseId) { bInsolvent = House.Money.GetRawValue() <= Scenario->InsolvencyThresholdPfennig; break; }
		bool bViable = false;
		for (const FHansaRouteState& Route : ReadOnly.GetRoutes())
			bViable |= Route.OwnerId == HouseId && Route.Lifecycle != EHansaRouteLifecycleState::Inactive && Route.Lifecycle != EHansaRouteLifecycleState::Cancelled;
		for (const FHansaProductionProjection& Production : ReadOnly.BuildProductionProjection())
			bViable |= Production.bActive;
		Progress.ConsecutiveFailureTicks = bInsolvent && !bViable ? Progress.ConsecutiveFailureTicks + 1 : 0;

		for (FHansaVictoryPathProgress& Path : Progress.VictoryPaths)
		{
			Path.bAllObjectivesMet = true;
			for (FHansaScenarioObjectiveProgress& ObjectiveProgress : Path.Objectives)
			{
				const FHansaCompiledScenarioObjective* Objective = Registry.FindScenarioObjective(ObjectiveProgress.ObjectiveId);
				if (Objective == nullptr) return false;
				ObjectiveProgress.CurrentValue = Measure(*Objective, ReadOnly, HouseId);
				ObjectiveProgress.bMet = IsMet(*Objective, ObjectiveProgress.CurrentValue);
				Path.bAllObjectivesMet &= ObjectiveProgress.bMet;
			}
			Path.ConsecutiveSatisfiedTicks = Path.bAllObjectivesMet ? Path.ConsecutiveSatisfiedTicks + 1 : 0;
		}

		// Failure resolves first on the exact same tick to prevent a bankrupt final-tick exploit.
		if (Progress.ConsecutiveFailureTicks >= Progress.RequiredFailureTicks)
		{
			Progress.Outcome = EHansaScenarioOutcome::Failure;
			Progress.FailureReason = TEXT("Sustained insolvency with no active route or production.");
			return true;
		}
		FHansaVictoryPathProgress* Winner = nullptr;
		for (FHansaVictoryPathProgress& Path : Progress.VictoryPaths)
		{
			if (Path.ConsecutiveSatisfiedTicks < Path.RequiredSustainTicks) continue;
			if (Winner == nullptr || Path.EndingPriority < Winner->EndingPriority ||
				(Path.EndingPriority == Winner->EndingPriority && Path.VictoryId < Winner->VictoryId)) Winner = &Path;
		}
		if (Winner != nullptr)
		{
			Progress.Outcome = EHansaScenarioOutcome::Victory;
			Progress.WinningVictoryId = Winner->VictoryId;
			Winner->bVictorious = true;
		}
		return true;
	}
}
