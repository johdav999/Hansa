#pragma once

#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	enum class EHansaCommandOrigin : uint8
	{
		PlayerInput = 0,
		ArtificialIntelligence,
		MultiplayerRpc,
		ControlledAutomation
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaCommandOrigin Origin);

	/** Transport-neutral authority claim. Every origin is subject to the same authoritative validation. */
	struct FHansaCommandAuthorityContext
	{
		FHansaHouseId IssuingHouseId;
		uint64 PrincipalId = 0;
		EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput;
	};

	struct FHansaCommandHeader
	{
		static constexpr uint16 CurrentSchemaVersion = 1;

		FHansaCommandId CommandId;
		FHansaCommandAuthorityContext Authority;
		FHansaSimulationTick RequestedExecutionTick;
		uint64 GlobalSequence = 0;
		uint16 SchemaVersion = CurrentSchemaVersion;
	};

	struct FHansaCreateTestEntityCommand
	{
		FHansaTestEntityId EntityId;
		int64 InitialValue = 0;
	};

	struct FHansaCancelTestEntityCommand
	{
		FHansaTestEntityId EntityId;
	};

	struct FHansaNoOpTestCommand
	{
		int64 CorrelationValue = 0;
	};

	enum class EHansaGameplayCommandType : uint8
	{
		CreateTestEntity = 0,
		CancelTestEntity,
		NoOpTest
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaGameplayCommandType Type);

	/**
	 * Closed typed command envelope for the current protocol version. Callers cannot provide a trusted fingerprint;
	 * it is derived from the complete header and active payload by deterministic code.
	 */
	class HANSASIMULATION_API FHansaGameplayCommand final
	{
	public:
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaCreateTestEntityCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaCancelTestEntityCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaNoOpTestCommand& Payload);

		[[nodiscard]] const FHansaCommandHeader& GetHeader() const { return Header; }
		[[nodiscard]] EHansaGameplayCommandType GetType() const { return Type; }
		[[nodiscard]] const FHansaCreateTestEntityCommand& GetCreateTestEntity() const;
		[[nodiscard]] const FHansaCancelTestEntityCommand& GetCancelTestEntity() const;
		[[nodiscard]] const FHansaNoOpTestCommand& GetNoOpTest() const;
		[[nodiscard]] uint64 ComputeStableFingerprint() const;

	private:
		FHansaCommandHeader Header;
		EHansaGameplayCommandType Type = EHansaGameplayCommandType::NoOpTest;
		FHansaCreateTestEntityCommand CreateTestEntity;
		FHansaCancelTestEntityCommand CancelTestEntity;
		FHansaNoOpTestCommand NoOpTest;
	};
}
