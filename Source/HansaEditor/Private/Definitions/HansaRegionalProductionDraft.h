#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class UHansaDefinitionBase;

namespace Hansa::Editor::RegionalProduction
{
	bool ApplyDraft(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions,FString& OutError);
}
