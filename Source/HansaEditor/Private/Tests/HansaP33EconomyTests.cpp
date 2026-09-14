#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaDefinitionBase.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"
#include "Save/HansaSaveEnvelope.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Editor::Tests::EconomyP33
{
using namespace Hansa::Simulation;
struct FRun
{
	FHansaLubeckScenarioState Scenario;
	FHansaSimulationTransientCache Cache;
	uint64 Sequence = 0;
	bool Step() { return FHansaGameplayCommandGateway::ExecuteTick(Scenario.State, Scenario.Definitions, {}, Cache).IsSuccess(); }
	template<class T> bool Command(const T& Payload)
	{
		FHansaCommandHeader Header;
		Header.CommandId = FHansaCommandId::TryCreate(++Sequence).Value;
		Header.GlobalSequence = Sequence;
		Header.Authority.IssuingHouseId = Scenario.HouseId;
		Header.Authority.PrincipalId = 1;
		Header.RequestedExecutionTick = Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).GetClock().GetTick();
		TArray<FHansaGameplayCommand> Commands { FHansaGameplayCommand::Create(Header, Payload) };
		return FHansaGameplayCommandGateway::ExecuteTick(Scenario.State, Scenario.Definitions, Commands, Cache).IsSuccess();
	}
	bool Production(uint64 Id, bool Active) { return Command(FHansaSetProductionActiveCommand { FHansaProductionId::TryCreate(Id).Value, Active }); }
	int64 Stock(const TCHAR* Good, uint64 Inventory = 1) const
	{
		const auto Stock = Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).GetInventories().QueryStock(
			FHansaInventoryId::TryCreate(Inventory).Value, FHansaGoodId::TryParse(Good).Value);
		return Stock.IsSet() ? Stock->Available.GetRawValue() : -1;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaP33EconomyPlaythroughTest,
	"Hansa.Integration.EconomyP33.Playthroughs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaP33EconomyPlaythroughTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Editor::Tests::EconomyP33;
	FString Error;
	auto Candidate = Hansa::Editor::EconomicDefinitions::CreateP33EconomyCandidate(Error);
	if (!TestFalse(*Error, Candidate.IsEmpty())) return false;
	TArray<const UHansaDefinitionBase*> Definitions;
	for (const auto& D : Candidate) Definitions.Add(D.Get());
	const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Definitions);
	if (!TestTrue(TEXT("Candidate passes the production definition compiler"), Compiled.IsValid())) return false;
	TestEqual(TEXT("Exactly ten goods"), Compiled.Registry.GetGoods().Num(), 10);
	TArray<FString> Chains;
	for (const auto& Building : Compiled.Registry.GetBuildings())
		if (Building.bShowInConstructionMenu && !Building.ConstructionChainOutputGoodId.IsEmpty()) Chains.AddUnique(Building.ConstructionChainOutputGoodId);
	Chains.Sort();
	TestTrue(TEXT("Only Bread, Fish and Planks have buildable chains"), Chains == TArray<FString>{TEXT("Good.Bread"), TEXT("Good.Fish"), TEXT("Good.Planks")});
	TArray<TSharedPtr<FJsonValue>> Results;
	for (const FString Mode : {TEXT("growth"), TEXT("upgrade"), TEXT("neglect"), TEXT("shortage"), TEXT("recovery"), TEXT("surplus"), TEXT("trade-dependence"), TEXT("collapse-recovery")})
	{
		uint64 FirstHash = 0;
		for (int32 Repeat = 0; Repeat < 2; ++Repeat)
		{
			FRun Run;
			if (!TestTrue(TEXT("Real runtime scenario initializes against candidate"), FHansaLubeckScenarioInitializer::TryCreate(
				EHansaRuntimeScenario::LubeckGrainShortage, Compiled.Registry, Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(FHansaHouseId::TryCreate(1).Value, Compiled.Registry).Value, Run.Scenario, Error))) { AddError(Error); return false; }
			const auto Initial = Run.Scenario.State.CreateReadOnlyAccess(Run.Scenario.Definitions);
			TestFalse(TEXT("No starter Brewery"), Initial.GetBuildings().ContainsByPredicate([](const auto& B) { return B.DefinitionId.ToString() == TEXT("Building.Brewery"); }));
			TestEqual(TEXT("Explicit opening stock is honored"), Run.Stock(TEXT("Good.Bread")), int64(12000));
			if (Mode != TEXT("neglect") && Mode != TEXT("shortage") && Mode != TEXT("recovery") && Mode != TEXT("collapse-recovery"))
				TestTrue(TEXT("Start farm through command gateway"), Run.Production(1, true));
			if (Mode == TEXT("neglect") || Mode == TEXT("collapse-recovery"))
			{
				TestTrue(TEXT("Neglect pauses mill"), Run.Production(2, false));
				TestTrue(TEXT("Neglect pauses bakery"), Run.Production(3, false));
			}
			const auto StartTrade = [&]()
			{
				FHansaRouteStop Local, Remote;
				Local.CityId = Run.Scenario.CityId;
				Remote.CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
				for (const TCHAR* GoodText : {TEXT("Good.Bread"), TEXT("Good.Fish"), TEXT("Good.Beer"), TEXT("Good.Tools")})
				{
					const auto Good = FHansaGoodId::TryParse(GoodText).Value;
					Local.Actions.Add({ EHansaRouteCargoActionKind::Unload, EHansaRouteCargoCondition::Always, Good, FHansaQuantity::FromRaw(10000), {} });
					Remote.Actions.Add({ EHansaRouteCargoActionKind::Load, EHansaRouteCargoCondition::Always, Good, FHansaQuantity::FromRaw(10000), FHansaQuantity::FromRaw(5000) });
				}
				const auto Route = FHansaRouteId::TryCreate(1).Value;
				TestTrue(TEXT("Trade plan uses normal edit command"), Run.Command(FHansaEditRouteCommand { Route, {Local, Remote} }));
				TestTrue(TEXT("Trade activation uses normal command"), Run.Command(FHansaSetRouteActiveCommand { Route, true }));
			};
			if (Mode == TEXT("surplus") || Mode == TEXT("trade-dependence")) StartTrade();
			int64 PeakIncoming = 0;
			const int32 Ticks = Mode == TEXT("growth") || Mode == TEXT("upgrade") ? 120 : Mode == TEXT("collapse-recovery") ? 2400 : Mode == TEXT("trade-dependence") ? 3600 : 1200;
			for (int32 Tick = 0; Tick < Ticks; ++Tick)
			{
				if (Mode == TEXT("collapse-recovery") && Tick == 1200)
				{
					const auto Empty = Run.Scenario.State.CreateReadOnlyAccess(Run.Scenario.Definitions).QueryCityPopulation(Run.Scenario.CityId);
					TestEqual(TEXT("Stress case first reaches zero population"), Empty->TotalResidents, 0);
					TestTrue(TEXT("Reopen farm"), Run.Production(1, true));
					TestTrue(TEXT("Reopen mill"), Run.Production(2, true));
					TestTrue(TEXT("Reopen bakery"), Run.Production(3, true));
					StartTrade();
				}
				if (Mode == TEXT("recovery") && Tick == 300) TestTrue(TEXT("Recovery reactivates farm"), Run.Production(1, true));
				if (Mode == TEXT("upgrade") && Tick == 10)
					TestTrue(TEXT("Satisfied home upgrades through normal command"), Run.Command(FHansaUpgradeResidenceCommand { FHansaBuildingId::TryCreate(9).Value }));
				if (Mode == TEXT("trade-dependence") && Tick == 600)
					TestTrue(TEXT("Trade can be cancelled in transit"), Run.Command(FHansaCancelRouteCommand { FHansaRouteId::TryCreate(1).Value }));
				if (!TestTrue(TEXT("Deterministic playthrough tick succeeds"), Run.Step())) return false;
				const auto Bread = Run.Scenario.State.CreateReadOnlyAccess(Run.Scenario.Definitions).QueryMarket(Run.Scenario.CityId, FHansaGoodId::TryParse(TEXT("Good.Bread")).Value);
				if (Bread.IsSet()) PeakIncoming = FMath::Max(PeakIncoming, Bread->ExpectedIncomingSupply.GetRawValue());
			}
			const auto View = Run.Scenario.State.CreateReadOnlyAccess(Run.Scenario.Definitions);
			const auto Population = View.QueryCityPopulation(Run.Scenario.CityId);
			if (!TestTrue(TEXT("Population summary exists"), Population.IsSet())) return false;
			if (Mode == TEXT("collapse-recovery")) TestTrue(TEXT("Real imports recover an empty city and its workforce"), Population->TotalResidents >= 24 && Population->LaborerWorkforceSupply >= 12);
			if (Mode == TEXT("growth")) TestTrue(TEXT("Supply produces real population growth"), Population->TotalResidents > 38);
			if (Mode == TEXT("upgrade")) TestTrue(TEXT("Upgrade produces skilled residents"), Population->ArtisanResidents > 8);
			if (Mode == TEXT("neglect")) TestTrue(TEXT("Neglect produces population decline"), Population->TotalResidents < 38);
			for (const auto& Good : Compiled.Registry.GetGoods())
			{
				TestTrue(TEXT("Local goods never become negative or unobservable"), Run.Stock(*Good.StableId) >= 0);
				const auto* Profile = Compiled.Registry.FindCityMarket(TEXT("City.Rostock"))->Goods.FindByPredicate([&](const auto& G) { return G.GoodId == Good.StableId; });
				TestTrue(TEXT("Remote goods stay inside per-good stock ceiling"), Run.Stock(*Good.StableId, 4) <= Profile->DesiredReserveMilliUnits * 3);
			}
			const auto Bread = View.QueryMarket(Run.Scenario.CityId, FHansaGoodId::TryParse(TEXT("Good.Bread")).Value);
			if (Mode == TEXT("surplus")) TestTrue(TEXT("Only actual loaded cargo supplies the incoming projection"), PeakIncoming > 0 && PeakIncoming <= 10000);
			if (Mode == TEXT("trade-dependence")) TestEqual(TEXT("Cancelled route stops advertising expected supply"), Bread->ExpectedIncomingSupply.GetRawValue(), int64(0));
			const uint64 Hash = View.BuildStateHashReport().GetOverallHash();
			FHansaSaveSnapshot Snapshot, Restored;
			Snapshot.State = Run.Scenario.State; Snapshot.BuildVersion = TEXT("EMVP-P33-candidate");
			Snapshot.SavedUtc = TEXT("2026-09-09T00:00:00Z"); Snapshot.DisplayName = Mode;
			Snapshot.Players.Add({1, Run.Scenario.HouseId});
			TArray<uint8> Bytes;
			TestTrue(TEXT("Economy state encodes"), FHansaSaveEnvelope::Encode(Snapshot, Run.Scenario.Definitions, Bytes).IsSuccess());
			TestTrue(TEXT("Economy state decodes under its exact catalog"), FHansaSaveEnvelope::Decode(Bytes, Run.Scenario.Definitions, Restored).IsSuccess());
			TestEqual(TEXT("Save round trip preserves the authoritative economy"), Restored.State.CreateReadOnlyAccess(Run.Scenario.Definitions).BuildStateHashReport().GetOverallHash(), Hash);
			if (Mode == TEXT("growth") && Repeat == 0)
			{
				FHansaEconomicRegistry Baseline;
				TestTrue(TEXT("Reviewed production catalog remains loadable"), FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Baseline, Error));
				const auto OldContext = FHansaSimulationDefinitionContext::TryCreate(
					FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value, Baseline.GetRegistryHash(), MoveTemp(Baseline));
				FHansaSaveSnapshot Rejected;
				TestTrue(TEXT("Old catalog rejects the candidate save instead of silently reinterpreting it"), OldContext.IsSuccess() && !FHansaSaveEnvelope::Decode(Bytes, OldContext.Value, Rejected).IsSuccess());
			}
			if (Repeat == 0) FirstHash = Hash;
			else TestEqual(TEXT("Replay preserves exact authoritative fingerprint"), Hash, FirstHash);
			if (Repeat == 0)
			{
				auto Row = MakeShared<FJsonObject>();
				Row->SetStringField(TEXT("scenario"), Mode); Row->SetNumberField(TEXT("ticks"), Ticks);
				Row->SetNumberField(TEXT("population"), Population->TotalResidents);
				Row->SetNumberField(TEXT("artisans"), Population->ArtisanResidents);
				Row->SetNumberField(TEXT("satisfactionBasisPoints"), Population->SatisfactionBasisPoints);
				Row->SetNumberField(TEXT("breadMilliUnits"), Run.Stock(TEXT("Good.Bread")));
				Row->SetNumberField(TEXT("beerMilliUnits"), Run.Stock(TEXT("Good.Beer")));
				Row->SetNumberField(TEXT("breadPriceMilliMarks"), Bread->CurrentPriceMilliMarks);
				Row->SetNumberField(TEXT("peakIncomingBreadMilliUnits"), PeakIncoming);
				Row->SetNumberField(TEXT("workforce"), Population->LaborerWorkforceSupply + Population->ArtisanWorkforceSupply);
				Row->SetStringField(TEXT("fingerprint"), FString::Printf(TEXT("%016llX"), Hash));
				Results.Add(MakeShared<FJsonValueObject>(Row));
			}
		}
	}
	const auto ResultFor = [&Results](const TCHAR* Name)
	{
		for (const auto& V : Results) if (V->AsObject()->GetStringField(TEXT("scenario")) == Name) return V->AsObject();
		return TSharedPtr<FJsonObject>();
	};
	TestTrue(TEXT("Restarting production recovers bread relative to leaving the shortage"), ResultFor(TEXT("recovery"))->GetNumberField(TEXT("breadMilliUnits")) > ResultFor(TEXT("shortage"))->GetNumberField(TEXT("breadMilliUnits")));
	TestTrue(TEXT("Ongoing trade builds surplus relative to stopped trade"), ResultFor(TEXT("surplus"))->GetNumberField(TEXT("beerMilliUnits")) > ResultFor(TEXT("trade-dependence"))->GetNumberField(TEXT("beerMilliUnits")));
	TestTrue(TEXT("Shortages raise prices relative to delivered surplus"), ResultFor(TEXT("shortage"))->GetNumberField(TEXT("breadPriceMilliMarks")) > ResultFor(TEXT("surplus"))->GetNumberField(TEXT("breadPriceMilliMarks")));
	TestTrue(TEXT("Premature upgrades cost laborer supply and wellbeing"), ResultFor(TEXT("upgrade"))->GetNumberField(TEXT("satisfactionBasisPoints")) < ResultFor(TEXT("growth"))->GetNumberField(TEXT("satisfactionBasisPoints")));
	TestTrue(TEXT("Trade interruption eventually reduces wellbeing"), ResultFor(TEXT("trade-dependence"))->GetNumberField(TEXT("satisfactionBasisPoints")) < ResultFor(TEXT("surplus"))->GetNumberField(TEXT("satisfactionBasisPoints")));
	auto EvidenceRoot = MakeShared<FJsonObject>(); EvidenceRoot->SetArrayField(TEXT("playthroughs"), Results);
	EvidenceRoot->SetStringField(TEXT("candidateRegistryHash"), FString::Printf(TEXT("%016llX"), Compiled.Registry.GetRegistryHash()));
	FString Json; FJsonSerializer::Serialize(EvidenceRoot, TJsonWriterFactory<>::Create(&Json));
	const FString File = FPaths::ProjectDir() / TEXT("Docs/Development/EconomyP33/playthroughs.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	TestTrue(TEXT("Playthrough evidence saved"), FFileHelper::SaveStringToFile(Json, *File));
	return true;
}
#endif
