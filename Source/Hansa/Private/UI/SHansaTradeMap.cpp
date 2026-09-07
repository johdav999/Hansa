#include "UI/SHansaTradeMap.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
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
	class STradeRouteCanvas final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STradeRouteCanvas){} SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		void SetSnapshot(const FHansaTradeMapSnapshot& In){Snapshot=In;Invalidate(EInvalidateWidgetReason::Paint);}
		virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(720,520);}
		virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled)const override
		{
			(void)Args;(void)Cull;(void)Style;(void)ParentEnabled;
			const FVector2D Size=Geometry.GetLocalSize();
			auto Pos=[&](FName Id){const auto* C=Snapshot.Cities.FindByPredicate([Id](const auto& X){return X.StableId==Id;});return C?C->NormalizedPosition*Size:FVector2D::ZeroVector;};
			for(const auto& Route:Snapshot.Routes)
			{
				TArray<FVector2D> Points; const bool Selected=Route.RouteValue==Snapshot.SelectedRouteValue;
				const auto* SourceStops=Selected?&Snapshot.Stops:nullptr;
				if(SourceStops)for(const auto& Stop:*SourceStops)Points.Add(Pos(Stop.CityStableId));
				if (Points.Num() > 1)
				{
					// UE 5.8 rejects Add(Array[...]) because the source aliases the container being modified.
					const FVector2D ClosingPoint = Points[0];
					Points.Add(ClosingPoint);
					const FLinearColor Color = Route.bReserveRisk
						? UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::WarningAmber)
						: UHansaUiStyleLibrary::GetColor(Route.bSea ? EHansaUiColorToken::BalticBlue : EHansaUiColorToken::Oak);
					FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Selected ? 5.f : 3.f);
				}
			}
			for(const auto& City:Snapshot.Cities)
			{
				const FVector2D P=Pos(City.StableId);const FLinearColor Color=City.bUnknown?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk):City.bStale?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::WarningAmber):UHansaUiStyleLibrary::GetColor(City.bOwned?EHansaUiColorToken::Brass:EHansaUiColorToken::Chalk);
				TArray<FVector2D> Ring;for(int32 I=0;I<=16;++I){const float A=2.f*PI*I/16.f;Ring.Add(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*10.f);}FSlateDrawElement::MakeLines(Out,Layer+1,Geometry.ToPaintGeometry(),Ring,ESlateDrawEffect::None,Color,true,3.f);
				FSlateDrawElement::MakeText(Out, Layer + 2,
					Geometry.ToPaintGeometry(FVector2f(180.0f, 24.0f), FSlateLayoutTransform(FVector2f(P.X + 14.0f, P.Y - 12.0f))),
					City.Label, UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Data), ESlateDrawEffect::None, Color);
			}
			return Layer+3;
		}
	private: FHansaTradeMapSnapshot Snapshot;
	};

	SHansaTradeMap::~SHansaTradeMap(){if(auto* P=Model.Get())P->OnChanged().Remove(ChangedHandle);}
	void SHansaTradeMap::Construct(const FArguments& A)
	{
		Model=A._Model;PresentationSize=A._InitialViewportSize;
		WorkingBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working); FloatingBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Floating); OverlayBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		PrimaryButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary); SecondaryButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary); IconButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Icon);
		LightBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,false); LightCaptionStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption,false); DarkBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,true); LightHeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,false); HeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1,true);
		ChildSlot[SAssignNew(PresentationBox,SBox).WidthOverride(PresentationSize.X).HeightOverride(PresentationSize.Y)[SNew(SBorder).BorderImage(&OverlayBrush).Padding(16)[SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(RouteTitle,STextBlock).Text(LOCTEXT("Title","European trade")).TextStyle(&HeadingStyle)]+SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Mode.Filter")))[SAssignNew(ModeText,STextBlock).TextStyle(&DarkBodyStyle)]]+SHorizontalBox::Slot().AutoWidth().Padding(4)[SNew(SButton).ButtonStyle(&IconButtonStyle).Text(LOCTEXT("Close","Close ×")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Close")))]]
			+SVerticalBox::Slot().FillHeight(1).Padding(0,8)[SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(.23f).Padding(0,0,8,0)[SNew(SBorder).BorderImage(&WorkingBrush).Padding(12)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(RouteList,SVerticalBox)]]]
				+SHorizontalBox::Slot().FillWidth(.49f)[SAssignNew(CanvasHost,SBox)[SAssignNew(RouteCanvas,STradeRouteCanvas)]]
				+SHorizontalBox::Slot().FillWidth(.28f).Padding(8,0,0,0)[SNew(SBorder).BorderImage(&WorkingBrush).Padding(12)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Simple","Simple route editor")).TextStyle(&LightBodyStyle)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(StopList,SVerticalBox)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Action","Load / unload")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Action.Cycle")))]+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Minus","− qty")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Quantity.Decrease")))]+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Plus","+ qty")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Quantity.Increase")))]]
					+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("ReserveMinus","− reserve")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Reserve.Decrease")))]+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("ReservePlus","+ reserve")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Reserve.Increase")))]]
					+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("MoveUp","Move stop up")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Stop.Up")))]+SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("MoveDown","Move stop down")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Stop.Down")))]]
					+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(ReserveRisk,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteMetrics,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Save","Save route")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.Save")))]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(RouteStateCard,SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"))).Padding(10)[SNew(SVerticalBox)
						+SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteStateHeading,STextBlock).TextStyle(&LightHeadingStyle)]
						+SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)[SAssignNew(RouteStateDetail,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ToggleActiveButton,SButton).ButtonStyle(&PrimaryButtonStyle).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.ToggleActive")))[SAssignNew(ToggleActiveText,STextBlock).TextStyle(&DarkBodyStyle).Justification(ETextJustify::Center)]]
					+SVerticalBox::Slot().AutoHeight()[SAssignNew(ToggleActiveHint,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SAssignNew(EditorStatus,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]]]
			+SVerticalBox::Slot().AutoHeight()[SAssignNew(BottomPanel,SBorder).BorderImage(&FloatingBrush).Padding(10)[SNew(STextBlock).Text(LOCTEXT("Legend","━━ sea route    ┄┄ land route    ⚠ reserve risk    dashed/estimated information grows less certain with report age")).TextStyle(&DarkBodyStyle)]]]]];
		MapWidget(TEXT("TradeMap.Root"),SharedThis(this));MapWidget(TEXT("TradeMap.Close"),SharedThis(this));MapWidget(TEXT("TradeMap.Mode.Filter"),SharedThis(this));MapWidget(TEXT("TradeMap.Canvas"),CanvasHost);MapWidget(TEXT("TradeMap.Editor.Action.Cycle"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Quantity.Decrease"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Quantity.Increase"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Reserve.Decrease"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Reserve.Increase"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Stop.Up"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Stop.Down"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.Save"),SharedThis(this));MapWidget(TEXT("TradeMap.Editor.RouteState"),RouteStateCard);MapWidget(TEXT("TradeMap.Editor.ToggleActive"),ToggleActiveButton);
		if(auto* P=Model.Get()){ChangedHandle=P->OnChanged().AddSP(SharedThis(this),&SHansaTradeMap::Refresh);Refresh(P->GetSnapshot(),P->GetRevision());}
	}
	void SHansaTradeMap::SetPresentationSize(FIntPoint S){PresentationSize=S;if(PresentationBox){PresentationBox->SetWidthOverride(S.X);PresentationBox->SetHeightOverride(S.Y);}if(auto* P=Model.Get())P->SetCompact(S.X<1500||S.Y<850);}
	void SHansaTradeMap::Refresh(const FHansaTradeMapSnapshot& S,uint64)
	{
		RouteTitle->SetText(S.Title);
		ModeText->SetText(S.ModeFilter==EHansaTradeMapModeFilter::All?LOCTEXT("All","All routes"):S.ModeFilter==EHansaTradeMapModeFilter::Sea?LOCTEXT("SeaOnly","Sea routes"):LOCTEXT("LandOnly","Land routes"));
		RebuildRoutes(S);RebuildStops(S);if(RouteCanvas)RouteCanvas->SetSnapshot(S);
		const auto* R=S.Routes.FindByPredicate([&](const auto& X){return X.RouteValue==S.SelectedRouteValue;});
		RouteMetrics->SetText(R?FText::Format(LOCTEXT("Metrics","{0}\n{1}\nCapacity: {2}\nUpkeep: {3}\nExpected profit: {4}\n{5}"),R->StopSummary,R->RoundTripTime,R->Capacity,R->Upkeep,R->ExpectedProfitRange,R->Uncertainty):FText());
		ReserveRisk->SetText(S.ReserveRisk);
		RouteStateHeading->SetText(R?R->StateHeading:FText());
		RouteStateDetail->SetText(R?R->StateDetail:FText());
		ToggleActiveText->SetText(R?R->ToggleActionLabel:FText());
		ToggleActiveHint->SetText(R?R->ToggleActionHint:FText());
		ToggleActiveButton->SetEnabled(R&&R->bCanToggleActive);
		ToggleActiveButton->SetButtonStyle(R&&R->bActive?&SecondaryButtonStyle:&PrimaryButtonStyle);
		const FLinearColor StateColor=!R?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment)
			:R->bTraveling?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticBlue)
			:R->bActive?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal)
			:R->bOwnedByPlayer?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment)
			:UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk);
		RouteStateCard->SetBorderBackgroundColor(StateColor.CopyWithNewOpacity(R&&R->bActive?.24f:.36f));
		EditorStatus->SetText(S.EditorStatus);
		BottomPanel->SetVisibility(S.bCompact?EVisibility::Collapsed:EVisibility::Visible);
	}
	void SHansaTradeMap::RebuildRoutes(const FHansaTradeMapSnapshot& S){RouteList->ClearChildren();for(const auto& R:S.Routes){const FString Id=FString::Printf(TEXT("TradeMap.Route.%lld"),R.RouteValue);TSharedPtr<SButton>B;RouteList->AddSlot().AutoHeight().Padding(0,3)[SAssignNew(B,SButton).ButtonStyle(R.RouteValue==S.SelectedRouteValue?&PrimaryButtonStyle:&SecondaryButtonStyle).OnClicked(this,&SHansaTradeMap::Invoke,Id)[SNew(STextBlock).Text(FText::Format(LOCTEXT("RouteRow","{0}\n{1} · {2}\n{3}"),R.Label,R.Mode,R.State,R.StopSummary)).AutoWrapText(true)]];MapWidget(Id,B);}}
	void SHansaTradeMap::RebuildStops(const FHansaTradeMapSnapshot& S){StopList->ClearChildren();for(const auto& Stop:S.Stops){const FString Id=FString::Printf(TEXT("TradeMap.Stop.%d"),Stop.Index);TSharedPtr<SButton>B;StopList->AddSlot().AutoHeight().Padding(0,3)[SAssignNew(B,SButton).ButtonStyle(Stop.Index==S.SelectedStopIndex?&PrimaryButtonStyle:&SecondaryButtonStyle).OnClicked(this,&SHansaTradeMap::Invoke,Id)[SNew(STextBlock).Text(Stop.AccessibleLabel).AutoWrapText(true)]];MapWidget(Id,B);}}
	FReply SHansaTradeMap::Invoke(const FString Id){return ActivateSemanticId(Id)?FReply::Handled():FReply::Unhandled();}
	bool SHansaTradeMap::ActivateSemanticId(const FString& Id){auto* P=Model.Get();if(!P)return false;if(Id==TEXT("TradeMap.Close"))return P->CloseIntent();if(Id==TEXT("TradeMap.Mode.Filter"))return P->CycleModeFilterIntent();if(Id==TEXT("TradeMap.Editor.Action.Cycle"))return P->CycleCargoActionIntent();if(Id==TEXT("TradeMap.Editor.Quantity.Decrease"))return P->AdjustQuantityIntent(-5000);if(Id==TEXT("TradeMap.Editor.Quantity.Increase"))return P->AdjustQuantityIntent(5000);if(Id==TEXT("TradeMap.Editor.Reserve.Decrease"))return P->AdjustMinimumReserveIntent(-5000);if(Id==TEXT("TradeMap.Editor.Reserve.Increase"))return P->AdjustMinimumReserveIntent(5000);if(Id==TEXT("TradeMap.Editor.Stop.Up"))return P->MoveStopIntent(-1);if(Id==TEXT("TradeMap.Editor.Stop.Down"))return P->MoveStopIntent(1);if(Id==TEXT("TradeMap.Editor.Save"))return P->CommitIntent();if(Id==TEXT("TradeMap.Editor.ToggleActive"))return P->ToggleActiveIntent();if(Id.StartsWith(TEXT("TradeMap.Route.")))return P->SelectRouteIntent(FCString::Atoi64(*Id.RightChop(15)));if(Id.StartsWith(TEXT("TradeMap.Stop.")))return P->SelectStopIntent(FCString::Atoi(*Id.RightChop(14)));return false;}
	bool SHansaTradeMap::FocusSemanticId(const FString& Id){auto* P=Model.Get();const auto* Found=SemanticWidgets.Find(Id);const TSharedPtr<SWidget> Widget=Found?Found->Pin():nullptr;if(!P||!Widget.IsValid()||!Widget->IsEnabled())return false;P->SetFocusedSemanticId(FName(*Id));if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);return true;}
	TArray<FString> SHansaTradeMap::GetControllerFocusOrder()const{TArray<FString> R={TEXT("TradeMap.Close"),TEXT("TradeMap.Mode.Filter")};const FHansaTradeMapRoutePresentation* Selected=nullptr;if(auto* P=Model.Get()){for(const auto& X:P->GetSnapshot().Routes){R.Add(FString::Printf(TEXT("TradeMap.Route.%lld"),X.RouteValue));if(X.RouteValue==P->GetSnapshot().SelectedRouteValue)Selected=&X;}for(const auto& X:P->GetSnapshot().Stops)R.Add(FString::Printf(TEXT("TradeMap.Stop.%d"),X.Index));}R.Append({TEXT("TradeMap.Editor.Action.Cycle"),TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("TradeMap.Editor.Quantity.Increase"),TEXT("TradeMap.Editor.Reserve.Decrease"),TEXT("TradeMap.Editor.Reserve.Increase"),TEXT("TradeMap.Editor.Stop.Up"),TEXT("TradeMap.Editor.Stop.Down"),TEXT("TradeMap.Editor.Save")});if(Selected&&Selected->bCanToggleActive)R.Add(TEXT("TradeMap.Editor.ToggleActive"));return R;}
	FReply SHansaTradeMap::OnKeyDown(const FGeometry& MyGeometry,const FKeyEvent& InKeyEvent){(void)MyGeometry;const EHansaUiNavigationIntent Intent=ClassifyNavigationIntent(InKeyEvent);auto* P=Model.Get();if(!P)return FReply::Unhandled();if(Intent==EHansaUiNavigationIntent::Back)return P->CloseIntent()?FReply::Handled():FReply::Unhandled();if(Intent==EHansaUiNavigationIntent::Activate)return ActivateSemanticId(P->GetSnapshot().FocusedSemanticId.ToString())?FReply::Handled():FReply::Unhandled();if(Intent==EHansaUiNavigationIntent::Next||Intent==EHansaUiNavigationIntent::Previous){const FString Target=FindWrappedFocusTarget(GetControllerFocusOrder(),P->GetSnapshot().FocusedSemanticId.ToString(),Intent==EHansaUiNavigationIntent::Next);return !Target.IsEmpty()&&FocusSemanticId(Target)?FReply::Handled():FReply::Unhandled();}return FReply::Unhandled();}
	TArray<FHansaHudSemanticNode> SHansaTradeMap::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaTradeMapPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return Nodes;
		const FHansaTradeMapSnapshot& State = Pinned->GetSnapshot();
		auto Add = [&Nodes, &State](FString Id, FString Parent, FString Label, EHansaHudSemanticRole Role,
			bool bActivate = false, FString ValueType = {}, FString Value = {}, bool bSelected = false, bool bWarning = false,
			bool bEnabled = true)
		{
			FHansaHudSemanticNode Node;
			Node.Id = MoveTemp(Id); Node.ParentId = MoveTemp(Parent); Node.Label = MoveTemp(Label); Node.Role = Role;
			Node.bCanActivate = bActivate; Node.bCanFocus = bActivate; Node.State.bEnabled = bEnabled; Node.State.bVisible = State.bOpen;
			Node.State.bSelected = bSelected; Node.State.bWarning = bWarning; Node.State.bFocused = State.FocusedSemanticId == FName(*Node.Id);
			Node.State.ValueType = MoveTemp(ValueType); Node.State.Value = MoveTemp(Value); Nodes.Add(MoveTemp(Node));
		};
		Add(TEXT("TradeMap.Root"), TEXT("HUD.TradeMapHost"), State.Title.ToString(), EHansaHudSemanticRole::Panel,
			false, TEXT("layout"), State.bCompact ? TEXT("compact") : TEXT("wide"));
		Add(TEXT("TradeMap.Close"), TEXT("TradeMap.Root"), TEXT("Close trade map"), EHansaHudSemanticRole::Button, true);
		Add(TEXT("TradeMap.Mode.Filter"), TEXT("TradeMap.Root"), TEXT("Filter route mode"), EHansaHudSemanticRole::Button,
			true, TEXT("mode"), FString::FromInt(static_cast<int32>(State.ModeFilter)));
		Add(TEXT("TradeMap.Canvas"), TEXT("TradeMap.Root"), TEXT("Four-city European trade map"), EHansaHudSemanticRole::Panel,
			false, TEXT("geometry"), TEXT("native"));
		for (const FHansaTradeMapCityPresentation& City : State.Cities)
		{
			Add(FString::Printf(TEXT("TradeMap.City.%s"), *City.StableId.ToString().Replace(TEXT("."), TEXT("_"))),
				TEXT("TradeMap.Canvas"), City.Label.ToString(), EHansaHudSemanticRole::Status, false, TEXT("report"),
				City.Information.ToString(), false, City.bStale || City.bUnknown);
		}
		for (const FHansaTradeMapRoutePresentation& Route : State.Routes)
		{
			Add(FString::Printf(TEXT("TradeMap.Route.%lld"), Route.RouteValue), TEXT("TradeMap.Root"), Route.Label.ToString(),
				EHansaHudSemanticRole::ListItem, true, TEXT("route"),
				FString::Printf(TEXT("%s;%s;%s;%s"), *Route.StopSummary.ToString(), *Route.RoundTripTime.ToString(),
					*Route.ExpectedProfitRange.ToString(), *Route.Uncertainty.ToString()),
				Route.RouteValue == State.SelectedRouteValue, Route.bReserveRisk);
		}
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
		const FHansaTradeMapRoutePresentation* SelectedRoute = State.Routes.FindByPredicate(
			[&State](const FHansaTradeMapRoutePresentation& Route){return Route.RouteValue==State.SelectedRouteValue;});
		if (SelectedRoute != nullptr)
		{
			Add(TEXT("TradeMap.Editor.RouteState"), TEXT("TradeMap.Root"), SelectedRoute->StateHeading.ToString(),
				EHansaHudSemanticRole::Status, false, TEXT("route-state"),
				FString::Printf(TEXT("%s;%s;%s"), *SelectedRoute->State.ToString(), *SelectedRoute->StateDetail.ToString(), *SelectedRoute->Ownership.ToString()));
			Add(TEXT("TradeMap.Editor.ToggleActive"), TEXT("TradeMap.Root"), SelectedRoute->ToggleActionLabel.ToString(),
				EHansaHudSemanticRole::Button, SelectedRoute->bCanToggleActive, TEXT("route-action"),
				SelectedRoute->ToggleActionHint.ToString(), false, false, SelectedRoute->bCanToggleActive);
		}
		Add(TEXT("TradeMap.Editor.ReserveRisk"), TEXT("TradeMap.Root"), State.ReserveRisk.ToString(), EHansaHudSemanticRole::Alert,
			false, TEXT("risk"), State.ReserveRisk.ToString(), false, State.ReserveRisk.ToString().StartsWith(TEXT("⚠")));
		return Nodes;
	}
	void SHansaTradeMap::MapWidget(const FString& Id,const TSharedPtr<SWidget>& W){SemanticWidgets.Add(Id,W);}
}

#undef LOCTEXT_NAMESPACE
