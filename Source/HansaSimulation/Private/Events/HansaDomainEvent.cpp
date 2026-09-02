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
		default: return TEXT("UnknownDomainEvent");
		}
	}

	FString FHansaDomainEvent::ToDebugString() const
	{
		return FString::Printf(
			TEXT("DomainEvent[type=%s;sequence=%llu;tick=%lld;command=%s]"),
			LexToString(Type),
			static_cast<unsigned long long>(GlobalSequence),
			static_cast<long long>(Tick.GetValue()),
			*SourceCommandId.ToDebugString());
	}
}
