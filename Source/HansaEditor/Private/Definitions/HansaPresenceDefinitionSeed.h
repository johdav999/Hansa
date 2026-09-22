#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class UHansaDefinitionBase;
class UObject;

namespace Hansa::Editor::EconomicDefinitions
{
	void AppendTradePresenceDefinitions(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer);
}
