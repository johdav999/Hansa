#include "UI/HansaTradeMapPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Trade/HansaCargoPlan.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UI/HansaTradeMapGeometry.h"

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
                ClientAction.CargoSlotIndex = Action.CargoSlotIndex;
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
			return Hansa::UI::TradeGeometry::Project(City.MapLongitudeMilliDegrees/1000.0, City.MapLatitudeMilliDegrees/1000.0);
		}
		if(const auto* Known=TradeCityLocations.Find(City.StableId))return *Known;
		return {-1,-1}; // Never invent a geographic location from a stable-ID hash.
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

bool operator==(const FHansaTradeMapCityPresentation& A,const FHansaTradeMapCityPresentation& B){return A.ReportedPriceMilliMarks==B.ReportedPriceMilliMarks&&A.ReportedStockMilliUnits==B.ReportedStockMilliUnits&&A.PresenceReport.EqualTo(B.PresenceReport)&&A.MapAlert.EqualTo(B.MapAlert)&&A.StableId==B.StableId&&A.Label.EqualTo(B.Label)&&A.NormalizedPosition==B.NormalizedPosition&&A.GoodReport.EqualTo(B.GoodReport)&&A.Information.EqualTo(B.Information)&&A.CapabilitySummary.EqualTo(B.CapabilitySummary)&&A.bOwned==B.bOwned&&A.bStale==B.bStale&&A.bUnknown==B.bUnknown&&A.bRendered==B.bRendered&&A.bVisitable==B.bVisitable&&A.bBuildable==B.bBuildable&&A.bMarketOnly==B.bMarketOnly&&A.bHasPresence==B.bHasPresence&&A.bPresenceAttention==B.bPresenceAttention&&A.bHasRoute==B.bHasRoute&&A.ReportAgeTicks==B.ReportAgeTicks;}
bool operator==(const FHansaTradeMapStopPresentation& A,const FHansaTradeMapStopPresentation& B){return A.Index==B.Index&&A.CityStableId==B.CityStableId&&A.CityLabel.EqualTo(B.CityLabel)&&A.GoodStableId==B.GoodStableId&&A.ActionLabel.EqualTo(B.ActionLabel)&&A.Quantity.EqualTo(B.Quantity)&&A.MinimumReserve.EqualTo(B.MinimumReserve)&&A.AccessibleLabel.EqualTo(B.AccessibleLabel)&&A.bReserveRisk==B.bReserveRisk;}
bool operator==(const FHansaTradeMapRoutePresentation& A,const FHansaTradeMapRoutePresentation& B){return A.bCancelled==B.bCancelled&&A.CityIds==B.CityIds&&A.CapacityMilliUnits==B.CapacityMilliUnits&&A.RouteValue==B.RouteValue&&A.VehicleValue==B.VehicleValue&&A.DefinitionStableId==B.DefinitionStableId&&A.Label.EqualTo(B.Label)&&A.Mode.EqualTo(B.Mode)&&A.State.EqualTo(B.State)&&A.Ownership.EqualTo(B.Ownership)&&A.StateHeading.EqualTo(B.StateHeading)&&A.StateDetail.EqualTo(B.StateDetail)&&A.ToggleActionLabel.EqualTo(B.ToggleActionLabel)&&A.ToggleActionHint.EqualTo(B.ToggleActionHint)&&A.StopSummary.EqualTo(B.StopSummary)&&A.Capacity.EqualTo(B.Capacity)&&A.Upkeep.EqualTo(B.Upkeep)&&A.RoundTripTime.EqualTo(B.RoundTripTime)&&A.ExpectedProfitRange.EqualTo(B.ExpectedProfitRange)&&A.Uncertainty.EqualTo(B.Uncertainty)&&A.bSea==B.bSea&&A.bActive==B.bActive&&A.bOwnedByPlayer==B.bOwnedByPlayer&&A.bCanCancel==B.bCanCancel&&A.bCanToggleActive==B.bCanToggleActive&&A.bTraveling==B.bTraveling&&A.bReserveRisk==B.bReserveRisk&&A.bProfitKnown==B.bProfitKnown;}
bool operator==(const FHansaTradeMapSnapshot& A,const FHansaTradeMapSnapshot& B){return A.StationOrderEditorKey==B.StationOrderEditorKey&&A.bAnyCommandPending==B.bAnyCommandPending&&A.RecoveryKey==B.RecoveryKey&&A.RecoveryItem==B.RecoveryItem&&A.RecoveryFeedback==B.RecoveryFeedback&&A.bRecoveryReview==B.bRecoveryReview&&A.bRecoveryPending==B.bRecoveryPending&&A.DecisionsKey==B.DecisionsKey&&A.DecisionId==B.DecisionId&&A.DecisionSource==B.DecisionSource&&A.DecisionFeedback==B.DecisionFeedback&&A.bDecisionReview==B.bDecisionReview&&A.bDecisionPending==B.bDecisionPending&&A.ConstructionKey==B.ConstructionKey&&A.SpecializationKey==B.SpecializationKey&&A.SpecializationSourceId==B.SpecializationSourceId&&A.bSpecializationReview==B.bSpecializationReview&&A.bSpecializationPending==B.bSpecializationPending&&A.SpecializationReview.EqualTo(B.SpecializationReview)&&A.LedgerKey==B.LedgerKey&&A.Establishment==B.Establishment&&A.CityInspector==B.CityInspector&&A.ScheduleKey==B.ScheduleKey&&A.bDiscardConfirmation==B.bDiscardConfirmation&&A.bCommandPending==B.bCommandPending&&A.ValidationTarget==B.ValidationTarget&&A.Directory==B.Directory&&A.bFleetView==B.bFleetView&&A.DirectoryFilter==B.DirectoryFilter&&A.SelectedVehicleValue==B.SelectedVehicleValue&&A.WorkspacePage==B.WorkspacePage&&A.bHasProjection==B.bHasProjection&&A.ActiveSection==B.ActiveSection&&A.SelectedCityStableId==B.SelectedCityStableId&&A.PresenceSpecializationComparison.EqualTo(B.PresenceSpecializationComparison)&&A.PresenceSpecializationAction.EqualTo(B.PresenceSpecializationAction)&&A.PresenceSpecializationFeedback.EqualTo(B.PresenceSpecializationFeedback)&&A.SelectedPresenceSpecializationId==B.SelectedPresenceSpecializationId&&A.bCanPresenceSpecializationAction==B.bCanPresenceSpecializationAction&&A.PresenceProgress.EqualTo(B.PresenceProgress)&&A.PresenceRequirementsKey==B.PresenceRequirementsKey&&A.PresenceStageSummary.EqualTo(B.PresenceStageSummary)&&A.PresenceConsequences.EqualTo(B.PresenceConsequences)&&A.PresenceFundingDetail.EqualTo(B.PresenceFundingDetail)&&A.PresenceHistory.EqualTo(B.PresenceHistory)&&A.PresenceReview.EqualTo(B.PresenceReview)&&A.PresenceStatus.EqualTo(B.PresenceStatus)&&A.PresenceSourceId==B.PresenceSourceId&&A.bPresenceReview==B.bPresenceReview&&A.bPresenceOfficeVisual==B.bPresenceOfficeVisual&&A.PresenceUpgradeAction.EqualTo(B.PresenceUpgradeAction)&&A.bCanPresenceUpgradeAction==B.bCanPresenceUpgradeAction&&A.StationOrderText.EqualTo(B.StationOrderText)&&A.StationOrderList.EqualTo(B.StationOrderList)&&A.StationOrderListCompact.EqualTo(B.StationOrderListCompact)&&A.bConfirmStationOrderCancel==B.bConfirmStationOrderCancel&&A.SelectedStationOrderId==B.SelectedStationOrderId&&A.StationOrderTargetInput==B.StationOrderTargetInput&&A.StationOrderCapInput==B.StationOrderCapInput&&A.StationOrderBudgetPfennig==B.StationOrderBudgetPfennig&&A.StationOrderFeedback.EqualTo(B.StationOrderFeedback)&&A.bShipInTransit==B.bShipInTransit&&A.ShipPosition==B.ShipPosition&&A.bCreating==B.bCreating&&A.bReview==B.bReview&&A.bCanCreate==B.bCanCreate&&A.bReassignCog==B.bReassignCog&&A.CogValue==B.CogValue&&A.DraftName==B.DraftName&&A.CogLabel.EqualTo(B.CogLabel)&&A.CreatorReview.EqualTo(B.CreatorReview)&&A.Validation.EqualTo(B.Validation)&&A.Cities==B.Cities&&A.Routes==B.Routes&&A.Stops==B.Stops&&A.FocusedSemanticId==B.FocusedSemanticId&&A.PreferredGoodStableId==B.PreferredGoodStableId&&A.Title.EqualTo(B.Title)&&A.EditorStatus.EqualTo(B.EditorStatus)&&A.ReserveRisk.EqualTo(B.ReserveRisk)&&A.SelectedRouteValue==B.SelectedRouteValue&&A.SelectedStopIndex==B.SelectedStopIndex&&A.ModeFilter==B.ModeFilter&&A.CityFilter==B.CityFilter&&A.CitySearchText==B.CitySearchText&&A.MatchingCityCount==B.MatchingCityCount&&A.MatchingRouteCount==B.MatchingRouteCount&&A.RouteWindowStart==B.RouteWindowStart&&A.bOpen==B.bOpen&&A.bCompact==B.bCompact&&A.bDirty==B.bDirty&&A.TradeStationValue==B.TradeStationValue&&A.TradeStationState.EqualTo(B.TradeStationState)&&A.TradeStationDetail.EqualTo(B.TradeStationDetail)&&A.TradeStationAction.EqualTo(B.TradeStationAction)&&A.bCanTradeStationAction==B.bCanTradeStationAction;}

void UHansaTradeMapPresentationModel::InitializeDefaults()
{
    bWorldStationDetail=false;
	Recoveries.Reset();RecoveryStation=RecoverySequence=RecoveryNonce=0;RecoveryReviewedKey.Reset();ProjectedCityId=NAME_None;ProjectedViewerId=0;bRemoteEstablishment=false;RemoteEstablishments.Reset();RemoteLedgers.Reset();SelectedStationSiteId.Reset();SelectedFundingSource.Reset();EstablishmentReviewKey.Reset();EstablishmentFeedback=FText();bEstablishmentPending=false;EstablishmentSequence=EstablishmentNonce=0;Snapshot = {}; Snapshot.Title = LOCTEXT("Title", "Baltic trade"); Snapshot.EditorStatus = LOCTEXT("Empty", "Select a route to edit its stops and cargo rules.");
}

void UHansaTradeMapPresentationModel::BindRuntime(UHansaRuntimeSimulationHost* RuntimeHost) { Runtime = RuntimeHost; }

bool UHansaTradeMapPresentationModel::ApplyProjection(const FHansaSimulationProjection& Projection, const FHansaEconomicRegistry& Registry, bool bDirectoryOnly)
{
	const FHansaTradeMapSnapshot Previous = Snapshot;
    Snapshot.bHasProjection=true;
    const uint64 ViewerId=(Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse).GetValue();
    if(ProjectedCityId!=Snapshot.SelectedCityStableId||ProjectedViewerId!=ViewerId){
        if(ProjectedViewerId!=ViewerId){Recoveries.Reset();RecoveryStation=RecoverySequence=RecoveryNonce=0;RecoveryReviewedKey.Reset();Snapshot.bRecoveryPending=false;Snapshot.RecoveryFeedback.Reset();Snapshot.DecisionId.Reset();Snapshot.DecisionSource.Reset();Snapshot.DecisionFeedback.Reset();DecisionReviewedKey.Reset();Snapshot.bDecisionPending=false;DecisionSequence=DecisionNonce=0;}
        SelectedConstructionLease=0;SelectedConstructionBuilding=NAME_None;ConstructionReturnFocus=NAME_None;ProjectedCityId=Snapshot.SelectedCityStableId;ProjectedViewerId=ViewerId;SelectedStationSiteId.Reset();SelectedFundingSource.Reset();EstablishmentReviewKey.Reset();EstablishmentFeedback=FText();SelectedPresenceSource.Reset();PresenceReviewKey.Reset();PresenceFeedback=FText();SelectedStationOrder=0;OrderDraft={};Snapshot.bConfirmStationOrderCancel=false;
        Snapshot.StationOrderFeedback=FText();Snapshot.PresenceSpecializationFeedback=FText();Snapshot.SelectedPresenceSpecializationId.Reset();Snapshot.SpecializationSourceId.Reset();SpecializationReviewedKey.Reset();
    }
	LastProjection=MakeShared<FHansaSimulationProjection>(Projection);LastRegistry=&Registry;
	Snapshot.Cities.Reset(); Snapshot.Routes.Reset(); AllCities.Reset(); AllRoutes.Reset(); AvailableGoods.Reset();
	for(const auto& Good:Registry.GetGoods())AvailableGoods.Add(FName(*Good.StableId));
	AvailableGoods.Sort(FNameLexicalLess());
	if(Snapshot.PreferredGoodStableId.IsNone()&&!AvailableGoods.IsEmpty())Snapshot.PreferredGoodStableId=AvailableGoods.Contains(TEXT("Good.Grain"))?FName(TEXT("Good.Grain")):AvailableGoods[0];
	const FHansaHouseId PlayerHouse=Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse;
    if(!bDirectoryOnly){
	StationOrders.Reset(); OrderGoods.Reset(); StationOrderCapacity=0;bStationOrdersWritable=false;
	PresenceUpgradeStageId.Reset();PresenceFundingInventoryId=FHansaInventoryId();bPresenceUpgradeFunding=false;PresenceSpecializations.Reset();PresenceSpecializationRevision=0;Snapshot.PresenceSpecializationComparison=FText();Snapshot.PresenceSpecializationAction=LOCTEXT("SpecializationUnavailable","Specializations require an active Merchant Office");Snapshot.bCanPresenceSpecializationAction=false;Snapshot.PresenceProgress=FText();Snapshot.PresenceRequirements.Reset();Snapshot.PresenceRequirementsKey.Reset();Snapshot.PresenceSources.Reset();Snapshot.PresenceSourceId=SelectedPresenceSource;Snapshot.PresenceStageSummary=FText();Snapshot.PresenceConsequences=FText();Snapshot.PresenceFundingDetail=FText();Snapshot.PresenceHistory=FText();Snapshot.PresenceReview=FText();Snapshot.PresenceStatus=FText();Snapshot.bPresenceReview=false;Snapshot.bPresenceOfficeVisual=false;Snapshot.PresenceUpgradeAction=LOCTEXT("PresenceNoUpgrade","No presence review available");Snapshot.bCanPresenceUpgradeAction=false;
	Snapshot.TradeStationValue=0;Snapshot.TradeStationState=FText::Format(LOCTEXT("SelectedContact","{0} · Visiting contact"),CityLabel(Snapshot.SelectedCityStableId.ToString()));Snapshot.TradeStationDetail=LOCTEXT("StationOpportunity","No leased plot or station storage. Meet the listed presence requirements, then establish a station at the harbor site.");Snapshot.TradeStationAction=LOCTEXT("EstablishStation","Establish trade station");Snapshot.bCanTradeStationAction=false;
    if(Runtime.IsValid())Recoveries=Runtime->BuildTradeRecovery(PlayerHouse);RefreshRecovery();
    Snapshot.Decisions=Hansa::UI::BuildTradeDecisions(Projection,Registry,PlayerHouse,Snapshot.SelectedCityStableId,Runtime.IsValid()?Runtime->GetAuthorityScenarioId():FString());RefreshDecisions();
    Snapshot.Specialization=Hansa::UI::BuildTradeSpecialization(Projection,Registry,PlayerHouse,Snapshot.SelectedCityStableId);RefreshSpecialization();
	if(const auto* Presence=Projection.GetForeignPresences().FindByPredicate([&](const auto& V){return (PlayerHouse.IsValid()&&V.HouseId==PlayerHouse)&&V.CityId.ToString()==Snapshot.SelectedCityStableId.ToString();}))
	{
		// Proposal reserves the plot; construction goods and money are paid by the separate funding command.
		Snapshot.bCanTradeStationAction=!SelectedStationSiteId.IsEmpty()&&Presence->Status==EHansaForeignPresenceStatus::Active&&!Presence->StationId.IsValid()&&Presence->NextStages.ContainsByPredicate([&](const auto& V){const auto* Stage=Registry.FindPresenceStage(V.StageId);return V.bProgressRequirementsMet&&Stage&&Stage->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"));});
        const bool LocalConstruction=Projection.GetTradeStations().ContainsByPredicate([&](const auto& S){return S.Station.Id==Presence->StationId&&S.Station.ConstructionSite.bLocalDelivery;});
        const auto* Next=Presence->NextStages.IsEmpty()?nullptr:&Presence->NextStages[0];
        const auto* Stage=Next?Registry.FindPresenceStage(Next->StageId):nullptr;
        if(Next){Snapshot.PresenceRequirements=Next->Requirements;PresenceUpgradeStageId=Next->StageId;for(const auto& Requirement:Snapshot.PresenceRequirements)Snapshot.PresenceRequirementsKey+=FString::Printf(TEXT("|%s:%lld:%lld:%d"),*Requirement.RequirementId,Requirement.CurrentValue,Requirement.RequiredValue,Requirement.bMet);}
        Snapshot.bPresenceOfficeVisual=Presence->CurrentStageId.Contains(TEXT("MerchantOffice"))||(Next&&Next->StageId.Contains(TEXT("MerchantOffice")));
        const TCHAR* State=Presence->Status==EHansaForeignPresenceStatus::Suspended?TEXT("Suspended"):Presence->Status==EHansaForeignPresenceStatus::Revoked?TEXT("Revoked"):Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Funded?TEXT("Under construction"):Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Requested?TEXT("Awaiting funding"):Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::AwaitingMaterials?TEXT("Awaiting materials"):TEXT("Active");
        if(Previous.PresenceStatus.ToString()==TEXT("Suspended")&&FCString::Strcmp(State,TEXT("Active"))==0)State=TEXT("Recovered");
        Snapshot.PresenceStatus=FText::FromString(State);
        if(bPresencePending&&!Previous.PresenceStatus.EqualTo(Snapshot.PresenceStatus))bPresencePending=false;
        FString StageSummary=FString::Printf(TEXT("%s · %s\n"),*Presence->CurrentStageDisplayName,State);
        if(Next)StageSummary+=FString::Printf(TEXT("Next: %s"),*Next->DisplayName);
        else StageSummary+=TEXT("Current commercial stage is active");
        Snapshot.PresenceStageSummary=FText::FromString(StageSummary);
        FString Consequences=TEXT("Progress: lawful trade, deliveries, qualifying investment, and reliable operation.\nOn completion: ");
        if(Stage){for(const auto& Id:Stage->GrantedCapabilityIds){const auto* Capability=Registry.FindPresenceCapability(Id);Consequences+=(Capability?Capability->DisplayName:Id)+TEXT("; ");}}
        else Consequences+=TEXT("No further stage defined.");
        Consequences+=TEXT("\nCommercial: only the listed trading rights. Physical: only owned facilities or separately leased plots. Operations: only house facilities. Civic: no city authority; a separate charter is required.");
        Snapshot.PresenceConsequences=FText::FromString(Consequences);
        FString History;
        for(int32 I=FMath::Max(0,Presence->History.Num()-8);I<Presence->History.Num();++I){const auto& Entry=Presence->History[I];const TCHAR* Kind=TEXT("Lawful contribution");switch(Entry.Kind){case EHansaPresenceHistoryKind::UpgradeRequested:Kind=TEXT("Review accepted");break;case EHansaPresenceHistoryKind::UpgradeFunded:Kind=TEXT("Funding accepted");break;case EHansaPresenceHistoryKind::UpgradeCompleted:Kind=TEXT("Construction completed");break;default:break;}History+=FString::Printf(TEXT("%s ago · %s%s%s%s\n"),*Hansa::UI::PresenceDuration(FMath::Max<int64>(0,Projection.GetClock().GetTick().GetValue()-Entry.Tick.GetValue()),Projection.GetClock().GetMinutesPerTick()).ToString(),Kind,Entry.QuantityMilliUnits?*FString::Printf(TEXT(" · %.1f units"),double(Entry.QuantityMilliUnits)/1000.):TEXT(""),Entry.MoneyPfennig?*FString::Printf(TEXT(" · %lld pfennig"),Entry.MoneyPfennig):TEXT(""),Entry.SourceEventSequence?*FString::Printf(TEXT(" · event #%llu"),Entry.SourceEventSequence):TEXT(""));}
        Snapshot.PresenceHistory=FText::FromString(History.IsEmpty()?TEXT("No recorded contributions yet."):History);
        if(Presence->Status!=EHansaForeignPresenceStatus::Active)PresenceFeedback=LOCTEXT("PresenceSuspendedRemedy","Rights are inactive. Restore station and city access before continuing.");
        if(Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Funded){
            Snapshot.PresenceUpgradeAction=LOCTEXT("PresenceBuilding","Under construction");
            Snapshot.PresenceFundingDetail=FText::Format(LOCTEXT("OfficeTimeRemaining","Funded. All required materials received. Ready in {0}."),Hansa::UI::PresenceDuration(FMath::Max<int64>(0,Presence->Upgrade.CompletionTick.GetValue()-Projection.GetClock().GetTick().GetValue()),Projection.GetClock().GetMinutesPerTick()));
        }else if(Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::AwaitingMaterials){
            Snapshot.PresenceUpgradeAction=LOCTEXT("UpgradeAwaitingMaterials","Upgrade paid · awaiting materials");
            FString Delivery=FString::Printf(TEXT("Deliver materials using inventory #%llu in this city. The current station remains operational.\n"),Presence->Upgrade.FundingInventoryId.GetValue());
            if(Stage)for(const auto& Cost:Stage->UpgradeGoods){const auto* G=Presence->Upgrade.DeliveredGoods.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==Cost.GoodId;});Delivery+=FString::Printf(TEXT("%s: delivered %.1f / %.1f units\n"),*Cost.GoodId,double(G?G->Quantity.GetRawValue():0)/1000.,double(Cost.QuantityMilliUnits)/1000.);}
            Snapshot.PresenceFundingDetail=FText::FromString(Delivery);
        }else if(Next){
            bPresenceUpgradeFunding=Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Requested;
            const auto* House=Projection.GetHouses().FindByPredicate([&](const auto& H){return H.Id==PlayerHouse;});
            const int64 Treasury=House?House->Money.GetRawValue():0;
            if(bPresenceUpgradeFunding&&Stage){
                for(const auto& Inv:Projection.GetInventories()){
                    bool Owned=false;FString Label;
                    if(Inv.OwnerKind==EHansaInventoryOwnerKind::TradeStation){Owned=Projection.GetTradeStations().ContainsByPredicate([&](const auto& X){return X.Station.InventoryId==Inv.Id&&X.Station.OwnerId==PlayerHouse;});Label=TEXT("Station");}
                    else if(Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle){Owned=Projection.GetVehicles().ContainsByPredicate([&](const auto& X){return X.Id==Inv.VehicleId&&X.OwnerId==PlayerHouse;});Label=TEXT("Ship or vehicle");}
                    else if(Inv.OwnerKind==EHansaInventoryOwnerKind::Building||Inv.OwnerKind==EHansaInventoryOwnerKind::Warehouse){Owned=Projection.GetBuildingWorldProjections().ContainsByPredicate([&](const auto& X){return X.BuildingId==Inv.BuildingId&&X.OwnerId==PlayerHouse;});Label=TEXT("Building or warehouse");}
                    if(!Owned)continue;
                    FHansaEstablishmentChoice C;C.Id=LexToString(Inv.Id.GetValue());C.Label=FText::FromString(FString::Printf(TEXT("%s · inventory #%llu"),*Label,Inv.Id.GetValue()));C.bEligible=Treasury>=Stage->UpgradeCostPfennig&&Presence->Status==EHansaForeignPresenceStatus::Active;
                    FString Detail=FString::Printf(TEXT("Treasury %lld pfennig · spend %lld · afterward %lld.\n"),Treasury,Stage->UpgradeCostPfennig,Treasury-Stage->UpgradeCostPfennig);
                    for(const auto& G:Stage->UpgradeGoods){const auto* Stock=Inv.Stocks.FindByPredicate([&](const auto& X){return X.GoodId.ToString()==G.GoodId;});const int64 Available=Stock?Stock->Available.GetRawValue():0;C.bEligible&=LocalConstruction?(Inv.OwnerKind==EHansaInventoryOwnerKind::Vehicle||Inv.CityId==Presence->CityId):Available>=G.QuantityMilliUnits;const auto* Good=Registry.FindGood(G.GoodId);Detail+=FString::Printf(TEXT("%s: need %.1f · available %.1f units.\n"),*(Good?Good->DisplayName:G.GoodId),double(G.QuantityMilliUnits)/1000.,double(Available)/1000.);}
                    if(!C.bEligible)Detail+=TEXT("Insufficient treasury, materials, or access. Replenish this source before review.\n");
                    Detail+=LocalConstruction?TEXT("Pay once on confirmation. Deliver the listed materials in this city to start the upgrade; the current station stays operational."):TEXT("Selected stock and treasury are consumed only after confirmation. Check route and production commitments.");
                    C.Detail=FText::FromString(Detail);C.Transfer=C.Detail;Snapshot.PresenceSources.Add(MoveTemp(C));
                }
                Snapshot.PresenceSources.Sort([](const auto& A,const auto& B){return FCString::Strtoui64(*A.Id,nullptr,10)<FCString::Strtoui64(*B.Id,nullptr,10);});
            }
            const auto* Chosen=Snapshot.PresenceSources.FindByPredicate([&](const auto& X){return X.Id==SelectedPresenceSource;});
            if(Chosen){PresenceFundingInventoryId=FHansaInventoryId::TryCreate(FCString::Strtoui64(*Chosen->Id,nullptr,10)).Value;Snapshot.PresenceFundingDetail=Chosen->Detail;}
            else if(bPresenceUpgradeFunding)Snapshot.PresenceFundingDetail=LOCTEXT("ChooseOfficeSource","Choose an owned station, home, or ship inventory. No source is selected automatically.");
            Snapshot.bCanPresenceUpgradeAction=!bPresencePending&&Presence->Status==EHansaForeignPresenceStatus::Active&&(bPresenceUpgradeFunding?Chosen&&Chosen->bEligible:Next->bProgressRequirementsMet);
            Snapshot.PresenceUpgradeAction=bPresenceUpgradeFunding?LOCTEXT("ReviewOfficeFunding","Review office funding"):LOCTEXT("ReviewOfficeProgress","Review office conditions");
            Snapshot.PresenceReview=FText::FromString(bPresenceUpgradeFunding?FString::Printf(TEXT("Fund %s using %s.\n%s\nConstruction begins after accepted payment. The city remains autonomous."),*Next->DisplayName,Chosen?*Chosen->Label.ToString():TEXT("no selected inventory"),*Snapshot.PresenceFundingDetail.ToString()):FString::Printf(TEXT("Request review for %s. This step spends no money or materials. Funding follows as a separate choice."),*Next->DisplayName));
        }
        Snapshot.PresenceSourceId=SelectedPresenceSource;
        if(LocalConstruction&&Next&&Next->StageId==TEXT("PresenceStage.MerchantOffice")){
            const auto Sources=Hansa::UI::BuildMerchantOfficeSources(Projection,Registry,PlayerHouse,Snapshot.SelectedCityStableId);
            if(Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::None||Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::Requested){Snapshot.PresenceSources=Sources;ConfigureMerchantOfficeUpgrade(Next->bProgressRequirementsMet);}
            else if(Presence->Upgrade.Status==EHansaPresenceUpgradeStatus::AwaitingMaterials){const auto* Chosen=Sources.FindByPredicate([&](const auto& C){return FCString::Strtoui64(*C.Id,nullptr,10)==Presence->Upgrade.FundingInventoryId.GetValue();});if(Chosen)Snapshot.PresenceFundingDetail=FText::Format(LOCTEXT("OfficeDeliveryProgress","Upgrade paid. {0}\n{1}"),Chosen->DeliveryStatus,Chosen->Transfer);}
        }
        const FString CurrentKey=PresenceReviewSignature();
        if(!PresenceReviewKey.IsEmpty()&&PresenceReviewKey!=CurrentKey){PresenceReviewKey.Reset();PresenceFeedback=LOCTEXT("OfficeStale","Review changed. Check current progress, treasury, and source stock; nothing was spent.");}
        Snapshot.bPresenceReview=!PresenceReviewKey.IsEmpty();
        if(Snapshot.bPresenceReview)Snapshot.PresenceUpgradeAction=IsAutomaticMerchantOfficeUpgrade()?LOCTEXT("ConfirmDirectOffice","Confirm Merchant Office upgrade"):bPresenceUpgradeFunding?LOCTEXT("ConfirmOfficeFunding","Confirm office funding"):LOCTEXT("ConfirmOfficeRequest","Submit office review");
        Snapshot.PresenceProgress=Snapshot.PresenceStageSummary;
        if(!PresenceFeedback.IsEmpty())Snapshot.PresenceReview=FText::Format(LOCTEXT("OfficeReviewFeedback","{0}\n{1}"),Snapshot.PresenceReview,PresenceFeedback);

	}
	if(const auto* Station=Hansa::UI::FindTradeCityStation(Projection,Snapshot.SelectedCityStableId,PlayerHouse))
	{
		Snapshot.TradeStationValue=static_cast<int64>(Station->Station.Id.GetValue());
        StationOrders=Station->Station.Orders; StationOrderCapacity=Station->StorageCapacity.GetRawValue();

        const auto* Policy=Registry.FindCityTradePolicyForCity(Station->Station.CityId.ToString());
        const auto* OrderPresence=Projection.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==PlayerHouse&&X.CityId==Station->Station.CityId;});
        bStationOrdersWritable=Station->Station.Status==EHansaTradeStationStatus::Active&&Policy&&OrderPresence&&OrderPresence->Status==EHansaForeignPresenceStatus::Active&&!Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.StationOrders"))&&OrderPresence->Capabilities.ContainsByPredicate([](const auto& C){return C.CapabilityId==TEXT("PresenceCapability.StationOrders")&&C.bGranted;});
        if(Policy){StationOrderMaxCap=Policy->MaximumOrderCapMilliUnits;StationOrderMaxBudget=Policy->MaximumOrderBudgetPfennig;}
        for(const auto& Market:Projection.GetMarkets()) if(Market.CityId==Station->Station.CityId) OrderGoods.Add(Market.GoodId);
        OrderGoods.Sort();
        if(!OrderDraft.GoodId.IsValid()&&!OrderGoods.IsEmpty()){OrderDraft.GoodId=OrderGoods[0];OrderDraft.TargetOrReserveMilliUnits=FMath::Min<int64>(10000,StationOrderCapacity);OrderDraft.TotalBudgetPfennig=FMath::Min<int64>(10000,StationOrderMaxBudget);}

		const TCHAR* Operational=TEXT("Active");switch(Station->Station.OperationalState){case EHansaTradeStationOperationalState::Underfunded:Operational=TEXT("Underfunded");break;case EHansaTradeStationOperationalState::StorageBlocked:Operational=TEXT("Storage blocked");break;case EHansaTradeStationOperationalState::OrderSuspended:Operational=TEXT("Orders suspended");break;case EHansaTradeStationOperationalState::RightsSuspended:Operational=TEXT("Rights suspended");break;case EHansaTradeStationOperationalState::VoluntarilyClosed:Operational=TEXT("Voluntarily closed");break;case EHansaTradeStationOperationalState::Revoked:Operational=TEXT("Revoked");break;default:break;}
		Snapshot.TradeStationState=FText::Format(LOCTEXT("SelectedStationState","{1} station · {0}"),FText::FromString(Station->Station.Status==EHansaTradeStationStatus::Proposed?(Station->Station.ConstructionSite.bLocalDelivery?TEXT("Paid · awaiting materials"):TEXT("Proposed")):Station->Station.Status==EHansaTradeStationStatus::UnderConstruction?TEXT("Under construction"):Operational),CityLabel(Snapshot.SelectedCityStableId.ToString()));
		Snapshot.TradeStationDetail=FText::Format(LOCTEXT("StationDetail","Site {0} · Factor #{1} · Lease #{2}\nStorage {3}/{4} units · reserved {5} · upkeep {6} pfennig/tick · arrears {7}\n{8}\nPreserved: {9}\nCosts: {10}\nRecovery: {11}"),FText::FromString(Station->Station.SiteId),FText::AsNumber(Station->Station.FactorId.GetValue()),FText::AsNumber(Station->Station.LeasedPlotId.GetValue()),FText::AsNumber(double(Station->StorageUsed.GetRawValue())/1000.),FText::AsNumber(double(Station->StorageCapacity.GetRawValue())/1000.),FText::AsNumber(double(Station->StorageReserved.GetRawValue())/1000.),FText::AsNumber(Station->Station.UpkeepPfennigPerTick),FText::AsNumber(Station->Station.OutstandingUpkeepPfennig),FText::FromString(Station->Blocker),FText::FromString(Station->PreservedAssets),FText::FromString(Station->ContinuingCosts),FText::FromString(Station->NextStep));
		if(Station->Station.Status==EHansaTradeStationStatus::Proposed)Snapshot.TradeStationAction=LOCTEXT("FundStation","Fund construction");
		else if(Station->Station.OperationalState==EHansaTradeStationOperationalState::Underfunded)Snapshot.TradeStationAction=LOCTEXT("PayStationArrears","Pay arrears and reopen");
		else if(Station->Station.Status==EHansaTradeStationStatus::Closed)Snapshot.TradeStationAction=LOCTEXT("FinalizeStationClosure","Finalize empty closure");
		else Snapshot.TradeStationAction=LOCTEXT("CloseStation","Close station safely");
		Snapshot.bCanTradeStationAction=Station->Station.Status!=EHansaTradeStationStatus::Closed||(Station->StorageUsed.GetRawValue()==0&&Station->StorageReserved.GetRawValue()==0&&Station->Lease.OccupyingBuildingIds.IsEmpty());
	}
	// Establishment has its own command. Do not advertise an office upgrade while the station is absent or unfinished.
	const auto* ActiveStation=Projection.GetTradeStations().FindByPredicate([&](const auto& V){return (PlayerHouse.IsValid()&&V.Station.OwnerId==PlayerHouse)&&V.Station.CityId.ToString()==Snapshot.SelectedCityStableId.ToString()&&V.Station.Status==EHansaTradeStationStatus::Active;});
	if(!ActiveStation)
	{
		Snapshot.bCanPresenceUpgradeAction=false;PresenceReviewKey.Reset();Snapshot.bPresenceReview=false;
		Snapshot.PresenceUpgradeAction=LOCTEXT("OfficeNeedsStation","Merchant Office requires an active trade station");
	}
	RefreshStationOrderText();
    }
	for (const FHansaCompiledCityMarketProfileDefinition& Profile : Registry.GetCityMarkets())
	{
		const auto CityId = FHansaCityDefinitionId::TryParse(Profile.StableId); if (!CityId) continue;
		FHansaTradeMapCityPresentation City; City.StableId = FName(*Profile.StableId); City.Label = CityLabel(Profile.StableId); City.NormalizedPosition = CityPosition(Profile); City.bOwned = Profile.PresentationClass == 3 || (Profile.PresentationClass == 0 && !Profile.bMarketOnly); // Authored home/founded classification; commands still enforce ownership.
		City.bMarketOnly=Profile.PresentationClass==1||(Profile.PresentationClass==0&&Profile.bMarketOnly);City.bRendered=Profile.PresentationClass==2||City.bOwned;City.bVisitable=City.bRendered;City.bBuildable=City.bOwned;
		City.bHasPresence=Projection.GetForeignPresences().ContainsByPredicate([&](const auto& Presence){return Presence.CityId==CityId.Value&&(PlayerHouse.IsValid()&&Presence.HouseId==PlayerHouse);});
		City.bHasRoute=Projection.GetRoutes().ContainsByPredicate([&](const auto& Route){return Route.OwnerId==PlayerHouse&&Route.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==CityId.Value;});});
        City.PresenceReport=LOCTEXT("MapNoPresence","No commercial presence");
        for(const auto& Presence:Projection.GetForeignPresences())if(Presence.CityId==CityId.Value&&(PlayerHouse.IsValid()&&Presence.HouseId==PlayerHouse))
        {
            City.PresenceReport=FText::FromString(Presence.CurrentStageDisplayName);
            City.bPresenceAttention=Presence.Status!=EHansaForeignPresenceStatus::Active||Presence.Upgrade.Status!=EHansaPresenceUpgradeStatus::None||Presence.NextStages.ContainsByPredicate([](const auto& Stage){return Stage.StageId.Contains(TEXT("MerchantOffice"))&&Stage.bProgressRequirementsMet;});
            if(Presence.Status==EHansaForeignPresenceStatus::Suspended)City.MapAlert=LOCTEXT("MapSuspended","Presence suspended. Open Presence to restore rights.");
            else if(Presence.Status==EHansaForeignPresenceStatus::Revoked)City.MapAlert=LOCTEXT("MapRevoked","Presence revoked. Open Presence for recovery.");
            else if(Presence.Upgrade.Status==EHansaPresenceUpgradeStatus::Requested)City.MapAlert=LOCTEXT("MapOfficeFunding","Merchant Office review accepted. Open Presence to choose funding.");
            else if(Presence.Upgrade.Status==EHansaPresenceUpgradeStatus::Funded)City.MapAlert=LOCTEXT("MapOfficeBuilding","Merchant Office under construction. Open Presence for progress.");
            else if(City.bPresenceAttention)City.MapAlert=LOCTEXT("MapOfficeReady","Merchant Office conditions met. Open Presence to review.");
        }
        for(const auto& Entry:Projection.GetTradeStations())if(Entry.Station.CityId==CityId.Value&&(PlayerHouse.IsValid()&&Entry.Station.OwnerId==PlayerHouse)&&!Entry.Blocker.IsEmpty())
            City.MapAlert=FText::FromString(Entry.Blocker+TEXT(" ")+Entry.NextStep);

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
        City.GoodReport=LOCTEXT("MapGoodUnknown","Price unknown · stock unknown");
        if(Runtime.IsValid())
        {
            const auto Good=FHansaGoodId::TryParse(Snapshot.PreferredGoodStableId.ToString());
            if(Good)
            {
                const auto Price=Runtime->QueryKnownMarketPrice(CityId.Value,Good.Value);
                const auto Supply=Runtime->QueryKnownMarketSupply(CityId.Value,Good.Value);
                City.ReportedPriceMilliMarks=Price&&Price->PriceMilliMarks.IsSet()?Price->PriceMilliMarks.GetValue():-1;
                City.ReportedStockMilliUnits=Supply&&Supply->Stock.IsSet()?Supply->Stock->GetRawValue():-1;
                City.GoodReport=FText::Format(LOCTEXT("MapGoodReport","Price: {0} Mark · stock: {1} units"),
                    Price&&Price->PriceMilliMarks.IsSet()?FText::AsNumber(double(Price->PriceMilliMarks.GetValue())/1000.):LOCTEXT("MapUnknown","unknown"),
                    Supply&&Supply->Stock.IsSet()?FText::AsNumber(double(Supply->Stock->GetRawValue())/1000.):LOCTEXT("MapUnknown","unknown"));
            }
        }
		City.Information = City.bUnknown ? LOCTEXT("NoReport", "No recent report") : City.bStale
			? FText::Format(LOCTEXT("StaleReport", "Stale report · {0} ticks old"), FText::AsNumber(MaxAge))
			: FText::Format(LOCTEXT("CurrentReport", "Current · {0} ticks old"), FText::AsNumber(MaxAge));
		if(Runtime.IsValid()) {
			const auto Good=FHansaGoodId::TryParse(Snapshot.PreferredGoodStableId.ToString());const auto Report=Good?Runtime->QueryKnownMarketPrice(CityId.Value,Good.Value):TOptional<FHansaKnownMarketPriceProjection>();
            if(Report.IsSet()&&!City.bUnknown)City.Information=FText::Format(LOCTEXT("KnownReportAge","{0} · {1} ticks old"),FText::FromString(LexToString(Report->InformationState)),FText::AsNumber(MaxAge));
        }
        for (const auto& Entry : Projection.GetTradeStations())
            if (Entry.Station.CityId == CityId.Value && Entry.Station.OwnerId == PlayerHouse && Entry.Station.Status == EHansaTradeStationStatus::Active)
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
		Item.bCancelled=Route.Lifecycle==EHansaRouteLifecycleState::Cancelled;
		Item.bTraveling = Route.Lifecycle == EHansaRouteLifecycleState::Traveling;
		Item.bOwnedByPlayer = Route.OwnerId == (Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse);
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
		TArray<FString> Names; for (const FHansaRouteStop& Stop : Route.Stops) { Names.Add(CityLabel(Stop.CityId.ToString()).ToString()); Item.CityIds.Add(FName(*Stop.CityId.ToString())); } Item.StopSummary = FText::FromString(FString::Join(Names, TEXT(" / ")));
		int32 RoundTrip = 0; if (Definition != nullptr && Route.Stops.Num() > 1) for (int32 I=0; I<Route.Stops.Num(); ++I) RoundTrip += LegTicks(*Definition, Route.Stops[I].CityId.ToString(), Route.Stops[(I+1)%Route.Stops.Num()].CityId.ToString());
		Item.RoundTripTime = FText::Format(LOCTEXT("Ticks", "{0} ticks round trip"), FText::AsNumber(RoundTrip));
		if (Vehicle != nullptr)
		{
			Item.CapacityMilliUnits=Vehicle->Capacity.GetRawValue();
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
        if(!Item.bOwnedByPlayer) {
            Item.CapacityMilliUnits=0;Item.CityIds.Reset();Item.StopSummary=LOCTEXT("PrivateStops","Stop plan private");
            Item.RoundTripTime=LOCTEXT("PrivateDuration","Voyage duration private");
            Item.State=FText::FromString(LexToString(Route.Lifecycle));Item.StateHeading=Item.State;
            Item.Capacity=LOCTEXT("PrivateCargo","Cargo private");
            Item.Upkeep=LOCTEXT("PrivateUpkeep","Upkeep private");
            Item.ExpectedProfitRange=LOCTEXT("PrivateCash","Cash effect private");
            Item.Uncertainty=LOCTEXT("PrivateOrders","Cargo instructions private");
            Item.StateDetail=LOCTEXT("OwnerOnlyDetail","Only the owning house can inspect cargo and change this route.");
            Item.bProfitKnown=false;Item.bReserveRisk=false;
        }
		AllRoutes.Add(MoveTemp(Item));
	}
	RebuildFilteredProjection();
    if(bDirectoryOnly){RebuildDirectory();return true;}
    if(!Snapshot.bCreating&&!Snapshot.bDirty&&Snapshot.SelectedRouteValue==0&&Snapshot.CogValue>0&&!Snapshot.DraftName.IsEmpty())
        if(const auto* Created=AllRoutes.FindByPredicate([&](const auto& Route){return Route.bOwnedByPlayer&&!Route.bCancelled&&Route.VehicleValue==Snapshot.CogValue&&Route.Label.ToString().Contains(Snapshot.DraftName);}))Snapshot.SelectedRouteValue=Created->RouteValue;
	if (!Snapshot.bCreating && !Snapshot.bDirty && !Snapshot.Routes.IsEmpty() && ((Snapshot.SelectedRouteValue == 0 && Snapshot.SelectedVehicleValue==0) || (Snapshot.SelectedRouteValue!=0&&!AllRoutes.ContainsByPredicate(
		[this](const FHansaTradeMapRoutePresentation& Route){ return Route.RouteValue == Snapshot.SelectedRouteValue; }))))
	{
		Snapshot.SelectedRouteValue = Snapshot.Routes[0].RouteValue;Snapshot.SelectedVehicleValue=Snapshot.Routes[0].VehicleValue;
	}
	const FHansaRouteProjection* Selected = Projection.GetRoutes().FindByPredicate([this](const FHansaRouteProjection& R){ return static_cast<int64>(R.Id.GetValue()) == Snapshot.SelectedRouteValue; });
	if (!Snapshot.bCreating && !Snapshot.bDirty && Selected != nullptr) {
        const auto* Visible=AllRoutes.FindByPredicate([&](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});
        if(Visible&&Visible->bOwnedByPlayer)DraftStops=Selected->Stops;else DraftStops.Reset();
    }
    Snapshot.bShipInTransit = !Snapshot.bCreating && Selected && Selected->OwnerId==PlayerHouse && Selected->Lifecycle==EHansaRouteLifecycleState::Traveling && Selected->Stops.IsValidIndex(Selected->CurrentStopIndex) && Selected->Stops.IsValidIndex(Selected->NextStopIndex);
    if(Snapshot.bShipInTransit) {
		auto Position=[&](const FHansaCityDefinitionId Id){const auto* City=AllCities.FindByPredicate([&](const auto& V){return V.StableId==FName(*Id.ToString());});return City?City->NormalizedPosition:FVector2D::ZeroVector;};
		const auto A=Position(Selected->Stops[Selected->CurrentStopIndex].CityId),B=Position(Selected->Stops[Selected->NextStopIndex].CityId);
        const auto Mid=(A+B)*.5;
        const double T=1.0-double(Selected->RemainingTravelTicks)/FMath::Max(1,Selected->TotalTravelTicks);
        Snapshot.ShipPosition=A*(1-T)*(1-T)+Mid*(2*T*(1-T))+B*T*T;
    }
    Snapshot.CityInspector=Hansa::UI::BuildTradeCityInspector(Snapshot.SelectedCityStableId,GetSelectedCityPresentation(),Projection,Registry,PlayerHouse);
    const auto& CitySummary=Snapshot.CityInspector;
    if(Snapshot.TradeStationValue==0){
        Snapshot.TradeStationState=FText::Format(LOCTEXT("SelectedPresenceState","{0} · {1}"),CityLabel(Snapshot.SelectedCityStableId.ToString()),CitySummary.Presence);
        Snapshot.TradeStationDetail=FText::Format(LOCTEXT("SelectedPresenceDetail","{0}\n{1}"),CitySummary.Overview,CitySummary.Issue);
        Snapshot.bCanTradeStationAction &= CitySummary.bStationSupported;
        if(!CitySummary.bStationSupported){
            Snapshot.TradeStationAction=LOCTEXT("StationNotApplicable","Station unavailable in this city");
            Snapshot.bCanPresenceUpgradeAction=false;
            Snapshot.PresenceProgress=Snapshot.TradeStationDetail;
            Snapshot.PresenceUpgradeAction=LOCTEXT("NoOfficeHere","No office upgrade available here");
            Snapshot.PresenceSpecializationComparison=Snapshot.TradeStationDetail;
        }
        Snapshot.StationOrderText=FText::Format(LOCTEXT("SelectedOrdersLocked","Station orders unavailable in {0}. {1}\n{2}"),CityLabel(Snapshot.SelectedCityStableId.ToString()),CitySummary.Overview,CitySummary.Issue);
    }
    RebuildDirectory(); RebuildStops(); RefreshEstablishment();Snapshot.LedgerKey=GetLedgerPresentation().Key();Snapshot.ConstructionKey=GetConstructionPresentation().Key();
	PublishIfChanged(Previous); return true;
}
FString UHansaTradeMapPresentationModel::PresenceReviewSignature() const
{
    const auto* Source=Snapshot.PresenceSources.FindByPredicate([&](const auto& C){return C.Id==SelectedPresenceSource;});
    FString Key=Snapshot.SelectedCityStableId.ToString()+TEXT("|")+PresenceUpgradeStageId+TEXT("|")+SelectedPresenceSource+TEXT("|")+Snapshot.PresenceStageSummary.ToString()+TEXT("|")+Snapshot.PresenceConsequences.ToString()+TEXT("|")+
        (Source&&!Source->ReviewKey.IsEmpty()?Source->ReviewKey:Snapshot.PresenceFundingDetail.ToString())+TEXT("|")+FString::FromInt(Snapshot.bCanPresenceUpgradeAction);
    // Requirements are thresholds. Earned progress above the threshold does not change the order.
    for(const auto& R:Snapshot.PresenceRequirements)Key+=FString::Printf(TEXT("|%s:%lld:%d"),*R.RequirementId,R.RequiredValue,R.bMet);
    return Key;
}
bool UHansaTradeMapPresentationModel::IsAutomaticMerchantOfficeUpgrade() const {
    return Snapshot.Establishment.bLocalDelivery&&PresenceUpgradeStageId==TEXT("PresenceStage.MerchantOffice");
}
void UHansaTradeMapPresentationModel::ConfigureMerchantOfficeUpgrade(bool ProgressMet) {
    bPresenceUpgradeFunding=true;
    if(SelectedPresenceSource.IsEmpty()){
        const auto* Suggested=Snapshot.PresenceSources.FindByPredicate([](const auto& C){return C.bEligible;});
        if(Suggested)SelectedPresenceSource=Suggested->Id;
    }
    const auto* Chosen=Snapshot.PresenceSources.FindByPredicate([&](const auto& C){return C.Id==SelectedPresenceSource;});
    PresenceFundingInventoryId=Chosen?FHansaInventoryId::TryCreate(FCString::Strtoui64(*Chosen->Id,nullptr,10)).Value:FHansaInventoryId();
    Snapshot.PresenceSourceId=SelectedPresenceSource;
    Snapshot.PresenceFundingDetail=Chosen?Chosen->Detail:LOCTEXT("OfficeNeedDelivery","No suitable delivery source. Choose a Cog with a route through a home city and this destination, or supply the station's storage.");
    Snapshot.bCanPresenceUpgradeAction=!bPresencePending&&ProgressMet&&Chosen&&Chosen->bEligible;
    Snapshot.PresenceUpgradeAction=LOCTEXT("DirectOfficeUpgrade","Upgrade to Merchant Office");
    Snapshot.PresenceReview=FText::Format(LOCTEXT("DirectOfficeReview","Source: {0}\n{1}\n{2}"),Chosen?Chosen->Label:LOCTEXT("OfficeNoSource","No suitable delivery source"),Snapshot.PresenceFundingDetail,
        ProgressMet?LOCTEXT("OfficeProgressMet","Trading requirements met."):LOCTEXT("OfficeProgressUnmet","Trading requirements are not yet met. Continue lawful deliveries and reliable operation."));
}
bool UHansaTradeMapPresentationModel::PresenceSourceIntent()
{
    if(bPresencePending||!bPresenceUpgradeFunding||Snapshot.PresenceSources.IsEmpty())return false;
    const int32 Index=Snapshot.PresenceSources.IndexOfByPredicate([&](const auto& X){return X.Id==SelectedPresenceSource;});
    return SelectPresenceSource(Snapshot.PresenceSources[(Index+1)%Snapshot.PresenceSources.Num()].Id);
}
bool UHansaTradeMapPresentationModel::SelectPresenceSource(const FString& SourceId)
{
    if(bPresencePending||!bPresenceUpgradeFunding)return false;
    const auto* Candidate=Snapshot.PresenceSources.FindByPredicate([&](const auto& X){return X.Id==SourceId;});if(!Candidate)return false;
    const auto Before=Snapshot;const auto& Choice=*Candidate;
    SelectedPresenceSource=Choice.Id;Snapshot.PresenceSourceId=Choice.Id;PresenceFundingInventoryId=FHansaInventoryId::TryCreate(FCString::Strtoui64(*Choice.Id,nullptr,10)).Value;
    PresenceReviewKey.Reset();PresenceFeedback=FText();Snapshot.bPresenceReview=false;Snapshot.PresenceFundingDetail=Choice.Detail;Snapshot.bCanPresenceUpgradeAction=Choice.bEligible;
    Snapshot.PresenceUpgradeAction=LOCTEXT("ReviewOfficeFunding","Review office funding");
    Snapshot.PresenceReview=FText::Format(LOCTEXT("ReviewChosenOfficeSource","Fund the next stage using {0}.\n{1}\nConstruction begins after accepted payment. The city remains autonomous."),Choice.Label,Choice.Detail);
    if(IsAutomaticMerchantOfficeUpgrade())ConfigureMerchantOfficeUpgrade(!Snapshot.PresenceRequirements.ContainsByPredicate([](const auto& R){return R.RequirementId!=TEXT("AvailableMoney")&&!R.RequirementId.StartsWith(TEXT("UpgradeGood."))&&!R.bMet;}));
    PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::PresenceCancelReviewIntent()
{
    if(PresenceReviewKey.IsEmpty())return false;const auto Before=Snapshot;PresenceReviewKey.Reset();Snapshot.bPresenceReview=false;
    Snapshot.PresenceUpgradeAction=IsAutomaticMerchantOfficeUpgrade()?LOCTEXT("DirectOfficeUpgrade","Upgrade to Merchant Office"):bPresenceUpgradeFunding?LOCTEXT("ReviewOfficeFunding","Review office funding"):LOCTEXT("ReviewOfficeProgress","Review office conditions");PublishIfChanged(Before);return true;
}
bool UHansaTradeMapPresentationModel::PresenceUpgradeActionIntent()
{
    if(IsAnyTradeCommandPending()||(!Runtime.IsValid()&&!NetworkCommandIntent)||!Snapshot.bCanPresenceUpgradeAction||PresenceUpgradeStageId.IsEmpty())return false;
    const auto Previous=Snapshot;
    const FString CurrentKey=PresenceReviewSignature();
    if(PresenceReviewKey.IsEmpty()){PresenceReviewKey=CurrentKey;Snapshot.bPresenceReview=true;Snapshot.PresenceUpgradeAction=IsAutomaticMerchantOfficeUpgrade()?LOCTEXT("ConfirmDirectOffice","Confirm Merchant Office upgrade"):bPresenceUpgradeFunding?LOCTEXT("ConfirmOfficeFunding","Confirm office funding"):LOCTEXT("ConfirmOfficeRequest","Submit office review");PublishIfChanged(Previous);return true;}
    if(PresenceReviewKey!=CurrentKey){PresenceReviewKey.Reset();Snapshot.bPresenceReview=false;PresenceFeedback=LOCTEXT("OfficeStale","Review changed. Check current progress, treasury, and source stock; nothing was spent.");PublishIfChanged(Previous);return false;}
    const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());if(!City)return false;bool Sent=false;
    if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=bPresenceUpgradeFunding?EHansaClientIntentType::FundPresenceUpgrade:EHansaClientIntentType::RequestPresenceUpgrade;I.CityId=City.Value.ToString();I.PresenceStageId=PresenceUpgradeStageId;I.FundingInventoryId=static_cast<int64>(PresenceFundingInventoryId.GetValue());bPresencePending=true;PresencePendingSequence=PresencePendingNonce=0;Sent=NetworkCommandIntent(I);if(!Sent)bPresencePending=false;}
    else if(bPresenceUpgradeFunding)Sent=Runtime->FundPresenceUpgrade({City.Value,PresenceUpgradeStageId,PresenceFundingInventoryId}).IsSuccess();else Sent=Runtime->RequestPresenceUpgrade({City.Value,PresenceUpgradeStageId}).IsSuccess();
    PresenceReviewKey.Reset();PresenceFeedback=Sent?LOCTEXT("PresenceIntentSent","Accepted. The authoritative stage will update shortly."):LOCTEXT("PresenceIntentFailed","Rejected. Refresh progress, rights, treasury, and source stock before reviewing again.");
    if(Sent&&Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}
    Snapshot.EditorStatus=PresenceFeedback;Snapshot.bPresenceReview=false;if(bPresencePending){Snapshot.bCanPresenceUpgradeAction=false;Snapshot.PresenceUpgradeAction=LOCTEXT("OfficePending","Awaiting authority");}PublishIfChanged(Previous);return Sent;
}

bool UHansaTradeMapPresentationModel::ReceivePresenceFeedback(const FHansaClientCommandFeedback& Feedback)
{
    if(!bPresencePending)return false;
    if(Feedback.State==EHansaClientCommandState::Pending){if(PresencePendingSequence==0){PresencePendingSequence=Feedback.ClientSequence;PresencePendingNonce=Feedback.ClientNonce;}return true;}
    if(PresencePendingSequence==0||Feedback.ClientSequence!=PresencePendingSequence||Feedback.ClientNonce!=PresencePendingNonce)return false;
    const auto Before=Snapshot;bPresencePending=false;PresencePendingSequence=PresencePendingNonce=0;
    PresenceFeedback=FText::FromString(Feedback.Message+TEXT(" ")+Feedback.Remedy);
    Snapshot.PresenceReview=PresenceFeedback;
    Snapshot.PresenceUpgradeAction=IsAutomaticMerchantOfficeUpgrade()?LOCTEXT("DirectOfficeUpgrade","Upgrade to Merchant Office"):bPresenceUpgradeFunding?LOCTEXT("ReviewOfficeFunding","Review office funding"):LOCTEXT("ReviewOfficeProgress","Review office conditions");
    if(!Feedback.bAccepted)Snapshot.PresenceStatus=LOCTEXT("OfficeRejected","Rejected / stale");
    Snapshot.EditorStatus=PresenceFeedback;PublishIfChanged(Before);return true;
}

bool UHansaTradeMapPresentationModel::Open(const FName FocusOrigin, const FName PreferredGood, const FName SourceCity, const bool bBeginRoute)
{
	const bool WasStation=bWorldStationDetail; bWorldStationDetail=false;
	const FHansaTradeMapSnapshot Previous=Snapshot; FocusOriginSemanticId=FocusOrigin; Snapshot.bOpen=true; if(!PreferredGood.IsNone())Snapshot.PreferredGoodStableId=PreferredGood;
    if (bBeginRoute && !PreferredGood.IsNone() && !Snapshot.bCreating) return BeginCreateIntent(PreferredGood, SourceCity);
    if (!Snapshot.bCreating && !Snapshot.bDirty && Runtime.IsValid()) { const auto P=Runtime->BuildProjection(); if(P){ const auto* R=P.Value.GetRoutes().FindByPredicate([this](const auto& X){return static_cast<int64>(X.Id.GetValue())==Snapshot.SelectedRouteValue;}); if(R&&R->OwnerId==ViewerHouse){DraftStops=R->Stops;Snapshot.SelectedStopIndex=0;RebuildStops();}} }

	Snapshot.FocusedSemanticId=TEXT("TradeMap.Close");
    if(!ConstructionReturnFocus.IsNone()&&Snapshot.ActiveSection==TEXT("Construction"))
    {
        const auto Construction=GetConstructionPresentation();
        Snapshot.FocusedSemanticId=Construction.SelectedBuilding.IsNone()?FName(TEXT("TradeMap.Construction.Visit")):ConstructionReturnFocus;
    }
    ConstructionReturnFocus=NAME_None;
    PublishIfChanged(Previous); if(WasStation)Changed.Broadcast(Snapshot,++Revision); return Previous.bOpen != Snapshot.bOpen || Previous.PreferredGoodStableId != PreferredGood;
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
	Snapshot.RouteWindowStart=0; Snapshot.Routes=MoveTemp(Matching);
	RebuildDirectory();
}
bool UHansaTradeMapPresentationModel::CycleModeFilterIntent(){if(Snapshot.bCreating)return false;const auto Previous=Snapshot;Snapshot.ModeFilter=static_cast<EHansaTradeMapModeFilter>((static_cast<uint8>(Snapshot.ModeFilter)+1)%3);Snapshot.RouteWindowStart=0;RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CycleCityFilterIntent(){if(Snapshot.bCreating)return false;const auto Previous=Snapshot;Snapshot.CityFilter=static_cast<EHansaTradeMapCityFilter>((static_cast<uint8>(Snapshot.CityFilter)+1)%3);RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::SetCitySearchIntent(const FString& SearchText){if(Snapshot.bCreating)return false;const FString Normalized=SearchText.Left(64);if(Snapshot.CitySearchText==Normalized)return true;const auto Previous=Snapshot;Snapshot.CitySearchText=Normalized;RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CycleSelectedGoodIntent(){if(Snapshot.bCreating||AvailableGoods.IsEmpty())return false;const auto Previous=Snapshot;int32 Index=AvailableGoods.IndexOfByKey(Snapshot.PreferredGoodStableId);Snapshot.PreferredGoodStableId=AvailableGoods[(Index+1)%AvailableGoods.Num()];if(bRemoteEstablishment){RefreshRemoteCityReports();RebuildFilteredProjection();}RebuildDirectory();if(Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)return ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::MoveRouteWindowIntent(const int32 Direction){if(Snapshot.bCreating||Direction==0)return false;const auto Previous=Snapshot;Snapshot.RouteWindowStart=FMath::Clamp(Snapshot.RouteWindowStart+Direction*20,0,FMath::Max(0,Snapshot.MatchingRouteCount-1));RebuildFilteredProjection();PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::SelectRouteIntent(const int64 Value)
{
    if(Snapshot.SelectedRouteValue==Value&&Value!=0){bShipDetailOpen=true;ShipDetailTab=TEXT("Route");Changed.Broadcast(Snapshot,++Revision);return true;}
    if(CargoEditor.bOpen||IsAnyTradeCommandPending()||Snapshot.bCreating||Snapshot.bDirty)return false;
    const auto* Found=AllRoutes.FindByPredicate([Value](const auto& R){return R.RouteValue==Value;});if(!Found)return false;
    bShipDetailOpen=true;ShipDetailTab=TEXT("Route");
    const auto Previous=Snapshot;Snapshot.SelectedRouteValue=Value;Snapshot.SelectedVehicleValue=Found->VehicleValue;Snapshot.SelectedStopIndex=0;Snapshot.bDirty=false;
    Snapshot.ActiveSection=TEXT("Overview");
    if(!Found->CityIds.IsEmpty())Snapshot.SelectedCityStableId=Found->CityIds[0];
    if(LastProjection)if(const auto* V=LastProjection->GetVehicles().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==Found->VehicleValue;}))Snapshot.SelectedCityStableId=FName(*V->CurrentCityId.ToString());
    if(Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}
    else if(LastProjection&&LastRegistry){const auto Projection=LastProjection;ApplyProjection(*Projection,*LastRegistry);}
    Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Route.%lld"),Value));
    if(bRemoteEstablishment)RefreshRecovery();RebuildStops();PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::SelectStopIntent(const int32 Index){ if(!DraftStops.IsValidIndex(Index))return false; const auto Previous=Snapshot; Snapshot.SelectedStopIndex=Index; Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Stop.%d"),Index)); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::CycleCargoActionIntent(){ if(!CanEditStops()||!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; for(int32 Candidate=0;Candidate<6;++Candidate) {
 A.Kind=static_cast<EHansaRouteCargoActionKind>((static_cast<uint8>(A.Kind)+1)%6);
 const auto& City=DraftStops[Snapshot.SelectedStopIndex].CityId;
 const auto House=Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse;
 if(IsStationTransfer(A.Kind)&&bRemoteEstablishment){if(!Recoveries.ContainsByPredicate([&](const auto& R){return R.City.ToString()==City.ToString()&&(A.Kind==EHansaRouteCargoActionKind::StationLoad?R.Station>0:R.Status==TEXT("Active"));}))continue;}
 else if(IsStationTransfer(A.Kind)&&(!LastProjection||!LastProjection->GetTradeStations().ContainsByPredicate([&](const auto& S){return S.Station.OwnerId==House&&S.Station.CityId==City&&(A.Kind==EHansaRouteCargoActionKind::StationLoad?(S.Station.Status!=EHansaTradeStationStatus::Proposed&&S.Station.Status!=EHansaTradeStationStatus::UnderConstruction):S.Station.Status==EHansaTradeStationStatus::Active);})))continue;
 if((A.Kind==EHansaRouteCargoActionKind::OwnedCityLoad||A.Kind==EHansaRouteCargoActionKind::OwnedCityUnload)&&!AllCities.ContainsByPredicate([&](const auto& C){return C.StableId.ToString()==City.ToString()&&C.bOwned;}))continue;
 break;
 } Snapshot.bDirty=true;Snapshot.bReview=false; RebuildStops(true); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::AdjustQuantityIntent(const int32 Delta){ if(!CanEditStops()||!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.QuantityLimit=FHansaQuantity::FromRaw(FMath::Max<int64>(1000,A.QuantityLimit.GetRawValue()+Delta)); Snapshot.bDirty=true;Snapshot.bReview=false; RebuildStops(true); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::AdjustMinimumReserveIntent(const int32 Delta){ if(!CanEditStops()||!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty())return false; const auto Previous=Snapshot; auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0]; A.MinimumSourceReserve=FHansaQuantity::FromRaw(FMath::Max<int64>(0,A.MinimumSourceReserve.GetRawValue()+Delta)); Snapshot.bDirty=true;Snapshot.bReview=false; RebuildStops(true); PublishIfChanged(Previous); return true; }
bool UHansaTradeMapPresentationModel::MoveStopIntent(const int32 Direction){const int32 To=Snapshot.SelectedStopIndex+Direction;if(CargoEditor.bOpen||!CanEditStops()||!DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)||!DraftStops.IsValidIndex(To))return false;const auto Previous=Snapshot;DraftStops.Swap(Snapshot.SelectedStopIndex,To);Snapshot.SelectedStopIndex=To;Snapshot.bDirty=true;Snapshot.bReview=false;RebuildStops(true);PublishIfChanged(Previous);return true;}
bool UHansaTradeMapPresentationModel::CommitIntent()
{
	if (Snapshot.bCreating) return ReviewCreateIntent();
	if (IsAnyTradeCommandPending() || !Snapshot.bDirty || (!Runtime.IsValid() && !NetworkCommandIntent)) return false;
	if (!FHansaCargoPlan::Validate(GetSlotDraft(), GetShipCapacity()))
	{
		const auto Previous = Snapshot;
		Snapshot.EditorStatus = FHansaCargoPlan::HasConflictingCityActions(GetSlotDraft())
            ? LOCTEXT("CargoCityConflict", "Route not saved: a product is set to both load and unload in the same city. Remove one of those instructions, then save again.")
            : LOCTEXT("CargoSlotConflict", "Route not saved: a cargo slot still holds a different product. Unload it or use another slot, then save again.");
		PublishIfChanged(Previous);
		return false;
	}
	const auto Id = FHansaRouteId::TryCreate(static_cast<uint64>(Snapshot.SelectedRouteValue)); if (!Id) return false;
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::EditRoute; Intent.RouteId = Snapshot.SelectedRouteValue;
		AppendClientRouteStops(DraftStops, Intent);
        if(bRemoteEstablishment)Intent.ExpectedRoutePlanKey=RemoteDraftPlanKey;
		const auto Previous = Snapshot; Snapshot.bCommandPending=true;bPendingCreate=false;bPendingRouteEdit=true;PendingSequence=PendingNonce=0;const bool bSent = NetworkCommandIntent(Intent);
		if(!bSent||Snapshot.bCommandPending) Snapshot.EditorStatus = bSent ? LOCTEXT("EditPending", "Route changes sent to the authoritative server.") : LOCTEXT("EditSendFailed", "Route changes could not be sent.");
		if (!bSent) Snapshot.bCommandPending=false; PublishIfChanged(Previous); return bSent;
	}
	const auto Result=Runtime->EditRoute(Id.Value,DraftStops);const auto Previous=Snapshot;const FText Failure=Result.GetError()==EHansaCommandGatewayError::ResearchEffectRequired ? LOCTEXT("EditResearchRequired", "complete Reserve instructions research before saving a minimum reserve; your draft is preserved") : Result.GetError()==EHansaCommandGatewayError::RouteStateInvalid ? LOCTEXT("EditStoppedEmpty", "cargo instructions can change on an active route; changing the stops requires a paused route with an empty hold at the first stop. Your draft is preserved") : FText::FromString(Result.GetRoutePlanError()!=EHansaRoutePlanError::None ? LexToString(Result.GetRoutePlanError()) : LexToString(Result.GetError()));Snapshot.EditorStatus=Result?LOCTEXT("Saved","Route changes saved. Cargo instructions apply at the next stop; the current voyage and cargo are preserved."):FText::Format(LOCTEXT("SaveFailed","Route could not be saved: {0}."),Failure);if(Result)Snapshot.bDirty=false;PublishIfChanged(Previous);return !!Result;
}
bool UHansaTradeMapPresentationModel::ToggleActiveIntent()
{
	if (IsAnyTradeCommandPending() || (!Runtime.IsValid() && !NetworkCommandIntent) || Snapshot.bCreating) return false;
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
    if(bRemoteEstablishment&&bActivate&&Snapshot.bDirty){const auto Before=Snapshot;Snapshot.EditorStatus=LOCTEXT("SaveBeforeStart","Save the route changes and wait for the server acknowledgement before starting this route.");PublishIfChanged(Before);return false;}
	if (bActivate && Snapshot.bDirty && !CommitIntent()) return false;
    if(Snapshot.bCommandPending)return false;
	const auto Previous = Snapshot;
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::SetRouteActive;
		Intent.RouteId = Snapshot.SelectedRouteValue; Intent.bActive = bActivate;
		Snapshot.bCommandPending=true;bPendingCreate=false;bPendingRouteEdit=false;PendingSequence=PendingNonce=0;
		const bool bSent = NetworkCommandIntent(Intent);
        if(!bSent)Snapshot.bCommandPending=false;
		if(!bSent||Snapshot.bCommandPending) Snapshot.EditorStatus = bSent ? LOCTEXT("RouteTogglePending", "Route state change sent to the authoritative server.") : LOCTEXT("RouteToggleSendFailed", "Route state change could not be sent.");
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
 if(Snapshot.Establishment.bVisible&&(Snapshot.TradeStationValue==0||Snapshot.Establishment.bProposed||Snapshot.Establishment.bConstructing||Snapshot.Establishment.bComplete||Snapshot.Establishment.bArrears))return EstablishmentIntent(Snapshot.Establishment.bLocalDelivery&&Snapshot.Establishment.bProposed?TEXT("Arrange"):TEXT("Review"));
 if(!Snapshot.bCanTradeStationAction||bEstablishmentPending)return false;
 const auto Id=FHansaTradeStationId::TryCreate(uint64(Snapshot.TradeStationValue));if(!Id)return false;
 return OpenRecovery(Snapshot.SelectedCityStableId,Snapshot.TradeStationValue,TEXT("TradeMap.Station.Action"));
}
bool UHansaTradeMapPresentationModel::SelectCityIntent(const FName CityId)
{
    if(IsAnyTradeCommandPending()||Snapshot.bCreating||!AllCities.ContainsByPredicate([&](const auto& City){return City.StableId==CityId;})||(!bRemoteEstablishment&&(!LastProjection||!LastRegistry)))return false;
    const auto Previous=Snapshot;
    if(CityId==Snapshot.SelectedCityStableId){const auto* Selected=AllCities.FindByPredicate([&](const auto& C){return C.StableId==CityId;});if(Selected&&Selected->bPresenceAttention){Snapshot.ActiveSection=TEXT("Presence");Snapshot.WorkspacePage=TEXT("Workspace");PublishIfChanged(Previous);}return true;}
    Snapshot.SelectedCityStableId=CityId;const auto* ChosenCity=AllCities.FindByPredicate([&](const auto& C){return C.StableId==CityId;});Snapshot.ActiveSection=ChosenCity&&ChosenCity->bPresenceAttention?TEXT("Presence"):TEXT("Overview");if(ChosenCity&&ChosenCity->bPresenceAttention)Snapshot.WorkspacePage=TEXT("Workspace");SelectedStationOrder=0;OrderDraft={};Snapshot.bConfirmStationOrderCancel=false;Snapshot.StationOrderFeedback=FText();Snapshot.PresenceSpecializationFeedback=FText();Snapshot.SelectedPresenceSpecializationId.Reset();Snapshot.SpecializationSourceId.Reset();SpecializationReviewedKey.Reset();
    if(bRemoteEstablishment){if(RemoteInterestRequested)RemoteInterestRequested(CityId);SelectedConstructionLease=0;SelectedConstructionBuilding=NAME_None;ConstructionReturnFocus=NAME_None;SelectedStationSiteId.Reset();SelectedFundingSource.Reset();EstablishmentReviewKey.Reset();SelectedPresenceSource.Reset();PresenceReviewKey.Reset();RefreshEstablishment();RefreshRemotePresence();Snapshot.ConstructionKey=GetConstructionPresentation().Key();}else{const auto Projection=LastProjection;ApplyProjection(*Projection,*LastRegistry);}PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::CycleCityIntent(const int32 Direction)
{
    if(Snapshot.Cities.IsEmpty()||Snapshot.bCreating)return false;
    const int32 Index=Snapshot.Cities.IndexOfByPredicate([&](const auto& City){return City.StableId==Snapshot.SelectedCityStableId;});
    return SelectCityIntent(Snapshot.Cities[(FMath::Max(0,Index)+Direction+Snapshot.Cities.Num())%Snapshot.Cities.Num()].StableId);
}
bool UHansaTradeMapPresentationModel::SelectSectionIntent(const FString& Section)
{
    if(Snapshot.bCreating || (Section!=TEXT("Overview")&&Section!=TEXT("Route")&&Section!=TEXT("Presence")&&Section!=TEXT("StationUpgrade")&&Section!=TEXT("Specialization")&&Section!=TEXT("Orders")&&Section!=TEXT("Ledger")&&Section!=TEXT("Construction")&&Section!=TEXT("Decisions")&&Section!=TEXT("Recovery")))return false;
    if(Section==TEXT("Route")&&Snapshot.SelectedVehicleValue){bShipDetailOpen=true;ShipDetailTab=TEXT("Route");Changed.Broadcast(Snapshot,++Revision);}
    if(Snapshot.ActiveSection==Section)return true;
    const auto Previous=Snapshot;Snapshot.ActiveSection=Section;PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::SelectWorkspacePageIntent(const FString& Page)
{
    if(Page!=TEXT("Map")&&Page!=TEXT("Routes")&&Page!=TEXT("Workspace")&&Page!=TEXT("Schedule"))return false;
    if(Snapshot.WorkspacePage==Page)return true;
    const auto Previous=Snapshot;Snapshot.WorkspacePage=Page;PublishIfChanged(Previous);return true;
}
void UHansaTradeMapPresentationModel::SetCompact(const bool Value){if(Snapshot.bCompact==Value)return;const auto Previous=Snapshot;Snapshot.bCompact=Value;PublishIfChanged(Previous);}
void UHansaTradeMapPresentationModel::SetFocusedSemanticId(const FName Id){if(Snapshot.FocusedSemanticId==Id)return;const auto Previous=Snapshot;Snapshot.FocusedSemanticId=Id;PublishIfChanged(Previous);}
const FHansaTradeMapRoutePresentation* UHansaTradeMapPresentationModel::FindSelectedRoute()const{return AllRoutes.FindByPredicate([this](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});}
bool UHansaTradeMapPresentationModel::CancelRouteIntent()
{
    if (IsAnyTradeCommandPending() || (!Runtime.IsValid() && !NetworkCommandIntent) || Snapshot.bCreating) return false;
    const auto* Route = FindSelectedRoute();
    if (!Route || !Route->bCanCancel) return false;
    const auto Previous = Snapshot;
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::CancelRoute; Intent.RouteId = Snapshot.SelectedRouteValue;
		Snapshot.bCommandPending=true;bPendingCreate=false;bPendingRouteEdit=false;PendingSequence=PendingNonce=0;
		const bool bSent = NetworkCommandIntent(Intent);
        if(!bSent)Snapshot.bCommandPending=false;
		if(!bSent||Snapshot.bCommandPending) Snapshot.EditorStatus = bSent ? LOCTEXT("RouteCancelPending", "Route cancellation sent to the authoritative server.") : LOCTEXT("RouteCancelSendFailed", "Route cancellation could not be sent.");
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

void UHansaTradeMapPresentationModel::RebuildStops(const bool bDraftChanged)
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
    if (Snapshot.bDirty && bDraftChanged) Snapshot.EditorStatus = LOCTEXT("Unsaved", "Unsaved route changes.");
    UpdateCreatorReview();
}
void UHansaTradeMapPresentationModel::PublishIfChanged(const FHansaTradeMapSnapshot& Previous){Snapshot.bAnyCommandPending=IsAnyTradeCommandPending();const auto Schedule=GetSchedulePresentation();Snapshot.ScheduleKey=Schedule.Context.ToString()+Schedule.Evidence.ToString();for(const auto& Row:Schedule.Rows)Snapshot.ScheduleKey+=Row.Id+Row.Detail.ToString();RebuildDirectory();if(Snapshot==Previous)return;++Revision;Changed.Broadcast(Snapshot,Revision);}

FText UHansaTradeMapPresentationModel::GetOrderGoodLabel(FHansaGoodId Good) const
{
    const FString Id=Good.ToString();
    if(LastRegistry)if(const auto* Definition=LastRegistry->FindGood(Id))return FText::FromString(Definition->DisplayName);
    return FText::FromString(Id.StartsWith(TEXT("Good."))?Id.RightChop(5):Id);
}
void UHansaTradeMapPresentationModel::RefreshStationOrderText()
{
    RefreshStationOrderEditor();
    Snapshot.SelectedStationOrderId=static_cast<int64>(SelectedStationOrder);
    const auto Amount=[](int64 Milli){FString Value=FString::Printf(TEXT("%.3f"),double(Milli)/1000.);while(Value.EndsWith(TEXT("0")))Value.LeftChopInline(1);if(Value.EndsWith(TEXT(".")))Value.LeftChopInline(1);return Value;};
    Snapshot.StationOrderTargetInput=Amount(OrderDraft.TargetOrReserveMilliUnits);
    Snapshot.StationOrderCapInput=Amount(OrderDraft.CapMilliUnits);
    Snapshot.StationOrderBudgetPfennig=OrderDraft.TotalBudgetPfennig;
    StationOrderRows.Reset();
    if(Snapshot.TradeStationValue<=0) {
        Snapshot.StationOrderList=LOCTEXT("OrdersRequireStation","No station orders. Establish a station in the selected city to create one.");Snapshot.StationOrderListCompact=Snapshot.StationOrderList;
        Snapshot.StationOrderText=FText::Format(LOCTEXT("OrdersCityPrerequisite","Station orders in {0} require an active trade station. Open Presence to review the requirements."),CityLabel(Snapshot.SelectedCityStableId.ToString()));
        return;
    }
    FString Rows;FString CompactRows;
    for(const auto& Order:StationOrders) {
        const auto* Last=Order.History.IsEmpty()?nullptr:&Order.History.Last();
        const int64 Remaining=FMath::Max<int64>(0,Order.Terms.TotalBudgetPfennig-Order.SpentPfennig);
        const TCHAR* State=Order.bCancelled?TEXT("Cancelled"):Order.bPaused?TEXT("Paused"):Last&&Last->Outcome==EHansaStationOrderOutcome::Suspended?TEXT("Suspended"):Last&&Last->Outcome==EHansaStationOrderOutcome::Blocked?TEXT("Blocked"):Last&&Last->Outcome==EHansaStationOrderOutcome::Partial?TEXT("Partial"):TEXT("Active");
        const TCHAR* Result=Last?LexToString(Last->Outcome):TEXT("No execution yet");
        const TCHAR* Remedy=Order.bCancelled?TEXT("History only; create another order"):Order.bPaused?TEXT("Resume to run"):Last&&Last->Blocker!=EHansaStationOrderBlocker::None?LexToString(Last->Blocker):TEXT("Review order");
        FString Age=TEXT("unavailable");
        if(Runtime.IsValid()){
            const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());
            if(City){const auto Known=Runtime->QueryKnownMarketPrice(City.Value,Order.Terms.GoodId);if(Known&&Known->ReportAgeTicks.IsSet())Age=FString::Printf(TEXT("%lld ticks old"),Known->ReportAgeTicks.GetValue());}
        }else if(const auto* Market=RemoteMarkets.FindByPredicate([&](const auto& M){return M.CityId==Snapshot.SelectedCityStableId.ToString()&&M.GoodId==Order.Terms.GoodId.ToString()&&M.CurrentPriceMilliMarks>0;})){
            Age=FString::Printf(TEXT("%lld ticks old%s"),Market->ReportAgeTicks,Market->bStale?TEXT(" (stale)"):TEXT(""));
        }
        FHansaStationOrderRowPresentation& Row=StationOrderRows.AddDefaulted_GetRef();
        Row.Id=Order.Id;Row.GoodId=FName(*Order.Terms.GoodId.ToString());Row.Good=GetOrderGoodLabel(Order.Terms.GoodId);
        Row.Side=Order.Terms.Side==EHansaStationOrderSide::Acquire?LOCTEXT("RowAcquire","Acquire"):LOCTEXT("RowRelease","Release");
        Row.Target=FText::AsNumber(double(Order.Terms.TargetOrReserveMilliUnits)/1000.);
        Row.Cap=FText::AsNumber(double(Order.Terms.CapMilliUnits)/1000.);
        Row.Budget=Order.Terms.Side==EHansaStationOrderSide::Acquire?FText::Format(LOCTEXT("RowBudget","{0} pfennig"),FText::AsNumber(Remaining)):LOCTEXT("RowSaleBudget","No purchase budget");
        Row.State=FText::FromString(State);Row.ReportAge=FText::FromString(Age);
        Row.LastResult=FText::FromString(Result);Row.Remedy=FText::FromString(Remedy);
        const FString Label=GetOrderGoodLabel(Order.Terms.GoodId).ToString();
        const FString Budget=Order.Terms.Side==EHansaStationOrderSide::Acquire?FString::Printf(TEXT("%lld pfennig remaining"),Remaining):TEXT("Sale; no purchase budget");
        Rows+=FString::Printf(TEXT("%s #%llu · %s · %s\n  Target/reserve %.3f · cap %.3f/update · %s\n  %s · report %s · last %s · next: %s\n"),
            Order.Id==SelectedStationOrder?TEXT("▶"):TEXT(""),Order.Id,*Label,Order.Terms.Side==EHansaStationOrderSide::Acquire?TEXT("Acquire"):TEXT("Release"),
            double(Order.Terms.TargetOrReserveMilliUnits)/1000.,double(Order.Terms.CapMilliUnits)/1000.,*Budget,State,*Age,Result,Remedy);
        CompactRows+=FString::Printf(TEXT("%s#%llu %s · %s · %.3f target/reserve · %.3f/update · %s · %s · %s · %s · %s\n"),
            Order.Id==SelectedStationOrder?TEXT("▶"):TEXT(""),Order.Id,*Label,
            Order.Terms.Side==EHansaStationOrderSide::Acquire?TEXT("Acquire"):TEXT("Release"),
            double(Order.Terms.TargetOrReserveMilliUnits)/1000.,double(Order.Terms.CapMilliUnits)/1000.,
            *Budget,State,*Age,Result,Remedy);
    }
    Snapshot.StationOrderList=Rows.IsEmpty()?LOCTEXT("OrdersEmpty","No orders yet. Choose a good and create the first order."):FText::FromString(Rows);
    Snapshot.StationOrderListCompact=CompactRows.IsEmpty()?Snapshot.StationOrderList:FText::FromString(CompactRows);
    FText Report=LOCTEXT("OrderUnknownReport","Market report unavailable. Prices are sampled at execution.");
    if(Runtime.IsValid()&&OrderDraft.GoodId.IsValid()) {
        const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());
        if(City) {
            const auto Known=Runtime->QueryKnownMarketPrice(City.Value,OrderDraft.GoodId);
            if(Known && Known->PriceMilliMarks.IsSet() && Known->ReportAgeTicks.IsSet()) Report=FText::Format(LOCTEXT("OrderMarketReport","Reported price {0} milli-marks · {1} ticks old ({2}). Prices are sampled at execution."),FText::AsNumber(Known->PriceMilliMarks.GetValue()),FText::AsNumber(Known->ReportAgeTicks.GetValue()),FText::FromString(LexToString(Known->InformationState)));
        }
    }
    const auto* Selected=StationOrders.FindByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
    FText History=LOCTEXT("OrderNoHistory","No execution yet.");
    if(Selected) {
        FString Lines;
        for(const auto& E:Selected->History) Lines+=FString::Printf(TEXT("Tick %lld: %s · %lld milli-units · %+lld pfennig · %s\n"),E.Tick,LexToString(E.Outcome),E.AppliedMilliUnits,E.MoneyDelta,LexToString(E.Blocker));
        History=FText::Format(LOCTEXT("OrderHistory","Spent {0} pfennig · next update tick {1} · {2}\n{3}"),FText::AsNumber(Selected->SpentPfennig),FText::AsNumber(Selected->NextUpdateTick),Selected->bCancelled?LOCTEXT("OrderCancelled","Cancelled"):Selected->bPaused?LOCTEXT("OrderPaused","Paused"):LOCTEXT("OrderRunning","Enabled"),FText::FromString(Lines));
    }
    if(!bStationOrdersWritable)Report=LOCTEXT("OrderRightsUnavailable","Saving and resuming orders are unavailable: an active station, active presence and station-order rights are required. Existing orders can still be paused or cancelled.");
    const FText Limit=OrderDraft.LimitUnitPriceMilliMarks>0?FText::Format(LOCTEXT("OrderLimitActive","Price limit {0} milli-marks. Requires Market specialization and a current reviewed market update."),FText::AsNumber(OrderDraft.LimitUnitPriceMilliMarks)):LOCTEXT("OrderLimitNone","No price limit. Market specialization is required for a price-limited order; prices are sampled at execution.");
    Snapshot.StationOrderText=FText::Format(LOCTEXT("OrderDraft","{0} · {1} · {2}\nTarget / reserve: {3} units · cap: {4} units/update\nTotal purchase budget: {5} pfennig\n{6}\n{7}\n{8}"),Selected?FText::AsNumber(SelectedStationOrder):LOCTEXT("OrderNew","New order"),GetOrderGoodLabel(OrderDraft.GoodId),OrderDraft.Side==EHansaStationOrderSide::Acquire?LOCTEXT("OrderAcquire","Acquire to target"):LOCTEXT("OrderRelease","Sell down to reserve"),FText::AsNumber(double(OrderDraft.TargetOrReserveMilliUnits)/1000),FText::AsNumber(double(OrderDraft.CapMilliUnits)/1000),FText::AsNumber(OrderDraft.TotalBudgetPfennig),Limit,Report,History);
}
bool UHansaTradeMapPresentationModel::SelectStationOrder(uint64 Id)
{
    if(IsAnyTradeCommandPending())return false;
    if(Id!=0&&!StationOrders.ContainsByPredicate([&](const auto& O){return O.Id==Id;}))return false;
    const auto Previous=Snapshot;SelectedStationOrder=Id;Snapshot.bConfirmStationOrderCancel=false;
    if(const auto* Order=StationOrders.FindByPredicate([&](const auto& O){return O.Id==Id;}))OrderDraft=Order->Terms;
    else if(Id==0){OrderDraft={};if(!OrderGoods.IsEmpty())OrderDraft.GoodId=OrderGoods[0];OrderDraft.TargetOrReserveMilliUnits=FMath::Min<int64>(10000,StationOrderCapacity);OrderDraft.CapMilliUnits=FMath::Min<int64>(1000,StationOrderMaxCap);OrderDraft.TotalBudgetPfennig=FMath::Min<int64>(10000,StationOrderMaxBudget);}
    RefreshStationOrderText();PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::SelectStationOrderGood(FName Good) {
 if(!CanStationOrderAction(TEXT("Good")))return false;
 const auto Parsed=FHansaGoodId::TryParse(Good.ToString());if(!Parsed||!OrderGoods.Contains(Parsed.Value))return false;
 const auto Previous=Snapshot;OrderDraft.GoodId=Parsed.Value;Snapshot.bConfirmStationOrderCancel=false;
 RefreshStationOrderText();PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::SetStationOrderNumber(const FString& Field,const FString& Value)
{
    if(!CanStationOrderAction(Field+TEXT(".Increase"))&&!CanStationOrderAction(Field+TEXT(".Decrease")))return false;
    const auto ParseQuantity=[](FString Input,int64& Raw) {
        Input.TrimStartAndEndInline();
        FString Whole,Decimal;
        if(!Input.Split(TEXT("."),&Whole,&Decimal))Whole=Input;
        if(Whole.IsEmpty()||Decimal.Len()>3)return false;
        for(TCHAR C:Whole)if(!FChar::IsDigit(C))return false;
        for(TCHAR C:Decimal)if(!FChar::IsDigit(C))return false;
        int64 Units=0,Fraction=0;
        if(!LexTryParseString(Units,*Whole)||Units>MAX_int64/1000)return false;
        if(!Decimal.IsEmpty()){if(!LexTryParseString(Fraction,*Decimal))return false;for(int32 I=Decimal.Len();I<3;++I)Fraction*=10;}
        Raw=Units*1000+Fraction;return Raw>=0;
    };
    int64 Parsed=0;
    if(Field==TEXT("Budget")){if(!LexTryParseString(Parsed,*Value)||Parsed<0||Parsed>StationOrderMaxBudget)return false;}
    else if(!ParseQuantity(Value,Parsed))return false;
    const auto Previous=Snapshot;Snapshot.bConfirmStationOrderCancel=false;
    if(Field==TEXT("Target")){if(Parsed>StationOrderCapacity)return false;OrderDraft.TargetOrReserveMilliUnits=Parsed;}
    else if(Field==TEXT("Cap")){if(Parsed<1||Parsed>StationOrderMaxCap)return false;OrderDraft.CapMilliUnits=Parsed;}
    else if(Field==TEXT("Budget"))OrderDraft.TotalBudgetPfennig=Parsed;
    else return false;
    RefreshStationOrderText();PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::CanStationOrderAction(const FString& Action) const
{
    if(IsAnyTradeCommandPending()||!Snapshot.bOpen||Snapshot.bCreating||Snapshot.TradeStationValue<=0)return false;
    const auto* Selected=StationOrders.FindByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
    if(Action==TEXT("Select")||Action==TEXT("New")||Action==TEXT("Back"))return true;
    if(Action==TEXT("Cancel"))return Selected&&!Selected->bCancelled;
    if(Action==TEXT("Pause"))return Selected&&!Selected->bCancelled&&(!Selected->bPaused||bStationOrdersWritable);
    if(Action==TEXT("Save")&&!bStationOrdersWritable)return false;
    if(Selected&&Selected->bCancelled)return false;
    if(Action==TEXT("Good")||Action.StartsWith(TEXT("Choice."))||Action==TEXT("Side")||Action==TEXT("Buy")||Action==TEXT("Sell"))return !Selected&&!OrderGoods.IsEmpty();
    if(Action==TEXT("Target.Decrease"))return OrderDraft.TargetOrReserveMilliUnits>0;
    if(Action==TEXT("Target.Increase"))return OrderDraft.TargetOrReserveMilliUnits<StationOrderCapacity;
    if(Action==TEXT("Cap.Decrease"))return OrderDraft.CapMilliUnits>1;
    if(Action==TEXT("Cap.Increase"))return OrderDraft.CapMilliUnits<StationOrderMaxCap;
    if(Action.StartsWith(TEXT("Budget"))){if(OrderDraft.Side==EHansaStationOrderSide::Release)return false;if(Action==TEXT("Budget.Value"))return true;return Action.EndsWith(TEXT("Increase"))?OrderDraft.TotalBudgetPfennig<StationOrderMaxBudget:OrderDraft.TotalBudgetPfennig>0;}
    return OrderDraft.GoodId.IsValid();
}
bool UHansaTradeMapPresentationModel::StationOrderIntent(const FString& Action)
{
    if(!CanStationOrderAction(Action))return false;
    const auto Previous=Snapshot;
    if(Action!=TEXT("Cancel"))Snapshot.bConfirmStationOrderCancel=false;
    if(Action==TEXT("Cancel")&&!Snapshot.bConfirmStationOrderCancel){Snapshot.bConfirmStationOrderCancel=true;Snapshot.StationOrderFeedback=LOCTEXT("ConfirmOrderCancel","Cancel this order permanently? Activate Cancel again to confirm.");PublishIfChanged(Previous);return true;}
    if(Action==TEXT("Select")) {
        int32 Index=StationOrders.IndexOfByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
        ++Index; SelectedStationOrder=StationOrders.IsValidIndex(Index)?StationOrders[Index].Id:0;
        if(SelectedStationOrder)OrderDraft=StationOrders[Index].Terms;
    } else if(Action==TEXT("Good")) {
        if(SelectedStationOrder||OrderGoods.IsEmpty())return false;
        OrderDraft.GoodId=OrderGoods[(OrderGoods.IndexOfByKey(OrderDraft.GoodId)+1)%OrderGoods.Num()];
    } else if(Action==TEXT("Side")||Action==TEXT("Buy")||Action==TEXT("Sell")) {
        if(SelectedStationOrder)return false;
        OrderDraft.Side=Action==TEXT("Buy")?EHansaStationOrderSide::Acquire:Action==TEXT("Sell")?EHansaStationOrderSide::Release:OrderDraft.Side==EHansaStationOrderSide::Acquire?EHansaStationOrderSide::Release:EHansaStationOrderSide::Acquire;
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
        if(NetworkCommandIntent){FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::ManageStationOrder;I.TradeStationId=Snapshot.TradeStationValue;I.StationOrderId=P.OrderId;I.StationOrderAction=static_cast<uint8>(P.Action);I.GoodId=P.Terms.GoodId.ToString();I.StationOrderSide=static_cast<uint8>(P.Terms.Side);I.StationOrderTarget=P.Terms.TargetOrReserveMilliUnits;I.StationOrderCap=P.Terms.CapMilliUnits;I.StationOrderBudget=P.Terms.TotalBudgetPfennig;I.StationOrderLimitUnitPriceMilliMarks=P.Terms.LimitUnitPriceMilliMarks;I.StationOrderReviewedMarketUpdateTick=P.Terms.ReviewedMarketUpdateTick;I.StationOrderReviewedUnitPriceMilliMarks=P.Terms.ReviewedUnitPriceMilliMarks;bOrderPending=true;OrderSequence=OrderNonce=0;Sent=NetworkCommandIntent(I);if(!Sent)bOrderPending=false;}
        else if(Runtime.IsValid()) Sent=Runtime->ManageStationOrder(P).IsSuccess();
        Snapshot.bConfirmStationOrderCancel=false;if(!NetworkCommandIntent||!Sent||bOrderPending)Snapshot.StationOrderFeedback=Sent?LOCTEXT("OrderAccepted","Order submitted through the command gateway."):LOCTEXT("OrderRejected","Order rejected. Check station rights, report availability, policy limits and remaining budget; cancelled orders cannot be edited. Keep the same good and direction when editing.");
        if(Sent){SelectedStationOrder=P.OrderId;if(Runtime.IsValid()){const auto Projection=Runtime->BuildProjection();if(Projection)ApplyProjection(Projection.Value,*Runtime->GetEconomicRegistry());}}
        RefreshStationOrderText();PublishIfChanged(Previous);return Sent;
    }
    RefreshStationOrderText();PublishIfChanged(Previous);return true;
}


void UHansaTradeMapPresentationModel::RebuildDirectory()
{
 Snapshot.Directory.Reset();
 if(bRemoteEstablishment){RebuildRemoteDirectory();return;}
 if(!LastProjection)return;
 const auto House=Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse;
 auto Add=[&](FHansaTradeDirectoryEntry E) {
  if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Sea&&!E.bSea)return;
  if(Snapshot.ModeFilter==EHansaTradeMapModeFilter::Land&&E.bSea)return;
  if(Snapshot.CityFilter==EHansaTradeMapCityFilter::Presence&&!E.bPresence)return;
  if(Snapshot.CityFilter==EHansaTradeMapCityFilter::Routes&&E.RouteValue==0)return;
  const int32 F=Snapshot.DirectoryFilter;
  if((F==1&&!E.bActive)||(F==2&&(E.bActive||!E.RouteValue))||F==3||(F==4&&!E.bAttention)||(F==5&&!E.bAvailable)||(F==6&&!E.bPresence)||(F==7&&!E.Goods.Contains(Snapshot.PreferredGoodStableId)))return;
  if(!Snapshot.CitySearchText.IsEmpty()&&!E.Label.ToString().Contains(Snapshot.CitySearchText)&&!E.Detail.ToString().Contains(Snapshot.CitySearchText))return;
  Snapshot.Directory.Add(MoveTemp(E));
 };
 auto Make=[&](const FHansaVehicleProjection* V,const FHansaTradeMapRoutePresentation* R) {
  FHansaTradeDirectoryEntry E;
  E.RouteValue=R?R->RouteValue:0; E.VehicleValue=V?int64(V->Id.GetValue()):(R?R->VehicleValue:0);
  E.bOwned=V?House.IsValid()&&V->OwnerId==House:R&&R->bOwnedByPlayer;
  E.bSea=V?V->Mode==EHansaRouteMode::Sea:R&&R->bSea;
  E.bActive=R&&R->bActive; E.bAvailable=E.bOwned&&V&&!R&&!V->Navigation.IsMoving()&&V->Cargo.GetRawValue()==0;
  E.CityId=V?FName(*V->CurrentCityId.ToString()):NAME_None;
  const auto* City=AllCities.FindByPredicate([&](const auto& C){return C.StableId==E.CityId;});
  E.bPresence=City&&City->bHasPresence;
  E.SemanticId=Snapshot.bFleetView?FString::Printf(TEXT("TradeMap.Fleet.%lld"),E.VehicleValue):FString::Printf(TEXT("TradeMap.Route.%lld"),E.RouteValue);
  const FText Ship=FText::Format(LOCTEXT("DirectoryVehicle","{0} #{1}"),E.bSea?LOCTEXT("DirectoryCog","Cog"):LOCTEXT("DirectoryWagon","Wagon"),FText::AsNumber(E.VehicleValue));
  E.Label=Snapshot.bFleetView?Ship:(R?R->Label:FText());
  FText Cargo=LOCTEXT("DirectoryCargoPrivate","Cargo private");
  if(E.bOwned&&V)Cargo=FText::Format(LOCTEXT("DirectoryCargo","Cargo {0} / {1} units"),FText::AsNumber(double(V->Cargo.GetRawValue())/1000),FText::AsNumber(double(V->Capacity.GetRawValue())/1000));
  if(!V)Cargo=LOCTEXT("DirectoryMissingVehicle","Vehicle unavailable · request a fresh report");
  FText State=R?R->State:E.bAvailable?LOCTEXT("DirectoryAvailable","Available"):LOCTEXT("DirectoryUnassigned","Unassigned · availability unconfirmed");
  if(!R||!R->bTraveling)State=FText::Format(LOCTEXT("DirectoryArrivalUnknown","{0} · next arrival not reported"),State);
  FText Age=City?(City->bUnknown?LOCTEXT("DirectoryUnknownReport","Market report unknown"):FText::Format(LOCTEXT("DirectoryReportAge","{0} market report · {1} ticks old"),City->bStale?LOCTEXT("DirectoryStale","Stale"):LOCTEXT("DirectoryCurrent","Current"),FText::AsNumber(City->ReportAgeTicks))):LOCTEXT("DirectoryUnknownLocation","Location / report unknown");
  E.Detail=FText::Format(LOCTEXT("DirectoryDetail","{0} · {1}\n{2}\n{3}\n{4}\n{5}"),E.bOwned?LOCTEXT("DirectoryYours","Your house"):LOCTEXT("DirectoryOther","Other / unknown owner"),Snapshot.bFleetView?(R?R->Label:LOCTEXT("DirectoryNoRoute","No route assigned")):Ship,State,City?City->Label:LOCTEXT("DirectoryLocationUnknown","Location unknown"),Cargo,Age);
  if(R&&R->bOwnedByPlayer) {
   const auto* Raw=LastProjection->GetRoutes().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==R->RouteValue;});
   if(Raw) {
    for(const auto& Stop:Raw->Stops)for(const auto& A:Stop.Actions)E.Goods.AddUnique(FName(*A.GoodId.ToString()));
    if(Raw->LastTransfer.Outcome!=EHansaRouteTransferOutcome::None&&Raw->LastTransfer.AppliedQuantity<Raw->LastTransfer.RequestedQuantity) {
     E.bAttention=true;E.Alert=LOCTEXT("DirectoryMissed","Attention: partial / missed transfer. Review stock, reserves and destination capacity.");
    }
    for(const auto& Stop:Raw->Stops)if(const auto* C=AllCities.FindByPredicate([&](const auto& X){return X.StableId==FName(*Stop.CityId.ToString());})) {
     E.bPresence|=C->bHasPresence;
     if(!C->MapAlert.IsEmpty()){E.bAttention=true;E.Alert=C->MapAlert;}
    }
   }
  }
  if(!V){E.bAttention=true;E.Alert=LOCTEXT("DirectoryUnavailable","Unavailable: refresh the vehicle report before changing this route.");}
  return E;
 };
 if(Snapshot.bFleetView) {
  for(const auto& V:LastProjection->GetVehicles()) {
   const auto* R=AllRoutes.FindByPredicate([&](const auto& X){return X.VehicleValue==int64(V.Id.GetValue())&&!X.bCancelled;});
   Add(Make(&V,R));
  }
 } else for(const auto& R:AllRoutes)Add(Make(VehicleFor(*LastProjection,FHansaVehicleId::TryCreate(R.VehicleValue).Value),&R));
 if(Snapshot.bCreating&&!Snapshot.bFleetView&&(Snapshot.DirectoryFilter==0||Snapshot.DirectoryFilter==3)) {
  FHansaTradeDirectoryEntry Draft;Draft.SemanticId=TEXT("TradeMap.Directory.Draft");Draft.Label=FText::FromString(Snapshot.DraftName);Draft.bOwned=true;Draft.bSea=true;
  Draft.Detail=LOCTEXT("DirectoryDraftDetail","Your draft · not active\nReview stops, cargo instructions and ship assignment in the route editor.");Snapshot.Directory.Insert(MoveTemp(Draft),0);
 }
 Snapshot.Directory.StableSort([](const auto& A,const auto& B){return A.bAttention!=B.bAttention?A.bAttention>B.bAttention:A.SemanticId<B.SemanticId;});
}

bool UHansaTradeMapPresentationModel::DirectoryIntent(const FString& Action)
{
 if(Snapshot.bDirty&&!Snapshot.bCreating)return false;
 const auto Previous=Snapshot;
 if(Action==TEXT("Routes"))Snapshot.bFleetView=false;
 else if(Action==TEXT("Fleet"))Snapshot.bFleetView=true;
 else if(Action==TEXT("Filter"))Snapshot.DirectoryFilter=(Snapshot.DirectoryFilter+1)%8;
 else return false;
 RebuildDirectory();PublishIfChanged(Previous);return true;
}
bool UHansaTradeMapPresentationModel::SelectFleetIntent(int64 Value)
{
 if(Snapshot.SelectedVehicleValue==Value&&Value!=0){bShipDetailOpen=true;ShipDetailTab=TEXT("Overview");Changed.Broadcast(Snapshot,++Revision);Snapshot.FocusedSemanticId=FName(*FString::Printf(TEXT("TradeMap.Fleet.%lld"),Value));return true;}
 if(CargoEditor.bOpen||IsAnyTradeCommandPending()||Snapshot.bCreating||Snapshot.bDirty)return false;
 const auto& FleetEntries=bRemoteEstablishment?RemoteWorkspace.FleetDirectory:Snapshot.Directory;
 const auto* E=FleetEntries.FindByPredicate([&](const auto& X){return X.VehicleValue==Value;});
 FHansaTradeDirectoryEntry DirectEntry;
 if(!E&&LastProjection){if(const auto* V=LastProjection->GetVehicles().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==Value;})){
  DirectEntry.VehicleValue=Value;DirectEntry.CityId=FName(*V->CurrentCityId.ToString());DirectEntry.SemanticId=FString::Printf(TEXT("TradeMap.Fleet.%lld"),Value);
  if(const auto* R=AllRoutes.FindByPredicate([&](const auto& X){return X.VehicleValue==Value&&!X.bCancelled;}))DirectEntry.RouteValue=R->RouteValue;
  E=&DirectEntry;
 }}
 if(!E)return false;
 const auto Entry=*E; const auto Previous=Snapshot;
 if(Entry.RouteValue&&!SelectRouteIntent(Entry.RouteValue))return false;
 bShipDetailOpen=true;ShipDetailTab=TEXT("Overview");Snapshot.SelectedVehicleValue=Value;
 if(!Entry.RouteValue){Snapshot.SelectedRouteValue=0;DraftStops.Reset();Snapshot.Stops.Reset();Snapshot.bShipInTransit=false;Snapshot.SelectedCityStableId=Entry.CityId;Snapshot.EditorStatus=Entry.Detail;}
 if(!Entry.RouteValue&&LastProjection&&LastRegistry){auto Projection=LastProjection;ApplyProjection(*Projection,*LastRegistry);}
 if(bRemoteEstablishment&&!Entry.RouteValue){if(RemoteInterestRequested)RemoteInterestRequested(Snapshot.SelectedCityStableId);RefreshEstablishment();RefreshRemotePresence();}
 Snapshot.FocusedSemanticId=FName(*Entry.SemanticId);PublishIfChanged(Previous);return true;
}

#undef LOCTEXT_NAMESPACE

bool UHansaTradeMapPresentationModel::VisitSelectedStopIntent()
{
    if(bRemoteEstablishment){const auto Before=Snapshot;Snapshot.EditorStatus=RemoteActionReason(TEXT("TradeMap.Editor.Visit"));PublishIfChanged(Before);return false;}
    if(!Snapshot.bOpen||Snapshot.bCreating||!Snapshot.Stops.IsValidIndex(Snapshot.SelectedStopIndex)||!VisitRequested)return false;
    return VisitRequested(Snapshot.Stops[Snapshot.SelectedStopIndex].CityStableId);
}
