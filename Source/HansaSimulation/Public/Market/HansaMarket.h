#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Internationalization/Text.h"
#include "Math/HansaFixedPoint.h"
#include "Misc/Optional.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Production/HansaProduction.h"

namespace Hansa::Simulation
{
	enum class EHansaMarketExplanationFactor : uint8
	{
		Scarcity = 0,
		CitizenDemand,
		IndustrialDemand,
		IncomingSupply,
		UnmetDemand,
		SeasonModifier,
		CityModifier,
		TargetClamp
	};

	enum class EHansaMarketAlertType : uint8
	{
		Shortage = 0,
		LowReserve,
		Affordability
	};

	enum class EHansaMarketAlertSeverity : uint8
	{
		Warning = 0,
		Critical
	};

	enum class EHansaMarketConsumerKind : uint8
	{
		Citizen = 0,
		Industry
	};

	enum class EHansaMarketProducerKind : uint8
	{
		BuildingRecipe = 0,
		BackgroundSupply
	};

	enum class EHansaMarketSuggestedActionType : uint8
	{
		IncreaseLocalProduction = 0,
		ImportGood,
		InspectBlockedConsumers,
		ReplenishReserve,
		ScheduleIncomingSupply,
		IncreaseAffordableSupply
	};

	/** Age/provenance of information available to a player or automation client. */
	enum class EHansaMarketInformationState : uint8
	{
		Current = 0,
		Recent,
		Stale,
		Estimated,
		Unknown
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketExplanationFactor Factor);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketAlertType Type);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketAlertSeverity Severity);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketConsumerKind Kind);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketProducerKind Kind);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketSuggestedActionType Action);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMarketInformationState State);

	struct HANSASIMULATION_API FHansaMarketAlertPolicy final
	{
		static constexpr int32 CriticalReserveRatioBasisPoints = 2500;
		static constexpr int32 AffordabilityWarningBasisPoints = 10000;
		static constexpr int32 CriticalAffordabilityBasisPoints = 5000;
	};

	struct HANSASIMULATION_API FHansaMarketSettings final
	{
		int32 UpdateCadenceTicks = 5;
		int32 PriceHistoryCapacity = 64;
		int32 TargetSmoothingBasisPoints = 2500;
		int32 MaximumMovementBasisPointsPerUpdate = 1000;
		int32 StaleAfterTicks = 10;
	};

	struct HANSASIMULATION_API FHansaMarketReportPolicy final
	{
		int32 ReportCadenceTicks = 5;
		int32 CurrentMaxAgeTicks = 0;
		int32 RecentMaxAgeTicks = 5;
		int32 StaleMaxAgeTicks = 10;
		int32 EstimatedMaxAgeTicks = 20;
	};

	struct HANSASIMULATION_API FHansaCityMarketInitialization final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		TArray<FHansaInventoryId> InventoryIds;
		FHansaQuantity DesiredReserve;
		FHansaQuantity ConfirmedIncomingSupplyPerUpdate;
		bool bMarketOnly = false;
		FHansaQuantity BackgroundProductionPerUpdate;
		FHansaQuantity BackgroundCitizenDemandPerUpdate;
		FHansaQuantity BackgroundIndustrialDemandPerUpdate;
		FHansaMarketReportPolicy ReportPolicy;
		int32 SeasonModifierBasisPoints = 0;
		int32 CityModifierBasisPoints = 0;
		int64 MinimumPriceMilliMarks = 1;
		int64 MaximumPriceMilliMarks = 1'000'000'000;
		int64 InitialPriceMilliMarks = 0;
		int64 InitialLastUpdateTick = -1;
		int64 InitialReportTick = -1;
	};

	struct HANSASIMULATION_API FHansaMarketPriceFactors final
	{
		int32 ScarcityBasisPoints = 0;
		int32 CitizenDemandBasisPoints = 0;
		int32 IndustrialDemandBasisPoints = 0;
		int32 IncomingSupplyBasisPoints = 0;
		int32 UnmetDemandBasisPoints = 0;
		int32 SeasonModifierBasisPoints = 0;
		int32 CityModifierBasisPoints = 0;
		int32 TargetMultiplierBasisPoints = 10000;
	};

	struct HANSASIMULATION_API FHansaMarketPriceHistoryEntry final
	{
		FHansaSimulationTick Tick;
		FHansaQuantity Stock;
		FHansaQuantity CitizenDemand;
		FHansaQuantity IndustrialDemand;
		FHansaQuantity LocalProduction;
		FHansaQuantity ExpectedIncomingSupply;
		FHansaQuantity UnmetDemand;
		int32 MinimumConsumerAffordabilityBasisPoints = 10000;
		int64 PriceMilliMarks = 0;
	};

	/** Immutable values captured when a city publishes a report. */
	struct HANSASIMULATION_API FHansaMarketReportState final
	{
		bool bAvailable = false;
		int64 ReportTick = -1;
		int64 MarketUpdateTick = -1;
		FHansaQuantity Stock;
		FHansaQuantity DesiredReserve;
		FHansaQuantity CitizenDemand;
		FHansaQuantity IndustrialDemand;
		FHansaQuantity RecentLocalProduction;
		FHansaQuantity ExpectedIncomingSupply;
		FHansaQuantity UnmetDemand;
		int64 PriceMilliMarks = 0;
		FHansaMarketPriceFactors Factors;
		TArray<FHansaMarketPriceHistoryEntry> PriceHistory;
	};

	struct HANSASIMULATION_API FHansaCityMarketState final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		TArray<FHansaInventoryId> InventoryIds;
		FHansaQuantity DesiredReserve;
		FHansaQuantity ConfirmedIncomingSupplyPerUpdate;
		bool bMarketOnly = false;
		FHansaQuantity BackgroundProductionPerUpdate;
		FHansaQuantity BackgroundCitizenDemandPerUpdate;
		FHansaQuantity BackgroundIndustrialDemandPerUpdate;
		FHansaMarketReportPolicy ReportPolicy;
		int32 SeasonModifierBasisPoints = 0;
		int32 CityModifierBasisPoints = 0;
		int64 MinimumPriceMilliMarks = 1;
		int64 MaximumPriceMilliMarks = 1;
		int64 CurrentPriceMilliMarks = 1;
		int64 LastUpdateTick = -1;
		FHansaQuantity CurrentStock;
		FHansaQuantity CitizenDemand;
		FHansaQuantity IndustrialDemand;
		FHansaQuantity RecentLocalProduction;
		FHansaQuantity AccumulatedLocalProductionSinceUpdate;
		FHansaQuantity ExpectedIncomingSupply;
		FHansaQuantity UnmetDemand;
		int32 MinimumConsumerAffordabilityBasisPoints = 10000;
		int64 ShortageSinceTick = -1;
		int64 LowReserveSinceTick = -1;
		int64 AffordabilitySinceTick = -1;
		FHansaMarketPriceFactors Factors;
		TArray<FHansaMarketPriceHistoryEntry> PriceHistory;
		FHansaMarketReportState Report;
	};

	struct HANSASIMULATION_API FHansaMarketReportAgeProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		EHansaMarketInformationState InformationState = EHansaMarketInformationState::Unknown;
		TOptional<int64> ReportTick;
		TOptional<int64> MarketUpdateTick;
		TOptional<int64> AgeTicks;
	};

	struct HANSASIMULATION_API FHansaKnownMarketPriceProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		EHansaMarketInformationState InformationState = EHansaMarketInformationState::Unknown;
		TOptional<int64> PriceMilliMarks;
		TOptional<int64> RecentAveragePriceMilliMarks;
		TOptional<int64> ReportTick;
		TOptional<int64> ReportAgeTicks;
	};

	struct HANSASIMULATION_API FHansaKnownMarketSupplyDemandProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		EHansaMarketInformationState InformationState = EHansaMarketInformationState::Unknown;
		TOptional<FHansaQuantity> Stock;
		TOptional<FHansaQuantity> DesiredReserve;
		TOptional<FHansaQuantity> CitizenDemand;
		TOptional<FHansaQuantity> IndustrialDemand;
		TOptional<FHansaQuantity> TotalDemand;
		TOptional<FHansaQuantity> RecentLocalProduction;
		TOptional<FHansaQuantity> ExpectedIncomingSupply;
		TOptional<FHansaQuantity> UnmetDemand;
	};

	struct HANSASIMULATION_API FHansaMarketOpportunityComparisonProjection final
	{
		FHansaCityDefinitionId SourceCityId;
		FHansaCityDefinitionId DestinationCityId;
		FHansaGoodId GoodId;
		EHansaMarketInformationState SourceInformationState = EHansaMarketInformationState::Unknown;
		EHansaMarketInformationState DestinationInformationState = EHansaMarketInformationState::Unknown;
		bool bComparable = false;
		TOptional<int64> SourcePriceMilliMarks;
		TOptional<int64> DestinationPriceMilliMarks;
		TOptional<int64> GrossMarginMilliMarks;
		TOptional<FHansaQuantity> SourceAvailableAboveReserve;
		TOptional<FHansaQuantity> DestinationDemandGap;
	};

	/** One localized, ordered contribution to the authoritative target multiplier. */
	struct HANSASIMULATION_API FHansaMarketExplanationEntry final
	{
		EHansaMarketExplanationFactor Factor = EHansaMarketExplanationFactor::Scarcity;
		FName MessageKey;
		FText Message;
		int32 ContributionBasisPoints = 0;
	};

	struct HANSASIMULATION_API FHansaMarketExplanationProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		int32 BaseMultiplierBasisPoints = 10000;
		int32 RawMultiplierBasisPoints = 10000;
		int32 TargetMultiplierBasisPoints = 10000;
		TArray<FHansaMarketExplanationEntry> Factors;
	};

	struct HANSASIMULATION_API FHansaMarketPriceProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		int64 CurrentPriceMilliMarks = 0;
		int64 RecentAveragePriceMilliMarks = 0;
		int64 LastUpdateTick = -1;
		int64 NextUpdateTick = 0;
		int64 ReportAgeTicks = 0;
		bool bIsStale = true;
		TArray<FHansaMarketPriceHistoryEntry> History;
	};

	struct HANSASIMULATION_API FHansaMarketSupplyDemandProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		FHansaQuantity Stock;
		FHansaQuantity DesiredReserve;
		FHansaQuantity CitizenDemand;
		FHansaQuantity IndustrialDemand;
		FHansaQuantity TotalDemand;
		FHansaQuantity RecentLocalProduction;
		FHansaQuantity ExpectedIncomingSupply;
		FHansaQuantity UnmetDemand;
	};

	struct HANSASIMULATION_API FHansaMarketReserveProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		FHansaQuantity Stock;
		FHansaQuantity DemandPerTick;
		int64 ReserveMilliDays = 0;
		bool bHasDemand = false;
	};

	struct HANSASIMULATION_API FHansaMarketConsumerProjection final
	{
		EHansaMarketConsumerKind Kind = EHansaMarketConsumerKind::Citizen;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		FHansaPopulationCohortId PopulationCohortId;
		FHansaBuildingId BuildingId;
		FHansaProductionId ProductionId;
		FHansaRecipeId RecipeId;
		FHansaQuantity DemandPerTick;
		FHansaQuantity FulfilledLastTick;
		int32 AffordabilityBasisPoints = 10000;
		int64 ReserveMilliDays = 0;
		EHansaProductionBlocker ProductionBlocker = EHansaProductionBlocker::None;
	};

	struct HANSASIMULATION_API FHansaMarketProducerProjection final
	{
		EHansaMarketProducerKind Kind = EHansaMarketProducerKind::BuildingRecipe;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		FHansaProductionId ProductionId;
		FHansaBuildingId BuildingId;
		FHansaRecipeId RecipeId;
		FHansaQuantity NominalQuantityPerCycle;
		FHansaQuantity ActualQuantityLastTick;
		int32 CycleTicks = 0;
		bool bActive = false;
		EHansaProductionBlocker Blocker = EHansaProductionBlocker::None;
	};

	struct HANSASIMULATION_API FHansaMarketSuggestedAction final
	{
		EHansaMarketSuggestedActionType Type = EHansaMarketSuggestedActionType::IncreaseLocalProduction;
		FName MessageKey;
		FText Message;
	};

	struct HANSASIMULATION_API FHansaMarketAlertProjection final
	{
		EHansaMarketAlertType Type = EHansaMarketAlertType::Shortage;
		EHansaMarketAlertSeverity Severity = EHansaMarketAlertSeverity::Warning;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		TArray<FHansaPopulationCohortId> PopulationCohortIds;
		TArray<FHansaProductionId> ProductionIds;
		FName CauseMessageKey;
		FText Cause;
		int64 ActiveSinceTick = -1;
		int64 AgeTicks = 0;
		TArray<FHansaMarketSuggestedAction> SuggestedActions;
	};

	/** Owning market report shared by game UI, diagnostics and allowlisted automation. */
	struct HANSASIMULATION_API FHansaCityMarketProjection final
	{
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		FHansaQuantity CurrentStock;
		FHansaQuantity DesiredReserve;
		FHansaQuantity CitizenDemand;
		FHansaQuantity IndustrialDemand;
		FHansaQuantity RecentLocalProduction;
		FHansaQuantity ExpectedIncomingSupply;
		FHansaQuantity UnmetDemand;
		int64 CurrentPriceMilliMarks = 0;
		int64 RecentAveragePriceMilliMarks = 0;
		FHansaMarketPriceFactors Factors;
		int64 LastUpdateTick = -1;
		int64 NextUpdateTick = 0;
		int64 ReportAgeTicks = 0;
		bool bIsStale = true;
		TArray<FHansaMarketPriceHistoryEntry> PriceHistory;
	};

	class HANSASIMULATION_API FHansaMarketSnapshot final
	{
	public:
		[[nodiscard]] const FHansaMarketSettings& GetSettings() const { return Settings; }
		[[nodiscard]] TConstArrayView<FHansaCityMarketState> GetMarkets() const { return Markets; }

	private:
		friend class FHansaSimulationReadOnlyAccess;
		FHansaMarketSettings Settings;
		TArray<FHansaCityMarketState> Markets;
	};
}
