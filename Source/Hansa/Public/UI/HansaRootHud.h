#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "HansaRootHud.generated.h"

class UHansaHudPresentationModel;
class UHansaBuildMenuPresentationModel;
class UHansaCityOverviewPresentationModel;
class UHansaInspectorPresentationModel;
class UHansaMarketTablePresentationModel;
class UHansaResearchPresentationModel;
class UHansaScenarioPresentationModel;
class UHansaSaveLoadPresentationModel;
class UHansaTradeMapPresentationModel;
class UHansaRuntimeSimulationHost;
class UHansaSaveSubsystem;
class SWidget;
class FViewport;
namespace Hansa::UI { class SHansaRootHud; }

/** Production viewport owner for the native root HUD shell. */
UCLASS()
class HANSA_API AHansaRootHud final : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|HUD")
	UHansaHudPresentationModel* GetPresentationModel() const { return PresentationModel; }

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Build")
	UHansaBuildMenuPresentationModel* GetBuildMenuPresentationModel() const { return BuildMenuPresentationModel; }

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Inspector")
	UHansaInspectorPresentationModel* GetInspectorPresentationModel() const { return InspectorPresentationModel; }

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|City Overview")
	UHansaCityOverviewPresentationModel* GetCityOverviewPresentationModel() const { return CityOverviewPresentationModel; }

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Market")
	UHansaMarketTablePresentationModel* GetMarketTablePresentationModel() const { return MarketTablePresentationModel; }
	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Trade")
	UHansaTradeMapPresentationModel* GetTradeMapPresentationModel() const { return TradeMapPresentationModel; }
	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Research")
	UHansaResearchPresentationModel* GetResearchPresentationModel() const { return ResearchPresentationModel; }
	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Scenario")
	UHansaScenarioPresentationModel* GetScenarioPresentationModel() const { return ScenarioPresentationModel; }
	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Save")
	UHansaSaveLoadPresentationModel* GetSaveLoadPresentationModel() const { return SaveLoadPresentationModel; }

private:
	UFUNCTION()
	void HandleWorldSelectionChanged(AActor* SelectedActor, const FHitResult& HitResult);
	void HandleHudPresentationChanged(const struct FHansaHudPresentationSnapshot& Snapshot, uint64 Revision);
	void HandleSimulationAdvanced(int64 SimulationTick);
	void RefreshCityOverview();
	void RefreshScenario();
	void HandleMarketRouteRequested(FName GoodStableId);
	void HandleCityOverviewRelatedTarget(FName SemanticId, int64 BuildingValue);
	void RefreshInspectorFromBuilding(const class AHansaBuildingWorldProjectionActor& Building);
	void HandleInspectorFrameRequested(int64 BuildingValue);
	void HandleViewportResized(FViewport* Viewport, uint32 Unused);

	UPROPERTY(Transient)
	TObjectPtr<UHansaHudPresentationModel> PresentationModel;

	UPROPERTY(Transient)
	TObjectPtr<UHansaBuildMenuPresentationModel> BuildMenuPresentationModel;

	UPROPERTY(Transient)
	TObjectPtr<UHansaInspectorPresentationModel> InspectorPresentationModel;

	UPROPERTY(Transient)
	TObjectPtr<UHansaCityOverviewPresentationModel> CityOverviewPresentationModel;

	UPROPERTY(Transient)
	TObjectPtr<UHansaMarketTablePresentationModel> MarketTablePresentationModel;
	UPROPERTY(Transient)
	TObjectPtr<UHansaTradeMapPresentationModel> TradeMapPresentationModel;
	UPROPERTY(Transient)
	TObjectPtr<UHansaResearchPresentationModel> ResearchPresentationModel;

	UPROPERTY(Transient)
	TObjectPtr<UHansaScenarioPresentationModel> ScenarioPresentationModel;
	UPROPERTY(Transient)
	TObjectPtr<UHansaSaveLoadPresentationModel> SaveLoadPresentationModel;

	UPROPERTY(Transient)
	TObjectPtr<UHansaSaveSubsystem> SaveSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UHansaRuntimeSimulationHost> SimulationHost;

	FDelegateHandle HudPresentationChangedHandle;
	FDelegateHandle SimulationAdvancedHandle;

	TSharedPtr<Hansa::UI::SHansaRootHud> RootHudWidget;
	TSharedPtr<SWidget> ViewportContent;
};
