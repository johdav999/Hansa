#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "UObject/StrongObjectPtr.h"

class UHansaDefinitionBase;

namespace Hansa::Editor::Generation
{
	struct FHansaProposalReviewError final
	{
		FString Code;
		FString Message;
		FString Remedy;
		bool IsSet() const { return !Code.IsEmpty(); }
	};

	struct FHansaProposalFixturePhase final
	{
		int64 Tick = 0;
		int64 StockMilliUnits = 0;
		int64 ReserveMilliUnits = 0;
		int64 PriceMilliMarks = 0;
		int64 UnmetDemandMilliUnits = 0;
		uint64 StateHash = 0;
		bool bShortage = false;
	};

	struct FHansaProposalFixtureRun final
	{
		uint64 RegistryHash = 0;
		FHansaProposalFixturePhase Baseline;
		FHansaProposalFixturePhase Shortage;
		FHansaProposalFixturePhase Recovery;
		bool bRecoveryContractPassed = false;
	};

	struct FHansaProposalFixtureComparison final
	{
		FHansaProposalFixtureRun Current;
		FHansaProposalFixtureRun Proposed;
	};
	struct FHansaProposalFieldDiff final
	{
		FString FieldName;
		FString DisplayName;
		FString BeforeJson;
		FString AfterJson;
		TSharedPtr<FJsonValue> ProposedValue;
		bool bSelected = true;
	};

	class FHansaDefinitionProposalReview final
	{
	public:
		static bool BuildContract(const UHansaDefinitionBase* Definition, const FHansaDefinitionClassSchema& Schema,
			const TArray<UHansaDefinitionBase*>& Definitions, TSharedPtr<FJsonObject>& OutContract, FHansaProposalReviewError& OutError);

		bool Load(UHansaDefinitionBase* Definition, const FHansaDefinitionClassSchema& Schema,
			const TSharedRef<FJsonObject>& Proposal, const TArray<UHansaDefinitionBase*>& Definitions);
		bool SetSelected(const FString& FieldName, bool bSelected);
		bool ValidateSelected(const TArray<UHansaDefinitionBase*>& Definitions);
		bool ApplySelected(const TArray<UHansaDefinitionBase*>& Definitions);
		void Reset();

		TArray<TSharedPtr<FHansaProposalFieldDiff>>& GetDiffs() { return Diffs; }
		const TArray<TSharedPtr<FHansaProposalFieldDiff>>& GetDiffs() const { return Diffs; }
		const FHansaProposalReviewError& GetError() const { return Error; }
		const FString& GetPreviewSummary() const { return PreviewSummary; }
		const TOptional<FHansaProposalFixtureComparison>& GetFixturePreview() const { return FixturePreview; }
		bool IsLoaded() const { return Target.IsValid(); }
		bool IsValidated() const { return bValidated; }
		bool HasSelectedChanges() const;

	private:
		void SetError(FString Code, FString Message, FString Remedy = FString());
		bool IsCurrentBase() const;
		bool ApplyTo(UHansaDefinitionBase* Destination, bool bIncrementRevision);

		TWeakObjectPtr<UHansaDefinitionBase> Target;
		FHansaDefinitionClassSchema TargetSchema;
		TArray<TSharedPtr<FHansaProposalFieldDiff>> Diffs;
		FHansaProposalReviewError Error;
		FString PreviewSummary;
		TOptional<FHansaProposalFixtureComparison> FixturePreview;
		int32 BaseRevision = 0;
		uint64 BaseContentHash = 0;
		bool bValidated = false;
	};
}
