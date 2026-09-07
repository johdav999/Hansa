#include "Misc/AutomationTest.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Save/HansaSaveEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveCommandVariantsTest, "Hansa.Integration.Save.PendingCommandVariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveCommandVariantsTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	auto Fixture = FHansaProductionFixture::TryCreate(); if (!Fixture) return false;
	const auto& D = Fixture.Value.GetDefinitions();
	FHansaSaveSnapshot S;
	S.State = Fixture.Value.GetState(); S.BuildVersion = TEXT("SaveCommandTest"); S.SavedUtc = TEXT("2026-09-06T00:00:00Z");
	const auto View = S.State.CreateReadOnlyAccess(D);
	S.Players.Add({1, View.GetHouses()[0].Id});
	FHansaCommandHeader H; H.Authority = {S.Players[0].HouseId, 1, EHansaCommandOrigin::PlayerInput};
	H.RequestedExecutionTick = View.GetClock().GetTick();
	auto Add = [&](const auto& P)
	{
		H.GlobalSequence = S.PendingCommands.Num() + 1;
		H.CommandId = FHansaCommandId::TryCreate(H.GlobalSequence).Value;
		S.PendingCommands.Add(FHansaGameplayCommand::Create(H, P));
	};
	const auto B = FHansaBuildingId::TryCreate(100, 7).Value;
	const auto R = FHansaRouteId::TryCreate(3).Value;
	Add(FHansaCreateTestEntityCommand{FHansaTestEntityId::TryCreate(12).Value, MIN_int64});
	Add(FHansaCancelTestEntityCommand{FHansaTestEntityId::TryCreate(12).Value});
	Add(FHansaNoOpTestCommand{MAX_int64});
	Add(FHansaSetProductionActiveCommand{FHansaProductionId::TryCreate(1).Value, false});
	FHansaPlacementSpec Placement;
	Placement.CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
	Placement.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Mill")).Value;
	Placement.Anchor = {-19, 23}; Placement.Rotation = EHansaGridRotation::West;
	Add(FHansaPlaceBuildingCommand{B, Placement});
	Add(FHansaCancelConstructionCommand{B}); Add(FHansaRemoveBuildingCommand{B}); Add(FHansaUpgradeResidenceCommand{B});
	FHansaRouteStop Stop; Stop.CityId = Placement.CityId;
	Stop.Actions.Add({EHansaRouteCargoActionKind::Unload, EHansaRouteCargoCondition::Always,
		FHansaGoodId::TryParse(TEXT("Good.Grain")).Value, FHansaQuantity::FromRaw(9317), FHansaQuantity::FromRaw(283)});
	FHansaCreateRouteCommand Create;
	Create.RouteId = R; Create.VehicleId = FHansaVehicleId::TryCreate(4).Value;
	Create.RouteDefinitionId = FHansaRouteDefinitionId::TryParse(TEXT("Route.Sea")).Value;
	Create.Stops.Add(Stop); Create.bActivate = true; Add(Create);
	Add(FHansaEditRouteCommand{R, {Stop}}); Add(FHansaSetRouteActiveCommand{R, false}); Add(FHansaCancelRouteCommand{R});
	Add(FHansaQueueResearchCommand{TEXT("Technology.ReserveAutomation")});
	TArray<uint8> Bytes; const auto Encoded = FHansaSaveEnvelope::Encode(S, D, Bytes);
	if (!TestTrue(*Encoded.Message, Encoded.IsSuccess())) return false;
	FHansaSaveSnapshot Loaded; const auto Decoded = FHansaSaveEnvelope::Decode(Bytes, D, Loaded);
	if (!TestTrue(*Decoded.Message, Decoded.IsSuccess())) return false;
	if (!TestEqual(TEXT("All closed command variants survive"), Loaded.PendingCommands.Num(), 13)) return false;
	for (int32 I = 0; I < 13; ++I)
		TestEqual(TEXT("Header and active payload fingerprint"), Loaded.PendingCommands[I].ComputeStableFingerprint(), S.PendingCommands[I].ComputeStableFingerprint());
	// The future command is serialized faithfully; execution still goes through normal payload/authority validation.
	Loaded.PendingCommands.Swap(0, 1);
	TestTrue(TEXT("Out-of-order queue rejected"), FHansaSaveEnvelope::Encode(Loaded, D, Bytes).Error == EHansaSaveError::InvalidSnapshot);
	return !HasAnyErrors();
}
#endif

