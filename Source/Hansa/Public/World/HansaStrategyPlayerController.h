#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Network/HansaMultiplayerTypes.h"

#include "HansaStrategyPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AHansaBuildingPlacementGhost;
class AHansaLubeckWorldFoundation;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHansaWorldSelectionChanged,
	AActor*, SelectedActor,
	const FHitResult&, HitResult);

/** Enhanced Input adapter, world selection, and owner-only multiplayer RPC/projection endpoint. */
UCLASS(Blueprintable)
class HANSA_API AHansaStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHansaStrategyPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void ServerSubmitHansaIntent(const FHansaClientCommandIntent& Intent);

	UFUNCTION(Server, Reliable)
	void ServerSetHansaInterest(const FHansaClientInterest& Interest);

	UFUNCTION(Server, Reliable)
	void ServerRequestHansaProjectionRefresh(int64 ClientKnownRevision);

	UFUNCTION(Client, Reliable)
	void ClientReceiveHansaCommandFeedback(const FHansaClientCommandFeedback& Feedback);

	void SetServerAuthorityIdentity(uint64 PrincipalId, int64 HouseId);
	void PublishServerProjection(const FHansaClientProjectionSnapshot& Projection);
	void PublishCommandFeedback(const FHansaClientCommandFeedback& Feedback);

	[[nodiscard]] const FHansaClientProjectionSnapshot& GetClientProjection() const { return ClientProjection; }
	[[nodiscard]] const FHansaClientCommandFeedback& GetLastCommandFeedback() const { return LastCommandFeedback; }
	[[nodiscard]] uint64 GetAuthorityPrincipalId() const { return AuthorityPrincipalId; }

	UFUNCTION(BlueprintCallable, Category = "Hansa|World|Selection")
	bool TraceWorldSelection(FHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable, Category = "Hansa|World|Selection")
	void PerformWorldSelection();

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Selection")
	AActor* GetSelectedWorldActor() const { return SelectedWorldActor.Get(); }

	bool BeginBuildingPlacementDrag(FName BuildingDefinitionId, FVector2D ScreenPosition, bool bPointerOverMenu, bool bUseLivePointer = true);
	bool UpdateBuildingPlacementDrag(FVector2D ScreenPosition, bool bPointerOverMenu, bool bUseLivePointer = true);
	bool EndBuildingPlacementDrag(FVector2D ScreenPosition, bool bPointerOverMenu, bool bUseLivePointer = true);
	bool BeginRoadDrawing(FVector2D ScreenPosition, bool bUseLivePointer = true);
	bool UpdateRoadDrawing(FVector2D ScreenPosition, bool bUseLivePointer = true);
	bool EndRoadDrawing(FVector2D ScreenPosition, bool bPointerOverWorld, bool bUseLivePointer = true);
	bool ResolvePlacementCellAtScreenPosition(FVector2D ScreenPosition, FIntPoint& OutCell, FVector& OutWorldLocation) const;
	void CancelBuildingPlacement();
	void RefreshBuildingPlacementPresentation();
	/** Routes the platform Back/Escape intent through placement cancellation and the HUD layer stack. */
	void HandleEscapeIntent();
	[[nodiscard]] AHansaBuildingPlacementGhost* GetPlacementGhost() const { return PlacementGhost.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Hansa|World|Selection")
	FHansaWorldSelectionChanged OnWorldSelectionChanged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hansa|Input")
	TObjectPtr<UInputMappingContext> StrategyMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hansa|Input")
	TObjectPtr<UInputAction> PanAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hansa|Input")
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hansa|Input")
	TObjectPtr<UInputAction> RotateAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hansa|Input")
	TObjectPtr<UInputAction> FastPanAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hansa|Input")
	TObjectPtr<UInputAction> SelectAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|World|Selection", meta = (ClampMin = "1.0"))
	float SelectionTraceDistance = 200000.0f;

private:
	UFUNCTION()
	void OnRep_HansaClientProjection();

	void EnsureStrategyInputObjects();
	void AddDefaultMappings();
	class AHansaStrategyCameraPawn* GetStrategyCameraPawn() const;
	void UpdateProjectionSelection(AActor* SelectedActor);

	void HandlePan(const FInputActionValue& Value);
	void HandlePanCompleted(const FInputActionValue& Value);
	void HandleZoom(const FInputActionValue& Value);
	void HandleRotate(const FInputActionValue& Value);
	void HandleRotateCompleted(const FInputActionValue& Value);
	void HandleFastPan(const FInputActionValue& Value);
	void HandleFastPanCompleted(const FInputActionValue& Value);
	void HandleSelect(const FInputActionValue& Value);
	void HandleSelectHeld(const FInputActionValue& Value);
	void HandleSelectReleased(const FInputActionValue& Value);
    void HandleCameraDragPressed();
    void HandleCameraDragReleased();
    void UpdateCameraDrag();
    void HandleSessionMenu();
    void HandleContextHelp();
	class UHansaBuildMenuPresentationModel* GetBuildMenuModel() const;
	AHansaLubeckWorldFoundation* FindPlacementFoundation() const;
	void SyncPlacementGhostAndCursor();
	FVector2D ResolveCurrentPointer(FVector2D Fallback) const;

	UPROPERTY(ReplicatedUsing = OnRep_HansaClientProjection)
	FHansaClientProjectionSnapshot ClientProjection;

	UPROPERTY(Transient)
	FHansaClientCommandFeedback LastCommandFeedback;

	uint64 AuthorityPrincipalId = 0;
	int64 AuthorityHouseId = 0;
	TWeakObjectPtr<AActor> SelectedWorldActor;
	TWeakObjectPtr<AHansaBuildingPlacementGhost> PlacementGhost;
	bool bOwnsRuntimeMappingContext = false;
	bool bPlacementRotateHeld = false;
	bool bRoadPointerHeld = false;
    bool bCameraDragHeld = false;
    double LastCameraDragDiagnosticTime = -1.0;
    FVector2D PreviousCameraDragPointer = FVector2D::ZeroVector;
	bool IsPointerOverWorldViewport() const;
	bool TryGetPlacementPointer(FVector2D& OutPointer) const;
};
