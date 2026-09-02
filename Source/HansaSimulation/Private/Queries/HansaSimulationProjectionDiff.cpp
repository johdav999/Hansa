#include "Queries/HansaSimulationProjectionDiff.h"

namespace Hansa::Simulation
{
	namespace
	{
		void AddEntry(
			TArray<FHansaProjectionDiffEntry>& Entries,
			const EHansaProjectionDiffField Field,
			const FString& StableKey,
			const FString& BeforeValue,
			const FString& AfterValue)
		{
			FHansaProjectionDiffEntry Entry;
			Entry.Field = Field;
			Entry.StableKey = StableKey;
			Entry.BeforeValue = BeforeValue;
			Entry.AfterValue = AfterValue;
			Entries.Add(MoveTemp(Entry));
		}

		template <typename TValue>
		void AddNumericDifference(
			TArray<FHansaProjectionDiffEntry>& Entries,
			const EHansaProjectionDiffField Field,
			const TCHAR* StableKey,
			const TValue Before,
			const TValue After)
		{
			if (Before != After)
			{
				AddEntry(Entries, Field, StableKey,
					FString::Printf(TEXT("%lld"), static_cast<long long>(Before)),
					FString::Printf(TEXT("%lld"), static_cast<long long>(After)));
			}
		}

		void AddUnsignedDifference(
			TArray<FHansaProjectionDiffEntry>& Entries,
			const EHansaProjectionDiffField Field,
			const TCHAR* StableKey,
			const uint64 Before,
			const uint64 After)
		{
			if (Before != After)
			{
				AddEntry(Entries, Field, StableKey,
					FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Before)),
					FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(After)));
			}
		}

		FString HouseKey(const FHansaHouseId Id)
		{
			return FString::Printf(
				TEXT("House#%llu@%u.Money"),
				static_cast<unsigned long long>(Id.GetValue()),
				Id.GetGeneration());
		}
	}

	const TCHAR* LexToString(const EHansaProjectionDiffField Field)
	{
		switch (Field)
		{
		case EHansaProjectionDiffField::Tick: return TEXT("Tick");
		case EHansaProjectionDiffField::ProcessedCommandCount: return TEXT("ProcessedCommandCount");
		case EHansaProjectionDiffField::PublishedDomainEventCount: return TEXT("PublishedDomainEventCount");
		case EHansaProjectionDiffField::CityCount: return TEXT("CityCount");
		case EHansaProjectionDiffField::BuildingCount: return TEXT("BuildingCount");
		case EHansaProjectionDiffField::VehicleCount: return TEXT("VehicleCount");
		case EHansaProjectionDiffField::RouteCount: return TEXT("RouteCount");
		case EHansaProjectionDiffField::TestEntityCount: return TEXT("TestEntityCount");
		case EHansaProjectionDiffField::HouseMoney: return TEXT("HouseMoney");
		case EHansaProjectionDiffField::Fingerprint: return TEXT("Fingerprint");
		default: return TEXT("UnknownProjectionDiffField");
		}
	}

	FHansaSimulationProjectionDiff FHansaSimulationProjectionDiff::Compare(
		const FHansaSimulationProjection& Before,
		const FHansaSimulationProjection& After)
	{
		FHansaSimulationProjectionDiff Diff;
		AddNumericDifference(Diff.Entries, EHansaProjectionDiffField::Tick, TEXT("Tick"),
			Before.GetClock().GetTick().GetValue(), After.GetClock().GetTick().GetValue());
		AddUnsignedDifference(Diff.Entries, EHansaProjectionDiffField::ProcessedCommandCount,
			TEXT("ProcessedCommandCount"), Before.GetProcessedCommandCount(), After.GetProcessedCommandCount());
		AddUnsignedDifference(Diff.Entries, EHansaProjectionDiffField::PublishedDomainEventCount,
			TEXT("PublishedDomainEventCount"), Before.GetPublishedDomainEventCount(), After.GetPublishedDomainEventCount());
		AddNumericDifference(Diff.Entries, EHansaProjectionDiffField::CityCount,
			TEXT("CityCount"), Before.GetCityCount(), After.GetCityCount());
		AddNumericDifference(Diff.Entries, EHansaProjectionDiffField::BuildingCount,
			TEXT("BuildingCount"), Before.GetBuildingCount(), After.GetBuildingCount());
		AddNumericDifference(Diff.Entries, EHansaProjectionDiffField::VehicleCount,
			TEXT("VehicleCount"), Before.GetVehicleCount(), After.GetVehicleCount());
		AddNumericDifference(Diff.Entries, EHansaProjectionDiffField::RouteCount,
			TEXT("RouteCount"), Before.GetRouteCount(), After.GetRouteCount());
		AddNumericDifference(Diff.Entries, EHansaProjectionDiffField::TestEntityCount,
			TEXT("TestEntityCount"), Before.GetTestEntityCount(), After.GetTestEntityCount());

		const TConstArrayView<FHansaHouseProjection> BeforeHouses = Before.GetHouses();
		const TConstArrayView<FHansaHouseProjection> AfterHouses = After.GetHouses();
		int32 BeforeIndex = 0;
		int32 AfterIndex = 0;
		while (BeforeIndex < BeforeHouses.Num() || AfterIndex < AfterHouses.Num())
		{
			if (AfterIndex >= AfterHouses.Num() ||
				(BeforeIndex < BeforeHouses.Num() && BeforeHouses[BeforeIndex].Id < AfterHouses[AfterIndex].Id))
			{
				AddEntry(Diff.Entries, EHansaProjectionDiffField::HouseMoney,
					HouseKey(BeforeHouses[BeforeIndex].Id),
					FString::Printf(TEXT("%lld"), static_cast<long long>(BeforeHouses[BeforeIndex].Money.GetRawValue())),
					TEXT("missing"));
				++BeforeIndex;
			}
			else if (BeforeIndex >= BeforeHouses.Num() || AfterHouses[AfterIndex].Id < BeforeHouses[BeforeIndex].Id)
			{
				AddEntry(Diff.Entries, EHansaProjectionDiffField::HouseMoney,
					HouseKey(AfterHouses[AfterIndex].Id),
					TEXT("missing"),
					FString::Printf(TEXT("%lld"), static_cast<long long>(AfterHouses[AfterIndex].Money.GetRawValue())));
				++AfterIndex;
			}
			else
			{
				if (BeforeHouses[BeforeIndex].Money != AfterHouses[AfterIndex].Money)
				{
					AddEntry(Diff.Entries, EHansaProjectionDiffField::HouseMoney,
						HouseKey(BeforeHouses[BeforeIndex].Id),
						FString::Printf(TEXT("%lld"), static_cast<long long>(BeforeHouses[BeforeIndex].Money.GetRawValue())),
						FString::Printf(TEXT("%lld"), static_cast<long long>(AfterHouses[AfterIndex].Money.GetRawValue())));
				}
				++BeforeIndex;
				++AfterIndex;
			}
		}

		if (Before.GetFingerprint() != After.GetFingerprint())
		{
			AddEntry(Diff.Entries, EHansaProjectionDiffField::Fingerprint, TEXT("Fingerprint"),
				Before.GetFingerprint().ToDebugString(), After.GetFingerprint().ToDebugString());
		}
		return Diff;
	}

	FString FHansaSimulationProjectionDiff::ToCompactDebugString(const int32 MaximumEntries) const
	{
		if (Entries.IsEmpty())
		{
			return TEXT("ProjectionDiff[none]");
		}
		const int32 BoundedMaximum = FMath::Max(0, MaximumEntries);
		const int32 Included = FMath::Min(Entries.Num(), BoundedMaximum);
		FString Result = TEXT("ProjectionDiff[");
		for (int32 Index = 0; Index < Included; ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(";");
			}
			Result += FString::Printf(TEXT("%s:%s=%s->%s"),
				LexToString(Entries[Index].Field),
				*Entries[Index].StableKey,
				*Entries[Index].BeforeValue,
				*Entries[Index].AfterValue);
		}
		if (Included < Entries.Num())
		{
			Result += FString::Printf(TEXT(";+%d"), Entries.Num() - Included);
		}
		Result += TEXT("]");
		return Result;
	}
}
