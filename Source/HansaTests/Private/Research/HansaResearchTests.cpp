#include "Misc/AutomationTest.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Research/HansaResearch.h"
#include "Systems/HansaSimulationPipeline.h"

using namespace Hansa::Simulation;

namespace
{
	TArray<FHansaCompiledTechnologyDefinition> MakeTechnologies()
	{
		return {
			{TEXT("Technology.Commerce.Root"),TEXT("Reports"),EHansaResearchBranch::Commerce,{},100,2,TEXT("Better reports"),{{EHansaResearchEffectKind::MarketReportAgeReductionTicks,TEXT("City.Lubeck"),5}},1},
			{TEXT("Technology.Commerce.Credit"),TEXT("Credit"),EHansaResearchBranch::Commerce,{TEXT("Technology.Commerce.Root")},150,2,TEXT("Lower friction"),{{EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints,TEXT("City.Lubeck"),500}},2},
			{TEXT("Technology.Production.Root"),TEXT("Milling"),EHansaResearchBranch::Production,{},100,1,TEXT("Faster mill"),{{EHansaResearchEffectKind::ProductionThroughputBasisPoints,TEXT("Recipe.MillFlour"),1000}},3},
			{TEXT("Technology.Logistics.Root"),TEXT("Handling"),EHansaResearchBranch::Logistics,{},100,1,TEXT("Faster handling"),{{EHansaResearchEffectKind::WarehouseHandlingBasisPoints,TEXT("Building.Warehouse"),1000}},4}
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResearchGraphValidationTest,
	"Hansa.Simulation.Research.MissingCycleAndUnreachable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaResearchGraphValidationTest::RunTest(const FString&)
{
	auto Technologies=MakeTechnologies();
	Technologies.Add({TEXT("Technology.Orphan"),TEXT("Orphan"),EHansaResearchBranch::Commerce,{},1,1,TEXT("Orphan"),{},5});
	Technologies.Add({TEXT("Technology.CycleA"),TEXT("A"),EHansaResearchBranch::Commerce,{TEXT("Technology.CycleB")},1,1,TEXT("A"),{},6});
	Technologies.Add({TEXT("Technology.CycleB"),TEXT("B"),EHansaResearchBranch::Commerce,{TEXT("Technology.CycleA"),TEXT("Technology.Missing")},1,1,TEXT("B"),{},7});
	TSet<FString> Known={TEXT("City.Lubeck"),TEXT("Recipe.MillFlour"),TEXT("Building.Warehouse")};
	for(const auto& T:Technologies) Known.Add(T.StableId);
	const TArray<FString> Roots={TEXT("Technology.Commerce.Root"),TEXT("Technology.Production.Root"),TEXT("Technology.Logistics.Root")};
	const auto Diagnostics=FHansaResearchGraphValidator::Validate(Technologies,Roots,Known);
	TestTrue(TEXT("Missing prerequisite is reported"),Diagnostics.ContainsByPredicate([](const auto& D){return D.Issue==EHansaResearchGraphIssue::MissingNode && D.RelatedStableId==TEXT("Technology.Missing");}));
	TestTrue(TEXT("Cycle is reported"),Diagnostics.ContainsByPredicate([](const auto& D){return D.Issue==EHansaResearchGraphIssue::Cycle;}));
	TestTrue(TEXT("Orphan is unreachable from declared branch roots"),Diagnostics.ContainsByPredicate([](const auto& D){return D.Issue==EHansaResearchGraphIssue::Unreachable && D.TechnologyId==TEXT("Technology.Orphan");}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResearchQueueEffectsSerializationTest,
	"Hansa.Simulation.Research.PrerequisitesEffectsAndSerializationReadiness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaResearchQueueEffectsSerializationTest::RunTest(const FString&)
{
	const auto Technologies=MakeTechnologies();
	const auto House=FHansaHouseId::TryCreate(1,1).Value;
	TArray<FHansaHouseResearchState> States={{House,500}};
	TestEqual(TEXT("Dependent technology is rejected"),FHansaResearchExecutor::TryQueue(States,House,TEXT("Technology.Commerce.Credit"),Technologies).Error,EHansaResearchQueueError::PrerequisiteMissing);
	TestTrue(TEXT("Root queues through one authoritative slot"),FHansaResearchExecutor::TryQueue(States,House,TEXT("Technology.Commerce.Root"),Technologies).IsSuccess());
	TestEqual(TEXT("Cost is charged atomically"),States[0].AvailableResearchPoints,400);
	TestEqual(TEXT("Second queue item is rejected"),FHansaResearchExecutor::TryQueue(States,House,TEXT("Technology.Production.Root"),Technologies).Error,EHansaResearchQueueError::QueueFull);
	TArray<FHansaResearchCompletion> Completed; FHansaResearchExecutor::AdvanceOneTick(States,Technologies,Completed); FHansaResearchExecutor::AdvanceOneTick(States,Technologies,Completed);
	TestTrue(TEXT("Prerequisite completes"),States[0].IsCompleted(TEXT("Technology.Commerce.Root")));
	TestEqual(TEXT("Stable-target effect applies exactly once"),States[0].GetEffectMagnitude(EHansaResearchEffectKind::MarketReportAgeReductionTicks,TEXT("City.Lubeck")),5);
	TestTrue(TEXT("Dependent technology becomes queueable"),FHansaResearchExecutor::TryQueue(States,House,TEXT("Technology.Commerce.Credit"),Technologies).IsSuccess());

	FHansaHouseResearchInitialization Serialized{States[0].HouseId,States[0].AvailableResearchPoints,States[0].ActiveTechnologyId,States[0].ProgressTicks,States[0].CompletedTechnologyIds,States[0].AppliedEffects};
	TArray<FHansaHouseResearchState> Restored;
	TestTrue(TEXT("Canonical restore accepts snapshot-shaped serialization data"),FHansaResearchExecutor::RestoreCanonical(Restored,MakeArrayView(&Serialized,1),Technologies));
	TestEqual(TEXT("Active stable ID round-trips"),Restored[0].ActiveTechnologyId,States[0].ActiveTechnologyId);
	TestEqual(TEXT("Applied effects round-trip canonically"),Restored[0].AppliedEffects,States[0].AppliedEffects);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResearchAuthoritativePipelineTest,
	"Hansa.Simulation.Research.AuthoritativeCommandProgressAndHash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaResearchAuthoritativePipelineTest::RunTest(const FString&)
{
	const auto Technologies=MakeTechnologies();
	FHansaEconomicRegistry Registry({}, {}, {}, 0xAA55, {}, {}, {}, {}, {}, Technologies);
	const auto Scenario=FHansaScenarioId::TryParse(TEXT("Scenario.ResearchTest")).Value;
	FHansaSimulationDefinitionContext Definitions=FHansaSimulationDefinitionContext::TryCreate(Scenario,0xAA55,MoveTemp(Registry)).Value;
	const auto House=FHansaHouseId::TryCreate(1,1).Value;
	FHansaSimulationInitialization Initial;
	Initial.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value).Value;
	Initial.Houses={{House,FHansaMoney::FromRaw(1000)}};
	Initial.Research={{House,500}};
	FHansaSimulationState State=FHansaSimulationState::TryCreate(MoveTemp(Initial)).Value;
	FHansaSimulationTransientCache Cache;
	FHansaCommandHeader Header; Header.CommandId=FHansaCommandId::TryCreate(1,1).Value;Header.Authority.IssuingHouseId=House;Header.Authority.PrincipalId=1;Header.RequestedExecutionTick=FHansaSimulationTick::TryCreate(0).Value;Header.GlobalSequence=1;
	const FHansaGameplayCommand Queue=FHansaGameplayCommand::Create(Header,FHansaQueueResearchCommand{TEXT("Technology.Commerce.Root")});
	const auto Queued=FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,MakeArrayView(&Queue,1),Cache);
	TestTrue(TEXT("Queue command succeeds through the sole mutation gateway"),Queued.IsSuccess());
	TestTrue(TEXT("Queue event is published"),Queued.GetEvents().ContainsByPredicate([](const FHansaDomainEvent& Event){return Event.GetType()==EHansaDomainEventType::ResearchQueued;}));
	const uint64 HashAfterQueue=State.CreateReadOnlyAccess(Definitions).BuildStateHashReport().GetOverallHash();
	const auto Completed=FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,{},Cache);
	TestTrue(TEXT("Second server tick completes the two-tick technology"),Completed.GetEvents().ContainsByPredicate([](const FHansaDomainEvent& Event){return Event.GetType()==EHansaDomainEventType::ResearchCompleted;}));
	const auto Research=State.CreateReadOnlyAccess(Definitions).GetResearch();
	TestTrue(TEXT("Completion and effects are visible through read-only projection"),Research.Num()==1&&Research[0].IsCompleted(TEXT("Technology.Commerce.Root"))&&Research[0].AppliedEffects.Num()==1);
	TestNotEqual(TEXT("Research progress participates in deterministic state hash"),State.CreateReadOnlyAccess(Definitions).BuildStateHashReport().GetOverallHash(),HashAfterQueue);
	return true;
}
