#include "Misc/AutomationTest.h"
#include "Multiplayer/HansaHouseControl.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Hansa::Simulation;

namespace
{
	template <typename TId> TId Id(const uint64 Value) { return TId::TryCreate(Value).Value; }

	TArray<FHansaHouseControlState> MakeRoster(const int32 HumanCount)
	{
		TArray<FHansaHouseControlState> States;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const bool bHuman = Index < HumanCount;
			States.Add({Id<FHansaHouseId>(Index + 1),
				bHuman ? EHansaHouseController::Human : EHansaHouseController::AI,
				bHuman ? Id<FHansaParticipantId>(100 + Index) : FHansaParticipantId(),
				{true, true}, 1});
		}
		return States;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaEightHouseControlTransitionsTest,
	"Hansa.Multiplayer.HouseControl.EightHouseTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEightHouseControlTransitionsTest::RunTest(const FString& Parameters)
{
	FString Error;
	FHansaHouseControlRoster ZeroAIRoster;
	TestTrue(TEXT("Eight-human zero-AI roster validates"), ZeroAIRoster.Initialize(MakeRoster(8), Error));
	for (int32 Index = 1; Index <= 8; ++Index)
	{
		TestTrue(TEXT("Every human house has one controller"), ZeroAIRoster.IsHumanControlled(Id<FHansaHouseId>(Index)));
		TestFalse(TEXT("A human house has no competing AI stream"), ZeroAIRoster.IsAIControlled(Id<FHansaHouseId>(Index)));
	}

	FHansaHouseControlRoster Mixed;
	TestTrue(TEXT("Mixed human/AI roster validates"), Mixed.Initialize(MakeRoster(3), Error));
	const FHansaHouseId TakeoverHouse = Id<FHansaHouseId>(4);
	const FHansaParticipantId NewHuman = Id<FHansaParticipantId>(204);
	const uint64 BeforeEpoch = Mixed.Find(TakeoverHouse)->ControlEpoch;
	TestTrue(TEXT("Authorized human atomically replaces AI"), Mixed.ClaimForHuman(TakeoverHouse, NewHuman, Error));
	TestTrue(TEXT("Taken-over house is human controlled"), Mixed.IsHumanControlled(TakeoverHouse));
	TestFalse(TEXT("Taken-over house immediately stops AI"), Mixed.IsAIControlled(TakeoverHouse));
	TestEqual(TEXT("Takeover increments the control epoch once"), Mixed.Find(TakeoverHouse)->ControlEpoch, BeforeEpoch + 1);
	TestFalse(TEXT("A simultaneous second claimant cannot double-claim the house"),
		Mixed.ClaimForHuman(TakeoverHouse, Id<FHansaParticipantId>(205), Error));
	TestTrue(TEXT("Session policy resumes AI after human release"), Mixed.ReleaseHuman(NewHuman, Error));
	TestTrue(TEXT("Released house has exactly one resumed AI controller"), Mixed.IsAIControlled(TakeoverHouse));
	TestFalse(TEXT("Released house no longer has a human stream"), Mixed.IsHumanControlled(TakeoverHouse));

	TArray<FHansaHouseControlState> NoResume = MakeRoster(0);
	NoResume[3].Policy.bResumeAIOnHumanRelease = false;
	FHansaHouseControlRoster PolicyRoster;
	TestTrue(TEXT("No-resume policy roster validates"), PolicyRoster.Initialize(NoResume, Error));
	TestTrue(TEXT("Human takeover works under no-resume policy"), PolicyRoster.ClaimForHuman(TakeoverHouse, NewHuman, Error));
	TestTrue(TEXT("Human releases no-resume house"), PolicyRoster.ReleaseHuman(NewHuman, Error));
	TestFalse(TEXT("AI resumes only when session policy permits"), PolicyRoster.IsAIControlled(TakeoverHouse));
	TestFalse(TEXT("No-resume house is not human controlled"), PolicyRoster.IsHumanControlled(TakeoverHouse));
	return !HasAnyErrors();
}

#endif
