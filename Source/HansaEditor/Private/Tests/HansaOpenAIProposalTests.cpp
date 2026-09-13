#include "Definitions/HansaEconomicDefinitions.h"
#include "Editor.h"
#include "Generation/HansaDefinitionProposalReview.h"
#include "Generation/HansaGenerationWorkerBridge.h"
#include "Misc/AutomationTest.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Hansa::Editor::Generation;

namespace Hansa::Editor::Tests
{
	TSharedRef<FJsonObject> ProposalFor(const UHansaDefinitionBase* Definition, const FHansaDefinitionClassSchema& Schema, const TSharedRef<FJsonObject>& Contract, const int64 BaseValueMilliMarks)
	{
		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* Writable = nullptr;
		Contract->TryGetObjectField(TEXT("writableFields"), Writable);
		for (const auto& Pair : (*Writable)->Values) Patch->SetField(Pair.Key.ToView(), MakeShared<FJsonValueNull>());
		Patch->SetNumberField(TEXT("BaseValueMilliMarks"), static_cast<double>(BaseValueMilliMarks));
		TSharedRef<FJsonObject> Proposal = MakeShared<FJsonObject>();
		Proposal->SetNumberField(TEXT("proposalVersion"), 1);
		Proposal->SetStringField(TEXT("schemaId"), Schema.SchemaId);
		Proposal->SetNumberField(TEXT("schemaVersion"), Schema.SchemaVersion);
		FString SourceSchemaHash;
		Contract->TryGetStringField(TEXT("sourceSchemaHash"), SourceSchemaHash);
		Proposal->SetStringField(TEXT("sourceSchemaHash"), SourceSchemaHash);
		Proposal->SetStringField(TEXT("baseStableId"), Definition->StableDefinitionId);
		Proposal->SetNumberField(TEXT("baseRevision"), Definition->AuthoredRevision);
		Proposal->SetStringField(TEXT("baseContentHash"), FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Definition->ComputeDeterministicContentHash())));
		Proposal->SetObjectField(TEXT("patch"), Patch);
		return Proposal;
	}

	class FProposalReadTransport final : public IHansaGenerationWorkerTransport
	{
	public:
		virtual bool Request(const FString& Operation, const TSharedRef<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutResult, FHansaWorkerError& OutError) override
		{
			if (Operation != TEXT("job.output.read")) { OutError = { TEXT("UnexpectedOperation"), Operation, FString(), false }; return false; }
			OutResult = MakeShared<FJsonObject>();
			TSharedRef<FJsonObject> Document = MakeShared<FJsonObject>();
			Document->SetNumberField(TEXT("proposalVersion"), 1);
			OutResult->SetObjectField(TEXT("document"), Document);
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaDefinitionProposalReviewTest,
	"Hansa.Architecture.GenerationWorker.OpenAIProposalReview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaDefinitionProposalReviewTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UHansaGoodDefinition> Definition(NewObject<UHansaGoodDefinition>(GetTransientPackage()));
	Definition->SetFlags(RF_Transactional);
	Definition->StableDefinitionId = TEXT("Good.ProposalReview");
	Definition->AuthoredRevision = 7;
	Definition->BaseValueMilliMarks = 42000;
	Definition->RefreshContentHash();
	FHansaEditorSchemaRegistry Registry;
	const FHansaDefinitionClassSchema Schema = Registry.BuildSchemaForClass(Definition->GetClass());
	TArray<UHansaDefinitionBase*> Definitions = { Definition.Get() };
	TSharedPtr<FJsonObject> Contract;
	FHansaProposalReviewError ContractError;
	TestTrue(TEXT("Schema-derived proposal contract builds"), FHansaDefinitionProposalReview::BuildContract(Definition.Get(), Schema, Definitions, Contract, ContractError));
	if (!Contract.IsValid()) return false;
	FString SourceHash;
	TestTrue(TEXT("Exported schema SHA-256 is present"), Contract->TryGetStringField(TEXT("sourceSchemaHash"), SourceHash) && SourceHash.Len() == 64);

	FHansaDefinitionProposalReview Review;
	const TSharedRef<FJsonObject> Proposal = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 57000);
	TestTrue(TEXT("Exact proposal loads"), Review.Load(Definition.Get(), Schema, Proposal, Definitions));
	TestEqual(TEXT("Only non-null changed field appears"), Review.GetDiffs().Num(), 1);
	if (Review.GetDiffs().IsEmpty()) return false;
	TestEqual(TEXT("Diff identifies BaseValueMilliMarks"), Review.GetDiffs()[0]->FieldName, FString(TEXT("BaseValueMilliMarks")));
	Review.SetSelected(TEXT("BaseValueMilliMarks"), false);
	TestFalse(TEXT("Empty selection is rejected"), Review.ValidateSelected(Definitions));
	Review.SetSelected(TEXT("BaseValueMilliMarks"), true);
	TestTrue(TEXT("Temporary registry validates selected patch"), Review.ValidateSelected(Definitions));
	TestTrue(TEXT("Preview reports deterministic registry hashes"), Review.GetPreviewSummary().Contains(TEXT("Temporary registry valid")));
	TestTrue(TEXT("Selected patch applies transactionally"), Review.ApplySelected(Definitions));
	TestEqual(TEXT("Selected value applied"), Definition->BaseValueMilliMarks, static_cast<int64>(57000));
	TestEqual(TEXT("Authored revision increments once"), Definition->AuthoredRevision, 8);
	if (GEditor) { GEditor->UndoTransaction(); TestEqual(TEXT("Undo restores value"), Definition->BaseValueMilliMarks, static_cast<int64>(42000)); TestEqual(TEXT("Undo restores revision"), Definition->AuthoredRevision, 7); }

	FHansaDefinitionProposalReview StaleReview;
	TestTrue(TEXT("Fresh proposal loads before concurrent edit"), StaleReview.Load(Definition.Get(), Schema, Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 61000), Definitions));
	Definition->BaseValueMilliMarks = 43000;
	Definition->RefreshContentHash();
	TestFalse(TEXT("Concurrent content edit makes proposal stale"), StaleReview.ValidateSelected(Definitions));
	TestEqual(TEXT("Stale rejection is structured"), StaleReview.GetError().Code, FString(TEXT("StaleProposal")));

	FHansaDefinitionProposalReview SchemaHashReview;
	TSharedRef<FJsonObject> WrongSchemaHash = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 62000);
	WrongSchemaHash->SetStringField(TEXT("sourceSchemaHash"), FString::ChrN(64, TCHAR('b')));
	TestFalse(TEXT("Changed exported schema hash is rejected"), SchemaHashReview.Load(Definition.Get(), Schema, WrongSchemaHash, Definitions));
	TestEqual(TEXT("Schema hash rejection is structured"), SchemaHashReview.GetError().Code, FString(TEXT("StaleOrMismatchedProposal")));

	Definition->BaseValueMilliMarks = 42000;
	Definition->RefreshContentHash();
	TSharedRef<FJsonObject> Forbidden = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 60000);
	const TSharedPtr<FJsonObject>* Patch = nullptr;
	Forbidden->TryGetObjectField(TEXT("patch"), Patch);
	(*Patch)->SetStringField(TEXT("StableDefinitionId"), TEXT("Good.Evil"));
	FHansaDefinitionProposalReview ForbiddenReview;
	TestFalse(TEXT("Non-writable identity field is rejected"), ForbiddenReview.Load(Definition.Get(), Schema, Forbidden, Definitions));
	TestEqual(TEXT("Forbidden field rejection is structured"), ForbiddenReview.GetError().Code, FString(TEXT("ForbiddenProposalField")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaProposalReadBridgeTest,
	"Hansa.Architecture.GenerationWorker.ProposalOutputRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProposalReadBridgeTest::RunTest(const FString& Parameters)
{
	TSharedRef<Hansa::Editor::Tests::FProposalReadTransport> Transport = MakeShared<Hansa::Editor::Tests::FProposalReadTransport>();
	FHansaGenerationJobController Controller(Transport);
	TSharedPtr<FJsonObject> Document;
	TestTrue(TEXT("Bridge reads bounded JSON proposal through worker operation"), Controller.ReadJsonOutput(TEXT("00000000-0000-0000-0000-000000000001"), 0, Document));
	double Version = 0;
	TestTrue(TEXT("Proposal document preserved"), Document.IsValid() && Document->TryGetNumberField(TEXT("proposalVersion"), Version) && Version == 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaProposalExactSchemaTest,
	"Hansa.Architecture.GenerationWorker.ProposalExactSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProposalExactSchemaTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UHansaGoodDefinition> Definition(NewObject<UHansaGoodDefinition>(GetTransientPackage()));
	Definition->StableDefinitionId = TEXT("Good.ExactSchema");
	Definition->AuthoredRevision = 7;
	Definition->BaseValueMilliMarks = 42000;
	Definition->RefreshContentHash();
	FHansaEditorSchemaRegistry Registry;
	const auto Schema = Registry.BuildSchemaForClass(Definition->GetClass());
	TArray<UHansaDefinitionBase*> Definitions = { Definition.Get() };
	TSharedPtr<FJsonObject> Contract;
	FHansaProposalReviewError Error;
	if (!TestTrue(TEXT("Build exact schema contract"), FHansaDefinitionProposalReview::BuildContract(Definition.Get(), Schema, Definitions, Contract, Error))) return false;
	for (const TCHAR* Identity : { TEXT("schemaVersion"), TEXT("baseRevision") })
	{
		auto Proposal = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 57000);
		Proposal->SetNumberField(Identity, Proposal->GetNumberField(Identity) + 0.5);
		FHansaDefinitionProposalReview Review;
		TestFalse(TEXT("Fractional schema or revision cannot be truncated"), Review.Load(Definition.Get(), Schema, Proposal, Definitions));
	}
	for (double Number : { 57000.5, 9007199254740992.0, -1.0 })
	{
		auto Proposal = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 57000);
		Proposal->GetObjectField(TEXT("patch"))->SetNumberField(TEXT("BaseValueMilliMarks"), Number);
		FHansaDefinitionProposalReview Review;
		TestFalse(TEXT("Fractional, unsafe or negative integer rejected before review"), Review.Load(Definition.Get(), Schema, Proposal, Definitions));
		TestEqual(TEXT("Precise schema error"), Review.GetError().Code, FString(TEXT("SchemaMismatch")));
	}
	auto Proposal = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 57000);
	FHansaDefinitionProposalReview Review;
	TestTrue(TEXT("Valid proposal loads"), Review.Load(Definition.Get(), Schema, Proposal, Definitions));
	Proposal->GetObjectField(TEXT("patch"))->SetNumberField(TEXT("BaseValueMilliMarks"), 99000);
	TestEqual(TEXT("Loaded diff owns reviewed bytes"), Review.GetDiffs()[0]->ProposedValue->AsNumber(), 57000.0);
	TestTrue(TEXT("Owned proposal validates"), Review.ValidateSelected(Definitions));
	TestEqual(TEXT("Preview never mutates accepted asset"), Definition->BaseValueMilliMarks, int64(42000));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaProposalArrayReferenceTest,
	"Hansa.Architecture.GenerationWorker.ProposalArrayReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProposalArrayReferenceTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UHansaBuildingDefinition> Definition(NewObject<UHansaBuildingDefinition>(GetTransientPackage()));
	Definition->StableDefinitionId = TEXT("Building.ArrayProposal");
	Definition->AuthoredRevision = 1;
	Definition->RefreshContentHash();
	FHansaEditorSchemaRegistry Registry;
	const auto Schema = Registry.BuildSchemaForClass(Definition->GetClass());
	TArray<UHansaDefinitionBase*> Definitions = { Definition.Get() };
	TSharedPtr<FJsonObject> Contract;
	FHansaProposalReviewError Error;
	if (!TestTrue(TEXT("Build array contract"), FHansaDefinitionProposalReview::BuildContract(Definition.Get(), Schema, Definitions, Contract, Error))) return false;
	const auto ItemSchema = Contract->GetObjectField(TEXT("writableFields"))->GetObjectField(TEXT("RecipeIds"))->GetObjectField(TEXT("items"));
	TestEqual(TEXT("Array items inherit reference metadata"), ItemSchema->GetStringField(TEXT("x-hansa-reference")), FString(TEXT("Recipe")));
	auto Proposal = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 1);
	const auto Patch = Proposal->GetObjectField(TEXT("patch"));
	Patch->RemoveField(TEXT("BaseValueMilliMarks"));
	Patch->SetArrayField(TEXT("RecipeIds"), { MakeShared<FJsonValueString>(TEXT("Recipe.Missing")) });
	FHansaDefinitionProposalReview Review;
	TestFalse(TEXT("Unknown array reference rejected at load"), Review.Load(Definition.Get(), Schema, Proposal, Definitions));
	TestEqual(TEXT("Array rejection is a reference error"), Review.GetError().Code, FString(TEXT("UnknownStableReference")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketAccessProposalContractTest,
	"Hansa.Architecture.GenerationWorker.MarketAccessProposalContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketAccessProposalContractTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UHansaBuildingDefinition> Definition(NewObject<UHansaBuildingDefinition>(GetTransientPackage()));
	Definition->StableDefinitionId = TEXT("Building.ProposedMarket");
	Definition->AuthoredRevision = 3;
	Definition->bRequiresRoad = true;
	Definition->StorageCapacityMilliUnits = 10000;
	Definition->RefreshContentHash();
	FHansaEditorSchemaRegistry Registry;
	const FHansaDefinitionClassSchema Schema = Registry.BuildSchemaForClass(Definition->GetClass());
	TArray<UHansaDefinitionBase*> Definitions = { Definition.Get() };
	TSharedPtr<FJsonObject> Contract;
	FHansaProposalReviewError Error;
	if (!TestTrue(TEXT("Building proposal contract builds"), FHansaDefinitionProposalReview::BuildContract(Definition.Get(), Schema, Definitions, Contract, Error))) return false;
	const TSharedPtr<FJsonObject> Writable = Contract->GetObjectField(TEXT("writableFields"));
	TestTrue(TEXT("AI context exposes market access capability"), Writable->HasField(TEXT("bProvidesMarketAccess")));
	TestEqual(TEXT("Market capability is an exact boolean"), Writable->GetObjectField(TEXT("bProvidesMarketAccess"))->GetStringField(TEXT("type")), FString(TEXT("boolean")));
	TestFalse(TEXT("AI context includes current market capability"), Contract->GetObjectField(TEXT("baseValues"))->GetBoolField(TEXT("bProvidesMarketAccess")));

	TSharedRef<FJsonObject> Proposal = Hansa::Editor::Tests::ProposalFor(Definition.Get(), Schema, Contract.ToSharedRef(), 1);
	const TSharedPtr<FJsonObject> Patch = Proposal->GetObjectField(TEXT("patch"));
	Patch->RemoveField(TEXT("BaseValueMilliMarks"));
	Patch->SetBoolField(TEXT("bProvidesMarketAccess"), true);
	FHansaDefinitionProposalReview Review;
	TestTrue(TEXT("Mock provider proposal loads through the exact schema"), Review.Load(Definition.Get(), Schema, Proposal, Definitions));
	TestEqual(TEXT("Proposal identifies one market capability change"), Review.GetDiffs().Num(), 1);
	TestFalse(TEXT("Review remains a draft until approved"), Definition->bProvidesMarketAccess);
	return !HasAnyErrors();
}

#endif
