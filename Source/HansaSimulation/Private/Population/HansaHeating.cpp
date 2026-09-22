#include "Population/HansaHeating.h"
#include "Population/HansaSeasonalNeeds.h"

namespace Hansa::Simulation
{
	namespace
	{
		int64 Daily(const FHansaCompiledPopulationTierNeed& Requirement, int32 Residents, int32 Factor, uint32 Minutes)
		{
			const auto Rate = FHansaCheckedIntegerMath::TryMultiplyDivide(static_cast<int64>(Requirement.ConsumptionMilliUnitsPerResidentPerTick) * Residents,
				static_cast<int64>(Factor) * 1440, static_cast<int64>(Minutes) * 10000, EHansaRoundingMode::Ceiling);
			return Rate ? Rate.Value : 0;
		}
	}

	FHansaHeatingProjection FHansaHeating::Project(FHansaCityDefinitionId CityId, const FHansaInventoryReadOnlyAccess& Inventories,
		TConstArrayView<FHansaCityState> Cities, TConstArrayView<FHansaPopulationCohortState> Cohorts,
		const FHansaEconomicRegistry& Registry, const FHansaSimulationClock& Clock)
	{
		FHansaHeatingProjection Result;
		const auto* Need = Registry.FindNeed(TEXT("Need.Heating"));
		if (!Need) return Result;
		Result.ReserveDays = Need->DefaultReserveDays;
		for (const auto& City : Cities) if (City.DefinitionId == CityId)
		{
			if (City.HeatingReserveDays >= 0) Result.ReserveDays = City.HeatingReserveDays;
			Result.bOverride = City.bReleaseHeatingReserve;
		}
		Result.SeasonMultiplier = FHansaSeasonalNeeds::Multiplier(*Need, Clock.GetTick(), Clock.GetMinutesPerTick());
		TArray<FHansaInventoryId> Pools;
		for (const auto& Cohort : Cohorts)
		{
			if (Cohort.CityId != CityId || !Cohort.bResidenceOperational || Cohort.Residents <= 0) continue;
			const auto* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
			if (!Tier) continue;
			for (const auto& Requirement : Tier->Needs) if (Requirement.NeedId == Need->StableId)
			{
				Result.HouseholdDailyRaw += Daily(Requirement, Cohort.Residents, Result.SeasonMultiplier, Clock.GetMinutesPerTick());
				Result.WinterDailyRaw += Daily(Requirement, Cohort.Residents, 10000, Clock.GetMinutesPerTick());
				if (Cohort.bHasMarketAccess) Pools.AddUnique(Cohort.ConsumptionInventoryId);
			}
		}
		const auto Good = FHansaGoodId::TryParse(Need->GoodId);
		if (!Good) return Result;
		for (auto Id : Pools)
		{
			const auto Stock = Inventories.QueryStock(Id, Good.Value);
			if (!Stock) continue;
			Result.StockRaw += Stock->Stock.GetRawValue();
			Result.CommittedRaw += Stock->Reserved.GetRawValue();
			Result.ProtectedRaw += Inventories.QueryProtectedRaw(Id, Good.Value);
			Result.SurplusRaw += FMath::Max<int64>(0, Stock->Available.GetRawValue() - Inventories.QueryProtectedRaw(Id, Good.Value));
		}
		return Result;
	}

	void FHansaHeating::RefreshProtection(FHansaInventoryLedger& Ledger, const TArray<FHansaCityState>& Cities,
		const TArray<FHansaPopulationCohortState>& Cohorts, const FHansaEconomicRegistry& Registry,
		const FHansaSimulationClock& Clock)
	{
		TArray<FHansaHouseholdStockProtection> Floors;
		const auto* Need = Registry.FindNeed(TEXT("Need.Heating"));
		if (!Need) { Ledger.SetHouseholdProtection({}); return; }
		const auto Good = FHansaGoodId::TryParse(Need->GoodId);
		if (!Good) { Ledger.SetHouseholdProtection({}); return; }
		const int32 Factor = FHansaSeasonalNeeds::Multiplier(*Need, Clock.GetTick(), Clock.GetMinutesPerTick());
		for (const auto& Cohort : Cohorts)
		{
			if (!Cohort.bResidenceOperational || !Cohort.bHasMarketAccess || Cohort.Residents <= 0) continue;
			const auto* City = Cities.FindByPredicate([&](const auto& C) { return C.DefinitionId == Cohort.CityId; });
			if (!City || City->bReleaseHeatingReserve) continue;
			const int32 Days = City->HeatingReserveDays >= 0 ? City->HeatingReserveDays : Need->DefaultReserveDays;
			const auto* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
			if (!Tier) continue;
			const auto Pool = Ledger.CreateReadOnlyAccess().QueryInventory(Cohort.ConsumptionInventoryId);
			if (!Pool || Pool->OwnerKind != EHansaInventoryOwnerKind::City || Pool->CityId != Cohort.CityId) continue;
			for (const auto& Requirement : Tier->Needs) if (Requirement.NeedId == Need->StableId)
			{
				auto* Floor = Floors.FindByPredicate([&](const auto& V) { return V.InventoryId == Cohort.ConsumptionInventoryId; });
				if (!Floor) { Floors.Add({Cohort.ConsumptionInventoryId, Good.Value, 0}); Floor = &Floors.Last(); }
				Floor->TargetRaw += Daily(Requirement, Cohort.Residents, Factor, Clock.GetMinutesPerTick()) * Days;
			}
		}
		Ledger.SetHouseholdProtection(MoveTemp(Floors));
	}
}
