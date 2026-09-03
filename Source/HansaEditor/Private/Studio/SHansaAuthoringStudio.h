#pragma once

#include "CoreMinimal.h"
#include "Definitions/HansaDefinitionBase.h"
#include "EditorUndoClient.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;

struct FHansaDefinitionListItem final
{
	explicit FHansaDefinitionListItem(UHansaDefinitionBase* InDefinition, FString InSourcePath)
		: Definition(InDefinition)
		, SourcePath(MoveTemp(InSourcePath))
	{
	}

	TStrongObjectPtr<UHansaDefinitionBase> Definition;
	FString SourcePath;
};

struct FHansaStudioValidationItem final
{
	EHansaSchemaDiagnosticSeverity Severity = EHansaSchemaDiagnosticSeverity::Information;
	FString SeverityLabel;
	FString Code;
	FString PropertyPath;
	FString Cause;
	FString Remedy;
};

class SHansaAuthoringStudio final : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SHansaAuthoringStudio) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	void DiscoverDefinitions();
	void ApplySearchFilter(const FText& SearchText);
	void OnDefinitionSelected(TSharedPtr<FHansaDefinitionListItem> Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> GenerateDefinitionRow(
		TSharedPtr<FHansaDefinitionListItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> GenerateValidationRow(
		TSharedPtr<FHansaStudioValidationItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable) const;

	FReply ValidateSelectedDefinition();
	FReply ExportSchemas();
	FReply RunGrainShortageFixture();
	FReply Undo();
	FReply Redo();
	bool CanUndo() const;
	bool CanRedo() const;
	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
	void RefreshAfterTransaction();
	void RebuildValidationResults();
	void AddSchemaDiagnostic(const FHansaSchemaDiagnostic& Diagnostic);
	void AddDefinitionIssue(const FHansaDefinitionValidationIssue& Issue);

	FText GetDefinitionCountText() const;
	FText GetSelectedDefinitionText() const;
	FText GetValidationSummaryText() const;
	FSlateColor GetValidationSummaryColor() const;
	FText GetFixturePreviewText() const;
	FSlateColor GetFixturePreviewColor() const;

	FHansaEditorSchemaRegistry SchemaRegistry;
	TArray<TSharedPtr<FHansaDefinitionListItem>> AllDefinitions;
	TArray<TSharedPtr<FHansaDefinitionListItem>> FilteredDefinitions;
	TArray<TSharedPtr<FHansaStudioValidationItem>> ValidationItems;
	TSharedPtr<FHansaDefinitionListItem> SelectedDefinition;
	TSharedPtr<SListView<TSharedPtr<FHansaDefinitionListItem>>> DefinitionListView;
	TSharedPtr<SListView<TSharedPtr<FHansaStudioValidationItem>>> ValidationListView;
	TSharedPtr<IDetailsView> DetailsView;
	FText FixturePreviewText;
	bool bFixturePreviewRan = false;
	bool bFixturePreviewSucceeded = false;
};
