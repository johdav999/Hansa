#pragma once

#include "CoreMinimal.h"
#include "Definitions/HansaDefinitionBase.h"

#include "HansaMarketDefinitions.generated.h"

USTRUCT(BlueprintType)
struct HANSA_API FHansaMarketGoodProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Good ID", ToolTip = "Stable Good.* identity traded by this city market.", HansaRequired = "true",
		HansaReference = "Good", HansaBulkEditable = "false", HansaAIAccess = "Generate", HansaMigration = "RequiresMigration",
		HansaSerialization = "Included", HansaValidation = "StableReference"))
	FString GoodId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Desired reserve", ToolTip = "Target available stock for scarcity evaluation in milli-units.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "MilliUnit", HansaMin = "0", HansaMax = "9223372036854775807"))
	int64 DesiredReserveMilliUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Confirmed incoming supply", ToolTip = "Confirmed background or route supply expected during one market update window.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "NonNegative",
		HansaUnit = "MilliUnitPerUpdate", HansaMin = "0", HansaMax = "9223372036854775807"))
	int64 ConfirmedIncomingSupplyMilliUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Season modifier", ToolTip = "Bounded seasonal contribution to the target price multiplier.", ClampMin = "-5000", ClampMax = "5000",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "-5000", HansaMax = "5000"))
	int32 SeasonModifierBasisPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "City modifier", ToolTip = "Bounded city-specific contribution to the target price multiplier.", ClampMin = "-5000", ClampMax = "5000",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "-5000", HansaMax = "5000"))
	int32 CityModifierBasisPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Minimum price", ToolTip = "Inclusive lower price bound in milli-Marks.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "MilliMark", HansaMin = "1", HansaMax = "1000000000000000"))
	int64 MinimumPriceMilliMarks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Maximum price", ToolTip = "Inclusive upper price bound in milli-Marks.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "PriceBounds",
		HansaUnit = "MilliMark", HansaMin = "1", HansaMax = "1000000000000000"))
	int64 MaximumPriceMilliMarks = 1000000000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market", meta = (
		DisplayName = "Initial price", ToolTip = "Starting price inside the authored inclusive price bounds.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "PriceBounds",
		HansaUnit = "MilliMark", HansaMin = "1", HansaMax = "1000000000000000"))
	int64 InitialPriceMilliMarks = 1000;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "City market profile definition",
	HansaSchemaId = "Hansa.CityMarketProfileDefinition",
	HansaSchemaVersion = "1"))
class HANSA_API UHansaCityMarketProfileDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaCityMarketProfileDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market|Cadence", meta = (
		DisplayName = "Update cadence", ToolTip = "Simulation ticks between deterministic price reports.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Positive",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "2147483647"))
	int32 UpdateCadenceTicks = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market|History", meta = (
		DisplayName = "History capacity", ToolTip = "Maximum authoritative price reports retained per good.", ClampMin = "1", ClampMax = "4096",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "Report", HansaMin = "1", HansaMax = "4096"))
	int32 PriceHistoryCapacity = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market|Price", meta = (
		DisplayName = "Target smoothing", ToolTip = "Fraction of target-price difference applied before the movement cap.", ClampMin = "1", ClampMax = "10000",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPoint", HansaMin = "1", HansaMax = "10000"))
	int32 TargetSmoothingBasisPoints = 2500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market|Price", meta = (
		DisplayName = "Maximum movement", ToolTip = "Maximum price movement per market update as a fraction of current price.", ClampMin = "1", ClampMax = "10000",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range",
		HansaUnit = "BasisPointPerUpdate", HansaMin = "1", HansaMax = "10000"))
	int32 MaximumMovementBasisPointsPerUpdate = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market|Reporting", meta = (
		DisplayName = "Stale after", ToolTip = "Report age in ticks after which readers must show stale state; cannot be shorter than cadence.", ClampMin = "1",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "MarketStaleness",
		HansaUnit = "SimulationTick", HansaMin = "1", HansaMax = "2147483647"))
	int32 StaleAfterTicks = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market|Goods", meta = (
		DisplayName = "Goods", ToolTip = "Per-good reserve, incoming supply, modifiers and price bounds for this city.",
		HansaRequired = "true", HansaReference = "Good", HansaBulkEditable = "false", HansaAIAccess = "Generate",
		HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "MarketGoods"))
	TArray<FHansaMarketGoodProfile> Goods;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
