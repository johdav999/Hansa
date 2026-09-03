#include "Commands/HansaGameplayCommand.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint64 FnvOffset = 14695981039346656037ULL;
		constexpr uint64 FnvPrime = 1099511628211ULL;

		void AddByte(uint64& Hash, const uint8 Value)
		{
			Hash ^= Value;
			Hash *= FnvPrime;
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

	uint64 FHansaGameplayCommand::ComputeStableFingerprint() const
	{
		uint64 Hash = FnvOffset;
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
		default:
			break;
		}
		return Hash;
	}
}
