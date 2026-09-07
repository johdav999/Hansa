#if WITH_DEV_AUTOMATION_TESTS

#include "AI/HansaMerchantAI.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Simulation;

namespace Hansa::Tests::MerchantAI
{
	const FHansaRouteProjection* FindRivalRoute(const FHansaSimulationProjection& Projection, const FHansaHouseId RivalHouseId)
	{
		for (const FHansaRouteProjection& Route : Projection.GetRoutes())
		{
			if (Route.OwnerId == RivalHouseId) return &Route;
		}
		return nullptr;
	}

	const FHansaHouseResearchState* FindRivalResearch(const FHansaSimulationProjection& Projection, const FHansaHouseId RivalHouseId)
	{
		for (const FHansaHouseResearchState& Research : Projection.GetResearch())
		{
			if (Research.HouseId == RivalHouseId) return &Research;
		}
		return nullptr;
	}

	bool TraceEqual(const FHansaMerchantAIDecisionTrace& Left, const FHansaMerchantAIDecisionTrace& Right)
	{
		if (Left.DecisionTick != Right.DecisionTick || Left.SelectedGoal != Right.SelectedGoal ||
			Left.ChosenOptionId != Right.ChosenOptionId || Left.bCommandAccepted != Right.bCommandAccepted ||
			Left.GatewayError != Right.GatewayError || Left.ConsideredOptions.Num() != Right.ConsideredOptions.Num()) return false;
		for (int32 Index = 0; Index < Left.ConsideredOptions.Num(); ++Index)
		{
			const FHansaMerchantAIConsideredOption& A = Left.ConsideredOptions[Index];
			const FHansaMerchantAIConsideredOption& B = Right.ConsideredOptions[Index];
			if (A.Kind != B.Kind || A.StableOptionId != B.StableOptionId || A.Utility != B.Utility ||
				A.SeededTieBreak != B.SeededTieBreak || A.bEligible != B.bEligible || A.Reason != B.Reason) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantAIRivalScenarioTest,
	"Hansa.Simulation.AI.RivalRecoversShortageResearchesAndTrades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMerchantAIRivalScenarioTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("The merchant-rival scenario initializes"), Host->InitializeForLubeck(nullptr, Error)))
	{
		AddError(Error);
		return false;
	}
	const FHansaHouseId RivalHouseId = Host->GetRivalHouseId();
	TestTrue(TEXT("The rival has a separate stable house identity"), RivalHouseId.IsValid() && RivalHouseId != Host->GetHouseId());
	const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
	TestTrue(TEXT("Exactly one authored merchant tuning is compiled"), Registry != nullptr && Registry->GetMerchantAITunings().Num() == 1);

	TestTrue(TEXT("The deterministic rival runs through two complete trade legs"), Host->AdvanceTicks(55));
	const auto ProjectionResult = Host->BuildProjection();
	if (!TestTrue(TEXT("The post-AI scenario remains projectable"), ProjectionResult.IsSuccess())) return false;
	bool bGrainReserveRecovered = false;
	for (const FHansaCityMarketProjection& Market : ProjectionResult.Value.GetMarkets())
	{
		if (Market.CityId.ToString() == TEXT("City.Lubeck") && Market.GoodId.ToString() == TEXT("Good.Grain"))
		{
			bGrainReserveRecovered = Market.CurrentStock.GetRawValue() >= Market.DesiredReserve.GetRawValue();
			break;
		}
	}
	TestTrue(TEXT("The rival delivery recovers Lübeck grain to its authored reserve"), bGrainReserveRecovered);
	const FHansaRouteProjection* Route = Hansa::Tests::MerchantAI::FindRivalRoute(ProjectionResult.Value, RivalHouseId);
	if (!TestNotNull(TEXT("The rival owns one visible route"), Route)) return false;
	TestTrue(TEXT("The rival activated and operated its normal route"), Route->Lifecycle != EHansaRouteLifecycleState::Inactive);
	TestTrue(TEXT("The rival completed its bounded trade objective"), Route->CompletedLegCount >= 2);
	TestTrue(TEXT("The rival transferred cargo without direct stock mutation"),
		Route->LastTransfer.Outcome == EHansaRouteTransferOutcome::Completed ||
		Route->LastTransfer.Outcome == EHansaRouteTransferOutcome::Partial);

	const FHansaHouseResearchState* Research = Hansa::Tests::MerchantAI::FindRivalResearch(ProjectionResult.Value, RivalHouseId);
	if (!TestNotNull(TEXT("The rival has independent research state"), Research)) return false;
	TestTrue(TEXT("The rival completed its first preferred research through QueueResearch"),
		Research->IsCompleted(TEXT("Technology.Commerce.MarketReports")));

	const TConstArrayView<FHansaMerchantAIDecisionTrace> History = Host->GetMerchantAIDecisionHistory();
	TestTrue(TEXT("Decision diagnostics retain multiple bounded cadence evaluations"), History.Num() >= 3 && History.Num() <= 32);
	bool bTracedRoute = false;
	bool bTracedResearch = false;
	bool bTracedKnownFacts = false;
	for (const FHansaMerchantAIDecisionTrace& Trace : History)
	{
		bTracedRoute |= Trace.bCommandAccepted && Trace.SubmittedCommandType.IsSet() &&
			Trace.SubmittedCommandType.GetValue() == EHansaGameplayCommandType::SetRouteActive;
		bTracedResearch |= Trace.bCommandAccepted && Trace.SubmittedCommandType.IsSet() &&
			Trace.SubmittedCommandType.GetValue() == EHansaGameplayCommandType::QueueResearch;
		bTracedKnownFacts |= !Trace.KnownFacts.IsEmpty() && !Trace.ConsideredOptions.IsEmpty();
	}
	TestTrue(TEXT("At least one accepted normal route command is traceable"), bTracedRoute);
	TestTrue(TEXT("At least one accepted normal research command is traceable"), bTracedResearch);
	TestTrue(TEXT("Diagnostics expose known facts and considered options"), bTracedKnownFacts);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantAIDeterminismTest,
	"Hansa.Simulation.AI.SeededCadenceAndTieBreakDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMerchantAIDeterminismTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> First(NewObject<UHansaRuntimeSimulationHost>());
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Second(NewObject<UHansaRuntimeSimulationHost>());
	FString FirstError;
	FString SecondError;
	if (!TestTrue(TEXT("First deterministic rival initializes"), First->InitializeForLubeck(nullptr, FirstError)) ||
		!TestTrue(TEXT("Second deterministic rival initializes"), Second->InitializeForLubeck(nullptr, SecondError)))
	{
		AddError(FirstError + TEXT(" ") + SecondError);
		return false;
	}
	TestTrue(TEXT("First rival run advances"), First->AdvanceTicks(55));
	TestTrue(TEXT("Second rival run advances"), Second->AdvanceTicks(55));
	const auto FirstProjection = First->BuildProjection();
	const auto SecondProjection = Second->BuildProjection();
	if (!FirstProjection || !SecondProjection) return false;
	TestEqual(TEXT("Equal seeds and observations produce the same authoritative checksum"),
		FirstProjection.Value.GetFingerprint().Value, SecondProjection.Value.GetFingerprint().Value);
	const auto FirstHistory = First->GetMerchantAIDecisionHistory();
	const auto SecondHistory = Second->GetMerchantAIDecisionHistory();
	TestEqual(TEXT("Decision cadence produces the same trace count"), FirstHistory.Num(), SecondHistory.Num());
	for (int32 Index = 0; Index < FMath::Min(FirstHistory.Num(), SecondHistory.Num()); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Decision trace %d is deterministic"), Index),
			Hansa::Tests::MerchantAI::TraceEqual(FirstHistory[Index], SecondHistory[Index]));
	}
	return !HasAnyErrors();
}

#endif
