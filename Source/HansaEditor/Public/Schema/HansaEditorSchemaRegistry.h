#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "UObject/WeakObjectPtr.h"

class FProperty;
class UHansaDefinitionBase;
class UClass;

enum class EHansaSchemaDiagnosticSeverity : uint8
{
	Information,
	Warning,
	Error
};

struct HANSAEDITOR_API FHansaSchemaDiagnostic final
{
	EHansaSchemaDiagnosticSeverity Severity = EHansaSchemaDiagnosticSeverity::Error;
	FString Code;
	FString SchemaId;
	FString PropertyPath;
	FString Cause;
	FString Remedy;
};

struct HANSAEDITOR_API FHansaEditorSchemaProperty final
{
	FString Name;
	FString DisplayName;
	FString Description;
	FString Category;
	FString JsonType;
	FString JsonItemType;
	FString Unit;
	FString ReferenceType;
	FString AIAccess;
	FString Migration;
	FString Serialization;
	FString Validation;
	bool bRequired = false;
	bool bBulkEditable = false;
	bool bReadOnly = false;
	TOptional<double> Minimum;
	TOptional<double> Maximum;
	const FProperty* ReflectedProperty = nullptr;
};

struct HANSAEDITOR_API FHansaDefinitionClassSchema final
{
	FString SchemaId;
	int32 SchemaVersion = 0;
	FString DisplayName;
	TWeakObjectPtr<UClass> DefinitionClass;
	TArray<FHansaEditorSchemaProperty> Properties;
	TArray<FHansaSchemaDiagnostic> Diagnostics;

	[[nodiscard]] bool IsValid() const;
};

/** Discovers reflected definition classes and derives one deterministic schema contract. */
class HANSAEDITOR_API FHansaEditorSchemaRegistry final
{
public:
	void Refresh();

	[[nodiscard]] const TArray<FHansaDefinitionClassSchema>& GetSchemas() const { return Schemas; }
	[[nodiscard]] const TArray<FHansaSchemaDiagnostic>& GetDiagnostics() const { return Diagnostics; }
	[[nodiscard]] const FHansaDefinitionClassSchema* FindSchema(const UClass* DefinitionClass) const;

	[[nodiscard]] FHansaDefinitionClassSchema BuildSchemaForClass(const UClass* DefinitionClass) const;
	[[nodiscard]] FString ExportJsonSchema(const FHansaDefinitionClassSchema& Schema) const;
	bool ExportAllJsonSchemas(const FString& Directory, TArray<FString>& OutFiles, FString& OutError) const;

private:
	TArray<FHansaDefinitionClassSchema> Schemas;
	TArray<FHansaSchemaDiagnostic> Diagnostics;
};
