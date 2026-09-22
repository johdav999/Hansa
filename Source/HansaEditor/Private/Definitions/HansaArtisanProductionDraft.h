#pragma once
#include "Definitions/HansaDefinitionBase.h"
#include "UObject/StrongObjectPtr.h"

namespace Hansa::Editor::ArtisanProduction
{
// Edits only caller-owned transient copies. Never mutates the accepted registry.
bool ApplyDraft(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, FString& OutError);
}
