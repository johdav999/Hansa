#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "HansaCityTerrainToolset.generated.h"

/** Bounded city-survey authoring. No arbitrary paths, code execution, or production writes. */
UCLASS()
class UHansaCityTerrainToolset : public UToolsetDefinition
{

	GENERATED_BODY()
public:
	/** Read project identity, current map, PIE and ALL dirty packages, including external actors. */
	UFUNCTION(meta=(AICallable))
	static FString InspectAuthoringContext();

	/** Import only the pinned P31 Rostock survey into a NEW staged World Partition map.
	 * Refuses unsaved work, PIE, changed source files and existing destinations. Does not promote.
	 */
	UFUNCTION(meta=(AICallable))
	static FString ImportRostockSurveyDraft();

	/** Read back the loaded staged Rostock Landscape, measured edit layer and georeference. */
	UFUNCTION(meta=(AICallable))
	static FString InspectRostockSurveyDraft();
};
