#include "Multiplayer/HansaHouseControl.h"

#include "Containers/Set.h"

namespace Hansa::Simulation
{
	bool FHansaHouseControlRoster::Initialize(
		TConstArrayView<FHansaHouseControlState> InitialStates, FString& OutError)
	{
		if (InitialStates.Num() < MinimumHouses || InitialStates.Num() > MaximumHouses)
		{
			OutError = TEXT("A house-control roster requires two through eight houses.");
			return false;
		}
		TSet<FHansaHouseId> Houses;
		TSet<FHansaParticipantId> Humans;
		for (const FHansaHouseControlState& State : InitialStates)
		{
			const bool bHuman = State.Controller == EHansaHouseController::Human;
			if (!State.HouseId.IsValid() || Houses.Contains(State.HouseId) ||
				bHuman != State.HumanParticipantId.IsValid() ||
				(State.HumanParticipantId.IsValid() && Humans.Contains(State.HumanParticipantId)) ||
				State.ControlEpoch == 0)
			{
				OutError = TEXT("House-control identities, controller ownership, or epochs are invalid.");
				return false;
			}
			Houses.Add(State.HouseId);
			if (State.HumanParticipantId.IsValid()) Humans.Add(State.HumanParticipantId);
		}
		States.Reset(InitialStates.Num());
		States.Append(InitialStates.GetData(), InitialStates.Num());
		States.Sort([](const FHansaHouseControlState& Left, const FHansaHouseControlState& Right)
		{
			return Left.HouseId < Right.HouseId;
		});
		OutError.Reset();
		return true;
	}

	const FHansaHouseControlState* FHansaHouseControlRoster::Find(const FHansaHouseId HouseId) const
	{
		return States.FindByPredicate([HouseId](const FHansaHouseControlState& State)
		{
			return State.HouseId == HouseId;
		});
	}

	bool FHansaHouseControlRoster::ClaimForHuman(
		const FHansaHouseId HouseId, const FHansaParticipantId ParticipantId, FString& OutError)
	{
		if (!ParticipantId.IsValid())
		{
			OutError = TEXT("A human controller requires a valid participant identity.");
			return false;
		}
		if (States.ContainsByPredicate([ParticipantId](const FHansaHouseControlState& State)
			{ return State.HumanParticipantId == ParticipantId; }))
		{
			OutError = TEXT("The participant already controls a house.");
			return false;
		}
		FHansaHouseControlState* State = States.FindByPredicate([HouseId](const FHansaHouseControlState& Candidate)
			{ return Candidate.HouseId == HouseId; });
		if (State == nullptr)
		{
			OutError = TEXT("The requested house is not in the scenario roster.");
			return false;
		}
		if (State->Controller == EHansaHouseController::Human)
		{
			OutError = TEXT("The requested house already has a human controller.");
			return false;
		}
		if (State->Controller == EHansaHouseController::AI && !State->Policy.bAllowHumanTakeover)
		{
			OutError = TEXT("The session policy does not allow takeover of this AI house.");
			return false;
		}
		State->Controller = EHansaHouseController::Human;
		State->HumanParticipantId = ParticipantId;
		++State->ControlEpoch;
		OutError.Reset();
		return true;
	}

	bool FHansaHouseControlRoster::ReleaseHuman(const FHansaParticipantId ParticipantId, FString& OutError)
	{
		FHansaHouseControlState* State = States.FindByPredicate([ParticipantId](const FHansaHouseControlState& Candidate)
			{ return Candidate.Controller == EHansaHouseController::Human && Candidate.HumanParticipantId == ParticipantId; });
		if (State == nullptr)
		{
			OutError = TEXT("The participant does not own an active house-control lease.");
			return false;
		}
		State->HumanParticipantId = FHansaParticipantId();
		State->Controller = State->Policy.bResumeAIOnHumanRelease
			? EHansaHouseController::AI : EHansaHouseController::Dormant;
		++State->ControlEpoch;
		OutError.Reset();
		return true;
	}

	bool FHansaHouseControlRoster::IsAIControlled(const FHansaHouseId HouseId) const
	{
		const FHansaHouseControlState* State = Find(HouseId);
		return State != nullptr && State->Controller == EHansaHouseController::AI;
	}

	bool FHansaHouseControlRoster::IsHumanControlled(const FHansaHouseId HouseId) const
	{
		const FHansaHouseControlState* State = Find(HouseId);
		return State != nullptr && State->Controller == EHansaHouseController::Human;
	}
}
