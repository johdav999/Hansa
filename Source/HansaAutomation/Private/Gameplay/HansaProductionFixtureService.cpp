#include "Gameplay/HansaProductionFixtureService.h"

#include "Inventory/HansaInventory.h"
#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Automation
{
	namespace
	{
		bool TryIntegral(const TSharedRef<FJsonObject>& Object, const TCHAR* Field, int64& OutValue)
		{
			double Value = 0.0;
			if (!Object->TryGetNumberField(Field, Value) || !FMath::IsFinite(Value) ||
				!FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value)))
			{
				return false;
			}
			OutValue = static_cast<int64>(Value);
			return true;
		}

		FString Hex64(const uint64 Value)
		{
			return FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Value));
		}

		bool ParseProductionId(const TSharedRef<FJsonObject>& Object, Hansa::Simulation::FHansaProductionId& OutId)
		{
			int64 Value = 0;
			if (!TryIntegral(Object, TEXT("productionId"), Value) || Value <= 0)
			{
				return false;
			}
			const auto Parsed = Hansa::Simulation::FHansaProductionId::TryCreate(static_cast<uint64>(Value));
			if (!Parsed)
			{
				return false;
			}
			OutId = Parsed.Value;
			return true;
		}

		bool ParseBuildingId(const TSharedRef<FJsonObject>& Object, Hansa::Simulation::FHansaBuildingId& OutId)
		{
			int64 Value = 0;
			if (!TryIntegral(Object, TEXT("buildingId"), Value) || Value <= 0)
			{
				return false;
			}
			const auto Parsed = Hansa::Simulation::FHansaBuildingId::TryCreate(static_cast<uint64>(Value));
			if (!Parsed)
			{
				return false;
			}
			OutId = Parsed.Value;
			return true;
		}

		bool ParseRouteId(const TSharedRef<FJsonObject>& Object, Hansa::Simulation::FHansaRouteId& OutId)
		{
			int64 Value = 0;
			if (!TryIntegral(Object, TEXT("routeId"), Value) || Value <= 0) return false;
			const auto Parsed = Hansa::Simulation::FHansaRouteId::TryCreate(static_cast<uint64>(Value));
			if (!Parsed) return false;
			OutId = Parsed.Value;
			return true;
		}

		bool ParsePopulationCohortId(const TSharedRef<FJsonObject>& Object,
			Hansa::Simulation::FHansaPopulationCohortId& OutId)
		{
			int64 Value = 0;
			if (!TryIntegral(Object, TEXT("populationCohortId"), Value) || Value <= 0)
			{
				return false;
			}
			const auto Parsed = Hansa::Simulation::FHansaPopulationCohortId::TryCreate(
				static_cast<uint64>(Value));
			if (!Parsed)
			{
				return false;
			}
			OutId = Parsed.Value;
			return true;
		}

		bool ParseMarketIds(const TSharedRef<FJsonObject>& Object,
			Hansa::Simulation::FHansaCityDefinitionId& OutCityId,
			Hansa::Simulation::FHansaGoodId& OutGoodId)
		{
			FString CityText;
			FString GoodText;
			if (!Object->TryGetStringField(TEXT("cityId"), CityText) ||
				!Object->TryGetStringField(TEXT("goodId"), GoodText))
			{
				return false;
			}
			const auto CityId = Hansa::Simulation::FHansaCityDefinitionId::TryParse(CityText);
			const auto GoodId = Hansa::Simulation::FHansaGoodId::TryParse(GoodText);
			if (!CityId || !GoodId)
			{
				return false;
			}
			OutCityId = CityId.Value;
			OutGoodId = GoodId.Value;
			return true;
		}

		bool ParseCityId(const TSharedRef<FJsonObject>& Object,
			Hansa::Simulation::FHansaCityDefinitionId& OutCityId)
		{
			FString CityText;
			if (!Object->TryGetStringField(TEXT("cityId"), CityText)) return false;
			const auto Parsed = Hansa::Simulation::FHansaCityDefinitionId::TryParse(CityText);
			if (!Parsed) return false;
			OutCityId = Parsed.Value;
			return true;
		}

		bool ParseOpportunityIds(const TSharedRef<FJsonObject>& Object,
			Hansa::Simulation::FHansaCityDefinitionId& OutSourceCityId,
			Hansa::Simulation::FHansaCityDefinitionId& OutDestinationCityId,
			Hansa::Simulation::FHansaGoodId& OutGoodId)
		{
			FString SourceText;
			FString DestinationText;
			FString GoodText;
			if (!Object->TryGetStringField(TEXT("sourceCityId"), SourceText) ||
				!Object->TryGetStringField(TEXT("destinationCityId"), DestinationText) ||
				!Object->TryGetStringField(TEXT("goodId"), GoodText))
			{
				return false;
			}
			const auto Source = Hansa::Simulation::FHansaCityDefinitionId::TryParse(SourceText);
			const auto Destination = Hansa::Simulation::FHansaCityDefinitionId::TryParse(DestinationText);
			const auto Good = Hansa::Simulation::FHansaGoodId::TryParse(GoodText);
			if (!Source || !Destination || !Good) return false;
			OutSourceCityId = Source.Value;
			OutDestinationCityId = Destination.Value;
			OutGoodId = Good.Value;
			return true;
		}
	}

	TSharedRef<FJsonObject> FHansaProductionFixtureService::ListFixtures() const
	{
		TSharedRef<FJsonObject> Descriptor = MakeShared<FJsonObject>();
		Descriptor->SetStringField(TEXT("fixtureId"), Hansa::Simulation::FHansaProductionFixture::StableFixtureId);
		Descriptor->SetNumberField(TEXT("fixtureVersion"), Hansa::Simulation::FHansaProductionFixture::FixtureVersion);
		Descriptor->SetStringField(TEXT("registryHash"), Hex64(Hansa::Simulation::FHansaProductionFixture::RegistryHash));
		Descriptor->SetStringField(TEXT("purpose"), TEXT("Headless deterministic MVP production chains"));
		TArray<TSharedPtr<FJsonValue>> Fixtures;
		Fixtures.Add(MakeShared<FJsonValueObject>(Descriptor));
		TSharedRef<FJsonObject> Shortage = MakeShared<FJsonObject>();
		Shortage->SetStringField(TEXT("fixtureId"), Hansa::Simulation::FHansaProductionFixture::GrainShortageFixtureId);
		Shortage->SetNumberField(TEXT("fixtureVersion"), Hansa::Simulation::FHansaProductionFixture::FixtureVersion);
		Shortage->SetStringField(TEXT("registryHash"), Hex64(Hansa::Simulation::FHansaProductionFixture::RegistryHash));
		Shortage->SetStringField(TEXT("purpose"), TEXT("Lubeck grain shortage onset, causal inspection, and controlled recovery"));
		Fixtures.Add(MakeShared<FJsonValueObject>(Shortage));
		TSharedRef<FJsonObject> RouteDelivery = MakeShared<FJsonObject>();
		RouteDelivery->SetStringField(TEXT("fixtureId"), Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId);
		RouteDelivery->SetNumberField(TEXT("fixtureVersion"), Hansa::Simulation::FHansaProductionFixture::FixtureVersion);
		RouteDelivery->SetStringField(TEXT("registryHash"), Hex64(Hansa::Simulation::FHansaProductionFixture::RegistryHash));
		RouteDelivery->SetStringField(TEXT("purpose"), TEXT("Lubeck shortage relief by the Rostock cog route with deterministic delivery timing"));
		Fixtures.Add(MakeShared<FJsonValueObject>(RouteDelivery));
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("fixtures"), MoveTemp(Fixtures));
		return Result;
	}

	bool FHansaProductionFixtureService::Load(
		const FString& FixtureId,
		TSharedRef<FJsonObject>& OutPayload,
		FString& OutError)
	{
		if (FixtureId != Hansa::Simulation::FHansaProductionFixture::StableFixtureId &&
			FixtureId != Hansa::Simulation::FHansaProductionFixture::GrainShortageFixtureId &&
			FixtureId != Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId)
		{
			OutError = TEXT("Unknown fixtureId; call fixture_list and use an exact allowlisted identifier.");
			return false;
		}
		const auto Created = FixtureId == Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId
			? Hansa::Simulation::FHansaProductionFixture::TryCreateRouteDelivery()
			: FixtureId == Hansa::Simulation::FHansaProductionFixture::GrainShortageFixtureId
				? Hansa::Simulation::FHansaProductionFixture::TryCreateGrainShortage()
				: Hansa::Simulation::FHansaProductionFixture::TryCreate();
		if (!Created)
		{
			OutError = TEXT("The named production fixture failed deterministic initialization.");
			return false;
		}
		Fixture = Created.Value;
		OutPayload = MakeSummary();
		return true;
	}

	TSharedRef<FJsonObject> FHansaProductionFixtureService::MakeProduction(
		const Hansa::Simulation::FHansaProductionProjection& Production)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("productionId"), static_cast<double>(Production.Id.GetValue()));
		Json->SetNumberField(TEXT("buildingId"), static_cast<double>(Production.BuildingId.GetValue()));
		Json->SetStringField(TEXT("recipeId"), Production.RecipeId.ToString());
		Json->SetBoolField(TEXT("active"), Production.bActive);
		Json->SetNumberField(TEXT("progressTicks"), Production.ProgressTicks);
		Json->SetNumberField(TEXT("cycleTicks"), Production.CycleTicks);
		Json->SetStringField(TEXT("completedCycles"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Production.CompletedCycles)));
		Json->SetStringField(TEXT("blocker"), Hansa::Simulation::LexToString(Production.Blocker));
		Json->SetBoolField(TEXT("usesCityWorkforce"), Production.bUsesCityWorkforce);
		Json->SetNumberField(TEXT("allocatedLaborerWorkforce"), Production.AllocatedLaborerWorkforce);
		Json->SetNumberField(TEXT("requiredLaborerWorkforce"), Production.RequiredLaborerWorkforce);
		Json->SetNumberField(TEXT("allocatedArtisanWorkforce"), Production.AllocatedArtisanWorkforce);
		Json->SetNumberField(TEXT("requiredArtisanWorkforce"), Production.RequiredArtisanWorkforce);
		Json->SetStringField(TEXT("blockingGoodId"), Production.BlockingGoodId.ToString());
		Json->SetNumberField(TEXT("blockingRequiredMilliUnits"), Production.BlockingRequiredQuantity.GetRawValue());
		Json->SetNumberField(TEXT("blockingAvailableMilliUnits"), Production.BlockingAvailableQuantity.GetRawValue());
		return Json;
	}

	TSharedRef<FJsonObject> FHansaProductionFixtureService::MakeSummary(const int32 TicksAdvanced) const
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("loaded"), Fixture.IsSet());
		Result->SetNumberField(TEXT("ticksAdvanced"), TicksAdvanced);
		if (!Fixture.IsSet())
		{
			return Result;
		}
		const auto Projection = Fixture->BuildProjection();
		Result->SetStringField(TEXT("fixtureId"), Fixture->GetFixtureId());
		Result->SetNumberField(TEXT("fixtureVersion"), Fixture->GetFixtureVersion());
		Result->SetStringField(TEXT("registryHash"), Hex64(Fixture->GetRegistryHash()));
		Result->SetStringField(TEXT("stateHash"), Hex64(Fixture->BuildStateHashes().GetOverallHash()));
		Result->SetNumberField(TEXT("eventCount"), Fixture->GetEvents().Num());
		if (Projection)
		{
			Result->SetNumberField(TEXT("tick"), static_cast<double>(Projection.Value.GetClock().GetTick().GetValue()));
			Result->SetNumberField(TEXT("productionCount"), Projection.Value.GetProductions().Num());
			Result->SetNumberField(TEXT("routeCount"), Projection.Value.GetRoutes().Num());
			Result->SetNumberField(TEXT("vehicleCount"), Projection.Value.GetVehicles().Num());
			if (Fixture->GetFixtureId() == Hansa::Simulation::FHansaProductionFixture::RouteDeliveryFixtureId)
			{
				Result->SetNumberField(TEXT("expectedRemoteArrivalTick"), 13);
				Result->SetNumberField(TEXT("expectedLubeckArrivalTick"), 24);
				Result->SetNumberField(TEXT("expectedDeliveryTick"), 25);
				Result->SetNumberField(TEXT("expectedDeliveryTicksAfterActivation"), 22);
			}
		}
		return Result;
	}

	TSharedRef<FJsonObject> FHansaProductionFixtureService::MakeMarket(
		const Hansa::Simulation::FHansaCityMarketProjection& Market)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("cityId"), Market.CityId.ToString());
		Json->SetStringField(TEXT("goodId"), Market.GoodId.ToString());
		Json->SetNumberField(TEXT("stockMilliUnits"), Market.CurrentStock.GetRawValue());
		Json->SetNumberField(TEXT("desiredReserveMilliUnits"), Market.DesiredReserve.GetRawValue());
		Json->SetNumberField(TEXT("citizenDemandMilliUnits"), Market.CitizenDemand.GetRawValue());
		Json->SetNumberField(TEXT("industrialDemandMilliUnits"), Market.IndustrialDemand.GetRawValue());
		Json->SetNumberField(TEXT("localProductionMilliUnits"), Market.RecentLocalProduction.GetRawValue());
		Json->SetNumberField(TEXT("incomingSupplyMilliUnits"), Market.ExpectedIncomingSupply.GetRawValue());
		Json->SetNumberField(TEXT("unmetDemandMilliUnits"), Market.UnmetDemand.GetRawValue());
		Json->SetNumberField(TEXT("priceMilliMarks"), Market.CurrentPriceMilliMarks);
		Json->SetNumberField(TEXT("averagePriceMilliMarks"), Market.RecentAveragePriceMilliMarks);
		Json->SetNumberField(TEXT("lastUpdateTick"), static_cast<double>(Market.LastUpdateTick));
		Json->SetNumberField(TEXT("nextUpdateTick"), static_cast<double>(Market.NextUpdateTick));
		Json->SetNumberField(TEXT("reportAgeTicks"), static_cast<double>(Market.ReportAgeTicks));
		Json->SetBoolField(TEXT("stale"), Market.bIsStale);
		return Json;
	}

	TSharedRef<FJsonObject> FHansaProductionFixtureService::MakeVehicle(
		const Hansa::Simulation::FHansaVehicleProjection& Vehicle)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("vehicleId"), static_cast<double>(Vehicle.Id.GetValue()));
		Json->SetStringField(TEXT("definitionId"), Vehicle.DefinitionId.ToString());
		Json->SetStringField(TEXT("mode"), Hansa::Simulation::LexToString(Vehicle.Mode));
		Json->SetStringField(TEXT("currentCityId"), Vehicle.CurrentCityId.ToString());
		Json->SetNumberField(TEXT("cargoInventoryId"), static_cast<double>(Vehicle.CargoInventoryId.GetValue()));
		Json->SetNumberField(TEXT("cargoMilliUnits"), Vehicle.Cargo.GetRawValue());
		Json->SetNumberField(TEXT("capacityMilliUnits"), Vehicle.Capacity.GetRawValue());
		Json->SetNumberField(TEXT("freeCapacityMilliUnits"), Vehicle.FreeCapacity.GetRawValue());
		Json->SetNumberField(TEXT("accruedUpkeepPfennig"), static_cast<double>(Vehicle.AccruedUpkeepPfennig));
        TSharedRef<FJsonObject> Navigation=MakeShared<FJsonObject>();
        Navigation->SetStringField(TEXT("cityId"),Vehicle.Navigation.CityId.ToString());
        Navigation->SetBoolField(TEXT("moving"),Vehicle.Navigation.IsMoving());
        Navigation->SetNumberField(TEXT("cellX"),Vehicle.Navigation.Cell.X);
        Navigation->SetNumberField(TEXT("cellY"),Vehicle.Navigation.Cell.Y);
        Navigation->SetNumberField(TEXT("homeX"),Vehicle.Navigation.Home.X);
        Navigation->SetNumberField(TEXT("homeY"),Vehicle.Navigation.Home.Y);
        Navigation->SetNumberField(TEXT("remainingCells"),Vehicle.Navigation.Path.Num()-Vehicle.Navigation.NextIndex);
        Json->SetObjectField(TEXT("navigation"),Navigation);
		return Json;
	}

	TSharedRef<FJsonObject> FHansaProductionFixtureService::MakeRoute(
		const Hansa::Simulation::FHansaRouteProjection& Route)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("routeId"), static_cast<double>(Route.Id.GetValue()));
		Json->SetNumberField(TEXT("vehicleId"), static_cast<double>(Route.VehicleId.GetValue()));
		Json->SetStringField(TEXT("definitionId"), Route.RouteDefinitionId.ToString());
		Json->SetStringField(TEXT("mode"), Hansa::Simulation::LexToString(Route.Mode));
		Json->SetStringField(TEXT("lifecycle"), Hansa::Simulation::LexToString(Route.Lifecycle));
		Json->SetNumberField(TEXT("currentStopIndex"), Route.CurrentStopIndex);
		Json->SetNumberField(TEXT("nextStopIndex"), Route.NextStopIndex);
		Json->SetNumberField(TEXT("remainingTravelTicks"), Route.RemainingTravelTicks);
		Json->SetNumberField(TEXT("totalTravelTicks"), Route.TotalTravelTicks);
		Json->SetNumberField(TEXT("completedLegCount"), static_cast<double>(Route.CompletedLegCount));
		Json->SetNumberField(TEXT("missedCargoActionCount"), static_cast<double>(Route.MissedCargoActionCount));
		Json->SetNumberField(TEXT("lastTransferAppliedMilliUnits"), Route.LastTransfer.AppliedQuantity.GetRawValue());
		Json->SetStringField(TEXT("lastTransferKind"), Hansa::Simulation::LexToString(Route.LastTransfer.Kind));
		Json->SetStringField(TEXT("lastTransferCityId"), Route.LastTransfer.CityId.ToString());
		TArray<TSharedPtr<FJsonValue>> Stops;
		for (const Hansa::Simulation::FHansaRouteStop& Stop : Route.Stops)
		{
			TSharedRef<FJsonObject> StopJson = MakeShared<FJsonObject>();
			StopJson->SetStringField(TEXT("cityId"), Stop.CityId.ToString());
			TArray<TSharedPtr<FJsonValue>> Actions;
			for (const Hansa::Simulation::FHansaRouteCargoAction& Action : Stop.Actions)
			{
				TSharedRef<FJsonObject> ActionJson = MakeShared<FJsonObject>();
				ActionJson->SetStringField(TEXT("kind"), Hansa::Simulation::LexToString(Action.Kind));
				ActionJson->SetStringField(TEXT("goodId"), Action.GoodId.ToString());
				ActionJson->SetNumberField(TEXT("quantityLimitMilliUnits"), Action.QuantityLimit.GetRawValue());
				ActionJson->SetNumberField(TEXT("minimumSourceReserveMilliUnits"), Action.MinimumSourceReserve.GetRawValue());
				Actions.Add(MakeShared<FJsonValueObject>(ActionJson));
			}
			StopJson->SetArrayField(TEXT("actions"), MoveTemp(Actions));
			Stops.Add(MakeShared<FJsonValueObject>(StopJson));
		}
		Json->SetArrayField(TEXT("stops"), MoveTemp(Stops));
		return Json;
	}

	bool FHansaProductionFixtureService::Query(
		const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload,
		FString& OutError) const
	{
		if (!Fixture.IsSet())
		{
			OutError = TEXT("No production fixture is loaded; call fixture_load first.");
			return false;
		}
		FString Query;
		if (!Request->TryGetStringField(TEXT("query"), Query))
		{
			OutError = TEXT("gameplay_query requires an allowlisted query name.");
			return false;
		}
		if (Query == TEXT("fixture.summary"))
		{
			OutPayload = MakeSummary();
			return true;
		}
		const auto Projection = Fixture->BuildProjection();
		if (!Projection)
		{
			OutError = TEXT("The fixture projection could not be built.");
			return false;
		}
		if (Query == TEXT("production.list"))
		{
			TArray<TSharedPtr<FJsonValue>> Productions;
			for (const auto& Production : Projection.Value.GetProductions())
			{
				Productions.Add(MakeShared<FJsonValueObject>(MakeProduction(Production)));
			}
			OutPayload->SetArrayField(TEXT("productions"), MoveTemp(Productions));
			return true;
		}
		if (Query == TEXT("production.get"))
		{
			Hansa::Simulation::FHansaProductionId ProductionId;
			if (!ParseProductionId(Request, ProductionId))
			{
				OutError = TEXT("production.get requires a positive integral productionId.");
				return false;
			}
			const auto Found = Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryProduction(ProductionId);
			if (!Found.IsSet())
			{
				OutError = TEXT("The requested productionId does not exist in the loaded fixture.");
				return false;
			}
			OutPayload->SetObjectField(TEXT("production"), MakeProduction(Found.GetValue()));
			return true;
		}
		if (Query == TEXT("route.list"))
		{
			TArray<TSharedPtr<FJsonValue>> Routes;
			for (const auto& Route : Projection.Value.GetRoutes()) Routes.Add(MakeShared<FJsonValueObject>(MakeRoute(Route)));
			OutPayload->SetArrayField(TEXT("routes"), MoveTemp(Routes));
			return true;
		}
		if (Query == TEXT("route.get") || Query == TEXT("route.cargo"))
		{
			Hansa::Simulation::FHansaRouteId RouteId;
			if (!ParseRouteId(Request, RouteId)) { OutError = TEXT("route.get and route.cargo require a positive integral routeId."); return false; }
			const auto Route = Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryRoute(RouteId);
			if (!Route.IsSet()) { OutError = TEXT("The requested routeId does not exist."); return false; }
			OutPayload->SetObjectField(TEXT("route"), MakeRoute(Route.GetValue()));
			const auto Vehicle = Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryVehicle(Route->VehicleId);
			if (Vehicle.IsSet()) OutPayload->SetObjectField(TEXT("vehicle"), MakeVehicle(Vehicle.GetValue()));
			return true;
		}
		if (Query == TEXT("vehicle.list"))
		{
			TArray<TSharedPtr<FJsonValue>> Vehicles;
			for (const auto& Vehicle : Projection.Value.GetVehicles()) Vehicles.Add(MakeShared<FJsonValueObject>(MakeVehicle(Vehicle)));
			OutPayload->SetArrayField(TEXT("vehicles"), MoveTemp(Vehicles));
			return true;
		}
		if (Query == TEXT("route.events"))
		{
			Hansa::Simulation::FHansaRouteId RouteId;
			if (!ParseRouteId(Request, RouteId)) { OutError = TEXT("route.events requires a positive integral routeId."); return false; }
			TArray<TSharedPtr<FJsonValue>> Events;
			for (const auto& Event : Fixture->GetEvents())
			{
				if (Event.GetRouteId() != RouteId) continue;
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("sequence"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Event.GetGlobalSequence())));
				Item->SetNumberField(TEXT("tick"), static_cast<double>(Event.GetTick().GetValue()));
				Item->SetStringField(TEXT("type"), Hansa::Simulation::LexToString(Event.GetType()));
				Item->SetStringField(TEXT("cityId"), Event.GetCityId().ToString());
				Item->SetStringField(TEXT("goodId"), Event.GetGoodId().ToString());
				Item->SetStringField(TEXT("cargoAction"), Hansa::Simulation::LexToString(Event.GetRouteCargoActionKind()));
				Item->SetNumberField(TEXT("appliedMilliUnits"), static_cast<double>(Event.GetValue()));
				Item->SetNumberField(TEXT("requestedMilliUnits"), static_cast<double>(Event.GetRelatedValue()));
				Events.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetStringField(TEXT("stateHash"), Hex64(Fixture->BuildStateHashes().GetOverallHash()));
			OutPayload->SetArrayField(TEXT("events"), MoveTemp(Events));
			return true;
		}
        if(Query==TEXT("inventory.spoilage"))
        {
            TArray<TSharedPtr<FJsonValue>> Rows;
            for(const auto& Loss:Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).GetInventories().QuerySpoilage())
            {
                auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("goodId"),Loss.GoodId.ToString());
                Row->SetNumberField(TEXT("destroyedMilliUnits"),Loss.DestroyedMilliUnits);Row->SetNumberField(TEXT("remainderNumerator"),Loss.RemainderNumerator);
                Rows.Add(MakeShared<FJsonValueObject>(Row));
            }
            OutPayload->SetArrayField(TEXT("spoilage"),Rows);return true;
        }
		if (Query == TEXT("inventory.stock"))
		{
			int64 InventoryValue = 0;
			FString GoodText;
			if (!TryIntegral(Request, TEXT("inventoryId"), InventoryValue) || InventoryValue <= 0 ||
				!Request->TryGetStringField(TEXT("goodId"), GoodText))
			{
				OutError = TEXT("inventory.stock requires inventoryId and canonical goodId.");
				return false;
			}
			const auto InventoryId = Hansa::Simulation::FHansaInventoryId::TryCreate(static_cast<uint64>(InventoryValue));
			const auto GoodId = Hansa::Simulation::FHansaGoodId::TryParse(GoodText);
			if (!InventoryId || !GoodId)
			{
				OutError = TEXT("inventory.stock identifiers are invalid.");
				return false;
			}
			const auto Stock = Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions())
				.GetInventories().QueryStock(InventoryId.Value, GoodId.Value);
			if (!Stock.IsSet())
			{
				OutError = TEXT("The requested inventory/good stock does not exist.");
				return false;
			}
			OutPayload->SetNumberField(TEXT("inventoryId"), static_cast<double>(InventoryValue));
			OutPayload->SetStringField(TEXT("goodId"), GoodText);
			OutPayload->SetNumberField(TEXT("stockMilliUnits"), Stock->Stock.GetRawValue());
			OutPayload->SetNumberField(TEXT("reservedMilliUnits"), Stock->Reserved.GetRawValue());
			OutPayload->SetNumberField(TEXT("availableMilliUnits"), Stock->Available.GetRawValue());
			return true;
		}
		const Hansa::Simulation::FHansaSimulationReadOnlyAccess ReadOnly =
			Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions());
		if (Query == TEXT("logistics.requests"))
		{
			TArray<TSharedPtr<FJsonValue>> Requests;
			for (const auto& RequestProjection : ReadOnly.BuildLogisticsRequestProjection())
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("requestId"), static_cast<double>(RequestProjection.Id.GetValue()));
				Item->SetNumberField(TEXT("sourceInventoryId"), static_cast<double>(RequestProjection.SourceInventoryId.GetValue()));
				Item->SetNumberField(TEXT("destinationInventoryId"), static_cast<double>(RequestProjection.DestinationInventoryId.GetValue()));
				Item->SetStringField(TEXT("goodId"), RequestProjection.GoodId.ToString());
				Item->SetNumberField(TEXT("requestedMilliUnits"), RequestProjection.RequestedQuantity.GetRawValue());
				Item->SetNumberField(TEXT("remainingMilliUnits"), RequestProjection.RemainingQuantity.GetRawValue());
				Item->SetNumberField(TEXT("inFlightMilliUnits"), RequestProjection.InFlightQuantity.GetRawValue());
				Item->SetStringField(TEXT("priority"), Hansa::Simulation::LexToString(RequestProjection.Priority));
				Item->SetStringField(TEXT("status"), Hansa::Simulation::LexToString(RequestProjection.Status));
				Item->SetStringField(TEXT("bottleneck"), Hansa::Simulation::LexToString(RequestProjection.Bottleneck));
				Requests.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("requests"), MoveTemp(Requests));
			return true;
		}
		if (Query == TEXT("logistics.jobs"))
		{
			TArray<TSharedPtr<FJsonValue>> Jobs;
			for (const auto& Job : ReadOnly.BuildLogisticsJobProjection())
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("jobId"), static_cast<double>(Job.Id.GetValue()));
				Item->SetNumberField(TEXT("requestId"), static_cast<double>(Job.RequestId.GetValue()));
				Item->SetNumberField(TEXT("sourceInventoryId"), static_cast<double>(Job.SourceInventoryId.GetValue()));
				Item->SetNumberField(TEXT("destinationInventoryId"), static_cast<double>(Job.DestinationInventoryId.GetValue()));
				Item->SetStringField(TEXT("goodId"), Job.GoodId.ToString());
				Item->SetNumberField(TEXT("quantityMilliUnits"), Job.Quantity.GetRawValue());
				Item->SetNumberField(TEXT("cargoMilliUnits"), Job.CargoQuantity.GetRawValue());
				Item->SetNumberField(TEXT("pickupTick"), static_cast<double>(Job.PickupTick.GetValue()));
				Item->SetNumberField(TEXT("deliveryTick"), static_cast<double>(Job.DeliveryTick.GetValue()));
				Item->SetNumberField(TEXT("roadDistanceCells"), Job.RoadDistanceCells);
				Item->SetNumberField(TEXT("selectedMarketBuildingId"), static_cast<double>(Job.SelectedMarketBuildingId.GetValue()));
				Item->SetNumberField(TEXT("elapsedTravelTicks"), Job.ElapsedTravelTicks);
				Item->SetNumberField(TEXT("remainingTravelTicks"), Job.RemainingTravelTicks);
				Item->SetStringField(TEXT("pauseReason"), Hansa::Simulation::LexToString(Job.PauseReason));
				TArray<TSharedPtr<FJsonValue>> RouteCells;
				for (const Hansa::Simulation::FHansaGridCoordinate Cell : Job.RouteCells)
				{
					TSharedRef<FJsonObject> CellJson = MakeShared<FJsonObject>();
					CellJson->SetNumberField(TEXT("x"), Cell.X);
					CellJson->SetNumberField(TEXT("y"), Cell.Y);
					RouteCells.Add(MakeShared<FJsonValueObject>(CellJson));
				}
				Item->SetArrayField(TEXT("routeCells"), MoveTemp(RouteCells));
				Item->SetStringField(TEXT("status"), Hansa::Simulation::LexToString(Job.Status));
				Jobs.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("jobs"), MoveTemp(Jobs));
			return true;
		}
		if (Query == TEXT("logistics.path"))
		{
			int64 SourceValue = 0;
			int64 DestinationValue = 0;
			if (!TryIntegral(Request, TEXT("sourceInventoryId"), SourceValue) || SourceValue <= 0 ||
				!TryIntegral(Request, TEXT("destinationInventoryId"), DestinationValue) || DestinationValue <= 0)
			{
				OutError = TEXT("logistics.path requires positive integral sourceInventoryId and destinationInventoryId.");
				return false;
			}
			const auto SourceId = Hansa::Simulation::FHansaInventoryId::TryCreate(static_cast<uint64>(SourceValue));
			const auto DestinationId = Hansa::Simulation::FHansaInventoryId::TryCreate(static_cast<uint64>(DestinationValue));
			if (!SourceId || !DestinationId)
			{
				OutError = TEXT("logistics.path inventory identifiers are invalid.");
				return false;
			}
			const auto Path = ReadOnly.QueryLogisticsRoadPath(SourceId.Value, DestinationId.Value);
			OutPayload->SetNumberField(TEXT("sourceInventoryId"), static_cast<double>(SourceValue));
			OutPayload->SetNumberField(TEXT("destinationInventoryId"), static_cast<double>(DestinationValue));
			OutPayload->SetStringField(TEXT("cityId"), Path.CityId.ToString());
			OutPayload->SetBoolField(TEXT("connected"), Path.bConnected);
			OutPayload->SetBoolField(TEXT("marketEligible"), Path.bMarketEligible);
			OutPayload->SetNumberField(TEXT("selectedMarketBuildingId"),
				static_cast<double>(Path.SelectedMarketBuildingId.GetValue()));
			OutPayload->SetStringField(TEXT("failure"), Hansa::Simulation::LexToString(Path.Failure));
			OutPayload->SetStringField(TEXT("messageKey"), Path.MessageKey.ToString());
			OutPayload->SetStringField(TEXT("remedyKey"), Path.RemedyKey.ToString());
			OutPayload->SetNumberField(TEXT("roadDistanceCells"), Path.RoadDistanceCells);
			TArray<TSharedPtr<FJsonValue>> RouteCells;
			for (const Hansa::Simulation::FHansaGridCoordinate Cell : Path.RouteCells)
			{
				TSharedRef<FJsonObject> CellJson = MakeShared<FJsonObject>();
				CellJson->SetNumberField(TEXT("x"), Cell.X);
				CellJson->SetNumberField(TEXT("y"), Cell.Y);
				RouteCells.Add(MakeShared<FJsonValueObject>(CellJson));
			}
			OutPayload->SetArrayField(TEXT("routeCells"), MoveTemp(RouteCells));
			return true;
		}
		if (Query == TEXT("building.market_access"))
		{
			int64 BuildingValue = 0;
			if (!TryIntegral(Request, TEXT("buildingId"), BuildingValue) || BuildingValue <= 0)
			{
				OutError = TEXT("building.market_access requires a positive integral buildingId.");
				return false;
			}
			const auto BuildingId = Hansa::Simulation::FHansaBuildingId::TryCreate(static_cast<uint64>(BuildingValue));
			const Hansa::Simulation::FHansaBuildingWorldProjection* Building = BuildingId
				? Projection.Value.GetBuildingWorldProjections().FindByPredicate(
					[&BuildingId](const Hansa::Simulation::FHansaBuildingWorldProjection& Candidate)
					{ return Candidate.BuildingId == BuildingId.Value; })
				: nullptr;
			if (Building == nullptr)
			{
				OutError = TEXT("The requested buildingId does not exist in the loaded fixture.");
				return false;
			}
			OutPayload->SetNumberField(TEXT("buildingId"), static_cast<double>(BuildingValue));
			OutPayload->SetStringField(TEXT("buildingDefinitionId"), Building->Placement.BuildingDefinitionId.ToString());
			OutPayload->SetStringField(TEXT("cityId"), Building->Placement.CityId.ToString());
			OutPayload->SetBoolField(TEXT("roadRequired"), Building->bRequiresRoad);
			OutPayload->SetBoolField(TEXT("roadConnected"), Building->bHasRoadAccess);
			OutPayload->SetStringField(TEXT("roadFailure"),
				Hansa::Simulation::LexToString(Building->RoadAccessFailure));
			OutPayload->SetBoolField(TEXT("marketConnected"), Building->bHasMarketAccess);
			// Compatibility: connected historically meant market eligibility.
			OutPayload->SetBoolField(TEXT("connected"), Building->bHasMarketAccess);
			OutPayload->SetStringField(TEXT("failure"), Hansa::Simulation::LexToString(Building->MarketAccessFailure));
			OutPayload->SetStringField(TEXT("messageKey"), Building->MarketAccessMessageKey.ToString());
			OutPayload->SetStringField(TEXT("remedyKey"), Building->MarketAccessRemedyKey.ToString());
			OutPayload->SetNumberField(TEXT("selectedMarketBuildingId"), static_cast<double>(Building->SelectedMarketBuildingId.GetValue()));
			OutPayload->SetNumberField(TEXT("roadDistanceCells"), Building->MarketRoadDistanceCells);
			OutPayload->SetBoolField(TEXT("deliveryBlocked"), Building->bDeliveryBlocked);
			OutPayload->SetNumberField(TEXT("blockedDeliveryCount"), Building->BlockedDeliveryCount);
			OutPayload->SetStringField(TEXT("deliveryFailure"), Hansa::Simulation::LexToString(Building->DeliveryFailure));
			return true;
		}
		if (Query == TEXT("city.population"))
		{
			Hansa::Simulation::FHansaCityDefinitionId CityId;
			if (!ParseCityId(Request, CityId))
			{
				OutError = TEXT("city.population requires a canonical cityId.");
				return false;
			}
			const auto City = ReadOnly.QueryCityPopulation(CityId);
			if (!City.IsSet())
			{
				OutError = TEXT("The requested city does not exist.");
				return false;
			}
			OutPayload->SetStringField(TEXT("cityId"), City->CityId.ToString());
			OutPayload->SetNumberField(TEXT("totalResidents"), City->TotalResidents);
			OutPayload->SetNumberField(TEXT("residentChangeLastTick"), City->ResidentChangeLastTick);
			OutPayload->SetStringField(TEXT("trend"), Hansa::Simulation::LexToString(City->Trend));
			OutPayload->SetNumberField(TEXT("housingCapacity"), City->HousingCapacity);
			OutPayload->SetNumberField(TEXT("laborerResidents"), City->LaborerResidents);
			OutPayload->SetNumberField(TEXT("artisanResidents"), City->ArtisanResidents);
			OutPayload->SetNumberField(TEXT("laborerWorkforceSupply"), City->LaborerWorkforceSupply);
			OutPayload->SetNumberField(TEXT("laborerWorkforceAssigned"), City->LaborerWorkforceAssigned);
			OutPayload->SetNumberField(TEXT("laborerWorkforceAvailable"), City->LaborerWorkforceAvailable);
			OutPayload->SetNumberField(TEXT("artisanWorkforceSupply"), City->ArtisanWorkforceSupply);
			OutPayload->SetNumberField(TEXT("artisanWorkforceAssigned"), City->ArtisanWorkforceAssigned);
			OutPayload->SetNumberField(TEXT("artisanWorkforceAvailable"), City->ArtisanWorkforceAvailable);
			OutPayload->SetNumberField(TEXT("satisfactionBasisPoints"), City->SatisfactionBasisPoints);
			OutPayload->SetNumberField(TEXT("stapleReserveMilliDays"), static_cast<double>(City->StapleReserveMilliDays));
			OutPayload->SetBoolField(TEXT("hasMarketAccess"), City->bHasMarketAccess);
			return true;
		}
		if (Query == TEXT("population.cohort"))
		{
			Hansa::Simulation::FHansaPopulationCohortId CohortId;
			if (!ParsePopulationCohortId(Request, CohortId))
			{
				OutError = TEXT("population.cohort requires a positive integral populationCohortId.");
				return false;
			}
			const auto Cohort = ReadOnly.QueryPopulationCohort(CohortId);
			if (!Cohort.IsSet())
			{
				OutError = TEXT("The requested population cohort does not exist.");
				return false;
			}
			OutPayload->SetNumberField(TEXT("populationCohortId"), static_cast<double>(Cohort->Id.GetValue()));
			OutPayload->SetNumberField(TEXT("residenceBuildingId"), static_cast<double>(Cohort->ResidenceBuildingId.GetValue()));
			OutPayload->SetStringField(TEXT("cityId"), Cohort->CityId.ToString());
			OutPayload->SetNumberField(TEXT("consumptionInventoryId"), static_cast<double>(Cohort->ConsumptionInventoryId.GetValue()));
			OutPayload->SetStringField(TEXT("tierId"), Cohort->TierId.ToString());
			OutPayload->SetNumberField(TEXT("residents"), Cohort->Residents);
			OutPayload->SetNumberField(TEXT("residenceCapacity"), Cohort->ResidenceCapacity);
			OutPayload->SetBoolField(TEXT("residenceOperational"), Cohort->bResidenceOperational);
			OutPayload->SetBoolField(TEXT("hasMarketAccess"), Cohort->bHasMarketAccess);
			OutPayload->SetNumberField(TEXT("workforceSupply"), Cohort->WorkforceSupply);
			OutPayload->SetNumberField(TEXT("accessBasisPoints"), Cohort->AccessBasisPoints);
			OutPayload->SetNumberField(TEXT("affordabilityBasisPoints"), Cohort->AffordabilityBasisPoints);
			OutPayload->SetNumberField(TEXT("reliabilityBasisPoints"), Cohort->ReliabilityBasisPoints);
			OutPayload->SetNumberField(TEXT("satisfactionBasisPoints"), Cohort->SatisfactionBasisPoints);
			OutPayload->SetNumberField(TEXT("residentChangeLastTick"), Cohort->ResidentChangeLastTick);
            TSharedRef<FJsonObject> Consumption = MakeShared<FJsonObject>();
            Consumption->SetNumberField(TEXT("coveredMinutes"), Cohort->Consumption.CoveredMinutes);
            Consumption->SetBoolField(TEXT("fullWindow"), Cohort->Consumption.bFullWindow);
            Consumption->SetBoolField(TEXT("known"), Cohort->Consumption.CoveredMinutes > 0);
            TArray<TSharedPtr<FJsonValue>> ConsumedGoods;
            for (const auto& Total : Cohort->Consumption.Goods)
            {
                TSharedRef<FJsonObject> Good = MakeShared<FJsonObject>();
                Good->SetStringField(TEXT("goodId"), Total.GoodId.ToString());
                Good->SetNumberField(TEXT("requiredMilliUnits"), Total.Required);
                Good->SetNumberField(TEXT("consumedMilliUnits"), Total.Consumed);
                if (Total.Required > 0) Good->SetNumberField(TEXT("percent"), 100.0 * (double(Total.Consumed) / Total.Required));
                ConsumedGoods.Add(MakeShared<FJsonValueObject>(Good));
            }
            Consumption->SetArrayField(TEXT("goods"), MoveTemp(ConsumedGoods));
            OutPayload->SetObjectField(TEXT("consumption30Days"), Consumption);
			TArray<TSharedPtr<FJsonValue>> Needs;
			for (const Hansa::Simulation::FHansaPopulationNeedState& Need : Cohort->Needs)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("needId"), Need.NeedId.ToString());
				Item->SetStringField(TEXT("goodId"), Need.GoodId.ToString());
				Item->SetNumberField(TEXT("requiredLastTickMilliUnits"), Need.RequiredLastTick.GetRawValue());
				Item->SetNumberField(TEXT("consumedLastTickMilliUnits"), Need.ConsumedLastTick.GetRawValue());
				Item->SetNumberField(TEXT("accessBasisPoints"), Need.AccessBasisPoints);
				Item->SetNumberField(TEXT("affordabilityBasisPoints"), Need.AffordabilityBasisPoints);
				Item->SetNumberField(TEXT("reliabilityBasisPoints"), Need.ReliabilityBasisPoints);
				Item->SetNumberField(TEXT("satisfactionBasisPoints"), Need.SatisfactionBasisPoints);
				Item->SetNumberField(TEXT("reserveMilliDays"), static_cast<double>(Need.ReserveMilliDays));
				Needs.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("needs"), MoveTemp(Needs));
			return true;
		}
		Hansa::Simulation::FHansaCityDefinitionId CityId;
		Hansa::Simulation::FHansaGoodId GoodId;
		if (Query == TEXT("market.opportunity"))
		{
			Hansa::Simulation::FHansaCityDefinitionId SourceCityId;
			Hansa::Simulation::FHansaCityDefinitionId DestinationCityId;
			if (!ParseOpportunityIds(Request, SourceCityId, DestinationCityId, GoodId))
			{
				OutError = TEXT("market.opportunity requires canonical sourceCityId, destinationCityId and goodId fields.");
				return false;
			}
			const auto Opportunity = ReadOnly.CompareMarketOpportunity(SourceCityId, DestinationCityId, GoodId);
			if (!Opportunity.IsSet())
			{
				OutError = TEXT("The requested market pair does not exist or names the same city twice.");
				return false;
			}
			OutPayload->SetStringField(TEXT("sourceCityId"), SourceCityId.ToString());
			OutPayload->SetStringField(TEXT("destinationCityId"), DestinationCityId.ToString());
			OutPayload->SetStringField(TEXT("goodId"), GoodId.ToString());
			OutPayload->SetStringField(TEXT("sourceInformationState"),
				Hansa::Simulation::LexToString(Opportunity->SourceInformationState));
			OutPayload->SetStringField(TEXT("destinationInformationState"),
				Hansa::Simulation::LexToString(Opportunity->DestinationInformationState));
			OutPayload->SetBoolField(TEXT("comparable"), Opportunity->bComparable);
			if (Opportunity->SourcePriceMilliMarks.IsSet())
				OutPayload->SetNumberField(TEXT("sourcePriceMilliMarks"), Opportunity->SourcePriceMilliMarks.GetValue());
			if (Opportunity->DestinationPriceMilliMarks.IsSet())
				OutPayload->SetNumberField(TEXT("destinationPriceMilliMarks"), Opportunity->DestinationPriceMilliMarks.GetValue());
			if (Opportunity->GrossMarginMilliMarks.IsSet())
				OutPayload->SetNumberField(TEXT("grossMarginMilliMarks"), Opportunity->GrossMarginMilliMarks.GetValue());
			if (Opportunity->SourceAvailableAboveReserve.IsSet())
				OutPayload->SetNumberField(TEXT("sourceAvailableAboveReserveMilliUnits"),
					Opportunity->SourceAvailableAboveReserve.GetValue().GetRawValue());
			if (Opportunity->DestinationDemandGap.IsSet())
				OutPayload->SetNumberField(TEXT("destinationDemandGapMilliUnits"),
					Opportunity->DestinationDemandGap.GetValue().GetRawValue());
			return true;
		}
		if (Query.StartsWith(TEXT("market.")) && !ParseMarketIds(Request, CityId, GoodId))
		{
			OutError = TEXT("Market queries require canonical cityId and goodId fields.");
			return false;
		}
		if (Query == TEXT("market.report_age"))
		{
			const auto Age = ReadOnly.QueryMarketReportAge(CityId, GoodId);
			if (!Age.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetStringField(TEXT("cityId"), CityId.ToString());
			OutPayload->SetStringField(TEXT("goodId"), GoodId.ToString());
			OutPayload->SetStringField(TEXT("informationState"), Hansa::Simulation::LexToString(Age->InformationState));
			OutPayload->SetBoolField(TEXT("hasReport"), Age->ReportTick.IsSet());
			if (Age->ReportTick.IsSet()) OutPayload->SetNumberField(TEXT("reportTick"), Age->ReportTick.GetValue());
			if (Age->MarketUpdateTick.IsSet()) OutPayload->SetNumberField(TEXT("marketUpdateTick"), Age->MarketUpdateTick.GetValue());
			if (Age->AgeTicks.IsSet()) OutPayload->SetNumberField(TEXT("ageTicks"), Age->AgeTicks.GetValue());
			return true;
		}
		if (Query == TEXT("market.known_price"))
		{
			const auto Price = ReadOnly.QueryKnownMarketPrice(CityId, GoodId);
			if (!Price.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetStringField(TEXT("cityId"), CityId.ToString());
			OutPayload->SetStringField(TEXT("goodId"), GoodId.ToString());
			OutPayload->SetStringField(TEXT("informationState"), Hansa::Simulation::LexToString(Price->InformationState));
			OutPayload->SetBoolField(TEXT("known"), Price->PriceMilliMarks.IsSet());
			if (Price->PriceMilliMarks.IsSet()) OutPayload->SetNumberField(TEXT("priceMilliMarks"), Price->PriceMilliMarks.GetValue());
			if (Price->RecentAveragePriceMilliMarks.IsSet()) OutPayload->SetNumberField(TEXT("averagePriceMilliMarks"), Price->RecentAveragePriceMilliMarks.GetValue());
			if (Price->ReportTick.IsSet()) OutPayload->SetNumberField(TEXT("reportTick"), Price->ReportTick.GetValue());
			if (Price->ReportAgeTicks.IsSet()) OutPayload->SetNumberField(TEXT("reportAgeTicks"), Price->ReportAgeTicks.GetValue());
			return true;
		}
		if (Query == TEXT("market.known_components"))
		{
			const auto Components = ReadOnly.QueryKnownMarketSupplyDemand(CityId, GoodId);
			if (!Components.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetStringField(TEXT("cityId"), CityId.ToString());
			OutPayload->SetStringField(TEXT("goodId"), GoodId.ToString());
			OutPayload->SetStringField(TEXT("informationState"), Hansa::Simulation::LexToString(Components->InformationState));
			OutPayload->SetBoolField(TEXT("known"), Components->Stock.IsSet());
			if (Components->Stock.IsSet()) OutPayload->SetNumberField(TEXT("stockMilliUnits"), Components->Stock.GetValue().GetRawValue());
			if (Components->DesiredReserve.IsSet()) OutPayload->SetNumberField(TEXT("desiredReserveMilliUnits"), Components->DesiredReserve.GetValue().GetRawValue());
			if (Components->CitizenDemand.IsSet()) OutPayload->SetNumberField(TEXT("citizenDemandMilliUnits"), Components->CitizenDemand.GetValue().GetRawValue());
			if (Components->IndustrialDemand.IsSet()) OutPayload->SetNumberField(TEXT("industrialDemandMilliUnits"), Components->IndustrialDemand.GetValue().GetRawValue());
			if (Components->TotalDemand.IsSet()) OutPayload->SetNumberField(TEXT("totalDemandMilliUnits"), Components->TotalDemand.GetValue().GetRawValue());
			if (Components->RecentLocalProduction.IsSet()) OutPayload->SetNumberField(TEXT("localProductionMilliUnits"), Components->RecentLocalProduction.GetValue().GetRawValue());
			if (Components->ExpectedIncomingSupply.IsSet()) OutPayload->SetNumberField(TEXT("incomingSupplyMilliUnits"), Components->ExpectedIncomingSupply.GetValue().GetRawValue());
			if (Components->UnmetDemand.IsSet()) OutPayload->SetNumberField(TEXT("unmetDemandMilliUnits"), Components->UnmetDemand.GetValue().GetRawValue());
			return true;
		}
		if (Query == TEXT("market.price") || Query == TEXT("market.components"))
		{
			const auto Market = ReadOnly.QueryMarket(CityId, GoodId);
			if (!Market.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetObjectField(TEXT("market"), MakeMarket(Market.GetValue()));
			if (Query == TEXT("market.components"))
			{
                const auto ConsumptionView = ReadOnly.BuildProjection();
                if (!ConsumptionView) { OutError = TEXT("Consumption projection unavailable."); return false; }
                const auto& Consumption = ConsumptionView.Value.GetCitizenConsumption();
                TSharedRef<FJsonObject> Fulfillment = MakeShared<FJsonObject>();
                Fulfillment->SetNumberField(TEXT("coveredMinutes"), Consumption.CoveredMinutes);
                Fulfillment->SetBoolField(TEXT("fullWindow"), Consumption.bFullWindow);
                Fulfillment->SetBoolField(TEXT("known"), Consumption.CoveredMinutes > 0);
                int64 Required = 0, Consumed = 0;
                for (const auto& Total : Consumption.Goods)
                    if (Total.CityId == CityId && Total.GoodId == GoodId) { Required = Total.Required; Consumed = Total.Consumed; break; }
                Fulfillment->SetNumberField(TEXT("requiredMilliUnits"), Required);
                Fulfillment->SetNumberField(TEXT("consumedMilliUnits"), Consumed);
                if (Consumption.CoveredMinutes > 0 && Required > 0)
                    Fulfillment->SetNumberField(TEXT("percent"), 100.0 * (double(Consumed) / Required));
                OutPayload->SetObjectField(TEXT("citizenFulfillment30Days"), Fulfillment);
				TSharedRef<FJsonObject> Factors = MakeShared<FJsonObject>();
				Factors->SetNumberField(TEXT("scarcityBasisPoints"), Market->Factors.ScarcityBasisPoints);
				Factors->SetNumberField(TEXT("citizenDemandBasisPoints"), Market->Factors.CitizenDemandBasisPoints);
				Factors->SetNumberField(TEXT("industrialDemandBasisPoints"), Market->Factors.IndustrialDemandBasisPoints);
				Factors->SetNumberField(TEXT("incomingSupplyBasisPoints"), Market->Factors.IncomingSupplyBasisPoints);
				Factors->SetNumberField(TEXT("unmetDemandBasisPoints"), Market->Factors.UnmetDemandBasisPoints);
				Factors->SetNumberField(TEXT("seasonModifierBasisPoints"), Market->Factors.SeasonModifierBasisPoints);
				Factors->SetNumberField(TEXT("cityModifierBasisPoints"), Market->Factors.CityModifierBasisPoints);
				Factors->SetNumberField(TEXT("targetMultiplierBasisPoints"), Market->Factors.TargetMultiplierBasisPoints);
				OutPayload->SetObjectField(TEXT("factors"), Factors);
			}
			return true;
		}
		if (Query == TEXT("market.history"))
		{
			TArray<TSharedPtr<FJsonValue>> History;
			for (const auto& Entry : ReadOnly.QueryMarketPriceHistory(CityId, GoodId))
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetNumberField(TEXT("tick"), static_cast<double>(Entry.Tick.GetValue()));
				Item->SetNumberField(TEXT("stockMilliUnits"), Entry.Stock.GetRawValue());
				Item->SetNumberField(TEXT("citizenDemandMilliUnits"), Entry.CitizenDemand.GetRawValue());
				Item->SetNumberField(TEXT("industrialDemandMilliUnits"), Entry.IndustrialDemand.GetRawValue());
				Item->SetNumberField(TEXT("unmetDemandMilliUnits"), Entry.UnmetDemand.GetRawValue());
				Item->SetNumberField(TEXT("priceMilliMarks"), Entry.PriceMilliMarks);
				History.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("history"), MoveTemp(History));
			return true;
		}
		if (Query == TEXT("market.reserve"))
		{
			const auto Reserve = ReadOnly.QueryMarketReserveDays(CityId, GoodId);
			if (!Reserve.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetNumberField(TEXT("stockMilliUnits"), Reserve->Stock.GetRawValue());
			OutPayload->SetNumberField(TEXT("demandPerTickMilliUnits"), Reserve->DemandPerTick.GetRawValue());
			OutPayload->SetNumberField(TEXT("reserveMilliDays"), static_cast<double>(Reserve->ReserveMilliDays));
			OutPayload->SetBoolField(TEXT("hasDemand"), Reserve->bHasDemand);
			return true;
		}
		if (Query == TEXT("market.explanation"))
		{
			const auto Explanation = ReadOnly.QueryMarketExplanation(CityId, GoodId);
			if (!Explanation.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetNumberField(TEXT("baseMultiplierBasisPoints"), Explanation->BaseMultiplierBasisPoints);
			OutPayload->SetNumberField(TEXT("rawMultiplierBasisPoints"), Explanation->RawMultiplierBasisPoints);
			OutPayload->SetNumberField(TEXT("targetMultiplierBasisPoints"), Explanation->TargetMultiplierBasisPoints);
			TArray<TSharedPtr<FJsonValue>> Factors;
			for (const auto& Factor : Explanation->Factors)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("factor"), Hansa::Simulation::LexToString(Factor.Factor));
				Item->SetStringField(TEXT("messageKey"), Factor.MessageKey.ToString());
				Item->SetStringField(TEXT("message"), Factor.Message.ToString());
				Item->SetNumberField(TEXT("contributionBasisPoints"), Factor.ContributionBasisPoints);
				Factors.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("factors"), MoveTemp(Factors));
			return true;
		}
		if (Query == TEXT("market.alerts"))
		{
			TArray<TSharedPtr<FJsonValue>> Alerts;
			for (const auto& Alert : ReadOnly.QueryMarketAlerts(CityId, GoodId))
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("type"), Hansa::Simulation::LexToString(Alert.Type));
				Item->SetStringField(TEXT("severity"), Hansa::Simulation::LexToString(Alert.Severity));
				Item->SetStringField(TEXT("causeMessageKey"), Alert.CauseMessageKey.ToString());
				Item->SetStringField(TEXT("cause"), Alert.Cause.ToString());
				Item->SetNumberField(TEXT("activeSinceTick"), static_cast<double>(Alert.ActiveSinceTick));
				Item->SetNumberField(TEXT("ageTicks"), static_cast<double>(Alert.AgeTicks));
				Alerts.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("alerts"), MoveTemp(Alerts));
			return true;
		}
		if (Query == TEXT("market.consumers"))
		{
			TArray<TSharedPtr<FJsonValue>> Consumers;
			for (const auto& Consumer : ReadOnly.QueryMarketConsumers(CityId, GoodId))
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("kind"), Hansa::Simulation::LexToString(Consumer.Kind));
				Item->SetNumberField(TEXT("populationCohortId"), static_cast<double>(Consumer.PopulationCohortId.GetValue()));
				Item->SetNumberField(TEXT("productionId"), static_cast<double>(Consumer.ProductionId.GetValue()));
				Item->SetStringField(TEXT("recipeId"), Consumer.RecipeId.ToString());
				Item->SetNumberField(TEXT("demandPerTickMilliUnits"), Consumer.DemandPerTick.GetRawValue());
				Item->SetStringField(TEXT("productionBlocker"), Hansa::Simulation::LexToString(Consumer.ProductionBlocker));
				Consumers.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("consumers"), MoveTemp(Consumers));
			return true;
		}
		if (Query == TEXT("market.producers"))
		{
			TArray<TSharedPtr<FJsonValue>> Producers;
			for (const auto& Producer : ReadOnly.QueryMarketProducers(CityId, GoodId))
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("kind"), Hansa::Simulation::LexToString(Producer.Kind));
				Item->SetNumberField(TEXT("productionId"), static_cast<double>(Producer.ProductionId.GetValue()));
				Item->SetStringField(TEXT("recipeId"), Producer.RecipeId.ToString());
				Item->SetNumberField(TEXT("nominalQuantityPerCycle"), Producer.NominalQuantityPerCycle.GetRawValue());
				Item->SetBoolField(TEXT("active"), Producer.bActive);
				Item->SetStringField(TEXT("blocker"), Hansa::Simulation::LexToString(Producer.Blocker));
				Producers.Add(MakeShared<FJsonValueObject>(Item));
			}
			OutPayload->SetArrayField(TEXT("producers"), MoveTemp(Producers));
			return true;
		}
		OutError = TEXT("Query is not allowlisted. Use fixture.summary, building.market_access, production.*, route.*, vehicle.list, inventory.stock, logistics.*, city.population, population.cohort, or documented market.* queries.");
		return false;
	}

	bool FHansaProductionFixtureService::Command(
		const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload,
		FString& OutError)
	{
		if (!Fixture.IsSet()) { OutError = TEXT("No fixture is loaded; call fixture_load first."); return false; }
		FString CommandName;
		if (!Request->TryGetStringField(TEXT("command"), CommandName))
		{
			OutError = TEXT("gameplay_command requires an allowlisted command name.");
			return false;
		}
        if (CommandName == TEXT("production.set_mode") || CommandName == TEXT("production.upgrade") || CommandName == TEXT("market.set_preserved_fish_household_availability"))
        {
            using namespace Hansa::Simulation;
            FHansaCommandGatewayResult Result;
            if(CommandName == TEXT("market.set_preserved_fish_household_availability"))
            {
                FHansaBuildingId Building; bool Available;
                if(!ParseBuildingId(Request,Building) || !Request->TryGetBoolField(TEXT("available"),Available)) { OutError=TEXT("Requires buildingId and available.");return false; }
                Result=Fixture->SetHouseholdAvailability(Building,Available);
            }
            else
            {
                FHansaProductionId Production;
                if(!ParseProductionId(Request,Production)) { OutError=TEXT("Requires productionId.");return false; }
                if(CommandName==TEXT("production.upgrade")) Result=Fixture->UpgradeProduction(Production);
                else
                {
                    FString Recipe;bool Fallback;
                    if(!Request->TryGetStringField(TEXT("recipeId"),Recipe) || !Request->TryGetBoolField(TEXT("fallbackToFresh"),Fallback)) { OutError=TEXT("Requires recipeId and fallbackToFresh.");return false; }
                    const auto Id=FHansaRecipeId::TryParse(Recipe);
                    if(!Id){OutError=TEXT("Invalid recipeId.");return false;}
                    Result=Fixture->SetProductionMode(Production,Id.Value,Fallback);
                }
            }
            if(!Result){OutError=FString::Printf(TEXT("Command rejected: %s"),LexToString(Result.GetError()));return false;}
            OutPayload=MakeSummary(1);OutPayload->SetStringField(TEXT("command"),CommandName);return true;
        }
		if (CommandName == TEXT("production.set_active"))
		{
			Hansa::Simulation::FHansaProductionId ProductionId;
			bool bActive = false;
			if (!ParseProductionId(Request, ProductionId) || !Request->TryGetBoolField(TEXT("active"), bActive))
			{
				OutError = TEXT("production.set_active requires productionId and active.");
				return false;
			}
			const auto Result = Fixture->SetProductionActive(ProductionId, bActive);
			if (!Result)
			{
				OutError = FString::Printf(TEXT("The authoritative command gateway rejected the command: %s."),
					Hansa::Simulation::LexToString(Result.GetError()));
				return false;
			}
			OutPayload = MakeSummary(1);
			OutPayload->SetStringField(TEXT("command"), CommandName);
			OutPayload->SetNumberField(TEXT("productionId"), static_cast<double>(ProductionId.GetValue()));
			OutPayload->SetBoolField(TEXT("active"), bActive);
			return true;
		}
		if (CommandName == TEXT("residence.upgrade"))
		{
			Hansa::Simulation::FHansaBuildingId BuildingId;
			if (!ParseBuildingId(Request, BuildingId))
			{
				OutError = TEXT("residence.upgrade requires a positive integral buildingId.");
				return false;
			}
			const auto Result = Fixture->UpgradeResidence(BuildingId);
			if (!Result)
			{
				OutError = FString::Printf(TEXT("The authoritative command gateway rejected the command: %s."),
					Hansa::Simulation::LexToString(Result.GetError()));
				return false;
			}
			OutPayload = MakeSummary(1);
			OutPayload->SetStringField(TEXT("command"), CommandName);
			OutPayload->SetNumberField(TEXT("buildingId"), static_cast<double>(BuildingId.GetValue()));
			return true;
		}
		if (CommandName == TEXT("route.edit"))
		{
			Hansa::Simulation::FHansaRouteId RouteId;
			FString SourceText, DestinationText, GoodText;
			int64 Quantity = 0, Reserve = 0;
			if (!ParseRouteId(Request, RouteId) || !Request->TryGetStringField(TEXT("sourceCityId"), SourceText) ||
				!Request->TryGetStringField(TEXT("destinationCityId"), DestinationText) ||
				!Request->TryGetStringField(TEXT("goodId"), GoodText) ||
				!TryIntegral(Request, TEXT("quantityMilliUnits"), Quantity) || Quantity <= 0 ||
				!TryIntegral(Request, TEXT("minimumReserveMilliUnits"), Reserve) || Reserve < 0)
			{
				OutError = TEXT("route.edit requires routeId, sourceCityId, destinationCityId, goodId, positive quantityMilliUnits and non-negative minimumReserveMilliUnits.");
				return false;
			}
			const auto Source = Hansa::Simulation::FHansaCityDefinitionId::TryParse(SourceText);
			const auto Destination = Hansa::Simulation::FHansaCityDefinitionId::TryParse(DestinationText);
			const auto Good = Hansa::Simulation::FHansaGoodId::TryParse(GoodText);
			if (!Source || !Destination || !Good) { OutError = TEXT("route.edit identifiers are invalid."); return false; }
			Hansa::Simulation::FHansaRouteStop Home;
			Home.CityId = Source.Value;
			Home.Actions.Add({ Hansa::Simulation::EHansaRouteCargoActionKind::Unload,
				Hansa::Simulation::EHansaRouteCargoCondition::Always, Good.Value,
				Hansa::Simulation::FHansaQuantity::FromRaw(Quantity), Hansa::Simulation::FHansaQuantity() });
			Hansa::Simulation::FHansaRouteStop Remote;
			Remote.CityId = Destination.Value;
			Remote.Actions.Add({ Hansa::Simulation::EHansaRouteCargoActionKind::Load,
				Hansa::Simulation::EHansaRouteCargoCondition::Always, Good.Value,
				Hansa::Simulation::FHansaQuantity::FromRaw(Quantity), Hansa::Simulation::FHansaQuantity::FromRaw(Reserve) });
			const auto Result = Fixture->EditRoute(RouteId, { MoveTemp(Home), MoveTemp(Remote) });
			if (!Result) { OutError = FString::Printf(TEXT("The authoritative route edit was rejected: %s/%s."),
				Hansa::Simulation::LexToString(Result.GetError()), Hansa::Simulation::LexToString(Result.GetRoutePlanError())); return false; }
			OutPayload = MakeSummary(1); OutPayload->SetStringField(TEXT("command"), CommandName);
			OutPayload->SetNumberField(TEXT("routeId"), static_cast<double>(RouteId.GetValue()));
			return true;
		}
		if (CommandName == TEXT("route.set_active") || CommandName == TEXT("route.cancel"))
		{
			Hansa::Simulation::FHansaRouteId RouteId;
			bool bActive = true;
			if (!ParseRouteId(Request, RouteId) || (CommandName == TEXT("route.set_active") && !Request->TryGetBoolField(TEXT("active"), bActive)))
			{ OutError = TEXT("route.set_active requires routeId and active; route.cancel requires routeId."); return false; }
			const auto Result = CommandName == TEXT("route.cancel") ? Fixture->CancelRoute(RouteId) : Fixture->SetRouteActive(RouteId, bActive);
			if (!Result) { OutError = FString::Printf(TEXT("The authoritative route command was rejected: %s."), Hansa::Simulation::LexToString(Result.GetError())); return false; }
			OutPayload = MakeSummary(1); OutPayload->SetStringField(TEXT("command"), CommandName);
			OutPayload->SetNumberField(TEXT("routeId"), static_cast<double>(RouteId.GetValue()));
			return true;
		}
		OutError = TEXT("Command is not allowlisted. Use production.set_active, residence.upgrade, route.edit, route.set_active, or route.cancel.");
		return false;
	}

	bool FHansaProductionFixtureService::Step(
		const int32 TickCount,
		TSharedRef<FJsonObject>& OutPayload,
		FString& OutError)
	{
		if (!Fixture.IsSet())
		{
			OutError = TEXT("No production fixture is loaded; call fixture_load first.");
			return false;
		}
		if (TickCount <= 0 || TickCount > 10'000)
		{
			OutError = TEXT("tickCount must be between 1 and 10000.");
			return false;
		}
		const auto Result = Fixture->Step(TickCount);
		if (!Result)
		{
			OutError = FString::Printf(TEXT("The gameplay command gateway rejected the run: %s."), Hansa::Simulation::LexToString(Result.GetError()));
			return false;
		}
		OutPayload = MakeSummary(TickCount);
		return true;
	}

	bool FHansaProductionFixtureService::MatchesPredicate(
		const TSharedRef<FJsonObject>& Predicate,
		FString& OutError) const
	{
		FString Kind;
		if (!Predicate->TryGetStringField(TEXT("kind"), Kind))
		{
			OutError = TEXT("run_until predicate requires kind.");
			return false;
		}
		if (Kind == TEXT("production.completed_cycles_at_least"))
		{
			Hansa::Simulation::FHansaProductionId ProductionId;
			int64 Minimum = 0;
			if (!ParseProductionId(Predicate, ProductionId) || !TryIntegral(Predicate, TEXT("minimumCompletedCycles"), Minimum) || Minimum < 0)
			{
				OutError = TEXT("completed-cycles predicate requires productionId and non-negative minimumCompletedCycles.");
				return false;
			}
			const auto Projection = Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryProduction(ProductionId);
			if (!Projection.IsSet())
			{
				OutError = TEXT("Predicate productionId does not exist.");
				return false;
			}
			return Projection->CompletedCycles >= static_cast<uint64>(Minimum);
		}
		if (Kind == TEXT("production.blocker_equals"))
		{
			Hansa::Simulation::FHansaProductionId ProductionId;
			FString Expected;
			if (!ParseProductionId(Predicate, ProductionId) || !Predicate->TryGetStringField(TEXT("blocker"), Expected))
			{
				OutError = TEXT("blocker predicate requires productionId and blocker.");
				return false;
			}
			const auto Projection = Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryProduction(ProductionId);
			if (!Projection.IsSet())
			{
				OutError = TEXT("Predicate productionId does not exist.");
				return false;
			}
			return Expected == Hansa::Simulation::LexToString(Projection->Blocker);
		}
		if (Kind == TEXT("route.departed") || Kind == TEXT("route.arrived") || Kind == TEXT("route.delivered"))
		{
			Hansa::Simulation::FHansaRouteId RouteId;
			if (!ParseRouteId(Predicate, RouteId)) { OutError = TEXT("Route predicates require a positive integral routeId."); return false; }
			for (const auto& Event : Fixture->GetEvents())
			{
				if (Event.GetRouteId() != RouteId) continue;
				if (Kind == TEXT("route.departed") && Event.GetType() == Hansa::Simulation::EHansaDomainEventType::RouteDeparted) return true;
				if (Kind == TEXT("route.arrived") && Event.GetType() == Hansa::Simulation::EHansaDomainEventType::RouteArrived) return true;
				if (Kind == TEXT("route.delivered") &&
					(Event.GetType() == Hansa::Simulation::EHansaDomainEventType::RouteCargoTransferred ||
					 Event.GetType() == Hansa::Simulation::EHansaDomainEventType::RouteCargoMissed) &&
					Event.GetRouteCargoActionKind() == Hansa::Simulation::EHansaRouteCargoActionKind::Unload &&
					Event.GetValue() > 0) return true;
			}
			return false;
		}
		Hansa::Simulation::FHansaCityDefinitionId CityId;
		Hansa::Simulation::FHansaGoodId GoodId;
		if (Kind.StartsWith(TEXT("market.")) && !ParseMarketIds(Predicate, CityId, GoodId))
		{
			OutError = TEXT("Market predicates require canonical cityId and goodId fields.");
			return false;
		}
		const Hansa::Simulation::FHansaSimulationReadOnlyAccess ReadOnly =
			Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions());
		if (Kind == TEXT("market.alert_active"))
		{
			FString AlertType;
			if (!Predicate->TryGetStringField(TEXT("alertType"), AlertType))
			{
				OutError = TEXT("market.alert_active requires alertType.");
				return false;
			}
			for (const auto& Alert : ReadOnly.QueryMarketAlerts(CityId, GoodId))
			{
				if (AlertType == Hansa::Simulation::LexToString(Alert.Type)) { return true; }
			}
			return false;
		}
		const auto Market = ReadOnly.QueryMarket(CityId, GoodId);
		if ((Kind == TEXT("market.stock_at_least") || Kind == TEXT("market.price_at_most") ||
			Kind == TEXT("market.reserve_recovered")) && !Market.IsSet())
		{
			OutError = TEXT("Predicate city/good market does not exist.");
			return false;
		}
		if (Kind == TEXT("market.stock_at_least"))
		{
			int64 Minimum = 0;
			if (!TryIntegral(Predicate, TEXT("minimumStockMilliUnits"), Minimum) || Minimum < 0)
			{
				OutError = TEXT("market.stock_at_least requires non-negative minimumStockMilliUnits.");
				return false;
			}
			return Market->CurrentStock.GetRawValue() >= Minimum;
		}
		if (Kind == TEXT("market.price_at_most"))
		{
			int64 Maximum = 0;
			if (!TryIntegral(Predicate, TEXT("maximumPriceMilliMarks"), Maximum) || Maximum <= 0)
			{
				OutError = TEXT("market.price_at_most requires positive maximumPriceMilliMarks.");
				return false;
			}
			return Market->CurrentPriceMilliMarks <= Maximum;
		}
		if (Kind == TEXT("market.reserve_recovered"))
		{
			const bool bShortage = ReadOnly.QueryMarketAlerts(CityId, GoodId).ContainsByPredicate([](const auto& Alert)
			{
				return Alert.Type == Hansa::Simulation::EHansaMarketAlertType::Shortage;
			});
			return !bShortage && Market->CurrentStock.GetRawValue() >= Market->DesiredReserve.GetRawValue();
		}
		OutError = TEXT("Predicate is not allowlisted. Use production.* or documented market.* predicates.");
		return false;
	}

	bool FHansaProductionFixtureService::AssertPredicate(
		const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload,
		FString& OutError) const
	{
		if (!Fixture.IsSet()) { OutError = TEXT("No fixture is loaded; call fixture_load first."); return false; }
		if (!Request->HasTypedField<EJson::Object>(TEXT("predicate")))
		{
			OutError = TEXT("gameplay_assert requires a predicate object.");
			return false;
		}
		FString PredicateError;
		const TSharedRef<FJsonObject> Predicate = Request->GetObjectField(TEXT("predicate")).ToSharedRef();
		const bool bMatched = MatchesPredicate(Predicate, PredicateError);
		if (!PredicateError.IsEmpty()) { OutError = PredicateError; return false; }
		OutPayload = MakeSummary();
		OutPayload->SetBoolField(TEXT("matched"), bMatched);
		OutPayload->SetObjectField(TEXT("predicate"), Predicate);
		return true;
	}

	bool FHansaProductionFixtureService::RunUntil(
		const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload,
		FString& OutError)
	{
		if (!Fixture.IsSet())
		{
			OutError = TEXT("No production fixture is loaded; call fixture_load first.");
			return false;
		}
		int64 MaximumTicks = 0;
		if (!TryIntegral(Request, TEXT("maximumTicks"), MaximumTicks) || MaximumTicks <= 0 || MaximumTicks > 10'000 ||
			!Request->HasTypedField<EJson::Object>(TEXT("predicate")))
		{
			OutError = TEXT("run_until requires maximumTicks from 1 to 10000 and an allowlisted predicate object.");
			return false;
		}
		const TSharedRef<FJsonObject> Predicate = Request->GetObjectField(TEXT("predicate")).ToSharedRef();
		FString PredicateError;
		bool bMatched = MatchesPredicate(Predicate, PredicateError);
		if (!bMatched && !PredicateError.IsEmpty())
		{
			OutError = PredicateError;
			return false;
		}
		int32 Ticks = 0;
		while (!bMatched && Ticks < MaximumTicks)
		{
			if (!Step(1, OutPayload, OutError))
			{
				return false;
			}
			++Ticks;
			PredicateError.Reset();
			bMatched = MatchesPredicate(Predicate, PredicateError);
			if (!PredicateError.IsEmpty())
			{
				OutError = PredicateError;
				return false;
			}
		}
		OutPayload = MakeSummary(Ticks);
		OutPayload->SetBoolField(TEXT("matched"), bMatched);
		return true;
	}
}
