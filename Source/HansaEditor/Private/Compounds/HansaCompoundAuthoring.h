#pragma once
#include "CoreMinimal.h"
class UHansaBuildingDefinition;
class UHansaResidentialCompoundDefinition;
class UHansaDefinitionBase;
/** Editor-only dry-run migration and impact service. Never expands an occupied legacy parcel. */
namespace Hansa::Editor::Compounds
{
 UHansaBuildingDefinition* CreateBindingDraft(const UHansaBuildingDefinition& Source,UHansaResidentialCompoundDefinition& Compound,const FString& NewStableId,FString& Error);
 TArray<FString> DescribeImpact(const UHansaResidentialCompoundDefinition& Compound,const TArray<UHansaDefinitionBase*>& Definitions);
 bool ExportInterchange(const UHansaResidentialCompoundDefinition& Definition,FString& Json);
 UHansaResidentialCompoundDefinition* ImportDraft(const FString& Json,FString& Error);
}
