#include "Queries/HansaSimulationReadOnly.h"

#include "Construction/HansaConstructionInternal.h"
#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaBuildingWorldStatus Status)
	{
		switch (Status)
		{
		case EHansaBuildingWorldStatus::UnderConstruction: return TEXT("UnderConstruction");
		case EHansaBuildingWorldStatus::Ready: return TEXT("Ready");
		case EHansaBuildingWorldStatus::Blocked: return TEXT("Blocked");
		default: return TEXT("UnknownBuildingWorldStatus");
		}
	}

	namespace
	{
		int64 RefundAmount(const int64 Paid, const int32 BasisPoints)
		{
			if (Paid < 0 || BasisPoints < 0 || BasisPoints > 10'000)
			{
				return 0;
			}
			const THansaValueResult<int64> Result = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Paid, BasisPoints, 10'000, EHansaRoundingMode::TowardZero);
			return Result ? FMath::Clamp<int64>(Result.Value, 0, Paid) : 0;
		}

		TOptional<FHansaConstructionProjection> BuildOneConstructionProjection(
			const FHansaBuildingState& Building,
			const FHansaPlacementState& PlacementState,
			const FHansaEconomicRegistry* Registry)
		{
			const FHansaPlacedBuildingRecord* Placement = PlacementState.FindPlacement(Building.Id);
			const FHansaCompiledBuildingDefinition* Definition = Registry != nullptr
				? Registry->FindBuilding(Building.DefinitionId.ToString()) : nullptr;
			if (Placement == nullptr || Definition == nullptr)
			{
				return TOptional<FHansaConstructionProjection>();
			}
			FHansaConstructionProjection Projection;
			Projection.BuildingId = Building.Id;
			Projection.OwnerId = Building.OwnerId;
			Projection.CityId = Placement->Spec.CityId;
			Projection.BuildingDefinitionId = Building.DefinitionId;
			Projection.State = Building.ConstructionState;
			Projection.StartedTick = Building.ConstructionStartedTick;
			Projection.ElapsedTicks = Building.ConstructionElapsedTicks;
			Projection.TotalTicks = Definition->BuildTicks;
			Projection.Progress = Building.ConstructionProgress;
			Projection.PaidCurrency = FHansaMoney::FromRaw(Definition->ConstructionCostPfennig);
			Projection.CancellationCurrencyRefund = FHansaMoney::FromRaw(RefundAmount(
				Definition->ConstructionCostPfennig, Definition->CancellationRefundBasisPoints));
			for (const FHansaCompiledGoodAmount& Cost : Definition->ConstructionCosts)
			{
				const THansaValueResult<FHansaGoodId> GoodId = FHansaGoodId::TryParse(Cost.GoodId);
				if (!GoodId)
				{
					continue;
				}
				FHansaConstructionResourceCostProjection Paid;
				Paid.GoodId = GoodId.Value;
				Paid.Required = FHansaQuantity::FromRaw(Cost.QuantityMilliUnits);
				Paid.Available = Paid.Required;
				Projection.PaidResources.Add(Paid);
				FHansaConstructionResourceCostProjection Refund;
				Refund.GoodId = GoodId.Value;
				Refund.Required = FHansaQuantity::FromRaw(RefundAmount(
					Cost.QuantityMilliUnits, Definition->CancellationRefundBasisPoints));
				Refund.Available = Refund.Required;
				Projection.CancellationResourceRefunds.Add(Refund);
			}
			return TOptional<FHansaConstructionProjection>(MoveTemp(Projection));
		}

		const FHansaBuildingState* FindBuilding(
			const TArray<FHansaBuildingState>& Buildings,
			const FHansaBuildingId BuildingId)
		{
			return Buildings.FindByPredicate([BuildingId](const FHansaBuildingState& Building)
			{
				return Building.Id == BuildingId;
			});
		}

		TOptional<FHansaProductionProjection> BuildOneProductionProjection(
			const FHansaProductionState& Production,
			const TArray<FHansaBuildingState>& Buildings,
			const FHansaEconomicRegistry* Registry)
		{
			FHansaProductionProjection Projection;
			Projection.Id = Production.Id;
			Projection.Kind = Production.Kind;
			Projection.BuildingId = Production.BuildingId;
			Projection.CityId = Production.CityId;
			Projection.RecipeId = Production.RecipeId;
			Projection.InputInventoryId = Production.InputInventoryId;
			Projection.OutputInventoryId = Production.OutputInventoryId;
			Projection.bActive = Production.bActive;
			Projection.ProgressTicks = Production.ProgressTicks;
			Projection.CompletedCycles = Production.CompletedCycles;
			Projection.AllocatedLaborerWorkforce = Production.AllocatedLaborerWorkforce;
			Projection.AllocatedArtisanWorkforce = Production.AllocatedArtisanWorkforce;
			Projection.bUsesCityWorkforce = Production.bUsesCityWorkforce;
			Projection.Blocker = Production.Blocker;
			Projection.BlockingGoodId = Production.BlockingGoodId;
			Projection.BlockingRequiredQuantity = Production.BlockingRequiredQuantity;
			Projection.BlockingAvailableQuantity = Production.BlockingAvailableQuantity;

			if (Production.Kind == EHansaProductionKind::BackgroundSupply)
			{
				Projection.CycleTicks = Production.SupplyCycleTicks;
				FHansaProductionThroughputProjection Output;
				Output.GoodId = Production.SupplyGoodId;
				Output.NominalQuantityPerCycle = Production.SupplyQuantityPerCycle;
				Output.ActualQuantityLastTick = Production.bCompletedCycleLastTick
					? Production.SupplyQuantityPerCycle : FHansaQuantity();
				Projection.Outputs.Add(Output);
				return TOptional<FHansaProductionProjection>(MoveTemp(Projection));
			}

			const FHansaBuildingState* Building = FindBuilding(Buildings, Production.BuildingId);
			const FHansaCompiledRecipeDefinition* Recipe = Registry != nullptr
				? Registry->FindRecipe(Production.RecipeId.ToString()) : nullptr;
			const FHansaCompiledBuildingDefinition* BuildingDefinition =
				Registry != nullptr && Building != nullptr
				? Registry->FindBuilding(Building->DefinitionId.ToString()) : nullptr;
			if (Recipe == nullptr || BuildingDefinition == nullptr)
			{
				return TOptional<FHansaProductionProjection>(MoveTemp(Projection));
			}
			Projection.CycleTicks = Recipe->CycleTicks;
			Projection.RequiredLaborerWorkforce = FMath::Max(
				Recipe->LaborerWorkforce, BuildingDefinition->LaborerWorkforce);
			Projection.RequiredArtisanWorkforce = FMath::Max(
				Recipe->ArtisanWorkforce, BuildingDefinition->ArtisanWorkforce);
			Projection.Outputs.Reserve(Recipe->Outputs.Num());
			for (const FHansaCompiledGoodAmount& RecipeOutput : Recipe->Outputs)
			{
				const THansaValueResult<FHansaGoodId> GoodId = FHansaGoodId::TryParse(RecipeOutput.GoodId);
				if (!GoodId)
				{
					continue;
				}
				FHansaProductionThroughputProjection Output;
				Output.GoodId = GoodId.Value;
				Output.NominalQuantityPerCycle = FHansaQuantity::FromRaw(RecipeOutput.QuantityMilliUnits);
				Output.ActualQuantityLastTick = Production.bCompletedCycleLastTick
					? Output.NominalQuantityPerCycle : FHansaQuantity();
				Projection.Outputs.Add(Output);
			}
			return TOptional<FHansaProductionProjection>(MoveTemp(Projection));
		}

		FHansaPopulationCohortProjection BuildOnePopulationProjection(const FHansaPopulationCohortState& Cohort)
		{
			FHansaPopulationCohortProjection Projection;
			Projection.Id = Cohort.Id;
			Projection.ResidenceBuildingId = Cohort.ResidenceBuildingId;
			Projection.CityId = Cohort.CityId;
			Projection.ConsumptionInventoryId = Cohort.ConsumptionInventoryId;
			Projection.TierId = Cohort.TierId;
			Projection.Residents = Cohort.Residents;
			Projection.ResidenceCapacity = Cohort.ResidenceCapacity;
			Projection.bResidenceOperational = Cohort.bResidenceOperational;
			Projection.bHasMarketAccess = Cohort.bHasMarketAccess;
			Projection.WorkforceSupply = Cohort.WorkforceSupply;
			Projection.AccessBasisPoints = Cohort.AccessBasisPoints;
			Projection.AffordabilityBasisPoints = Cohort.AffordabilityBasisPoints;
			Projection.ReliabilityBasisPoints = Cohort.ReliabilityBasisPoints;
			Projection.SatisfactionBasisPoints = Cohort.SatisfactionBasisPoints;
			Projection.ResidentChangeLastTick = Cohort.ResidentChangeLastTick;
			Projection.Needs = Cohort.Needs;
			return Projection;
		}

		FHansaCityDefinitionId ResolveProductionCity(const FHansaProductionState& Production,
			const FHansaPlacementState& Placement, const FHansaInventoryReadOnlyAccess& Inventories)
		{
			if (Production.CityId.IsValid()) return Production.CityId;
			if (const FHansaPlacedBuildingRecord* Record = Placement.FindPlacement(Production.BuildingId))
			{
				return Record->Spec.CityId;
			}
			const TOptional<FHansaInventoryProjection> Input = Inventories.QueryInventory(Production.InputInventoryId);
			if (Input.IsSet() && Input->OwnerKind == EHansaInventoryOwnerKind::City) return Input->CityId;
			const TOptional<FHansaInventoryProjection> Output = Inventories.QueryInventory(Production.OutputInventoryId);
			return Output.IsSet() && Output->OwnerKind == EHansaInventoryOwnerKind::City
				? Output->CityId : FHansaCityDefinitionId();
		}

		FHansaLogisticsRequestProjection BuildOneLogisticsRequestProjection(
			const FHansaLogisticsRequestState& Request)
		{
			FHansaLogisticsRequestProjection Projection;
			Projection.Id = Request.Id;
			Projection.SourceInventoryId = Request.SourceInventoryId;
			Projection.DestinationInventoryId = Request.DestinationInventoryId;
			Projection.GoodId = Request.GoodId;
			Projection.RequestedQuantity = Request.RequestedQuantity;
			Projection.RemainingQuantity = Request.RemainingQuantity;
			Projection.InFlightQuantity = Request.InFlightQuantity;
			Projection.Priority = Request.Priority;
			Projection.Status = Request.Status;
			Projection.Bottleneck = Request.Bottleneck;
			Projection.CreatedTick = Request.CreatedTick;
			return Projection;
		}

		FHansaLogisticsJobProjection BuildOneLogisticsJobProjection(const FHansaLogisticsJobState& Job)
		{
			FHansaLogisticsJobProjection Projection;
			Projection.Id = Job.Id;
			Projection.RequestId = Job.RequestId;
			Projection.SourceInventoryId = Job.SourceInventoryId;
			Projection.DestinationInventoryId = Job.DestinationInventoryId;
			Projection.GoodId = Job.GoodId;
			Projection.Quantity = Job.Quantity;
			Projection.CargoQuantity = Job.CargoQuantity;
			Projection.DispatchTick = Job.DispatchTick;
			Projection.PickupTick = Job.PickupTick;
			Projection.DeliveryTick = Job.DeliveryTick;
			Projection.RoadDistanceCells = Job.RoadDistanceCells;
			Projection.Status = Job.Status;
			return Projection;
		}

		FHansaCityMarketProjection BuildOneMarketProjection(const FHansaCityMarketState& Market,
			const FHansaMarketSettings& Settings, const FHansaSimulationTick CurrentTick)
		{
			FHansaCityMarketProjection Projection;
			Projection.CityId = Market.CityId;
			Projection.GoodId = Market.GoodId;
			Projection.CurrentStock = Market.CurrentStock;
			Projection.DesiredReserve = Market.DesiredReserve;
			Projection.CitizenDemand = Market.CitizenDemand;
			Projection.IndustrialDemand = Market.IndustrialDemand;
			Projection.RecentLocalProduction = Market.RecentLocalProduction;
			Projection.ExpectedIncomingSupply = Market.ExpectedIncomingSupply;
			Projection.UnmetDemand = Market.UnmetDemand;
			Projection.CurrentPriceMilliMarks = Market.CurrentPriceMilliMarks;
			Projection.Factors = Market.Factors;
			Projection.LastUpdateTick = Market.LastUpdateTick;
			Projection.PriceHistory = Market.PriceHistory;
			int64 PriceTotal = 0;
			for (const FHansaMarketPriceHistoryEntry& Entry : Market.PriceHistory)
			{
				const THansaValueResult<int64> Sum = FHansaCheckedIntegerMath::TryAdd(PriceTotal, Entry.PriceMilliMarks);
				PriceTotal = Sum ? Sum.Value : TNumericLimits<int64>::Max();
			}
			Projection.RecentAveragePriceMilliMarks = Market.PriceHistory.IsEmpty()
				? Market.CurrentPriceMilliMarks : PriceTotal / Market.PriceHistory.Num();
			const int64 Current = CurrentTick.GetValue();
			if (Market.LastUpdateTick < 0)
			{
				Projection.ReportAgeTicks = Current + 1;
				Projection.bIsStale = true;
			}
			else
			{
				Projection.ReportAgeTicks = FMath::Max<int64>(0, Current - Market.LastUpdateTick);
				Projection.bIsStale = Projection.ReportAgeTicks > Settings.StaleAfterTicks;
			}
			const int64 Remainder = Current % Settings.UpdateCadenceTicks;
			Projection.NextUpdateTick = Current + (Remainder == 0 ? Settings.UpdateCadenceTicks
				: Settings.UpdateCadenceTicks - Remainder);
			return Projection;
		}

		const FHansaCityMarketState* FindMarket(const TArray<FHansaCityMarketState>& Markets,
			const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId)
		{
			return Markets.FindByPredicate([CityId, GoodId](const FHansaCityMarketState& Market)
			{
				return Market.CityId == CityId && Market.GoodId == GoodId;
			});
		}

		int64 SafeAdd(const int64 Left, const int64 Right)
		{
			const THansaValueResult<int64> Sum = FHansaCheckedIntegerMath::TryAdd(Left, Right);
			return Sum ? Sum.Value : TNumericLimits<int64>::Max();
		}

		FText MarketFactorLabel(const EHansaMarketExplanationFactor Factor)
		{
			switch (Factor)
			{
			case EHansaMarketExplanationFactor::Scarcity: return NSLOCTEXT("HansaMarket", "FactorScarcity", "Stock versus reserve");
			case EHansaMarketExplanationFactor::CitizenDemand: return NSLOCTEXT("HansaMarket", "FactorCitizenDemand", "Citizen demand");
			case EHansaMarketExplanationFactor::IndustrialDemand: return NSLOCTEXT("HansaMarket", "FactorIndustrialDemand", "Industrial demand");
			case EHansaMarketExplanationFactor::IncomingSupply: return NSLOCTEXT("HansaMarket", "FactorIncomingSupply", "Confirmed incoming supply");
			case EHansaMarketExplanationFactor::UnmetDemand: return NSLOCTEXT("HansaMarket", "FactorUnmetDemand", "Unmet demand");
			case EHansaMarketExplanationFactor::SeasonModifier: return NSLOCTEXT("HansaMarket", "FactorSeason", "Season modifier");
			case EHansaMarketExplanationFactor::CityModifier: return NSLOCTEXT("HansaMarket", "FactorCity", "City modifier");
			case EHansaMarketExplanationFactor::TargetClamp: return NSLOCTEXT("HansaMarket", "FactorTargetClamp", "Price-policy bound");
			default: return NSLOCTEXT("HansaMarket", "FactorUnknown", "Unknown factor");
			}
		}

		FHansaMarketExplanationEntry ExplanationEntry(const EHansaMarketExplanationFactor Factor,
			const int32 Contribution)
		{
			FHansaMarketExplanationEntry Entry;
			Entry.Factor = Factor;
			Entry.MessageKey = FName(*FString::Printf(TEXT("Market.Factor.%s"), LexToString(Factor)));
			Entry.ContributionBasisPoints = Contribution;
			Entry.Message = FText::Format(
				NSLOCTEXT("HansaMarket", "FactorContribution", "{0}: {1} basis points."),
				MarketFactorLabel(Factor), FText::AsNumber(Contribution));
			return Entry;
		}

		FHansaMarketExplanationProjection BuildExplanation(const FHansaCityMarketState& Market)
		{
			FHansaMarketExplanationProjection Result;
			Result.CityId = Market.CityId;
			Result.GoodId = Market.GoodId;
			const TPair<EHansaMarketExplanationFactor, int32> Contributions[] = {
				{ EHansaMarketExplanationFactor::Scarcity, Market.Factors.ScarcityBasisPoints },
				{ EHansaMarketExplanationFactor::CitizenDemand, Market.Factors.CitizenDemandBasisPoints },
				{ EHansaMarketExplanationFactor::IndustrialDemand, Market.Factors.IndustrialDemandBasisPoints },
				{ EHansaMarketExplanationFactor::IncomingSupply, Market.Factors.IncomingSupplyBasisPoints },
				{ EHansaMarketExplanationFactor::UnmetDemand, Market.Factors.UnmetDemandBasisPoints },
				{ EHansaMarketExplanationFactor::SeasonModifier, Market.Factors.SeasonModifierBasisPoints },
				{ EHansaMarketExplanationFactor::CityModifier, Market.Factors.CityModifierBasisPoints }
			};
			for (const auto& Contribution : Contributions)
			{
				Result.Factors.Add(ExplanationEntry(Contribution.Key, Contribution.Value));
				Result.RawMultiplierBasisPoints += Contribution.Value;
			}
			Result.TargetMultiplierBasisPoints = Market.Factors.TargetMultiplierBasisPoints;
			const int32 ClampContribution = Result.TargetMultiplierBasisPoints - Result.RawMultiplierBasisPoints;
			if (ClampContribution != 0)
			{
				Result.Factors.Add(ExplanationEntry(EHansaMarketExplanationFactor::TargetClamp, ClampContribution));
			}
			return Result;
		}

		bool InventoryBelongsToMarket(const FHansaCityMarketState& Market, const FHansaInventoryId InventoryId)
		{
			return Market.InventoryIds.Contains(InventoryId);
		}

		TArray<FHansaMarketConsumerProjection> BuildConsumers(const FHansaCityMarketState& Market,
			const TArray<FHansaPopulationCohortState>& Cohorts, const TArray<FHansaProductionState>& Productions,
			const FHansaEconomicRegistry* Registry)
		{
			TArray<FHansaMarketConsumerProjection> Result;
			for (const FHansaPopulationCohortState& Cohort : Cohorts)
			{
				if (Cohort.CityId != Market.CityId)
				{
					continue;
				}
				for (const FHansaPopulationNeedState& Need : Cohort.Needs)
				{
					if (Need.GoodId != Market.GoodId || Need.RequiredLastTick.GetRawValue() <= 0)
					{
						continue;
					}
					FHansaMarketConsumerProjection Consumer;
					Consumer.Kind = EHansaMarketConsumerKind::Citizen;
					Consumer.CityId = Market.CityId;
					Consumer.GoodId = Market.GoodId;
					Consumer.PopulationCohortId = Cohort.Id;
					Consumer.BuildingId = Cohort.ResidenceBuildingId;
					Consumer.DemandPerTick = Need.RequiredLastTick;
					Consumer.FulfilledLastTick = Need.ConsumedLastTick;
					Consumer.AffordabilityBasisPoints = Need.AffordabilityBasisPoints;
					Consumer.ReserveMilliDays = Need.ReserveMilliDays;
					Result.Add(MoveTemp(Consumer));
				}
			}
			if (Registry != nullptr)
			{
				for (const FHansaProductionState& Production : Productions)
				{
					if (!Production.bActive || Production.Kind != EHansaProductionKind::BuildingRecipe ||
						!InventoryBelongsToMarket(Market, Production.InputInventoryId))
					{
						continue;
					}
					const FHansaCompiledRecipeDefinition* Recipe = Registry->FindRecipe(Production.RecipeId.ToString());
					if (Recipe == nullptr || Recipe->CycleTicks <= 0)
					{
						continue;
					}
					for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
					{
						if (Input.GoodId != Market.GoodId.ToString())
						{
							continue;
						}
						FHansaMarketConsumerProjection Consumer;
						Consumer.Kind = EHansaMarketConsumerKind::Industry;
						Consumer.CityId = Market.CityId;
						Consumer.GoodId = Market.GoodId;
						Consumer.ProductionId = Production.Id;
						Consumer.BuildingId = Production.BuildingId;
						Consumer.RecipeId = Production.RecipeId;
						Consumer.DemandPerTick = FHansaQuantity::FromRaw(FMath::DivideAndRoundUp(
							Input.QuantityMilliUnits, static_cast<int64>(Recipe->CycleTicks)));
						Consumer.FulfilledLastTick = Production.bCompletedCycleLastTick
							? FHansaQuantity::FromRaw(Input.QuantityMilliUnits) : FHansaQuantity();
						Consumer.ProductionBlocker = Production.Blocker;
						Result.Add(MoveTemp(Consumer));
					}
				}
			}
			Result.Sort([](const FHansaMarketConsumerProjection& Left, const FHansaMarketConsumerProjection& Right)
			{
				if (Left.Kind != Right.Kind) return Left.Kind < Right.Kind;
				if (Left.PopulationCohortId != Right.PopulationCohortId) return Left.PopulationCohortId < Right.PopulationCohortId;
				return Left.ProductionId < Right.ProductionId;
			});
			return Result;
		}

		TArray<FHansaMarketProducerProjection> BuildProducers(const FHansaCityMarketState& Market,
			const TArray<FHansaProductionState>& Productions, const FHansaEconomicRegistry* Registry)
		{
			TArray<FHansaMarketProducerProjection> Result;
			for (const FHansaProductionState& Production : Productions)
			{
				if (!InventoryBelongsToMarket(Market, Production.OutputInventoryId))
				{
					continue;
				}
				if (Production.Kind == EHansaProductionKind::BackgroundSupply)
				{
					if (Production.CityId == Market.CityId && Production.SupplyGoodId == Market.GoodId)
					{
						FHansaMarketProducerProjection Producer;
						Producer.Kind = EHansaMarketProducerKind::BackgroundSupply;
						Producer.CityId = Market.CityId;
						Producer.GoodId = Market.GoodId;
						Producer.ProductionId = Production.Id;
						Producer.NominalQuantityPerCycle = Production.SupplyQuantityPerCycle;
						Producer.ActualQuantityLastTick = Production.bCompletedCycleLastTick
							? Production.SupplyQuantityPerCycle : FHansaQuantity();
						Producer.CycleTicks = Production.SupplyCycleTicks;
						Producer.bActive = Production.bActive;
						Producer.Blocker = Production.Blocker;
						Result.Add(MoveTemp(Producer));
					}
					continue;
				}
				const FHansaCompiledRecipeDefinition* Recipe = Registry != nullptr
					? Registry->FindRecipe(Production.RecipeId.ToString()) : nullptr;
				if (Recipe == nullptr)
				{
					continue;
				}
				for (const FHansaCompiledGoodAmount& Output : Recipe->Outputs)
				{
					if (Output.GoodId == Market.GoodId.ToString())
					{
						FHansaMarketProducerProjection Producer;
						Producer.Kind = EHansaMarketProducerKind::BuildingRecipe;
						Producer.CityId = Market.CityId;
						Producer.GoodId = Market.GoodId;
						Producer.ProductionId = Production.Id;
						Producer.BuildingId = Production.BuildingId;
						Producer.RecipeId = Production.RecipeId;
						Producer.NominalQuantityPerCycle = FHansaQuantity::FromRaw(Output.QuantityMilliUnits);
						Producer.ActualQuantityLastTick = Production.bCompletedCycleLastTick
							? Producer.NominalQuantityPerCycle : FHansaQuantity();
						Producer.CycleTicks = Recipe->CycleTicks;
						Producer.bActive = Production.bActive;
						Producer.Blocker = Production.Blocker;
						Result.Add(MoveTemp(Producer));
					}
				}
			}
			Result.Sort([](const FHansaMarketProducerProjection& Left, const FHansaMarketProducerProjection& Right)
			{
				return Left.ProductionId < Right.ProductionId;
			});
			return Result;
		}

		FHansaMarketSuggestedAction SuggestedAction(const EHansaMarketSuggestedActionType Type)
		{
			FHansaMarketSuggestedAction Action;
			Action.Type = Type;
			Action.MessageKey = FName(*FString::Printf(TEXT("Market.Action.%s"), LexToString(Type)));
			switch (Type)
			{
			case EHansaMarketSuggestedActionType::IncreaseLocalProduction:
				Action.Message = NSLOCTEXT("HansaMarket", "ActionIncreaseProduction", "Increase or unblock local production."); break;
			case EHansaMarketSuggestedActionType::ImportGood:
				Action.Message = NSLOCTEXT("HansaMarket", "ActionImport", "Import this good from another city."); break;
			case EHansaMarketSuggestedActionType::InspectBlockedConsumers:
				Action.Message = NSLOCTEXT("HansaMarket", "ActionInspectConsumers", "Inspect affected consumers and blocked industries."); break;
			case EHansaMarketSuggestedActionType::ReplenishReserve:
				Action.Message = NSLOCTEXT("HansaMarket", "ActionReplenishReserve", "Replenish stock toward the desired reserve."); break;
			case EHansaMarketSuggestedActionType::ScheduleIncomingSupply:
				Action.Message = NSLOCTEXT("HansaMarket", "ActionScheduleSupply", "Schedule confirmed incoming supply."); break;
			case EHansaMarketSuggestedActionType::IncreaseAffordableSupply:
				Action.Message = NSLOCTEXT("HansaMarket", "ActionAffordableSupply", "Increase affordable supply to lower consumer pressure."); break;
			default: break;
			}
			return Action;
		}
	}

	FString FHansaDeterminismFingerprint::ToDebugString() const
	{
		return FString::Printf(
			TEXT("StateFingerprint[version=%u;pipeline=%u;value=%016llX]"),
			Version,
			SystemPipelineVersion,
			static_cast<unsigned long long>(Value));
	}

	FHansaSimulationReadOnlyAccess FHansaSimulationState::CreateReadOnlyAccess(
		const FHansaSimulationDefinitionContext& Definitions) const
	{
		check(bInitialized);
		check(Definitions.IsValid());
		return FHansaSimulationReadOnlyAccess(*this, Definitions);
	}

	const FHansaSimulationClock& FHansaSimulationReadOnlyAccess::GetClock() const
	{
		return State->Clock;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetCampaignSeed() const
	{
		return State->CampaignSeed;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetProcessedCommandCount() const
	{
		return State->ProcessedCommandCount;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetLastProcessedCommandSequence() const
	{
		return State->LastProcessedCommandSequence;
	}

	FHansaCommandId FHansaSimulationReadOnlyAccess::GetLastProcessedCommandId() const
	{
		return State->LastProcessedCommandId;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetPublishedDomainEventCount() const
	{
		return State->PublishedDomainEventCount;
	}

	FHansaDeterminismFingerprint FHansaSimulationReadOnlyAccess::GetFingerprint() const
	{
		FHansaDeterminismFingerprint Fingerprint;
		Fingerprint.Value = State->ComputeDeterminismFingerprint(*Definitions);
		return Fingerprint;
	}

	FHansaStateHashReport FHansaSimulationReadOnlyAccess::BuildStateHashReport() const
	{
		return FHansaStateHasher::Compute(*State, *Definitions);
	}

	TConstArrayView<FHansaHouseState> FHansaSimulationReadOnlyAccess::GetHouses() const
	{
		return State->Houses;
	}

	TConstArrayView<FHansaCityState> FHansaSimulationReadOnlyAccess::GetCities() const
	{
		return State->Cities;
	}

	TConstArrayView<FHansaBuildingState> FHansaSimulationReadOnlyAccess::GetBuildings() const
	{
		return State->Buildings;
	}

	TConstArrayView<FHansaVehicleState> FHansaSimulationReadOnlyAccess::GetVehicles() const
	{
		return State->Vehicles;
	}

	TConstArrayView<FHansaRouteState> FHansaSimulationReadOnlyAccess::GetRoutes() const
	{
		return State->Routes;
	}

	TConstArrayView<FHansaTestEntityState> FHansaSimulationReadOnlyAccess::GetTestEntities() const
	{
		return State->TestEntities;
	}

	const FHansaPlacementState& FHansaSimulationReadOnlyAccess::GetPlacement() const
	{
		return State->Placement;
	}

	FHansaPlacementValidationResult FHansaSimulationReadOnlyAccess::ValidatePlacement(
		const FHansaHouseId IssuingHouseId,
		const FHansaPlacementSpec& Spec) const
	{
		const FHansaEconomicRegistry* Registry = Definitions->GetEconomicRegistry();
		if (Registry != nullptr)
		{
			return FHansaPlacementRules::Validate(State->Placement, *Registry, IssuingHouseId, Spec);
		}
		return FHansaPlacementRules::Validate(State->Placement, FHansaEconomicRegistry(), IssuingHouseId, Spec);
	}

	FHansaConstructionCostProjection FHansaSimulationReadOnlyAccess::QueryConstructionCost(
		const FHansaHouseId HouseId,
		const FHansaCityDefinitionId CityId,
		const FHansaBuildingTypeId BuildingDefinitionId) const
	{
		const FHansaEconomicRegistry* Registry = Definitions->GetEconomicRegistry();
		return Registry != nullptr
			? FHansaConstructionExecutor::BuildCostProjection(
				State->Houses, State->InventoryLedger, *Registry, HouseId, CityId, BuildingDefinitionId)
			: FHansaConstructionCostProjection();
	}

	TOptional<FHansaConstructionProjection> FHansaSimulationReadOnlyAccess::QueryConstruction(
		const FHansaBuildingId BuildingId) const
	{
		const FHansaBuildingState* Building = FindBuilding(State->Buildings, BuildingId);
		return Building != nullptr
			? BuildOneConstructionProjection(*Building, State->Placement, Definitions->GetEconomicRegistry())
			: TOptional<FHansaConstructionProjection>();
	}

	TArray<FHansaConstructionProjection> FHansaSimulationReadOnlyAccess::BuildConstructionProjection() const
	{
		TArray<FHansaConstructionProjection> Result;
		Result.Reserve(State->Buildings.Num());
		for (const FHansaBuildingState& Building : State->Buildings)
		{
			const TOptional<FHansaConstructionProjection> Projection = BuildOneConstructionProjection(
				Building, State->Placement, Definitions->GetEconomicRegistry());
			if (Projection.IsSet())
			{
				Result.Add(Projection.GetValue());
			}
		}
		return Result;
	}

	FHansaInventoryReadOnlyAccess FHansaSimulationReadOnlyAccess::GetInventories() const
	{
		return State->InventoryLedger.CreateReadOnlyAccess();
	}

	FHansaLogisticsRoadPathProjection FHansaSimulationReadOnlyAccess::QueryLogisticsRoadPath(
		const FHansaInventoryId SourceInventoryId,
		const FHansaInventoryId DestinationInventoryId) const
	{
		return FHansaLocalLogisticsQueries::QueryRoadPath(
			SourceInventoryId, DestinationInventoryId,
			State->InventoryLedger.CreateReadOnlyAccess(), State->Placement, State->Buildings);
	}

	TOptional<FHansaLogisticsRequestProjection> FHansaSimulationReadOnlyAccess::QueryLogisticsRequest(
		const FHansaLogisticsRequestId RequestId) const
	{
		const FHansaLogisticsRequestState* Request = State->LocalLogisticsRequests.FindByPredicate(
			[RequestId](const FHansaLogisticsRequestState& Value) { return Value.Id == RequestId; });
		return Request != nullptr
			? TOptional<FHansaLogisticsRequestProjection>(BuildOneLogisticsRequestProjection(*Request))
			: TOptional<FHansaLogisticsRequestProjection>();
	}

	TOptional<FHansaLogisticsJobProjection> FHansaSimulationReadOnlyAccess::QueryLogisticsJob(
		const FHansaLogisticsJobId JobId) const
	{
		const FHansaLogisticsJobState* Job = State->LocalLogisticsJobs.FindByPredicate(
			[JobId](const FHansaLogisticsJobState& Value) { return Value.Id == JobId; });
		return Job != nullptr
			? TOptional<FHansaLogisticsJobProjection>(BuildOneLogisticsJobProjection(*Job))
			: TOptional<FHansaLogisticsJobProjection>();
	}

	TArray<FHansaLogisticsRequestProjection> FHansaSimulationReadOnlyAccess::BuildLogisticsRequestProjection() const
	{
		TArray<FHansaLogisticsRequestProjection> Result;
		Result.Reserve(State->LocalLogisticsRequests.Num());
		for (const FHansaLogisticsRequestState& Request : State->LocalLogisticsRequests)
		{
			Result.Add(BuildOneLogisticsRequestProjection(Request));
		}
		return Result;
	}

	TArray<FHansaLogisticsJobProjection> FHansaSimulationReadOnlyAccess::BuildLogisticsJobProjection() const
	{
		TArray<FHansaLogisticsJobProjection> Result;
		Result.Reserve(State->LocalLogisticsJobs.Num());
		for (const FHansaLogisticsJobState& Job : State->LocalLogisticsJobs)
		{
			Result.Add(BuildOneLogisticsJobProjection(Job));
		}
		return Result;
	}

	TOptional<FHansaProductionProjection> FHansaSimulationReadOnlyAccess::QueryProduction(
		const FHansaProductionId ProductionId) const
	{
		if (!ProductionId.IsValid())
		{
			return TOptional<FHansaProductionProjection>();
		}
		const FHansaProductionState* Production = State->Productions.FindByPredicate(
			[ProductionId](const FHansaProductionState& Value) { return Value.Id == ProductionId; });
		return Production != nullptr
			? BuildOneProductionProjection(*Production, State->Buildings, Definitions->GetEconomicRegistry())
			: TOptional<FHansaProductionProjection>();
	}

	TArray<FHansaProductionProjection> FHansaSimulationReadOnlyAccess::BuildProductionProjection() const
	{
		TArray<FHansaProductionProjection> Result;
		Result.Reserve(State->Productions.Num());
		for (const FHansaProductionState& Production : State->Productions)
		{
			const TOptional<FHansaProductionProjection> Projection =
				BuildOneProductionProjection(Production, State->Buildings, Definitions->GetEconomicRegistry());
			if (Projection.IsSet())
			{
				Result.Add(Projection.GetValue());
			}
		}
		return Result;
	}

	TOptional<FHansaPopulationCohortProjection> FHansaSimulationReadOnlyAccess::QueryPopulationCohort(
		const FHansaPopulationCohortId CohortId) const
	{
		const FHansaPopulationCohortState* Cohort = State->PopulationCohorts.FindByPredicate(
			[CohortId](const FHansaPopulationCohortState& Value) { return Value.Id == CohortId; });
		return Cohort != nullptr ? TOptional<FHansaPopulationCohortProjection>(BuildOnePopulationProjection(*Cohort))
			: TOptional<FHansaPopulationCohortProjection>();
	}

	TArray<FHansaPopulationCohortProjection> FHansaSimulationReadOnlyAccess::BuildPopulationProjection() const
	{
		TArray<FHansaPopulationCohortProjection> Result;
		Result.Reserve(State->PopulationCohorts.Num());
		for (const FHansaPopulationCohortState& Cohort : State->PopulationCohorts)
		{
			Result.Add(BuildOnePopulationProjection(Cohort));
		}
		return Result;
	}

	TOptional<FHansaCityPopulationProjection> FHansaSimulationReadOnlyAccess::QueryCityPopulation(
		const FHansaCityDefinitionId CityId) const
	{
		if (!State->Cities.ContainsByPredicate([CityId](const FHansaCityState& City)
			{ return City.DefinitionId == CityId; }))
		{
			return TOptional<FHansaCityPopulationProjection>();
		}
		FHansaCityPopulationProjection Result;
		Result.CityId = CityId;
		int64 SatisfactionTotal = 0;
		int64 SatisfactionWeight = 0;
		bool bHasOperationalResidence = false;
		bool bAllOperationalResidencesHaveMarketAccess = true;
		const FHansaEconomicRegistry* Registry = Definitions->GetEconomicRegistry();
		for (const FHansaPopulationCohortState& Cohort : State->PopulationCohorts)
		{
			if (Cohort.CityId != CityId) continue;
			Result.TotalResidents += Cohort.Residents;
			Result.ResidentChangeLastTick += Cohort.ResidentChangeLastTick;
			if (Cohort.bResidenceOperational) Result.HousingCapacity += Cohort.ResidenceCapacity;
			const FHansaCompiledPopulationTierDefinition* Tier = Registry != nullptr
				? Registry->FindPopulationTier(Cohort.TierId.ToString()) : nullptr;
			if (Tier != nullptr && !Tier->PreviousTierId.IsEmpty())
			{
				Result.ArtisanResidents += Cohort.Residents;
				Result.ArtisanWorkforceSupply += Cohort.WorkforceSupply;
			}
			else
			{
				Result.LaborerResidents += Cohort.Residents;
				Result.LaborerWorkforceSupply += Cohort.WorkforceSupply;
			}
			SatisfactionTotal += static_cast<int64>(Cohort.SatisfactionBasisPoints) * Cohort.Residents;
			SatisfactionWeight += Cohort.Residents;
			if (Cohort.bResidenceOperational)
			{
				bHasOperationalResidence = true;
				bAllOperationalResidencesHaveMarketAccess &= Cohort.bHasMarketAccess;
			}
		}
		Result.SatisfactionBasisPoints = SatisfactionWeight > 0
			? static_cast<int32>(SatisfactionTotal / SatisfactionWeight) : 0;
		Result.Trend = Result.ResidentChangeLastTick < 0
			? EHansaPopulationTrend::Declining
			: (Result.ResidentChangeLastTick > 0 ? EHansaPopulationTrend::Growing : EHansaPopulationTrend::Stable);
		Result.bHasMarketAccess = bHasOperationalResidence && bAllOperationalResidencesHaveMarketAccess;
		const FHansaInventoryReadOnlyAccess Inventories = State->InventoryLedger.CreateReadOnlyAccess();
		for (const FHansaProductionState& Production : State->Productions)
		{
			if (Production.Kind != EHansaProductionKind::BuildingRecipe ||
				ResolveProductionCity(Production, State->Placement, Inventories) != CityId) continue;
			Result.LaborerWorkforceAssigned += Production.AllocatedLaborerWorkforce;
			Result.ArtisanWorkforceAssigned += Production.AllocatedArtisanWorkforce;
		}
		Result.LaborerWorkforceAvailable = FMath::Max(0,
			Result.LaborerWorkforceSupply - Result.LaborerWorkforceAssigned);
		Result.ArtisanWorkforceAvailable = FMath::Max(0,
			Result.ArtisanWorkforceSupply - Result.ArtisanWorkforceAssigned);
		const auto Bread = FHansaGoodId::TryParse(TEXT("Good.Bread"));
		if (Bread)
		{
			const TOptional<FHansaMarketReserveProjection> Reserve = QueryMarketReserveDays(CityId, Bread.Value);
			if (Reserve.IsSet()) Result.StapleReserveMilliDays = Reserve->ReserveMilliDays;
		}
		return TOptional<FHansaCityPopulationProjection>(MoveTemp(Result));
	}

	TArray<FHansaCityPopulationProjection> FHansaSimulationReadOnlyAccess::BuildCityPopulationProjection() const
	{
		TArray<FHansaCityPopulationProjection> Result;
		Result.Reserve(State->Cities.Num());
		for (const FHansaCityState& City : State->Cities)
		{
			const TOptional<FHansaCityPopulationProjection> Projection = QueryCityPopulation(City.DefinitionId);
			if (Projection.IsSet()) Result.Add(Projection.GetValue());
		}
		return Result;
	}

	TOptional<FHansaCityMarketProjection> FHansaSimulationReadOnlyAccess::QueryMarket(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const FHansaCityMarketState* Market = State->Markets.FindByPredicate([CityId, GoodId](const FHansaCityMarketState& Value)
		{
			return Value.CityId == CityId && Value.GoodId == GoodId;
		});
		return Market != nullptr ? TOptional<FHansaCityMarketProjection>(
			BuildOneMarketProjection(*Market, State->MarketSettings, State->Clock.GetTick()))
			: TOptional<FHansaCityMarketProjection>();
	}

	TArray<FHansaCityMarketProjection> FHansaSimulationReadOnlyAccess::BuildMarketProjection() const
	{
		TArray<FHansaCityMarketProjection> Result;
		Result.Reserve(State->Markets.Num());
		for (const FHansaCityMarketState& Market : State->Markets)
		{
			Result.Add(BuildOneMarketProjection(Market, State->MarketSettings, State->Clock.GetTick()));
		}
		return Result;
	}

	TOptional<FHansaMarketPriceProjection> FHansaSimulationReadOnlyAccess::QueryMarketPrice(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const TOptional<FHansaCityMarketProjection> Market = QueryMarket(CityId, GoodId);
		if (!Market.IsSet())
		{
			return TOptional<FHansaMarketPriceProjection>();
		}
		FHansaMarketPriceProjection Result;
		Result.CityId = CityId;
		Result.GoodId = GoodId;
		Result.CurrentPriceMilliMarks = Market->CurrentPriceMilliMarks;
		Result.RecentAveragePriceMilliMarks = Market->RecentAveragePriceMilliMarks;
		Result.LastUpdateTick = Market->LastUpdateTick;
		Result.NextUpdateTick = Market->NextUpdateTick;
		Result.ReportAgeTicks = Market->ReportAgeTicks;
		Result.bIsStale = Market->bIsStale;
		Result.History = Market->PriceHistory;
		return TOptional<FHansaMarketPriceProjection>(MoveTemp(Result));
	}

	TArray<FHansaMarketPriceHistoryEntry> FHansaSimulationReadOnlyAccess::QueryMarketPriceHistory(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const TOptional<FHansaMarketPriceProjection> Price = QueryMarketPrice(CityId, GoodId);
		return Price.IsSet() ? Price->History : TArray<FHansaMarketPriceHistoryEntry>();
	}

	TOptional<FHansaMarketSupplyDemandProjection> FHansaSimulationReadOnlyAccess::QueryMarketSupplyDemand(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const FHansaCityMarketState* Market = FindMarket(State->Markets, CityId, GoodId);
		if (Market == nullptr)
		{
			return TOptional<FHansaMarketSupplyDemandProjection>();
		}
		FHansaMarketSupplyDemandProjection Result;
		Result.CityId = CityId;
		Result.GoodId = GoodId;
		Result.Stock = Market->CurrentStock;
		Result.DesiredReserve = Market->DesiredReserve;
		Result.CitizenDemand = Market->CitizenDemand;
		Result.IndustrialDemand = Market->IndustrialDemand;
		Result.TotalDemand = FHansaQuantity::FromRaw(SafeAdd(
			Market->CitizenDemand.GetRawValue(), Market->IndustrialDemand.GetRawValue()));
		Result.RecentLocalProduction = Market->RecentLocalProduction;
		Result.ExpectedIncomingSupply = Market->ExpectedIncomingSupply;
		Result.UnmetDemand = Market->UnmetDemand;
		return TOptional<FHansaMarketSupplyDemandProjection>(MoveTemp(Result));
	}

	TOptional<FHansaMarketReserveProjection> FHansaSimulationReadOnlyAccess::QueryMarketReserveDays(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const TOptional<FHansaMarketSupplyDemandProjection> Components = QueryMarketSupplyDemand(CityId, GoodId);
		if (!Components.IsSet())
		{
			return TOptional<FHansaMarketReserveProjection>();
		}
		FHansaMarketReserveProjection Result;
		Result.CityId = CityId;
		Result.GoodId = GoodId;
		Result.Stock = Components->Stock;
		Result.DemandPerTick = Components->TotalDemand;
		Result.bHasDemand = Result.DemandPerTick.GetRawValue() > 0;
		if (Result.bHasDemand)
		{
			const THansaValueResult<int64> Reserve = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Result.Stock.GetRawValue(), static_cast<int64>(State->Clock.GetMinutesPerTick()) * 1000,
				Result.DemandPerTick.GetRawValue() * 1440, EHansaRoundingMode::TowardZero);
			Result.ReserveMilliDays = Reserve ? FMath::Max<int64>(0, Reserve.Value) : 0;
		}
		return TOptional<FHansaMarketReserveProjection>(MoveTemp(Result));
	}

	TOptional<FHansaMarketExplanationProjection> FHansaSimulationReadOnlyAccess::QueryMarketExplanation(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const FHansaCityMarketState* Market = FindMarket(State->Markets, CityId, GoodId);
		return Market != nullptr ? TOptional<FHansaMarketExplanationProjection>(BuildExplanation(*Market))
			: TOptional<FHansaMarketExplanationProjection>();
	}

	TArray<FHansaMarketConsumerProjection> FHansaSimulationReadOnlyAccess::QueryMarketConsumers(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const FHansaCityMarketState* Market = FindMarket(State->Markets, CityId, GoodId);
		return Market != nullptr ? BuildConsumers(*Market, State->PopulationCohorts, State->Productions,
			Definitions->GetEconomicRegistry()) : TArray<FHansaMarketConsumerProjection>();
	}

	TArray<FHansaMarketProducerProjection> FHansaSimulationReadOnlyAccess::QueryMarketProducers(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		const FHansaCityMarketState* Market = FindMarket(State->Markets, CityId, GoodId);
		return Market != nullptr ? BuildProducers(*Market, State->Productions, Definitions->GetEconomicRegistry())
			: TArray<FHansaMarketProducerProjection>();
	}

	TArray<FHansaMarketAlertProjection> FHansaSimulationReadOnlyAccess::QueryMarketAlerts(
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId) const
	{
		TArray<FHansaMarketAlertProjection> Result;
		const FHansaCityMarketState* Market = FindMarket(State->Markets, CityId, GoodId);
		if (Market == nullptr)
		{
			return Result;
		}
		const TArray<FHansaMarketConsumerProjection> Consumers = QueryMarketConsumers(CityId, GoodId);
		const int64 CurrentTick = State->Clock.GetTick().GetValue();
		if (Market->ShortageSinceTick >= 0)
		{
			FHansaMarketAlertProjection Alert;
			Alert.Type = EHansaMarketAlertType::Shortage;
			Alert.Severity = Market->CurrentStock.GetRawValue() == 0
				? EHansaMarketAlertSeverity::Critical : EHansaMarketAlertSeverity::Warning;
			Alert.CityId = CityId;
			Alert.GoodId = GoodId;
			Alert.CauseMessageKey = TEXT("Market.Alert.Shortage.Cause");
			Alert.Cause = FText::Format(NSLOCTEXT("HansaMarket", "ShortageCause",
				"{0} has {1} milli-units of unmet demand in {2}."),
				FText::FromString(GoodId.ToString()), FText::AsNumber(Market->UnmetDemand.GetRawValue()),
				FText::FromString(CityId.ToString()));
			Alert.ActiveSinceTick = Market->ShortageSinceTick;
			Alert.AgeTicks = FMath::Max<int64>(0, CurrentTick - Alert.ActiveSinceTick);
			for (const FHansaMarketConsumerProjection& Consumer : Consumers)
			{
				if (Consumer.PopulationCohortId.IsValid()) Alert.PopulationCohortIds.AddUnique(Consumer.PopulationCohortId);
				if (Consumer.ProductionId.IsValid()) Alert.ProductionIds.AddUnique(Consumer.ProductionId);
			}
			Alert.SuggestedActions = { SuggestedAction(EHansaMarketSuggestedActionType::IncreaseLocalProduction),
				SuggestedAction(EHansaMarketSuggestedActionType::ImportGood),
				SuggestedAction(EHansaMarketSuggestedActionType::InspectBlockedConsumers) };
			Result.Add(MoveTemp(Alert));
		}
		if (Market->LowReserveSinceTick >= 0)
		{
			FHansaMarketAlertProjection Alert;
			Alert.Type = EHansaMarketAlertType::LowReserve;
			int64 ReserveRatio = 0;
			if (Market->DesiredReserve.GetRawValue() > 0)
			{
				const THansaValueResult<int64> Ratio = FHansaCheckedIntegerMath::TryMultiplyDivide(
					Market->CurrentStock.GetRawValue(), 10000, Market->DesiredReserve.GetRawValue(),
					EHansaRoundingMode::TowardZero);
				ReserveRatio = Ratio ? Ratio.Value : 0;
			}
			Alert.Severity = ReserveRatio <= FHansaMarketAlertPolicy::CriticalReserveRatioBasisPoints
				? EHansaMarketAlertSeverity::Critical : EHansaMarketAlertSeverity::Warning;
			Alert.CityId = CityId;
			Alert.GoodId = GoodId;
			Alert.CauseMessageKey = TEXT("Market.Alert.LowReserve.Cause");
			Alert.Cause = FText::Format(NSLOCTEXT("HansaMarket", "LowReserveCause",
				"{0} stock is {1} against a desired reserve of {2} in {3}."),
				FText::FromString(GoodId.ToString()), FText::AsNumber(Market->CurrentStock.GetRawValue()),
				FText::AsNumber(Market->DesiredReserve.GetRawValue()), FText::FromString(CityId.ToString()));
			Alert.ActiveSinceTick = Market->LowReserveSinceTick;
			Alert.AgeTicks = FMath::Max<int64>(0, CurrentTick - Alert.ActiveSinceTick);
			Alert.SuggestedActions = { SuggestedAction(EHansaMarketSuggestedActionType::ReplenishReserve),
				SuggestedAction(EHansaMarketSuggestedActionType::ScheduleIncomingSupply) };
			Result.Add(MoveTemp(Alert));
		}
		if (Market->AffordabilitySinceTick >= 0)
		{
			FHansaMarketAlertProjection Alert;
			Alert.Type = EHansaMarketAlertType::Affordability;
			Alert.Severity = Market->MinimumConsumerAffordabilityBasisPoints <
				FHansaMarketAlertPolicy::CriticalAffordabilityBasisPoints
				? EHansaMarketAlertSeverity::Critical : EHansaMarketAlertSeverity::Warning;
			Alert.CityId = CityId;
			Alert.GoodId = GoodId;
			Alert.CauseMessageKey = TEXT("Market.Alert.Affordability.Cause");
			Alert.Cause = FText::Format(NSLOCTEXT("HansaMarket", "AffordabilityCause",
				"{0} affordability has fallen to {1} basis points in {2}."),
				FText::FromString(GoodId.ToString()),
				FText::AsNumber(Market->MinimumConsumerAffordabilityBasisPoints),
				FText::FromString(CityId.ToString()));
			Alert.ActiveSinceTick = Market->AffordabilitySinceTick;
			Alert.AgeTicks = FMath::Max<int64>(0, CurrentTick - Alert.ActiveSinceTick);
			for (const FHansaMarketConsumerProjection& Consumer : Consumers)
			{
				if (Consumer.Kind == EHansaMarketConsumerKind::Citizen &&
					Consumer.AffordabilityBasisPoints < FHansaMarketAlertPolicy::AffordabilityWarningBasisPoints)
				{
					Alert.PopulationCohortIds.AddUnique(Consumer.PopulationCohortId);
				}
			}
			Alert.SuggestedActions = { SuggestedAction(EHansaMarketSuggestedActionType::IncreaseAffordableSupply) };
			Result.Add(MoveTemp(Alert));
		}
		Result.Sort([](const FHansaMarketAlertProjection& Left, const FHansaMarketAlertProjection& Right)
		{
			if (Left.Severity != Right.Severity) return Left.Severity > Right.Severity;
			return Left.Type < Right.Type;
		});
		return Result;
	}

	TArray<FHansaMarketAlertProjection> FHansaSimulationReadOnlyAccess::BuildActiveMarketAlerts() const
	{
		TArray<FHansaMarketAlertProjection> Result;
		for (const FHansaCityMarketState& Market : State->Markets)
		{
			Result.Append(QueryMarketAlerts(Market.CityId, Market.GoodId));
		}
		Result.Sort([](const FHansaMarketAlertProjection& Left, const FHansaMarketAlertProjection& Right)
		{
			if (Left.Severity != Right.Severity) return Left.Severity > Right.Severity;
			if (Left.CityId != Right.CityId) return Left.CityId < Right.CityId;
			if (Left.GoodId != Right.GoodId) return Left.GoodId < Right.GoodId;
			return Left.Type < Right.Type;
		});
		return Result;
	}

	FHansaSimulationSnapshot FHansaSimulationReadOnlyAccess::CaptureSnapshot() const
	{
		FHansaSimulationSnapshot Snapshot;
		Snapshot.Clock = State->Clock;
		Snapshot.CampaignSeed = State->CampaignSeed;
		Snapshot.ProcessedCommandCount = State->ProcessedCommandCount;
		Snapshot.LastProcessedCommandSequence = State->LastProcessedCommandSequence;
		Snapshot.LastProcessedCommandId = State->LastProcessedCommandId;
		Snapshot.CommandHistoryFingerprint = State->CommandHistoryFingerprint;
		Snapshot.PublishedDomainEventCount = State->PublishedDomainEventCount;
		Snapshot.Fingerprint = GetFingerprint();
		Snapshot.RandomStreams = State->RandomStreams;
		Snapshot.Houses = State->Houses;
		Snapshot.Cities = State->Cities;
		Snapshot.Buildings = State->Buildings;
		Snapshot.Vehicles = State->Vehicles;
		Snapshot.Routes = State->Routes;
		Snapshot.TestEntities = State->TestEntities;
		Snapshot.Inventories = State->InventoryLedger.CreateReadOnlyAccess().CaptureSnapshot();
		Snapshot.Productions.NextReservationValue = State->NextProductionReservationValue;
		Snapshot.Productions.Productions = State->Productions;
		Snapshot.Population.Cohorts = State->PopulationCohorts;
		Snapshot.Market.Settings = State->MarketSettings;
		Snapshot.Market.Markets = State->Markets;
		Snapshot.Placement = State->Placement;
		Snapshot.LocalLogistics.Settings = State->LocalLogisticsSettings;
		Snapshot.LocalLogistics.Requests = State->LocalLogisticsRequests;
		Snapshot.LocalLogistics.Jobs = State->LocalLogisticsJobs;
		return Snapshot;
	}

	THansaValueResult<FHansaSimulationProjection> FHansaSimulationReadOnlyAccess::BuildProjection() const
	{
		const THansaValueResult<FHansaCalendarProjection> Calendar = State->Clock.TryProjectCalendar();
		if (!Calendar)
		{
			return THansaValueResult<FHansaSimulationProjection>::Failure(Calendar.Error);
		}

		FHansaSimulationProjection Projection;
		Projection.Clock = State->Clock;
		Projection.Calendar = Calendar.Value;
		Projection.Fingerprint = GetFingerprint();
		Projection.ProcessedCommandCount = State->ProcessedCommandCount;
		Projection.PublishedDomainEventCount = State->PublishedDomainEventCount;
		Projection.CityCount = State->Cities.Num();
		Projection.BuildingCount = State->Buildings.Num();
		Projection.VehicleCount = State->Vehicles.Num();
		Projection.RouteCount = State->Routes.Num();
		Projection.TestEntityCount = State->TestEntities.Num();
		Projection.Houses.Reserve(State->Houses.Num());
		for (const FHansaHouseState& House : State->Houses)
		{
			FHansaHouseProjection HouseProjection;
			HouseProjection.Id = House.Id;
			HouseProjection.Money = House.Money;
			Projection.Houses.Add(HouseProjection);
		}
		Projection.Inventories = State->InventoryLedger.CreateReadOnlyAccess().BuildProjection();
		Projection.Productions = BuildProductionProjection();
		Projection.PopulationCohorts = BuildPopulationProjection();
		Projection.CityPopulations = BuildCityPopulationProjection();
		for (const FHansaPopulationCohortProjection& Cohort : Projection.PopulationCohorts)
		{
			Projection.TotalResidents += Cohort.Residents;
			Projection.TotalWorkforceSupply += Cohort.WorkforceSupply;
		}
		Projection.Markets = BuildMarketProjection();
		Projection.ActiveMarketAlerts = BuildActiveMarketAlerts();
		Projection.Placements.Append(State->Placement.GetPlacements());
		Projection.Constructions = BuildConstructionProjection();
		Projection.LogisticsRequests = BuildLogisticsRequestProjection();
		Projection.LogisticsJobs = BuildLogisticsJobProjection();
		Projection.BuildingWorldProjections.Reserve(Projection.Placements.Num());
		const FHansaEconomicRegistry* Registry = Definitions->GetEconomicRegistry();
		for (const FHansaPlacedBuildingRecord& Placement : Projection.Placements)
		{
			const FHansaBuildingState* Building = State->Buildings.FindByPredicate(
				[&Placement](const FHansaBuildingState& Value) { return Value.Id == Placement.BuildingId; });
			if (Building == nullptr)
			{
				continue;
			}

			FHansaBuildingWorldProjection WorldProjection;
			WorldProjection.BuildingId = Placement.BuildingId;
			WorldProjection.OwnerId = Placement.OwnerId;
			WorldProjection.Placement = Placement.Spec;
			WorldProjection.OccupiedCells = Placement.OccupiedCells;
			WorldProjection.ConstructionProgress = Building->ConstructionProgress;
			if (Registry != nullptr)
			{
				if (const FHansaCompiledBuildingDefinition* Definition =
					Registry->FindBuilding(Placement.Spec.BuildingDefinitionId.ToString()))
				{
					WorldProjection.FootprintWidthCells = Definition->FootprintWidthCells;
					WorldProjection.FootprintHeightCells = Definition->FootprintHeightCells;
				}
			}

			if (Building->ConstructionState == EHansaConstructionState::Completed)
			{
				WorldProjection.Status = EHansaBuildingWorldStatus::Ready;
				for (const FHansaProductionState& Production : State->Productions)
				{
					if (Production.BuildingId == Building->Id && Production.Blocker != EHansaProductionBlocker::None)
					{
						WorldProjection.Status = EHansaBuildingWorldStatus::Blocked;
						WorldProjection.ProductionBlocker = Production.Blocker;
						break;
					}
				}
			}
			Projection.BuildingWorldProjections.Add(MoveTemp(WorldProjection));
		}
		return THansaValueResult<FHansaSimulationProjection>::Success(Projection);
	}
}
