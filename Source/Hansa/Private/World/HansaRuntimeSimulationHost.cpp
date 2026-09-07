#include "World/HansaRuntimeSimulationHost.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HansaLog.h"
#include "Systems/HansaSimulationPipeline.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckWorldFoundation.h"

using namespace Hansa::Simulation;

struct FHansaRuntimeSimulationState final
{
	FHansaSimulationDefinitionContext Definitions;
	FHansaSimulationState State;
	FHansaSimulationTransientCache Cache;
	FHansaHouseId HouseId;
	FHansaHouseId RivalHouseId;
	FHansaMerchantAIController MerchantAI;
	FHansaScenarioEvaluator ScenarioEvaluator;
	FHansaCityDefinitionId CityId;
	TArray<FString> SaveMigrationHistory;
	uint64 NextCommandId = 1;
	uint64 NextBuildingId = 1;
	uint64 CampaignSeed = 0;
	TArray<FHansaDomainEvent> EventHistory;
	EHansaRuntimeScenario Scenario = EHansaRuntimeScenario::LubeckGrainShortage;
	bool bReady = false;
	bool bMerchantAIEnabled = true;
};

UHansaRuntimeSimulationHost::~UHansaRuntimeSimulationHost() = default;

bool UHansaRuntimeSimulationHost::InitializeForLubeck(
	UWorld* World,
	FString& OutError,
	const EHansaRuntimeScenario Scenario,
	const uint64 CampaignSeedOverride)
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
	const auto Placement = Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(
		HouseId.Value, Registry);
	if (!Placement)
	{
		OutError = TEXT("Unable to create the Lübeck placement grid.");
		Runtime.Reset();
		return false;
	}

	FHansaLubeckScenarioState ScenarioState;
	if (!FHansaLubeckScenarioInitializer::TryCreate(
		Scenario, MoveTemp(Registry), Placement.Value, ScenarioState, OutError, CampaignSeedOverride))
	{
		Runtime.Reset();
		return false;
	}

	Runtime->Definitions = MoveTemp(ScenarioState.Definitions);
	Runtime->State = MoveTemp(ScenarioState.State);
	Runtime->HouseId = ScenarioState.HouseId;
	Runtime->RivalHouseId = ScenarioState.RivalHouseId;
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

EHansaRuntimeScenario UHansaRuntimeSimulationHost::GetScenario() const
{
	return IsReady() ? Runtime->Scenario : EHansaRuntimeScenario::LubeckGrainShortage;
}

void UHansaRuntimeSimulationHost::SetSpeed(const EHansaRuntimeSimulationSpeed NewSpeed)
{
	Speed = NewSpeed;
	if (Speed == EHansaRuntimeSimulationSpeed::Paused) TickAccumulator = 0.0;
}

bool UHansaRuntimeSimulationHost::IsReady() const
{
	return Runtime.IsValid() && Runtime->bReady;
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
	const double Rate = TicksPerSecond();
	if (!IsReady() || DeltaSeconds <= 0.0 || Rate <= 0.0) return IsReady();
	TickAccumulator += FMath::Min(DeltaSeconds, 1.0) * Rate;
	const int32 DueTicks = FMath::Min(FMath::FloorToInt(TickAccumulator), 16);
	if (DueTicks <= 0) return true;
	if (!AdvanceTicks(DueTicks)) return false;
	TickAccumulator -= DueTicks;
	return true;
}

bool UHansaRuntimeSimulationHost::AdvanceTicks(const int32 TickCount)
{
	if (!IsReady() || TickCount < 0 || TickCount > 10000) return false;
	TArray<FHansaDomainEvent> Events;
	for (int32 Index = 0; Index < TickCount; ++Index)
	{
		const FHansaSimulationReadOnlyAccess Before = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
		TOptional<FHansaGameplayCommand> AICommand;
		if (Runtime->Scenario == EHansaRuntimeScenario::LubeckGrainShortage &&
			Runtime->bMerchantAIEnabled)
		{
			const FHansaEconomicRegistry* Registry = Runtime->Definitions.GetEconomicRegistry();
			const FHansaCompiledMerchantAITuning* Tuning = Registry != nullptr
				? Registry->FindMerchantAITuning(TEXT("AITuning.MerchantRival")) : nullptr;
			if (Tuning != nullptr && Runtime->MerchantAI.IsDue(Before, *Tuning))
			{
				const auto CommandId = FHansaCommandId::TryCreate(Runtime->NextCommandId);
				if (!CommandId) return false;
				FHansaMerchantAIDecision Decision = Runtime->MerchantAI.Evaluate(
					Before, *Registry, *Tuning, Runtime->RivalHouseId, 2, CommandId.Value);
				AICommand = MoveTemp(Decision.Command);
			}
		}
		const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
			Runtime->State, Runtime->Definitions,
			AICommand.IsSet() ? MakeArrayView(&AICommand.GetValue(), 1) : TConstArrayView<FHansaGameplayCommand>(),
			Runtime->Cache);
		if (AICommand.IsSet())
		{
			Runtime->MerchantAI.RecordGatewayOutcome(Result);
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
	return IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).ValidatePlacement(Runtime->HouseId, Spec)
		: FHansaPlacementValidationResult();
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
	return IsReady() ? Runtime->MerchantAI.GetLastDecision() : nullptr;
}

TConstArrayView<FHansaMerchantAIDecisionTrace> UHansaRuntimeSimulationHost::GetMerchantAIDecisionHistory() const
{
	return IsReady() ? Runtime->MerchantAI.GetDecisionHistory() : TConstArrayView<FHansaMerchantAIDecisionTrace>();
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
	return Foundation == nullptr || Manager == nullptr || Manager->Synchronize(Projection.Value, *Foundation);
}

bool UHansaRuntimeSimulationHost::PublishStateChange(const TConstArrayView<FHansaDomainEvent> Events)
{
	if (!IsReady()) return false;
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
	Snapshot.BuildVersion = TEXT("Hansa-S11-P04"); Snapshot.SavedUtc = SavedUtc; Snapshot.DisplayName = DisplayName;
	Snapshot.NextCommandId = Runtime->NextCommandId; Snapshot.NextBuildingId = Runtime->NextBuildingId;
	Snapshot.MigrationHistory = Runtime->SaveMigrationHistory;
	Snapshot.Players.Add({1, Runtime->HouseId});
	if (Runtime->RivalHouseId.IsValid()) Snapshot.Players.Add({2, Runtime->RivalHouseId});
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
	for (const auto& P : Snapshot.Players)
		if ((P.PrincipalId != 1 || P.HouseId != Runtime->HouseId) &&
			(P.PrincipalId != 2 || P.HouseId != Runtime->RivalHouseId)) return Failure;
	if (Snapshot.Players.Num() != (Runtime->RivalHouseId.IsValid() ? 2 : 1)) return Failure;
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
	Runtime->CampaignSeed = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetCampaignSeed();
	Runtime->Cache.Discard(); Runtime->EventHistory.Reset(); Runtime->MerchantAI = {};
	TickAccumulator = 0.0; Speed = EHansaRuntimeSimulationSpeed::Paused;
	PublishStateChange({});
	return Result;
}
