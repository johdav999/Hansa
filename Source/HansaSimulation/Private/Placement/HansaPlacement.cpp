#include "Placement/HansaPlacement.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint64 TopologyFnvOffset = 14695981039346656037ULL;
		constexpr uint64 TopologyFnvPrime = 1099511628211ULL;

		void AddTopologyByte(uint64& Hash, const uint8 Value)
		{
			Hash ^= Value;
			Hash *= TopologyFnvPrime;
		}

		void AddTopologyUInt32(uint64& Hash, const uint32 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
			{
				AddTopologyByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddTopologyUInt64(uint64& Hash, const uint64 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				AddTopologyByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddTopologyString(uint64& Hash, const FString& Value)
		{
			AddTopologyUInt32(Hash, static_cast<uint32>(Value.Len()));
			for (const TCHAR Character : Value)
			{
				AddTopologyByte(Hash, static_cast<uint8>(Character));
			}
		}

		bool IsKnownRotation(const EHansaGridRotation Rotation)
		{
			return Rotation == EHansaGridRotation::North || Rotation == EHansaGridRotation::East ||
				Rotation == EHansaGridRotation::South || Rotation == EHansaGridRotation::West;
		}

		bool IsKnownTerrain(const EHansaPlacementTerrain Terrain)
		{
			return Terrain == EHansaPlacementTerrain::Land || Terrain == EHansaPlacementTerrain::Shore ||
				Terrain == EHansaPlacementTerrain::Water;
		}

		bool IsInside(const FHansaPlacementMapInitialization& Map, const FHansaGridCoordinate Cell)
		{
			return Cell.X >= Map.BoundsMin.X && Cell.X <= Map.BoundsMax.X &&
				Cell.Y >= Map.BoundsMin.Y && Cell.Y <= Map.BoundsMax.Y;
		}

		bool IsAdjacent(const FHansaGridCoordinate Left, const FHansaGridCoordinate Right)
		{
			return FMath::Abs(Left.X - Right.X) + FMath::Abs(Left.Y - Right.Y) == 1;
		}

		FName MessageKey(const EHansaPlacementFailure Failure)
		{
			return FName(*FString::Printf(TEXT("Placement.Validation.%s"), LexToString(Failure)));
		}

		FName RemedyKey(const EHansaPlacementFailure Failure)
		{
			return FName(*FString::Printf(TEXT("Placement.Remedy.%s"), LexToString(Failure)));
		}

	}

	const TCHAR* LexToString(const EHansaGridRotation Rotation)
	{
		switch (Rotation)
		{
		case EHansaGridRotation::North: return TEXT("North");
		case EHansaGridRotation::East: return TEXT("East");
		case EHansaGridRotation::South: return TEXT("South");
		case EHansaGridRotation::West: return TEXT("West");
		default: return TEXT("UnknownRotation");
		}
	}

	const TCHAR* LexToString(const EHansaPlacementTerrain Terrain)
	{
		switch (Terrain)
		{
		case EHansaPlacementTerrain::Land: return TEXT("Land");
		case EHansaPlacementTerrain::Shore: return TEXT("Shore");
		case EHansaPlacementTerrain::Water: return TEXT("Water");
		default: return TEXT("UnknownTerrain");
		}
	}

	const TCHAR* LexToString(const EHansaPlacementFailure Failure)
	{
		switch (Failure)
		{
		case EHansaPlacementFailure::None: return TEXT("None");
		case EHansaPlacementFailure::InvalidRequest: return TEXT("InvalidRequest");
		case EHansaPlacementFailure::UnknownCity: return TEXT("UnknownCity");
		case EHansaPlacementFailure::UnknownBuildingDefinition: return TEXT("UnknownBuildingDefinition");
		case EHansaPlacementFailure::MissingPrerequisite: return TEXT("MissingPrerequisite");
		case EHansaPlacementFailure::OutsideBounds: return TEXT("OutsideBounds");
		case EHansaPlacementFailure::CellUnavailable: return TEXT("CellUnavailable");
		case EHansaPlacementFailure::WrongOwner: return TEXT("WrongOwner");
		case EHansaPlacementFailure::TerrainNotBuildable: return TEXT("TerrainNotBuildable");
		case EHansaPlacementFailure::CellBlocked: return TEXT("CellBlocked");
		case EHansaPlacementFailure::Occupied: return TEXT("Occupied");
		case EHansaPlacementFailure::ShorelineRequired: return TEXT("ShorelineRequired");
		case EHansaPlacementFailure::RoadRequired: return TEXT("RoadRequired");
		default: return TEXT("UnknownPlacementFailure");
		}
	}

	FHansaPlacementTopology::FHansaPlacementTopology()
	{
		TopologyHash = TopologyFnvOffset;
		AddTopologyUInt32(TopologyHash, 1);
		AddTopologyUInt32(TopologyHash, 0);
	}

	THansaValueResult<FHansaPlacementTopology> FHansaPlacementTopology::TryCreate(
		TArray<FHansaPlacementMapInitialization> InMaps)
	{
		for (FHansaPlacementMapInitialization& Map : InMaps)
		{
			Map.Cells.Sort([](const FHansaPlacementGridCell& Left, const FHansaPlacementGridCell& Right)
			{
				return Left.Coordinate < Right.Coordinate;
			});
		}
		InMaps.Sort([](const FHansaPlacementMapInitialization& Left,
			const FHansaPlacementMapInitialization& Right)
		{
			return Left.CityId < Right.CityId;
		});
		uint64 RecordCount = static_cast<uint64>(InMaps.Num());
		for (int32 MapIndex = 0; MapIndex < InMaps.Num(); ++MapIndex)
		{
			const FHansaPlacementMapInitialization& Map = InMaps[MapIndex];
			if (!Map.CityId.IsValid() || !Map.RoadBuildingDefinitionId.IsValid() ||
				Map.BoundsMin.X > Map.BoundsMax.X || Map.BoundsMin.Y > Map.BoundsMax.Y ||
				(MapIndex > 0 && InMaps[MapIndex - 1].CityId == Map.CityId))
			{
				return THansaValueResult<FHansaPlacementTopology>::Failure(EHansaValueError::InvalidFormat);
			}
			RecordCount += static_cast<uint64>(Map.Cells.Num());
			if (RecordCount > MAX_uint32)
			{
				return THansaValueResult<FHansaPlacementTopology>::Failure(EHansaValueError::OutOfRange);
			}
			for (int32 CellIndex = 0; CellIndex < Map.Cells.Num(); ++CellIndex)
			{
				const FHansaPlacementGridCell& Cell = Map.Cells[CellIndex];
				if (!Cell.OwnerId.IsValid() || !IsKnownTerrain(Cell.Terrain) || !IsInside(Map, Cell.Coordinate) ||
					(CellIndex > 0 && Map.Cells[CellIndex - 1].Coordinate == Cell.Coordinate))
				{
					return THansaValueResult<FHansaPlacementTopology>::Failure(EHansaValueError::InvalidFormat);
				}
			}
		}

		FHansaPlacementTopology Topology;
		Topology.Maps = MoveTemp(InMaps);
		Topology.RecordCount = static_cast<uint32>(RecordCount);
		Topology.TopologyHash = TopologyFnvOffset;
		AddTopologyUInt32(Topology.TopologyHash, 1);
		AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Topology.Maps.Num()));
		for (const FHansaPlacementMapInitialization& Map : Topology.Maps)
		{
			AddTopologyString(Topology.TopologyHash, Map.CityId.ToString());
			AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Map.BoundsMin.X));
			AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Map.BoundsMin.Y));
			AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Map.BoundsMax.X));
			AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Map.BoundsMax.Y));
			AddTopologyString(Topology.TopologyHash, Map.RoadBuildingDefinitionId.ToString());
			AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Map.Cells.Num()));
			for (const FHansaPlacementGridCell& Cell : Map.Cells)
			{
				AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Cell.Coordinate.X));
				AddTopologyUInt32(Topology.TopologyHash, static_cast<uint32>(Cell.Coordinate.Y));
				AddTopologyByte(Topology.TopologyHash, static_cast<uint8>(Cell.Terrain));
				AddTopologyUInt64(Topology.TopologyHash, Cell.OwnerId.GetValue());
				AddTopologyUInt32(Topology.TopologyHash, Cell.OwnerId.GetGeneration());
				AddTopologyByte(Topology.TopologyHash, Cell.bBlocked ? 1 : 0);
			}
		}
		return THansaValueResult<FHansaPlacementTopology>::Success(MoveTemp(Topology));
	}

	const FHansaPlacementMapInitialization* FHansaPlacementTopology::FindMap(
		const FHansaCityDefinitionId CityId) const
	{
		return Maps.FindByPredicate([CityId](const FHansaPlacementMapInitialization& Map)
		{
			return Map.CityId == CityId;
		});
	}

	const FHansaPlacementGridCell* FHansaPlacementTopology::FindCell(
		const FHansaCityDefinitionId CityId,
		const FHansaGridCoordinate Coordinate) const
	{
		const FHansaPlacementMapInitialization* Map = FindMap(CityId);
		if (!Map) return nullptr;
		int32 First = 0;
		int32 Last = Map->Cells.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (Map->Cells[Middle].Coordinate < Coordinate) First = Middle + 1;
			else Last = Middle;
		}
		return Map->Cells.IsValidIndex(First) && Map->Cells[First].Coordinate == Coordinate
			? &Map->Cells[First] : nullptr;
	}

	THansaValueResult<FHansaPlacementState> FHansaPlacementState::TryCreate(
		FHansaPlacementInitialization Initialization,
		TSharedPtr<const FHansaPlacementTopology> ImmutableTopology)
	{
		if (!Initialization.Maps.IsEmpty())
		{
			THansaValueResult<FHansaPlacementTopology> SuppliedTopology =
				FHansaPlacementTopology::TryCreate(MoveTemp(Initialization.Maps));
			if (!SuppliedTopology)
			{
				return THansaValueResult<FHansaPlacementState>::Failure(SuppliedTopology.Error);
			}
			if (ImmutableTopology.IsValid() &&
				ImmutableTopology->GetTopologyHash() != SuppliedTopology.Value.GetTopologyHash())
			{
				return THansaValueResult<FHansaPlacementState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ImmutableTopology.IsValid())
			{
				ImmutableTopology = MakeShared<FHansaPlacementTopology>(MoveTemp(SuppliedTopology.Value));
			}
		}
		Initialization.Entitlements.Sort([](const FHansaPlacementEntitlement& Left,
			const FHansaPlacementEntitlement& Right)
		{
			return Left.HouseId != Right.HouseId ? Left.HouseId < Right.HouseId :
				Left.BuildingDefinitionId < Right.BuildingDefinitionId;
		});
		for (FHansaPlacedBuildingRecord& Placement : Initialization.Placements)
		{
			Placement.OccupiedCells.Sort();
		}
		Initialization.Placements.Sort([](const FHansaPlacedBuildingRecord& Left,
			const FHansaPlacedBuildingRecord& Right)
		{
			return Left.BuildingId < Right.BuildingId;
		});

		for (int32 Index = 0; Index < Initialization.Entitlements.Num(); ++Index)
		{
			const FHansaPlacementEntitlement& Entitlement = Initialization.Entitlements[Index];
			if (!Entitlement.HouseId.IsValid() || !Entitlement.BuildingDefinitionId.IsValid() ||
				(Index > 0 && Initialization.Entitlements[Index - 1].HouseId == Entitlement.HouseId &&
					Initialization.Entitlements[Index - 1].BuildingDefinitionId == Entitlement.BuildingDefinitionId))
			{
				return THansaValueResult<FHansaPlacementState>::Failure(EHansaValueError::InvalidFormat);
			}
		}

		FHansaPlacementState State;
		State.Topology = MoveTemp(ImmutableTopology);
		State.Entitlements = MoveTemp(Initialization.Entitlements);
		for (int32 PlacementIndex = 0; PlacementIndex < Initialization.Placements.Num(); ++PlacementIndex)
		{
			const FHansaPlacedBuildingRecord& Placement = Initialization.Placements[PlacementIndex];
			if (!Placement.BuildingId.IsValid() || !Placement.OwnerId.IsValid() ||
				!Placement.Spec.CityId.IsValid() || !Placement.Spec.BuildingDefinitionId.IsValid() ||
				!IsKnownRotation(Placement.Spec.Rotation) || Placement.OccupiedCells.IsEmpty() ||
				(PlacementIndex > 0 && Initialization.Placements[PlacementIndex - 1].BuildingId == Placement.BuildingId))
			{
				return THansaValueResult<FHansaPlacementState>::Failure(EHansaValueError::InvalidFormat);
			}
			const FHansaPlacementMapInitialization* Map = State.FindMap(Placement.Spec.CityId);
			if (Map == nullptr)
			{
				return THansaValueResult<FHansaPlacementState>::Failure(EHansaValueError::InvalidFormat);
			}
			for (int32 CellIndex = 0; CellIndex < Placement.OccupiedCells.Num(); ++CellIndex)
			{
				const FHansaGridCoordinate CellCoordinate = Placement.OccupiedCells[CellIndex];
				const FHansaPlacementGridCell* Cell = State.FindCell(Placement.Spec.CityId, CellCoordinate);
				if (Cell == nullptr || Cell->OwnerId != Placement.OwnerId || Cell->bBlocked ||
					Cell->Terrain == EHansaPlacementTerrain::Water ||
					(CellIndex > 0 && Placement.OccupiedCells[CellIndex - 1] == CellCoordinate))
				{
					return THansaValueResult<FHansaPlacementState>::Failure(EHansaValueError::InvalidFormat);
				}
				for (const FHansaPlacedBuildingRecord& Existing : State.Placements)
				{
					if (Existing.Spec.CityId == Placement.Spec.CityId && Existing.OccupiedCells.Contains(CellCoordinate))
					{
						return THansaValueResult<FHansaPlacementState>::Failure(EHansaValueError::InvalidFormat);
					}
				}
			}
			State.Placements.Add(Placement);
		}
		return THansaValueResult<FHansaPlacementState>::Success(MoveTemp(State));
	}

	const FHansaPlacementMapInitialization* FHansaPlacementState::FindMap(const FHansaCityDefinitionId CityId) const
	{
		return Topology.IsValid() ? Topology->FindMap(CityId) : nullptr;
	}

	const FHansaPlacementGridCell* FHansaPlacementState::FindCell(
		const FHansaCityDefinitionId CityId,
		const FHansaGridCoordinate Coordinate) const
	{
		return Topology.IsValid() ? Topology->FindCell(CityId, Coordinate) : nullptr;
	}

	const FHansaPlacedBuildingRecord* FHansaPlacementState::FindPlacement(const FHansaBuildingId BuildingId) const
	{
		return Placements.FindByPredicate([BuildingId](const FHansaPlacedBuildingRecord& Placement)
		{
			return Placement.BuildingId == BuildingId;
		});
	}

	FHansaPlacementValidationResult FHansaPlacementRules::Validate(
		const FHansaPlacementState& State,
		const FHansaEconomicRegistry& Definitions,
		const FHansaHouseId IssuingHouseId,
		const FHansaPlacementSpec& Spec)
	{
		FHansaPlacementValidationResult Result;
		if (!IssuingHouseId.IsValid() || !Spec.CityId.IsValid() || !Spec.BuildingDefinitionId.IsValid() ||
			!IsKnownRotation(Spec.Rotation))
		{
			AddReason(Result, EHansaPlacementFailure::InvalidRequest, Spec.Anchor);
			return Result;
		}

		const FHansaPlacementMapInitialization* Map = State.FindMap(Spec.CityId);
		if (Map == nullptr)
		{
			AddReason(Result, EHansaPlacementFailure::UnknownCity, Spec.Anchor);
			return Result;
		}
		const FHansaCompiledBuildingDefinition* Building = Definitions.FindBuilding(Spec.BuildingDefinitionId.ToString());
		if (Building == nullptr || Building->FootprintWidthCells <= 0 || Building->FootprintWidthCells > 64 ||
			Building->FootprintHeightCells <= 0 || Building->FootprintHeightCells > 64)
		{
			AddReason(Result, EHansaPlacementFailure::UnknownBuildingDefinition, Spec.Anchor);
			return Result;
		}
		const bool bEntitled = State.Entitlements.ContainsByPredicate(
			[IssuingHouseId, &Spec](const FHansaPlacementEntitlement& Entitlement)
			{
				return Entitlement.HouseId == IssuingHouseId &&
					Entitlement.BuildingDefinitionId == Spec.BuildingDefinitionId;
			});
		if (!bEntitled)
		{
			AddReason(Result, EHansaPlacementFailure::MissingPrerequisite, Spec.Anchor);
			return Result;
		}

		const bool bSwapDimensions = Spec.Rotation == EHansaGridRotation::East || Spec.Rotation == EHansaGridRotation::West;
		const int32 Width = bSwapDimensions ? Building->FootprintHeightCells : Building->FootprintWidthCells;
		const int32 Height = bSwapDimensions ? Building->FootprintWidthCells : Building->FootprintHeightCells;
		Result.OccupiedCells.Reserve(Width * Height);
		bool bTouchesShoreline = false;
		bool bBridgesLandAndWater = false;
		bool bTouchesRoad = false;

		for (int32 XOffset = 0; XOffset < Width; ++XOffset)
		{
			for (int32 YOffset = 0; YOffset < Height; ++YOffset)
			{
				const int64 CoordinateX = static_cast<int64>(Spec.Anchor.X) + XOffset;
				const int64 CoordinateY = static_cast<int64>(Spec.Anchor.Y) + YOffset;
				if (CoordinateX < TNumericLimits<int32>::Min() || CoordinateX > TNumericLimits<int32>::Max() ||
					CoordinateY < TNumericLimits<int32>::Min() || CoordinateY > TNumericLimits<int32>::Max())
				{
					AddReason(Result, EHansaPlacementFailure::OutsideBounds, Spec.Anchor);
					continue;
				}
				const FHansaGridCoordinate Coordinate {
					static_cast<int32>(CoordinateX),
					static_cast<int32>(CoordinateY)
				};
				Result.OccupiedCells.Add(Coordinate);
				if (!IsInside(*Map, Coordinate))
				{
					AddReason(Result, EHansaPlacementFailure::OutsideBounds, Coordinate);
					continue;
				}
				const FHansaPlacementGridCell* Cell = State.FindCell(Spec.CityId, Coordinate);
				if (Cell == nullptr)
				{
					AddReason(Result, EHansaPlacementFailure::CellUnavailable, Coordinate);
					continue;
				}
				if (Cell->OwnerId != IssuingHouseId)
				{
					AddReason(Result, EHansaPlacementFailure::WrongOwner, Coordinate);
				}
				if (Cell->Terrain == EHansaPlacementTerrain::Water)
				{
					AddReason(Result, EHansaPlacementFailure::TerrainNotBuildable, Coordinate);
				}
				if (Cell->bBlocked)
				{
					AddReason(Result, EHansaPlacementFailure::CellBlocked, Coordinate);
				}
				bTouchesShoreline |= Cell->Terrain == EHansaPlacementTerrain::Shore;
				for (const FHansaPlacedBuildingRecord& Existing : State.Placements)
				{
					if (Existing.Spec.CityId == Spec.CityId && Existing.OccupiedCells.Contains(Coordinate))
					{
						AddReason(Result, EHansaPlacementFailure::Occupied, Coordinate);
						break;
					}
				}
			}
		}
		Result.OccupiedCells.Sort();

		if (Building->bRequiresShoreline && bTouchesShoreline)
		{
			const auto EdgeContainsTerrain = [&State, &Spec](
				const FHansaGridCoordinate Start,
				const FHansaGridCoordinate Step,
				const int32 Length,
				const EHansaPlacementTerrain Terrain)
			{
				for (int32 Index = 0; Index < Length; ++Index)
				{
					const FHansaGridCoordinate Coordinate {
						Start.X + Step.X * Index,
						Start.Y + Step.Y * Index
					};
					const FHansaPlacementGridCell* Cell = State.FindCell(Spec.CityId, Coordinate);
					if (Cell != nullptr && Cell->Terrain == Terrain)
					{
						return true;
					}
				}
				return false;
			};

			const bool bWaterWest = EdgeContainsTerrain(
				{ Spec.Anchor.X - 1, Spec.Anchor.Y }, { 0, 1 }, Height, EHansaPlacementTerrain::Water);
			const bool bWaterEast = EdgeContainsTerrain(
				{ Spec.Anchor.X + Width, Spec.Anchor.Y }, { 0, 1 }, Height, EHansaPlacementTerrain::Water);
			const bool bWaterSouth = EdgeContainsTerrain(
				{ Spec.Anchor.X, Spec.Anchor.Y - 1 }, { 1, 0 }, Width, EHansaPlacementTerrain::Water);
			const bool bWaterNorth = EdgeContainsTerrain(
				{ Spec.Anchor.X, Spec.Anchor.Y + Height }, { 1, 0 }, Width, EHansaPlacementTerrain::Water);
			const bool bLandWest = EdgeContainsTerrain(
				{ Spec.Anchor.X - 1, Spec.Anchor.Y }, { 0, 1 }, Height, EHansaPlacementTerrain::Land);
			const bool bLandEast = EdgeContainsTerrain(
				{ Spec.Anchor.X + Width, Spec.Anchor.Y }, { 0, 1 }, Height, EHansaPlacementTerrain::Land);
			const bool bLandSouth = EdgeContainsTerrain(
				{ Spec.Anchor.X, Spec.Anchor.Y - 1 }, { 1, 0 }, Width, EHansaPlacementTerrain::Land);
			const bool bLandNorth = EdgeContainsTerrain(
				{ Spec.Anchor.X, Spec.Anchor.Y + Height }, { 1, 0 }, Width, EHansaPlacementTerrain::Land);
			bBridgesLandAndWater = (bWaterWest && bLandEast) || (bWaterEast && bLandWest) ||
				(bWaterSouth && bLandNorth) || (bWaterNorth && bLandSouth);
		}
		if (Building->bRequiresShoreline && (!bTouchesShoreline || !bBridgesLandAndWater))
		{
			AddReason(Result, EHansaPlacementFailure::ShorelineRequired, Spec.Anchor);
		}
		if (Building->bRequiresRoad)
		{
			for (const FHansaPlacedBuildingRecord& Existing : State.Placements)
			{
				if (Existing.Spec.CityId != Spec.CityId ||
					Existing.Spec.BuildingDefinitionId != Map->RoadBuildingDefinitionId)
				{
					continue;
				}
				for (const FHansaGridCoordinate Candidate : Result.OccupiedCells)
				{
					for (const FHansaGridCoordinate RoadCell : Existing.OccupiedCells)
					{
						bTouchesRoad |= IsAdjacent(Candidate, RoadCell);
					}
				}
			}
			if (!bTouchesRoad)
			{
				AddReason(Result, EHansaPlacementFailure::RoadRequired, Spec.Anchor);
			}
		}
		return Result;
	}

	void FHansaPlacementRules::AddReason(
		FHansaPlacementValidationResult& Result,
		const EHansaPlacementFailure Failure,
		const FHansaGridCoordinate Cell)
	{
		FHansaPlacementValidationReason Reason;
		Reason.Failure = Failure;
		Reason.Cell = Cell;
		Reason.MessageKey = MessageKey(Failure);
		Reason.RemedyKey = RemedyKey(Failure);
		Result.Reasons.Add(Reason);
	}

	void FHansaPlacementRules::ApplyValidated(
		FHansaPlacementState& State,
		const FHansaBuildingId BuildingId,
		const FHansaHouseId OwnerId,
		const FHansaPlacementSpec& Spec,
		const TConstArrayView<FHansaGridCoordinate> OccupiedCells)
	{
		FHansaPlacedBuildingRecord Record;
		Record.BuildingId = BuildingId;
		Record.OwnerId = OwnerId;
		Record.Spec = Spec;
		Record.OccupiedCells.Append(OccupiedCells);
		Record.OccupiedCells.Sort();
		int32 Index = 0;
		while (Index < State.Placements.Num() && State.Placements[Index].BuildingId < BuildingId)
		{
			++Index;
		}
		State.Placements.Insert(MoveTemp(Record), Index);
	}

	bool FHansaPlacementRules::Remove(FHansaPlacementState& State, const FHansaBuildingId BuildingId)
	{
		const int32 Index = State.Placements.IndexOfByPredicate([BuildingId](const FHansaPlacedBuildingRecord& Placement)
		{
			return Placement.BuildingId == BuildingId;
		});
		if (Index == INDEX_NONE)
		{
			return false;
		}
		State.Placements.RemoveAt(Index);
		return true;
	}

	void FHansaPlacementSession::SelectBuilding(
		const FHansaCityDefinitionId InCityId,
		const FHansaBuildingTypeId InBuildingDefinitionId,
		const bool bInRoad,
		const bool bRepeatAfterConfirmation)
	{
		bActive = InCityId.IsValid() && InBuildingDefinitionId.IsValid();
		CityId = InCityId;
		BuildingDefinitionId = InBuildingDefinitionId;
		bRoad = bInRoad;
		bRepeat = bRepeatAfterConfirmation;
		bHasAnchor = false;
		bHasDragStart = false;
		Rotation = EHansaGridRotation::North;
	}

	void FHansaPlacementSession::SetAnchor(const FHansaGridCoordinate InAnchor)
	{
		Anchor = InAnchor;
		bHasAnchor = bActive;
	}

	void FHansaPlacementSession::RotateClockwise()
	{
		if (bActive && !bRoad)
		{
			Rotation = static_cast<EHansaGridRotation>((static_cast<uint8>(Rotation) + 1) % 4);
		}
	}

	void FHansaPlacementSession::BeginRoadDrag(const FHansaGridCoordinate Start)
	{
		if (bActive && bRoad)
		{
			DragStart = Start;
			Anchor = Start;
			bHasAnchor = true;
			bHasDragStart = true;
		}
	}

	void FHansaPlacementSession::UpdateRoadDrag(const FHansaGridCoordinate End)
	{
		if (bHasDragStart)
		{
			Anchor = End;
			bHasAnchor = true;
		}
	}

	void FHansaPlacementSession::CancelRoadDrag()
	{
		if (bActive && bRoad)
		{
			bHasAnchor = false;
			bHasDragStart = false;
		}
	}

	void FHansaPlacementSession::Cancel()
	{
		*this = FHansaPlacementSession();
	}

	void FHansaPlacementSession::OnConfirmationSucceeded()
	{
		if (!bRepeat)
		{
			Cancel();
			return;
		}
		bHasAnchor = false;
		bHasDragStart = false;
	}

	TArray<FHansaPlacementSpec> FHansaPlacementSession::BuildConfirmationSpecs() const
	{
		TArray<FHansaPlacementSpec> Result;
		if (!bActive || !bHasAnchor)
		{
			return Result;
		}
		const TArray<FHansaGridCoordinate> Coordinates = bRoad && bHasDragStart
			? FHansaRoadDragBuilder::BuildPath(DragStart, Anchor)
			: TArray<FHansaGridCoordinate> { Anchor };
		Result.Reserve(Coordinates.Num());
		for (const FHansaGridCoordinate Coordinate : Coordinates)
		{
			FHansaPlacementSpec Spec;
			Spec.CityId = CityId;
			Spec.BuildingDefinitionId = BuildingDefinitionId;
			Spec.Anchor = Coordinate;
			Spec.Rotation = bRoad ? EHansaGridRotation::North : Rotation;
			Result.Add(Spec);
		}
		return Result;
	}

	TArray<FHansaGridCoordinate> FHansaRoadDragBuilder::BuildPath(
		const FHansaGridCoordinate Start,
		const FHansaGridCoordinate End)
	{
		TArray<FHansaGridCoordinate> Result;
		FHansaGridCoordinate Cursor = Start;
		Result.Add(Cursor);
		const int32 XStep = End.X >= Cursor.X ? 1 : -1;
		while (Cursor.X != End.X)
		{
			Cursor.X += XStep;
			Result.Add(Cursor);
		}
		const int32 YStep = End.Y >= Cursor.Y ? 1 : -1;
		while (Cursor.Y != End.Y)
		{
			Cursor.Y += YStep;
			Result.Add(Cursor);
		}
		return Result;
	}
}
