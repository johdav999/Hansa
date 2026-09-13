#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "HansaTradeDefinitions.generated.h"

class AHansaCargoVehiclePresentation;

UENUM(BlueprintType)
enum class EHansaAuthoredRouteMode : uint8
{
	Sea = 0 UMETA(DisplayName = "Sea"),
	Land UMETA(DisplayName = "Land")
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaRouteConnectionDefinition final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (
		DisplayName = "Source city", ToolTip = "Canonical City.* identity at one end of this reachable connection.",
		HansaRequired = "true", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString SourceCityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (
		DisplayName = "Destination city", ToolTip = "Canonical City.* identity at the other end of this bidirectional connection.",
		HansaRequired = "true", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString DestinationCityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Route", meta = (
		DisplayName = "Travel time", ToolTip = "Deterministic travel ticks for this connection before seasonal modifiers.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "2147483647"))
	int32 TravelTicks = 1;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Vehicle definition", HansaSchemaId = "Hansa.VehicleDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaVehicleDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaVehicleDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade|Presentation", meta = (
		DisplayName = "Presentation actor", ToolTip = "Optional promoted cargo vehicle skin. Empty preserves legacy data; never changes simulation identity or capacity.",
		HansaRequired = "false", HansaReference = "ActorClass", HansaBulkEditable = "false", HansaAIAccess = "Never",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "OptionalAsset"))
	TSoftClassPtr<AHansaCargoVehiclePresentation> PresentationActorClass;

	UClass* LoadPresentationActorClass() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (
		DisplayName = "Route mode", ToolTip = "Network mode this vehicle can traverse.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Enum"))
	EHansaAuthoredRouteMode Mode = EHansaAuthoredRouteMode::Sea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (
		DisplayName = "Cargo capacity", ToolTip = "Maximum combined cargo carried by the vehicle.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "MilliUnit", HansaMin = "1", HansaMax = "9223372036854775807"))
	int64 CargoCapacityMilliUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (
		DisplayName = "Travel upkeep", ToolTip = "Operating cost charged for each tick spent traveling.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "PfennigPerTravelTick", HansaMin = "0", HansaMax = "1000000000"))
	int64 UpkeepPfennigPerTravelTick = 0;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Route definition", HansaSchemaId = "Hansa.RouteDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaRouteDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaRouteDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (
		DisplayName = "Route mode", ToolTip = "Sea or land network used by every connection.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Enum"))
	EHansaAuthoredRouteMode Mode = EHansaAuthoredRouteMode::Sea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (
		DisplayName = "Reachable connections", ToolTip = "Bidirectional city pairs and deterministic travel times available to this route family.",
		HansaRequired = "true", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "RouteConnections"))
	TArray<FHansaRouteConnectionDefinition> Connections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trade|Advanced", meta = (
		DisplayName = "Cargo-rule schema", ToolTip = "Reserved schema version for future conditional cargo rules; MVP supports unconditional actions only.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "false", HansaAIAccess = "Read",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "ExactOne",
		HansaUnit = "Version", HansaMin = "1", HansaMax = "1"))
	int32 CargoRuleSchemaVersion = 1;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
