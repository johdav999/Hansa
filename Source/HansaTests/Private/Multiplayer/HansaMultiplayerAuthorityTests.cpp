#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaGameMode.h"
#include "World/HansaGameState.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckWorldFoundation.h"
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
	FHansaAdmissionGrant MissingAdmission;
	TestFalse(TEXT("Authority rejects a connection without an admitted participant capability"),
		Authority.RegisterAdmittedClient(MissingAdmission, Lubeck, Error));
	FHansaClientProjectionSnapshot RejectedProjection;
	TestFalse(TEXT("Rejected connection cannot receive a private projection"),
		Authority.BuildProjection(999, 0, true, RejectedProjection, Error));
	TestTrue(TEXT("Player principal binds to house one"),
		Authority.RegisterAdmittedClient({101, FHansaParticipantId::TryCreate(1001).Value, Host->GetHouseId(), EHansaAdmissionMode::LanOffline}, Lubeck, Error));
	TestTrue(TEXT("Rival principal binds to house two"),
		Authority.RegisterAdmittedClient({202, FHansaParticipantId::TryCreate(1002).Value, Host->GetRivalHouseId(), EHansaAdmissionMode::LanOffline}, Hamburg, Error));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaEightHouseAuthorityTest,
	"Hansa.Multiplayer.Authority.EightIndependentHouses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaEightHouseAuthorityTest::RunTest(const FString& Parameters)
{
	UHansaRuntimeSimulationHost* Host = NewObject<UHansaRuntimeSimulationHost>();
	FString Error;
	if (!TestTrue(TEXT("Eight-house scenario initializes"),
		Host->InitializeForLubeck(nullptr, Error, EHansaRuntimeScenario::LubeckGrainShortage, 0x4d503034ULL)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Scenario exposes eight stable houses"), Host->GetHouseIds().Num(), 8);
	TestEqual(TEXT("Scenario exposes eight finite buildable opportunities"), Host->GetStartingOpportunities().Num(), 8);
	const FHansaGridCoordinate PlayableStart = Hansa::Game::LubeckPlacementGrid::WorldToGrid(
		Hansa::Game::LubeckMap::AutomationStartTransform().GetLocation());
	const FHansaPlacementGridCell* PlayableStartCell = Host->FindPlacementMap()->Cells.FindByPredicate(
		[PlayableStart](const FHansaPlacementGridCell& Cell) { return Cell.Coordinate == PlayableStart; });
	if (!TestNotNull(TEXT("Prototype opening resolves to an authoritative placement cell"), PlayableStartCell)) return false;
	TestEqual(TEXT("Prototype opening belongs to the local player's house"),
		PlayableStartCell->OwnerId, Host->GetHouseId());
	TArray<FHansaGridCoordinate> OpportunityAnchors;
	for (const FHansaHouseStartOpportunity& Opportunity : Host->GetStartingOpportunities())
	{
		TestTrue(TEXT("Starting opportunity belongs to a valid house"), Opportunity.HouseId.IsValid());
		TestTrue(TEXT("Starting opportunity has finite buildable cells"), Opportunity.BuildableCellCount > 0);
		TestFalse(TEXT("Starting opportunity anchors do not overlap"), OpportunityAnchors.Contains(Opportunity.Anchor));
		OpportunityAnchors.AddUnique(Opportunity.Anchor);
	}
	const auto SimulationProjection = Host->BuildProjection();
	TestTrue(TEXT("Eight-house projection builds"), SimulationProjection.IsSuccess());
	if (SimulationProjection)
	{
		TestEqual(TEXT("Projection contains all eight independent house economies"),
			SimulationProjection.Value.GetHouses().Num(), 8);
		for (const FHansaHouseId HouseId : Host->GetHouseIds())
		{
			TestTrue(TEXT("Every house owns a starting vehicle/route asset"),
				SimulationProjection.Value.GetRoutes().ContainsByPredicate([HouseId](const FHansaRouteProjection& Route)
					{ return Route.OwnerId == HouseId; }));
		}
	}
	UHansaRuntimeSimulationHost* ReplayHost = NewObject<UHansaRuntimeSimulationHost>();
	FString ReplayError;
	TestTrue(TEXT("Identical seed reinitializes a second eight-house scenario"),
		ReplayHost->InitializeForLubeck(nullptr, ReplayError,
			EHansaRuntimeScenario::LubeckGrainShortage, 0x4d503034ULL));
	const auto ReplayProjection = ReplayHost->BuildProjection();
	TestTrue(TEXT("Identical eight-house initialization has a deterministic fingerprint"),
		SimulationProjection && ReplayProjection &&
		SimulationProjection.Value.GetFingerprint() == ReplayProjection.Value.GetFingerprint());
	TestEqual(TEXT("Identical eight-house initialization has the same opportunity count"),
		ReplayHost->GetStartingOpportunities().Num(), Host->GetStartingOpportunities().Num());
	for (int32 Index = 0; Index < Host->GetStartingOpportunities().Num() &&
		Index < ReplayHost->GetStartingOpportunities().Num(); ++Index)
	{
		TestEqual(TEXT("Starting opportunity owner is deterministic"),
			ReplayHost->GetStartingOpportunities()[Index].HouseId,
			Host->GetStartingOpportunities()[Index].HouseId);
		TestEqual(TEXT("Starting opportunity anchor X is deterministic"),
			ReplayHost->GetStartingOpportunities()[Index].Anchor.X,
			Host->GetStartingOpportunities()[Index].Anchor.X);
		TestEqual(TEXT("Starting opportunity anchor Y is deterministic"),
			ReplayHost->GetStartingOpportunities()[Index].Anchor.Y,
			Host->GetStartingOpportunities()[Index].Anchor.Y);
		TestEqual(TEXT("Starting opportunity capacity is deterministic"),
			ReplayHost->GetStartingOpportunities()[Index].BuildableCellCount,
			Host->GetStartingOpportunities()[Index].BuildableCellCount);
	}
	UHansaRuntimeSimulationHost* FourPlayerHost = NewObject<UHansaRuntimeSimulationHost>();
	FString FourPlayerError;
	TestTrue(TEXT("Four-player mixed session initializes"), FourPlayerHost->InitializeForLubeck(
		nullptr, FourPlayerError, EHansaRuntimeScenario::LubeckGrainShortage, 0x4d503034ULL));
	FHansaMultiplayerAuthority FourPlayerAuthority;
	TestTrue(TEXT("Four-player mixed authority initializes"), FourPlayerAuthority.Initialize(*FourPlayerHost));
	FHansaClientInterest FourPlayerInterest;
	FourPlayerInterest.CityIds.Add(TEXT("City.Lubeck"));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestTrue(TEXT("Four-player mixed session claims a distinct house"),
			FourPlayerAuthority.RegisterAdmittedClient(
				{static_cast<uint64>(3001 + Index), FHansaParticipantId::TryCreate(4001 + Index).Value,
					FourPlayerHost->GetHouseIds()[Index], EHansaAdmissionMode::LanOffline},
				FourPlayerInterest, FourPlayerError));
	}
	TestEqual(TEXT("Four-player mixed session has four human controllers"),
		FourPlayerAuthority.GetRegisteredClientCount(), 4);
	TestEqual(TEXT("Four-player mixed session fills the other four houses with AI"),
		FourPlayerHost->GetAIControlledHouseCount(), 4);

	FHansaMultiplayerAuthority Authority;
	TestTrue(TEXT("Authority initializes for eight-house campaign"), Authority.Initialize(*Host));
	FHansaClientInterest Interest;
	Interest.CityIds.Add(TEXT("City.Lubeck"));
	TArray<uint64> Principals;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const uint64 Principal = static_cast<uint64>(1001 + Index);
		const FHansaParticipantId Participant = FHansaParticipantId::TryCreate(2001 + Index).Value;
		TestTrue(*FString::Printf(TEXT("Client %d claims a distinct house"), Index + 1),
			Authority.RegisterAdmittedClient(
				{Principal, Participant, Host->GetHouseIds()[Index], EHansaAdmissionMode::LanOffline},
				Interest, Error));
		Principals.Add(Principal);
		FHansaClientProjectionSnapshot Projection;
		TestTrue(TEXT("Independent client receives its owner projection"),
			Authority.BuildProjection(Principal, 0, true, Projection, Error));
		TestEqual(TEXT("Owner projection binds the distinct house"),
			Projection.OwnerHouseId, static_cast<int64>(Host->GetHouseIds()[Index].GetValue()));
	}
	TestEqual(TEXT("All eight clients are registered"), Authority.GetRegisteredClientCount(), 8);
	TestEqual(TEXT("Zero AI controllers remain when all houses are human"), Host->GetAIControlledHouseCount(), 0);
	TestFalse(TEXT("A ninth client cannot double-claim a house"),
		Authority.RegisterAdmittedClient(
			{9009, FHansaParticipantId::TryCreate(9009).Value, Host->GetHouseIds()[0], EHansaAdmissionMode::LanOffline},
			Interest, Error));
	TestEqual(TEXT("Rejected ninth client does not change capacity"), Authority.GetRegisteredClientCount(), 8);
	const int32 StartingRouteCount = SimulationProjection ? SimulationProjection.Value.GetRoutes().Num() : 0;
	Authority.UnregisterClient(Principals[3]);
	TestEqual(TEXT("Disconnect releases exactly one house"), Authority.GetRegisteredClientCount(), 7);
	TestEqual(TEXT("AI resumes for exactly the released house by policy"), Host->GetAIControlledHouseCount(), 1);
	FHansaClientProjectionSnapshot ReleasedProjection;
	TestFalse(TEXT("Released principal immediately loses its private projection"),
		Authority.BuildProjection(Principals[3], 0, true, ReleasedProjection, Error));
	const FHansaHouseId SwappedHouse = Host->GetHouseIds()[3];
	TestTrue(TEXT("Replacement human atomically takes the released house from AI"),
		Authority.RegisterAdmittedClient(
			{9010, FHansaParticipantId::TryCreate(9010).Value, SwappedHouse, EHansaAdmissionMode::LanOffline},
			Interest, Error));
	TestEqual(TEXT("Replacement returns the session to eight human controllers"),
		Authority.GetRegisteredClientCount(), 8);
	TestEqual(TEXT("Replacement leaves no competing AI controller"), Host->GetAIControlledHouseCount(), 0);
	const auto SwappedProjection = Host->BuildProjection();
	TestTrue(TEXT("Ownership swap preserves the authoritative simulation"), SwappedProjection.IsSuccess());
	if (SwappedProjection)
	{
		TestEqual(TEXT("Ownership swap does not duplicate starting assets"),
			SwappedProjection.Value.GetRoutes().Num(), StartingRouteCount);
		TestEqual(TEXT("Ownership swap preserves all house economies"),
			SwappedProjection.Value.GetHouses().Num(), 8);
	}
	return !HasAnyErrors();
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
	TestTrue(TEXT("First owner joins"), Authority.RegisterAdmittedClient(
		{101, FHansaParticipantId::TryCreate(1001).Value, Host->GetHouseId(), EHansaAdmissionMode::LanOffline}, Lubeck, Error));
	TestTrue(TEXT("Second owner joins"), Authority.RegisterAdmittedClient(
		{202, FHansaParticipantId::TryCreate(1002).Value, Host->GetRivalHouseId(), EHansaAdmissionMode::LanOffline}, Lubeck, Error));

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
		Authority.RegisterAdmittedClient({303, FHansaParticipantId::TryCreate(1002).Value, Host->GetRivalHouseId(), EHansaAdmissionMode::LanOffline}, Lubeck, Error));

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

	TestEqual(TEXT("TR05 fixture pins fingerprint contract"),FHansaSimulationState::DeterminismFingerprintVersion,28U);
	TestEqual(TEXT("TR05 fixture pins command contract"),FHansaCommandHeader::CurrentSchemaVersion,uint16(11));
	const FString ExpectedHash =
		FixtureJson->GetStringField(TEXT("expectedFinalAuthoritativeHashV28Command11"));
	AddInfo(FString::Printf(TEXT("two_player_authority_v1 final authoritative hash: %s"),
		*FirstFinal.AuthoritativeHash));
	if (!ExpectedHash.StartsWith(TEXT("record-after")))
	{
		TestEqual(TEXT("Fixture final authoritative checksum is stable"),
			FirstFinal.AuthoritativeHash, ExpectedHash);
	}
	return true;
}
