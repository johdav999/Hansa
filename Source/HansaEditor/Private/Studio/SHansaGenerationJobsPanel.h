#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Generation/HansaGenerationWorkerBridge.h"
#include "Generation/HansaDefinitionProposalReview.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UHansaDefinitionBase;

class SHansaGenerationJobsPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHansaGenerationJobsPanel) {}
		SLATE_ARGUMENT(TFunction<UHansaDefinitionBase*()>, GetSelectedDefinition)
		SLATE_ARGUMENT(TFunction<TArray<UHansaDefinitionBase*>()>, GetAllDefinitions)
		SLATE_EVENT(FSimpleDelegate, OnProposalApplied)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);

private:
	friend class FHansaGenerationPanelAsyncTest;
	using FJob = Hansa::Editor::Generation::FHansaGenerationJob;
	using FUpload = Hansa::Editor::Generation::FHansaUploadPreview;
	using FController = Hansa::Editor::Generation::FHansaGenerationJobController;
	struct FOperationResult
	{
		TUniquePtr<FController> Controller;
		TSharedPtr<FJsonObject> Document;
		bool bSucceeded = false;
	};
	void StartOperation(TFunction<bool(FController&, TSharedPtr<FJsonObject>&)> Operation,
		TFunction<void(bool, TSharedPtr<FJsonObject>)> Completion);
	EActiveTimerReturnType PollOperation(double CurrentTime, float DeltaTime);
	TFuture<TUniquePtr<FOperationResult>> PendingOperation;
	TFunction<void(bool, TSharedPtr<FJsonObject>)> OperationCompletion;
	bool bBusy = false;
	double NextQueueRefresh = 0.0;

	TSharedRef<SWidget> BuildSubmissionPanel();
	TSharedRef<SWidget> BuildQueuePanel();
	TSharedRef<SWidget> BuildResultPanel();
	TSharedRef<ITableRow> GenerateUploadRow(TSharedPtr<FUpload> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> GenerateJobRow(TSharedPtr<FJob> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> GenerateProposalDiffRow(TSharedPtr<Hansa::Editor::Generation::FHansaProposalFieldDiff> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnJobSelected(TSharedPtr<FJob> Item, ESelectInfo::Type SelectInfo);
	void SyncItems();
	void InvalidateEstimate();
	void UpdateFeedback(bool bSucceeded, const FText& SuccessText);
	Hansa::Editor::Generation::FHansaGenerationSubmission CurrentSubmission() const;

	FReply Refresh();
	FReply AddInputs();
	FReply ClearInputs();
	FReply Estimate();
	FReply Submit();
	FReply CancelJob(FString JobId);
	FReply RetryJob(FString JobId);
	FReply CycleProvider();
	FReply LoadProposal();
	FReply ValidateProposal();
	FReply ApplyProposal();
	TArray<UHansaDefinitionBase*> ProposalDefinitions() const;

	FText GetConnectionText() const;
	FSlateColor GetConnectionColor() const;
	FText GetCapabilityText() const;
	FText GetEstimateText() const;
	FText GetSubmitReasonText() const;
	FText GetSelectedResultText() const;
	bool CanEstimate() const;
	bool CanSubmit() const;

	TUniquePtr<Hansa::Editor::Generation::FHansaGenerationJobController> Controller;
	TArray<TSharedPtr<FUpload>> UploadItems;
	TArray<TSharedPtr<FJob>> JobItems;
	TSharedPtr<FJob> SelectedJob;
	TSharedPtr<SListView<TSharedPtr<FUpload>>> UploadList;
	TSharedPtr<SListView<TSharedPtr<FJob>>> JobList;
	TSharedPtr<SListView<TSharedPtr<Hansa::Editor::Generation::FHansaProposalFieldDiff>>> ProposalDiffList;
	Hansa::Editor::Generation::FHansaDefinitionProposalReview ProposalReview;
	FHansaEditorSchemaRegistry ProposalSchemaRegistry;
	TFunction<UHansaDefinitionBase*()> GetSelectedDefinition;
	TFunction<TArray<UHansaDefinitionBase*>()> GetAllDefinitions;
	FSimpleDelegate OnProposalApplied;
	int32 SelectedCapabilityIndex = 0;
	FText Prompt;
	FText IntendedRole = FText::FromString(TEXT("DefinitionPatch"));
	FText ApprovedBy;
	int64 MaximumCostMinorUnits = 0;
	int64 MaximumOutputBytes = 1024 * 1024;
	bool bRightsAcknowledged = false;
	bool bSpendApproved = false;
	bool bConnected = false;
	FText Feedback;
	bool bFeedbackError = false;
};
