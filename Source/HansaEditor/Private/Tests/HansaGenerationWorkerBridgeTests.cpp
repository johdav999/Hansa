#include "Generation/HansaGenerationWorkerBridge.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Studio/SHansaGenerationJobsPanel.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonValue.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Editor::Generation::Tests
{
	TSharedRef<FJsonObject> MakeCapabilities()
	{
		TSharedRef<FJsonObject> Capability = MakeShared<FJsonObject>();
		Capability->SetStringField(TEXT("capability"), TEXT("StructuredDataDraft"));
		Capability->SetBoolField(TEXT("supportsCancellation"), true);
		Capability->SetBoolField(TEXT("supportsSeed"), true);
		Capability->SetArrayField(TEXT("inputMediaTypes"), { MakeShared<FJsonValueString>(TEXT("text/plain")) });
		Capability->SetArrayField(TEXT("outputMediaTypes"), { MakeShared<FJsonValueString>(TEXT("application/json")) });
		TSharedRef<FJsonObject> Model = MakeShared<FJsonObject>();
		Model->SetStringField(TEXT("modelVersion"), TEXT("mock-v1"));
		Model->SetBoolField(TEXT("pinned"), true);
		TSharedRef<FJsonObject> Provider = MakeShared<FJsonObject>();
		Provider->SetStringField(TEXT("providerId"), TEXT("mock"));
		Provider->SetStringField(TEXT("adapterVersion"), TEXT("1.0.0"));
		Provider->SetArrayField(TEXT("models"), { MakeShared<FJsonValueObject>(Model) });
		Provider->SetArrayField(TEXT("capabilities"), { MakeShared<FJsonValueObject>(Capability) });
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("providers"), { MakeShared<FJsonValueObject>(Provider) });
		return Result;
	}

	TSharedRef<FJsonObject> MakeJob(const FString& Status = TEXT("Review"))
	{
		TSharedRef<FJsonObject> Job = MakeShared<FJsonObject>();
		Job->SetStringField(TEXT("jobId"), TEXT("gen-00000001"));
		Job->SetStringField(TEXT("status"), Status);
		Job->SetStringField(TEXT("createdAt"), TEXT("2026-09-06T12:00:00.000Z"));
		Job->SetStringField(TEXT("requestHash"), FString::ChrN(64, TEXT('a')));
		TSharedRef<FJsonObject> Provider = MakeShared<FJsonObject>();
		Provider->SetStringField(TEXT("providerId"), TEXT("mock"));
		Provider->SetStringField(TEXT("modelVersion"), TEXT("mock-v1"));
		Job->SetObjectField(TEXT("provider"), Provider);
		TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TEXT("capability"), TEXT("StructuredDataDraft"));
		TSharedRef<FJsonObject> Budgets = MakeShared<FJsonObject>();
		Budgets->SetNumberField(TEXT("maximumCostMinorUnits"), 12);
		Request->SetObjectField(TEXT("budgets"), Budgets);
		Job->SetObjectField(TEXT("request"), Request);
		TSharedRef<FJsonObject> Progress = MakeShared<FJsonObject>();
		Progress->SetStringField(TEXT("message"), TEXT("Ready for editor review."));
		Progress->SetNumberField(TEXT("percent"), 100);
		Job->SetObjectField(TEXT("progress"), Progress);
		TSharedRef<FJsonObject> Estimate = MakeShared<FJsonObject>();
		Estimate->SetStringField(TEXT("currency"), TEXT("USD"));
		Estimate->SetNumberField(TEXT("estimatedMinorUnits"), 0);
		Job->SetObjectField(TEXT("estimate"), Estimate);
		TSharedRef<FJsonObject> Usage = MakeShared<FJsonObject>();
		Usage->SetStringField(TEXT("currency"), TEXT("USD"));
		Usage->SetNumberField(TEXT("actualMinorUnits"), 0);
		Job->SetObjectField(TEXT("actualUsage"), Usage);
		TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
		Output->SetStringField(TEXT("logicalName"), TEXT("structured-proposal"));
		Output->SetStringField(TEXT("mediaType"), TEXT("application/json"));
		Output->SetStringField(TEXT("relativePath"), TEXT("output/artifact-0.json"));
		Output->SetStringField(TEXT("sha256"), FString::ChrN(64, TEXT('b')));
		Output->SetNumberField(TEXT("sizeBytes"), 42);
		Job->SetArrayField(TEXT("outputs"), { MakeShared<FJsonValueObject>(Output) });
		TSharedRef<FJsonObject> Qa = MakeShared<FJsonObject>();
		Qa->SetStringField(TEXT("check"), TEXT("OutputHashes"));
		Qa->SetBoolField(TEXT("passed"), true);
		Job->SetArrayField(TEXT("qaResults"), { MakeShared<FJsonValueObject>(Qa) });
		TSharedRef<FJsonObject> Provenance = MakeShared<FJsonObject>();
		Provenance->SetStringField(TEXT("adapterVersion"), TEXT("1.0.0"));
		Provenance->SetStringField(TEXT("providerJobId"), TEXT("mock-provider-job"));
		Job->SetObjectField(TEXT("provenance"), Provenance);
		TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
		Manifest->SetStringField(TEXT("sha256"), FString::ChrN(64, TEXT('c')));
		Job->SetArrayField(TEXT("manifests"), { MakeShared<FJsonValueObject>(Manifest) });
		Job->SetArrayField(TEXT("errors"), {});
		return Job;
	}

	class FFakeWorkerTransport final : public IHansaGenerationWorkerTransport
	{
	public:
		TArray<FString> Operations;
		TMap<FString, TSharedPtr<FJsonObject>> Payloads;
		TSharedRef<FJsonObject> Job = MakeJob();
		double EstimatedCost = 0.0;
		FString EstimateCurrency = TEXT("USD");
		bool bLoseSubmitResponse = false;

		virtual bool Request(const FString& Operation, const TSharedRef<FJsonObject>& Payload,
			TSharedPtr<FJsonObject>& OutResult, FHansaWorkerError& OutError) override
		{
			Operations.Add(Operation);
			Payloads.Add(Operation, Payload);
			OutError = {};
			OutResult = MakeShared<FJsonObject>();
			if (Operation == TEXT("worker.capabilities")) OutResult = MakeCapabilities();
			else if (Operation == TEXT("job.estimate"))
			{
				TSharedRef<FJsonObject> Estimate = MakeShared<FJsonObject>();
				Estimate->SetStringField(TEXT("currency"), TEXT("USD"));
				Estimate->SetNumberField(TEXT("estimatedMinorUnits"), EstimatedCost);
				Estimate->SetStringField(TEXT("currency"), EstimateCurrency);
				OutResult->SetObjectField(TEXT("estimate"), Estimate);
				OutResult->SetStringField(TEXT("requestHash"), FString::ChrN(64, TEXT('d')));
				OutResult->SetStringField(TEXT("inputHash"), FString::ChrN(64, TEXT('e')));
			}
			else if (Operation == TEXT("job.list")) OutResult->SetArrayField(TEXT("jobs"), { MakeShared<FJsonValueObject>(Job) });
			else if (Operation == TEXT("job.submit"))
			{
				if (bLoseSubmitResponse)
				{
					OutError = { TEXT("WorkerIoFailed"), TEXT("Mock response lost after acceptance."), FString(), true };
					return false;
				}
				OutResult->SetObjectField(TEXT("job"), Job);
			}
			else if (Operation == TEXT("job.cancel") || Operation == TEXT("job.retry")) OutResult->SetObjectField(TEXT("job"), Job);
			else
			{
				OutError = { TEXT("UnknownOperation"), TEXT("Unexpected fake operation."), FString(), false };
				return false;
			}
			return true;
		}
	};

	FString Serialize(const TSharedRef<FJsonObject>& Json)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Json, Writer);
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGenerationEditorBridgeTest,
	"Hansa.Architecture.GenerationWorker.EditorBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGenerationEditorBridgeTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Generation;
	using namespace Hansa::Editor::Generation::Tests;
	const TSharedRef<FFakeWorkerTransport> Transport = MakeShared<FFakeWorkerTransport>();
	FHansaGenerationJobController Controller(Transport);
	TestTrue(TEXT("Capabilities load from provider-neutral mock"), Controller.RefreshCapabilities());
	TestEqual(TEXT("One provider capability is exposed"), Controller.GetCapabilities().Num(), 1);

	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("generation_worker_editor_v1"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString InputPath = FPaths::Combine(Directory, TEXT("exact-upload.txt"));
	TestTrue(TEXT("Input fixture is written"), FFileHelper::SaveStringToFile(TEXT("owned mock input"), *InputPath));
	TestTrue(TEXT("Exact upload preview hashes the selected file"), Controller.PreviewUploads({ InputPath }, TEXT("Owned test fixture")));
	TestEqual(TEXT("One exact upload is listed"), Controller.GetUploads().Num(), 1);
	TestEqual(TEXT("Upload media type is exact"), Controller.GetUploads()[0].MediaType, FString(TEXT("text/plain")));
	TestEqual(TEXT("Upload SHA-256 has 64 hex characters"), Controller.GetUploads()[0].Sha256.Len(), 64);

	FHansaGenerationSubmission Submission;
	Submission.Prompt = TEXT("Propose a deterministic definition draft.");
	Submission.ApprovedBy = TEXT("Automation Operator");
	Submission.bRightsAcknowledged = true;
	FString Reason;
	TestFalse(TEXT("Submission is blocked before estimate"), Controller.CanSubmit(Submission, Reason));
	TestTrue(TEXT("Read-only exact-request estimate succeeds"), Controller.Estimate(Submission));
	TestTrue(TEXT("Estimate has a request hash"), Controller.GetEstimatedRequestHash().Len() == 64);
	TestFalse(TEXT("Submission remains blocked before spend approval"), Controller.CanSubmit(Submission, Reason));
	Submission.bSpendApproved = true;
	TestTrue(TEXT("Estimated approved request can submit"), Controller.CanSubmit(Submission, Reason));
	FHansaGenerationSubmission Changed = Submission;
	Changed.Prompt += TEXT(" changed");
	TestFalse(TEXT("Changing request after estimate invalidates approval"), Controller.CanSubmit(Changed, Reason));
	TestTrue(TEXT("Approved exact request queues"), Controller.Submit(Submission));

	const TSharedPtr<FJsonObject>* SubmitPayload = Transport->Payloads.Find(TEXT("job.submit"));
	TestTrue(TEXT("Mock captured job.submit payload"), SubmitPayload != nullptr && SubmitPayload->IsValid());
	if (SubmitPayload != nullptr && SubmitPayload->IsValid())
	{
		const FString Json = Serialize(SubmitPayload->ToSharedRef());
		TestFalse(TEXT("Editor payload contains no authentication token"), Json.Contains(TEXT("authToken")));
		TestFalse(TEXT("Editor payload contains no provider credential fields"), Json.Contains(TEXT("apiKey")) || Json.Contains(TEXT("clientSecret")));
		TestTrue(TEXT("Rights declaration crosses only with the exact upload"), Json.Contains(TEXT("Owned test fixture")));
		TestTrue(TEXT("Explicit spend approval is serialized"), Json.Contains(TEXT("\"approved\":true")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGenerationQueueActionsTest,
	"Hansa.Architecture.GenerationWorker.QueueActionsAndProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGenerationQueueActionsTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Generation;
	using namespace Hansa::Editor::Generation::Tests;
	const TSharedRef<FFakeWorkerTransport> Transport = MakeShared<FFakeWorkerTransport>();
	FHansaGenerationJobController Controller(Transport);
	TestTrue(TEXT("Mock job list refreshes"), Controller.RefreshJobs());
	TestEqual(TEXT("One job is parsed"), Controller.GetJobs().Num(), 1);
	if (!Controller.GetJobs().IsEmpty())
	{
		const FHansaGenerationJob& Job = Controller.GetJobs()[0];
		TestEqual(TEXT("Review state parsed"), Job.Status, FString(TEXT("Review")));
		TestEqual(TEXT("Retry displays stored job hard budget"), Job.MaximumCostMinorUnits, int64(12));
		TestEqual(TEXT("Output SHA-256 parsed"), Job.Outputs[0].Sha256.Len(), 64);
		TestEqual(TEXT("Provider job ID stays provenance"), Job.ProviderJobId, FString(TEXT("mock-provider-job")));
		TestEqual(TEXT("Manifest hash parsed"), Job.ManifestSha256.Len(), 64);
		TestEqual(TEXT("Deterministic QA parsed"), Job.QaResults[0], FString(TEXT("PASS OutputHashes")));
	}
	TestFalse(TEXT("Retry rejects missing fresh spend approval"), Controller.Retry(TEXT("gen-00000001"), TEXT("Automation"), false));
	TestTrue(TEXT("Retry accepts fresh identified spend approval"), Controller.Retry(TEXT("gen-00000001"), TEXT("Automation"), true));
	TestTrue(TEXT("Cancellation routes to the worker"), Controller.Cancel(TEXT("gen-00000001")));
	TestTrue(TEXT("Retry operation was sent"), Transport->Operations.Contains(TEXT("job.retry")));
	TestTrue(TEXT("Cancel operation was sent"), Transport->Operations.Contains(TEXT("job.cancel")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGenerationApprovalFailuresTest,
	"Hansa.Architecture.GenerationWorker.ApprovalFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGenerationApprovalFailuresTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Generation;
	using namespace Hansa::Editor::Generation::Tests;
	const auto Transport = MakeShared<FFakeWorkerTransport>();
	FHansaGenerationJobController Controller(Transport);
	TestTrue(TEXT("Capabilities load"), Controller.RefreshCapabilities());
	FHansaGenerationSubmission Submission;
	Submission.Prompt = TEXT("Bounded mock proposal");
	Submission.ApprovedBy = TEXT("Automation");
	Submission.bSpendApproved = true;
	FString Reason;
	for (double InvalidCost : { -1.0, 0.5, 1.0 })
	{
		Transport->EstimatedCost = InvalidCost;
		TestFalse(TEXT("Negative, fractional or over-budget cost fails closed"), Controller.Estimate(Submission));
		TestFalse(TEXT("Invalid estimate cannot authorize spend"), Controller.CanSubmit(Submission, Reason));
	}
	Transport->EstimatedCost = 0;
	Transport->EstimateCurrency = TEXT("EUR");
	TestFalse(TEXT("Currency mismatch fails closed"), Controller.Estimate(Submission));
	Transport->EstimateCurrency = TEXT("USD");
	TestTrue(TEXT("Valid estimate recovers"), Controller.Estimate(Submission));
	Transport->bLoseSubmitResponse = true;
	TestFalse(TEXT("Lost submission response is reported"), Controller.Submit(Submission));
	TestFalse(TEXT("Uncertain submission consumes its approval"), Controller.CanSubmit(Submission, Reason));
	const int32 RequestsBefore = Transport->Operations.Num();
	TestFalse(TEXT("A repeated click cannot submit again"), Controller.Submit(Submission));
	TestEqual(TEXT("Repeated click performs no I/O"), Transport->Operations.Num(), RequestsBefore);

	const FString InputPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("generation_changed_input.txt"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(InputPath), true);
	TestTrue(TEXT("Write original input"), FFileHelper::SaveStringToFile(TEXT("Original"), *InputPath));
	TestTrue(TEXT("Preview input"), Controller.PreviewUploads({InputPath}, TEXT("Owned fixture")));
	Submission.bRightsAcknowledged = true;
	TestTrue(TEXT("Estimate original bytes"), Controller.Estimate(Submission));
	TestTrue(TEXT("Change input after estimate"), FFileHelper::SaveStringToFile(TEXT("Changed"), *InputPath));
	const int32 BeforeChangedSubmit = Transport->Operations.Num();
	TestFalse(TEXT("Changed file cannot be uploaded"), Controller.Submit(Submission));
	TestEqual(TEXT("Changed file rejected before transport"), Transport->Operations.Num(), BeforeChangedSubmit);
	return !HasAnyErrors();
}

#if PLATFORM_WINDOWS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGenerationPipeTimeoutTest,
	"Hansa.Architecture.GenerationWorker.StalledPipeTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGenerationPipeTimeoutTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Generation;
	const FString PipeName = TEXT("hansa-generation-timeout-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString FullName = TEXT("\\\\.\\pipe\\") + PipeName;
	const HANDLE Pipe = CreateNamedPipeW(*FullName, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, nullptr);
	if (!TestTrue(TEXT("Create isolated mock pipe"), Pipe != INVALID_HANDLE_VALUE)) return false;
	const HANDLE Finish = CreateEventW(nullptr, true, false, nullptr);
	if (!Finish) { CloseHandle(Pipe); AddError(TEXT("Create mock completion event")); return false; }
	const FString PreviousToken = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_GENERATION_WORKER_TOKEN"));
	FPlatformMisc::SetEnvironmentVar(TEXT("HANSA_GENERATION_WORKER_TOKEN"), TEXT("mock-timeout-test-token-0001"));
	ON_SCOPE_EXIT { FPlatformMisc::SetEnvironmentVar(TEXT("HANSA_GENERATION_WORKER_TOKEN"), *PreviousToken); };
	auto Server = Async(EAsyncExecution::Thread, [Pipe, Finish]
	{
		OVERLAPPED Connection = {};
		Connection.hEvent = CreateEventW(nullptr, true, false, nullptr);
		if (Connection.hEvent)
		{
			const BOOL Connected = ConnectNamedPipe(Pipe, &Connection);
			const DWORD Error = Connected ? ERROR_SUCCESS : GetLastError();
			if (Connected || Error == ERROR_PIPE_CONNECTED ||
				(Error == ERROR_IO_PENDING && WaitForSingleObject(Connection.hEvent, 8000) == WAIT_OBJECT_0))
				WaitForSingleObject(Finish, 8000); // Deliberately never return a response frame.
			CancelIoEx(Pipe, &Connection);
			DWORD Ignored = 0;
			if (Error == ERROR_IO_PENDING) GetOverlappedResult(Pipe, &Connection, &Ignored, true);
			CloseHandle(Connection.hEvent);
		}
		DisconnectNamedPipe(Pipe);
		CloseHandle(Pipe);
	});
	FHansaGenerationWorkerNamedPipeTransport Transport(PipeName);
	TSharedPtr<FJsonObject> Result;
	FHansaWorkerError Error;
	const double Start = FPlatformTime::Seconds();
	TestFalse(TEXT("Stalled worker returns an error"), Transport.Request(TEXT("job.list"), MakeShared<FJsonObject>(), Result, Error));
	const double Elapsed = FPlatformTime::Seconds() - Start;
	SetEvent(Finish);
	Server.Wait();
	CloseHandle(Finish);
	TestEqual(TEXT("Timeout is a retryable I/O failure"), Error.Code, FString(TEXT("WorkerIoFailed")));
	TestTrue(TEXT("Stalled response is bounded"), Elapsed >= 2.0 && Elapsed < 6.0);
	TestTrue(TEXT("Timeout can be recovered"), Error.bRetryable);
	return !HasAnyErrors();
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGenerationPanelAsyncTest,
	"Hansa.Architecture.GenerationWorker.PanelAsyncLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGenerationPanelAsyncTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::Generation;
	struct FGate
	{
		FEvent* Started = FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* Release = FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* Finished = FPlatformProcess::GetSynchEventFromPool(true);
		~FGate()
		{
			FPlatformProcess::ReturnSynchEventToPool(Started);
			FPlatformProcess::ReturnSynchEventToPool(Release);
			FPlatformProcess::ReturnSynchEventToPool(Finished);
		}
	};
	const auto Gate = MakeShared<FGate, ESPMode::ThreadSafe>();
	TSharedPtr<SHansaGenerationJobsPanel> Panel = SNew(SHansaGenerationJobsPanel);
	bool bCompleted = false;
	Panel->StartOperation([Gate](FHansaGenerationJobController&, TSharedPtr<FJsonObject>&)
		{
			Gate->Started->Trigger();
			const bool bReleased = Gate->Release->Wait(5000);
			Gate->Finished->Trigger();
			return bReleased;
		}, [&bCompleted](bool bOk, TSharedPtr<FJsonObject>){ bCompleted = bOk && IsInGameThread(); });
	TestTrue(TEXT("Worker starts independently of Slate"), Gate->Started->Wait(2000));
	TestTrue(TEXT("Panel remains busy while worker is blocked"), Panel->bBusy);
	TestFalse(TEXT("Completion does not run on the worker thread"), bCompleted);
	Gate->Release->Trigger();
	TestTrue(TEXT("Worker finishes after release"), Gate->Finished->Wait(2000));
	const double Deadline = FPlatformTime::Seconds() + 2.0;
	while (!Panel->PendingOperation.IsReady() && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
	Panel->PollOperation(FPlatformTime::Seconds(), 0.1f);
	TestTrue(TEXT("Completion is delivered on the editor thread"), bCompleted);
	TestFalse(TEXT("Completed operation releases the busy state"), Panel->bBusy);

	const auto CloseGate = MakeShared<FGate, ESPMode::ThreadSafe>();
	Panel->StartOperation([CloseGate](FHansaGenerationJobController&, TSharedPtr<FJsonObject>&)
		{
			CloseGate->Started->Trigger();
			const bool bReleased = CloseGate->Release->Wait(5000);
			CloseGate->Finished->Trigger();
			return bReleased;
		}, [](bool, TSharedPtr<FJsonObject>){});
	TestTrue(TEXT("Second operation starts"), CloseGate->Started->Wait(2000));
	TWeakPtr<SHansaGenerationJobsPanel> WeakPanel = Panel;
	Panel.Reset();
	TestFalse(TEXT("Pending I/O does not retain the closed panel"), WeakPanel.IsValid());
	CloseGate->Release->Trigger();
	TestTrue(TEXT("Pending I/O safely finishes after tab closure"), CloseGate->Finished->Wait(2000));
	return !HasAnyErrors();
}

#endif
