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
		case EHansaDomainEventType::ResearchQueued: return TEXT("ResearchQueued");
		case EHansaDomainEventType::ResearchCompleted: return TEXT("ResearchCompleted");
		default: return TEXT("UnknownDomainEvent");
		}
	}

	FString FHansaDomainEvent::ToDebugString() const
	{
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
