#include "Construction/HansaConstructionInternal.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Math/NumericLimits.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	namespace
	{
		const FHansaHouseState* FindHouse(const TArray<FHansaHouseState>& Houses, const FHansaHouseId HouseId)
		{
			return Houses.FindByPredicate([HouseId](const FHansaHouseState& House) { return House.Id == HouseId; });
		}

		FHansaHouseState* FindHouse(TArray<FHansaHouseState>& Houses, const FHansaHouseId HouseId)
		{
			return Houses.FindByPredicate([HouseId](const FHansaHouseState& House) { return House.Id == HouseId; });
		}

		TOptional<FHansaInventoryProjection> FindCityInventory(
			const FHansaInventoryLedger& Ledger,
			const FHansaCityDefinitionId CityId)
		{
			for (const FHansaInventoryProjection& Inventory : Ledger.CreateReadOnlyAccess().BuildProjection())
			{
				if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City && Inventory.CityId == CityId)
				{
					return Inventory;
				}
			}
			return TOptional<FHansaInventoryProjection>();
		}

		int64 SafeMissing(const int64 Required, const int64 Available)
		{
			return Required > Available ? Required - Available : 0;
		}

		bool TryScaledRefund(const int64 Paid, const int32 BasisPoints, int64& OutRefund)
		{
			if (Paid < 0 || BasisPoints < 0 || BasisPoints > 10'000)
			{
				return false;
			}
			const THansaValueResult<int64> Refund = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Paid, BasisPoints, 10'000, EHansaRoundingMode::TowardZero);
			if (!Refund || Refund.Value < 0 || Refund.Value > Paid)
			{
				return false;
			}
			OutRefund = Refund.Value;
			return true;
		}
	}

	FHansaConstructionCostProjection FHansaConstructionExecutor::BuildCostProjection(
		const TArray<FHansaHouseState>& Houses,
		const FHansaInventoryLedger& Inventories,
		const FHansaEconomicRegistry& Definitions,
		const FHansaHouseId HouseId,
		const FHansaCityDefinitionId CityId,
		const FHansaBuildingTypeId BuildingDefinitionId)
	{
		FHansaConstructionCostProjection Result;
		Result.HouseId = HouseId;
		Result.CityId = CityId;
		Result.BuildingDefinitionId = BuildingDefinitionId;
		const FHansaCompiledBuildingDefinition* Definition = Definitions.FindBuilding(BuildingDefinitionId.ToString());
		const FHansaHouseState* House = FindHouse(Houses, HouseId);
		if (Definition == nullptr || House == nullptr || Definition->ConstructionCostPfennig < 0 ||
			Definition->CancellationRefundBasisPoints < 0 || Definition->CancellationRefundBasisPoints > 10'000 ||
			Definition->ConstructionCosts.ContainsByPredicate([](const FHansaCompiledGoodAmount& Cost)
				{ return Cost.QuantityMilliUnits < 0; }))
		{
			Result.MissingCurrency = FHansaMoney::FromRaw(1);
			return Result;
		}

		Result.RequiredCurrency = FHansaMoney::FromRaw(Definition->ConstructionCostPfennig);
		Result.AvailableCurrency = FHansaMoney::FromRaw(FMath::Max<int64>(0, House->Money.GetRawValue()));
		Result.MissingCurrency = FHansaMoney::FromRaw(SafeMissing(
			Result.RequiredCurrency.GetRawValue(), Result.AvailableCurrency.GetRawValue()));

		const TOptional<FHansaInventoryProjection> CityInventory = FindCityInventory(Inventories, CityId);
		for (const FHansaCompiledGoodAmount& Cost : Definition->ConstructionCosts)
		{
			const THansaValueResult<FHansaGoodId> GoodId = FHansaGoodId::TryParse(Cost.GoodId);
			FHansaConstructionResourceCostProjection Resource;
			if (GoodId)
			{
				Resource.GoodId = GoodId.Value;
			}
			Resource.Required = FHansaQuantity::FromRaw(Cost.QuantityMilliUnits);
			if (CityInventory.IsSet() && Resource.GoodId.IsValid())
			{
				const FHansaInventoryStockProjection* Stock = CityInventory->Stocks.FindByPredicate(
					[&Resource](const FHansaInventoryStockProjection& Value) { return Value.GoodId == Resource.GoodId; });
				if (Stock != nullptr)
				{
					Resource.Available = Stock->Available;
				}
			}
			Resource.Missing = FHansaQuantity::FromRaw(SafeMissing(
				Resource.Required.GetRawValue(), Resource.Available.GetRawValue()));
			Result.Resources.Add(Resource);
		}
		return Result;
	}

	bool FHansaConstructionExecutor::TryPayCost(
		TArray<FHansaHouseState>& Houses,
		FHansaInventoryLedger& Inventories,
		const FHansaEconomicRegistry& Definitions,
		const FHansaHouseId HouseId,
		const FHansaCityDefinitionId CityId,
		const FHansaBuildingTypeId BuildingDefinitionId,
		const FHansaSimulationTick Tick)
	{
		const FHansaConstructionCostProjection Projection = BuildCostProjection(
			Houses, Inventories, Definitions, HouseId, CityId, BuildingDefinitionId);
		if (!Projection.IsAffordable())
		{
			return false;
		}
		const FHansaCompiledBuildingDefinition* Definition = Definitions.FindBuilding(BuildingDefinitionId.ToString());
		FHansaHouseState* House = FindHouse(Houses, HouseId);
		const TOptional<FHansaInventoryProjection> CityInventory = FindCityInventory(Inventories, CityId);
		if (Definition == nullptr || House == nullptr || Definition->ConstructionCostPfennig < 0 ||
			Definition->CancellationRefundBasisPoints < 0 || Definition->CancellationRefundBasisPoints > 10'000 ||
			(!Definition->ConstructionCosts.IsEmpty() && !CityInventory.IsSet()))
		{
			return false;
		}
		const THansaValueResult<FHansaMoney> MoneyAfter = FHansaMoney::TrySubtract(
			House->Money, FHansaMoney::FromRaw(Definition->ConstructionCostPfennig));
		if (!MoneyAfter || MoneyAfter.Value.GetRawValue() < 0)
		{
			return false;
		}
		House->Money = MoneyAfter.Value;
		for (const FHansaConstructionResourceCostProjection& Resource : Projection.Resources)
		{
			const FHansaInventoryTransactionResult Transfer = Inventories.TryTransfer(
				FHansaInventoryEndpoint::Inventory(CityInventory->Id),
				FHansaInventoryEndpoint::Sink(TEXT("Construction.Cost")),
				Resource.GoodId,
				Resource.Required,
				Tick,
				Inventories.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
			if (!Transfer.IsSuccess())
			{
				return false;
			}
		}
		return true;
	}

	bool FHansaConstructionExecutor::TryRefundCancellation(
		TArray<FHansaHouseState>& Houses,
		FHansaInventoryLedger& Inventories,
		const FHansaEconomicRegistry& Definitions,
		const FHansaBuildingState& Building,
		const FHansaCityDefinitionId CityId,
		const FHansaSimulationTick Tick,
		FHansaMoney& OutCurrencyRefund)
	{
		const FHansaCompiledBuildingDefinition* Definition = Definitions.FindBuilding(Building.DefinitionId.ToString());
		FHansaHouseState* House = FindHouse(Houses, Building.OwnerId);
		if (Definition == nullptr || House == nullptr || Definition->CancellationRefundBasisPoints < 0 ||
			Definition->CancellationRefundBasisPoints > 10'000)
		{
			return false;
		}
		int64 CurrencyRefund = 0;
		if (!TryScaledRefund(Definition->ConstructionCostPfennig,
			Definition->CancellationRefundBasisPoints, CurrencyRefund))
		{
			return false;
		}
		const THansaValueResult<FHansaMoney> MoneyAfter = FHansaMoney::TryAdd(
			House->Money, FHansaMoney::FromRaw(CurrencyRefund));
		if (!MoneyAfter)
		{
			return false;
		}

		const TOptional<FHansaInventoryProjection> CityInventory = FindCityInventory(Inventories, CityId);
		for (const FHansaCompiledGoodAmount& Cost : Definition->ConstructionCosts)
		{
			int64 Refund = 0;
			const THansaValueResult<FHansaGoodId> GoodId = FHansaGoodId::TryParse(Cost.GoodId);
			if (!GoodId || !TryScaledRefund(Cost.QuantityMilliUnits,
				Definition->CancellationRefundBasisPoints, Refund))
			{
				return false;
			}
			if (Refund == 0)
			{
				continue;
			}
			if (!CityInventory.IsSet() || CityInventory->FreeCapacity.GetRawValue() < Refund ||
				!CityInventory->AcceptedGoods.Contains(GoodId.Value))
			{
				return false;
			}
			const FHansaInventoryTransactionResult Transfer = Inventories.TryTransfer(
				FHansaInventoryEndpoint::Source(TEXT("Construction.CancellationRefund")),
				FHansaInventoryEndpoint::Inventory(CityInventory->Id),
				GoodId.Value,
				FHansaQuantity::FromRaw(Refund),
				Tick,
				Inventories.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
			if (!Transfer.IsSuccess())
			{
				return false;
			}
		}
		House->Money = MoneyAfter.Value;
		OutCurrencyRefund = FHansaMoney::FromRaw(CurrencyRefund);
		return true;
	}

	void FHansaConstructionExecutor::AdvanceOneTick(
		TArray<FHansaBuildingState>& Buildings,
		const FHansaEconomicRegistry& Definitions,
		const FHansaSimulationTick Tick,
		uint64& InOutPublishedEventCount,
		TArray<FHansaDomainEvent>& OutEvents)
	{
		for (FHansaBuildingState& Building : Buildings)
		{
			if (Building.ConstructionState == EHansaConstructionState::Completed ||
				Tick < Building.ConstructionStartedTick || Tick == Building.ConstructionStartedTick)
			{
				continue;
			}
			const FHansaCompiledBuildingDefinition* Definition = Definitions.FindBuilding(Building.DefinitionId.ToString());
			if (Definition == nullptr || Definition->BuildTicks <= 0 ||
				Building.ConstructionElapsedTicks >= Definition->BuildTicks)
			{
				continue;
			}
			++Building.ConstructionElapsedTicks;
			const THansaValueResult<FHansaRate> Progress = FHansaRate::TryRatio(
				Building.ConstructionElapsedTicks, Definition->BuildTicks, EHansaRoundingMode::TowardZero);
			if (!Progress)
			{
				continue;
			}
			Building.ConstructionProgress = Progress.Value;

			FHansaDomainEvent ProgressEvent;
			ProgressEvent.GlobalSequence = ++InOutPublishedEventCount;
			ProgressEvent.Tick = Tick;
			ProgressEvent.Type = EHansaDomainEventType::ConstructionProgressed;
			ProgressEvent.IssuingHouseId = Building.OwnerId;
			ProgressEvent.BuildingId = Building.Id;
			ProgressEvent.Value = Building.ConstructionProgress.GetPartsPerMillion();
			OutEvents.Add(MoveTemp(ProgressEvent));

			if (Building.ConstructionElapsedTicks == Definition->BuildTicks)
			{
				Building.ConstructionState = EHansaConstructionState::Completed;
				Building.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
				FHansaDomainEvent CompletedEvent;
				CompletedEvent.GlobalSequence = ++InOutPublishedEventCount;
				CompletedEvent.Tick = Tick;
				CompletedEvent.Type = EHansaDomainEventType::ConstructionCompleted;
				CompletedEvent.IssuingHouseId = Building.OwnerId;
				CompletedEvent.BuildingId = Building.Id;
				CompletedEvent.Value = Definition->BuildTicks;
				OutEvents.Add(MoveTemp(CompletedEvent));
			}
		}
	}
}
