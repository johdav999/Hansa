#pragma once

#include "CoreMinimal.h"
#include "Definitions/HansaDefinitionBase.h"

#include "HansaEconomicDefinitions.generated.h"

class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EHansaGoodUnit : uint8
{
	Kilogram,
	Item,
	Litre
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaGoodAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Amount", meta = (
		DisplayName = "Good ID",
		ToolTip = "Stable Good.* identity referenced by this amount.",
		HansaRequired = "true",
		HansaReference = "Good",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "StableReference"))
	FString GoodId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Amount", meta = (
		DisplayName = "Quantity",
		ToolTip = "Positive amount in one-thousandth of the referenced good's declared unit.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Positive",
		HansaUnit = "MilliUnit",
		HansaMin = "1",
		HansaMax = "9223372036854775807"))
	int64 QuantityMilliUnits = 1000;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Good definition",
	HansaSchemaId = "Hansa.GoodDefinition",
	HansaSchemaVersion = "1"))
class HANSA_API UHansaGoodDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaGoodDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good|Market", meta = (
		DisplayName = "Quantity unit",
		ToolTip = "Player-facing physical unit used by every quantity of this good.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "Enum"))
	EHansaGoodUnit QuantityUnit = EHansaGoodUnit::Kilogram;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good|Market", meta = (
		DisplayName = "Base value",
		ToolTip = "Nominal market value in one-thousandth of a Mark per declared unit.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Positive",
		HansaUnit = "MilliMark",
		HansaMin = "1",
		HansaMax = "9223372036854775807"))
	int64 BaseValueMilliMarks = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good|Market", meta = (
		DisplayName = "Price elasticity",
		ToolTip = "Relative price response to stock pressure in basis points.",
		ClampMin = "0",
		ClampMax = "50000",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Range",
		HansaUnit = "BasisPoint",
		HansaMin = "0",
		HansaMax = "50000"))
	int32 PriceElasticityBasisPoints = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good|Storage", meta = (
		DisplayName = "Spoilage per day",
		ToolTip = "Daily spoilage fraction in basis points; zero disables spoilage for the MVP.",
		ClampMin = "0",
		ClampMax = "10000",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Range",
		HansaUnit = "BasisPointPerDay",
		HansaMin = "0",
		HansaMax = "10000"))
	int32 SpoilageBasisPointsPerDay = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good|Presentation", meta = (
		DisplayName = "Icon",
		ToolTip = "Optional promoted production icon; identity never depends on this asset path.",
		HansaRequired = "false",
		HansaReference = "Texture2D",
		HansaBulkEditable = "false",
		HansaAIAccess = "Never",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "OptionalAsset"))
	TSoftObjectPtr<UTexture2D> Icon;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Recipe definition",
	HansaSchemaId = "Hansa.RecipeDefinition",
	HansaSchemaVersion = "1"))
class HANSA_API UHansaRecipeDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaRecipeDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Flow", meta = (
		DisplayName = "Inputs",
		ToolTip = "Goods consumed once per completed cycle; source recipes may leave this empty.",
		HansaRequired = "false",
		HansaReference = "Good",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "GoodAmounts"))
	TArray<FHansaGoodAmount> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Flow", meta = (
		DisplayName = "Outputs",
		ToolTip = "Goods produced once per completed cycle; at least one positive output is required unless this is a declared sink.",
		HansaRequired = "true",
		HansaReference = "Good",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "PositiveGoodAmounts"))
	TArray<FHansaGoodAmount> Outputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Timing", meta = (
		DisplayName = "Cycle time",
		ToolTip = "Deterministic simulation ticks required to complete one production cycle.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Positive",
		HansaUnit = "SimulationTick",
		HansaMin = "1",
		HansaMax = "2147483647"))
	int32 CycleTicks = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Workforce", meta = (
		DisplayName = "Laborer workforce",
		ToolTip = "Placeholder laborer workforce required for nominal throughput.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "PopulationTier.Laborer",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "Worker",
		HansaMin = "0",
		HansaMax = "2147483647"))
	int32 LaborerWorkforce = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Workforce", meta = (
		DisplayName = "Artisan workforce",
		ToolTip = "Placeholder artisan workforce required for nominal throughput.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "PopulationTier.Artisan",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "Worker",
		HansaMin = "0",
		HansaMax = "2147483647"))
	int32 ArtisanWorkforce = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Classification", meta = (
		DisplayName = "Declared source",
		ToolTip = "Marks a recipe whose inputs come from an abstract natural source rather than another good.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "SourceContract"))
	bool bDeclaredSource = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Classification", meta = (
		DisplayName = "Declared sink",
		ToolTip = "Marks a recipe that deliberately consumes goods without producing a stored good.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "SinkContract"))
	bool bDeclaredSink = false;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Building definition",
	HansaSchemaId = "Hansa.BuildingDefinition",
	HansaSchemaVersion = "2"))
class HANSA_API UHansaBuildingDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaBuildingDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction", meta = (
		DisplayName = "Construction costs",
		ToolTip = "Positive goods consumed when the building is constructed.",
		HansaRequired = "false",
		HansaReference = "Good",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "GoodAmounts"))
	TArray<FHansaGoodAmount> ConstructionCosts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction", meta = (
		DisplayName = "Currency cost",
		ToolTip = "Non-negative currency charged atomically with the construction resources.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "Pfennig",
		HansaMin = "0",
		HansaMax = "9223372036854775807"))
	int64 ConstructionCostPfennig = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction", meta = (
		DisplayName = "Cancellation refund",
		ToolTip = "Share of paid currency and resources refunded when unfinished construction is cancelled.",
		ClampMin = "0",
		ClampMax = "10000",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Range",
		HansaUnit = "BasisPoint",
		HansaMin = "0",
		HansaMax = "10000"))
	int32 CancellationRefundBasisPoints = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Production", meta = (
		DisplayName = "Recipes",
		ToolTip = "Stable Recipe.* identities this building can execute.",
		HansaRequired = "false",
		HansaReference = "Recipe",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "StableReferences"))
	TArray<FString> RecipeIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Upgrade", meta = (
		DisplayName = "Upgrade target",
		ToolTip = "Optional stable Building.* identity reached by upgrading this building.",
		HansaRequired = "false",
		HansaReference = "Building",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "OptionalStableReference"))
	FString UpgradeTargetBuildingId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Footprint", meta = (
		DisplayName = "Footprint width",
		ToolTip = "Positive east-west footprint size in placement grid cells.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "Positive",
		HansaUnit = "GridCell",
		HansaMin = "1",
		HansaMax = "64"))
	int32 FootprintWidthCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Footprint", meta = (
		DisplayName = "Footprint height",
		ToolTip = "Positive north-south footprint size in placement grid cells.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "Positive",
		HansaUnit = "GridCell",
		HansaMin = "1",
		HansaMax = "64"))
	int32 FootprintHeightCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction", meta = (
		DisplayName = "Build time",
		ToolTip = "Deterministic simulation ticks required to construct the building.",
		ClampMin = "1",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Positive",
		HansaUnit = "SimulationTick",
		HansaMin = "1",
		HansaMax = "2147483647"))
	int32 BuildTicks = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Capacity", meta = (
		DisplayName = "Storage capacity",
		ToolTip = "Aggregate placeholder inventory capacity in milli-units.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "MilliUnit",
		HansaMin = "0",
		HansaMax = "2147483647"))
	int32 StorageCapacityMilliUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Capacity", meta = (
		DisplayName = "Residence capacity",
		ToolTip = "Placeholder resident capacity; zero means the building is not a residence.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "Resident",
		HansaMin = "0",
		HansaMax = "2147483647"))
	int32 ResidenceCapacity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Capacity", meta = (
		DisplayName = "Resident population tier",
		ToolTip = "PopulationTier.* hosted by this residence. Required when residence capacity is positive and empty for non-residences.",
		HansaRequired = "false",
		HansaReference = "PopulationTier",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "ResidenceTierContract"))
	FString ResidentPopulationTierId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Workforce", meta = (
		DisplayName = "Laborer workforce",
		ToolTip = "Placeholder laborer workforce required to operate at nominal capacity.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "PopulationTier.Laborer",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "Worker",
		HansaMin = "0",
		HansaMax = "2147483647"))
	int32 LaborerWorkforce = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Workforce", meta = (
		DisplayName = "Artisan workforce",
		ToolTip = "Placeholder artisan workforce required to operate at nominal capacity.",
		ClampMin = "0",
		HansaRequired = "true",
		HansaReference = "PopulationTier.Artisan",
		HansaBulkEditable = "true",
		HansaAIAccess = "Suggest",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "NonNegative",
		HansaUnit = "Worker",
		HansaMin = "0",
		HansaMax = "2147483647"))
	int32 ArtisanWorkforce = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Placement", meta = (
		DisplayName = "Requires road",
		ToolTip = "Whether normal operation requires adjacency to a connected road.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Boolean"))
	bool bRequiresRoad = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Placement", meta = (
		DisplayName = "Requires shoreline",
		ToolTip = "Whether placement requires a shoreline or harbor edge.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "Boolean"))
	bool bRequiresShoreline = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Presentation", meta = (
		DisplayName = "Presentation mesh",
		ToolTip = "Promoted static mesh used for initial presentation; stable identity never depends on this path.",
		HansaRequired = "true",
		HansaReference = "StaticMesh",
		HansaBulkEditable = "false",
		HansaAIAccess = "Never",
		HansaMigration = "Compatible",
		HansaSerialization = "Included",
		HansaValidation = "RequiredAsset"))
	TSoftObjectPtr<UStaticMesh> PresentationMesh;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
