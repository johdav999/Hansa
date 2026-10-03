#pragma once

#include "Containers/Array.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	/** Land rights only. Exact building validity still comes from ValidatePlacement. */
	enum class EHansaLandAccess : uint8 { Unavailable, Permitted, Conditional, Denied };
	enum class EHansaLandAccessReason : uint8
	{
		UnknownCell, StartingCity, RecordedOwner, ActiveLease, ForeignLeaseRequired,
		ForeignLeaseInactive, ForeignPresenceInsufficient
	};
	enum class EHansaLandQueryFailure : uint8 { None, InvalidViewer, UnknownCity, InvalidBounds, TooLarge };

	struct HANSASIMULATION_API FHansaLandCellView final
	{
		FHansaGridCoordinate Coordinate;
        // Presentation survey runs extend along +Y; ordinary cell queries keep length one.
        int32 RunLength = 1;
        int32 RegionId = 0;
		FHansaHouseId RecordedOwnerId;
		EHansaPlacementTerrain Terrain = EHansaPlacementTerrain::Land;
		EHansaLandAccess Access = EHansaLandAccess::Unavailable;
		EHansaLandAccessReason Reason = EHansaLandAccessReason::UnknownCell;
		FHansaLeasedPlotId ViewerLeaseId;
		FHansaBuildingId OccupyingBuildingId;
		// Derived presentation clip only; run pages cover authored bounds and never set this.
        bool bOutsideSurvey = false;
		bool bSurveyKnown = false;
		bool bProtected = false;
	};

	/** Only leases belonging to the requesting viewer are included. Bounds are authoritative, not render chunks. */
	struct HANSASIMULATION_API FHansaLandLeaseView final
	{
		FHansaLeasedPlotId Id;
		FHansaGridCoordinate BoundsMin;
		FHansaGridCoordinate BoundsMax;
		TArray<FString> PermittedBuildingCategories;
		bool bActive = false;
	};

	struct HANSASIMULATION_API FHansaLandQueryResult final
	{
		FHansaCityDefinitionId CityId;
		FHansaHouseId ViewerHouseId;
		FHansaGridCoordinate BoundsMin;
		FHansaGridCoordinate BoundsMax;
		uint64 StateRevision = 0;
        int32 SurveyPages = 0;
		EHansaLandQueryFailure Failure = EHansaLandQueryFailure::None;
		TArray<FHansaLandCellView> Cells;
		TArray<FHansaLandLeaseView> ViewerLeases;
	};
}
