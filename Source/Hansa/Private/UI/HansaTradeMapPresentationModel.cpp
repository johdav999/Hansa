#include "UI/HansaTradeMapPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"

#define LOCTEXT_NAMESPACE "HansaTradeMapPresentationModel"
using namespace Hansa::Simulation;

namespace
{
	#include "HansaTradeCityLocations.inl"
	void AppendClientRouteStops(const TConstArrayView<FHansaRouteStop> Stops, FHansaClientCommandIntent& Intent)
	{
		for (const FHansaRouteStop& Stop : Stops)
		{
			FHansaClientRouteStopIntent& ClientStop = Intent.RouteStops.AddDefaulted_GetRef();
			ClientStop.CityId = Stop.CityId.ToString();
			for (const FHansaRouteCargoAction& Action : Stop.Actions)
			{
				FHansaClientRouteActionIntent& ClientAction = ClientStop.Actions.AddDefaulted_GetRef();
				ClientAction.Kind = static_cast<uint8>(Action.Kind);
				ClientAction.GoodId = Action.GoodId.ToString();
				ClientAction.QuantityMilliUnits = Action.QuantityLimit.GetRawValue();
				ClientAction.MinimumSourceReserveMilliUnits = Action.MinimumSourceReserve.GetRawValue();
			}
		}
	}
	FText CityLabel(const FString& Id)
	{
		if (Id.EndsWith(TEXT("Lubeck"))) return LOCTEXT("Lubeck", "Lübeck");
		if (Id.EndsWith(TEXT("Hamburg"))) return LOCTEXT("Hamburg", "Hamburg");
		if (Id.EndsWith(TEXT("Luneburg"))) return LOCTEXT("Luneburg", "Lüneburg");
		if (Id.EndsWith(TEXT("Rostock"))) return LOCTEXT("Rostock", "Rostock");
		return FText::FromString(Id.StartsWith(TEXT("City.")) ? Id.RightChop(5) : Id);
	}
	FVector2D CityPosition(const FHansaCompiledCityMarketProfileDefinition& City)
	{
		if (City.MapLongitudeMilliDegrees != 0 || City.MapLatitudeMilliDegrees != 0)
		{
			const double X=FMath::Clamp((City.MapLongitudeMilliDegrees/1000.0+1.0)/33.0,.04,.96);
			const double Y=FMath::Clamp(1.0-(City.MapLatitudeMilliDegrees/1000.0-50.0)/11.0,.06,.94);
			return {X,Y};
		}
		if(const auto* Known=TradeCityLocations.Find(City.StableId))return *Known;
		auto Position=[](double Longitude,double Latitude){return FVector2D((Longitude+1)/33.,(61-Latitude)/11.);};
		if (City.StableId.EndsWith(TEXT("Hamburg"))) return Position(9.9933,53.5503);
		if (City.StableId.EndsWith(TEXT("Luneburg"))) return Position(10.4149,53.2487);
		if (City.StableId.EndsWith(TEXT("Rostock"))) return Position(12.14,54.0887);
		if (City.StableId.EndsWith(TEXT("Lubeck"))) return Position(10.6866,53.8677);
		return {-1,-1}; // Never invent a geographic location from a stable-ID hash.
	}
	FVector2D CityPosition(const FString& Id)
	{
		if (Id.EndsWith(TEXT("Hamburg"))) return { .18f, .66f };
		if (Id.EndsWith(TEXT("Luneburg"))) return { .30f, .83f };
		if (Id.EndsWith(TEXT("Rostock"))) return { .78f, .56f };
		return { .25f, .72f };
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
		case EHansaCommandGatewayError::ResearchEffectRequired:
			return LOCTEXT("ToggleResearchRequired", "Complete the research shown for this route before starting its scheduled operation. Open Research, finish the required technology, then try again.");
		default:
			return FText::Format(LOCTEXT("ToggleFailedWithCause", "Route state could not be changed ({0}). Try again after the next simulation tick."),
				FText::FromString(LexToString(Result.GetError())));
		}
	}
}

bool operator==(const FHansaTradeMapCityPresentation& A,const FHansaTradeMapCityPresentation& B){return A.StableId==B.StableId&&A.Label.EqualTo(B.Label)&&A.NormalizedPosition==B.NormalizedPosition&&A.Information.EqualTo(B.Information)&&A.CapabilitySummary.EqualTo(B.CapabilitySummary)&&A.bOwned==B.bOwned&&A.bStale==B.bStale&&A.bUnknown==B.bUnknown&&A.bRendered==B.bRendered&&A.bVisitable==B.bVisitable&&A.bBuildable==B.bBuildable&&A.bMarketOnly==B.bMarketOnly&&A.bHasPresence==B.bHasPresence&&A.bHasRoute==B.bHasRoute&&A.ReportAgeTicks==B.ReportAgeTicks;}
bool operator==(const FHansaTradeMapStopPresentation& A,const FHansaTradeMapStopPresentation& B){return A.Index==B.Index&&A.CityStableId==B.CityStableId&&A.CityLabel.EqualTo(B.CityLabel)&&A.GoodStableId==B.GoodStableId&&A.ActionLabel.EqualTo(B.ActionLabel)&&A.Quantity.EqualTo(B.Quantity)&&A.MinimumReserve.EqualTo(B.MinimumReserve)&&A.AccessibleLabel.EqualTo(B.AccessibleLabel)&&A.bReserveRisk==B.bReserveRisk;}
bool operator==(const FHansaTradeMapRoutePresentation& A,const FHansaTradeMapRoutePresentation& B){return A.RouteValue==B.RouteValue&&A.VehicleValue==B.VehicleValue&&A.DefinitionStableId==B.DefinitionStableId&&A.Label.EqualTo(B.Label)&&A.Mode.EqualTo(B.Mode)&&A.State.EqualTo(B.State)&&A.Ownership.EqualTo(B.Ownership)&&A.StateHeading.EqualTo(B.StateHeading)&&A.StateDetail.EqualTo(B.StateDetail)&&A.ToggleActionLabel.EqualTo(B.ToggleActionLabel)&&A.ToggleActionHint.EqualTo(B.ToggleActionHint)&&A.StopSummary.EqualTo(B.StopSummary)&&A.Capacity.EqualTo(B.Capacity)&&A.Upkeep.EqualTo(B.Upkeep)&&A.RoundTripTime.EqualTo(B.RoundTripTime)&&A.ExpectedProfitRange.EqualTo(B.ExpectedProfitRange)&&A.Uncertainty.EqualTo(B.Uncertainty)&&A.bSea==B.bSea&&A.bActive==B.bActive&&A.bOwnedByPlayer==B.bOwnedByPlayer&&A.bCanCancel==B.bCanCancel&&A.bCanToggleActive==B.bCanToggleActive&&A.bTraveling==B.bTraveling&&A.bReserveRisk==B.bReserveRisk&&A.bProfitKnown==B.bProfitKnown;}
bool operator==(const FHansaTradeMapSnapshot& A,const FHansaTradeMapSnapshot& B){return A.SelectedCityStableId==B.SelectedCityStableId&&A.PresenceSpecializationComparison.EqualTo(B.PresenceSpecializationComparison)&&A.PresenceSpecializationAction.EqualTo(B.PresenceSpecializationAction)&&A.PresenceSpecializationFeedback.EqualTo(B.PresenceSpecializationFeedback)&&A.SelectedPresenceSpecializationId==B.SelectedPresenceSpecializationId&&A.bCanPresenceSpecializationAction==B.bCanPresenceSpecializationAction&&A.PresenceProgress.EqualTo(B.PresenceProgress)&&A.PresenceUpgradeAction.EqualTo(B.PresenceUpgradeAction)&&A.bCanPresenceUpgradeAction==B.bCanPresenceUpgradeAction&&A.StationOrderText.EqualTo(B.StationOrderText)&&A.StationOrderFeedback.EqualTo(B.StationOrderFeedback)&&A.bShipInTransit==B.bShipInTransit&&A.ShipPosition==B.ShipPosition&&A.bCreating==B.bCreating&&A.bReview==B.bReview&&A.bCanCreate==B.bCanCreate&&A.bReassignCog==B.bReassignCog&&A.CogValue==B.CogValue&&A.DraftName==B.DraftName&&A.CogLabel.EqualTo(B.CogLabel)&&A.CreatorReview.EqualTo(B.CreatorReview)&&A.Validation.EqualTo(B.Validation)&&A.Cities==B.Cities&&A.Routes==B.Routes&&A.Stops==B.Stops&&A.FocusedSemanticId==B.FocusedSemanticId&&A.PreferredGoodStableId==B.PreferredGoodStableId&&A.Title.EqualTo(B.Title)&&A.EditorStatus.EqualTo(B.EditorStatus)&&A.ReserveRisk.EqualTo(B.ReserveRisk)&&A.SelectedRouteValue==B.SelectedRouteValue&&A.SelectedStopIndex==B.SelectedStopIndex&&A.ModeFilter==B.ModeFilter&&A.CityFilter==B.CityFilter&&A.CitySearchText==B.CitySearchText&&A.MatchingCityCount==B.MatchingCityCount&&A.MatchingRouteCount==B.MatchingRouteCount&&A.RouteWindowStart==B.RouteWindowStart&&A.bOpen==B.bOpen&&A.bCompact==B.bCompact&&A.bDirty==B.bDirty&&A.TradeStationValue==B.TradeStationValue&&A.TradeStationState.EqualTo(B.TradeStationState)&&A.TradeStationDetail.EqualTo(B.TradeStationDetail)&&A.TradeStationAction.EqualTo(B.TradeStationAction)&&A.bCanTradeStationAction==B.bCanTradeStationAction;}

void UHansaTradeMapPresentationModel::InitializeDefaults()
{
	Snapshot = {}; Snapshot.Title = LOCTEXT("Title", "Baltic trade"); Snapshot.EditorStatus = LOCTEXT("Empty", "Select a route to edit its stops and cargo rules.");
}

void UHansaTradeMapPresentationModel::BindRuntime(UHansaRuntimeSimulationHost* RuntimeHost) { Runtime = RuntimeHost; }

bool UHansaTradeMapPresentationModel::ApplyProjection(const FHansaSimulationProjection& Projection, const FHansaEconomicRegistry& Registry)
{
	const FHansaTradeMapSnapshot Previous = Snapshot;
	LastProjection=MakeShared<FHansaSimulationProjection>(Projection);LastRegistry=&Registry;
	SelectedStationSiteId.Reset();
	if(const auto* Policy=Registry.FindCityTradePolicyForCity(Snapshot.SelectedCityStableId.ToString()))
		if(!Policy->TradeStationSites.IsEmpty())SelectedStationSiteId=Policy->TradeStationSites[0].SiteId;
	Snapshot.Cities.Reset(); Snapshot.Routes.Reset(); AllCities.Reset(); AllRoutes.Reset(); AvailableGoods.Reset();
	for(const auto& Good:Registry.GetGoods())AvailableGoods.Add(FName(*Good.StableId));
	AvailableGoods.Sort(FNameLexicalLess());
	if(Snapshot.PreferredGoodStableId.IsNone()&&!AvailableGoods.IsEmpty())Snapshot.PreferredGoodStableId=AvailableGoods.Contains(TEXT("Good.Grain"))?FName(TEXT("Good.Grain")):AvailableGoods[0];
	StationOrders.Reset(); OrderGoods.Reset(); StationOrderCapacity=0;
	PresenceUpgradeStageId.Reset();PresenceFundingInventoryId=FHansaInventoryId();bPresenceUpgradeFunding=false;PresenceSpecializations.Reset();PresenceSpecializationRevision=0;Snapshot.PresenceSpecializationComparison=FText();Snapshot.PresenceSpecializationAction=LOCTEXT("SpecializationUnavailable","Specializations require an active Merchant Office");Snapshot.bCanPresenceSpecializationAction=false;Snapshot.PresenceProgress=FText();Snapshot.PresenceUpgradeAction=LOCTEXT("PresenceNoUpgrade","No presence review available");Snapshot.bCanPresenceUpgradeAction=false;
	Snapshot.TradeStationValue=0;Snapshot.TradeStationState=FText::Format(LOCTEXT("SelectedContact","{0} · Visiting contact"),CityLabel(Snapshot.SelectedCityStableId.ToString()));Snapshot.TradeStationDetail=LOCTEXT("StationOpportunity","No leased plot or station storage. Meet the listed presence requirements, then establish a station at the harbor site.");Snapshot.TradeStationAction=LOCTEXT("EstablishStation","Establish trade station");Snapshot.bCanTradeStationAction=false;
	const FHansaHouseId PlayerHouse=Runtime.IsValid()?Runtime->GetHouseId():FHansaHouseId();
	if(const auto* Presence=Projection.GetForeignPresences().FindByPredicate([&](const auto& V){return (!PlayerHouse.IsValid()||V.HouseId==PlayerHouse)&&V.CityId.ToString()==Snapshot.SelectedCityStableId.ToString();}))
	{
		// Proposal reserves the plot; construction goods and money are paid by the separate funding command.
		Snapshot.bCanTradeStationAction=!SelectedStationSiteId.IsEmpty()&&Presence->Status==EHansaForeignPresenceStatus::Active&&!Presence->StationId.IsValid()&&Presence->NextStages.ContainsByPredicate([&](const auto& V){const auto* Stage=Registry.FindPresenceStage(V.StageId);return V.bProgressRequirementsMet&&Stage&&Stage->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"));});
		FString Lines=FString::Printf(TEXT("%s · lawful volume %lld · deliveries %lld · transaction value %lld pfennig · shortage relief %lld\nReliable %lld ticks · solvent %lld ticks\n"),*Presence->CurrentStageDisplayName,Presence->Contributions.LawfulTradeVolumeMilliUnits,Presence->Contributions.CompletedDeliveryCount,Presence->Contributions.TransactionValuePfennig,Presence->Contributions.FulfilledShortageMilliUnits,Presence->Contributions.ReliableOperatingTicks,Presence->Contributions.SolventOperatingTicks);
		if(!Presence->StationId.IsValid())Lines+=TEXT("Establish after meeting contribution requirements. Then fund construction from your Cog cargo. Material availability below refers to station storage, not your Cog.\n");
		const auto* Next=Presence->NextStages.IsEmpty()?nullptr:&Presence->NextStages[0];
		if(Next){PresenceUpgradeStageId=Next->StageId;Lines+=FString::Printf(TEXT("Next: %s\n"),*Next->DisplayName);for(const auto& R:Next->Requirements)Lines+=FString::Printf(TEXT("%s %lld/%lld%s\n"),*R.Description,R.CurrentValue,R.RequiredValue,R.bMet?TEXT(" ✓"):TEXT(" — unmet"));const auto* Stage=Registry.FindPresenceStage(Next->StageId);if(Stage){Lines+=TEXT("Unlocks: ");for(const auto& Id:Stage->GrantedCapabilityIds)if(!Presence->Capabilities.ContainsByPredicate([&](const auto& C){return C.CapabilityId==Id&&C.bGranted;}))Lines+=Id.RightChop(19)+TEXT(" · ");}}
		if(Presence->Status==EHansaForeignPresenceStatus::Suspended)Lines+=TEXT("Alert: presence suspended; upgrades are unavailable until status is restored.\n");
		else if(Presence->Status==EHansaForeignPresenceStatus::Revoked)Lines+=TEXT("Alert: presence revoked; this office cannot be upgraded.\n");
		if(Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Funded){Lines+=FString::Printf(TEXT("Construction funded · completes tick %lld\n"),Presence->Upgrade.CompletionTick.GetValue());Snapshot.PresenceUpgradeAction=LOCTEXT("PresenceBuilding","Merchant Office under construction");}
		else if(Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Requested){bPresenceUpgradeFunding=true;Snapshot.PresenceUpgradeAction=LOCTEXT("FundOffice","Fund Merchant Office");Snapshot.bCanPresenceUpgradeAction=Next&&Next->bFundingAvailable;if(!Snapshot.bCanPresenceUpgradeAction)Lines+=TEXT("Alert: funding unavailable; review the exact money and station-material requirements above.\n");}
		else{Snapshot.PresenceUpgradeAction=LOCTEXT("RequestOffice","Request Merchant Office review");Snapshot.bCanPresenceUpgradeAction=Next&&Next->bProgressRequirementsMet;if(Next&&!Next->bProgressRequirementsMet)Lines+=TEXT("Review blocked: complete every unmet contribution requirement above.\n");}
		PresenceSpecializations=Presence->Specializations;PresenceSpecializationRevision=Presence->SpecializationRevision;
		if(!PresenceSpecializations.IsEmpty())
		{
			if(!PresenceSpecializations.ContainsByPredicate([&](const auto& V){return V.SpecializationId==Snapshot.SelectedPresenceSpecializationId;}))
			{
				const auto* First=PresenceSpecializations.FindByPredicate([](const auto& V){return V.bAvailable;});Snapshot.SelectedPresenceSpecializationId=First?First->SpecializationId:PresenceSpecializations[0].SpecializationId;
			}
			FString Compare=TEXT("Choose one exclusive Merchant Office specialization. Goods are non-refundable; respec returns 25% of the prior money cost.\n");
			for(const auto& O:PresenceSpecializations)Compare+=FString::Printf(TEXT("%s%s — %lld pfennig; storage +%lld; order slots +%d; station handling +%lld%s%s\n"),O.SpecializationId==Snapshot.SelectedPresenceSpecializationId?TEXT("▶ "):TEXT(""),*O.DisplayName,O.InvestmentCostPfennig,O.StorageCapacityBonusMilliUnits,O.AdditionalOrderSlots,O.StationTransferCapBonusMilliUnits,O.bSelected?TEXT(" — selected"):TEXT(""),O.Blocker.IsEmpty()?TEXT(""):*FString(TEXT(" — ")+O.Blocker));
			Snapshot.PresenceSpecializationComparison=FText::FromString(Compare);const auto* Selected=PresenceSpecializations.FindByPredicate([&](const auto& V){return V.SpecializationId==Snapshot.SelectedPresenceSpecializationId;});
			const bool bHasSelection=PresenceSpecializations.ContainsByPredicate([](const auto& V){return V.bSelected;});Snapshot.bCanPresenceSpecializationAction=Selected&&!Selected->bSelected&&Selected->bAvailable;Snapshot.PresenceSpecializationAction=bHasSelection?LOCTEXT("RespecSpecialization","Review and respec specialization"):LOCTEXT("SelectSpecialization","Review and establish specialization");
		}
		const auto* AuthorityPolicy=Registry.FindCityTradePolicyForCity(Presence->CityId.ToString());if(AuthorityPolicy)
		{
			Lines+=TEXT("\nCommercial presence / scoped privileges / city partnership / governance\n");
			for(const auto& V:AuthorityPolicy->Privileges){const auto* Active=Presence->Privileges.FindByPredicate([&](const auto& X){return X.PrivilegeId==V.PrivilegeId&&X.Status==EHansaCityPrivilegeStatus::Active;});Lines+=FString::Printf(TEXT("Privilege: %s — %lld pfennig — %s — issuer city / recipient house — city stock, roads and buildings remain autonomous%s\n"),*V.DisplayName,V.CostPfennig,V.bReversible?TEXT("reversible while plot is empty"):TEXT("permanent"),Active?TEXT(" — active"):TEXT(""));}
			for(const auto& V:AuthorityPolicy->CityProjects){const auto* Active=Presence->CityProjects.FindByPredicate([&](const auto& X){return X.ProjectId==V.ProjectId;});Lines+=FString::Printf(TEXT("Partnership: %s — shared reserve +%lld — %s\n"),*V.DisplayName,V.SharedReserveBonusMilliUnits,Active?(Active->Status==EHansaCityProjectStatus::Completed?TEXT("completed"):TEXT("funded / constructing")):TEXT("available for review"));}
			Lines+=AuthorityPolicy->bExceptionalGovernanceAllowed?FString::Printf(TEXT("Governance: exceptional scenario charter %s; existing city assets, debts, routes and inventories do not transfer%s\n"),*AuthorityPolicy->GovernanceCharterId,Presence->bGovernanceAuthority?TEXT(" — granted"):TEXT("")):TEXT("Governance: not permitted by city policy; trade volume cannot confer sovereignty\n");
		}
		Lines+=TEXT("History: ");for(int32 I=FMath::Max(0,Presence->History.Num()-4);I<Presence->History.Num();++I){const TCHAR* Kind=TEXT("Contribution accepted");switch(Presence->History[I].Kind){case EHansaPresenceHistoryKind::UpgradeRequested:Kind=TEXT("Upgrade requested");break;case EHansaPresenceHistoryKind::UpgradeFunded:Kind=TEXT("Upgrade funded");break;case EHansaPresenceHistoryKind::UpgradeCompleted:Kind=TEXT("Upgrade completed");break;default:break;}Lines+=FString::Printf(TEXT("tick %lld %s · "),Presence->History[I].Tick.GetValue(),Kind);}Snapshot.PresenceProgress=FText::FromString(Lines);
	}
	if(const auto* Station=Projection.GetTradeStations().FindByPredicate([&](const auto& V){return (!PlayerHouse.IsValid()||V.Station.OwnerId==PlayerHouse)&&V.Station.CityId.ToString()==Snapshot.SelectedCityStableId.ToString();}))
	{
		Snapshot.TradeStationValue=static_cast<int64>(Station->Station.Id.GetValue());
        StationOrders=Station->Station.Orders; StationOrderCapacity=Station->StorageCapacity.GetRawValue();
		PresenceFundingInventoryId=Station->Station.InventoryId;
        const auto* Policy=Registry.FindCityTradePolicyForCity(Station->Station.CityId.ToString());
        if(Policy){StationOrderMaxCap=Policy->MaximumOrderCapMilliUnits;StationOrderMaxBudget=Policy->MaximumOrderBudgetPfennig;}
        for(const auto& Market:Projection.GetMarkets()) if(Market.CityId==Station->Station.CityId) OrderGoods.Add(Market.GoodId);
        OrderGoods.Sort();
        if(!OrderDraft.GoodId.IsValid()&&!OrderGoods.IsEmpty()){OrderDraft.GoodId=OrderGoods[0];OrderDraft.TargetOrReserveMilliUnits=FMath::Min<int64>(10000,StationOrderCapacity);OrderDraft.TotalBudgetPfennig=FMath::Min<int64>(10000,StationOrderMaxBudget);}

		const TCHAR* Operational=TEXT("Active");switch(Station->Station.OperationalState){case EHansaTradeStationOperationalState::Underfunded:Operational=TEXT("Underfunded");break;case EHansaTradeStationOperationalState::StorageBlocked:Operational=TEXT("Storage blocked");break;case EHansaTradeStationOperationalState::OrderSuspended:Operational=TEXT("Orders suspended");break;case EHansaTradeStationOperationalState::RightsSuspended:Operational=TEXT("Rights suspended");break;case EHansaTradeStationOperationalState::VoluntarilyClosed:Operational=TEXT("Voluntarily closed");break;case EHansaTradeStationOperationalState::Revoked:Operational=TEXT("Revoked");break;default:break;}
		Snapshot.TradeStationState=FText::Format(LOCTEXT("SelectedStationState","{1} station · {0}"),FText::FromString(Station->Station.Status==EHansaTradeStationStatus::Proposed?TEXT("Proposed"):Station->Station.Status==EHansaTradeStationStatus::UnderConstruction?TEXT("Under construction"):Operational),CityLabel(Snapshot.SelectedCityStableId.ToString()));
		Snapshot.TradeStationDetail=FText::Format(LOCTEXT("StationDetail","Site {0} · Factor #{1} · Lease #{2}\nStorage {3}/{4} units · reserved {5} · upkeep {6} pfennig/tick · arrears {7}\n{8}\nPreserved: {9}\nCosts: {10}\nRecovery: {11}"),FText::FromString(Station->Station.SiteId),FText::AsNumber(Station->Station.FactorId.GetValue()),FText::AsNumber(Station->Station.LeasedPlotId.GetValue()),FText::AsNumber(double(Station->StorageUsed.GetRawValue())/1000.),FText::AsNumber(double(Station->StorageCapacity.GetRawValue())/1000.),FText::AsNumber(double(Station->StorageReserved.GetRawValue())/1000.),FText::AsNumber(Station->Station.UpkeepPfennigPerTick),FText::AsNumber(Station->Station.OutstandingUpkeepPfennig),FText::FromString(Station->Blocker),FText::FromString(Station->PreservedAssets),FText::FromString(Station->ContinuingCosts),FText::FromString(Station->NextStep));
		if(Station->Station.Status==EHansaTradeStationStatus::Proposed)Snapshot.TradeStationAction=LOCTEXT("FundStation","Fund construction");
		else if(Station->Station.OperationalState==EHansaTradeStationOperationalState::Underfunded)Snapshot.TradeStationAction=LOCTEXT("PayStationArrears","Pay arrears and reopen");
		else if(Station->Station.Status==EHansaTradeStationStatus::Closed)Snapshot.TradeStationAction=LOCTEXT("FinalizeStationClosure","Finalize empty closure");
		else Snapshot.TradeStationAction=LOCTEXT("CloseStation","Close station safely");
		Snapshot.bCanTradeStationAction=Station->Station.Status!=EHansaTradeStationStatus::Closed||(Station->StorageUsed.GetRawValue()==0&&Station->StorageReserved.GetRawValue()==0&&Station->Lease.OccupyingBuildingIds.IsEmpty());
	}
	// Establishment has its own command. Do not advertise an office upgrade while the station is absent or unfinished.
	const auto* ActiveStation=Projection.GetTradeStations().FindByPredicate([&](const auto& V){return (!PlayerHouse.IsValid()||V.Station.OwnerId==PlayerHouse)&&V.Station.CityId.ToString()==Snapshot.SelectedCityStableId.ToString()&&V.Station.Status==EHansaTradeStationStatus::Active;});
	if(!ActiveStation)
	{
		Snapshot.bCanPresenceUpgradeAction=false;
		Snapshot.PresenceUpgradeAction=LOCTEXT("OfficeNeedsStation","Merchant Office requires an active trade station");
	}
	RefreshStationOrderText();
	for (const FHansaCompiledCityMarketProfileDefinition& Profile : Registry.GetCityMarkets())
	{
		const auto CityId = FHansaCityDefinitionId::TryParse(Profile.StableId); if (!CityId) continue;
		FHansaTradeMapCityPresentation City; City.StableId = FName(*Profile.StableId); City.Label = CityLabel(Profile.StableId); City.NormalizedPosition = CityPosition(Profile); City.bOwned = Profile.StableId == TEXT("City.Lubeck");
		City.bMarketOnly=Profile.PresentationClass==1;City.bRendered=Profile.PresentationClass==2||Profile.PresentationClass==3;City.bVisitable=City.bRendered;City.bBuildable=Profile.PresentationClass==3;
		City.bHasPresence=Projection.GetForeignPresences().ContainsByPredicate([&](const auto& Presence){return Presence.CityId==CityId.Value&&(!PlayerHouse.IsValid()||Presence.HouseId==PlayerHouse);});
		City.bHasRoute=Projection.GetRoutes().ContainsByPredicate([&](const auto& Route){return Route.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==CityId.Value;});});
		int64 MaxAge = 0; bool bAny = false; bool bAllUnknown = true;
		for (const FHansaCityMarketProjection& Market : Projection.GetMarkets()) if (Market.CityId == CityId.Value)
		{
			bAny = true; MaxAge = FMath::Max(MaxAge, Market.ReportAgeTicks); City.bStale |= Market.bIsStale; bAllUnknown &= Market.LastUpdateTick < 0;
		}
		City.bUnknown = !bAny || bAllUnknown;
        if (Runtime.IsValid())
        {
			const auto Good=FHansaGoodId::TryParse(Snapshot.PreferredGoodStableId.ToString());const auto Report = Good?Runtime->QueryKnownMarketPrice(CityId.Value,Good.Value):TOptional<FHansaKnownMarketPriceProjection>();
            City.bUnknown = !Report.IsSet() || !Report->ReportAgeTicks.IsSet();
            City.bStale = Report.IsSet() && Report->InformationState != EHansaMarketInformationState::Current && !City.bUnknown;
            MaxAge = Report.IsSet() ? Report->ReportAgeTicks.Get(0) : 0;
        }
		City.ReportAgeTicks=MaxAge;
		City.Information = City.bUnknown ? LOCTEXT("NoReport", "No recent report") : City.bStale
			? FText::Format(LOCTEXT("StaleReport", "Stale report · {0} ticks old"), FText::AsNumber(MaxAge))
			: FText::Format(LOCTEXT("CurrentReport", "Current · {0} ticks old"), FText::AsNumber(MaxAge));
		if(Runtime.IsValid()) {
			const auto Good=FHansaGoodId::TryParse(Snapshot.PreferredGoodStableId.ToString());const auto Report=Good?Runtime->QueryKnownMarketPrice(CityId.Value,Good.Value):TOptional<FHansaKnownMarketPriceProjection>();
            if(Report.IsSet()&&!City.bUnknown)City.Information=FText::Format(LOCTEXT("KnownReportAge","{0} · {1} ticks old"),FText::FromString(LexToString(Report->InformationState)),FText::AsNumber(MaxAge));
        }
        for (const auto& Entry : Projection.GetTradeStations())
            if (Entry.Station.CityId == CityId.Value && Entry.Station.OwnerId == (Runtime.IsValid() ? Runtime->GetHouseId() : FHansaHouseId::TryCreate(1).Value) && Entry.Station.Status == EHansaTradeStationStatus::Active)
                City.Information = FText::Format(LOCTEXT("BufferedCity", "{0} · Your station: {1}/{2} units"), City.Information, FText::AsNumber(double(Entry.StorageUsed.GetRawValue())/1000.), FText::AsNumber(double(Entry.StorageCapacity.GetRawValue())/1000.));
		City.CapabilitySummary=City.bBuildable?LOCTEXT("BuildableCity","Rendered · visitable · buildable"):City.bVisitable?LOCTEXT("VisitableCity","Rendered · visitable · construction unavailable"):LOCTEXT("MarketOnlyCity","Market-only · abstract authoritative inventory · no world visit or construction");
		AllCities.Add(MoveTemp(City));
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
		Item.Mode = Item.bSea ? LOCTEXT("Sea", "Sea route") : LOCTEXT("Land", "Land route");
		const FText CurrentCity = Route.Stops.IsValidIndex(Route.CurrentStopIndex)
			? CityLabel(Route.Stops[Route.CurrentStopIndex].CityId.ToString()) : LOCTEXT("UnknownCurrentCity", "an unknown stop");
		const FText NextCity = Route.Stops.IsValidIndex(Route.NextStopIndex)
			? CityLabel(Route.Stops[Route.NextStopIndex].CityId.ToString()) : LOCTEXT("UnknownNextCity", "the next stop");
		switch (Route.Lifecycle)
		{
		case EHansaRouteLifecycleState::Inactive:
			Item.State = LOCTEXT("StoppedListState", "Stopped");
			Item.StateHeading = LOCTEXT("StoppedHeading", "Route stopped");
			Item.StateDetail = FText::Format(LOCTEXT("StoppedDetail", "Ready to depart from {0}."), CurrentCity);
			Item.ToggleActionLabel = LOCTEXT("StartRoute", "Start route");
			Item.ToggleActionHint = LOCTEXT("StartRouteHint", "Starts immediately and repeats this route.");
			Item.bCanToggleActive = Item.bOwnedByPlayer;
			break;
		case EHansaRouteLifecycleState::AtStop:
			Item.State = FText::Format(LOCTEXT("AtStopListState", "Active · At {0}"), CurrentCity);
			Item.StateHeading = LOCTEXT("ActiveHeading", "ROUTE ACTIVE");
			Item.StateDetail = FText::Format(LOCTEXT("ActiveDetail", "At {0}. Cargo actions run before the next departure."), CurrentCity);
			Item.ToggleActionLabel = LOCTEXT("PauseRoute", "Pause route");
			Item.ToggleActionHint = LOCTEXT("PauseRouteHint", "Pauses before the vehicle's next departure.");
			Item.bCanToggleActive = Item.bOwnedByPlayer;
			break;
		case EHansaRouteLifecycleState::Traveling:
			Item.State = FText::Format(LOCTEXT("TravelingListState", "In transit · {0} ticks to {1}"), FText::AsNumber(Route.RemainingTravelTicks), NextCity);
			Item.StateHeading = FText::Format(LOCTEXT("TravelingHeading", "IN TRANSIT TO {0} · {1} TICKS"), NextCity, FText::AsNumber(Route.RemainingTravelTicks));
			Item.StateDetail = LOCTEXT("TravelingDetail", "The route remains active and will continue after arrival.");
			Item.ToggleActionLabel = LOCTEXT("PauseAtStop", "Pause available at next stop");
			Item.ToggleActionHint = LOCTEXT("PauseAtStopHint", "Wait until the vehicle arrives to pause this route.");
			Item.bCanToggleActive = false;
			break;
		case EHansaRouteLifecycleState::Cancelled:
		default:
			Item.State = LOCTEXT("CancelledListState", "Cancelled");
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
		TArray<FString> Names; for (const FHansaRouteStop& Stop : Route.Stops) Names.Add(CityLabel(Stop.CityId.ToString()).ToString()); Item.StopSummary = FText::FromString(FString::Join(Names, TEXT(" / ")));
		int32 RoundTrip = 0; if (Definition != nullptr && Route.Stops.Num() > 1) for (int32 I=0; I<Route.Stops.Num(); ++I) RoundTrip += LegTicks(*Definition, Route.Stops[I].CityId.ToString(), Route.Stops[(I+1)%Route.Stops.Num()].CityId.ToString());
		Item.RoundTripTime = FText::Format(LOCTEXT("Ticks", "{0} ticks round trip"), FText::AsNumber(RoundTrip));
		if (Vehicle != nullptr)
		{
			Item.Capacity = FText::Format(LOCTEXT("Capacity", "{0} / {1} cargo"), FText::AsNumber(Vehicle->Cargo.GetRawValue()/1000), FText::AsNumber(Vehicle->Capacity.GetRawValue()/1000));
			Item.Upkeep = FText::Format(LOCTEXT("Upkeep", "{0} pfennig / round trip"), FText::AsNumber(RoundTrip * Vehicle->UpkeepPfennigPerTravelTick));
		}
        const int64 Cost = Vehicle ? RoundTrip * Vehicle->UpkeepPfennigPerTravelTick : 0;
        Item.ExpectedProfitRange = Vehicle ? FText::Format(LOCTEXT("NetCash", "{0} to {0} pfennig"), FText::AsNumber(-Cost)) : LOCTEXT("NoCashReport", "Unavailable: vehicle details missing");
        Item.Uncertainty = LOCTEXT("TransferNoSale", "Inventory transfer; no automatic sale revenue. Stock and delivery can change before arrival.");
        Item.bProfitKnown = Vehicle != nullptr;
        bool bBuffered = false;
        for (const auto& Stop : Route.Stops) for (const auto& Action : Stop.Actions) bBuffered |= IsStationTransfer(Action.Kind);
        if (bBuffered)
        {
            Item.bProfitKnown = false;
            Item.ExpectedProfitRange = LOCTEXT("BufferedCashUnknown", "Not estimated; factor orders settle independently");
            Item.Uncertainty = LOCTEXT("BufferedCash", "Station ↔ Cog transfers have no purchase or sale proceeds. Travel and station upkeep still apply. Restore station access or free stock/storage if a transfer is missed.");
        }
        if (Runtime.IsValid()) { const FString Name = Runtime->GetRouteLabel(Route.Id.GetValue()); if (!Name.IsEmpty()) Item.Label = FText::FromString(Name); }
        if (Route.LastTransfer.Outcome != EHansaRouteTransferOutcome::None)
            Item.StateDetail = FText::Format(LOCTEXT("LastTransfer", "{0} · Last {1}: {2} units at {3} ({4})"), Item.StateDetail,
                IsStationTransfer(Route.LastTransfer.Kind) ? (IsRouteLoad(Route.LastTransfer.Kind) ? LOCTEXT("StationLoading", "station load") : LOCTEXT("StationDelivery", "station unload")) : (!IsRouteLoad(Route.LastTransfer.Kind) ? LOCTEXT("Delivery", "delivery") : LOCTEXT("Loading", "load")),
                FText::AsNumber(double(Route.LastTransfer.AppliedQuantity.GetRawValue()) / 1000.0), CityLabel(Route.LastTransfer.CityId.ToString()),
                FText::FromString(LexToString(Route.LastTransfer.Outcome)));
		Item.bCanCancel = Item.bOwnedByPlayer && Item.bCanToggleActive && Vehicle && Vehicle->Cargo.GetRawValue() == 0;
		AllRoutes.Add(MoveTemp(Item));
	}
	RebuildFilteredProjection();
	if (!Snapshot.bCreating && !Snapshot.Routes.IsEmpty() && (Snapshot.SelectedRouteValue == 0 || !Snapshot.Routes.ContainsByPredicate(
		[this](const FHansaTradeMapRoutePresentation& Route){ return Route.RouteValue == Snapshot.SelectedRouteValue; })))
	{
		Snapshot.SelectedRouteValue = Snapshot.Routes[0].RouteValue;
	}
	const FHansaRouteProjection* Selected = Projection.GetRoutes().FindByPredicate([this](const FHansaRouteProjection& R){ return static_cast<int64>(R.Id.GetValue()) == Snapshot.SelectedRouteValue; });
	if (!Snapshot.bCreating && !Snapshot.bDirty && Selected != nullptr) DraftStops = Selected->Stops;
    Snapshot.bShipInTransit = !Snapshot.bCreating && Selected && Selected->Lifecycle==EHansaRouteLifecycleState::Traveling && Selected->Stops.IsValidIndex(Selected->CurrentStopIndex) && Selected->Stops.IsValidIndex(Selected->NextStopIndex);
    if(Snapshot.bShipInTransit) {
		auto Position=[&](const FHansaCityDefinitionId Id){const auto* City=AllCities.FindByPredicate([&](const auto& V){return V.StableId==FName(*Id.ToString());});return City?City->NormalizedPosition:FVector2D::ZeroVector;};
		const auto A=Position(Selected->Stops[Selected->CurrentStopIndex].CityId),B=Position(Selected->Stops[Selected->NextStopIndex].CityId);
        const auto Mid=(A+B)*.5;
        const double T=1.0-double(Selected->RemainingTravelTicks)/FMath::Max(1,Selected->TotalTravelTicks);
        Snapshot.ShipPosition=A*(1-T)*(1-T)+Mid*(2*T*(1-T))+B*T*T;
    }
	RebuildStops(); PublishIfChanged(Previous); return true;
}
bool UHansaTradeMapPresentationModel::PresenceUpgradeActionIntent()
{
	if((!Runtime.IsValid()&&!NetworkCommandIntent)||!Snapshot.bCanPresenceUpgradeAction||PresenceUpgradeStageId.IsEmpty())return false;const auto Previous=Snapshot;const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());if(!City)return false;bool Sent=false;
	if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=bPresenceUpgradeFunding?EHansaClientIntentType::FundPresenceUpgrade:EHansaClientIntentType::RequestPresenceUpgrade;I.CityId=City.Value.ToString();I.PresenceStageId=PresenceUpgradeStageId;I.FundingInventoryId=static_cast<int64>(PresenceFundingInventoryId.GetValue());Sent=NetworkCommandIntent(I);}
	else if(bPresenceUpgradeFunding)Sent=Runtime->FundPresenceUpgrade({City.Value,PresenceUpgradeStageId,PresenceFundingInventoryId}).IsSuccess();else Sent=Runtime->RequestPresenceUpgrade({City.Value,PresenceUpgradeStageId}).IsSuccess();
	Snapshot.EditorStatus=Sent?LOCTEXT("PresenceIntentSent","Presence review updated from authoritative state."):LOCTEXT("PresenceIntentFailed","Presence upgrade was rejected. Review unmet progress, funds, materials, status, and city policy.");PublishIfChanged(Previous);return Sent;
}

bool UHansaTradeMapPresentationModel::CanPresenceSpecializationIntent(const FString& Action) const
{
	if(Action==TEXT("Warehouse")||Action==TEXT("Market")||Action==TEXT("Harbor"))return PresenceSpecializations.ContainsByPredicate([&](const auto& V){return V.SpecializationId==Action;});
	return Action==TEXT("Apply")&&Snapshot.bCanPresenceSpecializationAction&&PresenceFundingInventoryId.IsValid();
}

bool UHansaTradeMapPresentationModel::PresenceSpecializationIntent(const FString& Action)
{
	if(!CanPresenceSpecializationIntent(Action))return false;const auto Previous=Snapshot;
	if(Action!=TEXT("Apply"))
	{
		Snapshot.SelectedPresenceSpecializationId=Action;const auto* Selected=PresenceSpecializations.FindByPredicate([&](const auto& V){return V.SpecializationId==Action;});Snapshot.bCanPresenceSpecializationAction=Selected&&Selected->bAvailable&&!Selected->bSelected;
		FString Compare=TEXT("Choose one exclusive Merchant Office specialization. Goods are non-refundable; respec returns 25% of the prior money cost.\n");for(const auto& O:PresenceSpecializations)Compare+=FString::Printf(TEXT("%s%s — %lld pfennig; storage +%lld; order slots +%d; station handling +%lld%s%s\n"),O.SpecializationId==Action?TEXT("▶ "):TEXT(""),*O.DisplayName,O.InvestmentCostPfennig,O.StorageCapacityBonusMilliUnits,O.AdditionalOrderSlots,O.StationTransferCapBonusMilliUnits,O.bSelected?TEXT(" — selected"):TEXT(""),O.Blocker.IsEmpty()?TEXT(""):*FString(TEXT(" — ")+O.Blocker));Snapshot.PresenceSpecializationComparison=FText::FromString(Compare);PublishIfChanged(Previous);return true;
	}
	const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());if(!City||(!Runtime.IsValid()&&!NetworkCommandIntent))return false;const bool bRespec=PresenceSpecializations.ContainsByPredicate([](const auto& V){return V.bSelected;});bool Sent=false;
	FHansaApplyPresenceSpecializationCommand P{City.Value,Snapshot.SelectedPresenceSpecializationId,PresenceFundingInventoryId,bRespec?EHansaPresenceSpecializationAction::Respec:EHansaPresenceSpecializationAction::Select,PresenceSpecializationRevision};
	if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::ApplyPresenceSpecialization;I.CityId=City.Value.ToString();I.FundingInventoryId=static_cast<int64>(PresenceFundingInventoryId.GetValue());I.PresenceSpecializationId=P.SpecializationId;I.PresenceSpecializationAction=static_cast<uint8>(P.Action);I.PresenceSpecializationRevision=P.ReviewedRevision;Sent=NetworkCommandIntent(I);}else Sent=Runtime->ApplyPresenceSpecialization(P).IsSuccess();
	Snapshot.PresenceSpecializationFeedback=Sent?LOCTEXT("SpecializationAccepted","Specialization transaction accepted; authoritative effects are active."):LOCTEXT("SpecializationRejected","Specialization rejected. Refresh the review and check exclusivity, funds, station materials, capacity, and merchant-office status.");
	if(Sent&&Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}PublishIfChanged(Previous);return Sent;
}
bool UHansaTradeMapPresentationModel::Open(const FName FocusOrigin, const FName PreferredGood, const FName SourceCity)
{
	const FHansaTradeMapSnapshot Previous=Snapshot; FocusOriginSemanticId=FocusOrigin; Snapshot.bOpen=true; if(!PreferredGood.IsNone())Snapshot.PreferredGoodStableId=PreferredGood;
    if (!PreferredGood.IsNone() && !Snapshot.bCreating) return BeginCreateIntent(PreferredGood, SourceCity);
    if (!Snapshot.bCreating && !Snapshot.bDirty && Runtime.IsValid()) { const auto P=Runtime->BuildProjection(); if(P){ const auto* R=P.Value.GetRoutes().FindByPredicate([this](const auto& X){return static_cast<int64>(X.Id.GetValue())==Snapshot.SelectedRouteValue;}); if(R){DraftStops=R->Stops;Snapshot.SelectedStopIndex=0;RebuildStops();}} }

	Snapshot.FocusedSemanticId=TEXT("TradeMap.Close"); PublishIfChanged(Previous); return Previous.bOpen != Snapshot.bOpen || Previous.PreferredGoodStableId != PreferredGood;
}
bool UHansaTradeMapPresentationModel::CloseIntent(){ if(!Snapshot.bOpen)return false; const auto Previous=Snapshot; Snapshot.bOpen=false; Snapshot.FocusedSemanticId=FocusOriginSemanticId; PublishIfChanged(Previous); FocusRestoreRequested.Broadcast(FocusOriginSemanticId); return true; }
void UHansaTradeMapPresentationModel::RebuildFilteredProjection()
{
	Snapshot.Cities.Reset();
	for(const auto& City:AllCities)
	{
		if(!Snapshot.CitySearchText.IsEmpty()&&!City.Label.ToString().Contains(Snapshot.CitySearchText,ESearchCase::IgnoreCase)&&!City.StableId.ToString().Contains(Snapshot.CitySearchText,ESearchCase::IgnoreCase))continue;
		if(Snapshot.CityFilter==EHansaTradeMapCityFilter::Presence&&!City.bHasPresence)continue;
		if(Snapshot.CityFilter==EHansaTradeMapCityFilter::Routes&&!City.bHasRoute)continue;
		if(Snapshot.Cities.Num()<48)Snapshot.Cities.Add(City);
	}
	Snapshot.MatchingCityCount=0;
	for(const auto& City:AllCities)
	{
		const bool bSearch=Snapshot.CitySearchText.IsEmpty()||City.Label.ToString().Contains(Snapshot.CitySearchText,ESearchCase::IgnoreCase)||City.StableId.ToString().Contains(Snapshot.CitySearchText,ESearchCase::IgnoreCase);
		const bool bMode=Snapshot.CityFilter==EHansaTradeMapCityFilter::All||(Snapshot.CityFilter==EHansaTradeMapCityFilter::Presence&&City.bHasPresence)||(Snapshot.CityFilter==EHansaTradeMapCityFilter::Routes&&City.bHasRoute);
		Snapshot.MatchingCityCount+=bSearch&&bMode?1:0;
	}
	TArray<FHansaTradeMapRoutePresentation> Matching;
	for(const auto& Route:AllRoutes){if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Sea&&!Route.bSea)continue;if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Land&&Route.bSea)continue;Matching.Add(Route);}
	Snapshot.MatchingRouteCount=Matching.Num();
	Snapshot.RouteWindowStart=FMath::Clamp(Snapshot.RouteWindowStart,0,FMath::Max(0,Matching.Num()-1));
	Snapshot.Routes.Reset();for(int32 Index=Snapshot.RouteWindowStart;Index<Matching.Num()&&Snapshot.Routes.Num()<20;++Index)Snapshot.Routes.Add(Matching[Index]);
	if(!Snapshot.Routes.ContainsByPredicate([this](const auto& Route){return Route.RouteValue==Snapshot.SelectedRouteValue;})&&!Snapshot.Routes.IsEmpty())Snapshot.SelectedRouteValue=Snapshot.Routes[0].RouteValue;
}
bool UHansaTradeMapPresentationModel::CycleModeFilterIntent(){if(Snapshot.bCreating)return false;const auto Previous=Snapshot;Snapshot.ModeFilter=static_cast<EHansaTradeMapModeFilter>((static_cast<uint8>(Snapshot.ModeFilter)+1)%3);Snapshot.RouteWindowStart=0;RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CycleCityFilterIntent(){if(Snapshot.bCreating)return false;const auto Previous=Snapshot;Snapshot.CityFilter=static_cast<EHansaTradeMapCityFilter>((static_cast<uint8>(Snapshot.CityFilter)+1)%3);RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::SetCitySearchIntent(const FString& SearchText){if(Snapshot.bCreating)return false;const FString Normalized=SearchText.Left(64);if(Snapshot.CitySearchText==Normalized)return true;const auto Previous=Snapshot;Snapshot.CitySearchText=Normalized;RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CycleSelectedGoodIntent(){if(Snapshot.bCreating||AvailableGoods.IsEmpty())return false;const auto Previous=Snapshot;int32 Index=AvailableGoods.IndexOfByKey(Snapshot.PreferredGoodStableId);Snapshot.PreferredGoodStableId=AvailableGoods[(Index+1)%AvailableGoods.Num()];if(Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)return ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::MoveRouteWindowIntent(const int32 Direction){if(Snapshot.bCreating||Direction==0)return false;const auto Previous=Snapshot;Snapshot.RouteWindowStart=FMath::Clamp(Snapshot.RouteWindowStart+Direction*20,0,FMath::Max(0,Snapshot.MatchingRouteCount-1));RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::SelectRouteIntent(const int64 Value){ if(Snapshot.bCreating)return false; const auto* Found=Snapshot.Routes.FindByPredicate([Value](const auto& R){return R.RouteValue==Value;}); if(!Found)return false; const auto Previous=Snapshot; Snapshot.SelectedRouteValue=Value; Snapshot.SelectedStopIndex=0; Snapshot.bDirty=false; Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Route.%lld"),Value)); if(Runtime.IsValid()){const auto P=Runtime->BuildProjection(); if(P){const auto* R=P.Value.GetRoutes().FindByPredicate([Value](const auto& X){return static_cast<int64>(X.Id.GetValue())==Value;}); if(R)DraftStops=R->Stops;}} RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::SelectStopIntent(const int32 Index){ if(!DraftStops.IsValidIndex(Index))return false; const auto Previous=Snapshot; Snapshot.SelectedStopIndex=Index; Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Stop.%d"),Index)); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::CycleCargoActionIntent(){ if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.Kind=static_cast<EHansaRouteCargoActionKind>((static_cast<uint8>(A.Kind)+1)%6); Snapshot.bDirty=true;Snapshot.bReview=false; RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::AdjustQuantityIntent(const int32 Delta){ if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.QuantityLimit=FHansaQuantity::FromRaw(FMath::Max<int64>(1000,A.QuantityLimit.GetRawValue()+Delta)); Snapshot.bDirty=true;Snapshot.bReview=false; RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::AdjustMinimumReserveIntent(const int32 Delta){ if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.MinimumSourceReserve=FHansaQuantity::FromRaw(FMath::Max<int64>(0,A.MinimumSourceReserve.GetRawValue()+Delta)); Snapshot.bDirty=true;Snapshot.bReview=false; RebuildStops(); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::MoveStopIntent(const int32 Direction){const int32 To=Snapshot.SelectedStopIndex+Direction;if(!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||!DraftStops.IsValidIndex(To))return false;const auto Previous=Snapshot;DraftStops.Swap(Snapshot.SelectedStopIndex,To);Snapshot.SelectedStopIndex=To;Snapshot.bDirty=true;Snapshot.bReview=false;RebuildStops();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CommitIntent()
{
	if (Snapshot.bCreating) return ReviewCreateIntent();
	if (!Snapshot.bDirty || (!Runtime.IsValid() && !NetworkCommandIntent)) return false;
	const auto Id = FHansaRouteId::TryCreate(static_cast<uint64>(Snapshot.SelectedRouteValue)); if (!Id) return false;
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::EditRoute; Intent.RouteId = Snapshot.SelectedRouteValue;
		AppendClientRouteStops(DraftStops, Intent);
		const bool bSent = NetworkCommandIntent(Intent); const auto Previous = Snapshot;
		Snapshot.EditorStatus = bSent ? LOCTEXT("EditPending", "Route changes sent to the authoritative server.") : LOCTEXT("EditSendFailed", "Route changes could not be sent.");
		if (bSent) Snapshot.bDirty = false; PublishIfChanged(Previous); return bSent;
	}
	const auto Result=Runtime->EditRoute(Id.Value,DraftStops);const auto Previous=Snapshot;const FText Failure=Result.GetError()==EHansaCommandGatewayError::ResearchEffectRequired ? LOCTEXT("EditResearchRequired", "complete Reserve instructions research before saving a minimum reserve; your draft is preserved") : Result.GetError()==EHansaCommandGatewayError::RouteStateInvalid ? LOCTEXT("EditStoppedEmpty", "pause the route with an empty hold and move its current port to the first stop; your draft is preserved") : FText::FromString(Result.GetRoutePlanError()!=EHansaRoutePlanError::None ? LexToString(Result.GetRoutePlanError()) : LexToString(Result.GetError()));Snapshot.EditorStatus=Result?LOCTEXT("Saved","Route changes saved."):FText::Format(LOCTEXT("SaveFailed","Route could not be saved: {0}."),Failure);if(Result)Snapshot.bDirty=false;PublishIfChanged(Previous);return !!Result;
}
bool UHansaTradeMapPresentationModel::ToggleActiveIntent()
{
	if ((!Runtime.IsValid() && !NetworkCommandIntent) || Snapshot.bCreating) return false;
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
	if (bActivate && Snapshot.bDirty && !CommitIntent()) return false;
	const auto Previous = Snapshot;
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::SetRouteActive;
		Intent.RouteId = Snapshot.SelectedRouteValue; Intent.bActive = bActivate;
		const bool bSent = NetworkCommandIntent(Intent);
		Snapshot.EditorStatus = bSent ? LOCTEXT("RouteTogglePending", "Route state change sent to the authoritative server.") : LOCTEXT("RouteToggleSendFailed", "Route state change could not be sent.");
		PublishIfChanged(Previous); return bSent;
	}
	const FHansaCommandGatewayResult Result = Runtime->SetRouteActive(Id.Value, bActivate);
	Snapshot.EditorStatus = Result
		? (bActivate ? LOCTEXT("Started", "Route started. It will repeat until paused.")
			: LOCTEXT("Paused", "Route paused. Cargo and route progress are preserved."))
		: RouteToggleFailure(Result);
	PublishIfChanged(Previous);
	return !!Result;
}
bool UHansaTradeMapPresentationModel::TradeStationActionIntent()
{
	if((!Runtime.IsValid()&&!NetworkCommandIntent)||!Snapshot.bCanTradeStationAction)return false;const auto Previous=Snapshot;bool bSent=false;
	if(Snapshot.TradeStationValue==0)
	{
		if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::ProposeTradeStation;I.CityId=Snapshot.SelectedCityStableId.ToString();I.TradeStationSiteId=SelectedStationSiteId;bSent=NetworkCommandIntent(I);}
		else{FHansaTradeStationId Id;bSent=Runtime->ProposeTradeStation(FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString()).Value,SelectedStationSiteId,Id).IsSuccess();}
	}
	else
	{
		const auto Station=FHansaTradeStationId::TryCreate(static_cast<uint64>(Snapshot.TradeStationValue));if(!Station)return false;const auto Projection=Runtime.IsValid()?Runtime->BuildProjection():THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidFormat);
		const auto* Item=Projection?Projection.Value.GetTradeStations().FindByPredicate([&](const auto& V){return V.Station.Id==Station.Value;}):nullptr;
		if(Item&&(Item->Station.Status==EHansaTradeStationStatus::Proposed||Item->Station.OperationalState==EHansaTradeStationOperationalState::Underfunded))
		{
			FHansaInventoryId Funding;for(const auto& V:Projection.Value.GetVehicles())if(V.OwnerId==Runtime->GetHouseId()){Funding=V.CargoInventoryId;break;}
			if(!Funding.IsValid())return false;if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::FundTradeStation;I.TradeStationId=Snapshot.TradeStationValue;I.FundingInventoryId=static_cast<int64>(Funding.GetValue());bSent=NetworkCommandIntent(I);}else bSent=Runtime->FundTradeStation(Station.Value,Funding).IsSuccess();
		}
		else if(Item){if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::CloseTradeStation;I.TradeStationId=Snapshot.TradeStationValue;bSent=NetworkCommandIntent(I);}else bSent=Runtime->CloseTradeStation(Station.Value).IsSuccess();}
	}
	Snapshot.EditorStatus=bSent?LOCTEXT("StationIntentSent","Trade-station state changed; the inspector will refresh from authoritative state."):LOCTEXT("StationIntentFailed","Trade-station action was rejected. Review its blocker and required remedy.");PublishIfChanged(Previous);return bSent;
}
bool UHansaTradeMapPresentationModel::SelectCityIntent(const FName CityId)
{
    if(Snapshot.bCreating||!AllCities.ContainsByPredicate([&](const auto& City){return City.StableId==CityId;})||!LastProjection||!LastRegistry)return false;
    const auto Previous=Snapshot;
    if(CityId==Snapshot.SelectedCityStableId)return true;
    Snapshot.SelectedCityStableId=CityId;SelectedStationOrder=0;OrderDraft={};Snapshot.StationOrderFeedback=FText();Snapshot.PresenceSpecializationFeedback=FText();Snapshot.SelectedPresenceSpecializationId.Reset();
    const auto Projection=LastProjection;ApplyProjection(*Projection,*LastRegistry);PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::CycleCityIntent(const int32 Direction)
{
    if(Snapshot.Cities.IsEmpty()||Snapshot.bCreating)return false;
    const int32 Index=Snapshot.Cities.IndexOfByPredicate([&](const auto& City){return City.StableId==Snapshot.SelectedCityStableId;});
    return SelectCityIntent(Snapshot.Cities[(FMath::Max(0,Index)+Direction+Snapshot.Cities.Num())%Snapshot.Cities.Num()].StableId);
}
void UHansaTradeMapPresentationModel::SetCompact(const bool Value){if(Snapshot.bCompact==Value)return;const auto Previous=Snapshot;Snapshot.bCompact=Value;PublishIfChanged(Previous);}
void UHansaTradeMapPresentationModel::SetFocusedSemanticId(const FName Id){if(Snapshot.FocusedSemanticId==Id)return;const auto Previous=Snapshot;Snapshot.FocusedSemanticId=Id;PublishIfChanged(Previous);}
const FHansaTradeMapRoutePresentation* UHansaTradeMapPresentationModel::FindSelectedRoute()const{return Snapshot.Routes.FindByPredicate([this](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});}
bool UHansaTradeMapPresentationModel::CancelRouteIntent()
{
    if ((!Runtime.IsValid() && !NetworkCommandIntent) || Snapshot.bCreating) return false;
    const auto* Route = FindSelectedRoute();
    if (!Route || !Route->bCanCancel) return false;
    const auto Previous = Snapshot;
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::CancelRoute; Intent.RouteId = Snapshot.SelectedRouteValue;
		const bool bSent = NetworkCommandIntent(Intent);
		Snapshot.EditorStatus = bSent ? LOCTEXT("RouteCancelPending", "Route cancellation sent to the authoritative server.") : LOCTEXT("RouteCancelSendFailed", "Route cancellation could not be sent.");
		PublishIfChanged(Previous); return bSent;
	}
    const auto Result = Runtime->CancelRoute(FHansaRouteId::TryCreate(Snapshot.SelectedRouteValue).Value);
    if (Result)
    {
        Snapshot.bDirty = false;
        const auto P = Runtime->BuildProjection();
        if (P) ApplyProjection(P.Value, *Runtime->GetEconomicRegistry());
    }
    Snapshot.EditorStatus = Result ? LOCTEXT("CancelledByPlayer", "Route cancelled. The empty Cog remains at port and can be assigned to a new voyage.") : RouteToggleFailure(Result);
    PublishIfChanged(Previous);
    return !!Result;
}

void UHansaTradeMapPresentationModel::RebuildStops()
{
    Snapshot.Stops.Reset();
    bool bRisk = false, bUncertain = false;
    for (int32 I = 0; I < DraftStops.Num(); ++I)
    {
        const auto& S = DraftStops[I];
        FHansaTradeMapStopPresentation P;
        P.Index = I; P.CityStableId = FName(*S.CityId.ToString()); P.CityLabel = CityLabel(S.CityId.ToString());
        if (!S.Actions.IsEmpty())
        {
            const auto& A = S.Actions[0];
            P.GoodStableId = FName(*A.GoodId.ToString());
            switch (A.Kind)
            {
            case EHansaRouteCargoActionKind::StationLoad: P.ActionLabel = LOCTEXT("StationLoad", "Load from station"); break;
            case EHansaRouteCargoActionKind::StationUnload: P.ActionLabel = LOCTEXT("StationUnload", "Unload to station"); break;
            case EHansaRouteCargoActionKind::OwnedCityLoad: P.ActionLabel = LOCTEXT("HomeLoad", "Load home stock"); break;
            case EHansaRouteCargoActionKind::OwnedCityUnload: P.ActionLabel = LOCTEXT("HomeUnload", "Unload to home"); break;
            default: P.ActionLabel = S.CityId.ToString() == TEXT("City.Lubeck")
                ? (IsRouteLoad(A.Kind) ? LOCTEXT("Load", "Load") : LOCTEXT("Unload", "Unload"))
                : (IsRouteLoad(A.Kind) ? LOCTEXT("LegacyBuy", "Buy at market") : LOCTEXT("LegacySell", "Sell at market")); break;
            }
            P.Quantity = FText::Format(LOCTEXT("NamedQuantity", "{0} {1}"), FText::AsNumber(double(A.QuantityLimit.GetRawValue())/1000.), FText::FromString(A.GoodId.ToString().RightChop(5)));
            P.MinimumReserve = FText::Format(LOCTEXT("Reserve", "{0} minimum reserve"), FText::AsNumber(double(A.MinimumSourceReserve.GetRawValue())/1000.));
            if (IsRouteLoad(A.Kind) && !IsStationTransfer(A.Kind))
            {
                const auto Supply = Runtime.IsValid() ? Runtime->QueryKnownMarketSupply(S.CityId, A.GoodId) : TOptional<FHansaKnownMarketSupplyDemandProjection>();
                bUncertain |= !Supply.IsSet() || !Supply->Stock.IsSet() || Supply->InformationState != EHansaMarketInformationState::Current;
                P.bReserveRisk = Supply.IsSet() && Supply->Stock.IsSet() && Supply->Stock->GetRawValue() - A.MinimumSourceReserve.GetRawValue() < A.QuantityLimit.GetRawValue();
                bRisk |= P.bReserveRisk;
            }
            P.AccessibleLabel = FText::Format(LOCTEXT("StopAccessible", "Stop {0}, {1}, {2} {3}, {4}.{5}"), FText::AsNumber(I+1), P.CityLabel, P.ActionLabel, P.Quantity, P.MinimumReserve, P.bReserveRisk ? LOCTEXT("StopRisk", " Partial load risk.") : FText());
        }
        Snapshot.Stops.Add(MoveTemp(P));
    }
    Snapshot.ReserveRisk = bRisk ? LOCTEXT("Risk", "Planned load exceeds reported stock above reserve. Loading may be partial; the reserve stays protected.")
        : bUncertain ? LOCTEXT("UnknownReserve", "? Stock reports are missing or older. Delivery quantity is uncertain; minimum reserves remain protected.")
        : LOCTEXT("Safe", "Reported stock covers the planned load and protected reserve.");
    for (const auto& Stop : DraftStops) for (const auto& Action : Stop.Actions) if (IsStationTransfer(Action.Kind))
        Snapshot.ReserveRisk = LOCTEXT("StationReserveReview", "Station cargo is limited by active access, unreserved stock, protected reserves and free storage. Review prepared cargo before departure; partial transfers remain possible.");
    if (Snapshot.bDirty) Snapshot.EditorStatus = LOCTEXT("Unsaved", "Unsaved route changes.");
    UpdateCreatorReview();
}
void UHansaTradeMapPresentationModel::PublishIfChanged(const FHansaTradeMapSnapshot& Previous){if(Snapshot==Previous)return;++Revision;Changed.Broadcast(Snapshot,Revision);}

void UHansaTradeMapPresentationModel::RefreshStationOrderText()
{
    FText Report=LOCTEXT("OrderUnknownReport","Market report unavailable. Prices are sampled at execution.");
    if(Runtime.IsValid()&&OrderDraft.GoodId.IsValid()) {
        const auto Known=Runtime->QueryKnownMarketPrice(FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString()).Value,OrderDraft.GoodId);
        if(Known && Known->PriceMilliMarks.IsSet() && Known->ReportAgeTicks.IsSet()) Report=FText::Format(LOCTEXT("OrderMarketReport","Reported price {0} milli-marks · {1} ticks old ({2}). Prices are sampled at execution."),FText::AsNumber(Known->PriceMilliMarks.GetValue()),FText::AsNumber(Known->ReportAgeTicks.GetValue()),FText::FromString(LexToString(Known->InformationState)));
    }
    const auto* Selected=StationOrders.FindByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
    FText History=LOCTEXT("OrderNoHistory","No execution yet.");
    if(Selected) {
        FString Lines;
        for(const auto& E:Selected->History) Lines+=FString::Printf(TEXT("Tick %lld: %s · %lld milli-units · %+lld pfennig · %s\n"),E.Tick,LexToString(E.Outcome),E.AppliedMilliUnits,E.MoneyDelta,LexToString(E.Blocker));
        History=FText::Format(LOCTEXT("OrderHistory","Spent {0} pfennig · next update tick {1} · {2}\n{3}"),FText::AsNumber(Selected->SpentPfennig),FText::AsNumber(Selected->NextUpdateTick),Selected->bCancelled?LOCTEXT("OrderCancelled","Cancelled"):Selected->bPaused?LOCTEXT("OrderPaused","Paused"):LOCTEXT("OrderRunning","Enabled"),FText::FromString(Lines));
    }
    Snapshot.StationOrderText=FText::Format(LOCTEXT("OrderDraft","Orders · {0}\n{1} · {2}\nTarget / reserve: {3} units · cap: {4} units/update\nTotal purchase budget: {5} pfennig\n{6}\n{7}"),Selected?FText::AsNumber(SelectedStationOrder):LOCTEXT("OrderNew","New order"),FText::FromString(OrderDraft.GoodId.ToString()),OrderDraft.Side==EHansaStationOrderSide::Acquire?LOCTEXT("OrderAcquire","Acquire to target"):LOCTEXT("OrderRelease","Sell down to reserve"),FText::AsNumber(double(OrderDraft.TargetOrReserveMilliUnits)/1000),FText::AsNumber(double(OrderDraft.CapMilliUnits)/1000),FText::AsNumber(OrderDraft.TotalBudgetPfennig),Report,History);
}
bool UHansaTradeMapPresentationModel::CanStationOrderAction(const FString& Action) const
{
    if(!Snapshot.bOpen||Snapshot.bCreating||Snapshot.TradeStationValue<=0)return false;
    const auto* Selected=StationOrders.FindByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
    if(Action==TEXT("Select"))return !StationOrders.IsEmpty();
    if(Action==TEXT("Pause")||Action==TEXT("Cancel"))return Selected&&!Selected->bCancelled;
    if(Selected&&Selected->bCancelled)return false;
    if(Action==TEXT("Good")||Action==TEXT("Side"))return !Selected&&!OrderGoods.IsEmpty();
    if(Action==TEXT("Target.Decrease"))return OrderDraft.TargetOrReserveMilliUnits>0;
    if(Action==TEXT("Target.Increase"))return OrderDraft.TargetOrReserveMilliUnits<StationOrderCapacity;
    if(Action==TEXT("Cap.Decrease"))return OrderDraft.CapMilliUnits>1;
    if(Action==TEXT("Cap.Increase"))return OrderDraft.CapMilliUnits<StationOrderMaxCap;
    if(Action.StartsWith(TEXT("Budget"))){if(OrderDraft.Side==EHansaStationOrderSide::Release)return false;return Action.EndsWith(TEXT("Increase"))?OrderDraft.TotalBudgetPfennig<StationOrderMaxBudget:OrderDraft.TotalBudgetPfennig>0;}
    return OrderDraft.GoodId.IsValid();
}
bool UHansaTradeMapPresentationModel::StationOrderIntent(const FString& Action)
{
    if(!CanStationOrderAction(Action))return false;
    const auto Previous=Snapshot;
    if(Action==TEXT("Select")) {
        int32 Index=StationOrders.IndexOfByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
        ++Index; SelectedStationOrder=StationOrders.IsValidIndex(Index)?StationOrders[Index].Id:0;
        if(SelectedStationOrder)OrderDraft=StationOrders[Index].Terms;
    } else if(Action==TEXT("Good")) {
        if(SelectedStationOrder||OrderGoods.IsEmpty())return false;
        OrderDraft.GoodId=OrderGoods[(OrderGoods.IndexOfByKey(OrderDraft.GoodId)+1)%OrderGoods.Num()];
    } else if(Action==TEXT("Side")) {
        if(SelectedStationOrder)return false;
        OrderDraft.Side=OrderDraft.Side==EHansaStationOrderSide::Acquire?EHansaStationOrderSide::Release:EHansaStationOrderSide::Acquire;
    } else if(Action.StartsWith(TEXT("Target"))) OrderDraft.TargetOrReserveMilliUnits=FMath::Clamp<int64>(OrderDraft.TargetOrReserveMilliUnits+(Action.EndsWith(TEXT("Increase"))?1000:-1000),0,StationOrderCapacity);
    else if(Action.StartsWith(TEXT("Cap"))) OrderDraft.CapMilliUnits=FMath::Clamp<int64>(OrderDraft.CapMilliUnits+(Action.EndsWith(TEXT("Increase"))?1000:-1000),1,StationOrderMaxCap);
    else if(Action.StartsWith(TEXT("Budget"))) OrderDraft.TotalBudgetPfennig=FMath::Clamp<int64>(OrderDraft.TotalBudgetPfennig+(Action.EndsWith(TEXT("Increase"))?1000:-1000),0,StationOrderMaxBudget);
    else {
        FHansaManageStationOrderCommand P;const auto Id=FHansaTradeStationId::TryCreate(static_cast<uint64>(Snapshot.TradeStationValue));if(!Id)return false;P.StationId=Id.Value;P.Terms=OrderDraft;P.OrderId=SelectedStationOrder;
        const auto* Selected=StationOrders.FindByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
        if(Action==TEXT("Save")) {
            P.Action=Selected?EHansaStationOrderAction::Edit:EHansaStationOrderAction::Create;
            if(!Selected){P.OrderId=1;for(const auto& O:StationOrders){if(O.Id>=MAX_int64)return false;P.OrderId=FMath::Max(P.OrderId,O.Id+1);}}
        } else if(Action==TEXT("Pause")&&Selected) P.Action=Selected->bPaused?EHansaStationOrderAction::Resume:EHansaStationOrderAction::Pause;
        else if(Action==TEXT("Cancel")&&Selected)P.Action=EHansaStationOrderAction::Cancel;
        else return false;
        bool Sent=false;
        if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::ManageStationOrder;I.TradeStationId=Snapshot.TradeStationValue;I.StationOrderId=P.OrderId;I.StationOrderAction=static_cast<uint8>(P.Action);I.GoodId=P.Terms.GoodId.ToString();I.StationOrderSide=static_cast<uint8>(P.Terms.Side);I.StationOrderTarget=P.Terms.TargetOrReserveMilliUnits;I.StationOrderCap=P.Terms.CapMilliUnits;I.StationOrderBudget=P.Terms.TotalBudgetPfennig;Sent=NetworkCommandIntent(I);}
        else if(Runtime.IsValid()) Sent=Runtime->ManageStationOrder(P).IsSuccess();
        Snapshot.StationOrderFeedback=Sent?LOCTEXT("OrderAccepted","Order submitted through the command gateway."):LOCTEXT("OrderRejected","Order rejected. Check station rights, report availability, policy limits and remaining budget; cancelled orders cannot be edited. Keep the same good and direction when editing.");
        if(Sent){SelectedStationOrder=P.OrderId;if(Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}}
        RefreshStationOrderText();PublishIfChanged(Previous);return Sent;
    }
    RefreshStationOrderText();PublishIfChanged(Previous);return true;
}

#undef LOCTEXT_NAMESPACE

bool UHansaTradeMapPresentationModel::VisitSelectedStopIntent()
{
    if(!Snapshot.bOpen||Snapshot.bCreating||!Snapshot.Stops.IsValidIndex(Snapshot.SelectedStopIndex)||!VisitRequested)return false;
    return VisitRequested(Snapshot.Stops[Snapshot.SelectedStopIndex].CityStableId);
}
