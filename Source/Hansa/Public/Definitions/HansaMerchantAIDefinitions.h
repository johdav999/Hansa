#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "HansaMerchantAIDefinitions.generated.h"

USTRUCT(BlueprintType)
struct HANSA_API FHansaMerchantAITradePlanDefinition final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Stable plan ID", ToolTip = "Stable diagnostic identity for this bounded opportunity.",
		HansaRequired = "true", HansaReference = "AIPlan", HansaBulkEditable = "false", HansaAIAccess = "Never",
		HansaMigration = "Identity", HansaSerialization = "Included", HansaValidation = "StableId"))
	FString StablePlanId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Route definition", ToolTip = "Allowed route network definition used by this opportunity.",
		HansaRequired = "true", HansaReference = "Route", HansaBulkEditable = "false", HansaAIAccess = "Read",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString RouteDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Vehicle definition", ToolTip = "Owned vehicle type eligible for this opportunity.",
		HansaRequired = "true", HansaReference = "Vehicle", HansaBulkEditable = "false", HansaAIAccess = "Read",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString VehicleDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Source city", ToolTip = "City whose allowed report supplies goods.",
		HansaRequired = "true", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString SourceCityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Destination city", ToolTip = "City whose allowed report supplies demand information.",
		HansaRequired = "true", HansaReference = "City", HansaBulkEditable = "false", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString DestinationCityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Good", ToolTip = "Good evaluated and transferred by this opportunity.",
		HansaRequired = "true", HansaReference = "Good", HansaBulkEditable = "false", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString GoodId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Quantity limit", ToolTip = "Maximum transfer quantity in milli-units.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "MilliUnit", HansaMin = "1", HansaMax = "1000000000"))
	int64 QuantityLimitMilliUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Minimum source reserve", ToolTip = "Stock protected at the source in milli-units.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "MilliUnit", HansaMin = "0", HansaMax = "1000000000"))
	int64 MinimumSourceReserveMilliUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Utility bias", ToolTip = "Signed deterministic utility adjustment for this option.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Integer",
		HansaUnit = "Utility", HansaMin = "-1000000", HansaMax = "1000000"))
	int64 UtilityBias = 0;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Merchant AI tuning definition", HansaSchemaId = "Hansa.MerchantAITuningDefinition", HansaSchemaVersion = "1"))
class HANSA_API UHansaMerchantAITuningDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaMerchantAITuningDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Decision cadence", ToolTip = "Fixed simulation ticks between deterministic evaluations.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "10000"))
	int32 DecisionCadenceTicks = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Decision history capacity", ToolTip = "Bounded number of trace records retained for diagnostics.", ClampMin = "1", ClampMax = "256",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "Record", HansaMin = "1", HansaMax = "256"))
	int32 DecisionHistoryCapacity = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Minimum destination demand gap", ToolTip = "Known demand gap that qualifies as a shortage response.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "MilliUnit", HansaMin = "0", HansaMax = "1000000000"))
	int64 MinimumDestinationDemandGapMilliUnits = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Minimum gross margin", ToolTip = "Known source-to-destination price margin required when no shortage qualifies.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Integer",
		HansaUnit = "MilliMark", HansaMin = "-1000000000", HansaMax = "1000000000"))
	int64 MinimumGrossMarginMilliMarks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Shortage utility per unit", ToolTip = "Utility contribution per whole unit of known demand gap.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "UtilityPerUnit", HansaMin = "0", HansaMax = "1000000"))
	int64 ShortageUtilityPerUnit = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Margin utility", ToolTip = "Utility contribution per milli-mark of known margin.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "UtilityPerMilliMark", HansaMin = "0", HansaMax = "1000000"))
	int64 MarginUtilityPerMilliMark = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Research utility", ToolTip = "Base utility assigned to the first eligible preferred technology.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Integer",
		HansaUnit = "Utility", HansaMin = "-1000000", HansaMax = "1000000"))
	int64 ResearchUtility = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Production utility", ToolTip = "Base utility assigned to an idle owned production process.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Integer",
		HansaUnit = "Utility", HansaMin = "-1000000", HansaMax = "1000000"))
	int64 ProductionUtility = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Protected cash reserve", ToolTip = "Cash the merchant will not commit to discretionary trade, stations, orders or progression.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "Pfennig", HansaMin = "0", HansaMax = "1000000000"))
	int64 ProtectedCashReservePfennig = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Action cooldown", ToolTip = "Minimum ticks after an accepted command before another command may be submitted.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "SimulationTick", HansaMin = "0", HansaMax = "10000"))
	int32 ActionCooldownTicks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Direct trade quantity", ToolTip = "Maximum milli-units submitted by one public spot-trade decision.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "MilliUnit", HansaMin = "1", HansaMax = "1000000000"))
	int64 DirectTradeQuantityMilliUnits = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Station order target", ToolTip = "Default stock target for a lawful acquire order.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "MilliUnit", HansaMin = "1", HansaMax = "1000000000"))
	int64 StationOrderTargetMilliUnits = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Station order cap", ToolTip = "Maximum quantity acquired by the AI order at one market update.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "MilliUnit", HansaMin = "1", HansaMax = "1000000000"))
	int64 StationOrderCapMilliUnits = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Station order budget", ToolTip = "Lifetime purchase budget for one AI-created station order.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "Pfennig", HansaMin = "1", HansaMax = "1000000000"))
	int64 StationOrderBudgetPfennig = 20000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Presence utility", ToolTip = "Base utility for station, order, upgrade and specialization actions.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Integer",
		HansaUnit = "Utility", HansaMin = "-1000000", HansaMax = "1000000"))
	int64 PresenceUtility = 75;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Trade objective legs", ToolTip = "Completed route legs required for the bounded MVP trade objective.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "RouteLeg", HansaMin = "1", HansaMax = "1000000"))
	int64 TargetCompletedTradeLegs = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Preferred research", ToolTip = "Canonical priority list of technologies the merchant may queue.",
		HansaRequired = "false", HansaReference = "Technology", HansaBulkEditable = "false", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "StableReferences"))
	TArray<FString> PreferredResearchTechnologyIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Merchant AI", meta = (
		DisplayName = "Trade plans", ToolTip = "Bounded opportunities evaluated only from allowed market reports.",
		HansaRequired = "true", HansaReference = "AIPlan", HansaBulkEditable = "false", HansaAIAccess = "Suggest",
		HansaMigration = "RequiresMigration", HansaSerialization = "Included", HansaValidation = "MerchantTradePlans"))
	TArray<FHansaMerchantAITradePlanDefinition> TradePlans;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
