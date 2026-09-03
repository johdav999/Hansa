#include "Studio/SHansaAuthoringStudio.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaFoundationSampleDefinition.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Fixtures/HansaProductionFixture.h"
#include "IDetailsView.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

namespace Hansa::Editor::Studio
{
	const FLinearColor BalticNavy = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("152A35")));
	const FLinearColor HarborSlate = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("29424D")));
	const FLinearColor Brass = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("C19A52")));
	const FLinearColor ProsperityTeal = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("35766F")));
	const FLinearColor WarningAmber = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D09132")));
	const FLinearColor Oxblood = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("762F32")));

	EHansaSchemaDiagnosticSeverity ConvertSeverity(const EHansaDefinitionValidationSeverity Severity)
	{
		switch (Severity)
		{
		case EHansaDefinitionValidationSeverity::Information:
			return EHansaSchemaDiagnosticSeverity::Information;
		case EHansaDefinitionValidationSeverity::Warning:
			return EHansaSchemaDiagnosticSeverity::Warning;
		default:
			return EHansaSchemaDiagnosticSeverity::Error;
		}
	}

	FString SeverityLabel(const EHansaSchemaDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EHansaSchemaDiagnosticSeverity::Information:
			return TEXT("Info");
		case EHansaSchemaDiagnosticSeverity::Warning:
			return TEXT("Warning");
		default:
			return TEXT("Error");
		}
	}

	FSlateColor SeverityColor(const EHansaSchemaDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EHansaSchemaDiagnosticSeverity::Information:
			return FSlateColor(FLinearColor(0.34f, 0.67f, 0.82f));
		case EHansaSchemaDiagnosticSeverity::Warning:
			return FSlateColor(WarningAmber);
		default:
			return FSlateColor(Oxblood);
		}
	}
}

void SHansaAuthoringStudio::Construct(const FArguments& InArgs)
{
	SchemaRegistry.Refresh();
	DiscoverDefinitions();
	FixturePreviewText = NSLOCTEXT("HansaAuthoringStudio", "FixturePreviewEmpty",
		"Run the versioned Lübeck fixture to preview baseline, shortage, and recovery metrics.");

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &SHansaAuthoringStudio::OnFinishedChangingProperties);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.BorderBackgroundColor(Hansa::Editor::Studio::BalticNavy)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				.BorderBackgroundColor(Hansa::Editor::Studio::HarborSlate)
				.Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("HansaAuthoringStudio", "Title", "Hansa Authoring Studio"))
						.TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.ToolTipText(NSLOCTEXT("HansaAuthoringStudio", "ValidateTip", "Validate reflected metadata and the selected definition. Errors identify cause and remedy."))
						.OnClicked(this, &SHansaAuthoringStudio::ValidateSelectedDefinition)
						.ContentPadding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock).Text(NSLOCTEXT("HansaAuthoringStudio", "Validate", "✓ Validate"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.ToolTipText(NSLOCTEXT("HansaAuthoringStudio", "RunShortageTip", "Run lubeck_grain_shortage_v1 headlessly and compare deterministic market metrics before, during, and after controlled recovery."))
						.OnClicked(this, &SHansaAuthoringStudio::RunGrainShortageFixture)
						.ContentPadding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock).Text(NSLOCTEXT("HansaAuthoringStudio", "RunShortage", "▶ Run shortage fixture"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.ToolTipText(NSLOCTEXT("HansaAuthoringStudio", "ExportTip", "Export deterministic JSON Schemas under Saved/SchemaExport."))
						.OnClicked(this, &SHansaAuthoringStudio::ExportSchemas)
						.ContentPadding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock).Text(NSLOCTEXT("HansaAuthoringStudio", "Export", "{} Export schema"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(12.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.IsEnabled(this, &SHansaAuthoringStudio::CanUndo)
						.OnClicked(this, &SHansaAuthoringStudio::Undo)
						.ContentPadding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock).Text(NSLOCTEXT("HansaAuthoringStudio", "Undo", "↶ Undo"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SButton)
						.IsEnabled(this, &SHansaAuthoringStudio::CanRedo)
						.OnClicked(this, &SHansaAuthoringStudio::Redo)
						.ContentPadding(FMargin(8.0f, 4.0f))
						[
							SNew(STextBlock).Text(NSLOCTEXT("HansaAuthoringStudio", "Redo", "↷ Redo"))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(0.75f)
			[
				SNew(SSplitter)
				.PhysicalSplitterHandleSize(4.0f)
				+ SSplitter::Slot()
				.Value(0.20f)
				.MinSize(240.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
					.Padding(8.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("HansaAuthoringStudio", "Definitions", "Definitions"))
							.TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
						[
							SNew(SSearchBox)
							.HintText(NSLOCTEXT("HansaAuthoringStudio", "SearchHint", "Search definitions"))
							.OnTextChanged(this, &SHansaAuthoringStudio::ApplySearchFilter)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock).Text(this, &SHansaAuthoringStudio::GetDefinitionCountText)
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SAssignNew(DefinitionListView, SListView<TSharedPtr<FHansaDefinitionListItem>>)
							.ListItemsSource(&FilteredDefinitions)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &SHansaAuthoringStudio::GenerateDefinitionRow)
							.OnSelectionChanged(this, &SHansaAuthoringStudio::OnDefinitionSelected)
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.55f)
				.MinSize(420.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
					.Padding(8.0f)
					[
						DetailsView.ToSharedRef()
					]
				]
				+ SSplitter::Slot()
				.Value(0.25f)
				.MinSize(280.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
					.Padding(12.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("HansaAuthoringStudio", "Validation", "Validation"))
							.TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.Text(this, &SHansaAuthoringStudio::GetValidationSummaryText)
							.ColorAndOpacity(this, &SHansaAuthoringStudio::GetValidationSummaryColor)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("HansaAuthoringStudio", "SelectedDefinition", "Selected definition"))
							.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SHansaAuthoringStudio::GetSelectedDefinitionText)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("HansaAuthoringStudio", "FixturePreview", "Fixture preview"))
							.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SHansaAuthoringStudio::GetFixturePreviewText)
							.ColorAndOpacity(this, &SHansaAuthoringStudio::GetFixturePreviewColor)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SNew(SSpacer)
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(0.25f)
			.MinHeight(180.0f)
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				.Padding(4.0f)
				[
					SAssignNew(ValidationListView, SListView<TSharedPtr<FHansaStudioValidationItem>>)
					.ListItemsSource(&ValidationItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SHansaAuthoringStudio::GenerateValidationRow)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(TEXT("Severity")).DefaultLabel(NSLOCTEXT("HansaAuthoringStudio", "Severity", "Severity")).FixedWidth(100.0f)
						+ SHeaderRow::Column(TEXT("Code")).DefaultLabel(NSLOCTEXT("HansaAuthoringStudio", "Code", "Code")).FixedWidth(130.0f)
						+ SHeaderRow::Column(TEXT("Cause")).DefaultLabel(NSLOCTEXT("HansaAuthoringStudio", "Cause", "Cause")).FillWidth(0.38f)
						+ SHeaderRow::Column(TEXT("Field")).DefaultLabel(NSLOCTEXT("HansaAuthoringStudio", "Field", "Field path")).FillWidth(0.18f)
						+ SHeaderRow::Column(TEXT("Remedy")).DefaultLabel(NSLOCTEXT("HansaAuthoringStudio", "Remedy", "Remedy")).FillWidth(0.44f)
					)
				]
			]
		]
	];

	if (FilteredDefinitions.Num() > 0)
	{
		DefinitionListView->SetSelection(FilteredDefinitions[0], ESelectInfo::Direct);
	}
	else
	{
		RebuildValidationResults();
	}
}

void SHansaAuthoringStudio::DiscoverDefinitions()
{
	AllDefinitions.Reset();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByClass(UHansaDefinitionBase::StaticClass()->GetClassPathName(), Assets, true);
	Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.GetObjectPathString().Compare(Right.GetObjectPathString(), ESearchCase::CaseSensitive) < 0;
	});
	for (const FAssetData& Asset : Assets)
	{
		if (UHansaDefinitionBase* Definition = Cast<UHansaDefinitionBase>(Asset.GetAsset()))
		{
			AllDefinitions.Add(MakeShared<FHansaDefinitionListItem>(Definition, Asset.GetObjectPathString()));
		}
	}

	UHansaFoundationSampleDefinition* Sample = NewObject<UHansaFoundationSampleDefinition>(GetTransientPackage());
	Sample->SetFlags(RF_Transactional);
	AllDefinitions.Add(MakeShared<FHansaDefinitionListItem>(Sample, TEXT("Transient editor sample")));
	AllDefinitions.Sort([](const TSharedPtr<FHansaDefinitionListItem>& Left, const TSharedPtr<FHansaDefinitionListItem>& Right)
	{
		return Left->Definition->StableDefinitionId.Compare(Right->Definition->StableDefinitionId, ESearchCase::CaseSensitive) < 0;
	});
	FilteredDefinitions = AllDefinitions;
}

void SHansaAuthoringStudio::ApplySearchFilter(const FText& SearchText)
{
	const FString Search = SearchText.ToString().TrimStartAndEnd();
	FilteredDefinitions = AllDefinitions.FilterByPredicate([&Search](const TSharedPtr<FHansaDefinitionListItem>& Item)
	{
		return Search.IsEmpty() ||
			Item->Definition->StableDefinitionId.Contains(Search, ESearchCase::IgnoreCase) ||
			Item->Definition->GetClass()->GetDisplayNameText().ToString().Contains(Search, ESearchCase::IgnoreCase) ||
			Item->SourcePath.Contains(Search, ESearchCase::IgnoreCase);
	});
	if (DefinitionListView.IsValid())
	{
		DefinitionListView->RequestListRefresh();
	}
}

void SHansaAuthoringStudio::OnDefinitionSelected(
	TSharedPtr<FHansaDefinitionListItem> Item,
	ESelectInfo::Type SelectInfo)
{
	SelectedDefinition = MoveTemp(Item);
	DetailsView->SetObject(SelectedDefinition.IsValid() ? SelectedDefinition->Definition.Get() : nullptr, true);
	RebuildValidationResults();
}

TSharedRef<ITableRow> SHansaAuthoringStudio::GenerateDefinitionRow(
	TSharedPtr<FHansaDefinitionListItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FHansaDefinitionListItem>>, OwnerTable)
		.Padding(FMargin(8.0f, 6.0f))
		.ToolTipText(FText::FromString(Item->SourcePath))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Item->Definition->StableDefinitionId))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Item->Definition->GetClass()->GetDisplayNameText())
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

TSharedRef<ITableRow> SHansaAuthoringStudio::GenerateValidationRow(
	TSharedPtr<FHansaStudioValidationItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FHansaStudioValidationItem>>, OwnerTable)
		.Padding(FMargin(4.0f, 5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(92.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->SeverityLabel))
					.ColorAndOpacity(Hansa::Editor::Studio::SeverityColor(Item->Severity))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(122.0f)[SNew(STextBlock).Text(FText::FromString(Item->Code))]
			]
			+ SHorizontalBox::Slot().FillWidth(0.38f).Padding(4.0f, 0.0f)
			[
				SNew(STextBlock).Text(FText::FromString(Item->Cause)).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().FillWidth(0.18f).Padding(4.0f, 0.0f)
			[
				SNew(STextBlock).Text(FText::FromString(Item->PropertyPath)).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().FillWidth(0.44f).Padding(4.0f, 0.0f)
			[
				SNew(STextBlock).Text(FText::FromString(Item->Remedy)).AutoWrapText(true)
			]
		];
}

FReply SHansaAuthoringStudio::ValidateSelectedDefinition()
{
	SchemaRegistry.Refresh();
	RebuildValidationResults();
	return FReply::Handled();
}

FReply SHansaAuthoringStudio::ExportSchemas()
{
	SchemaRegistry.Refresh();
	TArray<FString> Files;
	FString Error;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SchemaExport"));
	ValidationItems.Reset();
	if (SchemaRegistry.ExportAllJsonSchemas(Directory, Files, Error))
	{
		ValidationItems.Add(MakeShared<FHansaStudioValidationItem>(FHansaStudioValidationItem {
			EHansaSchemaDiagnosticSeverity::Information,
			TEXT("Info"),
			TEXT("HSA-SCHEMA-EXPORT"),
			TEXT("SchemaExport"),
			FString::Printf(TEXT("Exported %d deterministic JSON Schema file(s)."), Files.Num()),
			Directory
		}));
	}
	else
	{
		ValidationItems.Add(MakeShared<FHansaStudioValidationItem>(FHansaStudioValidationItem {
			EHansaSchemaDiagnosticSeverity::Error,
			TEXT("Error"),
			TEXT("HSA-SCHEMA-EXPORT"),
			TEXT("SchemaExport"),
			Error,
			TEXT("Resolve schema metadata errors and retry the export.")
		}));
	}
	ValidationListView->RequestListRefresh();
	return FReply::Handled();
}

FReply SHansaAuthoringStudio::RunGrainShortageFixture()
{
	using namespace Hansa::Simulation;
	bFixturePreviewRan = true;
	bFixturePreviewSucceeded = false;
	const THansaValueResult<FHansaProductionFixture> Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!Created)
	{
		FixturePreviewText = NSLOCTEXT("HansaAuthoringStudio", "FixtureInitFailed",
			"✕ The versioned fixture failed deterministic initialization.");
		return FReply::Handled();
	}

	FHansaProductionFixture Fixture = Created.Value;
	const FHansaCityDefinitionId CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
	const FHansaGoodId GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
	const FHansaInventoryId InventoryId = FHansaInventoryId::TryCreate(1).Value;
	const auto Capture = [&Fixture, CityId, GoodId, InventoryId](int64& OutTick, int64& OutStock,
		int64& OutReserve, int64& OutPrice, int64& OutUnmet, bool& bOutShortage)
	{
		const FHansaSimulationReadOnlyAccess ReadOnly = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions());
		const TOptional<FHansaCityMarketProjection> Market = ReadOnly.QueryMarket(CityId, GoodId);
		const TOptional<FHansaInventoryStockProjection> Stock = ReadOnly.GetInventories().QueryStock(InventoryId, GoodId);
		if (!Market.IsSet() || !Stock.IsSet())
		{
			return false;
		}
		OutTick = ReadOnly.GetClock().GetTick().GetValue();
		OutStock = Stock->Stock.GetRawValue();
		OutReserve = Market->DesiredReserve.GetRawValue();
		OutPrice = Market->CurrentPriceMilliMarks;
		OutUnmet = Market->UnmetDemand.GetRawValue();
		bOutShortage = ReadOnly.QueryMarketAlerts(CityId, GoodId).ContainsByPredicate([](const FHansaMarketAlertProjection& Alert)
		{
			return Alert.Type == EHansaMarketAlertType::Shortage;
		});
		return true;
	};

	int64 BaselineTick = 0, BaselineStock = 0, BaselineReserve = 0, BaselinePrice = 0, BaselineUnmet = 0;
	int64 ShortageTick = 0, ShortageStock = 0, ShortageReserve = 0, ShortagePrice = 0, ShortageUnmet = 0;
	int64 RecoveryTick = 0, RecoveryStock = 0, RecoveryReserve = 0, RecoveryPrice = 0, RecoveryUnmet = 0;
	bool bBaselineShortage = false, bShortage = false, bRecoveryShortage = false;
	const FHansaProductionId RecoveryProduction = FHansaProductionId::TryCreate(10).Value;
	const bool bCompleted = Capture(BaselineTick, BaselineStock, BaselineReserve, BaselinePrice, BaselineUnmet, bBaselineShortage) &&
		Fixture.Step(5).IsSuccess() &&
		Capture(ShortageTick, ShortageStock, ShortageReserve, ShortagePrice, ShortageUnmet, bShortage) &&
		Fixture.SetProductionActive(RecoveryProduction, true).IsSuccess() &&
		Fixture.SetProductionActive(RecoveryProduction, false).IsSuccess() &&
		Fixture.Step(3).IsSuccess() &&
		Capture(RecoveryTick, RecoveryStock, RecoveryReserve, RecoveryPrice, RecoveryUnmet, bRecoveryShortage);

	bFixturePreviewSucceeded = bCompleted && !bBaselineShortage && bShortage && !bRecoveryShortage &&
		RecoveryStock >= RecoveryReserve && ShortagePrice > BaselinePrice && RecoveryPrice < ShortagePrice;
	if (!bCompleted)
	{
		FixturePreviewText = NSLOCTEXT("HansaAuthoringStudio", "FixtureRunFailed",
			"✕ The fixture could not complete through the gameplay command gateway.");
		return FReply::Handled();
	}
	FixturePreviewText = FText::Format(
		bFixturePreviewSucceeded
			? NSLOCTEXT("HansaAuthoringStudio", "FixturePreviewPassed",
				"✓ lubeck_grain_shortage_v1\nBaseline · tick {0} · stock {1} · price {2}\nShortage · tick {3} · stock {4} · price {5} · unmet {6}\nRecovered · tick {7} · stock {8}/{9} reserve · price {10}")
			: NSLOCTEXT("HansaAuthoringStudio", "FixturePreviewFailed",
				"✕ lubeck_grain_shortage_v1 did not meet its recovery contract\nBaseline · tick {0} · stock {1} · price {2}\nShortage · tick {3} · stock {4} · price {5} · unmet {6}\nFinal · tick {7} · stock {8}/{9} reserve · price {10}"),
		FText::AsNumber(BaselineTick), FText::AsNumber(BaselineStock), FText::AsNumber(BaselinePrice),
		FText::AsNumber(ShortageTick), FText::AsNumber(ShortageStock), FText::AsNumber(ShortagePrice), FText::AsNumber(ShortageUnmet),
		FText::AsNumber(RecoveryTick), FText::AsNumber(RecoveryStock), FText::AsNumber(RecoveryReserve), FText::AsNumber(RecoveryPrice));
	return FReply::Handled();
}

FReply SHansaAuthoringStudio::Undo()
{
	if (GEditor != nullptr)
	{
		GEditor->UndoTransaction();
	}
	return FReply::Handled();
}

FReply SHansaAuthoringStudio::Redo()
{
	if (GEditor != nullptr)
	{
		GEditor->RedoTransaction();
	}
	return FReply::Handled();
}

bool SHansaAuthoringStudio::CanUndo() const
{
	return GEditor != nullptr && GEditor->Trans != nullptr && GEditor->Trans->CanUndo();
}

bool SHansaAuthoringStudio::CanRedo() const
{
	return GEditor != nullptr && GEditor->Trans != nullptr && GEditor->Trans->CanRedo();
}

void SHansaAuthoringStudio::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SelectedDefinition.IsValid())
	{
		SelectedDefinition->Definition->RefreshContentHash();
	}
	RebuildValidationResults();
}

void SHansaAuthoringStudio::PostUndo(const bool bSuccess)
{
	if (bSuccess)
	{
		RefreshAfterTransaction();
	}
}

void SHansaAuthoringStudio::PostRedo(const bool bSuccess)
{
	if (bSuccess)
	{
		RefreshAfterTransaction();
	}
}

void SHansaAuthoringStudio::RefreshAfterTransaction()
{
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
	RebuildValidationResults();
}

void SHansaAuthoringStudio::RebuildValidationResults()
{
	ValidationItems.Reset();
	for (const FHansaSchemaDiagnostic& Diagnostic : SchemaRegistry.GetDiagnostics())
	{
		if (!SelectedDefinition.IsValid() || Diagnostic.SchemaId.IsEmpty() ||
			Diagnostic.SchemaId == SelectedDefinition->Definition->GetClass()->GetMetaData(TEXT("HansaSchemaId")))
		{
			AddSchemaDiagnostic(Diagnostic);
		}
	}

	if (SelectedDefinition.IsValid())
	{
		TArray<FHansaDefinitionValidationIssue> Issues;
		SelectedDefinition->Definition->ValidateDefinition(Issues);
		for (const FHansaDefinitionValidationIssue& Issue : Issues)
		{
			AddDefinitionIssue(Issue);
		}

		if (SelectedDefinition->Definition->IsA<UHansaGoodDefinition>() ||
			SelectedDefinition->Definition->IsA<UHansaRecipeDefinition>() ||
			SelectedDefinition->Definition->IsA<UHansaBuildingDefinition>() ||
			SelectedDefinition->Definition->IsA<UHansaNeedDefinition>() ||
			SelectedDefinition->Definition->IsA<UHansaPopulationTierDefinition>() ||
			SelectedDefinition->Definition->IsA<UHansaCityMarketProfileDefinition>())
		{
			TArray<const UHansaDefinitionBase*> EconomicDefinitions;
			for (const TSharedPtr<FHansaDefinitionListItem>& Item : AllDefinitions)
			{
				if (Item->Definition->IsA<UHansaGoodDefinition>() ||
					Item->Definition->IsA<UHansaRecipeDefinition>() ||
					Item->Definition->IsA<UHansaBuildingDefinition>() ||
					Item->Definition->IsA<UHansaNeedDefinition>() ||
					Item->Definition->IsA<UHansaPopulationTierDefinition>() ||
					Item->Definition->IsA<UHansaCityMarketProfileDefinition>())
				{
					EconomicDefinitions.Add(Item->Definition.Get());
				}
			}
			const FHansaEconomicRegistryCompileResult CompileResult =
				FHansaEconomicDefinitionCompiler::Compile(EconomicDefinitions);
			for (const FHansaDefinitionValidationIssue& Issue : CompileResult.Issues)
			{
				if (Issue.Code.ToString().StartsWith(TEXT("HSA-REGISTRY")) &&
					Issue.PropertyPath.StartsWith(SelectedDefinition->Definition->StableDefinitionId))
				{
					AddDefinitionIssue(Issue);
				}
			}
			if (CompileResult.IsValid())
			{
				ValidationItems.Add(MakeShared<FHansaStudioValidationItem>(FHansaStudioValidationItem {
					EHansaSchemaDiagnosticSeverity::Information,
					TEXT("Info"),
					TEXT("HSA-REGISTRY-READY"),
					TEXT("EconomicRegistry"),
					FString::Printf(
						TEXT("✓ Compiled %d goods, %d recipes, %d buildings, %d needs, %d population tiers and %d city markets — hash %016llx."),
						CompileResult.Registry.GetGoods().Num(),
						CompileResult.Registry.GetRecipes().Num(),
						CompileResult.Registry.GetBuildings().Num(),
						CompileResult.Registry.GetNeeds().Num(),
						CompileResult.Registry.GetPopulationTiers().Num(),
						CompileResult.Registry.GetCityMarkets().Num(),
						static_cast<unsigned long long>(CompileResult.Registry.GetRegistryHash())),
					TEXT("No action required.")
				}));
			}
		}
	}

	if (ValidationItems.IsEmpty())
	{
		ValidationItems.Add(MakeShared<FHansaStudioValidationItem>(FHansaStudioValidationItem {
			EHansaSchemaDiagnosticSeverity::Information,
			TEXT("Info"),
			TEXT("HSA-READY"),
			TEXT("Definition"),
			TEXT("✓ Ready — reflected metadata and selected definition validation passed."),
			TEXT("No action required.")
		}));
	}

	if (ValidationListView.IsValid())
	{
		ValidationListView->RequestListRefresh();
	}
}

void SHansaAuthoringStudio::AddSchemaDiagnostic(const FHansaSchemaDiagnostic& Diagnostic)
{
	ValidationItems.Add(MakeShared<FHansaStudioValidationItem>(FHansaStudioValidationItem {
		Diagnostic.Severity,
		Hansa::Editor::Studio::SeverityLabel(Diagnostic.Severity),
		Diagnostic.Code,
		Diagnostic.PropertyPath,
		Diagnostic.Cause,
		Diagnostic.Remedy
	}));
}

void SHansaAuthoringStudio::AddDefinitionIssue(const FHansaDefinitionValidationIssue& Issue)
{
	const EHansaSchemaDiagnosticSeverity Severity = Hansa::Editor::Studio::ConvertSeverity(Issue.Severity);
	ValidationItems.Add(MakeShared<FHansaStudioValidationItem>(FHansaStudioValidationItem {
		Severity,
		Hansa::Editor::Studio::SeverityLabel(Severity),
		Issue.Code.ToString(),
		Issue.PropertyPath,
		Issue.Cause.ToString(),
		Issue.Remedy.ToString()
	}));
}

FText SHansaAuthoringStudio::GetDefinitionCountText() const
{
	return FText::Format(
		NSLOCTEXT("HansaAuthoringStudio", "DefinitionCount", "{0} definition(s)"),
		FText::AsNumber(FilteredDefinitions.Num()));
}

FText SHansaAuthoringStudio::GetSelectedDefinitionText() const
{
	if (!SelectedDefinition.IsValid())
	{
		return NSLOCTEXT("HansaAuthoringStudio", "NoSelection", "No definition selected.");
	}
	return FText::Format(
		NSLOCTEXT("HansaAuthoringStudio", "SelectionFormat", "{0}\n{1}\n{2}"),
		FText::FromString(SelectedDefinition->Definition->StableDefinitionId),
		SelectedDefinition->Definition->GetClass()->GetDisplayNameText(),
		FText::FromString(SelectedDefinition->SourcePath));
}

FText SHansaAuthoringStudio::GetValidationSummaryText() const
{
	int32 Errors = 0;
	int32 Warnings = 0;
	for (const TSharedPtr<FHansaStudioValidationItem>& Item : ValidationItems)
	{
		if (Item->Severity == EHansaSchemaDiagnosticSeverity::Error)
		{
			++Errors;
		}
		else if (Item->Severity == EHansaSchemaDiagnosticSeverity::Warning)
		{
			++Warnings;
		}
	}
	if (Errors > 0)
	{
		return FText::Format(NSLOCTEXT("HansaAuthoringStudio", "ErrorsSummary", "✕ {0} error(s)"), FText::AsNumber(Errors));
	}
	if (Warnings > 0)
	{
		return FText::Format(NSLOCTEXT("HansaAuthoringStudio", "WarningsSummary", "⚠ {0} warning(s)"), FText::AsNumber(Warnings));
	}
	return NSLOCTEXT("HansaAuthoringStudio", "ReadySummary", "✓ Ready");
}

FSlateColor SHansaAuthoringStudio::GetValidationSummaryColor() const
{
	if (ValidationItems.ContainsByPredicate([](const TSharedPtr<FHansaStudioValidationItem>& Item)
	{
		return Item->Severity == EHansaSchemaDiagnosticSeverity::Error;
	}))
	{
		return FSlateColor(Hansa::Editor::Studio::Oxblood);
	}
	if (ValidationItems.ContainsByPredicate([](const TSharedPtr<FHansaStudioValidationItem>& Item)
	{
		return Item->Severity == EHansaSchemaDiagnosticSeverity::Warning;
	}))
	{
		return FSlateColor(Hansa::Editor::Studio::WarningAmber);
	}
	return FSlateColor(Hansa::Editor::Studio::ProsperityTeal);
}

FText SHansaAuthoringStudio::GetFixturePreviewText() const
{
	return FixturePreviewText;
}

FSlateColor SHansaAuthoringStudio::GetFixturePreviewColor() const
{
	if (!bFixturePreviewRan)
	{
		return FSlateColor::UseForeground();
	}
	return bFixturePreviewSucceeded
		? FSlateColor(Hansa::Editor::Studio::ProsperityTeal)
		: FSlateColor(Hansa::Editor::Studio::Oxblood);
}
