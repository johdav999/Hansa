#include "Schema/HansaEditorSchemaRegistry.h"

#include "Definitions/HansaDefinitionBase.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace Hansa::Editor::Schema
{
	const TArray<FName> RequiredPropertyMetadata = {
		TEXT("DisplayName"),
		TEXT("ToolTip"),
		TEXT("Category"),
		TEXT("HansaRequired"),
		TEXT("HansaReference"),
		TEXT("HansaBulkEditable"),
		TEXT("HansaAIAccess"),
		TEXT("HansaMigration"),
		TEXT("HansaSerialization"),
		TEXT("HansaValidation")
	};

	bool IsAllowed(const FString& Value, const std::initializer_list<const TCHAR*>& Allowed)
	{
		for (const TCHAR* Candidate : Allowed)
		{
			if (Value == Candidate)
			{
				return true;
			}
		}
		return false;
	}

	bool IsNumericProperty(const FProperty* Property)
	{
		return CastField<FNumericProperty>(Property) != nullptr;
	}

	FString JsonTypeForProperty(const FProperty* Property)
	{
		if (CastField<FBoolProperty>(Property) != nullptr)
		{
			return TEXT("boolean");
		}
		if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
		{
			return Numeric->IsInteger() ? TEXT("integer") : TEXT("number");
		}
		if (CastField<FStrProperty>(Property) != nullptr ||
			CastField<FNameProperty>(Property) != nullptr ||
			CastField<FTextProperty>(Property) != nullptr ||
			CastField<FObjectPropertyBase>(Property) != nullptr ||
			CastField<FSoftObjectProperty>(Property) != nullptr ||
			CastField<FSoftClassProperty>(Property) != nullptr)
		{
			return TEXT("string");
		}
		if (CastField<FArrayProperty>(Property) != nullptr || CastField<FSetProperty>(Property) != nullptr)
		{
			return TEXT("array");
		}
		if (CastField<FMapProperty>(Property) != nullptr || CastField<FStructProperty>(Property) != nullptr)
		{
			return TEXT("object");
		}
		return TEXT("string");
	}

	FString JsonItemTypeForProperty(const FProperty* Property)
	{
		if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			return JsonTypeForProperty(Array->Inner);
		}
		if (const FSetProperty* Set = CastField<FSetProperty>(Property))
		{
			return JsonTypeForProperty(Set->ElementProp);
		}
		return FString();
	}

	bool ParseBooleanMetadata(const FProperty* Property, const FName Key)
	{
		return Property->GetMetaData(Key).Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	void AddDiagnostic(
		TArray<FHansaSchemaDiagnostic>& OutDiagnostics,
		const FString& Code,
		const FString& SchemaId,
		const FString& PropertyPath,
		const FString& Cause,
		const FString& Remedy)
	{
		OutDiagnostics.Add(FHansaSchemaDiagnostic {
			EHansaSchemaDiagnosticSeverity::Error,
			Code,
			SchemaId,
			PropertyPath,
			Cause,
			Remedy
		});
	}

	void CollectDefinitionProperties(const UClass* DefinitionClass, TArray<const FProperty*>& OutProperties)
	{
		for (const UClass* Cursor = DefinitionClass;
			Cursor != nullptr && Cursor->IsChildOf(UHansaDefinitionBase::StaticClass());
			Cursor = Cursor->GetSuperClass())
		{
			for (TFieldIterator<FProperty> It(Cursor, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				OutProperties.Add(*It);
			}
		}
		OutProperties.Sort([](const FProperty& Left, const FProperty& Right)
		{
			return Left.GetName().Compare(Right.GetName(), ESearchCase::CaseSensitive) < 0;
		});
	}
}

bool FHansaDefinitionClassSchema::IsValid() const
{
	return !SchemaId.IsEmpty() && SchemaVersion > 0 && DefinitionClass.IsValid() &&
		!Diagnostics.ContainsByPredicate([](const FHansaSchemaDiagnostic& Diagnostic)
		{
			return Diagnostic.Severity == EHansaSchemaDiagnosticSeverity::Error;
		});
}

void FHansaEditorSchemaRegistry::Refresh()
{
	Schemas.Reset();
	Diagnostics.Reset();

	TArray<UClass*> DefinitionClasses;
	GetDerivedClasses(UHansaDefinitionBase::StaticClass(), DefinitionClasses, true);
	DefinitionClasses.RemoveAll([](const UClass* Class)
	{
		return Class == nullptr || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists);
	});
	DefinitionClasses.Sort([](const UClass& Left, const UClass& Right)
	{
		return Left.GetPathName().Compare(Right.GetPathName(), ESearchCase::CaseSensitive) < 0;
	});

	for (const UClass* DefinitionClass : DefinitionClasses)
	{
		FHansaDefinitionClassSchema Schema = BuildSchemaForClass(DefinitionClass);
		Diagnostics.Append(Schema.Diagnostics);
		Schemas.Add(MoveTemp(Schema));
	}

	Schemas.Sort([](const FHansaDefinitionClassSchema& Left, const FHansaDefinitionClassSchema& Right)
	{
		return Left.SchemaId.Compare(Right.SchemaId, ESearchCase::CaseSensitive) < 0;
	});
}

const FHansaDefinitionClassSchema* FHansaEditorSchemaRegistry::FindSchema(const UClass* DefinitionClass) const
{
	return Schemas.FindByPredicate([DefinitionClass](const FHansaDefinitionClassSchema& Schema)
	{
		return Schema.DefinitionClass.Get() == DefinitionClass;
	});
}

FHansaDefinitionClassSchema FHansaEditorSchemaRegistry::BuildSchemaForClass(const UClass* DefinitionClass) const
{
	using namespace Hansa::Editor::Schema;
	FHansaDefinitionClassSchema Result;
	if (DefinitionClass == nullptr || !DefinitionClass->IsChildOf(UHansaDefinitionBase::StaticClass()))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("HSA-SCHEMA-001"), FString(), FString(),
			TEXT("The requested class is not a Hansa definition."),
			TEXT("Derive authoring definitions from UHansaDefinitionBase."));
		return Result;
	}

	Result.DefinitionClass = const_cast<UClass*>(DefinitionClass);
	Result.SchemaId = DefinitionClass->GetMetaData(TEXT("HansaSchemaId"));
	Result.DisplayName = DefinitionClass->GetDisplayNameText().ToString();
	LexTryParseString(Result.SchemaVersion, *DefinitionClass->GetMetaData(TEXT("HansaSchemaVersion")));
	if (Result.SchemaId.IsEmpty() || Result.SchemaVersion < 1)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("HSA-SCHEMA-002"), Result.SchemaId, FString(),
			TEXT("Definition class is missing HansaSchemaId or a positive HansaSchemaVersion."),
			TEXT("Add stable class-level schema identity and version metadata."));
	}

	TArray<const FProperty*> Properties;
	CollectDefinitionProperties(DefinitionClass, Properties);
	for (const FProperty* Property : Properties)
	{
		const FString PropertyPath = Property->GetName();
		for (const FName Key : RequiredPropertyMetadata)
		{
			if (!Property->HasMetaData(Key))
			{
				AddDiagnostic(Result.Diagnostics, TEXT("HSA-SCHEMA-003"), Result.SchemaId, PropertyPath,
					FString::Printf(TEXT("Reflected property '%s' is missing required '%s' metadata."), *PropertyPath, *Key.ToString()),
					TEXT("Classify the field for generic editing, validation, serialization, migration and AI access."));
			}
		}

		if (IsNumericProperty(Property))
		{
			for (const FName Key : { FName(TEXT("HansaUnit")), FName(TEXT("HansaMin")), FName(TEXT("HansaMax")) })
			{
				if (!Property->HasMetaData(Key))
				{
					AddDiagnostic(Result.Diagnostics, TEXT("HSA-SCHEMA-004"), Result.SchemaId, PropertyPath,
						FString::Printf(TEXT("Numeric property '%s' is missing required '%s' metadata."), *PropertyPath, *Key.ToString()),
						TEXT("Declare the authored unit and deterministic inclusive numeric range."));
				}
			}
		}

		FHansaEditorSchemaProperty Descriptor;
		Descriptor.Name = PropertyPath;
		Descriptor.DisplayName = Property->GetDisplayNameText().ToString();
		Descriptor.Description = Property->GetToolTipText().ToString();
		Descriptor.Category = Property->GetMetaData(TEXT("Category"));
		Descriptor.JsonType = JsonTypeForProperty(Property);
		Descriptor.JsonItemType = JsonItemTypeForProperty(Property);
		Descriptor.Unit = Property->GetMetaData(TEXT("HansaUnit"));
		Descriptor.ReferenceType = Property->GetMetaData(TEXT("HansaReference"));
		Descriptor.AIAccess = Property->GetMetaData(TEXT("HansaAIAccess"));
		Descriptor.Migration = Property->GetMetaData(TEXT("HansaMigration"));
		Descriptor.Serialization = Property->GetMetaData(TEXT("HansaSerialization"));
		Descriptor.Validation = Property->GetMetaData(TEXT("HansaValidation"));
		Descriptor.bRequired = ParseBooleanMetadata(Property, TEXT("HansaRequired"));
		Descriptor.bBulkEditable = ParseBooleanMetadata(Property, TEXT("HansaBulkEditable"));
		Descriptor.bReadOnly = !Property->HasAnyPropertyFlags(CPF_Edit) || Descriptor.Serialization != TEXT("Included");
		Descriptor.ReflectedProperty = Property;

		double Minimum = 0.0;
		if (LexTryParseString(Minimum, *Property->GetMetaData(TEXT("HansaMin"))))
		{
			Descriptor.Minimum = Minimum;
		}
		double Maximum = 0.0;
		if (LexTryParseString(Maximum, *Property->GetMetaData(TEXT("HansaMax"))))
		{
			Descriptor.Maximum = Maximum;
		}

		if (!IsAllowed(Descriptor.AIAccess, { TEXT("Never"), TEXT("Read"), TEXT("Suggest"), TEXT("Generate") }) ||
			!IsAllowed(Descriptor.Migration, { TEXT("Identity"), TEXT("Compatible"), TEXT("RequiresMigration"), TEXT("Derived") }) ||
			!IsAllowed(Descriptor.Serialization, { TEXT("Included"), TEXT("Derived"), TEXT("Excluded") }) ||
			Descriptor.ReferenceType.IsEmpty() || Descriptor.Validation.IsEmpty())
		{
			AddDiagnostic(Result.Diagnostics, TEXT("HSA-SCHEMA-005"), Result.SchemaId, PropertyPath,
				FString::Printf(TEXT("Reflected property '%s' contains an invalid or empty authoring classification."), *PropertyPath),
				TEXT("Use an approved AI, migration, serialization, reference and validation classification."));
		}

		Result.Properties.Add(MoveTemp(Descriptor));
	}

	return Result;
}

FString FHansaEditorSchemaRegistry::ExportJsonSchema(const FHansaDefinitionClassSchema& Schema) const
{
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("$schema"), TEXT("https://json-schema.org/draft/2020-12/schema"));
	Writer->WriteValue(TEXT("$id"), FString::Printf(TEXT("hansa://schema/%s/v%d"), *Schema.SchemaId, Schema.SchemaVersion));
	Writer->WriteValue(TEXT("title"), Schema.DisplayName);
	Writer->WriteValue(TEXT("type"), TEXT("object"));
	Writer->WriteValue(TEXT("additionalProperties"), false);
	Writer->WriteValue(TEXT("x-hansa-schema-version"), Schema.SchemaVersion);
	Writer->WriteObjectStart(TEXT("properties"));
	for (const FHansaEditorSchemaProperty& Property : Schema.Properties)
	{
		Writer->WriteObjectStart(Property.Name);
		Writer->WriteValue(TEXT("title"), Property.DisplayName);
		Writer->WriteValue(TEXT("description"), Property.Description);
		Writer->WriteValue(TEXT("type"), Property.JsonType);
		Writer->WriteValue(TEXT("x-hansa-category"), Property.Category);
		Writer->WriteValue(TEXT("x-hansa-reference"), Property.ReferenceType);
		Writer->WriteValue(TEXT("x-hansa-ai-access"), Property.AIAccess);
		Writer->WriteValue(TEXT("x-hansa-bulk-editable"), Property.bBulkEditable);
		Writer->WriteValue(TEXT("x-hansa-migration"), Property.Migration);
		Writer->WriteValue(TEXT("x-hansa-serialization"), Property.Serialization);
		Writer->WriteValue(TEXT("x-hansa-validation"), Property.Validation);
		if (!Property.Unit.IsEmpty())
		{
			Writer->WriteValue(TEXT("x-hansa-unit"), Property.Unit);
		}
		if (Property.Minimum.IsSet())
		{
			Writer->WriteValue(TEXT("minimum"), Property.Minimum.GetValue());
		}
		if (Property.Maximum.IsSet())
		{
			Writer->WriteValue(TEXT("maximum"), Property.Maximum.GetValue());
		}
		if (Property.JsonType == TEXT("array"))
		{
			Writer->WriteObjectStart(TEXT("items"));
			Writer->WriteValue(TEXT("type"), Property.JsonItemType.IsEmpty() ? TEXT("string") : Property.JsonItemType);
			Writer->WriteObjectEnd();
		}
		if (Property.bReadOnly)
		{
			Writer->WriteValue(TEXT("readOnly"), true);
		}
		Writer->WriteObjectEnd();
	}
	Writer->WriteObjectEnd();
	Writer->WriteArrayStart(TEXT("required"));
	for (const FHansaEditorSchemaProperty& Property : Schema.Properties)
	{
		if (Property.bRequired)
		{
			Writer->WriteValue(Property.Name);
		}
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	Json.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Json += TEXT("\n");
	return Json;
}

bool FHansaEditorSchemaRegistry::ExportAllJsonSchemas(
	const FString& Directory,
	TArray<FString>& OutFiles,
	FString& OutError) const
{
	OutFiles.Reset();
	OutError.Reset();
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("Could not create schema export directory: %s"), *Directory);
		return false;
	}

	for (const FHansaDefinitionClassSchema& Schema : Schemas)
	{
		if (!Schema.IsValid())
		{
			OutError = FString::Printf(TEXT("Cannot export invalid schema: %s"), *Schema.SchemaId);
			return false;
		}
		FString FileName = Schema.SchemaId.Replace(TEXT("."), TEXT("_"));
		FileName += TEXT(".schema.json");
		const FString Path = FPaths::Combine(Directory, FileName);
		if (!FFileHelper::SaveStringToFile(ExportJsonSchema(Schema), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("Could not write schema: %s"), *Path);
			return false;
		}
		OutFiles.Add(Path);
	}
	return true;
}
