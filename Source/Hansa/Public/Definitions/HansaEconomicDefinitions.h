#pragma once

#include "CoreMinimal.h"
#include "Definitions/HansaDefinitionBase.h"

#include "HansaEconomicDefinitions.generated.h"

class UStaticMesh;
class AActor;
class UTexture2D;
class UHansaResidentialCompoundDefinition;

UENUM(BlueprintType)
enum class EHansaConstructionMenuCategory : uint8
{
	Roads,
	Residences,
	Production,
	Storage,
	Harbor,
	Civic,
	Decoration
};

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Good|Storage", meta = (DisplayName = "Enable spoilage", ToolTip = "Apply the authored rate in physical storage and cargo. Legacy goods remain disabled until explicitly balanced.", HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true", HansaAIAccess = "Suggest", HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Boolean"))
    bool bSpoilageEnabled = false;

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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Classification", meta = (DisplayName = "Internal catch recipe", ToolTip = "Optional source recipe caught internally in this batch, without buying its output. Packaging inputs remain normal inputs. The source and processing modes must share the same shoreline fishery.", HansaRequired = "false", HansaReference = "Recipe", HansaBulkEditable = "false", HansaAIAccess = "Suggest", HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "InternalCatch"))
    FString InternalCatchRecipeId;


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
		ToolTip = "Marks a recipe whose inputs come from a natural source rather than another good. Recipe.FellTimber requires uncovered standing trees within 12 four-metre cells of its building footprint in spatial scenarios.",
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

UENUM(BlueprintType)
enum class EHansaConstructionTier : uint8
{
    Legacy,
    DayLaborers,
    Craftsmen,
    Merchants
};

UCLASS(BlueprintType, meta = (
	DisplayName = "Building definition",
	HansaSchemaId = "Hansa.BuildingDefinition",
	HansaSchemaVersion = "5"))
class HANSA_API UHansaBuildingDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()

public:
	UHansaBuildingDefinition();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
        DisplayName = "Construction tier", ToolTip = "Owns this construction card independently of workforce and household demand. Legacy preserves existing catalogs; new production buildings should select one explicit tier.",
        HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
        HansaAIAccess = "Suggest", HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Enum"))
    EHansaConstructionTier ConstructionTier = EHansaConstructionTier::Legacy;


 // Optional extension: empty preserves the schema-v5 content hash and existing save footprints.
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building|Compound", meta=(DisplayName="Residential compound", ToolTip="Optional compound definition. Footprint and population tier must match; larger parcels require an explicit migration.", HansaRequired="false", HansaReference="Compound", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="CompoundFootprint"))
 TSoftObjectPtr<UHansaResidentialCompoundDefinition> ResidentialCompound;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building|Compound", meta=(DisplayName="Compound stage", ToolTip="Visual development stage; child structures never add population.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="Stage", HansaMin="1", HansaMax="3"))
 int32 CompoundStage = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building|Compound", meta=(DisplayName="Compound district", ToolTip="Optional authored district eligibility key. Empty selects unrestricted layouts.", HansaRequired="false", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Optional"))
 FString CompoundDistrictId;
 UHansaResidentialCompoundDefinition* LoadResidentialCompound() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Presentation", meta = (
		DisplayName = "Presentation actor",
		ToolTip = "Optional promoted Actor Blueprint, used instead of the mesh while preserving the building identity and placement footprint. Empty preserves existing mesh-only content.",
		HansaRequired = "false", HansaReference = "ActorClass", HansaBulkEditable = "false",
		HansaAIAccess = "Never", HansaMigration = "Compatible", HansaSerialization = "Included",
		HansaValidation = "OptionalAsset"))
	TSoftClassPtr<AActor> PresentationActorClass;

	UClass* LoadPresentationActorClass() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Show in construction menu",
		ToolTip = "Includes this stable building definition in the player construction catalog.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "Boolean"))
	bool bShowInConstructionMenu = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Construction category",
		ToolTip = "Player-facing construction category. This controls grouping, never gameplay identity.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "Enum"))
	EHansaConstructionMenuCategory ConstructionMenuCategory = EHansaConstructionMenuCategory::Production;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Construction menu order",
		ToolTip = "Stable ascending order within the category or production chain.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "NonNegative", HansaUnit = "Ordinal", HansaMin = "0", HansaMax = "2147483647"))
	int32 ConstructionMenuOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Production chain output",
		ToolTip = "Final Good.* output whose chain selector exposes this production building; empty outside Production.",
		HansaRequired = "false", HansaReference = "Good", HansaBulkEditable = "false",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "ConstructionChain"))
	FString ConstructionChainOutputGoodId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Production chain stage",
		ToolTip = "One-based position in the selected production chain.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "ConstructionChain", HansaUnit = "Ordinal", HansaMin = "0", HansaMax = "32"))
	int32 ConstructionChainStage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Production chain stage count",
		ToolTip = "Expected member count for completeness validation; zero outside a production chain.", ClampMin = "0",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "ConstructionChain", HansaUnit = "Count", HansaMin = "0", HansaMax = "32"))
	int32 ConstructionChainStageCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Required technology",
		ToolTip = "Optional Technology.* that must be completed before direct construction is available.",
		HansaRequired = "false", HansaReference = "Technology", HansaBulkEditable = "false",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "OptionalStableReference"))
	FString RequiredConstructionTechnologyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Upgrade only",
		ToolTip = "Shows the card and its causal lock reason but requires upgrade from another building rather than direct placement.",
		HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Generate", HansaMigration = "RequiresMigration", HansaSerialization = "Included",
		HansaValidation = "Boolean"))
	bool bUpgradeOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction Menu", meta = (
		DisplayName = "Presentation purpose",
		ToolTip = "Localized purpose text for buildings without recipes; recipe flows are always derived from Recipe definitions.",
		HansaRequired = "false", HansaReference = "None", HansaBulkEditable = "true",
		HansaAIAccess = "Suggest", HansaMigration = "Compatible", HansaSerialization = "Included",
		HansaValidation = "Optional"))
	FText ConstructionPresentationPurpose;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Construction", meta = (
		DisplayName = "Construction costs",
		ToolTip = "Positive goods consumed when the building is constructed.",
		HansaRequired = "false",
		HansaReference = "Good",
		HansaBulkEditable = "false",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
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
		ToolTip = "Optional same-footprint Building.* target: next population tier, or consecutive development stage within the same compound, district and population tier.",
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
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "Boolean"))
	bool bRequiresRoad = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Logistics", meta = (
		DisplayName = "Provides market access",
		ToolTip = "Marks this completed building as a physical local-market hub. Its bound city inventory and adjacent connected roads become eligible endpoints for local deliveries and citizen access.",
		HansaRequired = "true",
		HansaReference = "None",
		HansaBulkEditable = "true",
		HansaAIAccess = "Generate",
		HansaMigration = "RequiresMigration",
		HansaSerialization = "Included",
		HansaValidation = "MarketAccessProvider"))
	bool bProvidesMarketAccess = false;

 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Logistics", meta = (
  DisplayName = "Maximum market road distance", ToolTip = "Maximum shortest completed-road route from a building to this market, including both entrance steps. One cell is 4 metres. Inclusive limit; default 40 cells (160 metres). Only market-access providers use this value.",
  ClampMin = "2", ClampMax = "4096", HansaUnit = "RoadCells", HansaMin = "2", HansaMax = "4096",
  HansaRequired = "true", HansaReference = "None", HansaBulkEditable = "true",
  HansaAIAccess = "Generate", HansaMigration = "Compatible", HansaSerialization = "Included", HansaValidation = "Range"))
 int32 MaximumMarketRoadDistanceCells = 40;

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


	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;

protected:
	virtual bool UsesLegacyBuildingMeshHash() const override { return true; }
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};
