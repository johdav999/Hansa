#include "Misc/AutomationTest.h"

#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Multiplayer;
using namespace Hansa::Simulation;

namespace
{
	FHansaClientRouteStopIntent ClientStop(const FHansaRouteStop& Source)
	{
		FHansaClientRouteStopIntent Result;
		Result.CityId = Source.CityId.ToString();
		for (const FHansaRouteCargoAction& SourceAction : Source.Actions)
		{
			FHansaClientRouteActionIntent& Action = Result.Actions.AddDefaulted_GetRef();
			Action.Kind = static_cast<uint8>(SourceAction.Kind);
			Action.GoodId = SourceAction.GoodId.ToString();
			Action.QuantityMilliUnits = SourceAction.QuantityLimit.GetRawValue();
			Action.MinimumSourceReserveMilliUnits = SourceAction.MinimumSourceReserve.GetRawValue();
		}
		return Result;
	}

	bool IsOwnerBuilding(const FHansaSimulationProjection& Projection,
		const FHansaHouseId HouseId, const FHansaBuildingId BuildingId)
	{
		const auto* Building = Projection.GetBuildingWorldProjections().FindByPredicate(
			[BuildingId](const FHansaBuildingWorldProjection& Item) { return Item.BuildingId == BuildingId; });
		return Building != nullptr && Building->OwnerId == HouseId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAllCurrentCommandPathsTest,
	"Hansa.Multiplayer.CommandPaths.AllCurrentActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaAllCurrentCommandPathsTest::RunTest(const FString& Parameters)
{
	UHansaRuntimeSimulationHost* Host = NewObject<UHansaRuntimeSimulationHost>();
	FString Error;
	if (!TestTrue(TEXT("Command-path authority initializes"), Host->InitializeForLubeck(
		nullptr, Error, EHansaRuntimeScenario::LubeckGrainShortage, 0x4d503035ULL)))
	{
		AddError(Error);
		return false;
	}
	FHansaMultiplayerAuthority Authority;
	if (!TestTrue(TEXT("Command-path gateway initializes"), Authority.Initialize(*Host))) return false;
	FHansaClientInterest Interest;
	Interest.CityIds.Add(TEXT("City.Lubeck"));
	const FHansaHouseId House = Host->GetHouseId();
	TestTrue(TEXT("Owner is admitted"), Authority.RegisterAdmittedClient(
		{ 501, FHansaParticipantId::TryCreate(1501).Value, House, EHansaAdmissionMode::LanOffline },
		Interest, Error));

	int64 Sequence = 0;
	int64 Nonce = 50000;
	auto Submit = [&](FHansaClientCommandIntent Intent, const TCHAR* Label)
	{
		Intent.ClientSequence = ++Sequence;
		Intent.ClientNonce = ++Nonce;
		Intent.ExpectedServerTick = Host->GetSimulationTick();
		const FHansaClientCommandFeedback Feedback = Authority.SubmitIntent(501, Intent);
		TestTrue(*FString::Printf(TEXT("%s returns structured cause/remedy"), Label),
			Feedback.bAccepted || (!Feedback.Message.IsEmpty() && !Feedback.Remedy.IsEmpty()));
		TestTrue(*FString::Printf(TEXT("%s passes the network payload boundary"), Label),
			Feedback.Rejection != EHansaClientCommandRejection::InvalidPayload &&
			Feedback.Rejection != EHansaClientCommandRejection::NotAuthorized);
		return Feedback;
	};

	const FHansaHouseStartOpportunity* Opportunity = Host->GetStartingOpportunities().FindByPredicate(
		[House](const FHansaHouseStartOpportunity& Item) { return Item.HouseId == House; });
	if (!TestNotNull(TEXT("Owner has a finite build opportunity"), Opportunity)) return false;

	FHansaClientCommandIntent Place;
	Place.Type = EHansaClientIntentType::PlaceBuilding;
	FHansaClientPlacementIntent& FirstRoad = Place.Placements.AddDefaulted_GetRef();
	FirstRoad.CityId = TEXT("City.Lubeck");
	FirstRoad.BuildingDefinitionId = TEXT("Building.Road");
	FirstRoad.AnchorX = Opportunity->Anchor.X;
	FirstRoad.AnchorY = Opportunity->Anchor.Y;
	const FHansaClientPlacementIntent RoadTemplate = FirstRoad;
	FHansaClientPlacementIntent& SecondRoad = Place.Placements.AddDefaulted_GetRef();
	SecondRoad = RoadTemplate;
	SecondRoad.AnchorX += 1;
	const FHansaClientCommandFeedback Placed = Submit(Place, TEXT("Atomic held-road placement"));
	TestTrue(TEXT("Atomic held-road placement is accepted"), Placed.bAccepted);
	FHansaClientCommandIntent ReplayedPlace = Place;
	ReplayedPlace.ClientSequence = 1;
	ReplayedPlace.ClientNonce = 50001;
	ReplayedPlace.ExpectedServerTick = 0;
	const uint64 BeforeReplay = Host->GetLastProcessedCommandSequence();
	const auto ReplayFeedback = Authority.SubmitIntent(501, ReplayedPlace);
	TestEqual(TEXT("Repeated accepted packet is rejected idempotently"), ReplayFeedback.Rejection,
		EHansaClientCommandRejection::DuplicateCommand);
	TestEqual(TEXT("Repeated packet cannot double-spend or duplicate buildings"),
		Host->GetLastProcessedCommandSequence(), BeforeReplay);

	auto ProjectionResult = Host->BuildProjection();
	if (!TestTrue(TEXT("Projection follows placement"), ProjectionResult.IsSuccess())) return false;
	const FHansaBuildingWorldProjection* NewRoad = ProjectionResult.Value.GetBuildingWorldProjections().FindByPredicate(
		[House, Opportunity](const FHansaBuildingWorldProjection& Item)
		{
			return Item.OwnerId == House && Item.Placement.Anchor == Opportunity->Anchor &&
				Item.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Road");
		});
	if (!TestNotNull(TEXT("Placed road has a server identity"), NewRoad)) return false;

	FHansaClientCommandIntent Cancel;
	Cancel.Type = EHansaClientIntentType::CancelConstruction;
	Cancel.BuildingId = NewRoad->BuildingId.GetValue();
	TestTrue(TEXT("Construction cancellation is accepted"), Submit(Cancel, TEXT("Cancel construction")).bAccepted);

	ProjectionResult = Host->BuildProjection();
	if (!ProjectionResult) return false;
	const FHansaProductionProjection* Production = ProjectionResult.Value.GetProductions().FindByPredicate(
		[&](const FHansaProductionProjection& Item)
		{
			return IsOwnerBuilding(ProjectionResult.Value, House, Item.BuildingId);
		});
	if (TestNotNull(TEXT("Fixture exposes an owned production"), Production))
	{
		FHansaClientCommandIntent Active;
		Active.Type = EHansaClientIntentType::SetProductionActive;
		Active.ProductionId = Production->Id.GetValue();
		Active.bActive = !Production->bActive;
		TestTrue(TEXT("Production pause/resume is accepted"), Submit(Active, TEXT("Production active")).bAccepted);

		FHansaClientCommandIntent Mode;
		Mode.Type = EHansaClientIntentType::SetProductionMode;
		Mode.ProductionId = Production->Id.GetValue();
		Mode.RecipeId = Production->RequestedRecipeId.ToString();
		Mode.bFallback = Production->bFallbackToFresh;
		Submit(Mode, TEXT("Production recipe/mode"));

		FHansaClientCommandIntent Upgrade;
		Upgrade.Type = EHansaClientIntentType::UpgradeProduction;
		Upgrade.ProductionId = Production->Id.GetValue();
		Submit(Upgrade, TEXT("Production upgrade"));
	}

	const FHansaBuildingWorldProjection* OwnedBuilding =
		ProjectionResult.Value.GetBuildingWorldProjections().FindByPredicate(
			[House](const FHansaBuildingWorldProjection& Item) { return Item.OwnerId == House; });
	if (TestNotNull(TEXT("Fixture exposes an owned building"), OwnedBuilding))
	{
		FHansaClientCommandIntent Remove;
		Remove.Type = EHansaClientIntentType::RemoveBuilding;
		Remove.BuildingId = OwnedBuilding->BuildingId.GetValue();
		Submit(Remove, TEXT("Building demolition"));
	}

	const FHansaPopulationCohortProjection* Residence =
		ProjectionResult.Value.GetPopulationCohorts().FindByPredicate(
		[&](const FHansaPopulationCohortProjection& Item)
		{
			return IsOwnerBuilding(ProjectionResult.Value, House, Item.ResidenceBuildingId);
		});
	if (Residence != nullptr)
	{
		FHansaClientCommandIntent UpgradeResidence;
		UpgradeResidence.Type = EHansaClientIntentType::UpgradeResidence;
		UpgradeResidence.BuildingId = Residence->ResidenceBuildingId.GetValue();
		Submit(UpgradeResidence, TEXT("Residence upgrade"));
	}

	const FHansaBuildingWorldProjection* Market =
		ProjectionResult.Value.GetBuildingWorldProjections().FindByPredicate(
			[House](const FHansaBuildingWorldProjection& Item)
			{
				return Item.OwnerId == House &&
					Item.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Market");
			});
	if (Market != nullptr)
	{
		FHansaClientCommandIntent Heating;
		Heating.Type = EHansaClientIntentType::SetHeatingReserve;
		Heating.BuildingId = Market->BuildingId.GetValue();
		Heating.ReserveDays = Host->QueryHeating().ReserveDays;
		Heating.bReleaseProtection = Host->QueryHeating().bOverride;
		TestTrue(TEXT("Heating reserve is accepted"), Submit(Heating, TEXT("Heating reserve")).bAccepted);

		FHansaClientCommandIntent Availability;
		Availability.Type = EHansaClientIntentType::SetHouseholdAvailability;
		Availability.BuildingId = Market->BuildingId.GetValue();
		Availability.GoodId = TEXT("Good.PreservedFish");
		Availability.bAvailable = true;
		Submit(Availability, TEXT("Household availability"));
	}

	const FHansaRouteProjection* Route = ProjectionResult.Value.GetRoutes().FindByPredicate(
		[House](const FHansaRouteProjection& Item)
		{
			return Item.OwnerId == House && Item.Lifecycle != EHansaRouteLifecycleState::Cancelled;
		});
	if (TestNotNull(TEXT("Fixture exposes an owned route"), Route))
	{
		FHansaClientCommandIntent Edit;
		Edit.Type = EHansaClientIntentType::EditRoute;
		Edit.RouteId = Route->Id.GetValue();
		for (const FHansaRouteStop& Stop : Route->Stops) Edit.RouteStops.Add(ClientStop(Stop));
		Submit(Edit, TEXT("Route edit/load/unload"));

		FHansaClientCommandIntent Move;
		Move.Type = EHansaClientIntentType::MoveShip;
		Move.VehicleId = Route->VehicleId.GetValue();
		const auto* Vehicle = ProjectionResult.Value.GetVehicles().FindByPredicate(
			[Route](const FHansaVehicleProjection& Item) { return Item.Id == Route->VehicleId; });
		if (Vehicle != nullptr)
		{
			Move.TargetX = Vehicle->Navigation.Cell.X;
			Move.TargetY = Vehicle->Navigation.Cell.Y;
			Submit(Move, TEXT("Manual ship order"));
		}

		FHansaClientCommandIntent Active;
		Active.Type = EHansaClientIntentType::SetRouteActive;
		Active.RouteId = Route->Id.GetValue();
		Active.bActive = Route->Lifecycle == EHansaRouteLifecycleState::Inactive;
		Submit(Active, TEXT("Route start/pause"));

		FHansaClientCommandIntent CancelRoute;
		CancelRoute.Type = EHansaClientIntentType::CancelRoute;
		CancelRoute.RouteId = Route->Id.GetValue();
		Submit(CancelRoute, TEXT("Route cancellation"));

		FHansaClientCommandIntent Create;
		Create.Type = EHansaClientIntentType::CreateRoute;
		Create.VehicleId = Route->VehicleId.GetValue();
		Create.RouteName = TEXT("MP-05 replacement voyage");
		for (const FHansaRouteStop& Stop : Route->Stops) Create.RouteStops.Add(ClientStop(Stop));
		Submit(Create, TEXT("Route create and activate"));
	}

	FHansaClientCommandIntent Research;
	Research.Type = EHansaClientIntentType::QueueResearch;
	Research.TechnologyId = TEXT("Technology.Commerce.MarketReports");
	Submit(Research, TEXT("Research queue"));

	FHansaClientCommandIntent Stale = Research;
	Stale.ClientSequence = Sequence + 1;
	Stale.ClientNonce = ++Nonce;
	Stale.ExpectedServerTick = Host->GetSimulationTick() + 1;
	const auto StaleFeedback = Authority.SubmitIntent(501, Stale);
	TestEqual(TEXT("Future/stale selection is rejected before mutation"),
		StaleFeedback.Rejection, EHansaClientCommandRejection::StaleProjection);
	TestEqual(TEXT("Stale rejection does not consume client order"),
		Authority.GetExpectedClientSequence(501), static_cast<uint64>(Sequence + 1));

	FHansaClientCommandIntent InvalidBatch;
	InvalidBatch.Type = EHansaClientIntentType::PlaceBuilding;
	for (int32 Index = 0; Index <= FHansaClientCommandIntent::MaximumPlacementCount; ++Index)
		InvalidBatch.Placements.Add(RoadTemplate);
	InvalidBatch.ClientSequence = Sequence + 1;
	InvalidBatch.ClientNonce = ++Nonce;
	const uint64 BeforeInvalidBatch = Host->GetLastProcessedCommandSequence();
	const auto InvalidBatchFeedback = Authority.SubmitIntent(501, InvalidBatch);
	TestEqual(TEXT("Oversized held stroke is rejected"), InvalidBatchFeedback.Rejection,
		EHansaClientCommandRejection::InvalidPayload);
	TestEqual(TEXT("Rejected batch performs no partial mutation"),
		Host->GetLastProcessedCommandSequence(), BeforeInvalidBatch);
	return !HasAnyErrors();
}
