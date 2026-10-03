#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/HansaAmbientView.h"
#include "HansaAmbientAnimals.generated.h"
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class AHansaLubeckWorldFoundation;
class UHansaRuntimeSimulationHost;

UENUM(BlueprintType)
enum class EHansaAnimalActivity : uint8 { Still, Walk, Jump };

/** Local presentation profile: no simulation identity, provider dependency or save data. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaAmbientAnimalProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Identity") FName SpeciesId = TEXT("Rabbit");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Population", meta=(ClampMin="0", ClampMax="32")) int32 MinimumPopulation = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Population", meta=(ClampMin="0", ClampMax="32")) int32 MaximumPopulation = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Population", meta=(ClampMin="1000", ClampMax="16000", Units="cm")) float TownRadius = 6000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets") TSoftObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets") TSoftObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets", meta=(ToolTip="Optional authored jump; only required when WalksBetweenJumps is positive.")) TSoftObjectPtr<UAnimSequence> Jump;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Contract", meta=(ClampMin="1")) int32 ExpectedBones = 25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Contract", meta=(ClampMin="0.1", Units="s")) float WalkCycleSeconds = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ClampMin="1", Units="cm", ToolTip="Authored forward distance per walk cycle. Speed is derived from distance / clip duration.")) float WalkCycleDistance = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ClampMin="1", ClampMax="100")) int32 MinimumWalkCycles = 7;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ClampMin="1", ClampMax="100")) int32 MaximumWalkCycles = 15;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ClampMin="0", Units="s")) float MinimumRestSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ClampMin="0", Units="s")) float MaximumRestSeconds = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ClampMin="0", ToolTip="Zero disables jumping; positive values require a compatible root-motion jump clip.")) int32 WalksBetweenJumps = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behaviour", meta=(ToolTip="Use the mesh's grounded reference pose for rests and turns rather than freezing walk frame zero.")) bool bRestInReferencePose = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clearance", meta=(ClampMin="10", Units="cm")) float BodyRadius = 65;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clearance", meta=(ClampMin="10", Units="cm")) float BodyHeight = 80;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets", meta=(ClampMin="0", ToolTip="0 = automatic; 1 = LOD0; 2 = LOD1.")) int32 ForcedLOD = 2;
    static FHansaAmbientAnimalProfile Rabbit();
    static FHansaAmbientAnimalProfile Dog();
};

USTRUCT(BlueprintType)
struct FHansaAnimalObservation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Animal") FString StableId;
    UPROPERTY(BlueprintReadOnly, Category="Animal") FName SpeciesId;
    UPROPERTY(BlueprintReadOnly, Category="Animal") EHansaAnimalActivity Activity = EHansaAnimalActivity::Still;
    UPROPERTY(BlueprintReadOnly, Category="Animal") FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Animal") FVector Destination = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Animal") float AnimationTime = 0;
    UPROPERTY(BlueprintReadOnly, Category="Animal") bool bVisible = false;
    UPROPERTY(BlueprintReadOnly, Category="Animal") bool bTurning = false;
};

USTRUCT()
struct FHansaAmbientAnimal
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    int32 ProfileIndex = 0;
    int32 SpeciesOrdinal = 0;
    EHansaAnimalActivity Activity = EHansaAnimalActivity::Still;
    FVector Start = FVector::ZeroVector, Destination = FVector::ZeroVector;
    FQuat Heading = FQuat::Identity, TurnStart = FQuat::Identity, TargetHeading = FQuat::Identity;
    float TurnElapsed = 0, TurnDuration = 0, Elapsed = 0, Duration = 0, PoseTime = 0;
    int32 WalksSinceJump = 0;
    bool bTurning = false, bPlaced = false;
};
USTRUCT()
struct FHansaAmbientAnimalAssets
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Jump;
    FVector JumpTravel = FVector::ZeroVector;
    FQuat MeshFacing = FQuat::Identity;
    float JumpPeak = 0;
    FString Error;
};

/** Shared terrain-safe local wildlife; species profiles own population and motion policy. */
UCLASS(BlueprintType, Blueprintable)
class HANSA_API AHansaAmbientAnimals : public AActor
{
    GENERATED_BODY()
public:
    AHansaAmbientAnimals();
    virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animals", meta=(TitleProperty="SpeciesId", ToolTip="Species-specific assets, population, contacts, clearance and wandering behaviour.")) TArray<FHansaAmbientAnimalProfile> Species;
    UFUNCTION(BlueprintPure, Category="Animals|Diagnostics") int32 GetLiveAnimalCount(FName SpeciesId = NAME_None) const;
    UFUNCTION(BlueprintPure, Category="Animals|Diagnostics") TArray<FHansaAnimalObservation> QueryAnimals() const;
    UFUNCTION(BlueprintPure, Category="Animals|Diagnostics") int32 GetCompletedJumpCount() const { return CompletedJumps; }
    UFUNCTION(BlueprintPure, Category="Animals|Diagnostics") FString GetValidationError() const { return ValidationError; }
    UFUNCTION(BlueprintPure, Category="Animals|Diagnostics") FName GetAmbientCity() const { return View.City; }
    static bool ValidateProfile(const FHansaAmbientAnimalProfile& Profile, USkeletalMesh* Mesh, UAnimSequence* Walk, UAnimSequence* Jump, FString& Error);
protected:
    bool bInitialized = false;
private:
    friend class FHansaRabbitBehaviorTest;
    friend class FHansaAnimalBehaviorTest;
    friend class FHansaAmbientCityTest;
    bool SafeGround(const FVector& Candidate, FVector& Ground, const FHansaAmbientAnimalProfile& Profile) const;
    bool SafePath(const FVector& From, const FVector& To, const FHansaAmbientAnimalProfile& Profile) const;
    bool Place(FHansaAmbientAnimal& Animal);
    void Stand(FHansaAmbientAnimal& Animal);
    static void BeginTurn(FHansaAmbientAnimal& Animal, const FQuat& Target);
    static void AdvanceTurn(FHansaAmbientAnimal& Animal, float Step);
    void Sample(FHansaAmbientAnimal& Animal);
    void RefreshObstacles();
    void InitializeSpecies();
    UPROPERTY(Transient) TArray<FHansaAmbientAnimal> Animals;
    UPROPERTY(Transient) TArray<FHansaAmbientAnimalAssets> Loaded;
    UPROPERTY(Transient) TObjectPtr<AHansaLubeckWorldFoundation> Foundation;
    UPROPERTY(Transient) TObjectPtr<UHansaRuntimeSimulationHost> Host;
    TSet<FIntPoint> Occupied;
    FRandomStream Random;
    FVector TownCenter = FVector::ZeroVector;
    float RefreshIn = 0;
    int32 CompletedJumps = 0;
    bool bProjectionReady = false;
    FString ValidationError;
    Hansa::Game::FAmbientView View;
    TArray<FBox> MunicipalObstacles;
};
