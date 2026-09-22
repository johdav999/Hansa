#pragma once
#include "CoreMinimal.h"
#include "Definitions/HansaDefinitionBase.h"
#include "HansaResidentialCompoundDefinition.generated.h"

class UStaticMesh;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct HANSA_API FHansaCompoundVariant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="VariantId", ToolTip="Stable variant key, independent of array order.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FString VariantId = TEXT("Default");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Mesh", ToolTip="Promoted mesh at its authored real scale.", HansaRequired="true", HansaReference="StaticMesh", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="PromotedAsset"))
	TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Materials", ToolTip="Optional slot-ordered promoted material overrides.", HansaRequired="true", HansaReference="MaterialInterface", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="PromotedAssets"))
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="LocalPosition", ToolTip="Position in centimetres relative to parcel centre.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector LocalPosition = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="LocalYaw", ToolTip="Explicit bounded yaw; principal stays within 25 degrees of +X.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="Degree", HansaMin="-180", HansaMax="180"))
	float LocalYaw = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Scale", ToolTip="Architecture requires unit scale. Surface ground sheets alone allow positive XY dimensions, with Z scale one and at most 2cm thickness.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="BoundsMin", ToolTip="Conservative mesh-local envelope in centimetres, including overhangs.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector BoundsMin = FVector(-100,-100,0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="BoundsMax", ToolTip="Conservative mesh-local envelope in centimetres.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector BoundsMax = FVector(100,100,300);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Weight", ToolTip="Positive deterministic variant weight.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="Weight", HansaMin="1", HansaMax="10000"))
	int32 Weight = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Use collision for access", ToolTip="Open passage only: use authored convex collision below 2.1m for pedestrian clearance, retaining full geometry bounds for overlap.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Never", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="ConvexAccess"))
 bool bUseSimpleCollisionForAccess = false;

};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCompoundSlot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="SlotId", ToolTip="Stable slot key retained between stages.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FString SlotId = TEXT("Principal");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Group", ToolTip="Dwelling, Workshop, Fence, Vegetation, YardProp or Surface. Thin fence end posts may join within a 25cm envelope; crossings/stacking remain invalid. Surface is a walkable dirt-coverage intent, fitted to terrain at runtime with feathered edges. Dwelling/Workshop get level foundations; no group creates gameplay entities.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FName Group = TEXT("Dwelling");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="bRequired", ToolTip="Missing required art falls back for the complete parcel; optional art may be omitted.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	bool bRequired = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="bPrincipal", ToolTip="Exactly one required principal dwelling faces local +X, within twenty-five degrees.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	bool bPrincipal = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="PresenceBasisPoints", ToolTip="Deterministic optional slot inclusion; required slots must be 10000.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="BasisPoint", HansaMin="0", HansaMax="10000"))
	int32 PresenceBasisPoints = 10000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="EntranceNodeId", ToolTip="Required for dwellings and workshops; connects the slot to the checked access network.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FString EntranceNodeId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Variants", ToolTip="Weighted explicit alternatives may include reviewed bounded yaw offsets; no unconstrained jitter or architectural scaling.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	TArray<FHansaCompoundVariant> Variants;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCompoundNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="NodeId", ToolTip="Stable access or activity node key.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FString NodeId = TEXT("Entrance");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Purpose", ToolTip="Entrance, YardWork, Rest or Delivery.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FName Purpose = TEXT("Entrance");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Position", ToolTip="Parcel-local centimetres, on the ground plane.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector Position = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="bRoadEntrance", ToolTip="Road entrance must meet the local +X parcel edge.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	bool bRoadEntrance = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Links", ToolTip="Undirected walkable links; all activity nodes must reach the road entrance.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	TArray<FString> Links;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCompoundLayout
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="LayoutId", ToolTip="Stable layout key.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FString LayoutId = TEXT("Straight");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="DevelopmentStage", ToolTip="Visual stage chosen by owning building definition, never by child population.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="Stage", HansaMin="1", HansaMax="3"))
	int32 DevelopmentStage = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Context", ToolTip="Straight, CornerLeft, CornerRight or Edge; evaluated against adjacent roads and map edge.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FName Context = TEXT("Straight");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="DistrictIds", ToolTip="Empty allows any district; otherwise exact author-supplied district IDs.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	TArray<FString> DistrictIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Weight", ToolTip="Positive deterministic layout weight.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Positive", HansaUnit="Weight", HansaMin="1", HansaMax="10000"))
	int32 Weight = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Slots", ToolTip="Named structural, boundary and yard slots.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	TArray<FHansaCompoundSlot> Slots;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Nodes", ToolTip="Connected, clearance-validated entrance and activity graph.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	TArray<FHansaCompoundNode> Nodes;
};

struct HANSA_API FHansaCompoundInstance
{
 FString SlotId;
 FName Group;
 TSoftObjectPtr<UStaticMesh> Mesh;
 TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
 FTransform Transform;
 FBox AuthoredBounds = FBox(ForceInit);
 bool bRequired = true;
};
struct HANSA_API FHansaCompoundComposition
{
 FString LayoutId;
 TArray<FHansaCompoundInstance> Instances;
 TArray<FHansaCompoundNode> Nodes;
 TArray<FHansaDefinitionValidationIssue> Issues;
 bool IsValid() const { return !LayoutId.IsEmpty() && Issues.IsEmpty(); }
};
UCLASS(BlueprintType, meta=(DisplayName="Residential compound", HansaSchemaId="Hansa.ResidentialCompoundDefinition", HansaSchemaVersion="1"))
class HANSA_API UHansaResidentialCompoundDefinition final : public UHansaDefinitionBase
{
 GENERATED_BODY()
public:
 UHansaResidentialCompoundDefinition();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="FootprintWidthCells", ToolTip="Logical unrotated X footprint in four-metre grid cells; default 16 metres.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="GridCell", HansaMin="1", HansaMax="64"))
	int32 FootprintWidthCells = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="FootprintHeightCells", ToolTip="Logical unrotated Y footprint in four-metre grid cells.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="GridCell", HansaMin="1", HansaMax="64"))
	int32 FootprintHeightCells = 4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="AllowedRoadFrontMask", ToolTip="Allowed authoritative quarter turns: bit 0 +X, bit 1 +Y, bit 2 -X, bit 3 -Y.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="BitMask", HansaMin="1", HansaMax="15"))
	int32 AllowedRoadFrontMask = 15;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="PopulationTierId", ToolTip="Laborer or Artisan household tier; must match the owning residence.", HansaRequired="true", HansaReference="PopulationTier", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="StableReference"))
	FString PopulationTierId = TEXT("PopulationTier.Laborer");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="ClearanceRadius", ToolTip="Minimum pedestrian radius in centimetres, checked around every node and link.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Range", HansaUnit="Centimetre", HansaMin="40", HansaMax="200"))
	float ClearanceRadius = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="BoundsMin", ToolTip="Simplified complete compound envelope, centred on the logical parcel.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector BoundsMin = FVector(-800,-800,0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="BoundsMax", ToolTip="Simplified complete compound envelope.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Generate", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	FVector BoundsMax = FVector(800,800,900);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound", meta=(DisplayName="Layouts", ToolTip="Weighted authored alternatives selected by persistent parcel identity. Equal weights give equal chances; retain IDs to preserve saved appearance.", HansaRequired="true", HansaReference="None", HansaBulkEditable="false", HansaAIAccess="Read", HansaMigration="Compatible", HansaSerialization="Included", HansaValidation="Compound"))
	TArray<FHansaCompoundLayout> Layouts;

 virtual void ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const override;
 void ValidateLayout(const FHansaCompoundLayout& Layout, TArray<FHansaDefinitionValidationIssue>& Issues, bool bCheckAssets) const;
 FHansaCompoundComposition Compose(uint64 ParcelSeed, int32 Stage, FName Context, const FString& DistrictId) const;
 static uint64 ParcelSeed(const FString& CityId, uint64 BuildingValue, uint32 Generation);
protected:
 virtual void AppendDefinitionHashData(FString& Data) const override;
};
