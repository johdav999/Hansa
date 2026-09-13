#include "UI/HansaBuildMenuPresentationModel.h"

#include "Commands/HansaGameplayCommandGateway.h"
#include "Construction/HansaConstruction.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Placement/HansaPlacement.h"
#include "World/HansaRuntimeSimulationHost.h"

#define LOCTEXT_NAMESPACE "HansaBuildMenuPresentationModel"

using namespace Hansa::Simulation;

struct FHansaBuildRuntimeState final
{
	FHansaPlacementSession Placement;
	TWeakObjectPtr<UHansaRuntimeSimulationHost> Host;
};

namespace
{
	EHansaBuildCategory CategoryFromDefinition(const FString& Category)
	{
		if (Category == TEXT("Roads")) return EHansaBuildCategory::Roads;
		if (Category == TEXT("Residences")) return EHansaBuildCategory::Residences;
		if (Category == TEXT("Storage")) return EHansaBuildCategory::Storage;
		if (Category == TEXT("Harbor")) return EHansaBuildCategory::Harbor;
		if (Category == TEXT("Civic")) return EHansaBuildCategory::Civic;
		if (Category == TEXT("Decoration")) return EHansaBuildCategory::Decoration;
		return EHansaBuildCategory::Production;
	}

	FString StableIdLeaf(const FString& StableId)
	{
		FString Left;
		FString Right;
		return StableId.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Right : StableId;
	}

	FString FormatQuantity(const int64 MilliUnits)
	{
		return MilliUnits % 1000 == 0
			? FString::Printf(TEXT("%lld"), static_cast<long long>(MilliUnits / 1000))
			: FString::Printf(TEXT("%.1f"), static_cast<double>(MilliUnits) / 1000.0);
	}

	FString GoodName(const FHansaEconomicRegistry& Registry, const FString& GoodId)
	{
		const FHansaCompiledGoodDefinition* Good = Registry.FindGood(GoodId);
		return Good != nullptr && !Good->DisplayName.IsEmpty() ? Good->DisplayName : StableIdLeaf(GoodId);
	}

	FString FormatAmounts(const FHansaEconomicRegistry& Registry, TConstArrayView<FHansaCompiledGoodAmount> Amounts)
	{
		TArray<FString> Parts;
		for (const FHansaCompiledGoodAmount& Amount : Amounts)
		{
			Parts.Add(FormatQuantity(Amount.QuantityMilliUnits) + TEXT(" ") + GoodName(Registry, Amount.GoodId).ToLower());
		}
		return FString::Join(Parts, TEXT(" + "));
	}

	FString FormatFlow(const FHansaEconomicRegistry& Registry, const FHansaCompiledBuildingDefinition& Building)
	{
		TArray<FHansaCompiledGoodAmount> Inputs;
		TArray<FHansaCompiledGoodAmount> Outputs;
		for (const FString& RecipeId : Building.RecipeIds)
		{
			if (const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(RecipeId))
			{
				Inputs.Append(Recipe->Inputs);
				Outputs.Append(Recipe->Outputs);
			}
		}
		if (Outputs.IsEmpty()) return Building.ConstructionPresentationPurpose;
		const FString InputText = Inputs.IsEmpty() ? TEXT("Natural source") : FormatAmounts(Registry, Inputs);
		return InputText + TEXT(" → ") + FormatAmounts(Registry, Outputs);
	}

	FString FormatTier(const FHansaCompiledBuildingDefinition& Building)
	{
		if (!Building.ResidentPopulationTierId.IsEmpty()) return StableIdLeaf(Building.ResidentPopulationTierId);
		if (Building.ArtisanWorkforce > 0) return TEXT("Artisan");
		if (Building.LaborerWorkforce > 0) return TEXT("Laborer");
		return Building.ConstructionMenuCategory == TEXT("Harbor") ? TEXT("Harbor") : TEXT("Civic");
	}

	FString FormatWorkforce(const FHansaCompiledBuildingDefinition& Building)
	{
		TArray<FString> Parts;
		if (Building.LaborerWorkforce > 0) Parts.Add(FString::Printf(TEXT("%d laborers"), Building.LaborerWorkforce));
		if (Building.ArtisanWorkforce > 0) Parts.Add(FString::Printf(TEXT("%d artisans"), Building.ArtisanWorkforce));
		if (Building.ResidenceCapacity > 0) Parts.Add(FString::Printf(TEXT("%d resident capacity"), Building.ResidenceCapacity));
		return Parts.IsEmpty() ? TEXT("No workforce") : FString::Join(Parts, TEXT(" · "));
	}

	void DeriveCategories(FHansaBuildMenuSnapshot& Snapshot)
	{
		Snapshot.Categories.Reset();
		for (const FHansaBuildCardPresentation& Card : Snapshot.Cards)
		{
			Snapshot.Categories.AddUnique(Card.Category);
		}
	}

	FText FailureCause(const EHansaPlacementFailure Failure)
	{
		switch (Failure)
		{
		case EHansaPlacementFailure::RoadRequired: return LOCTEXT("RoadRequired", "Road required");
		case EHansaPlacementFailure::ShorelineRequired: return LOCTEXT("ShoreRequired", "Land and water access required");
		case EHansaPlacementFailure::Occupied: return LOCTEXT("Occupied", "Footprint occupied");
		case EHansaPlacementFailure::OutsideBounds: return LOCTEXT("Outside", "Outside buildable area");
		case EHansaPlacementFailure::TerrainNotBuildable: return LOCTEXT("Terrain", "Terrain not buildable");
		case EHansaPlacementFailure::None: return LOCTEXT("Valid", "Valid placement");
		default: return FText::FromString(FString::Printf(TEXT("%s"), LexToString(Failure)));
		}
	}

	FText FailureRemedy(const EHansaPlacementFailure Failure)
	{
		switch (Failure)
		{
		case EHansaPlacementFailure::RoadRequired: return LOCTEXT("RoadRemedy", "Build next to a road.");
		case EHansaPlacementFailure::ShorelineRequired: return LOCTEXT("ShoreRemedy", "Place across the shoreline with land on one side and water on the other.");
		case EHansaPlacementFailure::Occupied: return LOCTEXT("OccupiedRemedy", "Choose an empty footprint.");
		case EHansaPlacementFailure::OutsideBounds: return LOCTEXT("BoundsRemedy", "Move inside the Lübeck boundary.");
		case EHansaPlacementFailure::TerrainNotBuildable: return LOCTEXT("TerrainRemedy", "Choose buildable land.");
		case EHansaPlacementFailure::None: return LOCTEXT("ConfirmRemedy", "Confirm to begin construction.");
		default: return LOCTEXT("OtherRemedy", "Choose another target.");
		}
	}

	FName CardSemanticId(const FName BuildingId)
	{
		FString Suffix = BuildingId.ToString();
		Suffix.ReplaceInline(TEXT("."), TEXT("_"));
		return FName(*FString::Printf(TEXT("BuildMenu.Card.%s"), *Suffix));
	}

	void ResetRoadPreview(FHansaBuildMenuSnapshot& Snapshot)
	{
		Snapshot.bRoadDrawing = false;
		Snapshot.RoadPreviewCells.Reset();
		Snapshot.RoadStartCell = FIntPoint::ZeroValue;
		Snapshot.RoadEndCell = FIntPoint::ZeroValue;
		Snapshot.RoadTotalCost = FText::GetEmpty();
		Snapshot.RoadNewCellCount = 0;
		Snapshot.RoadExistingCellCount = 0;
		Snapshot.RoadInvalidCellCount = 0;
	}

	int64 ScaledCost(const int64 PerCell, const int32 CellCount)
	{
		return CellCount <= 0 || PerCell <= 0 ? 0 :
			(PerCell > MAX_int64 / CellCount ? MAX_int64 : PerCell * CellCount);
	}
}

const TCHAR* LexToString(const EHansaBuildCategory Category)
{
	switch (Category)
	{
	case EHansaBuildCategory::Roads: return TEXT("Roads");
	case EHansaBuildCategory::Residences: return TEXT("Residences");
	case EHansaBuildCategory::Production: return TEXT("Production");
	case EHansaBuildCategory::Storage: return TEXT("Storage");
	case EHansaBuildCategory::Harbor: return TEXT("Harbor");
	case EHansaBuildCategory::Civic: return TEXT("Civic");
	case EHansaBuildCategory::Decoration: return TEXT("Decoration");
	default: return TEXT("Unknown");
	}
}

bool operator==(const FHansaBuildCardPresentation& Left, const FHansaBuildCardPresentation& Right)
{
	return Left.StableId == Right.StableId && Left.Name.EqualTo(Right.Name) && Left.Category == Right.Category &&
		Left.Tier.EqualTo(Right.Tier) && Left.Cost.EqualTo(Right.Cost) &&
		Left.WorkforceAndUpkeep.EqualTo(Right.WorkforceAndUpkeep) && Left.Footprint.EqualTo(Right.Footprint) &&
		Left.InputOutput.EqualTo(Right.InputOutput) && Left.LockedReason.EqualTo(Right.LockedReason) &&
		Left.AvailabilityReason.EqualTo(Right.AvailabilityReason) &&
		Left.ProductionChainOutputGoodId == Right.ProductionChainOutputGoodId && Left.MenuOrder == Right.MenuOrder &&
		Left.bLocked == Right.bLocked && Left.bAvailable == Right.bAvailable && Left.bFavorite == Right.bFavorite;
}

bool operator==(const FHansaBuildMenuSnapshot& Left, const FHansaBuildMenuSnapshot& Right)
{
	return Left.Cards == Right.Cards && Left.Categories == Right.Categories && Left.ProductionChains == Right.ProductionChains &&
		Left.SelectedCategory == Right.SelectedCategory && Left.SelectedBuildingId == Right.SelectedBuildingId &&
		Left.SelectedProductionChainOutputGoodId == Right.SelectedProductionChainOutputGoodId &&
		Left.FocusedSemanticId == Right.FocusedSemanticId &&
		Left.ValidationCause.EqualTo(Right.ValidationCause) && Left.ValidationRemedy.EqualTo(Right.ValidationRemedy) &&
		Left.PlacementSummary.EqualTo(Right.PlacementSummary) && Left.LastResult.EqualTo(Right.LastResult) &&
		Left.Feedback == Right.Feedback && Left.RotationQuarterTurns == Right.RotationQuarterTurns &&
		Left.AnchorCell == Right.AnchorCell && Left.FootprintCells == Right.FootprintCells &&
		Left.RoadPreviewCells == Right.RoadPreviewCells && Left.RoadStartCell == Right.RoadStartCell &&
		Left.RoadEndCell == Right.RoadEndCell && Left.RoadTotalCost.EqualTo(Right.RoadTotalCost) &&
		Left.RoadNewCellCount == Right.RoadNewCellCount &&
		Left.RoadExistingCellCount == Right.RoadExistingCellCount &&
		Left.RoadInvalidCellCount == Right.RoadInvalidCellCount &&
		Left.bOpen == Right.bOpen && Left.bHasTarget == Right.bHasTarget && Left.bCanConfirm == Right.bCanConfirm &&
		Left.bDraggingCard == Right.bDraggingCard && Left.bRoadDrawing == Right.bRoadDrawing &&
		Left.bPointerOverWorld == Right.bPointerOverWorld &&
		Left.bRepeat == Right.bRepeat && Left.bGridOverlay == Right.bGridOverlay && Left.bRoadOverlay == Right.bRoadOverlay;
}

UHansaBuildMenuPresentationModel::~UHansaBuildMenuPresentationModel() = default;

bool UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(
	const FHansaEconomicRegistry& Registry,
	const TSet<FString>& CompletedTechnologyIds,
	TArray<FHansaBuildCardPresentation>& OutCards,
	TArray<FHansaBuildChainPresentation>& OutChains,
	FString& OutError)
{
	OutCards.Reset();
	OutChains.Reset();
	OutError.Reset();
	TSet<FString> SeenChainIds;
	for (const FHansaCompiledBuildingDefinition& Building : Registry.GetBuildings())
	{
		if (!Building.bShowInConstructionMenu) continue;
		FHansaBuildCardPresentation Card;
		Card.StableId = FName(*Building.StableId);
		Card.Name = FText::FromString(Building.DisplayName);
		Card.Category = CategoryFromDefinition(Building.ConstructionMenuCategory);
		Card.MenuOrder = Building.ConstructionMenuOrder;
		Card.ProductionChainOutputGoodId = FName(*Building.ConstructionChainOutputGoodId);
		Card.Tier = FText::FromString(FormatTier(Building));
		TArray<FString> CostParts;
		CostParts.Add(FString::Printf(TEXT("%lld pf"), static_cast<long long>(Building.ConstructionCostPfennig)));
		const FString ResourceCosts = FormatAmounts(Registry, Building.ConstructionCosts);
		if (!ResourceCosts.IsEmpty()) CostParts.Add(ResourceCosts);
		Card.Cost = FText::FromString(FString::Join(CostParts, TEXT(" · ")));
		Card.WorkforceAndUpkeep = FText::FromString(FormatWorkforce(Building));
		Card.Footprint = FText::FromString(FString::Printf(TEXT("%d × %d"), Building.FootprintWidthCells, Building.FootprintHeightCells));
		Card.InputOutput = FText::FromString(FormatFlow(Registry, Building));
		Card.bLocked = Building.bUpgradeOnly || (!Building.RequiredConstructionTechnologyId.IsEmpty() &&
			!CompletedTechnologyIds.Contains(Building.RequiredConstructionTechnologyId));
		if (Building.bUpgradeOnly)
		{
			Card.LockedReason = LOCTEXT("UpgradeOnlyCard", "Upgrade an eligible lower-tier building");
		}
		else if (Card.bLocked)
		{
			const FHansaCompiledTechnologyDefinition* Technology = Registry.FindTechnology(Building.RequiredConstructionTechnologyId);
			Card.LockedReason = FText::Format(LOCTEXT("ResearchLockedCard", "Complete {0}"),
				FText::FromString(Technology != nullptr ? Technology->DisplayName : Building.RequiredConstructionTechnologyId));
		}
		OutCards.Add(MoveTemp(Card));

		if (!Building.ConstructionChainOutputGoodId.IsEmpty() && !SeenChainIds.Contains(Building.ConstructionChainOutputGoodId))
		{
			SeenChainIds.Add(Building.ConstructionChainOutputGoodId);
			FHansaBuildChainPresentation Chain;
			Chain.OutputGoodId = FName(*Building.ConstructionChainOutputGoodId);
			Chain.Name = FText::FromString(GoodName(Registry, Building.ConstructionChainOutputGoodId));
			Chain.StageCount = Building.ConstructionChainStageCount;
			OutChains.Add(MoveTemp(Chain));
		}
	}
	OutCards.Sort([](const FHansaBuildCardPresentation& Left, const FHansaBuildCardPresentation& Right)
	{
		if (Left.Category != Right.Category) return static_cast<uint8>(Left.Category) < static_cast<uint8>(Right.Category);
		const int32 ChainOrder = Left.ProductionChainOutputGoodId.Compare(Right.ProductionChainOutputGoodId);
		if (ChainOrder != 0) return ChainOrder < 0;
		return Left.MenuOrder != Right.MenuOrder ? Left.MenuOrder < Right.MenuOrder : Left.StableId.LexicalLess(Right.StableId);
	});
	OutChains.Sort([](const FHansaBuildChainPresentation& Left, const FHansaBuildChainPresentation& Right)
	{
		return Left.OutputGoodId.LexicalLess(Right.OutputGoodId);
	});
	if (OutCards.IsEmpty())
	{
		OutError = TEXT("The compiled registry contains no building definitions enabled for the construction menu.");
		return false;
	}
	return true;
}

bool UHansaBuildMenuPresentationModel::InitializeForLubeck(UWorld* World, FString& OutError)
{
	OwnedSimulationHost = NewObject<UHansaRuntimeSimulationHost>(this, TEXT("StandaloneBuildRuntime"));
	// A standalone build-menu model is the focused placement-test seam. Playable HUDs pass the GameMode-owned
	// host below and therefore use the selected populated scenario.
	if (!OwnedSimulationHost->InitializeForLubeck(World, OutError, EHansaRuntimeScenario::EmptyLubeckBuild))
	{
		return false;
	}
	return InitializeForLubeck(World, OwnedSimulationHost, OutError);
}

bool UHansaBuildMenuPresentationModel::InitializeForLubeck(
	UWorld* World,
	UHansaRuntimeSimulationHost* SimulationHost,
	FString& OutError)
{
	if (SimulationHost == nullptr)
	{
		OutError = TEXT("A runtime simulation host is required.");
		return false;
	}
	Runtime = MakeShared<FHansaBuildRuntimeState>();
	Runtime->Host = SimulationHost;
	if (!SimulationHost->IsReady() && !SimulationHost->InitializeForLubeck(World, OutError)) return false;

	FHansaBuildMenuSnapshot NewSnapshot;
	TSet<FString> CompletedTechnologies;
	if (const FHansaEconomicRegistry* Registry = SimulationHost->GetEconomicRegistry())
	{
		for (const FHansaCompiledTechnologyDefinition& Technology : Registry->GetTechnologies())
			if (SimulationHost->IsTechnologyCompleted(Technology.StableId)) CompletedTechnologies.Add(Technology.StableId);
		if (!BuildCatalogFromDefinitions(*Registry, CompletedTechnologies, NewSnapshot.Cards, NewSnapshot.ProductionChains, OutError)) return false;
		DeriveCategories(NewSnapshot);
	}
	else
	{
		OutError = TEXT("The runtime simulation host has no compiled economic registry.");
		return false;
	}
	if (!NewSnapshot.ProductionChains.IsEmpty())
	{
		const FHansaBuildChainPresentation* Bread = NewSnapshot.ProductionChains.FindByPredicate(
			[](const FHansaBuildChainPresentation& Chain) { return Chain.OutputGoodId == TEXT("Good.Bread"); });
		NewSnapshot.SelectedProductionChainOutputGoodId = Bread != nullptr ? Bread->OutputGoodId : NewSnapshot.ProductionChains[0].OutputGoodId;
	}
	Snapshot = MoveTemp(NewSnapshot);
	RefreshAvailability();
	++Revision; Changed.Broadcast(Snapshot, Revision);
	return true;
}

void UHansaBuildMenuPresentationModel::SetOpen(const bool bOpen)
{
    if (bOpen && !bConstructionAllowed) return;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.bOpen = bOpen;
	if (!bOpen && GetSimulationHost() != nullptr)
	{
		EndBuildingStroke();
		Runtime->Placement.Cancel();
		Snapshot.SelectedBuildingId = NAME_None;
		Snapshot.bHasTarget = false;
		Snapshot.bCanConfirm = false;
		Snapshot.bDraggingCard = false;
		Snapshot.bPointerOverWorld = false;
		Snapshot.FootprintCells.Reset();
		ResetRoadPreview(Snapshot);
		Snapshot.Feedback = EHansaPlacementFeedback::None;
		Snapshot.ValidationCause = FText::GetEmpty();
		Snapshot.ValidationRemedy = FText::GetEmpty();
	}
	PublishIfChanged(Previous);
}

bool UHansaBuildMenuPresentationModel::SelectCategory(const EHansaBuildCategory Category)
{
    if (!bConstructionAllowed) return false;
	if (GetSimulationHost() == nullptr || !Snapshot.Categories.Contains(Category)) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	EndBuildingStroke();
	Snapshot.SelectedProductionChainOutputGoodId = NAME_None;
	Snapshot.bOpen = true; Snapshot.SelectedCategory = Category; Snapshot.SelectedBuildingId = NAME_None;
	Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.bDraggingCard = false; Snapshot.bPointerOverWorld = false; Snapshot.FootprintCells.Reset();
	ResetRoadPreview(Snapshot);
	Runtime->Placement.Cancel(); PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::SelectProductionChain(const FName OutputGoodId)
{
    if (!bConstructionAllowed) return false;
	if (GetSimulationHost() == nullptr || !Snapshot.ProductionChains.ContainsByPredicate(
		[OutputGoodId](const FHansaBuildChainPresentation& Chain) { return Chain.OutputGoodId == OutputGoodId; })) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.bOpen = true; Snapshot.SelectedCategory = EHansaBuildCategory::Production;
	EndBuildingStroke();
	Snapshot.SelectedProductionChainOutputGoodId = OutputGoodId;
	Snapshot.SelectedBuildingId = NAME_None;
	Snapshot.bHasTarget = false;
	Snapshot.bCanConfirm = false;
	Snapshot.bDraggingCard = false;
	Snapshot.bPointerOverWorld = false;
	Snapshot.FootprintCells.Reset();
	ResetRoadPreview(Snapshot);
	Snapshot.Feedback = EHansaPlacementFeedback::None;
	Runtime->Placement.Cancel();
	PublishIfChanged(Previous);
	return true;
}

const FHansaBuildCardPresentation* UHansaBuildMenuPresentationModel::FindCard(const FName BuildingId) const
{
	return Snapshot.Cards.FindByPredicate([BuildingId](const FHansaBuildCardPresentation& Card) { return Card.StableId == BuildingId; });
}

bool UHansaBuildMenuPresentationModel::SelectBuilding(const FName BuildingId)
{
	EndBuildingStroke();
    if (!bConstructionAllowed) return false;
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return false;
	const FHansaBuildCardPresentation* Selected = FindCard(BuildingId);
	if (Selected == nullptr || Selected->bLocked || !Selected->bAvailable) return false;
	const auto DefinitionId = FHansaBuildingTypeId::TryParse(BuildingId.ToString());
	if (!DefinitionId) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.bOpen = true; Snapshot.SelectedCategory = Selected->Category; Snapshot.SelectedBuildingId = BuildingId;
	if (Selected->Category == EHansaBuildCategory::Production && !Selected->ProductionChainOutputGoodId.IsNone())
	{
		Snapshot.SelectedProductionChainOutputGoodId = Selected->ProductionChainOutputGoodId;
	}
	Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.bDraggingCard = false; Snapshot.bPointerOverWorld = false; Snapshot.FootprintCells.Reset();
	ResetRoadPreview(Snapshot);
	Snapshot.ValidationCause = FText::GetEmpty(); Snapshot.ValidationRemedy = LOCTEXT("ChooseTarget", "Move over the city to preview. Click to build; hold and move to place more.");
	Runtime->Placement.SelectBuilding(Host->GetCityId(), DefinitionId.Value, BuildingId == TEXT("Building.Road"), Snapshot.bRepeat);
	PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::BeginCardDrag(const FName BuildingId)
{
	if (!SelectBuilding(BuildingId)) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.bDraggingCard = true;
	Snapshot.bPointerOverWorld = false;
	Snapshot.FocusedSemanticId = CardSemanticId(BuildingId);
	Snapshot.LastResult = LOCTEXT("DragStarted", "Drag into the city · release on a valid footprint");
	PublishIfChanged(Previous);
	return true;
}

bool UHansaBuildMenuPresentationModel::UpdateCardDragTarget(const int32 X, const int32 Y)
{
	if (!Snapshot.bDraggingCard) return false;
	return TargetGridCell(X, Y);
}

bool UHansaBuildMenuPresentationModel::ClearCardDragTarget()
{
	if (!Snapshot.bDraggingCard) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.bPointerOverWorld = false;
	Snapshot.bHasTarget = false;
	Snapshot.bCanConfirm = false;
	Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.FootprintCells.Reset();
	Snapshot.ValidationCause = LOCTEXT("PointerOutsideWorld", " Outside the buildable world");
	Snapshot.ValidationRemedy = LOCTEXT("PointerOutsideWorldRemedy", "Move over the city, or release to return to the building card.");
	PublishIfChanged(Previous);
	return true;
}

bool UHansaBuildMenuPresentationModel::EndCardDrag(const bool bReleasedOverWorld)
{
	if (!Snapshot.bDraggingCard || GetSimulationHost() == nullptr) return false;
	const FName BuildingId = Snapshot.SelectedBuildingId;
	const FName ReturnFocus = CardSemanticId(BuildingId);
	if (bReleasedOverWorld && Snapshot.bCanConfirm)
	{
		Snapshot.bDraggingCard = false;
		Snapshot.bPointerOverWorld = false;
		const bool bCommitted = ConfirmIntent();
		SetFocusedSemanticId(ReturnFocus);
		return bCommitted;
	}

	const FHansaBuildMenuSnapshot Previous = Snapshot;
	const FText Cause = Snapshot.ValidationCause;
	const FText Remedy = Snapshot.ValidationRemedy;
	Runtime->Placement.Cancel();
	Snapshot.SelectedBuildingId = NAME_None;
	Snapshot.bDraggingCard = false;
	Snapshot.bPointerOverWorld = false;
	Snapshot.bHasTarget = false;
	Snapshot.bCanConfirm = false;
	Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.FootprintCells.Reset();
	Snapshot.FocusedSemanticId = ReturnFocus;
	Snapshot.LastResult = bReleasedOverWorld
		? FText::Format(LOCTEXT("InvalidDragDrop", "Not placed · {0} · {1}"), Cause, Remedy)
		: LOCTEXT("OutsideDragDrop", " Placement cancelled · release over a valid city footprint");
	Snapshot.ValidationCause = FText::GetEmpty();
	Snapshot.ValidationRemedy = FText::GetEmpty();
	Snapshot.PlacementSummary = FText::GetEmpty();
	PublishIfChanged(Previous);
	return false;
}

bool UHansaBuildMenuPresentationModel::BeginRoadDraw(const int32 X, const int32 Y)
{
    if (!bConstructionAllowed) return false;
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr || Snapshot.SelectedBuildingId != TEXT("Building.Road")) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Runtime->Placement.BeginRoadDrag({ X, Y });
	Snapshot.bRoadDrawing = true;
	Snapshot.bDraggingCard = false;
	Snapshot.bPointerOverWorld = true;
	Snapshot.bHasTarget = true;
	Snapshot.RoadStartCell = FIntPoint(X, Y);
	Snapshot.RoadEndCell = Snapshot.RoadStartCell;
	Snapshot.AnchorCell = Snapshot.RoadEndCell;
	Snapshot.LastResult = LOCTEXT("RoadDrawStarted", "Road drawing · drag to extend · release to build");
	RefreshRoadValidation();
	PublishIfChanged(Previous);
	return true;
}

bool UHansaBuildMenuPresentationModel::UpdateRoadDraw(const int32 X, const int32 Y)
{
    if (!bConstructionAllowed) return false;
	if (!Snapshot.bRoadDrawing || GetSimulationHost() == nullptr) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Runtime->Placement.UpdateRoadDrag({ X, Y });
	Snapshot.bPointerOverWorld = true;
	Snapshot.bHasTarget = true;
	Snapshot.RoadEndCell = FIntPoint(X, Y);
	Snapshot.AnchorCell = Snapshot.RoadEndCell;
	RefreshRoadValidation();
	PublishIfChanged(Previous);
	return true;
}

bool UHansaBuildMenuPresentationModel::EndRoadDraw(const bool bReleasedOverWorld)
{
    if (!bConstructionAllowed) return false;
	if (!Snapshot.bRoadDrawing || GetSimulationHost() == nullptr) return false;
	const FName ReturnFocus = CardSemanticId(Snapshot.SelectedBuildingId);
	if (bReleasedOverWorld && Snapshot.bCanConfirm)
	{
		const bool bCommitted = ConfirmIntent();
		SetFocusedSemanticId(ReturnFocus);
		return bCommitted;
	}

	const FHansaBuildMenuSnapshot Previous = Snapshot;
	const FText Cause = Snapshot.ValidationCause;
	const FText Remedy = Snapshot.ValidationRemedy;
	Runtime->Placement.CancelRoadDrag();
	ResetRoadPreview(Snapshot);
	Snapshot.bPointerOverWorld = false;
	Snapshot.bHasTarget = false;
	Snapshot.bCanConfirm = false;
	Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.FootprintCells.Reset();
	Snapshot.FocusedSemanticId = ReturnFocus;
	Snapshot.LastResult = bReleasedOverWorld
		? FText::Format(LOCTEXT("InvalidRoadRelease", "Road not built · {0} · {1}"), Cause, Remedy)
		: LOCTEXT("CancelledRoadRelease", " Road drawing cancelled · press and drag across the city");
	PublishIfChanged(Previous);
	return false;
}

bool UHansaBuildMenuPresentationModel::TargetGridCell(const int32 X, const int32 Y)
{
    if (!bConstructionAllowed) return false;
	if (GetSimulationHost() == nullptr || Snapshot.SelectedBuildingId.IsNone()) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Runtime->Placement.SetAnchor({ X, Y }); Snapshot.AnchorCell = FIntPoint(X, Y); Snapshot.bHasTarget = true;
	Snapshot.bPointerOverWorld = true;
	RefreshValidation(); PublishIfChanged(Previous); return true;
}

FIntPoint UHansaBuildMenuPresentationModel::GetRoadAdjacentTarget() const
{
	constexpr int32 RoadX = 18;
	constexpr int32 RoadY = 16;
	int32 FootprintWidth = 2;
	if (UHansaRuntimeSimulationHost* Host = GetSimulationHost(); Host != nullptr && !Snapshot.SelectedBuildingId.IsNone())
	{
		if (const FHansaCompiledBuildingDefinition* Definition = Host->FindBuildingDefinition(Snapshot.SelectedBuildingId.ToString()))
		{
			const bool bQuarterTurn = Snapshot.RotationQuarterTurns % 2 != 0;
			FootprintWidth = FMath::Max(1, bQuarterTurn ? Definition->FootprintHeightCells : Definition->FootprintWidthCells);
		}
	}
	return FIntPoint(RoadX - FootprintWidth, RoadY);
}

bool UHansaBuildMenuPresentationModel::TargetRoadAdjacentIntent()
{
	const FIntPoint Target = GetRoadAdjacentTarget();
	return TargetGridCell(Target.X, Target.Y);
}

TOptional<FIntPoint> UHansaBuildMenuPresentationModel::FindValidShorelineTarget() const
{
	const TOptional<TPair<FIntPoint, int32>> Placement = FindValidShorelinePlacement();
	return Placement.IsSet() ? TOptional<FIntPoint>(Placement->Key) : TOptional<FIntPoint>();
}

TOptional<TPair<FIntPoint, int32>> UHansaBuildMenuPresentationModel::FindValidShorelinePlacement() const
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr || Snapshot.SelectedBuildingId.IsNone()) return {};
	const FHansaCompiledBuildingDefinition* Definition = Host->FindBuildingDefinition(Snapshot.SelectedBuildingId.ToString());
	const auto DefinitionId = FHansaBuildingTypeId::TryParse(Snapshot.SelectedBuildingId.ToString());
	if (Definition == nullptr || !Definition->bRequiresShoreline || !DefinitionId) return {};

	const FHansaPlacementMapInitialization* Map = Host->FindPlacementMap();
	if (Map == nullptr) return {};
	// This is the authored central quay approach. Validation, rather than this preference, remains authoritative.
	const FIntPoint PreferredHarborArea(27, 17);
	TArray<FHansaGridCoordinate> Candidates;
	for (int32 X = Map->BoundsMin.X; X <= Map->BoundsMax.X; ++X)
	{
		for (int32 Y = Map->BoundsMin.Y; Y <= Map->BoundsMax.Y; ++Y) Candidates.Add({ X, Y });
	}
	Candidates.Sort([PreferredHarborArea](const FHansaGridCoordinate& Left, const FHansaGridCoordinate& Right)
	{
		const int32 LeftDistance = FMath::Abs(Left.X - PreferredHarborArea.X) + FMath::Abs(Left.Y - PreferredHarborArea.Y);
		const int32 RightDistance = FMath::Abs(Right.X - PreferredHarborArea.X) + FMath::Abs(Right.Y - PreferredHarborArea.Y);
		if (LeftDistance != RightDistance) return LeftDistance < RightDistance;
		return Left.X != Right.X ? Left.X < Right.X : Left.Y < Right.Y;
	});

	for (const FHansaGridCoordinate Candidate : Candidates)
	{
		for (int32 RotationQuarterTurns = 0; RotationQuarterTurns < 4; ++RotationQuarterTurns)
		{
			FHansaPlacementSpec Spec;
			Spec.CityId = Host->GetCityId();
			Spec.BuildingDefinitionId = DefinitionId.Value;
			Spec.Anchor = Candidate;
			Spec.Rotation = static_cast<EHansaGridRotation>(RotationQuarterTurns);
			if (Host->ValidatePlacement(Spec).CanPlace())
			{
				return TPair<FIntPoint, int32>(FIntPoint(Candidate.X, Candidate.Y), RotationQuarterTurns);
			}
		}
	}
	return {};
}

bool UHansaBuildMenuPresentationModel::TargetShorelineIntent()
{
	const TOptional<TPair<FIntPoint, int32>> Placement = FindValidShorelinePlacement();
	if (!Placement.IsSet()) return false;
	while (Snapshot.RotationQuarterTurns != Placement->Value)
	{
		Runtime->Placement.RotateClockwise();
		Snapshot.RotationQuarterTurns = (Snapshot.RotationQuarterTurns + 1) % 4;
	}
	return TargetGridCell(Placement->Key.X, Placement->Key.Y);
}

bool UHansaBuildMenuPresentationModel::RotateIntent()
{
	if (GetSimulationHost() == nullptr || Snapshot.SelectedBuildingId.IsNone()) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Runtime->Placement.RotateClockwise(); Snapshot.RotationQuarterTurns = (Snapshot.RotationQuarterTurns + 1) % 4;
	RefreshValidation(); PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::ToggleRepeatIntent()
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr || Snapshot.SelectedBuildingId.IsNone()) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot; Snapshot.bRepeat = !Snapshot.bRepeat;
	const FName Selected = Snapshot.SelectedBuildingId; Runtime->Placement.Cancel();
	const auto DefinitionId = FHansaBuildingTypeId::TryParse(Selected.ToString());
	Runtime->Placement.SelectBuilding(Host->GetCityId(), DefinitionId.Value, Selected == TEXT("Building.Road"), Snapshot.bRepeat);
	Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	ResetRoadPreview(Snapshot);
	PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::ToggleGridOverlayIntent()
{
	const FHansaBuildMenuSnapshot Previous = Snapshot; Snapshot.bGridOverlay = !Snapshot.bGridOverlay; PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::ToggleRoadOverlayIntent()
{
	const FHansaBuildMenuSnapshot Previous = Snapshot; Snapshot.bRoadOverlay = !Snapshot.bRoadOverlay; PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::ToggleFavoriteIntent()
{
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	FHansaBuildCardPresentation* Card = Snapshot.Cards.FindByPredicate(
		[this](const FHansaBuildCardPresentation& Candidate) { return Candidate.StableId == Snapshot.SelectedBuildingId; });
	if (Card == nullptr) return false;
	Card->bFavorite = !Card->bFavorite;
	PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::CompareIntent()
{
	const FHansaBuildCardPresentation* Card = FindCard(Snapshot.SelectedBuildingId);
	if (Card == nullptr) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.LastResult = FText::Format(LOCTEXT("CompareResult", "Compare pinned · {0} · {1} · {2}"),
		Card->Name, Card->Cost, Card->WorkforceAndUpkeep);
	PublishIfChanged(Previous); return true;
}


bool UHansaBuildMenuPresentationModel::ClearPointerTarget()
{
 const FHansaBuildMenuSnapshot Previous=Snapshot;
 StrokePrevious.Reset();
 Snapshot.bHasTarget=false; Snapshot.bCanConfirm=false; Snapshot.bPointerOverWorld=false;
 Snapshot.FootprintCells.Reset(); Snapshot.Feedback=EHansaPlacementFeedback::None;
 PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::BeginBuildingStroke(int32 X,int32 Y)
{
 EndBuildingStroke();
 if(Snapshot.SelectedBuildingId.IsNone() || Snapshot.SelectedBuildingId==TEXT("Building.Road"))return false;
 if(!TargetGridCell(X,Y) || !Snapshot.bCanConfirm || !ConfirmIntent(true))return false;
 bBuildingStroke=true; StrokePrevious=FIntPoint(X,Y); StrokeVisited.Add(FIntPoint(X,Y));
 return true;
}

bool UHansaBuildMenuPresentationModel::UpdateBuildingStroke(int32 X,int32 Y)
{
 if(!bBuildingStroke || !bConstructionAllowed || Snapshot.SelectedBuildingId.IsNone())return false;
 const FIntPoint End(X,Y), Start=StrokePrevious.Get(End);
 // Integer line traversal prevents gaps when input skips cells. Every footprint
 // still passes the authoritative affordability/occupancy/terrain checks.
 const int32 DX=FMath::Abs(X-Start.X),DY=FMath::Abs(Y-Start.Y);
 if(FMath::Max(DX,DY)>1024){StrokePrevious=End;return false;}
 int32 CX=Start.X,CY=Start.Y,Error=DX-DY;
 bool Placed=false;
 for(;;){
  const FIntPoint Cell(CX,CY);
  if(!StrokeVisited.Contains(Cell)){
   StrokeVisited.Add(Cell);
   if(TargetGridCell(CX,CY) && Snapshot.bCanConfirm)Placed=ConfirmIntent(true)||Placed;
  }
  if(CX==X && CY==Y)break;
  const int32 Twice=2*Error;
  if(Twice>-DY){Error-=DY;CX+=Start.X<X?1:-1;}
  if(Twice<DX){Error+=DX;CY+=Start.Y<Y?1:-1;}
 }
 StrokePrevious=End; TargetGridCell(X,Y); return Placed;
}
void UHansaBuildMenuPresentationModel::EndBuildingStroke()
{
 bBuildingStroke=false;StrokePrevious.Reset();StrokeVisited.Reset();
}

bool UHansaBuildMenuPresentationModel::ConfirmIntent(const bool bKeepSelection)
{
    if (!bConstructionAllowed) return false;
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return false;
	RefreshValidation();
	const bool bRoadDrawing = Snapshot.bRoadDrawing;
	const int32 RoadCellCount = Snapshot.RoadNewCellCount;
	const TArray<FHansaPlacementSpec> Specs = bRoadDrawing
		? BuildRoadConstructionSpecs() : Runtime->Placement.BuildConfirmationSpecs();
	if (!Snapshot.bCanConfirm || Specs.IsEmpty()) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	const FHansaCommandGatewayResult Result = Host->PlaceBuildings(Specs);
	if (!Result)
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Invalid; Snapshot.bCanConfirm = false;
		if (Result.GetPlacementValidation().IsSet())
		{
			const EHansaPlacementFailure Failure = Result.GetPlacementValidation()->GetPrimaryFailure();
			Snapshot.ValidationCause = FailureCause(Failure); Snapshot.ValidationRemedy = FailureRemedy(Failure);
		}
		PublishIfChanged(Previous); return false;
	}
	Snapshot.LastResult = bRoadDrawing
		? FText::Format(LOCTEXT("RoadPlacedResult", "Road committed · {0} new cells"), FText::AsNumber(RoadCellCount))
		: FText::Format(LOCTEXT("PlacedResult", "Construction committed · {0}"), FText::FromName(Snapshot.SelectedBuildingId));
	if (!bKeepSelection) Runtime->Placement.OnConfirmationSucceeded(); Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false;
	Snapshot.Feedback = EHansaPlacementFeedback::None; Snapshot.ValidationCause = FText::GetEmpty(); Snapshot.ValidationRemedy = FText::GetEmpty();
	Snapshot.FootprintCells.Reset(); Snapshot.bPointerOverWorld = false;
	ResetRoadPreview(Snapshot);
	if (!Snapshot.bRepeat && !bKeepSelection) Snapshot.SelectedBuildingId = NAME_None;
	RefreshAvailability();
	PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::CancelIntent()
{
	EndBuildingStroke();
	if (GetSimulationHost() == nullptr) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	const FName ReturnFocus = Snapshot.SelectedBuildingId.IsNone() ? Snapshot.FocusedSemanticId : CardSemanticId(Snapshot.SelectedBuildingId);
	Runtime->Placement.Cancel(); Snapshot.SelectedBuildingId = NAME_None; Snapshot.bHasTarget = false;
	Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.bDraggingCard = false; Snapshot.bPointerOverWorld = false; Snapshot.FootprintCells.Reset();
	ResetRoadPreview(Snapshot);
	Snapshot.ValidationCause = FText::GetEmpty(); Snapshot.ValidationRemedy = FText::GetEmpty();
	Snapshot.FocusedSemanticId = ReturnFocus;
	PublishIfChanged(Previous); return true;
}

void UHansaBuildMenuPresentationModel::SetFocusedSemanticId(const FName SemanticId)
{
	const FHansaBuildMenuSnapshot Previous = Snapshot; Snapshot.FocusedSemanticId = SemanticId; PublishIfChanged(Previous);
}

void UHansaBuildMenuPresentationModel::RefreshValidation()
{
	Snapshot.bCanConfirm = false;
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr || !Snapshot.bHasTarget) return;
	if (Snapshot.bRoadDrawing)
	{
		RefreshRoadValidation();
		return;
	}
	const TArray<FHansaPlacementSpec> Specs = Runtime->Placement.BuildConfirmationSpecs();
	if (Specs.IsEmpty()) return;
	const FHansaPlacementValidationResult Validation = Host->ValidatePlacement(Specs[0]);
	const EHansaPlacementFailure Failure = Validation.GetPrimaryFailure();
	Snapshot.FootprintCells.Reset();
	for (const FHansaGridCoordinate Cell : Validation.GetOccupiedCells()) Snapshot.FootprintCells.Add(FIntPoint(Cell.X, Cell.Y));
	Snapshot.bCanConfirm = Validation.CanPlace();
	const FHansaCompiledBuildingDefinition* Definition = Host->FindBuildingDefinition(Snapshot.SelectedBuildingId.ToString());
	if (Snapshot.bCanConfirm && (Snapshot.bRepeat || (Definition != nullptr && Definition->bRequiresShoreline)))
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Warning;
		Snapshot.ValidationCause = Snapshot.bRepeat
			? LOCTEXT("RepeatWarning", "Valid · repeated placement enabled")
			: LOCTEXT("ShoreWarning", "Valid shoreline footprint");
		Snapshot.ValidationRemedy = Snapshot.bRepeat
			? LOCTEXT("RepeatWarningRemedy", "Release to build; the same card remains active.")
			: LOCTEXT("ShoreWarningRemedy", "Release to build across the required land and water cells.");
	}
	else
	{
		Snapshot.Feedback = Snapshot.bCanConfirm ? EHansaPlacementFeedback::Valid : EHansaPlacementFeedback::Invalid;
		Snapshot.ValidationCause = FailureCause(Failure); Snapshot.ValidationRemedy = FailureRemedy(Failure);
	}
	const FHansaBuildCardPresentation* Card = FindCard(Snapshot.SelectedBuildingId);
	Snapshot.PlacementSummary = FText::Format(LOCTEXT("PlacementSummary", "{0} · cell {1},{2} · rotation {3}° · {4}"),
		FText::FromName(Snapshot.SelectedBuildingId), FText::AsNumber(Snapshot.AnchorCell.X), FText::AsNumber(Snapshot.AnchorCell.Y),
		FText::AsNumber(Snapshot.RotationQuarterTurns * 90), Card != nullptr ? Card->Cost : FText::GetEmpty());
}

TArray<FHansaPlacementSpec> UHansaBuildMenuPresentationModel::BuildRoadConstructionSpecs() const
{
	TArray<FHansaPlacementSpec> Result;
	const TArray<FHansaPlacementSpec> Path = Runtime.IsValid()
		? Runtime->Placement.BuildConfirmationSpecs() : TArray<FHansaPlacementSpec>();
	for (int32 Index = 0; Index < Path.Num() && Index < Snapshot.RoadPreviewCells.Num(); ++Index)
	{
		if (Snapshot.RoadPreviewCells[Index].State == EHansaRoadPreviewCellState::NewValid)
		{
			Result.Add(Path[Index]);
		}
	}
	return Result;
}

void UHansaBuildMenuPresentationModel::RefreshRoadValidation()
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return;
	const TArray<FHansaPlacementSpec> Path = Runtime->Placement.BuildConfirmationSpecs();
	Snapshot.RoadPreviewCells.Reset();
	Snapshot.FootprintCells.Reset();
	Snapshot.RoadNewCellCount = 0;
	Snapshot.RoadExistingCellCount = 0;
	Snapshot.RoadInvalidCellCount = 0;
	EHansaPlacementFailure FirstFailure = EHansaPlacementFailure::None;
	for (const FHansaPlacementSpec& Spec : Path)
	{
		FHansaRoadPreviewCell Cell;
		Cell.Cell = FIntPoint(Spec.Anchor.X, Spec.Anchor.Y);
		Snapshot.FootprintCells.Add(Cell.Cell);
		if (Host->IsOwnedRoadCell(Spec.Anchor))
		{
			Cell.State = EHansaRoadPreviewCellState::ExistingRoad;
			++Snapshot.RoadExistingCellCount;
		}
		else
		{
			const FHansaPlacementValidationResult Validation = Host->ValidatePlacement(Spec);
			if (Validation.CanPlace())
			{
				Cell.State = EHansaRoadPreviewCellState::NewValid;
				++Snapshot.RoadNewCellCount;
			}
			else
			{
				Cell.State = EHansaRoadPreviewCellState::Invalid;
				const EHansaPlacementFailure Failure = Validation.GetPrimaryFailure();
				Cell.Failure = FName(LexToString(Failure));
				if (FirstFailure == EHansaPlacementFailure::None) FirstFailure = Failure;
				++Snapshot.RoadInvalidCellCount;
			}
		}
		Snapshot.RoadPreviewCells.Add(Cell);
	}

	const FHansaConstructionCostProjection UnitCost = Host->QueryConstructionCost(TEXT("Building.Road"));
	TArray<FString> CostParts;
	const int64 RequiredCurrency = ScaledCost(UnitCost.RequiredCurrency.GetRawValue(), Snapshot.RoadNewCellCount);
	if (RequiredCurrency > 0) CostParts.Add(FString::Printf(TEXT("%lld pf"), static_cast<long long>(RequiredCurrency)));
	bool bAffordable = RequiredCurrency <= UnitCost.AvailableCurrency.GetRawValue();
	const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
	for (const FHansaConstructionResourceCostProjection& Resource : UnitCost.Resources)
	{
		const int64 Required = ScaledCost(Resource.Required.GetRawValue(), Snapshot.RoadNewCellCount);
		if (Required <= 0) continue;
		const FString GoodId = Resource.GoodId.ToString();
		CostParts.Add(FormatQuantity(Required) + TEXT(" ") +
			(Registry != nullptr ? GoodName(*Registry, GoodId).ToLower() : StableIdLeaf(GoodId).ToLower()));
		bAffordable &= Required <= Resource.Available.GetRawValue();
	}
	Snapshot.RoadTotalCost = FText::FromString(FString::Printf(TEXT("%d new cells · %s"),
		Snapshot.RoadNewCellCount, CostParts.IsEmpty() ? TEXT("no cost") : *FString::Join(CostParts, TEXT(" + "))));
	Snapshot.bCanConfirm = Snapshot.RoadNewCellCount > 0 && Snapshot.RoadInvalidCellCount == 0 && bAffordable;
	if (Snapshot.RoadInvalidCellCount > 0)
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Invalid;
		Snapshot.ValidationCause = FailureCause(FirstFailure);
		Snapshot.ValidationRemedy = FailureRemedy(FirstFailure);
	}
	else if (Snapshot.RoadNewCellCount == 0)
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Warning;
		Snapshot.ValidationCause = LOCTEXT("RoadAlreadyBuilt", "Road already exists on this path");
		Snapshot.ValidationRemedy = LOCTEXT("RoadAlreadyBuiltRemedy", "Drag beyond the existing road to add new cells.");
	}
	else if (!bAffordable)
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Invalid;
		Snapshot.ValidationCause = LOCTEXT("RoadInsufficientFunds", "Insufficient construction resources");
		Snapshot.ValidationRemedy = LOCTEXT("RoadInsufficientFundsRemedy", "Shorten the road or acquire the missing resources.");
	}
	else if (Snapshot.RoadExistingCellCount > 0)
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Warning;
		Snapshot.ValidationCause = LOCTEXT("RoadContinuation", "Valid road continuation");
		Snapshot.ValidationRemedy = LOCTEXT("RoadContinuationRemedy", "Existing intersections are reused and only new cells are charged.");
	}
	else
	{
		Snapshot.Feedback = EHansaPlacementFeedback::Valid;
		Snapshot.ValidationCause = LOCTEXT("RoadValid", "Valid connected road");
		Snapshot.ValidationRemedy = LOCTEXT("RoadValidRemedy", "Release to build the complete path.");
	}
	Snapshot.PlacementSummary = FText::Format(
		LOCTEXT("RoadPlacementSummary", "Road · {0},{1} → {2},{3} · horizontal first · {4}"),
		FText::AsNumber(Snapshot.RoadStartCell.X), FText::AsNumber(Snapshot.RoadStartCell.Y),
		FText::AsNumber(Snapshot.RoadEndCell.X), FText::AsNumber(Snapshot.RoadEndCell.Y),
		Snapshot.RoadTotalCost);
}

void UHansaBuildMenuPresentationModel::RefreshAvailability()
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return;
	for (FHansaBuildCardPresentation& Card : Snapshot.Cards)
	{
		Card.bAvailable = !Card.bLocked;
		Card.AvailabilityReason = Card.bLocked ? Card.LockedReason : FText::GetEmpty();
		if (Card.bLocked) continue;
		const FHansaConstructionCostProjection Cost = Host->QueryConstructionCost(Card.StableId.ToString());
		if (Cost.IsAffordable()) continue;
		Card.bAvailable = false;
		TArray<FString> Missing;
		if (Cost.MissingCurrency.GetRawValue() > 0)
		{
			Missing.Add(FString::Printf(TEXT("%lld pf"), static_cast<long long>(Cost.MissingCurrency.GetRawValue())));
		}
		const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
		for (const FHansaConstructionResourceCostProjection& Resource : Cost.Resources)
		{
			if (Resource.Missing.GetRawValue() <= 0) continue;
			const FString GoodId = Resource.GoodId.ToString();
			Missing.Add(FormatQuantity(Resource.Missing.GetRawValue()) + TEXT(" ") +
				(Registry != nullptr ? GoodName(*Registry, GoodId).ToLower() : StableIdLeaf(GoodId).ToLower()));
		}
		Card.AvailabilityReason = FText::FromString(Missing.IsEmpty()
			? TEXT("Construction resources unavailable")
			: TEXT("Missing ") + FString::Join(Missing, TEXT(" + ")));
	}
}

bool UHansaBuildMenuPresentationModel::ReloadCatalog(FString& OutError)
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	const FHansaEconomicRegistry* Registry = Host != nullptr ? Host->GetEconomicRegistry() : nullptr;
	if (Host == nullptr || Registry == nullptr)
	{
		OutError = TEXT("A ready runtime host and compiled registry are required to reload the construction catalog.");
		return false;
	}
	TSet<FString> CompletedTechnologies;
	for (const FHansaCompiledTechnologyDefinition& Technology : Registry->GetTechnologies())
		if (Host->IsTechnologyCompleted(Technology.StableId)) CompletedTechnologies.Add(Technology.StableId);
	TArray<FHansaBuildCardPresentation> Cards;
	TArray<FHansaBuildChainPresentation> Chains;
	if (!BuildCatalogFromDefinitions(*Registry, CompletedTechnologies, Cards, Chains, OutError)) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	for (FHansaBuildCardPresentation& Card : Cards)
	{
		if (const FHansaBuildCardPresentation* Existing = FindCard(Card.StableId)) Card.bFavorite = Existing->bFavorite;
	}
	Snapshot.Cards = MoveTemp(Cards);
	Snapshot.ProductionChains = MoveTemp(Chains);
	DeriveCategories(Snapshot);
	if (!Snapshot.Categories.Contains(Snapshot.SelectedCategory))
	{
		Snapshot.SelectedCategory = Snapshot.Categories.IsEmpty() ? EHansaBuildCategory::Roads : Snapshot.Categories[0];
	}
	if (!Snapshot.ProductionChains.ContainsByPredicate([this](const FHansaBuildChainPresentation& Chain)
		{ return Chain.OutputGoodId == Snapshot.SelectedProductionChainOutputGoodId; }))
	{
		Snapshot.SelectedProductionChainOutputGoodId = Snapshot.ProductionChains.IsEmpty()
			? NAME_None : Snapshot.ProductionChains[0].OutputGoodId;
	}
	if (FindCard(Snapshot.SelectedBuildingId) == nullptr) Snapshot.SelectedBuildingId = NAME_None;
	RefreshAvailability();
	PublishIfChanged(Previous);
	return true;
}

void UHansaBuildMenuPresentationModel::PublishIfChanged(const FHansaBuildMenuSnapshot& Previous)
{
	if (!(Previous == Snapshot)) { ++Revision; Changed.Broadcast(Snapshot, Revision); }
}

int32 UHansaBuildMenuPresentationModel::GetPlacedBuildingCount() const
{
	const UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	return Host != nullptr ? Host->GetPlacedBuildingCount() : 0;
}

int64 UHansaBuildMenuPresentationModel::GetSimulationTick() const
{
	const UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	return Host != nullptr ? Host->GetSimulationTick() : 0;
}

FString UHansaBuildMenuPresentationModel::GetBuildingWorldStatus(const int64 BuildingValue) const
{
	const UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	return Host != nullptr ? Host->GetBuildingWorldStatus(BuildingValue) : FString();
}

bool UHansaBuildMenuPresentationModel::AdvanceSimulationTicks(const int32 TickCount)
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr || !Host->AdvanceTicks(TickCount)) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	RefreshAvailability();
	PublishIfChanged(Previous);
	return true;
}

UHansaRuntimeSimulationHost* UHansaBuildMenuPresentationModel::GetSimulationHost() const
{
	return Runtime.IsValid() ? Runtime->Host.Get() : nullptr;
}

#undef LOCTEXT_NAMESPACE

void UHansaBuildMenuPresentationModel::SetConstructionAllowed(bool Allowed)
{
    if(bConstructionAllowed==Allowed)return;
    if(!Allowed){CancelIntent();SetOpen(false);}
    bConstructionAllowed=Allowed;
}
