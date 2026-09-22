#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

USTRUCT()
struct FHansaAmbientRabbit
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    EHansaRabbitActivity Activity = EHansaRabbitActivity::Still;
    FVector Start = FVector::ZeroVector;
    FVector Destination = FVector::ZeroVector;
    FQuat Heading = FQuat::Identity;
    float Elapsed = 0;
    float Duration = 0;
    float PoseTime = 0;
    int32 WalksSinceJump = 0;
    bool bPlaced = false;
};

/** Local cosmetic wildlife. Never owns economy, collision, save state or multiplayer authority. */
UCLASS(BlueprintType, Blueprintable)
class HANSA_API AHansaAmbientRabbits : public AActor
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
    UFUNCTION(BlueprintPure, Category="Rabbit|Diagnostics") int32 GetCompletedJumpCount() const { return CompletedJumps; }
    UFUNCTION(BlueprintPure, Category="Rabbit|Diagnostics") FString GetValidationError() const { return ValidationError; }
    /** Fail-closed asset contract, also shared by automation. */
    static bool ValidateClips(USkeletalMesh* Mesh, UAnimSequence* WalkClip, UAnimSequence* JumpClip, FString& Error);

private:
    friend class FHansaRabbitBehaviorTest;
    bool SafeGround(const FVector& Candidate, FVector& Ground) const;
    bool SafePath(const FVector& From, const FVector& To) const;
    bool Place(FHansaAmbientRabbit& Rabbit);
    void Stand(FHansaAmbientRabbit& Rabbit);
    void Sample(FHansaAmbientRabbit& Rabbit);
    void RefreshObstacles();
    UPROPERTY(Transient) TArray<FHansaAmbientRabbit> Rabbits;
    UPROPERTY(Transient) TObjectPtr<AHansaLubeckWorldFoundation> Foundation;
    UPROPERTY(Transient) TObjectPtr<UHansaRuntimeSimulationHost> Host;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> JumpClip;
    TSet<FIntPoint> Occupied;
    FRandomStream Random;
    FQuat MeshFacing = FQuat::Identity;
    FVector JumpTravel = FVector::ZeroVector;
    FVector TownCenter = FVector::ZeroVector;
    float RefreshIn = 0;
    bool bInitialized = false;
    int32 CompletedJumps = 0;
    FString ValidationError;
};
