#include "Trade/HansaTrade.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaRouteMode Mode)
	{
		switch (Mode)
		{
		case EHansaRouteMode::Sea: return TEXT("Sea");
		case EHansaRouteMode::Land: return TEXT("Land");
		default: return TEXT("UnknownRouteMode");
		}
	}

	const TCHAR* LexToString(const EHansaRouteCargoActionKind Kind)
	{
		switch (Kind)
		{
		case EHansaRouteCargoActionKind::Load: return TEXT("Load");
		case EHansaRouteCargoActionKind::Unload: return TEXT("Unload");
		case EHansaRouteCargoActionKind::StationLoad: return TEXT("Station load (transfer)");
		case EHansaRouteCargoActionKind::StationUnload: return TEXT("Station unload (transfer)");
		case EHansaRouteCargoActionKind::OwnedCityLoad: return TEXT("Home load (transfer)");
		case EHansaRouteCargoActionKind::OwnedCityUnload: return TEXT("Home unload (transfer)");
		default: return TEXT("UnknownCargoAction");
		}
	}

	const TCHAR* LexToString(const EHansaRouteLifecycleState State)
	{
		switch (State)
		{
		case EHansaRouteLifecycleState::Inactive: return TEXT("Inactive");
		case EHansaRouteLifecycleState::AtStop: return TEXT("AtStop");
		case EHansaRouteLifecycleState::Traveling: return TEXT("Traveling");
		case EHansaRouteLifecycleState::Cancelled: return TEXT("Cancelled");
		default: return TEXT("UnknownRouteLifecycle");
		}
	}

	const TCHAR* LexToString(const EHansaRouteTransferOutcome Outcome)
	{
		switch (Outcome)
		{
		case EHansaRouteTransferOutcome::None: return TEXT("None");
		case EHansaRouteTransferOutcome::Completed: return TEXT("Completed");
		case EHansaRouteTransferOutcome::Partial: return TEXT("Partial");
		case EHansaRouteTransferOutcome::Missed: return TEXT("Missed");
		default: return TEXT("UnknownRouteTransferOutcome");
		}
	}

	const TCHAR* LexToString(const EHansaSpotTradeSide Side)
	{
		switch (Side) { case EHansaSpotTradeSide::BuyFromCity: return TEXT("BuyFromCity"); case EHansaSpotTradeSide::SellToCity: return TEXT("SellToCity"); default: return TEXT("UnknownSpotTradeSide"); }
	}
	const TCHAR* LexToString(const EHansaSpotTradeOutcome Outcome)
	{
		switch (Outcome) { case EHansaSpotTradeOutcome::None: return TEXT("None"); case EHansaSpotTradeOutcome::Completed: return TEXT("Completed"); case EHansaSpotTradeOutcome::Partial: return TEXT("Partial"); case EHansaSpotTradeOutcome::Missed: return TEXT("Missed"); default: return TEXT("UnknownSpotTradeOutcome"); }
	}
	const TCHAR* LexToString(const EHansaSpotTradeBlocker Blocker)
	{
		switch (Blocker) { case EHansaSpotTradeBlocker::None: return TEXT("None"); case EHansaSpotTradeBlocker::InsufficientFunds: return TEXT("InsufficientFunds"); case EHansaSpotTradeBlocker::InsufficientMarketStock: return TEXT("InsufficientMarketStock"); case EHansaSpotTradeBlocker::InsufficientShipCapacity: return TEXT("InsufficientShipCapacity"); case EHansaSpotTradeBlocker::InsufficientShipStock: return TEXT("InsufficientShipStock"); case EHansaSpotTradeBlocker::InsufficientMarketCapacity: return TEXT("InsufficientMarketCapacity"); default: return TEXT("UnknownSpotTradeBlocker"); }
	}
	const TCHAR* LexToString(const EHansaRoutePlanError Error)
	{
		switch (Error)
		{
		case EHansaRoutePlanError::None: return TEXT("None");
		case EHansaRoutePlanError::InvalidIdentity: return TEXT("InvalidIdentity");
		case EHansaRoutePlanError::VehicleNotFound: return TEXT("VehicleNotFound");
		case EHansaRoutePlanError::DefinitionNotFound: return TEXT("DefinitionNotFound");
		case EHansaRoutePlanError::ModeMismatch: return TEXT("ModeMismatch");
		case EHansaRoutePlanError::InvalidStops: return TEXT("InvalidStops");
		case EHansaRoutePlanError::UnreachableLeg: return TEXT("UnreachableLeg");
		case EHansaRoutePlanError::InvalidAction: return TEXT("InvalidAction");
		case EHansaRoutePlanError::VehicleLocationMismatch: return TEXT("VehicleLocationMismatch");
		case EHansaRoutePlanError::InvalidCargoInventory: return TEXT("InvalidCargoInventory");
		default: return TEXT("UnknownRoutePlanError");
		}
	}
}
