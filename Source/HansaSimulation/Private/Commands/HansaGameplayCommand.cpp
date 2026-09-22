#include "Commands/HansaGameplayCommand.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint64 GameplayCommandFnvOffset = 14695981039346656037ULL;
		constexpr uint64 GameplayCommandFnvPrime = 1099511628211ULL;

		void AddByte(uint64& Hash, const uint8 Value)
		{
			Hash ^= Value;
			Hash *= GameplayCommandFnvPrime;
		}

		void AddUInt16(uint64& Hash, const uint16 Value)
		{
			AddByte(Hash, static_cast<uint8>(Value));
			AddByte(Hash, static_cast<uint8>(Value >> 8));
		}

		void AddUInt32(uint64& Hash, const uint32 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
			{
				AddByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddUInt64(uint64& Hash, const uint64 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				AddByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddString(uint64& Hash, const FString& Value)
		{
			AddUInt32(Hash, static_cast<uint32>(Value.Len()));
			for (const TCHAR Character : Value)
			{
				AddByte(Hash, static_cast<uint8>(Character));
			}
		}

		void AddRouteStops(uint64& Hash, const TArray<FHansaRouteStop>& Stops)
		{
			AddUInt32(Hash, static_cast<uint32>(Stops.Num()));
			for (const FHansaRouteStop& Stop : Stops)
			{
				AddString(Hash, Stop.CityId.ToString());
				AddUInt32(Hash, static_cast<uint32>(Stop.Actions.Num()));
				for (const FHansaRouteCargoAction& Action : Stop.Actions)
				{
					AddByte(Hash, static_cast<uint8>(Action.Kind));
					AddByte(Hash, static_cast<uint8>(Action.Condition));
					AddString(Hash, Action.GoodId.ToString());
					AddUInt64(Hash, static_cast<uint64>(Action.QuantityLimit.GetRawValue()));
					AddUInt64(Hash, static_cast<uint64>(Action.MinimumSourceReserve.GetRawValue()));
				}
			}
		}
	}

	const TCHAR* LexToString(const EHansaCommandOrigin Origin)
	{
		switch (Origin)
		{
		case EHansaCommandOrigin::PlayerInput: return TEXT("PlayerInput");
		case EHansaCommandOrigin::ArtificialIntelligence: return TEXT("ArtificialIntelligence");
		case EHansaCommandOrigin::MultiplayerRpc: return TEXT("MultiplayerRpc");
		case EHansaCommandOrigin::ControlledAutomation: return TEXT("ControlledAutomation");
		default: return TEXT("UnknownCommandOrigin");
		}
	}

	const TCHAR* LexToString(const EHansaGameplayCommandType Type)
	{
		switch (Type)
		{
		case EHansaGameplayCommandType::CreateTestEntity: return TEXT("CreateTestEntity");
		case EHansaGameplayCommandType::CancelTestEntity: return TEXT("CancelTestEntity");
		case EHansaGameplayCommandType::NoOpTest: return TEXT("NoOpTest");
		case EHansaGameplayCommandType::SetProductionActive: return TEXT("SetProductionActive");
		case EHansaGameplayCommandType::PlaceBuilding: return TEXT("PlaceBuilding");
		case EHansaGameplayCommandType::CancelConstruction: return TEXT("CancelConstruction");
		case EHansaGameplayCommandType::RemoveBuilding: return TEXT("RemoveBuilding");
		case EHansaGameplayCommandType::UpgradeResidence: return TEXT("UpgradeResidence");
		case EHansaGameplayCommandType::CreateRoute: return TEXT("CreateRoute");
		case EHansaGameplayCommandType::EditRoute: return TEXT("EditRoute");
		case EHansaGameplayCommandType::SetRouteActive: return TEXT("SetRouteActive");
		case EHansaGameplayCommandType::CancelRoute: return TEXT("CancelRoute");
		case EHansaGameplayCommandType::QueueResearch: return TEXT("QueueResearch");
		case EHansaGameplayCommandType::SetHeatingReserve: return TEXT("SetHeatingReserve");
		case EHansaGameplayCommandType::SetProductionMode: return TEXT("SetProductionMode");
		case EHansaGameplayCommandType::UpgradeProduction: return TEXT("UpgradeProduction");
		case EHansaGameplayCommandType::MoveShip: return TEXT("MoveShip");
		case EHansaGameplayCommandType::SpotTrade: return TEXT("SpotTrade");
		case EHansaGameplayCommandType::ProposeTradeStation: return TEXT("ProposeTradeStation");
		case EHansaGameplayCommandType::FundTradeStation: return TEXT("FundTradeStation");
		case EHansaGameplayCommandType::CloseTradeStation: return TEXT("CloseTradeStation");
		case EHansaGameplayCommandType::ManageStationOrder: return TEXT("ManageStationOrder");
		case EHansaGameplayCommandType::RequestPresenceUpgrade: return TEXT("RequestPresenceUpgrade");
		case EHansaGameplayCommandType::FundPresenceUpgrade: return TEXT("FundPresenceUpgrade");
		case EHansaGameplayCommandType::ApplyPresenceSpecialization: return TEXT("ApplyPresenceSpecialization");
        case EHansaGameplayCommandType::SetHouseholdAvailability: return TEXT("SetHouseholdAvailability");
		default: return TEXT("UnknownGameplayCommand");
		}
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaCreateTestEntityCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::CreateTestEntity;
		Command.CreateTestEntity = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header,
		const FHansaCreateRouteCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::CreateRoute;
		Command.CreateRoute = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header,
		const FHansaEditRouteCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::EditRoute;
		Command.EditRoute = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header,
		const FHansaSetRouteActiveCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::SetRouteActive;
		Command.SetRouteActive = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header,
		const FHansaCancelRouteCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::CancelRoute;
		Command.CancelRoute = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header,
		const FHansaQueueResearchCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::QueueResearch;
		Command.QueueResearch = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaCancelConstructionCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::CancelConstruction;
		Command.CancelConstruction = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaRemoveBuildingCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::RemoveBuilding;
		Command.RemoveBuilding = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaUpgradeResidenceCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::UpgradeResidence;
		Command.UpgradeResidence = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaPlaceBuildingCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::PlaceBuilding;
		Command.PlaceBuilding = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaSetProductionActiveCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::SetProductionActive;
		Command.SetProductionActive = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaCancelTestEntityCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::CancelTestEntity;
		Command.CancelTestEntity = Payload;
		return Command;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(
		const FHansaCommandHeader& Header,
		const FHansaNoOpTestCommand& Payload)
	{
		FHansaGameplayCommand Command;
		Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::NoOpTest;
		Command.NoOpTest = Payload;
		return Command;
	}

	const FHansaCreateTestEntityCommand& FHansaGameplayCommand::GetCreateTestEntity() const
	{
		check(Type == EHansaGameplayCommandType::CreateTestEntity);
		return CreateTestEntity;
	}

	const FHansaCancelTestEntityCommand& FHansaGameplayCommand::GetCancelTestEntity() const
	{
		check(Type == EHansaGameplayCommandType::CancelTestEntity);
		return CancelTestEntity;
	}

	const FHansaNoOpTestCommand& FHansaGameplayCommand::GetNoOpTest() const
	{
		check(Type == EHansaGameplayCommandType::NoOpTest);
		return NoOpTest;
	}

	const FHansaSetProductionActiveCommand& FHansaGameplayCommand::GetSetProductionActive() const
	{
		check(Type == EHansaGameplayCommandType::SetProductionActive);
		return SetProductionActive;
	}

	const FHansaPlaceBuildingCommand& FHansaGameplayCommand::GetPlaceBuilding() const
	{
		check(Type == EHansaGameplayCommandType::PlaceBuilding);
		return PlaceBuilding;
	}

	const FHansaCancelConstructionCommand& FHansaGameplayCommand::GetCancelConstruction() const
	{
		check(Type == EHansaGameplayCommandType::CancelConstruction);
		return CancelConstruction;
	}

	const FHansaRemoveBuildingCommand& FHansaGameplayCommand::GetRemoveBuilding() const
	{
		check(Type == EHansaGameplayCommandType::RemoveBuilding);
		return RemoveBuilding;
	}

	const FHansaUpgradeResidenceCommand& FHansaGameplayCommand::GetUpgradeResidence() const
	{
		check(Type == EHansaGameplayCommandType::UpgradeResidence);
		return UpgradeResidence;
	}

	const FHansaCreateRouteCommand& FHansaGameplayCommand::GetCreateRoute() const
	{
		check(Type == EHansaGameplayCommandType::CreateRoute);
		return CreateRoute;
	}

	const FHansaEditRouteCommand& FHansaGameplayCommand::GetEditRoute() const
	{
		check(Type == EHansaGameplayCommandType::EditRoute);
		return EditRoute;
	}

	const FHansaSetRouteActiveCommand& FHansaGameplayCommand::GetSetRouteActive() const
	{
		check(Type == EHansaGameplayCommandType::SetRouteActive);
		return SetRouteActive;
	}

	const FHansaCancelRouteCommand& FHansaGameplayCommand::GetCancelRoute() const
	{
		check(Type == EHansaGameplayCommandType::CancelRoute);
		return CancelRoute;
	}

	const FHansaQueueResearchCommand& FHansaGameplayCommand::GetQueueResearch() const
	{
		check(Type == EHansaGameplayCommandType::QueueResearch);
		return QueueResearch;
	}

	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaSetHeatingReserveCommand& Payload)
	{
		FHansaGameplayCommand Command; Command.Header = Header;
		Command.Type = EHansaGameplayCommandType::SetHeatingReserve; Command.SetHeatingReserve = Payload; return Command;
	}
	const FHansaSetHeatingReserveCommand& FHansaGameplayCommand::GetSetHeatingReserve() const
	{
		check(Type == EHansaGameplayCommandType::SetHeatingReserve); return SetHeatingReserve;
	}

FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaSetProductionModeCommand& Payload)
{
 FHansaGameplayCommand C; C.Header=Header; C.Type=EHansaGameplayCommandType::SetProductionMode; C.SetProductionMode=Payload; return C;
}
const FHansaSetProductionModeCommand& FHansaGameplayCommand::GetSetProductionMode() const { check(Type==EHansaGameplayCommandType::SetProductionMode); return SetProductionMode; }
FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaUpgradeProductionCommand& Payload)
{
 FHansaGameplayCommand C; C.Header=Header; C.Type=EHansaGameplayCommandType::UpgradeProduction; C.UpgradeProduction=Payload; return C;
}
const FHansaUpgradeProductionCommand& FHansaGameplayCommand::GetUpgradeProduction() const { check(Type==EHansaGameplayCommandType::UpgradeProduction); return UpgradeProduction; }
FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaSetHouseholdAvailabilityCommand& Payload)
{
 FHansaGameplayCommand C; C.Header=Header; C.Type=EHansaGameplayCommandType::SetHouseholdAvailability; C.SetHouseholdAvailability=Payload; return C;
}
const FHansaSetHouseholdAvailabilityCommand& FHansaGameplayCommand::GetSetHouseholdAvailability() const { check(Type==EHansaGameplayCommandType::SetHouseholdAvailability); return SetHouseholdAvailability; }
    FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaMoveShipCommand& Payload)
    {
        FHansaGameplayCommand C; C.Header=Header; C.Type=EHansaGameplayCommandType::MoveShip; C.MoveShip=Payload; return C;
    }
    const FHansaMoveShipCommand& FHansaGameplayCommand::GetMoveShip() const
    { check(Type==EHansaGameplayCommandType::MoveShip); return MoveShip; }
		FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& Header, const FHansaSpotTradeCommand& Payload)
	{
		FHansaGameplayCommand Command; Command.Header=Header; Command.Type=EHansaGameplayCommandType::SpotTrade; Command.SpotTrade=Payload; return Command;
	}
	const FHansaSpotTradeCommand& FHansaGameplayCommand::GetSpotTrade() const
	{
		check(Type==EHansaGameplayCommandType::SpotTrade); return SpotTrade;
	}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaProposeTradeStationCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::ProposeTradeStation;C.ProposeTradeStation=P;return C;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaFundTradeStationCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::FundTradeStation;C.FundTradeStation=P;return C;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaCloseTradeStationCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::CloseTradeStation;C.CloseTradeStation=P;return C;}
	const FHansaProposeTradeStationCommand& FHansaGameplayCommand::GetProposeTradeStation() const{check(Type==EHansaGameplayCommandType::ProposeTradeStation);return ProposeTradeStation;}
	const FHansaFundTradeStationCommand& FHansaGameplayCommand::GetFundTradeStation() const{check(Type==EHansaGameplayCommandType::FundTradeStation);return FundTradeStation;}
	const FHansaCloseTradeStationCommand& FHansaGameplayCommand::GetCloseTradeStation() const{check(Type==EHansaGameplayCommandType::CloseTradeStation);return CloseTradeStation;}
 FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaManageStationOrderCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::ManageStationOrder;C.ManageStationOrder=P;return C;}
 const FHansaManageStationOrderCommand& FHansaGameplayCommand::GetManageStationOrder() const{check(Type==EHansaGameplayCommandType::ManageStationOrder);return ManageStationOrder;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaRequestPresenceUpgradeCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::RequestPresenceUpgrade;C.RequestPresenceUpgrade=P;return C;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaFundPresenceUpgradeCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::FundPresenceUpgrade;C.FundPresenceUpgrade=P;return C;}
	const FHansaRequestPresenceUpgradeCommand& FHansaGameplayCommand::GetRequestPresenceUpgrade() const{check(Type==EHansaGameplayCommandType::RequestPresenceUpgrade);return RequestPresenceUpgrade;}
	const FHansaFundPresenceUpgradeCommand& FHansaGameplayCommand::GetFundPresenceUpgrade() const{check(Type==EHansaGameplayCommandType::FundPresenceUpgrade);return FundPresenceUpgrade;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaApplyPresenceSpecializationCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::ApplyPresenceSpecialization;C.ApplyPresenceSpecialization=P;return C;}
	const FHansaApplyPresenceSpecializationCommand& FHansaGameplayCommand::GetApplyPresenceSpecialization() const{check(Type==EHansaGameplayCommandType::ApplyPresenceSpecialization);return ApplyPresenceSpecialization;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaManageCityPrivilegeCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::ManageCityPrivilege;C.ManageCityPrivilege=P;return C;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaFundCityProjectCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::FundCityProject;C.FundCityProject=P;return C;}
	FHansaGameplayCommand FHansaGameplayCommand::Create(const FHansaCommandHeader& H,const FHansaTransitionCityAuthorityCommand& P){FHansaGameplayCommand C;C.Header=H;C.Type=EHansaGameplayCommandType::TransitionCityAuthority;C.TransitionCityAuthority=P;return C;}
	const FHansaManageCityPrivilegeCommand& FHansaGameplayCommand::GetManageCityPrivilege() const{check(Type==EHansaGameplayCommandType::ManageCityPrivilege);return ManageCityPrivilege;}
	const FHansaFundCityProjectCommand& FHansaGameplayCommand::GetFundCityProject() const{check(Type==EHansaGameplayCommandType::FundCityProject);return FundCityProject;}
	const FHansaTransitionCityAuthorityCommand& FHansaGameplayCommand::GetTransitionCityAuthority() const{check(Type==EHansaGameplayCommandType::TransitionCityAuthority);return TransitionCityAuthority;}

uint64 FHansaGameplayCommand::ComputeStableFingerprint() const
	{
		uint64 Hash = GameplayCommandFnvOffset;
		AddUInt16(Hash, Header.SchemaVersion);
		AddByte(Hash, static_cast<uint8>(Type));
		AddUInt64(Hash, Header.CommandId.GetValue());
		AddUInt32(Hash, Header.CommandId.GetGeneration());
		AddUInt64(Hash, Header.Authority.IssuingHouseId.GetValue());
		AddUInt32(Hash, Header.Authority.IssuingHouseId.GetGeneration());
		AddUInt64(Hash, Header.Authority.PrincipalId);
		AddByte(Hash, static_cast<uint8>(Header.Authority.Origin));
		AddUInt64(Hash, static_cast<uint64>(Header.RequestedExecutionTick.GetValue()));
		AddUInt64(Hash, Header.GlobalSequence);

		switch (Type)
		{
		case EHansaGameplayCommandType::CreateTestEntity:
			AddUInt64(Hash, CreateTestEntity.EntityId.GetValue());
			AddUInt32(Hash, CreateTestEntity.EntityId.GetGeneration());
			AddUInt64(Hash, static_cast<uint64>(CreateTestEntity.InitialValue));
			break;
		case EHansaGameplayCommandType::CancelTestEntity:
			AddUInt64(Hash, CancelTestEntity.EntityId.GetValue());
			AddUInt32(Hash, CancelTestEntity.EntityId.GetGeneration());
			break;
		case EHansaGameplayCommandType::NoOpTest:
			AddUInt64(Hash, NoOpTest.CorrelationValue);
			break;
		case EHansaGameplayCommandType::SetProductionActive:
			AddUInt64(Hash, SetProductionActive.ProductionId.GetValue());
			AddUInt32(Hash, SetProductionActive.ProductionId.GetGeneration());
			AddByte(Hash, SetProductionActive.bActive ? 1 : 0);
			break;
		case EHansaGameplayCommandType::PlaceBuilding:
			AddUInt64(Hash, PlaceBuilding.BuildingId.GetValue());
			AddUInt32(Hash, PlaceBuilding.BuildingId.GetGeneration());
			AddString(Hash, PlaceBuilding.Placement.CityId.ToString());
			AddString(Hash, PlaceBuilding.Placement.BuildingDefinitionId.ToString());
			AddUInt32(Hash, static_cast<uint32>(PlaceBuilding.Placement.Anchor.X));
			AddUInt32(Hash, static_cast<uint32>(PlaceBuilding.Placement.Anchor.Y));
			AddByte(Hash, static_cast<uint8>(PlaceBuilding.Placement.Rotation));
			break;
		case EHansaGameplayCommandType::CancelConstruction:
			AddUInt64(Hash, CancelConstruction.BuildingId.GetValue());
			AddUInt32(Hash, CancelConstruction.BuildingId.GetGeneration());
			break;
		case EHansaGameplayCommandType::RemoveBuilding:
			AddUInt64(Hash, RemoveBuilding.BuildingId.GetValue());
			AddUInt32(Hash, RemoveBuilding.BuildingId.GetGeneration());
			break;
		case EHansaGameplayCommandType::UpgradeResidence:
			AddUInt64(Hash, UpgradeResidence.BuildingId.GetValue());
			AddUInt32(Hash, UpgradeResidence.BuildingId.GetGeneration());
			break;
		case EHansaGameplayCommandType::CreateRoute:
			AddUInt64(Hash, CreateRoute.RouteId.GetValue());
			AddUInt32(Hash, CreateRoute.RouteId.GetGeneration());
			AddUInt64(Hash, CreateRoute.VehicleId.GetValue());
			AddUInt32(Hash, CreateRoute.VehicleId.GetGeneration());
			AddString(Hash, CreateRoute.RouteDefinitionId.ToString());
			AddRouteStops(Hash, CreateRoute.Stops);
			AddByte(Hash, CreateRoute.bActivate ? 1 : 0);
			break;
		case EHansaGameplayCommandType::EditRoute:
			AddUInt64(Hash, EditRoute.RouteId.GetValue());
			AddUInt32(Hash, EditRoute.RouteId.GetGeneration());
			AddRouteStops(Hash, EditRoute.Stops);
			break;
		case EHansaGameplayCommandType::SetRouteActive:
			AddUInt64(Hash, SetRouteActive.RouteId.GetValue());
			AddUInt32(Hash, SetRouteActive.RouteId.GetGeneration());
			AddByte(Hash, SetRouteActive.bActive ? 1 : 0);
			break;
		case EHansaGameplayCommandType::CancelRoute:
			AddUInt64(Hash, CancelRoute.RouteId.GetValue());
			AddUInt32(Hash, CancelRoute.RouteId.GetGeneration());
			break;
case EHansaGameplayCommandType::SetProductionMode:
AddUInt64(Hash, SetProductionMode.ProductionId.GetValue()); AddUInt32(Hash, SetProductionMode.ProductionId.GetGeneration());
AddString(Hash, SetProductionMode.RecipeId.ToString());
AddByte(Hash, SetProductionMode.bFallbackToFresh ? 1 : 0);
break;
case EHansaGameplayCommandType::UpgradeProduction:
AddUInt64(Hash, UpgradeProduction.ProductionId.GetValue()); AddUInt32(Hash, UpgradeProduction.ProductionId.GetGeneration());
break;
case EHansaGameplayCommandType::MoveShip:
AddUInt64(Hash,MoveShip.VehicleId.GetValue()); AddUInt32(Hash,MoveShip.VehicleId.GetGeneration());
AddUInt32(Hash,uint32(MoveShip.Target.X)); AddUInt32(Hash,uint32(MoveShip.Target.Y)); break;
case EHansaGameplayCommandType::SetHouseholdAvailability:
AddUInt64(Hash, SetHouseholdAvailability.MarketBuildingId.GetValue()); AddUInt32(Hash, SetHouseholdAvailability.MarketBuildingId.GetGeneration());
AddString(Hash, SetHouseholdAvailability.GoodId.ToString());
AddByte(Hash, SetHouseholdAvailability.bAvailable ? 1 : 0);
break;
		case EHansaGameplayCommandType::SetHeatingReserve:
			AddUInt64(Hash, SetHeatingReserve.MarketBuildingId.GetValue());
			AddUInt32(Hash, SetHeatingReserve.MarketBuildingId.GetGeneration());
			AddUInt32(Hash, SetHeatingReserve.ReserveDays);
			AddByte(Hash, SetHeatingReserve.bReleaseProtection ? 1 : 0);
			break;
		case EHansaGameplayCommandType::QueueResearch:
			AddString(Hash, QueueResearch.TechnologyId);
			break;
		case EHansaGameplayCommandType::SpotTrade:
			AddUInt64(Hash, SpotTrade.VehicleId.GetValue()); AddUInt32(Hash, SpotTrade.VehicleId.GetGeneration());
			AddString(Hash, SpotTrade.CityId.ToString()); AddString(Hash, SpotTrade.GoodId.ToString());
			AddByte(Hash, static_cast<uint8>(SpotTrade.Side)); AddUInt64(Hash, static_cast<uint64>(SpotTrade.Quantity.GetRawValue()));
			AddUInt64(Hash, static_cast<uint64>(SpotTrade.ReviewedMarketUpdateTick)); AddUInt64(Hash, static_cast<uint64>(SpotTrade.ReviewedUnitPriceMilliMarks));
			break;
		case EHansaGameplayCommandType::ProposeTradeStation:
			AddUInt64(Hash,ProposeTradeStation.StationId.GetValue());AddUInt64(Hash,ProposeTradeStation.FactorId.GetValue());AddUInt64(Hash,ProposeTradeStation.LeasedPlotId.GetValue());AddUInt64(Hash,ProposeTradeStation.InventoryId.GetValue());AddString(Hash,ProposeTradeStation.CityId.ToString());AddString(Hash,ProposeTradeStation.SiteId);break;
		case EHansaGameplayCommandType::FundTradeStation:
			AddUInt64(Hash,FundTradeStation.StationId.GetValue());AddUInt64(Hash,FundTradeStation.FundingInventoryId.GetValue());break;
        case EHansaGameplayCommandType::ManageStationOrder:
            AddUInt64(Hash, ManageStationOrder.StationId.GetValue()); AddUInt32(Hash, ManageStationOrder.StationId.GetGeneration());
            AddUInt64(Hash, ManageStationOrder.OrderId); AddByte(Hash, static_cast<uint8>(ManageStationOrder.Action));
            AddString(Hash, ManageStationOrder.Terms.GoodId.ToString()); AddByte(Hash, static_cast<uint8>(ManageStationOrder.Terms.Side));
            AddUInt64(Hash, ManageStationOrder.Terms.TargetOrReserveMilliUnits); AddUInt64(Hash, ManageStationOrder.Terms.CapMilliUnits); AddUInt64(Hash, ManageStationOrder.Terms.TotalBudgetPfennig);
            AddUInt64(Hash, ManageStationOrder.Terms.LimitUnitPriceMilliMarks); AddUInt64(Hash, ManageStationOrder.Terms.ReviewedMarketUpdateTick); AddUInt64(Hash, ManageStationOrder.Terms.ReviewedUnitPriceMilliMarks); break;
		case EHansaGameplayCommandType::CloseTradeStation:
			AddUInt64(Hash,CloseTradeStation.StationId.GetValue());break;
		case EHansaGameplayCommandType::RequestPresenceUpgrade:
			AddString(Hash,RequestPresenceUpgrade.CityId.ToString());AddString(Hash,RequestPresenceUpgrade.TargetStageId);break;
		case EHansaGameplayCommandType::FundPresenceUpgrade:
			AddString(Hash,FundPresenceUpgrade.CityId.ToString());AddString(Hash,FundPresenceUpgrade.TargetStageId);
			AddUInt64(Hash,FundPresenceUpgrade.FundingInventoryId.GetValue());AddUInt32(Hash,FundPresenceUpgrade.FundingInventoryId.GetGeneration());break;
		case EHansaGameplayCommandType::ApplyPresenceSpecialization:
			AddString(Hash,ApplyPresenceSpecialization.CityId.ToString());AddString(Hash,ApplyPresenceSpecialization.SpecializationId);
			AddUInt64(Hash,ApplyPresenceSpecialization.FundingInventoryId.GetValue());AddUInt32(Hash,ApplyPresenceSpecialization.FundingInventoryId.GetGeneration());
			AddByte(Hash,static_cast<uint8>(ApplyPresenceSpecialization.Action));AddUInt64(Hash,static_cast<uint64>(ApplyPresenceSpecialization.ReviewedRevision));break;
		case EHansaGameplayCommandType::ManageCityPrivilege:
			AddString(Hash,ManageCityPrivilege.CityId.ToString());AddString(Hash,ManageCityPrivilege.PrivilegeId);AddUInt64(Hash,ManageCityPrivilege.FundingInventoryId.GetValue());AddUInt64(Hash,ManageCityPrivilege.GrantedLeaseId.GetValue());AddByte(Hash,(uint8)ManageCityPrivilege.Action);AddUInt64(Hash,(uint64)ManageCityPrivilege.ReviewedRevision);break;
		case EHansaGameplayCommandType::FundCityProject:
			AddString(Hash,FundCityProject.CityId.ToString());AddString(Hash,FundCityProject.ProjectId);AddUInt64(Hash,FundCityProject.FundingInventoryId.GetValue());AddUInt64(Hash,(uint64)FundCityProject.ReviewedRevision);break;
		case EHansaGameplayCommandType::TransitionCityAuthority:
			AddString(Hash,TransitionCityAuthority.CityId.ToString());AddString(Hash,TransitionCityAuthority.CharterId);AddUInt64(Hash,(uint64)TransitionCityAuthority.ReviewedRevision);break;
		default:
			break;
		}
		return Hash;
	}
}
