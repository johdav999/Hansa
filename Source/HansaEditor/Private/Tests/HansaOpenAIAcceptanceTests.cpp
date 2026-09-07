#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Editor.h"
#include "Generation/HansaDefinitionProposalReview.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

using namespace Hansa::Editor::Generation;

namespace Hansa::Editor::OpenAIAcceptance
{
	TArray<UHansaDefinitionBase*> Mutable(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<UHansaDefinitionBase*> Result;
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions) Result.Add(Definition.Get());
		return Result;
	}

	TArray<const UHansaDefinitionBase*> Const(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<const UHansaDefinitionBase*> Result;
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions) Result.Add(Definition.Get());
		return Result;
	}

	UHansaDefinitionBase* Find(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, const FString& StableId)
	{
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions)
		{
			if (Definition->StableDefinitionId == StableId) return Definition.Get();
		}
		return nullptr;
	}

	TSharedRef<FJsonObject> Proposal(const UHansaDefinitionBase* Definition, const FHansaDefinitionClassSchema& Schema,
		const TSharedRef<FJsonObject>& Contract, const TSharedRef<FJsonObject>& Changes)
	{
		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* Writable = nullptr;
		Contract->TryGetObjectField(TEXT("writableFields"), Writable);
		for (const auto& Pair : (*Writable)->Values) Patch->SetField(Pair.Key.ToView(), MakeShared<FJsonValueNull>());
		for (const auto& Pair : Changes->Values) Patch->SetField(Pair.Key.ToView(), Pair.Value);
		FString SourceSchemaHash;
		Contract->TryGetStringField(TEXT("sourceSchemaHash"), SourceSchemaHash);
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("proposalVersion"), 1);
		Result->SetStringField(TEXT("schemaId"), Schema.SchemaId);
		Result->SetNumberField(TEXT("schemaVersion"), Schema.SchemaVersion);
		Result->SetStringField(TEXT("sourceSchemaHash"), SourceSchemaHash);
		Result->SetStringField(TEXT("baseStableId"), Definition->StableDefinitionId);
		Result->SetNumberField(TEXT("baseRevision"), Definition->AuthoredRevision);
		Result->SetStringField(TEXT("baseContentHash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Definition->ComputeDeterministicContentHash())));
		Result->SetObjectField(TEXT("patch"), Patch);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaOpenAIAuthoringAcceptanceTest,
	"Hansa.Integration.Authoring.OpenAIAcceptanceFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaOpenAIAuthoringAcceptanceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::OpenAIAcceptance;
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> OwnedDefinitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	TArray<UHansaDefinitionBase*> Definitions = Mutable(OwnedDefinitions);
	UHansaRecipeDefinition* Recipe = Cast<UHansaRecipeDefinition>(Find(OwnedDefinitions, TEXT("Recipe.MillFlour")));
	TestNotNull(TEXT("Acceptance recipe exists"), Recipe);
	if (Recipe == nullptr) return false;
	Recipe->SetFlags(RF_Transactional);
	const int32 InitialCycleTicks = Recipe->CycleTicks;
	const int32 InitialLaborers = Recipe->LaborerWorkforce;
	const int32 InitialRevision = Recipe->AuthoredRevision;
	const FHansaEconomicRegistryCompileResult InitialCompile = FHansaEconomicDefinitionCompiler::Compile(Const(OwnedDefinitions));
	TestTrue(TEXT("Accepted registry compiles before proposal"), InitialCompile.IsValid());

	FHansaEditorSchemaRegistry SchemaRegistry;
	const FHansaDefinitionClassSchema Schema = SchemaRegistry.BuildSchemaForClass(Recipe->GetClass());
	TSharedPtr<FJsonObject> Contract;
	FHansaProposalReviewError ContractError;
	TestTrue(TEXT("Bounded recipe contract builds"), FHansaDefinitionProposalReview::BuildContract(Recipe, Schema, Definitions, Contract, ContractError));
	if (!Contract.IsValid()) return false;
	const TSharedPtr<FJsonObject>* BaseValues = nullptr;
	TestTrue(TEXT("Contract carries current allowlisted values"), Contract->TryGetObjectField(TEXT("baseValues"), BaseValues) && BaseValues != nullptr);
	const TArray<TSharedPtr<FJsonValue>>* BaseInputs = nullptr;
	TestTrue(TEXT("Contract carries nested recipe inputs"), BaseValues != nullptr && (*BaseValues)->TryGetArrayField(TEXT("Inputs"), BaseInputs) && BaseInputs != nullptr && !BaseInputs->IsEmpty());
	if (BaseInputs != nullptr && !BaseInputs->IsEmpty())
	{
		const TSharedPtr<FJsonObject> BaseInput = (*BaseInputs)[0]->AsObject();
		FString BaseInputJson;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BaseInputJson);
		FJsonSerializer::Serialize(BaseInput.ToSharedRef(), Writer);
		TestTrue(TEXT("Nested base JSON uses exact schema field GoodId"), BaseInputJson.Contains(TEXT("\"GoodId\""), ESearchCase::CaseSensitive));
		TestTrue(TEXT("Nested base JSON uses exact schema field QuantityMilliUnits"), BaseInputJson.Contains(TEXT("\"QuantityMilliUnits\""), ESearchCase::CaseSensitive));
		TestFalse(TEXT("Nested base JSON does not use standardized goodId"), BaseInputJson.Contains(TEXT("\"goodId\""), ESearchCase::CaseSensitive));
		TestFalse(TEXT("Nested base JSON does not use standardized quantityMilliUnits"), BaseInputJson.Contains(TEXT("\"quantityMilliUnits\""), ESearchCase::CaseSensitive));
	}

	TSharedRef<FJsonObject> Changes = MakeShared<FJsonObject>();
	Changes->SetNumberField(TEXT("CycleTicks"), InitialCycleTicks + 1);
	Changes->SetNumberField(TEXT("LaborerWorkforce"), InitialLaborers + 1);
	TSharedRef<FJsonObject> ProposalJson = Proposal(Recipe, Schema, Contract.ToSharedRef(), Changes);
	const FString EvidenceRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_AUTHORING_ACCEPTANCE_ROOT"));
	const FString WorkerProposalPath = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_AUTHORING_ACCEPTANCE_PROPOSAL"));
	if (!EvidenceRoot.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*EvidenceRoot, true);
		FString ContractJson;
		FJsonSerializer::Serialize(Contract.ToSharedRef(), TJsonWriterFactory<>::Create(&ContractJson));
		if (!TestTrue(TEXT("Export real native contract for mock worker"), FFileHelper::SaveStringToFile(ContractJson,
			*FPaths::Combine(EvidenceRoot, TEXT("editor-contract.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) return false;
	}
	if (!WorkerProposalPath.IsEmpty())
	{
		FString WorkerJson;
		TSharedPtr<FJsonObject> WorkerProposal;
		if (!TestTrue(TEXT("Read worker-produced proposal"), FFileHelper::LoadFileToString(WorkerJson, *WorkerProposalPath)) ||
			!TestTrue(TEXT("Worker output is a JSON object"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WorkerJson), WorkerProposal) && WorkerProposal.IsValid())) return false;
		ProposalJson = WorkerProposal.ToSharedRef();
	}

	FHansaDefinitionProposalReview InvalidReferenceReview;
	TSharedRef<FJsonObject> InvalidReferenceChanges = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> InvalidAmount = MakeShared<FJsonObject>();
	InvalidAmount->SetStringField(TEXT("GoodId"), TEXT("Good.DoesNotExist"));
	InvalidAmount->SetNumberField(TEXT("QuantityMilliUnits"), 1000);
	InvalidReferenceChanges->SetArrayField(TEXT("Inputs"), { MakeShared<FJsonValueObject>(InvalidAmount) });
	TestFalse(TEXT("Unknown proposal reference is rejected"), InvalidReferenceReview.Load(Recipe, Schema,
		Proposal(Recipe, Schema, Contract.ToSharedRef(), InvalidReferenceChanges), Definitions));
	TestEqual(TEXT("Unknown reference has structured code"), InvalidReferenceReview.GetError().Code, FString(TEXT("UnknownStableReference")));

	FHansaDefinitionProposalReview ForbiddenReview;
	TSharedRef<FJsonObject> Forbidden = Proposal(Recipe, Schema, Contract.ToSharedRef(), Changes);
	const TSharedPtr<FJsonObject>* ForbiddenPatch = nullptr;
	Forbidden->TryGetObjectField(TEXT("patch"), ForbiddenPatch);
	(*ForbiddenPatch)->SetStringField(TEXT("StableDefinitionId"), TEXT("Recipe.ReplacedIdentity"));
	TestFalse(TEXT("Forbidden identity field is rejected"), ForbiddenReview.Load(Recipe, Schema, Forbidden, Definitions));
	TestEqual(TEXT("Forbidden field has structured code"), ForbiddenReview.GetError().Code, FString(TEXT("ForbiddenProposalField")));

	FHansaDefinitionProposalReview MalformedReview;
	TestFalse(TEXT("Malformed response envelope is rejected"), MalformedReview.Load(Recipe, Schema, MakeShared<FJsonObject>(), Definitions));
	TestEqual(TEXT("Malformed response has structured code"), MalformedReview.GetError().Code, FString(TEXT("StaleOrMismatchedProposal")));

	FHansaDefinitionProposalReview StaleReview;
	TestTrue(TEXT("Stale drill starts from valid worker proposal"), StaleReview.Load(Recipe, Schema, ProposalJson, Definitions));
	Recipe->CycleTicks = InitialCycleTicks + 2;
	Recipe->RefreshContentHash();
	TestFalse(TEXT("Concurrent edit blocks Apply"), StaleReview.ApplySelected(Definitions));
	TestEqual(TEXT("Stale Apply preserves concurrent edit"), Recipe->CycleTicks, InitialCycleTicks + 2);
	Recipe->CycleTicks = InitialCycleTicks;
	Recipe->RefreshContentHash();

	FHansaDefinitionProposalReview Review;
	TestTrue(TEXT("Two-field recipe proposal loads"), Review.Load(Recipe, Schema, ProposalJson, Definitions));
	TestEqual(TEXT("Two proposed fields are shown"), Review.GetDiffs().Num(), 2);
	TestTrue(TEXT("Designer rejects workforce field"), Review.SetSelected(TEXT("LaborerWorkforce"), false));
	TestTrue(TEXT("Designer accepts cycle field"), Review.SetSelected(TEXT("CycleTicks"), true));
	TestTrue(TEXT("Selected recipe patch validates"), Review.ValidateSelected(Definitions));
	TestTrue(TEXT("Lubeck fixture comparison is available"), Review.GetFixturePreview().IsSet());
	if (!Review.GetFixturePreview().IsSet()) return false;
	const FHansaProposalFixtureComparison Preview = Review.GetFixturePreview().GetValue();
	TestEqual(TEXT("Current fixture uses accepted registry"), Preview.Current.RegistryHash, InitialCompile.Registry.GetRegistryHash());
	TestNotEqual(TEXT("Proposal changes deterministic registry hash"), Preview.Proposed.RegistryHash, Preview.Current.RegistryHash);
	TestNotEqual(TEXT("Proposal changes fixture recovery state"), Preview.Proposed.Recovery.StateHash, Preview.Current.Recovery.StateHash);
	TestTrue(TEXT("Preview summary names fixture metrics"), Review.GetPreviewSummary().Contains(TEXT("lubeck_grain_shortage_v1")));

	TestEqual(TEXT("Preview preserves source value"), Recipe->CycleTicks, InitialCycleTicks);
	TestEqual(TEXT("Preview preserves revision"), Recipe->AuthoredRevision, InitialRevision);
	TestTrue(TEXT("Accepted field applies in one transaction"), Review.ApplySelected(Definitions));
	TestEqual(TEXT("Accepted cycle value applied"), Recipe->CycleTicks, InitialCycleTicks + 1);
	TestEqual(TEXT("Rejected workforce value unchanged"), Recipe->LaborerWorkforce, InitialLaborers);
	TestEqual(TEXT("Revision increments once"), Recipe->AuthoredRevision, InitialRevision + 1);
	const FHansaEconomicRegistryCompileResult AppliedCompile = FHansaEconomicDefinitionCompiler::Compile(Const(OwnedDefinitions));
	TestTrue(TEXT("Registry recompiles after apply"), AppliedCompile.IsValid());
	TestEqual(TEXT("Applied registry matches preview"), AppliedCompile.Registry.GetRegistryHash(), Preview.Proposed.RegistryHash);

	TestNotNull(TEXT("Editor transaction system is available"), GEditor);
	if (GEditor != nullptr)
	{
		GEditor->UndoTransaction();
		const FHansaEconomicRegistryCompileResult UndoCompile = FHansaEconomicDefinitionCompiler::Compile(Const(OwnedDefinitions));
		TestEqual(TEXT("Undo restores cycle value"), Recipe->CycleTicks, InitialCycleTicks);
		TestEqual(TEXT("Undo restores authored revision"), Recipe->AuthoredRevision, InitialRevision);
		TestEqual(TEXT("Undo restores deterministic registry"), UndoCompile.Registry.GetRegistryHash(), Preview.Current.RegistryHash);

		GEditor->RedoTransaction();
		const FHansaEconomicRegistryCompileResult RedoCompile = FHansaEconomicDefinitionCompiler::Compile(Const(OwnedDefinitions));
		TestEqual(TEXT("Redo restores accepted cycle"), Recipe->CycleTicks, InitialCycleTicks + 1);
		TestEqual(TEXT("Redo restores authored revision"), Recipe->AuthoredRevision, InitialRevision + 1);
		TestEqual(TEXT("Redo restores proposed registry"), RedoCompile.Registry.GetRegistryHash(), Preview.Proposed.RegistryHash);
	}
	if (!EvidenceRoot.IsEmpty() && !HasAnyErrors())
	{
		TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
		Evidence->SetStringField(TEXT("status"), TEXT("Succeeded"));
		Evidence->SetStringField(TEXT("proposalSource"), WorkerProposalPath.IsEmpty() ? TEXT("native-fixture") : TEXT("persistent-mock-worker"));
		Evidence->SetStringField(TEXT("acceptedField"), TEXT("CycleTicks"));
		Evidence->SetStringField(TEXT("rejectedField"), TEXT("LaborerWorkforce"));
		Evidence->SetBoolField(TEXT("undoRedoVerified"), true);
		Evidence->SetBoolField(TEXT("staleApplyRejected"), true);
		Evidence->SetStringField(TEXT("fixture"), TEXT("lubeck_grain_shortage_v1"));
		auto SaveRun = [](const FHansaProposalFixtureRun& Run)
		{
			auto Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Run.RegistryHash)));
			Json->SetBoolField(TEXT("recoveryContractPassed"), Run.bRecoveryContractPassed);
			auto SavePhase = [](const FHansaProposalFixturePhase& Phase)
			{
				auto Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("tick"), Phase.Tick);
				Item->SetNumberField(TEXT("stockMilliUnits"), Phase.StockMilliUnits);
				Item->SetNumberField(TEXT("reserveMilliUnits"), Phase.ReserveMilliUnits);
				Item->SetNumberField(TEXT("priceMilliMarks"), Phase.PriceMilliMarks);
				Item->SetNumberField(TEXT("unmetDemandMilliUnits"), Phase.UnmetDemandMilliUnits);
				Item->SetStringField(TEXT("stateHash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Phase.StateHash)));
				return Item;
			};
			Json->SetObjectField(TEXT("baseline"), SavePhase(Run.Baseline));
			Json->SetObjectField(TEXT("shortage"), SavePhase(Run.Shortage));
			Json->SetObjectField(TEXT("recovery"), SavePhase(Run.Recovery));
			return Json;
		};
		Evidence->SetObjectField(TEXT("current"), SaveRun(Preview.Current));
		Evidence->SetObjectField(TEXT("proposed"), SaveRun(Preview.Proposed));
		FString Json;
		FJsonSerializer::Serialize(Evidence, TJsonWriterFactory<>::Create(&Json));
		TestTrue(TEXT("Write measured acceptance evidence"), FFileHelper::SaveStringToFile(Json,
			*FPaths::Combine(EvidenceRoot, TEXT("editor-acceptance.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	}
	return !HasAnyErrors();
}

#endif
