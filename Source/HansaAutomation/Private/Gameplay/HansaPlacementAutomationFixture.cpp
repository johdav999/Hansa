#include "Gameplay/HansaPlacementAutomationFixture.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "World/HansaLubeckPlacementGrid.h"

namespace Hansa::Automation
{
	namespace PlacementFixture
	{
		using namespace Hansa::Simulation;

		template <typename TValue>
		bool Assign(const THansaValueResult<TValue>& Result, TValue& OutValue)
		{
			if (!Result)
			{
				return false;
			}
			OutValue = Result.Value;
			return true;
		}

		FHansaSemanticNode Node(
			const TCHAR* Id,
			const EHansaSemanticRole Role,
			const TCHAR* Label,
			const TCHAR* Parent = TEXT(""),
			TArray<EHansaSemanticAction> Actions = {})
		{
			FHansaSemanticNode Result;
			Result.Id = Id;
			Result.Role = Role;
			Result.Label = Label;
			Result.ParentId = Parent;
			Result.Actions = MoveTemp(Actions);
			return Result;
		}

		FHansaEconomicRegistry BuildRegistry()
		{
			FHansaCompiledGoodDefinition Timber;
			Timber.StableId = TEXT("Good.Timber");
			FHansaCompiledGoodDefinition Planks;
			Planks.StableId = TEXT("Good.Planks");
			FHansaCompiledGoodDefinition Tools;
			Tools.StableId = TEXT("Good.Tools");
			FHansaCompiledBuildingDefinition Road;
			Road.StableId = TEXT("Building.Road");
			Road.ConstructionCosts = { { TEXT("Good.Timber"), 500 } };
			Road.ConstructionCostPfennig = 25;
			Road.CancellationRefundBasisPoints = 5000;
			Road.FootprintWidthCells = 1;
			Road.FootprintHeightCells = 1;
			Road.BuildTicks = 1;

			FHansaCompiledBuildingDefinition Warehouse;
			Warehouse.StableId = TEXT("Building.Warehouse");
			Warehouse.ConstructionCosts = {
				{ TEXT("Good.Planks"), 10'000 }, { TEXT("Good.Tools"), 2'000 }
			};
			Warehouse.ConstructionCostPfennig = 2500;
			Warehouse.CancellationRefundBasisPoints = 5000;
			Warehouse.FootprintWidthCells = 2;
			Warehouse.FootprintHeightCells = 3;
			Warehouse.BuildTicks = 30;
			Warehouse.bRequiresRoad = true;
			return FHansaEconomicRegistry({ MoveTemp(Timber), MoveTemp(Planks), MoveTemp(Tools) }, {},
				{ MoveTemp(Road), MoveTemp(Warehouse) },
				FHansaPlacementAutomationFixture::RegistryHash);
		}

		FHansaEconomicRegistry BuildIntegratedRegistry()
		{
			FHansaCompiledGoodDefinition Grain;
			Grain.StableId = TEXT("Good.Grain");
			FHansaCompiledGoodDefinition Bread;
			Bread.StableId = TEXT("Good.Bread");

			FHansaCompiledRecipeDefinition BakeryRecipe;
			BakeryRecipe.StableId = TEXT("Recipe.BakeBread");
			BakeryRecipe.Inputs = { { TEXT("Good.Grain"), 100 } };
			BakeryRecipe.Outputs = { { TEXT("Good.Bread"), 1'000 } };
			BakeryRecipe.CycleTicks = 1;
			BakeryRecipe.LaborerWorkforce = 3;

			FHansaCompiledBuildingDefinition Road;
			Road.StableId = TEXT("Building.Road");
			Road.BuildTicks = 1;
			FHansaCompiledBuildingDefinition Warehouse;
			Warehouse.StableId = TEXT("Building.Warehouse");
			Warehouse.BuildTicks = 1;
			Warehouse.StorageCapacityMilliUnits = 50'000;
			Warehouse.bRequiresRoad = true;
			FHansaCompiledBuildingDefinition Bakery;
			Bakery.StableId = TEXT("Building.Bakery");
			Bakery.RecipeIds = { BakeryRecipe.StableId };
			Bakery.BuildTicks = 2;
			Bakery.LaborerWorkforce = 3;
			Bakery.bRequiresRoad = true;
			FHansaCompiledBuildingDefinition Residence;
			Residence.StableId = TEXT("Building.Residence.Laborer");
			Residence.BuildTicks = 3;
			Residence.ResidenceCapacity = 12;
			Residence.ResidentPopulationTierId = TEXT("PopulationTier.Laborer");
			Residence.bRequiresRoad = true;

			FHansaCompiledNeedDefinition BreadNeed;
			BreadNeed.StableId = TEXT("Need.Bread");
			BreadNeed.Kind = EHansaCompiledNeedKind::Good;
			BreadNeed.GoodId = Bread.StableId;
			FHansaCompiledPopulationTierDefinition Laborer;
			Laborer.StableId = TEXT("PopulationTier.Laborer");
			Laborer.Needs = { { BreadNeed.StableId, 10, 10'000 } };
			Laborer.WorkforcePerResidentBasisPoints = 6000;
			Laborer.GrowthSatisfactionBasisPoints = 8000;
			Laborer.DeclineSatisfactionBasisPoints = 3000;
			Laborer.EvaluationTicks = 4;
			Laborer.GrowthResidentsPerEvaluation = 1;
			Laborer.DeclineResidentsPerEvaluation = 1;

			return FHansaEconomicRegistry({ MoveTemp(Grain), MoveTemp(Bread) }, { MoveTemp(BakeryRecipe) },
				{ MoveTemp(Road), MoveTemp(Warehouse), MoveTemp(Bakery), MoveTemp(Residence) },
				FHansaPlacementAutomationFixture::IntegratedRegistryHash,
				{ MoveTemp(BreadNeed) }, { MoveTemp(Laborer) });
		}

		FHansaBuildingState Building(const uint64 Value, const FHansaBuildingTypeId DefinitionId,
			const FHansaHouseId HouseId, const bool bCompleted)
		{
			FHansaBuildingState Result;
			Result.Id = FHansaBuildingId::TryCreate(Value).Value;
			Result.DefinitionId = DefinitionId;
			Result.OwnerId = HouseId;
			if (bCompleted)
			{
				Result.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
				Result.ConstructionState = EHansaConstructionState::Completed;
			}
			return Result;
		}

		FHansaPlacedBuildingRecord Placement(const FHansaBuildingState& Building,
			const FHansaCityDefinitionId CityId, const int32 X, const int32 Y)
		{
			FHansaPlacedBuildingRecord Result;
			Result.BuildingId = Building.Id;
			Result.OwnerId = Building.OwnerId;
			Result.Spec.CityId = CityId;
			Result.Spec.BuildingDefinitionId = Building.DefinitionId;
			Result.Spec.Anchor = { X, Y };
			Result.OccupiedCells = { { X, Y } };
			return Result;
		}

		FHansaSemanticActionHandlers Handlers(
			TFunction<bool()> Activate,
			TFunction<bool()> Focus)
		{
			FHansaSemanticActionHandlers Result;
			Result.Activate = MoveTemp(Activate);
			Result.Focus = MoveTemp(Focus);
			return Result;
		}
	}

	bool FHansaPlacementAutomationFixture::Load(FHansaSemanticUiRegistry& InRegistry, FString& OutError)
	{
		bIntegrated = false;
		Registry = &InRegistry;
		Registry->Reset();
		bLoaded = InitializeState(OutError);
		if (!bLoaded)
		{
			return false;
		}
		RegisterSemantics();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::LoadIntegrated(FHansaSemanticUiRegistry& InRegistry, FString& OutError)
	{
		bIntegrated = true;
		Registry = &InRegistry;
		Registry->Reset();
		bLoaded = InitializeIntegratedState(OutError);
		if (!bLoaded)
		{
			return false;
		}
		UpdateIntegratedCheckpoints();
		RegisterSemantics();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::InitializeState(FString& OutError)
	{
		using namespace Hansa::Simulation;
		using namespace PlacementFixture;
		FHansaScenarioId ScenarioId;
		FHansaSimulationVersion Version;
		FHansaSimulationTick Tick;
		FHansaSimulationClock Clock;
		if (!Assign(FHansaScenarioId::TryParse(TEXT("Scenario.EmptyLubeckBuildV1")), ScenarioId) ||
			!Assign(FHansaSimulationDefinitionContext::TryCreate(
				ScenarioId, RegistryHash, BuildRegistry()), Definitions) ||
			!Assign(FHansaHouseId::TryCreate(1), HouseId) ||
			!Assign(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")), CityId) ||
			!Assign(FHansaBuildingTypeId::TryParse(TEXT("Building.Road")), RoadDefinitionId) ||
			!Assign(FHansaBuildingTypeId::TryParse(TEXT("Building.Warehouse")), WarehouseDefinitionId) ||
			!Assign(FHansaSimulationVersion::TryCreate(1), Version) ||
			!Assign(FHansaSimulationTick::TryCreate(0), Tick) ||
			!Assign(FHansaSimulationClock::TryCreate(Version, Tick), Clock))
		{
			OutError = TEXT("The placement fixture could not create its stable definitions or identities.");
			return false;
		}

		const THansaValueResult<FHansaPlacementInitialization> Placement =
			Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(HouseId, *Definitions.GetEconomicRegistry());
		if (!Placement)
		{
			OutError = TEXT("The placement fixture could not create the deterministic Lübeck grid.");
			return false;
		}

		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Clock;
		Initialization.CampaignSeed = 0x454D5054594C5542ULL;
		Initialization.Houses.Add({ HouseId, FHansaMoney::FromRaw(100'000) });
		Initialization.Cities.Add({ CityId, FHansaQuantity() });
		FHansaInventoryInitialization CityInventory;
		if (!Assign(FHansaInventoryId::TryCreate(1), CityInventory.Id))
		{
			OutError = TEXT("The placement fixture could not create its city inventory identity.");
			return false;
		}
		CityInventory.OwnerKind = EHansaInventoryOwnerKind::City;
		CityInventory.CityId = CityId;
		CityInventory.Capacity = FHansaQuantity::FromRaw(500'000);
		const auto TimberId = FHansaGoodId::TryParse(TEXT("Good.Timber"));
		const auto PlanksId = FHansaGoodId::TryParse(TEXT("Good.Planks"));
		const auto ToolsId = FHansaGoodId::TryParse(TEXT("Good.Tools"));
		if (!CityInventory.Id.IsValid() || !TimberId || !PlanksId || !ToolsId)
		{
			OutError = TEXT("The placement fixture could not create construction inventory identities.");
			return false;
		}
		CityInventory.AcceptedGoods = { TimberId.Value, PlanksId.Value, ToolsId.Value };
		CityInventory.InitialStock = {
			{ TimberId.Value, FHansaQuantity::FromRaw(100'000) },
			{ PlanksId.Value, FHansaQuantity::FromRaw(100'000) },
			{ ToolsId.Value, FHansaQuantity::FromRaw(100'000) }
		};
		Initialization.Inventories.Add(MoveTemp(CityInventory));
		Initialization.Placement = Placement.Value;
		if (!Assign(FHansaSimulationState::TryCreate(MoveTemp(Initialization)), State))
		{
			OutError = TEXT("The placement fixture failed authoritative state initialization.");
			return false;
		}

		PlacementSession.Cancel();
		Cache = FHansaSimulationTransientCache();
		PreviewValidation.Reset();
		LastPlacedBuildingId = FHansaBuildingId();
		FocusedSemanticId.Reset();
		SelectedTool = ESelectedTool::None;
		PrimaryFailure = EHansaPlacementFailure::None;
		bRepeat = true;
		bHasPreview = false;
		bPreviewCanPlace = false;
		IntegratedCheckpoints = FHansaIntegratedLubeckCheckpointState();
		NextCommandId = 1;
		NextBuildingId = 1;
		CameraStateValue = TEXT("focus=-3200,-700;yawDegrees=35;zoomDistance=6500");
		return true;
	}

	bool FHansaPlacementAutomationFixture::InitializeIntegratedState(FString& OutError)
	{
		using namespace Hansa::Simulation;
		using namespace PlacementFixture;
		FHansaScenarioId ScenarioId;
		FHansaSimulationVersion Version;
		FHansaSimulationTick Tick;
		FHansaSimulationClock Clock;
		if (!Assign(FHansaScenarioId::TryParse(TEXT("Scenario.IntegratedLubeckCityV1")), ScenarioId) ||
			!Assign(FHansaSimulationDefinitionContext::TryCreate(
				ScenarioId, IntegratedRegistryHash, BuildIntegratedRegistry()), Definitions) ||
			!Assign(FHansaHouseId::TryCreate(1), HouseId) ||
			!Assign(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")), CityId) ||
			!Assign(FHansaBuildingTypeId::TryParse(TEXT("Building.Road")), RoadDefinitionId) ||
			!Assign(FHansaBuildingTypeId::TryParse(TEXT("Building.Warehouse")), WarehouseDefinitionId) ||
			!Assign(FHansaSimulationVersion::TryCreate(1), Version) ||
			!Assign(FHansaSimulationTick::TryCreate(0), Tick) ||
			!Assign(FHansaSimulationClock::TryCreate(Version, Tick, 10), Clock))
		{
			OutError = TEXT("The integrated Lübeck fixture could not create its definitions or identities.");
			return false;
		}

		const FHansaBuildingTypeId BakeryDefinition = FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery")).Value;
		const FHansaBuildingTypeId ResidenceDefinition =
			FHansaBuildingTypeId::TryParse(TEXT("Building.Residence.Laborer")).Value;
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Clock;
		Initialization.CampaignSeed = 0x5330365030344C42ULL;
		Initialization.Houses.Add({ HouseId, FHansaMoney::FromRaw(100'000) });
		Initialization.Cities.Add({ CityId, FHansaQuantity() });

		const FHansaBuildingState Warehouse = Building(1, WarehouseDefinitionId, HouseId, true);
		const FHansaBuildingState Bakery = Building(2, BakeryDefinition, HouseId, false);
		const FHansaBuildingState Residence = Building(3, ResidenceDefinition, HouseId, false);
		Initialization.Buildings = { Warehouse, Bakery, Residence };
		for (uint64 X = 0; X <= 6; ++X)
		{
			Initialization.Buildings.Add(Building(10 + X, RoadDefinitionId, HouseId, true));
		}

		FHansaPlacementMapInitialization Map;
		Map.CityId = CityId;
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 6, 2 };
		Map.RoadBuildingDefinitionId = RoadDefinitionId;
		for (int32 X = 0; X <= 6; ++X)
		{
			for (int32 Y = 0; Y <= 2; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, HouseId, false });
			}
		}
		Initialization.Placement.Maps.Add(MoveTemp(Map));
		Initialization.Placement.Entitlements = {
			{ HouseId, RoadDefinitionId }, { HouseId, WarehouseDefinitionId },
			{ HouseId, BakeryDefinition }, { HouseId, ResidenceDefinition }
		};
		Initialization.Placement.Placements = {
			Placement(Warehouse, CityId, 0, 1), Placement(Bakery, CityId, 3, 1),
			Placement(Residence, CityId, 5, 1)
		};
		for (uint64 X = 0; X <= 6; ++X)
		{
			Initialization.Placement.Placements.Add(
				Placement(Initialization.Buildings[3 + static_cast<int32>(X)], CityId, static_cast<int32>(X), 0));
		}

		const FHansaGoodId Grain = FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
		const FHansaGoodId Bread = FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
		FHansaInventoryInitialization WarehouseInventory;
		WarehouseInventory.Id = FHansaInventoryId::TryCreate(1).Value;
		WarehouseInventory.OwnerKind = EHansaInventoryOwnerKind::Warehouse;
		WarehouseInventory.BuildingId = Warehouse.Id;
		WarehouseInventory.Capacity = FHansaQuantity::FromRaw(50'000);
		WarehouseInventory.AcceptedGoods = { Grain, Bread };
		WarehouseInventory.InitialStock = { { Grain, FHansaQuantity::FromRaw(20'000) } };
		FHansaInventoryInitialization BakeryInventory;
		BakeryInventory.Id = FHansaInventoryId::TryCreate(2).Value;
		BakeryInventory.OwnerKind = EHansaInventoryOwnerKind::Building;
		BakeryInventory.BuildingId = Bakery.Id;
		BakeryInventory.Capacity = FHansaQuantity::FromRaw(20'000);
		BakeryInventory.AcceptedGoods = { Grain, Bread };
		FHansaInventoryInitialization CityInventory;
		CityInventory.Id = FHansaInventoryId::TryCreate(3).Value;
		CityInventory.OwnerKind = EHansaInventoryOwnerKind::City;
		CityInventory.CityId = CityId;
		CityInventory.Capacity = FHansaQuantity::FromRaw(50'000);
		CityInventory.AcceptedGoods = { Bread };
		Initialization.Inventories = {
			MoveTemp(WarehouseInventory), MoveTemp(BakeryInventory), MoveTemp(CityInventory)
		};

		FHansaProductionInitialization Production;
		Production.Id = FHansaProductionId::TryCreate(1).Value;
		Production.BuildingId = Bakery.Id;
		Production.RecipeId = FHansaRecipeId::TryParse(TEXT("Recipe.BakeBread")).Value;
		Production.InputInventoryId = FHansaInventoryId::TryCreate(2).Value;
		Production.OutputInventoryId = FHansaInventoryId::TryCreate(2).Value;
		Production.bUsesCityWorkforce = true;
		Initialization.Productions.Add(Production);

		FHansaPopulationCohortInitialization Cohort;
		Cohort.Id = FHansaPopulationCohortId::TryCreate(1).Value;
		Cohort.ResidenceBuildingId = Residence.Id;
		Cohort.CityId = CityId;
		Cohort.ConsumptionInventoryId = FHansaInventoryId::TryCreate(3).Value;
		Cohort.TierId = FHansaPopulationTierId::TryParse(TEXT("PopulationTier.Laborer")).Value;
		Cohort.Residents = 6;
		Cohort.ResidenceCapacity = 12;
		Initialization.PopulationCohorts.Add(Cohort);

		FHansaCityMarketInitialization Market;
		Market.CityId = CityId;
		Market.GoodId = Bread;
		Market.InventoryIds = { FHansaInventoryId::TryCreate(3).Value };
		Market.DesiredReserve = FHansaQuantity::FromRaw(2'000);
		Market.MinimumPriceMilliMarks = 1;
		Market.MaximumPriceMilliMarks = 10'000;
		Market.InitialPriceMilliMarks = 800;
		Initialization.Markets.Add(Market);
		Initialization.LocalLogisticsSettings.JobCapacity = FHansaQuantity::FromRaw(500);
		Initialization.LocalLogisticsSettings.PickupDelayTicks = 1;
		Initialization.LocalLogisticsSettings.TicksPerRoadCell = 1;
		Initialization.LocalLogisticsSettings.MaximumConcurrentJobs = 4;

		if (!Assign(FHansaSimulationState::TryCreate(MoveTemp(Initialization)), State))
		{
			OutError = TEXT("The integrated Lübeck fixture failed authoritative state initialization.");
			return false;
		}
		PlacementSession.Cancel();
		Cache = FHansaSimulationTransientCache();
		PreviewValidation.Reset();
		LastPlacedBuildingId = Residence.Id;
		FocusedSemanticId.Reset();
		SelectedTool = ESelectedTool::None;
		PrimaryFailure = EHansaPlacementFailure::None;
		bRepeat = false;
		bHasPreview = false;
		bPreviewCanPlace = false;
		IntegratedCheckpoints = FHansaIntegratedLubeckCheckpointState();
		IntegratedCheckpoints.Residents = 6;
		NextCommandId = 1;
		NextBuildingId = 100;
		CameraStateValue = TEXT("focus=-3200,-700;yawDegrees=35;zoomDistance=6500");
		return true;
	}

	void FHansaPlacementAutomationFixture::RegisterSemantics()
	{
		using namespace PlacementFixture;
		using A = EHansaSemanticAction;
		Registry->RegisterNode(Node(TEXT("BuildMode.Screen"), EHansaSemanticRole::Screen, TEXT("Lübeck build mode")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Camera"), EHansaSemanticRole::Status, TEXT("Strategy camera"), TEXT("BuildMode.Screen")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Map"), EHansaSemanticRole::Panel, TEXT("Lübeck placement map"), TEXT("BuildMode.Screen")));

		auto RegisterButton = [this](const TCHAR* Id, const TCHAR* Label, const TCHAR* Parent, TFunction<bool()> Activate)
		{
			const FString StableId(Id);
			Registry->RegisterNode(Node(Id, EHansaSemanticRole::Button, Label, Parent, { A::Activate, A::Focus }),
				Handlers(MoveTemp(Activate), [this, StableId] { FocusIntent(StableId); return true; }));
		};
		RegisterButton(TEXT("BuildMode.Map.RoadTarget"), TEXT("Road target cell 18,16"), TEXT("BuildMode.Map"),
			[this] { return TargetRoadCellIntent(); });
		RegisterButton(TEXT("BuildMode.Map.InvalidTarget"), TEXT("Disconnected target cell 10,10"), TEXT("BuildMode.Map"),
			[this] { return TargetInvalidCellIntent(); });
		RegisterButton(TEXT("BuildMode.Map.ValidTarget"), TEXT("Road-adjacent target cell 16,16"), TEXT("BuildMode.Map"),
			[this] { return TargetValidCellIntent(); });
		Registry->RegisterNode(Node(TEXT("BuildMode.Placement.Preview"), EHansaSemanticRole::Status,
			TEXT("No placement preview"), TEXT("BuildMode.Map")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Placement.Validation"), EHansaSemanticRole::Alert,
			TEXT("Choose a build tool"), TEXT("BuildMode.Map")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Placement.Validation.Cause"), EHansaSemanticRole::Text,
			TEXT("No validation cause"), TEXT("BuildMode.Placement.Validation")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Placement.Validation.Remedy"), EHansaSemanticRole::Text,
			TEXT("Choose a road or building"), TEXT("BuildMode.Placement.Validation")));

		Registry->RegisterNode(Node(TEXT("BuildMode.Toolbar"), EHansaSemanticRole::Panel, TEXT("Build tools"), TEXT("BuildMode.Screen")));
		RegisterButton(TEXT("BuildMode.Tool.Road"), TEXT("Road"), TEXT("BuildMode.Toolbar"),
			[this] { return SelectRoadIntent(); });
		RegisterButton(TEXT("BuildMode.Tool.Warehouse"), TEXT("Warehouse"), TEXT("BuildMode.Toolbar"),
			[this] { return SelectWarehouseIntent(); });
		RegisterButton(TEXT("BuildMode.Action.Rotate"), TEXT("Rotate"), TEXT("BuildMode.Toolbar"),
			[this] { return RotateIntent(); });
		RegisterButton(TEXT("BuildMode.Action.Repeat"), TEXT("Repeat"), TEXT("BuildMode.Toolbar"),
			[this] { return ToggleRepeatIntent(); });
		RegisterButton(TEXT("BuildMode.Action.Confirm"), TEXT("Confirm"), TEXT("BuildMode.Toolbar"),
			[this] { return ConfirmIntent(); });
		RegisterButton(TEXT("BuildMode.Action.Cancel"), TEXT("Cancel"), TEXT("BuildMode.Toolbar"),
			[this] { return CancelIntent(); });
		Registry->RegisterNode(Node(TEXT("BuildMode.Result.Building"), EHansaSemanticRole::Status,
			TEXT("No committed building"), TEXT("BuildMode.Screen")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Construction.Status"), EHansaSemanticRole::Status,
			TEXT("No active construction"), TEXT("BuildMode.Screen")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Construction.Cost"), EHansaSemanticRole::Status,
			TEXT("No selected construction cost"), TEXT("BuildMode.Screen")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Integrated"), EHansaSemanticRole::Panel,
			TEXT("Integrated Lübeck city loop"), TEXT("BuildMode.Screen")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Integrated.Construction"), EHansaSemanticRole::Status,
			TEXT("Integrated construction pending"), TEXT("BuildMode.Integrated")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Integrated.Logistics"), EHansaSemanticRole::Status,
			TEXT("Warehouse delivery pending"), TEXT("BuildMode.Integrated")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Integrated.Production"), EHansaSemanticRole::Status,
			TEXT("Bread production pending"), TEXT("BuildMode.Integrated")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Integrated.Population"), EHansaSemanticRole::Status,
			TEXT("Residence growth pending"), TEXT("BuildMode.Integrated")));
		Registry->RegisterNode(Node(TEXT("BuildMode.Integrated.Bread"), EHansaSemanticRole::Status,
			TEXT("Bread consumption pending"), TEXT("BuildMode.Integrated")));
	}

	bool FHansaPlacementAutomationFixture::SelectRoadIntent()
	{
		if (!bLoaded) return false;
		SelectedTool = ESelectedTool::Road;
		PlacementSession.SelectBuilding(CityId, RoadDefinitionId, true, bRepeat);
		bHasPreview = false;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::SelectWarehouseIntent()
	{
		if (!bLoaded) return false;
		SelectedTool = ESelectedTool::Warehouse;
		PlacementSession.SelectBuilding(CityId, WarehouseDefinitionId, false, bRepeat);
		bHasPreview = false;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::TargetRoadCellIntent()
	{
		if (SelectedTool != ESelectedTool::Road) return false;
		PlacementSession.SetAnchor({ 18, 16 });
		bHasPreview = true;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::TargetInvalidCellIntent()
	{
		if (SelectedTool != ESelectedTool::Warehouse) return false;
		PlacementSession.SetAnchor({ 10, 10 });
		bHasPreview = true;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::TargetValidCellIntent()
	{
		if (SelectedTool != ESelectedTool::Warehouse) return false;
		PlacementSession.SetAnchor({ 16, 16 });
		bHasPreview = true;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::RotateIntent()
	{
		if (SelectedTool != ESelectedTool::Warehouse) return false;
		PlacementSession.RotateClockwise();
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::ToggleRepeatIntent()
	{
		if (SelectedTool == ESelectedTool::None) return false;
		bRepeat = !bRepeat;
		const ESelectedTool Tool = SelectedTool;
		Tool == ESelectedTool::Road ? SelectRoadIntent() : SelectWarehouseIntent();
		return true;
	}

	bool FHansaPlacementAutomationFixture::ConfirmIntent()
	{
		using namespace Hansa::Simulation;
		RefreshPreviewValidation();
		const TArray<FHansaPlacementSpec> Specs = PlacementSession.BuildConfirmationSpecs();
		if (!bPreviewCanPlace || Specs.IsEmpty())
		{
			SynchronizeSemantics();
			return false;
		}

		const FHansaSimulationReadOnlyAccess ReadOnly = State.CreateReadOnlyAccess(Definitions);
		TArray<FHansaGameplayCommand> Commands;
		Commands.Reserve(Specs.Num());
		TArray<FHansaBuildingId> BuildingIds;
		for (int32 Index = 0; Index < Specs.Num(); ++Index)
		{
			const auto CommandId = FHansaCommandId::TryCreate(NextCommandId + Index);
			const auto BuildingId = FHansaBuildingId::TryCreate(NextBuildingId + Index);
			if (!CommandId || !BuildingId)
			{
				return false;
			}
			FHansaCommandHeader Header;
			Header.CommandId = CommandId.Value;
			Header.Authority.IssuingHouseId = HouseId;
			Header.Authority.PrincipalId = 1;
			Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
			Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
			Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + Index + 1;
			Commands.Add(FHansaGameplayCommand::Create(Header,
				FHansaPlaceBuildingCommand { BuildingId.Value, Specs[Index] }));
			BuildingIds.Add(BuildingId.Value);
		}

		const FHansaCommandGatewayResult Result =
			FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
		if (!Result)
		{
			if (Result.GetPlacementValidation().IsSet())
			{
				PreviewValidation = Result.GetPlacementValidation().GetValue();
				PrimaryFailure = PreviewValidation->GetPrimaryFailure();
				bPreviewCanPlace = false;
			}
			SynchronizeSemantics();
			return false;
		}

		NextCommandId += Specs.Num();
		NextBuildingId += Specs.Num();
		LastPlacedBuildingId = BuildingIds.Last();
		PlacementSession.OnConfirmationSucceeded();
		bHasPreview = false;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::CancelIntent()
	{
		if (!bLoaded) return false;
		PlacementSession.Cancel();
		SelectedTool = ESelectedTool::None;
		bHasPreview = false;
		RefreshPreviewValidation();
		SynchronizeSemantics();
		return true;
	}

	void FHansaPlacementAutomationFixture::RefreshPreviewValidation()
	{
		using namespace Hansa::Simulation;
		PreviewValidation.Reset();
		PrimaryFailure = EHansaPlacementFailure::None;
		bPreviewCanPlace = false;
		const TArray<FHansaPlacementSpec> Specs = PlacementSession.BuildConfirmationSpecs();
		if (!bHasPreview || Specs.IsEmpty())
		{
			return;
		}
		bPreviewCanPlace = true;
		const FHansaSimulationReadOnlyAccess ReadOnly = State.CreateReadOnlyAccess(Definitions);
		for (const FHansaPlacementSpec& Spec : Specs)
		{
			FHansaPlacementValidationResult Validation = ReadOnly.ValidatePlacement(HouseId, Spec);
			if (!Validation.CanPlace())
			{
				PreviewValidation = MoveTemp(Validation);
				PrimaryFailure = PreviewValidation->GetPrimaryFailure();
				bPreviewCanPlace = false;
				break;
			}
		}
	}

	void FHansaPlacementAutomationFixture::FocusIntent(const FString& SemanticId)
	{
		FocusedSemanticId = SemanticId;
		SynchronizeSemantics();
	}

	void FHansaPlacementAutomationFixture::UpdateSemanticState(
		const TCHAR* SemanticId,
		FHansaSemanticState SemanticState,
		const FString& Label)
	{
		if (Registry == nullptr) return;
		const FHansaSemanticNode* Existing = Registry->FindNode(SemanticId);
		if (Existing == nullptr) return;
		SemanticState.bFocused = FocusedSemanticId == SemanticId;
		Registry->UpdateNode(SemanticId, SemanticState, Existing->Bounds);
		if (!Label.IsEmpty()) Registry->SetLabel(SemanticId, Label);
	}

	void FHansaPlacementAutomationFixture::SynchronizeSemantics()
	{
		if (!bLoaded || Registry == nullptr) return;
		FHansaSemanticState Default;
		UpdateSemanticState(TEXT("BuildMode.Screen"), Default);
		FHansaSemanticState Camera = Default;
		Camera.ValueType = TEXT("strategy-camera");
		Camera.Value = CameraStateValue;
		UpdateSemanticState(TEXT("BuildMode.Camera"), Camera);
		UpdateSemanticState(TEXT("BuildMode.Map"), Default);

		FHansaSemanticState RoadTarget = Default;
		RoadTarget.bEnabled = SelectedTool == ESelectedTool::Road;
		RoadTarget.ValueType = TEXT("grid-coordinate");
		RoadTarget.Value = TEXT("18,16");
		UpdateSemanticState(TEXT("BuildMode.Map.RoadTarget"), RoadTarget);
		FHansaSemanticState InvalidTarget = Default;
		InvalidTarget.bEnabled = SelectedTool == ESelectedTool::Warehouse;
		InvalidTarget.ValueType = TEXT("grid-coordinate");
		InvalidTarget.Value = TEXT("10,10");
		UpdateSemanticState(TEXT("BuildMode.Map.InvalidTarget"), InvalidTarget);
		FHansaSemanticState ValidTarget = InvalidTarget;
		ValidTarget.Value = TEXT("16,16");
		UpdateSemanticState(TEXT("BuildMode.Map.ValidTarget"), ValidTarget);

		FHansaSemanticState Preview = Default;
		Preview.bVisible = bHasPreview;
		Preview.bSelected = bHasPreview;
		Preview.bError = bHasPreview && !bPreviewCanPlace;
		Preview.ValueType = TEXT("placement-preview");
		const FString ToolName = SelectedTool == ESelectedTool::Road ? TEXT("Building.Road") :
			(SelectedTool == ESelectedTool::Warehouse ? TEXT("Building.Warehouse") : TEXT("None"));
		const FString Anchor = SelectedTool == ESelectedTool::Road ? TEXT("18,16") :
			(PrimaryFailure == Hansa::Simulation::EHansaPlacementFailure::RoadRequired ? TEXT("10,10") : TEXT("16,16"));
		Preview.Value = FString::Printf(TEXT("definition=%s;anchor=%s;rotation=%s;canPlace=%s"),
			*ToolName, *Anchor, Hansa::Simulation::LexToString(PlacementSession.GetRotation()), bPreviewCanPlace ? TEXT("true") : TEXT("false"));
		UpdateSemanticState(TEXT("BuildMode.Placement.Preview"), Preview,
			bHasPreview ? FString::Printf(TEXT("%s preview at %s"), *ToolName, *Anchor) : TEXT("No placement preview"));

		FHansaSemanticState Validation = Default;
		Validation.bVisible = bHasPreview;
		Validation.bError = bHasPreview && !bPreviewCanPlace;
		Validation.bSelected = bHasPreview && bPreviewCanPlace;
		Validation.ValueType = TEXT("placement-validation");
		Validation.Value = bHasPreview ? Hansa::Simulation::LexToString(PrimaryFailure) : TEXT("AwaitingPreview");
		const FString Failure = bHasPreview ? HumanFailure(PrimaryFailure) : TEXT("Choose a target cell");
		const FString Remedy = bHasPreview ? HumanRemedy(PrimaryFailure) : TEXT("Select a visible map target");
		UpdateSemanticState(TEXT("BuildMode.Placement.Validation"), Validation,
			bHasPreview ? FString::Printf(TEXT("%s — %s"), *Failure, *Remedy) : TEXT("Choose a target cell"));
		UpdateSemanticState(TEXT("BuildMode.Placement.Validation.Cause"), Validation, Failure);
		UpdateSemanticState(TEXT("BuildMode.Placement.Validation.Remedy"), Validation, Remedy);

		UpdateSemanticState(TEXT("BuildMode.Toolbar"), Default);
		FHansaSemanticState Road = Default;
		Road.bSelected = SelectedTool == ESelectedTool::Road;
		Road.ValueType = TEXT("building-definition-id");
		Road.Value = TEXT("Building.Road");
		UpdateSemanticState(TEXT("BuildMode.Tool.Road"), Road);
		FHansaSemanticState Warehouse = Default;
		Warehouse.bSelected = SelectedTool == ESelectedTool::Warehouse;
		Warehouse.ValueType = TEXT("building-definition-id");
		Warehouse.Value = TEXT("Building.Warehouse");
		UpdateSemanticState(TEXT("BuildMode.Tool.Warehouse"), Warehouse);
		FHansaSemanticState Rotate = Default;
		Rotate.bEnabled = SelectedTool == ESelectedTool::Warehouse;
		Rotate.ValueType = TEXT("grid-rotation");
		Rotate.Value = Hansa::Simulation::LexToString(PlacementSession.GetRotation());
		UpdateSemanticState(TEXT("BuildMode.Action.Rotate"), Rotate);
		FHansaSemanticState Repeat = Default;
		Repeat.bEnabled = SelectedTool != ESelectedTool::None;
		Repeat.bSelected = bRepeat;
		Repeat.ValueType = TEXT("boolean");
		Repeat.Value = bRepeat ? TEXT("true") : TEXT("false");
		UpdateSemanticState(TEXT("BuildMode.Action.Repeat"), Repeat);
		FHansaSemanticState Confirm = Default;
		Confirm.bEnabled = bPreviewCanPlace;
		Confirm.bSelected = false;
		UpdateSemanticState(TEXT("BuildMode.Action.Confirm"), Confirm);
		FHansaSemanticState Cancel = Default;
		Cancel.bEnabled = SelectedTool != ESelectedTool::None;
		UpdateSemanticState(TEXT("BuildMode.Action.Cancel"), Cancel);

		FHansaSemanticState Result = Default;
		Result.bVisible = LastPlacedBuildingId.IsValid();
		Result.bSelected = LastPlacedBuildingId.IsValid();
		Result.ValueType = TEXT("building-entity-id");
		Result.Value = GetLastPlacedEntityValue();
		UpdateSemanticState(TEXT("BuildMode.Result.Building"), Result,
			LastPlacedBuildingId.IsValid() ? FString::Printf(TEXT("Committed %s"), *Result.Value) : TEXT("No committed building"));

		FHansaSemanticState Construction = Default;
		Construction.bVisible = LastPlacedBuildingId.IsValid();
		Construction.ValueType = TEXT("construction-state");
		const TOptional<Hansa::Simulation::FHansaConstructionProjection> ConstructionProjection =
			QueryConstruction(LastPlacedBuildingId);
		if (ConstructionProjection.IsSet())
		{
			Construction.bSelected = ConstructionProjection->State ==
				Hansa::Simulation::EHansaConstructionState::Completed;
			Construction.Value = FString::Printf(TEXT("building=%s;state=%s;elapsedTicks=%d;totalTicks=%d;progressPpm=%lld"),
				*GetLastPlacedEntityValue(), Hansa::Simulation::LexToString(ConstructionProjection->State),
				ConstructionProjection->ElapsedTicks, ConstructionProjection->TotalTicks,
				static_cast<long long>(ConstructionProjection->Progress.GetPartsPerMillion()));
		}
		UpdateSemanticState(TEXT("BuildMode.Construction.Status"), Construction,
			ConstructionProjection.IsSet() ? FString::Printf(TEXT("%s: %d/%d ticks"),
				Hansa::Simulation::LexToString(ConstructionProjection->State), ConstructionProjection->ElapsedTicks,
				ConstructionProjection->TotalTicks) : TEXT("No active construction"));

		FHansaSemanticState Cost = Default;
		Cost.bVisible = SelectedTool != ESelectedTool::None;
		Cost.ValueType = TEXT("construction-cost");
		const Hansa::Simulation::FHansaBuildingTypeId SelectedDefinition = SelectedTool == ESelectedTool::Road
			? RoadDefinitionId : WarehouseDefinitionId;
		if (SelectedTool != ESelectedTool::None)
		{
			const Hansa::Simulation::FHansaConstructionCostProjection CostProjection =
				QueryConstructionCost(SelectedDefinition);
			Cost.bWarning = !CostProjection.IsAffordable();
			Cost.Value = FString::Printf(TEXT("definition=%s;currency=%lld;missingCurrency=%lld;affordable=%s"),
				*SelectedDefinition.ToString(),
				static_cast<long long>(CostProjection.RequiredCurrency.GetRawValue()),
				static_cast<long long>(CostProjection.MissingCurrency.GetRawValue()),
				CostProjection.IsAffordable() ? TEXT("true") : TEXT("false"));
		}
		UpdateSemanticState(TEXT("BuildMode.Construction.Cost"), Cost,
			SelectedTool != ESelectedTool::None ? Cost.Value : TEXT("No selected construction cost"));

		FHansaSemanticState Integrated = Default;
		Integrated.bVisible = bIntegrated;
		Integrated.ValueType = TEXT("fixture-id");
		Integrated.Value = bIntegrated ? IntegratedFixtureId : TEXT("");
		UpdateSemanticState(TEXT("BuildMode.Integrated"), Integrated);
		FHansaSemanticState IntegratedConstruction = Integrated;
		FHansaSemanticState IntegratedLogistics = Integrated;
		FHansaSemanticState IntegratedProduction = Integrated;
		FHansaSemanticState IntegratedPopulation = Integrated;
		FHansaSemanticState IntegratedBread = Integrated;
		if (bIntegrated)
		{
			IntegratedConstruction.bSelected = IntegratedCheckpoints.bConstructionCompleted;
			IntegratedConstruction.ValueType = TEXT("integrated-construction");
			IntegratedConstruction.Value = IntegratedCheckpoints.bConstructionCompleted
				? TEXT("Completed") : TEXT("UnderConstruction");
			IntegratedLogistics.bSelected = IntegratedCheckpoints.bInventoryMoved;
			IntegratedLogistics.ValueType = TEXT("completed-logistics-jobs");
			IntegratedLogistics.Value = FString::FromInt(IntegratedCheckpoints.CompletedDeliveries);
			IntegratedProduction.bSelected = IntegratedCheckpoints.bProductionCompleted;
			IntegratedProduction.ValueType = TEXT("completed-production-cycles");
			IntegratedProduction.Value = FString::Printf(TEXT("%llu"),
				static_cast<unsigned long long>(IntegratedCheckpoints.CompletedProductionCycles));
			IntegratedPopulation.bSelected = IntegratedCheckpoints.bPopulationGrown;
			IntegratedPopulation.ValueType = TEXT("city-residents");
			IntegratedPopulation.Value = FString::FromInt(IntegratedCheckpoints.Residents);
			IntegratedBread.bSelected = IntegratedCheckpoints.bBreadConsumed;
			IntegratedBread.ValueType = TEXT("bread-consumed-total");
			IntegratedBread.Value = FString::Printf(TEXT("%lld"),
				static_cast<long long>(IntegratedCheckpoints.BreadConsumedTotalMilliUnits));
		}
		UpdateSemanticState(TEXT("BuildMode.Integrated.Construction"), IntegratedConstruction,
			bIntegrated ? IntegratedConstruction.Value : TEXT("Integrated construction unavailable"));
		UpdateSemanticState(TEXT("BuildMode.Integrated.Logistics"), IntegratedLogistics,
			bIntegrated ? FString::Printf(TEXT("Completed deliveries: %s"), *IntegratedLogistics.Value) : TEXT("Warehouse delivery unavailable"));
		UpdateSemanticState(TEXT("BuildMode.Integrated.Production"), IntegratedProduction,
			bIntegrated ? FString::Printf(TEXT("Completed bread cycles: %s"), *IntegratedProduction.Value) : TEXT("Bread production unavailable"));
		UpdateSemanticState(TEXT("BuildMode.Integrated.Population"), IntegratedPopulation,
			bIntegrated ? FString::Printf(TEXT("Residents: %s"), *IntegratedPopulation.Value) : TEXT("Population unavailable"));
		UpdateSemanticState(TEXT("BuildMode.Integrated.Bread"), IntegratedBread,
			bIntegrated ? FString::Printf(TEXT("Bread consumed last tick: %s"), *IntegratedBread.Value) : TEXT("Bread consumption unavailable"));
	}

	int32 FHansaPlacementAutomationFixture::GetPlacedBuildingCount() const
	{
		return bLoaded ? State.CreateReadOnlyAccess(Definitions).GetPlacement().GetPlacements().Num() : 0;
	}

	int64 FHansaPlacementAutomationFixture::GetSimulationTick() const
	{
		return bLoaded ? State.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue() : 0;
	}

	FString FHansaPlacementAutomationFixture::GetLastPlacedEntityValue() const
	{
		return LastPlacedBuildingId.IsValid()
			? FString::Printf(TEXT("Building:%llu:%u"),
				static_cast<unsigned long long>(LastPlacedBuildingId.GetValue()), LastPlacedBuildingId.GetGeneration())
			: FString();
	}

	Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaSimulationProjection>
	FHansaPlacementAutomationFixture::BuildProjection() const
	{
		return bLoaded
			? State.CreateReadOnlyAccess(Definitions).BuildProjection()
			: Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaSimulationProjection>::Failure(
				Hansa::Simulation::EHansaValueError::InvalidFormat);
	}

	void FHansaPlacementAutomationFixture::UpdateIntegratedCheckpoints()
	{
		using namespace Hansa::Simulation;
		if (!bLoaded || !bIntegrated)
		{
			return;
		}
		const THansaValueResult<FHansaSimulationProjection> Projection = BuildProjection();
		if (!Projection)
		{
			return;
		}
		const auto BakeryConstruction = QueryConstruction(FHansaBuildingId::TryCreate(2).Value);
		const auto ResidenceConstruction = QueryConstruction(FHansaBuildingId::TryCreate(3).Value);
		IntegratedCheckpoints.bConstructionCompleted |= BakeryConstruction.IsSet() && ResidenceConstruction.IsSet() &&
			BakeryConstruction->State == EHansaConstructionState::Completed &&
			ResidenceConstruction->State == EHansaConstructionState::Completed;

		int32 CompletedDeliveries = 0;
		for (const FHansaLogisticsJobProjection& Job : Projection.Value.GetLogisticsJobs())
		{
			CompletedDeliveries += Job.Status == EHansaLogisticsJobStatus::Completed ? 1 : 0;
		}
		IntegratedCheckpoints.CompletedDeliveries = FMath::Max(
			IntegratedCheckpoints.CompletedDeliveries, CompletedDeliveries);
		IntegratedCheckpoints.bInventoryMoved |= CompletedDeliveries > 0;

		const auto Productions = Projection.Value.GetProductions();
		const uint64 CompletedCycles = Productions.IsEmpty() ? 0 : Productions[0].CompletedCycles;
		IntegratedCheckpoints.CompletedProductionCycles = FMath::Max(
			IntegratedCheckpoints.CompletedProductionCycles, CompletedCycles);
		IntegratedCheckpoints.bProductionCompleted |= CompletedCycles > 0;

		const auto CityPopulations = Projection.Value.GetCityPopulations();
		IntegratedCheckpoints.Residents = CityPopulations.IsEmpty() ? 0 : CityPopulations[0].TotalResidents;
		IntegratedCheckpoints.bPopulationGrown |= IntegratedCheckpoints.Residents > 6;

		int64 BreadConsumed = 0;
		for (const FHansaPopulationCohortProjection& Cohort : Projection.Value.GetPopulationCohorts())
		{
			for (const FHansaPopulationNeedState& Need : Cohort.Needs)
			{
				if (Need.GoodId.ToString() == TEXT("Good.Bread"))
				{
					BreadConsumed += Need.ConsumedLastTick.GetRawValue();
				}
			}
		}
		IntegratedCheckpoints.BreadConsumedLastTickMilliUnits = BreadConsumed;
		if (BreadConsumed > 0)
		{
			IntegratedCheckpoints.bBreadConsumed = true;
			IntegratedCheckpoints.BreadConsumedTotalMilliUnits += BreadConsumed;
		}
	}

	TOptional<Hansa::Simulation::FHansaConstructionProjection>
	FHansaPlacementAutomationFixture::QueryConstruction(const Hansa::Simulation::FHansaBuildingId BuildingId) const
	{
		return bLoaded && BuildingId.IsValid()
			? State.CreateReadOnlyAccess(Definitions).QueryConstruction(BuildingId)
			: TOptional<Hansa::Simulation::FHansaConstructionProjection>();
	}

	Hansa::Simulation::FHansaConstructionCostProjection
	FHansaPlacementAutomationFixture::QueryConstructionCost(
		const Hansa::Simulation::FHansaBuildingTypeId BuildingDefinitionId) const
	{
		return bLoaded ? State.CreateReadOnlyAccess(Definitions).QueryConstructionCost(
			HouseId, CityId, BuildingDefinitionId) : Hansa::Simulation::FHansaConstructionCostProjection();
	}

	bool FHansaPlacementAutomationFixture::AdvanceTicks(const int32 TickCount)
	{
		if (!bLoaded || TickCount <= 0 || TickCount > 10'000)
		{
			return false;
		}
		for (int32 Index = 0; Index < TickCount; ++Index)
		{
			if (!Hansa::Simulation::FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache))
			{
				return false;
			}
			UpdateIntegratedCheckpoints();
		}
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::CancelLastConstructionIntent()
	{
		using namespace Hansa::Simulation;
		if (!bLoaded || !LastPlacedBuildingId.IsValid())
		{
			return false;
		}
		const FHansaSimulationReadOnlyAccess ReadOnly = State.CreateReadOnlyAccess(Definitions);
		const auto CommandId = FHansaCommandId::TryCreate(NextCommandId);
		if (!CommandId)
		{
			return false;
		}
		FHansaCommandHeader Header;
		Header.CommandId = CommandId.Value;
		Header.Authority.IssuingHouseId = HouseId;
		Header.Authority.PrincipalId = 1;
		Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		const TArray<FHansaGameplayCommand> Commands {
			FHansaGameplayCommand::Create(Header, FHansaCancelConstructionCommand { LastPlacedBuildingId })
		};
		if (!FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache))
		{
			return false;
		}
		++NextCommandId;
		LastPlacedBuildingId = FHansaBuildingId();
		SynchronizeSemantics();
		return true;
	}

	bool FHansaPlacementAutomationFixture::RemoveLastBuildingIntent()
	{
		using namespace Hansa::Simulation;
		if (!bLoaded || !LastPlacedBuildingId.IsValid())
		{
			return false;
		}
		const FHansaSimulationReadOnlyAccess ReadOnly = State.CreateReadOnlyAccess(Definitions);
		const auto CommandId = FHansaCommandId::TryCreate(NextCommandId);
		if (!CommandId)
		{
			return false;
		}
		FHansaCommandHeader Header;
		Header.CommandId = CommandId.Value;
		Header.Authority.IssuingHouseId = HouseId;
		Header.Authority.PrincipalId = 1;
		Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Header.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Header.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		const TArray<FHansaGameplayCommand> Commands {
			FHansaGameplayCommand::Create(Header, FHansaRemoveBuildingCommand { LastPlacedBuildingId })
		};
		if (!FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache))
		{
			return false;
		}
		++NextCommandId;
		LastPlacedBuildingId = FHansaBuildingId();
		SynchronizeSemantics();
		return true;
	}

	void FHansaPlacementAutomationFixture::ObserveCameraState(
		const FVector2D& Focus,
		const float YawDegrees,
		const float ZoomDistance)
	{
		CameraStateValue = FString::Printf(TEXT("focus=%.0f,%.0f;yawDegrees=%.0f;zoomDistance=%.0f"),
			Focus.X, Focus.Y, YawDegrees, ZoomDistance);
	}

	FString FHansaPlacementAutomationFixture::HumanFailure(const Hansa::Simulation::EHansaPlacementFailure Failure)
	{
		switch (Failure)
		{
		case Hansa::Simulation::EHansaPlacementFailure::None: return TEXT("Valid placement");
		case Hansa::Simulation::EHansaPlacementFailure::RoadRequired: return TEXT("Road required");
		case Hansa::Simulation::EHansaPlacementFailure::Occupied: return TEXT("Cell occupied");
		case Hansa::Simulation::EHansaPlacementFailure::OutsideBounds: return TEXT("Outside buildable area");
		case Hansa::Simulation::EHansaPlacementFailure::TerrainNotBuildable: return TEXT("Terrain not buildable");
		default: return Hansa::Simulation::LexToString(Failure);
		}
	}

	FString FHansaPlacementAutomationFixture::HumanRemedy(const Hansa::Simulation::EHansaPlacementFailure Failure)
	{
		switch (Failure)
		{
		case Hansa::Simulation::EHansaPlacementFailure::None: return TEXT("Confirm placement");
		case Hansa::Simulation::EHansaPlacementFailure::RoadRequired: return TEXT("Build next to a road");
		case Hansa::Simulation::EHansaPlacementFailure::Occupied: return TEXT("Choose an empty footprint");
		case Hansa::Simulation::EHansaPlacementFailure::OutsideBounds: return TEXT("Move inside the city boundary");
		case Hansa::Simulation::EHansaPlacementFailure::TerrainNotBuildable: return TEXT("Choose buildable land");
		default: return TEXT("Choose another target");
		}
	}
}
