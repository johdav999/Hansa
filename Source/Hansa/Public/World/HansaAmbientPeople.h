#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaAmbientView.h"
#include "HansaAmbientPeople.generated.h"

class USkeletalMesh;
class UAnimSequence;
class USkeletalMeshComponent;
class UHansaRuntimeSimulationHost;
class AHansaLubeckWorldFoundation;

/** Cosmetic character contract. All clips must use this mesh's skeleton and an in-place root. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaAmbientPersonProfile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Identity") FName CharacterId = TEXT("Human.Laborer01");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Assets") TSoftObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Human.Locomotion.Walk") TSoftObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Human.Social.Idle") TSoftObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Human.Social.LookAround") TSoftObjectPtr<UAnimSequence> LookAround;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Population", meta=(ClampMin="1", ClampMax="100")) int32 Weight = 1;
    /** Centimetres per second at playback rate 1; tune from the source gait. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="20", ClampMax="250", Units="cm/s")) float WalkSpeed = 116.49f;
    /** Imported mesh forward axis relative to Unreal +X. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(Units="deg")) float MeshYaw = -90;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin="-20", ClampMax="20", Units="cm")) float GroundOffset = 2.5f;
    static FHansaAmbientPersonProfile Laborer();
};

UENUM(BlueprintType)
enum class EHansaPersonActivity : uint8 { Walking, Visiting, Idle };

USTRUCT(BlueprintType)
struct HANSA_API FHansaPersonObservation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 PersonId = 0;
    UPROPERTY(BlueprintReadOnly) FName CharacterId;
    UPROPERTY(BlueprintReadOnly) EHansaPersonActivity Activity = EHansaPersonActivity::Idle;
    UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) int64 DestinationBuilding = 0;
    UPROPERTY(BlueprintReadOnly) FName DestinationLandmark;
    UPROPERTY(BlueprintReadOnly) FName CityId;
    UPROPERTY(BlueprintReadOnly) bool bVisible = false;
};

namespace Hansa::Game
{
    struct HANSA_API FAmbientVisit
    {
        Hansa::Simulation::FHansaBuildingId Building;
        FIntPoint Road = FIntPoint::ZeroValue;
        FIntPoint Inside = FIntPoint::ZeroValue;
        FName Landmark;
    };
    /** Completed roads only. Frontages stay on road land outside occupied footprints. */
    struct HANSA_API FAmbientStreetNetwork
    {
        TSet<FIntPoint> Roads;
        TArray<FAmbientVisit> Visits;
        TMap<FIntPoint,FVector> MunicipalPositions;
        TMap<FIntPoint,TArray<FIntPoint>> MunicipalLinks;
        void BuildMunicipal(const AHansaCityCentrePresentation& Centre);
        void Build(TConstArrayView<Hansa::Simulation::FHansaBuildingWorldProjection> Buildings,
            Hansa::Simulation::FHansaCityDefinitionId City);
        bool Route(FIntPoint From, FIntPoint To, TArray<FIntPoint>& Out) const;
        static int32 DesiredCount(int32 Residents, int32 ResidentsPerPerson, int32 Maximum, int32 RoadCount, int32 VisitCount);
    };
}

USTRUCT()
struct FHansaLivePerson
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> LookAround;
    int32 Profile = 0;
    bool bActive = false;
    EHansaPersonActivity Activity = EHansaPersonActivity::Idle;
    FIntPoint Cell = FIntPoint::ZeroValue;
    TArray<FIntPoint> Route;
    int32 Waypoint = 0;
    Hansa::Game::FAmbientVisit Visit;
    FVector Frontage = FVector::ZeroVector;
    FVector Lane = FVector::ZeroVector;
    bool bAtFrontage = false;
    bool bReturning = false;
    float Wait = 0;
    float PoseTime = 0;
};

/** Population-scaled local presentation. Never creates residents, jobs, goods or save state. */
UCLASS(Blueprintable)
class HANSA_API AHansaAmbientPeople : public AActor
{
    GENERATED_BODY()
public:
    AHansaAmbientPeople();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hansa|People") TArray<FHansaAmbientPersonProfile> Characters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hansa|People", meta=(ClampMin="1", ClampMax="1000")) int32 ResidentsPerPerson = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hansa|People", meta=(ClampMin="0", ClampMax="128")) int32 MaximumPeople = 80;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|People") FString ValidationError;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|People") int32 TargetPopulation = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|People") int32 CompletedVisits = 0;
    UFUNCTION(BlueprintPure, Category="Hansa|People") FName GetAmbientCity() const { return View.City; }
    UFUNCTION(BlueprintPure, Category="Hansa|People") TArray<FHansaPersonObservation> QueryPeople() const;
    static bool ValidateProfile(const FHansaAmbientPersonProfile& Profile, FString& Error);
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
    // Explicit injection supports the same runtime in deterministic integration fixtures.
    UPROPERTY(Transient) TObjectPtr<UHansaRuntimeSimulationHost> Host;
    UPROPERTY(Transient) TObjectPtr<AHansaLubeckWorldFoundation> Foundation;
private:
    void RefreshCity();
    bool Activate(FHansaLivePerson& Person);
    bool ChooseVisit(FHansaLivePerson& Person);
    bool Ground(const FVector& Candidate, FVector& Out) const;
    void Sample(FHansaLivePerson& Person, float Delta);
    FVector StreetPosition(FIntPoint Cell) const;
    Hansa::Game::FAmbientView View;
    UPROPERTY(Transient) TArray<FHansaLivePerson> People;
    UPROPERTY(Transient) TArray<FHansaAmbientPersonProfile> RuntimeCharacters;
    Hansa::Game::FAmbientStreetNetwork Streets;
    TArray<int32> ValidProfiles;
    FRandomStream Random;
    float RefreshIn = 0;
    bool bInitialized = false;
};
