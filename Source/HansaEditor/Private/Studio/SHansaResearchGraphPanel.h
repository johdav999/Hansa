#pragma once

#include "CoreMinimal.h"
#include "Research/HansaResearch.h"
#include "Widgets/SCompoundWidget.h"

class UHansaDefinitionBase;
class SVerticalBox;

/** Specialized bounded research graph view; validation uses the same deterministic graph rules as compilation/CI. */
class SHansaResearchGraphPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHansaResearchGraphPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Refresh(const TArray<TSharedPtr<struct FHansaDefinitionListItem>>& Definitions);
	[[nodiscard]] const TArray<Hansa::Simulation::FHansaResearchGraphDiagnostic>& GetDiagnostics() const { return Diagnostics; }
	[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;

private:
	void RebuildLane(SVerticalBox& Lane, Hansa::Simulation::EHansaResearchBranch Branch,
		const TArray<Hansa::Simulation::FHansaCompiledTechnologyDefinition>& Technologies);
	TSharedPtr<SVerticalBox> CommerceLane;
	TSharedPtr<SVerticalBox> ProductionLane;
	TSharedPtr<SVerticalBox> LogisticsLane;
	TSharedPtr<SVerticalBox> ValidationRows;
	TArray<Hansa::Simulation::FHansaResearchGraphDiagnostic> Diagnostics;
	TArray<FString> NodeIds;
};

