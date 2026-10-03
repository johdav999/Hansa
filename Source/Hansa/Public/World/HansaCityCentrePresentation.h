#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HansaCityCentrePresentation.generated.h"

namespace Hansa::Simulation { class FHansaEconomicRegistry; struct FHansaCompiledCityMarketProfileDefinition; }

/** Authored scenery only: never a building, inventory, or production entity. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaCityCentreSlot
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, Category="City") FName Id;
 UPROPERTY(EditAnywhere, Category="City") FText Label;
 UPROPERTY(EditAnywhere, Category="City") TObjectPtr<class UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere, Category="City") FVector Location=FVector::ZeroVector;
 UPROPERTY(EditAnywhere, Category="City") float Yaw=0;
 UPROPERTY(EditAnywhere, Category="City") FName ProductionChain;
 UPROPERTY(EditAnywhere, Category="City") FName ProductionStage;
 UPROPERTY(EditAnywhere, Category="City") bool bSelectable=true;
};

/** Deterministic, editor-authored municipal centre; shared by standalone and clients. */
UCLASS()
class HANSA_API AHansaCityCentrePresentation : public AActor
{
 GENERATED_BODY()
public:
 AHansaCityCentrePresentation();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
 virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
 void ApplyCity(const Hansa::Simulation::FHansaEconomicRegistry& Registry);
 void Rebuild();
 bool ValidateLayout(FString& Error) const;
 static bool IsEnabled(const FHansaCityCentreSlot& Slot, const Hansa::Simulation::FHansaCompiledCityMarketProfileDefinition* Profile);
 const FHansaCityCentreSlot* FindSlot(const UPrimitiveComponent* Component) const;
 FVector GetMarketLocation() const;
 bool IsSlotActive(const FHansaCityCentreSlot& Slot) const { return !bProfileApplied || EnabledSlots.Contains(Slot.Id); }
 /** Cosmetic crowd target for municipal scenery, not simulated residents. Zero disables citizens. */
 UPROPERTY(EditAnywhere, Category="City centre|Ambient", meta=(ClampMin="0", ClampMax="128", ToolTip="Visible citizen target on municipal streets. Cosmetic only; creates no population or workforce.")) int32 AmbientCitizens=24;
 /** Minimum terrain height for terrestrial ambient life; exclude submerged ground. */
 UPROPERTY(EditAnywhere, Category="City centre|Ambient", meta=(Units="cm", ToolTip="World-space water surface height. Animals require terrain above this height.")) float AmbientWaterLevel=0;
 UPROPERTY(EditAnywhere, Category="City centre") FName CityId=TEXT("City.Rostock");
 UPROPERTY(EditAnywhere, Category="City centre", meta=(ClampMin="1")) int32 LayoutVersion=1;
 UPROPERTY(EditAnywhere, Category="City centre", meta=(TitleProperty="Id")) TArray<FHansaCityCentreSlot> Slots;
private:
 UPROPERTY(Transient) TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
 UPROPERTY(Transient) TArray<TObjectPtr<class UBoxComponent>> Selections;
 TSet<FName> EnabledSlots;
 bool bProfileApplied=false;
};
