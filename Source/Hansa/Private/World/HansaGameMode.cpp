#include "World/HansaGameMode.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaAmbientRabbits.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "HansaLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UI/HansaRootHud.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameState.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaPlayerState.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

AHansaGameMode::AHansaGameMode()
{
	DefaultPawnClass = AHansaStrategyCameraPawn::StaticClass();
	PlayerControllerClass = AHansaStrategyPlayerController::StaticClass();
	PlayerStateClass = AHansaPlayerState::StaticClass();
	GameStateClass = AHansaGameState::StaticClass();
	HUDClass = AHansaRootHud::StaticClass();
	bStartPlayersAsSpectators = false;
	PrimaryActorTick.bCanEverTick = true;
}

AHansaGameMode::~AHansaGameMode() = default;

void AHansaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString ScenarioOption = UGameplayStatics::ParseOption(Options, TEXT("Scenario"));
	RuntimeScenario = ScenarioOption.Equals(
		FHansaLubeckScenarioInitializer::EmptyBuildId, ESearchCase::IgnoreCase)
		? EHansaRuntimeScenario::EmptyLubeckBuild
		: EHansaRuntimeScenario::LubeckGrainShortage;
	const FString CampaignSeedOption = UGameplayStatics::ParseOption(Options, TEXT("CampaignSeed"));
	if (!CampaignSeedOption.IsEmpty())
	{
		TCHAR* End = nullptr;
		RuntimeCampaignSeedOverride = FCString::Strtoui64(*CampaignSeedOption, &End, 10);
		if (RuntimeCampaignSeedOverride == 0 || End == nullptr || *End != TEXT('\0'))
		{
			ErrorMessage = TEXT("CampaignSeed must be a positive unsigned integer.");
			return;
		}
	}
	bAuthorityFixtureMode = FParse::Param(FCommandLine::Get(), TEXT("HansaAuthorityFixture"));
	EnsureLubeckWorldComposition();
	GetSimulationHost();
	EnsureMultiplayerAuthority();
}

void AHansaGameMode::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (UHansaRuntimeSimulationHost* Host = GetSimulationHost())
	{
		const int64 Before = Host->GetSimulationTick();
		Host->AdvanceRealTime(DeltaSeconds);
        for (TActorIterator<AHansaCargoProjectionManager> It(GetWorld()); It; ++It) It->Sample(Host->GetPresentationTickFraction());
        for (TActorIterator<AHansaBuildingWorldProjectionActor> It(GetWorld()); It; ++It) It->SampleProduction(Host->GetPresentationTickFraction());
		if (Host->GetSimulationTick() != Before) RefreshMultiplayerProjections(false);
	}
}

void AHansaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(NewPlayer);
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Controller == nullptr || Host == nullptr || !EnsureMultiplayerAuthority()) return;

	FHansaClientCommandFeedback Failure;
	if (GetNetMode() != NM_Standalone && !bAuthorityFixtureMode)
	{
		Failure.Rejection = EHansaClientCommandRejection::ClientNotRegistered;
		Failure.Message = TEXT("The server requires validated multiplayer admission.");
		Failure.Remedy = TEXT("Join through the authenticated or server-credential session flow.");
		Controller->PublishCommandFeedback(Failure);
		return;
	}
	if (MultiplayerAuthority->GetRegisteredClientCount() >= 8)
	{
		Failure.Rejection = EHansaClientCommandRejection::NotAuthorized;
		Failure.Message = TEXT("The scenario already has eight human-controlled houses.");
		Failure.Remedy = TEXT("Disconnect an existing proof client before joining.");
		Controller->PublishCommandFeedback(Failure);
		return;
	}

	const uint64 PrincipalId = NextPrincipalId++;
	Hansa::Simulation::FHansaHouseId HouseId;
	for (const Hansa::Simulation::FHansaHouseId Candidate : Host->GetHouseIds())
	{
		if (!MultiplayerAuthority->IsHouseRegistered(Candidate))
		{
			HouseId = Candidate;
			break;
		}
	}
	if (!HouseId.IsValid())
	{
		Failure.Rejection = EHansaClientCommandRejection::NotAuthorized;
		Failure.Message = TEXT("No unclaimed scenario house is available.");
		Failure.Remedy = TEXT("Refresh the roster or disconnect an existing client.");
		Controller->PublishCommandFeedback(Failure);
		return;
	}
	FHansaClientInterest Interest;
	Interest.CityIds.Add(Host->GetCityId().ToString());
	FString Error;
	const auto ParticipantId = Hansa::Simulation::FHansaParticipantId::TryCreate(PrincipalId + 1000).Value;
	const Hansa::Simulation::FHansaAdmissionGrant Admission {
		PrincipalId, ParticipantId, HouseId,
		bAuthorityFixtureMode ? Hansa::Simulation::EHansaAdmissionMode::LanOffline : Hansa::Simulation::EHansaAdmissionMode::OnlineAuthenticated };
	if (!MultiplayerAuthority->RegisterAdmittedClient(Admission, Interest, Error))
	{
		Failure.Rejection = EHansaClientCommandRejection::AuthorityUnavailable;
		Failure.Message = Error;
		Failure.Remedy = TEXT("Restart the authoritative scenario session.");
		Controller->PublishCommandFeedback(Failure);
		return;
	}

	ClientPrincipals.Add(Controller, PrincipalId);
	UE_LOG(LogHansa, Display,
		TEXT("MP-04 authority client joined principal=%llu house=%llu clients=%d"),
		static_cast<unsigned long long>(PrincipalId),
		static_cast<unsigned long long>(HouseId.GetValue()),
		MultiplayerAuthority->GetRegisteredClientCount());
	Controller->SetServerAuthorityIdentity(PrincipalId, static_cast<int64>(HouseId.GetValue()));
	if (AHansaPlayerState* State = Controller->GetPlayerState<AHansaPlayerState>())
	{
		State->SetAuthorityIdentity(static_cast<int64>(HouseId.GetValue()));
	}
	RefreshMultiplayerProjections(true);
}

void AHansaGameMode::Logout(AController* Exiting)
{
	if (AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(Exiting))
	{
		const uint64 PrincipalId = FindPrincipal(*Controller);
		if (PrincipalId != 0 && MultiplayerAuthority)
		{
			MultiplayerAuthority->UnregisterClient(PrincipalId);
			UE_LOG(LogHansa, Display,
				TEXT("S11-P04 authority client left principal=%llu clients=%d"),
				static_cast<unsigned long long>(PrincipalId),
				MultiplayerAuthority->GetRegisteredClientCount());
		}
		ClientPrincipals.Remove(Controller);
	}
	Super::Logout(Exiting);
}

uint64 AHansaGameMode::FindPrincipal(const AHansaStrategyPlayerController& Controller) const
{
	const uint64* Found = ClientPrincipals.Find(&Controller);
	return Found != nullptr ? *Found : 0;
}

bool AHansaGameMode::EnsureMultiplayerAuthority()
{
	UHansaRuntimeSimulationHost* Host = GetSimulationHost();
	if (Host == nullptr) return false;
	if (!MultiplayerAuthority)
	{
		MultiplayerAuthority = MakeUnique<Hansa::Multiplayer::FHansaMultiplayerAuthority>();
		return MultiplayerAuthority->Initialize(*Host);
	}
	return true;
}

void AHansaGameMode::SubmitMultiplayerIntent(
	AHansaStrategyPlayerController& Controller, const FHansaClientCommandIntent& Intent)
{
	const uint64 PrincipalId = FindPrincipal(Controller);
	FHansaClientCommandFeedback Feedback;
	if (PrincipalId == 0 || !EnsureMultiplayerAuthority())
	{
		Feedback.ClientSequence = Intent.ClientSequence;
		Feedback.ClientNonce = Intent.ClientNonce;
		Feedback.Rejection = EHansaClientCommandRejection::ClientNotRegistered;
		Feedback.Message = TEXT("The server has not assigned this connection a house.");
		Feedback.Remedy = TEXT("Wait for authority readiness or rejoin the session.");
	}
	else
	{
		Feedback = MultiplayerAuthority->SubmitIntent(PrincipalId, Intent);
	}
	Controller.PublishCommandFeedback(Feedback);
	if (Feedback.bAccepted) RefreshMultiplayerProjections(false);
}

void AHansaGameMode::UpdateMultiplayerInterest(
	AHansaStrategyPlayerController& Controller, const FHansaClientInterest& Interest)
{
	const uint64 PrincipalId = FindPrincipal(Controller);
	FString Error;
	if (PrincipalId == 0 || !EnsureMultiplayerAuthority() ||
		!MultiplayerAuthority->SetClientInterest(PrincipalId, Interest, Error))
	{
		FHansaClientCommandFeedback Feedback;
		Feedback.Rejection = PrincipalId == 0
			? EHansaClientCommandRejection::ClientNotRegistered
			: EHansaClientCommandRejection::InvalidPayload;
		Feedback.Message = Error.IsEmpty() ? TEXT("The connection is not registered.") : Error;
		Feedback.Remedy = TEXT("Request up to four unique City.* stable IDs.");
		Controller.PublishCommandFeedback(Feedback);
		return;
	}
	RefreshMultiplayerProjections(true);
}

void AHansaGameMode::RequestMultiplayerProjectionRefresh(
	AHansaStrategyPlayerController& Controller, const int64 ClientKnownRevision)
{
	const uint64 PrincipalId = FindPrincipal(Controller);
	if (PrincipalId == 0 || !EnsureMultiplayerAuthority()) return;
	FHansaClientProjectionSnapshot Projection;
	FString Error;
	if (MultiplayerAuthority->BuildProjection(
		PrincipalId, ClientKnownRevision, false, Projection, Error))
	{
		Controller.PublishServerProjection(Projection);
	}
	else
	{
		UE_LOG(LogHansa, Warning, TEXT("Unable to refresh client projection: %s"), *Error);
	}
}

int32 AHansaGameMode::GetRegisteredAuthorityClientCount() const
{
	return MultiplayerAuthority ? MultiplayerAuthority->GetRegisteredClientCount() : 0;
}

void AHansaGameMode::RefreshMultiplayerProjections(const bool bForceFullRefresh)
{
	if (!EnsureMultiplayerAuthority()) return;
	bool bPublishedGlobal = false;
	for (auto It = ClientPrincipals.CreateIterator(); It; ++It)
	{
		AHansaStrategyPlayerController* Controller = It.Key().Get();
		if (Controller == nullptr)
		{
			MultiplayerAuthority->UnregisterClient(It.Value());
			It.RemoveCurrent();
			continue;
		}
		FHansaClientProjectionSnapshot Projection;
		FString Error;
		if (!MultiplayerAuthority->BuildProjection(It.Value(),
			Controller->GetClientProjection().Revision, bForceFullRefresh, Projection, Error))
		{
			UE_LOG(LogHansa, Warning, TEXT("Unable to build client projection: %s"), *Error);
			continue;
		}
		Controller->PublishServerProjection(Projection);
		if (!bPublishedGlobal)
		{
			if (AHansaGameState* State = GetGameState<AHansaGameState>())
			{
				State->PublishAuthoritativeProjection(Projection);
				bPublishedGlobal = true;
			}
		}
	}
}

UHansaRuntimeSimulationHost* AHansaGameMode::GetSimulationHost()
{
	if (SimulationHost == nullptr && !bSimulationHostInitializationAttempted)
	{
		bSimulationHostInitializationAttempted = true;
		SimulationHost = NewObject<UHansaRuntimeSimulationHost>(this, TEXT("LubeckRuntimeSimulation"));
		FString Error;
		if (!SimulationHost->InitializeForLubeck(
			GetWorld(), Error, RuntimeScenario, RuntimeCampaignSeedOverride))
		{
			UE_LOG(LogHansa, Error, TEXT("Hansa runtime simulation initialization failed: %s"), *Error);
			SimulationHost = nullptr;
		}
		else if (bAuthorityFixtureMode)
		{
			SimulationHost->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
			UE_LOG(LogHansa, Display,
				TEXT("S11-P04 authority fixture initialized seed=%llu and paused explicit stepping."),
				static_cast<unsigned long long>(SimulationHost->GetCampaignSeed()));
		}
	}
	return SimulationHost;
}

AActor* AHansaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHansaLubeckAutomationStart> It(World); It; ++It)
		{
			return *It;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AHansaGameMode::EnsureLubeckWorldComposition()
{
	UWorld* World = GetWorld();
	if (World == nullptr) return;

	AHansaLubeckWorldFoundation* Foundation = nullptr;
	for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It)
	{
		Foundation = *It;
		break;
	}
	if (Foundation == nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = TEXT("LubeckWorldFoundation");
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>(
			AHansaLubeckWorldFoundation::StaticClass(), FTransform::Identity, Parameters);
	}

	bool bHasProjectionManager = false;
	for (TActorIterator<AHansaPlacementProjectionManager> It(World); It; ++It)
	{
		bHasProjectionManager = true;
		break;
	}
	if (!bHasProjectionManager)
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = TEXT("LubeckPlacementProjectionManager");
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AHansaPlacementProjectionManager>(
			AHansaPlacementProjectionManager::StaticClass(), FTransform::Identity, Parameters);
	}

    bool bHasCargoManager = false;
    for (TActorIterator<AHansaCargoProjectionManager> It(World); It; ++It) {bHasCargoManager=true; break;}
    if (!bHasCargoManager) World->SpawnActor<AHansaCargoProjectionManager>();

    bool bHasRabbits = false;
    for (TActorIterator<AHansaAmbientRabbits> It(World); It; ++It) { bHasRabbits = true; break; }
    if (!bHasRabbits && World->GetNetMode() == NM_Standalone) World->SpawnActor<AHansaAmbientRabbits>();

	for (TActorIterator<AHansaLubeckAutomationStart> It(World); It; ++It)
	{
		// The saved map may still contain the inland prototype PlayerStart.
		if (Foundation && Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(World))
			It->SetActorTransform(Foundation->GetAutomationStartTransform());
		return;
	}

	FActorSpawnParameters Parameters;
	Parameters.Name = TEXT("LubeckAutomationStart");
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Transform = Foundation != nullptr ? Foundation->GetAutomationStartTransform() :
		Hansa::Game::LubeckMap::AutomationStartTransform();
	World->SpawnActor<AHansaLubeckAutomationStart>(
		AHansaLubeckAutomationStart::StaticClass(), Transform, Parameters);
}
