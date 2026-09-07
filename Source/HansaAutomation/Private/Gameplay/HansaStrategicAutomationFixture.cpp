#include "Gameplay/HansaStrategicAutomationFixture.h"

#include "AI/HansaMerchantAI.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Construction/HansaConstruction.h"
#include "Events/HansaDomainEvent.h"
#include "Placement/HansaPlacement.h"
#include "Research/HansaResearch.h"
#include "Scenario/HansaScenario.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Simulation;

namespace Hansa::Automation
{
	namespace
	{
		FString Hex64(const uint64 Value)
		{
			return FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Value));
		}

		bool Integral(const TSharedRef<FJsonObject>& Object, const TCHAR* Field, int64& Out)
		{
			double Value = 0.0;
			if (!Object->TryGetNumberField(Field, Value) || !FMath::IsFinite(Value) ||
				!FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value))) return false;
			Out = static_cast<int64>(Value);
			return true;
		}

		TSharedRef<FJsonObject> ScenarioJson(const FHansaScenarioProgress* Progress)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			if (Progress == nullptr) { Json->SetBoolField(TEXT("available"), false); return Json; }
			Json->SetBoolField(TEXT("available"), true);
			Json->SetStringField(TEXT("scenarioId"), Progress->ScenarioId);
			Json->SetStringField(TEXT("outcome"), LexToString(Progress->Outcome));
			Json->SetStringField(TEXT("winningVictoryId"), Progress->WinningVictoryId);
			Json->SetNumberField(TEXT("lastEvaluatedTick"), static_cast<double>(Progress->LastEvaluatedTick.GetValue()));
			TArray<TSharedPtr<FJsonValue>> Paths;
			for (const FHansaVictoryPathProgress& Path : Progress->VictoryPaths)
			{
				TSharedRef<FJsonObject> PathJson = MakeShared<FJsonObject>();
				PathJson->SetStringField(TEXT("victoryId"), Path.VictoryId);
				PathJson->SetBoolField(TEXT("allObjectivesMet"), Path.bAllObjectivesMet);
				PathJson->SetBoolField(TEXT("victorious"), Path.bVictorious);
				PathJson->SetNumberField(TEXT("satisfiedTicks"), Path.ConsecutiveSatisfiedTicks);
				PathJson->SetNumberField(TEXT("requiredTicks"), Path.RequiredSustainTicks);
				TArray<TSharedPtr<FJsonValue>> Objectives;
				for (const FHansaScenarioObjectiveProgress& Objective : Path.Objectives)
				{
					TSharedRef<FJsonObject> ObjectiveJson = MakeShared<FJsonObject>();
					ObjectiveJson->SetStringField(TEXT("objectiveId"), Objective.ObjectiveId);
					ObjectiveJson->SetStringField(TEXT("metric"), LexToString(Objective.Metric));
					ObjectiveJson->SetNumberField(TEXT("current"), static_cast<double>(Objective.CurrentValue));
					ObjectiveJson->SetNumberField(TEXT("target"), static_cast<double>(Objective.TargetValue));
					ObjectiveJson->SetBoolField(TEXT("met"), Objective.bMet);
					Objectives.Add(MakeShared<FJsonValueObject>(ObjectiveJson));
				}
				PathJson->SetArrayField(TEXT("objectives"), MoveTemp(Objectives));
				Paths.Add(MakeShared<FJsonValueObject>(PathJson));
			}
			Json->SetArrayField(TEXT("victoryPaths"), MoveTemp(Paths));
			return Json;
		}

		FString ProjectionDigestFor(const UHansaRuntimeSimulationHost* Host)
		{
			const auto Projection = Host != nullptr ? Host->BuildProjection() : THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidFormat);
			if (!Projection) return FString();
			FString Text = FString::Printf(TEXT("tick=%lld;fingerprint=%llu;"),
				static_cast<long long>(Host->GetSimulationTick()),
				static_cast<unsigned long long>(Projection.Value.GetFingerprint().Value));
			for (const FHansaConstructionProjection& C : Projection.Value.GetConstructions())
				Text += FString::Printf(TEXT("c:%llu:%d:%d:%d;"), static_cast<unsigned long long>(C.BuildingId.GetValue()), static_cast<int32>(C.State), C.ElapsedTicks, C.TotalTicks);
			for (const FHansaInventoryProjection& I : Projection.Value.GetInventories())
			{
				Text += FString::Printf(TEXT("i:%llu:%d:%lld;"), static_cast<unsigned long long>(I.Id.GetValue()), static_cast<int32>(I.OwnerKind), static_cast<long long>(I.UsedCapacity.GetRawValue()));
				for (const FHansaInventoryStockProjection& S : I.Stocks) Text += FString::Printf(TEXT("%s=%lld,"), *S.GoodId.ToString(), static_cast<long long>(S.Stock.GetRawValue()));
			}
			for (const FHansaCityMarketProjection& M : Projection.Value.GetMarkets())
				Text += FString::Printf(TEXT("m:%s:%s:%lld:%lld;"), *M.CityId.ToString(), *M.GoodId.ToString(), static_cast<long long>(M.CurrentStock.GetRawValue()), static_cast<long long>(M.CurrentPriceMilliMarks));
			for (const FHansaRouteProjection& R : Projection.Value.GetRoutes())
				Text += FString::Printf(TEXT("r:%llu:%llu:%d:%d:%d:%lld;"), static_cast<unsigned long long>(R.Id.GetValue()), static_cast<unsigned long long>(R.OwnerId.GetValue()), static_cast<int32>(R.Lifecycle), R.CurrentStopIndex, R.RemainingTravelTicks, static_cast<long long>(R.CompletedLegCount));
			for (const FHansaHouseResearchState& R : Projection.Value.GetResearch())
			{
				Text += FString::Printf(TEXT("q:%llu:%s:%d;"), static_cast<unsigned long long>(R.HouseId.GetValue()), *R.ActiveTechnologyId, R.ProgressTicks);
				for (const FString& Id : R.CompletedTechnologyIds) Text += Id + TEXT(",");
			}
			if (const FHansaScenarioProgress* Scenario = Host->GetScenarioProgress())
			{
				Text += FString::Printf(TEXT("o:%s:%d:%s:%lld;"), *Scenario->ScenarioId, static_cast<int32>(Scenario->Outcome), *Scenario->WinningVictoryId, static_cast<long long>(Scenario->LastEvaluatedTick.GetValue()));
				for (const FHansaVictoryPathProgress& Path : Scenario->VictoryPaths)
					for (const FHansaScenarioObjectiveProgress& Objective : Path.Objectives)
						Text += FString::Printf(TEXT("%s=%lld/%lld,"), *Objective.ObjectiveId, static_cast<long long>(Objective.CurrentValue), static_cast<long long>(Objective.TargetValue));
			}
			uint64 Hash = 14695981039346656037ULL;
			for (const TCHAR Character : Text) { Hash ^= static_cast<uint16>(Character); Hash *= 1099511628211ULL; }
			return Hex64(Hash);
		}
		TSharedRef<FJsonObject> ResearchJson(const FHansaSimulationProjection& Projection, const FHansaHouseId HouseId)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			const FHansaHouseResearchState* Research = Projection.GetResearch().FindByPredicate(
				[HouseId](const FHansaHouseResearchState& Candidate) { return Candidate.HouseId == HouseId; });
			if (Research == nullptr) { Json->SetBoolField(TEXT("available"), false); return Json; }
			Json->SetBoolField(TEXT("available"), true);
			Json->SetNumberField(TEXT("availablePoints"), Research->AvailableResearchPoints);
			Json->SetStringField(TEXT("activeTechnologyId"), Research->ActiveTechnologyId);
			Json->SetNumberField(TEXT("progressTicks"), Research->ProgressTicks);
			TArray<TSharedPtr<FJsonValue>> Completed;
			for (const FString& Id : Research->CompletedTechnologyIds) Completed.Add(MakeShared<FJsonValueString>(Id));
			Json->SetArrayField(TEXT("completedTechnologyIds"), MoveTemp(Completed));
			TArray<TSharedPtr<FJsonValue>> Effects;
			for (const FHansaAppliedResearchEffect& Effect : Research->AppliedEffects)
			{
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("sourceTechnologyId"), Effect.SourceTechnologyId);
				Item->SetNumberField(TEXT("kind"), static_cast<int32>(Effect.Kind));
				Item->SetStringField(TEXT("targetStableId"), Effect.TargetStableId);
				Item->SetNumberField(TEXT("magnitude"), Effect.Magnitude);
				Effects.Add(MakeShared<FJsonValueObject>(Item));
			}
			Json->SetArrayField(TEXT("appliedEffects"), MoveTemp(Effects));
			return Json;
		}
	}

	bool FHansaStrategicAutomationFixture::IsStrategicFixtureId(const FString& FixtureId)
	{
		return FixtureId == SeedAlphaFixtureId || FixtureId == SeedBetaFixtureId ||
			FixtureId == SaveRoundTripFixtureId || FixtureId == GoldenFixtureId;
	}

	void FHansaStrategicAutomationFixture::AppendFixtureDescriptors(TArray<TSharedPtr<FJsonValue>>& Fixtures) const
	{
		for (const TPair<const TCHAR*, uint64>& Spec : {
			TPair<const TCHAR*, uint64>(SeedAlphaFixtureId, SeedAlpha),
			TPair<const TCHAR*, uint64>(SeedBetaFixtureId, SeedBeta),
			TPair<const TCHAR*, uint64>(SaveRoundTripFixtureId, SeedAlpha) })
		{
			const bool bSaveRoundTrip = FCString::Strcmp(Spec.Key, SaveRoundTripFixtureId) == 0;
			TSharedRef<FJsonObject> Descriptor = MakeShared<FJsonObject>();
			Descriptor->SetStringField(TEXT("fixtureId"), Spec.Key);
			Descriptor->SetNumberField(TEXT("fixtureVersion"), FixtureVersion);
			Descriptor->SetStringField(TEXT("campaignSeed"), Hex64(Spec.Value));
			Descriptor->SetStringField(TEXT("purpose"), bSaveRoundTrip
				? TEXT("S11-P02 controlled save/load round trip across construction, economy, cargo, research, AI, and objectives")
				: TEXT("S10-P04 strategic path: build, diagnose, recover, research, rival AI, and authored victory"));
			Fixtures.Add(MakeShared<FJsonValueObject>(Descriptor));
		}
	}

	bool FHansaStrategicAutomationFixture::Load(const FString& FixtureId,
		TSharedRef<FJsonObject>& OutPayload, FString& OutError)
	{
		if (!IsStrategicFixtureId(FixtureId)) { OutError = TEXT("Unknown strategic fixture seed identifier."); return false; }
		TStrongObjectPtr<UHansaRuntimeSimulationHost> Created(NewObject<UHansaRuntimeSimulationHost>());
		const uint64 Seed = FixtureId == SeedBetaFixtureId ? SeedBeta : SeedAlpha;
		if (!Created->InitializeForLubeck(nullptr, OutError, EHansaRuntimeScenario::LubeckGrainShortage, Seed)) return false;
		Host = MoveTemp(Created);
		LoadedFixtureId = FixtureId;
		const auto InitialProjection = Host->BuildProjection();
		InitialStateHash = InitialProjection ? Hex64(InitialProjection.Value.GetFingerprint().Value) : FString();
		InitialBuildingCount = Host->GetPlacedBuildingCount();
		SaveSlots.Reset(); SaveSlotUtc.Reset(); SavedAuthoritativeHash.Reset(); SavedProjectionDigest.Reset(); bRoundTripEquivalent = false; bDeterministicContinuation = false; bSavedConstruction = false; bSavedInventories = false; bSavedPrices = false; bSavedRouteCargo = false; bSavedResearch = false; bSavedAi = false; bSavedObjectives = false;
		if (FixtureId == SaveRoundTripFixtureId && !PrepareSaveRoundTripState(OutError)) return false;
		OutPayload = MakeSummary();
		return true;
	}

	TSharedRef<FJsonObject> FHansaStrategicAutomationFixture::MakeSummary(const int32 TicksAdvanced) const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("loaded"), IsLoaded());
		Json->SetNumberField(TEXT("ticksAdvanced"), TicksAdvanced);
		if (!IsLoaded()) return Json;
		const auto Projection = Host->BuildProjection();
		Json->SetStringField(TEXT("fixtureId"), LoadedFixtureId);
		Json->SetStringField(TEXT("initialStateHash"), InitialStateHash);
		Json->SetNumberField(TEXT("fixtureVersion"), GetFixtureVersion());
		Json->SetStringField(TEXT("campaignSeed"), Hex64(Host->GetCampaignSeed()));
		const uint64 RegistryHash = Host->GetEconomicRegistry() != nullptr ? Host->GetEconomicRegistry()->GetRegistryHash() : 0;
		Json->SetStringField(TEXT("registryHash"), Hex64(RegistryHash));
		Json->SetStringField(TEXT("contentHash"), Hex64(RegistryHash));
		Json->SetStringField(TEXT("fixtureHash"), Hex64(RegistryHash ^ Host->GetCampaignSeed() ^ GetFixtureVersion()));
		Json->SetNumberField(TEXT("tick"), Host->GetSimulationTick());
		Json->SetNumberField(TEXT("placedBuildingCount"), Host->GetPlacedBuildingCount());
		Json->SetNumberField(TEXT("initialBuildingCount"), InitialBuildingCount);
		Json->SetNumberField(TEXT("eventCount"), Host->GetEventHistory().Num());
		Json->SetNumberField(TEXT("aiDecisionCount"), Host->GetMerchantAIDecisionHistory().Num());
		if (Projection) Json->SetStringField(TEXT("stateHash"), Hex64(Projection.Value.GetFingerprint().Value));
		Json->SetStringField(TEXT("projectionDigest"), ProjectionDigest());
		const FHansaScenarioProgress* Progress = Host->GetScenarioProgress();
		Json->SetStringField(TEXT("scenarioOutcome"), Progress != nullptr ? LexToString(Progress->Outcome) : TEXT("Unavailable"));
		Json->SetStringField(TEXT("winningVictoryId"), Progress != nullptr ? Progress->WinningVictoryId : FString());
		return Json;
	}

	TSharedRef<FJsonObject> FHansaStrategicAutomationFixture::MakeEvidenceSnapshot() const
	{
		TSharedRef<FJsonObject> Json = MakeSummary();
		if (!IsLoaded()) return Json;
		const auto Projection = Host->BuildProjection();
		if (!Projection) return Json;
		Json->SetObjectField(TEXT("research"), ResearchJson(Projection.Value, Host->GetHouseId()));
		Json->SetObjectField(TEXT("objectiveState"), ScenarioJson(Host->GetScenarioProgress()));
		TArray<TSharedPtr<FJsonValue>> Decisions;
		for (const FHansaMerchantAIDecisionTrace& Trace : Host->GetMerchantAIDecisionHistory())
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("tick"), Trace.DecisionTick);
			Item->SetStringField(TEXT("ordinal"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Trace.DecisionOrdinal)));
			Item->SetStringField(TEXT("goal"), LexToString(Trace.SelectedGoal));
			Item->SetStringField(TEXT("chosenOptionId"), Trace.ChosenOptionId);
			Item->SetBoolField(TEXT("commandAccepted"), Trace.bCommandAccepted);
			Item->SetStringField(TEXT("reason"), Trace.Reason);
			Decisions.Add(MakeShared<FJsonValueObject>(Item));
		}
		Json->SetArrayField(TEXT("aiDecisions"), MoveTemp(Decisions));
		TArray<TSharedPtr<FJsonValue>> Events;
		for (const FHansaDomainEvent& Event : Host->GetEventHistory())
		{
			if (Event.GetType() != EHansaDomainEventType::BuildingPlaced &&
				Event.GetType() != EHansaDomainEventType::ConstructionCompleted &&
				Event.GetType() != EHansaDomainEventType::RouteActivationChanged &&
				Event.GetType() != EHansaDomainEventType::RouteCargoTransferred &&
				Event.GetType() != EHansaDomainEventType::ResearchQueued &&
				Event.GetType() != EHansaDomainEventType::ResearchCompleted) continue;
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("sequence"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Event.GetGlobalSequence())));
			Item->SetNumberField(TEXT("tick"), static_cast<double>(Event.GetTick().GetValue()));
			Item->SetStringField(TEXT("type"), LexToString(Event.GetType()));
			Item->SetNumberField(TEXT("buildingId"), static_cast<double>(Event.GetBuildingId().GetValue()));
			Item->SetNumberField(TEXT("routeId"), static_cast<double>(Event.GetRouteId().GetValue()));
			Item->SetStringField(TEXT("technologyId"), Event.GetTechnologyId());
			Item->SetNumberField(TEXT("value"), static_cast<double>(Event.GetValue()));
			Events.Add(MakeShared<FJsonValueObject>(Item));
		}
		Json->SetArrayField(TEXT("causalEvents"), MoveTemp(Events));
		TSharedRef<FJsonObject> RoundTrip = MakeShared<FJsonObject>();
		RoundTrip->SetBoolField(TEXT("verified"), HasVerifiedRoundTrip());
		RoundTrip->SetStringField(TEXT("savedAuthoritativeHash"), SavedAuthoritativeHash);
		RoundTrip->SetStringField(TEXT("savedProjectionDigest"), SavedProjectionDigest);
		Json->SetObjectField(TEXT("saveRoundTrip"), RoundTrip);
		return Json;
	}

	TSharedRef<FJsonObject> FHansaStrategicAutomationFixture::MakeLogSnapshot(const int32 MaximumEntries) const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("fixtureId"), LoadedFixtureId);
		Json->SetNumberField(TEXT("tick"), IsLoaded() ? Host->GetSimulationTick() : 0);
		TArray<TSharedPtr<FJsonValue>> Entries;
		if (IsLoaded())
		{
			const TConstArrayView<FHansaDomainEvent> Events = Host->GetEventHistory();
			const int32 First = FMath::Max(0, Events.Num() - FMath::Clamp(MaximumEntries, 1, 512));
			for (int32 Index = First; Index < Events.Num(); ++Index)
			{
				const FHansaDomainEvent& Event = Events[Index];
				TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetStringField(TEXT("level"), TEXT("info"));
				Entry->SetStringField(TEXT("category"), TEXT("SimulationEvent"));
				Entry->SetNumberField(TEXT("tick"), static_cast<double>(Event.GetTick().GetValue()));
				Entry->SetStringField(TEXT("sequence"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Event.GetGlobalSequence())));
				Entry->SetStringField(TEXT("type"), LexToString(Event.GetType()));
				Entries.Add(MakeShared<FJsonValueObject>(Entry));
			}
			Json->SetBoolField(TEXT("truncated"), First > 0);
		}
		else Json->SetBoolField(TEXT("truncated"), false);
		Json->SetArrayField(TEXT("entries"), MoveTemp(Entries));
		return Json;
	}

	bool FHansaStrategicAutomationFixture::Query(const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload, FString& OutError) const
	{
		if (!IsLoaded()) { OutError = TEXT("No strategic fixture is loaded."); return false; }
		FString Query;
		if (!Request->TryGetStringField(TEXT("query"), Query)) { OutError = TEXT("gameplay_query requires query."); return false; }
		if (Query == TEXT("fixture.summary") || Query == TEXT("strategic.summary")) { OutPayload = MakeSummary(); return true; }
		if (Query == TEXT("strategic.evidence")) { OutPayload = MakeEvidenceSnapshot(); return true; }
		const auto Projection = Host->BuildProjection();
		if (!Projection) { OutError = TEXT("Strategic projection is unavailable."); return false; }
		if (Query == TEXT("research.state"))
		{
			OutPayload->SetObjectField(TEXT("research"), ResearchJson(Projection.Value, Host->GetHouseId()));
			return true;
		}
		if (Query == TEXT("scenario.progress"))
		{
			OutPayload->SetObjectField(TEXT("scenario"), ScenarioJson(Host->GetScenarioProgress()));
			return true;
		}
		if (Query == TEXT("ai.decision_history"))
		{
			TSharedRef<FJsonObject> Evidence = MakeEvidenceSnapshot();
			OutPayload->SetArrayField(TEXT("decisions"), Evidence->GetArrayField(TEXT("aiDecisions")));
			return true;
		}
		if (Query == TEXT("market.alerts") || Query == TEXT("market.diagnosis"))
		{
			FString CityText, GoodText;
			if (!Request->TryGetStringField(TEXT("cityId"), CityText) || !Request->TryGetStringField(TEXT("goodId"), GoodText))
			{ OutError = TEXT("market.alerts requires cityId and goodId."); return false; }
			TArray<TSharedPtr<FJsonValue>> Alerts;
			for (const FHansaMarketAlertProjection& Alert : Projection.Value.GetActiveMarketAlerts())
			{
				if (Alert.CityId.ToString() != CityText || Alert.GoodId.ToString() != GoodText) continue;
				TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("type"), LexToString(Alert.Type));
				Item->SetStringField(TEXT("severity"), LexToString(Alert.Severity));
				Item->SetStringField(TEXT("cause"), Alert.Cause.ToString());
				Alerts.Add(MakeShared<FJsonValueObject>(Item));
			}
			const FHansaCityMarketProjection* Market = Projection.Value.GetMarkets().FindByPredicate(
				[&CityText, &GoodText](const FHansaCityMarketProjection& Candidate)
				{ return Candidate.CityId.ToString() == CityText && Candidate.GoodId.ToString() == GoodText; });
			const bool bReserveDeficit = Market != nullptr && Market->CurrentStock.GetRawValue() < Market->DesiredReserve.GetRawValue();
			OutPayload->SetBoolField(TEXT("shortageDiagnosed"), bReserveDeficit || !Alerts.IsEmpty());
			if (Market != nullptr)
			{
				OutPayload->SetNumberField(TEXT("stockMilliUnits"), Market->CurrentStock.GetRawValue());
				OutPayload->SetNumberField(TEXT("desiredReserveMilliUnits"), Market->DesiredReserve.GetRawValue());
			}
			OutPayload->SetArrayField(TEXT("alerts"), MoveTemp(Alerts));
			if (Query == TEXT("market.diagnosis"))
			{
				TArray<TSharedPtr<FJsonValue>> Causes;
				if (Market != nullptr)
				{
					TSharedRef<FJsonObject> Scarcity = MakeShared<FJsonObject>();
					Scarcity->SetStringField(TEXT("factor"), TEXT("Scarcity"));
					Scarcity->SetNumberField(TEXT("stockMilliUnits"), Market->CurrentStock.GetRawValue());
					Scarcity->SetNumberField(TEXT("desiredReserveMilliUnits"), Market->DesiredReserve.GetRawValue());
					Causes.Add(MakeShared<FJsonValueObject>(Scarcity));
					TSharedRef<FJsonObject> Demand = MakeShared<FJsonObject>();
					Demand->SetStringField(TEXT("factor"), TEXT("Demand"));
					Demand->SetNumberField(TEXT("citizenDemandMilliUnits"), Market->CitizenDemand.GetRawValue());
					Demand->SetNumberField(TEXT("industrialDemandMilliUnits"), Market->IndustrialDemand.GetRawValue());
					Causes.Add(MakeShared<FJsonValueObject>(Demand));
				}
				OutPayload->SetArrayField(TEXT("causes"), MoveTemp(Causes));
			}
			return true;
		}
		OutError = TEXT("Strategic query is not allowlisted. Use strategic.summary, strategic.evidence, market.alerts, market.diagnosis, research.state, ai.decision_history, or scenario.progress.");
		return false;
	}

	bool FHansaStrategicAutomationFixture::Command(const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload, FString& OutError)
	{
		if (!IsLoaded()) { OutError = TEXT("No strategic fixture is loaded."); return false; }
		FString CommandName;
		if (!Request->TryGetStringField(TEXT("command"), CommandName)) { OutError = TEXT("gameplay_command requires command."); return false; }
		FHansaCommandGatewayResult Result;
		if (CommandName == TEXT("building.place"))
		{
			FString DefinitionText; int64 X = 0, Y = 0;
			if (!Request->TryGetStringField(TEXT("buildingDefinitionId"), DefinitionText) ||
				!Integral(Request, TEXT("x"), X) || !Integral(Request, TEXT("y"), Y))
			{ OutError = TEXT("building.place requires buildingDefinitionId, x and y."); return false; }
			const auto DefinitionId = FHansaBuildingTypeId::TryParse(DefinitionText);
			if (!DefinitionId) { OutError = TEXT("building.place buildingDefinitionId is invalid."); return false; }
			FHansaPlacementSpec Spec; Spec.CityId = Host->GetCityId(); Spec.BuildingDefinitionId = DefinitionId.Value;
			Spec.Anchor = {static_cast<int32>(X), static_cast<int32>(Y)};
			Result = Host->PlaceBuildings(MakeArrayView(&Spec, 1));
		}
		else if (CommandName == TEXT("route.set_active"))
		{
			int64 RouteValue = 0; bool bActive = false;
			if (!Integral(Request, TEXT("routeId"), RouteValue) || RouteValue <= 0 || !Request->TryGetBoolField(TEXT("active"), bActive))
			{ OutError = TEXT("route.set_active requires routeId and active."); return false; }
			const auto RouteId = FHansaRouteId::TryCreate(static_cast<uint64>(RouteValue));
			if (!RouteId) { OutError = TEXT("routeId is invalid."); return false; }
			Result = Host->SetRouteActive(RouteId.Value, bActive);
		}
		else if (CommandName == TEXT("research.queue"))
		{
			FString TechnologyId;
			if (!Request->TryGetStringField(TEXT("technologyId"), TechnologyId)) { OutError = TEXT("research.queue requires technologyId."); return false; }
			Result = Host->QueueResearch(TechnologyId);
		}
		else { OutError = TEXT("Strategic command is not allowlisted. Use building.place, route.set_active, or research.queue."); return false; }
		if (!Result) { OutError = FString::Printf(TEXT("Authoritative gameplay gateway rejected %s: %s."), *CommandName, LexToString(Result.GetError())); return false; }
		OutPayload = MakeSummary();
		OutPayload->SetStringField(TEXT("command"), CommandName);
		OutPayload->SetBoolField(TEXT("accepted"), true);
		return true;
	}

	bool FHansaStrategicAutomationFixture::MatchesPredicate(const TSharedRef<FJsonObject>& Predicate, FString& OutError) const
	{
		FString Kind;
		if (!Predicate->TryGetStringField(TEXT("kind"), Kind)) { OutError = TEXT("Predicate requires kind."); return false; }
		const auto Projection = Host->BuildProjection();
		if (!Projection) { OutError = TEXT("Strategic projection is unavailable."); return false; }
		if (Kind == TEXT("strategic.building_placed")) return Host->GetPlacedBuildingCount() > InitialBuildingCount;
		if (Kind == TEXT("strategic.building_completed"))
		{
			for (const FHansaDomainEvent& Event : Host->GetEventHistory()) if (Event.GetType() == EHansaDomainEventType::ConstructionCompleted && Event.GetBuildingId().GetValue() > static_cast<uint64>(InitialBuildingCount)) return true;
			return false;
		}
		if (Kind == TEXT("strategic.shortage_diagnosed"))
		{
			const FHansaCityMarketProjection* Market = Projection.Value.GetMarkets().FindByPredicate([](const FHansaCityMarketProjection& Candidate)
			{ return Candidate.CityId.ToString() == TEXT("City.Lubeck") && Candidate.GoodId.ToString() == TEXT("Good.Grain"); });
			return Market != nullptr && Market->CurrentStock.GetRawValue() < Market->DesiredReserve.GetRawValue();
		}
		if (Kind == TEXT("strategic.route_recovered") || Kind == TEXT("strategic.route_delivered"))
		{
			int32 ActivePlayerRoutes = 0;
			int64 CompletedPlayerLegs = 0;
			for (const FHansaRouteProjection& Route : Projection.Value.GetRoutes())
			{
				if (Route.Id.GetValue() > 2) continue;
				ActivePlayerRoutes += Route.Lifecycle != EHansaRouteLifecycleState::Inactive ? 1 : 0;
				CompletedPlayerLegs += Route.CompletedLegCount;
			}
			return ActivePlayerRoutes == 2 && CompletedPlayerLegs >= 2;
		}
		if (Kind == TEXT("strategic.route_cargo_in_transit"))
		{
			return Projection.Value.GetInventories().ContainsByPredicate([](const FHansaInventoryProjection& Inventory)
			{
				return Inventory.OwnerKind == EHansaInventoryOwnerKind::Vehicle && Inventory.UsedCapacity.GetRawValue() > 0;
			});
		}
		if (Kind == TEXT("strategic.research_completed"))
		{
			FString TechnologyId;
			if (!Predicate->TryGetStringField(TEXT("technologyId"), TechnologyId)) { OutError = TEXT("research predicate requires technologyId."); return false; }
			const FHansaHouseResearchState* Research = Projection.Value.GetResearch().FindByPredicate(
				[this](const FHansaHouseResearchState& Candidate) { return Candidate.HouseId == Host->GetHouseId(); });
			return Research != nullptr && Research->IsCompleted(TechnologyId) && !Research->AppliedEffects.IsEmpty();
		}
		if (Kind == TEXT("strategic.ai_progressed"))
		{
			int64 Minimum = 1;
			if (Predicate->HasField(TEXT("minimumDecisions")) && (!Integral(Predicate, TEXT("minimumDecisions"), Minimum) || Minimum < 1))
			{ OutError = TEXT("minimumDecisions must be positive."); return false; }
			return Host->GetMerchantAIDecisionHistory().Num() >= Minimum;
		}
		if (Kind == TEXT("strategic.victory"))
		{
			FString VictoryId;
			Predicate->TryGetStringField(TEXT("victoryId"), VictoryId);
			const FHansaScenarioProgress* Progress = Host->GetScenarioProgress();
			return Progress != nullptr && Progress->Outcome == EHansaScenarioOutcome::Victory &&
				(VictoryId.IsEmpty() || Progress->WinningVictoryId == VictoryId);
		}
		OutError = TEXT("Strategic predicate is not allowlisted.");
		return false;
	}

	bool FHansaStrategicAutomationFixture::AssertPredicate(const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload, FString& OutError) const
	{
		if (!IsLoaded() || !Request->HasTypedField<EJson::Object>(TEXT("predicate")))
		{ OutError = TEXT("gameplay_assert requires a loaded strategic fixture and predicate."); return false; }
		const TSharedRef<FJsonObject> Predicate = Request->GetObjectField(TEXT("predicate")).ToSharedRef();
		const bool bMatched = MatchesPredicate(Predicate, OutError);
		if (!OutError.IsEmpty()) return false;
		OutPayload = MakeSummary(); OutPayload->SetObjectField(TEXT("predicate"), Predicate); OutPayload->SetBoolField(TEXT("matched"), bMatched);
		if (!bMatched) OutError = TEXT("The strategic assertion did not match.");
		return bMatched;
	}

	bool FHansaStrategicAutomationFixture::PrepareSaveRoundTripState(FString& OutError)
	{
		FHansaPlacementSpec Spec; Spec.CityId = Host->GetCityId();
		const auto Road = FHansaBuildingTypeId::TryParse(TEXT("Building.Road"));
		if (!Road) { OutError = TEXT("The save fixture could not resolve Building.Road."); return false; }
		Spec.BuildingDefinitionId = Road.Value; Spec.Anchor = {10, 30};
		if (!Host->PlaceBuildings(MakeArrayView(&Spec, 1))) { OutError = TEXT("The save fixture could not place its construction checkpoint."); return false; }
		if (!Host->QueueResearch(TEXT("Technology.Commerce.MarketReports"))) { OutError = TEXT("The save fixture could not queue research."); return false; }
		for (uint64 RouteValue : {1ULL, 2ULL})
		{
			const auto RouteId = FHansaRouteId::TryCreate(RouteValue);
			if (!RouteId || !Host->SetRouteActive(RouteId.Value, true)) { OutError = TEXT("The save fixture could not activate its player trade routes."); return false; }
		}
		for (int32 Tick = 0; Tick < 64; ++Tick)
		{
			if (!Host->AdvanceTicks(1)) { OutError = TEXT("The save fixture failed while preparing nontrivial state."); return false; }
			const auto Projection = Host->BuildProjection();
			if (!Projection) continue;
			const bool bConstruction = !Projection.Value.GetConstructions().IsEmpty();
			const bool bInventory = Projection.Value.GetInventories().ContainsByPredicate([](const FHansaInventoryProjection& I){ return I.UsedCapacity.GetRawValue() > 0; });
			const bool bPrices = Projection.Value.GetMarkets().ContainsByPredicate([](const FHansaCityMarketProjection& M){ return M.CurrentPriceMilliMarks > 0 && M.CurrentStock.GetRawValue() > 0; });
			const bool bCargo = Projection.Value.GetInventories().ContainsByPredicate([](const FHansaInventoryProjection& I){ return I.OwnerKind == EHansaInventoryOwnerKind::Vehicle && I.UsedCapacity.GetRawValue() > 0; });
			const bool bResearch = Projection.Value.GetResearch().ContainsByPredicate([](const FHansaHouseResearchState& R){ return !R.ActiveTechnologyId.IsEmpty() || !R.CompletedTechnologyIds.IsEmpty(); });
			const bool bAi = !Host->GetMerchantAIDecisionHistory().IsEmpty();
			const bool bObjective = Host->GetScenarioProgress() != nullptr && !Host->GetScenarioProgress()->VictoryPaths.IsEmpty();
			if (bConstruction && bInventory && bPrices && bCargo && bResearch && bAi && bObjective) return true;
		}
		OutError = TEXT("The save fixture did not reach construction, inventory, price, in-transit cargo, research, AI, and objective checkpoints within 64 ticks.");
		return false;
	}

	FString FHansaStrategicAutomationFixture::ProjectionDigest() const { return ProjectionDigestFor(Host.Get()); }

	bool FHansaStrategicAutomationFixture::SaveSlot(const FString& SlotId, TSharedRef<FJsonObject>& OutPayload, FString& OutError)
	{
		if (!IsLoaded() || (SlotId != TEXT("manual") && SlotId != TEXT("autosave")))
		{ OutError = TEXT("save_create requires a loaded fixture and slotId manual or autosave."); return false; }
		TArray<uint8> Bytes; const FString SavedUtc = TEXT("2026-09-06T12:00:00Z");
		const FHansaSaveResult Saved = Host->CaptureSaveBytes(Bytes, SlotId == TEXT("manual") ? TEXT("Round-trip checkpoint") : TEXT("Autosave"), SavedUtc);
		if (!Saved) { OutError = Saved.Message; return false; }
		SaveSlots.Add(SlotId, MoveTemp(Bytes)); SaveSlotUtc.Add(SlotId, SavedUtc);
		SavedAuthoritativeHash = Hex64(Saved.AuthoritativeHash); SavedProjectionDigest = ProjectionDigest();
		const auto Projection = Host->BuildProjection();
		bSavedConstruction = Projection && !Projection.Value.GetConstructions().IsEmpty();
		bSavedInventories = Projection && Projection.Value.GetInventories().ContainsByPredicate([](const FHansaInventoryProjection& I){ return I.UsedCapacity.GetRawValue() > 0; });
		bSavedPrices = Projection && Projection.Value.GetMarkets().ContainsByPredicate([](const FHansaCityMarketProjection& M){ return M.CurrentPriceMilliMarks > 0; });
		bSavedRouteCargo = Projection && Projection.Value.GetInventories().ContainsByPredicate([](const FHansaInventoryProjection& I){ return I.OwnerKind == EHansaInventoryOwnerKind::Vehicle && I.UsedCapacity.GetRawValue() > 0; });
		bSavedResearch = Projection && Projection.Value.GetResearch().ContainsByPredicate([](const FHansaHouseResearchState& R){ return !R.ActiveTechnologyId.IsEmpty() || !R.CompletedTechnologyIds.IsEmpty(); });
		bSavedAi = !Host->GetMerchantAIDecisionHistory().IsEmpty(); bSavedObjectives = Host->GetScenarioProgress() != nullptr && !Host->GetScenarioProgress()->VictoryPaths.IsEmpty();
		bRoundTripEquivalent = false; bDeterministicContinuation = false;
		OutPayload->SetStringField(TEXT("slotId"), SlotId); OutPayload->SetStringField(TEXT("savedUtc"), SavedUtc);
		OutPayload->SetStringField(TEXT("scenarioId"), Host->GetScenarioProgress() ? Host->GetScenarioProgress()->ScenarioId : FString());
		OutPayload->SetNumberField(TEXT("formatVersion"), FHansaSaveEnvelope::CurrentFormatVersion);
		OutPayload->SetStringField(TEXT("authoritativeHash"), SavedAuthoritativeHash);
		OutPayload->SetStringField(TEXT("projectionDigest"), SavedProjectionDigest);
		OutPayload->SetBoolField(TEXT("compatible"), true); OutPayload->SetNumberField(TEXT("tick"), Host->GetSimulationTick());
		return true;
	}

	bool FHansaStrategicAutomationFixture::ListSaveSlots(TSharedRef<FJsonObject>& OutPayload, FString& OutError) const
	{
		if (!IsLoaded()) { OutError = TEXT("No save fixture is loaded."); return false; }
		TArray<TSharedPtr<FJsonValue>> Slots;
		for (const FString SlotId : {FString(TEXT("manual")), FString(TEXT("autosave"))})
		{
			TSharedRef<FJsonObject> Slot = MakeShared<FJsonObject>(); Slot->SetStringField(TEXT("slotId"), SlotId);
			Slot->SetBoolField(TEXT("exists"), SaveSlots.Contains(SlotId)); Slot->SetBoolField(TEXT("compatible"), SaveSlots.Contains(SlotId));
			Slot->SetStringField(TEXT("savedUtc"), SaveSlotUtc.FindRef(SlotId));
			Slot->SetStringField(TEXT("scenarioId"), SaveSlots.Contains(SlotId) && Host->GetScenarioProgress() ? Host->GetScenarioProgress()->ScenarioId : FString());
			Slot->SetNumberField(TEXT("formatVersion"), SaveSlots.Contains(SlotId) ? FHansaSaveEnvelope::CurrentFormatVersion : 0);
			Slots.Add(MakeShared<FJsonValueObject>(Slot));
		}
		OutPayload->SetArrayField(TEXT("slots"), MoveTemp(Slots)); return true;
	}

	bool FHansaStrategicAutomationFixture::LoadSlot(const FString& SlotId, TSharedRef<FJsonObject>& OutPayload, FString& OutError)
	{
		const TArray<uint8>* Bytes = SaveSlots.Find(SlotId);
		if (!IsLoaded() || Bytes == nullptr) { OutError = TEXT("save_load requires an existing manual or autosave slot created in this fixture session."); return false; }
		const FHansaSaveResult Loaded = Host->RestoreSaveBytes(*Bytes);
		if (!Loaded) { OutError = Loaded.Message; return false; }
		const FString RestoredHash = Hex64(Loaded.AuthoritativeHash); const FString RestoredDigest = ProjectionDigest();
		bRoundTripEquivalent = RestoredHash == SavedAuthoritativeHash && RestoredDigest == SavedProjectionDigest;
		TStrongObjectPtr<UHansaRuntimeSimulationHost> Reference(NewObject<UHansaRuntimeSimulationHost>());
		if (!Reference->InitializeForLubeck(nullptr, OutError, EHansaRuntimeScenario::LubeckGrainShortage, Host->GetCampaignSeed()) || !Reference->RestoreSaveBytes(*Bytes))
		{ OutError = TEXT("The deterministic continuation reference could not restore the same slot."); return false; }
		if (!Host->AdvanceTicks(1) || !Reference->AdvanceTicks(1)) { OutError = TEXT("Deterministic continuation could not advance one tick."); return false; }
		const auto Actual = Host->BuildProjection(); const auto Expected = Reference->BuildProjection();
		bDeterministicContinuation = Actual && Expected && Actual.Value.GetFingerprint() == Expected.Value.GetFingerprint() &&
			ProjectionDigestFor(Host.Get()) == ProjectionDigestFor(Reference.Get());
		OutPayload->SetStringField(TEXT("slotId"), SlotId); OutPayload->SetBoolField(TEXT("authoritativeEquivalent"), bRoundTripEquivalent);
		OutPayload->SetBoolField(TEXT("projectionEquivalent"), RestoredDigest == SavedProjectionDigest);
		OutPayload->SetBoolField(TEXT("deterministicContinuation"), bDeterministicContinuation);
		OutPayload->SetStringField(TEXT("savedAuthoritativeHash"), SavedAuthoritativeHash);
		OutPayload->SetStringField(TEXT("restoredAuthoritativeHash"), RestoredHash);
		OutPayload->SetStringField(TEXT("savedProjectionDigest"), SavedProjectionDigest);
		OutPayload->SetStringField(TEXT("restoredProjectionDigest"), RestoredDigest);
		OutPayload->SetNumberField(TEXT("continuedTick"), Host->GetSimulationTick());
		return bRoundTripEquivalent && bDeterministicContinuation;
	}

	bool FHansaStrategicAutomationFixture::AssertRoundTrip(TSharedRef<FJsonObject>& OutPayload, FString& OutError) const
	{
		TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
		Coverage->SetBoolField(TEXT("construction"), bSavedConstruction);
		Coverage->SetBoolField(TEXT("inventories"), bSavedInventories);
		Coverage->SetBoolField(TEXT("prices"), bSavedPrices);
		Coverage->SetBoolField(TEXT("routeCargo"), bSavedRouteCargo);
		Coverage->SetBoolField(TEXT("research"), bSavedResearch);
		Coverage->SetBoolField(TEXT("ai"), bSavedAi);
		Coverage->SetBoolField(TEXT("objectives"), bSavedObjectives);
		const bool bMatched = bRoundTripEquivalent && bDeterministicContinuation &&
			bSavedConstruction && bSavedInventories && bSavedPrices && bSavedRouteCargo &&
			bSavedResearch && bSavedAi && bSavedObjectives;
		OutPayload->SetBoolField(TEXT("matched"), bMatched);
		OutPayload->SetBoolField(TEXT("authoritativeEquivalent"), bRoundTripEquivalent);
		OutPayload->SetBoolField(TEXT("deterministicContinuation"), bDeterministicContinuation);
		OutPayload->SetObjectField(TEXT("coverage"), Coverage);
		if (!bMatched) OutError = TEXT("The save round-trip assertion has not reached every required checkpoint.");
		return bMatched;
	}
	bool FHansaStrategicAutomationFixture::Step(const int32 TickCount,
		TSharedRef<FJsonObject>& OutPayload, FString& OutError)
	{
		if (!IsLoaded() || TickCount <= 0 || TickCount > 10'000) { OutError = TEXT("Step requires a loaded fixture and 1 through 10000 ticks."); return false; }
		if (!Host->AdvanceTicks(TickCount)) { OutError = TEXT("The strategic runtime rejected deterministic advancement."); return false; }
		OutPayload = MakeSummary(TickCount);
		return true;
	}

	bool FHansaStrategicAutomationFixture::RunUntil(const TSharedRef<FJsonObject>& Request,
		TSharedRef<FJsonObject>& OutPayload, FString& OutError)
	{
		int64 MaximumTicks = 0;
		if (!IsLoaded() || !Request->HasTypedField<EJson::Object>(TEXT("predicate")) ||
			!Integral(Request, TEXT("maximumTicks"), MaximumTicks) || MaximumTicks <= 0 || MaximumTicks > 10'000)
		{ OutError = TEXT("run_until requires a loaded strategic fixture, predicate, and maximumTicks 1 through 10000."); return false; }
		const TSharedRef<FJsonObject> Predicate = Request->GetObjectField(TEXT("predicate")).ToSharedRef();
		int32 Advanced = 0;
		while (Advanced <= MaximumTicks)
		{
			FString PredicateError;
			if (MatchesPredicate(Predicate, PredicateError))
			{
				OutPayload = MakeSummary(Advanced); OutPayload->SetBoolField(TEXT("matched"), true); OutPayload->SetObjectField(TEXT("predicate"), Predicate); return true;
			}
			if (!PredicateError.IsEmpty()) { OutError = PredicateError; return false; }
			if (Advanced == MaximumTicks) break;
			TSharedRef<FJsonObject> Ignored = MakeShared<FJsonObject>();
			if (!Step(1, Ignored, OutError)) return false;
			++Advanced;
		}
		FString Kind;
		Predicate->TryGetStringField(TEXT("kind"), Kind);
		OutError = FString::Printf(TEXT("The strategic predicate %s did not match within %lld ticks (current tick %lld)."),
			*Kind, static_cast<long long>(MaximumTicks), static_cast<long long>(Host->GetSimulationTick()));
		return false;
	}
}
