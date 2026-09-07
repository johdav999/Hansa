#include "Multiplayer/HansaMultiplayerAutomationService.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHansaAutomation, Log, All);
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Network/HansaMultiplayerTypes.h"
#include "World/HansaGameMode.h"
#include "World/HansaGameState.h"
#include "World/HansaPlayerState.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyPlayerController.h"

namespace Hansa::Automation
{
	namespace
	{
		UWorld* FindGameWorld()
		{
			if (GEngine == nullptr) return nullptr;
			UWorld* Fallback = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World == nullptr || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
				{
					continue;
				}
				if (World->GetNetMode() == NM_Client || World->GetAuthGameMode<AHansaGameMode>() != nullptr)
				{
					return World;
				}
				Fallback = World;
			}
			return Fallback;
		}

		AHansaStrategyPlayerController* FindLocalController(UWorld& World)
		{
			for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It)
			{
				AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(It->Get());
				if (Controller != nullptr && Controller->IsLocalController()) return Controller;
			}
			return nullptr;
		}

		const TCHAR* NetModeName(const ENetMode Mode)
		{
			switch (Mode)
			{
			case NM_Standalone: return TEXT("Standalone");
			case NM_DedicatedServer: return TEXT("DedicatedServer");
			case NM_ListenServer: return TEXT("ListenServer");
			case NM_Client: return TEXT("Client");
			default: return TEXT("Unknown");
			}
		}

		FString RejectionName(const EHansaClientCommandRejection Rejection)
		{
			if (const UEnum* Enum = StaticEnum<EHansaClientCommandRejection>())
			{
				return Enum->GetNameStringByValue(static_cast<int64>(Rejection));
			}
			return TEXT("Unknown");
		}

		bool TryIntegral(const TSharedRef<FJsonObject>& Object, const TCHAR* Name, int64& OutValue)
		{
			double Number = 0.0;
			if (!Object->TryGetNumberField(Name, Number) || !FMath::IsFinite(Number) ||
				!FMath::IsNearlyEqual(Number, FMath::RoundToDouble(Number)))
			{
				return false;
			}
			OutValue = static_cast<int64>(Number);
			return true;
		}

		TSharedRef<FJsonObject> MakeFeedback(const FHansaClientCommandFeedback& Feedback)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetBoolField(TEXT("accepted"), Feedback.bAccepted);
			Json->SetNumberField(TEXT("clientSequence"), static_cast<double>(Feedback.ClientSequence));
			Json->SetNumberField(TEXT("clientNonce"), static_cast<double>(Feedback.ClientNonce));
			Json->SetStringField(TEXT("rejection"), RejectionName(Feedback.Rejection));
			Json->SetStringField(TEXT("gatewayError"), Feedback.GatewayError);
			Json->SetStringField(TEXT("message"), Feedback.Message);
			Json->SetStringField(TEXT("remedy"), Feedback.Remedy);
			Json->SetNumberField(TEXT("serverTick"), static_cast<double>(Feedback.ServerTick));
			Json->SetNumberField(TEXT("acceptedGlobalSequence"),
				static_cast<double>(Feedback.AcceptedGlobalSequence));
			return Json;
		}

		void AddProjection(const FHansaClientProjectionSnapshot& Projection,
			const TSharedRef<FJsonObject>& Json)
		{
			Json->SetNumberField(TEXT("projectionSchemaVersion"), Projection.SchemaVersion);
			Json->SetNumberField(TEXT("projectionRevision"), static_cast<double>(Projection.Revision));
			Json->SetNumberField(TEXT("serverTick"), static_cast<double>(Projection.ServerTick));
			Json->SetBoolField(TEXT("fullRefresh"), Projection.bFullRefresh);
			Json->SetNumberField(TEXT("startingEventSequence"),
				static_cast<double>(Projection.StartingEventSequence));
			Json->SetNumberField(TEXT("lastEventSequence"),
				static_cast<double>(Projection.LastEventSequence));
			Json->SetStringField(TEXT("authoritativeHash"), Projection.AuthoritativeHash);
			Json->SetStringField(TEXT("projectionDigest"), Projection.ProjectionDigest);
			Json->SetNumberField(TEXT("ownerHouseId"), static_cast<double>(Projection.OwnerHouseId));
			Json->SetNumberField(TEXT("ownerMoneyPfennig"),
				static_cast<double>(Projection.OwnerMoneyPfennig));
			Json->SetStringField(TEXT("scenarioOutcome"), Projection.ScenarioOutcome);
			Json->SetStringField(TEXT("winningVictoryId"), Projection.WinningVictoryId);
			Json->SetNumberField(TEXT("placementCount"), Projection.Placements.Num());
			Json->SetNumberField(TEXT("marketCount"), Projection.Markets.Num());
			Json->SetNumberField(TEXT("routeCount"), Projection.Routes.Num());
			Json->SetNumberField(TEXT("victoryObjectiveCount"), Projection.VictoryObjectives.Num());
			Json->SetNumberField(TEXT("eventCount"), Projection.Events.Num());

			TArray<TSharedPtr<FJsonValue>> Placements;
			for (const FHansaReplicatedPlacement& Placement : Projection.Placements)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("buildingId"), static_cast<double>(Placement.BuildingId));
				Item->SetNumberField(TEXT("ownerHouseId"), static_cast<double>(Placement.OwnerHouseId));
				Item->SetStringField(TEXT("cityId"), Placement.CityId);
				Item->SetStringField(TEXT("buildingDefinitionId"), Placement.BuildingDefinitionId);
				Item->SetNumberField(TEXT("anchorX"), Placement.AnchorX);
				Item->SetNumberField(TEXT("anchorY"), Placement.AnchorY);
				Item->SetStringField(TEXT("status"), Placement.Status);
				Placements.Add(MakeShared<FJsonValueObject>(Item));
			}
			Json->SetArrayField(TEXT("placements"), MoveTemp(Placements));

			TArray<TSharedPtr<FJsonValue>> Routes;
			for (const FHansaReplicatedRoute& Route : Projection.Routes)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("routeId"), static_cast<double>(Route.RouteId));
				Item->SetNumberField(TEXT("ownerHouseId"), static_cast<double>(Route.OwnerHouseId));
				Item->SetStringField(TEXT("lifecycle"), Route.Lifecycle);
				Item->SetStringField(TEXT("currentCityId"), Route.CurrentCityId);
				Item->SetBoolField(TEXT("cargoVisible"), Route.bCargoVisible);
				Item->SetNumberField(TEXT("cargoMilliUnits"),
					static_cast<double>(Route.CargoMilliUnits));
				Routes.Add(MakeShared<FJsonValueObject>(Item));
			}
			Json->SetArrayField(TEXT("routes"), MoveTemp(Routes));
		}
	}

	void FHansaMultiplayerAutomationService::AppendFixtureDescriptor(
		TArray<TSharedPtr<FJsonValue>>& Fixtures) const
	{
		TSharedRef<FJsonObject> Fixture = MakeShared<FJsonObject>();
		Fixture->SetStringField(TEXT("fixtureId"), FixtureId);
		Fixture->SetNumberField(TEXT("fixtureVersion"), FixtureVersion);
		Fixture->SetStringField(TEXT("registryHash"), TEXT("runtime-compiled"));
		Fixture->SetStringField(TEXT("purpose"),
			TEXT("Two-owner server authority, private projection, rejection, and reconnect proof"));
		Fixtures.Add(MakeShared<FJsonValueObject>(Fixture));
	}

	bool FHansaMultiplayerAutomationService::ActivateFixture(
		const TSharedRef<FJsonObject> OutResult, FString& OutError)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("HansaAuthorityFixture")))
		{
			OutError = TEXT("two_player_authority_v1 requires -HansaAuthorityFixture on the launched process.");
			return false;
		}
		UWorld* World = FindGameWorld();
		if (World == nullptr)
		{
			OutError = TEXT("The multiplayer world is not ready.");
			return false;
		}
		bFixtureActive = true;
		OutResult->SetBoolField(TEXT("loaded"), true);
		OutResult->SetStringField(TEXT("fixtureId"), FixtureId);
		OutResult->SetNumberField(TEXT("fixtureVersion"), FixtureVersion);
		OutResult->SetStringField(TEXT("seed"),
			FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(FixtureSeed)));
		OutResult->SetStringField(TEXT("netMode"), NetModeName(World->GetNetMode()));
		OutResult->SetNumberField(TEXT("processId"), FPlatformProcess::GetCurrentProcessId());
		return true;
	}

	bool FHansaMultiplayerAutomationService::BuildStatus(
		const FString& CorrelationId, const TSharedRef<FJsonObject> OutResult, FString& OutError) const
	{
		UWorld* World = FindGameWorld();
		if (!bFixtureActive || World == nullptr)
		{
			OutError = TEXT("Load two_player_authority_v1 after the multiplayer world is ready.");
			return false;
		}

		OutResult->SetStringField(TEXT("fixtureId"), FixtureId);
		OutResult->SetStringField(TEXT("correlationId"), CorrelationId);
		OutResult->SetNumberField(TEXT("processId"), FPlatformProcess::GetCurrentProcessId());
		OutResult->SetStringField(TEXT("netMode"), NetModeName(World->GetNetMode()));

		const AHansaGameState* GameState = World->GetGameState<AHansaGameState>();
		if (GameState != nullptr)
		{
			OutResult->SetNumberField(TEXT("publicProjectionRevision"),
				static_cast<double>(GameState->PublicProjectionRevision));
			OutResult->SetNumberField(TEXT("publicServerTick"),
				static_cast<double>(GameState->ServerSimulationTick));
			OutResult->SetStringField(TEXT("publicAuthoritativeHash"),
				GameState->ServerAuthoritativeHash);
		}

		if (AHansaGameMode* Mode = World->GetAuthGameMode<AHansaGameMode>())
		{
			const int32 Count = Mode->GetRegisteredAuthorityClientCount();
			OutResult->SetBoolField(TEXT("ready"), Mode->IsAuthorityFixtureMode());
			OutResult->SetNumberField(TEXT("registeredClientCount"), Count);
			if (UHansaRuntimeSimulationHost* Host = Mode->GetSimulationHost())
			{
				OutResult->SetNumberField(TEXT("serverTick"),
					static_cast<double>(Host->GetSimulationTick()));
				OutResult->SetStringField(TEXT("campaignSeed"),
					FString::Printf(TEXT("%llu"),
						static_cast<unsigned long long>(Host->GetCampaignSeed())));
			}
			return true;
		}

		AHansaStrategyPlayerController* Controller = FindLocalController(*World);
		const AHansaPlayerState* PlayerState = Controller != nullptr
			? Controller->GetPlayerState<AHansaPlayerState>() : nullptr;
		if (Controller == nullptr || PlayerState == nullptr)
		{
			OutResult->SetBoolField(TEXT("ready"), false);
			OutError = TEXT("The client controller or replicated player state is not ready.");
			return true;
		}
		const FHansaClientProjectionSnapshot& Projection = Controller->GetClientProjection();
		const bool bReady = PlayerState->bAuthorityReady && PlayerState->PublicHouseId > 0 &&
			Projection.OwnerHouseId == PlayerState->PublicHouseId &&
			!Projection.AuthoritativeHash.IsEmpty() && !Projection.ProjectionDigest.IsEmpty();
		OutResult->SetBoolField(TEXT("ready"), bReady);
		OutResult->SetNumberField(TEXT("publicHouseId"),
			static_cast<double>(PlayerState->PublicHouseId));
		AddProjection(Projection, OutResult);
		OutResult->SetObjectField(TEXT("lastCommandFeedback"),
			MakeFeedback(Controller->GetLastCommandFeedback()));
		return true;
	}

	bool FHansaMultiplayerAutomationService::Query(
		const FString& CorrelationId, const TSharedRef<FJsonObject> Payload,
		const TSharedRef<FJsonObject> OutResult, FString& OutError) const
	{
		FString Query;
		if (!Payload->TryGetStringField(TEXT("query"), Query) ||
			Query != TEXT("multiplayer.status"))
		{
			OutError = TEXT("The multiplayer fixture allows only multiplayer.status.");
			return false;
		}
		return BuildStatus(CorrelationId, OutResult, OutError);
	}

	bool FHansaMultiplayerAutomationService::Command(
		const FString& CorrelationId, const TSharedRef<FJsonObject> Payload,
		const TSharedRef<FJsonObject> OutResult, FString& OutError) const
	{
		UWorld* World = FindGameWorld();
		AHansaStrategyPlayerController* Controller =
			World != nullptr ? FindLocalController(*World) : nullptr;
		if (!bFixtureActive || World == nullptr || World->GetNetMode() != NM_Client ||
			Controller == nullptr)
		{
			OutError = TEXT("Multiplayer fixture commands must be submitted by a connected client process.");
			return false;
		}

		FString Command;
		if (!Payload->TryGetStringField(TEXT("command"), Command))
		{
			OutError = TEXT("The multiplayer command requires a stable command name.");
			return false;
		}

		OutResult->SetStringField(TEXT("fixtureId"), FixtureId);
		OutResult->SetStringField(TEXT("correlationId"), CorrelationId);
		OutResult->SetStringField(TEXT("command"), Command);

		if (Command == TEXT("multiplayer.request_refresh"))
		{
			int64 KnownRevision = 0;
			if (!TryIntegral(Payload, TEXT("knownRevision"), KnownRevision) || KnownRevision < 0)
			{
				OutError = TEXT("multiplayer.request_refresh requires a nonnegative knownRevision.");
				return false;
			}
			Controller->ServerRequestHansaProjectionRefresh(KnownRevision);
			OutResult->SetBoolField(TEXT("submitted"), true);
			return true;
		}

		int64 ClientSequence = 0;
		int64 ClientNonce = 0;
		if (!TryIntegral(Payload, TEXT("clientSequence"), ClientSequence) || ClientSequence <= 0 ||
			!TryIntegral(Payload, TEXT("clientNonce"), ClientNonce) || ClientNonce <= 0)
		{
			OutError = TEXT("Multiplayer intents require positive integral clientSequence and clientNonce.");
			return false;
		}

		FHansaClientCommandIntent Intent;
		Intent.ClientSequence = ClientSequence;
		Intent.ClientNonce = ClientNonce;
		if (Command == TEXT("multiplayer.place_building"))
		{
			Intent.Type = EHansaClientIntentType::PlaceBuilding;
			int64 AnchorX = 0;
			int64 AnchorY = 0;
			if (!Payload->TryGetStringField(TEXT("cityId"), Intent.CityId) ||
				!Payload->TryGetStringField(TEXT("buildingDefinitionId"),
					Intent.BuildingDefinitionId) ||
				!TryIntegral(Payload, TEXT("anchorX"), AnchorX) ||
				!TryIntegral(Payload, TEXT("anchorY"), AnchorY) ||
				AnchorX < MIN_int32 || AnchorX > MAX_int32 ||
				AnchorY < MIN_int32 || AnchorY > MAX_int32)
			{
				OutError = TEXT("multiplayer.place_building requires cityId, buildingDefinitionId, anchorX, and anchorY.");
				return false;
			}
			Intent.AnchorX = static_cast<int32>(AnchorX);
			Intent.AnchorY = static_cast<int32>(AnchorY);
		}
		else if (Command == TEXT("multiplayer.set_route_active"))
		{
			Intent.Type = EHansaClientIntentType::SetRouteActive;
			if (!TryIntegral(Payload, TEXT("routeId"), Intent.RouteId) ||
				!Payload->TryGetBoolField(TEXT("active"), Intent.bActive))
			{
				OutError = TEXT("multiplayer.set_route_active requires routeId and active.");
				return false;
			}
		}
		else if (Command == TEXT("multiplayer.queue_research"))
		{
			Intent.Type = EHansaClientIntentType::QueueResearch;
			if (!Payload->TryGetStringField(TEXT("technologyId"), Intent.TechnologyId))
			{
				OutError = TEXT("multiplayer.queue_research requires technologyId.");
				return false;
			}
		}
		else
		{
			OutError = TEXT("The multiplayer command is not allowlisted.");
			return false;
		}

		Controller->ServerSubmitHansaIntent(Intent);
		OutResult->SetBoolField(TEXT("submitted"), true);
		OutResult->SetNumberField(TEXT("clientSequence"),
			static_cast<double>(ClientSequence));
		OutResult->SetNumberField(TEXT("clientNonce"), static_cast<double>(ClientNonce));
		UE_LOG(LogHansaAutomation, Display,
			TEXT("S11-P04 client intent correlation=%s command=%s sequence=%lld nonce=%lld"),
			*CorrelationId, *Command, static_cast<long long>(ClientSequence),
			static_cast<long long>(ClientNonce));
		return true;
	}

	bool FHansaMultiplayerAutomationService::Step(
		const FString& CorrelationId, const int32 TickCount,
		const TSharedRef<FJsonObject> OutResult, FString& OutError) const
	{
		UWorld* World = FindGameWorld();
		AHansaGameMode* Mode = World != nullptr
			? World->GetAuthGameMode<AHansaGameMode>() : nullptr;
		UHansaRuntimeSimulationHost* Host = Mode != nullptr ? Mode->GetSimulationHost() : nullptr;
		if (!bFixtureActive || Mode == nullptr || Host == nullptr ||
			TickCount <= 0 || TickCount > 10'000 || !Host->AdvanceTicks(TickCount))
		{
			OutError = TEXT("Only the authority process may advance the fixture by 1 through 10000 ticks.");
			return false;
		}
		Mode->RefreshMultiplayerProjections(false);
		OutResult->SetStringField(TEXT("correlationId"), CorrelationId);
		OutResult->SetNumberField(TEXT("ticksAdvanced"), TickCount);
		OutResult->SetNumberField(TEXT("tick"),
			static_cast<double>(Host->GetSimulationTick()));
		return true;
	}

	TSharedRef<FJsonObject> FHansaMultiplayerAutomationService::MakeEvidenceSnapshot(
		const FString& CorrelationId, FString& OutError) const
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		BuildStatus(CorrelationId, Result, OutError);
		return Result;
	}
}