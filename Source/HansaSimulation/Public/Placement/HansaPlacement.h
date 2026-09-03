#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Model/HansaIds.h"
#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	struct FHansaCompiledBuildingDefinition;
	class FHansaEconomicRegistry;
	class FHansaSimulationPipeline;
	class FHansaStateHasher;

	struct HANSASIMULATION_API FHansaGridCoordinate final
	{
		int32 X = 0;
		int32 Y = 0;

		friend bool operator==(const FHansaGridCoordinate& Left, const FHansaGridCoordinate& Right)
		{
			return Left.X == Right.X && Left.Y == Right.Y;
		}

		friend bool operator!=(const FHansaGridCoordinate& Left, const FHansaGridCoordinate& Right)
		{
			return !(Left == Right);
		}

		friend bool operator<(const FHansaGridCoordinate& Left, const FHansaGridCoordinate& Right)
		{
			return Left.X != Right.X ? Left.X < Right.X : Left.Y < Right.Y;
		}
	};

	enum class EHansaGridRotation : uint8
	{
		North = 0,
		East,
		South,
		West
	};

	enum class EHansaPlacementTerrain : uint8
	{
		Land = 0,
		Shore,
		Water
	};

	enum class EHansaPlacementFailure : uint8
	{
		None = 0,
		InvalidRequest,
		UnknownCity,
		UnknownBuildingDefinition,
		MissingPrerequisite,
		OutsideBounds,
		CellUnavailable,
		WrongOwner,
		TerrainNotBuildable,
		CellBlocked,
		Occupied,
		ShorelineRequired,
		RoadRequired
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaGridRotation Rotation);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaPlacementTerrain Terrain);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaPlacementFailure Failure);

	struct FHansaPlacementGridCell final
	{
		FHansaGridCoordinate Coordinate;
		EHansaPlacementTerrain Terrain = EHansaPlacementTerrain::Land;
		FHansaHouseId OwnerId;
		bool bBlocked = false;
	};

	struct FHansaPlacementMapInitialization final
	{
		FHansaCityDefinitionId CityId;
		FHansaGridCoordinate BoundsMin;
		FHansaGridCoordinate BoundsMax;
		FHansaBuildingTypeId RoadBuildingDefinitionId;
		TArray<FHansaPlacementGridCell> Cells;
	};

	/** Per-house unlock supplied by scenario/research state; absence is a deterministic prerequisite failure. */
	struct FHansaPlacementEntitlement final
	{
		FHansaHouseId HouseId;
		FHansaBuildingTypeId BuildingDefinitionId;
	};

	struct FHansaPlacementSpec final
	{
		FHansaCityDefinitionId CityId;
		FHansaBuildingTypeId BuildingDefinitionId;
		FHansaGridCoordinate Anchor;
		EHansaGridRotation Rotation = EHansaGridRotation::North;
	};

	/** Authoritative placement record. Occupied cells are stored canonically for save validation and definition-independent hashing. */
	struct FHansaPlacedBuildingRecord final
	{
		FHansaBuildingId BuildingId;
		FHansaHouseId OwnerId;
		FHansaPlacementSpec Spec;
		TArray<FHansaGridCoordinate> OccupiedCells;
	};

	struct FHansaPlacementInitialization final
	{
		TArray<FHansaPlacementMapInitialization> Maps;
		TArray<FHansaPlacementEntitlement> Entitlements;
		TArray<FHansaPlacedBuildingRecord> Placements;
	};

	struct FHansaPlacementValidationReason final
	{
		EHansaPlacementFailure Failure = EHansaPlacementFailure::None;
		FHansaGridCoordinate Cell;
		FName MessageKey;
		FName RemedyKey;
	};

	class HANSASIMULATION_API FHansaPlacementValidationResult final
	{
	public:
		[[nodiscard]] bool CanPlace() const { return Reasons.IsEmpty(); }
		explicit operator bool() const { return CanPlace(); }
		[[nodiscard]] EHansaPlacementFailure GetPrimaryFailure() const
		{
			return Reasons.IsEmpty() ? EHansaPlacementFailure::None : Reasons[0].Failure;
		}
		[[nodiscard]] TConstArrayView<FHansaPlacementValidationReason> GetReasons() const { return Reasons; }
		[[nodiscard]] TConstArrayView<FHansaGridCoordinate> GetOccupiedCells() const { return OccupiedCells; }

	private:
		friend class FHansaPlacementRules;

		TArray<FHansaPlacementValidationReason> Reasons;
		TArray<FHansaGridCoordinate> OccupiedCells;
	};

	/** Canonical authoritative map/occupancy records. Mutation is private to the command pipeline. */
	class HANSASIMULATION_API FHansaPlacementState final
	{
	public:
		static THansaValueResult<FHansaPlacementState> TryCreate(FHansaPlacementInitialization Initialization);

		[[nodiscard]] TConstArrayView<FHansaPlacementMapInitialization> GetMaps() const { return Maps; }
		[[nodiscard]] TConstArrayView<FHansaPlacementEntitlement> GetEntitlements() const { return Entitlements; }
		[[nodiscard]] TConstArrayView<FHansaPlacedBuildingRecord> GetPlacements() const { return Placements; }
		[[nodiscard]] const FHansaPlacementMapInitialization* FindMap(FHansaCityDefinitionId CityId) const;
		[[nodiscard]] const FHansaPlacementGridCell* FindCell(
			FHansaCityDefinitionId CityId,
			FHansaGridCoordinate Coordinate) const;
		[[nodiscard]] const FHansaPlacedBuildingRecord* FindPlacement(FHansaBuildingId BuildingId) const;

	private:
		friend class FHansaPlacementRules;
		friend class FHansaSimulationPipeline;
		friend class FHansaStateHasher;

		TArray<FHansaPlacementMapInitialization> Maps;
		TArray<FHansaPlacementEntitlement> Entitlements;
		TArray<FHansaPlacedBuildingRecord> Placements;
	};

	class HANSASIMULATION_API FHansaPlacementRules final
	{
	public:
		[[nodiscard]] static FHansaPlacementValidationResult Validate(
			const FHansaPlacementState& State,
			const FHansaEconomicRegistry& Definitions,
			FHansaHouseId IssuingHouseId,
			const FHansaPlacementSpec& Spec);

	private:
		friend class FHansaSimulationPipeline;

		static void AddReason(
			FHansaPlacementValidationResult& Result,
			EHansaPlacementFailure Failure,
			FHansaGridCoordinate Cell);
		static void ApplyValidated(
			FHansaPlacementState& State,
			FHansaBuildingId BuildingId,
			FHansaHouseId OwnerId,
			const FHansaPlacementSpec& Spec,
			TConstArrayView<FHansaGridCoordinate> OccupiedCells);
		static bool Remove(FHansaPlacementState& State, FHansaBuildingId BuildingId);
	};

	/** Presentation-only placement lifecycle. It never owns occupancy or mutates simulation state. */
	class HANSASIMULATION_API FHansaPlacementSession final
	{
	public:
		void SelectBuilding(
			FHansaCityDefinitionId CityId,
			FHansaBuildingTypeId BuildingDefinitionId,
			bool bRoad,
			bool bRepeatAfterConfirmation = true);
		void SetAnchor(FHansaGridCoordinate Anchor);
		void RotateClockwise();
		void BeginRoadDrag(FHansaGridCoordinate Start);
		void UpdateRoadDrag(FHansaGridCoordinate End);
		void Cancel();
		void OnConfirmationSucceeded();

		[[nodiscard]] bool IsActive() const { return bActive; }
		[[nodiscard]] bool IsRoadDragActive() const { return bRoad && bHasDragStart; }
		[[nodiscard]] EHansaGridRotation GetRotation() const { return Rotation; }
		[[nodiscard]] TArray<FHansaPlacementSpec> BuildConfirmationSpecs() const;

	private:
		bool bActive = false;
		bool bRoad = false;
		bool bRepeat = true;
		bool bHasAnchor = false;
		bool bHasDragStart = false;
		FHansaCityDefinitionId CityId;
		FHansaBuildingTypeId BuildingDefinitionId;
		FHansaGridCoordinate Anchor;
		FHansaGridCoordinate DragStart;
		EHansaGridRotation Rotation = EHansaGridRotation::North;
	};

	class HANSASIMULATION_API FHansaRoadDragBuilder final
	{
	public:
		/** Inclusive Manhattan path with a stable horizontal-then-vertical bend. */
		[[nodiscard]] static TArray<FHansaGridCoordinate> BuildPath(
			FHansaGridCoordinate Start,
			FHansaGridCoordinate End);
	};
}
