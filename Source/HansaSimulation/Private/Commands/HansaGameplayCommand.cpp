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
		default:
			break;
		}
		return Hash;
	}
}
