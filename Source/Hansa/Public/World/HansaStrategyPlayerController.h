#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "HansaStrategyPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FHansaWorldSelectionChanged,
	AActor*, SelectedActor,
	const FHitResult&, HitResult);

/** Enhanced Input adapter and world-selection trace for the strategy camera. */
UCLASS(Blueprintable)
class HANSA_API AHansaStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHansaStrategyPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "Hansa|World|Selection")
	bool TraceWorldSelection(FHitResult& OutHit) const;

	UFUNCTION(BlueprintCallable, Category = "Hansa|World|Selection")
	void PerformWorldSelection();

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Selection")
	AActor* GetSelectedWorldActor() const { return SelectedWorldActor.Get(); }

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

	TWeakObjectPtr<AActor> SelectedWorldActor;
	bool bOwnsRuntimeMappingContext = false;
};
