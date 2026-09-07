#include "Generation/HansaDefinitionProposalReview.h"

#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaResearchDefinitions.h"
#include "Definitions/HansaScenarioDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"
#if WITH_HANSA_AUTOMATION
#include "Fixtures/HansaProductionFixture.h"
#include "Queries/HansaSimulationReadOnly.h"
#endif
#include "Editor.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace Hansa::Editor::Generation
{
	namespace
	{
		FString JsonText(const TSharedPtr<FJsonValue>& Value)
		{
			FString Text;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
			FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
			return Text;
		}

		FString SerializeObject(const TSharedRef<FJsonObject>& Object)
		{
			FString Text;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
			FJsonSerializer::Serialize(Object, Writer);
			return Text;
		}

		bool Sha256(const FString& Text, FString& OutHex)
		{
#if PLATFORM_WINDOWS
			const FTCHARToUTF8 Utf8(*Text);
			BCRYPT_ALG_HANDLE Algorithm = nullptr;
			if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
			uint8 Digest[32] = {};
			const NTSTATUS Status = BCryptHash(Algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<ANSICHAR*>(Utf8.Get())), static_cast<ULONG>(Utf8.Length()), Digest, UE_ARRAY_COUNT(Digest));
			BCryptCloseAlgorithmProvider(Algorithm, 0);
			if (Status < 0) return false;
			static const TCHAR Hex[] = TEXT("0123456789abcdef");
			OutHex.Reset(64);
			for (const uint8 Byte : Digest) { OutHex.AppendChar(Hex[Byte >> 4]); OutHex.AppendChar(Hex[Byte & 0x0f]); }
			return true;
#else
			return false;
#endif
		}

		TSharedRef<FJsonObject> FieldSchema(const FProperty* Property, int32 Depth = 0)
		{
			TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetStringField(TEXT("title"), Property->GetDisplayNameText().ToString());
			Result->SetStringField(TEXT("description"), Property->GetToolTipText().ToString().Left(1024));
			Result->SetStringField(TEXT("x-hansa-reference"), Property->GetMetaData(TEXT("HansaReference")));
			if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				Result->SetStringField(TEXT("type"), TEXT("array"));
				auto Items = FieldSchema(Array->Inner, Depth + 1);
				const FString Reference = Property->GetMetaData(TEXT("HansaReference"));
				if (!Reference.IsEmpty() && Reference != TEXT("None")) Items->SetStringField(TEXT("x-hansa-reference"), Reference);
				Result->SetObjectField(TEXT("items"), Items);
			}
			else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				Result->SetStringField(TEXT("type"), TEXT("object"));
				TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
				TArray<TSharedPtr<FJsonValue>> Required;
				for (TFieldIterator<FProperty> It(Struct->Struct); It && Depth < 6; ++It)
				{
					if (It->GetMetaData(TEXT("HansaSerialization")) == TEXT("Excluded")) continue;
					Properties->SetObjectField(It->GetName(), FieldSchema(*It, Depth + 1));
					Required.Add(MakeShared<FJsonValueString>(It->GetName()));
				}
				Result->SetObjectField(TEXT("properties"), Properties);
				Result->SetArrayField(TEXT("required"), Required);
				Result->SetBoolField(TEXT("additionalProperties"), false);
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
			{
				Result->SetStringField(TEXT("type"), TEXT("string"));
				TArray<TSharedPtr<FJsonValue>> Values;
				for (int32 Index = 0; Index < EnumProperty->GetEnum()->NumEnums() - 1; ++Index) Values.Add(MakeShared<FJsonValueString>(EnumProperty->GetEnum()->GetNameStringByIndex(Index)));
				Result->SetArrayField(TEXT("enum"), Values);
			}
			else if (const FByteProperty* Byte = CastField<FByteProperty>(Property); Byte && Byte->Enum)
			{
				Result->SetStringField(TEXT("type"), TEXT("string"));
				TArray<TSharedPtr<FJsonValue>> Values;
				for (int32 Index = 0; Index < Byte->Enum->NumEnums() - 1; ++Index) Values.Add(MakeShared<FJsonValueString>(Byte->Enum->GetNameStringByIndex(Index)));
				Result->SetArrayField(TEXT("enum"), Values);
			}
			else if (CastField<FBoolProperty>(Property)) Result->SetStringField(TEXT("type"), TEXT("boolean"));
			else if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
			{
				Result->SetStringField(TEXT("type"), Numeric->IsInteger() ? TEXT("integer") : TEXT("number"));
				double Minimum = 0.0, Maximum = 0.0;
				if (LexTryParseString(Minimum, *Property->GetMetaData(TEXT("HansaMin")))) Result->SetNumberField(TEXT("minimum"), Minimum);
				if (LexTryParseString(Maximum, *Property->GetMetaData(TEXT("HansaMax")))) Result->SetNumberField(TEXT("maximum"), Maximum);
			}
			else Result->SetStringField(TEXT("type"), TEXT("string"));
			return Result;
		}

		bool ValidateSchemaValue(const TSharedPtr<FJsonValue>& Value, const TSharedRef<FJsonObject>& Schema)
		{
			if (!Value.IsValid()) return false;
			const FString Type = Schema->GetStringField(TEXT("type"));
			if (Type == TEXT("integer") || Type == TEXT("number"))
			{
				double Number = 0, Limit = 0;
				if (Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false;
				if (Type == TEXT("integer") && (FMath::Abs(Number) > 9007199254740991.0 || FMath::FloorToDouble(Number) != Number)) return false;
				if (Schema->TryGetNumberField(TEXT("minimum"), Limit) && Number < Limit) return false;
				if (Schema->TryGetNumberField(TEXT("maximum"), Limit) && Number > Limit) return false;
				return true;
			}
			if (Type == TEXT("boolean")) return Value->Type == EJson::Boolean;
			if (Type == TEXT("string"))
			{
				if (Value->Type != EJson::String) return false;
				const TArray<TSharedPtr<FJsonValue>>* Enum = nullptr;
				return !Schema->TryGetArrayField(TEXT("enum"), Enum) || Enum->ContainsByPredicate([&Value](const TSharedPtr<FJsonValue>& Item){ return Item->AsString() == Value->AsString(); });
			}
			if (Type == TEXT("array"))
			{
				if (Value->Type != EJson::Array) return false;
				for (const auto& Item : Value->AsArray()) if (!ValidateSchemaValue(Item, Schema->GetObjectField(TEXT("items")).ToSharedRef())) return false;
				return true;
			}
			if (Type == TEXT("object"))
			{
				if (Value->Type != EJson::Object) return false;
				const auto Object = Value->AsObject();
				const auto Properties = Schema->GetObjectField(TEXT("properties"));
				if (!Object.IsValid() || Object->Values.Num() != Properties->Values.Num()) return false;
				for (const auto& Field : Properties->Values)
					if (!ValidateSchemaValue(Object->TryGetField(Field.Key.ToView()), Field.Value->AsObject().ToSharedRef())) return false;
				return true;
			}
			return false;
		}

		bool ValidateReferenceValue(const FProperty* Property, const TSharedPtr<FJsonValue>& Value, const TSet<FString>& Known, FString& OutPath, const FString& Path, const FString& InheritedReference = FString())
		{
			if (!Value.IsValid() || Value->IsNull()) return true;
			if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
			{
				const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
				for (int32 Index = 0; Index < Items.Num(); ++Index) if (!ValidateReferenceValue(Array->Inner, Items[Index], Known, OutPath, FString::Printf(TEXT("%s[%d]"), *Path, Index), Property->GetMetaData(TEXT("HansaReference")))) return false;
				return true;
			}
			if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
			{
				const TSharedPtr<FJsonObject> Object = Value->AsObject();
				if (!Object.IsValid()) { OutPath = Path; return false; }
				for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
				{
					const TSharedPtr<FJsonValue> Child = Object->TryGetField(It->GetName());
					if (Child.IsValid() && !ValidateReferenceValue(*It, Child, Known, OutPath, Path + TEXT(".") + It->GetName())) return false;
				}
				return true;
			}
			FString Reference = Property->GetMetaData(TEXT("HansaReference"));
			if (Reference.IsEmpty()) Reference = InheritedReference;
			if (Reference.IsEmpty() || Reference == TEXT("None") ||
				(!CastField<FStrProperty>(Property) && !CastField<FNameProperty>(Property))) return true;
			FString Candidate;
			if (!Value->TryGetString(Candidate) || (!Candidate.IsEmpty() && !Known.Contains(Candidate))) { OutPath = Path; return false; }
			return true;
		}

		const FHansaEditorSchemaProperty* FindWritable(const FHansaDefinitionClassSchema& Schema, const FString& Name)
		{
			return Schema.Properties.FindByPredicate([&Name](const FHansaEditorSchemaProperty& Item)
			{
				return Item.Name == Name && !Item.bReadOnly && (Item.AIAccess == TEXT("Suggest") || Item.AIAccess == TEXT("Generate"));
			});
		}
		bool IsCompiledRegistryDefinition(const UHansaDefinitionBase* Definition)
		{
			return IsValid(Definition) && (Definition->IsA<UHansaGoodDefinition>() ||
				Definition->IsA<UHansaRecipeDefinition>() || Definition->IsA<UHansaBuildingDefinition>() ||
				Definition->IsA<UHansaNeedDefinition>() || Definition->IsA<UHansaPopulationTierDefinition>() ||
				Definition->IsA<UHansaCityMarketProfileDefinition>() || Definition->IsA<UHansaVehicleDefinition>() ||
				Definition->IsA<UHansaRouteDefinition>() || Definition->IsA<UHansaTechnologyDefinition>() ||
				Definition->IsA<UHansaMerchantAITuningDefinition>() || Definition->IsA<UHansaScenarioObjectiveDefinition>() ||
				Definition->IsA<UHansaVictoryDefinition>() || Definition->IsA<UHansaScenarioDefinition>());
		}

#if WITH_HANSA_AUTOMATION
		bool CaptureFixturePhase(const Hansa::Simulation::FHansaProductionFixture& Fixture, FHansaProposalFixturePhase& OutPhase)
		{
			using namespace Hansa::Simulation;
			const FHansaSimulationReadOnlyAccess ReadOnly = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions());
			const FHansaCityDefinitionId CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
			const FHansaGoodId GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
			const FHansaInventoryId InventoryId = FHansaInventoryId::TryCreate(1).Value;
			const TOptional<FHansaCityMarketProjection> Market = ReadOnly.QueryMarket(CityId, GoodId);
			const TOptional<FHansaInventoryStockProjection> Stock = ReadOnly.GetInventories().QueryStock(InventoryId, GoodId);
			if (!Market.IsSet() || !Stock.IsSet()) return false;
			OutPhase.Tick = ReadOnly.GetClock().GetTick().GetValue();
			OutPhase.StockMilliUnits = Stock->Stock.GetRawValue();
			OutPhase.ReserveMilliUnits = Market->DesiredReserve.GetRawValue();
			OutPhase.PriceMilliMarks = Market->CurrentPriceMilliMarks;
			OutPhase.UnmetDemandMilliUnits = Market->UnmetDemand.GetRawValue();
			OutPhase.StateHash = Fixture.BuildStateHashes().GetOverallHash();
			OutPhase.bShortage = ReadOnly.QueryMarketAlerts(CityId, GoodId).ContainsByPredicate([](const FHansaMarketAlertProjection& Alert)
			{
				return Alert.Type == EHansaMarketAlertType::Shortage;
			});
			return true;
		}

		bool RunGrainShortagePreview(const Hansa::Simulation::FHansaEconomicRegistry& Registry, FHansaProposalFixtureRun& OutRun)
		{
			using namespace Hansa::Simulation;
			const THansaValueResult<FHansaProductionFixture> Created = FHansaProductionFixture::TryCreateGrainShortageWithRegistry(Registry);
			if (!Created) return false;
			FHansaProductionFixture Fixture = Created.Value;
			OutRun.RegistryHash = Fixture.GetRegistryHash();
			const FHansaProductionId RecoveryProduction = FHansaProductionId::TryCreate(10).Value;
			if (!CaptureFixturePhase(Fixture, OutRun.Baseline) || !Fixture.Step(5) ||
				!CaptureFixturePhase(Fixture, OutRun.Shortage) || !Fixture.SetProductionActive(RecoveryProduction, true) ||
				!Fixture.SetProductionActive(RecoveryProduction, false) || !Fixture.Step(3) ||
				!CaptureFixturePhase(Fixture, OutRun.Recovery)) return false;
			OutRun.bRecoveryContractPassed = !OutRun.Baseline.bShortage && OutRun.Shortage.bShortage &&
				!OutRun.Recovery.bShortage && OutRun.Recovery.StockMilliUnits >= OutRun.Recovery.ReserveMilliUnits &&
				OutRun.Shortage.PriceMilliMarks > OutRun.Baseline.PriceMilliMarks &&
				OutRun.Recovery.PriceMilliMarks < OutRun.Shortage.PriceMilliMarks;
			return true;
		}
#endif
	}

	void FHansaDefinitionProposalReview::SetError(FString Code, FString Message, FString Remedy)
	{
		Error = { MoveTemp(Code), MoveTemp(Message), MoveTemp(Remedy) };
	}

	bool FHansaDefinitionProposalReview::BuildContract(const UHansaDefinitionBase* Definition, const FHansaDefinitionClassSchema& Schema,
		const TArray<UHansaDefinitionBase*>& Definitions, TSharedPtr<FJsonObject>& OutContract, FHansaProposalReviewError& OutError)
	{
		OutContract.Reset(); OutError = {};
		if (!IsValid(Definition) || !Schema.IsValid() || Schema.DefinitionClass.Get() != Definition->GetClass()) { OutError = { TEXT("InvalidDefinitionSchema"), TEXT("The selected definition does not match a valid exported schema."), TEXT("Refresh schemas and select the definition again.") }; return false; }
		FHansaEditorSchemaRegistry Registry;
		const FString Exported = Registry.ExportJsonSchema(Schema);
		FString SchemaHash;
		if (!Sha256(Exported, SchemaHash)) { OutError = { TEXT("SchemaHashFailed"), TEXT("The deterministic exported JSON Schema could not be hashed."), FString() }; return false; }
		TSharedRef<FJsonObject> Writable = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> BaseValues = MakeShared<FJsonObject>();
		for (const FHansaEditorSchemaProperty& Field : Schema.Properties)
		{
			if (!Field.bReadOnly && (Field.AIAccess == TEXT("Suggest") || Field.AIAccess == TEXT("Generate")) && Field.ReflectedProperty)
			{
				Writable->SetObjectField(Field.Name, FieldSchema(Field.ReflectedProperty));
				const void* CurrentValue = Field.ReflectedProperty->ContainerPtrToValuePtr<void>(Definition);
				const TSharedPtr<FJsonValue> CurrentJson = FJsonObjectConverter::UPropertyToJsonValue(const_cast<FProperty*>(Field.ReflectedProperty), CurrentValue, 0, 0, nullptr, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
				if (!CurrentJson.IsValid()) { OutError = { TEXT("PropertyExportFailed"), TEXT("Could not serialize AI-writable field: ") + Field.Name, FString() }; return false; }
				BaseValues->SetField(Field.Name, CurrentJson);
			}
		}
		if (Writable->Values.IsEmpty()) { OutError = { TEXT("NoWritableFields"), TEXT("The selected schema has no AI-writable fields."), TEXT("Classify an approved field as Suggest or Generate before requesting a proposal.") }; return false; }
		TArray<FString> StableIds;
		for (const UHansaDefinitionBase* Item : Definitions) if (IsValid(Item) && !Item->StableDefinitionId.IsEmpty()) StableIds.AddUnique(Item->StableDefinitionId);
		StableIds.Sort();
		TArray<TSharedPtr<FJsonValue>> References;
		for (const FString& Id : StableIds) References.Add(MakeShared<FJsonValueString>(Id));
		TSharedRef<FJsonObject> Contract = MakeShared<FJsonObject>();
		Contract->SetNumberField(TEXT("contractVersion"), 1);
		Contract->SetStringField(TEXT("schemaId"), Schema.SchemaId);
		Contract->SetNumberField(TEXT("schemaVersion"), Schema.SchemaVersion);
		Contract->SetStringField(TEXT("sourceSchemaHash"), SchemaHash);
		Contract->SetStringField(TEXT("baseStableId"), Definition->StableDefinitionId);
		Contract->SetNumberField(TEXT("baseRevision"), Definition->AuthoredRevision);
		Contract->SetStringField(TEXT("baseContentHash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Definition->ComputeDeterministicContentHash())));
		Contract->SetObjectField(TEXT("writableFields"), Writable);
		Contract->SetObjectField(TEXT("baseValues"), BaseValues);
		Contract->SetArrayField(TEXT("stableReferences"), References);
		Contract->SetNumberField(TEXT("maxOutputTokens"), 2048);
		OutContract = Contract;
		return true;
	}

	void FHansaDefinitionProposalReview::Reset()
	{
		Target.Reset(); TargetSchema = {}; Diffs.Reset(); Error = {}; PreviewSummary.Reset(); FixturePreview.Reset(); BaseRevision = 0; BaseContentHash = 0; bValidated = false;
	}

	bool FHansaDefinitionProposalReview::Load(UHansaDefinitionBase* Definition, const FHansaDefinitionClassSchema& Schema,
		const TSharedRef<FJsonObject>& IncomingProposal, const TArray<UHansaDefinitionBase*>& Definitions)
	{
		Reset();
		// Own the reviewed bytes: callers cannot mutate a proposal after its diff is shown.
		TSharedPtr<FJsonObject> Proposal;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SerializeObject(IncomingProposal)), Proposal) || !Proposal.IsValid())
		{ SetError(TEXT("SchemaMismatch"), TEXT("The proposal must be a valid JSON object.")); return false; }
		if (!IsValid(Definition) || !Schema.IsValid() || Schema.DefinitionClass.Get() != Definition->GetClass()) { SetError(TEXT("InvalidDefinitionSchema"), TEXT("The selected definition does not match a valid schema.")); return false; }
		FHansaEditorSchemaRegistry Registry;
		FString CurrentSchemaHash;
		if (!Sha256(Registry.ExportJsonSchema(Schema), CurrentSchemaHash)) { SetError(TEXT("SchemaHashFailed"), TEXT("The current deterministic exported JSON Schema could not be hashed.")); return false; }
		const TSet<FString> AllowedRoot = { TEXT("proposalVersion"), TEXT("schemaId"), TEXT("schemaVersion"), TEXT("sourceSchemaHash"), TEXT("baseStableId"), TEXT("baseRevision"), TEXT("baseContentHash"), TEXT("patch") };
		for (const auto& Pair : Proposal->Values) { const FString Key(Pair.Key.ToView()); if (!AllowedRoot.Contains(Key)) { SetError(TEXT("UnknownProposalField"), TEXT("Proposal contains unknown root field: ") + Key); return false; } }
		double ProposalVersion = 0, SchemaVersion = 0, BaseRevisionNumber = 0;
		FString SchemaId, ProposalSchemaHash, StableId, BaseHash;
		const TSharedPtr<FJsonObject>* Patch = nullptr;
		if (!Proposal->TryGetNumberField(TEXT("proposalVersion"), ProposalVersion) || ProposalVersion != 1 ||
			!Proposal->TryGetStringField(TEXT("schemaId"), SchemaId) || SchemaId != Schema.SchemaId ||
			!Proposal->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != static_cast<double>(Schema.SchemaVersion) ||
			!Proposal->TryGetStringField(TEXT("sourceSchemaHash"), ProposalSchemaHash) || ProposalSchemaHash != CurrentSchemaHash ||
			!Proposal->TryGetStringField(TEXT("baseStableId"), StableId) || StableId != Definition->StableDefinitionId ||
			!Proposal->TryGetNumberField(TEXT("baseRevision"), BaseRevisionNumber) || BaseRevisionNumber != static_cast<double>(Definition->AuthoredRevision) ||
			!Proposal->TryGetStringField(TEXT("baseContentHash"), BaseHash) || BaseHash != FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Definition->ComputeDeterministicContentHash())) ||
			!Proposal->TryGetObjectField(TEXT("patch"), Patch) || Patch == nullptr)
		{
			SetError(TEXT("StaleOrMismatchedProposal"), TEXT("Proposal schema hash, identity, revision, or content hash does not match the selected definition."), TEXT("Request a new proposal from the current definition revision.")); return false;
		}
		TSet<FString> Known;
		for (const UHansaDefinitionBase* Item : Definitions) if (IsValid(Item)) Known.Add(Item->StableDefinitionId);
		for (const FHansaEditorSchemaProperty& Field : Schema.Properties)
		{
			if (!Field.bReadOnly && (Field.AIAccess == TEXT("Suggest") || Field.AIAccess == TEXT("Generate")) && !(*Patch)->HasField(Field.Name)) { SetError(TEXT("SchemaMismatch"), TEXT("Strict proposal patch is missing writable field: ") + Field.Name); return false; }
		}
		for (const auto& Pair : (*Patch)->Values)
		{
			const FString Key(Pair.Key.ToView());
			const FHansaEditorSchemaProperty* Field = FindWritable(Schema, Key);
			if (!Field || !Field->ReflectedProperty) { SetError(TEXT("ForbiddenProposalField"), TEXT("Proposal attempted to change non-writable field: ") + Key); return false; }
			if (Pair.Value->IsNull()) continue;
			if (!ValidateSchemaValue(Pair.Value, FieldSchema(Field->ReflectedProperty)))
			{ SetError(TEXT("SchemaMismatch"), TEXT("Proposal value does not match the exact reflected type, range or nested fields: ") + Key); return false; }
			FString BadReference;
			if (!ValidateReferenceValue(Field->ReflectedProperty, Pair.Value, Known, BadReference, Key)) { SetError(TEXT("UnknownStableReference"), TEXT("Proposal contains an unknown stable reference at ") + BadReference); return false; }
			const void* CurrentValue = Field->ReflectedProperty->ContainerPtrToValuePtr<void>(Definition);
			const TSharedPtr<FJsonValue> CurrentJson = FJsonObjectConverter::UPropertyToJsonValue(const_cast<FProperty*>(Field->ReflectedProperty), CurrentValue, 0, 0, nullptr, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
			if (!CurrentJson.IsValid()) { SetError(TEXT("PropertyExportFailed"), TEXT("Could not serialize field ") + Key); return false; }
			FHansaProposalFieldDiff Diff;
			Diff.FieldName = Key; Diff.DisplayName = Field->DisplayName; Diff.BeforeJson = JsonText(CurrentJson); Diff.AfterJson = JsonText(Pair.Value); Diff.ProposedValue = Pair.Value; Diff.bSelected = true;
			Diffs.Add(MakeShared<FHansaProposalFieldDiff>(MoveTemp(Diff)));
		}
		if (Diffs.IsEmpty()) { SetError(TEXT("EmptyProposal"), TEXT("Proposal contains no field changes.")); return false; }
		Target = Definition; TargetSchema = Schema; BaseRevision = Definition->AuthoredRevision; BaseContentHash = Definition->ComputeDeterministicContentHash();
		return true;
	}

	bool FHansaDefinitionProposalReview::SetSelected(const FString& FieldName, const bool bSelected)
	{
		const TSharedPtr<FHansaProposalFieldDiff>* Item = Diffs.FindByPredicate([&FieldName](const TSharedPtr<FHansaProposalFieldDiff>& Value){ return Value->FieldName == FieldName; });
		if (!Item) return false; (*Item)->bSelected = bSelected; bValidated = false; PreviewSummary.Reset(); FixturePreview.Reset(); return true;
	}

	bool FHansaDefinitionProposalReview::HasSelectedChanges() const
	{
		return Diffs.ContainsByPredicate([](const TSharedPtr<FHansaProposalFieldDiff>& Item){ return Item->bSelected; });
	}

	bool FHansaDefinitionProposalReview::IsCurrentBase() const
	{
		return Target.IsValid() && Target->AuthoredRevision == BaseRevision && Target->ComputeDeterministicContentHash() == BaseContentHash;
	}

	bool FHansaDefinitionProposalReview::ApplyTo(UHansaDefinitionBase* Destination, const bool bIncrementRevision)
	{
		for (const TSharedPtr<FHansaProposalFieldDiff>& Diff : Diffs)
		{
			if (!Diff->bSelected) continue;
			const FHansaEditorSchemaProperty* Field = FindWritable(TargetSchema, Diff->FieldName);
			FText Failure;
			if (!Field || !FJsonObjectConverter::JsonValueToUProperty(Diff->ProposedValue, const_cast<FProperty*>(Field->ReflectedProperty), Field->ReflectedProperty->ContainerPtrToValuePtr<void>(Destination), 0, 0, true, &Failure))
			{
				SetError(TEXT("SchemaMismatch"), FString::Printf(TEXT("Field %s does not match its reflected schema: %s"), *Diff->FieldName, *Failure.ToString())); return false;
			}
		}
		if (bIncrementRevision) Destination->AuthoredRevision = BaseRevision + 1;
		Destination->RefreshContentHash();
		return true;
	}

	bool FHansaDefinitionProposalReview::ValidateSelected(const TArray<UHansaDefinitionBase*>& Definitions)
	{
		Error = {}; bValidated = false; PreviewSummary.Reset(); FixturePreview.Reset();
		if (!IsCurrentBase()) { SetError(TEXT("StaleProposal"), TEXT("The definition changed after this proposal was loaded."), TEXT("Discard it and request a proposal from the current revision.")); return false; }
		if (BaseRevision == MAX_int32) { SetError(TEXT("RevisionOverflow"), TEXT("The authored revision cannot be incremented.")); return false; }
		if (!HasSelectedChanges()) { SetError(TEXT("NoSelectedChanges"), TEXT("Select at least one proposed field.")); return false; }
		TStrongObjectPtr<UHansaDefinitionBase> Preview(DuplicateObject<UHansaDefinitionBase>(Target.Get(), GetTransientPackage()));
		if (!Preview.IsValid() || !ApplyTo(Preview.Get(), true)) return false;
		TArray<FHansaDefinitionValidationIssue> Issues;
		Preview->ValidateDefinition(Issues);
		if (Issues.ContainsByPredicate([](const FHansaDefinitionValidationIssue& Item){ return Item.Severity == EHansaDefinitionValidationSeverity::Error; })) { SetError(TEXT("DefinitionValidationFailed"), TEXT("Selected fields fail deterministic definition validation."), Issues[0].Cause.ToString()); return false; }
		TArray<const UHansaDefinitionBase*> PreviewDefinitions;
		TArray<const UHansaDefinitionBase*> CurrentDefinitions;
		for (UHansaDefinitionBase* Item : Definitions)
		{
			if (!IsCompiledRegistryDefinition(Item)) continue;
			CurrentDefinitions.Add(Item);
			PreviewDefinitions.Add(Item == Target.Get() ? Preview.Get() : Item);
		}
		if (IsCompiledRegistryDefinition(Target.Get()) && !PreviewDefinitions.Contains(Preview.Get())) PreviewDefinitions.Add(Preview.Get());
		const FHansaEconomicRegistryCompileResult CurrentCompile = FHansaEconomicDefinitionCompiler::Compile(CurrentDefinitions);
		const FHansaEconomicRegistryCompileResult PreviewCompile = FHansaEconomicDefinitionCompiler::Compile(PreviewDefinitions);
		if (!CurrentCompile.IsValid()) { SetError(TEXT("CurrentRegistryValidationFailed"), TEXT("The accepted registry must be valid before proposal comparison."), CurrentCompile.Issues.IsEmpty() ? FString() : CurrentCompile.Issues[0].Cause.ToString()); return false; }
		if (!PreviewCompile.IsValid()) { SetError(TEXT("TemporaryRegistryValidationFailed"), TEXT("Selected fields fail temporary-registry validation."), PreviewCompile.Issues.IsEmpty() ? FString() : PreviewCompile.Issues[0].Cause.ToString()); return false; }
		int32 SelectedCount = 0;
		for (const TSharedPtr<FHansaProposalFieldDiff>& Item : Diffs) if (Item->bSelected) ++SelectedCount;
		PreviewSummary = FString::Printf(TEXT("Temporary registry valid · definition %016llx → %016llx · registry %016llx → %016llx · %d selected field(s)"),
			static_cast<unsigned long long>(BaseContentHash), static_cast<unsigned long long>(Preview->ComputeDeterministicContentHash()),
			static_cast<unsigned long long>(CurrentCompile.Registry.GetRegistryHash()), static_cast<unsigned long long>(PreviewCompile.Registry.GetRegistryHash()), SelectedCount);
#if WITH_HANSA_AUTOMATION
		if (Target->IsA<UHansaRecipeDefinition>() || Target->IsA<UHansaTechnologyDefinition>())
		{
			FHansaProposalFixtureComparison Comparison;
			if (!RunGrainShortagePreview(CurrentCompile.Registry, Comparison.Current) ||
				!RunGrainShortagePreview(PreviewCompile.Registry, Comparison.Proposed))
			{
				SetError(TEXT("FixturePreviewFailed"), TEXT("lubeck_grain_shortage_v1 could not run against both current and proposed registries."),
					TEXT("Restore the canonical MVP fixture dependencies or reject the incompatible proposed fields."));
				return false;
			}
			FixturePreview = Comparison;
			PreviewSummary += FString::Printf(
				TEXT("\nlubeck_grain_shortage_v1 · baseline stock %lld → %lld, price %lld → %lld")
				TEXT(" · shortage stock %lld → %lld, price %lld → %lld, unmet %lld → %lld")
				TEXT(" · recovery stock %lld → %lld, price %lld → %lld · state %016llx → %016llx · contract %s → %s"),
				static_cast<long long>(Comparison.Current.Baseline.StockMilliUnits), static_cast<long long>(Comparison.Proposed.Baseline.StockMilliUnits),
				static_cast<long long>(Comparison.Current.Baseline.PriceMilliMarks), static_cast<long long>(Comparison.Proposed.Baseline.PriceMilliMarks),
				static_cast<long long>(Comparison.Current.Shortage.StockMilliUnits), static_cast<long long>(Comparison.Proposed.Shortage.StockMilliUnits),
				static_cast<long long>(Comparison.Current.Shortage.PriceMilliMarks), static_cast<long long>(Comparison.Proposed.Shortage.PriceMilliMarks),
				static_cast<long long>(Comparison.Current.Shortage.UnmetDemandMilliUnits), static_cast<long long>(Comparison.Proposed.Shortage.UnmetDemandMilliUnits),
				static_cast<long long>(Comparison.Current.Recovery.StockMilliUnits), static_cast<long long>(Comparison.Proposed.Recovery.StockMilliUnits),
				static_cast<long long>(Comparison.Current.Recovery.PriceMilliMarks), static_cast<long long>(Comparison.Proposed.Recovery.PriceMilliMarks),
				static_cast<unsigned long long>(Comparison.Current.Recovery.StateHash), static_cast<unsigned long long>(Comparison.Proposed.Recovery.StateHash),
				Comparison.Current.bRecoveryContractPassed ? TEXT("pass") : TEXT("fail"), Comparison.Proposed.bRecoveryContractPassed ? TEXT("pass") : TEXT("fail"));
		}
#endif
		bValidated = true; return true;
	}

	bool FHansaDefinitionProposalReview::ApplySelected(const TArray<UHansaDefinitionBase*>& Definitions)
	{
		if (!ValidateSelected(Definitions) || !IsCurrentBase()) return false;
		FScopedTransaction Transaction(NSLOCTEXT("HansaDefinitionProposal", "Apply", "Apply reviewed AI definition proposal"));
		Target->Modify();
		if (!ApplyTo(Target.Get(), true)) { Transaction.Cancel(); return false; }
		Target->PostEditChange();
		Target->MarkPackageDirty();
		BaseRevision = Target->AuthoredRevision; BaseContentHash = Target->ComputeDeterministicContentHash();
		bValidated = false;
		PreviewSummary = TEXT("Applied selected fields in one editor transaction. Undo and redo remain available.");
		return true;
	}
}
