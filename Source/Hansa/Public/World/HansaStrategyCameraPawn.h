#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "World/HansaStrategyCameraModel.h"

#include "HansaStrategyCameraPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

/** Strategy camera driven only through device-neutral intents. */
UCLASS(Blueprintable)
class HANSA_API AHansaStrategyCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AHansaStrategyCameraPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void SetPanIntent(FVector2D Value);

    /** Screen delta is in pixels (positive Y down). Release clears pending drag. */
    UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
    void SetDragPanIntent(FVector2D PixelDelta, bool bActive);

    UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
    void SetDragRotateIntent(float HorizontalPixelDelta, bool bActive);

    /** Ctrl+RMB orbit: X changes yaw, negative screen Y tilts the view upward. */
    UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
    void SetDragOrbitIntent(FVector2D PixelDelta, bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void SetRotateIntent(float Value);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void AddZoomIntent(float Steps);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void SetFastPanIntent(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void ClearCameraIntents();

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void FocusWorldLocationIntent(FVector WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	FVector2D GetFocusLocation2D() const { return CameraState.Focus; }

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	float GetCameraYawDegrees() const { return CameraState.YawDegrees; }

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	float GetCameraPitchDegrees() const { return CameraState.PitchDegrees; }

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	float GetZoomDistance() const { return CameraState.ZoomDistance; }

	/** Applies the presentation lighting EV to the final player view. */
	void SetPresentationExposureEV100(float EV100);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
	float EdgePanMarginPixels = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera")
	bool bEnableMouseEdgePan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
	float PanUnitsPerSecond = 2400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
    float DragPanUnitsPerPixel = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
    float DragRotationDegreesPerPixel = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "1.0"))
	float FastPanMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
	float RotationDegreesPerSecond = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
	float ZoomUnitsPerStep = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "1.0"))
	float MinimumZoomDistance = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "1.0"))
	float MaximumZoomDistance = 9500.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|Camera")
	TObjectPtr<UCameraComponent> Camera;

    Hansa::Game::FHansaStrategyCameraState GetViewState() const { return CameraState; }
    void RestoreViewState(const Hansa::Game::FHansaStrategyCameraState& State);
    void SetViewBounds(FVector2D Min, FVector2D Max);
    void RestoreHomeBounds();
    FVector2D GetViewBoundsMin() const { return MapBoundsMin; }
    FVector2D GetViewBoundsMax() const { return MapBoundsMax; }
private:
	FVector2D GetMouseEdgePanIntent() const;
	void ResolveMapBounds();
	void ApplyCameraState();

	UPROPERTY(VisibleAnywhere, Category = "Hansa|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	Hansa::Game::FHansaStrategyCameraState CameraState;
	Hansa::Game::FHansaStrategyCameraIntent PendingIntent;
    bool bDragPanActive = false;
    bool bDragRotateActive = false;
    double LastCameraYawDiagnosticTime = -1.0;
	FVector2D MapBoundsMin = FVector2D(-12000.0f, -8000.0f);
	FVector2D MapBoundsMax = FVector2D(12000.0f, 8000.0f);
};

