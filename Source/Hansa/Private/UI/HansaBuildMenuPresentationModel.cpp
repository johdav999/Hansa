#include "UI/HansaBuildMenuPresentationModel.h"

#include "Commands/HansaGameplayCommandGateway.h"
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
	FHansaBuildCardPresentation Card(
		const TCHAR* Id, const TCHAR* Name, const EHansaBuildCategory Category, const TCHAR* Tier,
		const TCHAR* Cost, const TCHAR* Workforce, const TCHAR* Footprint, const TCHAR* Flow,
		const bool bLocked = false, const TCHAR* LockedReason = TEXT(""))
	{
		FHansaBuildCardPresentation Result;
		Result.StableId = FName(Id); Result.Name = FText::FromString(Name); Result.Category = Category;
		Result.Tier = FText::FromString(Tier); Result.Cost = FText::FromString(Cost);
		Result.WorkforceAndUpkeep = FText::FromString(Workforce); Result.Footprint = FText::FromString(Footprint);
		Result.InputOutput = FText::FromString(Flow); Result.bLocked = bLocked;
		Result.LockedReason = FText::FromString(LockedReason);
		return Result;
	}

	FText FailureCause(const EHansaPlacementFailure Failure)
	{
		switch (Failure)
		{
		case EHansaPlacementFailure::RoadRequired: return LOCTEXT("RoadRequired", "✕ Road required");
		case EHansaPlacementFailure::ShorelineRequired: return LOCTEXT("ShoreRequired", "✕ Land and water access required");
		case EHansaPlacementFailure::Occupied: return LOCTEXT("Occupied", "✕ Footprint occupied");
		case EHansaPlacementFailure::OutsideBounds: return LOCTEXT("Outside", "✕ Outside buildable area");
		case EHansaPlacementFailure::TerrainNotBuildable: return LOCTEXT("Terrain", "✕ Terrain not buildable");
		case EHansaPlacementFailure::None: return LOCTEXT("Valid", "✓ Valid placement");
		default: return FText::FromString(FString::Printf(TEXT("✕ %s"), LexToString(Failure)));
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
		Left.bLocked == Right.bLocked && Left.bFavorite == Right.bFavorite;
}

bool operator==(const FHansaBuildMenuSnapshot& Left, const FHansaBuildMenuSnapshot& Right)
{
	return Left.Cards == Right.Cards && Left.SelectedCategory == Right.SelectedCategory &&
		Left.SelectedBuildingId == Right.SelectedBuildingId && Left.FocusedSemanticId == Right.FocusedSemanticId &&
		Left.ValidationCause.EqualTo(Right.ValidationCause) && Left.ValidationRemedy.EqualTo(Right.ValidationRemedy) &&
		Left.PlacementSummary.EqualTo(Right.PlacementSummary) && Left.LastResult.EqualTo(Right.LastResult) &&
		Left.Feedback == Right.Feedback && Left.RotationQuarterTurns == Right.RotationQuarterTurns &&
		Left.bOpen == Right.bOpen && Left.bHasTarget == Right.bHasTarget && Left.bCanConfirm == Right.bCanConfirm &&
		Left.bRepeat == Right.bRepeat && Left.bGridOverlay == Right.bGridOverlay && Left.bRoadOverlay == Right.bRoadOverlay;
}

UHansaBuildMenuPresentationModel::~UHansaBuildMenuPresentationModel() = default;

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
	NewSnapshot.Cards = {
		Card(TEXT("Building.Road"), TEXT("Road"), EHansaBuildCategory::Roads, TEXT("Civic"), TEXT("25 pf"), TEXT("No workforce · 0 upkeep"), TEXT("1 × 1"), TEXT("Road connection")),
		Card(TEXT("Building.Residence.Laborer"), TEXT("Laborer residence"), EHansaBuildCategory::Residences, TEXT("Laborer"), TEXT("700 pf"), TEXT("Provides workforce · 4 pf upkeep"), TEXT("2 × 2"), TEXT("Bread + fish + beer → laborers")),
		Card(TEXT("Building.Residence.Artisan"), TEXT("Artisan residence"), EHansaBuildCategory::Residences, TEXT("Artisan"), TEXT("1,400 pf"), TEXT("Provides skilled workforce · 8 pf upkeep"), TEXT("2 × 2"), TEXT("Bread + beer + tools → artisans"), true, TEXT("Upgrade a laborer residence first")),
		Card(TEXT("Building.GrainFarm"), TEXT("Grain farm"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("1,100 pf"), TEXT("8 laborers · 5 pf upkeep"), TEXT("4 × 4"), TEXT("Fields → grain")),
		Card(TEXT("Building.Mill"), TEXT("Mill"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("1,800 pf"), TEXT("12 laborers · 8 pf upkeep"), TEXT("3 × 3"), TEXT("Grain → flour")),
		Card(TEXT("Building.Bakery"), TEXT("Bakery"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("2,200 pf"), TEXT("15 laborers · 10 pf upkeep"), TEXT("3 × 2"), TEXT("Flour → bread")),
		Card(TEXT("Building.Fishery"), TEXT("Fishery"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("1,900 pf"), TEXT("10 laborers · 8 pf upkeep"), TEXT("3 × 3"), TEXT("Coast → fish")),
		Card(TEXT("Building.LumberCamp"), TEXT("Lumber camp"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("1,300 pf"), TEXT("8 laborers · 6 pf upkeep"), TEXT("3 × 3"), TEXT("Forest → timber")),
		Card(TEXT("Building.Sawmill"), TEXT("Sawmill"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("1,700 pf"), TEXT("10 laborers · 7 pf upkeep"), TEXT("3 × 3"), TEXT("Timber → planks")),
		Card(TEXT("Building.Smithy"), TEXT("Smithy"), EHansaBuildCategory::Production, TEXT("Artisan"), TEXT("2,600 pf"), TEXT("18 laborers · 12 pf upkeep"), TEXT("3 × 3"), TEXT("Iron → tools")),
		Card(TEXT("Building.Brewery"), TEXT("Brewery"), EHansaBuildCategory::Production, TEXT("Laborer"), TEXT("2,400 pf"), TEXT("16 laborers · 11 pf upkeep"), TEXT("3 × 3"), TEXT("Grain → beer")),
		Card(TEXT("Building.Market"), TEXT("Market"), EHansaBuildCategory::Storage, TEXT("Civic"), TEXT("2,100 pf"), TEXT("10 laborers · 9 pf upkeep"), TEXT("3 × 3"), TEXT("Storage → local service")),
		Card(TEXT("Building.Warehouse"), TEXT("Warehouse"), EHansaBuildCategory::Storage, TEXT("Civic"), TEXT("2,500 pf"), TEXT("12 laborers · 10 pf upkeep"), TEXT("2 × 3"), TEXT("Cart input ↔ city storage"), false, TEXT("")),
		Card(TEXT("Building.Dock"), TEXT("Dock"), EHansaBuildCategory::Harbor, TEXT("Harbor"), TEXT("4,800 pf"), TEXT("20 laborers · 18 pf upkeep"), TEXT("4 × 3"), TEXT("Ships ↔ harbor storage")),
		Card(TEXT("BuildMenu.Civic.Placeholder"), TEXT("Guild hall"), EHansaBuildCategory::Civic, TEXT("Civic"), TEXT("—"), TEXT("—"), TEXT("3 × 3"), TEXT("Civic services"), true, TEXT("Requires civic research")),
		Card(TEXT("BuildMenu.Decoration.Placeholder"), TEXT("Decoration"), EHansaBuildCategory::Decoration, TEXT("Optional"), TEXT("—"), TEXT("No workforce"), TEXT("1 × 1"), TEXT("Visual amenity"), true, TEXT("Decoration is outside the MVP"))
	};
	Snapshot = MoveTemp(NewSnapshot);
	++Revision; Changed.Broadcast(Snapshot, Revision);
	return true;
}

void UHansaBuildMenuPresentationModel::SetOpen(const bool bOpen)
{
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.bOpen = bOpen;
	if (!bOpen && GetSimulationHost() != nullptr)
	{
		Runtime->Placement.Cancel();
		Snapshot.SelectedBuildingId = NAME_None;
		Snapshot.bHasTarget = false;
		Snapshot.bCanConfirm = false;
		Snapshot.Feedback = EHansaPlacementFeedback::None;
		Snapshot.ValidationCause = FText::GetEmpty();
		Snapshot.ValidationRemedy = FText::GetEmpty();
	}
	PublishIfChanged(Previous);
}

bool UHansaBuildMenuPresentationModel::SelectCategory(const EHansaBuildCategory Category)
{
	if (GetSimulationHost() == nullptr) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.SelectedCategory = Category; Snapshot.SelectedBuildingId = NAME_None;
	Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	Runtime->Placement.Cancel(); PublishIfChanged(Previous); return true;
}

const FHansaBuildCardPresentation* UHansaBuildMenuPresentationModel::FindCard(const FName BuildingId) const
{
	return Snapshot.Cards.FindByPredicate([BuildingId](const FHansaBuildCardPresentation& Card) { return Card.StableId == BuildingId; });
}

bool UHansaBuildMenuPresentationModel::SelectBuilding(const FName BuildingId)
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return false;
	const FHansaBuildCardPresentation* Selected = FindCard(BuildingId);
	if (Selected == nullptr || Selected->bLocked) return false;
	const auto DefinitionId = FHansaBuildingTypeId::TryParse(BuildingId.ToString());
	if (!DefinitionId) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Snapshot.SelectedCategory = Selected->Category; Snapshot.SelectedBuildingId = BuildingId;
	Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.ValidationCause = FText::GetEmpty(); Snapshot.ValidationRemedy = LOCTEXT("ChooseTarget", "Choose a target cell. Click placement is available; dragging is not required.");
	Runtime->Placement.SelectBuilding(Host->GetCityId(), DefinitionId.Value, BuildingId == TEXT("Building.Road"), Snapshot.bRepeat);
	PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::TargetGridCell(const int32 X, const int32 Y)
{
	if (GetSimulationHost() == nullptr || Snapshot.SelectedBuildingId.IsNone()) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Runtime->Placement.SetAnchor({ X, Y }); Snapshot.bHasTarget = true;
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

bool UHansaBuildMenuPresentationModel::ConfirmIntent()
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return false;
	RefreshValidation();
	const TArray<FHansaPlacementSpec> Specs = Runtime->Placement.BuildConfirmationSpecs();
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
	Snapshot.LastResult = FText::Format(LOCTEXT("PlacedResult", "✓ Construction committed · {0}"), FText::FromName(Snapshot.SelectedBuildingId));
	Runtime->Placement.OnConfirmationSucceeded(); Snapshot.bHasTarget = false; Snapshot.bCanConfirm = false;
	Snapshot.Feedback = EHansaPlacementFeedback::None; Snapshot.ValidationCause = FText::GetEmpty(); Snapshot.ValidationRemedy = FText::GetEmpty();
	if (!Snapshot.bRepeat) Snapshot.SelectedBuildingId = NAME_None;
	PublishIfChanged(Previous); return true;
}

bool UHansaBuildMenuPresentationModel::CancelIntent()
{
	if (GetSimulationHost() == nullptr) return false;
	const FHansaBuildMenuSnapshot Previous = Snapshot;
	Runtime->Placement.Cancel(); Snapshot.SelectedBuildingId = NAME_None; Snapshot.bHasTarget = false;
	Snapshot.bCanConfirm = false; Snapshot.Feedback = EHansaPlacementFeedback::None;
	Snapshot.ValidationCause = FText::GetEmpty(); Snapshot.ValidationRemedy = FText::GetEmpty();
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
	const TArray<FHansaPlacementSpec> Specs = Runtime->Placement.BuildConfirmationSpecs();
	if (Specs.IsEmpty()) return;
	const FHansaPlacementValidationResult Validation = Host->ValidatePlacement(Specs[0]);
	const EHansaPlacementFailure Failure = Validation.GetPrimaryFailure();
	Snapshot.bCanConfirm = Validation.CanPlace();
	Snapshot.Feedback = Snapshot.bCanConfirm ? EHansaPlacementFeedback::Valid : EHansaPlacementFeedback::Invalid;
	Snapshot.ValidationCause = FailureCause(Failure); Snapshot.ValidationRemedy = FailureRemedy(Failure);
	Snapshot.PlacementSummary = FText::Format(LOCTEXT("PlacementSummary", "Footprint target · {0} · rotation {1}°"),
		FText::FromName(Snapshot.SelectedBuildingId), FText::AsNumber(Snapshot.RotationQuarterTurns * 90));
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
	return Host != nullptr && Host->AdvanceTicks(TickCount);
}

UHansaRuntimeSimulationHost* UHansaBuildMenuPresentationModel::GetSimulationHost() const
{
	return Runtime.IsValid() ? Runtime->Host.Get() : nullptr;
}

#undef LOCTEXT_NAMESPACE
