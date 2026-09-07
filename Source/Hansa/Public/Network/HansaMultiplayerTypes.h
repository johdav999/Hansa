#pragma once

#include "CoreMinimal.h"

#include "HansaMultiplayerTypes.generated.h"

UENUM(BlueprintType)
enum class EHansaClientIntentType : uint8
{
	PlaceBuilding = 0,
	SetRouteActive,
	QueueResearch
};

UENUM(BlueprintType)
enum class EHansaClientCommandRejection : uint8
{
	None = 0,
	AuthorityUnavailable,
	ClientNotRegistered,
	DuplicateCommand,
	CommandOrderInvalid,
	InvalidPayload,
	NotAuthorized,
	GatewayRejected
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientInterest
{
	GENERATED_BODY()

	static constexpr int32 MaximumCityCount = 4;

	UPROPERTY()
	TArray<FString> CityIds;
};

/**
 * Untrusted client intent. The server derives house, principal, command identity,
 * authoritative tick, global order, prices, costs, and all resolved outcomes.
 */
USTRUCT(BlueprintType)
struct HANSA_API FHansaClientCommandIntent
{
	GENERATED_BODY()

	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY()
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int64 ClientSequence = 0;

	UPROPERTY()
	int64 ClientNonce = 0;

	UPROPERTY()
	EHansaClientIntentType Type = EHansaClientIntentType::PlaceBuilding;

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString BuildingDefinitionId;

	UPROPERTY()
	int32 AnchorX = 0;

	UPROPERTY()
	int32 AnchorY = 0;

	UPROPERTY()
	uint8 Rotation = 0;

	UPROPERTY()
	int64 RouteId = 0;

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	FString TechnologyId;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientCommandFeedback
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAccepted = false;

	UPROPERTY()
	int64 ClientSequence = 0;

	UPROPERTY()
	int64 ClientNonce = 0;

	UPROPERTY()
	EHansaClientCommandRejection Rejection = EHansaClientCommandRejection::None;

	UPROPERTY()
	FString GatewayError;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString Remedy;

	UPROPERTY()
	int64 ServerTick = 0;

	UPROPERTY()
	int64 AcceptedGlobalSequence = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedPlacement
{
	GENERATED_BODY()

	UPROPERTY()
	int64 BuildingId = 0;

	UPROPERTY()
	int64 OwnerHouseId = 0;

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString BuildingDefinitionId;

	UPROPERTY()
	int32 AnchorX = 0;

	UPROPERTY()
	int32 AnchorY = 0;

	UPROPERTY()
	FString Status;

	UPROPERTY()
	int64 ProgressPartsPerMillion = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedMarket
{
	GENERATED_BODY()

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString GoodId;

	UPROPERTY()
	int64 StockMilliUnits = 0;

	UPROPERTY()
	int64 DesiredReserveMilliUnits = 0;

	UPROPERTY()
	int64 CurrentPriceMilliMarks = 0;

	UPROPERTY()
	int64 ReportAgeTicks = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedRoute
{
	GENERATED_BODY()

	UPROPERTY()
	int64 RouteId = 0;

	UPROPERTY()
	int64 OwnerHouseId = 0;

	UPROPERTY()
	int64 VehicleId = 0;

	UPROPERTY()
	FString Mode;

	UPROPERTY()
	FString Lifecycle;

	UPROPERTY()
	FString CurrentCityId;

	UPROPERTY()
	int32 RemainingTravelTicks = 0;

	UPROPERTY()
	bool bCargoVisible = false;

	UPROPERTY()
	int64 CargoMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedResearch
{
	GENERATED_BODY()

	UPROPERTY()
	int64 HouseId = 0;

	UPROPERTY()
	int32 AvailableResearchPoints = 0;

	UPROPERTY()
	FString ActiveTechnologyId;

	UPROPERTY()
	int32 ProgressTicks = 0;

	UPROPERTY()
	TArray<FString> CompletedTechnologyIds;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedVictoryObjective
{
	GENERATED_BODY()

	UPROPERTY()
	FString VictoryId;

	UPROPERTY()
	FString ObjectiveId;

	UPROPERTY()
	int64 CurrentValue = 0;

	UPROPERTY()
	int64 TargetValue = 0;

	UPROPERTY()
	bool bMet = false;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	int64 GlobalSequence = 0;

	UPROPERTY()
	int64 Tick = 0;

	UPROPERTY()
	FString Type;

	UPROPERTY()
	int64 IssuingHouseId = 0;

	UPROPERTY()
	int64 BuildingId = 0;

	UPROPERTY()
	int64 RouteId = 0;

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString GoodId;

	UPROPERTY()
	FString TechnologyId;
};

/**
 * Purpose-built immutable client read model. It intentionally has no authoritative
 * simulation container, mutable inventory, command queue, RNG, or save state.
 */
USTRUCT(BlueprintType)
struct HANSA_API FHansaClientProjectionSnapshot
{
	GENERATED_BODY()

	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY()
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int64 Revision = 0;

	UPROPERTY()
	int64 ServerTick = 0;

	UPROPERTY()
	bool bFullRefresh = true;

	UPROPERTY()
	int64 StartingEventSequence = 0;

	UPROPERTY()
	int64 LastEventSequence = 0;

	UPROPERTY()
	FString AuthoritativeHash;

	UPROPERTY()
	FString ProjectionDigest;

	UPROPERTY()
	int64 OwnerHouseId = 0;

	UPROPERTY()
	int64 OwnerMoneyPfennig = 0;

	UPROPERTY()
	FString ScenarioOutcome;

	UPROPERTY()
	FString WinningVictoryId;

	UPROPERTY()
	TArray<FHansaReplicatedPlacement> Placements;

	UPROPERTY()
	TArray<FHansaReplicatedMarket> Markets;

	UPROPERTY()
	TArray<FHansaReplicatedRoute> Routes;

	UPROPERTY()
	FHansaReplicatedResearch Research;

	UPROPERTY()
	TArray<FHansaReplicatedVictoryObjective> VictoryObjectives;

	UPROPERTY()
	TArray<FHansaReplicatedEvent> Events;
};
