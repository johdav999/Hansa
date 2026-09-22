#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Save/HansaSaveEnvelope.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPlayerSaveDiagnosticTest, "Hansa.Integration.Save.PlayerArchive",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPlayerSaveDiagnosticTest::RunTest(const FString& Parameters)
{
 using namespace Hansa::Simulation;
 FString Path;
 if (!FParse::Value(FCommandLine::Get(), TEXT("HansaInspectSave="), Path)) return true;
 TArray<uint8> Bytes;
 if (!TestTrue(TEXT("Read requested archive"), FFileHelper::LoadFileToArray(Bytes, *Path))) return false;
 FHansaSaveMetadata Metadata;
 if (!TestTrue(TEXT("Archive integrity"), FHansaSaveEnvelope::InspectMetadata(Bytes, Metadata).IsSuccess())) return false;
 FHansaEconomicRegistry Registry; FString Error;
 if (!FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Registry, Error)) { AddError(Error); return false; }
 auto Placement = Hansa::Game::LubeckPlacementGrid::TryBuildSurveyInitialization(FHansaHouseId::TryCreate(1).Value, Registry);
 if (!Placement) return false;
 FHansaLubeckScenarioState Scenario;
 auto Kind = Metadata.ScenarioId == FHansaLubeckScenarioInitializer::EmptyBuildId ? EHansaRuntimeScenario::EmptyLubeckBuild : EHansaRuntimeScenario::LubeckGrainShortage;
 if (!FHansaLubeckScenarioInitializer::TryCreate(Kind, MoveTemp(Registry), Placement.Value, Scenario, Error, 0, true)) { AddError(Error); return false; }
 FHansaSaveSnapshot Snapshot;
 const auto Result = FHansaSaveEnvelope::Decode(Bytes, Scenario.Definitions, Snapshot);
 TestTrue(*Result.Message, Result.IsSuccess());
 if (Result) AddInfo(FHansaStateHasher::Compute(Snapshot.State, Scenario.Definitions).ToCompactDebugString());
 return !HasAnyErrors();
}
#endif
