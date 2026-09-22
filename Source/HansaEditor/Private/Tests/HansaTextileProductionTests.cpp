#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/HansaTextileProductionDraft.h"
#include "Definitions/HansaArtisanProductionDraft.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"
#include "UI/HansaBuildMenuPresentationModel.h"

using namespace Hansa::Simulation;

namespace
{
template<class T> T Entity(const uint64 Value) { return T::TryCreate(Value).Value; }
template<class T> T Stable(const TCHAR* Value) { return T::TryParse(Value).Value; }

struct FTextileCatalog
{
	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions;
	FHansaEconomicRegistry Registry;
	FHansaSimulationDefinitionContext Context;
};

bool MakeCatalog(FAutomationTestBase& Test, FTextileCatalog& Out)
{
	Out.Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	FString Error;
	if (!Test.TestTrue(TEXT("Accepted artisan layer applies to the programmatic fixture"), Hansa::Editor::ArtisanProduction::ApplyDraft(Out.Definitions, Error)))
	{
		Test.AddError(Error);
		return false;
	}
	const int32 BaselineCount = Out.Definitions.Num();
	Error.Reset();
	if (!Test.TestTrue(TEXT("Textile draft applies"), Hansa::Editor::TextileProduction::ApplyDraft(Out.Definitions, Error)))
	{
		Test.AddError(Error);
		return false;
	}
	Test.TestEqual(TEXT("Seven goods, five recipes, four buildings and two needs are added"), Out.Definitions.Num(), BaselineCount + 18);
	TArray<const UHansaDefinitionBase*> Pointers;
	for (const auto& Definition : Out.Definitions) Pointers.Add(Definition.Get());
	const auto Compiled = FHansaEconomicDefinitionCompiler::Compile(Pointers);
	for (const auto& Issue : Compiled.Issues)
	{
		if (Issue.Severity == EHansaDefinitionValidationSeverity::Error)
			Test.AddError(Issue.PropertyPath + TEXT(": ") + Issue.Cause.ToString());
	}
	if (!Test.TestTrue(TEXT("Textile catalog compiles"), Compiled.IsValid())) return false;
	Out.Registry = Compiled.Registry;
	const auto Context = FHansaSimulationDefinitionContext::TryCreate(
		Stable<FHansaScenarioId>(TEXT("Scenario.TextileProduction")), Out.Registry.GetRegistryHash(), Out.Registry);
	if (!Test.TestTrue(TEXT("Textile definition context is valid"), Context.IsSuccess())) return false;
	Out.Context = Context.Value;
	return true;
}

FHansaSimulationInitialization OneWorkshop(
	const FHansaEconomicRegistry& Registry, const TCHAR* BuildingId, const TCHAR* RecipeId,
	const bool bSupplyInput, const bool bSupplyWorkforce, const bool bSeparateBlockedOutput = false)
{
	const auto* Recipe = Registry.FindRecipe(RecipeId);
	FHansaSimulationInitialization Init;
	Init.Clock = FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value, FHansaSimulationTick::TryCreate(0).Value).Value;
	Init.CampaignSeed = 1919;
	Init.Houses.Add({Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(100000)});
	Init.Cities.Add({Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck")), FHansaQuantity()});
	Init.Buildings.Add({Entity<FHansaBuildingId>(1), Stable<FHansaBuildingTypeId>(BuildingId), Entity<FHansaHouseId>(1), FHansaRate::FromPartsPerMillion(FHansaRate::Scale)});
	FHansaInventoryInitialization Input;
	Input.Id = Entity<FHansaInventoryId>(1);
	Input.CityId = Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
	Input.OwnerKind = EHansaInventoryOwnerKind::City;
	Input.Capacity = FHansaQuantity::FromRaw(100000);
	for (const auto& Good : Registry.GetGoods()) Input.AcceptedGoods.Add(Stable<FHansaGoodId>(*Good.StableId));
	if (bSupplyInput)
	{
		for (const auto& Amount : Recipe->Inputs)
			Input.InitialStock.Add({Stable<FHansaGoodId>(*Amount.GoodId), FHansaQuantity::FromRaw(Amount.QuantityMilliUnits)});
	}
	Init.Inventories.Add(Input);
	if (bSeparateBlockedOutput)
	{
		FHansaInventoryInitialization Output = Input;
		Output.Id = Entity<FHansaInventoryId>(2);
		Output.OwnerKind = EHansaInventoryOwnerKind::Building;
		Output.CityId = FHansaCityDefinitionId();
		Output.BuildingId = Entity<FHansaBuildingId>(1);
		Output.Capacity = FHansaQuantity::FromRaw(500);
		Output.InitialStock.Reset();
		Init.Inventories.Add(Output);
	}
	FHansaProductionInitialization Production;
	Production.Id = Entity<FHansaProductionId>(1);
	Production.BuildingId = Entity<FHansaBuildingId>(1);
	Production.RecipeId = Stable<FHansaRecipeId>(RecipeId);
	Production.InputInventoryId = Entity<FHansaInventoryId>(1);
	Production.OutputInventoryId = Entity<FHansaInventoryId>(bSeparateBlockedOutput ? 2 : 1);
	Production.AllocatedLaborerWorkforce = bSupplyWorkforce ? Recipe->LaborerWorkforce : 0;
	Production.AllocatedArtisanWorkforce = bSupplyWorkforce ? Recipe->ArtisanWorkforce : 0;
	Init.Productions.Add(Production);
	return Init;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTextileDefinitionsAndBatches,
	"Hansa.TextileProduction.DefinitionsExactBatchesAndBlockers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTextileDefinitionsAndBatches::RunTest(const FString&)
{
	FTextileCatalog Catalog;
	if (!MakeCatalog(*this, Catalog)) return false;
	const TCHAR* Goods[] = {TEXT("Good.Flax"), TEXT("Good.Hemp"), TEXT("Good.Beeswax"), TEXT("Good.LinenCloth"), TEXT("Good.LinenClothing"), TEXT("Good.Candles"), TEXT("Good.Rope")};
	for (const TCHAR* Good : Goods) TestNotNull(Good, Catalog.Registry.FindGood(Good));
	struct FCase { const TCHAR* Building; const TCHAR* Recipe; };
	const FCase Cases[] = {
		{TEXT("Building.Weaver"), TEXT("Recipe.WeaveLinen")},
		{TEXT("Building.Tailor"), TEXT("Recipe.SewLinenClothing")},
		{TEXT("Building.Chandler"), TEXT("Recipe.DipCandles")},
		{TEXT("Building.Ropewalk"), TEXT("Recipe.LayHempRope")},
		{TEXT("Building.Ropewalk"), TEXT("Recipe.LayFlaxRope")},
	};
	for (const FCase& Value : Cases)
	{
		const auto* Building = Catalog.Registry.FindBuilding(Value.Building);
		const auto* Recipe = Catalog.Registry.FindRecipe(Value.Recipe);
		if (!TestNotNull(Value.Building, Building) || !TestNotNull(Value.Recipe, Recipe)) return false;
		TestEqual(TEXT("Workshop card belongs only to Craftsmen"), Building->ConstructionTier, FString(TEXT("Craftsmen")));
		for (int32 Mode = 0; Mode < 3; ++Mode)
		{
			const bool bInput = Mode != 1;
			const bool bWorkforce = Mode != 2;
			auto Created = FHansaSimulationState::TryCreate(OneWorkshop(Catalog.Registry, Value.Building, Value.Recipe, bInput, bWorkforce));
			if (!TestTrue(TEXT("Workshop state initializes"), Created.IsSuccess())) return false;
			auto State = Created.Value;
			FHansaSimulationTransientCache Cache;
			for (int32 Tick = 0; Tick < Recipe->CycleTicks + 1; ++Tick)
				if (!TestTrue(TEXT("Authoritative workshop tick"), FHansaGameplayCommandGateway::ExecuteTick(State, Catalog.Context, {}, Cache).IsSuccess())) return false;
			const auto View = State.CreateReadOnlyAccess(Catalog.Context);
			const auto Projection = View.QueryProduction(Entity<FHansaProductionId>(1));
			if (!TestTrue(TEXT("Production projection exists"), Projection.IsSet())) return false;
			TestEqual(TEXT("Only a fully supplied batch completes"), Projection->CompletedCycles, Mode == 0 ? uint64(1) : uint64(0));
			if (Mode == 1) TestEqual(TEXT("Missing ingredient has exact blocker"), Projection->Blocker, EHansaProductionBlocker::MissingInput);
			if (Mode == 2) TestEqual(TEXT("Missing craftsmen have exact blocker"), Projection->Blocker, EHansaProductionBlocker::InsufficientArtisanWorkforce);
			for (const auto& Input : Recipe->Inputs)
			{
				const auto Stock = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(*Input.GoodId));
				TestTrue(TEXT("Input stock is queryable"), Stock.IsSet());
				if (Stock) TestEqual(TEXT("Input consumed exactly once or retained while blocked"), Stock->Stock.GetRawValue(), Mode == 0 || Mode == 1 ? int64(0) : Input.QuantityMilliUnits);
			}
			for (const auto& Output : Recipe->Outputs)
			{
				const auto Stock = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(*Output.GoodId));
				if (Stock) TestEqual(TEXT("Output produced exactly once"), Stock->Stock.GetRawValue(), Mode == 0 ? Output.QuantityMilliUnits : int64(0));
			}
		}
	}

	auto StorageState = FHansaSimulationState::TryCreate(OneWorkshop(Catalog.Registry, TEXT("Building.Weaver"), TEXT("Recipe.WeaveLinen"), true, true, true));
	if (!TestTrue(TEXT("Storage-blocked state initializes"), StorageState.IsSuccess())) return false;
	FHansaSimulationTransientCache StorageCache;
	for (int32 Tick = 0; Tick < 122; ++Tick) FHansaGameplayCommandGateway::ExecuteTick(StorageState.Value, Catalog.Context, {}, StorageCache);
	const auto StorageProjection = StorageState.Value.CreateReadOnlyAccess(Catalog.Context).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Output capacity has an accurate blocker"), StorageProjection && StorageProjection->Blocker == EHansaProductionBlocker::StorageBlocked);

	const auto* Ropewalk = Catalog.Registry.FindBuilding(TEXT("Building.Ropewalk"));
	TestEqual(TEXT("Ropewalk offers two independent recipes"), Ropewalk->RecipeIds.Num(), 2);
	TestTrue(TEXT("Hemp recipe offered"), Ropewalk->RecipeIds.Contains(TEXT("Recipe.LayHempRope")));
	TestTrue(TEXT("Flax recipe offered"), Ropewalk->RecipeIds.Contains(TEXT("Recipe.LayFlaxRope")));
	TestEqual(TEXT("Ropewalk footprint follows its 32 m lane"), Ropewalk->FootprintWidthCells, 8);
	TestEqual(TEXT("Ropewalk remains narrow"), Ropewalk->FootprintHeightCells, 2);
	const auto* Artisan = Catalog.Registry.FindPopulationTier(TEXT("PopulationTier.Artisan"));
	TestTrue(TEXT("Craftsmen consume linen clothing conservatively"), Artisan && Artisan->Needs.ContainsByPredicate([](const auto& Need) { return Need.NeedId == TEXT("Need.LinenClothing") && Need.ConsumptionMilliUnitsPerResidentPerTick == 1; }));
	TestTrue(TEXT("Craftsmen consume candles conservatively"), Artisan && Artisan->Needs.ContainsByPredicate([](const auto& Need) { return Need.NeedId == TEXT("Need.Candles") && Need.ConsumptionMilliUnitsPerResidentPerTick == 1; }));
	const auto* Rostock = Catalog.Registry.FindCityMarket(TEXT("City.Rostock"));
	for (const TCHAR* Imported : {TEXT("Good.Flax"), TEXT("Good.Hemp"), TEXT("Good.Beeswax")})
		TestTrue(TEXT("Prepared fibre or wax is initially trade-sourced"), Rostock && Rostock->Goods.ContainsByPredicate([&](const auto& Good) { return Good.GoodId == Imported && Good.BackgroundProductionMilliUnitsPerUpdate > 0; }));

	TArray<FHansaBuildCardPresentation> Cards;
	TArray<FHansaBuildChainPresentation> Chains;
	FString CatalogError;
	if (!TestTrue(TEXT("Native construction catalog accepts the textile definitions"),
		UHansaBuildMenuPresentationModel::BuildCatalogFromDefinitions(Catalog.Registry, {}, Cards, Chains, CatalogError)))
	{
		AddError(CatalogError);
		return false;
	}
	const int32 CraftsmenOnly = 1 << static_cast<uint8>(EHansaBuildTier::Craftsmen);
	for (const TCHAR* BuildingId : {TEXT("Building.Weaver"), TEXT("Building.Tailor"), TEXT("Building.Chandler"), TEXT("Building.Ropewalk")})
	{
		const auto* Card = Cards.FindByPredicate([&](const auto& Value) { return Value.StableId == BuildingId; });
		TestTrue(TEXT("Workshop has a native construction card"), Card != nullptr);
		if (Card) TestEqual(TEXT("Workshop card cannot leak into another population tab"), Card->BrowsingTierMask, CraftsmenOnly);
	}
	for (const TCHAR* OutputId : {TEXT("Good.LinenClothing"), TEXT("Good.Candles"), TEXT("Good.Rope")})
	{
		const auto* Chain = Chains.FindByPredicate([&](const auto& Value) { return Value.OutputGoodId == OutputId; });
		TestTrue(TEXT("Requested end-product selector exists"), Chain != nullptr);
		if (Chain) TestEqual(TEXT("End-product selector belongs only to Craftsmen"), Chain->BrowsingTierMask, CraftsmenOnly);
	}
	const auto* RopeCard = Cards.FindByPredicate([](const auto& Value) { return Value.StableId == TEXT("Building.Ropewalk"); });
	// Registry compilation canonicalizes recipe order; either alternative may be first.
	TestTrue(TEXT("Ropewalk card presents the exact independent recipes"), RopeCard &&
		(RopeCard->InputOutput.ToString() == TEXT("2 hemp or 2.5 flax → 1 rope") ||
		 RopeCard->InputOutput.ToString() == TEXT("2.5 flax or 2 hemp → 1 rope")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRopewalkRecipePersistence,
	"Hansa.TextileProduction.RopewalkRecipeSelectionSaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRopewalkRecipePersistence::RunTest(const FString&)
{
	FTextileCatalog Catalog;
	if (!MakeCatalog(*this, Catalog)) return false;
	auto Init = OneWorkshop(Catalog.Registry, TEXT("Building.Ropewalk"), TEXT("Recipe.LayHempRope"), true, true);
	Init.Inventories[0].InitialStock.Add({Stable<FHansaGoodId>(TEXT("Good.Flax")), FHansaQuantity::FromRaw(2500)});
	const auto Created = FHansaSimulationState::TryCreate(Init);
	if (!TestTrue(TEXT("Ropewalk selection state initializes"), Created.IsSuccess())) return false;
	auto State = Created.Value;
	FHansaSimulationTransientCache Cache;
	FHansaCommandHeader Header;
	Header.CommandId = Entity<FHansaCommandId>(1);
	Header.GlobalSequence = 1;
	Header.Authority.IssuingHouseId = Entity<FHansaHouseId>(1);
	Header.Authority.PrincipalId = 1;
	Header.RequestedExecutionTick = State.CreateReadOnlyAccess(Catalog.Context).GetClock().GetTick();
	const auto Command = FHansaGameplayCommand::Create(Header, FHansaSetProductionModeCommand{
		Entity<FHansaProductionId>(1), Stable<FHansaRecipeId>(TEXT("Recipe.LayFlaxRope")), false});
	if (!TestTrue(TEXT("Flax recipe selection command succeeds"), FHansaGameplayCommandGateway::ExecuteTick(State, Catalog.Context, MakeArrayView(&Command, 1), Cache).IsSuccess())) return false;
	const auto Selected = State.CreateReadOnlyAccess(Catalog.Context).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Requested recipe is flax only"), Selected && Selected->RequestedRecipeId == Stable<FHansaRecipeId>(TEXT("Recipe.LayFlaxRope")));

	FHansaSaveSnapshot Save;
	Save.State = State;
	Save.BuildVersion = TEXT("TextileProductionCandidate");
	Save.SavedUtc = TEXT("2026-09-19T00:00:00Z");
	Save.Players.Add({1, Entity<FHansaHouseId>(1)});
	TArray<uint8> Bytes;
	FHansaSaveSnapshot Loaded;
	if (!TestTrue(TEXT("Selected Ropewalk recipe saves"), FHansaSaveEnvelope::Encode(Save, Catalog.Context, Bytes).IsSuccess())
		|| !TestTrue(TEXT("Selected Ropewalk recipe loads"), FHansaSaveEnvelope::Decode(Bytes, Catalog.Context, Loaded).IsSuccess())) return false;
	const auto LoadedProduction = Loaded.State.CreateReadOnlyAccess(Catalog.Context).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Flax selection survives save/load"), LoadedProduction && LoadedProduction->RequestedRecipeId == Stable<FHansaRecipeId>(TEXT("Recipe.LayFlaxRope")));

	FHansaSimulationTransientCache LoadedCache;
	for (int32 Tick = 0; Tick < 170; ++Tick)
		if (!FHansaGameplayCommandGateway::ExecuteTick(Loaded.State, Catalog.Context, {}, LoadedCache).IsSuccess()) return false;
	const auto View = Loaded.State.CreateReadOnlyAccess(Catalog.Context);
	const auto Hemp = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(TEXT("Good.Hemp")));
	const auto Flax = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(TEXT("Good.Flax")));
	const auto Rope = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(TEXT("Good.Rope")));
	TestTrue(TEXT("Stocks remain queryable"), Hemp && Flax && Rope);
	if (Hemp && Flax && Rope)
	{
		TestEqual(TEXT("Flax recipe does not consume hemp"), Hemp->Stock.GetRawValue(), int64(2000));
		TestEqual(TEXT("Flax recipe consumes flax once"), Flax->Stock.GetRawValue(), int64(0));
		TestEqual(TEXT("Flax recipe produces one rope batch"), Rope->Stock.GetRawValue(), int64(1000));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTextileLogistics,
    "Hansa.TextileProduction.WeaverTailorPhysicalDelivery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTextileLogistics::RunTest(const FString&)
{
    FTextileCatalog Catalog;
    if (!MakeCatalog(*this, Catalog)) return false;
    for (const bool Disconnected : {false, true})
    {
        auto Init = OneWorkshop(Catalog.Registry, TEXT("Building.Weaver"), TEXT("Recipe.WeaveLinen"), true, true);
        Init.Inventories[0].OwnerKind = EHansaInventoryOwnerKind::Building;
        Init.Inventories[0].BuildingId = Entity<FHansaBuildingId>(1);
        Init.Inventories[0].CityId = FHansaCityDefinitionId();
        Init.Inventories[0].InitialStock[0].Quantity = FHansaQuantity::FromRaw(4000);
        auto TailorInventory = Init.Inventories[0];
        TailorInventory.Id = Entity<FHansaInventoryId>(2);
        TailorInventory.BuildingId = Entity<FHansaBuildingId>(2);
        TailorInventory.InitialStock.Reset();
        Init.Inventories.Add(TailorInventory);
        auto TailorProduction = Init.Productions[0];
        TailorProduction.Id = Entity<FHansaProductionId>(2);
        TailorProduction.BuildingId = Entity<FHansaBuildingId>(2);
        TailorProduction.RecipeId = Stable<FHansaRecipeId>(TEXT("Recipe.SewLinenClothing"));
        TailorProduction.InputInventoryId = TailorProduction.OutputInventoryId = Entity<FHansaInventoryId>(2);
        TailorProduction.AllocatedArtisanWorkforce = 2;
        Init.Productions.Add(TailorProduction);
        Init.Buildings.Reset();
        const auto City = Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
        auto AddBuilding = [&](uint64 Id, const TCHAR* Type, int32 X, int32 Y, int32 Width, int32 Height)
        {
            FHansaBuildingState Building;
            Building.Id = Entity<FHansaBuildingId>(Id);
            Building.DefinitionId = Stable<FHansaBuildingTypeId>(Type);
            Building.OwnerId = Entity<FHansaHouseId>(1);
            Building.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
            Building.ConstructionState = EHansaConstructionState::Completed;
            Init.Buildings.Add(Building);
            FHansaPlacedBuildingRecord Placement;
            Placement.BuildingId = Building.Id;
            Placement.OwnerId = Building.OwnerId;
            Placement.Spec = {City, Building.DefinitionId, {X, Y}, EHansaGridRotation::North};
            for (int32 DX = 0; DX < Width; ++DX)
                for (int32 DY = 0; DY < Height; ++DY) Placement.OccupiedCells.Add({X + DX, Y + DY});
            Init.Placement.Placements.Add(Placement);
        };
        AddBuilding(1, TEXT("Building.Weaver"), 0, 1, 3, 3);
        AddBuilding(2, TEXT("Building.Tailor"), 4, 1, 3, 2);
        AddBuilding(3, TEXT("Building.Market"), 8, 1, 2, 2);
        FHansaPlacementMapInitialization Map;
        Map.CityId = City;
        Map.BoundsMin = {0, 0}; Map.BoundsMax = {10, 4};
        Map.RoadBuildingDefinitionId = Stable<FHansaBuildingTypeId>(TEXT("Building.Road"));
        for (int32 X = 0; X <= 10; ++X)
        {
            for (int32 Y = 0; Y <= 4; ++Y)
                Map.Cells.Add({{X, Y}, EHansaPlacementTerrain::Land, Entity<FHansaHouseId>(1), false});
            if (!Disconnected || X != 3) AddBuilding(10 + X, TEXT("Building.Road"), X, 0, 1, 1);
        }
        Init.Placement.Maps.Add(Map);
        FHansaLogisticsRequestInitialization Request;
        Request.Id = Entity<FHansaLogisticsRequestId>(1);
        Request.SourceInventoryId = Entity<FHansaInventoryId>(1);
        Request.DestinationInventoryId = Entity<FHansaInventoryId>(2);
        Request.GoodId = Stable<FHansaGoodId>(TEXT("Good.LinenCloth"));
        Request.Quantity = FHansaQuantity::FromRaw(1500);
        Init.LocalLogisticsRequests.Add(Request);
        auto Created = FHansaSimulationState::TryCreate(Init);
        if (!TestTrue(TEXT("Physical textile chain initializes"), Created.IsSuccess())) return false;
        FHansaSimulationTransientCache Cache;
        bool SawCargo = false;
        for (int32 Tick = 0; Tick < 400; ++Tick)
        {
            if (!TestTrue(TEXT("Textile delivery tick succeeds"), FHansaGameplayCommandGateway::ExecuteTick(Created.Value, Catalog.Context, {}, Cache).IsSuccess())) return false;
            for (const auto& Job : Created.Value.CreateReadOnlyAccess(Catalog.Context).BuildLogisticsJobProjection())
                SawCargo |= Job.CargoQuantity.GetRawValue() > 0;
        }
        const auto View = Created.Value.CreateReadOnlyAccess(Catalog.Context);
        const auto Delivery = View.QueryLogisticsRequest(Request.Id);
        const auto Clothing = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(2), Stable<FHansaGoodId>(TEXT("Good.LinenClothing")));
        const auto Cloth = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Request.GoodId);
        TestTrue(TEXT("Delivery and stocks are queryable"), Delivery && Clothing && Cloth);
        if (!Delivery || !Clothing || !Cloth) return false;
        TestEqual(TEXT("Only connected Tailor produces clothing"), Clothing->Stock.GetRawValue(), Disconnected ? int64(0) : int64(1000));
        TestEqual(TEXT("Two Weaver batches leave exactly the unshipped cloth"), Cloth->Stock.GetRawValue(), Disconnected ? int64(2000) : int64(500));
        TestEqual(TEXT("Cargo is physically in transit only on connected roads"), SawCargo, !Disconnected);
        if (Disconnected) TestEqual(TEXT("Disconnected delivery reports its actual blocker"), Delivery->Bottleneck, EHansaLogisticsBottleneck::DisconnectedRoad);
        else TestEqual(TEXT("Cloth delivery completes through logistics"), Delivery->Status, EHansaLogisticsRequestStatus::Completed);
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTextileHouseholdConsumption,
    "Hansa.TextileProduction.CraftsmenConsumeClothingAndCandles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTextileHouseholdConsumption::RunTest(const FString&)
{
    FTextileCatalog Catalog;
    if (!MakeCatalog(*this, Catalog)) return false;
    auto Init = OneWorkshop(Catalog.Registry, TEXT("Building.Residence.Artisan"), TEXT("Recipe.WeaveLinen"), false, false);
    Init.Productions.Reset();
    Init.Buildings[0].ConstructionState = EHansaConstructionState::Completed;
    auto Market = Init.Buildings[0];
    Market.Id = Entity<FHansaBuildingId>(2);
    Market.DefinitionId = Stable<FHansaBuildingTypeId>(TEXT("Building.Market"));
    Init.Buildings.Add(Market);
    const auto City = Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
    Init.Inventories[0].BuildingId = Market.Id;
    Init.Inventories[0].InitialStock = {
        {Stable<FHansaGoodId>(TEXT("Good.LinenClothing")), FHansaQuantity::FromRaw(1000)},
        {Stable<FHansaGoodId>(TEXT("Good.Candles")), FHansaQuantity::FromRaw(1000)} };
    FHansaPlacementMapInitialization Map;
    Map.CityId = City; Map.BoundsMin = {0, 0}; Map.BoundsMax = {5, 2};
    Map.RoadBuildingDefinitionId = Stable<FHansaBuildingTypeId>(TEXT("Building.Road"));
    for (int32 X = 0; X <= 5; ++X)
    {
        for (int32 Y = 0; Y <= 2; ++Y) Map.Cells.Add({{X,Y}, EHansaPlacementTerrain::Land, Entity<FHansaHouseId>(1), false});
        auto Road = Market; Road.Id = Entity<FHansaBuildingId>(10 + X); Road.DefinitionId = Map.RoadBuildingDefinitionId;
        Init.Buildings.Add(Road);
        Init.Placement.Placements.Add({Road.Id, Road.OwnerId, {City, Road.DefinitionId, {X, 0}, EHansaGridRotation::North}, {{X,0}}});
    }
    Init.Placement.Maps.Add(Map);
    Init.Placement.Placements.Add({Init.Buildings[0].Id, Market.OwnerId,
        {City, Init.Buildings[0].DefinitionId, {0,1}, EHansaGridRotation::North}, {{0,1},{1,1},{0,2},{1,2}}});
    Init.Placement.Placements.Add({Market.Id, Market.OwnerId,
        {City, Market.DefinitionId, {3,1}, EHansaGridRotation::North}, {{3,1},{4,1},{3,2},{4,2}}});
    FHansaPopulationCohortInitialization Cohort;
    Cohort.Id = Entity<FHansaPopulationCohortId>(1); Cohort.ResidenceBuildingId = Init.Buildings[0].Id;
    Cohort.CityId = City; Cohort.ConsumptionInventoryId = Init.Inventories[0].Id;
    Cohort.TierId = Stable<FHansaPopulationTierId>(TEXT("PopulationTier.Artisan"));
    Cohort.Residents = 4; Cohort.ResidenceCapacity = Catalog.Registry.FindBuilding(TEXT("Building.Residence.Artisan"))->ResidenceCapacity;
    Cohort.PurchasingPowerBasisPoints = Cohort.ServiceAccessBasisPoints = Cohort.ServiceReliabilityBasisPoints = 10000;
    Init.PopulationCohorts.Add(Cohort);
    Init.MarketSettings.UpdateCadenceTicks = 1;
    for (const TCHAR* Id : {TEXT("Good.LinenClothing"), TEXT("Good.Candles")})
    {
        FHansaCityMarketInitialization GoodMarket;
        GoodMarket.CityId = City; GoodMarket.GoodId = Stable<FHansaGoodId>(Id);
        GoodMarket.InventoryIds.Add(Init.Inventories[0].Id);
        GoodMarket.DesiredReserve = FHansaQuantity::FromRaw(1000);
        GoodMarket.MinimumPriceMilliMarks = 1; GoodMarket.MaximumPriceMilliMarks = 100000;
        GoodMarket.InitialPriceMilliMarks = Catalog.Registry.FindGood(Id)->BaseValueMilliMarks;
        Init.Markets.Add(GoodMarket);
    }
    auto State = FHansaSimulationState::TryCreate(Init);
    if (!TestTrue(TEXT("Household fixture initializes"), State.IsSuccess())) return false;
    FHansaSimulationTransientCache Cache;
    if (!TestTrue(TEXT("Household tick succeeds"), FHansaGameplayCommandGateway::ExecuteTick(State.Value, Catalog.Context, {}, Cache).IsSuccess())) return false;
    for (const TCHAR* Id : {TEXT("Good.LinenClothing"), TEXT("Good.Candles")})
    {
        const auto Stock = State.Value.CreateReadOnlyAccess(Catalog.Context).GetInventories().QueryStock(Init.Inventories[0].Id, Stable<FHansaGoodId>(Id));
        TestTrue(TEXT("Household good is queryable"), Stock.IsSet());
        if (Stock) TestEqual(FString(Id) + TEXT(" consumed at one milliunit per resident"), Stock->Stock.GetRawValue(), int64(996));
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRopewalkPendingRecipePersistence,
	"Hansa.TextileProduction.RopewalkPendingBatchSaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRopewalkPendingRecipePersistence::RunTest(const FString&)
{
	FTextileCatalog Catalog;
	if (!MakeCatalog(*this, Catalog)) return false;
	auto Init = OneWorkshop(Catalog.Registry, TEXT("Building.Ropewalk"), TEXT("Recipe.LayHempRope"), true, true);
	Init.Inventories[0].InitialStock.Add({Stable<FHansaGoodId>(TEXT("Good.Flax")), FHansaQuantity::FromRaw(2500)});
	const auto Created = FHansaSimulationState::TryCreate(Init);
	if (!TestTrue(TEXT("Ropewalk selection state initializes"), Created.IsSuccess())) return false;
	auto State = Created.Value;
	FHansaSimulationTransientCache Cache;
	if (!TestTrue(TEXT("Hemp batch starts before requesting flax"), FHansaGameplayCommandGateway::ExecuteTick(State, Catalog.Context, {}, Cache).IsSuccess())) return false;
    FHansaCommandHeader Header;
	Header.CommandId = Entity<FHansaCommandId>(1);
	Header.GlobalSequence = 1;
	Header.Authority.IssuingHouseId = Entity<FHansaHouseId>(1);
	Header.Authority.PrincipalId = 1;
	Header.RequestedExecutionTick = State.CreateReadOnlyAccess(Catalog.Context).GetClock().GetTick();
	const auto Command = FHansaGameplayCommand::Create(Header, FHansaSetProductionModeCommand{
		Entity<FHansaProductionId>(1), Stable<FHansaRecipeId>(TEXT("Recipe.LayFlaxRope")), false});
	if (!TestTrue(TEXT("Flax recipe selection command succeeds"), FHansaGameplayCommandGateway::ExecuteTick(State, Catalog.Context, MakeArrayView(&Command, 1), Cache).IsSuccess())) return false;
	const auto Selected = State.CreateReadOnlyAccess(Catalog.Context).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Requested recipe is flax only"), Selected && Selected->RequestedRecipeId == Stable<FHansaRecipeId>(TEXT("Recipe.LayFlaxRope")));

	TestTrue(TEXT("Hemp remains active until its batch finishes"), Selected && Selected->RecipeId == Stable<FHansaRecipeId>(TEXT("Recipe.LayHempRope")) && Selected->ProgressTicks > 0);
    FHansaSaveSnapshot Save;
	Save.State = State;
	Save.BuildVersion = TEXT("TextileProductionCandidate");
	Save.SavedUtc = TEXT("2026-09-19T00:00:00Z");
	Save.Players.Add({1, Entity<FHansaHouseId>(1)});
	TArray<uint8> Bytes;
	FHansaSaveSnapshot Loaded;
	if (!TestTrue(TEXT("Selected Ropewalk recipe saves"), FHansaSaveEnvelope::Encode(Save, Catalog.Context, Bytes).IsSuccess())
		|| !TestTrue(TEXT("Selected Ropewalk recipe loads"), FHansaSaveEnvelope::Decode(Bytes, Catalog.Context, Loaded).IsSuccess())) return false;
	const auto LoadedProduction = Loaded.State.CreateReadOnlyAccess(Catalog.Context).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Flax selection survives save/load"), LoadedProduction && LoadedProduction->RequestedRecipeId == Stable<FHansaRecipeId>(TEXT("Recipe.LayFlaxRope")));

	FHansaSimulationTransientCache LoadedCache;
	for (int32 Tick = 0; Tick < 320; ++Tick)
		if (!FHansaGameplayCommandGateway::ExecuteTick(Loaded.State, Catalog.Context, {}, LoadedCache).IsSuccess()) return false;
	const auto View = Loaded.State.CreateReadOnlyAccess(Catalog.Context);
	const auto Hemp = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(TEXT("Good.Hemp")));
	const auto Flax = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(TEXT("Good.Flax")));
	const auto Rope = View.GetInventories().QueryStock(Entity<FHansaInventoryId>(1), Stable<FHansaGoodId>(TEXT("Good.Rope")));
	TestTrue(TEXT("Stocks remain queryable"), Hemp && Flax && Rope);
	if (Hemp && Flax && Rope)
	{
		TestEqual(TEXT("Only the initial hemp batch consumes hemp"), Hemp->Stock.GetRawValue(), int64(0));
		TestEqual(TEXT("Flax recipe consumes flax once"), Flax->Stock.GetRawValue(), int64(0));
		TestEqual(TEXT("One hemp and one flax batch produce exactly two rope"), Rope->Stock.GetRawValue(), int64(2000));
	}
	return !HasAnyErrors();
}

#endif
