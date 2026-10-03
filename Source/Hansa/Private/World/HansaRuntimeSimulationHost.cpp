#include "World/HansaRuntimeSimulationHost.h"
#include "Placement/HansaRostockPlacement.h"
#include "Presence/HansaPresenceConstruction.h"
#include "Misc/SecureHash.h"
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
#include "Misc/ConfigCacheIni.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Systems/HansaSimulationPipeline.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckWorldFoundation.h"

using namespace Hansa::Simulation;

#if !UE_BUILD_SHIPPING
namespace Hansa::Simulation
{
// Isolated developer fixture: no definition or save schema is changed. The office
// itself is deliberately not granted; the player still uses the ordinary commands.
class FHansaMerchantOfficeTestSetup final
{
public:
 static bool Apply(FHansaSimulationState& State,const FHansaSimulationDefinitionContext& Definitions,
  FHansaHouseId HouseId,FString& Error)
 {
  const auto* Registry=Definitions.GetEconomicRegistry();
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock"));
  const auto* Policy=Registry&&City?Registry->FindCityTradePolicyForCity(City.Value.ToString()):nullptr;
  const auto* TradeStage=Registry?Registry->FindPresenceStage(TEXT("PresenceStage.TradeStation")):nullptr;
  const auto* Office=Registry?Registry->FindPresenceStage(TEXT("PresenceStage.MerchantOffice")):nullptr;
  if(!City||!Policy||!TradeStage||!Office){Error=TEXT("Merchant Office test definitions are unavailable.");return false;}
  FHansaSimulationState Candidate=State;
  auto* Presence=Candidate.ForeignPresences.FindByPredicate([&](const auto& P){return P.HouseId==HouseId&&P.CityId==City.Value;});
  auto* House=Candidate.Houses.FindByPredicate([&](const auto& H){return H.Id==HouseId;});
  if(!Presence||!House||Presence->Status!=EHansaForeignPresenceStatus::Active){Error=TEXT("Player has no active Rostock presence.");return false;}
  if(Presence->CurrentStageId==Office->StableId)return true;
  if(Presence->CurrentStageId!=TradeStage->StableId&&Presence->CurrentStageId!=Policy->InitialStageId){Error=TEXT("Rostock is already beyond the station test stage.");return false;}
  if(Presence->Upgrade.Status!=EHansaPresenceUpgradeStatus::None){Error=TEXT("An office upgrade is already in progress.");return false;}
  const auto Tick=Candidate.Clock.GetTick();
  auto* Station=Candidate.TradeStations.FindByPredicate([&](const auto& S){return S.Id==Presence->StationId&&S.OwnerId==HouseId&&S.CityId==City.Value;});
  if(!Station)
  {
   const FHansaCompiledCityTradePolicyDefinition::FStationSite* Site=nullptr;
   FHansaGridCoordinate Anchor;
   for(const auto& Option:Policy->TradeStationSites){
    if(Candidate.LeasedPlots.ContainsByPredicate([&](const auto& L){return L.CityId==City.Value&&L.SiteId==Option.SiteId&&L.bActive;}))continue;
    for(int32 X=Option.LeaseBoundsMin.X;X<Option.LeaseBoundsMax.X&&!Site;++X)
     for(int32 Y=Option.LeaseBoundsMin.Y;Y<Option.LeaseBoundsMax.Y;++Y)
      if(FHansaPresenceConstructionRules::ValidateSite(Candidate.Placement,City.Value,Option.LeaseBoundsMin,Option.LeaseBoundsMax,{X,Y},EHansaGridRotation::North).IsEmpty())
      {Site=&Option;Anchor={X,Y};break;}
    if(Site)break;
   }
   if(!Site){Error=TEXT("No clear Rostock commercial lease is available for the test station.");return false;}
   uint64 StationValue=1,LeaseValue=1,InventoryValue=1,FactorValue=1;
   for(const auto& S:Candidate.TradeStations){StationValue=FMath::Max(StationValue,S.Id.GetValue()+1);FactorValue=FMath::Max(FactorValue,S.FactorId.GetValue()+1);}
   for(const auto& L:Candidate.LeasedPlots)LeaseValue=FMath::Max(LeaseValue,L.Id.GetValue()+1);
   for(const auto& I:Candidate.InventoryLedger.Inventories)InventoryValue=FMath::Max(InventoryValue,I.Id.GetValue()+1);
   FHansaTradeStationState NewStation;NewStation.Id=FHansaTradeStationId::TryCreate(StationValue).Value;
   NewStation.OwnerId=HouseId;NewStation.CityId=City.Value;NewStation.SiteId=Site->SiteId;
   NewStation.InventoryId=FHansaInventoryId::TryCreate(InventoryValue).Value;
   NewStation.FactorId=FHansaFactorId::TryCreate(FactorValue).Value;
   NewStation.LeasedPlotId=FHansaLeasedPlotId::TryCreate(LeaseValue).Value;
   NewStation.Status=EHansaTradeStationStatus::Active;NewStation.ProposedTick=Tick;
   NewStation.FundedTick=Tick;NewStation.CompletionTick=Tick;NewStation.CompletedTick=Tick;
   NewStation.OperationalStateChangedTick=Tick;NewStation.SpentMoneyRaw=TradeStage->UpgradeCostPfennig;
   NewStation.UpkeepPfennigPerTick=Site->UpkeepPfennigPerTick;
   NewStation.FundingInventoryId=NewStation.InventoryId;
   NewStation.ConstructionSite={true,Anchor,EHansaGridRotation::North};
   FHansaLeasedPlotState Lease;Lease.Id=NewStation.LeasedPlotId;Lease.StationId=NewStation.Id;
   Lease.OwnerId=HouseId;Lease.CityId=City.Value;Lease.SiteId=Site->SiteId;
   Lease.PlotCategory=Site->PlotCategory;Lease.BoundsMin=Site->LeaseBoundsMin;Lease.BoundsMax=Site->LeaseBoundsMax;
   Lease.PermittedBuildingCategories=Site->PermittedBuildingCategories;Lease.bActive=true;Lease.bOccupied=true;
   FHansaInventoryInitialization Inventory;Inventory.Id=NewStation.InventoryId;
   Inventory.OwnerKind=EHansaInventoryOwnerKind::TradeStation;Inventory.CityId=City.Value;
   Inventory.TradeStationId=NewStation.Id;
   Inventory.Capacity=FHansaQuantity::FromRaw(FMath::Max<int64>(Site->StorageCapacityMilliUnits,50000));
   for(const auto& Good:Registry->GetGoods())Inventory.AcceptedGoods.Add(FHansaGoodId::TryParse(Good.StableId).Value);
   Inventory.AcceptedGoods.Sort();
   if(!Candidate.InventoryLedger.TryAddEmptyInventory(MoveTemp(Inventory))){Error=TEXT("Could not create the Rostock station inventory.");return false;}
   Candidate.LeasedPlots.Add(MoveTemp(Lease));Candidate.TradeStations.Add(MoveTemp(NewStation));
   Presence->StationId=Candidate.TradeStations.Last().Id;Presence->LeasedPlotId=Candidate.TradeStations.Last().LeasedPlotId;
   Station=&Candidate.TradeStations.Last();
  }
  else
  {
   if(!Station->ConstructionSite.bLocalDelivery){Error=TEXT("The existing station has no placed construction site.");return false;}
   if(Station->Status==EHansaTradeStationStatus::Closed){Error=TEXT("The existing station is closed.");return false;}
   for(const auto Reservation:Station->DeliveryReservations)
    if(!Candidate.InventoryLedger.TryReleaseReservation(Reservation,Tick,Candidate.InventoryLedger.LastMovementSequence+1).IsSuccess())
    {Error=TEXT("Could not release a station delivery reservation.");return false;}
   Station->DeliveryReservations.Reset();Station->DeliveryMode=0;
   Station->Status=EHansaTradeStationStatus::Active;Station->OperationalState=EHansaTradeStationOperationalState::Active;
   Station->OperationalStateChangedTick=Tick;Station->OutstandingUpkeepPfennig=0;
   Station->FundedTick=Tick;Station->CompletionTick=Tick;Station->CompletedTick=Tick;
   Station->FundingInventoryId=Station->InventoryId;
  }
  Presence->CurrentStageId=TradeStage->StableId;
  Presence->GrantedCapabilityIds=TradeStage->GrantedCapabilityIds;
  Presence->GrantedCapabilityIds.RemoveAll([&](const FString& Id){return Policy->DeniedCapabilityIds.Contains(Id);});
  Presence->GrantedCapabilityIds.Sort();Presence->LastUpgradeTick=Tick;
  auto& C=Presence->Contributions;
  C.LawfulTradeVolumeMilliUnits=FMath::Max(C.LawfulTradeVolumeMilliUnits,Office->RequiredLawfulTradeVolumeMilliUnits);
  C.CompletedDeliveryCount=FMath::Max(C.CompletedDeliveryCount,Office->RequiredCompletedDeliveries);
  C.InvestedPfennig=FMath::Max(C.InvestedPfennig,Office->RequiredInvestedPfennig);
  C.TransactionValuePfennig=FMath::Max(C.TransactionValuePfennig,Office->RequiredTransactionValuePfennig);
  C.FulfilledShortageMilliUnits=FMath::Max(C.FulfilledShortageMilliUnits,Office->RequiredFulfilledShortageMilliUnits);
  C.ReliableOperatingTicks=FMath::Max(C.ReliableOperatingTicks,Office->RequiredReliableOperatingTicks);
  C.SolventOperatingTicks=FMath::Max(C.SolventOperatingTicks,Office->RequiredSolventOperatingTicks);
  House->Money=FHansaMoney::FromRaw(FMath::Max<int64>(House->Money.GetRawValue(),250000));
  const auto LedgerView=Candidate.InventoryLedger.CreateReadOnlyAccess();
  for(const auto& Cost:Office->UpgradeGoods){
   // TryTransfer atomically replaces the ledger, so reacquire this record each time.
   auto* Inventory=Candidate.InventoryLedger.Inventories.FindByPredicate([&](const auto& I){return I.Id==Station->InventoryId;});
   if(!Inventory){Error=TEXT("The station has no inventory.");return false;}
   const auto Good=FHansaGoodId::TryParse(Cost.GoodId);if(!Good){Error=TEXT("An office material ID is invalid.");return false;}
   if(!Inventory->AcceptedGoods.Contains(Good.Value)){Inventory->AcceptedGoods.Add(Good.Value);Inventory->AcceptedGoods.Sort();}
   const auto Stock=LedgerView.QueryStock(Station->InventoryId,Good.Value);
   const int64 Available=Stock?Stock->Available.GetRawValue():0;
   const int64 Missing=FMath::Max<int64>(0,Cost.QuantityMilliUnits-Available);
   if(Missing>0){
    const auto Capacity=Inventory->Capacity.GetRawValue();
    if(!Candidate.InventoryLedger.TrySetCapacity(Station->InventoryId,FHansaQuantity::FromRaw(FMath::Max<int64>(Capacity,200000)))){Error=TEXT("Could not enlarge the test station inventory.");return false;}
    const auto Transfer=Candidate.InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Source(TEXT("MerchantOfficeTest")),
     FHansaInventoryEndpoint::Inventory(Station->InventoryId),Good.Value,FHansaQuantity::FromRaw(Missing),Tick,Candidate.InventoryLedger.LastMovementSequence+1);
    if(!Transfer.IsSuccess()){Error=FString::Printf(TEXT("Could not stock %s: %s"),*Cost.GoodId,LexToString(Transfer.Error));return false;}
   }
  }
  Candidate.TradeStations.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
  Candidate.LeasedPlots.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
  Candidate.InvalidateAllStateHashCaches();State=MoveTemp(Candidate);return true;
 }
};
}
#endif

struct FHansaRuntimeSimulationState final
{
	struct FSurveyCache { uint64 Signature=0; FHansaLandQueryResult Result; };
    TMap<FString,FSurveyCache> LandSurveys;
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
	// Derived presentation data, rebuilt only when the authoritative fingerprint changes.
	TOptional<FHansaSimulationProjection> CachedProjection;
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
#if !UE_BUILD_SHIPPING
    bool bMerchantOfficeTest=false;
    if(GConfig)GConfig->GetBool(TEXT("Hansa.MerchantOfficeTest"),TEXT("bEnableAtNewGame"),bMerchantOfficeTest,GGameIni);
    if(bMerchantOfficeTest && !Candidate->ApplyMerchantOfficeTestSetup(OutError))return false;
#endif
    Runtime=MoveTemp(Candidate->Runtime);TickAccumulator=0;Speed=EHansaRuntimeSimulationSpeed::Paused;
    return SynchronizeWorldProjection() && PublishStateChange({});
}

#if !UE_BUILD_SHIPPING
bool UHansaRuntimeSimulationHost::ApplyMerchantOfficeTestSetup(FString& OutError)
{
 if(!IsReady() || (BoundWorld.IsValid()&&BoundWorld->GetNetMode()==NM_Client))
 {OutError=TEXT("Merchant Office test setup requires an initialized authority.");return false;}
 FHansaSimulationState Before=Runtime->State;
 if(!FHansaMerchantOfficeTestSetup::Apply(Runtime->State,Runtime->Definitions,Runtime->HouseId,OutError))return false;
 TArray<uint8> Bytes;
 const auto Encoded=CaptureSaveBytes(Bytes,TEXT("Merchant Office construction test validation"),FDateTime::UtcNow().ToIso8601());
 FHansaSaveSnapshot Checked;int64 Tick=0;
 const auto Decoded=Encoded?InspectSaveBytes(Bytes,Checked,Tick):Encoded;
 if(!Decoded){Runtime->State=MoveTemp(Before);OutError=Decoded.Message;return false;}
 Runtime->Cache.Discard();Runtime->CachedProjection.Reset();
 if(!SynchronizeWorldProjection()||!PublishStateChange({}))
 {Runtime->State=MoveTemp(Before);OutError=TEXT("Could not publish the Merchant Office test setup.");return false;}
 return true;
}
#endif

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
	TRACE_CPUPROFILER_EVENT_SCOPE(Hansa_AdvanceRealTime);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Hansa_AdvanceTicks);
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

const FHansaPlacementMapInitialization* UHansaRuntimeSimulationHost::FindPlacementMap(FHansaCityDefinitionId City) const
{
	return IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetPlacement().FindMap(City.IsValid()?City:Runtime->CityId) : nullptr;
}

FString UHansaRuntimeSimulationHost::TradeHousePlacementError(const FHansaPlacementSpec& Spec,bool bGeometryOnly) const
{
 if(!IsReady())return TEXT("Start or load a campaign first.");
 const auto P=BuildProjection();if(!P)return TEXT("City information is unavailable.");
 const auto* Policy=GetEconomicRegistry()->FindCityTradePolicyForCity(Spec.CityId.ToString());
 const auto* Presence=P.Value.GetForeignPresences().FindByPredicate([&](const auto& V){return V.HouseId==GetHouseId()&&V.CityId==Spec.CityId;});
 if(!Policy)return TEXT("No commercial lease is authored for this city.");
 if(Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation")))return TEXT("This city does not permit trade stations.");
 if(!bGeometryOnly){
 if(!Presence||Presence->Status!=EHansaForeignPresenceStatus::Active||Presence->StationId.IsValid())return TEXT("A new trade house requires an available foreign-city presence and lease.");
 const auto* Stage=GetEconomicRegistry()->GetPresenceStages().FindByPredicate([&](const auto& V){return V.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"))&&GetEconomicRegistry()->IsValidPresenceTransition(Spec.CityId.ToString(),Presence->CurrentStageId,V.StableId);});
 if(!Stage)return TEXT("This city does not permit a new trade house at your current presence stage.");
 const auto* House=P.Value.GetHouses().FindByPredicate([&](const auto& H){return H.Id==GetHouseId();});
 if(!House||House->Money.GetRawValue()<Stage->UpgradeCostPfennig)return TEXT("Not enough money to place this trade house.");
 }
 // A remote HUD may use the empty-Lubeck preview host. Its public Rostock
 // topology must come from the same canonical survey as server initialization.
 static const auto PublicRostock=FHansaPlacementState::TryCreate({{RostockPlacement::CreateMap()},{},{}});
 const auto& Geometry=bGeometryOnly&&Spec.CityId.ToString()==TEXT("City.Rostock")&&PublicRostock?PublicRostock.Value:Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetPlacement();
 FString Error=TEXT("No available commercial lease at this location.");
 for(const auto& Site:Policy->TradeStationSites){
  if(!bGeometryOnly&&P.Value.GetLeasedPlots().ContainsByPredicate([&](const auto& L){return L.CityId==Spec.CityId&&L.SiteId==Site.SiteId&&L.bOccupied;}))continue;
  Error=FHansaPresenceConstructionRules::ValidateSite(Geometry,Spec.CityId,Site.LeaseBoundsMin,Site.LeaseBoundsMax,Spec.Anchor,Spec.Rotation,!bGeometryOnly);
  if(Error.IsEmpty())return {};
 }return Error;
}

FHansaPlacementValidationResult UHansaRuntimeSimulationHost::ValidatePlacement(const FHansaPlacementSpec& Spec) const
{
 auto Result=IsReady() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).ValidatePlacement(Runtime->HouseId, Spec)
  : FHansaPlacementValidationResult();
 if(IsReady()&&!IsCompoundTerrainBuildable(Spec))Result=Result.WithTerrainFailure(Spec.Anchor);
 return Result;
}

FHansaLandQueryResult UHansaRuntimeSimulationHost::QueryLand(
	const FHansaHouseId ViewerHouseId, const FHansaCityDefinitionId CityId,
	const FHansaGridCoordinate BoundsMin, const FHansaGridCoordinate BoundsMax, bool bCompactSurvey) const
{
    if (IsReady() && bCompactSurvey)
    {
        const auto Read=Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
        uint64 Signature=14695981039346656037ULL;
        auto Add=[&](uint64 V){Signature^=V;Signature*=1099511628211ULL;};
        Add(reinterpret_cast<UPTRINT>(Read.GetPlacement().FindMap(CityId)));
        for(const auto& P:Read.GetPlacement().GetPlacements()) if(P.Spec.CityId==CityId) { Add(P.BuildingId.GetValue()); Add(P.BuildingId.GetGeneration()); }
        for(const auto& L:Read.GetLeasedPlots()) if(L.CityId==CityId && L.OwnerId==ViewerHouseId)
        { Add(L.Id.GetValue());Add(L.bActive);Add(L.BoundsMin.X);Add(L.BoundsMin.Y);Add(L.BoundsMax.X);Add(L.BoundsMax.Y);for(const auto& C:L.PermittedBuildingCategories)Add(GetTypeHash(C)); }
        for(const auto& P:Read.GetForeignPresences()) if(P.CityId==CityId && P.HouseId==ViewerHouseId)
        { Add(uint8(P.Status));for(const auto& C:P.GrantedCapabilityIds)Add(GetTypeHash(C)); }
        const FString Key=CityId.ToString()+TEXT(":")+LexToString(ViewerHouseId.GetValue());
        if(!Runtime->LandSurveys.Contains(Key) && Runtime->LandSurveys.Num()>=16) Runtime->LandSurveys.Reset();
        auto& Cache=Runtime->LandSurveys.FindOrAdd(Key);
        if(Cache.Signature!=Signature || !Cache.Result.ViewerHouseId.IsValid())
        { Cache.Result=Read.QueryLand(ViewerHouseId,CityId,{}, {},true);Cache.Signature=Signature; }
        FHansaLandQueryResult Result;
        Result.CityId=Cache.Result.CityId;Result.ViewerHouseId=Cache.Result.ViewerHouseId;
        Result.StateRevision=Cache.Result.StateRevision;Result.Failure=Cache.Result.Failure;
        Result.BoundsMin=Cache.Result.BoundsMin;Result.BoundsMax=Cache.Result.BoundsMax;
        Result.SurveyPages=Cache.Result.SurveyPages;Result.ViewerLeases=Cache.Result.ViewerLeases;
        // A page has at most 256 exact runs. Never send the dense city grid.
        const int32 Page=BoundsMin.X;
        if(Page<0 || Page>=Result.SurveyPages) { Result.Cells.Reset();Result.Failure=EHansaLandQueryFailure::InvalidBounds;return Result; }
        const int32 Start=Page*256, Count=FMath::Min(256,Cache.Result.Cells.Num()-Start);
        Result.Cells.Reset(Count);Result.Cells.Append(Cache.Result.Cells.GetData()+Start,Count);
        return Result;
    }
    if (IsReady()) return Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryLand(
        ViewerHouseId, CityId, BoundsMin, BoundsMax);
	FHansaLandQueryResult Result;
	Result.Failure = EHansaLandQueryFailure::UnknownCity;
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

FHansaConstructionCostProjection UHansaRuntimeSimulationHost::QueryConstructionCost(const FString& BuildingStableId, FHansaCityDefinitionId City) const
{
	const auto BuildingId = FHansaBuildingTypeId::TryParse(BuildingStableId);
	return IsReady() && BuildingId
		? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryConstructionCost(
			Runtime->HouseId, City.IsValid()?City:Runtime->CityId, BuildingId.Value)
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
    if(Specs.Num()==1&&Specs[0].BuildingDefinitionId.ToString()==TEXT("Building.TradeHouse")){
        const auto* Policy=GetEconomicRegistry()->FindCityTradePolicyForCity(Specs[0].CityId.ToString());
        FString SiteId;
        if(Policy)for(const auto& Site:Policy->TradeStationSites)if(FHansaPresenceConstructionRules::ValidateSite(Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetPlacement(),Specs[0].CityId,Site.LeaseBoundsMin,Site.LeaseBoundsMax,Specs[0].Anchor,Specs[0].Rotation).IsEmpty()){SiteId=Site.SiteId;break;}
        // Submit invalid sites too: the gateway must return an explicit atomic rejection.
        FHansaTradeStationId Id;const FHansaPresenceConstructionSite Placement{true,Specs[0].Anchor,Specs[0].Rotation};
        return ProposeTradeStationForAuthority(Authority,Specs[0].CityId,SiteId,Id,&Placement);
    }

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
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ProposeTradeStationForAuthority(const FHansaCommandAuthorityContext& Authority, FHansaCityDefinitionId CityId, const FString& SiteId, FHansaTradeStationId& OutStationId,const FHansaPresenceConstructionSite* Placement)
{
	if(!IsReady())return {}; const uint64 Value=0x4000000000000000ULL+Runtime->NextCommandId;
	const auto Station=FHansaTradeStationId::TryCreate(Value);
	const auto Factor=FHansaFactorId::TryCreate(Value);
	const auto Lease=FHansaLeasedPlotId::TryCreate(Value);
	const auto Inventory=FHansaInventoryId::TryCreate(Value);
	if(!Station||!Factor||!Lease||!Inventory)return {};
	FHansaProposeTradeStationCommand Payload{Station.Value,Factor.Value,Lease.Value,Inventory.Value,CityId,SiteId};
    if(Placement){Payload.bPlaceAndPay=true;Payload.Anchor=Placement->Anchor;Payload.Rotation=Placement->Rotation;}
	auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Payload);if(Result){++Runtime->NextCommandId;OutStationId=Station.Value;PublishStateChange(Result.GetEvents(),true);}return Result;
}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundTradeStation(FHansaTradeStationId StationId,FHansaInventoryId FundingInventoryId,uint8 DeliveryMode)
{return FundTradeStationForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},StationId,FundingInventoryId,DeliveryMode);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundTradeStationForAuthority(const FHansaCommandAuthorityContext& Authority,FHansaTradeStationId StationId,FHansaInventoryId FundingInventoryId,uint8 DeliveryMode)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,FHansaFundTradeStationCommand{StationId,FundingInventoryId,DeliveryMode});if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
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
FString UHansaRuntimeSimulationHost::GetAuthorityScenarioId() const { return IsReady()?Runtime->Definitions.GetScenarioId().ToString():FString(); }
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ManageCityPrivilege(const FHansaManageCityPrivilegeCommand& Payload)
{return ManageCityPrivilegeForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::ManageCityPrivilegeForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaManageCityPrivilegeCommand& Payload)
{if(!IsReady())return {};auto Resolved=Payload;if(Resolved.Action==EHansaCityPrivilegeAction::Acquire){uint64 Next=1;for(const auto& L:Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetLeasedPlots()){if(L.Id.GetValue()==MAX_uint64)return {};Next=FMath::Max(Next,L.Id.GetValue()+1);}Resolved.GrantedLeaseId=FHansaLeasedPlotId::TryCreate(Next).Value;}auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Resolved);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundCityProject(const FHansaFundCityProjectCommand& Payload)
{return FundCityProjectForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::FundCityProjectForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaFundCityProjectCommand& Payload)
{if(!IsReady())return {};auto Resolved=Payload;auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Resolved);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::TransitionCityAuthority(const FHansaTransitionCityAuthorityCommand& Payload)
{return TransitionCityAuthorityForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},Payload);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::TransitionCityAuthorityForAuthority(const FHansaCommandAuthorityContext& Authority,const FHansaTransitionCityAuthorityCommand& Payload)
{if(!IsReady())return {};auto Resolved=Payload;auto Result=ExecuteRuntimeCommand(*Runtime,Authority,Resolved);if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CloseTradeStation(FHansaTradeStationId StationId)
{return CloseTradeStationForAuthority({GetHouseId(),1,EHansaCommandOrigin::PlayerInput},StationId);}
FHansaCommandGatewayResult UHansaRuntimeSimulationHost::CloseTradeStationForAuthority(const FHansaCommandAuthorityContext& Authority,FHansaTradeStationId StationId)
{if(!IsReady())return {};auto Result=ExecuteRuntimeCommand(*Runtime,Authority,FHansaCloseTradeStationCommand{StationId});if(Result){++Runtime->NextCommandId;PublishStateChange(Result.GetEvents(),true);}return Result;}

TArray<FHansaVisitingTradeOffer> UHansaRuntimeSimulationHost::BuildVisitingTradeOffers(FHansaHouseId Viewer) const
{
 TArray<FHansaVisitingTradeOffer> Offers;
 if(!IsReady()||!Viewer.IsValid())return Offers;
 const auto Access=Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
 const auto Projection=Access.BuildProjection();if(!Projection)return Offers;
 const auto& P=Projection.Value;
 const auto* House=P.GetHouses().FindByPredicate([&](const auto& H){return H.Id==Viewer;});
 for(const auto& V:P.GetVehicles()){
  if(V.OwnerId!=Viewer||V.Mode!=EHansaRouteMode::Sea)continue;
  const auto* Inventory=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==V.CargoInventoryId;});
  for(const auto& M:P.GetMarkets()){
   // Only the ship's location is a quote context. Traveling ships retain the authoritative blocker.
   if(M.CityId!=V.CurrentCityId)continue;
   const auto Known=Access.QueryKnownMarketPrice(M.CityId,M.GoodId,Viewer);
   const auto Supply=Access.QueryKnownMarketSupplyDemand(M.CityId,M.GoodId,Viewer);
   for(bool Buy:{true,false}){
    const auto Q=Access.QuerySpotTradeQuote(Viewer,V.Id,M.CityId,M.GoodId,Buy?EHansaSpotTradeSide::BuyFromCity:EHansaSpotTradeSide::SellToCity,FHansaQuantity::FromRaw(1'000'000'000));
    FHansaVisitingTradeOffer O;O.City=M.CityId.ToString();O.Good=M.GoodId.ToString();O.Vehicle=int64(V.Id.GetValue());O.bBuy=Buy;
    O.bCanSubmit=Q.bCanSubmit;O.Cause=Q.Cause;O.Remedy=Q.Remedy;O.Bound=Q.EstimatedQuantity.GetRawValue();O.Price=Q.ReviewedUnitPriceMilliMarks;O.ReportTick=Q.ReviewedMarketUpdateTick;
    O.ReportPrice=Known&&Known->PriceMilliMarks?Known->PriceMilliMarks.GetValue():0;O.ReportAge=Known&&Known->ReportAgeTicks?Known->ReportAgeTicks.GetValue():-1;O.ReportStock=Supply&&Supply->Stock?Supply->Stock->GetRawValue():-1;
    O.Money=House?House->Money.GetRawValue():0;O.FreeCapacity=Inventory?Inventory->FreeCapacity.GetRawValue():0;
    if(Inventory)for(const auto& Stock:Inventory->Stocks){if(Stock.GoodId==M.GoodId)O.Cargo=Stock.Available.GetRawValue();O.Manifest+=FString::Printf(TEXT("%s: %.3f cargo (%.3f reserved)\n"),*Stock.GoodId.ToString().RightChop(5),double(Stock.Stock.GetRawValue())/1000.,double(Stock.Reserved.GetRawValue())/1000.);}
    if(O.Manifest.IsEmpty())O.Manifest=TEXT("Cargo hold empty.");
    const auto& Receipt=V.LastSpotTrade;
    if(Receipt.CommandId.IsValid()&&Receipt.CityId==M.CityId&&Receipt.GoodId==M.GoodId){
     O.ReceiptId=int64(Receipt.CommandId.GetValue());
     const TCHAR* Outcome=Receipt.Outcome==EHansaSpotTradeOutcome::Completed?TEXT("Completed"):Receipt.Outcome==EHansaSpotTradeOutcome::Partial?TEXT("Partial"):TEXT("Missed");
     const TCHAR* Reason=Receipt.Blocker==EHansaSpotTradeBlocker::InsufficientFunds?TEXT("insufficient funds"):Receipt.Blocker==EHansaSpotTradeBlocker::InsufficientMarketStock?TEXT("market stock limited"):Receipt.Blocker==EHansaSpotTradeBlocker::InsufficientShipCapacity?TEXT("ship capacity limited"):Receipt.Blocker==EHansaSpotTradeBlocker::InsufficientShipStock?TEXT("ship stock limited"):Receipt.Blocker==EHansaSpotTradeBlocker::InsufficientMarketCapacity?TEXT("market capacity limited"):TEXT("filled as requested");
     O.Receipt=FString::Printf(TEXT("Executed receipt %lld · tick %lld · %s\n%s %.3f of %.3f cargo · unit %lld milli-marks · cash %+lld pfennig\n%s. Current ship stock %.3f cargo; latest reported city stock %.3f cargo."),O.ReceiptId,Receipt.Tick.GetValue(),Outcome,Receipt.Side==EHansaSpotTradeSide::BuyFromCity?TEXT("Bought"):TEXT("Sold"),double(Receipt.AppliedQuantity.GetRawValue())/1000.,double(Receipt.RequestedQuantity.GetRawValue())/1000.,Receipt.UnitPriceMilliMarks,Receipt.SettledMoneyRaw,Reason,double(O.Cargo)/1000.,double(O.ReportStock)/1000.);
    }
    Offers.Add(MoveTemp(O));
   }
  }
 }
 return Offers;
}

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
	const auto Projection = BuildProjection();
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Hansa_BuildProjection);
	if (!IsReady()) return THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
	const auto ReadOnly = Runtime->State.CreateReadOnlyAccess(Runtime->Definitions);
	if (Runtime->CachedProjection.IsSet() && Runtime->CachedProjection->GetFingerprint() == ReadOnly.GetFingerprint())
		return THansaValueResult<FHansaSimulationProjection>::Success(Runtime->CachedProjection.GetValue());
	auto Projection = ReadOnly.BuildProjection();
	if (Projection) Runtime->CachedProjection = Projection.Value;
	return Projection;
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
	const auto Projection = BuildProjection();
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Hansa_PublishStateChange);
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
	const auto Projection = BuildProjection();
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
	Runtime->LandSurveys.Reset();
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

TOptional<FHansaKnownMarketPriceProjection> UHansaRuntimeSimulationHost::QueryKnownMarketPrice(FHansaCityDefinitionId City, FHansaGoodId Good, FHansaHouseId Viewer) const
{
    return Runtime.IsValid() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryKnownMarketPrice(City,Good,Viewer.IsValid()?Viewer:Runtime->HouseId) : TOptional<FHansaKnownMarketPriceProjection>();
}
TOptional<FHansaKnownMarketSupplyDemandProjection> UHansaRuntimeSimulationHost::QueryKnownMarketSupply(FHansaCityDefinitionId City, FHansaGoodId Good, FHansaHouseId Viewer) const
{
    return Runtime.IsValid() ? Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).QueryKnownMarketSupplyDemand(City,Good,Viewer.IsValid()?Viewer:Runtime->HouseId) : TOptional<FHansaKnownMarketSupplyDemandProjection>();
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

TArray<FHansaTradeRecovery> UHansaRuntimeSimulationHost::BuildTradeRecovery(FHansaHouseId Viewer) const
{
 TArray<FHansaTradeRecovery> Out;if(!IsReady()||!Viewer.IsValid())return Out;
 const auto Projection=BuildProjection();if(!Projection)return Out;const auto& P=Projection.Value;
 const auto Reservations=Runtime->State.CreateReadOnlyAccess(Runtime->Definitions).GetInventories().CaptureSnapshot();
 for(const auto& T:P.GetTradeStations()){
  const auto& S=T.Station;if(S.OwnerId!=Viewer)continue;
  FHansaTradeRecovery V;V.City=FName(*S.CityId.ToString());V.Station=S.Id.GetValue();
  const auto Add=[&](FString Id,FString Label,FString Detail,FString Target=TEXT("Ledger"),int64 Entity=0){V.Items.Add({MoveTemp(Id),MoveTemp(Label),MoveTemp(Detail),MoveTemp(Target),Entity});};
  const auto* Inventory=P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==S.InventoryId;});
  const auto* Policy=GetEconomicRegistry()->FindCityTradePolicyForCity(S.CityId.ToString());
  const auto* Site=Policy?Policy->TradeStationSites.FindByPredicate([&](const auto& X){return X.SiteId==S.SiteId;}):nullptr;
  const bool Invalid=!Inventory||!Site||!T.RecoveryDiagnostic.IsEmpty()||!T.Lease.Id.IsValid();
  V.bFinalizing=S.Status==EHansaTradeStationStatus::Closed;
  const bool Retained=T.Lease.bOccupied;
  V.Status=Invalid?TEXT("Missing or incompatible definition"):V.bFinalizing?(Retained?TEXT("Closing · outbound-only recovery"):TEXT("Closed · plot released")):S.OperationalState==EHansaTradeStationOperationalState::Underfunded?TEXT("Underfunded"):S.OperationalState==EHansaTradeStationOperationalState::Revoked?TEXT("Revoked · outbound-only recovery"):S.Status==EHansaTradeStationStatus::Suspended?TEXT("Suspended · outbound recovery"):S.Status==EHansaTradeStationStatus::UnderConstruction?TEXT("Under construction"):S.Status==EHansaTradeStationStatus::Proposed?(S.FundingInventoryId.IsValid()?TEXT("Awaiting pickup"):TEXT("Awaiting funding")):S.OperationalState==EHansaTradeStationOperationalState::StorageBlocked?TEXT("Storage blocked"):S.OperationalState==EHansaTradeStationOperationalState::OrderSuspended?TEXT("Orders suspended"):TEXT("Active");
  V.bAttention=Invalid||S.Status==EHansaTradeStationStatus::Suspended||(V.bFinalizing&&Retained);
  V.bCanClose=!Invalid&&(!V.bFinalizing||(Retained&&Inventory->UsedCapacity.GetRawValue()==0&&Inventory->Reserved.GetRawValue()==0&&T.Lease.OccupyingBuildingIds.IsEmpty()));
  V.Cause=Invalid?TEXT("Preserve the save and restore compatible city/site/inventory definitions. Recovery commands are unavailable; no missing stock is assumed to be zero."):T.Blocker+TEXT(". ")+T.NextStep;
  if(V.bFinalizing&&!V.bCanClose)V.Cause+=Retained?TEXT(" Finalization blocked: clear station stock, hard reservations and occupying buildings first."):TEXT(" Closure is already finalized; station history and identity remain.");
  V.Terms=TEXT("PRESERVED: station, factor and inventory identities, physical stock, hard reservations, route identities and ship cargo.\nPAUSED: live orders and stopped routes using station transfers. Traveling routes retain their journey and cargo. Orders are paused, not cancelled.\nSTRANDED: inbound station delivery is unavailable after closure. Edit routes to load stock outbound here and unload at an authorized destination. Source reserves still apply.\nTRANSFERRED: no cargo is teleported or discarded. Recover through normal route operations.\nOWED: closure clears current station upkeep arrears and stops station upkeep under the current rules; vehicle travel costs continue.\nLEASE: retained while stock, reservations or buildings remain. Finalize after clearing all blockers; identity/history remain. Closed stations have no reopen command; establish a new station after releasing the plot.");
  if((S.Status==EHansaTradeStationStatus::UnderConstruction||(S.Status==EHansaTradeStationStatus::Proposed&&S.FundingInventoryId.IsValid()))&&Site)V.Terms+=FString::Printf(TEXT("\nCANCELLED CONSTRUCTION: %d basis-point refund; money returns to your treasury and materials to recovery inventory #%llu. If refund cannot fit, closure rejects atomically."),Site->CancellationRefundBasisPoints,(S.ConstructionSite.bLocalDelivery?S.InventoryId:S.FundingInventoryId).GetValue());
  if(S.Status==EHansaTradeStationStatus::UnderConstruction||(S.Status==EHansaTradeStationStatus::Proposed&&S.FundingInventoryId.IsValid())){
   FString Cost=S.Status==EHansaTradeStationStatus::Proposed?TEXT("Awaiting pickup. Construction has not started; cancelling stops future pickup. "):FString();Cost+=FString::Printf(TEXT("Construction schedule tick %lld. Paid %lld pfennig. Closure refund goes to recovery inventory #%llu."),S.CompletionTick.GetValue(),S.SpentMoneyRaw,(S.ConstructionSite.bLocalDelivery?S.InventoryId:S.FundingInventoryId).GetValue());
   if(Site)Cost+=FString::Printf(TEXT(" Money refunded: %lld pfennig."),S.SpentMoneyRaw*Site->CancellationRefundBasisPoints/10000);
   for(const auto& G:S.SpentGoods)Cost+=FString::Printf(TEXT("\n%s paid %lld milli-units; refund %lld."),*G.GoodId.ToString(),G.Quantity.GetRawValue(),Site?G.Quantity.GetRawValue()*Site->CancellationRefundBasisPoints/10000:0);
   Add(TEXT("Construction"),TEXT("Station construction · funded"),Cost,TEXT("Presence"));V.Terms+=TEXT("\n")+Cost;
  }
  if(const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==S.OwnerId&&X.CityId==S.CityId;});Presence&&Presence->Upgrade.ConstructionSite.bLocalDelivery&&Site){
   V.Terms+=FString::Printf(TEXT("\nPENDING UPGRADE: closure cancels the upgrade. Refund %d%% of its %lld pfennig payment to the treasury and %d%% of delivered upgrade goods to station storage. Clear refunded stock before releasing the lease."),Site->CancellationRefundBasisPoints/100,Presence->Upgrade.SpentMoneyPfennig,Site->CancellationRefundBasisPoints/100);
  }
  Add(TEXT("Station"),FString::Printf(TEXT("Station #%llu · %s"),S.Id.GetValue(),*V.Status),V.Cause,TEXT("Presence"));
  Add(TEXT("Arrears"),FString::Printf(TEXT("Arrears · %lld pfennig"),S.OutstandingUpkeepPfennig),FString::Printf(TEXT("Upkeep %lld pfennig/tick. Fund/reopen is available only for underfunding; suspended or revoked rights require their stated conditions/new grant. Review exact funding in Presence."),S.UpkeepPfennigPerTick),TEXT("Presence"));
  Add(TEXT("Lease"),FString::Printf(TEXT("Lease #%llu · %s"),T.Lease.Id.GetValue(),Retained?TEXT("retained"):TEXT("released")),TEXT("Owned lease and building identities are preserved. Inspect expansion for occupying buildings. Inactive rights do not permit new construction."),TEXT("Construction"));
  for(auto Id:T.Lease.OccupyingBuildingIds)Add(FString::Printf(TEXT("Building.%llu"),Id.GetValue()),FString::Printf(TEXT("Building #%llu · occupies lease"),Id.GetValue()),TEXT("This building blocks finalization. Inspect it through Expansion; only normal authorized removal/cancellation may release its footprint."),TEXT("Construction"),Id.GetValue());
  TSet<uint64> DependentBuildings;for(auto Id:T.Lease.OccupyingBuildingIds)DependentBuildings.Add(Id.GetValue());
  for(const auto& Lease:P.GetLeasedPlots())if(Lease.StationId==S.Id&&Lease.OwnerId==Viewer&&Lease.Id!=T.Lease.Id){
   const FString Detail=FString::Printf(TEXT("Additional station lease #%llu · %s. Closure preserves this lease and its buildings; it does not release this additional plot. Construction permissions require active station/presence rights. Inspect Expansion for the authoritative blocker."),Lease.Id.GetValue(),Lease.bOccupied?TEXT("occupied"):TEXT("unoccupied"));
   Add(FString::Printf(TEXT("Lease.%llu"),Lease.Id.GetValue()),FString::Printf(TEXT("Additional lease #%llu"),Lease.Id.GetValue()),Detail,TEXT("Construction"));V.Terms+=TEXT("\n")+Detail;
   for(auto Id:Lease.OccupyingBuildingIds){DependentBuildings.Add(Id.GetValue());Add(FString::Printf(TEXT("Building.%llu"),Id.GetValue()),FString::Printf(TEXT("Building #%llu · additional lease"),Id.GetValue()),FString::Printf(TEXT("Preserved on additional lease #%llu. This plot is not released by station finalization. Inspect Expansion for current rights and building state."),Lease.Id.GetValue()),TEXT("Construction"),Id.GetValue());}
  }
  if(Inventory)for(const auto& G:Inventory->Stocks)Add(TEXT("Stock.")+G.GoodId.ToString(),FString::Printf(TEXT("%s · %.3f cargo"),*(GetEconomicRegistry()->FindGood(G.GoodId.ToString())?GetEconomicRegistry()->FindGood(G.GoodId.ToString())->DisplayName:G.GoodId.ToString()+TEXT(" (definition unavailable)")),G.Stock.GetRawValue()/1000.),FString::Printf(TEXT("Inventory #%llu: physical %lld, hard reserved %lld, available %lld milli-units. Recover available stock with LoadStation and an authorized unload destination; reserved stock cannot be taken."),Inventory->Id.GetValue(),G.Stock.GetRawValue(),G.Reserved.GetRawValue(),G.Available.GetRawValue()));
  for(const auto& O:S.Orders)Add(FString::Printf(TEXT("Order.%llu"),O.Id),FString::Printf(TEXT("Order #%llu · %s"),O.Id,O.bCancelled?TEXT("cancelled"):O.bPaused?TEXT("paused"):TEXT("running")),FString::Printf(TEXT("%s · spent %lld / budget %lld pfennig. Target/reserve %lld milli-units. Pause or cancel through Orders. Pausing a release order preserves its sale reserve; cancelling removes that policy floor, not physical stock."),*O.Terms.GoodId.ToString(),O.SpentPfennig,O.Terms.TotalBudgetPfennig,O.Terms.TargetOrReserveMilliUnits),TEXT("Orders"),O.Id);
  for(auto& Item:V.Items)if(Item.Target==TEXT("Construction")&&Item.Entity){const auto* B=P.GetBuildingWorldProjections().FindByPredicate([&](const auto& X){return X.BuildingId.GetValue()==uint64(Item.Entity)&&X.OwnerId==Viewer;});Item.Detail+=B?FString::Printf(TEXT("\nAuthoritative building state: %s."),LexToString(B->Status)):TEXT("\nBuilding projection unavailable. Preserve its identity and resolve compatible content before removal.");}
  TSet<uint64> Inventories;Inventories.Add(S.InventoryId.GetValue());
  for(const auto& I:P.GetInventories())if(DependentBuildings.Contains(I.BuildingId.GetValue()))Inventories.Add(I.Id.GetValue());
  for(const auto& R:P.GetRoutes())if(R.OwnerId==Viewer&&R.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==S.CityId&&Stop.Actions.ContainsByPredicate([](const auto& A){return IsStationTransfer(A.Kind);});})){
   Add(FString::Printf(TEXT("Route.%llu"),R.Id.GetValue()),FString::Printf(TEXT("Route #%llu · %s"),R.Id.GetValue(),LexToString(R.Lifecycle)),TEXT("Inspect/edit this route to load station stock outbound and unload at a permitted destination. Pause/cancel/edit is constrained by berth and lifecycle; cargo and vehicle identity remain. Traveling ships must arrive before stop edits."),TEXT("Route"),R.Id.GetValue());
   auto& Item=V.Items.Last();Item.Vehicle=R.VehicleId.GetValue();Item.Lifecycle=LexToString(R.Lifecycle);Item.bCanToggle=R.Lifecycle==EHansaRouteLifecycleState::Inactive||R.Lifecycle==EHansaRouteLifecycleState::AtStop;
   for(const auto& Stop:R.Stops){FHansaRecoveryStop D;D.City=Stop.CityId.ToString();for(const auto& A:Stop.Actions)D.Actions.Add({A.GoodId.ToString(),uint8(A.Kind),A.QuantityLimit.GetRawValue(),A.MinimumSourceReserve.GetRawValue()});Item.Stops.Add(D);}
   if(const auto* Ship=P.GetVehicles().FindByPredicate([&](const auto& X){return X.Id==R.VehicleId&&X.OwnerId==Viewer;})){Inventories.Add(Ship->CargoInventoryId.GetValue());Item.bCanCancel=Item.bCanToggle&&Ship->Cargo.GetRawValue()==0;}
  }
  for(const auto& Ship:P.GetVehicles())if(Ship.OwnerId==Viewer&&Ship.CurrentCityId==S.CityId)Inventories.Add(Ship.CargoInventoryId.GetValue());
  for(const auto& I:P.GetInventories())if(I.OwnerKind==EHansaInventoryOwnerKind::Vehicle&&Inventories.Contains(I.Id.GetValue())){
   FString Cargo=TEXT("Owned ship cargo is preserved. Use an authorized route unload or quay sale; no automatic transfer.\n");for(const auto& G:I.Stocks)Cargo+=FString::Printf(TEXT("%s: %lld physical / %lld reserved milli-units\n"),*G.GoodId.ToString(),G.Stock.GetRawValue(),G.Reserved.GetRawValue());
   Add(FString::Printf(TEXT("Ship.%llu"),I.VehicleId.GetValue()),FString::Printf(TEXT("Ship #%llu · %.3f cargo"),I.VehicleId.GetValue(),I.UsedCapacity.GetRawValue()/1000.),Cargo,TEXT("Ship"),I.VehicleId.GetValue());
  }
  for(const auto& R:Reservations.GetReservations())if(Inventories.Contains(R.InventoryId.GetValue()))Add(FString::Printf(TEXT("Reservation.%llu"),R.Id.GetValue()),FString::Printf(TEXT("Reservation #%llu"),R.Id.GetValue()),FString::Printf(TEXT("Inventory #%llu · %s · %lld milli-units. Preserved hard reservation; not free stock. Resolve its owning production/logistics operation before removal; recovery cannot force-release it."),R.InventoryId.GetValue(),*R.GoodId.ToString(),R.Quantity.GetRawValue()));
  Add(TEXT("Pending"),TEXT("Pending authoritative commands · none"),TEXT("Commands execute synchronously at tick boundaries; the server retains no pending queue. A client submission remains explicitly pending until its correlated acknowledgement."),TEXT("Overview"));
  FString Key=FString::Printf(TEXT("%llu|%lld|%d|%s|%s"),Viewer.GetValue(),V.Station,V.bCanClose,*V.Status,*V.Terms);for(const auto& I:V.Items){Key+=TEXT("|")+I.Id+I.Label+I.Detail;for(const auto& Stop:I.Stops){Key+=Stop.City;for(const auto& A:Stop.Actions)Key+=FString::Printf(TEXT("|%s:%d:%lld:%lld"),*A.Good,A.Kind,A.Quantity,A.Reserve);}}
  // Stable dependency signature deliberately excludes tick/age: unrelated time cannot invalidate a review.
  const FTCHARToUTF8 Utf8(*Key);FSHAHash Digest;FSHA1::HashBuffer(Utf8.Get(),Utf8.Length(),Digest.Hash);V.ReviewKey=Digest.ToString();Out.Add(MoveTemp(V));
 }
 return Out;
}
