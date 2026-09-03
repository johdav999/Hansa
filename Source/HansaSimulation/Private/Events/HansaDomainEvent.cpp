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
		return FString::Printf(
			TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;command=%s]"),
			LexToString(Type),
			static_cast<unsigned long long>(GlobalSequence),
			static_cast<long long>(Tick.GetValue()),
			*SourceCommandId.ToDebugString());
	}
}
