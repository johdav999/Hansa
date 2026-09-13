#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class UHansaDefinitionBase;
class UObject;

namespace Hansa::Editor::EconomicDefinitions
{
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> CreateMvpDefinitionSet(UObject* Outer);
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> CreateP33EconomyCandidate(FString& OutError);
	int32 StageP33EconomyCandidate();
	bool SaveMvpDefinitionAssets(bool bReplaceExisting, TArray<FString>& OutSavedFiles, FString& OutError);
	bool MigrateMvpBuildingConstructionCosts(TArray<FString>& OutSavedFiles, FString& OutError);
	bool MigrateEnhancedConstructionCatalog(TArray<FString>& OutSavedFiles, FString& OutError);
}
