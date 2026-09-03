#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class UHansaDefinitionBase;
class UObject;

namespace Hansa::Editor::EconomicDefinitions
{
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> CreateMvpDefinitionSet(UObject* Outer);
	bool SaveMvpDefinitionAssets(bool bReplaceExisting, TArray<FString>& OutSavedFiles, FString& OutError);
	bool MigrateMvpBuildingConstructionCosts(TArray<FString>& OutSavedFiles, FString& OutError);
}
