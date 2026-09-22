#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaCompoundGround.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "World/HansaPresentationClock.h"
#include "World/HansaCargoProjectionManager.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HansaLog.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Systems/HansaSimulationPipeline.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckWorldFoundation.h"

using namespace Hansa::Simulation;

struct FHansaRuntimeSimulationState final
{
	struct FAIHouse final
	{
		FHansaHouseId HouseId;
		uint64 PrincipalId = 0;
		FHansaMerchantAIController Controller;
	};

	FHansaSimulationDefinitionContext Definitions;
	FHansaSimulationState State;
	FHansaSimulationTransientCache Cache;
	FHansaHouseId HouseId;
	FHansaHouseId RivalHouseId;
	TArray<FHansaHouseId> HouseIds;
	TArray<FHansaHouseStartOpportunity> StartingOpportunities;
	FHansaHouseControlRoster HouseControl;
	TArray<FAIHouse> AIHouses;
	FHansaScenarioEvaluator ScenarioEvaluator;
	FHansaCityDefinitionId CityId;
	TArray<FString> SaveMigrationHistory;
	TArray<FHansaSaveRouteLabel> RouteLabels;
	uint64 NextCommandId = 1;
	uint64 NextBuildingId = 1;
	uint64 CampaignSeed = 0;
	TArray<FHansaDomainEvent> EventHistory;
	EHansaRuntimeScenario Scenario = EHansaRuntimeScenario::LubeckGrainShortage;
	bool bReady = false;
	bool bMerchantAIEnabled = true;
	double LastEconomyLogSeconds = 0.0;
	int64 LastEconomyLogTick = -1;
	TMap<uint64, uint64> LoggedProductionCycles;
};

namespace
{
	TAutoConsoleVariable<float> CVarEconomyLogInterval(
		TEXT("hansa.Debug.EconomyLogInterval"), 10.0f,
		TEXT("Seconds between economy diagnostic snapshots; 0 disables logging."));

	template <typename TPayload>
	FHansaGameplayCommand MakeRuntimeCommand(const FHansaRuntimeSimulationState& Runtime,
		const TPayload& Payload, const FHansaCommandAuthorityContext& Authority)
	{
		const FHansaSimulationReadOnlyAccess ReadOnly = Runtime.State.CreateReadOnlyAccess(Runtime.Definitions);
		FHansaCommandHeader Header;
		Header.CommandId = FHansaCommandId::TryCreate(Runtime.NextCommandId).Value;
		Header.Authority = Authority;
		Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		return FHansaGameplayCommand::Create(Header, Payload);
	}

	template <typename TPayload>
	FHansaGameplayCommand MakeRuntimeCommand(const FHansaRuntimeSimulationState& Runtime, const TPayload& Payload)
	{
		return MakeRuntimeCommand(Runtime, Payload,
			{ Runtime.HouseId, 1, EHansaCommandOrigin::PlayerInput });
	}

	template <typename TPayload>
	FHansaCommandGatewayResult PreviewRuntimeCommand(const FHansaRuntimeSimulationState& Runtime, const TPayload& Payload)
	{
		FHansaSimulationState Candidate = Runtime.State;
		FHansaSimulationTransientCache CandidateCache = Runtime.Cache;
		const FHansaGameplayCommand Command = MakeRuntimeCommand(Runtime, Payload);
		return FHansaGameplayCommandGateway::ExecuteTick(
			Candidate, Runtime.Definitions, MakeArrayView(&Command, 1), CandidateCache);
	}

	template <typename TPayload>
	FHansaCommandGatewayResult ExecuteRuntimeCommand(FHansaRuntimeSimulationState& Runtime, const TPayload& Payload)
	{
		const FHansaGameplayCommand Command = MakeRuntimeCommand(Runtime, Payload);
		return FHansaGameplayCommandGateway::ExecuteTick(
			Runtime.State, Runtime.Definitions, MakeArrayView(&Command, 1), Runtime.Cache);
	}

	template <typename TPayload>
	FHansaCommandGatewayResult ExecuteRuntimeCommand(FHansaRuntimeSimulationState& Runtime,
		const FHansaCommandAuthorityContext& Authority, const TPayload& Payload)
	{
		const FHansaGameplayCommand Command = MakeRuntimeCommand(Runtime, Payload, Authority);
		return FHansaGameplayCommandGateway::ExecuteTick(
			Runtime.State, Runtime.Definitions, MakeArrayView(&Command, 1), Runtime.Cache);
	}
}

UHansaRuntimeSimulationHost::~UHansaRuntimeSimulationHost() = default;

bool UHansaRuntimeSimulationHost::InitializeForLubeck(
	UWorld* World,
	FString& OutError,
	const EHansaRuntimeScenario Scenario,
	const uint64 CampaignSeedOverride,
    const bool bEmptyPlayerCity)
{
	if (IsReady())
	{
		if (Runtime->Scenario != Scenario)
		{
			OutError = TEXT("The runtime simulation host is already initialized with a different scenario.");
			return false;
		}
		if (World != nullptr) BoundWorld = World;
		return true;
	}

	Runtime = MakeShared<FHansaRuntimeSimulationState>();
	BoundWorld = World;
	FHansaEconomicRegistry Registry;
	if (!FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Registry, OutError))
	{
		Runtime.Reset();
		return false;
	}

	const auto HouseId = FHansaHouseId::TryCreate(1);
	if (!HouseId)
	{
		OutError = TEXT("Unable to create the Lübeck player-house identity.");
		Runtime.Reset();
		return false;
	}
	const bool bSurvey = Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(World);
	const auto Placement = bSurvey
		? Hansa::Game::LubeckPlacementGrid::TryBuildSurveyInitialization(HouseId.Value, Registry)
		: Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(HouseId.Value, Registry);
	if (!Placement)
	{
		OutError = TEXT("Unable to create the Lübeck placement grid.");
		Runtime.Reset();
		return false;
	}

	FHansaLubeckScenarioState ScenarioState;
	if (!FHansaLubeckScenarioInitializer::TryCreate(
		Scenario, MoveTemp(Registry), Placement.Value, ScenarioState, OutError, CampaignSeedOverride, bEmptyPlayerCity || bSurvey))
	{
		Runtime.Reset();
		return false;
	}

	Runtime->Definitions = MoveTemp(ScenarioState.Definitions);
	Runtime->State = MoveTemp(ScenarioState.State);
	Runtime->HouseId = ScenarioState.HouseId;
	Runtime->RivalHouseId = ScenarioState.RivalHouseId;
	Runtime->HouseIds = MoveTemp(ScenarioState.HouseIds);
	Runtime->StartingOpportunities = MoveTemp(ScenarioState.StartingOpportunities);
	TArray<FHansaHouseControlState> Controls;
	for (int32 Index = 0; Index < Runtime->HouseIds.Num(); ++Index)
	{
		const FHansaHouseId RosterHouseId = Runtime->HouseIds[Index];
		Controls.Add({RosterHouseId, Index == 0 ? EHansaHouseController::Dormant : EHansaHouseController::AI,
			FHansaParticipantId(), {true, true}, 1});
		if (Index > 0) Runtime->AIHouses.Add({RosterHouseId, static_cast<uint64>(100 + Index), {}});
	}
	if (!Runtime->HouseControl.Initialize(Controls, OutError))
	{
		Runtime.Reset();
		return false;
	}
	Runtime->CityId = ScenarioState.CityId;
	Runtime->NextBuildingId = ScenarioState.NextBuildingId;
	Runtime->CampaignSeed = CampaignSeedOverride != 0 ? CampaignSeedOverride :
		(Scenario == EHansaRuntimeScenario::LubeckGrainShortage ? 0x4C554245434B4752ULL : 0x5330375030334255ULL);
	Runtime->Scenario = Scenario;
	if (Scenario == EHansaRuntimeScenario::LubeckGrainShortage)
	{
		const FHansaEconomicRegistry* RuntimeRegistry = Runtime->Definitions.GetEconomicRegistry();
		if (RuntimeRegistry == nullptr || !Runtime->ScenarioEvaluator.Initialize(*RuntimeRegistry,
			TEXT("Scenario.LubeckGrainShortageV1"), Runtime->HouseId) ||
			!Runtime->ScenarioEvaluator.Evaluate(Runtime->State.CreateReadOnlyAccess(Runtime->Definitions), *RuntimeRegistry))
		{
			OutError = TEXT("Unable to initialize the authored Lübeck scenario objectives.");
			Runtime.Reset();
			return false;
		}
	}
	Runtime->bReady = true;
	SynchronizeWorldProjection();
	return true;
}

bool UHansaRuntimeSimulationHost::StartNewGame(FString& OutError)
{
    auto* Candidate=NewObject<UHansaRuntimeSimulationHost>();
    if(!Candidate->InitializeForLubeck(BoundWorld.Get(),OutError,EHansaRuntimeScenario::LubeckGrainShortage,0,true))return false;
    Runtime=MoveTemp(Candidate->Runtime);TickAccumulator=0;Speed=EHansaRuntimeSimulationSpeed::Paused;
    return SynchronizeWorldProjection() && PublishStateChange({});
}

EHansaRuntimeScenario UHansaRuntimeSimulationHost::GetScenario() const
{
	return IsReady() ? Runtime->Scenario : EHansaRuntimeScenario::LubeckGrainShortage;
}

void UHansaRuntimeSimulationHost::SetSpeed(const EHansaRuntimeSimulationSpeed NewSpeed)
{
	Speed = NewSpeed;
    // Preserve fractional time on pause so projected vehicles and machinery do not jump backward.
    // New games and save restoration reset the adapter explicitly.
}

bool UHansaRuntimeSimulationHost::IsReady() const
{
	return Runtime.IsValid() && Runtime->bReady;
}

bool UHansaRuntimeSimulationHost::TryGetPresentationCalendar(
	FHansaCalendarProjection& OutCalendar,
	double& OutTickFraction,
	uint16& OutMinutesPerTick) const
{
	if (!IsReady()) return false;
	const FHansaSimulationClock& Clock = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetClock();
	const THansaValueResult<FHansaCalendarProjection> Calendar = Clock.TryProjectCalendar();
	if (!Calendar) return false;
	OutCalendar = Calendar.Value;
	OutTickFraction = GetPresentationTickFraction();
	OutMinutesPerTick = Clock.GetMinutesPerTick();
	Hansa::Game::PresentationClock::LockToMidday(OutCalendar, &OutTickFraction);
	return true;
}

double UHansaRuntimeSimulationHost::TicksPerSecond() const
{
	switch (Speed)
	{
	case EHansaRuntimeSimulationSpeed::Normal: return 1.0;
	case EHansaRuntimeSimulationSpeed::Fast: return 4.0;
	case EHansaRuntimeSimulationSpeed::Fastest: return 12.0;
	default: return 0.0;
	}
}

bool UHansaRuntimeSimulationHost::AdvanceRealTime(const double DeltaSeconds)
{
	LogEconomyDiagnostics();
	const double Rate = TicksPerSecond();
	if (!IsReady() || DeltaSeconds <= 0.0 || Rate <= 0.0) return IsReady();
	// Rendering owns the frame budget. Accumulate only enough debt for one fixed step and discard any accelerated-time
	// backlog; attempting to catch up multiple expensive ticks makes the frame slower and creates a feedback spiral.
	TickAccumulator = FMath::Min(1.0, TickAccumulator + FMath::Min(DeltaSeconds, 1.0) * Rate);
	if (TickAccumulator < 1.0) return true;
	if (!AdvanceTicks(1)) return false;
	TickAccumulator = 0.0;
	return true;
}

void UHansaRuntimeSimulationHost::LogEconomyDiagnostics()
{
	if (!IsReady() || !UE_LOG_ACTIVE(LogHansa, Log)) return;
	const double Interval = CVarEconomyLogInterval.GetValueOnGameThread();
	if (Interval <= 0.0) return;
	const double Now = FPlatformTime::Seconds();
	if (Runtime->LastEconomyLogSeconds > 0.0 &&
		Now - Runtime->LastEconomyLogSeconds < Interval) return;
	const auto Result = BuildProjection();
	if (!Result) return;
	const auto& View = Result.Value;
	const int64 Tick = GetSimulationTick();
	UE_LOG(LogHansa, Log, TEXT("[EconomyDebug] tick=%lld previousTick=%lld elapsedSeconds=%.1f paused=%d quantities=units lastTick=simulation-tick history=last-30-game-days"),
		Tick, Runtime->LastEconomyLogTick,
		Runtime->LastEconomyLogSeconds > 0.0 ? Now - Runtime->LastEconomyLogSeconds : 0.0,
		Speed == EHansaRuntimeSimulationSpeed::Paused);
	for (const auto& City : View.GetCityPopulations())
	{
		UE_LOG(LogHansa, Log, TEXT("[EconomyDebug][Population] city=%s citizens=%d capacity=%d laborers=%d artisans=%d laborSupply=%d laborAssigned=%d laborAvailable=%d artisanSupply=%d artisanAssigned=%d artisanAvailable=%d"),
			*City.CityId.ToString(), City.TotalResidents, City.HousingCapacity,
			City.LaborerResidents, City.ArtisanResidents, City.LaborerWorkforceSupply,
			City.LaborerWorkforceAssigned, City.LaborerWorkforceAvailable,
			City.ArtisanWorkforceSupply, City.ArtisanWorkforceAssigned, City.ArtisanWorkforceAvailable);
	}
	for (const auto& Inventory : View.GetInventories())
	{
		if (Inventory.OwnerKind != EHansaInventoryOwnerKind::City) continue;
		for (const auto& Good : Inventory.AcceptedGoods)
		{
			const auto* Stock = Inventory.Stocks.FindByPredicate([&Good](const auto& Item) { return Item.GoodId == Good; });
			UE_LOG(LogHansa, Log, TEXT("[EconomyDebug][MarketStock] city=%s inventory=%llu building=%llu good=%s stock=%.3f reserved=%.3f available=%.3f"),
				*Inventory.CityId.ToString(), Inventory.Id.GetValue(), Inventory.BuildingId.GetValue(), *Good.ToString(),
				Stock ? Stock->Stock.GetRawValue()/1000.0 : 0.0,
				Stock ? Stock->Reserved.GetRawValue()/1000.0 : 0.0,
				Stock ? Stock->Available.GetRawValue()/1000.0 : 0.0);
		}
	}
	for (const auto& Production : View.GetProductions())
	{
		const uint64* Previous = Runtime->LoggedProductionCycles.Find(Production.Id.GetValue());
		const uint64 Cycles = Previous && *Previous <= Production.CompletedCycles
			? Production.CompletedCycles - *Previous : 0;
		UE_LOG(LogHansa, Log, TEXT("[EconomyDebug][Production] id=%llu building=%llu city=%s recipe=%s active=%d blocker=%s blockingGood=%s required=%.3f available=%.3f labor=%d/%d artisans=%d/%d progress=%d/%d cyclesTotal=%llu cyclesSinceLog=%llu baseline=%d inputInventory=%llu outputInventory=%llu"),
			Production.Id.GetValue(), Production.BuildingId.GetValue(), *Production.CityId.ToString(),
			*Production.RecipeId.ToString(), Production.bActive, LexToString(Production.Blocker),
			*Production.BlockingGoodId.ToString(), Production.BlockingRequiredQuantity.GetRawValue()/1000.0,
			Production.BlockingAvailableQuantity.GetRawValue()/1000.0,
			Production.AllocatedLaborerWorkforce, Production.RequiredLaborerWorkforce,
			Production.AllocatedArtisanWorkforce, Production.RequiredArtisanWorkforce,
			Production.ProgressTicks, Production.CycleTicks, Production.CompletedCycles, Cycles,
			Previous == nullptr, Production.InputInventoryId.GetValue(), Production.OutputInventoryId.GetValue());
		for (const auto& Output : Production.Outputs)
			UE_LOG(LogHansa, Log, TEXT("[EconomyDebug][Output] production=%llu good=%s actualLastTick=%.3f nominalPerBatch=%.3f"),
				Production.Id.GetValue(), *Output.GoodId.ToString(),
				Output.ActualQuantityLastTick.GetRawValue()/1000.0, Output.NominalQuantityPerCycle.GetRawValue()/1000.0);
		Runtime->LoggedProductionCycles.Add(Production.Id.GetValue(), Production.CompletedCycles);
	}
	for (const auto& Home : View.GetPopulationCohorts())
	{
		UE_LOG(LogHansa, Log, TEXT("[EconomyDebug][Residence] id=%llu building=%llu city=%s citizens=%d workforce=%d inventory=%llu operational=%d marketAccess=%d satisfactionBP=%d"),
			Home.Id.GetValue(), Home.ResidenceBuildingId.GetValue(), *Home.CityId.ToString(),
			Home.Residents, Home.WorkforceSupply, Home.ConsumptionInventoryId.GetValue(),
			Home.bResidenceOperational, Home.bHasMarketAccess, Home.SatisfactionBasisPoints);
		for (const auto& Need : Home.Needs)
		{
			if (!Need.GoodId.IsValid()) continue;
			const auto* History = Home.Consumption.Goods.FindByPredicate([&Need](const auto& Item) { return Item.GoodId == Need.GoodId; });
			UE_LOG(LogHansa, Log, TEXT("[EconomyDebug][Consumption] residence=%llu good=%s requiredLastTick=%.3f consumedLastTick=%.3f accessBP=%d affordabilityBP=%d reliabilityBP=%d historyRequired=%.3f historyConsumed=%.3f historyMinutes=%lld"),
				Home.Id.GetValue(), *Need.GoodId.ToString(),
				Need.RequiredLastTick.GetRawValue()/1000.0, Need.ConsumedLastTick.GetRawValue()/1000.0,
				Need.AccessBasisPoints, Need.AffordabilityBasisPoints, Need.ReliabilityBasisPoints,
				History ? History->Required/1000.0 : 0.0, History ? History->Consumed/1000.0 : 0.0,
				Home.Consumption.CoveredMinutes);
		}
	}
	Runtime->LastEconomyLogSeconds = Now;
	Runtime->LastEconomyLogTick = Tick;
}

bool UHansaRuntimeSimulationHost::AdvanceTicks(const int32 TickCount)
{
	if (!IsReady() || TickCount < 0 || TickCount > 10000) return false;
	TArray<FHansaDomainEvent> Events;
	for (int32 Index = 0; Index < TickCount; ++Index)
	{
		const FHansaSimulationReadOnlyAccess Before = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
		TOptional<FHansaGameplayCommand> AICommand;
		FHansaRuntimeSimulationState::FAIHouse* SelectedAI = nullptr;
		if (Runtime->Scenario == EHansaRuntimeScenario::LubeckGrainShortage &&
			Runtime->bMerchantAIEnabled)
		{
			const FHansaEconomicRegistry* Registry = Runtime->Definitions.GetEconomicRegistry();
			const FHansaCompiledMerchantAITuning* Tuning = Registry != nullptr
				? Registry->FindMerchantAITuning(TEXT("AITuning.MerchantRival")) : nullptr;
			if (Tuning != nullptr && !Runtime->AIHouses.IsEmpty())
			{
				const int32 StartIndex = static_cast<int32>(Before.GetClock().GetTick().GetValue() %
					Runtime->AIHouses.Num());
				for (int32 Offset = 0; Offset < Runtime->AIHouses.Num(); ++Offset)
				{
					auto& Candidate = Runtime->AIHouses[(StartIndex + Offset) % Runtime->AIHouses.Num()];
					if (!Runtime->HouseControl.IsAIControlled(Candidate.HouseId) ||
						!Candidate.Controller.IsDue(Before, *Tuning)) continue;
					const auto CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId);
					if (!CommandId) return false;
					FHansaMerchantAIDecision Decision = Candidate.Controller.Evaluate(
						Before, *Registry, *Tuning, Candidate.HouseId, Candidate.PrincipalId, CommandId.Value);
					AICommand = MoveTemp(Decision.Command);
					SelectedAI = &Candidate;
					break;
				}
			}
		}
		const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
			Runtime->State, Runtime->Definitions,
			AICommand.IsSet() ? MakeArrayView(&AICommand.GetValue(), 1) : TConstArrayView<FHansaGameplayCommand>(),
			Runtime->Cache);
		if (AICommand.IsSet())
		{
			if (SelectedAI != nullptr) SelectedAI->Controller.RecordGatewayOutcome(Result);
			if (Result) ++Runtime->NextCommandId;
		}
		if (!Result)
		{
			UE_LOG(LogHansa, Error, TEXT("Runtime simulation tick failed: %s"), LexToString(Result.GetError()));
			Speed = EHansaRuntimeSimulationSpeed::Paused;
			return false;
		}
		Events.Append(Result.GetEvents());
		if (Runtime->Scenario == EHansaRuntimeScenario::LubeckGrainShortage)
		{
			const FHansaEconomicRegistry* Registry = Runtime->Definitions.GetEconomicRegistry();
			if (Registry == nullptr || !Runtime->ScenarioEvaluator.Evaluate(
				Runtime->State.CreateReadOnlyAccess(Runtime->Definitions), *Registry)) return false;
			if (Runtime->ScenarioEvaluator.GetProgress().Outcome != EHansaScenarioOutcome::Active) Speed = EHansaRuntimeSimulationSpeed::Paused;
		}
	}
	return TickCount == 0 || PublishStateChange(Events);
}

FHansaLogisticsRoadPathProjection UHansaRuntimeSimulationHost::QueryLocalRoadPath(
	const FHansaInventoryId SourceInventoryId, const FHansaInventoryId DestinationInventoryId) const
{
	return IsReady()
		? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryLogisticsRoadPath(
			SourceInventoryId, DestinationInventoryId)
		: FHansaLogisticsRoadPathProjection();
}

const FHansaCompiledBuildingDefinition* UHansaRuntimeSimulationHost::FindBuildingDefinition(
	const FString& StableId) const
{
	return IsReady() && Runtime->Definitions.GetEconomicRegistry() != nullptr
		? Runtime->Definitions.GetEconomicRegistry()->FindBuilding(StableId) : nullptr;
}

const FHansaPlacementMapInitialization* UHansaRuntimeSimulationHost::FindPlacementMap() const
{
	return IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetPlacement().FindMap(Runtime->CityId) : nullptr;
}

FHansaPlacementValidationResult UHansaRuntimeSimulationHost::ValidatePlacement(const FHansaPlacementSpec& Spec) const
{
 auto Result=IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).ValidatePlacement(Runtime->HouseId, Spec)
  : FHansaPlacementValidationResult();
 if(IsReady()&&!IsCompoundTerrainBuildable(Spec))Result=Result.WithTerrainFailure(Spec.Anchor);
 return Result;
}

bool UHansaRuntimeSimulationHost::IsOwnedRoadCell(const FHansaGridCoordinate Cell) const
{
	if (!IsReady()) return false;
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	const FHansaPlacementMapInitialization* Map = ReadOnly.GetPlacement().FindMap(Runtime->CityId);
	if (Map == nullptr) return false;
	return ReadOnly.GetPlacement().GetPlacements().ContainsByPredicate(
		[this, Map, Cell](const FHansaPlacedBuildingRecord& Placement)
		{
			return Placement.OwnerId == Runtime->HouseId && Placement.Spec.CityId == Runtime->CityId &&
				Placement.Spec.BuildingDefinitionId == Map->RoadBuildingDefinitionId &&
				Placement.OccupiedCells.Contains(Cell);
		});
}

FHansaConstructionCostProjection UHansaRuntimeSimulationHost::QueryConstructionCost(const FString& BuildingStableId) const
{
	const auto BuildingId = FHansaBuildingTypeId::TryParse(BuildingStableId);
	return IsReady() && BuildingId
		? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryConstructionCost(
			Runtime->HouseId, Runtime->CityId, BuildingId.Value)
		: FHansaConstructionCostProjection();
}

bool UHansaRuntimeSimulationHost::IsTechnologyCompleted(const FString& TechnologyId) const
{
	if (!IsReady() || TechnologyId.IsEmpty()) return TechnologyId.IsEmpty();
	for (const FHansaHouseResearchState& Research : Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetResearch())
	{
		if (Research.HouseId == Runtime->HouseId) return Research.IsCompleted(TechnologyId);
	}
	return false;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::PlaceBuildings(
	const TConstArrayView<FHansaPlacementSpec> Specs)
{
	const FHansaCommandAuthorityContext Authority {
		GetHouseId(), 1, EHansaCommandOrigin::PlayerInput };
	return PlaceBuildingsForAuthority(Authority, Specs);
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::PlaceBuildingsForAuthority(
	const FHansaCommandAuthorityContext& Authority,
	const TConstArrayView<FHansaPlacementSpec> Specs)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
 // Preflight the entire batch, including automation/authority callers, before any mutation.
 for(int32 Index=0;Index<Specs.Num();++Index)
 {
  if(IsCompoundTerrainBuildable(Specs[Index],Index))continue;
  const auto Validation=ReadOnly.ValidatePlacement(Authority.IssuingHouseId,Specs[Index]).WithTerrainFailure(Specs[Index].Anchor);
  const FHansaDeterminismFingerprint Fingerprint=ReadOnly.GetFingerprint();
  return FHansaCommandGatewayResult::RejectTerrain(Validation,Index,
   FHansaCommandId::TryCreate(Runtime->NextCommandId+Index).Value,ReadOnly.GetClock().GetTick(),Fingerprint);
 }
	TArray<FHansaGameplayCommand> Commands;
	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		FHansaCommandHeader Header;
		Header.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId + Index).Value;
		Header.Authority = Authority;
		Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + Index + 1;
		const FHansaBuildingId BuildingId = FHansaBuildingId::TryCreate(Runtime->NextBuildingId + Index).Value;
		Commands.Add(FHansaGameplayCommand::Create(Header, FHansaPlaceBuildingCommand { BuildingId, Specs[Index] }));
	}
	FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, Commands, Runtime->Cache);
	if (Result)
	{
		Runtime->NextCommandId += Specs.Num();
		Runtime->NextBuildingId += Specs.Num();
		PublishStateChange(Result.GetEvents());
	}
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::MoveShip(FHansaVehicleId VehicleId, FHansaGridCoordinate Target)
{
    if (!IsReady()) return {};
    auto Result=ExecuteRuntimeCommand(*Runtime,FHansaMoveShipCommand{VehicleId,Target});
    if (Result) { ++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true); }
    return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::MoveShipForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaVehicleId VehicleId,
	const FHansaGridCoordinate Target)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, FHansaMoveShipCommand { VehicleId, Target });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents(), true); }
	return Result;
}

FString UHansaRuntimeSimulationHost::GetRouteLabel(uint64 Id) const
{
    if (!Runtime) return {};
    const auto* L = Runtime->RouteLabels.FindByPredicate([Id](const auto& It){ return It.RouteValue == Id; });
    return L ? L->Label : FString();
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CreateTradeRoute(
    FHansaVehicleId VehicleId, TConstArrayView<FHansaRouteStop> Stops, const FString& Name,
    bool bReassignStopped, bool bPreview, uint64& OutRouteValue)
{
    OutRouteValue = 0;
    if (!IsReady() || Name.IsEmpty() || Name.Len() > 48 || Name.TrimStartAndEnd() != Name) return {};
    for (TCHAR C : Name) if (C < 32 || C == 127) return {};
    const auto Projection = BuildProjection();
    if (!Projection) return {};
    const auto* Vehicle = Projection.Value.GetVehicles().FindByPredicate([VehicleId](const auto& V){ return V.Id == VehicleId; });
    if (!Vehicle || Vehicle->OwnerId != Runtime->HouseId || Vehicle->DefinitionId.ToString() != TEXT("Vehicle.Cog") || Vehicle->Cargo.GetRawValue() != 0) return {};
    const auto View = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
    TArray<FHansaGameplayCommand> Commands;
    auto Header = [&]() {
        FHansaCommandHeader H;
        H.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId + Commands.Num()).Value;
        H.Authority = {Runtime->HouseId, 1, EHansaCommandOrigin::PlayerInput};
        H.RequestedExecutionTick = View.GetClock().GetTick();
        H.GlobalSequence = View.GetLastProcessedCommandSequence() + Commands.Num() + 1;
        return H;
    };
    uint64 NewId = 1;
    for (const auto& Route : Projection.Value.GetRoutes())
    {
        NewId = FMath::Max(NewId, Route.Id.GetValue() + 1);
        if (Route.VehicleId != VehicleId || Route.Lifecycle == EHansaRouteLifecycleState::Cancelled) continue;
        if (!bReassignStopped || Route.OwnerId != Runtime->HouseId || Route.Lifecycle != EHansaRouteLifecycleState::Inactive || Route.CurrentStopIndex != 0) return {};
        Commands.Add(FHansaGameplayCommand::Create(Header(), FHansaCancelRouteCommand{Route.Id}));
    }
    FHansaCreateRouteCommand Payload;
    Payload.RouteId = FHansaRouteId::TryCreate(NewId).Value;
    Payload.VehicleId = VehicleId;
    Payload.RouteDefinitionId = FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;
    Payload.Stops.Append(Stops); Payload.bActivate = true;
    Commands.Add(FHansaGameplayCommand::Create(Header(), Payload));
    if (bPreview)
    {
        FHansaSimulationState Candidate = Runtime->State;
        FHansaSimulationTransientCache Cache = Runtime->Cache;
        auto Result = FHansaGameplayCommandGateway::ExecuteTick(Candidate, Runtime->Definitions, Commands, Cache);
        if (Result) OutRouteValue = NewId;
        return Result;
    }
    auto Result = FHansaGameplayCommandGateway::ExecuteTick(Runtime->State, Runtime->Definitions, Commands, Runtime->Cache);
    if (Result)
    {
        Runtime->NextCommandId += Commands.Num();
        Runtime->RouteLabels.Add({NewId, Name}); OutRouteValue = NewId;
        PublishStateChange(Result.GetEvents());
    }
    return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CreateTradeRouteForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaVehicleId VehicleId,
	const TConstArrayView<FHansaRouteStop> Stops, const FString& Name,
	const bool bReassignStopped, uint64& OutRouteValue)
{
	OutRouteValue = 0;
	if (!IsReady() || Name.IsEmpty() || Name.Len() > 48 || Name.TrimStartAndEnd() != Name) return {};
	for (const TCHAR Character : Name) if (Character < 32 || Character == 127) return {};
	const auto Projection = BuildProjection();
	if (!Projection) return {};
	const auto* Vehicle = Projection.Value.GetVehicles().FindByPredicate(
		[VehicleId](const auto& Item) { return Item.Id == VehicleId; });
	if (!Vehicle || Vehicle->OwnerId != Authority.IssuingHouseId ||
		Vehicle->DefinitionId.ToString() != TEXT("Vehicle.Cog") ||
		Vehicle->Cargo.GetRawValue() != 0) return {};
	const auto View = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	TArray<FHansaGameplayCommand> Commands;
	auto Header = [&]()
	{
		FHansaCommandHeader Result;
		Result.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId + Commands.Num()).Value;
		Result.Authority = Authority;
		Result.RequestedExecutionTick = View.GetClock().GetTick();
		Result.GlobalSequence = View.GetLastProcessedCommandSequence() + Commands.Num() + 1;
		return Result;
	};
	uint64 NewId = 1;
	for (const auto& Route : Projection.Value.GetRoutes())
	{
		NewId = FMath::Max(NewId, Route.Id.GetValue() + 1);
		if (Route.VehicleId != VehicleId || Route.Lifecycle == EHansaRouteLifecycleState::Cancelled) continue;
		if (!bReassignStopped || Route.OwnerId != Authority.IssuingHouseId ||
			Route.Lifecycle != EHansaRouteLifecycleState::Inactive ||
			Route.CurrentStopIndex != 0) return {};
		Commands.Add(FHansaGameplayCommand::Create(Header(), FHansaCancelRouteCommand { Route.Id }));
	}
	FHansaCreateRouteCommand Payload;
	Payload.RouteId = FHansaRouteId::TryCreate(NewId).Value;
	Payload.VehicleId = VehicleId;
	Payload.RouteDefinitionId = FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;
	Payload.Stops.Append(Stops);
	Payload.bActivate = true;
	Commands.Add(FHansaGameplayCommand::Create(Header(), Payload));
	auto Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, Commands, Runtime->Cache);
	if (Result)
	{
		Runtime->NextCommandId += Commands.Num();
		Runtime->RouteLabels.Add({ NewId, Name });
		OutRouteValue = NewId;
		PublishStateChange(Result.GetEvents());
	}
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::EditRoute(
	const FHansaRouteId RouteId,
	const TConstArrayView<FHansaRouteStop> Stops)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	FHansaCommandHeader Header;
	Header.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId).Value;
	Header.Authority.IssuingHouseId = Runtime->HouseId;
	Header.Authority.PrincipalId = 1;
	Header.Authority.Origin = EHansaCommandOrigin::PlayerInput;
	Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
	Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
	FHansaEditRouteCommand Payload;
	Payload.RouteId = RouteId;
	Payload.Stops.Append(Stops);
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(Header, Payload);
	FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, MakeArrayView(&Command, 1), Runtime->Cache);
	if (Result)
	{
		++Runtime->NextCommandId;
		PublishStateChange(Result.GetEvents());
	}
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::EditRouteForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaRouteId RouteId,
	const TConstArrayView<FHansaRouteStop> Stops)
{
	if (!IsReady()) return {};
	FHansaEditRouteCommand Payload;
	Payload.RouteId = RouteId;
	Payload.Stops.Append(Stops);
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, Payload);
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ExecuteSpotTrade(const FHansaSpotTradeCommand& Payload)
{
	return ExecuteSpotTradeForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ExecuteSpotTradeForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaSpotTradeCommand& Payload)
{
	if (!IsReady()) return {};
	auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ProposeTradeStation(FHansaCityDefinitionId CityId, const FString& SiteId, FHansaTradeStationId& OutStationId)
{
	return ProposeTradeStationForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},CityId,SiteId,OutStationId);
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ProposeTradeStationForAuthority(const FHansaCommandAuthorityContext& Authority, FHansaCityDefinitionId CityId, const FString& SiteId, FHansaTradeStationId& OutStationId)
{
	if(!IsReady())return {}; const uint64 Value=0x4000000000000000ULL+Runtime->NextCommandId;
	const auto Station=FHansaTradeStationId::TryCreate(Value);
	const auto Factor=FHansaFactorId::TryCreate(Value);
	const auto Lease=FHansaLeasedPlotId::TryCreate(Value);
	const auto Inventory=FHansaInventoryId::TryCreate(Value);
	if(!Station||!Factor||!Lease||!Inventory)return {};
	FHansaProposeTradeStationCommand Payload{Station.Value,Factor.Value,Lease.Value,Inventory.Value,CityId,SiteId};
	auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);if(Result){++Runtime->NextCommandId;OutStationId=Station.Value;PublishStateChange(Result.GetEvents(),true);}return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundTradeStation(FHansaTradeStationId StationId,FHansaInventoryId FundingInventoryId)
{return FundTradeStationForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},StationId,FundingInventoryId);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundTradeStationForAuthority(const FHansaCommandAuthorityContext& Authority,FHansaTradeStationId StationId,FHansaInventoryId FundingInventoryId)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,FHansaFundTradeStationCommand{StationId,FundingInventoryId});if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ManageStationOrder(const FHansaManageStationOrderCommand& Payload)
{return ManageStationOrderForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ManageStationOrderForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaManageStationOrderCommand& Payload)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::RequestPresenceUpgrade(const FHansaRequestPresenceUpgradeCommand& Payload)
{return RequestPresenceUpgradeForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::RequestPresenceUpgradeForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaRequestPresenceUpgradeCommand& Payload)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundPresenceUpgrade(const FHansaFundPresenceUpgradeCommand& Payload)
{return FundPresenceUpgradeForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundPresenceUpgradeForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaFundPresenceUpgradeCommand& Payload)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ApplyPresenceSpecialization(const FHansaApplyPresenceSpecializationCommand& Payload)
{return ApplyPresenceSpecializationForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ApplyPresenceSpecializationForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaApplyPresenceSpecializationCommand& Payload)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CloseTradeStation(FHansaTradeStationId StationId)
{return CloseTradeStationForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},StationId);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CloseTradeStationForAuthority(const FHansaCommandAuthorityContext& Authority,FHansaTradeStationId StationId)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,FHansaCloseTradeStationCommand{StationId});if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}

FHansaSpotTradeQuoteProjection UHansaRuntimeSimulationHost::QuerySpotTradeQuote(FHansaVehicleId Vehicle,
	FHansaCityDefinitionId City,FHansaGoodId Good,EHansaSpotTradeSide Side,FHansaQuantity Quantity) const
{
	if (!IsReady()) return {};
	return Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QuerySpotTradeQuote(Runtime->HouseId,Vehicle,City,Good,Side,Quantity);
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetRouteActive(
	const FHansaRouteId RouteId,
	const bool bActive)
{
	const FHansaCommandAuthorityContext Authority {
		GetHouseId(), 1, EHansaCommandOrigin::PlayerInput };
	return SetRouteActiveForAuthority(Authority, RouteId, bActive);
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetRouteActiveForAuthority(
	const FHansaCommandAuthorityContext& Authority,
	const FHansaRouteId RouteId,
	const bool bActive)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	FHansaCommandHeader Header;
	Header.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId).Value;
	Header.Authority = Authority;
	Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
	Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
		Header, FHansaSetRouteActiveCommand { RouteId, bActive });
	FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, MakeArrayView(&Command, 1), Runtime->Cache);
	if (Result)
	{
		++Runtime->NextCommandId;
		PublishStateChange(Result.GetEvents());
	}
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetProductionActive(
	const FHansaProductionId ProductionId, const bool bActive)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	FHansaCommandHeader Header;
	Header.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId).Value;
	Header.Authority.IssuingHouseId = Runtime->HouseId;
	Header.Authority.PrincipalId = 1;
	Header.Authority.Origin = EHansaCommandOrigin::PlayerInput;
	Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
	Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(Header, FHansaSetProductionActiveCommand { ProductionId, bActive });
	FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, MakeArrayView(&Command, 1), Runtime->Cache);
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetProductionActiveForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaProductionId ProductionId,
	const bool bActive)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority,
		FHansaSetProductionActiveCommand { ProductionId, bActive });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::PreviewCancelConstruction(
	const FHansaBuildingId BuildingId) const
{
	return IsReady() ? PreviewRuntimeCommand(*Runtime, FHansaCancelConstructionCommand { BuildingId })
		: FHansaCommandGatewayResult();
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::PreviewRemoveBuilding(
	const FHansaBuildingId BuildingId) const
{
	return IsReady() ? PreviewRuntimeCommand(*Runtime, FHansaRemoveBuildingCommand { BuildingId })
		: FHansaCommandGatewayResult();
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::PreviewUpgradeResidence(
	const FHansaBuildingId BuildingId) const
{
	return IsReady() ? PreviewRuntimeCommand(*Runtime, FHansaUpgradeResidenceCommand { BuildingId })
		: FHansaCommandGatewayResult();
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CancelConstruction(const FHansaBuildingId BuildingId)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	FHansaCommandGatewayResult Result = ExecuteRuntimeCommand(*Runtime, FHansaCancelConstructionCommand { BuildingId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CancelConstructionForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaBuildingId BuildingId)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, FHansaCancelConstructionCommand { BuildingId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::RemoveBuilding(const FHansaBuildingId BuildingId)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	FHansaCommandGatewayResult Result = ExecuteRuntimeCommand(*Runtime, FHansaRemoveBuildingCommand { BuildingId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::RemoveBuildingForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaBuildingId BuildingId)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, FHansaRemoveBuildingCommand { BuildingId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetProductionMode(FHansaProductionId Id, FHansaRecipeId Recipe, bool Fallback)
{
 if (!IsReady()) return FHansaCommandGatewayResult();
 auto Result=ExecuteRuntimeCommand(*Runtime,FHansaSetProductionModeCommand{Id,Recipe,Fallback});
 if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); } return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetProductionModeForAuthority(
	const FHansaCommandAuthorityContext& Authority, FHansaProductionId Id,
	FHansaRecipeId Recipe, const bool Fallback)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority,
		FHansaSetProductionModeCommand { Id, Recipe, Fallback });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::UpgradeProduction(FHansaProductionId Id)
{
 if (!IsReady()) return FHansaCommandGatewayResult();
 auto Result=ExecuteRuntimeCommand(*Runtime,FHansaUpgradeProductionCommand{Id});
 if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); } return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::UpgradeProductionForAuthority(
	const FHansaCommandAuthorityContext& Authority, FHansaProductionId Id)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, FHansaUpgradeProductionCommand { Id });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::PreviewUpgradeProduction(FHansaProductionId Id) const
{ return IsReady() ? PreviewRuntimeCommand(*Runtime,FHansaUpgradeProductionCommand{Id}) : FHansaCommandGatewayResult(); }
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetHouseholdAvailability(FHansaBuildingId Market, bool Available)
{
 if (!IsReady()) return FHansaCommandGatewayResult();
 auto Result=ExecuteRuntimeCommand(*Runtime,FHansaSetHouseholdAvailabilityCommand{Market,FHansaGoodId::TryParse(TEXT("Good.PreservedFish")).Value,Available});
 if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); } return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetHouseholdAvailabilityForAuthority(
	const FHansaCommandAuthorityContext& Authority, FHansaBuildingId Market,
	FHansaGoodId Good, const bool Available)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority,
		FHansaSetHouseholdAvailabilityCommand { Market, Good, Available });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::UpgradeResidence(const FHansaBuildingId BuildingId)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	FHansaCommandGatewayResult Result = ExecuteRuntimeCommand(*Runtime, FHansaUpgradeResidenceCommand { BuildingId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::UpgradeResidenceForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaBuildingId BuildingId)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, FHansaUpgradeResidenceCommand { BuildingId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaHeatingProjection UHansaRuntimeSimulationHost::QueryHeating() const
{
	return IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryHeating(Runtime->CityId) : FHansaHeatingProjection();
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetHeatingReserve(FHansaBuildingId MarketId, int32 Days, bool bOverride)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	auto Result = ExecuteRuntimeCommand(*Runtime, FHansaSetHeatingReserveCommand{MarketId, Days, bOverride});
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::SetHeatingReserveForAuthority(
	const FHansaCommandAuthorityContext& Authority, FHansaBuildingId MarketId,
	const int32 Days, const bool bReleaseProtection)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority,
		FHansaSetHeatingReserveCommand { MarketId, Days, bReleaseProtection });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::QueueResearch(const FString& TechnologyId)
{
	const FHansaCommandAuthorityContext Authority {
		GetHouseId(), 1, EHansaCommandOrigin::PlayerInput };
	return QueueResearchForAuthority(Authority, TechnologyId);
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::QueueResearchForAuthority(
	const FHansaCommandAuthorityContext& Authority,
	const FString& TechnologyId)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	FHansaCommandHeader Header;
	Header.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId).Value;
	Header.Authority = Authority;
	Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
	Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
		Header, FHansaQueueResearchCommand { TechnologyId });
	FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, MakeArrayView(&Command, 1), Runtime->Cache);
	if (Result)
	{
		++Runtime->NextCommandId;
		PublishStateChange(Result.GetEvents());
	}
	return Result;
}

FHansaHouseId UHansaRuntimeSimulationHost::GetHouseId() const
{
	return IsReady() ? Runtime->HouseId : FHansaHouseId();
}

TConstArrayView<FHansaHouseId> UHansaRuntimeSimulationHost::GetHouseIds() const
{
	return IsReady() ? MakeArrayView(Runtime->HouseIds) : TConstArrayView<FHansaHouseId>();
}

TConstArrayView<FHansaHouseStartOpportunity> UHansaRuntimeSimulationHost::GetStartingOpportunities() const
{
	return IsReady() ? MakeArrayView(Runtime->StartingOpportunities) :
		TConstArrayView<FHansaHouseStartOpportunity>();
}

bool UHansaRuntimeSimulationHost::ClaimHouseForHuman(
	const FHansaHouseId HouseId, const FHansaParticipantId ParticipantId, FString& OutError)
{
	return IsReady() && Runtime->HouseControl.ClaimForHuman(HouseId, ParticipantId, OutError);
}

bool UHansaRuntimeSimulationHost::ReleaseHumanHouse(
	const FHansaParticipantId ParticipantId, FString& OutError)
{
	return IsReady() && Runtime->HouseControl.ReleaseHuman(ParticipantId, OutError);
}

bool UHansaRuntimeSimulationHost::IsHouseAIControlled(const FHansaHouseId HouseId) const
{
	return IsReady() && Runtime->HouseControl.IsAIControlled(HouseId);
}

int32 UHansaRuntimeSimulationHost::GetAIControlledHouseCount() const
{
	if (!IsReady()) return 0;
	int32 Count = 0;
	for (const FHansaHouseControlState& State : Runtime->HouseControl.GetStates())
	{
		if (State.Controller == EHansaHouseController::AI) ++Count;
	}
	return Count;
}

FHansaCityDefinitionId UHansaRuntimeSimulationHost::GetCityId() const
{
	return IsReady() ? Runtime->CityId : FHansaCityDefinitionId();
}

int32 UHansaRuntimeSimulationHost::GetPlacedBuildingCount() const
{
	return IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetPlacement().GetPlacements().Num() : 0;
}

int64 UHansaRuntimeSimulationHost::GetSimulationTick() const
{
	return IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetClock().GetTick().GetValue() : 0;
}

uint64 UHansaRuntimeSimulationHost::GetLastProcessedCommandSequence() const
{
	return IsReady()
		? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetLastProcessedCommandSequence()
		: 0;
}

FString UHansaRuntimeSimulationHost::GetBuildingWorldStatus(const int64 BuildingValue) const
{
	if (!IsReady() || BuildingValue <= 0) return {};
	const auto BuildingId = FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue));
	const auto Projection = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).BuildProjection();
	if (!BuildingId || !Projection) return {};
	const FHansaBuildingWorldProjection* Building = Projection.Value.GetBuildingWorldProjections().FindByPredicate(
		[&BuildingId](const FHansaBuildingWorldProjection& Candidate)
		{
			return Candidate.BuildingId == BuildingId.Value;
		});
	return Building != nullptr ? LexToString(Building->Status) : FString();
}

THansaValueResult<FHansaSimulationProjection> UHansaRuntimeSimulationHost::BuildProjection() const
{
	return IsReady()
		? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).BuildProjection()
		: THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
}

const FHansaEconomicRegistry* UHansaRuntimeSimulationHost::GetEconomicRegistry() const
{
	return IsReady() ? Runtime->Definitions.GetEconomicRegistry() : nullptr;
}

FHansaHouseId UHansaRuntimeSimulationHost::GetRivalHouseId() const
{
	return IsReady() ? Runtime->RivalHouseId : FHansaHouseId();
}

void UHansaRuntimeSimulationHost::SetMerchantAIEnabled(const bool bEnabled)
{
	if (IsReady()) Runtime->bMerchantAIEnabled = bEnabled;
}

bool UHansaRuntimeSimulationHost::IsMerchantAIEnabled() const
{
	return IsReady() && Runtime->bMerchantAIEnabled;
}

const FHansaMerchantAIDecisionTrace* UHansaRuntimeSimulationHost::GetLastMerchantAIDecision() const
{
	if (!IsReady()) return nullptr;
	const auto* Rival = Runtime->AIHouses.FindByPredicate([this](const auto& Item)
		{ return Item.HouseId == Runtime->RivalHouseId; });
	return Rival != nullptr ? Rival->Controller.GetLastDecision() : nullptr;
}

TConstArrayView<FHansaMerchantAIDecisionTrace> UHansaRuntimeSimulationHost::GetMerchantAIDecisionHistory() const
{
	if (!IsReady()) return {};
	const auto* Rival = Runtime->AIHouses.FindByPredicate([this](const auto& Item)
		{ return Item.HouseId == Runtime->RivalHouseId; });
	return Rival != nullptr ? Rival->Controller.GetDecisionHistory() : TConstArrayView<FHansaMerchantAIDecisionTrace>();
}

TArray<FHansaMerchantAIExplanationProjection> UHansaRuntimeSimulationHost::GetMerchantAIExplanationProjection() const
{
	if (!IsReady()) return {};
	const auto* Rival = Runtime->AIHouses.FindByPredicate([this](const auto& Item)
		{ return Item.HouseId == Runtime->RivalHouseId; });
	return Rival != nullptr ? Rival->Controller.BuildExplanationProjection() : TArray<FHansaMerchantAIExplanationProjection>();
}

const FHansaScenarioProgress* UHansaRuntimeSimulationHost::GetScenarioProgress() const
{
	return IsReady() && Runtime->ScenarioEvaluator.IsInitialized() ? &Runtime->ScenarioEvaluator.GetProgress() : nullptr;
}

uint64 UHansaRuntimeSimulationHost::GetCampaignSeed() const
{
	return IsReady() ? Runtime->CampaignSeed : 0;
}

TConstArrayView<FHansaDomainEvent> UHansaRuntimeSimulationHost::GetEventHistory() const
{
	return IsReady() ? MakeArrayView(Runtime->EventHistory) : TConstArrayView<FHansaDomainEvent>();
}

bool UHansaRuntimeSimulationHost::SynchronizeWorldProjection()
{
	if (!IsReady()) return false;
	const auto Projection = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).BuildProjection();
	if (!Projection) return false;
	UWorld* World = BoundWorld.Get();
	if (World == nullptr) return true;
	AHansaLubeckWorldFoundation* Foundation = nullptr;
	AHansaPlacementProjectionManager* Manager = nullptr;
	for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It) { Foundation = *It; break; }
	for (TActorIterator<AHansaPlacementProjectionManager> It(World); It; ++It) { Manager = *It; break; }
    const bool Success = Foundation == nullptr || Manager == nullptr || Manager->Synchronize(Projection.Value, *Foundation);
    if (Success && Foundation) for (TActorIterator<AHansaCargoProjectionManager> It(World); It; ++It) It->Synchronize(Projection.Value, *this, *Foundation);
    return Success;
}

bool UHansaRuntimeSimulationHost::PublishStateChange(const TConstArrayView<FHansaDomainEvent> Events, const bool bPreserveNavigationPosition)
{
	if (!IsReady()) return false;
	if (UE_LOG_ACTIVE(LogHansa, Verbose))
	{
		FString Triggers;
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			if (Index > 0) Triggers += TEXT(",");
			Triggers += FString::Printf(TEXT("%s(building=%llu)"),
				LexToString(Events[Index].GetType()),
				static_cast<unsigned long long>(Events[Index].GetBuildingId().GetValue()));
		}
		if (Triggers.IsEmpty()) Triggers = TEXT("ExplicitSynchronization");
		UE_LOG(LogHansa, Verbose,
			TEXT("[RoadConnectivity] projectionRefresh tick=%lld triggers=%s"),
			GetSimulationTick(), *Triggers);
	}
	const auto Projection = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).BuildProjection();
	if (!Projection) return false;
	UWorld* World = BoundWorld.Get();
	if (World != nullptr)
	{
		AHansaLubeckWorldFoundation* Foundation = nullptr;
		AHansaPlacementProjectionManager* Manager = nullptr;
		for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It) { Foundation = *It; break; }
		for (TActorIterator<AHansaPlacementProjectionManager> It(World); It; ++It) { Manager = *It; break; }
		if (Foundation != nullptr && Manager != nullptr && !Manager->ConsumeEvents(Events, Projection.Value, *Foundation))
		{
			return false;
		}
	}
    if (World) for (TActorIterator<AHansaLubeckWorldFoundation> F(World); F; ++F)
    {
        for (TActorIterator<AHansaCargoProjectionManager> It(World); It; ++It) It->Synchronize(Projection.Value, *this, **F, bPreserveNavigationPosition);
        break;
    }
	Runtime->EventHistory.Append(Events);
	constexpr int32 MaximumRetainedEvents = 1024;
	if (Runtime->EventHistory.Num() > MaximumRetainedEvents)
	{
		Runtime->EventHistory.RemoveAt(0, Runtime->EventHistory.Num() - MaximumRetainedEvents, EAllowShrinking::No);
	}
	SimulationAdvanced.Broadcast(GetSimulationTick());
	return true;
}

FHansaSaveResult UHansaRuntimeSimulationHost::CaptureSaveBytes(TArray<uint8>& OutBytes,
	const FString& DisplayName, const FString& SavedUtc) const
{
	if (!IsInGameThread() || !IsReady() || (BoundWorld.IsValid() && BoundWorld->GetNetMode() == NM_Client))
	{
		FHansaSaveResult Result; Result.Error = EHansaSaveError::InvalidSnapshot;
		Result.Message = TEXT("Capture requires an initialized authority on the game thread."); return Result;
	}
	FHansaSaveSnapshot Snapshot;
	Snapshot.State = Runtime->State;
	Snapshot.BuildVersion = TEXT("Hansa-TR05-S15"); Snapshot.SavedUtc = SavedUtc; Snapshot.DisplayName = DisplayName;
	Snapshot.NextCommandId = Runtime->NextCommandId; Snapshot.NextBuildingId = Runtime->NextBuildingId;
	Snapshot.MigrationHistory = Runtime->SaveMigrationHistory;
	Snapshot.RouteLabels = Runtime->RouteLabels;
	for (int32 Index = 0; Index < Runtime->HouseIds.Num(); ++Index)
	{
		Snapshot.Players.Add({static_cast<uint64>(Index + 1), Runtime->HouseIds[Index]});
	}
	Snapshot.Scenario = FHansaSaveEnvelope::CaptureScenario(Runtime->ScenarioEvaluator);
	// Commands execute synchronously at tick boundaries; there is no retained runtime queue.
	return FHansaSaveEnvelope::Encode(Snapshot, Runtime->Definitions, OutBytes);
}

FHansaSaveResult UHansaRuntimeSimulationHost::InspectSaveBytes(
	TConstArrayView<uint8> Bytes, FHansaSaveSnapshot& OutSnapshot, int64& OutSimulationTick) const
{
	FHansaSaveResult Failure; Failure.Error = EHansaSaveError::InvalidSnapshot;
	Failure.Message = TEXT("Save inspection requires the matching initialized runtime.");
	if (!IsInGameThread() || !IsReady()) return Failure;
	const FHansaSaveResult Result = FHansaSaveEnvelope::Decode(Bytes, Runtime->Definitions, OutSnapshot);
	if (Result) OutSimulationTick = OutSnapshot.State.CreateReadOnlyAccess(Runtime->Definitions).GetClock().GetTick().GetValue();
	return Result;
}
FHansaSaveResult UHansaRuntimeSimulationHost::RestoreSaveBytes(TConstArrayView<uint8> Bytes)
{
	FHansaSaveResult Failure; Failure.Error = EHansaSaveError::InvalidSnapshot;
	Failure.Message = TEXT("Restore requires the matching initialized authority and valid session ownership.");
	if (!IsInGameThread() || !IsReady() || (BoundWorld.IsValid() && BoundWorld->GetNetMode() == NM_Client)) return Failure;
	FHansaSaveSnapshot Snapshot;
	FHansaSaveResult Result = FHansaSaveEnvelope::Decode(Bytes, Runtime->Definitions, Snapshot);
	if (!Result) return Result;
	// This host has no deferred command scheduler. Never silently discard accepted future work.
	if (!Snapshot.PendingCommands.IsEmpty() || !Snapshot.NextCommandId || !Snapshot.NextBuildingId) return Failure;
	if (Snapshot.Players.Num() != Runtime->HouseIds.Num()) return Failure;
	for (int32 Index = 0; Index < Snapshot.Players.Num(); ++Index)
	{
		if (Snapshot.Players[Index].PrincipalId != static_cast<uint64>(Index + 1) ||
			Snapshot.Players[Index].HouseId != Runtime->HouseIds[Index]) return Failure;
	}
	FHansaScenarioEvaluator Evaluator;
	if (Runtime->ScenarioEvaluator.IsInitialized())
	{
		if (Snapshot.Scenario.ScenarioId != Runtime->ScenarioEvaluator.GetProgress().ScenarioId ||
			!FHansaSaveEnvelope::RestoreScenario(Snapshot.Scenario, Snapshot.State, Runtime->Definitions, Evaluator)) return Failure;
	}
	else if (!Snapshot.Scenario.ScenarioId.IsEmpty()) return Failure;
	Runtime->State = MoveTemp(Snapshot.State); Runtime->ScenarioEvaluator = MoveTemp(Evaluator);
	Runtime->NextCommandId = Snapshot.NextCommandId; Runtime->NextBuildingId = Snapshot.NextBuildingId;
	Runtime->SaveMigrationHistory = MoveTemp(Snapshot.MigrationHistory);
	Runtime->RouteLabels = MoveTemp(Snapshot.RouteLabels);
	Runtime->CampaignSeed = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetCampaignSeed();
	Runtime->Cache.Discard(); Runtime->EventHistory.Reset();
	for (auto& AI : Runtime->AIHouses) AI.Controller = {};
	TickAccumulator = 0.0; Speed = EHansaRuntimeSimulationSpeed::Paused;
    if (!SynchronizeWorldProjection()) return Failure;
	PublishStateChange({});
	return Result;
}

TOptional<FHansaKnownMarketPriceProjection> UHansaRuntimeSimulationHost::QueryKnownMarketPrice(FHansaCityDefinitionId City, FHansaGoodId Good) const
{
    return Runtime.IsValid() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryKnownMarketPrice(City,Good,Runtime->HouseId) : TOptional<FHansaKnownMarketPriceProjection>();
}
TOptional<FHansaKnownMarketSupplyDemandProjection> UHansaRuntimeSimulationHost::QueryKnownMarketSupply(FHansaCityDefinitionId City, FHansaGoodId Good) const
{
    return Runtime.IsValid() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryKnownMarketSupplyDemand(City,Good,Runtime->HouseId) : TOptional<FHansaKnownMarketSupplyDemandProjection>();
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CancelRoute(const FHansaRouteId RouteId)
{
	if (!IsReady()) return FHansaCommandGatewayResult();
	const FHansaSimulationReadOnlyAccess ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	FHansaCommandHeader Header;
	Header.CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId).Value;
	Header.Authority = FHansaCommandAuthorityContext { GetHouseId(), 1, EHansaCommandOrigin::PlayerInput };
	Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
	Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
		Header, FHansaCancelRouteCommand { RouteId });
	FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		Runtime->State, Runtime->Definitions, MakeArrayView(&Command, 1), Runtime->Cache);
	if (Result)
	{
		++Runtime->NextCommandId;
		PublishStateChange(Result.GetEvents());
	}
	return Result;
}

FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CancelRouteForAuthority(
	const FHansaCommandAuthorityContext& Authority, const FHansaRouteId RouteId)
{
	if (!IsReady()) return {};
	auto Result = ExecuteRuntimeCommand(*Runtime, Authority, FHansaCancelRouteCommand { RouteId });
	if (Result) { ++Runtime->NextCommandId; PublishStateChange(Result.GetEvents()); }
	return Result;
}

bool UHansaRuntimeSimulationHost::IsCompoundTerrainBuildable(const FHansaPlacementSpec& Spec,int32 BatchOffset) const
{
 UWorld* World=BoundWorld.Get();if(!World)return true;
 const auto* Compiled=FindBuildingDefinition(Spec.BuildingDefinitionId.ToString());
 if(!Compiled||Compiled->ResidentialCompoundId.IsEmpty())return true;
 const auto* Building=Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(Spec.BuildingDefinitionId.ToString()));
 auto* D=Building?Building->LoadResidentialCompound():nullptr;if(!D)return false;
 AHansaLubeckWorldFoundation* Foundation=nullptr;
 for(TActorIterator<AHansaLubeckWorldFoundation> It(World);It;++It){Foundation=*It;break;}
 if(!Foundation)return true;
 const int32 Turn=static_cast<int32>(Spec.Rotation);
 const int32 W=Turn%2?D->FootprintHeightCells:D->FootprintWidthCells;
 const int32 H=Turn%2?D->FootprintWidthCells:D->FootprintHeightCells;
 const FVector First=Hansa::Game::LubeckPlacementGrid::GridToWorld(Spec.Anchor);
 const FVector Last=Hansa::Game::LubeckPlacementGrid::GridToWorld({Spec.Anchor.X+W-1,Spec.Anchor.Y+H-1});
 const FTransform Transform=FTransform(FRotator(0,Turn*90,0),(First+Last)*.5)*Foundation->GetActorTransform();
 const uint64 Seed=UHansaResidentialCompoundDefinition::ParcelSeed(Spec.CityId.ToString(),Runtime->NextBuildingId+BatchOffset,0);
 // A later adjacent road can choose a different context; all supported contexts must fit.
 for(FName Context:{FName(TEXT("Straight")),FName(TEXT("CornerLeft")),FName(TEXT("CornerRight")),FName(TEXT("Edge"))})
 {
  if(!D->Layouts.ContainsByPredicate([&](const auto& L){return L.Context==Context&&L.DevelopmentStage==Building->CompoundStage&&(L.DistrictIds.IsEmpty()||L.DistrictIds.Contains(Building->CompoundDistrictId));}))continue;
  if(!Hansa::Game::CompoundGround::CanPlace(World,Transform,D->Compose(Seed,Building->CompoundStage,Context,Building->CompoundDistrictId),FBox(D->BoundsMin,D->BoundsMax)))return false;
 }
 return true;
}

uint64 UHansaRuntimeSimulationHost::GetNextParcelSeed() const
{
 return IsReady()?UHansaResidentialCompoundDefinition::ParcelSeed(Runtime->CityId.ToString(),Runtime->NextBuildingId,0):0;
}
