#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Hansa::Editor::Generation
{
	struct FHansaWorkerError final
	{
		FString Code;
		FString Message;
		FString Remedy;
		bool bRetryable = false;
		bool IsSet() const { return !Code.IsEmpty() || !Message.IsEmpty(); }
	};

	struct FHansaProviderCapability final
	{
		FString ProviderId;
		FString AdapterVersion;
		FString ModelVersion;
		FString Capability;
		TArray<FString> InputMediaTypes;
		TArray<FString> OutputMediaTypes;
		bool bSupportsCancellation = false;
		bool bSupportsSeed = false;
	};

	struct FHansaUploadPreview final
	{
		FString AbsolutePath;
		FString DisplayPath;
		FString Role = TEXT("ReferenceInput");
		FString MediaType;
		int64 SizeBytes = 0;
		FString Sha256;
	};

	struct FHansaOutputArtifact final
	{
		FString LogicalName;
		FString MediaType;
		FString RelativePath;
		int64 SizeBytes = 0;
		FString Sha256;
	};

	struct FHansaGenerationJob final
	{
		FString JobId;
		FString Status;
		FString ProviderId;
		FString ModelVersion;
		FString Capability;
		FString CreatedAt;
		FString ProgressMessage;
		double ProgressPercent = 0.0;
		int64 MaximumCostMinorUnits = 0;
		int64 EstimatedMinorUnits = 0;
		int64 ActualMinorUnits = 0;
		FString Currency;
		FString RequestHash;
		FString ManifestSha256;
		FString AdapterVersion;
		FString ProviderJobId;
		TArray<FHansaOutputArtifact> Outputs;
		TArray<FString> QaResults;
		FHansaWorkerError Error;

		bool CanCancel() const;
		bool CanRetry() const;
	};

	class IHansaGenerationWorkerTransport
	{
	public:
		virtual ~IHansaGenerationWorkerTransport() = default;
		virtual bool Request(const FString& Operation, const TSharedRef<FJsonObject>& Payload,
			TSharedPtr<FJsonObject>& OutResult, FHansaWorkerError& OutError) = 0;
	};

	class FHansaGenerationWorkerCredentialSource final
	{
	public:
		static bool ResolveToken(FString& OutToken, FString& OutSource, FHansaWorkerError& OutError);
	};

	class FHansaGenerationWorkerNamedPipeTransport final : public IHansaGenerationWorkerTransport
	{
	public:
		explicit FHansaGenerationWorkerNamedPipeTransport(FString InPipeName = FString());
		virtual bool Request(const FString& Operation, const TSharedRef<FJsonObject>& Payload,
			TSharedPtr<FJsonObject>& OutResult, FHansaWorkerError& OutError) override;
		const FString& GetPipeName() const { return PipeName; }
	private:
		FString PipeName;
	};

	struct FHansaGenerationSubmission final
	{
		FString ProviderId = TEXT("mock");
		FString ModelVersion = TEXT("mock-v1");
		FString Capability = TEXT("StructuredDataDraft");
		FString IntendedAssetRole = TEXT("DefinitionDraft");
		FString Prompt;
		FString ApprovedBy;
		int64 MaximumCostMinorUnits = 0;
		FString Currency = TEXT("USD");
		int64 MaximumOutputBytes = 1024 * 1024;
		int32 TimeoutMs = 30000;
		bool bRightsAcknowledged = false;
		bool bSpendApproved = false;
		TSharedPtr<FJsonObject> Parameters;
	};

	class FHansaGenerationJobController final
	{
	public:
		explicit FHansaGenerationJobController(TSharedRef<IHansaGenerationWorkerTransport> InTransport);
		bool RefreshCapabilities();
		bool RefreshJobs();
		bool PreviewUploads(const TArray<FString>& AbsolutePaths, const FString& RightsDeclaration);
		bool Estimate(const FHansaGenerationSubmission& Submission);
		bool CanSubmit(const FHansaGenerationSubmission& Submission, FString& OutReason) const;
		bool Submit(const FHansaGenerationSubmission& Submission);
		bool Cancel(const FString& JobId);
		bool Retry(const FString& JobId, const FString& ApprovedBy, bool bSpendApproved);
		bool ReadJsonOutput(const FString& JobId, int32 OutputIndex, TSharedPtr<FJsonObject>& OutDocument);
		const TArray<FHansaProviderCapability>& GetCapabilities() const { return Capabilities; }
		const TArray<FHansaUploadPreview>& GetUploads() const { return Uploads; }
		const TArray<FHansaGenerationJob>& GetJobs() const { return Jobs; }
		const FHansaWorkerError& GetLastError() const { return LastError; }
		const FString& GetRightsDeclaration() const { return RightsDeclaration; }
		bool HasCurrentEstimate() const { return bEstimateReady; }
		int64 GetEstimatedMinorUnits() const { return EstimatedMinorUnits; }
		const FString& GetEstimateCurrency() const { return EstimateCurrency; }
		const FString& GetEstimatedRequestHash() const { return EstimatedRequestHash; }
		void InvalidateEstimate() { bEstimateReady = false; EstimatedRequestHash.Reset(); }
		static FString MediaTypeForPath(const FString& Path);
		static bool ParseJob(const TSharedRef<FJsonObject>& Json, FHansaGenerationJob& OutJob, FHansaWorkerError& OutError);
	private:
		bool SendJobOperation(const FString& Operation, const TSharedRef<FJsonObject>& Payload);
		void SetError(FString Code, FString Message, FString Remedy = FString(), bool bRetryable = false);
		FString SubmissionIdentity(const FHansaGenerationSubmission& Submission) const;
		struct FTransportOwner
		{
			explicit FTransportOwner(TSharedRef<IHansaGenerationWorkerTransport> InTransport) : Instance(MoveTemp(InTransport)) {}
			TSharedRef<IHansaGenerationWorkerTransport> Instance;
		};
		// Snapshots can outlive a closed tab; only this thread-safe owner is shared.
		TSharedRef<FTransportOwner, ESPMode::ThreadSafe> Transport;
		TArray<FHansaProviderCapability> Capabilities;
		TArray<FHansaUploadPreview> Uploads;
		TArray<FHansaGenerationJob> Jobs;
		FHansaWorkerError LastError;
		FString RightsDeclaration;
		bool bEstimateReady = false;
		int64 EstimatedMinorUnits = 0;
		FString EstimateCurrency;
		FString EstimatedRequestHash;
		FString EstimatedIdentity;
	};
}
