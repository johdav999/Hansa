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
			FixtureId != Hansa::Simulation::FHansaProductionFixture::GrainShortageFixtureId)
		{
			OutError = TEXT("Unknown fixtureId; call fixture_list and use an exact allowlisted identifier.");
			return false;
		}
		const auto Created = FixtureId == Hansa::Simulation::FHansaProductionFixture::GrainShortageFixtureId
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
			OutPayload->SetNumberField(TEXT("roadDistanceCells"), Path.RoadDistanceCells);
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
		if (Query.StartsWith(TEXT("market.")) && !ParseMarketIds(Request, CityId, GoodId))
		{
			OutError = TEXT("Market queries require canonical cityId and goodId fields.");
			return false;
		}
		if (Query == TEXT("market.price") || Query == TEXT("market.components"))
		{
			const auto Market = ReadOnly.QueryMarket(CityId, GoodId);
			if (!Market.IsSet()) { OutError = TEXT("The requested city/good market does not exist."); return false; }
			OutPayload->SetObjectField(TEXT("market"), MakeMarket(Market.GetValue()));
			if (Query == TEXT("market.components"))
			{
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
		OutError = TEXT("Query is not allowlisted. Use fixture.summary, production.*, inventory.stock, logistics.*, city.population, population.cohort, or documented market.* queries.");
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
		OutError = TEXT("Command is not allowlisted. Use production.set_active or residence.upgrade.");
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
