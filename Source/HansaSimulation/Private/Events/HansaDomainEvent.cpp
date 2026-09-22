#include "Events/HansaDomainEvent.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaDomainEventType Type)
	{
		switch (Type)
		{
		case EHansaDomainEventType::TestEntityCreated: return TEXT("TestEntityCreated");
		case EHansaDomainEventType::TestEntityCancelled: return TEXT("TestEntityCancelled");
		case EHansaDomainEventType::NoOpCommandAccepted: return TEXT("NoOpCommandAccepted");
		case EHansaDomainEventType::ProductionCycleCompleted: return TEXT("ProductionCycleCompleted");
		case EHansaDomainEventType::ProductionBlockerChanged: return TEXT("ProductionBlockerChanged");
		case EHansaDomainEventType::ProductionActiveChanged: return TEXT("ProductionActiveChanged");
		case EHansaDomainEventType::BuildingPlaced: return TEXT("BuildingPlaced");
		case EHansaDomainEventType::ConstructionProgressed: return TEXT("ConstructionProgressed");
		case EHansaDomainEventType::ConstructionCompleted: return TEXT("ConstructionCompleted");
		case EHansaDomainEventType::ConstructionCancelled: return TEXT("ConstructionCancelled");
		case EHansaDomainEventType::BuildingRemoved: return TEXT("BuildingRemoved");
		case EHansaDomainEventType::ResidenceUpgraded: return TEXT("ResidenceUpgraded");
		case EHansaDomainEventType::RouteCreated: return TEXT("RouteCreated");
		case EHansaDomainEventType::RouteEdited: return TEXT("RouteEdited");
		case EHansaDomainEventType::RouteActivationChanged: return TEXT("RouteActivationChanged");
		case EHansaDomainEventType::RouteCancelled: return TEXT("RouteCancelled");
		case EHansaDomainEventType::RouteDeparted: return TEXT("RouteDeparted");
		case EHansaDomainEventType::RouteArrived: return TEXT("RouteArrived");
		case EHansaDomainEventType::RouteCargoTransferred: return TEXT("RouteCargoTransferred");
		case EHansaDomainEventType::RouteCargoMissed: return TEXT("RouteCargoMissed");
		case EHansaDomainEventType::ProductionModeChanged: return TEXT("ProductionModeChanged");
		case EHansaDomainEventType::ProductionUpgradeQueued: return TEXT("ProductionUpgradeQueued");
		case EHansaDomainEventType::ShipMoveOrdered: return TEXT("ShipMoveOrdered");
        case EHansaDomainEventType::ShipArrived: return TEXT("ShipArrived");
		case EHansaDomainEventType::SpotTradeCompleted: return TEXT("SpotTradeCompleted");
		case EHansaDomainEventType::SpotTradePartial: return TEXT("SpotTradePartial");
		case EHansaDomainEventType::SpotTradeMissed: return TEXT("SpotTradeMissed");
		case EHansaDomainEventType::TradeStationProposed: return TEXT("TradeStationProposed");
		case EHansaDomainEventType::TradeStationFunded: return TEXT("TradeStationFunded");
		case EHansaDomainEventType::TradeStationConstructionCompleted: return TEXT("TradeStationConstructionCompleted");
		case EHansaDomainEventType::TradeStationClosed: return TEXT("TradeStationClosed");
        case EHansaDomainEventType::StationOrderChanged: return TEXT("StationOrderChanged");
        case EHansaDomainEventType::StationOrderExecuted: return TEXT("StationOrderExecuted");
		case EHansaDomainEventType::PresenceContributionAccepted: return TEXT("PresenceContributionAccepted");
		case EHansaDomainEventType::PresenceUpgradeRequested: return TEXT("PresenceUpgradeRequested");
		case EHansaDomainEventType::PresenceUpgradeFunded: return TEXT("PresenceUpgradeFunded");
		case EHansaDomainEventType::PresenceUpgradeCompleted: return TEXT("PresenceUpgradeCompleted");
		case EHansaDomainEventType::PresenceSpecializationApplied: return TEXT("PresenceSpecializationApplied");
        case EHansaDomainEventType::RouteTradeSettled: return TEXT("RouteTradeSettled");
        case EHansaDomainEventType::HouseholdAvailabilityChanged: return TEXT("HouseholdAvailabilityChanged");
		case EHansaDomainEventType::HeatingReserveChanged: return TEXT("HeatingReserveChanged");
		case EHansaDomainEventType::ResearchQueued: return TEXT("ResearchQueued");
		case EHansaDomainEventType::ResearchCompleted: return TEXT("ResearchCompleted");
		default: return TEXT("UnknownDomainEvent");
		}
	}

	FString FHansaDomainEvent::ToDebugString() const
	{
        if (Type == EHansaDomainEventType::StationOrderChanged || Type == EHansaDomainEventType::StationOrderExecuted)
            return FString::Printf(TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;station=%s;order=%llu;good=%s;quantity=%lld;moneyDelta=%lld]"),LexToString(Type),GlobalSequence,Tick.GetValue(),*TradeStationId.ToDebugString(),StationOrderId,*GoodId.ToString(),Value,RelatedValue);
		if (Type == EHansaDomainEventType::BuildingPlaced)
		{
			return FString::Printf(
				TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;building=%s;definition=%s;cell=%d,%d;rotation=%s]"),
				LexToString(Type),
				static_cast<unsigned long long>(GlobalSequence),
				static_cast<long long>(Tick.GetValue()),
				*BuildingId.ToDebugString(),
				*Placement.BuildingDefinitionId.ToString(),
				Placement.Anchor.X,
				Placement.Anchor.Y,
				LexToString(Placement.Rotation));
		}
		if (Type == EHansaDomainEventType::ProductionCycleCompleted ||
			Type == EHansaDomainEventType::ProductionBlockerChanged ||
			Type == EHansaDomainEventType::ProductionActiveChanged)
		{
			return FString::Printf(
				TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;production=%s;recipe=%s;blocker=%s]"),
				LexToString(Type),
				static_cast<unsigned long long>(GlobalSequence),
				static_cast<long long>(Tick.GetValue()),
				*ProductionId.ToDebugString(),
				*RecipeId.ToString(),
				LexToString(ProductionBlocker));
		}
		if (Type >= EHansaDomainEventType::RouteCreated && Type <= EHansaDomainEventType::RouteCargoMissed)
		{
			return FString::Printf(
				TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;route=%s;vehicle=%s;city=%s;good=%s;value=%lld;related=%lld]"),
				LexToString(Type), static_cast<unsigned long long>(GlobalSequence),
				static_cast<long long>(Tick.GetValue()), *RouteId.ToDebugString(), *VehicleId.ToDebugString(),
				*CityId.ToString(), *GoodId.ToString(), static_cast<long long>(Value),
				static_cast<long long>(RelatedValue));
		}
		if (Type == EHansaDomainEventType::ResearchQueued || Type == EHansaDomainEventType::ResearchCompleted)
		{
			return FString::Printf(TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;technology=%s;value=%lld]"),
				LexToString(Type), static_cast<unsigned long long>(GlobalSequence), static_cast<long long>(Tick.GetValue()),
				*TechnologyId, static_cast<long long>(Value));
		}
		return FString::Printf(
			TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;command=%s]"),
			LexToString(Type),
			static_cast<unsigned long long>(GlobalSequence),
			static_cast<long long>(Tick.GetValue()),
			*SourceCommandId.ToDebugString());
	}
}
