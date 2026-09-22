#pragma once

#include "Definitions/HansaDefinitionBase.h"
#include "HansaPresenceDefinitions.generated.h"

USTRUCT(BlueprintType)
struct HANSA_API FHansaPresenceUpgradeGoodCost final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Good", ToolTip="Canonical Good.* material cost.", HansaRequired="true", HansaReference="Good", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="StableReference"))
	FString GoodId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Quantity", ToolTip="Non-negative authored material cost in milli-units.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="MilliUnit", HansaMin="0", HansaMax="9223372036854775807"))
	int64 QuantityMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeStationSiteDefinition final
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Site ID", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalName")) FName SiteId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Plot category", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalName")) FName PlotCategory = TEXT("Commercial");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Storage capacity", ClampMin="1", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="MilliUnit", HansaMin="1", HansaMax="9223372036854775807")) int64 StorageCapacityMilliUnits = 50000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Construction ticks", ClampMin="1", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="Tick", HansaMin="1", HansaMax="1000000")) int32 ConstructionTicks = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Upkeep", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="PfennigPerTick", HansaMin="0", HansaMax="9223372036854775807")) int64 UpkeepPfennigPerTick = 25;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Cancellation refund", ClampMin="0", ClampMax="10000", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="BasisPoints", HansaUnit="BasisPoints", HansaMin="0", HansaMax="10000")) int32 CancellationRefundBasisPoints = 5000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Presentation", HansaRequired="true", HansaReference="PresentationClass", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="ProductionAssetReference")) FSoftClassPath PresentationClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Construction", meta=(HansaMigration="RequiresMigration", HansaValidation="OrderedGridBounds")) FIntPoint LeaseBoundsMin = FIntPoint(0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Construction", meta=(HansaMigration="RequiresMigration", HansaValidation="OrderedGridBounds")) FIntPoint LeaseBoundsMax = FIntPoint(0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Construction", meta=(HansaReference="None", HansaMigration="RequiresMigration", HansaValidation="CanonicalNames")) TArray<FName> PermittedBuildingCategories = { TEXT("Storage"), TEXT("Commercial"), TEXT("Production") };
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaPresenceSpecializationDefinition final
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(HansaRequired="true", HansaValidation="CanonicalName")) FName SpecializationId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(HansaReference="PresenceStage")) FString RequiredStageId = TEXT("PresenceStage.MerchantOffice");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization") FName ExclusiveGroupId = TEXT("MerchantOfficePrimary");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(HansaReference="PresenceCapability")) TArray<FString> GrantedCapabilityIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(ClampMin="0")) int64 InvestmentCostPfennig = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization") TArray<FHansaPresenceUpgradeGoodCost> InvestmentGoods;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization") bool bAllowRespec = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(ClampMin="0", ClampMax="10000")) int32 RespecRefundBasisPoints = 2500;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(ClampMin="0")) int64 StorageCapacityBonusMilliUnits = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(ClampMin="0", ClampMax="64")) int32 AdditionalOrderSlots = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(ClampMin="0")) int64 StationTransferCapBonusMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCityPrivilegeDefinition final
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(HansaRequired="true", HansaValidation="CanonicalName")) FName PrivilegeId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege") FText DisplayName;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(HansaReference="PresenceStage")) FString RequiredStageId = TEXT("PresenceStage.PrivilegedPresence");
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(ClampMin="0")) int64 CostPfennig = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege") TArray<FHansaPresenceUpgradeGoodCost> CostGoods;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(ClampMin="0")) int64 DurationTicks = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege") bool bReversible = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(HansaValidation="OrderedGridBounds")) FIntPoint LeaseBoundsMin = FIntPoint(0, 0);
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(HansaValidation="OrderedGridBounds")) FIntPoint LeaseBoundsMax = FIntPoint(0, 0);
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(HansaValidation="CanonicalNames")) TArray<FName> PermittedBuildingCategories;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCityProjectDefinition final
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(HansaRequired="true", HansaValidation="CanonicalName")) FName ProjectId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project") FText DisplayName;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(HansaReference="PresenceStage")) FString RequiredStageId = TEXT("PresenceStage.PrivilegedPresence");
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(ClampMin="0")) int64 CostPfennig = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project") TArray<FHansaPresenceUpgradeGoodCost> CostGoods;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(ClampMin="1")) int32 ConstructionTicks = 3;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(HansaReference="Good")) FString SharedReserveGoodId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(ClampMin="1")) int64 SharedReserveBonusMilliUnits = 0;
};
UCLASS(BlueprintType, meta=(DisplayName="Presence capability definition", HansaSchemaId="Hansa.PresenceCapabilityDefinition", HansaSchemaVersion="1"))
class HANSA_API UHansaPresenceCapabilityDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()
public:
	UHansaPresenceCapabilityDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Semantics", ToolTip="Explicit causal meaning exposed by diagnostic queries; consumers opt in separately.", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonEmpty"))
	FText Semantics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Capability schema", ToolTip="Version of the typed capability contract.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Read", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="ExactOne", HansaUnit="Version", HansaMin="1", HansaMax="1"))
	int32 CapabilitySchemaVersion = 1;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;
protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

UCLASS(BlueprintType, meta=(DisplayName="Foreign-presence stage definition", HansaSchemaId="Hansa.ForeignPresenceStageDefinition", HansaSchemaVersion="1"))
class HANSA_API UHansaForeignPresenceStageDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()
public:
	UHansaForeignPresenceStageDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Ordinal", ToolTip="Unique stable ladder order.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Ordinal", HansaMin="0", HansaMax="100"))
	int32 Ordinal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Prerequisite stages", ToolTip="All prerequisite PresenceStage.* identities; cycles are rejected.", HansaRequired="false", HansaReference="PresenceStage", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="AcyclicStableReferences"))
	TArray<FString> PrerequisiteStageIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Granted capabilities", ToolTip="Typed capabilities granted by this stage, subject to city policy.", HansaRequired="false", HansaReference="PresenceCapability", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="StableReferences"))
	TArray<FString> GrantedCapabilityIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Permitted plot categories", ToolTip="Stable category keys for later leased-plot consumers.", HansaRequired="false", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalNames"))
	TArray<FName> PermittedPlotCategories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Permitted building categories", ToolTip="Stable construction categories for later permission consumers.", HansaRequired="false", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalNames"))
	TArray<FName> PermittedBuildingCategories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Upgrade currency cost", ToolTip="Future upgrade cost; TR-02 queries but never spends it.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Pfennig", HansaMin="0", HansaMax="9223372036854775807"))
	int64 UpgradeCostPfennig = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Upgrade goods", ToolTip="Future transactional material costs.", HansaRequired="false", HansaReference="Good", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegativeUniqueGoods"))
	TArray<FHansaPresenceUpgradeGoodCost> UpgradeGoods;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required lawful trade", ToolTip="Cumulative lawful trade volume needed for availability.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="MilliUnit", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredLawfulTradeVolumeMilliUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required deliveries", ToolTip="Completed delivery count needed for availability.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Count", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredCompletedDeliveries = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required investment", ToolTip="Cumulative qualifying investment needed for availability.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Pfennig", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredInvestedPfennig = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required transaction value", ToolTip="Cumulative absolute value of accepted lawful market settlements.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Pfennig", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredTransactionValuePfennig = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required shortage relief", ToolTip="Delivered volume that entered a market below its authored reserve.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="MilliUnit", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredFulfilledShortageMilliUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required reliable operation", ToolTip="Ticks with an active station; cumulative, capped at one contribution per simulation tick.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Tick", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredReliableOperatingTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Requirements", meta=(DisplayName="Required solvent operation", ToolTip="Ticks with an active station and enough cash for its stated upkeep.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Tick", HansaMin="0", HansaMax="9223372036854775807"))
	int64 RequiredSolventOperatingTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Upgrade", meta=(DisplayName="Upgrade construction ticks", ToolTip="Deterministic construction duration after atomic funding.", ClampMin="1", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="Tick", HansaMin="1", HansaMax="1000000"))
	int32 UpgradeConstructionTicks = 3;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;
protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

UCLASS(BlueprintType, meta=(DisplayName="City trade policy definition", HansaSchemaId="Hansa.CityTradePolicyDefinition", HansaSchemaVersion="1"))
class HANSA_API UHansaCityTradePolicyDefinition final : public UHansaDefinitionBase
{
	GENERATED_BODY()
public:
	UHansaCityTradePolicyDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="City", ToolTip="Canonical City.* governed by this unique policy.", HansaRequired="true", HansaReference="City", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="StableReference"))
	FString CityId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Initial foreign stage", ToolTip="Optional authored opening PresenceStage.* for each house.", HansaRequired="false", HansaReference="PresenceStage", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="OptionalStableReference"))
	FString InitialStageId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Allowed stages", ToolTip="Presence stages this city permits.", HansaRequired="true", HansaReference="PresenceStage", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="StableReferences"))
	TArray<FString> AllowedStageIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Denied capabilities", ToolTip="Capabilities unavailable despite stage grants.", HansaRequired="false", HansaReference="PresenceCapability", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="StableReferences"))
	TArray<FString> DeniedCapabilityIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Allowed plot categories", ToolTip="Plot categories this city may lease later.", HansaRequired="false", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalNames"))
	TArray<FName> AllowedPlotCategories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Allowed building categories", ToolTip="Building categories this city may permit later.", HansaRequired="false", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalNames"))
	TArray<FName> AllowedBuildingCategories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Station", meta=(DisplayName="Trade station sites", ToolTip="Authored eligible station sites, capacity, construction, upkeep, refund, and approved presentation.", HansaRequired="false", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="UniqueStationSites"))
	TArray<FHansaTradeStationSiteDefinition> TradeStationSites;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Specialization", meta=(DisplayName="Merchant-office specializations", ToolTip="Validated exclusive specialization graph with prerequisites, typed capabilities, investment, respec policy, and bounded station effects.", HansaRequired="true", HansaReference="PresenceSpecialization", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="ExclusiveBranchGraph"))
	TArray<FHansaPresenceSpecializationDefinition> Specializations;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Privilege", meta=(DisplayName="City privileges", ToolTip="Scoped city-issued privileges with real costs, duration, reversibility, and bounded effects.", HansaRequired="false", HansaReference="CityPrivilege", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="UniquePrivilegeDefinitions")) TArray<FHansaCityPrivilegeDefinition> Privileges;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Project", meta=(DisplayName="City projects", ToolTip="Player-funded shared city projects with resource, duration, and authoritative economic effects.", HansaRequired="false", HansaReference="CityProject", HansaBulkEditable="false", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="UniqueCityProjectDefinitions")) TArray<FHansaCityProjectDefinition> CityProjects;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Governance", meta=(DisplayName="Governance scenarios", ToolTip="Scenarios that may invoke this exceptional governance transition; ordinary commerce never satisfies this gate.", HansaRequired="false", HansaReference="Scenario", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="StableReferences")) TArray<FString> GovernanceScenarioIds;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Governance", meta=(DisplayName="Governance charter", ToolTip="Exact authored founding charter required for exceptional authority; empty means governance cannot transfer.", HansaRequired="false", HansaReference="GovernanceCharter", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="CanonicalName")) FName GovernanceCharterId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Orders", meta=(DisplayName="MaximumStationOrders", ToolTip="Bounded station-order policy; changing this limit requires campaign migration review.", ClampMin="1", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="Count", HansaMin="1", HansaMax="64")) int32 MaximumStationOrders = 8;

 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Orders", meta=(DisplayName="MaximumOrderCapMilliUnits", ToolTip="Bounded station-order policy; changing this limit requires campaign migration review.", ClampMin="1", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="MilliUnit", HansaMin="1", HansaMax="1000000000")) int64 MaximumOrderCapMilliUnits = 50000;

 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Orders", meta=(DisplayName="MaximumOrderBudgetPfennig", ToolTip="Bounded station-order policy; changing this limit requires campaign migration review.", ClampMin="1", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="Pfennig", HansaMin="1", HansaMax="1000000000000")) int64 MaximumOrderBudgetPfennig = 1000000;

 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Office", meta=(DisplayName="Merchant office storage bonus", ToolTip="Additional station capacity granted only while MerchantOffice capability is active.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="MilliUnit", HansaMin="0", HansaMax="1000000000")) int64 MerchantOfficeStorageBonusMilliUnits = 50000;

 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence|Office", meta=(DisplayName="Merchant office order slots", ToolTip="Additional live station-order slots granted only while MerchantOffice capability is active.", ClampMin="0", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="NonNegative", HansaUnit="Count", HansaMin="0", HansaMax="64")) int32 MerchantOfficeAdditionalOrderSlots = 4;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Public market access", ToolTip="Policy fact only; TR-02 does not consume it as a transaction permission.", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Boolean"))
	bool bPublicMarketAccess = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presence", meta=(DisplayName="Exceptional governance allowed", ToolTip="Whether a scenario may later grant final charter/governance.", HansaRequired="true", HansaReference="None", HansaBulkEditable="true", HansaAIAccess="Suggest", HansaMigration="RequiresMigration", HansaSerialization="Included", HansaValidation="Boolean"))
	bool bExceptionalGovernanceAllowed = false;

	virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;
protected:
	virtual void AppendDefinitionHashData(FString& InOutCanonicalData) const override;
};

