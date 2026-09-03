#pragma once

#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Placement/HansaPlacement.h"

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
		static constexpr uint16 CurrentSchemaVersion = 4;

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

	/** Authoritative activation change; stock and output remain consequences of normal simulation systems. */
	struct FHansaSetProductionActiveCommand
	{
		FHansaProductionId ProductionId;
		bool bActive = true;
	};

	/** Normal authoritative build command. Preview and automation submit this same payload. */
	struct FHansaPlaceBuildingCommand
	{
		FHansaBuildingId BuildingId;
		FHansaPlacementSpec Placement;
	};

	struct FHansaCancelConstructionCommand
	{
		FHansaBuildingId BuildingId;
	};

	struct FHansaRemoveBuildingCommand
	{
		FHansaBuildingId BuildingId;
	};

	/** Manual MVP residence progression; the authored upgrade target determines the next tier. */
	struct FHansaUpgradeResidenceCommand
	{
		FHansaBuildingId BuildingId;
	};

	enum class EHansaGameplayCommandType : uint8
	{
		CreateTestEntity = 0,
		CancelTestEntity,
		NoOpTest,
		SetProductionActive,
		PlaceBuilding,
		CancelConstruction,
		RemoveBuilding,
		UpgradeResidence
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
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaSetProductionActiveCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaPlaceBuildingCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaCancelConstructionCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaRemoveBuildingCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaUpgradeResidenceCommand& Payload);

		[[nodiscard]] const FHansaCommandHeader& GetHeader() const { return Header; }
		[[nodiscard]] EHansaGameplayCommandType GetType() const { return Type; }
		[[nodiscard]] const FHansaCreateTestEntityCommand& GetCreateTestEntity() const;
		[[nodiscard]] const FHansaCancelTestEntityCommand& GetCancelTestEntity() const;
		[[nodiscard]] const FHansaNoOpTestCommand& GetNoOpTest() const;
		[[nodiscard]] const FHansaSetProductionActiveCommand& GetSetProductionActive() const;
		[[nodiscard]] const FHansaPlaceBuildingCommand& GetPlaceBuilding() const;
		[[nodiscard]] const FHansaCancelConstructionCommand& GetCancelConstruction() const;
		[[nodiscard]] const FHansaRemoveBuildingCommand& GetRemoveBuilding() const;
		[[nodiscard]] const FHansaUpgradeResidenceCommand& GetUpgradeResidence() const;
		[[nodiscard]] uint64 ComputeStableFingerprint() const;

	private:
		FHansaCommandHeader Header;
		EHansaGameplayCommandType Type = EHansaGameplayCommandType::NoOpTest;
		FHansaCreateTestEntityCommand CreateTestEntity;
		FHansaCancelTestEntityCommand CancelTestEntity;
		FHansaNoOpTestCommand NoOpTest;
		FHansaSetProductionActiveCommand SetProductionActive;
		FHansaPlaceBuildingCommand PlaceBuilding;
		FHansaCancelConstructionCommand CancelConstruction;
		FHansaRemoveBuildingCommand RemoveBuilding;
		FHansaUpgradeResidenceCommand UpgradeResidence;
	};
}
