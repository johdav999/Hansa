#include "Logistics/HansaLocalLogistics.h"

#include "Construction/HansaConstruction.h"
#include "Inventory/HansaInventory.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaLogisticsPriority Priority)
	{
		switch (Priority)
		{
		case EHansaLogisticsPriority::Low: return TEXT("Low");
		case EHansaLogisticsPriority::Normal: return TEXT("Normal");
		case EHansaLogisticsPriority::High: return TEXT("High");
		case EHansaLogisticsPriority::Critical: return TEXT("Critical");
		default: return TEXT("UnknownLogisticsPriority");
		}
	}

	const TCHAR* LexToString(const EHansaLogisticsBottleneck Bottleneck)
	{
		switch (Bottleneck)
		{
		case EHansaLogisticsBottleneck::None: return TEXT("None");
		case EHansaLogisticsBottleneck::SourceInventoryMissing: return TEXT("SourceInventoryMissing");
		case EHansaLogisticsBottleneck::DestinationInventoryMissing: return TEXT("DestinationInventoryMissing");
		case EHansaLogisticsBottleneck::DisconnectedRoad: return TEXT("DisconnectedRoad");
		case EHansaLogisticsBottleneck::SourceStockUnavailable: return TEXT("SourceStockUnavailable");
		case EHansaLogisticsBottleneck::DestinationFull: return TEXT("DestinationFull");
		case EHansaLogisticsBottleneck::FleetCapacity: return TEXT("FleetCapacity");
		default: return TEXT("UnknownLogisticsBottleneck");
		}
	}

	const TCHAR* LexToString(const EHansaLogisticsRequestStatus Status)
	{
		switch (Status)
		{
		case EHansaLogisticsRequestStatus::Pending: return TEXT("Pending");
		case EHansaLogisticsRequestStatus::InProgress: return TEXT("InProgress");
		case EHansaLogisticsRequestStatus::Completed: return TEXT("Completed");
		default: return TEXT("UnknownLogisticsRequestStatus");
		}
	}

	const TCHAR* LexToString(const EHansaLogisticsJobStatus Status)
	{
		switch (Status)
		{
		case EHansaLogisticsJobStatus::AwaitingPickup: return TEXT("AwaitingPickup");
		case EHansaLogisticsJobStatus::InTransit: return TEXT("InTransit");
		case EHansaLogisticsJobStatus::Completed: return TEXT("Completed");
		default: return TEXT("UnknownLogisticsJobStatus");
		}
	}

	namespace
	{
		const FHansaBuildingState* FindBuilding(
			const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaBuildingId Id)
		{
			for (const FHansaBuildingState& Building : Buildings)
			{
				if (Building.Id == Id)
				{
					return &Building;
				}
			}
			return nullptr;
		}

		bool IsCompletedBuilding(
			const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaBuildingId Id)
		{
			const FHansaBuildingState* Building = FindBuilding(Buildings, Id);
			return Building != nullptr && Building->ConstructionState == EHansaConstructionState::Completed;
		}

		bool AreAdjacentOrEqual(const FHansaGridCoordinate Left, const FHansaGridCoordinate Right)
		{
			return FMath::Abs(Left.X - Right.X) + FMath::Abs(Left.Y - Right.Y) <= 1;
		}

		bool ContainsCell(const TArray<FHansaGridCoordinate>& Cells, const FHansaGridCoordinate Cell)
		{
			return Cells.Contains(Cell);
		}

		TArray<FHansaGridCoordinate> BuildRoadCells(
			const FHansaPlacementState& Placement,
			const FHansaPlacementMapInitialization& Map,
			const TConstArrayView<FHansaBuildingState> Buildings)
		{
			TArray<FHansaGridCoordinate> Result;
			for (const FHansaPlacedBuildingRecord& Record : Placement.GetPlacements())
			{
				if (Record.Spec.CityId == Map.CityId &&
					Record.Spec.BuildingDefinitionId == Map.RoadBuildingDefinitionId &&
					IsCompletedBuilding(Buildings, Record.BuildingId))
				{
					Result.Append(Record.OccupiedCells);
				}
			}
			Result.Sort();
			for (int32 Index = Result.Num() - 1; Index > 0; --Index)
			{
				if (Result[Index] == Result[Index - 1])
				{
					Result.RemoveAt(Index);
				}
			}
			return Result;
		}

		struct FEndpointAccess
		{
			bool bValid = false;
			FHansaCityDefinitionId CityId;
			TArray<FHansaGridCoordinate> Cells;
		};

		FEndpointAccess BuildEndpointAccess(
			const FHansaInventoryProjection& Inventory,
			const FHansaPlacementState& Placement,
			const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaPlacementMapInitialization& Map,
			const TArray<FHansaGridCoordinate>& RoadCells)
		{
			FEndpointAccess Result;
			if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City)
			{
				if (Inventory.CityId == Map.CityId)
				{
					// City market inventories use the completed local road network as their aggregate hand-off node.
					Result.bValid = true;
					Result.CityId = Inventory.CityId;
					Result.Cells = RoadCells;
				}
				return Result;
			}

			const FHansaPlacedBuildingRecord* EndpointPlacement = Placement.FindPlacement(Inventory.BuildingId);
			if (EndpointPlacement == nullptr || EndpointPlacement->Spec.CityId != Map.CityId ||
				!IsCompletedBuilding(Buildings, Inventory.BuildingId))
			{
				return Result;
			}
			Result.bValid = true;
			Result.CityId = EndpointPlacement->Spec.CityId;
			for (const FHansaGridCoordinate RoadCell : RoadCells)
			{
				for (const FHansaGridCoordinate EndpointCell : EndpointPlacement->OccupiedCells)
				{
					if (AreAdjacentOrEqual(RoadCell, EndpointCell))
					{
						Result.Cells.Add(RoadCell);
						break;
					}
				}
			}
			return Result;
		}
	}

	FHansaLogisticsRoadPathProjection FHansaLocalLogisticsQueries::QueryRoadPath(
		const FHansaInventoryId SourceInventoryId,
		const FHansaInventoryId DestinationInventoryId,
		const FHansaInventoryReadOnlyAccess& Inventories,
		const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings)
	{
		FHansaLogisticsRoadPathProjection Result;
		Result.SourceInventoryId = SourceInventoryId;
		Result.DestinationInventoryId = DestinationInventoryId;
		const TOptional<FHansaInventoryProjection> Source = Inventories.QueryInventory(SourceInventoryId);
		const TOptional<FHansaInventoryProjection> Destination = Inventories.QueryInventory(DestinationInventoryId);
		if (!Source.IsSet() || !Destination.IsSet())
		{
			return Result;
		}

		for (const FHansaPlacementMapInitialization& Map : Placement.GetMaps())
		{
			const TArray<FHansaGridCoordinate> RoadCells = BuildRoadCells(Placement, Map, Buildings);
			if (RoadCells.IsEmpty())
			{
				continue;
			}
			const FEndpointAccess SourceAccess = BuildEndpointAccess(
				Source.GetValue(), Placement, Buildings, Map, RoadCells);
			const FEndpointAccess DestinationAccess = BuildEndpointAccess(
				Destination.GetValue(), Placement, Buildings, Map, RoadCells);
			if (!SourceAccess.bValid || !DestinationAccess.bValid ||
				SourceAccess.CityId != DestinationAccess.CityId ||
				SourceAccess.Cells.IsEmpty() || DestinationAccess.Cells.IsEmpty())
			{
				continue;
			}

			Result.CityId = Map.CityId;
			Result.SourceAccessCells = SourceAccess.Cells;
			Result.DestinationAccessCells = DestinationAccess.Cells;
			TArray<FHansaGridCoordinate> Queue;
			TArray<FHansaGridCoordinate> Visited;
			TArray<int32> Distances;
			for (const FHansaGridCoordinate Cell : SourceAccess.Cells)
			{
				if (!Visited.Contains(Cell))
				{
					Visited.Add(Cell);
					Distances.Add(0);
					Queue.Add(Cell);
				}
			}

			int32 QueueIndex = 0;
			while (QueueIndex < Queue.Num())
			{
				const FHansaGridCoordinate Current = Queue[QueueIndex];
				const int32 CurrentDistance = Distances[QueueIndex];
				++QueueIndex;
				if (ContainsCell(DestinationAccess.Cells, Current))
				{
					Result.bConnected = true;
					Result.RoadDistanceCells = FMath::Max(1, CurrentDistance + 2);
					return Result;
				}
				TArray<FHansaGridCoordinate> Neighbors = {
					{ Current.X - 1, Current.Y }, { Current.X, Current.Y - 1 },
					{ Current.X, Current.Y + 1 }, { Current.X + 1, Current.Y }
				};
				Neighbors.Sort();
				for (const FHansaGridCoordinate Neighbor : Neighbors)
				{
					if (RoadCells.Contains(Neighbor) && !Visited.Contains(Neighbor))
					{
						Visited.Add(Neighbor);
						Distances.Add(CurrentDistance + 1);
						Queue.Add(Neighbor);
					}
				}
			}
			return Result;
		}
		return Result;
	}
}
