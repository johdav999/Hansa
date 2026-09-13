#include "Logistics/HansaLocalLogistics.h"

#include "Construction/HansaConstruction.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Model/HansaSimulationState.h"
#include "Algo/Reverse.h"

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
		case EHansaLogisticsJobStatus::PausedAwaitingPickup: return TEXT("PausedAwaitingPickup");
		case EHansaLogisticsJobStatus::PausedInTransit: return TEXT("PausedInTransit");
		default: return TEXT("UnknownLogisticsJobStatus");
		}
	}

	const TCHAR* LexToString(const EHansaLogisticsRoadPathFailure Failure)
	{
		switch (Failure)
		{
		case EHansaLogisticsRoadPathFailure::None: return TEXT("None");
		case EHansaLogisticsRoadPathFailure::SourceInventoryMissing: return TEXT("SourceInventoryMissing");
		case EHansaLogisticsRoadPathFailure::DestinationInventoryMissing: return TEXT("DestinationInventoryMissing");
		case EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable: return TEXT("SourceEndpointUnavailable");
		case EHansaLogisticsRoadPathFailure::DestinationEndpointUnavailable: return TEXT("DestinationEndpointUnavailable");
		case EHansaLogisticsRoadPathFailure::DifferentCities: return TEXT("DifferentCities");
		case EHansaLogisticsRoadPathFailure::NoCompletedRoad: return TEXT("NoCompletedRoad");
		case EHansaLogisticsRoadPathFailure::NoOperationalMarket: return TEXT("NoOperationalMarket");
		case EHansaLogisticsRoadPathFailure::NoMarketRoadAccess: return TEXT("NoMarketRoadAccess");
		case EHansaLogisticsRoadPathFailure::SourceNotAdjacentToRoad: return TEXT("SourceNotAdjacentToRoad");
		case EHansaLogisticsRoadPathFailure::DestinationNotAdjacentToRoad: return TEXT("DestinationNotAdjacentToRoad");
		case EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket: return TEXT("SourceNotConnectedToMarket");
		case EHansaLogisticsRoadPathFailure::DestinationNotConnectedToMarket: return TEXT("DestinationNotConnectedToMarket");
		case EHansaLogisticsRoadPathFailure::EndpointsDisconnected: return TEXT("EndpointsDisconnected");
		default: return TEXT("UnknownLogisticsRoadPathFailure");
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

		bool IsMarketAccessProvider(
			const FHansaPlacedBuildingRecord& Record,
			const FHansaEconomicRegistry* Registry)
		{
			if (Registry != nullptr)
			{
				const FHansaCompiledBuildingDefinition* Definition =
					Registry->FindBuilding(Record.Spec.BuildingDefinitionId.ToString());
				if (Definition == nullptr)
				{
					// Narrow compatibility for actor-free legacy fixtures whose minimal registry omits Market.
					return Record.Spec.BuildingDefinitionId.ToString() == TEXT("Building.Market");
				}
				return Definition->SchemaVersion >= 4
					? Definition->bProvidesMarketAccess
					: Definition->StableId == TEXT("Building.Market");
			}
			return Record.Spec.BuildingDefinitionId.ToString() == TEXT("Building.Market");
		}

		bool AreOrthogonallyAdjacent(const FHansaGridCoordinate Left, const FHansaGridCoordinate Right)
		{
			return FMath::Abs(Left.X - Right.X) + FMath::Abs(Left.Y - Right.Y) == 1;
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
			bool bCityInventory = false;
			FHansaCityDefinitionId CityId;
			FHansaBuildingId BoundMarketBuildingId;
			TArray<FHansaGridCoordinate> Cells;
		};

		struct FMarketAccess
		{
			FHansaBuildingId BuildingId;
			TArray<FHansaGridCoordinate> Cells;
		};

		void SetFailure(
			FHansaLogisticsRoadPathProjection& Result,
			const EHansaLogisticsRoadPathFailure Failure)
		{
			Result.bConnected = false;
			Result.bMarketEligible = false;
			Result.Failure = Failure;
			switch (Failure)
			{
			case EHansaLogisticsRoadPathFailure::None:
				Result.MessageKey = NAME_None;
				Result.RemedyKey = NAME_None;
				break;
			case EHansaLogisticsRoadPathFailure::SourceInventoryMissing:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.SourceInventoryMissing");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.SelectExistingSource");
				break;
			case EHansaLogisticsRoadPathFailure::DestinationInventoryMissing:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.DestinationInventoryMissing");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.SelectExistingDestination");
				break;
			case EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.SourceEndpointUnavailable");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.CompleteSourceBuilding");
				break;
			case EHansaLogisticsRoadPathFailure::DestinationEndpointUnavailable:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.DestinationEndpointUnavailable");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.CompleteDestinationBuilding");
				break;
			case EHansaLogisticsRoadPathFailure::DifferentCities:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.DifferentCities");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.UseIntercityRoute");
				break;
			case EHansaLogisticsRoadPathFailure::NoCompletedRoad:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.NoCompletedRoad");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.BuildRoad");
				break;
			case EHansaLogisticsRoadPathFailure::NoOperationalMarket:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.NoOperationalMarket");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.BuildAndCompleteMarket");
				break;
			case EHansaLogisticsRoadPathFailure::NoMarketRoadAccess:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.NoMarketRoadAccess");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.ConnectMarketToRoad");
				break;
			case EHansaLogisticsRoadPathFailure::SourceNotAdjacentToRoad:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.SourceNotAdjacentToRoad");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.ConnectSourceToRoad");
				break;
			case EHansaLogisticsRoadPathFailure::DestinationNotAdjacentToRoad:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.DestinationNotAdjacentToRoad");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.ConnectDestinationToRoad");
				break;
			case EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.SourceNotConnectedToMarket");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.ConnectSourceRoadToMarket");
				break;
			case EHansaLogisticsRoadPathFailure::DestinationNotConnectedToMarket:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.DestinationNotConnectedToMarket");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.ConnectDestinationRoadToMarket");
				break;
			case EHansaLogisticsRoadPathFailure::EndpointsDisconnected:
				Result.MessageKey = TEXT("Hansa.Logistics.Path.EndpointsDisconnected");
				Result.RemedyKey = TEXT("Hansa.Logistics.Path.Remedy.JoinRoadNetworks");
				break;
			}
		}

		TArray<FHansaGridCoordinate> BuildAccessCells(
			const TConstArrayView<FHansaGridCoordinate> Footprint,
			const TArray<FHansaGridCoordinate>& RoadCells)
		{
			TArray<FHansaGridCoordinate> Result;
			for (const FHansaGridCoordinate RoadCell : RoadCells)
			{
				for (const FHansaGridCoordinate EndpointCell : Footprint)
				{
					if (AreOrthogonallyAdjacent(RoadCell, EndpointCell))
					{
						Result.Add(RoadCell);
						break;
					}
				}
			}
			return Result;
		}

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
					Result.bValid = true;
					Result.bCityInventory = true;
					Result.CityId = Inventory.CityId;
					const FHansaPlacedBuildingRecord* SelectedMarket = nullptr;
					for (const FHansaPlacedBuildingRecord& Record : Placement.GetPlacements())
					{
						if (Record.Spec.CityId != Map.CityId ||
							Record.Spec.BuildingDefinitionId.ToString() != TEXT("Building.Market") ||
							!IsCompletedBuilding(Buildings, Record.BuildingId) ||
							(Inventory.BuildingId.IsValid() && Record.BuildingId != Inventory.BuildingId))
						{
							continue;
						}
						if (SelectedMarket == nullptr || Record.BuildingId.GetValue() < SelectedMarket->BuildingId.GetValue())
						{
							SelectedMarket = &Record;
						}
					}
					if (SelectedMarket != nullptr)
					{
						Result.BoundMarketBuildingId = SelectedMarket->BuildingId;
						Result.Cells = BuildAccessCells(SelectedMarket->OccupiedCells, RoadCells);
					}
					else if (Inventory.BuildingId.IsValid())
					{
						Result.bValid = false;
					}
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
			Result.Cells = BuildAccessCells(EndpointPlacement->OccupiedCells, RoadCells);
			return Result;
		}

		TArray<FHansaGridCoordinate> ShortestRoadPath(
			const TArray<FHansaGridCoordinate>& RoadCells,
			const TArray<FHansaGridCoordinate>& StartCells,
			const TArray<FHansaGridCoordinate>& TargetCells)
		{
			if (StartCells.IsEmpty() || TargetCells.IsEmpty())
			{
				return {};
			}
			TArray<FHansaGridCoordinate> OrderedStarts = StartCells;
			OrderedStarts.Sort();
			TArray<FHansaGridCoordinate> Queue;
			TArray<FHansaGridCoordinate> Visited;
			TArray<int32> ParentIndices;
			for (const FHansaGridCoordinate Cell : OrderedStarts)
			{
				if (!Visited.Contains(Cell))
				{
					Visited.Add(Cell);
					ParentIndices.Add(ParentIndices.Num());
					Queue.Add(Cell);
				}
			}
			int32 QueueIndex = 0;
			while (QueueIndex < Queue.Num())
			{
				const FHansaGridCoordinate Current = Queue[QueueIndex];
				++QueueIndex;
				if (ContainsCell(TargetCells, Current))
				{
					TArray<FHansaGridCoordinate> Result;
					int32 PathIndex = QueueIndex - 1;
					while (true)
					{
						Result.Add(Visited[PathIndex]);
						const int32 ParentIndex = ParentIndices[PathIndex];
						if (ParentIndex == PathIndex)
						{
							break;
						}
						PathIndex = ParentIndex;
					}
					Algo::Reverse(Result);
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
						ParentIndices.Add(QueueIndex - 1);
						Queue.Add(Neighbor);
					}
				}
			}
			return {};
		}

		int32 ShortestRoadDistance(
			const TArray<FHansaGridCoordinate>& RoadCells,
			const TArray<FHansaGridCoordinate>& StartCells,
			const TArray<FHansaGridCoordinate>& TargetCells)
		{
			const TArray<FHansaGridCoordinate> Path = ShortestRoadPath(RoadCells, StartCells, TargetCells);
			return Path.IsEmpty() ? INDEX_NONE : Path.Num() - 1;
		}
	}

	FHansaLogisticsRoadPathProjection FHansaLocalLogisticsQueries::QueryBuildingRoadAccess(
		const FHansaBuildingId SourceBuildingId,
		const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings)
	{
		FHansaLogisticsRoadPathProjection Result;
		const FHansaPlacedBuildingRecord* SourcePlacement = Placement.FindPlacement(SourceBuildingId);
		if (SourcePlacement == nullptr || !IsCompletedBuilding(Buildings, SourceBuildingId))
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable);
			return Result;
		}

		const FHansaPlacementMapInitialization* Map = Placement.GetMaps().FindByPredicate(
			[SourcePlacement](const FHansaPlacementMapInitialization& Candidate)
			{
				return Candidate.CityId == SourcePlacement->Spec.CityId;
			});
		if (Map == nullptr)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable);
			return Result;
		}

		Result.CityId = Map->CityId;
		const TArray<FHansaGridCoordinate> RoadCells = BuildRoadCells(Placement, *Map, Buildings);
		if (RoadCells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoCompletedRoad);
			return Result;
		}

		Result.SourceAccessCells = BuildAccessCells(SourcePlacement->OccupiedCells, RoadCells);
		if (Result.SourceAccessCells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceNotAdjacentToRoad);
			return Result;
		}

		Result.bConnected = true;
		Result.Failure = EHansaLogisticsRoadPathFailure::None;
		Result.MessageKey = NAME_None;
		Result.RemedyKey = NAME_None;
		return Result;
	}

	FHansaLogisticsRoadPathProjection FHansaLocalLogisticsQueries::QueryRoadPath(
		const FHansaInventoryId SourceInventoryId,
		const FHansaInventoryId DestinationInventoryId,
		const FHansaInventoryReadOnlyAccess& Inventories,
		const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings,
		const FHansaEconomicRegistry* Registry)
	{
		FHansaLogisticsRoadPathProjection Result;
		Result.SourceInventoryId = SourceInventoryId;
		Result.DestinationInventoryId = DestinationInventoryId;
		const TOptional<FHansaInventoryProjection> Source = Inventories.QueryInventory(SourceInventoryId);
		const TOptional<FHansaInventoryProjection> Destination = Inventories.QueryInventory(DestinationInventoryId);
		if (!Source.IsSet())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceInventoryMissing);
			return Result;
		}
		if (!Destination.IsSet())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationInventoryMissing);
			return Result;
		}

		FEndpointAccess SourceAccess;
		FEndpointAccess DestinationAccess;
		const FHansaPlacementMapInitialization* Map = nullptr;
		for (const FHansaPlacementMapInitialization& CandidateMap : Placement.GetMaps())
		{
			const TArray<FHansaGridCoordinate> CandidateRoads = BuildRoadCells(Placement, CandidateMap, Buildings);
			const FEndpointAccess CandidateSource = BuildEndpointAccess(
				Source.GetValue(), Placement, Buildings, CandidateMap, CandidateRoads);
			const FEndpointAccess CandidateDestination = BuildEndpointAccess(
				Destination.GetValue(), Placement, Buildings, CandidateMap, CandidateRoads);
			if (CandidateSource.bValid && CandidateDestination.bValid)
			{
				SourceAccess = CandidateSource;
				DestinationAccess = CandidateDestination;
				Map = &CandidateMap;
				break;
			}
		}
		if (Map == nullptr)
		{
			bool bSourceFound = false;
			bool bDestinationFound = false;
			FHansaCityDefinitionId SourceCity;
			FHansaCityDefinitionId DestinationCity;
			for (const FHansaPlacementMapInitialization& CandidateMap : Placement.GetMaps())
			{
				const TArray<FHansaGridCoordinate> CandidateRoads = BuildRoadCells(Placement, CandidateMap, Buildings);
				const FEndpointAccess CandidateSource = BuildEndpointAccess(
					Source.GetValue(), Placement, Buildings, CandidateMap, CandidateRoads);
				const FEndpointAccess CandidateDestination = BuildEndpointAccess(
					Destination.GetValue(), Placement, Buildings, CandidateMap, CandidateRoads);
				if (CandidateSource.bValid)
				{
					bSourceFound = true;
					SourceCity = CandidateSource.CityId;
				}
				if (CandidateDestination.bValid)
				{
					bDestinationFound = true;
					DestinationCity = CandidateDestination.CityId;
				}
			}
			if (!bSourceFound)
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable);
			}
			else if (!bDestinationFound)
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationEndpointUnavailable);
			}
			else if (SourceCity != DestinationCity)
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::DifferentCities);
			}
			else
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::EndpointsDisconnected);
			}
			return Result;
		}

		Result.CityId = Map->CityId;
		if (SourceAccess.CityId != DestinationAccess.CityId)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DifferentCities);
			return Result;
		}
		const TArray<FHansaGridCoordinate> RoadCells = BuildRoadCells(Placement, *Map, Buildings);
		if (RoadCells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoCompletedRoad);
			return Result;
		}
		Result.SourceAccessCells = SourceAccess.Cells;
		Result.DestinationAccessCells = DestinationAccess.Cells;
		if (!SourceAccess.bCityInventory && SourceAccess.Cells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceNotAdjacentToRoad);
			return Result;
		}
		if (!DestinationAccess.bCityInventory && DestinationAccess.Cells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationNotAdjacentToRoad);
			return Result;
		}

		int32 OperationalMarketCount = 0;
		TArray<FMarketAccess> Markets;
		for (const FHansaPlacedBuildingRecord& Record : Placement.GetPlacements())
		{
			if (Record.Spec.CityId != Map->CityId ||
				!IsMarketAccessProvider(Record, Registry) ||
				!IsCompletedBuilding(Buildings, Record.BuildingId))
			{
				continue;
			}
			++OperationalMarketCount;
			FMarketAccess Market;
			Market.BuildingId = Record.BuildingId;
			Market.Cells = BuildAccessCells(Record.OccupiedCells, RoadCells);
			if (!Market.Cells.IsEmpty())
			{
				Markets.Add(MoveTemp(Market));
			}
		}
		if (OperationalMarketCount == 0)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoOperationalMarket);
			return Result;
		}
		if (Markets.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoMarketRoadAccess);
			return Result;
		}
		if ((SourceAccess.bCityInventory && SourceAccess.Cells.IsEmpty()) ||
			(DestinationAccess.bCityInventory && DestinationAccess.Cells.IsEmpty()))
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoMarketRoadAccess);
			return Result;
		}
		Markets.Sort([](const FMarketAccess& Left, const FMarketAccess& Right)
		{
			return Left.BuildingId.GetValue() < Right.BuildingId.GetValue();
		});

		bool bSourceConnectedToAnyMarket = false;
		bool bDestinationConnectedToAnyMarket = false;
		int32 BestDeliveryDistance = INDEX_NONE;
		const FMarketAccess* BestMarket = nullptr;
		TArray<FHansaGridCoordinate> BestDeliveryPath;
		for (const FMarketAccess& Market : Markets)
		{
			const TArray<FHansaGridCoordinate>& SourceCells = SourceAccess.Cells;
			const TArray<FHansaGridCoordinate>& DestinationCells = DestinationAccess.Cells;
			const int32 SourceMarketDistance = ShortestRoadDistance(RoadCells, SourceAccess.Cells, Market.Cells);
			const int32 DestinationMarketDistance = ShortestRoadDistance(RoadCells, DestinationAccess.Cells, Market.Cells);
			bSourceConnectedToAnyMarket |= SourceMarketDistance != INDEX_NONE;
			bDestinationConnectedToAnyMarket |= DestinationMarketDistance != INDEX_NONE;
			if (SourceMarketDistance == INDEX_NONE || DestinationMarketDistance == INDEX_NONE)
			{
				continue;
			}
			const TArray<FHansaGridCoordinate> DeliveryPath = ShortestRoadPath(
				RoadCells, SourceCells, DestinationCells);
			if (DeliveryPath.IsEmpty())
			{
				continue;
			}
			const int32 DeliveryDistance = DeliveryPath.Num() - 1;
			if (BestMarket == nullptr || DeliveryDistance < BestDeliveryDistance ||
				(DeliveryDistance == BestDeliveryDistance &&
					Market.BuildingId.GetValue() < BestMarket->BuildingId.GetValue()))
			{
				BestMarket = &Market;
				BestDeliveryDistance = DeliveryDistance;
				BestDeliveryPath = DeliveryPath;
			}
		}

		if (BestMarket == nullptr)
		{
			if (!bSourceConnectedToAnyMarket)
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket);
			}
			else if (!bDestinationConnectedToAnyMarket)
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationNotConnectedToMarket);
			}
			else
			{
				SetFailure(Result, EHansaLogisticsRoadPathFailure::EndpointsDisconnected);
			}
			return Result;
		}

		Result.bConnected = true;
		Result.bMarketEligible = true;
		Result.SelectedMarketBuildingId = BestMarket->BuildingId;
		Result.Failure = EHansaLogisticsRoadPathFailure::None;
		Result.MessageKey = NAME_None;
		Result.RemedyKey = NAME_None;
		Result.SourceAccessCells = SourceAccess.Cells;
		Result.DestinationAccessCells = DestinationAccess.Cells;
		Result.RouteCells = MoveTemp(BestDeliveryPath);
		Result.RoadDistanceCells = FMath::Max(1, Result.RouteCells.Num() + 1);
		return Result;
	}

	FHansaLogisticsRoadPathProjection FHansaLocalLogisticsQueries::QueryBuildingMarketAccess(
		const FHansaBuildingId SourceBuildingId,
		const FHansaInventoryId MarketInventoryId,
		const FHansaInventoryReadOnlyAccess& Inventories,
		const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings,
		const FHansaEconomicRegistry* Registry)
	{
		FHansaLogisticsRoadPathProjection Result;
		Result.DestinationInventoryId = MarketInventoryId;
		const TOptional<FHansaInventoryProjection> Destination = Inventories.QueryInventory(MarketInventoryId);
		if (!Destination.IsSet())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationInventoryMissing);
			return Result;
		}
		if (Destination->OwnerKind != EHansaInventoryOwnerKind::City)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationEndpointUnavailable);
			return Result;
		}

		const FHansaPlacedBuildingRecord* SourcePlacement = Placement.FindPlacement(SourceBuildingId);
		if (SourcePlacement == nullptr || !IsCompletedBuilding(Buildings, SourceBuildingId))
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable);
			return Result;
		}
		if (SourcePlacement->Spec.CityId != Destination->CityId)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DifferentCities);
			return Result;
		}

		const FHansaPlacementMapInitialization* Map = Placement.GetMaps().FindByPredicate(
			[SourcePlacement](const FHansaPlacementMapInitialization& Candidate)
			{
				return Candidate.CityId == SourcePlacement->Spec.CityId;
			});
		if (Map == nullptr)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceEndpointUnavailable);
			return Result;
		}
		Result.CityId = Map->CityId;
		const TArray<FHansaGridCoordinate> RoadCells = BuildRoadCells(Placement, *Map, Buildings);
		if (RoadCells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoCompletedRoad);
			return Result;
		}
		const TArray<FHansaGridCoordinate> SourceCells = BuildAccessCells(SourcePlacement->OccupiedCells, RoadCells);
		if (SourceCells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::SourceNotAdjacentToRoad);
			return Result;
		}
		const FEndpointAccess DestinationAccess = BuildEndpointAccess(
			Destination.GetValue(), Placement, Buildings, *Map, RoadCells);
		if (!DestinationAccess.bValid)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::DestinationEndpointUnavailable);
			return Result;
		}

		int32 OperationalMarketCount = 0;
		TArray<FMarketAccess> Markets;
		for (const FHansaPlacedBuildingRecord& Record : Placement.GetPlacements())
		{
			if (Record.Spec.CityId != Map->CityId ||
				!IsMarketAccessProvider(Record, Registry) ||
				!IsCompletedBuilding(Buildings, Record.BuildingId))
			{
				continue;
			}
			++OperationalMarketCount;
			FMarketAccess Market;
			Market.BuildingId = Record.BuildingId;
			Market.Cells = BuildAccessCells(Record.OccupiedCells, RoadCells);
			if (!Market.Cells.IsEmpty()) Markets.Add(MoveTemp(Market));
		}
		if (OperationalMarketCount == 0)
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoOperationalMarket);
			return Result;
		}
		if (Markets.IsEmpty() || DestinationAccess.Cells.IsEmpty())
		{
			SetFailure(Result, EHansaLogisticsRoadPathFailure::NoMarketRoadAccess);
			return Result;
		}
		Markets.Sort([](const FMarketAccess& Left, const FMarketAccess& Right)
		{
			return Left.BuildingId.GetValue() < Right.BuildingId.GetValue();
		});

		const FMarketAccess* BestMarket = nullptr;
		int32 BestDistance = INDEX_NONE;
		TArray<FHansaGridCoordinate> BestPath;
		bool bSourceConnected = false;
		bool bDestinationConnected = false;
		for (const FMarketAccess& Market : Markets)
		{
			const int32 SourceMarketDistance = ShortestRoadDistance(RoadCells, SourceCells, Market.Cells);
			const int32 DestinationMarketDistance = ShortestRoadDistance(
				RoadCells, DestinationAccess.Cells, Market.Cells);
			bSourceConnected |= SourceMarketDistance != INDEX_NONE;
			bDestinationConnected |= DestinationMarketDistance != INDEX_NONE;
			if (SourceMarketDistance == INDEX_NONE || DestinationMarketDistance == INDEX_NONE) continue;
			const TArray<FHansaGridCoordinate> Path = ShortestRoadPath(
				RoadCells, SourceCells, DestinationAccess.Cells);
			const int32 Distance = Path.IsEmpty() ? INDEX_NONE : Path.Num() - 1;
			if (Distance != INDEX_NONE && (BestMarket == nullptr || Distance < BestDistance ||
				(Distance == BestDistance && Market.BuildingId.GetValue() < BestMarket->BuildingId.GetValue())))
			{
				BestMarket = &Market;
				BestDistance = Distance;
				BestPath = Path;
			}
		}
		if (BestMarket == nullptr)
		{
			SetFailure(Result, !bSourceConnected
				? EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket
				: !bDestinationConnected
					? EHansaLogisticsRoadPathFailure::DestinationNotConnectedToMarket
					: EHansaLogisticsRoadPathFailure::EndpointsDisconnected);
			return Result;
		}

		Result.bConnected = true;
		Result.bMarketEligible = true;
		Result.SelectedMarketBuildingId = BestMarket->BuildingId;
		Result.SourceAccessCells = SourceCells;
		Result.DestinationAccessCells = DestinationAccess.Cells;
		Result.RouteCells = MoveTemp(BestPath);
		Result.RoadDistanceCells = FMath::Max(1, Result.RouteCells.Num() + 1);
		return Result;
	}
}
