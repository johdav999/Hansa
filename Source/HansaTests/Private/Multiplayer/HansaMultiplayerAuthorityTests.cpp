#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaGameMode.h"
#include "World/HansaGameState.h"
#include "World/HansaPlayerState.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyPlayerController.h"

using namespace Hansa::Multiplayer;
using namespace Hansa::Simulation;

namespace
{
	FHansaClientCommandIntent Intent(
		const int64 Sequence, const int64 Nonce, const EHansaClientIntentType Type)
	{
		FHansaClientCommandIntent Result;
		Result.ClientSequence = Sequence;
		Result.ClientNonce = Nonce;
		Result.Type = Type;
		return Result;
	}

	bool EventsAreOrdered(const TArray<FHansaReplicatedEvent>& Events)
	{
		for (int32 Index = 1; Index < Events.Num(); ++Index)
		{
			if (Events[Index - 1].GlobalSequence >= Events[Index].GlobalSequence) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaServerAuthorityProjectionTest,
	"Hansa.Multiplayer.Authority.ServerValidatedProjections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaServerAuthorityProjectionTest::RunTest(const FString& Parameters)
{
	UHansaRuntimeSimulationHost* Host = NewObject<UHansaRuntimeSimulationHost>();
	FString Error;
	if (!TestTrue(TEXT("Authoritative host initializes"),
		Host->InitializeForLubeck(nullptr, Error, EHansaRuntimeScenario::LubeckGrainShortage, 0x533131503033ULL)))
	{
		AddError(Error);
		return false;
	}

	FHansaMultiplayerAuthority Authority;
	if (!TestTrue(TEXT("Authority binds only to a ready server host"), Authority.Initialize(*Host))) return false;

	FHansaClientInterest Lubeck;
	Lubeck.CityIds.Add(TEXT("City.Lubeck"));
	FHansaClientInterest Hamburg;
	Hamburg.CityIds.Add(TEXT("City.Hamburg"));
	TestTrue(TEXT("Player principal binds to house one"),
		Authority.RegisterClient(101, Host->GetHouseId(), Lubeck, Error));
	TestTrue(TEXT("Rival principal binds to house two"),
		Authority.RegisterClient(202, Host->GetRivalHouseId(), Hamburg, Error));
	TestEqual(TEXT("Exactly two proof clients are registered"), Authority.GetRegisteredClientCount(), 2);

	FHansaClientProjectionSnapshot PlayerInitial;
	FHansaClientProjectionSnapshot RivalInitial;
	TestTrue(TEXT("Player receives an initial projection"),
		Authority.BuildProjection(101, 0, false, PlayerInitial, Error));
	TestTrue(TEXT("Rival receives an initial projection"),
		Authority.BuildProjection(202, 0, false, RivalInitial, Error));
	TestTrue(TEXT("Initial player refresh is full"), PlayerInitial.bFullRefresh);
	TestTrue(TEXT("Initial rival refresh is full"), RivalInitial.bFullRefresh);
	TestEqual(TEXT("Clients diagnose the same authoritative server hash"),
		PlayerInitial.AuthoritativeHash, RivalInitial.AuthoritativeHash);
	TestTrue(TEXT("Filtered projection digests distinguish client views"),
		PlayerInitial.ProjectionDigest != RivalInitial.ProjectionDigest);
	TestTrue(TEXT("Player market projection is scoped to L�beck"),
		!PlayerInitial.Markets.IsEmpty() && PlayerInitial.Markets.ContainsByPredicate(
			[](const FHansaReplicatedMarket& Market) { return Market.CityId == TEXT("City.Lubeck"); }) &&
		!PlayerInitial.Markets.ContainsByPredicate(
			[](const FHansaReplicatedMarket& Market) { return Market.CityId != TEXT("City.Lubeck"); }));
	TestTrue(TEXT("Rival market projection is scoped to Hamburg"),
		!RivalInitial.Markets.IsEmpty() && RivalInitial.Markets.ContainsByPredicate(
			[](const FHansaReplicatedMarket& Market) { return Market.CityId == TEXT("City.Hamburg"); }) &&
		!RivalInitial.Markets.ContainsByPredicate(
			[](const FHansaReplicatedMarket& Market) { return Market.CityId != TEXT("City.Hamburg"); }));
	TestEqual(TEXT("Player receives only its private research"), PlayerInitial.Research.HouseId, int64(1));
	TestEqual(TEXT("Rival receives only its private research"), RivalInitial.Research.HouseId, int64(2));
	TestTrue(TEXT("Public victory progress is projected"), !PlayerInitial.VictoryObjectives.IsEmpty());
	TestTrue(TEXT("Owned route cargo is visible"),
		PlayerInitial.Routes.ContainsByPredicate([](const FHansaReplicatedRoute& Route)
		{
			return Route.OwnerHouseId == 1 && Route.bCargoVisible;
		}));
	TestTrue(TEXT("Other-house route cargo is hidden"),
		PlayerInitial.Routes.ContainsByPredicate([](const FHansaReplicatedRoute& Route)
		{
			return Route.OwnerHouseId == 2 && !Route.bCargoVisible && Route.CargoMilliUnits == 0;
		}));

	FHansaClientCommandIntent Place = Intent(1, 1001, EHansaClientIntentType::PlaceBuilding);
	Place.CityId = TEXT("City.Lubeck");
	Place.BuildingDefinitionId = TEXT("Building.Road");
	Place.AnchorX = 10;
	Place.AnchorY = 30;
	const FHansaClientCommandFeedback Placed = Authority.SubmitIntent(101, Place);
	TestTrue(TEXT("Owner placement intent is accepted through the server gateway"), Placed.bAccepted);
	TestTrue(TEXT("Server assigns a positive global command order"), Placed.AcceptedGlobalSequence > 0);

	FHansaClientCommandIntent Skipped = Intent(3, 1003, EHansaClientIntentType::QueueResearch);
	Skipped.TechnologyId = TEXT("Technology.Commerce.MarketReports");
	const FHansaClientCommandFeedback OrderRejected = Authority.SubmitIntent(101, Skipped);
	TestEqual(TEXT("Skipped client sequence is rejected before mutation"),
		OrderRejected.Rejection, EHansaClientCommandRejection::CommandOrderInvalid);
	TestEqual(TEXT("Rejected order reports the next expected sequence"),
		Authority.GetExpectedClientSequence(101), uint64(2));

	FHansaClientCommandIntent Unauthorized = Intent(1, 2001, EHansaClientIntentType::SetRouteActive);
	Unauthorized.RouteId = 1;
	Unauthorized.bActive = true;
	const FHansaClientCommandFeedback OwnershipRejected = Authority.SubmitIntent(202, Unauthorized);
	TestEqual(TEXT("A client cannot operate another house's route"),
		OwnershipRejected.Rejection, EHansaClientCommandRejection::NotAuthorized);
	TestEqual(TEXT("Gateway exposes the stable ownership cause"),
		OwnershipRejected.GatewayError, FString(TEXT("NotAuthorized")));
	TestTrue(TEXT("Ownership rejection includes a remedy"), !OwnershipRejected.Remedy.IsEmpty());

	FHansaClientCommandIntent OwnRoute = Intent(2, 2002, EHansaClientIntentType::SetRouteActive);
	OwnRoute.RouteId = 3;
	OwnRoute.bActive = true;
	const FHansaClientCommandFeedback RouteAccepted = Authority.SubmitIntent(202, OwnRoute);
	TestTrue(TEXT("A client may operate its own route"), RouteAccepted.bAccepted);
	TestTrue(TEXT("Server global order advances across principals"),
		RouteAccepted.AcceptedGlobalSequence > Placed.AcceptedGlobalSequence);

	FHansaClientCommandIntent Research = Intent(2, 1002, EHansaClientIntentType::QueueResearch);
	Research.TechnologyId = TEXT("Technology.Commerce.MarketReports");
	const FHansaClientCommandFeedback ResearchAccepted = Authority.SubmitIntent(101, Research);
	TestTrue(TEXT("Owner research intent is accepted"), ResearchAccepted.bAccepted);
	TestTrue(TEXT("Research follows the prior server command order"),
		ResearchAccepted.AcceptedGlobalSequence > RouteAccepted.AcceptedGlobalSequence);

	const FHansaClientCommandFeedback Duplicate = Authority.SubmitIntent(101, Research);
	TestEqual(TEXT("Duplicate nonce and sequence are rejected"),
		Duplicate.Rejection, EHansaClientCommandRejection::DuplicateCommand);

	FHansaClientProjectionSnapshot PlayerDelta;
	FHansaClientProjectionSnapshot RivalDelta;
	TestTrue(TEXT("Player receives an incremental projection"),
		Authority.BuildProjection(101, PlayerInitial.Revision, false, PlayerDelta, Error));
	TestTrue(TEXT("Rival receives an incremental projection"),
		Authority.BuildProjection(202, RivalInitial.Revision, false, RivalDelta, Error));
	TestFalse(TEXT("Matching revision yields a delta"), PlayerDelta.bFullRefresh);
	TestTrue(TEXT("Placement appears in the relevant player's projection"),
		PlayerDelta.Placements.ContainsByPredicate([](const FHansaReplicatedPlacement& Item)
		{
			return Item.CityId == TEXT("City.Lubeck") &&
				Item.BuildingDefinitionId == TEXT("Building.Road");
		}));
	TestTrue(TEXT("Placement stays outside the Hamburg-only projection"), RivalDelta.Placements.IsEmpty());
	TestEqual(TEXT("Queued research is owner-only"), PlayerDelta.Research.ActiveTechnologyId,
		FString(TEXT("Technology.Commerce.MarketReports")));
	TestTrue(TEXT("Other client does not receive that private research"),
		RivalDelta.Research.ActiveTechnologyId != TEXT("Technology.Commerce.MarketReports"));
	TestTrue(TEXT("Player event delta is strictly ordered"), EventsAreOrdered(PlayerDelta.Events));
	TestTrue(TEXT("Rival event delta is strictly ordered"), EventsAreOrdered(RivalDelta.Events));
	TestEqual(TEXT("Delta clients still diagnose one authoritative state"),
		PlayerDelta.AuthoritativeHash, RivalDelta.AuthoritativeHash);
	TestTrue(TEXT("Their partial projection digests remain distinct"),
		PlayerDelta.ProjectionDigest != RivalDelta.ProjectionDigest);

	FHansaClientProjectionSnapshot LateRefresh;
	TestTrue(TEXT("A stale client can request a late projection refresh"),
		Authority.BuildProjection(202, 0, false, LateRefresh, Error));
	TestTrue(TEXT("Stale revision produces a full refresh"), LateRefresh.bFullRefresh);
	TestEqual(TEXT("Late refresh catches the current authoritative hash"),
		LateRefresh.AuthoritativeHash, PlayerDelta.AuthoritativeHash);
	TestTrue(TEXT("Late refresh preserves public victory state"), !LateRefresh.VictoryObjectives.IsEmpty());

	const int32 AiDecisionCount = Host->GetMerchantAIDecisionHistory().Num();
	Host->SetMerchantAIEnabled(false);
	TestTrue(TEXT("Authority can advance while a human owns the rival house"), Host->AdvanceTicks(16));
	TestEqual(TEXT("Suspended rival AI issues no competing house commands"),
		Host->GetMerchantAIDecisionHistory().Num(), AiDecisionCount);
	TestTrue(TEXT("Victory evaluation continues while rival AI is suspended"),
		Host->GetScenarioProgress() != nullptr &&
		Host->GetScenarioProgress()->LastEvaluatedTick.GetValue() == Host->GetSimulationTick());
	Host->SetMerchantAIEnabled(true);

	const UFunction* SubmitRpc = AHansaStrategyPlayerController::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(AHansaStrategyPlayerController, ServerSubmitHansaIntent));
	TestTrue(TEXT("Intent transport is a reliable server RPC"), SubmitRpc != nullptr &&
		SubmitRpc->HasAllFunctionFlags(FUNC_Net | FUNC_NetReliable | FUNC_NetServer));
	const FProperty* ProjectionProperty = FindFProperty<FProperty>(
		AHansaStrategyPlayerController::StaticClass(), TEXT("ClientProjection"));
	TestTrue(TEXT("Client projection is a replicated property"), ProjectionProperty != nullptr &&
		ProjectionProperty->HasAnyPropertyFlags(CPF_Net));

	const AHansaGameMode* Mode = GetDefault<AHansaGameMode>();
	TestTrue(TEXT("Game mode selects replicated Hansa GameState"),
		Mode->GameStateClass == AHansaGameState::StaticClass());
	TestTrue(TEXT("Game mode selects replicated Hansa PlayerState"),
		Mode->PlayerStateClass == AHansaPlayerState::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTwoPlayerReconnectFixtureTest,
	"Hansa.Multiplayer.Authority.TwoPlayerReconnectFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTwoPlayerReconnectFixtureTest::RunTest(const FString& Parameters)
{
	const FString FixturePath = FPaths::Combine(
		FPaths::ProjectDir(), TEXT("Tests/Fixtures/two_player_authority_v1.json"));
	FString FixtureText;
	TSharedPtr<FJsonObject> FixtureJson;
	if (!TestTrue(TEXT("The two-player fixture file is readable"),
		FFileHelper::LoadFileToString(FixtureText, *FixturePath)) ||
		!TestTrue(TEXT("The two-player fixture file is valid JSON"),
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FixtureText), FixtureJson) &&
			FixtureJson.IsValid()))
	{
		return false;
	}
	TestEqual(TEXT("Fixture identity is stable"),
		FixtureJson->GetStringField(TEXT("fixtureId")),
		FString(TEXT("two_player_authority_v1")));
	const uint64 Seed = FCString::Strtoui64(
		*FixtureJson->GetStringField(TEXT("seed")), nullptr, 10);

	UHansaRuntimeSimulationHost* Host = NewObject<UHansaRuntimeSimulationHost>();
	FString Error;
	if (!TestTrue(TEXT("Reconnect fixture authority initializes"),
		Host->InitializeForLubeck(
			nullptr, Error, EHansaRuntimeScenario::LubeckGrainShortage, Seed)))
	{
		AddError(Error);
		return false;
	}

	FHansaMultiplayerAuthority Authority;
	if (!TestTrue(TEXT("Reconnect fixture binds to authority"), Authority.Initialize(*Host)))
	{
		return false;
	}
	FHansaClientInterest Lubeck;
	Lubeck.CityIds.Add(TEXT("City.Lubeck"));
	TestTrue(TEXT("First owner joins"), Authority.RegisterClient(
		101, Host->GetHouseId(), Lubeck, Error));
	TestTrue(TEXT("Second owner joins"), Authority.RegisterClient(
		202, Host->GetRivalHouseId(), Lubeck, Error));

	FHansaClientCommandIntent Place = Intent(
		1, 11001, EHansaClientIntentType::PlaceBuilding);
	Place.CityId = TEXT("City.Lubeck");
	Place.BuildingDefinitionId = TEXT("Building.Road");
	Place.AnchorX = 10;
	Place.AnchorY = 30;
	TestTrue(TEXT("First owner can place"), Authority.SubmitIntent(101, Place).bAccepted);

	FHansaClientCommandIntent FirstRejected = Intent(
		2, 11002, EHansaClientIntentType::SetRouteActive);
	FirstRejected.RouteId = 3;
	FirstRejected.bActive = true;
	TestEqual(TEXT("First owner cannot operate the rival route"),
		Authority.SubmitIntent(101, FirstRejected).Rejection,
		EHansaClientCommandRejection::NotAuthorized);

	FHansaClientCommandIntent SecondRejected = Intent(
		1, 22001, EHansaClientIntentType::SetRouteActive);
	SecondRejected.RouteId = 1;
	SecondRejected.bActive = true;
	TestEqual(TEXT("Second owner cannot operate the player route"),
		Authority.SubmitIntent(202, SecondRejected).Rejection,
		EHansaClientCommandRejection::NotAuthorized);

	FHansaClientCommandIntent SecondAllowed = Intent(
		2, 22002, EHansaClientIntentType::SetRouteActive);
	SecondAllowed.RouteId = 3;
	SecondAllowed.bActive = true;
	TestTrue(TEXT("Second owner can operate its route"),
		Authority.SubmitIntent(202, SecondAllowed).bAccepted);

	FHansaClientProjectionSnapshot FirstFinal;
	FHansaClientProjectionSnapshot SecondFinal;
	TestTrue(TEXT("First final projection is available"),
		Authority.BuildProjection(101, 0, false, FirstFinal, Error));
	TestTrue(TEXT("Second final projection is available"),
		Authority.BuildProjection(202, 0, false, SecondFinal, Error));
	TestEqual(TEXT("Owners share the final authoritative hash"),
		FirstFinal.AuthoritativeHash, SecondFinal.AuthoritativeHash);
	TestTrue(TEXT("Owners retain distinct partial digests"),
		FirstFinal.ProjectionDigest != SecondFinal.ProjectionDigest);

	Authority.UnregisterClient(202);
	TestEqual(TEXT("Disconnect releases one authority registration"),
		Authority.GetRegisteredClientCount(), 1);
	TestTrue(TEXT("Reconnecting owner receives the same house"),
		Authority.RegisterClient(303, Host->GetRivalHouseId(), Lubeck, Error));

	FHansaClientProjectionSnapshot Reconnected;
	TestTrue(TEXT("Reconnect projection is available"),
		Authority.BuildProjection(303, 0, false, Reconnected, Error));
	TestTrue(TEXT("Reconnect receives a full projection"), Reconnected.bFullRefresh);
	TestEqual(TEXT("Reconnect resynchronizes to the authoritative hash"),
		Reconnected.AuthoritativeHash, FirstFinal.AuthoritativeHash);
	TestEqual(TEXT("Reconnect restores second-owner private identity"),
		Reconnected.OwnerHouseId, int64(2));
	TestTrue(TEXT("Reconnect observes the second owner's route"),
		Reconnected.Routes.ContainsByPredicate([](const FHansaReplicatedRoute& Route)
		{
			return Route.RouteId == 3 && Route.OwnerHouseId == 2 && Route.bCargoVisible;
		}));

	const FString ExpectedHash =
		FixtureJson->GetStringField(TEXT("expectedFinalAuthoritativeHash"));
	AddInfo(FString::Printf(TEXT("two_player_authority_v1 final authoritative hash: %s"),
		*FirstFinal.AuthoritativeHash));
	if (!ExpectedHash.StartsWith(TEXT("record-after")))
	{
		TestEqual(TEXT("Fixture final authoritative checksum is stable"),
			FirstFinal.AuthoritativeHash, ExpectedHash);
	}
	return true;
}
