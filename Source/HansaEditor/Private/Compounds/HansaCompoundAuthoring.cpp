#include "Compounds/HansaCompoundAuthoring.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Model/HansaIds.h"
namespace Hansa::Editor::Compounds
{
UHansaBuildingDefinition* CreateBindingDraft(const UHansaBuildingDefinition& Source,UHansaResidentialCompoundDefinition& Compound,const FString& NewId,FString& Error)
{
 Error.Reset();
 if(!Hansa::Simulation::FHansaBuildingTypeId::TryParse(NewId)||NewId==Source.StableDefinitionId)
 {Error=TEXT("Use a new Building.* identity. Existing parcels cannot be expanded by a cosmetic migration.");return nullptr;}
 TArray<FHansaDefinitionValidationIssue> Issues;Compound.ValidateDefinition(Issues);
 if(!Issues.IsEmpty()){Error=Issues[0].Cause.ToString();return nullptr;}
 auto* Draft=DuplicateObject<UHansaBuildingDefinition>(&Source,GetTransientPackage());
 Draft->StableDefinitionId=NewId;Draft->ResidentialCompound=&Compound;Draft->PresentationActorClass.Reset();
 Draft->FootprintWidthCells=Compound.FootprintWidthCells;Draft->FootprintHeightCells=Compound.FootprintHeightCells;
 Draft->CompoundStage=1;Draft->ResidentPopulationTierId=Compound.PopulationTierId;Draft->bRequiresRoad=true;
 // Do not carry an upgrade into a differently sized legacy parcel.
 Draft->UpgradeTargetBuildingId.Reset();Draft->AuthoredRevision=1;Draft->RefreshContentHash();
 return Draft;
}
TArray<FString> DescribeImpact(const UHansaResidentialCompoundDefinition& Compound,const TArray<UHansaDefinitionBase*>& Definitions)
{
 TArray<FString> Result;
 for(const auto* Definition:Definitions)if(const auto* B=Cast<UHansaBuildingDefinition>(Definition))
  if(B->ResidentialCompound.Get()==&Compound||B->ResidentialCompound.ToSoftObjectPath()==FSoftObjectPath(&Compound))
   Result.Add(B->StableDefinitionId+TEXT(": compound hash, footprint, access, stage, terrain fit (15 degrees / 120cm per structure), entrance paths, grass exclusions, save content compatibility and upgrade chain require revalidation."));
 Result.Sort();
 Result.Add(TEXT("An empty legacy binding preserves existing hashes and occupied cells. A larger footprint uses a new building identity; no population multiplier or silent save expansion."));
 return Result;
}
bool ExportInterchange(const UHansaResidentialCompoundDefinition& D,FString& Json)
{
 // Preserve localization namespace/key as well as visible text. Legacy plain-string imports remain supported.
 auto Object=MakeShared<FJsonObject>();
 if(!FJsonObjectConverter::UStructToJsonObject(D.GetClass(),&D,Object,0,CPF_Transient,nullptr,EJsonObjectConversionFlags::WriteTextAsComplexString))return false;
 return FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Json));
}
UHansaResidentialCompoundDefinition* ImportDraft(const FString& Json,FString& Error)
{
 Error.Reset();auto* Draft=NewObject<UHansaResidentialCompoundDefinition>(GetTransientPackage());FText Failure;
 if(!FJsonObjectConverter::JsonObjectStringToUStruct(Json,Draft,0,CPF_Transient,true,&Failure))
 {Error=Failure.ToString();return nullptr;}
 if(Draft->SchemaVersion!=1){Error=TEXT("Unsupported compound interchange version; explicit migration required.");return nullptr;}
 TArray<FHansaDefinitionValidationIssue> Issues;Draft->ValidateDefinition(Issues);
 if(!Issues.IsEmpty()){Error=Issues[0].PropertyPath+TEXT(": ")+Issues[0].Cause.ToString();return nullptr;}
 Draft->RefreshContentHash();return Draft;
}
}
