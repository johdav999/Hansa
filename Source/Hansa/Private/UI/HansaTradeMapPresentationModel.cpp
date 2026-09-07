#include "UI/HansaTradeMapPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"

#define LOCTEXT_NAMESPACE "HansaTradeMapPresentationModel"
using namespace Hansa::Simulation;

namespace
{
	FText CityLabel(const FString& Id)
	{
		if (Id.EndsWith(TEXT("Lubeck"))) return LOCTEXT("Lubeck", "Lübeck");
		if (Id.EndsWith(TEXT("Hamburg"))) return LOCTEXT("Hamburg", "Hamburg");
		if (Id.EndsWith(TEXT("Luneburg"))) return LOCTEXT("Luneburg", "Lüneburg");
		if (Id.EndsWith(TEXT("Rostock"))) return LOCTEXT("Rostock", "Rostock");
		return FText::FromString(Id);
	}
	FVector2D CityPosition(const FString& Id)
	{
		if (Id.EndsWith(TEXT("Hamburg"))) return { .18f, .66f };
		if (Id.EndsWith(TEXT("Luneburg"))) return { .30f, .83f };
		if (Id.EndsWith(TEXT("Rostock"))) return { .82f, .34f };
		return { .54f, .57f };
	}
	const FHansaVehicleProjection* VehicleFor(const FHansaSimulationProjection& Projection, const FHansaVehicleId Id)
	{
		return Projection.GetVehicles().FindByPredicate([Id](const FHansaVehicleProjection& V){ return V.Id == Id; });
	}
	int32 LegTicks(const FHansaCompiledRouteDefinition& Def, const FString& A, const FString& B)
	{
		const FHansaCompiledRouteConnection* C = Def.Connections.FindByPredicate([&](const FHansaCompiledRouteConnection& It)
		{
			return (It.SourceCityId == A && It.DestinationCityId == B) || (It.SourceCityId == B && It.DestinationCityId == A);
		});
		return C != nullptr ? C->TravelTicks : 0;
	}
	const FHansaCityMarketProjection* MarketFor(const FHansaSimulationProjection& Projection, const FHansaCityDefinitionId City, const FHansaGoodId Good)
	{
		return Projection.GetMarkets().FindByPredicate([&](const FHansaCityMarketProjection& M){ return M.CityId == City && M.GoodId == Good; });
	}
	FText RouteToggleFailure(const FHansaCommandGatewayResult& Result)
	{
		switch (Result.GetError())
		{
		case EHansaCommandGatewayError::NotAuthorized:
			return LOCTEXT("ToggleNotAuthorized", "This route belongs to a rival house. Select one of your routes to change its operation.");
		case EHansaCommandGatewayError::RouteStateInvalid:
			return LOCTEXT("ToggleInvalidState", "The route can only be changed while its vehicle is stopped at a route stop. Wait for arrival and try again.");
		case EHansaCommandGatewayError::TargetNotFound:
			return LOCTEXT("ToggleMissingRoute", "This route no longer exists. Close and reopen the trade map to refresh the route list.");
		default:
			return FText::Format(LOCTEXT("ToggleFailedWithCause", "Route state could not be changed ({0}). Try again after the next simulation tick."),
				FText::FromString(LexToString(Result.GetError())));
		}
	}
}

bool operator==(const FHansaTradeMapCityPresentation& A,const FHansaTradeMapCityPresentation& B){return A.StableId==B.StableId&&A.Label.EqualTo(B.Label)&&A.NormalizedPosition==B.NormalizedPosition&&A.Information.EqualTo(B.Information)&&A.bOwned==B.bOwned&&A.bStale==B.bStale&&A.bUnknown==B.bUnknown;}
bool operator==(const FHansaTradeMapStopPresentation& A,const FHansaTradeMapStopPresentation& B){return A.Index==B.Index&&A.CityStableId==B.CityStableId&&A.CityLabel.EqualTo(B.CityLabel)&&A.GoodStableId==B.GoodStableId&&A.ActionLabel.EqualTo(B.ActionLabel)&&A.Quantity.EqualTo(B.Quantity)&&A.MinimumReserve.EqualTo(B.MinimumReserve)&&A.AccessibleLabel.EqualTo(B.AccessibleLabel)&&A.bReserveRisk==B.bReserveRisk;}
bool operator==(const FHansaTradeMapRoutePresentation& A,const FHansaTradeMapRoutePresentation& B){return A.RouteValue==B.RouteValue&&A.VehicleValue==B.VehicleValue&&A.DefinitionStableId==B.DefinitionStableId&&A.Label.EqualTo(B.Label)&&A.Mode.EqualTo(B.Mode)&&A.State.EqualTo(B.State)&&A.Ownership.EqualTo(B.Ownership)&&A.StateHeading.EqualTo(B.StateHeading)&&A.StateDetail.EqualTo(B.StateDetail)&&A.ToggleActionLabel.EqualTo(B.ToggleActionLabel)&&A.ToggleActionHint.EqualTo(B.ToggleActionHint)&&A.StopSummary.EqualTo(B.StopSummary)&&A.Capacity.EqualTo(B.Capacity)&&A.Upkeep.EqualTo(B.Upkeep)&&A.RoundTripTime.EqualTo(B.RoundTripTime)&&A.ExpectedProfitRange.EqualTo(B.ExpectedProfitRange)&&A.Uncertainty.EqualTo(B.Uncertainty)&&A.bSea==B.bSea&&A.bActive==B.bActive&&A.bOwnedByPlayer==B.bOwnedByPlayer&&A.bCanToggleActive==B.bCanToggleActive&&A.bTraveling==B.bTraveling&&A.bReserveRisk==B.bReserveRisk&&A.bProfitKnown==B.bProfitKnown;}
bool operator==(const FHansaTradeMapSnapshot& A,const FHansaTradeMapSnapshot& B){return A.Cities==B.Cities&&A.Routes==B.Routes&&A.Stops==B.Stops&&A.FocusedSemanticId==B.FocusedSemanticId&&A.PreferredGoodStableId==B.PreferredGoodStableId&&A.Title.EqualTo(B.Title)&&A.EditorStatus.EqualTo(B.EditorStatus)&&A.ReserveRisk.EqualTo(B.ReserveRisk)&&A.SelectedRouteValue==B.SelectedRouteValue&&A.SelectedStopIndex==B.SelectedStopIndex&&A.ModeFilter==B.ModeFilter&&A.bOpen==B.bOpen&&A.bCompact==B.bCompact&&A.bDirty==B.bDirty;}

void UHansaTradeMapPresentationModel::InitializeDefaults()
{
	Snapshot = {}; Snapshot.Title = LOCTEXT("Title", "European trade"); Snapshot.EditorStatus = LOCTEXT("Empty", "Select a route to edit its stops and cargo rules.");
}

void UHansaTradeMapPresentationModel::BindRuntime(UHansaRuntimeSimulationHost* RuntimeHost) { Runtime = RuntimeHost; }

bool UHansaTradeMapPresentationModel::ApplyProjection(const FHansaSimulationProjection& Projection, const FHansaEconomicRegistry& Registry)
{
	const FHansaTradeMapSnapshot Previous = Snapshot;
	Snapshot.Cities.Reset(); Snapshot.Routes.Reset(); AllRoutes.Reset();
	for (const TCHAR* IdText : { TEXT("City.Hamburg"), TEXT("City.Lubeck"), TEXT("City.Luneburg"), TEXT("City.Rostock") })
	{
		const auto CityId = FHansaCityDefinitionId::TryParse(IdText); if (!CityId) continue;
		FHansaTradeMapCityPresentation City; City.StableId = FName(IdText); City.Label = CityLabel(IdText); City.NormalizedPosition = CityPosition(IdText); City.bOwned = FCString::Strcmp(IdText, TEXT("City.Lubeck")) == 0;
		int64 MaxAge = 0; bool bAny = false; bool bAllUnknown = true;
		for (const FHansaCityMarketProjection& Market : Projection.GetMarkets()) if (Market.CityId == CityId.Value)
		{
			bAny = true; MaxAge = FMath::Max(MaxAge, Market.ReportAgeTicks); City.bStale |= Market.bIsStale; bAllUnknown &= Market.LastUpdateTick < 0;
		}
		City.bUnknown = !bAny || bAllUnknown;
		City.Information = City.bUnknown ? LOCTEXT("NoReport", "No recent report") : City.bStale
			? FText::Format(LOCTEXT("StaleReport", "Stale report · {0} ticks old"), FText::AsNumber(MaxAge))
			: FText::Format(LOCTEXT("CurrentReport", "Current · {0} ticks old"), FText::AsNumber(MaxAge));
		Snapshot.Cities.Add(MoveTemp(City));
	}

	for (const FHansaRouteProjection& Route : Projection.GetRoutes())
	{
		const FHansaVehicleProjection* Vehicle = VehicleFor(Projection, Route.VehicleId);
		const FHansaCompiledRouteDefinition* Definition = Registry.FindRoute(Route.RouteDefinitionId.ToString());
		FHansaTradeMapRoutePresentation Item; Item.RouteValue = static_cast<int64>(Route.Id.GetValue()); Item.VehicleValue = static_cast<int64>(Route.VehicleId.GetValue());
		Item.DefinitionStableId = FName(*Route.RouteDefinitionId.ToString()); Item.bSea = Route.Mode == EHansaRouteMode::Sea; Item.bActive = Route.Lifecycle == EHansaRouteLifecycleState::AtStop || Route.Lifecycle == EHansaRouteLifecycleState::Traveling;
		Item.bTraveling = Route.Lifecycle == EHansaRouteLifecycleState::Traveling;
		Item.bOwnedByPlayer = !Runtime.IsValid() || Route.OwnerId == Runtime->GetHouseId();
		Item.Ownership = Item.bOwnedByPlayer ? LOCTEXT("YourRoute", "Your route") : LOCTEXT("RivalRoute", "Rival route");
		const FText BaseLabel = Item.bSea ? LOCTEXT("BalticRoute", "Baltic grain circuit") : LOCTEXT("SaltRoad", "Lüneburg salt road");
		Item.Label = Item.bOwnedByPlayer ? BaseLabel : FText::Format(LOCTEXT("RivalRouteLabel", "{0} · Rival"), BaseLabel);
		Item.Mode = Item.bSea ? LOCTEXT("Sea", "━━ Sea route") : LOCTEXT("Land", "┄┄ Land route");
		const FText CurrentCity = Route.Stops.IsValidIndex(Route.CurrentStopIndex)
			? CityLabel(Route.Stops[Route.CurrentStopIndex].CityId.ToString()) : LOCTEXT("UnknownCurrentCity", "an unknown stop");
		const FText NextCity = Route.Stops.IsValidIndex(Route.NextStopIndex)
			? CityLabel(Route.Stops[Route.NextStopIndex].CityId.ToString()) : LOCTEXT("UnknownNextCity", "the next stop");
		switch (Route.Lifecycle)
		{
		case EHansaRouteLifecycleState::Inactive:
			Item.State = LOCTEXT("StoppedListState", "⏸ Stopped");
			Item.StateHeading = LOCTEXT("StoppedHeading", "ROUTE STOPPED");
			Item.StateDetail = FText::Format(LOCTEXT("StoppedDetail", "Ready to depart from {0}."), CurrentCity);
			Item.ToggleActionLabel = LOCTEXT("StartRoute", "Start route");
			Item.ToggleActionHint = LOCTEXT("StartRouteHint", "Starts immediately and repeats this route.");
			Item.bCanToggleActive = Item.bOwnedByPlayer;
			break;
		case EHansaRouteLifecycleState::AtStop:
			Item.State = FText::Format(LOCTEXT("AtStopListState", "● Active · At {0}"), CurrentCity);
			Item.StateHeading = LOCTEXT("ActiveHeading", "ROUTE ACTIVE");
			Item.StateDetail = FText::Format(LOCTEXT("ActiveDetail", "At {0}. Cargo actions run before the next departure."), CurrentCity);
			Item.ToggleActionLabel = LOCTEXT("PauseRoute", "Pause route");
			Item.ToggleActionHint = LOCTEXT("PauseRouteHint", "Pauses before the vehicle's next departure.");
			Item.bCanToggleActive = Item.bOwnedByPlayer;
			break;
		case EHansaRouteLifecycleState::Traveling:
			Item.State = FText::Format(LOCTEXT("TravelingListState", "→ In transit · {0} ticks to {1}"), FText::AsNumber(Route.RemainingTravelTicks), NextCity);
			Item.StateHeading = FText::Format(LOCTEXT("TravelingHeading", "IN TRANSIT TO {0} · {1} TICKS"), NextCity, FText::AsNumber(Route.RemainingTravelTicks));
			Item.StateDetail = LOCTEXT("TravelingDetail", "The route remains active and will continue after arrival.");
			Item.ToggleActionLabel = LOCTEXT("PauseAtStop", "Pause available at next stop");
			Item.ToggleActionHint = LOCTEXT("PauseAtStopHint", "Wait until the vehicle arrives to pause this route.");
			Item.bCanToggleActive = false;
			break;
		case EHansaRouteLifecycleState::Cancelled:
		default:
			Item.State = LOCTEXT("CancelledListState", "× Cancelled");
			Item.StateHeading = LOCTEXT("CancelledHeading", "ROUTE CANCELLED");
			Item.StateDetail = LOCTEXT("CancelledDetail", "This route can no longer operate.");
			Item.ToggleActionLabel = LOCTEXT("CancelledAction", "Route unavailable");
			Item.ToggleActionHint = LOCTEXT("CancelledHint", "Create a new route to resume service.");
			Item.bCanToggleActive = false;
			break;
		}
		if (!Item.bOwnedByPlayer)
		{
			Item.ToggleActionLabel = LOCTEXT("RivalAction", "Rival route");
			Item.ToggleActionHint = LOCTEXT("RivalActionHint", "You can inspect this route, but only its owner can change it.");
			Item.bCanToggleActive = false;
		}
		TArray<FString> Names; for (const FHansaRouteStop& Stop : Route.Stops) Names.Add(CityLabel(Stop.CityId.ToString()).ToString()); Item.StopSummary = FText::FromString(FString::Join(Names, TEXT(" → ")));
		int32 RoundTrip = 0; if (Definition != nullptr && Route.Stops.Num() > 1) for (int32 I=0; I<Route.Stops.Num(); ++I) RoundTrip += LegTicks(*Definition, Route.Stops[I].CityId.ToString(), Route.Stops[(I+1)%Route.Stops.Num()].CityId.ToString());
		Item.RoundTripTime = FText::Format(LOCTEXT("Ticks", "{0} ticks round trip"), FText::AsNumber(RoundTrip));
		if (Vehicle != nullptr)
		{
			Item.Capacity = FText::Format(LOCTEXT("Capacity", "{0} / {1} cargo"), FText::AsNumber(Vehicle->Cargo.GetRawValue()/1000), FText::AsNumber(Vehicle->Capacity.GetRawValue()/1000));
			Item.Upkeep = FText::Format(LOCTEXT("Upkeep", "{0} pfennig / round trip"), FText::AsNumber(RoundTrip * Vehicle->UpkeepPfennigPerTravelTick));
		}
		bool bUnknown = false; bool bEstimated = false; int64 GrossMilliMarks = 0;
		for (int32 SourceIndex=0; SourceIndex<Route.Stops.Num(); ++SourceIndex) for (const FHansaRouteCargoAction& Load : Route.Stops[SourceIndex].Actions) if (Load.Kind == EHansaRouteCargoActionKind::Load)
		{
			const FHansaCityMarketProjection* Source = MarketFor(Projection, Route.Stops[SourceIndex].CityId, Load.GoodId);
			const FHansaRouteStop* DestinationStop = Route.Stops.FindByPredicate([&](const FHansaRouteStop& Stop){ return Stop.Actions.ContainsByPredicate([&](const FHansaRouteCargoAction& A){ return A.Kind == EHansaRouteCargoActionKind::Unload && A.GoodId == Load.GoodId; }); });
			const FHansaCityMarketProjection* Destination = DestinationStop != nullptr ? MarketFor(Projection, DestinationStop->CityId, Load.GoodId) : nullptr;
			if (Source == nullptr || Destination == nullptr || Source->LastUpdateTick < 0 || Destination->LastUpdateTick < 0) { bUnknown = true; continue; }
			const int64 Available = FMath::Max<int64>(0, Source->CurrentStock.GetRawValue() - Load.MinimumSourceReserve.GetRawValue());
			const int64 Quantity = FMath::Min(Load.QuantityLimit.GetRawValue(), Available); Item.bReserveRisk |= Available < Load.QuantityLimit.GetRawValue();
			GrossMilliMarks += (Destination->CurrentPriceMilliMarks - Source->CurrentPriceMilliMarks) * Quantity / 1000;
			bEstimated |= Source->bIsStale || Destination->bIsStale;
		}
		if (bUnknown) { Item.ExpectedProfitRange = LOCTEXT("UnknownProfit", "Unknown · no recent report"); Item.Uncertainty = LOCTEXT("Unknown", "No recent report; profit is not represented as zero."); }
		else { const int64 Spread = FMath::Abs(GrossMilliMarks) * (bEstimated ? 35 : 20) / 100; Item.ExpectedProfitRange = FText::Format(LOCTEXT("ProfitRange", "{0}–{1} milli-mark expected gross"), FText::AsNumber(GrossMilliMarks-Spread), FText::AsNumber(GrossMilliMarks+Spread)); Item.Uncertainty = bEstimated ? LOCTEXT("Estimated", "Estimated / stale information · ±35% planning range") : LOCTEXT("Current", "Current reports · ±20% planning range"); Item.bProfitKnown = true; }
		AllRoutes.Add(MoveTemp(Item));
	}
	for (const FHansaTradeMapRoutePresentation& Route : AllRoutes)
	{
		if (Snapshot.ModeFilter == EHansaTradeMapModeFilter::Sea && !Route.bSea) continue;
		if (Snapshot.ModeFilter == EHansaTradeMapModeFilter::Land && Route.bSea) continue;
		Snapshot.Routes.Add(Route);
	}
	if (!Snapshot.Routes.IsEmpty() && (Snapshot.SelectedRouteValue == 0 || !Snapshot.Routes.ContainsByPredicate(
		[this](const FHansaTradeMapRoutePresentation& Route){ return Route.RouteValue == Snapshot.SelectedRouteValue; })))
	{
		Snapshot.SelectedRouteValue = Snapshot.Routes[0].RouteValue;
	}
	const FHansaRouteProjection* Selected = Projection.GetRoutes().FindByPredicate([this](const FHansaRouteProjection& R){ return static_cast<int64>(R.Id.GetValue()) == Snapshot.SelectedRouteValue; });
	if (!Snapshot.bDirty && Selected != nullptr) DraftStops = Selected->Stops;
	RebuildStops(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::Open(const FName FocusOrigin, const FName PreferredGood)
{
	const FHansaTradeMapSnapshot Previous=Snapshot; FocusOriginSemanticId=FocusOrigin; Snapshot.bOpen=true; Snapshot.PreferredGoodStableId=PreferredGood;
	if(PreferredGood==TEXT("Good.Salt")&&Snapshot.Routes.ContainsByPredicate([](const auto& R){return R.RouteValue==2;}))Snapshot.SelectedRouteValue=2;
	else if(PreferredGood==TEXT("Good.Grain")&&Snapshot.Routes.ContainsByPredicate([](const auto& R){return R.RouteValue==1;}))Snapshot.SelectedRouteValue=1;
	if(Runtime.IsValid()){const auto P=Runtime->BuildProjection();if(P){const auto* R=P.Value.GetRoutes().FindByPredicate([this](const auto& X){return static_cast<int64>(X.Id.GetValue())==Snapshot.SelectedRouteValue;});if(R){DraftStops=R->Stops;Snapshot.SelectedStopIndex=0;Snapshot.bDirty=false;RebuildStops();}}}
	Snapshot.FocusedSemanticId=TEXT("TradeMap.Close"); PublishIfChanged(Previous); return Previous.bOpen != Snapshot.bOpen || Previous.PreferredGoodStableId != PreferredGood;
}
bool UHansaTradeMapPresentationModel::CloseIntent(){ if(!Snapshot.bOpen)return false; const auto Previous=Snapshot; Snapshot.bOpen=false; Snapshot.FocusedSemanticId=FocusOriginSemanticId; PublishIfChanged(Previous); FocusRestoreRequested.Broadcast(FocusOriginSemanticId); return true; }
bool UHansaTradeMapPresentationModel::CycleModeFilterIntent(){ const auto Previous=Snapshot; Snapshot.ModeFilter=static_cast<EHansaTradeMapModeFilter>((static_cast<uint8>(Snapshot.ModeFilter)+1)%3);Snapshot.Routes.Reset();for(const auto& Route:AllRoutes){if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Sea&&!Route.bSea)continue;if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Land&&Route.bSea)continue;Snapshot.Routes.Add(Route);}if(!Snapshot.Routes.ContainsByPredicate([this](const auto&R){return R.RouteValue==Snapshot.SelectedRouteValue;})&&!Snapshot.Routes.IsEmpty()){Snapshot.SelectedRouteValue=Snapshot.Routes[0].RouteValue;SelectRouteIntent(Snapshot.SelectedRouteValue);}PublishIfChanged(Previous);return true; }
bool UHansaTradeMapPresentationModel::SelectRouteIntent(const int64 Value){ const auto* Found=Snapshot.Routes.FindByPredicate([Value](const auto& R){return R.RouteValue==Value;}); if(!Found)return false; const auto Previous=Snapshot; Snapshot.SelectedRouteValue=Value; Snapshot.SelectedStopIndex=0; Snapshot.bDirty=false; Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Route.%lld"),Value)); if(Runtime.IsValid()){const auto P=Runtime->BuildProjection(); if(P){const auto* R=P.Value.GetRoutes().FindByPredicate([Value](const auto& X){return static_cast<int64>(X.Id.GetValue())==Value;}); if(R)DraftStops=R->Stops;}} RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::SelectStopIntent(const int32 Index){ if(!DraftStops.IsValidIndex(Index))return false; const auto Previous=Snapshot; Snapshot.SelectedStopIndex=Index; Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Stop.%d"),Index)); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::CycleCargoActionIntent(){ if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.Kind=A.Kind==EHansaRouteCargoActionKind::Load?EHansaRouteCargoActionKind::Unload:EHansaRouteCargoActionKind::Load; Snapshot.bDirty=true; RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::AdjustQuantityIntent(const int32 Delta){ if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.QuantityLimit=FHansaQuantity::FromRaw(FMath::Max<int64>(1000,A.QuantityLimit.GetRawValue()+Delta)); Snapshot.bDirty=true; RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::AdjustMinimumReserveIntent(const int32 Delta){ if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.MinimumSourceReserve=FHansaQuantity::FromRaw(FMath::Max<int64>(0,A.MinimumSourceReserve.GetRawValue()+Delta)); Snapshot.bDirty=true; RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::MoveStopIntent(const int32 Direction){const int32 To=Snapshot.SelectedStopIndex+Direction;if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||!DraftStops.IsValidIndex(To))return false;const auto Previous=Snapshot;DraftStops.Swap(Snapshot.SelectedStopIndex,To);Snapshot.SelectedStopIndex=To;Snapshot.bDirty=true;RebuildStops();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CommitIntent(){if(!Runtime.IsValid()||!Snapshot.bDirty)return false;const auto Id=FHansaRouteId::TryCreate(static_cast<uint64>(Snapshot.SelectedRouteValue));if(!Id)return false;const auto Result=Runtime->EditRoute(Id.Value,DraftStops);const auto Previous=Snapshot;Snapshot.EditorStatus=Result?LOCTEXT("Saved","Route changes saved through the command gateway."):FText::Format(LOCTEXT("SaveFailed","Route could not be saved: {0}."),FText::FromString(LexToString(Result.GetRoutePlanError())));if(Result)Snapshot.bDirty=false;PublishIfChanged(Previous);return !!Result;}
bool UHansaTradeMapPresentationModel::ToggleActiveIntent()
{
	if (!Runtime.IsValid()) return false;
	const auto Id = FHansaRouteId::TryCreate(static_cast<uint64>(Snapshot.SelectedRouteValue));
	const FHansaTradeMapRoutePresentation* Route = FindSelectedRoute();
	if (!Id || Route == nullptr) return false;
	if (!Route->bCanToggleActive)
	{
		const auto Previous = Snapshot;
		Snapshot.EditorStatus = Route->ToggleActionHint;
		PublishIfChanged(Previous);
		return false;
	}
	const bool bActivate = !Route->bActive;
	if (Snapshot.bDirty && !CommitIntent()) return false;
	const auto Previous = Snapshot;
	const FHansaCommandGatewayResult Result = Runtime->SetRouteActive(Id.Value, bActivate);
	Snapshot.EditorStatus = Result
		? (bActivate ? LOCTEXT("Started", "Route started. It will repeat until paused.")
			: LOCTEXT("Paused", "Route paused. Cargo and route progress are preserved."))
		: RouteToggleFailure(Result);
	PublishIfChanged(Previous);
	return !!Result;
}
void UHansaTradeMapPresentationModel::SetCompact(const bool Value){if(Snapshot.bCompact==Value)return;const auto Previous=Snapshot;Snapshot.bCompact=Value;PublishIfChanged(Previous);}
void UHansaTradeMapPresentationModel::SetFocusedSemanticId(const FName Id){if(Snapshot.FocusedSemanticId==Id)return;const auto Previous=Snapshot;Snapshot.FocusedSemanticId=Id;PublishIfChanged(Previous);}
const FHansaTradeMapRoutePresentation* UHansaTradeMapPresentationModel::FindSelectedRoute()const{return Snapshot.Routes.FindByPredicate([this](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});}
void UHansaTradeMapPresentationModel::RebuildStops(){Snapshot.Stops.Reset();const auto* SelectedRoute=FindSelectedRoute();for(int32 I=0;I<DraftStops.Num();++I){const auto& S=DraftStops[I];FHansaTradeMapStopPresentation P;P.Index=I;P.CityStableId=FName(*S.CityId.ToString());P.CityLabel=CityLabel(S.CityId.ToString());if(!S.Actions.IsEmpty()){const auto& A=S.Actions[0];P.GoodStableId=FName(*A.GoodId.ToString());P.ActionLabel=A.Kind==EHansaRouteCargoActionKind::Load?LOCTEXT("Load","Load"):LOCTEXT("Unload","Unload");P.Quantity=FText::Format(LOCTEXT("Quantity","{0} cargo"),FText::AsNumber(A.QuantityLimit.GetRawValue()/1000));P.MinimumReserve=FText::Format(LOCTEXT("Reserve","{0} minimum reserve"),FText::AsNumber(A.MinimumSourceReserve.GetRawValue()/1000));P.bReserveRisk=SelectedRoute&&SelectedRoute->bReserveRisk&&A.Kind==EHansaRouteCargoActionKind::Load;P.AccessibleLabel=FText::Format(LOCTEXT("StopAccessible","Stop {0}, {1}, {2} {3}, {4}.{5}"),FText::AsNumber(I+1),P.CityLabel,P.ActionLabel,P.Quantity,P.MinimumReserve,P.bReserveRisk?LOCTEXT("StopRisk"," Reserve risk."):FText());}Snapshot.Stops.Add(MoveTemp(P));}Snapshot.ReserveRisk=SelectedRoute&&SelectedRoute->bReserveRisk?LOCTEXT("Risk","⚠ Reserve risk · planned load exceeds stock above the protected reserve."):LOCTEXT("Safe","✓ Protected reserve remains intact.");Snapshot.EditorStatus=Snapshot.bDirty?LOCTEXT("Unsaved","Unsaved route changes."):Snapshot.EditorStatus;}
void UHansaTradeMapPresentationModel::PublishIfChanged(const FHansaTradeMapSnapshot& Previous){if(Snapshot==Previous)return;++Revision;Changed.Broadcast(Snapshot,Revision);}

#undef LOCTEXT_NAMESPACE
