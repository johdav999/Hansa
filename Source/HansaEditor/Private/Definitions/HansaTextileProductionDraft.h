#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "UObject/StrongObjectPtr.h"

namespace Hansa::Editor::TextileProduction
{
// Edits caller-owned transient copies only. The reviewed MVP registry is never mutated.
bool ApplyDraft(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, FString& OutError);
}
