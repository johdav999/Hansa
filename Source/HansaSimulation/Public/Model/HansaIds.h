#pragma once

#include "Containers/UnrealString.h"
#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	class HANSASIMULATION_API FHansaDefinitionDomains final
	{
	public:
		static bool IsRegistered(const FString& Domain);
	};

	class HANSASIMULATION_API FHansaDefinitionId final
	{
	public:
		FHansaDefinitionId() = default;

		static THansaValueResult<FHansaDefinitionId> TryParse(const FString& Text);

		[[nodiscard]] bool IsValid() const { return !Canonical.IsEmpty(); }
		[[nodiscard]] const FString& ToString() const { return Canonical; }
		[[nodiscard]] FString GetDomain() const;

		friend bool operator==(const FHansaDefinitionId& Left, const FHansaDefinitionId& Right)
		{
			return Left.Canonical == Right.Canonical;
		}

		friend bool operator!=(const FHansaDefinitionId& Left, const FHansaDefinitionId& Right)
		{
			return !(Left == Right);
		}

		friend bool operator<(const FHansaDefinitionId& Left, const FHansaDefinitionId& Right)
		{
			return Left.Canonical.Compare(Right.Canonical, ESearchCase::CaseSensitive) < 0;
		}

		friend uint32 GetTypeHash(const FHansaDefinitionId& Id)
		{
			return GetTypeHash(Id.Canonical);
		}

	private:
		explicit FHansaDefinitionId(const FString& InCanonical)
			: Canonical(InCanonical)
		{
		}

		FString Canonical;
	};

#define HANSA_DEFINITION_ID_TRAITS(TraitsName, DomainText, TagValue) \
	struct TraitsName final \
	{ \
		static const TCHAR* Domain() { return TEXT(DomainText); } \
		static constexpr uint8 SerializationTag = TagValue; \
	}

	HANSA_DEFINITION_ID_TRAITS(FHansaGoodIdTraits, "Good", 10);
	HANSA_DEFINITION_ID_TRAITS(FHansaRecipeIdTraits, "Recipe", 11);
	HANSA_DEFINITION_ID_TRAITS(FHansaBuildingTypeIdTraits, "Building", 12);
	HANSA_DEFINITION_ID_TRAITS(FHansaNeedIdTraits, "Need", 13);
	HANSA_DEFINITION_ID_TRAITS(FHansaPopulationTierIdTraits, "PopulationTier", 14);
	HANSA_DEFINITION_ID_TRAITS(FHansaVehicleDefinitionIdTraits, "Vehicle", 15);
	HANSA_DEFINITION_ID_TRAITS(FHansaCityDefinitionIdTraits, "City", 16);
	HANSA_DEFINITION_ID_TRAITS(FHansaRegionDefinitionIdTraits, "Region", 17);
	HANSA_DEFINITION_ID_TRAITS(FHansaTechnologyIdTraits, "Technology", 18);
	HANSA_DEFINITION_ID_TRAITS(FHansaEventIdTraits, "Event", 19);
	HANSA_DEFINITION_ID_TRAITS(FHansaVictoryIdTraits, "Victory", 20);
	HANSA_DEFINITION_ID_TRAITS(FHansaScenarioIdTraits, "Scenario", 21);

#undef HANSA_DEFINITION_ID_TRAITS

	template <typename TTraits>
	class THansaDefinitionId final
	{
	public:
		THansaDefinitionId() = default;

		static THansaValueResult<THansaDefinitionId> TryParse(const FString& Text)
		{
			const THansaValueResult<FHansaDefinitionId> Parsed = FHansaDefinitionId::TryParse(Text);
			if (!Parsed)
			{
				return THansaValueResult<THansaDefinitionId>::Failure(Parsed.Error);
			}
			if (Parsed.Value.GetDomain() != TTraits::Domain())
			{
				return THansaValueResult<THansaDefinitionId>::Failure(EHansaValueError::WrongDomain);
			}
			return THansaValueResult<THansaDefinitionId>::Success(THansaDefinitionId(Parsed.Value));
		}

		[[nodiscard]] bool IsValid() const { return Value.IsValid(); }
		[[nodiscard]] const FString& ToString() const { return Value.ToString(); }
		[[nodiscard]] static constexpr uint8 GetSerializationTag() { return TTraits::SerializationTag; }

		friend bool operator==(const THansaDefinitionId& Left, const THansaDefinitionId& Right)
		{
			return Left.Value == Right.Value;
		}

		friend bool operator!=(const THansaDefinitionId& Left, const THansaDefinitionId& Right)
		{
			return !(Left == Right);
		}

		friend bool operator<(const THansaDefinitionId& Left, const THansaDefinitionId& Right)
		{
			return Left.Value < Right.Value;
		}

		friend uint32 GetTypeHash(const THansaDefinitionId& Id)
		{
			return GetTypeHash(Id.Value);
		}

	private:
		explicit THansaDefinitionId(const FHansaDefinitionId& InValue)
			: Value(InValue)
		{
		}

		FHansaDefinitionId Value;
	};

	using FHansaGoodId = THansaDefinitionId<FHansaGoodIdTraits>;
	using FHansaRecipeId = THansaDefinitionId<FHansaRecipeIdTraits>;
	using FHansaBuildingTypeId = THansaDefinitionId<FHansaBuildingTypeIdTraits>;
	using FHansaNeedId = THansaDefinitionId<FHansaNeedIdTraits>;
	using FHansaPopulationTierId = THansaDefinitionId<FHansaPopulationTierIdTraits>;
	using FHansaVehicleDefinitionId = THansaDefinitionId<FHansaVehicleDefinitionIdTraits>;
	using FHansaCityDefinitionId = THansaDefinitionId<FHansaCityDefinitionIdTraits>;
	using FHansaRegionDefinitionId = THansaDefinitionId<FHansaRegionDefinitionIdTraits>;
	using FHansaTechnologyId = THansaDefinitionId<FHansaTechnologyIdTraits>;
	using FHansaEventId = THansaDefinitionId<FHansaEventIdTraits>;
	using FHansaVictoryId = THansaDefinitionId<FHansaVictoryIdTraits>;
	using FHansaScenarioId = THansaDefinitionId<FHansaScenarioIdTraits>;

#define HANSA_ENTITY_ID_TRAITS(TraitsName, DebugText, TagValue) \
	struct TraitsName final \
	{ \
		static const TCHAR* DebugName() { return TEXT(DebugText); } \
		static constexpr uint8 SerializationTag = TagValue; \
	}

	HANSA_ENTITY_ID_TRAITS(FHansaHouseIdTraits, "House", 40);
	HANSA_ENTITY_ID_TRAITS(FHansaBuildingIdTraits, "Building", 41);
	HANSA_ENTITY_ID_TRAITS(FHansaVehicleIdTraits, "Vehicle", 42);
	HANSA_ENTITY_ID_TRAITS(FHansaRouteIdTraits, "Route", 43);
	HANSA_ENTITY_ID_TRAITS(FHansaContractIdTraits, "Contract", 44);
	HANSA_ENTITY_ID_TRAITS(FHansaCommandIdTraits, "Command", 45);
	HANSA_ENTITY_ID_TRAITS(FHansaTestEntityIdTraits, "TestEntity", 46);
	HANSA_ENTITY_ID_TRAITS(FHansaInventoryIdTraits, "Inventory", 47);
	HANSA_ENTITY_ID_TRAITS(FHansaReservationIdTraits, "Reservation", 48);
	HANSA_ENTITY_ID_TRAITS(FHansaProductionIdTraits, "Production", 49);
	HANSA_ENTITY_ID_TRAITS(FHansaPopulationCohortIdTraits, "PopulationCohort", 50);
	HANSA_ENTITY_ID_TRAITS(FHansaLogisticsRequestIdTraits, "LogisticsRequest", 51);
	HANSA_ENTITY_ID_TRAITS(FHansaLogisticsJobIdTraits, "LogisticsJob", 52);

#undef HANSA_ENTITY_ID_TRAITS

	template <typename TTraits>
	class THansaEntityId final
	{
	public:
		THansaEntityId() = default;

		static THansaValueResult<THansaEntityId> TryCreate(const uint64 InValue, const uint32 InGeneration = 0)
		{
			if (InValue == 0)
			{
				return THansaValueResult<THansaEntityId>::Failure(EHansaValueError::InvalidZero);
			}
			return THansaValueResult<THansaEntityId>::Success(THansaEntityId(InValue, InGeneration));
		}

		[[nodiscard]] bool IsValid() const { return Value != 0; }
		[[nodiscard]] uint64 GetValue() const { return Value; }
		[[nodiscard]] uint32 GetGeneration() const { return Generation; }
		[[nodiscard]] static constexpr uint8 GetSerializationTag() { return TTraits::SerializationTag; }

		[[nodiscard]] FString ToDebugString() const
		{
			return IsValid()
				? FString::Printf(TEXT("%s#%llu@%u"), TTraits::DebugName(), static_cast<unsigned long long>(Value), Generation)
				: FString::Printf(TEXT("%s#Invalid"), TTraits::DebugName());
		}

		friend bool operator==(const THansaEntityId& Left, const THansaEntityId& Right)
		{
			return Left.Value == Right.Value && Left.Generation == Right.Generation;
		}

		friend bool operator!=(const THansaEntityId& Left, const THansaEntityId& Right)
		{
			return !(Left == Right);
		}

		friend bool operator<(const THansaEntityId& Left, const THansaEntityId& Right)
		{
			return Left.Value < Right.Value || (Left.Value == Right.Value && Left.Generation < Right.Generation);
		}

		friend uint32 GetTypeHash(const THansaEntityId& Id)
		{
			return HashCombine(GetTypeHash(Id.Value), GetTypeHash(Id.Generation));
		}

	private:
		THansaEntityId(const uint64 InValue, const uint32 InGeneration)
			: Value(InValue)
			, Generation(InGeneration)
		{
		}

		uint64 Value = 0;
		uint32 Generation = 0;
	};

	using FHansaHouseId = THansaEntityId<FHansaHouseIdTraits>;
	using FHansaBuildingId = THansaEntityId<FHansaBuildingIdTraits>;
	using FHansaVehicleId = THansaEntityId<FHansaVehicleIdTraits>;
	using FHansaRouteId = THansaEntityId<FHansaRouteIdTraits>;
	using FHansaContractId = THansaEntityId<FHansaContractIdTraits>;
	using FHansaCommandId = THansaEntityId<FHansaCommandIdTraits>;
	using FHansaTestEntityId = THansaEntityId<FHansaTestEntityIdTraits>;
	using FHansaInventoryId = THansaEntityId<FHansaInventoryIdTraits>;
	using FHansaReservationId = THansaEntityId<FHansaReservationIdTraits>;
	using FHansaProductionId = THansaEntityId<FHansaProductionIdTraits>;
	using FHansaPopulationCohortId = THansaEntityId<FHansaPopulationCohortIdTraits>;
	using FHansaLogisticsRequestId = THansaEntityId<FHansaLogisticsRequestIdTraits>;
	using FHansaLogisticsJobId = THansaEntityId<FHansaLogisticsJobIdTraits>;
}
