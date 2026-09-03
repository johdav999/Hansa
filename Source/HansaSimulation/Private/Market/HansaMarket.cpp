#include "Market/HansaMarket.h"

#include "Market/HansaMarketInternal.h"

#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaMarketExplanationFactor Factor)
	{
		switch (Factor)
		{
		case EHansaMarketExplanationFactor::Scarcity: return TEXT("Scarcity");
		case EHansaMarketExplanationFactor::CitizenDemand: return TEXT("CitizenDemand");
		case EHansaMarketExplanationFactor::IndustrialDemand: return TEXT("IndustrialDemand");
		case EHansaMarketExplanationFactor::IncomingSupply: return TEXT("IncomingSupply");
		case EHansaMarketExplanationFactor::UnmetDemand: return TEXT("UnmetDemand");
		case EHansaMarketExplanationFactor::SeasonModifier: return TEXT("SeasonModifier");
		case EHansaMarketExplanationFactor::CityModifier: return TEXT("CityModifier");
		case EHansaMarketExplanationFactor::TargetClamp: return TEXT("TargetClamp");
		default: return TEXT("UnknownMarketFactor");
		}
	}

	const TCHAR* LexToString(const EHansaMarketAlertType Type)
	{
		switch (Type)
		{
		case EHansaMarketAlertType::Shortage: return TEXT("Shortage");
		case EHansaMarketAlertType::LowReserve: return TEXT("LowReserve");
		case EHansaMarketAlertType::Affordability: return TEXT("Affordability");
		default: return TEXT("UnknownMarketAlert");
		}
	}

	const TCHAR* LexToString(const EHansaMarketAlertSeverity Severity)
	{
		return Severity == EHansaMarketAlertSeverity::Critical ? TEXT("Critical") : TEXT("Warning");
	}

	const TCHAR* LexToString(const EHansaMarketConsumerKind Kind)
	{
		return Kind == EHansaMarketConsumerKind::Industry ? TEXT("Industry") : TEXT("Citizen");
	}

	const TCHAR* LexToString(const EHansaMarketProducerKind Kind)
	{
		return Kind == EHansaMarketProducerKind::BackgroundSupply ? TEXT("BackgroundSupply") : TEXT("BuildingRecipe");
	}

	const TCHAR* LexToString(const EHansaMarketSuggestedActionType Action)
	{
		switch (Action)
		{
		case EHansaMarketSuggestedActionType::IncreaseLocalProduction: return TEXT("IncreaseLocalProduction");
		case EHansaMarketSuggestedActionType::ImportGood: return TEXT("ImportGood");
		case EHansaMarketSuggestedActionType::InspectBlockedConsumers: return TEXT("InspectBlockedConsumers");
		case EHansaMarketSuggestedActionType::ReplenishReserve: return TEXT("ReplenishReserve");
		case EHansaMarketSuggestedActionType::ScheduleIncomingSupply: return TEXT("ScheduleIncomingSupply");
		case EHansaMarketSuggestedActionType::IncreaseAffordableSupply: return TEXT("IncreaseAffordableSupply");
		default: return TEXT("UnknownMarketAction");
		}
	}

	namespace
	{
		constexpr int32 BasisPointScale = 10000;
		constexpr int32 ScarcityWeight = 6000;
		constexpr int32 CitizenDemandWeight = 2500;
		constexpr int32 IndustrialDemandWeight = 2000;
		constexpr int32 IncomingSupplyWeight = 2500;
		constexpr int32 UnmetDemandWeight = 2000;
		constexpr int32 MinimumTargetMultiplier = 2500;
		constexpr int32 MaximumTargetMultiplier = 40000;

		int64 SafeAdd(const int64 Left, const int64 Right)
		{
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryAdd(Left, Right);
			return Result ? Result.Value : TNumericLimits<int64>::Max();
		}

		int32 RatioBasisPoints(const int64 Numerator, const int64 Denominator, const int32 Minimum, const int32 Maximum)
		{
			if (Denominator <= 0)
			{
				return 0;
			}
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Numerator, BasisPointScale, Denominator, EHansaRoundingMode::HalfAwayFromZero);
			return Result ? static_cast<int32>(FMath::Clamp<int64>(Result.Value, Minimum, Maximum))
				: (Numerator >= 0 ? Maximum : Minimum);
		}

		int32 WeightedFactor(const int32 Ratio, const int32 Weight)
		{
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Ratio, Weight, BasisPointScale, EHansaRoundingMode::HalfAwayFromZero);
			return Result ? static_cast<int32>(Result.Value) : 0;
		}

		FHansaQuantity SumStock(const FHansaCityMarketState& Market, const FHansaInventoryReadOnlyAccess& Inventories)
		{
			int64 Total = 0;
			for (const FHansaInventoryId InventoryId : Market.InventoryIds)
			{
				const TOptional<FHansaInventoryStockProjection> Stock = Inventories.QueryStock(InventoryId, Market.GoodId);
				if (Stock.IsSet())
				{
					Total = SafeAdd(Total, FMath::Max<int64>(0, Stock->Available.GetRawValue()));
				}
			}
			return FHansaQuantity::FromRaw(Total);
		}

		FHansaQuantity SumCitizenDemand(const FHansaCityMarketState& Market,
			const TArray<FHansaPopulationCohortState>& Cohorts)
		{
			int64 Total = 0;
			for (const FHansaPopulationCohortState& Cohort : Cohorts)
			{
				if (Cohort.CityId != Market.CityId)
				{
					continue;
				}
				for (const FHansaPopulationNeedState& Need : Cohort.Needs)
				{
					if (Need.GoodId == Market.GoodId)
					{
						Total = SafeAdd(Total, FMath::Max<int64>(0, Need.RequiredLastTick.GetRawValue()));
					}
				}
			}
			return FHansaQuantity::FromRaw(Total);
		}

		FHansaQuantity SumUnmetCitizenDemand(const FHansaCityMarketState& Market,
			const TArray<FHansaPopulationCohortState>& Cohorts)
		{
			int64 Total = 0;
			for (const FHansaPopulationCohortState& Cohort : Cohorts)
			{
				if (Cohort.CityId != Market.CityId)
				{
					continue;
				}
				for (const FHansaPopulationNeedState& Need : Cohort.Needs)
				{
					if (Need.GoodId == Market.GoodId)
					{
						Total = SafeAdd(Total, FMath::Max<int64>(0,
							Need.RequiredLastTick.GetRawValue() - Need.ConsumedLastTick.GetRawValue()));
					}
				}
			}
			return FHansaQuantity::FromRaw(Total);
		}

		int32 MinimumConsumerAffordability(const FHansaCityMarketState& Market,
			const TArray<FHansaPopulationCohortState>& Cohorts)
		{
			int32 Minimum = 10000;
			bool bHasConsumer = false;
			for (const FHansaPopulationCohortState& Cohort : Cohorts)
			{
				if (Cohort.CityId != Market.CityId)
				{
					continue;
				}
				for (const FHansaPopulationNeedState& Need : Cohort.Needs)
				{
					if (Need.GoodId == Market.GoodId && Need.RequiredLastTick.GetRawValue() > 0)
					{
						bHasConsumer = true;
						Minimum = FMath::Min(Minimum, Need.AffordabilityBasisPoints);
					}
				}
			}
			return bHasConsumer ? Minimum : 10000;
		}

		void UpdateAlertOnset(const bool bActive, const int64 Tick, int64& InOutSinceTick)
		{
			if (!bActive)
			{
				InOutSinceTick = -1;
			}
			else if (InOutSinceTick < 0)
			{
				InOutSinceTick = Tick;
			}
		}

		bool InventoryBelongsToMarket(const FHansaCityMarketState& Market, const FHansaInventoryId InventoryId)
		{
			return Market.InventoryIds.Contains(InventoryId);
		}

		FHansaQuantity SumIndustrialDemand(const FHansaCityMarketState& Market,
			const TArray<FHansaProductionState>& Productions, const FHansaEconomicRegistry& Registry)
		{
			int64 Total = 0;
			for (const FHansaProductionState& Production : Productions)
			{
				if (!Production.bActive || Production.Kind != EHansaProductionKind::BuildingRecipe ||
					!InventoryBelongsToMarket(Market, Production.InputInventoryId))
				{
					continue;
				}
				const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production.RecipeId.ToString());
				if (Recipe == nullptr || Recipe->CycleTicks <= 0)
				{
					continue;
				}
				for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
				{
					if (Input.GoodId == Market.GoodId.ToString())
					{
						const int64 PerTick = FMath::DivideAndRoundUp(Input.QuantityMilliUnits, static_cast<int64>(Recipe->CycleTicks));
						Total = SafeAdd(Total, FMath::Max<int64>(0, PerTick));
					}
				}
			}
			return FHansaQuantity::FromRaw(Total);
		}

		FHansaQuantity SumUnmetIndustrialDemand(const FHansaCityMarketState& Market,
			const TArray<FHansaProductionState>& Productions, const FHansaEconomicRegistry& Registry)
		{
			int64 Total = 0;
			for (const FHansaProductionState& Production : Productions)
			{
				if (!Production.bActive || Production.Kind != EHansaProductionKind::BuildingRecipe ||
					Production.Blocker != EHansaProductionBlocker::MissingInput ||
					Production.BlockingGoodId != Market.GoodId ||
					!InventoryBelongsToMarket(Market, Production.InputInventoryId))
				{
					continue;
				}
				const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production.RecipeId.ToString());
				if (Recipe != nullptr && Recipe->CycleTicks > 0)
				{
					for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
					{
						if (Input.GoodId == Market.GoodId.ToString())
						{
							Total = SafeAdd(Total, FMath::DivideAndRoundUp(
								Input.QuantityMilliUnits, static_cast<int64>(Recipe->CycleTicks)));
						}
					}
				}
			}
			return FHansaQuantity::FromRaw(Total);
		}

		FHansaQuantity SumLocalProduction(const FHansaCityMarketState& Market,
			const TArray<FHansaProductionState>& Productions, const FHansaEconomicRegistry& Registry)
		{
			int64 Total = 0;
			for (const FHansaProductionState& Production : Productions)
			{
				if (!Production.bCompletedCycleLastTick || !InventoryBelongsToMarket(Market, Production.OutputInventoryId))
				{
					continue;
				}
				if (Production.Kind == EHansaProductionKind::BackgroundSupply)
				{
					if (Production.SupplyGoodId == Market.GoodId)
					{
						Total = SafeAdd(Total, Production.SupplyQuantityPerCycle.GetRawValue());
					}
					continue;
				}
				const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production.RecipeId.ToString());
				if (Recipe == nullptr)
				{
					continue;
				}
				for (const FHansaCompiledGoodAmount& Output : Recipe->Outputs)
				{
					if (Output.GoodId == Market.GoodId.ToString())
					{
						Total = SafeAdd(Total, FMath::Max<int64>(0, Output.QuantityMilliUnits));
					}
				}
			}
			return FHansaQuantity::FromRaw(Total);
		}

		void UpdatePrice(FHansaCityMarketState& Market, const FHansaMarketSettings& Settings,
			const FHansaCompiledGoodDefinition& Good)
		{
			const int64 Reserve = Market.DesiredReserve.GetRawValue();
			const int64 Stock = Market.CurrentStock.GetRawValue();
			const int64 Citizen = Market.CitizenDemand.GetRawValue();
			const int64 Industrial = Market.IndustrialDemand.GetRawValue();
			const int64 Incoming = Market.ExpectedIncomingSupply.GetRawValue();
			const int64 Unmet = Market.UnmetDemand.GetRawValue();
			const int64 PressureDenominator = FMath::Max<int64>(1, Reserve);

			Market.Factors.ScarcityBasisPoints = WeightedFactor(
				RatioBasisPoints(Reserve - Stock, PressureDenominator, -5000, 10000), ScarcityWeight);
			Market.Factors.CitizenDemandBasisPoints = WeightedFactor(
				RatioBasisPoints(Citizen, PressureDenominator, 0, 10000), CitizenDemandWeight);
			Market.Factors.IndustrialDemandBasisPoints = WeightedFactor(
				RatioBasisPoints(Industrial, PressureDenominator, 0, 10000), IndustrialDemandWeight);
			Market.Factors.IncomingSupplyBasisPoints = -WeightedFactor(
				RatioBasisPoints(Incoming, PressureDenominator, 0, 10000), IncomingSupplyWeight);
			Market.Factors.UnmetDemandBasisPoints = WeightedFactor(
				RatioBasisPoints(Unmet, PressureDenominator, 0, 10000), UnmetDemandWeight);
			Market.Factors.SeasonModifierBasisPoints = Market.SeasonModifierBasisPoints;
			Market.Factors.CityModifierBasisPoints = Market.CityModifierBasisPoints;
			const int64 Multiplier = static_cast<int64>(BasisPointScale) + Market.Factors.ScarcityBasisPoints +
				Market.Factors.CitizenDemandBasisPoints + Market.Factors.IndustrialDemandBasisPoints +
				Market.Factors.IncomingSupplyBasisPoints + Market.Factors.UnmetDemandBasisPoints +
				Market.Factors.SeasonModifierBasisPoints + Market.Factors.CityModifierBasisPoints;
			Market.Factors.TargetMultiplierBasisPoints = static_cast<int32>(
				FMath::Clamp<int64>(Multiplier, MinimumTargetMultiplier, MaximumTargetMultiplier));

			const THansaValueResult<int64> TargetResult = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Good.BaseValueMilliMarks, Market.Factors.TargetMultiplierBasisPoints, BasisPointScale,
				EHansaRoundingMode::HalfAwayFromZero);
			const int64 Target = FMath::Clamp(TargetResult ? TargetResult.Value : Market.MaximumPriceMilliMarks,
				Market.MinimumPriceMilliMarks, Market.MaximumPriceMilliMarks);
			const int64 Difference = Target - Market.CurrentPriceMilliMarks;
			const THansaValueResult<int64> SmoothedResult = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Difference, Settings.TargetSmoothingBasisPoints, BasisPointScale, EHansaRoundingMode::HalfAwayFromZero);
			int64 SmoothedMovement = SmoothedResult ? SmoothedResult.Value : 0;
			const THansaValueResult<int64> MaximumMoveResult = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Market.CurrentPriceMilliMarks, Settings.MaximumMovementBasisPointsPerUpdate, BasisPointScale,
				EHansaRoundingMode::Ceiling);
			const int64 MaximumMove = FMath::Max<int64>(1, MaximumMoveResult ? MaximumMoveResult.Value : 1);
			SmoothedMovement = FMath::Clamp(SmoothedMovement, -MaximumMove, MaximumMove);
			if (Difference != 0 && SmoothedMovement == 0)
			{
				SmoothedMovement = Difference > 0 ? 1 : -1;
			}
			Market.CurrentPriceMilliMarks = FMath::Clamp(Market.CurrentPriceMilliMarks + SmoothedMovement,
				Market.MinimumPriceMilliMarks, Market.MaximumPriceMilliMarks);
		}
	}

	void FHansaMarketExecutor::AdvanceOneTick(TArray<FHansaCityMarketState>& Markets, const FHansaMarketSettings& Settings,
		const FHansaInventoryLedger& InventoryLedger, const TArray<FHansaProductionState>& Productions,
		const TArray<FHansaPopulationCohortState>& PopulationCohorts,
		const FHansaEconomicRegistry& Registry, const FHansaSimulationTick Tick)
	{
		if (Settings.UpdateCadenceTicks <= 0)
		{
			return;
		}
		for (FHansaCityMarketState& Market : Markets)
		{
			Market.AccumulatedLocalProductionSinceUpdate = FHansaQuantity::FromRaw(SafeAdd(
				Market.AccumulatedLocalProductionSinceUpdate.GetRawValue(),
				SumLocalProduction(Market, Productions, Registry).GetRawValue()));
		}
		if (Tick.GetValue() % Settings.UpdateCadenceTicks != 0)
		{
			return;
		}
		const FHansaInventoryReadOnlyAccess Inventories = InventoryLedger.CreateReadOnlyAccess();
		for (FHansaCityMarketState& Market : Markets)
		{
			const FHansaCompiledGoodDefinition* Good = Registry.FindGood(Market.GoodId.ToString());
			if (Good == nullptr || Good->BaseValueMilliMarks <= 0)
			{
				continue;
			}
			Market.CurrentStock = SumStock(Market, Inventories);
			Market.CitizenDemand = SumCitizenDemand(Market, PopulationCohorts);
			Market.IndustrialDemand = SumIndustrialDemand(Market, Productions, Registry);
			Market.RecentLocalProduction = Market.AccumulatedLocalProductionSinceUpdate;
			Market.AccumulatedLocalProductionSinceUpdate = FHansaQuantity();
			Market.ExpectedIncomingSupply = Market.ConfirmedIncomingSupplyPerUpdate;
			Market.UnmetDemand = FHansaQuantity::FromRaw(SafeAdd(
				SumUnmetCitizenDemand(Market, PopulationCohorts).GetRawValue(),
					SumUnmetIndustrialDemand(Market, Productions, Registry).GetRawValue()));
			Market.MinimumConsumerAffordabilityBasisPoints =
				MinimumConsumerAffordability(Market, PopulationCohorts);
			UpdatePrice(Market, Settings, *Good);
			Market.LastUpdateTick = Tick.GetValue();
			UpdateAlertOnset(Market.UnmetDemand.GetRawValue() > 0, Tick.GetValue(), Market.ShortageSinceTick);
			UpdateAlertOnset(Market.DesiredReserve.GetRawValue() > 0 &&
				Market.CurrentStock.GetRawValue() < Market.DesiredReserve.GetRawValue(),
				Tick.GetValue(), Market.LowReserveSinceTick);
			UpdateAlertOnset(Market.MinimumConsumerAffordabilityBasisPoints <
				FHansaMarketAlertPolicy::AffordabilityWarningBasisPoints,
				Tick.GetValue(), Market.AffordabilitySinceTick);

			FHansaMarketPriceHistoryEntry Entry;
			Entry.Tick = Tick;
			Entry.Stock = Market.CurrentStock;
			Entry.CitizenDemand = Market.CitizenDemand;
			Entry.IndustrialDemand = Market.IndustrialDemand;
			Entry.LocalProduction = Market.RecentLocalProduction;
			Entry.ExpectedIncomingSupply = Market.ExpectedIncomingSupply;
			Entry.UnmetDemand = Market.UnmetDemand;
			Entry.MinimumConsumerAffordabilityBasisPoints = Market.MinimumConsumerAffordabilityBasisPoints;
			Entry.PriceMilliMarks = Market.CurrentPriceMilliMarks;
			Market.PriceHistory.Add(MoveTemp(Entry));
			if (Market.PriceHistory.Num() > Settings.PriceHistoryCapacity)
			{
				Market.PriceHistory.RemoveAt(0, Market.PriceHistory.Num() - Settings.PriceHistoryCapacity);
			}
		}
	}
}
