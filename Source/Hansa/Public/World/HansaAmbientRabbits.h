#pragma once

#include "CoreMinimal.h"
#include "World/HansaAmbientAnimals.h"
#include "HansaAmbientRabbits.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class AHansaLubeckWorldFoundation;
class UHansaRuntimeSimulationHost;

UENUM(BlueprintType)
enum class EHansaRabbitActivity : uint8 { Still, Walk, Jump };

USTRUCT(BlueprintType)
struct FHansaRabbitObservation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Rabbit") FString StableId;
    UPROPERTY(BlueprintReadOnly, Category="Rabbit") EHansaRabbitActivity Activity = EHansaRabbitActivity::Still;
    UPROPERTY(BlueprintReadOnly, Category="Rabbit") FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Rabbit") FVector Destination = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Rabbit") float AnimationTime = 0;
    UPROPERTY(BlueprintReadOnly, Category="Rabbit") bool bVisible = false;
};

/** Local cosmetic wildlife. Never owns economy, collision, save state or multiplayer authority. */
UCLASS(BlueprintType, Blueprintable)
class HANSA_API AHansaAmbientRabbits : public AHansaAmbientAnimals
{
    GENERATED_BODY()
public:
    AHansaAmbientRabbits();
    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rabbit|Population", meta=(ClampMin="5", ClampMax="6"))
    int32 Population = 6;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rabbit|Population", meta=(ClampMin="1000", ClampMax="16000", Units="cm"))
    float TownRadius = 6000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rabbit|Assets")
    TSoftObjectPtr<USkeletalMesh> RabbitMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rabbit|Assets")
    TSoftObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rabbit|Assets")
    TSoftObjectPtr<UAnimSequence> Jump;

    UFUNCTION(BlueprintPure, Category="Rabbit|Diagnostics") int32 GetLiveRabbitCount() const;
    UFUNCTION(BlueprintPure, Category="Rabbit|Diagnostics") TArray<FHansaRabbitObservation> QueryRabbits() const;
    /** Fail-closed asset contract, also shared by automation. */
    static bool ValidateClips(USkeletalMesh* Mesh, UAnimSequence* WalkClip, UAnimSequence* JumpClip, FString& Error);

private:
    void SyncLegacyProfile();
};
