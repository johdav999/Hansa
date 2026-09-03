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

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void SetRotateIntent(float Value);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void AddZoomIntent(float Steps);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void SetFastPanIntent(bool bValue);

	UFUNCTION(BlueprintCallable, Category = "Hansa|Camera|Intent")
	void ClearCameraIntents();

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	FVector2D GetFocusLocation2D() const { return CameraState.Focus; }

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	float GetCameraYawDegrees() const { return CameraState.YawDegrees; }

	UFUNCTION(BlueprintPure, Category = "Hansa|Camera")
	float GetZoomDistance() const { return CameraState.ZoomDistance; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
	float EdgePanMarginPixels = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera")
	bool bEnableMouseEdgePan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|Camera", meta = (ClampMin = "0.0"))
	float PanUnitsPerSecond = 2400.0f;

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

private:
	FVector2D GetMouseEdgePanIntent() const;
	void ResolveMapBounds();
	void ApplyCameraState();

	UPROPERTY(VisibleAnywhere, Category = "Hansa|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	Hansa::Game::FHansaStrategyCameraState CameraState;
	Hansa::Game::FHansaStrategyCameraIntent PendingIntent;
	FVector2D MapBoundsMin = FVector2D(-12000.0f, -8000.0f);
	FVector2D MapBoundsMax = FVector2D(12000.0f, 8000.0f);
};
