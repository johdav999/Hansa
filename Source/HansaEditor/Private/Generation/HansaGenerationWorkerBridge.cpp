#include "Generation/HansaGenerationWorkerBridge.h"

#include "Algo/AllOf.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <wincred.h>
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace Hansa::Editor::Generation
{
	namespace
	{
		constexpr uint32 MaximumFrameBytes = 1024 * 1024;
		constexpr uint32 FrameHeaderBytes = 4;
		constexpr int32 ConnectionTimeoutMs = 3000;

		bool IsSafePipeName(const FString& Value)
		{
			if (Value.IsEmpty() || Value.Len() > 128) return false;
			for (const TCHAR Character : Value)
			{
				if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('_') || Character == TEXT('-'))) return false;
			}
			return true;
		}

		bool IsValidToken(const FString& Token)
		{
			if (Token.Len() < 16 || Token.Len() > 128) return false;
			for (const TCHAR Character : Token) if (FChar::IsWhitespace(Character)) return false;
			return true;
		}

		FString Serialize(const TSharedRef<FJsonObject>& Json)
		{
			FString Text;
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
			FJsonSerializer::Serialize(Json, Writer);
			return Text;
		}

		FHansaWorkerError ParseWorkerError(const TSharedPtr<FJsonObject>& Json)
		{
			FHansaWorkerError Result;
			if (!Json.IsValid())
			{
				Result.Code = TEXT("MalformedWorkerResponse");
				Result.Message = TEXT("The worker returned an invalid error object.");
				return Result;
			}
			Json->TryGetStringField(TEXT("code"), Result.Code);
			Json->TryGetStringField(TEXT("message"), Result.Message);
			Json->TryGetStringField(TEXT("remedy"), Result.Remedy);
			Json->TryGetBoolField(TEXT("retryable"), Result.bRetryable);
			return Result;
		}

		TSharedPtr<FJsonObject> ObjectField(const TSharedRef<FJsonObject>& Json, const TCHAR* Name)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			return Json->TryGetObjectField(Name, Object) && Object != nullptr ? *Object : nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* ArrayField(const TSharedRef<FJsonObject>& Json, const TCHAR* Name)
		{
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			return Json->TryGetArrayField(Name, Array) ? Array : nullptr;
		}

		void GetInt64(const TSharedPtr<FJsonObject>& Json, const TCHAR* Name, int64& OutValue)
		{
			double Value = 0.0;
			if (Json.IsValid() && Json->TryGetNumberField(Name, Value) && FMath::IsFinite(Value)) OutValue = static_cast<int64>(Value);
		}
		bool ComputeSha256(const TArray<uint8>& Bytes, FString& OutHex)
		{
#if PLATFORM_WINDOWS
			BCRYPT_ALG_HANDLE Algorithm = nullptr;
			if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
			uint8 Digest[32] = {};
			const NTSTATUS Status = BCryptHash(Algorithm, nullptr, 0,
				const_cast<PUCHAR>(Bytes.GetData()), static_cast<ULONG>(Bytes.Num()), Digest, UE_ARRAY_COUNT(Digest));
			BCryptCloseAlgorithmProvider(Algorithm, 0);
			if (Status < 0) return false;
			static const TCHAR Hex[] = TEXT("0123456789abcdef");
			OutHex.Reset(64);
			for (const uint8 Byte : Digest)
			{
				OutHex.AppendChar(Hex[Byte >> 4]);
				OutHex.AppendChar(Hex[Byte & 0x0f]);
			}
			return true;
#else
			return false;
#endif
		}

#if PLATFORM_WINDOWS
		bool TransferAll(const HANDLE Pipe, uint8* Data, uint32 Size, const bool bWrite, const ULONGLONG Deadline)
		{
			while (Size > 0)
			{
				const ULONGLONG Now = GetTickCount64();
				if (Now >= Deadline) return false;
				OVERLAPPED Operation = {};
				Operation.hEvent = CreateEventW(nullptr, true, false, nullptr);
				if (!Operation.hEvent) return false;
				DWORD Transferred = 0;
				BOOL Completed = bWrite ? WriteFile(Pipe, Data, Size, &Transferred, &Operation) :
					ReadFile(Pipe, Data, Size, &Transferred, &Operation);
				if (!Completed && GetLastError() == ERROR_IO_PENDING)
				{
					if (WaitForSingleObject(Operation.hEvent, static_cast<DWORD>(Deadline - Now)) == WAIT_OBJECT_0)
						Completed = GetOverlappedResult(Pipe, &Operation, &Transferred, false);
					else
					{
						CancelIoEx(Pipe, &Operation);
						// Drain cancellation before the stack OVERLAPPED and buffer go away.
						GetOverlappedResult(Pipe, &Operation, &Transferred, true);
					}
				}
				CloseHandle(Operation.hEvent);
				if (!Completed || Transferred == 0) return false;
				Data += Transferred;
				Size -= Transferred;
			}
			return true;
		}
#endif
	}

	bool FHansaGenerationJob::CanCancel() const
	{
		return Status == TEXT("Queued") || Status == TEXT("Running") || Status == TEXT("Downloading") ||
			Status == TEXT("ImportedToStaging") || Status == TEXT("Validating");
	}

	bool FHansaGenerationJob::CanRetry() const
	{
		return Status == TEXT("Failed") || Status == TEXT("Cancelled") || Status == TEXT("Expired");
	}

	bool FHansaGenerationWorkerCredentialSource::ResolveToken(FString& OutToken, FString& OutSource, FHansaWorkerError& OutError)
	{
		OutToken = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_GENERATION_WORKER_TOKEN"));
		if (IsValidToken(OutToken))
		{
			OutSource = TEXT("worker environment");
			return true;
		}
		OutToken.Reset();

#if PLATFORM_WINDOWS
		PCREDENTIALW Credential = nullptr;
		if (CredReadW(L"Hansa/GenerationWorker", CRED_TYPE_GENERIC, 0, &Credential) != 0 && Credential != nullptr)
		{
			if (Credential->CredentialBlob != nullptr && Credential->CredentialBlobSize > 0 && Credential->CredentialBlobSize <= 128 * sizeof(wchar_t))
			{
				const int32 CharacterCount = static_cast<int32>(Credential->CredentialBlobSize / sizeof(wchar_t));
				OutToken = FString(CharacterCount, reinterpret_cast<const TCHAR*>(Credential->CredentialBlob));
				OutToken.TrimStartAndEndInline();
			}
			CredFree(Credential);
			if (IsValidToken(OutToken))
			{
				OutSource = TEXT("Windows Credential Manager");
				return true;
			}
			OutToken.Reset();
		}
#endif

		OutError = { TEXT("CredentialUnavailable"), TEXT("Generation worker authentication is unavailable."),
			TEXT("Set HANSA_GENERATION_WORKER_TOKEN for both processes or store a generic credential named Hansa/GenerationWorker."), false };
		return false;
	}

	FHansaGenerationWorkerNamedPipeTransport::FHansaGenerationWorkerNamedPipeTransport(FString InPipeName)
		: PipeName(MoveTemp(InPipeName))
	{
		if (PipeName.IsEmpty()) PipeName = FPlatformMisc::GetEnvironmentVariable(TEXT("HANSA_GENERATION_WORKER_PIPE"));
		if (PipeName.IsEmpty()) PipeName = TEXT("hansa-generation-worker-v1");
	}

	bool FHansaGenerationWorkerNamedPipeTransport::Request(const FString& Operation, const TSharedRef<FJsonObject>& Payload,
		TSharedPtr<FJsonObject>& OutResult, FHansaWorkerError& OutError)
	{
		OutResult.Reset();
		OutError = {};
		if (!IsSafePipeName(PipeName))
		{
			OutError = { TEXT("InvalidPipeConfiguration"), TEXT("The generation worker pipe name is invalid."),
				TEXT("Use only letters, digits, period, underscore, or hyphen in HANSA_GENERATION_WORKER_PIPE."), false };
			return false;
		}
		FString Token;
		FString CredentialSource;
		if (!FHansaGenerationWorkerCredentialSource::ResolveToken(Token, CredentialSource, OutError)) return false;

		const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetStringField(TEXT("protocol"), TEXT("hansa.generation.worker"));
		Envelope->SetStringField(TEXT("version"), TEXT("1.0"));
		Envelope->SetStringField(TEXT("requestId"), RequestId);
		Envelope->SetStringField(TEXT("authToken"), Token);
		Envelope->SetStringField(TEXT("operation"), Operation);
		Envelope->SetObjectField(TEXT("payload"), Payload);
		const FString RequestText = Serialize(Envelope);
		Token.Reset();

		FTCHARToUTF8 Utf8(*RequestText);
		if (Utf8.Length() <= 0 || Utf8.Length() > static_cast<int32>(MaximumFrameBytes))
		{
			OutError = { TEXT("FrameTooLarge"), TEXT("The generation worker request exceeds the 1 MiB protocol limit."),
				TEXT("Remove input files or shorten the prompt before retrying."), false };
			return false;
		}

#if PLATFORM_WINDOWS
		const FString FullPipeName = FString::Printf(TEXT("\\\\.\\pipe\\%s"), *PipeName);
		if (WaitNamedPipeW(*FullPipeName, ConnectionTimeoutMs) == 0)
		{
			OutError = { TEXT("WorkerUnavailable"), TEXT("The generation worker is not accepting connections."),
				TEXT("Start Tools/HansaGenerationWorker with the same pipe name and authentication token."), true };
			return false;
		}
		const HANDLE Pipe = CreateFileW(*FullPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (Pipe == INVALID_HANDLE_VALUE)
		{
			OutError = { TEXT("WorkerUnavailable"), TEXT("The generation worker pipe could not be opened."),
				TEXT("Verify that the local worker is running and the pipe name matches."), true };
			return false;
		}
		const ULONGLONG Deadline = GetTickCount64() + ConnectionTimeoutMs;
		uint8 Header[FrameHeaderBytes] = {};
		const uint32 PayloadLength = static_cast<uint32>(Utf8.Length());
		Header[0] = static_cast<uint8>(PayloadLength & 0xff);
		Header[1] = static_cast<uint8>((PayloadLength >> 8) & 0xff);
		Header[2] = static_cast<uint8>((PayloadLength >> 16) & 0xff);
		Header[3] = static_cast<uint8>((PayloadLength >> 24) & 0xff);
		const bool bWrote = TransferAll(Pipe, Header, FrameHeaderBytes, true, Deadline) && TransferAll(Pipe, reinterpret_cast<uint8*>(const_cast<char*>(Utf8.Get())), PayloadLength, true, Deadline);
		if (!bWrote || !TransferAll(Pipe, Header, FrameHeaderBytes, false, Deadline))
		{
			CloseHandle(Pipe);
			OutError = { TEXT("WorkerIoFailed"), TEXT("The generation worker connection closed or timed out before a response arrived."),
				TEXT("Refresh the worker connection and retry the operation."), true };
			return false;
		}
		const uint32 ResponseLength = static_cast<uint32>(Header[0]) | (static_cast<uint32>(Header[1]) << 8) |
			(static_cast<uint32>(Header[2]) << 16) | (static_cast<uint32>(Header[3]) << 24);
		if (ResponseLength == 0 || ResponseLength > MaximumFrameBytes)
		{
			CloseHandle(Pipe);
			OutError = { TEXT("InvalidFrame"), TEXT("The generation worker returned an invalid frame length."), FString(), false };
			return false;
		}
		TArray<uint8> ResponseBytes;
		ResponseBytes.SetNumUninitialized(ResponseLength + 1);
		if (!TransferAll(Pipe, ResponseBytes.GetData(), ResponseLength, false, Deadline))
		{
			CloseHandle(Pipe);
			OutError = { TEXT("WorkerIoFailed"), TEXT("The generation worker response was incomplete or timed out."),
				TEXT("Refresh the worker connection and retry the operation."), true };
			return false;
		}
		CloseHandle(Pipe);
		ResponseBytes[ResponseLength] = 0;
		const FString ResponseText = UTF8_TO_TCHAR(reinterpret_cast<const char*>(ResponseBytes.GetData()));
#else
		OutError = { TEXT("UnsupportedPlatform"), TEXT("The local generation worker pipe is implemented for Windows editor hosts."), FString(), false };
		return false;
#endif

		TSharedPtr<FJsonObject> Response;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseText);
		if (!FJsonSerializer::Deserialize(Reader, Response) || !Response.IsValid())
		{
			OutError = { TEXT("MalformedWorkerResponse"), TEXT("The generation worker returned invalid JSON."), FString(), false };
			return false;
		}
		FString Protocol, Version, ResponseRequestId;
		bool bOk = false;
		if (!Response->TryGetStringField(TEXT("protocol"), Protocol) || Protocol != TEXT("hansa.generation.worker") ||
			!Response->TryGetStringField(TEXT("version"), Version) || Version != TEXT("1.0") ||
			!Response->TryGetStringField(TEXT("requestId"), ResponseRequestId) || ResponseRequestId != RequestId ||
			!Response->TryGetBoolField(TEXT("ok"), bOk))
		{
			OutError = { TEXT("MalformedWorkerResponse"), TEXT("The generation worker response did not match the requested protocol envelope."), FString(), false };
			return false;
		}
		if (!bOk)
		{
			OutError = ParseWorkerError(ObjectField(Response.ToSharedRef(), TEXT("error")));
			return false;
		}
		OutResult = ObjectField(Response.ToSharedRef(), TEXT("result"));
		if (!OutResult.IsValid())
		{
			OutError = { TEXT("MalformedWorkerResponse"), TEXT("The generation worker success response did not contain a result object."), FString(), false };
			return false;
		}
		return true;
	}

	FHansaGenerationJobController::FHansaGenerationJobController(TSharedRef<IHansaGenerationWorkerTransport> InTransport)
		: Transport(MakeShared<FTransportOwner, ESPMode::ThreadSafe>(MoveTemp(InTransport)))
	{
	}

	void FHansaGenerationJobController::SetError(FString Code, FString Message, FString Remedy, const bool bRetryable)
	{
		LastError = { MoveTemp(Code), MoveTemp(Message), MoveTemp(Remedy), bRetryable };
	}

	bool FHansaGenerationJobController::RefreshCapabilities()
	{
		LastError = {};
		bEstimateReady = false;
		TSharedPtr<FJsonObject> Result;
		if (!Transport->Instance->Request(TEXT("worker.capabilities"), MakeShared<FJsonObject>(), Result, LastError)) return false;
		Capabilities.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Providers = ArrayField(Result.ToSharedRef(), TEXT("providers"));
		if (Providers == nullptr)
		{
			SetError(TEXT("MalformedCapabilities"), TEXT("The worker capability response did not contain providers."));
			return false;
		}
		for (const TSharedPtr<FJsonValue>& ProviderValue : *Providers)
		{
			const TSharedPtr<FJsonObject> Provider = ProviderValue.IsValid() ? ProviderValue->AsObject() : nullptr;
			if (!Provider.IsValid()) continue;
			FString ProviderId, AdapterVersion, ModelVersion;
			Provider->TryGetStringField(TEXT("providerId"), ProviderId);
			Provider->TryGetStringField(TEXT("adapterVersion"), AdapterVersion);
			if (const TArray<TSharedPtr<FJsonValue>>* Models = ArrayField(Provider.ToSharedRef(), TEXT("models")); Models && Models->Num() > 0)
			{
				if (const TSharedPtr<FJsonObject> Model = (*Models)[0]->AsObject(); Model.IsValid()) Model->TryGetStringField(TEXT("modelVersion"), ModelVersion);
			}
			if (const TArray<TSharedPtr<FJsonValue>>* Items = ArrayField(Provider.ToSharedRef(), TEXT("capabilities")))
			{
				for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
				{
					const TSharedPtr<FJsonObject> Item = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
					if (!Item.IsValid()) continue;
					FHansaProviderCapability Capability;
					Capability.ProviderId = ProviderId;
					Capability.AdapterVersion = AdapterVersion;
					Capability.ModelVersion = ModelVersion;
					Item->TryGetStringField(TEXT("capability"), Capability.Capability);
					Item->TryGetBoolField(TEXT("supportsCancellation"), Capability.bSupportsCancellation);
					Item->TryGetBoolField(TEXT("supportsSeed"), Capability.bSupportsSeed);
					if (const TArray<TSharedPtr<FJsonValue>>* Inputs = ArrayField(Item.ToSharedRef(), TEXT("inputMediaTypes")))
						for (const TSharedPtr<FJsonValue>& Value : *Inputs) Capability.InputMediaTypes.Add(Value->AsString());
					if (const TArray<TSharedPtr<FJsonValue>>* Outputs = ArrayField(Item.ToSharedRef(), TEXT("outputMediaTypes")))
						for (const TSharedPtr<FJsonValue>& Value : *Outputs) Capability.OutputMediaTypes.Add(Value->AsString());
					Capabilities.Add(MoveTemp(Capability));
				}
			}
		}
		return true;
	}

	bool FHansaGenerationJobController::RefreshJobs()
	{
		LastError = {};
		TSharedPtr<FJsonObject> Result;
		if (!Transport->Instance->Request(TEXT("job.list"), MakeShared<FJsonObject>(), Result, LastError)) return false;
		const TArray<TSharedPtr<FJsonValue>>* JobValues = ArrayField(Result.ToSharedRef(), TEXT("jobs"));
		if (JobValues == nullptr)
		{
			SetError(TEXT("MalformedJobList"), TEXT("The worker job list response did not contain jobs."));
			return false;
		}
		TArray<FHansaGenerationJob> Parsed;
		for (const TSharedPtr<FJsonValue>& Value : *JobValues)
		{
			const TSharedPtr<FJsonObject> JobJson = Value.IsValid() ? Value->AsObject() : nullptr;
			FHansaGenerationJob Job;
			FHansaWorkerError ParseError;
			if (!JobJson.IsValid() || !ParseJob(JobJson.ToSharedRef(), Job, ParseError))
			{
				LastError = MoveTemp(ParseError);
				return false;
			}
			Parsed.Add(MoveTemp(Job));
		}
		Parsed.Sort([](const FHansaGenerationJob& Left, const FHansaGenerationJob& Right)
		{
			return Left.CreatedAt.Compare(Right.CreatedAt, ESearchCase::CaseSensitive) > 0;
		});
		Jobs = MoveTemp(Parsed);
		return true;
	}

	FString FHansaGenerationJobController::MediaTypeForPath(const FString& Path)
	{
		const FString Extension = FPaths::GetExtension(Path).ToLower();
		if (Extension == TEXT("json")) return TEXT("application/json");
		if (Extension == TEXT("txt") || Extension == TEXT("md")) return TEXT("text/plain");
		if (Extension == TEXT("png")) return TEXT("image/png");
		if (Extension == TEXT("jpg") || Extension == TEXT("jpeg")) return TEXT("image/jpeg");
		if (Extension == TEXT("wav")) return TEXT("audio/wav");
		return TEXT("application/octet-stream");
	}

	bool FHansaGenerationJobController::PreviewUploads(const TArray<FString>& AbsolutePaths, const FString& InRightsDeclaration)
	{
		LastError = {};
		bEstimateReady = false;
		if (AbsolutePaths.Num() > 16)
		{
			SetError(TEXT("TooManyInputs"), TEXT("A generation request may contain at most 16 input files."));
			return false;
		}
		TArray<FHansaUploadPreview> NewUploads;
		for (const FString& RequestedPath : AbsolutePaths)
		{
			const FString FullPath = FPaths::ConvertRelativePathToFull(RequestedPath);
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *FullPath))
			{
				SetError(TEXT("InputUnreadable"), FString::Printf(TEXT("Input file cannot be read: %s"), *FullPath),
					TEXT("Remove the file or restore read access before queueing."));
				return false;
			}
			if (Bytes.Num() > 16 * 1024 * 1024)
			{
				SetError(TEXT("InputTooLarge"), FString::Printf(TEXT("Input file exceeds 16 MiB: %s"), *FullPath));
				return false;
			}
			FString Hash;
			if (!ComputeSha256(Bytes, Hash))
			{
				SetError(TEXT("InputHashFailed"), FString::Printf(TEXT("Input file could not be hashed: %s"), *FullPath));
				return false;
			}
			FHansaUploadPreview Preview;
			Preview.AbsolutePath = FullPath;
			Preview.DisplayPath = FullPath;
			FPaths::MakePathRelativeTo(Preview.DisplayPath, *FPaths::ProjectDir());
			Preview.MediaType = MediaTypeForPath(FullPath);
			Preview.SizeBytes = Bytes.Num();
			Preview.Sha256 = Hash;
			NewUploads.Add(MoveTemp(Preview));
		}
		Uploads = MoveTemp(NewUploads);
		RightsDeclaration = InRightsDeclaration.Left(512);
		return true;
	}

	FString FHansaGenerationJobController::SubmissionIdentity(const FHansaGenerationSubmission& Submission) const
	{
		FString Identity = FString::Printf(TEXT("%s\x1f%s\x1f%s\x1f%s\x1f%s\x1f%lld\x1f%s\x1f%lld\x1f%d\x1f%s"),
			*Submission.ProviderId, *Submission.ModelVersion, *Submission.Capability, *Submission.IntendedAssetRole,
			*Submission.Prompt, Submission.MaximumCostMinorUnits, *Submission.Currency, Submission.MaximumOutputBytes,
			Submission.TimeoutMs, *RightsDeclaration);
		if (Submission.Parameters.IsValid()) Identity += TEXT("\x1d") + Serialize(Submission.Parameters.ToSharedRef());
		for (const FHansaUploadPreview& Upload : Uploads)
		{
			Identity += FString::Printf(TEXT("\x1e%s\x1f%s\x1f%s\x1f%lld\x1f%s"), *Upload.DisplayPath, *Upload.Role,
				*Upload.MediaType, Upload.SizeBytes, *Upload.Sha256);
		}
		return Identity;
	}

	bool FHansaGenerationJobController::Estimate(const FHansaGenerationSubmission& Submission)
	{
		LastError = {};
		bEstimateReady = false;
		if (Capabilities.IsEmpty()) { SetError(TEXT("EstimateNotReady"), TEXT("Connect to the worker and load provider capabilities.")); return false; }
		if (Submission.Prompt.TrimStartAndEnd().IsEmpty()) { SetError(TEXT("EstimateNotReady"), TEXT("Enter a generation prompt.")); return false; }
		if (!Uploads.IsEmpty() && (!Submission.bRightsAcknowledged || RightsDeclaration.IsEmpty()))
		{
			SetError(TEXT("RightsAcknowledgementRequired"), TEXT("Acknowledge rights for every file in the exact upload preview."));
			return false;
		}
		const bool bAdvertised = Capabilities.ContainsByPredicate([&Submission](const FHansaProviderCapability& Item)
		{
			return Item.ProviderId == Submission.ProviderId && Item.ModelVersion == Submission.ModelVersion && Item.Capability == Submission.Capability;
		});
		if (!bAdvertised) { SetError(TEXT("CapabilityUnavailable"), TEXT("The selected provider, model, and capability are not advertised by the worker.")); return false; }

		TArray<TSharedPtr<FJsonValue>> Inputs;
		for (const FHansaUploadPreview& Upload : Uploads)
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *Upload.AbsolutePath))
			{
				SetError(TEXT("InputChanged"), FString::Printf(TEXT("Input file changed or became unreadable after preview: %s"), *Upload.DisplayPath));
				return false;
			}
			FString Hash;
			if (!ComputeSha256(Bytes, Hash) || Hash != Upload.Sha256)
			{
				SetError(TEXT("InputChanged"), FString::Printf(TEXT("Input file changed after preview: %s"), *Upload.DisplayPath),
					TEXT("Refresh the exact upload preview before estimating."));
				return false;
			}
			TSharedRef<FJsonObject> Input = MakeShared<FJsonObject>();
			Input->SetStringField(TEXT("role"), Upload.Role);
			Input->SetStringField(TEXT("mediaType"), Upload.MediaType);
			Input->SetStringField(TEXT("contentBase64"), FBase64::Encode(Bytes));
			Input->SetStringField(TEXT("rightsDeclaration"), RightsDeclaration);
			Inputs.Add(MakeShared<FJsonValueObject>(Input));
		}
		TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetNumberField(TEXT("schemaVersion"), 1);
		Request->SetStringField(TEXT("idempotencyKey"), FString::Printf(TEXT("editor-estimate-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Request->SetStringField(TEXT("providerId"), Submission.ProviderId);
		Request->SetStringField(TEXT("modelVersion"), Submission.ModelVersion);
		Request->SetStringField(TEXT("capability"), Submission.Capability);
		Request->SetStringField(TEXT("intendedAssetRole"), Submission.IntendedAssetRole);
		Request->SetStringField(TEXT("prompt"), Submission.Prompt.Left(16384));
		Request->SetArrayField(TEXT("inputArtifacts"), MoveTemp(Inputs));
		TSharedRef<FJsonObject> Rights = MakeShared<FJsonObject>();
		Rights->SetBoolField(TEXT("acknowledged"), Submission.bRightsAcknowledged || Uploads.IsEmpty());
		Rights->SetStringField(TEXT("declaration"), RightsDeclaration);
		Request->SetObjectField(TEXT("rights"), Rights);
		TSharedRef<FJsonObject> Budgets = MakeShared<FJsonObject>();
		Budgets->SetNumberField(TEXT("maximumCostMinorUnits"), static_cast<double>(Submission.MaximumCostMinorUnits));
		Budgets->SetStringField(TEXT("currency"), Submission.Currency);
		Budgets->SetNumberField(TEXT("maximumOutputBytes"), static_cast<double>(Submission.MaximumOutputBytes));
		Request->SetObjectField(TEXT("budgets"), Budgets);
		TSharedRef<FJsonObject> OutputContract = MakeShared<FJsonObject>();
		OutputContract->SetNumberField(TEXT("version"), 1);
		OutputContract->SetNumberField(TEXT("maximumArtifacts"), 1);
		TArray<TSharedPtr<FJsonValue>> AllowedMediaTypes;
		AllowedMediaTypes.Add(MakeShared<FJsonValueString>(TEXT("application/json")));
		OutputContract->SetArrayField(TEXT("allowedMediaTypes"), MoveTemp(AllowedMediaTypes));
		Request->SetObjectField(TEXT("outputContract"), OutputContract);
		if (Submission.Parameters.IsValid()) Request->SetObjectField(TEXT("parameters"), Submission.Parameters.ToSharedRef());
		Request->SetNumberField(TEXT("timeoutMs"), Submission.TimeoutMs);
		TSharedRef<FJsonObject> Spend = MakeShared<FJsonObject>();
		Spend->SetBoolField(TEXT("approved"), false);
		Request->SetObjectField(TEXT("spendApproval"), Spend);
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("request"), Request);
		TSharedPtr<FJsonObject> Result;
		if (!Transport->Instance->Request(TEXT("job.estimate"), Payload, Result, LastError)) return false;
		const TSharedPtr<FJsonObject> EstimateJson = ObjectField(Result.ToSharedRef(), TEXT("estimate"));
		if (!EstimateJson.IsValid()) { SetError(TEXT("MalformedEstimate"), TEXT("The worker estimate response did not contain an estimate.")); return false; }
		double EstimateValue = 0.0;
		FString Currency, RequestHash;
		if (!EstimateJson->TryGetNumberField(TEXT("estimatedMinorUnits"), EstimateValue) ||
			!FMath::IsFinite(EstimateValue) || EstimateValue < 0.0 || EstimateValue > 9007199254740991.0 ||
			FMath::FloorToDouble(EstimateValue) != EstimateValue || EstimateValue > static_cast<double>(Submission.MaximumCostMinorUnits) ||
			!EstimateJson->TryGetStringField(TEXT("currency"), Currency) || Currency != Submission.Currency ||
			!Result->TryGetStringField(TEXT("requestHash"), RequestHash) || RequestHash.Len() != 64 ||
			!Algo::AllOf(RequestHash, [](TCHAR Character){ return FChar::IsHexDigit(Character); }))
		{
			SetError(TEXT("MalformedEstimate"), TEXT("The worker estimate has invalid cost, currency, or request hash."),
				TEXT("Refresh capabilities and obtain a valid estimate within the hard budget."));
			return false;
		}
		EstimatedMinorUnits = static_cast<int64>(EstimateValue);
		EstimateCurrency = MoveTemp(Currency);
		EstimatedRequestHash = MoveTemp(RequestHash);
		EstimatedIdentity = SubmissionIdentity(Submission);
		bEstimateReady = !EstimateCurrency.IsEmpty() && !EstimatedRequestHash.IsEmpty();
		return bEstimateReady;
	}
	bool FHansaGenerationJobController::CanSubmit(const FHansaGenerationSubmission& Submission, FString& OutReason) const
	{
		if (Capabilities.IsEmpty()) { OutReason = TEXT("Connect to the worker and load provider capabilities."); return false; }
		if (!bEstimateReady) { OutReason = TEXT("Estimate this exact request before approving spend."); return false; }
		if (EstimatedIdentity != SubmissionIdentity(Submission)) { OutReason = TEXT("The request changed after estimation; estimate it again."); return false; }
		if (Submission.Prompt.TrimStartAndEnd().IsEmpty()) { OutReason = TEXT("Enter a generation prompt."); return false; }
		if (Submission.ApprovedBy.TrimStartAndEnd().IsEmpty()) { OutReason = TEXT("Identify the person approving the spend."); return false; }
		if (!Submission.bSpendApproved) { OutReason = TEXT("Confirm the maximum spend for this submission."); return false; }
		if (!Uploads.IsEmpty() && (!Submission.bRightsAcknowledged || RightsDeclaration.IsEmpty()))
		{
			OutReason = TEXT("Acknowledge rights for every file in the exact upload preview.");
			return false;
		}
		if (Submission.MaximumCostMinorUnits < 0 || Submission.MaximumOutputBytes < 1)
		{
			OutReason = TEXT("Enter valid cost and output byte limits.");
			return false;
		}
		const bool bAdvertised = Capabilities.ContainsByPredicate([&Submission](const FHansaProviderCapability& Item)
		{
			return Item.ProviderId == Submission.ProviderId && Item.ModelVersion == Submission.ModelVersion && Item.Capability == Submission.Capability;
		});
		if (!bAdvertised) { OutReason = TEXT("Select a provider, model, and capability advertised by the connected worker."); return false; }
		OutReason.Reset();
		return true;
	}

	bool FHansaGenerationJobController::Submit(const FHansaGenerationSubmission& Submission)
	{
		FString Reason;
		if (!CanSubmit(Submission, Reason))
		{
			SetError(TEXT("SubmissionNotReady"), Reason);
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Inputs;
		for (const FHansaUploadPreview& Upload : Uploads)
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *Upload.AbsolutePath))
			{
				SetError(TEXT("InputChanged"), FString::Printf(TEXT("Input file changed or became unreadable after preview: %s"), *Upload.DisplayPath),
					TEXT("Refresh the exact upload preview before queueing."));
				return false;
			}
			FString Hash;
			if (!ComputeSha256(Bytes, Hash) || Hash != Upload.Sha256)
			{
				SetError(TEXT("InputChanged"), FString::Printf(TEXT("Input file changed after preview: %s"), *Upload.DisplayPath),
					TEXT("Refresh the exact upload preview before queueing."));
				return false;
			}
			TSharedRef<FJsonObject> Input = MakeShared<FJsonObject>();
			Input->SetStringField(TEXT("role"), Upload.Role);
			Input->SetStringField(TEXT("mediaType"), Upload.MediaType);
			Input->SetStringField(TEXT("contentBase64"), FBase64::Encode(Bytes));
			Input->SetStringField(TEXT("rightsDeclaration"), RightsDeclaration);
			Inputs.Add(MakeShared<FJsonValueObject>(Input));
		}

		TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetNumberField(TEXT("schemaVersion"), 1);
		Request->SetStringField(TEXT("idempotencyKey"), FString::Printf(TEXT("editor-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Request->SetStringField(TEXT("providerId"), Submission.ProviderId);
		Request->SetStringField(TEXT("modelVersion"), Submission.ModelVersion);
		Request->SetStringField(TEXT("capability"), Submission.Capability);
		Request->SetStringField(TEXT("intendedAssetRole"), Submission.IntendedAssetRole);
		Request->SetStringField(TEXT("prompt"), Submission.Prompt.Left(16384));
		Request->SetArrayField(TEXT("inputArtifacts"), MoveTemp(Inputs));
		TSharedRef<FJsonObject> Rights = MakeShared<FJsonObject>();
		Rights->SetBoolField(TEXT("acknowledged"), Submission.bRightsAcknowledged || Uploads.IsEmpty());
		Rights->SetStringField(TEXT("declaration"), RightsDeclaration);
		Request->SetObjectField(TEXT("rights"), Rights);
		TSharedRef<FJsonObject> Budgets = MakeShared<FJsonObject>();
		Budgets->SetNumberField(TEXT("maximumCostMinorUnits"), static_cast<double>(Submission.MaximumCostMinorUnits));
		Budgets->SetStringField(TEXT("currency"), Submission.Currency);
		Budgets->SetNumberField(TEXT("maximumOutputBytes"), static_cast<double>(Submission.MaximumOutputBytes));
		Request->SetObjectField(TEXT("budgets"), Budgets);
		TSharedRef<FJsonObject> OutputContract = MakeShared<FJsonObject>();
		OutputContract->SetNumberField(TEXT("version"), 1);
		OutputContract->SetNumberField(TEXT("maximumArtifacts"), 1);
		TArray<TSharedPtr<FJsonValue>> AllowedMediaTypes;
		AllowedMediaTypes.Add(MakeShared<FJsonValueString>(TEXT("application/json")));
		OutputContract->SetArrayField(TEXT("allowedMediaTypes"), MoveTemp(AllowedMediaTypes));
		Request->SetObjectField(TEXT("outputContract"), OutputContract);
		if (Submission.Parameters.IsValid()) Request->SetObjectField(TEXT("parameters"), Submission.Parameters.ToSharedRef());
		Request->SetNumberField(TEXT("timeoutMs"), Submission.TimeoutMs);
		TSharedRef<FJsonObject> Spend = MakeShared<FJsonObject>();
		Spend->SetBoolField(TEXT("approved"), true);
		Spend->SetStringField(TEXT("approvedBy"), Submission.ApprovedBy.Left(128));
		Spend->SetStringField(TEXT("approvedAt"), FDateTime::UtcNow().ToIso8601());
		Request->SetObjectField(TEXT("spendApproval"), Spend);
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("request"), Request);
		// A dispatched request may have been accepted even when the response is lost.
		// Consume approval before I/O so a second click cannot create another billable job.
		InvalidateEstimate();
		return SendJobOperation(TEXT("job.submit"), Payload);
	}

	bool FHansaGenerationJobController::Cancel(const FString& JobId)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("jobId"), JobId);
		return SendJobOperation(TEXT("job.cancel"), Payload);
	}

	bool FHansaGenerationJobController::Retry(const FString& JobId, const FString& ApprovedBy, const bool bSpendApproved)
	{
		if (!bSpendApproved || ApprovedBy.TrimStartAndEnd().IsEmpty())
		{
			SetError(TEXT("SpendApprovalRequired"), TEXT("Retry requires a fresh identified spend approval."));
			return false;
		}
		TSharedRef<FJsonObject> Spend = MakeShared<FJsonObject>();
		Spend->SetBoolField(TEXT("approved"), true);
		Spend->SetStringField(TEXT("approvedBy"), ApprovedBy.Left(128));
		Spend->SetStringField(TEXT("approvedAt"), FDateTime::UtcNow().ToIso8601());
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("jobId"), JobId);
		Payload->SetStringField(TEXT("retryIdempotencyKey"), FString::Printf(TEXT("editor-retry-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Payload->SetStringField(TEXT("reason"), TEXT("Explicit retry from Hansa Authoring Studio"));
		Payload->SetObjectField(TEXT("spendApproval"), Spend);
		return SendJobOperation(TEXT("job.retry"), Payload);
	}

	bool FHansaGenerationJobController::ReadJsonOutput(const FString& JobId, const int32 OutputIndex, TSharedPtr<FJsonObject>& OutDocument)
	{
		LastError = {};
		OutDocument.Reset();
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("jobId"), JobId);
		Payload->SetNumberField(TEXT("outputIndex"), OutputIndex);
		TSharedPtr<FJsonObject> Result;
		if (!Transport->Instance->Request(TEXT("job.output.read"), Payload, Result, LastError)) return false;
		const TSharedPtr<FJsonObject>* Document = nullptr;
		if (!Result.IsValid() || !Result->TryGetObjectField(TEXT("document"), Document) || Document == nullptr)
		{
			SetError(TEXT("MalformedWorkerResponse"), TEXT("Worker proposal read did not contain one JSON document."));
			return false;
		}
		OutDocument = *Document;
		return true;
	}
	bool FHansaGenerationJobController::SendJobOperation(const FString& Operation, const TSharedRef<FJsonObject>& Payload)
	{
		LastError = {};
		TSharedPtr<FJsonObject> Result;
		if (!Transport->Instance->Request(Operation, Payload, Result, LastError)) return false;
		return RefreshJobs();
	}

	bool FHansaGenerationJobController::ParseJob(const TSharedRef<FJsonObject>& Json, FHansaGenerationJob& OutJob, FHansaWorkerError& OutError)
	{
		if (!Json->TryGetStringField(TEXT("jobId"), OutJob.JobId) || !Json->TryGetStringField(TEXT("status"), OutJob.Status))
		{
			OutError = { TEXT("MalformedJob"), TEXT("A worker job is missing its stable ID or status."), FString(), false };
			return false;
		}
		Json->TryGetStringField(TEXT("createdAt"), OutJob.CreatedAt);
		Json->TryGetStringField(TEXT("requestHash"), OutJob.RequestHash);
		if (const TSharedPtr<FJsonObject> Provider = ObjectField(Json, TEXT("provider")))
		{
			Provider->TryGetStringField(TEXT("providerId"), OutJob.ProviderId);
			Provider->TryGetStringField(TEXT("modelVersion"), OutJob.ModelVersion);
		}
		if (const TSharedPtr<FJsonObject> Request = ObjectField(Json, TEXT("request")))
		{
			Request->TryGetStringField(TEXT("capability"), OutJob.Capability);
			GetInt64(ObjectField(Request.ToSharedRef(), TEXT("budgets")), TEXT("maximumCostMinorUnits"), OutJob.MaximumCostMinorUnits);
		}
		if (const TSharedPtr<FJsonObject> Progress = ObjectField(Json, TEXT("progress")))
		{
			Progress->TryGetStringField(TEXT("message"), OutJob.ProgressMessage);
			Progress->TryGetNumberField(TEXT("percent"), OutJob.ProgressPercent);
		}
		if (const TSharedPtr<FJsonObject> Estimate = ObjectField(Json, TEXT("estimate")))
		{
			GetInt64(Estimate, TEXT("estimatedMinorUnits"), OutJob.EstimatedMinorUnits);
			Estimate->TryGetStringField(TEXT("currency"), OutJob.Currency);
		}
		if (const TSharedPtr<FJsonObject> Usage = ObjectField(Json, TEXT("actualUsage")))
		{
			GetInt64(Usage, TEXT("actualMinorUnits"), OutJob.ActualMinorUnits);
			Usage->TryGetStringField(TEXT("currency"), OutJob.Currency);
		}
		if (const TSharedPtr<FJsonObject> Provenance = ObjectField(Json, TEXT("provenance")))
		{
			Provenance->TryGetStringField(TEXT("adapterVersion"), OutJob.AdapterVersion);
			Provenance->TryGetStringField(TEXT("providerJobId"), OutJob.ProviderJobId);
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Outputs = ArrayField(Json, TEXT("outputs")))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Outputs)
			{
				const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Item.IsValid()) continue;
				FHansaOutputArtifact Artifact;
				Item->TryGetStringField(TEXT("logicalName"), Artifact.LogicalName);
				Item->TryGetStringField(TEXT("mediaType"), Artifact.MediaType);
				Item->TryGetStringField(TEXT("relativePath"), Artifact.RelativePath);
				Item->TryGetStringField(TEXT("sha256"), Artifact.Sha256);
				GetInt64(Item, TEXT("sizeBytes"), Artifact.SizeBytes);
				OutJob.Outputs.Add(MoveTemp(Artifact));
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Qa = ArrayField(Json, TEXT("qaResults")))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Qa)
			{
				const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Item.IsValid()) continue;
				FString Check;
				bool bPassed = false;
				Item->TryGetStringField(TEXT("check"), Check);
				Item->TryGetBoolField(TEXT("passed"), bPassed);
				OutJob.QaResults.Add(FString::Printf(TEXT("%s %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Check));
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Errors = ArrayField(Json, TEXT("errors")); Errors && Errors->Num() > 0)
			OutJob.Error = ParseWorkerError((*Errors)[0]->AsObject());
		if (const TArray<TSharedPtr<FJsonValue>>* Manifests = ArrayField(Json, TEXT("manifests")); Manifests && Manifests->Num() > 0)
		{
			const TSharedPtr<FJsonObject> Manifest = Manifests->Last()->AsObject();
			if (Manifest.IsValid()) Manifest->TryGetStringField(TEXT("sha256"), OutJob.ManifestSha256);
		}
		return true;
	}
}

