#include "UI/SHansaTradeMap.h"
#include "Trade/HansaTradeWorkspaceComponents.h"
#include "Trade/STradeConstruction.h"
#include "Trade/STradeDecisions.h"
#include "Trade/STradeRecovery.h"
#include "Trade/STradeShipDetail.h"
#include "Trade/STradeStationDetails.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "UI/SHansaReferenceFrame.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaTradeMap"

namespace Hansa::UI
{

	SHansaTradeMap::~SHansaTradeMap(){if(auto* P=Model.Get())P->OnChanged().Remove(ChangedHandle);}
	void SHansaTradeMap::Construct(const FArguments& A)
	{
		Model=A._Model;Preferences=A._Preferences;PresentationSize=A._InitialViewportSize;
        auto Context=MakeShared<FTradeComponentContext>();
        Context->Preferences=Preferences;Context->Model=Model;
        Context->Invoke=[Weak=TWeakPtr<SHansaTradeMap>(SharedThis(this))](const FString& Id){if(auto View=Weak.Pin())return View->Invoke(Id);return FReply::Unhandled();};
        Context->Register=[Weak=TWeakPtr<SHansaTradeMap>(SharedThis(this))](const FString& Id,TSharedPtr<SWidget> W){if(auto View=Weak.Pin())View->MapWidget(Id,W);};
        SAssignNew(Directory,STradeDirectory,Context);
        SAssignNew(RegionalMap,STradeRegionalMap,Context);
        SAssignNew(RouteEditor,STradeRouteEditor,Context);
        SAssignNew(Presence,STradePresence,Context);
        SAssignNew(Ledger,STradeLedger,Context);
        SAssignNew(Specialization,STradeSpecialization,Context);
        SAssignNew(Orders,STradeOrders,Context);
        SAssignNew(Decisions,STradeDecisions,Context);
        SAssignNew(Recovery,STradeRecovery,Context);
        SAssignNew(Construction,STradeConstruction,Context);
        SAssignNew(Feedback,STradeFeedback,Context);
        SAssignNew(Schedule,STradeSchedule,Context);
        const TArray<TSharedRef<SWidget>> Views={RouteEditor.ToSharedRef(),Presence.ToSharedRef(),SNullWidget::NullWidget,Orders.ToSharedRef(),Feedback.ToSharedRef(),Construction.ToSharedRef(),Decisions.ToSharedRef(),Recovery.ToSharedRef(),Presence->Footer.ToSharedRef()};
        SAssignNew(ContextHost,STradeContext,Context,Views);
        ContextHost->OrdersScroll=Orders->EditorScroll;Orders->SetPageScroll(ContextHost->OrdersScroll);
        SAssignNew(Shell,STradeShell,Context,Directory.ToSharedRef(),RegionalMap.ToSharedRef(),ContextHost.ToSharedRef(),Schedule.ToSharedRef());
        Shell->ContentViews.Add(Ledger.ToSharedRef());Shell->ContentViews.Add(Specialization.ToSharedRef());
        SAssignNew(ShipDetail,STradeShipDetail,Context);
        ChildSlot[SAssignNew(PresentationBox,SBox).WidthOverride(PresentationSize.X).HeightOverride(PresentationSize.Y)[SNew(SOverlay)+SOverlay::Slot()[Shell.ToSharedRef()]+SOverlay::Slot()[ShipDetail.ToSharedRef()]]];
        MapWidget(TEXT("TradeMap.Root"),SharedThis(this)); MapWidget(TEXT("TradeMap.Canvas"),RegionalMap->CanvasHost);MapWidget(TEXT("TradeMap.Schedule"),Schedule);MapWidget(TEXT("TradeMap.Workspace.Status"),Shell->WorkspaceStatus);MapWidget(TEXT("TradeMap.Selection.Summary"),ContextHost->SelectedRouteText);MapWidget(TEXT("TradeMap.Overview.Identity"),ContextHost->SelectedCityText);
		MapWidget(TEXT("TradeMap.City.Search"),Directory->CitySearchInput);
        MapWidget(TEXT("TradeMap.Creator.Name"),RouteEditor->RouteNameInput); MapWidget(TEXT("TradeMap.Creator.Cog"),RouteEditor->CogLabel->GetParentWidget());
        MapWidget(TEXT("TradeMap.Creator.ReviewText"),RouteEditor->ReviewText); MapWidget(TEXT("TradeMap.Creator.Validation"),Feedback->ValidationText);
        MapWidget(TEXT("TradeMap.Editor.RouteState"),RouteEditor->RouteStateCard); MapWidget(TEXT("TradeMap.Editor.ToggleActive"),RouteEditor->ToggleActiveButton);
		MapWidget(TEXT("TradeMap.Station.Action"),Presence->TradeStationButton);
		MapWidget(TEXT("TradeMap.Presence.Progress"),Presence->PresenceProgressText);MapWidget(TEXT("TradeMap.Presence.Upgrade"),Presence->PresenceUpgradeButton);MapWidget(TEXT("TradeMap.Presence.Source"),Presence->PresenceSourcePicker);MapWidget(TEXT("TradeMap.Presence.Cancel"),Presence->PresenceCancelButton);
        MapWidget(TEXT("TradeMap.Orders.Status"),Orders->StationOrderText);MapWidget(TEXT("TradeMap.Orders.List"),Orders->RowsPanel);

		if(auto* P=Model.Get()){ChangedHandle=P->OnChanged().AddSP(SharedThis(this),&SHansaTradeMap::Refresh);Refresh(P->GetSnapshot(),P->GetRevision());SetPresentationSize(PresentationSize);}
	}
	void SHansaTradeMap::SetPresentationSize(FIntPoint S){PresentationSize=S;if(ShipDetail)ShipDetail->SetViewportSize(S);Schedule->SetCompact(S.X<1500||S.Y<850||Preferences.bLargeText);if(PresentationBox){PresentationBox->SetWidthOverride(S.X);PresentationBox->SetHeightOverride(S.Y);}if(auto* P=Model.Get())P->SetCompact(S.X<1500||S.Y<850||Preferences.bLargeText);Shell->RefreshLayout(Model.IsValid()&&Model->GetSnapshot().bCompact,Model.IsValid()&&Model->bWorldStationDetail?TEXT("WorldStation"):Model.IsValid()?(Model->GetSnapshot().ActiveSection==TEXT("Ledger")||Model->GetSnapshot().ActiveSection==TEXT("Specialization")?Model->GetSnapshot().ActiveSection:Model->GetSnapshot().WorkspacePage):TEXT("Map"));}
	void SHansaTradeMap::Refresh(const FHansaTradeMapSnapshot& S,uint64)
	{
        const auto* SelectedCity=Model.IsValid()?Model->GetSelectedCityPresentation():nullptr;
        Shell->RefreshLayout(S.bCompact,Model->bWorldStationDetail?TEXT("WorldStation"):S.ActiveSection==TEXT("Ledger")||S.ActiveSection==TEXT("Specialization")?S.ActiveSection:S.WorkspacePage);
        Schedule->SetOrdersPreview(S.ActiveSection==TEXT("Orders"));
        Schedule->SetVisibility(!S.bCompact&&(S.ActiveSection==TEXT("Construction")||S.ActiveSection==TEXT("Decisions")||S.ActiveSection==TEXT("Recovery"))?EVisibility::Collapsed:EVisibility::Visible);
        Ledger->Refresh(Model->GetLedgerPresentation());
        ContextHost->RefreshOverview(SelectedCity);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Overview.Presence"))){StaticCastSharedPtr<SHansaAction>(W)->SetLabel(S.CityInspector.PrimaryAction);W->SetToolTipText(S.CityInspector.Issue);}
        ContextHost->SelectedCityText->SetText(SelectedCity?SelectedCity->Label:FText::FromName(S.SelectedCityStableId));
        if(auto Visit=ResolveSemanticWidget(TEXT("TradeMap.Editor.Visit"))){Visit->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);Visit->SetEnabled(!Model->IsRemoteView());Visit->SetToolTipText(Model->RemoteActionReason(TEXT("TradeMap.Editor.Visit")));}
        Shell->RouteTitle->SetText(S.Title);
        RouteEditor->EditPanel->SetVisibility(S.bReview?EVisibility::Collapsed:EVisibility::Visible);
        Orders->StationOrdersPanel->SetVisibility(EVisibility::Visible);
        Orders->StationOrderText->SetText(S.StationOrderText);Orders->SetCompact(S.bCompact);Orders->StationOrderList->SetText(S.bCompact?S.StationOrderListCompact:S.StationOrderList);Orders->RefreshRows(S);Orders->StationOrderFeedback->SetText(S.StationOrderFeedback);Orders->StationOrderFeedback->SetVisibility(S.StationOrderFeedback.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);Orders->OrderPicker->SetVisibility(S.TradeStationValue>0&&!S.bCompact?EVisibility::Visible:EVisibility::Collapsed);
        auto UpdateOrderInput=[](const TSharedPtr<SEditableTextBox>& Input,const FString& Number) {
            if(!Input)return;
            if(FSlateApplication::IsInitialized()&&FSlateApplication::Get().GetKeyboardFocusedWidget()==Input)return;
            const FText Value=FText::FromString(Number);
            if(!Input->GetText().EqualTo(Value))Input->SetText(Value);
        };
        UpdateOrderInput(Orders->TargetInput,S.StationOrderTargetInput);
        UpdateOrderInput(Orders->CapInput,S.StationOrderCapInput);
        UpdateOrderInput(Orders->BudgetInput,LexToString(S.StationOrderBudgetPfennig));
        if(const auto* P=Model.Get())for(const auto& Entry:SemanticWidgets)if(Entry.Key.StartsWith(TEXT("TradeMap.Orders."))&&Entry.Key!=TEXT("TradeMap.Orders.Status")&&Entry.Key!=TEXT("TradeMap.Orders.List")&&Entry.Key!=TEXT("TradeMap.Orders.Select")&&!Entry.Key.StartsWith(TEXT("TradeMap.Orders.Row.")))if(auto W=Entry.Value.Pin()){W->SetEnabled(P->CanStationOrderAction(Entry.Key.RightChop(16)));W->SetVisibility(S.TradeStationValue>0?EVisibility::Visible:EVisibility::Collapsed);}
        Orders->RefreshEditor(S);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Editor.Discard"))){W->SetVisibility(!S.bCreating&&S.bDirty?EVisibility::Visible:EVisibility::Collapsed);W->SetEnabled(!S.bAnyCommandPending);}
        Recovery->Refresh(S);Decisions->Refresh(S);Specialization->Refresh(S);Construction->Refresh(Model->GetConstructionPresentation());
        for(const TCHAR* Id:{TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good"),TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove")})
            if(auto W=ResolveSemanticWidget(Id))W->SetVisibility(Model.IsValid()&&Model->CanEditStops()?EVisibility::Visible:EVisibility::Collapsed);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Activate"))) W->SetEnabled(S.bCanCreate&&!S.bCommandPending&&!S.bDiscardConfirmation);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Keep")))W->SetVisibility(S.bDiscardConfirmation?EVisibility::Visible:EVisibility::Collapsed);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Discard"))) {StaticCastSharedPtr<SHansaAction>(W)->SetLabel(S.bDiscardConfirmation?LOCTEXT("ConfirmDiscard","Confirm discard"):LOCTEXT("DiscardDraft","Discard draft"));W->SetEnabled(!S.bCommandPending);}
        RouteEditor->SetupPanel->SetEnabled(!S.bCommandPending&&!S.bDiscardConfirmation);
        RouteEditor->EditPanel->SetEnabled(!S.bAnyCommandPending&&!S.bDiscardConfirmation);
        for(const TCHAR* Id:{TEXT("TradeMap.Editor.Action.Cycle"),TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("TradeMap.Editor.Quantity.Increase"),TEXT("TradeMap.Editor.Reserve.Decrease"),TEXT("TradeMap.Editor.Reserve.Increase"),TEXT("TradeMap.Editor.Stop.Up"),TEXT("TradeMap.Editor.Stop.Down")})if(auto W=ResolveSemanticWidget(Id))W->SetEnabled(Model->CanEditStops()&&!S.Stops.IsEmpty());
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Review")))W->SetEnabled(!S.bCommandPending&&!S.bDiscardConfirmation);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Edit")))W->SetEnabled(!S.bCommandPending&&!S.bDiscardConfirmation);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.New"))) {W->SetEnabled(!S.bCreating&&!S.bDirty&&!Model->IsRemoteView()&&!Model->IsAnyTradeCommandPending());W->SetToolTipText(Model->IsRemoteView()?Model->RemoteActionReason(TEXT("TradeMap.New")):S.bCreating?LOCTEXT("FinishDraft","Finish or discard this draft before choosing another route."):FText());}
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Mode.Filter"))) {W->SetEnabled(!S.bCreating);W->SetToolTipText(S.bCreating?LOCTEXT("FinishDraft","Finish or discard this draft before choosing another route."):FText());}
		for(const TCHAR* Id:{TEXT("TradeMap.City.Filter"),TEXT("TradeMap.Good.Filter"),TEXT("TradeMap.City.Search")})if(auto W=ResolveSemanticWidget(Id))W->SetEnabled(!S.bCreating);
		if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Route.Page.Previous")))W->SetEnabled(!S.bCreating&&S.RouteWindowStart>0);
		if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Route.Page.Next")))W->SetEnabled(!S.bCreating&&S.RouteWindowStart+S.Routes.Num()<S.MatchingRouteCount);
		const auto FocusedBefore=FSlateApplication::IsInitialized()?FSlateApplication::Get().GetKeyboardFocusedWidget():nullptr;
        const FString FocusId=S.FocusedSemanticId.ToString();const bool RestoreRow=FocusedBefore&&ResolveSemanticWidget(FocusId)==FocusedBefore;
        RebuildRoutes(S);RebuildStops(S);
        if(RestoreRow&&(FocusId.StartsWith(TEXT("TradeMap.Stop."))||FocusId.StartsWith(TEXT("TradeMap.Route."))))if(auto W=ResolveSemanticWidget(FocusId))if(W!=FocusedBefore)FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);RegionalMap->Refresh({S.Cities,S.Stops,S.Routes,S.SelectedRouteValue,S.PreferredGoodStableId,S.bCreating,S.SelectedCityStableId,S.ShipPosition,S.bShipInTransit});
		const auto* R=Model.IsValid()?Model->GetSelectedRoutePresentation():nullptr;
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Editor.Cancel")))W->SetEnabled(R&&R->bCanCancel);
        Schedule->Refresh(Model.IsValid()?Model->GetSchedulePresentation():FHansaTradeSchedulePresentation());
        Presence->Refresh({S.TradeStationState,S.TradeStationDetail,S.TradeStationAction,S.PresenceProgress,S.PresenceUpgradeAction,S.bCanTradeStationAction,S.bCanPresenceUpgradeAction});
        Directory->Refresh({S.CitySearchText,S.ModeFilter,S.CityFilter,S.MatchingCityCount,S.PreferredGoodStableId,S.bCreating,S.bCompact,S.RouteWindowStart>0||S.RouteWindowStart+S.Routes.Num()<S.MatchingRouteCount});
        Feedback->Refresh({S.bCreating,S.bReview,S.Validation});
        for(const TCHAR* Field:{TEXT("TradeMap.Creator.Name"),TEXT("TradeMap.Creator.Cog"),TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Editor.Action.Cycle"),TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("TradeMap.Editor.Reserve.Decrease")})
            if(auto W=ResolveSemanticWidget(Field))W->SetToolTipText(S.bCreating&&S.ValidationTarget==FName(Field)&&!S.bCanCreate?S.Validation:FText());
        RouteEditor->Refresh({S.DraftName,S.bCreating,S.bReview,S.CogLabel,S.CreatorReview,S.ReserveRisk,S.EditorStatus}, R);
        if(Model->IsRemoteView()&&S.bDirty&&R&&!R->bActive){RouteEditor->ToggleActiveButton->SetEnabled(false);RouteEditor->ToggleActiveHint->SetText(LOCTEXT("SaveBeforeRemoteStart","Save changes and await the server acknowledgement before starting."));}

        if(!R&&S.SelectedVehicleValue) {
            if(const auto* Entry=S.Directory.FindByPredicate([&](const auto& E){return E.VehicleValue==S.SelectedVehicleValue;})) {
                RouteEditor->RouteMetrics->SetText(Entry->Detail);
                RouteEditor->RouteStateDetail->SetText(Entry->Alert);
                Schedule->ScheduleText->SetText(LOCTEXT("UnassignedSchedule","No route assigned. Create a route to schedule this vehicle."));
            }
        }
        ContextHost->SelectedRouteText->SetText(S.bCreating?FText::FromString(S.DraftName):FText::Format(LOCTEXT("CityIdentityStageCompact","{0} · {1}"),S.CityInspector.Identity,S.CityInspector.Presence));
        ApplySectionVisibility();
        if(Model->bWorldStationDetail){
            const bool Lease=S.ActiveSection==TEXT("Presence")&&S.Establishment.bComplete&&Presence->Details->IsLeaseOpen();
            ContextHost->SelectedCityText->SetText(Lease?LOCTEXT("WorldLeaseTitle","Lease terms & trading rights"):FText::Format(S.Establishment.bOfficeBuilt?LOCTEXT("WorldOfficeTitle","Merchant Office · {0}"):LOCTEXT("WorldStationTitle","Trade station · {0}"),S.Establishment.CityLabel));
            ContextHost->SelectedRouteText->SetText(FText::Format(LOCTEXT("StationState","{0} · {1}"),S.Establishment.CityLabel,S.Establishment.OperationalLabel));
            if(S.Establishment.bComplete)MapWidget(TEXT("TradeMap.Station.Identity"),ContextHost->SelectedCityText);
        }
        RefreshWorkspaceStatus();ShipDetail->Refresh();Shell->SetVisibility(Model->bShipDetailOpen?EVisibility::Hidden:EVisibility::Visible);
	}
    void SHansaTradeMap::ApplySectionVisibility()
    {
        const auto* P=Model.Get();if(!P)return;const auto& S=P->GetSnapshot();
        const FString ActiveSection=S.bCreating?TEXT("Route"):S.ActiveSection;
        ContextHost->ShowSection(ActiveSection,S.bCreating);
        const FString VisibleSection=ActiveSection==TEXT("Route")?TEXT("Overview"):ActiveSection;
        auto Show=[](bool Visible){return Visible?EVisibility::Visible:EVisibility::Collapsed;};
        RouteEditor->EditPanel->SetVisibility(Show(!S.bReview&&ActiveSection==TEXT("Route")));
        Presence->PresencePanel->SetVisibility(Show(!S.bCreating&&(ActiveSection==TEXT("Presence")||ActiveSection==TEXT("StationUpgrade"))));
        Specialization->SpecializationPanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Specialization")));
        Orders->StationOrdersPanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Orders")));
        RouteEditor->RouteDetailsPanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Route")));
        for(const TCHAR* Section:{TEXT("Overview"),TEXT("Route"),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders"),TEXT("Ledger"),TEXT("Construction"),TEXT("Decisions"),TEXT("Recovery")})
            if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Navigate.")+FString(Section)))StaticCastSharedPtr<SHansaAction>(W)->SetState(VisibleSection==Section?EUiState::Selected:EUiState::Default);
    }
 void SHansaTradeMap::RebuildRoutes(const FHansaTradeMapSnapshot& S){Directory->RefreshRows(S);}
 void SHansaTradeMap::RebuildStops(const FHansaTradeMapSnapshot& S){RouteEditor->RefreshStops(S.Stops,S.SelectedStopIndex);}
	FReply SHansaTradeMap::Invoke(const FString Id){ActivateSemanticId(Id);return FReply::Handled();}
    bool SHansaTradeMap::ActivateSemanticId(const FString& Id) {
        if(!Id.StartsWith(TEXT("TradeMap.")))return false;
        if(Id.StartsWith(TEXT("TradeMap.Editor."))||Id.StartsWith(TEXT("TradeMap.Stop."))||Id.StartsWith(TEXT("TradeMap.Creator.")))return false;
        if(ShipDetail&&(Id.StartsWith(TEXT("TradeMap.Cargo."))||Id.StartsWith(TEXT("TradeMap.Ship."))))return ShipDetail->Dispatch(Id);
        const bool Accepted=DispatchIntent(Id);
        FeedbackKind=Accepted?FString():TEXT("error");
        WorkspaceFeedback=Accepted?FText():FText::Format(LOCTEXT("WorkspaceRejected","Action unavailable. Check the selected workflow requirements and route state. {0}"),Model.IsValid()?Model->GetSnapshot().EditorStatus:FText());
        RefreshWorkspaceStatus();return Accepted;
    }
    void SHansaTradeMap::RefreshWorkspaceStatus() {
        if(!Model.IsValid())return;
        const auto& S=Model->GetSnapshot();const auto* City=Model->GetSelectedCityPresentation();
        FText Text=WorkspaceFeedback;FString Kind=WorkspaceFeedback.IsEmpty()?FString():TEXT("error");
        if(!S.bHasProjection){Kind=TEXT("loading");Text=LOCTEXT("WorkspaceLoading","Loading trade data. Waiting for the authoritative projection.");}
        else if(Text.IsEmpty()&&!City){Kind=TEXT("unsupported");Text=LOCTEXT("WorkspaceUnsupported","City data unavailable. Select a supported city on the map.");}
        else if(Text.IsEmpty()&&City->bUnknown){Kind=TEXT("unknown");Text=LOCTEXT("WorkspaceUnknown","Market report unavailable. Visit this city to obtain current information.");}
        else if(Text.IsEmpty()&&City->bStale){Kind=TEXT("stale");Text=FText::Format(LOCTEXT("WorkspaceStale","Older market report ({0} ticks). Visit the city to refresh it; future prices are not guaranteed."),FText::AsNumber(City->ReportAgeTicks));}
        else if(Text.IsEmpty()&&City->bMarketOnly){Kind=TEXT("unsupported");Text=City->CapabilitySummary;}
        else if(Text.IsEmpty()&&S.Routes.IsEmpty()&&!S.bCreating){Kind=TEXT("empty");Text=LOCTEXT("WorkspaceEmpty","No matching routes. Change the route filter or create a route.");}
        Shell->WorkspaceStatus->SetText(Text);Shell->WorkspaceStatus->SetVisibility(Text.IsEmpty()||(S.bCompact&&S.WorkspacePage!=TEXT("Map")&&Kind!=TEXT("error"))?EVisibility::Collapsed:EVisibility::Visible);
        FeedbackKind=Kind;
    }
	bool SHansaTradeMap::DispatchIntent(const FString& Id){auto* P=Model.Get();if(!P||!P->GetSnapshot().bOpen)return false;
        if(Id==TEXT("TradeMap.Presence.Privilege")||Id==TEXT("TradeMap.Presence.Project")||Id==TEXT("TradeMap.Presence.Governance"))return DispatchIntent(TEXT("TradeMap.Navigate.Decisions"));
        if(Id.StartsWith(TEXT("TradeMap.Recovery."))){FString A=Id.RightChop(18);if(A.StartsWith(TEXT("Item.")))A=A.RightChop(5);const bool Done=P->RecoveryIntent(A);if(Done){if(A==TEXT("Review"))FocusSemanticId(TEXT("TradeMap.Recovery.Confirm"));else if(A==TEXT("Cancel"))FocusSemanticId(TEXT("TradeMap.Recovery.Review"));else if(P->GetSnapshot().ActiveSection==TEXT("Recovery"))FocusSemanticId(TEXT("TradeMap.Recovery.Detail"));}return Done;}
        if(Id.StartsWith(TEXT("TradeMap.Decisions."))){const FString A=Id.RightChop(19);const bool Done=P->DecisionIntent(A);if(Done){if(A==TEXT("Review"))FocusSemanticId(TEXT("TradeMap.Decisions.Confirm"));else if(A==TEXT("Confirm")||A==TEXT("Cancel"))FocusSemanticId(TEXT("TradeMap.Decisions.")+P->GetSnapshot().DecisionId);}return Done;}
        if(Id.StartsWith(TEXT("TradeMap.Construction.")))return P->ConstructionIntent(Id.RightChop(22));
        if(Id.StartsWith(TEXT("TradeMap.Ledger."))){const bool Done=Ledger->Intent(Id.RightChop(16));if(Done){if(Id==TEXT("TradeMap.Ledger.Back"))FocusSemanticId(TEXT("TradeMap.Navigate.Ledger"));else if(Id.StartsWith(TEXT("TradeMap.Ledger.Good.")))FocusSemanticId(TEXT("TradeMap.Ledger.Detail"));}return Done;}
        if(Id==TEXT("TradeMap.City.More")){ContextHost->SectionMenu->SetIsOpen(true);return true;}
        if(Id==TEXT("TradeMap.City.Close"))return DispatchIntent(TEXT("TradeMap.Close"));
        if(Id==TEXT("TradeMap.City.Locate")&&P->bWorldStationDetail)return P->EstablishmentIntent(TEXT("ShowOnMap"));
        if(Id==TEXT("TradeMap.City.Locate"))return RegionalMap->FocusCity(P->GetSnapshot().SelectedCityStableId);
        if(Id==TEXT("TradeMap.Overview.Refresh"))return DispatchIntent(TEXT("TradeMap.Overview.Market"));
        if(Id==TEXT("TradeMap.Directory.More")){Directory->MoreMenu->SetIsOpen(true);return true;}
        if(Id==TEXT("TradeMap.Chart.Tools")){RegionalMap->ToolsMenu->SetIsOpen(true);return true;}
        if(Id.StartsWith(TEXT("TradeMap.Port.")))return P->SelectCityIntent(FName(*Id.RightChop(14)));
        if(Id==TEXT("TradeMap.Overview.Market"))return P->MarketRequested&&P->MarketRequested(P->GetSnapshot().SelectedCityStableId,P->GetSnapshot().PreferredGoodStableId);
        if(Id==TEXT("TradeMap.Overview.Presence")){
            if(P->GetSnapshot().CityInspector.PrimarySection==TEXT("Route")){
                const auto* City=P->GetSelectedCityPresentation();if(!City)return false;
                const FString Name=City->Label.ToString();P->DirectoryIntent(TEXT("Routes"));P->SetCitySearchIntent(Name);return P->SelectWorkspacePageIntent(TEXT("Routes"));
            }
            return P->SelectSectionIntent(P->GetSnapshot().CityInspector.PrimarySection);
        }
        if(Id==TEXT("TradeMap.Schedule.ChooseOwned")){for(const auto& Entry:P->GetSnapshot().Directory)if(Entry.bOwned){const bool Result=Entry.RouteValue?P->SelectRouteIntent(Entry.RouteValue):P->SelectFleetIntent(Entry.VehicleValue);if(Result)P->SelectSectionIntent(TEXT("Overview"));return Result;}P->SelectWorkspacePageIntent(TEXT("Routes"));return true;}
        if(Id==TEXT("TradeMap.Schedule.OpenJourney")){
            if(Schedule->View.RouteId)return P->SelectRouteIntent(Schedule->View.RouteId);
            if(P->GetSnapshot().SelectedVehicleValue){P->SelectSectionIntent(TEXT("Route"));return P->bShipDetailOpen;}
            return P->SelectWorkspacePageIntent(TEXT("Routes"));
        }
        if(Id.StartsWith(TEXT("TradeMap.Schedule.Tab."))){const bool Result=Schedule->SelectTab(Id.RightChop(22));if(Result)FocusSemanticId(Id);return Result;}
        if(Id.StartsWith(TEXT("TradeMap.Schedule.")))return P->SelectScheduleRowIntent(Id);
        if(Id.StartsWith(TEXT("TradeMap.Page."))) { const bool Result=P->SelectWorkspacePageIntent(Id.RightChop(14));if(Result)FocusSemanticId(Id);return Result; }
        if(Id==TEXT("TradeMap.Back")) {
            if(P->GetSnapshot().ActiveSection==TEXT("Recovery")&&P->GetSnapshot().bRecoveryReview)return DispatchIntent(TEXT("TradeMap.Recovery.Cancel"));
            if(P->GetSnapshot().ActiveSection==TEXT("Decisions")&&P->GetSnapshot().bDecisionReview)return DispatchIntent(TEXT("TradeMap.Decisions.Cancel"));
            if(P->GetSnapshot().ActiveSection==TEXT("Specialization"))return DispatchIntent(TEXT("TradeMap.Presence.Specialization.Back"));
            if(P->GetSnapshot().ActiveSection==TEXT("Orders")&&Orders->IsEditor())return DispatchIntent(TEXT("TradeMap.Orders.Back"));
            if(P->GetSnapshot().ActiveSection==TEXT("Ledger")){P->SelectSectionIntent(TEXT("Presence"));return FocusSemanticId(P->bWorldStationDetail?TEXT("TradeMap.WorldStation.Tab.Details"):TEXT("TradeMap.Station.Overview.Stock"));}
            if(P->GetSnapshot().Establishment.bReview)return P->EstablishmentIntent(TEXT("Cancel"));
            if(P->GetSnapshot().bPresenceReview)return DispatchIntent(TEXT("TradeMap.Presence.Cancel"));
            if(P->GetSnapshot().Establishment.bComplete&&Presence->Details->IsLeaseOpen())return DispatchIntent(TEXT("TradeMap.Station.Return"));
            if(P->bWorldStationDetail)return P->GetSnapshot().ActiveSection!=TEXT("Presence")?DispatchIntent(TEXT("TradeMap.WorldStation.Tab.Details")):P->CloseIntent();
            if(P->GetSnapshot().bCompact&&P->GetSnapshot().WorkspacePage!=TEXT("Map")) {const FString Origin=TEXT("TradeMap.Page.")+P->GetSnapshot().WorkspacePage;P->SelectWorkspacePageIntent(TEXT("Map"));FocusSemanticId(Origin);return true;}
            return P->CloseIntent();
        }
        if(Id.StartsWith(TEXT("TradeMap.WorldStation.Tab."))||Id.StartsWith(TEXT("TradeMap.Station.Tab."))) {
            if((Id.StartsWith(TEXT("TradeMap.WorldStation.Tab."))&&!P->bWorldStationDetail)||P->GetSnapshot().bCreating)return false;
            if(!Id.EndsWith(TEXT("Details"))&&!Id.EndsWith(TEXT("Overview"))&&!Id.EndsWith(TEXT("Orders"))&&!Id.EndsWith(TEXT("Upgrade")))return false;
            const bool ShowOrders=Id.EndsWith(TEXT("Orders"));
            if(!ShowOrders)Orders->CloseGoodMenu();
            Presence->Details->CloseTerms();
            if(!P->SelectSectionIntent(ShowOrders?TEXT("Orders"):Id.EndsWith(TEXT("Upgrade"))?TEXT("StationUpgrade"):TEXT("Presence")))return false;
            Presence->Details->Refresh();ApplySectionVisibility();ContextHost->PresenceScroll->ScrollToStart();return FocusSemanticId(Id);
        }
        if(Id.StartsWith(TEXT("TradeMap.Navigate.")))
        {
            if(P->GetSnapshot().bCreating||!ContextHost)return false;
            const FString Section=Id.RightChop(18);
            if(Section==TEXT("Route")){
                if(P->GetSnapshot().SelectedVehicleValue){P->SelectSectionIntent(TEXT("Route"));return P->bShipDetailOpen;}
                return P->SelectWorkspacePageIntent(TEXT("Routes"));
            }
            if(Section!=TEXT("Overview")&&Section!=TEXT("Route")&&Section!=TEXT("Presence")&&Section!=TEXT("Specialization")&&Section!=TEXT("Orders")&&Section!=TEXT("Ledger")&&Section!=TEXT("Construction")&&Section!=TEXT("Decisions")&&Section!=TEXT("Recovery"))return false;
            P->SelectWorkspacePageIntent(TEXT("Workspace"));P->SelectSectionIntent(Section);ApplySectionVisibility();
            if(Section==TEXT("Presence"))ContextHost->PresenceScroll->ScrollToStart();
            if(Section==TEXT("Specialization")){Specialization->Scroll->ScrollToStart();return FocusSemanticId(TEXT("TradeMap.Presence.Specialization.Back"));}
            if(Section==TEXT("Orders")||Section==TEXT("Construction")||Section==TEXT("Decisions")||Section==TEXT("Recovery")||Section==TEXT("Ledger")){ContextHost->SectionMenu->SetIsOpen(false);FocusSemanticId(TEXT("TradeMap.City.More"));}
            else FocusSemanticId(Id);
            return true;
        }
        if(Id==TEXT("TradeMap.Selection.PreviousCity"))return P->CycleCityIntent(-1);
        if(Id==TEXT("TradeMap.Selection.NextCity"))return P->CycleCityIntent(1);
        if(Id==TEXT("TradeMap.Chart.Overlay")){RegionalMap->CycleOverlay();return true;}
        if(Id==TEXT("TradeMap.Chart.Thickness")){RegionalMap->CycleThickness();return true;}
        if(Id==TEXT("TradeMap.Chart.NextRoute"))return RegionalMap->NextRoute(1);
        if(Id==TEXT("TradeMap.Chart.PreviousCity")||Id==TEXT("TradeMap.Chart.NextCity")){const bool Result=P->CycleCityIntent(Id.EndsWith(TEXT("NextCity"))?1:-1);if(Result)RegionalMap->FocusCity(P->GetSnapshot().SelectedCityStableId);return Result;}
        if(Id==TEXT("TradeMap.Chart.Focus")||Id==TEXT("TradeMap.Chart.CanvasFocus")){P->SetFocusedSemanticId(TEXT("TradeMap.Chart.CanvasFocus"));FSlateApplication::Get().SetKeyboardFocus(RegionalMap->FocusWidget(),EFocusCause::Navigation);return true;}
        if(Id==TEXT("TradeMap.Chart.ZoomIn")){RegionalMap->ChangeZoom(.5f);return true;}
        if(Id==TEXT("TradeMap.Chart.ZoomOut")){RegionalMap->ChangeZoom(-.5f);return true;}
        if(Id==TEXT("TradeMap.Chart.Reset")){RegionalMap->ResetView();return true;}
        if(Id.StartsWith(TEXT("TradeMap.City.City_")))return P->SelectCityIntent(FName(*Id.RightChop(14).Replace(TEXT("_"),TEXT("."))));
        if(Id==TEXT("TradeMap.Editor.Cancel"))
        {
            const bool Result=P->CancelRouteIntent();
            if(Result){FocusSemanticId(TEXT("TradeMap.New"));ContextHost->RouteEditorScroll->ScrollToEnd();}
            return Result;
        }
        if(Id==TEXT("TradeMap.Editor.Discard"))return P->DiscardRouteEditsIntent();
        if(Id==TEXT("TradeMap.Editor.Visit"))return P->VisitSelectedStopIntent();
		if(Id.StartsWith(TEXT("TradeMap.Orders.Row."))){
            uint64 RowId=0;
            if(!LexTryParseString(RowId,*Id.RightChop(20))||!P->SelectStationOrder(RowId))return false;
            Orders->ShowEditor(true);return true;
        }
        if(Id==TEXT("TradeMap.Orders.New")){if(!P->SelectStationOrder(0))return false;Orders->ShowEditor(true);ContextHost->OrdersScroll->ScrollToStart();return true;}
        if(Id==TEXT("TradeMap.Orders.Back")){Orders->ShowEditor(false);ContextHost->OrdersScroll->ScrollToStart();return FocusSemanticId(TEXT("TradeMap.Orders.New"));}
        if(Id==TEXT("TradeMap.Orders.Good"))return Orders->OpenGoodMenu();
        if(Id.StartsWith(TEXT("TradeMap.Orders.Choice."))){const bool Result=P->SelectStationOrderGood(FName(*Id.RightChop(23)));if(Result)Orders->CloseGoodMenu();return Result;}
        if(Id.StartsWith(TEXT("TradeMap.Orders."))){const bool Result=P->StationOrderIntent(Id.RightChop(16));if(Result&&Id==TEXT("TradeMap.Orders.Select")){Orders->ShowEditor(true);ContextHost->OrdersScroll->ScrollToStart();}return Result;}
        if(Id==TEXT("TradeMap.Station.Overview.Stock")){if(!P->GetLedgerPresentation().bAvailable)return false;P->bWorldStationDetail=false;if(!P->SelectSectionIntent(TEXT("Ledger")))return false;return FocusSemanticId(TEXT("TradeMap.Ledger.Stock"));}
        if(Id==TEXT("TradeMap.Station.Overview.Orders"))return P->bWorldStationDetail?DispatchIntent(TEXT("TradeMap.WorldStation.Tab.Orders")):DispatchIntent(TEXT("TradeMap.Navigate.Orders"));
        if(Id==TEXT("TradeMap.Station.Terms")||Id==TEXT("TradeMap.Station.Return")){
            if(!P->CanEstablishmentIntent(TEXT("Terms")))return false;
            if(!P->GetSnapshot().Establishment.bComplete)return Presence->Establishment->ToggleTerms();
            Presence->Details->ToggleTerms();Refresh(P->GetSnapshot(),P->GetRevision());ContextHost->PresenceScroll->ScrollToStart();
            FocusSemanticId(Presence->Details->IsLeaseOpen()?TEXT("TradeMap.Station.Return"):TEXT("TradeMap.Station.Terms"));return true;
        }
        if(Id==TEXT("TradeMap.Station.Market"))return P->GetSnapshot().Establishment.bCanOpenMarket&&!P->IsAnyTradeCommandPending()&&DispatchIntent(TEXT("TradeMap.Overview.Market"));
        if(Id==TEXT("TradeMap.Station.Action"))return P->GetSnapshot().Establishment.bComplete&&P->GetSnapshot().Establishment.bArrears?P->OpenRecovery(P->GetSnapshot().SelectedCityStableId,P->GetSnapshot().Establishment.StationId,FName(*Id)):P->TradeStationActionIntent();
        if(Id.StartsWith(TEXT("TradeMap.Station.")))return P->EstablishmentIntent(Id.RightChop(17));
		if(Id.StartsWith(TEXT("TradeMap.Presence.Locate."))){const FString Requirement=Id.RightChop(25);return DispatchIntent(Requirement.StartsWith(TEXT("UpgradeGood."))||Requirement==TEXT("AvailableMoney")?TEXT("TradeMap.Navigate.Ledger"):TEXT("TradeMap.Navigate.Route"));}if(Id==TEXT("TradeMap.Presence.Source"))return P->PresenceSourceIntent();if(Id==TEXT("TradeMap.Presence.Cancel")){const bool Result=P->PresenceCancelReviewIntent();if(Result)FocusSemanticId(TEXT("TradeMap.Presence.Upgrade"));return Result;}if(Id==TEXT("TradeMap.Presence.Upgrade")){if(P->GetSnapshot().Establishment.bComplete&&P->GetSnapshot().ActiveSection!=TEXT("StationUpgrade"))return false;const bool Result=P->PresenceUpgradeActionIntent();if(Result&&P->GetSnapshot().bPresenceReview){FocusSemanticId(TEXT("TradeMap.Presence.Upgrade"));if(P->GetSnapshot().Establishment.bComplete){ContextHost->PresenceScroll->ScrollDescendantIntoView(nullptr,false);ContextHost->PresenceScroll->ScrollToStart();}}return Result;}if(Id.StartsWith(TEXT("TradeMap.Presence.Specialization."))){const FString Action=Id.RightChop(33);const bool Result=P->PresenceSpecializationIntent(Action);if(Result){if(Action==TEXT("Apply")||Action==TEXT("Review")){Specialization->Scroll->ScrollToStart();FocusSemanticId(TEXT("TradeMap.Presence.Specialization.Confirm"));}else if(Action==TEXT("Confirm")||Action==TEXT("Cancel"))FocusSemanticId(TEXT("TradeMap.Presence.Specialization.")+P->GetSnapshot().SelectedPresenceSpecializationId);else if(Action==TEXT("Back"))FocusSemanticId(TEXT("TradeMap.Navigate.Specialization"));}return Result;}
        if(Id==TEXT("TradeMap.New")){const bool Result=P->BeginCreateIntent();if(Result)FocusSemanticId(TEXT("TradeMap.Ship.RouteName"));return Result;}
        if(Id==TEXT("TradeMap.Creator.Cog"))return P->CycleCogIntent();
        if(Id==TEXT("TradeMap.Creator.City"))return P->CycleStopCityIntent();
        if(Id==TEXT("TradeMap.Creator.Good"))return P->CycleStopGoodIntent();
        if(Id==TEXT("TradeMap.Creator.Add"))return P->AddStopIntent();
        if(Id==TEXT("TradeMap.Creator.Remove"))return P->RemoveStopIntent();
        if(Id==TEXT("TradeMap.Creator.Keep")){const bool R=P->KeepDraftIntent();if(R)FocusSemanticId(TEXT("TradeMap.Creator.Discard"));return R;}
        if(Id==TEXT("TradeMap.Creator.Discard")){const bool R=P->DiscardCreateIntent();if(R)FocusSemanticId(P->GetSnapshot().bDiscardConfirmation?TEXT("TradeMap.Creator.Keep"):TEXT("TradeMap.New"));return R;}
        if(Id==TEXT("TradeMap.Creator.Review")){const bool Result=P->ReviewCreateIntent();if(Result)FocusSemanticId(P->GetSnapshot().bCanCreate?TEXT("TradeMap.Creator.Activate"):TEXT("TradeMap.Creator.Edit"));return Result;}
        if(Id==TEXT("TradeMap.Creator.Edit")){const bool Result=P->EditCreateIntent();if(Result)FocusSemanticId(P->GetSnapshot().bCanCreate?TEXT("TradeMap.Creator.Name"):P->GetSnapshot().ValidationTarget.ToString());return Result;}
        if(Id==TEXT("TradeMap.Creator.Activate"))return P->CreateAndActivateIntent();
        if(Id.StartsWith(TEXT("TradeMap.Fleet."))){const auto Before=P->GetSnapshot().SelectedVehicleValue;const bool Result=P->SelectFleetIntent(FCString::Atoi64(*Id.RightChop(15)));if(Result&&Before!=P->GetSnapshot().SelectedVehicleValue)RegionalMap->FrameSelection();return Result;}
        if(Id.StartsWith(TEXT("TradeMap.Directory."))) {
            const FString Action=Id.RightChop(19);
            if(Action==TEXT("Draft")){P->SelectWorkspacePageIntent(TEXT("Workspace"));return true;}
            if(Action==TEXT("Toggle"))return P->ToggleActiveIntent();
            if(Action==TEXT("Cancel"))return P->CancelRouteIntent();
            if(Action==TEXT("Recovery"))return P->OpenRecovery(P->GetSnapshot().SelectedCityStableId,0,TEXT("TradeMap.Directory.Recovery"));
            if(Action==TEXT("Locate")){P->SelectWorkspacePageIntent(TEXT("Map"));return RegionalMap->FrameSelection();}
            return P->DirectoryIntent(Action);
        }
        if(Id==TEXT("TradeMap.Close"))return P->CloseIntent();if(Id==TEXT("TradeMap.Mode.Filter"))return P->CycleModeFilterIntent();if(Id==TEXT("TradeMap.City.Filter"))return P->CycleCityFilterIntent();if(Id==TEXT("TradeMap.Good.Filter"))return P->CycleSelectedGoodIntent();if(Id==TEXT("TradeMap.Route.Page.Previous"))return P->MoveRouteWindowIntent(-1);if(Id==TEXT("TradeMap.Route.Page.Next"))return P->MoveRouteWindowIntent(1);if(Id==TEXT("TradeMap.Editor.Action.Cycle"))return P->CycleCargoActionIntent();if(Id==TEXT("TradeMap.Editor.Quantity.Decrease"))return P->AdjustQuantityIntent(-5000);if(Id==TEXT("TradeMap.Editor.Quantity.Increase"))return P->AdjustQuantityIntent(5000);if(Id==TEXT("TradeMap.Editor.Reserve.Decrease"))return P->AdjustMinimumReserveIntent(-5000);if(Id==TEXT("TradeMap.Editor.Reserve.Increase"))return P->AdjustMinimumReserveIntent(5000);if(Id==TEXT("TradeMap.Editor.Stop.Up"))return P->MoveStopIntent(-1);if(Id==TEXT("TradeMap.Editor.Stop.Down"))return P->MoveStopIntent(1);if(Id==TEXT("TradeMap.Editor.Save"))return P->CommitIntent();if(Id==TEXT("TradeMap.Editor.ToggleActive"))return P->ToggleActiveIntent();if(Id.StartsWith(TEXT("TradeMap.Route."))){const auto Before=P->GetSnapshot().SelectedRouteValue;const bool Result=P->SelectRouteIntent(FCString::Atoi64(*Id.RightChop(15)));if(Result&&Before!=P->GetSnapshot().SelectedRouteValue)RegionalMap->FrameSelection();return Result;}if(Id.StartsWith(TEXT("TradeMap.Stop.")))return P->SelectStopIntent(FCString::Atoi(*Id.RightChop(14)));return false;}
	bool SHansaTradeMap::FocusSemanticId(const FString& Id)
    {
        auto* P=Model.Get();
        if(P&&P->GetSnapshot().bCompact) {
            if(Id.StartsWith(TEXT("TradeMap.Decisions."))||Id.StartsWith(TEXT("TradeMap.Construction."))||Id.StartsWith(TEXT("TradeMap.Overview."))||Id.StartsWith(TEXT("TradeMap.Navigate."))||Id.StartsWith(TEXT("TradeMap.Creator."))||Id.StartsWith(TEXT("TradeMap.Editor."))||Id.StartsWith(TEXT("TradeMap.Stop."))||Id.StartsWith(TEXT("TradeMap.Orders."))||Id.StartsWith(TEXT("TradeMap.Presence."))||Id.StartsWith(TEXT("TradeMap.Station.")))P->SelectWorkspacePageIntent(TEXT("Workspace"));
            else if(Id.StartsWith(TEXT("TradeMap.Port."))||Id.StartsWith(TEXT("TradeMap.Directory."))||Id.StartsWith(TEXT("TradeMap.Fleet."))||Id.StartsWith(TEXT("TradeMap.Route."))||Id==TEXT("TradeMap.Mode.Filter")||Id==TEXT("TradeMap.City.Filter")||Id==TEXT("TradeMap.Good.Filter")||Id==TEXT("TradeMap.City.Search"))P->SelectWorkspacePageIntent(TEXT("Routes"));
            else if(Id.StartsWith(TEXT("TradeMap.Chart.")))P->SelectWorkspacePageIntent(TEXT("Map"));
        }
        if(Id.StartsWith(TEXT("TradeMap.City.City_"))) {
            if(!P||!P->GetSnapshot().bOpen)return false;
            P->SelectWorkspacePageIntent(TEXT("Map"));
            const bool Found=RegionalMap->FocusCity(FName(*Id.RightChop(14).Replace(TEXT("_"),TEXT("."))));
            if(Found)FSlateApplication::Get().SetKeyboardFocus(RegionalMap->FocusWidget(),EFocusCause::Navigation);
            return Found;
        }
        if(P&&Id.StartsWith(TEXT("TradeMap.Ledger."))){P->SelectSectionIntent(TEXT("Ledger"));if(Id.StartsWith(TEXT("TradeMap.Ledger.Good."))){P->SetFocusedSemanticId(FName(*Id));return Ledger->Reveal(Id);}}
        if(P&&Id.StartsWith(TEXT("TradeMap.Schedule."))) {
            if(P->GetSnapshot().bCompact)P->SelectWorkspacePageIntent(TEXT("Schedule"));
            if(!Id.StartsWith(TEXT("TradeMap.Schedule.Tab."))&&Id!=TEXT("TradeMap.Schedule.ChooseOwned")&&Id!=TEXT("TradeMap.Schedule.OpenJourney")) {
                const auto Data=P->GetSchedulePresentation();
                if(const auto* Row=Data.Rows.FindByPredicate([&](const auto& X){return X.Id==Id;})) {
                    if(Schedule->Group!=Row->Group)Schedule->SelectTab(Row->Group);
                    P->SelectScheduleRowIntent(Id);return Schedule->Reveal(Id);
                }
                return false;
            }
        }
        if(ContextHost->SectionMenuScroll&&Id!=TEXT("TradeMap.City.More"))for(auto W=ResolveSemanticWidget(Id);W;W=W->GetParentWidget())if(W==ContextHost->SectionMenuScroll){ContextHost->SectionMenu->SetIsOpen(true);break;}
        if(Directory->DirectoryToolbar&&Id!=TEXT("TradeMap.Directory.More"))for(auto W=ResolveSemanticWidget(Id);W;W=W->GetParentWidget())if(W==Directory->DirectoryToolbar){Directory->MoreMenu->SetIsOpen(true);break;}
        if(RegionalMap->MapToolbar&&Id!=TEXT("TradeMap.Chart.Tools"))for(auto W=ResolveSemanticWidget(Id);W;W=W->GetParentWidget())if(W==RegionalMap->MapToolbar){RegionalMap->ToolsMenu->SetIsOpen(true);break;}
        const auto Widget=ResolveSemanticWidget(Id);
        if(P&&!P->GetSnapshot().bCreating&&Widget&&Widget->IsEnabled())
        {
            if(Id.StartsWith(TEXT("TradeMap.Recovery.")))P->SelectSectionIntent(TEXT("Recovery"));
            if(Id.StartsWith(TEXT("TradeMap.Decisions.")))P->SelectSectionIntent(TEXT("Decisions"));
            if(Id.StartsWith(TEXT("TradeMap.Construction.")))P->SelectSectionIntent(TEXT("Construction"));
            else if(Id.StartsWith(TEXT("TradeMap.Overview.")))P->SelectSectionIntent(TEXT("Overview"));
            else if(Id.StartsWith(TEXT("TradeMap.Presence.Specialization.")))P->SelectSectionIntent(TEXT("Specialization"));
            else if(Id==TEXT("TradeMap.Station.Tab.Upgrade")||(P->GetSnapshot().Establishment.bComplete&&(Id.StartsWith(TEXT("TradeMap.Presence."))||Id.StartsWith(TEXT("TradeMap.Station.Material.")))))P->SelectSectionIntent(TEXT("StationUpgrade"));
            else if(Id.StartsWith(TEXT("TradeMap.Station.Tab.")))P->SelectSectionIntent(TEXT("Presence"));
            else if(Id.StartsWith(TEXT("TradeMap.Presence."))||Id.StartsWith(TEXT("TradeMap.Station."))){if(P->GetSnapshot().ActiveSection!=TEXT("StationUpgrade"))P->SelectSectionIntent(TEXT("Presence"));}
            else if(Id.StartsWith(TEXT("TradeMap.Orders.")))P->SelectSectionIntent(TEXT("Orders"));
            else if(Id.StartsWith(TEXT("TradeMap.Editor."))||Id.StartsWith(TEXT("TradeMap.Stop.")))P->SelectSectionIntent(TEXT("Route"));
            ApplySectionVisibility();
        }
        if(!P||!P->GetSnapshot().bOpen||!GetControllerFocusOrder().Contains(Id)||!Widget||!Widget->IsEnabled())return false;
        P->SetFocusedSemanticId(FName(*Id));
        Directory->Reveal(Id);
        // Reveal descendants by ancestry, so newly added presence controls cannot be omitted by a prefix list.
        for(const auto& Scroll:GetScrollRegions())
        {
            for(auto Ancestor=Widget;Scroll&&Ancestor;Ancestor=Ancestor->GetParentWidget())
                if(Ancestor==Scroll){Scroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);break;}
        }
        if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
        // Page/creator visibility changes settle after layout. A reveal using the old
        // geometry can leave a newly focused field partially clipped at large UI scales.
        RegisterActiveTimer(0.f,FWidgetActiveTimerDelegate::CreateLambda([Weak=TWeakPtr<SHansaTradeMap>(SharedThis(this)),Id,Pass=0](double,float) mutable {
            auto View=Weak.Pin();if(!View||!View->Model.IsValid()||View->Model->GetSnapshot().FocusedSemanticId!=FName(*Id))return EActiveTimerReturnType::Stop;
            if(++Pass<2)return EActiveTimerReturnType::Continue;
            if(auto Target=View->ResolveSemanticWidget(Id))for(const auto& Scroll:View->GetScrollRegions())
                for(auto Ancestor=Target;Scroll&&Ancestor;Ancestor=Ancestor->GetParentWidget())
                    if(Ancestor==Scroll){Scroll->ScrollDescendantIntoView(Target,false,EDescendantScrollDestination::Center);break;}
            return EActiveTimerReturnType::Stop;
        }));
        return true;
    }
    TArray<FString> SHansaTradeMap::GetControllerFocusOrder()const
    {
        TArray<FString> R; const auto* P=Model.Get(); if(!P||!P->GetSnapshot().bOpen)return R;
        if(P->bShipDetailOpen)return ShipDetail->FocusOrder();
        const auto& S=P->GetSnapshot();if(S.ActiveSection==TEXT("Ledger")){R=Ledger->FocusOrder();R.Add(TEXT("TradeMap.New"));R.Add(TEXT("TradeMap.Close"));return R;} R.Add(TEXT("TradeMap.Close"));R.Append({TEXT("TradeMap.City.More"),TEXT("TradeMap.City.Locate"),TEXT("TradeMap.City.Close"),TEXT("TradeMap.Overview.Refresh"),TEXT("TradeMap.Directory.More"),TEXT("TradeMap.Chart.Tools"),TEXT("TradeMap.Overview.Presence"),TEXT("TradeMap.Overview.Market"),TEXT("TradeMap.Schedule.ChooseOwned"),TEXT("TradeMap.Schedule.OpenJourney")});for(const auto& C:S.Cities)R.Add(TEXT("TradeMap.Port.")+C.StableId.ToString());
        if(S.bCompact)R.Append({TEXT("TradeMap.Page.Map"),TEXT("TradeMap.Page.Routes"),TEXT("TradeMap.Page.Workspace"),TEXT("TradeMap.Page.Schedule")});
        if(!S.bCreating) { R.Append({TEXT("TradeMap.New"),TEXT("TradeMap.Mode.Filter"),TEXT("TradeMap.City.Filter"),TEXT("TradeMap.Good.Filter"),TEXT("TradeMap.City.Search"),TEXT("TradeMap.Route.Page.Previous"),TEXT("TradeMap.Route.Page.Next")}); R.Append({TEXT("TradeMap.Directory.Routes"),TEXT("TradeMap.Directory.Fleet"),TEXT("TradeMap.Directory.Filter"),TEXT("TradeMap.Directory.Toggle"),TEXT("TradeMap.Directory.Cancel"),TEXT("TradeMap.Directory.Locate"),TEXT("TradeMap.Directory.Recovery")});for(const auto& X:S.Directory)R.Add(X.SemanticId); }
        if(S.bCreating){R.Append({TEXT("TradeMap.Directory.Routes"),TEXT("TradeMap.Directory.Fleet"),TEXT("TradeMap.Directory.Filter"),TEXT("TradeMap.Directory.Draft")});}
        if(S.bCreating&&!S.bReview)R.Append({TEXT("TradeMap.Creator.Name"),TEXT("TradeMap.Creator.Cog")});
        if(!S.bReview) {
            for(const auto& X:S.Stops)R.Add(FString::Printf(TEXT("TradeMap.Stop.%d"),X.Index));
            if(Model.IsValid()&&Model->CanEditStops())R.Append({TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good")});
            R.Append({TEXT("TradeMap.Editor.Action.Cycle"),TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("TradeMap.Editor.Quantity.Increase"),TEXT("TradeMap.Editor.Reserve.Decrease"),TEXT("TradeMap.Editor.Reserve.Increase"),TEXT("TradeMap.Editor.Stop.Up"),TEXT("TradeMap.Editor.Stop.Down")});
            if(Model.IsValid()&&Model->CanEditStops())R.Append({TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove")});if(S.bCreating)R.Add(TEXT("TradeMap.Creator.Review"));
            else {if(S.bCanTradeStationAction)R.Add(TEXT("TradeMap.Station.Action"));R.Add(TEXT("TradeMap.Editor.Visit"));R.Add(TEXT("TradeMap.Editor.Save"));R.Add(TEXT("TradeMap.Editor.Discard"));const auto* Selected=Model.IsValid()?Model->GetSelectedRoutePresentation():nullptr;if(Selected&&Selected->bCanToggleActive)R.Add(TEXT("TradeMap.Editor.ToggleActive"));if(Selected&&Selected->bCanCancel)R.Add(TEXT("TradeMap.Editor.Cancel"));}
        }
        if(!S.bCreating)R.Append({TEXT("TradeMap.WorldStation.Tab.Details"),TEXT("TradeMap.WorldStation.Tab.Orders"),TEXT("TradeMap.WorldStation.Tab.Upgrade"),TEXT("TradeMap.Station.Tab.Overview"),TEXT("TradeMap.Station.Tab.Upgrade"),TEXT("TradeMap.Station.Overview.Stock"),TEXT("TradeMap.Station.Overview.Orders"),TEXT("TradeMap.Navigate.Overview"),TEXT("TradeMap.Navigate.Presence"),TEXT("TradeMap.Navigate.Specialization"),TEXT("TradeMap.Navigate.Orders"),TEXT("TradeMap.Navigate.Ledger"),TEXT("TradeMap.Navigate.Construction"),TEXT("TradeMap.Navigate.Decisions"),TEXT("TradeMap.Navigate.Recovery")});
        if(!S.bCreating&&S.Establishment.bVisible){if(S.Establishment.bCanOpenMarket&&!S.Establishment.StationId&&!S.Establishment.bReview)R.Add(TEXT("TradeMap.Station.Market"));for(const TCHAR* A:{TEXT("Site"),TEXT("Terms"),TEXT("Source"),TEXT("Confirm"),TEXT("Cancel"),TEXT("Close"),TEXT("ShowOnMap"),TEXT("Priority")})if(P->CanEstablishmentIntent(A))R.Add(TEXT("TradeMap.Station.")+FString(A));}
        if(!S.bCreating)for(const auto& Q:S.PresenceRequirements)if(!Q.bMet)R.Add(TEXT("TradeMap.Presence.Locate.")+Q.RequirementId);if(!S.bCreating&&!S.bPresenceReview&&!S.PresenceSources.IsEmpty())R.Add(TEXT("TradeMap.Presence.Source"));if(!S.bCreating&&S.bCanPresenceUpgradeAction)R.Add(TEXT("TradeMap.Presence.Upgrade"));if(!S.bCreating&&S.bPresenceReview)R.Add(TEXT("TradeMap.Presence.Cancel"));
        if(!S.bCreating&&S.Establishment.bComplete){R.Add(TEXT("TradeMap.Station.Return"));if(S.Establishment.bCanOpenMarket&&!P->IsAnyTradeCommandPending())R.AddUnique(TEXT("TradeMap.Station.Market"));if(S.Establishment.bArrears&&!P->IsAnyTradeCommandPending())R.AddUnique(TEXT("TradeMap.Station.Action"));}
        if(!S.bCreating&&S.ActiveSection==TEXT("Specialization"))R.Append(Specialization->FocusOrder());
        if(!S.bCreating&&S.ActiveSection==TEXT("Recovery"))R.Append(Recovery->FocusOrder());
        if(!S.bCreating&&S.ActiveSection==TEXT("Decisions"))R.Append(Decisions->FocusOrder());
        if(!S.bCreating&&S.ActiveSection==TEXT("Construction"))R.Append(Construction->FocusOrder());
        if(!S.bCreating&&S.TradeStationValue>0){for(const auto& Row:P->GetStationOrderRows())R.Add(FString::Printf(TEXT("TradeMap.Orders.Row.%llu"),Row.Id));R.Append({TEXT("TradeMap.Orders.Select"),TEXT("TradeMap.Orders.New"),TEXT("TradeMap.Orders.Back"),TEXT("TradeMap.Orders.Good"),TEXT("TradeMap.Orders.Buy"),TEXT("TradeMap.Orders.Sell"),TEXT("TradeMap.Orders.Target.Value"),TEXT("TradeMap.Orders.Target.Decrease"),TEXT("TradeMap.Orders.Target.Increase"),TEXT("TradeMap.Orders.Cap.Value"),TEXT("TradeMap.Orders.Cap.Decrease"),TEXT("TradeMap.Orders.Cap.Increase"),TEXT("TradeMap.Orders.Budget.Value"),TEXT("TradeMap.Orders.Budget.Decrease"),TEXT("TradeMap.Orders.Budget.Increase"),TEXT("TradeMap.Orders.Save"),TEXT("TradeMap.Orders.Pause"),TEXT("TradeMap.Orders.Cancel")});}
        if(S.bCreating&&S.bReview){R.Add(TEXT("TradeMap.Creator.Edit"));if(S.bCanCreate)R.Add(TEXT("TradeMap.Creator.Activate"));}
        if(Orders->IsGoodMenuOpen())for(const auto G:P->GetStationOrderGoods())R.Add(TEXT("TradeMap.Orders.Choice.")+G.ToString());
        if(S.bCreating)R.Add(TEXT("TradeMap.Creator.Discard"));
        if(S.bDiscardConfirmation)R.Add(TEXT("TradeMap.Creator.Keep"));
        R.Append({TEXT("TradeMap.Chart.ZoomIn"),TEXT("TradeMap.Chart.ZoomOut"),TEXT("TradeMap.Chart.Reset"),TEXT("TradeMap.Chart.Overlay"),TEXT("TradeMap.Chart.Thickness"),TEXT("TradeMap.Chart.NextRoute"),TEXT("TradeMap.Chart.PreviousCity"),TEXT("TradeMap.Chart.Focus"),TEXT("TradeMap.Chart.CanvasFocus"),TEXT("TradeMap.Chart.NextCity")});
        if(!S.bCreating)R.Append({TEXT("TradeMap.Selection.PreviousCity"),TEXT("TradeMap.Selection.NextCity")});
        if(!S.bCompact||S.WorkspacePage==TEXT("Schedule")){for(const TCHAR* Tab:{TEXT("Stops"),TEXT("Cargo"),TEXT("Arrivals")})R.Add(TEXT("TradeMap.Schedule.Tab.")+FString(Tab));for(const auto& Row:Schedule->Rows)R.Add(Row->Data.Id);}
        R.RemoveAll([&](const FString& Id){if(Id.StartsWith(TEXT("TradeMap.Editor."))||Id.StartsWith(TEXT("TradeMap.Stop."))||Id.StartsWith(TEXT("TradeMap.Creator.")))return true;auto W=ResolveSemanticWidget(Id);while(W&&W.Get()!=this){if(W==ContextHost->SectionMenuScroll)return !ContextHost->SectionMenu->IsOpen();if(W==Directory->DirectoryToolbar)return !Directory->MoreMenu->IsOpen();if(W==RegionalMap->MapToolbar)return !RegionalMap->ToolsMenu->IsOpen();if(!W->GetVisibility().IsVisible())return true;W=W->GetParentWidget();}return !W&&!(Id.StartsWith(TEXT("TradeMap.Schedule."))&&(!S.bCompact||S.WorkspacePage==TEXT("Schedule")))&&!(Id.StartsWith(TEXT("TradeMap.Route."))&&(!S.bCompact||S.WorkspacePage==TEXT("Routes")));});
        return R;
    }

    FReply SHansaTradeMap::OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
    {
        const auto Intent=ClassifyNavigationIntent(Event);
        if(Model.IsValid()&&Model->bWorldStationDetail&&Model->GetSnapshot().FocusedSemanticId.ToString().StartsWith(TEXT("TradeMap.WorldStation.Tab."))&&
            (Event.GetKey()==EKeys::Left||Event.GetKey()==EKeys::Right||Event.GetKey()==EKeys::Gamepad_LeftShoulder||Event.GetKey()==EKeys::Gamepad_RightShoulder))
            return OnKeyDown(Geometry,Event);
        if(Model.IsValid()&&Model->bShipDetailOpen){
            auto Focused=FSlateApplication::Get().GetKeyboardFocusedWidget();
            for(const auto& Id:ShipDetail->FocusOrder()){auto Target=ResolveSemanticWidget(Id);for(auto W=Focused;W;W=W->GetParentWidget())if(W==Target){Model->SetFocusedSemanticId(FName(*Id));break;}}
            const auto Key=Event.GetKey();const FString Id=Model->GetSnapshot().FocusedSemanticId.ToString();
            if((Id==TEXT("TradeMap.Cargo.Quantity")||Id==TEXT("TradeMap.Cargo.Reserve")||Id==TEXT("TradeMap.Cargo.Search")||Id==TEXT("TradeMap.Ship.RouteName"))&&(Key==EKeys::Left||Key==EKeys::Right||Key==EKeys::Up||Key==EKeys::Down))return FReply::Unhandled();
            TArray<FString> Parts;Id.ParseIntoArray(Parts,TEXT("."));
            if(!Model->CargoEditor.bOpen&&Parts.Num()==5&&Parts[1]==TEXT("Cargo")&&Parts[2].IsNumeric()){
                int32 Row=FCString::Atoi(*Parts[2])*2+(Parts[4]==TEXT("Load")?1:0),Slot=FCString::Atoi(*Parts[3]);bool Direction=true;
                if(Key==EKeys::Left||Key==EKeys::Gamepad_DPad_Left)--Slot;else if(Key==EKeys::Right||Key==EKeys::Gamepad_DPad_Right)++Slot;else if(Key==EKeys::Up||Key==EKeys::Gamepad_DPad_Up)--Row;else if(Key==EKeys::Down||Key==EKeys::Gamepad_DPad_Down)++Row;else Direction=false;
                if(Direction){if(Slot>=0&&Slot<3&&Row>=0&&Row<Model->GetSlotDraft().Num()*2)FocusSemanticId(FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"),Row/2,Slot,Row%2?TEXT("Load"):TEXT("Unload")));return FReply::Handled();}
            }
        }
        if(Model.IsValid()&&Model->bShipDetailOpen&&(Intent==EHansaUiNavigationIntent::Next||Intent==EHansaUiNavigationIntent::Previous))return OnKeyDown(Geometry,Event);
        // Traverse the semantic comparison order, including offscreen dimensions.
        // Native geometric button navigation cannot reach cards outside a scroll viewport.
        if(Model.IsValid()&&(Model->GetSnapshot().ActiveSection==TEXT("Specialization")||Model->GetSnapshot().ActiveSection==TEXT("Construction")||Model->GetSnapshot().ActiveSection==TEXT("Decisions")||Model->GetSnapshot().ActiveSection==TEXT("Recovery"))&&
            (Intent==EHansaUiNavigationIntent::Next||Intent==EHansaUiNavigationIntent::Previous))
            return OnKeyDown(Geometry,Event);
        return FReply::Unhandled();
    }
	FReply SHansaTradeMap::OnKeyDown(const FGeometry& MyGeometry,const FKeyEvent& InKeyEvent)
    {
        (void)MyGeometry;const auto Intent=ClassifyNavigationIntent(InKeyEvent);auto* P=Model.Get();if(!P)return FReply::Unhandled();
        const auto Key=InKeyEvent.GetKey();
        const FString Focus=P->GetSnapshot().FocusedSemanticId.ToString();
        if(Key==EKeys::PageDown||Key==EKeys::PageUp||Key==EKeys::Gamepad_RightStick_Down||Key==EKeys::Gamepad_RightStick_Up) {
            if(P->bShipDetailOpen&&!ShipDetail->Scrolls.IsEmpty()){auto Scroll=ShipDetail->Scrolls.Last();Scroll->SetScrollOffset(FMath::Max(0.f,Scroll->GetScrollOffset()+(Key==EKeys::PageDown||Key==EKeys::Gamepad_RightStick_Down?180.f:-180.f)));return FReply::Handled();}
            const auto& S=P->GetSnapshot();TSharedPtr<SScrollBox> Scroll;
            if(S.ActiveSection==TEXT("Recovery")){Recovery->Scroll->SetScrollOffset(Recovery->Scroll->GetScrollOffset()+(Key==EKeys::PageDown||Key==EKeys::Gamepad_RightStick_Down?240.f:-240.f));return FReply::Handled();}
            if(S.ActiveSection==TEXT("Decisions")){Decisions->Scroll->SetScrollOffset(Decisions->Scroll->GetScrollOffset()+(Key==EKeys::PageDown||Key==EKeys::Gamepad_RightStick_Down?240.f:-240.f));return FReply::Handled();}
            if(S.ActiveSection==TEXT("Specialization")){Specialization->Scroll->SetScrollOffset(Specialization->Scroll->GetScrollOffset()+(Key==EKeys::PageDown||Key==EKeys::Gamepad_RightStick_Down?240.f:-240.f));return FReply::Handled();}
            if(S.ActiveSection==TEXT("Ledger")){Ledger->Scroll(Key==EKeys::PageDown||Key==EKeys::Gamepad_RightStick_Down?1.f:-1.f);return FReply::Handled();}
            if((S.bCompact&&S.WorkspacePage==TEXT("Schedule"))||Focus.StartsWith(TEXT("TradeMap.Schedule."))){Schedule->List->AddScrollOffset((Key==EKeys::PageUp?-1.f:1.f)*3.f);return FReply::Handled();}
            else if(!S.bCompact||S.WorkspacePage==TEXT("Workspace")) {
                const TArray<FString> Sections={TEXT("Route"),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders"),TEXT("Overview"),TEXT("Ledger")};
                const int32 Index=Sections.IndexOfByKey(S.bCreating?FString(TEXT("Route")):S.ActiveSection);
                if(Index!=INDEX_NONE)Scroll=ContextHost->Scrolls()[Index];
            }
            if(Scroll){const bool Forward=Key==EKeys::PageDown||Key==EKeys::Gamepad_RightStick_Down;Scroll->SetScrollOffset(FMath::Max(0.f,Scroll->GetScrollOffset()+(Forward?1.f:-1.f)*FMath::Max(48.f,static_cast<float>(Scroll->GetCachedGeometry().GetLocalSize().Y)*.8f)));return FReply::Handled();}
        }
        if((Key==EKeys::Left||Key==EKeys::Right||Key==EKeys::Gamepad_LeftShoulder||Key==EKeys::Gamepad_RightShoulder)&&(Focus.StartsWith(TEXT("TradeMap.Navigate."))||Focus.StartsWith(TEXT("TradeMap.Page."))||Focus.StartsWith(TEXT("TradeMap.WorldStation.Tab.")))) {
            const bool Pages=Focus.StartsWith(TEXT("TradeMap.Page.")),WorldTabs=Focus.StartsWith(TEXT("TradeMap.WorldStation.Tab."));const FString Prefix=Pages?TEXT("TradeMap.Page."):WorldTabs?TEXT("TradeMap.WorldStation.Tab."):TEXT("TradeMap.Navigate.");
            const TArray<FString> Names=WorldTabs?TArray<FString>{TEXT("Details"),TEXT("Orders")}:Pages?TArray<FString>{TEXT("Map"),TEXT("Routes"),TEXT("Workspace"),TEXT("Schedule")}:TArray<FString>{TEXT("Overview"),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders"),TEXT("Ledger"),TEXT("Construction"),TEXT("Decisions"),TEXT("Recovery")};
            const int32 Step=(Key==EKeys::Left||Key==EKeys::Gamepad_LeftShoulder)?-1:1;
            const int32 Index=Names.IndexOfByKey(Focus.RightChop(Prefix.Len()));
            ActivateSemanticId(Prefix+Names[(FMath::Max(0,Index)+Step+Names.Num())%Names.Num()]);return FReply::Handled();
        }

        if(Intent==EHansaUiNavigationIntent::Back&&P->bShipDetailOpen)return ShipDetail->Dispatch(TEXT("TradeMap.Ship.Close"))?FReply::Handled():FReply::Unhandled();
        if(Intent==EHansaUiNavigationIntent::Back)return ActivateSemanticId(TEXT("TradeMap.Back"))?FReply::Handled():FReply::Unhandled();
        if(Intent==EHansaUiNavigationIntent::Activate)return ActivateSemanticId(P->GetSnapshot().FocusedSemanticId.ToString())?FReply::Handled():FReply::Unhandled();
        if(Intent==EHansaUiNavigationIntent::Next||Intent==EHansaUiNavigationIntent::Previous)
        {
            const auto Order=GetControllerFocusOrder();FString Target=P->GetSnapshot().FocusedSemanticId.ToString();
            for(int32 Index=0;Index<Order.Num();++Index)
            {
                Target=FindWrappedFocusTarget(Order,Target,Intent==EHansaUiNavigationIntent::Next);
                if(FocusSemanticId(Target))return FReply::Handled();
            }
        }
        return FReply::Unhandled();
    }
	TArray<FHansaHudSemanticNode> SHansaTradeMap::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaTradeMapPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return Nodes;
		const FHansaTradeMapSnapshot& State = Pinned->GetSnapshot();
		auto Add = [&Nodes, &State, Pinned](FString Id, FString Parent, FString Label, EHansaHudSemanticRole Role,
			bool bActivate = false, FString ValueType = {}, FString Value = {}, bool bSelected = false, bool bWarning = false,
			bool bEnabled = true)
		{
			FHansaHudSemanticNode Node;
			Node.Id = MoveTemp(Id); Node.ParentId = MoveTemp(Parent); Node.Label = MoveTemp(Label); Node.Role = Role;
            if(Node.Label==Node.Id){
                static const TMap<FString,FString> Labels={
                    {TEXT("TradeMap.Chart.ZoomIn"),TEXT("Zoom chart in")},{TEXT("TradeMap.Chart.ZoomOut"),TEXT("Zoom chart out")},{TEXT("TradeMap.Chart.Reset"),TEXT("Reset chart view")},
                    {TEXT("TradeMap.Chart.Overlay"),TEXT("Change chart overlay")},{TEXT("TradeMap.Chart.Thickness"),TEXT("Change route line thickness")},{TEXT("TradeMap.Chart.NextRoute"),TEXT("Select next route")},{TEXT("TradeMap.Chart.PreviousCity"),TEXT("Select previous city")},{TEXT("TradeMap.Chart.NextCity"),TEXT("Select next city")},{TEXT("TradeMap.Chart.Focus"),TEXT("Frame selection")},{TEXT("TradeMap.Chart.CanvasFocus"),TEXT("Focus chart")},
                    {TEXT("TradeMap.Selection.PreviousCity"),TEXT("Select previous city")},{TEXT("TradeMap.Selection.NextCity"),TEXT("Select next city")},
                    {TEXT("TradeMap.Editor.Action.Cycle"),TEXT("Change cargo action")},{TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("Decrease planned cargo by five units")},{TEXT("TradeMap.Editor.Quantity.Increase"),TEXT("Increase planned cargo by five units")},{TEXT("TradeMap.Editor.Reserve.Decrease"),TEXT("Decrease minimum reserve by five units")},{TEXT("TradeMap.Editor.Reserve.Increase"),TEXT("Increase minimum reserve by five units")},{TEXT("TradeMap.Editor.Stop.Up"),TEXT("Move stop earlier")},{TEXT("TradeMap.Editor.Stop.Down"),TEXT("Move stop later")},{TEXT("TradeMap.Editor.Save"),TEXT("Save route changes")},
                    {TEXT("TradeMap.Creator.Name"),TEXT("Route name")},{TEXT("TradeMap.Creator.Cog"),TEXT("Choose an owned Cog")},{TEXT("TradeMap.Creator.City"),TEXT("Change stop city")},{TEXT("TradeMap.Creator.Good"),TEXT("Change cargo good")},{TEXT("TradeMap.Creator.Add"),TEXT("Add stop")},{TEXT("TradeMap.Creator.Remove"),TEXT("Remove stop")},{TEXT("TradeMap.Creator.Review"),TEXT("Review voyage")},{TEXT("TradeMap.Creator.Edit"),TEXT("Edit voyage draft")},{TEXT("TradeMap.Creator.Activate"),TEXT("Confirm and activate voyage")},{TEXT("TradeMap.Creator.Discard"),TEXT("Discard voyage draft")},{TEXT("TradeMap.Creator.Keep"),TEXT("Keep voyage draft")}
                };
                if(const auto* Readable=Labels.Find(Node.Id))Node.Label=*Readable;
                else{Node.Label=Node.Id;Node.Label.RemoveFromStart(TEXT("TradeMap."));Node.Label.ReplaceInline(TEXT("."),TEXT(" "));}
            }

			if(Node.Id.StartsWith(TEXT("TradeMap.Orders."))&&!Node.Id.StartsWith(TEXT("TradeMap.Orders.Row."))&&bActivate)bEnabled=Pinned->CanStationOrderAction(Node.Id.RightChop(16));if(Node.Id.StartsWith(TEXT("TradeMap.Presence.Specialization."))&&bActivate)bEnabled=Pinned->CanPresenceSpecializationIntent(Node.Id.RightChop(33));
            Node.bCanActivate = bActivate; Node.bCanFocus = bActivate; Node.State.bEnabled = bEnabled; Node.State.bVisible = State.bOpen;
			Node.State.bSelected = bSelected; Node.State.bWarning = bWarning; Node.State.bFocused = State.FocusedSemanticId == FName(*Node.Id);
			Node.State.ValueType = MoveTemp(ValueType); Node.State.Value = MoveTemp(Value); Nodes.Add(MoveTemp(Node));
		};
		Add(TEXT("TradeMap.Root"), TEXT("HUD.TradeMapHost"), State.Title.ToString(), EHansaHudSemanticRole::Panel,
			false, TEXT("layout"), State.bCompact ? TEXT("compact") : TEXT("wide"));
		Add(TEXT("TradeMap.Close"), TEXT("TradeMap.Root"), TEXT("Close trade map"), EHansaHudSemanticRole::Button, true);
        Add(TEXT("TradeMap.WorldStation.Tab.Details"),TEXT("TradeMap.Root"),LOCTEXT("WorldDetailsTab","Establishment overview").ToString(),EHansaHudSemanticRole::Tab,true,{}, {},State.ActiveSection==TEXT("Presence"));
        Add(TEXT("TradeMap.WorldStation.Tab.Orders"),TEXT("TradeMap.Root"),LOCTEXT("WorldOrdersTab","Trading orders").ToString(),EHansaHudSemanticRole::Tab,true,{}, {},State.ActiveSection==TEXT("Orders"));
        Add(TEXT("TradeMap.WorldStation.Tab.Upgrade"),TEXT("TradeMap.Root"),LOCTEXT("WorldUpgradeTab","Establishment upgrade").ToString(),EHansaHudSemanticRole::Tab,true,{}, {},State.ActiveSection==TEXT("StationUpgrade"));
        for(const TCHAR* Id:{TEXT("TradeMap.Chart.ZoomIn"),TEXT("TradeMap.Chart.ZoomOut"),TEXT("TradeMap.Chart.Reset"),TEXT("TradeMap.Chart.Overlay"),TEXT("TradeMap.Chart.Thickness"),TEXT("TradeMap.Chart.NextRoute"),TEXT("TradeMap.Chart.PreviousCity"),TEXT("TradeMap.Chart.Focus"),TEXT("TradeMap.Chart.CanvasFocus"),TEXT("TradeMap.Chart.NextCity"),TEXT("TradeMap.Selection.PreviousCity"),TEXT("TradeMap.Selection.NextCity")})
            Add(Id,TEXT("TradeMap.Root"),Id,EHansaHudSemanticRole::Button,true);
        if(!State.bCreating)
        {
            Add(TEXT("TradeMap.Directory.More"),TEXT("TradeMap.Root"),TEXT("Filters and actions"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Chart.Tools"),TEXT("TradeMap.Root"),TEXT("Map tools"),EHansaHudSemanticRole::Button,true);
            for(const auto& C:State.Cities)Add(TEXT("TradeMap.Port.")+C.StableId.ToString(),TEXT("TradeMap.Root"),C.Label.ToString(),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Navigate.Overview"),TEXT("TradeMap.Root"),TEXT("City overview"),EHansaHudSemanticRole::Tab,true);
            Add(TEXT("TradeMap.City.More"),TEXT("TradeMap.Root"),TEXT("More city sections"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.City.Locate"),TEXT("TradeMap.Root"),TEXT("Locate city"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.City.Close"),TEXT("TradeMap.Root"),TEXT("Close trade map"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.City.Panel"),TEXT("TradeMap.Root"),TEXT("City inspector"),EHansaHudSemanticRole::Status,false);
            Add(TEXT("TradeMap.Overview.Scroll"),TEXT("TradeMap.City.Panel"),TEXT("City overview content"),EHansaHudSemanticRole::Status,false);
            Add(TEXT("TradeMap.Overview.ConstructionStatus"),TEXT("TradeMap.City.Panel"),TEXT("Construction rights"),EHansaHudSemanticRole::Status,false,TEXT("rights"),ContextHost->ConstructionStatus->GetText().ToString());
            Add(TEXT("TradeMap.Overview.Refresh"),TEXT("TradeMap.Root"),TEXT("Open market to refresh lawful report"),EHansaHudSemanticRole::Button,true);
            for(const TCHAR* Field:{TEXT("Price"),TEXT("Stock"),TEXT("Routes"),TEXT("Ships")}) {
                const FString Key=TEXT("TradeMap.Overview.")+FString(Field);
                if(auto W=ResolveSemanticWidget(Key))Add(Key,TEXT("TradeMap.City.Panel"),Field,EHansaHudSemanticRole::Status,false,TEXT("reported"),StaticCastSharedPtr<STextBlock>(W)->GetText().ToString());
            }
            Add(TEXT("TradeMap.Overview.Market"),TEXT("TradeMap.Root"),TEXT("Open market / quay trade"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Overview.Presence"),TEXT("TradeMap.Root"),State.CityInspector.PrimaryAction.ToString(),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Overview.Report"),TEXT("TradeMap.Root"),TEXT("Market access and report"),EHansaHudSemanticRole::Status,false,TEXT("city-report"),ContextHost->OverviewReport->GetText().ToString());
            Add(TEXT("TradeMap.Overview.Access"),TEXT("TradeMap.Root"),TEXT("City rights and arrivals"),EHansaHudSemanticRole::Status,false,TEXT("city-access"),ContextHost->OverviewAccess->GetText().ToString());
            Add(TEXT("TradeMap.Overview.Identity"),TEXT("TradeMap.Root"),TEXT("Selected city"),EHansaHudSemanticRole::Status,false,TEXT("city-id"),Pinned->bWorldStationDetail?ContextHost->SelectedCityText->GetText().ToString():State.SelectedCityStableId.ToString());
            Add(TEXT("TradeMap.Schedule.ChooseOwned"),TEXT("TradeMap.Schedule"),TEXT("Select your vessel"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Navigate.Presence"),TEXT("TradeMap.Root"),LOCTEXT("JumpPresence","Foreign presence").ToString(),EHansaHudSemanticRole::Tab,true);
            Add(TEXT("TradeMap.Navigate.Specialization"),TEXT("TradeMap.Root"),LOCTEXT("JumpSpecialization","Specializations").ToString(),EHansaHudSemanticRole::Tab,true);
            Add(TEXT("TradeMap.Navigate.Ledger"),TEXT("TradeMap.Root"),TEXT("Station ledger"),EHansaHudSemanticRole::Tab,true);
            Add(TEXT("TradeMap.Navigate.Orders"),TEXT("TradeMap.Root"),LOCTEXT("JumpOrders","Station orders").ToString(),EHansaHudSemanticRole::Tab,true);
            if(State.TradeStationValue<=0)Add(TEXT("TradeMap.Orders.Status"),TEXT("TradeMap.Root"),TEXT("Station orders locked"),EHansaHudSemanticRole::Status,false,TEXT("orders"),Orders->StationOrderText->GetText().ToString());
        }
		Add(TEXT("TradeMap.Presence.Progress"),TEXT("TradeMap.Root"),State.PresenceProgress.ToString(),EHansaHudSemanticRole::Status,false,TEXT("presence-progress"),TEXT("Authoritative requirements, unlocks and history."),false,false,false);
		Add(TEXT("TradeMap.Presence.Commercial"),TEXT("TradeMap.Presence.Progress"),TEXT("Commercial presence"),EHansaHudSemanticRole::Status,false,TEXT("presence-kind"),TEXT("Trade access and merchant facilities do not imply city ownership."));



        Add(TEXT("TradeMap.Station.Return"),TEXT("TradeMap.Root"),TEXT("Return to station"),EHansaHudSemanticRole::Button,true);
        if(State.Establishment.bComplete){
            Add(TEXT("TradeMap.Station.ConstructionPaid"),TEXT("TradeMap.Root"),TEXT("Construction paid"),EHansaHudSemanticRole::Status,false,TEXT("pfennig"),State.Establishment.ConstructionPaid.ToString());
            for(const auto& Right:State.Establishment.Rights)Add(TEXT("TradeMap.Station.Right.")+Right.Id,TEXT("TradeMap.Root"),Right.Label.ToString(),EHansaHudSemanticRole::Status,false,TEXT("right"),Right.bGranted?TEXT("Granted"):TEXT("Unavailable"));
            if(const auto* Source=State.PresenceSources.FindByPredicate([&](const auto& C){return C.Id==State.PresenceSourceId;}))for(const auto& Material:Source->Materials)Add(TEXT("TradeMap.Station.Material.")+Material.GoodId,TEXT("TradeMap.Root"),Material.Label.ToString(),EHansaHudSemanticRole::Status,false,TEXT("quantity"),FString::Printf(TEXT("Required %lld; available %lld milli-units"),Material.RequiredMilliUnits,Material.AvailableMilliUnits));
        }
        Add(TEXT("TradeMap.Presence.Source"),TEXT("TradeMap.Root"),TEXT("Choose office funding source"),EHansaHudSemanticRole::Button,true,TEXT("inventory-choice"),State.PresenceFundingDetail.ToString(),false,State.PresenceSources.IsEmpty(),!State.PresenceSources.IsEmpty());
        Add(TEXT("TradeMap.Presence.Cancel"),TEXT("TradeMap.Root"),TEXT("Edit office review"),EHansaHudSemanticRole::Button,true,TEXT("review"),State.PresenceReview.ToString(),false,!State.bPresenceReview,State.bPresenceReview);
        Add(TEXT("TradeMap.Presence.Upgrade"),TEXT("TradeMap.Root"),State.PresenceUpgradeAction.ToString(),EHansaHudSemanticRole::Button,true,TEXT("presence-upgrade"),State.PresenceConsequences.ToString(),false,!State.bCanPresenceUpgradeAction,State.bCanPresenceUpgradeAction);
        Add(TEXT("TradeMap.Navigate.Recovery"),TEXT("TradeMap.Root"),TEXT("Recovery"),EHansaHudSemanticRole::Tab,true);
        Add(TEXT("TradeMap.Recovery.Status"),TEXT("TradeMap.Root"),TEXT("Recovery state"),EHansaHudSemanticRole::Status,false,TEXT("station"),State.Recovery.Status+TEXT(" ")+State.Recovery.Cause);
        Add(TEXT("TradeMap.Recovery.Feedback"),TEXT("TradeMap.Root"),TEXT("Recovery result"),EHansaHudSemanticRole::Status,false,TEXT("result"),State.bRecoveryPending?TEXT("Pending authoritative result"):State.RecoveryFeedback);
        const auto* RecoveryItem=State.Recovery.Items.FindByPredicate([&](const auto& X){return X.Id==State.RecoveryItem;});
        Add(TEXT("TradeMap.Recovery.Detail"),TEXT("TradeMap.Root"),TEXT("Recovery dossier"),EHansaHudSemanticRole::Status,false,TEXT("dossier"),State.bRecoveryReview?State.Recovery.Terms:RecoveryItem?RecoveryItem->Detail:TEXT("Select a dependency"));
        for(const auto& I:State.Recovery.Items)Add(TEXT("TradeMap.Recovery.Item.")+I.Id,TEXT("TradeMap.Root"),I.Label,EHansaHudSemanticRole::ListItem,true,TEXT("dependency"),I.Detail,State.RecoveryItem==I.Id);
        for(const TCHAR* A:{TEXT("Inspect"),TEXT("Review"),TEXT("Confirm"),TEXT("Cancel"),TEXT("Back")})Add(TEXT("TradeMap.Recovery.")+FString(A),TEXT("TradeMap.Root"),FString(A),EHansaHudSemanticRole::Button,true,TEXT("recovery"),State.Recovery.Cause,false,false,Pinned->CanRecoveryIntent(A));
        Add(TEXT("TradeMap.Navigate.Decisions"),TEXT("TradeMap.Root"),TEXT("City decisions"),EHansaHudSemanticRole::Tab,true);
        for(const auto& O:State.Decisions.Options)Add(TEXT("TradeMap.Decisions.")+O.Id,TEXT("TradeMap.Root"),O.Title.ToString(),EHansaHudSemanticRole::Button,true,TEXT("decision"),O.Status.ToString(),State.DecisionId==O.Id);
        for(const TCHAR* A:{TEXT("Source"),TEXT("Review"),TEXT("Confirm"),TEXT("Cancel")})Add(TEXT("TradeMap.Decisions.")+FString(A),TEXT("TradeMap.Root"),FString(A),EHansaHudSemanticRole::Button,true,TEXT("decision"),State.DecisionFeedback,false,false,Pinned->CanDecisionIntent(A));
        const auto* Decision=State.Decisions.Options.FindByPredicate([&](const auto& O){return O.Id==State.DecisionId;});
        Add(TEXT("TradeMap.Decisions.Terms"),TEXT("TradeMap.Root"),TEXT("Decision terms"),EHansaHudSemanticRole::Status,false,TEXT("dossier"),Decision?Decision->Terms.ToString():TEXT("Select a decision"));
        Add(TEXT("TradeMap.Decisions.Status"),TEXT("TradeMap.Root"),TEXT("Decision status"),EHansaHudSemanticRole::Status,false,TEXT("dossier"),Decision?Decision->Status.ToString():TEXT("Unavailable"));
        Add(TEXT("TradeMap.Navigate.Construction"),TEXT("TradeMap.Root"),TEXT("Expansion / construction"),EHansaHudSemanticRole::Tab,true);
        const auto ConstructionView=Pinned->GetConstructionPresentation();
        Add(TEXT("TradeMap.Construction.Status"),TEXT("TradeMap.Root"),TEXT("Lease permissions"),EHansaHudSemanticRole::Status,false,TEXT("lease-status"),ConstructionView.Status.ToString());
        Add(TEXT("TradeMap.Construction.Bounds"),TEXT("TradeMap.Root"),TEXT("Lease bounds and occupied cells"),EHansaHudSemanticRole::Status,false,TEXT("lease-grid"),ConstructionView.Key());
        Add(TEXT("TradeMap.Construction.WorldStatus"),TEXT("TradeMap.Root"),TEXT("World placement"),EHansaHudSemanticRole::Status,false,TEXT("lease-authority"),TEXT("Place a permitted building inside its active lease; authority revalidates on confirmation."),false,false);
        for(const auto& Plot:ConstructionView.Plots)Add(TEXT("TradeMap.Construction.Plot.")+LexToString(Plot.Id),TEXT("TradeMap.Root"),Plot.Summary.ToString(),EHansaHudSemanticRole::Button,true,TEXT("lease"),Plot.Summary.ToString(),Plot.Id==ConstructionView.SelectedLease,!Plot.bActive);
        for(const auto& Option:ConstructionView.Options){
            Add(TEXT("TradeMap.Construction.")+Option.Id.ToString(),TEXT("TradeMap.Root"),Option.Name.ToString(),EHansaHudSemanticRole::Button,true,TEXT("construction-review"),Option.Reason.ToString()+TEXT("\n")+Option.Detail.ToString(),Option.Id==ConstructionView.SelectedBuilding,!Option.bPermitted||!Option.bAffordable);
            if(Option.Id==ConstructionView.SelectedBuilding)Add(TEXT("TradeMap.Construction.Detail"),TEXT("TradeMap.Root"),Option.Name.ToString(),EHansaHudSemanticRole::Status,false,TEXT("construction-costs"),Option.Reason.ToString()+TEXT("\n")+Option.Detail.ToString());
        }
        const auto* PlaceOption=ConstructionView.Options.FindByPredicate([&](const auto& O){return O.Id==ConstructionView.SelectedBuilding;});
        Add(TEXT("TradeMap.Construction.Place"),TEXT("TradeMap.Root"),TEXT("Place selected building"),EHansaHudSemanticRole::Button,true,TEXT("placement"),ConstructionView.City.ToString(),false,false,PlaceOption&&PlaceOption->bPermitted&&PlaceOption->bAffordable);
        Add(TEXT("TradeMap.Construction.Visit"),TEXT("TradeMap.Root"),TEXT("Inspect city"),EHansaHudSemanticRole::Button,true,TEXT("city"),ConstructionView.City.ToString(),false,false,ConstructionView.City==TEXT("City.Rostock"));
        Add(TEXT("TradeMap.Construction.Presence"),TEXT("TradeMap.Root"),TEXT("Review rights"),EHansaHudSemanticRole::Button,true);
        Add(TEXT("TradeMap.Construction.Ledger"),TEXT("TradeMap.Root"),TEXT("Station materials"),EHansaHudSemanticRole::Button,true);
        const auto DimensionLabels=SpecializationDimensionLabels();
        for(const auto& O:State.Specialization.Options){
            const FString Prefix=TEXT("TradeMap.Presence.Specialization.");
            Add(Prefix+O.Id,TEXT("TradeMap.Root"),O.Name.ToString(),EHansaHudSemanticRole::Button,true,TEXT("exclusive-specialization"),O.bCurrent?TEXT("Current"):O.Blocker.ToString(),State.SelectedPresenceSpecializationId==O.Id);
            for(int32 D=0;D<O.Dimensions.Num();++D)Add(Prefix+FString::Printf(TEXT("Dimension.%s.%d"),*O.Id,D),Prefix+O.Id,O.Name.ToString()+TEXT(" · ")+DimensionLabels[D].ToString(),EHansaHudSemanticRole::Button,true,TEXT("comparison-dimension"),O.Dimensions[D].ToString());
        }
        for(const TCHAR* Action:{TEXT("Back"),TEXT("Source"),TEXT("Apply"),TEXT("Confirm"),TEXT("Cancel")})
            Add(TEXT("TradeMap.Presence.Specialization.")+FString(Action),TEXT("TradeMap.Root"),FString(Action),EHansaHudSemanticRole::Button,true,TEXT("specialization-review"),State.SpecializationReview.ToString());

        if(State.Establishment.bVisible){
            const auto& E=State.Establishment;
            if(E.bComplete){
                const auto V=Pinned->GetStationOverviewPresentation();
                Add(TEXT("TradeMap.Station.Tab.Overview"),TEXT("TradeMap.Root"),TEXT("Overview"),EHansaHudSemanticRole::Tab,true,{}, {},State.ActiveSection==TEXT("Presence"));
                Add(TEXT("TradeMap.Station.Tab.Upgrade"),TEXT("TradeMap.Root"),TEXT("Upgrade"),EHansaHudSemanticRole::Tab,true,{}, {},State.ActiveSection==TEXT("StationUpgrade"));
                for(const TCHAR* Id:{TEXT("Stock"),TEXT("Orders")})Add(TEXT("TradeMap.Station.Overview.")+FString(Id),TEXT("TradeMap.Root"),FString(Id),EHansaHudSemanticRole::Button,true);
                for(const TCHAR* Id:{TEXT("Trading"),TEXT("Blocker"),TEXT("Upgrade"),TEXT("Upkeep")}){const FString Full=TEXT("TradeMap.Station.Overview.")+FString(Id);if(auto W=ResolveSemanticWidget(Full))Add(Full,TEXT("TradeMap.Root"),FString(Id),EHansaHudSemanticRole::Status,false,TEXT("summary"),StaticCastSharedPtr<STextBlock>(W)->GetText().ToString());}
                int32 Count=0;for(const auto& R:V.Ledger.Rows)if(R.Matches(0)&&Count++<5)Add(TEXT("TradeMap.Station.Overview.Good.")+R.Good.ToString(),TEXT("TradeMap.Root"),R.Label.ToString(),EHansaHudSemanticRole::Status,false,TEXT("stock"),FString::Printf(TEXT("Available %s units · Reserved %s units"),*FText::AsNumber(double(R.Available)/1000.).ToString(),*FText::AsNumber(double(R.Reserved)/1000.).ToString()));
                for(int32 I=0;I<FMath::Min(2,V.Transport.Num());++I){const auto& R=V.Transport[I];Add(TEXT("TradeMap.Station.Overview.Route.")+R.Id,TEXT("TradeMap.Root"),R.Label.ToString(),EHansaHudSemanticRole::Status,false,TEXT("transport"),R.Detail.ToString());}
                for(const auto& R:V.Activity)Add(TEXT("TradeMap.Station.Overview.Activity.")+R.Id,TEXT("TradeMap.Root"),R.Label.ToString(),EHansaHudSemanticRole::Status,false,TEXT("receipt"),R.Detail.ToString());
            }
            for(const TCHAR* Field:{TEXT("Identity"),TEXT("BuildingName"),TEXT("Storage")}){
                const FString Id=TEXT("TradeMap.Station.")+FString(Field);
                if(auto W=ResolveSemanticWidget(Id))Add(Id,TEXT("TradeMap.Root"),Field,EHansaHudSemanticRole::Status,false,TEXT("building"),StaticCastSharedPtr<STextBlock>(W)->GetText().ToString());
            }
            Add(TEXT("TradeMap.Station.Market"),TEXT("TradeMap.Root"),TEXT("Open market / quay trade"),EHansaHudSemanticRole::Button,true,TEXT("market"),TEXT("Open the selected city's market"),false,false,E.bCanOpenMarket&&!E.bPending);
            Add(TEXT("TradeMap.Station.Scroll"),TEXT("TradeMap.Root"),TEXT("Presence details"),EHansaHudSemanticRole::Status,false);
            Add(TEXT("TradeMap.Station.Footer"),TEXT("TradeMap.Root"),TEXT("Presence actions"),EHansaHudSemanticRole::Status,false);
            Add(TEXT("TradeMap.Station.SiteCard"),TEXT("TradeMap.Root"),E.SiteName.ToString(),EHansaHudSemanticRole::Status,false,TEXT("site"),E.SiteStatus.ToString()+TEXT(" · ")+E.Storage.ToString()+TEXT(" · ")+E.BuildDuration.ToString()+TEXT(" · ")+E.DailyUpkeep.ToString());
            Add(TEXT("TradeMap.Station.Requirements"),TEXT("TradeMap.Root"),TEXT("Trading requirements"),EHansaHudSemanticRole::Status,false,TEXT("requirements"),LexToString(E.UnmetRequirements)+TEXT(" remaining"));
            Add(TEXT("TradeMap.Station.Funding"),TEXT("TradeMap.Root"),TEXT("Establishment cost"),EHansaHudSemanticRole::Status,false,TEXT("funding"),E.Treasury.ToString()+TEXT(" · ")+E.Cost.ToString()+TEXT(" · ")+E.Remainder.ToString());
            Add(TEXT("TradeMap.Station.Blocker"),TEXT("TradeMap.Root"),TEXT("Next step"),EHansaHudSemanticRole::Status,false,TEXT("reason"),E.Blocker.ToString());
            for(const auto& Q:E.Requirements)Add(TEXT("TradeMap.Station.Requirement.")+Q.Id,TEXT("TradeMap.Station.Requirements"),Q.Label.ToString(),EHansaHudSemanticRole::Status,false,Q.bMet?TEXT("met"):TEXT("unmet"),Q.Value.ToString()+TEXT(" · ")+Q.Hint.ToString());
            for(const TCHAR* A:{TEXT("Site"),TEXT("Terms"),TEXT("Source"),TEXT("Confirm"),TEXT("Cancel"),TEXT("Close"),TEXT("ShowOnMap"),TEXT("Priority")})Add(TEXT("TradeMap.Station.")+FString(A),TEXT("TradeMap.Root"),FString(A)==TEXT("Confirm")?(State.Establishment.bProposed||State.Establishment.bArrears?FString::Printf(TEXT("Confirm inventory #%s spending"),*State.Establishment.SourceId):FString(TEXT("Confirm site reservation"))):FString(A),EHansaHudSemanticRole::Button,true,TEXT("establishment"),FString(A)==TEXT("Confirm")?State.Establishment.Confirmation.ToString():State.Establishment.Feedback.ToString(),false,false,Pinned->CanEstablishmentIntent(A));
            Add(TEXT("TradeMap.Station.Summary"),TEXT("TradeMap.Root"),TEXT("Establishment"),EHansaHudSemanticRole::Status,false,TEXT("review"),State.Establishment.Summary.ToString());
            Add(TEXT("TradeMap.Station.SourceDetail"),TEXT("TradeMap.Root"),TEXT("Funding inventory"),EHansaHudSemanticRole::Status,false,TEXT("inventory"),State.Establishment.SourceDetail.ToString());
            Add(TEXT("TradeMap.Station.Review"),TEXT("TradeMap.Root"),TEXT("Exact transfer"),EHansaHudSemanticRole::Status,false,TEXT("confirmation"),State.Establishment.bReview?State.Establishment.Confirmation.ToString():FString());
            Add(TEXT("TradeMap.Station.Feedback"),TEXT("TradeMap.Root"),TEXT("Station command status"),EHansaHudSemanticRole::Status,false,TEXT("status"),State.Establishment.Feedback.ToString());
        }
		Add(TEXT("TradeMap.Station.Action"), TEXT("TradeMap.Root"), State.TradeStationAction.ToString(), EHansaHudSemanticRole::Button, true,
			TEXT("station-state"), State.TradeStationState.ToString()+TEXT(" · ")+State.TradeStationDetail.ToString(), false, State.Establishment.bComplete&&State.Establishment.bArrears?Pinned->IsAnyTradeCommandPending():!State.bCanTradeStationAction, State.Establishment.bComplete&&State.Establishment.bArrears?!Pinned->IsAnyTradeCommandPending():State.bCanTradeStationAction);
        if(!State.bCreating&&State.TradeStationValue>0){
            Add(TEXT("TradeMap.Orders.List"),TEXT("TradeMap.Root"),TEXT("Station order list"),EHansaHudSemanticRole::Status,false,TEXT("orders"),(State.bCompact?State.StationOrderListCompact:State.StationOrderList).ToString());
            Add(TEXT("TradeMap.Orders.Status"),TEXT("TradeMap.Root"),TEXT("Station orders"),EHansaHudSemanticRole::Status,false,TEXT("orders"),State.StationOrderText.ToString());
            for(const auto& Row:Pinned->GetStationOrderRows()){
                const FString RowId=FString::Printf(TEXT("TradeMap.Orders.Row.%llu"),Row.Id);
                const FString Value=FString::Printf(TEXT("%s · target %s · cap %s · %s · %s · report %s · last %s · next %s"),
                    *Row.Side.ToString(),*Row.Target.ToString(),*Row.Cap.ToString(),*Row.Budget.ToString(),
                    *Row.State.ToString(),*Row.ReportAge.ToString(),*Row.LastResult.ToString(),*Row.Remedy.ToString());
                Add(RowId,TEXT("TradeMap.Root"),Row.Good.ToString(),EHansaHudSemanticRole::Button,true,TEXT("order"),Value);
            }
            Add(TEXT("TradeMap.Orders.Select"),TEXT("TradeMap.Root"),TEXT("Choose order / new"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.New"),TEXT("TradeMap.Root"),TEXT("Create new order"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Back"),TEXT("TradeMap.Root"),TEXT("Back to orders"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Good"),TEXT("TradeMap.Root"),TEXT("Choose commodity"),EHansaHudSemanticRole::Button,true);
            if(Orders->IsGoodMenuOpen())for(const auto G:Pinned->GetStationOrderGoods())Add(TEXT("TradeMap.Orders.Choice.")+G.ToString(),TEXT("TradeMap.Root"),Pinned->GetOrderGoodLabel(G).ToString(),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Buy"),TEXT("TradeMap.Root"),TEXT("Buy"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Sell"),TEXT("TradeMap.Root"),TEXT("Sell"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Target.Value"),TEXT("TradeMap.Root"),TEXT("Target or reserve units"),EHansaHudSemanticRole::Text,true,TEXT("units"),State.StationOrderTargetInput);
            Add(TEXT("TradeMap.Orders.Cap.Value"),TEXT("TradeMap.Root"),TEXT("Units per update"),EHansaHudSemanticRole::Text,true,TEXT("units/update"),State.StationOrderCapInput);
            Add(TEXT("TradeMap.Orders.Budget.Value"),TEXT("TradeMap.Root"),TEXT("Total purchase budget"),EHansaHudSemanticRole::Text,true,TEXT("pfennig"),LexToString(State.StationOrderBudgetPfennig));
            Add(TEXT("TradeMap.Orders.Target.Decrease"),TEXT("TradeMap.Root"),TEXT("− 1 target / reserve"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Target.Increase"),TEXT("TradeMap.Root"),TEXT("+ 1 target / reserve"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Cap.Decrease"),TEXT("TradeMap.Root"),TEXT("− 1 cap"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Cap.Increase"),TEXT("TradeMap.Root"),TEXT("+ 1 cap"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Budget.Decrease"),TEXT("TradeMap.Root"),TEXT("− 1,000 budget"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Budget.Increase"),TEXT("TradeMap.Root"),TEXT("+ 1,000 budget"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Save"),TEXT("TradeMap.Root"),TEXT("Create / save order"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Pause"),TEXT("TradeMap.Root"),TEXT("Pause / resume order"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Cancel"),TEXT("TradeMap.Root"),TEXT("Cancel order"),EHansaHudSemanticRole::Button,true);
        }
		Add(TEXT("TradeMap.Mode.Filter"), TEXT("TradeMap.Root"), TEXT("Filter route mode"), EHansaHudSemanticRole::Button,
			true, TEXT("mode"), FString::FromInt(static_cast<int32>(State.ModeFilter)));
		Add(TEXT("TradeMap.City.Filter"),TEXT("TradeMap.Root"),TEXT("Filter cities by presence or route"),EHansaHudSemanticRole::Button,true,TEXT("city-mode"),FString::FromInt(static_cast<int32>(State.CityFilter)));
		Add(TEXT("TradeMap.Good.Filter"),TEXT("TradeMap.Root"),TEXT("Cycle selected report good"),EHansaHudSemanticRole::Button,true,TEXT("selected-good"),State.PreferredGoodStableId.ToString());
		Add(TEXT("TradeMap.City.Search"),TEXT("TradeMap.Root"),TEXT("Search cities"),EHansaHudSemanticRole::Text,true,TEXT("search"),State.CitySearchText);
		Add(TEXT("TradeMap.Route.Page.Previous"),TEXT("TradeMap.Root"),TEXT("Previous route rows"),EHansaHudSemanticRole::Button,true,TEXT("virtual-window"),FString::FromInt(State.RouteWindowStart),false,false,State.RouteWindowStart>0);
		Add(TEXT("TradeMap.Route.Page.Next"),TEXT("TradeMap.Root"),TEXT("Next route rows"),EHansaHudSemanticRole::Button,true,TEXT("virtual-window"),FString::FromInt(State.MatchingRouteCount),false,false,State.RouteWindowStart+State.Routes.Num()<State.MatchingRouteCount);
		Add(TEXT("TradeMap.Canvas"), TEXT("TradeMap.Root"), TEXT("Regional trade network map"), EHansaHudSemanticRole::Panel,
			false, TEXT("geometry"), TEXT("native"));
		for (const FHansaTradeMapCityPresentation& City : State.Cities)
		{
			Add(FString::Printf(TEXT("TradeMap.City.%s"), *City.StableId.ToString().Replace(TEXT("."), TEXT("_"))),
				TEXT("TradeMap.Canvas"), City.Label.ToString(), EHansaHudSemanticRole::Button, true, TEXT("report"),
				City.Information.ToString()+TEXT("; ")+City.CapabilitySummary.ToString()+TEXT("; ")+City.GoodReport.ToString(), City.StableId==State.SelectedCityStableId, City.bStale || City.bUnknown);
		}

        for(const TCHAR* Action:{TEXT("Routes"),TEXT("Fleet"),TEXT("Filter"),TEXT("Toggle"),TEXT("Cancel"),TEXT("Locate"),TEXT("Recovery")})
            Add(TEXT("TradeMap.Directory.")+FString(Action),TEXT("TradeMap.Root"),Action,EHansaHudSemanticRole::Button,true);
        for(const auto& Entry:State.Directory)
            Add(Entry.SemanticId,TEXT("TradeMap.Root"),Entry.Label.ToString(),EHansaHudSemanticRole::ListItem,true,TEXT("directory"),Entry.Detail.ToString()+TEXT("; ")+Entry.Alert.ToString(),State.bFleetView?Entry.VehicleValue==State.SelectedVehicleValue:Entry.RouteValue==State.SelectedRouteValue,Entry.bAttention);

		for (const FHansaTradeMapStopPresentation& Stop : State.Stops)
		{
			Add(FString::Printf(TEXT("TradeMap.Stop.%d"), Stop.Index), TEXT("TradeMap.Root"), Stop.CityLabel.ToString(),
				EHansaHudSemanticRole::ListItem, true, TEXT("cargo-action"), Stop.AccessibleLabel.ToString(),
				Stop.Index == State.SelectedStopIndex, Stop.bReserveRisk);
		}
		for (const FString& Id : { TEXT("TradeMap.Editor.Action.Cycle"), TEXT("TradeMap.Editor.Quantity.Decrease"),
			TEXT("TradeMap.Editor.Quantity.Increase"), TEXT("TradeMap.Editor.Reserve.Decrease"), TEXT("TradeMap.Editor.Reserve.Increase"),
			TEXT("TradeMap.Editor.Stop.Up"), TEXT("TradeMap.Editor.Stop.Down"), TEXT("TradeMap.Editor.Save") })
		{
			Add(Id, TEXT("TradeMap.Root"), Id, EHansaHudSemanticRole::Button, true);
		}
		const FHansaTradeMapRoutePresentation* SelectedRoute = Model.IsValid()?Model->GetSelectedRoutePresentation():nullptr;
		if (SelectedRoute != nullptr)
		{
            Add(TEXT("TradeMap.Editor.Cancel"), TEXT("TradeMap.Root"), TEXT("Cancel route"), EHansaHudSemanticRole::Button, SelectedRoute->bCanCancel, TEXT("route-action"), TEXT("Stop and unload before cancelling."), false, false, SelectedRoute->bCanCancel);
			Add(TEXT("TradeMap.Editor.RouteState"), TEXT("TradeMap.Root"), SelectedRoute->StateHeading.ToString(),
				EHansaHudSemanticRole::Status, false, TEXT("route-state"),
				FString::Printf(TEXT("%s;%s;%s"), *SelectedRoute->State.ToString(), *SelectedRoute->StateDetail.ToString(), *SelectedRoute->Ownership.ToString()));
			Add(TEXT("TradeMap.Editor.ToggleActive"), TEXT("TradeMap.Root"), SelectedRoute->ToggleActionLabel.ToString(),
				EHansaHudSemanticRole::Button, SelectedRoute->bCanToggleActive, TEXT("route-action"),
				SelectedRoute->ToggleActionHint.ToString(), false, false, SelectedRoute->bCanToggleActive);
		}
		Add(TEXT("TradeMap.Editor.ReserveRisk"), TEXT("TradeMap.Root"), State.ReserveRisk.ToString(), EHansaHudSemanticRole::Alert,
			false, TEXT("risk"), State.ReserveRisk.ToString(), false, State.ReserveRisk.ToString().StartsWith(TEXT("Planned load exceeds")));
        Add(TEXT("TradeMap.New"),TEXT("TradeMap.Root"),TEXT("New route"),EHansaHudSemanticRole::Button,true);
        Add(TEXT("TradeMap.Editor.Visit"),TEXT("TradeMap.Root"),TEXT("Visit selected stop"),EHansaHudSemanticRole::Button,!State.bCreating);
        for(const FString& Id:{TEXT("TradeMap.Creator.Name"),TEXT("TradeMap.Creator.Cog"),TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good"),TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove"),TEXT("TradeMap.Creator.Review"),TEXT("TradeMap.Creator.Edit"),TEXT("TradeMap.Creator.Activate"),TEXT("TradeMap.Creator.Discard"),TEXT("TradeMap.Creator.Keep")})
            Add(Id,TEXT("TradeMap.Root"),Id,EHansaHudSemanticRole::Button,true);
        Add(TEXT("TradeMap.Creator.ReviewText"),TEXT("TradeMap.Root"),State.CreatorReview.ToString(),EHansaHudSemanticRole::Status,false,TEXT("review"),State.CreatorReview.ToString());
        Add(TEXT("TradeMap.Creator.Validation"),TEXT("TradeMap.Root"),State.Validation.ToString(),EHansaHudSemanticRole::Status,false,TEXT("validation"),State.Validation.ToString());

        Add(TEXT("TradeMap.Ledger.Title"),TEXT("TradeMap.Root"),TEXT("Station ledger"),EHansaHudSemanticRole::Status,false,TEXT("station"),Ledger->View.Identity.ToString());
        for(const auto& Id:Ledger->FocusOrder())if(!Id.StartsWith(TEXT("TradeMap.Ledger.Good.")))Add(Id,TEXT("TradeMap.Root"),Id,EHansaHudSemanticRole::Button,true);
        Add(TEXT("TradeMap.Ledger.DetailText"),TEXT("TradeMap.Root"),TEXT("Ledger causes and operations"),EHansaHudSemanticRole::Status,false,TEXT("ledger"),Ledger->AccessibleDetail().ToString());
        for(const auto& R:Ledger->View.Rows)Add(TEXT("TradeMap.Ledger.Good.")+R.Good.ToString(),TEXT("TradeMap.Root"),R.Label.ToString(),EHansaHudSemanticRole::ListItem,true,TEXT("milli-units"),R.Detail.ToString());

        Add(TEXT("TradeMap.Schedule"),TEXT("TradeMap.Root"),TEXT("Route schedule"),EHansaHudSemanticRole::Status,false);
        Add(TEXT("TradeMap.Schedule.OpenJourney"),TEXT("TradeMap.Schedule"),TEXT("Open journey"),EHansaHudSemanticRole::Button,Schedule->View.RouteId!=0);
        Add(TEXT("TradeMap.Schedule.Context"),TEXT("TradeMap.Schedule"),Schedule->View.Context.ToString(),EHansaHudSemanticRole::Status,false,TEXT("journey"),Schedule->View.Evidence.ToString());
        Add(TEXT("TradeMap.Schedule.Progress"),TEXT("TradeMap.Schedule"),TEXT("Journey progress"),EHansaHudSemanticRole::Status,false,TEXT("fraction"),Schedule->View.Progress.IsSet()?FString::SanitizeFloat(Schedule->View.Progress.GetValue()):TEXT("Unavailable"));
        for(const TCHAR* Tab:{TEXT("Stops"),TEXT("Cargo"),TEXT("Arrivals")})Add(TEXT("TradeMap.Schedule.Tab.")+FString(Tab),TEXT("TradeMap.Schedule"),Tab,EHansaHudSemanticRole::Tab,true,TEXT("schedule-view"),Tab,Schedule->Group==Tab);
        for(const auto& Row:Schedule->Rows)Add(Row->Data.Id,TEXT("TradeMap.Schedule"),Row->Data.Label.ToString(),EHansaHudSemanticRole::ListItem,true,Row->Data.Group,Row->Data.Detail.ToString()+TEXT("; ")+Row->Data.Evidence.ToString(),State.FocusedSemanticId==FName(*Row->Data.Id),Row->Data.bWarning);
        for(const TCHAR* Page:{TEXT("Map"),TEXT("Routes"),TEXT("Workspace"),TEXT("Schedule")})Add(TEXT("TradeMap.Page.")+FString(Page),TEXT("TradeMap.Root"),Page,EHansaHudSemanticRole::Tab,true,TEXT("page"),Page,State.WorkspacePage==Page,false,State.bCompact);
        Add(TEXT("TradeMap.Workspace.Status"),TEXT("TradeMap.Root"),Shell->WorkspaceStatus->GetText().ToString(),EHansaHudSemanticRole::Alert,false,FeedbackKind,Shell->WorkspaceStatus->GetText().ToString(),false,FeedbackKind==TEXT("stale")||FeedbackKind==TEXT("error"));
        Add(TEXT("TradeMap.Selection.Summary"),TEXT("TradeMap.Root"),ContextHost->SelectedRouteText->GetText().ToString(),EHansaHudSemanticRole::Status,false,TEXT("selection"),State.SelectedCityStableId.ToString());
        Add(TEXT("TradeMap.Chart.Summary"),TEXT("TradeMap.Canvas"),RegionalMap->MapSummary->GetText().ToString(),EHansaHudSemanticRole::Status,false,TEXT("map-overlay"),RegionalMap->AccessibleSummary().ToString());
        for(const auto& Requirement:State.PresenceRequirements){Add(TEXT("TradeMap.Presence.Requirement.")+Requirement.RequirementId,TEXT("TradeMap.Root"),Requirement.Description,EHansaHudSemanticRole::Status,false,TEXT("requirement"),FString::Printf(TEXT("%lld / %lld; %s"),Requirement.CurrentValue,Requirement.RequiredValue,Requirement.bMet?TEXT("Met"):TEXT("Unmet")));if(!Requirement.bMet)Add(TEXT("TradeMap.Presence.Locate.")+Requirement.RequirementId,TEXT("TradeMap.Root"),TEXT("Locate remedy for ")+Requirement.Description,EHansaHudSemanticRole::Button,true,TEXT("requirement-remedy"),Requirement.Description);}
        Add(TEXT("TradeMap.Editor.Discard"),TEXT("TradeMap.Root"),TEXT("Discard unsaved edits"),EHansaHudSemanticRole::Button,true);
        auto Order=GetControllerFocusOrder();
        // Disabled matrix cells still need semantic state, even though they are
        // excluded from keyboard/controller focus traversal.
        if(Pinned->bShipDetailOpen&&!Pinned->CargoEditor.bOpen&&Pinned->ShipDetailTab==TEXT("Route"))
            for(int32 Stop=0;Stop<Pinned->GetSlotDraft().Num();++Stop)for(int32 Slot=0;Slot<3;++Slot)for(const TCHAR* Direction:{TEXT("Unload"),TEXT("Load")})
                Order.AddUnique(FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"),Stop,Slot,Direction));
        if(Pinned->bShipDetailOpen)for(const auto& Id:Order) {
            FString Value=Id,Label=Id,ValueType=TEXT("route-control");
            if(Id==TEXT("TradeMap.Cargo.Quantity"))Value=LexToString(Pinned->CargoEditor.Draft.QuantityLimit.GetRawValue());
            if(Id==TEXT("TradeMap.Cargo.Reserve"))Value=LexToString(Pinned->CargoEditor.Draft.MinimumSourceReserve.GetRawValue());
            if(Id==TEXT("TradeMap.Cargo.Quantity")||Id==TEXT("TradeMap.Cargo.Reserve"))ValueType=TEXT("milliunits-input");
            TArray<FString> Parts;Id.ParseIntoArray(Parts,TEXT("."));
            if(Parts.Num()==5&&Parts[1]==TEXT("Cargo")&&Parts[2].IsNumeric()){
                const int32 Stop=FCString::Atoi(*Parts[2]),Slot=FCString::Atoi(*Parts[3]);const bool Load=Parts[4]==TEXT("Load");const auto Draft=Pinned->GetSlotDraft();
                if(Draft.IsValidIndex(Stop)){const auto* A=Draft[Stop].Actions.FindByPredicate([&](const auto& Action){return Action.CargoSlotIndex==Slot&&Hansa::Simulation::IsRouteLoad(Action.Kind)==Load;});Label=FString::Printf(TEXT("%s, %s, cargo slot %d, %s"),*Draft[Stop].CityId.ToString(),Load?TEXT("Load"):TEXT("Unload"),Slot+1,A?*Pinned->GetOrderGoodLabel(A->GoodId).ToString():TEXT("empty instruction"));Value=FString::Printf(TEXT("ship=%lld;stop=%d;slot=%d;action=%s;good=%s;quantityMilliUnits=%lld"),State.SelectedVehicleValue,Stop,Slot,*Parts[4],A?*A->GoodId.ToString():TEXT(""),A?A->QuantityLimit.GetRawValue():0);ValueType=TEXT("cargo-cell");}
            }
            Add(Id,TEXT("TradeMap.Root"),Label,EHansaHudSemanticRole::Button,true,ValueType,Value);
        }
        for(auto& Node:Nodes) {
            if(Node.Id.StartsWith(TEXT("TradeMap.City.City_"))) {
                const bool Visible=State.bOpen&&(!State.bCompact||State.WorkspacePage==TEXT("Map"));
                Node.State.bVisible=Visible;Node.bCanFocus=Visible;Node.bCanActivate=Visible&&!State.bCreating;Node.State.bEnabled=Node.bCanActivate;
                const auto Local=RegionalMap->MarkerPosition(FName(*Node.Id.RightChop(14).Replace(TEXT("_"),TEXT("."))));
                const auto G=RegionalMap->FocusWidget()->GetCachedGeometry();const auto A=G.LocalToAbsolute(Local-FVector2D(24,24)),B=G.LocalToAbsolute(Local+FVector2D(24,24));
                if(Visible&&Local.X>=0&&Local.Y>=0&&Local.X<=G.GetLocalSize().X&&Local.Y<=G.GetLocalSize().Y)Node.Bounds=FIntRect(FMath::RoundToInt(A.X),FMath::RoundToInt(A.Y),FMath::RoundToInt(B.X),FMath::RoundToInt(B.Y));
                continue;
            }
            if(Node.Id.StartsWith(TEXT("TradeMap.Ledger.")))Node.State.bVisible&=State.ActiveSection==TEXT("Ledger")&&ResolveSemanticWidget(Node.Id).IsValid();
            if(Node.Id==TEXT("TradeMap.Workspace.Status"))Node.State.bError=FeedbackKind==TEXT("error");
            Node.State.bLoading=Pinned->IsAnyTradeCommandPending()&&(Node.Id.StartsWith(TEXT("TradeMap.Recovery."))||Node.Id.StartsWith(TEXT("TradeMap.Orders."))||Node.Id.StartsWith(TEXT("TradeMap.Station."))||Node.Id.StartsWith(TEXT("TradeMap.Presence."))||Node.Id.StartsWith(TEXT("TradeMap.Decisions."))||Node.Id.StartsWith(TEXT("TradeMap.Editor.")));
            if((Node.Id==TEXT("TradeMap.New")||Node.Id==TEXT("TradeMap.Construction.Place")||Node.Id==TEXT("TradeMap.Construction.Visit")||Node.Id==TEXT("TradeMap.Editor.Visit"))&&Pinned->IsRemoteView()){Node.State.ValueType=TEXT("unavailable-reason");Node.State.Value=Pinned->RemoteActionReason(Node.Id).ToString();}
            if(Node.Id.StartsWith(TEXT("TradeMap.Navigate.")))Node.State.bSelected=Node.Id==TEXT("TradeMap.Navigate.")+(State.ActiveSection==TEXT("Route")?TEXT("Overview"):State.ActiveSection);
            if(Node.Id.StartsWith(TEXT("TradeMap.Presence.")))Node.State.bVisible&=!State.bCreating&&(Node.Id.StartsWith(TEXT("TradeMap.Presence.Specialization."))?State.ActiveSection==TEXT("Specialization"):(State.ActiveSection==TEXT("Presence")||State.ActiveSection==TEXT("StationUpgrade")));
            if(Node.bCanFocus) {Node.bCanFocus=Order.Contains(Node.Id)&&Node.State.bEnabled;Node.bCanActivate=Node.bCanFocus;Node.State.bEnabled=Node.bCanFocus;}
            if(Node.Id==TEXT("TradeMap.Recovery.Detail")){Node.bCanFocus=State.bOpen&&State.ActiveSection==TEXT("Recovery");Node.bCanActivate=false;Node.State.bEnabled=true;}
            if(Node.Id.StartsWith(TEXT("TradeMap.Recovery.")))Node.State.bVisible&=State.ActiveSection==TEXT("Recovery");
            if(Node.Id==TEXT("TradeMap.Decisions.Terms")){Node.bCanFocus=State.bOpen&&State.ActiveSection==TEXT("Decisions")&&!State.DecisionId.IsEmpty();Node.bCanActivate=false;Node.State.bEnabled=true;}
            if(Node.Id.StartsWith(TEXT("TradeMap.Creator.")))Node.State.bVisible=State.bOpen&&(State.bCreating||((Node.Id==TEXT("TradeMap.Creator.City")||Node.Id==TEXT("TradeMap.Creator.Good")||Node.Id==TEXT("TradeMap.Creator.Add")||Node.Id==TEXT("TradeMap.Creator.Remove"))&&Model.IsValid()&&Model->CanEditStops()));
            if(auto Widget=ResolveSemanticWidget(Node.Id)) {
                Node.State.bEnabled &= Widget->IsEnabled();Node.bCanActivate &= Node.State.bEnabled;Node.bCanFocus &= Node.State.bEnabled;
                bool Visible=State.bOpen; auto Parent=Widget;
                bool Popup=false;
                while(Parent.IsValid()&&Parent.Get()!=this){if(Parent==ContextHost->SectionMenuScroll){Visible&=ContextHost->SectionMenu->IsOpen();Popup=true;break;}if(Parent==Directory->DirectoryToolbar){Visible&=Directory->MoreMenu->IsOpen();Popup=true;break;}if(Parent==RegionalMap->MapToolbar){Visible&=RegionalMap->ToolsMenu->IsOpen();Popup=true;break;}Visible &= Parent->GetVisibility().IsVisible();Node.State.bEnabled &= Parent->IsEnabled();Parent=Parent->GetParentWidget();}
                Visible &= Popup||Parent.Get()==this;
                const auto G=Widget->GetCachedGeometry();const auto A=G.GetAbsolutePosition();const auto Z=G.GetAbsoluteSize();
                FVector2D Min=A,Max=A+Z;
                TArray<TSharedPtr<SWidget>> ClipRegions;for(const auto& Region:GetScrollRegions())ClipRegions.Add(Region);ClipRegions.Add(Directory->RouteList);ClipRegions.Add(Schedule->List);if(auto Table=ResolveSemanticWidget(TEXT("TradeMap.Ledger.Table")))ClipRegions.Add(Table);
                for(const auto& Scroll:ClipRegions)
                if(Scroll&&Node.Id!=TEXT("TradeMap.Root")&&Node.Id!=TEXT("TradeMap.Canvas")&&Node.Id!=TEXT("TradeMap.Close")&&Node.Id!=TEXT("TradeMap.New")&&Node.Id!=TEXT("TradeMap.Mode.Filter")) {
                    bool Descendant=false;auto Ancestor=Widget;while(Ancestor){if(Ancestor==Scroll){Descendant=true;break;}Ancestor=Ancestor->GetParentWidget();}
                    if(Descendant){const auto Clip=Scroll->GetCachedGeometry();const auto C=Clip.GetAbsolutePosition(),D=C+Clip.GetAbsoluteSize();Min.X=FMath::Max(Min.X,C.X);Min.Y=FMath::Max(Min.Y,C.Y);Max.X=FMath::Min(Max.X,D.X);Max.Y=FMath::Min(Max.Y,D.Y);}
                }
                Visible &= Max.X>Min.X&&Max.Y>Min.Y;
                Node.State.bClipped=Min!=A||Max!=A+Z;
                Node.State.bVisible &= Visible;
                if(Visible)Node.Bounds=FIntRect(FMath::RoundToInt(Min.X),FMath::RoundToInt(Min.Y),FMath::RoundToInt(Max.X),FMath::RoundToInt(Max.Y));
                if(Node.Id==TEXT("TradeMap.Creator.Name")){Node.Label=TEXT("Route name");Node.State.Value=State.DraftName;Node.bCanActivate=false;}
                if(Node.Id==TEXT("TradeMap.Creator.Cog"))Node.Label=State.CogLabel.ToString();
            }
            // Action capability is independent of clipping; consumers also inspect bVisible and Bounds.
            Node.bCanActivate&=Node.State.bEnabled;Node.bCanFocus&=Node.State.bEnabled;
        }
        Nodes.RemoveAll([](const FHansaHudSemanticNode& Node){return Node.Id.StartsWith(TEXT("TradeMap.Editor."))||Node.Id.StartsWith(TEXT("TradeMap.Stop."))||Node.Id.StartsWith(TEXT("TradeMap.Creator."));});
        return Nodes;
	}
    TArray<TSharedPtr<SScrollBox>> SHansaTradeMap::GetScrollRegions() const { auto Result=ContextHost->Scrolls();Result.Add(Orders->ListScroll);Result.Add(Orders->EditorScroll);Result.Add(Feedback->ValidationScroll);Result.Add(RegionalMap->MapToolbar);Result.Add(Directory->DirectoryToolbar);Result.Add(Directory->PortScroll);Result.Add(Ledger->DetailsScroll);Result.Add(Ledger->Horizontal);Result.Add(Specialization->Scroll);Result.Add(Construction->Scroll);Result.Add(Decisions->Scroll);Result.Add(Recovery->Scroll);if(ShipDetail)Result.Append(ShipDetail->Scrolls);return Result; }

	void SHansaTradeMap::MapWidget(const FString& Id,const TSharedPtr<SWidget>& W){SemanticWidgets.Add(Id,W);}
}

#undef LOCTEXT_NAMESPACE
