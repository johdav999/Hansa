#include "Algo/Reverse.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicImpact.h"
#include "Definitions/HansaPresenceDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presence/HansaForeignPresenceInitialization.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Systems/HansaSimulationPipeline.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/SHansaMarketTable.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Editor::Tests::TradePresence
{
	using namespace Hansa::Simulation;

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradePresenceDefinitionTest,
		"Hansa.Editor.TradePresence.DefinitionsDeterminismAndValidation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FHansaTradePresenceDefinitionTest::RunTest(const FString& Parameters)
	{
		auto Owned = EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		TArray<const UHansaDefinitionBase*> Forward;
		for (const auto& Value : Owned) Forward.Add(Value.Get());
		const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Forward);
		if (!TestTrue(TEXT("Authored presence catalog compiles"), Compiled.IsValid())) return false;
		TestTrue(TEXT("Transient authoring seed produces a nonzero catalog hash"), Compiled.Registry.GetRegistryHash() != 0);
		TestEqual(TEXT("Deterministic seed set includes Prompt 8 specialization capabilities"), Compiled.DefinitionHashes.Num(), 115);
		TestEqual(TEXT("Typed capability count"), Compiled.Registry.GetPresenceCapabilities().Num(), 17);
		TestEqual(TEXT("Stage count"), Compiled.Registry.GetPresenceStages().Num(), 6);
		TestEqual(TEXT("Current-city policy count"), Compiled.Registry.GetCityTradePolicies().Num(), 4);
		const auto* SpecializationPolicy = Compiled.Registry.FindCityTradePolicyForCity(TEXT("City.Rostock"));
		if (!TestNotNull(TEXT("Rostock specialization policy exists"), SpecializationPolicy)) return false;
		TestEqual(TEXT("Exactly three mutually exclusive merchant-office branches are authored"), SpecializationPolicy->Specializations.Num(), 3);
		const auto* Warehouse = SpecializationPolicy->Specializations.FindByPredicate([](const auto& V){ return V.SpecializationId == TEXT("Warehouse"); });
		const auto* Market = SpecializationPolicy->Specializations.FindByPredicate([](const auto& V){ return V.SpecializationId == TEXT("Market"); });
		const auto* Harbor = SpecializationPolicy->Specializations.FindByPredicate([](const auto& V){ return V.SpecializationId == TEXT("Harbor"); });
		TestTrue(TEXT("All three named branches exist"), Warehouse && Market && Harbor);
		if (Warehouse && Market && Harbor)
		{
			TestEqual(TEXT("Warehouse adds 75t storage"), Warehouse->StorageCapacityBonusMilliUnits, int64(75000));
			TestEqual(TEXT("Market adds four order slots"), Market->AdditionalOrderSlots, 4);
			TestEqual(TEXT("Harbor adds 50t handling cap"), Harbor->StationTransferCapBonusMilliUnits, int64(50000));
			TestTrue(TEXT("All branches share one exclusive group"), Warehouse->ExclusiveGroupId == Market->ExclusiveGroupId && Market->ExclusiveGroupId == Harbor->ExclusiveGroupId);
			TestTrue(TEXT("Respec is explicit and uniformly allowed"), Warehouse->bAllowRespec && Market->bAllowRespec && Harbor->bAllowRespec);
		}
		TestEqual(TEXT("Rostock authored opening"), Compiled.Registry.FindCityTradePolicyForCity(TEXT("City.Rostock"))->InitialStageId,
			FString(TEXT("PresenceStage.VisitingContact")));

		Algo::Reverse(Forward);
		const auto Reversed = FHansaEconomicDefinitionCompiler::Compile(Forward);
		TestTrue(TEXT("Reverse discovery compiles"), Reversed.IsValid());
		TestEqual(TEXT("Registry hash is discovery-order independent"), Reversed.Registry.GetRegistryHash(), Compiled.Registry.GetRegistryHash());

		FHansaEditorSchemaRegistry SchemaRegistry;
		const auto StageSchema=SchemaRegistry.BuildSchemaForClass(UHansaForeignPresenceStageDefinition::StaticClass());
		const auto PolicySchema=SchemaRegistry.BuildSchemaForClass(UHansaCityTradePolicyDefinition::StaticClass());
		const auto CapabilitySchema=SchemaRegistry.BuildSchemaForClass(UHansaPresenceCapabilityDefinition::StaticClass());
		TestTrue(TEXT("Presence stage schema is generic-editor ready"),StageSchema.IsValid());
		TestTrue(TEXT("City policy schema is generic-editor ready"),PolicySchema.IsValid());
        const auto RouteSchema=SchemaRegistry.BuildSchemaForClass(UHansaRouteDefinition::StaticClass());
        TestTrue(TEXT("Route target help exports through generic schema"),RouteSchema.IsValid());
        FFileHelper::SaveStringToFile(SchemaRegistry.ExportJsonSchema(RouteSchema),*(FPaths::ProjectSavedDir()/TEXT("TR06-Route.schema.json")));
        TestTrue(TEXT("City-policy impact includes physical route effects"),EconomicDefinitions::DescribeEconomicImpact(TEXT("CityTradePolicy.Rostock"),Forward).Contains(TEXT("City.Rostock.StationRoutes (RouteAccess/LocalStorage denial blocks transfers; physical cargo is preserved)")));
		TestTrue(TEXT("Capability schema is generic-editor ready"),CapabilitySchema.IsValid());
        FFileHelper::SaveStringToFile(SchemaRegistry.ExportJsonSchema(PolicySchema),*(FPaths::ProjectSavedDir()/TEXT("TR05-CityTradePolicy.schema.json")));
        TestTrue(TEXT("Order-policy impact identifies affected campaign city"),EconomicDefinitions::DescribeEconomicImpact(TEXT("CityTradePolicy.Rostock"),Forward).Contains(TEXT("City.Rostock.StationOrders (cap, budget, slot limits; active saves require migration review)")));
		TestEqual(TEXT("Presence JSON schema export is deterministic"),SchemaRegistry.ExportJsonSchema(StageSchema),SchemaRegistry.ExportJsonSchema(StageSchema));

		const auto GoodImpact = EconomicDefinitions::DescribeEconomicImpact(TEXT("Good.Planks"), Forward);
		TestTrue(TEXT("Upgrade material has reverse impact"), GoodImpact.Contains(TEXT("PresenceStage.TradeStation.UpgradeGoods")));
		const auto StageImpact = EconomicDefinitions::DescribeEconomicImpact(TEXT("PresenceStage.VisitingContact"), Forward);
		TestTrue(TEXT("Prerequisite has reverse impact"), StageImpact.Contains(TEXT("PresenceStage.TradeStation.PrerequisiteStageIds")));
		TestTrue(TEXT("Initial policy has reverse impact"), StageImpact.Contains(TEXT("CityTradePolicy.Rostock.InitialStageId")));
		auto* RostockPolicy = CastChecked<UHansaCityTradePolicyDefinition>(Owned.FindByPredicate([](const auto& Value)
		{
			return Value->StableDefinitionId == TEXT("CityTradePolicy.Rostock");
		})->Get());
        TestTrue(TEXT("Order limits exported to generic authoring schema"),SchemaRegistry.ExportJsonSchema(PolicySchema).Contains(TEXT("MaximumOrderCapMilliUnits")));
        const uint64 OriginalPolicyHash=RostockPolicy->ContentHash;
        RostockPolicy->MaximumOrderCapMilliUnits=0;RostockPolicy->RefreshContentHash();
        TestFalse(TEXT("Zero order cap is rejected"),FHansaEconomicDefinitionCompiler::Compile(Forward).IsValid());
        RostockPolicy->MaximumOrderCapMilliUnits=1000;RostockPolicy->RefreshContentHash();
        TestTrue(TEXT("Nondefault order limits affect definition identity"),RostockPolicy->ContentHash!=OriginalPolicyHash);
        RostockPolicy->MaximumOrderCapMilliUnits=50000;RostockPolicy->RefreshContentHash();
		RostockPolicy->TradeStationSites[0].StorageCapacityMilliUnits = 0;
		RostockPolicy->RefreshContentHash();
		Forward.Reset(); for (const auto& Value : Owned) Forward.Add(Value.Get());
		TestFalse(TEXT("Capacity-invalid station site is rejected by definition compilation"),
			FHansaEconomicDefinitionCompiler::Compile(Forward).IsValid());
		RostockPolicy->TradeStationSites[0].StorageCapacityMilliUnits = 50000;
		RostockPolicy->RefreshContentHash();

		auto* Station = Owned.FindByPredicate([](const auto& Value)
		{
			return Value->StableDefinitionId == TEXT("PresenceStage.TradeStation");
		})->Get();
		CastChecked<UHansaForeignPresenceStageDefinition>(Station)->PrerequisiteStageIds = {TEXT("PresenceStage.MerchantOffice")};
		Station->RefreshContentHash();
		Forward.Reset(); for (const auto& Value : Owned) Forward.Add(Value.Get());
		const auto Cycle = FHansaEconomicDefinitionCompiler::Compile(Forward);
		TestFalse(TEXT("Cyclic prerequisite graph is rejected"), Cycle.IsValid());
		return !HasAnyErrors();
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradePresenceStateTest,
		"Hansa.Integration.TradePresence.SeedQuerySaveAndInertTick",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FHansaTradePresenceStateTest::RunTest(const FString& Parameters)
	{
		auto Owned = EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		TArray<const UHansaDefinitionBase*> Raw; for (const auto& Value : Owned) Raw.Add(Value.Get());
		auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Raw);
		if (!TestTrue(TEXT("Catalog compiles"), Compiled.IsValid())) return false;
		const uint64 RegistryHash = Compiled.Registry.GetRegistryHash();
		auto Definitions = FHansaSimulationDefinitionContext::TryCreate(
			FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1" )).Value,
			RegistryHash, MoveTemp(Compiled.Registry)).Value;
		const FHansaEconomicRegistry* OwnedRegistry = Definitions.GetEconomicRegistry();
		if (!TestNotNull(TEXT("Definition context owns the compiled registry"), OwnedRegistry)) return false;
		const auto House = FHansaHouseId::TryCreate(1).Value;
		const auto Rostock = FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
		const auto Lubeck = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
		FHansaSimulationInitialization Initial;
		Initial.Clock = FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value, FHansaSimulationTick()).Value;
		Initial.CampaignSeed = 2202; Initial.Houses.Add({House, FHansaMoney::FromRaw(250000)});
		Initial.Cities.Add({Lubeck, FHansaQuantity::FromRaw(0)}); Initial.Cities.Add({Rostock, FHansaQuantity::FromRaw(0)});
		TestTrue(TEXT("Authored opening is seeded"), FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Initial, *OwnedRegistry));
		TestEqual(TEXT("Only explicit city opening is seeded"), Initial.ForeignPresences.Num(), 1);
		TestEqual(TEXT("Opening city is Rostock"), Initial.ForeignPresences[0].CityId, Rostock);
		TestEqual(TEXT("Opening stage"), Initial.ForeignPresences[0].CurrentStageId, FString(TEXT("PresenceStage.VisitingContact")));
		TestTrue(TEXT("Repeated seeding remains valid"), FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Initial, *OwnedRegistry));
		TestEqual(TEXT("Repeated seeding remains idempotent"), Initial.ForeignPresences.Num(), 1);

		auto StateResult = FHansaSimulationState::TryCreate(MoveTemp(Initial));
		if (!TestTrue(TEXT("Presence state validates"), StateResult.IsSuccess())) return false;
		FHansaSimulationState State = MoveTemp(StateResult.Value);
		auto Read = State.CreateReadOnlyAccess(Definitions);
		const auto Projection = Read.QueryForeignPresence(House, Rostock);
		if (!TestTrue(TEXT("House-city query resolves"), Projection.IsSet())) return false;
		TestEqual(TEXT("All-query returns one record"), Read.BuildForeignPresenceProjection().Num(), 1);
		TestTrue(TEXT("Current capability explains its grant"), Projection->Capabilities.ContainsByPredicate([](const auto& Value)
		{
			return Value.CapabilityId == TEXT("PresenceCapability.PublicMarketTrade") && Value.bGranted && !Value.Reason.IsEmpty();
		}));
		TestTrue(TEXT("Next stage exposes unmet requirements"), Projection->NextStages.Num() == 1 &&
			Projection->NextStages[0].Requirements.ContainsByPredicate([](const auto& Value){ return !Value.bMet; }));

		FHansaSaveSnapshot Snapshot; Snapshot.State = State; Snapshot.BuildVersion=TEXT("TR-02-Test");
		Snapshot.SavedUtc=TEXT("2026-09-20T00:00:00Z"); Snapshot.Players.Add({1,House});
		TArray<uint8> Bytes; const auto Encoded=FHansaSaveEnvelope::Encode(Snapshot,Definitions,Bytes);
		if (!TestTrue(*Encoded.Message,Encoded.IsSuccess())) return false;
		FHansaSaveSnapshot Loaded; const auto Decoded=FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded);
		if (!TestTrue(*Decoded.Message,Decoded.IsSuccess())) return false;
		TestEqual(TEXT("Presence survives save round trip"),Loaded.State.CreateReadOnlyAccess(Definitions).GetForeignPresences().Num(),1);

		const int64 MoneyBefore=Read.GetHouses()[0].Money.GetRawValue();
		const FString StageBefore=Read.GetForeignPresences()[0].CurrentStageId;
		FHansaSimulationTransientCache Cache;
		TestTrue(TEXT("Empty tick executes"),FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,{},Cache).IsSuccess());
		auto After=State.CreateReadOnlyAccess(Definitions);
		TestEqual(TEXT("Presence capability is economically inert"),After.GetHouses()[0].Money.GetRawValue(),MoneyBefore);
		TestEqual(TEXT("Presence stage does not advance implicitly"),After.GetForeignPresences()[0].CurrentStageId,StageBefore);
		return !HasAnyErrors();
	}
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaVisitingSpotTradeTest,
		"Hansa.Integration.TradePresence.VisitingSpotTrade",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FHansaVisitingSpotTradeTest::RunTest(const FString& Parameters)
	{
		auto Owned=EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		TArray<const UHansaDefinitionBase*> Raw; for(const auto& Value:Owned)Raw.Add(Value.Get());
		auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Raw); if(!TestTrue(TEXT("Catalog compiles"),Compiled.IsValid()))return false;
		auto Definitions=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value,
			Compiled.Registry.GetRegistryHash(),MoveTemp(Compiled.Registry)).Value;
		const auto* Registry=Definitions.GetEconomicRegistry(); const auto House=FHansaHouseId::TryCreate(1).Value;
		const auto Rostock=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
		const auto VehicleId=FHansaVehicleId::TryCreate(1).Value;const auto MarketInventory=FHansaInventoryId::TryCreate(1).Value;const auto CargoInventory=FHansaInventoryId::TryCreate(2).Value;
		FHansaSimulationInitialization Initial;Initial.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick()).Value;
		Initial.CampaignSeed=303;Initial.Houses.Add({House,FHansaMoney::FromRaw(100000)});Initial.Cities.Add({Rostock,{}});
		FHansaVehicleState Vehicle;Vehicle.Id=VehicleId;Vehicle.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;Vehicle.OwnerId=House;
		Vehicle.CargoInventoryId=CargoInventory;Vehicle.Mode=EHansaRouteMode::Sea;Vehicle.Capacity=FHansaQuantity::FromRaw(10000);Vehicle.CurrentCityId=Rostock;Initial.Vehicles.Add(Vehicle);
		FHansaInventoryInitialization CityStock;CityStock.Id=MarketInventory;CityStock.OwnerKind=EHansaInventoryOwnerKind::City;CityStock.CityId=Rostock;
		CityStock.Capacity=FHansaQuantity::FromRaw(50000);CityStock.AcceptedGoods.Add(Grain);CityStock.InitialStock.Add({Grain,FHansaQuantity::FromRaw(20000)});Initial.Inventories.Add(CityStock);
		FHansaInventoryInitialization Cargo;Cargo.Id=CargoInventory;Cargo.OwnerKind=EHansaInventoryOwnerKind::Vehicle;Cargo.VehicleId=VehicleId;
		Cargo.Capacity=FHansaQuantity::FromRaw(10000);Cargo.AcceptedGoods.Add(Grain);Cargo.InitialStock.Add({Grain,{}});Initial.Inventories.Add(Cargo);
		Initial.MarketSettings.UpdateCadenceTicks=5;Initial.MarketSettings.PriceHistoryCapacity=8;FHansaCityMarketInitialization Market;Market.CityId=Rostock;Market.GoodId=Grain;
		Market.InventoryIds.Add(MarketInventory);Market.DesiredReserve=FHansaQuantity::FromRaw(10000);Market.bMarketOnly=true;Market.MinimumPriceMilliMarks=100;Market.MaximumPriceMilliMarks=5000;
		Market.InitialPriceMilliMarks=1000;Market.InitialLastUpdateTick=0;Market.InitialReportTick=0;Initial.Markets.Add(Market);
		if(!TestTrue(TEXT("Visiting contact seeds"),FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Initial,*Registry)))return false;
		auto Created=FHansaSimulationState::TryCreate(MoveTemp(Initial));if(!TestTrue(TEXT("Spot state validates"),Created.IsSuccess()))return false;
		FHansaSimulationState State=MoveTemp(Created.Value);FHansaSimulationTransientCache Cache;
		auto Submit=[&](uint64 Id,uint64 Sequence,const FHansaSpotTradeCommand& Payload)
		{
			FHansaCommandHeader Header;Header.CommandId=FHansaCommandId::TryCreate(Id).Value;Header.GlobalSequence=Sequence;Header.Authority={House,1,EHansaCommandOrigin::ControlledAutomation};
			Header.RequestedExecutionTick=State.CreateReadOnlyAccess(Definitions).GetClock().GetTick();const auto Command=FHansaGameplayCommand::Create(Header,Payload);
			return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,MakeArrayView(&Command,1),Cache);
		};
		auto Quote=State.CreateReadOnlyAccess(Definitions).QuerySpotTradeQuote(House,VehicleId,Rostock,Grain,EHansaSpotTradeSide::BuyFromCity,FHansaQuantity::FromRaw(5000));
		if(!TestTrue(TEXT("Berthed Cog receives a quote"),Quote.bCanSubmit))return false;
		FHansaSpotTradeCommand Buy{VehicleId,Rostock,Grain,EHansaSpotTradeSide::BuyFromCity,FHansaQuantity::FromRaw(5000),Quote.ReviewedMarketUpdateTick,Quote.ReviewedUnitPriceMilliMarks};
		auto Bought=Submit(1,1,Buy);TestTrue(TEXT("Explicit buy settles"),Bought.IsSuccess());
		auto Read=State.CreateReadOnlyAccess(Definitions);auto Receipt=Read.QueryVehicle(VehicleId)->LastSpotTrade;
		TestEqual(TEXT("Buy completes"),Receipt.Outcome,EHansaSpotTradeOutcome::Completed);TestEqual(TEXT("Buy applies 5t"),Receipt.AppliedQuantity.GetRawValue(),int64(5000));
		TestEqual(TEXT("Buy charges 105 percent with friction"),Receipt.SettledMoneyRaw,int64(-5250));TestEqual(TEXT("Buy money conserved"),Read.GetHouses()[0].Money.GetRawValue(),int64(94750));
		TestEqual(TEXT("Buy goods conserved"),Read.GetInventories().QueryStock(MarketInventory,Grain)->Stock.GetRawValue()+Read.GetInventories().QueryStock(CargoInventory,Grain)->Stock.GetRawValue(),int64(20000));
		const uint64 BeforeStale=Read.GetFingerprint().Value;Buy.ReviewedUnitPriceMilliMarks++;
		auto Stale=Submit(2,2,Buy);TestEqual(TEXT("Changed review rejects"),Stale.GetError(),EHansaCommandGatewayError::SpotTradeStaleReview);
		TestEqual(TEXT("Stale rejection rolls back"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,BeforeStale);
		auto Duplicate=Submit(1,2,Buy);TestEqual(TEXT("Repeated command identity is idempotently rejected"),Duplicate.GetError(),EHansaCommandGatewayError::CommandIdentityOrderInvalid);
		auto SellQuote=State.CreateReadOnlyAccess(Definitions).QuerySpotTradeQuote(House,VehicleId,Rostock,Grain,EHansaSpotTradeSide::SellToCity,FHansaQuantity::FromRaw(10000));
		FHansaSpotTradeCommand Sell{VehicleId,Rostock,Grain,EHansaSpotTradeSide::SellToCity,FHansaQuantity::FromRaw(10000),SellQuote.ReviewedMarketUpdateTick,SellQuote.ReviewedUnitPriceMilliMarks};
		auto Sold=Submit(3,2,Sell);TestTrue(TEXT("Oversized sale is accepted partially"),Sold.IsSuccess());Read=State.CreateReadOnlyAccess(Definitions);Receipt=Read.QueryVehicle(VehicleId)->LastSpotTrade;
		TestEqual(TEXT("Sale is partial"),Receipt.Outcome,EHansaSpotTradeOutcome::Partial);TestEqual(TEXT("Sale blocker is carried stock"),Receipt.Blocker,EHansaSpotTradeBlocker::InsufficientShipStock);
		TestEqual(TEXT("Round trip loses only explicit friction"),Read.GetHouses()[0].Money.GetRawValue(),int64(99500));
		FHansaSaveSnapshot Snapshot;Snapshot.State=State;Snapshot.BuildVersion=TEXT("TR-03-Test");Snapshot.SavedUtc=TEXT("2026-09-20T00:00:00Z");Snapshot.Players.Add({1,House});
		TArray<uint8> Bytes;auto Encoded=FHansaSaveEnvelope::Encode(Snapshot,Definitions,Bytes);if(!TestTrue(*Encoded.Message,Encoded.IsSuccess()))return false;
		FHansaSaveSnapshot Loaded;auto Decoded=FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded);if(!TestTrue(*Decoded.Message,Decoded.IsSuccess()))return false;
		TestEqual(TEXT("Receipt survives save reload"),Loaded.State.CreateReadOnlyAccess(Definitions).QueryVehicle(VehicleId)->LastSpotTrade.CommandId.GetValue(),uint64(3));
		auto BuildConstraintState=[&](int64 MoneyRaw,int64 CityStockRaw,int64 CityCapacityRaw,int64 CargoStockRaw,int64 CargoCapacityRaw)
		{
			FHansaSimulationInitialization I;I.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick()).Value;I.CampaignSeed=404;
			I.Houses.Add({House,FHansaMoney::FromRaw(MoneyRaw)});I.Cities.Add({Rostock,{}});FHansaVehicleState V;V.Id=VehicleId;V.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;
			V.OwnerId=House;V.CargoInventoryId=CargoInventory;V.Mode=EHansaRouteMode::Sea;V.Capacity=FHansaQuantity::FromRaw(CargoCapacityRaw);V.CurrentCityId=Rostock;I.Vehicles.Add(V);
			FHansaInventoryInitialization M;M.Id=MarketInventory;M.OwnerKind=EHansaInventoryOwnerKind::City;M.CityId=Rostock;M.Capacity=FHansaQuantity::FromRaw(CityCapacityRaw);M.AcceptedGoods.Add(Grain);M.InitialStock.Add({Grain,FHansaQuantity::FromRaw(CityStockRaw)});I.Inventories.Add(M);
			FHansaInventoryInitialization C;C.Id=CargoInventory;C.OwnerKind=EHansaInventoryOwnerKind::Vehicle;C.VehicleId=VehicleId;C.Capacity=FHansaQuantity::FromRaw(CargoCapacityRaw);C.AcceptedGoods.Add(Grain);C.InitialStock.Add({Grain,FHansaQuantity::FromRaw(CargoStockRaw)});I.Inventories.Add(C);
			I.MarketSettings.UpdateCadenceTicks=5;I.MarketSettings.PriceHistoryCapacity=8;FHansaCityMarketInitialization MI;MI.CityId=Rostock;MI.GoodId=Grain;MI.InventoryIds.Add(MarketInventory);MI.DesiredReserve={};MI.bMarketOnly=true;
			MI.MinimumPriceMilliMarks=100;MI.MaximumPriceMilliMarks=5000;MI.InitialPriceMilliMarks=1000;MI.InitialLastUpdateTick=0;MI.InitialReportTick=0;I.Markets.Add(MI);
			FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(I,*Registry);return FHansaSimulationState::TryCreate(MoveTemp(I));
		};
		auto CheckConstraint=[&](const TCHAR* Label,int64 MoneyRaw,int64 CityStockRaw,int64 CityCapacityRaw,int64 CargoStockRaw,int64 CargoCapacityRaw,EHansaSpotTradeSide Side,int64 RequestRaw,EHansaSpotTradeBlocker Expected,int64 AppliedRaw)
		{
			auto Made=BuildConstraintState(MoneyRaw,CityStockRaw,CityCapacityRaw,CargoStockRaw,CargoCapacityRaw);if(!TestTrue(Label,Made.IsSuccess()))return;
			FHansaSimulationState S=MoveTemp(Made.Value);auto Q=S.CreateReadOnlyAccess(Definitions).QuerySpotTradeQuote(House,VehicleId,Rostock,Grain,Side,FHansaQuantity::FromRaw(RequestRaw));
			FHansaSpotTradeCommand P{VehicleId,Rostock,Grain,Side,FHansaQuantity::FromRaw(RequestRaw),Q.ReviewedMarketUpdateTick,Q.ReviewedUnitPriceMilliMarks};FHansaCommandHeader H;
			H.CommandId=FHansaCommandId::TryCreate(1).Value;H.GlobalSequence=1;H.Authority={House,1,EHansaCommandOrigin::ControlledAutomation};H.RequestedExecutionTick=S.CreateReadOnlyAccess(Definitions).GetClock().GetTick();
			const auto Command=FHansaGameplayCommand::Create(H,P);FHansaSimulationTransientCache LocalCache;const auto Result=FHansaGameplayCommandGateway::ExecuteTick(S,Definitions,MakeArrayView(&Command,1),LocalCache);
			TestTrue(FString(Label)+TEXT(" executes typed partial/missed outcome"),Result.IsSuccess());const auto R=S.CreateReadOnlyAccess(Definitions).QueryVehicle(VehicleId)->LastSpotTrade;
			TestEqual(FString(Label)+TEXT(" blocker"),R.Blocker,Expected);TestEqual(FString(Label)+TEXT(" applied quantity"),R.AppliedQuantity.GetRawValue(),AppliedRaw);
			const auto View=S.CreateReadOnlyAccess(Definitions);TestEqual(FString(Label)+TEXT(" goods conserve"),View.GetInventories().QueryStock(MarketInventory,Grain)->Stock.GetRawValue()+View.GetInventories().QueryStock(CargoInventory,Grain)->Stock.GetRawValue(),CityStockRaw+CargoStockRaw);
		};
		CheckConstraint(TEXT("Insufficient funds"),500,20000,50000,0,10000,EHansaSpotTradeSide::BuyFromCity,1000,EHansaSpotTradeBlocker::InsufficientFunds,476);
		CheckConstraint(TEXT("Insufficient market stock"),100000,500,50000,0,10000,EHansaSpotTradeSide::BuyFromCity,1000,EHansaSpotTradeBlocker::InsufficientMarketStock,500);
		CheckConstraint(TEXT("Insufficient ship capacity"),100000,20000,50000,9000,10000,EHansaSpotTradeSide::BuyFromCity,5000,EHansaSpotTradeBlocker::InsufficientShipCapacity,1000);
		CheckConstraint(TEXT("Insufficient market capacity"),100000,49000,50000,5000,10000,EHansaSpotTradeSide::SellToCity,5000,EHansaSpotTradeBlocker::InsufficientMarketCapacity,1000);
		CheckConstraint(TEXT("Missed empty-cargo sale"),100000,20000,50000,0,10000,EHansaSpotTradeSide::SellToCity,1000,EHansaSpotTradeBlocker::InsufficientShipStock,0);
		TStrongObjectPtr<UHansaMarketTablePresentationModel> UiModel(NewObject<UHansaMarketTablePresentationModel>());UiModel->InitializeDefaults();
		const auto UiProjection=State.CreateReadOnlyAccess(Definitions).BuildProjection();UiModel->ApplyProjection(UiProjection.Value,*Registry,Rostock);UiModel->SelectGoodIntent(TEXT("Good.Grain"));
		UiModel->SetSpotTradeTestContext(Rostock,VehicleId,[&](EHansaSpotTradeSide Side,FHansaQuantity Quantity){FHansaSpotTradeQuoteProjection Q;Q.bCanSubmit=true;Q.Side=Side;Q.RequestedQuantity=Q.EstimatedQuantity=Quantity;Q.ReviewedMarketUpdateTick=0;Q.ReviewedUnitPriceMilliMarks=Side==EHansaSpotTradeSide::BuyFromCity?1050:950;Q.EstimatedSettlementMoneyRaw=Quantity.GetRawValue()*Q.ReviewedUnitPriceMilliMarks/1000;return Q;});
		bool bSubmitted=false;FHansaClientCommandIntent Submitted;UiModel->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Intent){bSubmitted=true;Submitted=Intent;return true;});
		auto Ui=SNew(Hansa::UI::SHansaMarketTable).Model(UiModel.Get());TestTrue(TEXT("Spot confirmation is semantic"),Ui->GetSemanticSnapshot().ContainsByPredicate([](const auto& N){return N.Id==TEXT("Market.Detail.SpotTrade.Confirm");}));
		const int64 BeforeQuantity=UiModel->GetSnapshot().SelectedGood.SpotTradeQuantityRaw;TestTrue(TEXT("Ordinary input increases quantity"),Ui->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Quantity.Increase")));
		TestEqual(TEXT("Quantity intent refreshes reviewed quote"),UiModel->GetSnapshot().SelectedGood.SpotTradeQuantityRaw,BeforeQuantity+1000);TestTrue(TEXT("Ordinary input changes side"),Ui->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Side")));
		TestTrue(TEXT("Ordinary input confirms reviewed command"),Ui->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.Confirm")));TestTrue(TEXT("Confirmation reaches command intent"),bSubmitted);
		TestEqual(TEXT("Semantic command carries quantity"),Submitted.QuantityMilliUnits,BeforeQuantity+1000);TestFalse(TEXT("Semantic command carries selected sell side"),Submitted.bSpotTradeBuy);
		return !HasAnyErrors();
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeStationLifecycleTest,
		"Hansa.Integration.TradePresence.EstablishTradeStation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FHansaTradeStationLifecycleTest::RunTest(const FString& Parameters)
	{
		auto Owned=EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());TArray<const UHansaDefinitionBase*> Raw;for(const auto& V:Owned)Raw.Add(V.Get());
		auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Raw);if(!TestTrue(TEXT("Station catalog compiles"),Compiled.IsValid()))return false;
		auto Definitions=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value,Compiled.Registry.GetRegistryHash(),MoveTemp(Compiled.Registry)).Value;
		const auto* Registry=Definitions.GetEconomicRegistry();const auto House=FHansaHouseId::TryCreate(1).Value;const auto Rival=FHansaHouseId::TryCreate(2).Value;const auto Rostock=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
		const auto VehicleId=FHansaVehicleId::TryCreate(1).Value;const auto Funding=FHansaInventoryId::TryCreate(1).Value;const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;const auto Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
		FHansaSimulationInitialization Initial;Initial.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick()).Value;Initial.CampaignSeed=40404;
		Initial.Houses.Add({House,FHansaMoney::FromRaw(200000)});Initial.Houses.Add({Rival,FHansaMoney::FromRaw(200000)});Initial.Cities.Add({Rostock,{}});
		FHansaVehicleState Vehicle;Vehicle.Id=VehicleId;Vehicle.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;Vehicle.OwnerId=House;Vehicle.CargoInventoryId=Funding;Vehicle.Mode=EHansaRouteMode::Sea;Vehicle.Capacity=FHansaQuantity::FromRaw(30000);Vehicle.CurrentCityId=Rostock;Initial.Vehicles.Add(Vehicle);
		FHansaInventoryInitialization Cargo;Cargo.Id=Funding;Cargo.OwnerKind=EHansaInventoryOwnerKind::Vehicle;Cargo.VehicleId=VehicleId;Cargo.Capacity=Vehicle.Capacity;Cargo.AcceptedGoods={Timber,Planks};Cargo.InitialStock={{Timber,FHansaQuantity::FromRaw(8000)},{Planks,FHansaQuantity::FromRaw(4000)}};Initial.Inventories.Add(Cargo);
		if(!TestTrue(TEXT("Visiting contact seeds"),FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Initial,*Registry)))return false;
		// Qualify through trade, with no prior station investment or station inventory.
		Initial.ForeignPresences[0].Contributions={50000,3,0,50000,10000,3,3};auto Made=FHansaSimulationState::TryCreate(MoveTemp(Initial));if(!TestTrue(TEXT("Station fixture validates"),Made.IsSuccess()))return false;
		FHansaSimulationState State=MoveTemp(Made.Value);FHansaSimulationTransientCache Cache;uint64 Next=1;
		TStrongObjectPtr<UHansaTradeMapPresentationModel> ProposalUi(NewObject<UHansaTradeMapPresentationModel>());ProposalUi->InitializeDefaults();
		ProposalUi->ApplyProjection(State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Registry);ProposalUi->Open();
		TestTrue(TEXT("Qualified visiting contact can propose without nonexistent station stock"),ProposalUi->GetSnapshot().bCanTradeStationAction);
		TestFalse(TEXT("Station establishment is not advertised as an office upgrade"),ProposalUi->GetSnapshot().bCanPresenceUpgradeAction);
		auto Submit=[&](FHansaHouseId Issuer,const auto& Payload){FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(Next).Value;H.GlobalSequence=Next++;H.Authority={Issuer,Issuer==House?uint64(1):uint64(2),EHansaCommandOrigin::ControlledAutomation};H.RequestedExecutionTick=State.CreateReadOnlyAccess(Definitions).GetClock().GetTick();const auto C=FHansaGameplayCommand::Create(H,Payload);return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,MakeArrayView(&C,1),Cache);};
		const auto Station=FHansaTradeStationId::TryCreate(1).Value;
		const auto Factor=FHansaFactorId::TryCreate(1).Value;
		const auto Lease=FHansaLeasedPlotId::TryCreate(1).Value;
		const auto Storage=FHansaInventoryId::TryCreate(2).Value;
		FHansaProposeTradeStationCommand Proposal{Station,Factor,Lease,Storage,Rostock,TEXT("TradeStationSite.Rostock.Harbor.01")};
		TestEqual(TEXT("Wrong house cannot establish another house's qualified presence"),Submit(Rival,Proposal).GetError(),EHansaCommandGatewayError::TradeStationAccessUnavailable);
		FHansaProposeTradeStationCommand InvalidSite{FHansaTradeStationId::TryCreate(9).Value,FHansaFactorId::TryCreate(9).Value,FHansaLeasedPlotId::TryCreate(9).Value,FHansaInventoryId::TryCreate(9).Value,Rostock,TEXT("TradeStationSite.Rostock.Unknown")};
		TestEqual(TEXT("Unauthored station site is rejected without mutation"),Submit(House,InvalidSite).GetError(),EHansaCommandGatewayError::TradeStationSiteUnavailable);
		Proposal.StationId=FHansaTradeStationId::TryCreate(2).Value;Proposal.FactorId=FHansaFactorId::TryCreate(2).Value;Proposal.LeasedPlotId=FHansaLeasedPlotId::TryCreate(2).Value;Proposal.InventoryId=FHansaInventoryId::TryCreate(3).Value;
		TestTrue(TEXT("Ordinary proposal reserves authored site"),Submit(House,Proposal).IsSuccess());auto Read=State.CreateReadOnlyAccess(Definitions);TestEqual(TEXT("Proposal creates empty station inventory"),Read.GetInventories().QueryInventory(Proposal.InventoryId)->UsedCapacity.GetRawValue(),int64(0));
		FHansaProposeTradeStationCommand Conflict{FHansaTradeStationId::TryCreate(3).Value,FHansaFactorId::TryCreate(3).Value,FHansaLeasedPlotId::TryCreate(3).Value,FHansaInventoryId::TryCreate(4).Value,Rostock,Proposal.SiteId};
		TestEqual(TEXT("Occupied site is rejected deterministically"),Submit(House,Conflict).GetError(),EHansaCommandGatewayError::TargetAlreadyExists);
		TestEqual(TEXT("Unowned funding inventory is rejected"),Submit(House,FHansaFundTradeStationCommand{Proposal.StationId,Proposal.InventoryId}).GetError(),EHansaCommandGatewayError::TradeStationCostUnavailable);
		const int64 MoneyBefore=State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue();TestTrue(TEXT("Funding consumes authored cost"),Submit(House,FHansaFundTradeStationCommand{Proposal.StationId,Funding}).IsSuccess());
		Read=State.CreateReadOnlyAccess(Definitions);TestEqual(TEXT("Funding money cost applied"),Read.GetHouses()[0].Money.GetRawValue(),MoneyBefore-50000);TestEqual(TEXT("Timber cost has no duplication"),Read.GetInventories().QueryStock(Funding,Timber)->Stock.GetRawValue(),int64(0));TestEqual(TEXT("Plank cost has no duplication"),Read.GetInventories().QueryStock(Funding,Planks)->Stock.GetRawValue(),int64(0));
		for(int32 I=0;I<2;++I)TestTrue(TEXT("Construction advances"),FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,{},Cache).IsSuccess());
		Read=State.CreateReadOnlyAccess(Definitions);const auto StationProjection=Read.QueryTradeStation(Proposal.StationId);if(!TestTrue(TEXT("Completed station is inspectable"),StationProjection.IsSet()))return false;
		TestEqual(TEXT("Station becomes active"),StationProjection->Station.Status,EHansaTradeStationStatus::Active);TestEqual(TEXT("Presence advances to station stage"),Read.QueryForeignPresence(House,Rostock)->CurrentStageId,FString(TEXT("PresenceStage.TradeStation")));TestEqual(TEXT("Station storage remains empty"),StationProjection->StorageUsed.GetRawValue(),int64(0));TestTrue(TEXT("Approved world presentation is authored"),StationProjection->PresentationClassPath.Contains(TEXT("BP_Harbor_Review")));
		FHansaSaveSnapshot Snapshot;Snapshot.State=State;Snapshot.BuildVersion=TEXT("TR-04-Test");Snapshot.SavedUtc=TEXT("2026-09-20T00:00:00Z");Snapshot.Players={{1,House},{2,Rival}};TArray<uint8> Bytes;auto Encoded=FHansaSaveEnvelope::Encode(Snapshot,Definitions,Bytes);if(!TestTrue(*Encoded.Message,Encoded.IsSuccess()))return false;
		FHansaSaveSnapshot Loaded;auto Decoded=FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded);if(!TestTrue(*Decoded.Message,Decoded.IsSuccess()))return false;TestEqual(TEXT("Station survives save/reload"),Loaded.State.CreateReadOnlyAccess(Definitions).QueryTradeStation(Proposal.StationId)->Station.FactorId,Proposal.FactorId);
		TestTrue(TEXT("Empty completed station closes voluntarily"),Submit(House,FHansaCloseTradeStationCommand{Proposal.StationId}).IsSuccess());Read=State.CreateReadOnlyAccess(Definitions);TestEqual(TEXT("Closure releases lease"),Read.GetLeasedPlots()[0].bOccupied,false);TestEqual(TEXT("Closure returns presence to authored opening"),Read.QueryForeignPresence(House,Rostock)->CurrentStageId,FString(TEXT("PresenceStage.VisitingContact")));
		TestFalse(TEXT("Closure clears active station reference"),Read.QueryForeignPresence(House,Rostock)->StationId.IsValid());TestFalse(TEXT("Closure clears active lease reference"),Read.QueryForeignPresence(House,Rostock)->LeasedPlotId.IsValid());
		FHansaProposeTradeStationCommand Reopen{FHansaTradeStationId::TryCreate(4).Value,FHansaFactorId::TryCreate(4).Value,FHansaLeasedPlotId::TryCreate(4).Value,FHansaInventoryId::TryCreate(4).Value,Rostock,Proposal.SiteId};
		TestTrue(TEXT("Released site can be established again with new stable identities"),Submit(House,Reopen).IsSuccess());
		TStrongObjectPtr<UHansaTradeMapPresentationModel> UiModel(NewObject<UHansaTradeMapPresentationModel>());UiModel->InitializeDefaults();UiModel->ApplyProjection(Loaded.State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Registry);UiModel->Open();auto Ui=SNew(Hansa::UI::SHansaTradeMap).Model(UiModel.Get());TestTrue(TEXT("Station action is a semantic ordinary control"),Ui->GetSemanticSnapshot().ContainsByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Station.Action");}));
		TStrongObjectPtr<UHansaInspectorPresentationModel> StationInspector(NewObject<UHansaInspectorPresentationModel>());StationInspector->InitializeDefaults();
		const auto LoadedProjection=Loaded.State.CreateReadOnlyAccess(Definitions).BuildProjection();
		const auto* LoadedStation=LoadedProjection.Value.GetTradeStations().FindByPredicate([&](const auto& V){return V.Station.Id==Proposal.StationId;});
		const auto* LoadedPresence=LoadedProjection.Value.GetForeignPresences().FindByPredicate([&](const auto& V){return V.HouseId==House&&V.CityId==Rostock;});
		if(!TestNotNull(TEXT("Station projection feeds world inspector"),LoadedStation)||!TestNotNull(TEXT("Presence projection feeds world inspector"),LoadedPresence))return false;
		StationInspector->ShowTradeStation(*LoadedStation,*LoadedPresence,TEXT("World.Selection.TradeStation"));
		TestEqual(TEXT("Station inspector has dedicated kind"),StationInspector->GetSnapshot().Kind,EHansaInspectorObjectKind::TradeStation);
		TestTrue(TEXT("Station inspector exposes physical storage"),StationInspector->GetSnapshot().Flows.ContainsByPredicate([](const auto& V){return V.StableId==TEXT("Inspector.Station.Storage");}));
		TestTrue(TEXT("Station inspector links to station controls"),StationInspector->GetSnapshot().Causal.RelatedSemanticId==TEXT("TradeMap.Station.Action"));
		return !HasAnyErrors();
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantOfficeProgressionTest,
		"Hansa.Integration.TradePresence.MerchantOfficeProgression",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FHansaMerchantOfficeProgressionTest::RunTest(const FString& Parameters)
	{
		auto Owned=EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());TArray<const UHansaDefinitionBase*> Raw;for(const auto& V:Owned)Raw.Add(V.Get());
		auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Raw);if(!TestTrue(TEXT("Progression catalog compiles"),Compiled.IsValid()))return false;
		auto Definitions=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value,Compiled.Registry.GetRegistryHash(),MoveTemp(Compiled.Registry)).Value;
		const auto* Registry=Definitions.GetEconomicRegistry();const auto House=FHansaHouseId::TryCreate(1).Value;const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
		const auto StationId=FHansaTradeStationId::TryCreate(1).Value;const auto InventoryId=FHansaInventoryId::TryCreate(1).Value;const auto LeaseId=FHansaLeasedPlotId::TryCreate(1).Value;
		const auto Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;const auto Tools=FHansaGoodId::TryParse(TEXT("Good.Tools")).Value;
		FHansaSimulationInitialization Initial;Initial.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick()).Value;Initial.CampaignSeed=70707;Initial.Houses.Add({House,FHansaMoney::FromRaw(500000)});Initial.Cities.Add({City,{}});
		const auto* TradeStage=Registry->FindPresenceStage(TEXT("PresenceStage.TradeStation"));if(!TestNotNull(TEXT("Trade-station stage exists"),TradeStage))return false;
		FHansaForeignPresenceState Presence;Presence.HouseId=House;Presence.CityId=City;Presence.CurrentStageId=TradeStage->StableId;Presence.GrantedCapabilityIds=TradeStage->GrantedCapabilityIds;Presence.GrantedCapabilityIds.Sort();Presence.StationId=StationId;Presence.LeasedPlotId=LeaseId;
		Presence.Contributions.LawfulTradeVolumeMilliUnits=250000;Presence.Contributions.CompletedDeliveryCount=12;Presence.Contributions.InvestedPfennig=25000;Presence.Contributions.TransactionValuePfennig=250000;Presence.Contributions.FulfilledShortageMilliUnits=50000;Presence.Contributions.ReliableOperatingTicks=12;Presence.Contributions.SolventOperatingTicks=8;Initial.ForeignPresences.Add(Presence);
		FHansaTradeStationState Station;Station.Id=StationId;Station.OwnerId=House;Station.CityId=City;Station.SiteId=TEXT("TradeStationSite.Rostock.Harbor.01");Station.InventoryId=InventoryId;Station.FactorId=FHansaFactorId::TryCreate(1).Value;Station.LeasedPlotId=LeaseId;Station.Status=EHansaTradeStationStatus::Active;Station.UpkeepPfennigPerTick=25;Initial.TradeStations.Add(Station);
		FHansaLeasedPlotState Lease;Lease.Id=LeaseId;Lease.StationId=StationId;Lease.OwnerId=House;Lease.CityId=City;Lease.SiteId=Station.SiteId;Lease.PlotCategory=TEXT("Commercial");Lease.BoundsMin={8,8};Lease.BoundsMax={19,19};Lease.PermittedBuildingCategories={TEXT("Storage"),TEXT("Commercial"),TEXT("Production")};Lease.bActive=true;Lease.bOccupied=true;Initial.LeasedPlots.Add(Lease);
		FHansaInventoryInitialization Storage;Storage.Id=InventoryId;Storage.OwnerKind=EHansaInventoryOwnerKind::TradeStation;Storage.CityId=City;Storage.TradeStationId=StationId;Storage.Capacity=FHansaQuantity::FromRaw(50000);Storage.AcceptedGoods={Planks,Tools};Storage.InitialStock={{Planks,FHansaQuantity::FromRaw(30000)},{Tools,FHansaQuantity::FromRaw(7000)}};Initial.Inventories.Add(Storage);
		auto Made=FHansaSimulationState::TryCreate(MoveTemp(Initial));if(!TestTrue(TEXT("Progression fixture validates"),Made.IsSuccess()))return false;FHansaSimulationState State=MoveTemp(Made.Value);FHansaSimulationTransientCache Cache;uint64 Sequence=1;
		auto Submit=[&](const auto& Payload){FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(Sequence).Value;H.GlobalSequence=Sequence++;H.Authority={House,1,EHansaCommandOrigin::ControlledAutomation};H.RequestedExecutionTick=State.CreateReadOnlyAccess(Definitions).GetClock().GetTick();const auto C=FHansaGameplayCommand::Create(H,Payload);return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,MakeArrayView(&C,1),Cache);};
		const FString Office=TEXT("PresenceStage.MerchantOffice");auto Before=State.CreateReadOnlyAccess(Definitions).QueryForeignPresence(House,City);TestTrue(TEXT("Exact requirements are queryable"),Before.IsSet()&&!Before->NextStages.IsEmpty()&&Before->NextStages[0].bProgressRequirementsMet&&Before->NextStages[0].bFundingAvailable);
		TStrongObjectPtr<UHansaTradeMapPresentationModel> UpgradeUi(NewObject<UHansaTradeMapPresentationModel>());UpgradeUi->InitializeDefaults();UpgradeUi->ApplyProjection(State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Registry);UpgradeUi->Open();
		auto UpgradeScreen=SNew(Hansa::UI::SHansaTradeMap).Model(UpgradeUi.Get());
		TestTrue(TEXT("Presence tab opens through ordinary input"),UpgradeScreen->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence")));
		TestTrue(TEXT("Available office upgrade participates in controller navigation"),UpgradeScreen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Presence.Upgrade")));
		TestTrue(TEXT("Available office upgrade accepts keyboard focus"),UpgradeScreen->FocusSemanticId(TEXT("TradeMap.Presence.Upgrade")));
		TestTrue(TEXT("Review request is accepted"),Submit(FHansaRequestPresenceUpgradeCommand{City,Office}).IsSuccess());const uint64 RequestedHash=State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value;
		TestEqual(TEXT("Repeated request is rejected"),Submit(FHansaRequestPresenceUpgradeCommand{City,Office}).GetError(),EHansaCommandGatewayError::PresenceUpgradeUnavailable);TestEqual(TEXT("Rejected request does not mutate"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,RequestedHash);
		const int64 MoneyBefore=State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue();TestTrue(TEXT("Funding is atomic"),Submit(FHansaFundPresenceUpgradeCommand{City,Office,InventoryId}).IsSuccess());auto Read=State.CreateReadOnlyAccess(Definitions);TestEqual(TEXT("Authored money and one station-upkeep tick consumed"),Read.GetHouses()[0].Money.GetRawValue(),MoneyBefore-150025);TestEqual(TEXT("Planks consumed once"),Read.GetInventories().QueryStock(InventoryId,Planks)->Stock.GetRawValue(),int64(18000));TestEqual(TEXT("Tools consumed once"),Read.GetInventories().QueryStock(InventoryId,Tools)->Stock.GetRawValue(),int64(5000));
		FHansaSaveSnapshot Snapshot;Snapshot.State=State;Snapshot.BuildVersion=TEXT("TR-07-Test");Snapshot.SavedUtc=TEXT("2026-09-20T00:00:00Z");Snapshot.Players={{1,House}};TArray<uint8> Bytes;TestTrue(TEXT("Funded progression saves"),FHansaSaveEnvelope::Encode(Snapshot,Definitions,Bytes).IsSuccess());FHansaSaveSnapshot Loaded;TestTrue(TEXT("Funded progression reloads"),FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded).IsSuccess());State=MoveTemp(Loaded.State);
		for(int32 I=0;I<3;++I)TestTrue(TEXT("Office construction advances"),FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,{},Cache).IsSuccess());Read=State.CreateReadOnlyAccess(Definitions);const auto After=Read.QueryForeignPresence(House,City);if(!TestTrue(TEXT("Completed office is queryable"),After.IsSet()))return false;
		TestEqual(TEXT("Stage advances exactly once"),After->CurrentStageId,Office);TestTrue(TEXT("Only authored office capability is granted"),After->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.MerchantOffice")&&C.bGranted;}));TestEqual(TEXT("Station identity preserved"),After->StationId,StationId);TestEqual(TEXT("Storage expands from authored city policy"),Read.QueryTradeStation(StationId)->StorageCapacity.GetRawValue(),int64(100000));TestTrue(TEXT("History preserves request, funding and completion"),After->History.Num()>=3);
		const uint64 BeforeUnavailable=Read.GetFingerprint().Value;FHansaApplyPresenceSpecializationCommand MissingResources{City,TEXT("Market"),FHansaInventoryId::TryCreate(999).Value,EHansaPresenceSpecializationAction::Select,After->SpecializationRevision};TestEqual(TEXT("Unavailable branch funding resources reject"),Submit(MissingResources).GetError(),EHansaCommandGatewayError::PresenceSpecializationCostUnavailable);TestEqual(TEXT("Resource rejection rolls back exactly"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,BeforeUnavailable);
		FHansaApplyPresenceSpecializationCommand ChooseMarket{City,TEXT("Market"),InventoryId,EHansaPresenceSpecializationAction::Select,After->SpecializationRevision};
		const int64 BeforeBranchMoney=Read.GetHouses()[0].Money.GetRawValue();
		TestTrue(TEXT("Market specialization applies atomically"),Submit(ChooseMarket).IsSuccess());
		Read=State.CreateReadOnlyAccess(Definitions);auto Specialized=Read.QueryForeignPresence(House,City);
		TestEqual(TEXT("Specialization revision advances"),Specialized->SpecializationRevision,int64(1));
		TestTrue(TEXT("Market capability is granted"),Specialized->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.MarketSpecialization")&&C.bGranted;}));
		TestEqual(TEXT("Market cost and one station-upkeep tick are charged once"),Read.GetHouses()[0].Money.GetRawValue(),BeforeBranchMoney-75025);
		const uint64 SpecializedHash=Read.GetFingerprint().Value;
		FHansaApplyPresenceSpecializationCommand StaleHarbor{City,TEXT("Harbor"),InventoryId,EHansaPresenceSpecializationAction::Respec,0};
		TestEqual(TEXT("Stale branch review is rejected"),Submit(StaleHarbor).GetError(),EHansaCommandGatewayError::PresenceSpecializationStaleReview);
		TestEqual(TEXT("Stale rejection rolls back exactly"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,SpecializedHash);
		FHansaApplyPresenceSpecializationCommand ExclusiveHarbor{City,TEXT("Harbor"),InventoryId,EHansaPresenceSpecializationAction::Select,1};
		TestEqual(TEXT("A second exclusive selection is rejected"),Submit(ExclusiveHarbor).GetError(),EHansaCommandGatewayError::PresenceSpecializationStateInvalid);
		const int64 BeforeRespecMoney=State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue();
		ExclusiveHarbor.Action=EHansaPresenceSpecializationAction::Respec;
		TestTrue(TEXT("Explicit respec applies atomically"),Submit(ExclusiveHarbor).IsSuccess());
		Read=State.CreateReadOnlyAccess(Definitions);Specialized=Read.QueryForeignPresence(House,City);
		TestEqual(TEXT("Respec revision advances"),Specialized->SpecializationRevision,int64(2));
		TestTrue(TEXT("Harbor replaces Market"),Specialized->Specializations.ContainsByPredicate([](const auto& B){return B.SpecializationId==TEXT("Harbor")&&B.bSelected;})&&!Specialized->Specializations.ContainsByPredicate([](const auto& B){return B.SpecializationId==TEXT("Market")&&B.bSelected;}));
		TestEqual(TEXT("Respec refunds 25 percent of prior money only after station upkeep"),Read.GetHouses()[0].Money.GetRawValue(),BeforeRespecMoney-110000+18750-25);
		FHansaSaveSnapshot BranchSave;BranchSave.State=State;BranchSave.BuildVersion=TEXT("TR-08-Test");BranchSave.SavedUtc=TEXT("2026-09-20T00:00:00Z");BranchSave.Players={{1,House}};TArray<uint8> BranchBytes;const auto BranchEncoded=FHansaSaveEnvelope::Encode(BranchSave,Definitions,BranchBytes);if(!TestTrue(*BranchEncoded.Message,BranchEncoded.IsSuccess()))return false;FHansaSaveSnapshot BranchLoaded;const auto BranchDecoded=FHansaSaveEnvelope::Decode(BranchBytes,Definitions,BranchLoaded);if(!TestTrue(*BranchDecoded.Message,BranchDecoded.IsSuccess()))return false;TestEqual(TEXT("Reload preserves selected branch"),BranchLoaded.State.CreateReadOnlyAccess(Definitions).QueryForeignPresence(House,City)->SpecializationRevision,int64(2));
		TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->ApplyProjection(Read.BuildProjection().Value,*Registry);Model->Open();auto Ui=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());const auto Semantics=Ui->GetSemanticSnapshot();TestTrue(TEXT("Presence progression is semantic"),Semantics.ContainsByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Presence.Progress");}));TestTrue(TEXT("Presence upgrade action is semantic"),Semantics.ContainsByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Presence.Upgrade");}));
		for(const TCHAR* Id:{TEXT("TradeMap.Presence.Specialization.Warehouse"),TEXT("TradeMap.Presence.Specialization.Market"),TEXT("TradeMap.Presence.Specialization.Harbor"),TEXT("TradeMap.Presence.Specialization.Apply")})
		{
			const bool bExpectedEnabled=Model->CanPresenceSpecializationIntent(FString(Id).RightChop(33));
			Ui->ActivateSemanticId(TEXT("TradeMap.Navigate.Specialization"));const auto TabSemantics=Ui->GetSemanticSnapshot();
			TestTrue(FString::Printf(TEXT("%s semantics respect actual action availability"),Id),TabSemantics.ContainsByPredicate([&](const auto& N){return N.Id==Id&&N.State.bEnabled==bExpectedEnabled&&N.bCanFocus==bExpectedEnabled&&N.bCanActivate==bExpectedEnabled;}));
			TestTrue(FString::Printf(TEXT("%s participates in its tab controller focus order"),Id),Ui->GetControllerFocusOrder().Contains(Id));
		}
		TestTrue(TEXT("Ordinary input can compare Warehouse"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Specialization.Warehouse")));
		TestEqual(TEXT("Ordinary specialization input updates the reviewed choice"),Model->GetSnapshot().SelectedPresenceSpecializationId,FString(TEXT("Warehouse")));
		Ui->SetPresentationSize(FIntPoint(1280,720));TestTrue(TEXT("Compact specialization comparison remains semantic and focusable"),Model->GetSnapshot().bCompact&&Ui->GetControllerFocusOrder().Contains(TEXT("TradeMap.Presence.Specialization.Apply")));
		Ui->SetPresentationSize(FIntPoint(1920,1080));TestTrue(TEXT("Wide specialization comparison remains semantic and focusable"),!Model->GetSnapshot().bCompact&&Ui->GetControllerFocusOrder().Contains(TEXT("TradeMap.Presence.Specialization.Apply")));
		return !HasAnyErrors();
	}
}

#include "HansaPrivilegeProjectTests.inl"
#include "HansaLeasedConstructionTests.inl"
#endif
