#include "Studio/SHansaGenerationJobsPanel.h"

#include "Async/Async.h"
#include "DesktopPlatformModule.h"
#include "Definitions/HansaDefinitionBase.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Styling/AppStyle.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

using namespace Hansa::Editor::Generation;

namespace Hansa::Editor::GenerationJobs
{
	const FLinearColor Navy = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy);
	const FLinearColor Slate = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HarborSlate);
	const FLinearColor Brass = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
	const FLinearColor Teal = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal);
	const FLinearColor Amber = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::WarningAmber);
	const FLinearColor Oxblood = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oxblood);

	TSharedRef<SWidget> Label(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")));
	}
}

void SHansaGenerationJobsPanel::Construct(const FArguments& InArgs)
{
	GetSelectedDefinition = InArgs._GetSelectedDefinition;
	GetAllDefinitions = InArgs._GetAllDefinitions;
	OnProposalApplied = InArgs._OnProposalApplied;
	ProposalSchemaRegistry.Refresh();
	Controller = MakeUnique<FHansaGenerationJobController>(MakeShared<FHansaGenerationWorkerNamedPipeTransport>());
	Feedback = NSLOCTEXT("HansaGenerationJobs", "ConnectPrompt", "Refresh to connect to the external generation worker.");

	RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SHansaGenerationJobsPanel::PollOperation));

	ChildSlot
	[
		SNew(SBorder)
		.IsEnabled_Lambda([this]{ return !bBusy; })
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.BorderBackgroundColor(Hansa::Editor::GenerationJobs::Navy)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				.BorderBackgroundColor(Hansa::Editor::GenerationJobs::Slate)
				.Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Title", "Generation jobs")).TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))]
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Subtitle", "External worker queue · generated outputs remain drafts until reviewed and promoted.")).Font(FAppStyle::GetFontStyle(TEXT("SmallFont"))).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
					[
						SNew(STextBlock).Text(this, &SHansaGenerationJobsPanel::GetConnectionText).ColorAndOpacity(this, &SHansaGenerationJobsPanel::GetConnectionColor)
					]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked(this, &SHansaGenerationJobsPanel::Refresh)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Refresh", "↻ Refresh"))]]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.34f).MinSize(330.0f)[BuildSubmissionPanel()]
				+ SSplitter::Slot().Value(0.38f).MinSize(380.0f)[BuildQueuePanel()]
				+ SSplitter::Slot().Value(0.28f).MinSize(300.0f)[BuildResultPanel()]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(8.0f)
				[
					SNew(STextBlock).Text_Lambda([this]{ return Feedback; }).ColorAndOpacity_Lambda([this]
					{
						return FSlateColor(bFeedbackError ? Hansa::Editor::GenerationJobs::Oxblood : Hansa::Editor::GenerationJobs::Teal);
					}).AutoWrapText(true)
				]
			]
		]
	];
}

TSharedRef<SWidget> SHansaGenerationJobsPanel::BuildSubmissionPanel()
{
	return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(8.0f)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "RequestHeading", "1. Request and approvals"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock).Text(this, &SHansaGenerationJobsPanel::GetCapabilityText).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(6.0f, 0.0f)
				[
					SNew(SButton).IsEnabled_Lambda([this]{ return Controller->GetCapabilities().Num() > 1; }).OnClicked(this, &SHansaGenerationJobsPanel::CycleProvider)
					[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "NextProvider", "Next provider"))]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "PromptLabel", "Prompt"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).MinDesiredHeight(100.0f)
				[
					SNew(SMultiLineEditableTextBox).HintText(NSLOCTEXT("HansaGenerationJobs", "PromptHint", "Describe the draft to generate"))
					.OnTextChanged_Lambda([this](const FText& Value){ Prompt = Value; InvalidateEstimate(); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "RoleLabel", "Intended asset role"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SEditableTextBox).Text(IntendedRole).OnTextChanged_Lambda([this](const FText& Value){ IntendedRole = Value; InvalidateEstimate(); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "Uploads", "Exact upload preview"))]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked(this, &SHansaGenerationJobsPanel::AddInputs)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "AddInputs", "+ Add files"))]]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)[SNew(SButton).OnClicked(this, &SHansaGenerationJobsPanel::ClearInputs)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "ClearInputs", "Clear"))]]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(160.0f)
				[
					SAssignNew(UploadList, SListView<TSharedPtr<FUpload>>).ListItemsSource(&UploadItems).OnGenerateRow(this, &SHansaGenerationJobsPanel::GenerateUploadRow).SelectionMode(ESelectionMode::Single)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(SCheckBox).IsChecked_Lambda([this]{ return bRightsAcknowledged ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State){ bRightsAcknowledged = State == ECheckBoxState::Checked; InvalidateEstimate(); })
				[
					SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Rights", "I have the rights to send every listed input to the selected provider.")).AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "Budget", "Hard maximum spend (minor units)"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SNumericEntryBox<int64>).Value_Lambda([this]{ return TOptional<int64>(MaximumCostMinorUnits); }).MinValue(0).MaxValue(1000000000)
				.OnValueChanged_Lambda([this](int64 Value){ MaximumCostMinorUnits = Value; InvalidateEstimate(); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "Approver", "Spend approver"))]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SEditableTextBox).HintText(NSLOCTEXT("HansaGenerationJobs", "ApproverHint", "Name or operator ID")).OnTextChanged_Lambda([this](const FText& Value){ ApprovedBy = Value; bSpendApproved = false; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsEnabled(this, &SHansaGenerationJobsPanel::CanEstimate).OnClicked(this, &SHansaGenerationJobsPanel::Estimate)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Estimate", "Estimate exact request"))]]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f)[SNew(STextBlock).Text(this, &SHansaGenerationJobsPanel::GetEstimateText).AutoWrapText(true)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)
			[
				SNew(SCheckBox).IsEnabled_Lambda([this]{ return Controller->HasCurrentEstimate() || (SelectedJob.IsValid() && SelectedJob->CanRetry()); }).IsChecked_Lambda([this]{ return bSpendApproved ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State){ bSpendApproved = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Text_Lambda([this]
					{
						if (!Controller->HasCurrentEstimate() && SelectedJob.IsValid() && SelectedJob->CanRetry())
							return FText::Format(NSLOCTEXT("HansaGenerationJobs", "RetryApproval", "I approve retrying {0} up to its stored hard maximum."), FText::FromString(SelectedJob->JobId));
						return NSLOCTEXT("HansaGenerationJobs", "SpendApproval", "I approve this estimate up to the displayed hard maximum.");
					}).AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).IsEnabled(this, &SHansaGenerationJobsPanel::CanSubmit).OnClicked(this, &SHansaGenerationJobsPanel::Submit)
				.ToolTipText(this, &SHansaGenerationJobsPanel::GetSubmitReasonText).ContentPadding(FMargin(8.0f, 6.0f))
				[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Queue", "▶ Queue approved job"))]
			]
		]
	];
}

TSharedRef<SWidget> SHansaGenerationJobsPanel::BuildQueuePanel()
{
	return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(8.0f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "QueueHeading", "2. Job queue"))]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(JobList, SListView<TSharedPtr<FJob>>).ListItemsSource(&JobItems).OnGenerateRow(this, &SHansaGenerationJobsPanel::GenerateJobRow)
			.OnSelectionChanged(this, &SHansaGenerationJobsPanel::OnJobSelected).SelectionMode(ESelectionMode::Single)
		]
	];
}

TSharedRef<SWidget> SHansaGenerationJobsPanel::BuildResultPanel()
{
	return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(8.0f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "ResultHeading", "3. Result, provenance, and proposal review"))]
		+ SVerticalBox::Slot().FillHeight(0.38f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()[SNew(STextBlock).Text(this, &SHansaGenerationJobsPanel::GetSelectedResultText).AutoWrapText(true)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[Hansa::Editor::GenerationJobs::Label(NSLOCTEXT("HansaGenerationJobs", "ProposalHeading", "Field-level definition patch"))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).OnClicked(this, &SHansaGenerationJobsPanel::LoadProposal)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "LoadProposal", "Load reviewed JSON"))]]
		]
		+ SVerticalBox::Slot().FillHeight(0.42f)
		[
			SAssignNew(ProposalDiffList, SListView<TSharedPtr<FHansaProposalFieldDiff>>)
			.ListItemsSource(&ProposalReview.GetDiffs())
			.OnGenerateRow(this, &SHansaGenerationJobsPanel::GenerateProposalDiffRow)
			.SelectionMode(ESelectionMode::None)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
		[
			SNew(STextBlock).Text_Lambda([this]{ return FText::FromString(ProposalReview.GetPreviewSummary()); }).AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(Hansa::Editor::GenerationJobs::Teal))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SButton).IsEnabled_Lambda([this]{ return ProposalReview.IsLoaded() && ProposalReview.HasSelectedChanges(); }).OnClicked(this, &SHansaGenerationJobsPanel::ValidateProposal)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "ValidateProposal", "Validate + preview fixture"))]]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f)[SNew(SButton).IsEnabled_Lambda([this]{ return ProposalReview.IsValidated(); }).OnClicked(this, &SHansaGenerationJobsPanel::ApplyProposal)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "ApplyProposal", "Apply selected fields"))]]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).BorderBackgroundColor(Hansa::Editor::GenerationJobs::Slate).Padding(8.0f)
			[
				SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "ReviewGate", "REVIEW GATE\nOnly checked fields can be applied. The current revision and content hash are rechecked, validation runs against a temporary registry, and apply creates one undoable Editor transaction.")).AutoWrapText(true)
			]
		]
	];
}
TSharedRef<ITableRow> SHansaGenerationJobsPanel::GenerateUploadRow(TSharedPtr<FUpload> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const FString ShortHash = Item->Sha256.Len() > 16 ? Item->Sha256.Left(8) + TEXT("…") + Item->Sha256.Right(8) : Item->Sha256;
	return SNew(STableRow<TSharedPtr<FUpload>>, OwnerTable).Padding(FMargin(4.0f, 5.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Item->DisplayPath)).ToolTipText(FText::FromString(Item->AbsolutePath))]
		+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s · %s · %lld bytes · SHA-256 %s"), *Item->Role, *Item->MediaType, Item->SizeBytes, *ShortHash))).Font(FAppStyle::GetFontStyle(TEXT("SmallFont"))).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
	];
}

TSharedRef<ITableRow> SHansaGenerationJobsPanel::GenerateJobRow(TSharedPtr<FJob> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bFailure = Item->Status == TEXT("Failed") || Item->Status == TEXT("Expired");
	const FLinearColor StatusColor = bFailure ? Hansa::Editor::GenerationJobs::Oxblood :
		(Item->Status == TEXT("Review") ? Hansa::Editor::GenerationJobs::Teal : Hansa::Editor::GenerationJobs::Brass);
	TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
	if (Item->CanCancel())
	{
		Actions->AddSlot().AutoWidth()[SNew(SButton).OnClicked(this, &SHansaGenerationJobsPanel::CancelJob, Item->JobId)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Cancel", "Cancel"))]];
	}
	if (Item->CanRetry())
	{
		Actions->AddSlot().AutoWidth()[SNew(SButton).OnClicked(this, &SHansaGenerationJobsPanel::RetryJob, Item->JobId)[SNew(STextBlock).Text(NSLOCTEXT("HansaGenerationJobs", "Retry", "Retry"))]];
	}
	return SNew(STableRow<TSharedPtr<FJob>>, OwnerTable).Padding(FMargin(6.0f))
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(FText::FromString(Item->JobId)).Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(Item->Status)).ColorAndOpacity(StatusColor)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SProgressBar).Percent(static_cast<float>(FMath::Clamp(Item->ProgressPercent / 100.0, 0.0, 1.0)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(FText::FromString(Item->ProgressMessage)).AutoWrapText(true).Font(FAppStyle::GetFontStyle(TEXT("SmallFont"))).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
			+ SHorizontalBox::Slot().AutoWidth()[Actions]
		]
	];
}

TSharedRef<ITableRow> SHansaGenerationJobsPanel::GenerateProposalDiffRow(TSharedPtr<FHansaProposalFieldDiff> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FHansaProposalFieldDiff>>, OwnerTable).Padding(FMargin(4.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(SCheckBox).IsChecked_Lambda([Item]{ return Item->bSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState State){ ProposalReview.SetSelected(Item->FieldName, State == ECheckBoxState::Checked); })
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Item->DisplayName)).Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Item->BeforeJson + TEXT(" → ") + Item->AfterJson)).AutoWrapText(true).Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))]
		]
	];
}
void SHansaGenerationJobsPanel::OnJobSelected(TSharedPtr<FJob> Item, ESelectInfo::Type SelectInfo)
{
	const bool bSelectionChanged = !SelectedJob.IsValid() || !Item.IsValid() || SelectedJob->JobId != Item->JobId;
	if (bSelectionChanged) bSpendApproved = false;
	if (SelectInfo != ESelectInfo::Direct) InvalidateEstimate();
	SelectedJob = MoveTemp(Item);
	if (bSelectionChanged)
	{
		ProposalReview.Reset();
		if (ProposalDiffList.IsValid()) ProposalDiffList->RequestListRefresh();
	}
}

void SHansaGenerationJobsPanel::SyncItems()
{
	const FString SelectedId = SelectedJob.IsValid() ? SelectedJob->JobId : FString();
	UploadItems.Reset();
	for (const FUpload& Upload : Controller->GetUploads()) UploadItems.Add(MakeShared<FUpload>(Upload));
	JobItems.Reset();
	for (const FJob& Job : Controller->GetJobs()) JobItems.Add(MakeShared<FJob>(Job));
	SelectedJob = JobItems.FindByPredicate([&SelectedId](const TSharedPtr<FJob>& Item){ return Item->JobId == SelectedId; }) ?
		*JobItems.FindByPredicate([&SelectedId](const TSharedPtr<FJob>& Item){ return Item->JobId == SelectedId; }) : nullptr;
	if (!SelectedJob.IsValid() && !JobItems.IsEmpty()) SelectedJob = JobItems[0];
	if (UploadList.IsValid()) UploadList->RequestListRefresh();
	if (JobList.IsValid())
	{
		JobList->RequestListRefresh();
		if (SelectedJob.IsValid()) JobList->SetSelection(SelectedJob, ESelectInfo::Direct);
	}
}

void SHansaGenerationJobsPanel::InvalidateEstimate()
{
	Controller->InvalidateEstimate();
	bSpendApproved = false;
}

void SHansaGenerationJobsPanel::UpdateFeedback(const bool bSucceeded, const FText& SuccessText)
{
	bFeedbackError = !bSucceeded;
	if (bSucceeded)
	{
		Feedback = SuccessText;
		return;
	}
	const FHansaWorkerError& Error = Controller->GetLastError();
	Feedback = FText::FromString(FString::Printf(TEXT("%s: %s%s%s"), *Error.Code, *Error.Message,
		Error.Remedy.IsEmpty() ? TEXT("") : TEXT(" — "), *Error.Remedy));
}

FHansaGenerationSubmission SHansaGenerationJobsPanel::CurrentSubmission() const
{
	FHansaGenerationSubmission Value;
	Value.Prompt = Prompt.ToString();
	Value.IntendedAssetRole = IntendedRole.ToString();
	Value.ApprovedBy = ApprovedBy.ToString();
	Value.MaximumCostMinorUnits = MaximumCostMinorUnits;
	Value.MaximumOutputBytes = MaximumOutputBytes;
	Value.bRightsAcknowledged = bRightsAcknowledged;
	Value.bSpendApproved = bSpendApproved;
	if (!Controller->GetCapabilities().IsEmpty())
	{
		const int32 Index = FMath::Clamp(SelectedCapabilityIndex, 0, Controller->GetCapabilities().Num() - 1);
		const FHansaProviderCapability& Capability = Controller->GetCapabilities()[Index];
		Value.ProviderId = Capability.ProviderId;
		Value.ModelVersion = Capability.ModelVersion;
		Value.Capability = Capability.Capability;
	}
	if (Value.IntendedAssetRole == TEXT("DefinitionPatch") && GetSelectedDefinition)
	{
		if (UHansaDefinitionBase* Definition = GetSelectedDefinition())
		{
			const FHansaDefinitionClassSchema Schema = ProposalSchemaRegistry.BuildSchemaForClass(Definition->GetClass());
			TSharedPtr<FJsonObject> Contract;
			FHansaProposalReviewError ContractError;
			if (FHansaDefinitionProposalReview::BuildContract(Definition, Schema, ProposalDefinitions(), Contract, ContractError))
			{
				Value.Parameters = MakeShared<FJsonObject>();
				Value.Parameters->SetObjectField(TEXT("definitionProposal"), Contract.ToSharedRef());
			}
		}
	}
	return Value;
}

void SHansaGenerationJobsPanel::StartOperation(TFunction<bool(FController&, TSharedPtr<FJsonObject>&)> Operation,
	TFunction<void(bool, TSharedPtr<FJsonObject>)> Completion)
{
	if (bBusy) return;
	bBusy = true;
	OperationCompletion = MoveTemp(Completion);
	// The worker owns a snapshot; Slate continues reading the unchanged controller.
	// No widget or UObject is captured by the background task, even if the tab closes.
	auto Result = MakeUnique<FOperationResult>();
	Result->Controller = MakeUnique<FController>(*Controller);
	PendingOperation = Async(EAsyncExecution::ThreadPool,
		[Result = MoveTemp(Result), Operation = MoveTemp(Operation)]() mutable
		{
			Result->bSucceeded = Operation(*Result->Controller, Result->Document);
			return MoveTemp(Result);
		});
}

EActiveTimerReturnType SHansaGenerationJobsPanel::PollOperation(double CurrentTime, float DeltaTime)
{
	if (bBusy && PendingOperation.IsReady())
	{
		auto Result = PendingOperation.Consume();
		Controller = MoveTemp(Result->Controller);
		bBusy = false;
		auto Completion = MoveTemp(OperationCompletion);
		Completion(Result->bSucceeded, MoveTemp(Result->Document));
		NextQueueRefresh = CurrentTime + 1.0;
	}
	if (!bBusy && bConnected && CurrentTime >= NextQueueRefresh &&
		Controller->GetJobs().ContainsByPredicate([](const FJob& Job){ return Job.CanCancel(); }))
	{
		StartOperation([](FController& Worker, TSharedPtr<FJsonObject>&){ return Worker.RefreshJobs(); },
			[this](bool bOk, TSharedPtr<FJsonObject>)
			{
				if (bOk) SyncItems();
				else { bConnected = false; UpdateFeedback(false, FText::GetEmpty()); }
			});
	}
	return EActiveTimerReturnType::Continue;
}

FReply SHansaGenerationJobsPanel::Refresh()
{
	StartOperation([](FController& Worker, TSharedPtr<FJsonObject>&)
		{ return Worker.RefreshCapabilities() && Worker.RefreshJobs(); },
		[this](bool bOk, TSharedPtr<FJsonObject>)
		{
			bConnected = bOk;
			SelectedCapabilityIndex = FMath::Clamp(SelectedCapabilityIndex, 0, FMath::Max(0, Controller->GetCapabilities().Num() - 1));
			bSpendApproved = false;
			SyncItems();
			UpdateFeedback(bOk, NSLOCTEXT("HansaGenerationJobs", "Refreshed", "Worker capabilities and job queue refreshed."));
		});
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::AddInputs()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform == nullptr) return FReply::Handled();
	TArray<FString> Files;
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (DesktopPlatform->OpenFileDialog(Parent, TEXT("Select exact generation inputs"), FPaths::ProjectDir(), TEXT(""),
		TEXT("Supported inputs (*.json;*.txt;*.md;*.png;*.jpg;*.jpeg;*.wav)|*.json;*.txt;*.md;*.png;*.jpg;*.jpeg;*.wav|All files (*.*)|*.*"),
		EFileDialogFlags::Multiple, Files))
	{
		bRightsAcknowledged = false;
		bSpendApproved = false;
		StartOperation([Files](FController& Worker, TSharedPtr<FJsonObject>&)
			{ return Worker.PreviewUploads(Files, TEXT("Operator confirms the listed inputs are owned or licensed for provider processing.")); },
			[this, Count = Files.Num()](bool bOk, TSharedPtr<FJsonObject>)
			{
				SyncItems();
				UpdateFeedback(bOk, FText::Format(NSLOCTEXT("HansaGenerationJobs", "UploadsReady", "Exact upload preview prepared for {0} file(s)."), FText::AsNumber(Count)));
			});
	}
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::ClearInputs()
{
	const bool bOk = Controller->PreviewUploads({}, FString());
	bRightsAcknowledged = false;
	SyncItems();
	UpdateFeedback(bOk, NSLOCTEXT("HansaGenerationJobs", "UploadsCleared", "Exact upload preview cleared."));
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::Estimate()
{
	const auto Submission = CurrentSubmission();
	bSpendApproved = false;
	StartOperation([Submission](FController& Worker, TSharedPtr<FJsonObject>&){ return Worker.Estimate(Submission); },
		[this](bool bOk, TSharedPtr<FJsonObject>)
		{ UpdateFeedback(bOk, NSLOCTEXT("HansaGenerationJobs", "Estimated", "Read-only worker estimate recorded. Review it before approving spend.")); });
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::Submit()
{
	const auto Submission = CurrentSubmission();
	bSpendApproved = false;
	StartOperation([Submission](FController& Worker, TSharedPtr<FJsonObject>&){ return Worker.Submit(Submission); },
		[this](bool bOk, TSharedPtr<FJsonObject>)
		{
			Controller->InvalidateEstimate();
			SyncItems();
			UpdateFeedback(bOk, NSLOCTEXT("HansaGenerationJobs", "Submitted", "Approved generation job queued in the external worker."));
		});
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::CancelJob(FString JobId)
{
	StartOperation([JobId](FController& Worker, TSharedPtr<FJsonObject>&){ return Worker.Cancel(JobId); },
		[this](bool bOk, TSharedPtr<FJsonObject>)
		{
			SyncItems();
			UpdateFeedback(bOk, NSLOCTEXT("HansaGenerationJobs", "Cancelled", "Cancellation requested."));
		});
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::RetryJob(FString JobId)
{
	if (!SelectedJob.IsValid() || SelectedJob->JobId != JobId || Controller->HasCurrentEstimate())
	{
		bSpendApproved = false;
		bFeedbackError = true;
		Feedback = NSLOCTEXT("HansaGenerationJobs", "RetrySelection", "Select this job and review its estimate before approving a retry.");
		return FReply::Handled();
	}
	const FString Approver = ApprovedBy.ToString();
	const bool bApproved = bSpendApproved;
	bSpendApproved = false;
	StartOperation([JobId, Approver, bApproved](FController& Worker, TSharedPtr<FJsonObject>&){ return Worker.Retry(JobId, Approver, bApproved); },
		[this](bool bOk, TSharedPtr<FJsonObject>)
		{
			SyncItems();
			UpdateFeedback(bOk, NSLOCTEXT("HansaGenerationJobs", "Retried", "Retry queued with a fresh spend approval."));
		});
	return FReply::Handled();
}

TArray<UHansaDefinitionBase*> SHansaGenerationJobsPanel::ProposalDefinitions() const
{
	return GetAllDefinitions ? GetAllDefinitions() : TArray<UHansaDefinitionBase*>();
}

FReply SHansaGenerationJobsPanel::CycleProvider()
{
	const int32 Count = Controller->GetCapabilities().Num();
	if (Count > 1) { SelectedCapabilityIndex = (SelectedCapabilityIndex + 1) % Count; InvalidateEstimate(); }
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::LoadProposal()
{
	ProposalReview.Reset();
	TWeakObjectPtr<UHansaDefinitionBase> Definition = GetSelectedDefinition ? GetSelectedDefinition() : nullptr;
	if (!SelectedJob.IsValid() || !Definition.IsValid() || SelectedJob->Outputs.IsEmpty())
	{
		bFeedbackError = true;
		Feedback = NSLOCTEXT("HansaGenerationJobs", "ProposalLoadUnavailable", "Select a Review job with a JSON result and a definition in the Data workspace.");
		return FReply::Handled();
	}
	StartOperation([JobId = SelectedJob->JobId](FController& Worker, TSharedPtr<FJsonObject>& Document)
		{ return Worker.ReadJsonOutput(JobId, 0, Document); },
		[this, Definition](bool bOk, TSharedPtr<FJsonObject> Document)
		{
			if (!bOk) { UpdateFeedback(false, FText::GetEmpty()); return; }
			bOk = Definition.IsValid() && Document.IsValid();
			if (bOk)
			{
				const auto Schema = ProposalSchemaRegistry.BuildSchemaForClass(Definition->GetClass());
				bOk = ProposalReview.Load(Definition.Get(), Schema, Document.ToSharedRef(), ProposalDefinitions());
			}
			if (ProposalDiffList.IsValid()) ProposalDiffList->RequestListRefresh();
			bFeedbackError = !bOk;
			Feedback = bOk ? NSLOCTEXT("HansaGenerationJobs", "ProposalLoaded", "Schema-valid proposal loaded. Select fields, then validate the temporary registry.") :
				FText::FromString(ProposalReview.GetError().Code + TEXT(": ") + ProposalReview.GetError().Message);
		});
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::ValidateProposal()
{
	const bool bOk = ProposalReview.ValidateSelected(ProposalDefinitions());
	bFeedbackError = !bOk;
	Feedback = bOk ? FText::FromString(ProposalReview.GetPreviewSummary()) : FText::FromString(ProposalReview.GetError().Code + TEXT(": ") + ProposalReview.GetError().Message + TEXT(" — ") + ProposalReview.GetError().Remedy);
	return FReply::Handled();
}

FReply SHansaGenerationJobsPanel::ApplyProposal()
{
	const bool bOk = ProposalReview.ApplySelected(ProposalDefinitions());
	bFeedbackError = !bOk;
	Feedback = bOk ? FText::FromString(ProposalReview.GetPreviewSummary()) : FText::FromString(ProposalReview.GetError().Code + TEXT(": ") + ProposalReview.GetError().Message + TEXT(" — ") + ProposalReview.GetError().Remedy);
	if (bOk) OnProposalApplied.ExecuteIfBound();
	return FReply::Handled();
}
FText SHansaGenerationJobsPanel::GetConnectionText() const
{
	if (bBusy) return NSLOCTEXT("HansaGenerationJobs", "Busy", "Contacting worker…");
	return bConnected ? NSLOCTEXT("HansaGenerationJobs", "Connected", "● Worker connected") : NSLOCTEXT("HansaGenerationJobs", "Disconnected", "⚠ Worker disconnected");
}

FSlateColor SHansaGenerationJobsPanel::GetConnectionColor() const
{
	return FSlateColor(bConnected ? Hansa::Editor::GenerationJobs::Teal : Hansa::Editor::GenerationJobs::Amber);
}

FText SHansaGenerationJobsPanel::GetCapabilityText() const
{
	if (Controller->GetCapabilities().IsEmpty()) return NSLOCTEXT("HansaGenerationJobs", "NoCapabilities", "Provider capabilities unavailable. Credentials are read only from the worker environment or Windows Credential Manager.");
	const FHansaProviderCapability& Value = Controller->GetCapabilities()[FMath::Clamp(SelectedCapabilityIndex, 0, Controller->GetCapabilities().Num() - 1)];
	return FText::FromString(FString::Printf(TEXT("Provider %s · capability %s · pinned model %s · adapter %s\nInputs: %s\nOutputs: %s · cancellation %s · seed %s"),
		*Value.ProviderId, *Value.Capability, *Value.ModelVersion, *Value.AdapterVersion,
		*FString::Join(Value.InputMediaTypes, TEXT(", ")), *FString::Join(Value.OutputMediaTypes, TEXT(", ")),
		Value.bSupportsCancellation ? TEXT("supported") : TEXT("unavailable"), Value.bSupportsSeed ? TEXT("supported") : TEXT("unavailable")));
}

FText SHansaGenerationJobsPanel::GetEstimateText() const
{
	if (!Controller->HasCurrentEstimate())
	{
		if (SelectedJob.IsValid() && SelectedJob->CanRetry())
			return FText::FromString(FString::Printf(TEXT("Retry %s: estimated %lld minor units %s · stored hard cap %lld"),
				*SelectedJob->JobId, SelectedJob->EstimatedMinorUnits, *SelectedJob->Currency, SelectedJob->MaximumCostMinorUnits));
		return NSLOCTEXT("HansaGenerationJobs", "NoEstimate", "No current estimate.");
	}
	return FText::FromString(FString::Printf(TEXT("Estimated %lld minor units %s · hard cap %lld · request %s"),
		Controller->GetEstimatedMinorUnits(), *Controller->GetEstimateCurrency(), MaximumCostMinorUnits,
		*Controller->GetEstimatedRequestHash().Left(12)));
}

FText SHansaGenerationJobsPanel::GetSubmitReasonText() const
{
	FString Reason;
	return Controller->CanSubmit(CurrentSubmission(), Reason) ? NSLOCTEXT("HansaGenerationJobs", "Ready", "Queue this reviewed request.") : FText::FromString(Reason);
}

bool SHansaGenerationJobsPanel::CanEstimate() const
{
	const FHansaGenerationSubmission Value = CurrentSubmission();
	return bConnected && !Value.Prompt.TrimStartAndEnd().IsEmpty() && (Controller->GetUploads().IsEmpty() || bRightsAcknowledged) && (Value.IntendedAssetRole != TEXT("DefinitionPatch") || Value.Parameters.IsValid());
}

bool SHansaGenerationJobsPanel::CanSubmit() const
{
	FString Reason;
	return bConnected && Controller->CanSubmit(CurrentSubmission(), Reason);
}

FText SHansaGenerationJobsPanel::GetSelectedResultText() const
{
	if (!SelectedJob.IsValid()) return NSLOCTEXT("HansaGenerationJobs", "NoSelection", "No job selected. Select a queue row to inspect result and provenance.");
	FString Text = FString::Printf(TEXT("%s\nStatus: %s\nProvider: %s\nPinned model: %s\nCapability: %s\nProgress: %.0f%% — %s\nEstimated spend: %lld %s\nActual spend: %lld %s\n\nStaged output artifacts (%d)"),
		*SelectedJob->JobId, *SelectedJob->Status, *SelectedJob->ProviderId, *SelectedJob->ModelVersion, *SelectedJob->Capability,
		SelectedJob->ProgressPercent, *SelectedJob->ProgressMessage, SelectedJob->EstimatedMinorUnits, *SelectedJob->Currency,
		SelectedJob->ActualMinorUnits, *SelectedJob->Currency, SelectedJob->Outputs.Num());
	for (const FHansaOutputArtifact& Output : SelectedJob->Outputs)
	{
		Text += FString::Printf(TEXT("\n\n%s\n%s · %lld bytes\nSHA-256 %s\n%s"), *Output.LogicalName, *Output.MediaType,
			Output.SizeBytes, *Output.Sha256, *Output.RelativePath);
	}
	Text += TEXT("\n\nDeterministic QA");
	for (const FString& Qa : SelectedJob->QaResults) Text += TEXT("\n") + Qa;
	Text += FString::Printf(TEXT("\n\nProvenance\nAdapter: %s\nProvider job: %s\nRequest hash: %s\nManifest hash: %s"),
		*SelectedJob->AdapterVersion, *SelectedJob->ProviderJobId, *SelectedJob->RequestHash, *SelectedJob->ManifestSha256);
	if (SelectedJob->Error.IsSet())
	{
		Text += FString::Printf(TEXT("\n\nERROR %s\n%s\nRemedy: %s"), *SelectedJob->Error.Code, *SelectedJob->Error.Message, *SelectedJob->Error.Remedy);
	}
	return FText::FromString(Text);
}
