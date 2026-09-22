#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Model/HansaIds.h"

namespace Hansa::Simulation
{
	enum class EHansaHouseController : uint8
	{
		Human = 0,
		AI,
		Dormant
	};

	struct HANSASIMULATION_API FHansaHouseControlPolicy final
	{
		bool bAllowHumanTakeover = true;
		bool bResumeAIOnHumanRelease = true;
	};

	struct HANSASIMULATION_API FHansaHouseControlState final
	{
		FHansaHouseId HouseId;
		EHansaHouseController Controller = EHansaHouseController::Dormant;
		FHansaParticipantId HumanParticipantId;
		FHansaHouseControlPolicy Policy;
		uint64 ControlEpoch = 1;
	};

	/**
	 * Server-owned controller lease table. Transitions replace the prior controller in one operation,
	 * so a house can never have simultaneous human and AI command streams.
	 */
	class HANSASIMULATION_API FHansaHouseControlRoster final
	{
	public:
		static constexpr int32 MinimumHouses = 2;
		static constexpr int32 MaximumHouses = 8;

		bool Initialize(TConstArrayView<FHansaHouseControlState> InitialStates, FString& OutError);
		bool ClaimForHuman(FHansaHouseId HouseId, FHansaParticipantId ParticipantId, FString& OutError);
		bool ReleaseHuman(FHansaParticipantId ParticipantId, FString& OutError);
		[[nodiscard]] bool IsAIControlled(FHansaHouseId HouseId) const;
		[[nodiscard]] bool IsHumanControlled(FHansaHouseId HouseId) const;
		[[nodiscard]] const FHansaHouseControlState* Find(FHansaHouseId HouseId) const;
		[[nodiscard]] TConstArrayView<FHansaHouseControlState> GetStates() const { return States; }

	private:
		TArray<FHansaHouseControlState> States;
	};
}
