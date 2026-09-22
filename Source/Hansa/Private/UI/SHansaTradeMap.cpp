#include "UI/SHansaTradeMap.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "UI/SHansaReferenceFrame.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
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
    #include "HansaTradeChartData.inl"
    class STradeRouteCanvas final : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STradeRouteCanvas){} SLATE_ARGUMENT(FUiPreferences,Preferences) SLATE_ARGUMENT(UHansaTradeMapPresentationModel*,Model) SLATE_END_ARGS()
        void Construct(const FArguments& A){Preferences=A._Preferences;Model=A._Model;SetClipping(EWidgetClipping::ClipToBounds);}
        void SetSnapshot(const FHansaTradeMapSnapshot& In){Snapshot=In;Invalidate(EInvalidateWidgetReason::Paint);}
        virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(720,520);}
        FVector2D ChartSize(const FVector2D Size)const{const double Width=FMath::Min(Size.X,Size.Y*1.68);return FVector2D(Width,Width/1.68)*Zoom;}
        FVector2D Point(const FVector2D P,const FVector2D Size)const{return (P-FVector2D(.5,.5))*ChartSize(Size)+Size*.5+Pan;}
        void ChangeZoom(float Delta){Zoom=FMath::Clamp(Zoom+Delta,1.f,6.f);if(Zoom==1.f)Pan=FVector2D::ZeroVector;Invalidate(EInvalidateWidgetReason::Paint);}
        void ResetView(){Zoom=1;Pan=FVector2D::ZeroVector;Invalidate(EInvalidateWidgetReason::Paint);}
        virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& E)override{ChangeZoom(E.GetWheelDelta()*.25f);return FReply::Handled();}
        virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)override
        {
            if(E.GetEffectingButton()==EKeys::RightMouseButton)return FReply::Handled().CaptureMouse(SharedThis(this));
            if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
            const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
            const FHansaTradeMapCityPresentation* Nearest=nullptr;double Distance=24*24;
            for(const auto& City:Snapshot.Cities){const double D=(Point(City.NormalizedPosition,G.GetLocalSize())-P).SizeSquared();if(D<Distance){Distance=D;Nearest=&City;}}
            if(Nearest&&Model.IsValid()){Model->SelectCityIntent(Nearest->StableId);return FReply::Handled();}
            return FReply::Unhandled();
        }
        virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E)override
        {
            if(!HasMouseCapture())return FReply::Unhandled();
            Pan+=E.GetCursorDelta()/G.GetAccumulatedLayoutTransform().GetScale();
            const auto Limit=ChartSize(G.GetLocalSize())*.5;Pan.X=FMath::Clamp(Pan.X,-Limit.X,Limit.X);Pan.Y=FMath::Clamp(Pan.Y,-Limit.Y,Limit.Y);
            Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
        }
        virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& E)override{return E.GetEffectingButton()==EKeys::RightMouseButton?FReply::Handled().ReleaseMouseCapture():FReply::Unhandled();}
        virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override
        {
            const auto Size=G.GetLocalSize();const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            auto Color=[](EHansaUiColorToken T){return UHansaUiStyleLibrary::GetColor(T);};
            auto Line=[&](const TArray<FVector2D>& P,int32 L,FLinearColor C,float Width=1.f){FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Width);};
            auto Label=[&](FVector2D P,FText Text,FLinearColor C,EHansaUiTypographyToken Font=EHansaUiTypographyToken::Caption){FSlateDrawElement::MakeText(Out,Layer+6,G.ToPaintGeometry(FVector2f(240,32),FSlateLayoutTransform(FVector2f(P))),Text,GetComponentFont(Font,Preferences),ESlateDrawEffect::None,C);};
            FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),White,ESlateDrawEffect::None,Color(EHansaUiColorToken::BalticNavy));
            TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;Vertices.Reserve(UE_ARRAY_COUNT(TradeLandVertices));Indices.Reserve(UE_ARRAY_COUNT(TradeLandVertices));
            const FColor Land=Color(EHansaUiColorToken::HarborSlate).ToFColor(true);
            for(const auto& P:TradeLandVertices){Indices.Add(Vertices.Num());Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(Point(FVector2D(P),Size)),FVector2f(.5,.5),Land));}
            const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
            FSlateDrawElement::MakeCustomVerts(Out,Layer+1,Resource,Vertices,Indices,nullptr,0,0);
            for(const auto& Ring:TradeCoastlines){TArray<FVector2D> P;P.Reserve(Ring.Num());for(const auto& V:Ring)P.Add(Point(V,Size));Line(P,Layer+2,Color(EHansaUiColorToken::Brass).CopyWithNewOpacity(.55f));}
            for(int I=1;I<8;++I){const double V=I/8.;Line({Point({V,0},Size),Point({V,1},Size)},Layer+2,Color(EHansaUiColorToken::Chalk).CopyWithNewOpacity(.06f));Line({Point({0,V},Size),Point({1,V},Size)},Layer+2,Color(EHansaUiColorToken::Chalk).CopyWithNewOpacity(.06f));}
            auto CityPoint=[&](FName Id){const auto* City=Snapshot.Cities.FindByPredicate([&](const auto& C){return C.StableId==Id;});return City?Point(City->NormalizedPosition,Size):FVector2D::ZeroVector;};
            for(int32 Leg=0;Leg+1<Snapshot.Stops.Num();++Leg)
            {
                const auto A=CityPoint(Snapshot.Stops[Leg].CityStableId),B=CityPoint(Snapshot.Stops[Leg+1].CityStableId);
                const auto Mid=(A+B)*.5;
                auto Curve=[&](double T){return A*(1-T)*(1-T)+Mid*(2*T*(1-T))+B*T*T;};
                for(int I=0;I<40;I+=2)Line({Curve(I/40.),Curve((I+1)/40.)},Layer+3,Color(EHansaUiColorToken::Chalk),2.f);
            }
            if(Snapshot.bShipInTransit){const auto P=Point(Snapshot.ShipPosition,Size);FSlateDrawElement::MakeBox(Out,Layer+4,G.ToPaintGeometry(FVector2f(28,28),FSlateLayoutTransform(FVector2f(P-FVector2D(14,14)))),GetGeneratedIconBrush(EUiGlyph::Ship,FMath::CeilToInt(28*G.GetAccumulatedLayoutTransform().GetScale())),ESlateDrawEffect::None,FLinearColor::White);}
            TArray<FSlateRect> Labels;
            // Selected city gets label priority. Other overlapping labels appear as the chart is zoomed.
            for(int Pass=0;Pass<2;++Pass)for(const auto& City:Snapshot.Cities)
            {
                const bool Selected=City.StableId==Snapshot.SelectedCityStableId;if(Selected!=(Pass==0))continue;
                const auto P=Point(City.NormalizedPosition,Size);if(P.X<8||P.Y<8||P.X>Size.X-8||P.Y>Size.Y-8)continue;
                const float Radius=Selected?9:City.bRendered?6:4;TArray<FVector2D> Ring;
                for(int I=0;I<=24;++I){const double A=2*PI*I/24;Ring.Add(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius);}
                Line(Ring,Layer+4,Color(Selected?EHansaUiColorToken::Brass:EHansaUiColorToken::Chalk),Selected?3:1.5f);
                const FVector2D L(FMath::Clamp(P.X+12,4.,FMath::Max(4.,Size.X-150)),FMath::Clamp(P.Y-12,30.,FMath::Max(30.,Size.Y-42)));
                const FSlateRect Rect(L.X,L.Y,L.X+145,L.Y+30);
                if(!Selected&&Labels.ContainsByPredicate([&](const auto& R){return FSlateRect::DoRectanglesIntersect(R,Rect);}))continue;
                Labels.Add(Rect);
                FSlateDrawElement::MakeBox(Out,Layer+5,G.ToPaintGeometry(FVector2f(145,30),FSlateLayoutTransform(FVector2f(L))),White,ESlateDrawEffect::None,Color(EHansaUiColorToken::BalticNavy).CopyWithNewOpacity(.9f));
                Label(L+FVector2D(5,4),City.Label,Color(Selected?EHansaUiColorToken::Brass:EHansaUiColorToken::Chalk));
            }
            Label({16,12},LOCTEXT("GeographicChart","BALTIC & NORTH SEA"),Color(EHansaUiColorToken::Chalk),EHansaUiTypographyToken::Heading2);
            Label({16,Size.Y-30},LOCTEXT("MapHelp","Select a city · Wheel to zoom · Right-drag to pan"),Color(EHansaUiColorToken::Chalk));
            return Layer+6;
        }
    private:
        FHansaTradeMapSnapshot Snapshot;FUiPreferences Preferences;TWeakObjectPtr<UHansaTradeMapPresentationModel> Model;float Zoom=1.f;FVector2D Pan=FVector2D::ZeroVector;
    };

	SHansaTradeMap::~SHansaTradeMap(){if(auto* P=Model.Get())P->OnChanged().Remove(ChangedHandle);}
	void SHansaTradeMap::Construct(const FArguments& A)
	{
		Model=A._Model;Preferences=A._Preferences;PresentationSize=A._InitialViewportSize;
		WorkingBrush=GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush; FloatingBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Floating); OverlayBrush=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		PrimaryButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary); SecondaryButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary); IconButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Icon);
		LightBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,false); LightBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)); LightCaptionStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption,false); LightCaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences)); DarkBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,true); DarkBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)); LightHeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,false); LightHeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences)); HeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1,true); HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
        RouteNameStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
        RouteNameStyle.SetBackgroundImageNormal(WorkingBrush).SetBackgroundImageHovered(WorkingBrush).SetBackgroundImageFocused(GetComponentStyle(EUiSurface::Panel,EUiState::Selected,Preferences).Brush).SetBackgroundImageReadOnly(WorkingBrush);
        RouteNameStyle.SetForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).SetFocusedForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)).SetReadOnlyForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink));
        RouteNameStyle.SetPadding(FMargin(10,8));
		auto MakeControl=[this](const TCHAR* Id,const FText& Label,EHansaUiButtonStyle Kind){
			auto Button=SNew(SHansaAction).Preferences(Preferences).Kind(Kind).Label(Label).OnClicked(this,&SHansaTradeMap::Invoke,FString(Id));
			Button->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Name=FName(Id)]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(Name);}));
			MapWidget(Id,Button);return Button;
		};
        auto Pair = [&](const TCHAR* AId, const FText& ALabel, const TCHAR* BId, const FText& BLabel) {
            return SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[MakeControl(AId,ALabel,EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().FillWidth(1)[MakeControl(BId,BLabel,EHansaUiButtonStyle::Secondary)];
        };
        auto CreationVisibility = [this](bool Review) { const auto* P=Model.Get(); return P && P->GetSnapshot().bCreating && P->GetSnapshot().bReview==Review ? EVisibility::Visible : EVisibility::Collapsed; };
        ChildSlot[SAssignNew(PresentationBox,SBox).WidthOverride(PresentationSize.X).HeightOverride(PresentationSize.Y)
        [SNew(SHansaReferenceFrame).Dark(true).Padding(16)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(RouteTitle,STextBlock).TextStyle(&HeadingStyle)]
                +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[MakeControl(TEXT("TradeMap.New"),LOCTEXT("New","+ New route"),EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[MakeControl(TEXT("TradeMap.Mode.Filter"),LOCTEXT("Filter","Filter routes"),EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[MakeControl(TEXT("TradeMap.City.Filter"),LOCTEXT("CityFilter","City mode"),EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[MakeControl(TEXT("TradeMap.Good.Filter"),LOCTEXT("GoodFilter","Selected good"),EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().AutoWidth()[MakeControl(TEXT("TradeMap.Close"),LOCTEXT("Close","Close"),EHansaUiButtonStyle::Icon)]]
            +SVerticalBox::Slot().FillHeight(1).Padding(0,12)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(.18f).Padding(0,0,8,0)[SNew(SHansaReferenceFrame).Dark(false).Padding(12)
                    [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Directory","Routes")).TextStyle(&LightHeadingStyle)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ModeText,STextBlock).TextStyle(&LightCaptionStyle)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(CityModeText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(GoodModeText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(CitySearchInput,SEditableTextBox).Style(&RouteNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)).HintText(LOCTEXT("SearchCities","Search cities")).OnTextChanged_Lambda([this](const FText& T){if(auto* P=Model.Get())P->SetCitySearchIntent(T.ToString());})]
					+SVerticalBox::Slot().FillHeight(1)[SAssignNew(RouteScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(RouteList,SVerticalBox)]]
					+SVerticalBox::Slot().AutoHeight().Padding(0,4)[Pair(TEXT("TradeMap.Route.Page.Previous"),LOCTEXT("PreviousRoutes","Previous"),TEXT("TradeMap.Route.Page.Next"),LOCTEXT("NextRoutes","Next"))]]]
                +SHorizontalBox::Slot().FillWidth(.52f)[SNew(SHansaReferenceFrame).Dark(true).Padding(8)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1)[MakeControl(TEXT("TradeMap.Chart.ZoomIn"),LOCTEXT("ZoomIn","Zoom in"),EHansaUiButtonStyle::Secondary)]
                        +SHorizontalBox::Slot().FillWidth(1).Padding(4,0)[MakeControl(TEXT("TradeMap.Chart.ZoomOut"),LOCTEXT("ZoomOut","Zoom out"),EHansaUiButtonStyle::Secondary)]
                        +SHorizontalBox::Slot().FillWidth(1)[MakeControl(TEXT("TradeMap.Chart.Reset"),LOCTEXT("FitChart","Fit region"),EHansaUiButtonStyle::Secondary)]]
                    +SVerticalBox::Slot().FillHeight(1)[SAssignNew(CanvasHost,SBox)[SAssignNew(RouteCanvas,STradeRouteCanvas).Preferences(Preferences).Model(Model.Get())]]]]
                +SHorizontalBox::Slot().FillWidth(.30f).Padding(8,0,0,0)[SNew(SHansaReferenceFrame).Dark(false).Padding(16)
                    [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SAssignNew(SelectedCityText,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Pair(TEXT("TradeMap.Selection.PreviousCity"),LOCTEXT("PreviousCity","Previous city"),TEXT("TradeMap.Selection.NextCity"),LOCTEXT("NextCity","Next city"))]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                    [SNew(SVerticalBox).Visibility_Lambda([this]{return Model.IsValid()&&!Model->GetSnapshot().bCreating?EVisibility::Visible:EVisibility::Collapsed;})
                        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)[Pair(TEXT("TradeMap.Navigate.Route"),LOCTEXT("JumpRoute","Route"),TEXT("TradeMap.Navigate.Presence"),LOCTEXT("JumpPresence","Presence"))]
                        +SVerticalBox::Slot().AutoHeight()[Pair(TEXT("TradeMap.Navigate.Specialization"),LOCTEXT("JumpSpecialization","Office"),TEXT("TradeMap.Navigate.Orders"),LOCTEXT("JumpOrders","Orders"))]]
                    +SVerticalBox::Slot().FillHeight(1)[SAssignNew(EditorScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[SAssignNew(SetupPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Plan","Plan your voyage")).TextStyle(&LightHeadingStyle)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(LOCTEXT("Name","Route name · up to 48 characters")).TextStyle(&LightCaptionStyle)]
                            +SVerticalBox::Slot().AutoHeight()[SNew(SBox).MinDesiredHeight(48)[SAssignNew(RouteNameInput,SEditableTextBox).Style(&RouteNameStyle).Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences))
                                .OnTextChanged_Lambda([this](const FText& T){if(auto* P=Model.Get())P->SetRouteNameIntent(T.ToString());})]]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary)
                                .OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Creator.Cog")))
                                [SAssignNew(CogLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]]
                        +SVerticalBox::Slot().AutoHeight()[SAssignNew(EditPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text(LOCTEXT("Stops","Ordered stops")).TextStyle(&LightHeadingStyle)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(LOCTEXT("ForeignSettlement","Foreign market loads buy goods; unloads sell them at the current price. Local transfers are free.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(StopList,SVerticalBox)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Editor.Visit"),LOCTEXT("VisitStop","Visit selected stop"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Pair(TEXT("TradeMap.Creator.City"),LOCTEXT("City","Change city"),TEXT("TradeMap.Creator.Good"),LOCTEXT("Good","Change good"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Editor.Action.Cycle"),LOCTEXT("Action","Switch load / unload"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Editor.Quantity.Decrease"),LOCTEXT("QtyMinus","− 5 cargo"),TEXT("TradeMap.Editor.Quantity.Increase"),LOCTEXT("QtyPlus","+ 5 cargo"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Editor.Reserve.Decrease"),LOCTEXT("ReserveMinus","− 5 reserve"),TEXT("TradeMap.Editor.Reserve.Increase"),LOCTEXT("ReservePlus","+ 5 reserve"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Editor.Stop.Up"),LOCTEXT("Up","Move up"),TEXT("TradeMap.Editor.Stop.Down"),LOCTEXT("Down","Move down"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Pair(TEXT("TradeMap.Creator.Add"),LOCTEXT("Add","+ Add stop"),TEXT("TradeMap.Creator.Remove"),LOCTEXT("Remove","Remove stop"))]]
                        +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReviewPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[SNew(STextBlock).Text(LOCTEXT("Ready","Departure review")).TextStyle(&LightHeadingStyle)]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReviewText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(ExistingPanel,SVerticalBox)
							+SVerticalBox::Slot().AutoHeight()[SAssignNew(PresencePanel,SVerticalBox)
							+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(LOCTEXT("StationInspector","Foreign presence · Station inspector")).TextStyle(&LightHeadingStyle)]
							+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(TradeStationText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
							+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(TradeStationButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Station.Action")))]
							+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(PresenceUpgradeButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Presence.Upgrade")))]
							+SVerticalBox::Slot().AutoHeight().Padding(0,8,0,4)[SAssignNew(PresenceProgressText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
							+SVerticalBox::Slot().AutoHeight()[SAssignNew(SpecializationPanel,SVerticalBox)
							+SVerticalBox::Slot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(LOCTEXT("SpecializationHeading","Merchant Office specialization")).TextStyle(&LightHeadingStyle)]
							+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(PresenceSpecializationText,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
							+SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(WarehouseSpecializationButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("WarehouseBranch","Warehouse · capacity and reserves")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Presence.Specialization.Warehouse")))]
							+SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(MarketSpecializationButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("MarketBranch","Market · orders and price limits")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Presence.Specialization.Market")))]
							+SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(HarborSpecializationButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("HarborBranch","Harbor · route handling")).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Presence.Specialization.Harbor")))]
							+SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ApplySpecializationButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Presence.Specialization.Apply")))]
							+SVerticalBox::Slot().AutoHeight()[SAssignNew(PresenceSpecializationFeedback,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(StationOrdersPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(StationOrderText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Select"),LOCTEXT("OrderSelect","Next order / new"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Good"),LOCTEXT("OrderGood","Change good"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Side"),LOCTEXT("OrderSide","Acquire / release"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Target.Decrease"),LOCTEXT("OrderTargetDecrease","− 1 target / reserve"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Target.Increase"),LOCTEXT("OrderTargetIncrease","+ 1 target / reserve"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Cap.Decrease"),LOCTEXT("OrderCapDecrease","− 1 cap"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Cap.Increase"),LOCTEXT("OrderCapIncrease","+ 1 cap"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Budget.Decrease"),LOCTEXT("OrderBudgetDecrease","− 1,000 budget"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Budget.Increase"),LOCTEXT("OrderBudgetIncrease","+ 1,000 budget"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Save"),LOCTEXT("OrderSave","Create / save order"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Pause"),LOCTEXT("OrderPause","Pause / resume order"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[MakeControl(TEXT("TradeMap.Orders.Cancel"),LOCTEXT("OrderCancel","Cancel order"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(StationOrderFeedback,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteDetailsPanel,SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(ReserveRisk,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteMetrics,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[MakeControl(TEXT("TradeMap.Editor.Save"),LOCTEXT("Save","Save changes"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(RouteStateCard,SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"))).Padding(10)[SNew(SVerticalBox)
                                +SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteStateHeading,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
                                +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(RouteStateDetail,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]
                            +SVerticalBox::Slot().AutoHeight()[SAssignNew(ToggleActiveButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).OnClicked(this,&SHansaTradeMap::Invoke,FString(TEXT("TradeMap.Editor.ToggleActive")))
                                [SAssignNew(ToggleActiveText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]]
                             +SVerticalBox::Slot().AutoHeight()[SAssignNew(ToggleActiveHint,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,8)[MakeControl(TEXT("TradeMap.Editor.Cancel"),LOCTEXT("CancelRoute","Cancel route"),EHansaUiButtonStyle::Secondary)]
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("CancelHint","Cancel when stopped with an empty hold. To keep cargo moving, resume and wait for unloading.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(EditorStatus,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(ValidationText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).Visibility_Lambda([CreationVisibility]{return CreationVisibility(false);})[MakeControl(TEXT("TradeMap.Creator.Review"),LOCTEXT("Review","Review voyage"),EHansaUiButtonStyle::Primary)]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).Visibility_Lambda([CreationVisibility]{return CreationVisibility(true);})[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(.36f).Padding(0,0,4,0)[MakeControl(TEXT("TradeMap.Creator.Edit"),LOCTEXT("Edit","Edit stops"),EHansaUiButtonStyle::Secondary)]+SHorizontalBox::Slot().FillWidth(.64f)[MakeControl(TEXT("TradeMap.Creator.Activate"),LOCTEXT("Activate","Create and activate"),EHansaUiButtonStyle::Primary)]]]
                    +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this]{return Model.IsValid()&&Model->GetSnapshot().bCreating?EVisibility::Visible:EVisibility::Collapsed;})[MakeControl(TEXT("TradeMap.Creator.Discard"),LOCTEXT("Discard","Discard draft"),EHansaUiButtonStyle::Icon)]]]]]
            +SVerticalBox::Slot().AutoHeight()[SAssignNew(BottomPanel,SBorder).BorderImage(&FloatingBrush).Padding(8)
                [SAssignNew(ScheduleText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]]]]];
        MapWidget(TEXT("TradeMap.Root"),SharedThis(this)); MapWidget(TEXT("TradeMap.Canvas"),CanvasHost);
		MapWidget(TEXT("TradeMap.City.Search"),CitySearchInput);
        MapWidget(TEXT("TradeMap.Creator.Name"),RouteNameInput); MapWidget(TEXT("TradeMap.Creator.Cog"),CogLabel->GetParentWidget());
        MapWidget(TEXT("TradeMap.Creator.ReviewText"),ReviewText); MapWidget(TEXT("TradeMap.Creator.Validation"),ValidationText);
        MapWidget(TEXT("TradeMap.Editor.RouteState"),RouteStateCard); MapWidget(TEXT("TradeMap.Editor.ToggleActive"),ToggleActiveButton);
		MapWidget(TEXT("TradeMap.Station.Action"),TradeStationButton);
		MapWidget(TEXT("TradeMap.Presence.Progress"),PresenceProgressText);MapWidget(TEXT("TradeMap.Presence.Upgrade"),PresenceUpgradeButton);MapWidget(TEXT("TradeMap.Presence.Specialization.Warehouse"),WarehouseSpecializationButton);MapWidget(TEXT("TradeMap.Presence.Specialization.Market"),MarketSpecializationButton);MapWidget(TEXT("TradeMap.Presence.Specialization.Harbor"),HarborSpecializationButton);MapWidget(TEXT("TradeMap.Presence.Specialization.Apply"),ApplySpecializationButton);
        MapWidget(TEXT("TradeMap.Orders.Status"),StationOrderText);

		if(auto* P=Model.Get()){ChangedHandle=P->OnChanged().AddSP(SharedThis(this),&SHansaTradeMap::Refresh);Refresh(P->GetSnapshot(),P->GetRevision());}
	}
	void SHansaTradeMap::SetPresentationSize(FIntPoint S){PresentationSize=S;if(PresentationBox){PresentationBox->SetWidthOverride(S.X);PresentationBox->SetHeightOverride(S.Y);}if(auto* P=Model.Get())P->SetCompact(S.X<1500||S.Y<850);}
	void SHansaTradeMap::Refresh(const FHansaTradeMapSnapshot& S,uint64)
	{
        if(!PresentedCity.IsNone()&&PresentedCity!=S.SelectedCityStableId&&!S.bCreating)ActiveSection=TEXT("Presence");
        PresentedCity=S.SelectedCityStableId;
        const auto* SelectedCity=S.Cities.FindByPredicate([&](const auto& C){return C.StableId==S.SelectedCityStableId;});
        SelectedCityText->SetText(SelectedCity?SelectedCity->Label:FText::FromName(S.SelectedCityStableId));
        if(auto Visit=ResolveSemanticWidget(TEXT("TradeMap.Editor.Visit")))Visit->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);
        RouteTitle->SetText(S.Title);
        if (RouteNameInput->GetText().ToString() != S.DraftName) RouteNameInput->SetText(FText::FromString(S.DraftName));
		if (CitySearchInput->GetText().ToString() != S.CitySearchText) CitySearchInput->SetText(FText::FromString(S.CitySearchText));
        SetupPanel->SetVisibility(S.bCreating&&!S.bReview?EVisibility::Visible:EVisibility::Collapsed);
        EditPanel->SetVisibility(S.bReview?EVisibility::Collapsed:EVisibility::Visible);
        ReviewPanel->SetVisibility(S.bCreating&&S.bReview?EVisibility::Visible:EVisibility::Collapsed);
        ExistingPanel->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);
        StationOrdersPanel->SetVisibility(EVisibility::Visible);
        StationOrderText->SetText(S.TradeStationValue>0?S.StationOrderText:LOCTEXT("OrdersLocked","Station orders are locked. Establish and fund a trade station in Rostock first. Foreign presence shows your progress and unmet requirements."));StationOrderFeedback->SetText(S.StationOrderFeedback);
        if(const auto* P=Model.Get())for(const auto& Entry:SemanticWidgets)if(Entry.Key.StartsWith(TEXT("TradeMap.Orders."))&&Entry.Key!=TEXT("TradeMap.Orders.Status"))if(auto W=Entry.Value.Pin()){W->SetEnabled(P->CanStationOrderAction(Entry.Key.RightChop(16)));W->SetVisibility(S.TradeStationValue>0?EVisibility::Visible:EVisibility::Collapsed);}
		TradeStationText->SetText(FText::Format(LOCTEXT("StationInspectorBody","{0}\n{1}"),S.TradeStationState,S.TradeStationDetail));TradeStationButton->SetLabel(S.TradeStationAction);TradeStationButton->SetEnabled(S.bCanTradeStationAction);
		PresenceProgressText->SetText(S.PresenceProgress);PresenceUpgradeButton->SetLabel(S.PresenceUpgradeAction);PresenceUpgradeButton->SetEnabled(S.bCanPresenceUpgradeAction);PresenceSpecializationText->SetText(S.PresenceSpecializationComparison);PresenceSpecializationFeedback->SetText(S.PresenceSpecializationFeedback);ApplySpecializationButton->SetLabel(S.PresenceSpecializationAction);if(const auto* P=Model.Get()){WarehouseSpecializationButton->SetEnabled(P->CanPresenceSpecializationIntent(TEXT("Warehouse")));MarketSpecializationButton->SetEnabled(P->CanPresenceSpecializationIntent(TEXT("Market")));HarborSpecializationButton->SetEnabled(P->CanPresenceSpecializationIntent(TEXT("Harbor")));ApplySpecializationButton->SetEnabled(P->CanPresenceSpecializationIntent(TEXT("Apply")));}
        ValidationText->SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);
        ValidationText->SetText(S.Validation); CogLabel->SetText(S.CogLabel);
        ReviewText->SetText(FText::Format(LOCTEXT("NamedReview","{0}\n{1}\n\n{2}"),FText::FromString(S.DraftName),S.CogLabel,S.CreatorReview));
        for(const TCHAR* Id:{TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good"),TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove")})
            if(auto W=ResolveSemanticWidget(Id))W->SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Activate"))) W->SetEnabled(S.bCanCreate);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.New"))) {W->SetEnabled(!S.bCreating);W->SetToolTipText(S.bCreating?LOCTEXT("FinishDraft","Finish or discard this draft before choosing another route."):FText());}
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Mode.Filter"))) {W->SetEnabled(!S.bCreating);W->SetToolTipText(S.bCreating?LOCTEXT("FinishDraft","Finish or discard this draft before choosing another route."):FText());}
		for(const TCHAR* Id:{TEXT("TradeMap.City.Filter"),TEXT("TradeMap.Good.Filter"),TEXT("TradeMap.City.Search")})if(auto W=ResolveSemanticWidget(Id))W->SetEnabled(!S.bCreating);
		if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Route.Page.Previous")))W->SetEnabled(!S.bCreating&&S.RouteWindowStart>0);
		if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Route.Page.Next")))W->SetEnabled(!S.bCreating&&S.RouteWindowStart+S.Routes.Num()<S.MatchingRouteCount);
        RouteList->SetEnabled(!S.bCreating);
		ModeText->SetText(S.ModeFilter==EHansaTradeMapModeFilter::All?LOCTEXT("All","All routes"):S.ModeFilter==EHansaTradeMapModeFilter::Sea?LOCTEXT("SeaOnly","Sea routes"):LOCTEXT("LandOnly","Land routes"));
		CityModeText->SetText(FText::Format(LOCTEXT("CityModeSummary","{0} · {1} matching cities"),S.CityFilter==EHansaTradeMapCityFilter::All?LOCTEXT("AllCities","All cities"):S.CityFilter==EHansaTradeMapCityFilter::Presence?LOCTEXT("PresenceCities","Your presence"):LOCTEXT("RouteCities","Route cities"),FText::AsNumber(S.MatchingCityCount)));
		GoodModeText->SetText(FText::Format(LOCTEXT("SelectedGoodSummary","Selected good: {0} · reports retain age"),FText::FromName(S.PreferredGoodStableId)));
		const auto FocusedBefore=FSlateApplication::IsInitialized()?FSlateApplication::Get().GetKeyboardFocusedWidget():nullptr;
        const FString FocusId=S.FocusedSemanticId.ToString();const bool RestoreRow=FocusedBefore&&ResolveSemanticWidget(FocusId)==FocusedBefore;
        RebuildRoutes(S);RebuildStops(S);
        if(RestoreRow&&(FocusId.StartsWith(TEXT("TradeMap.Stop."))||FocusId.StartsWith(TEXT("TradeMap.Route."))))if(auto W=ResolveSemanticWidget(FocusId))if(W!=FocusedBefore)FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);if(RouteCanvas)RouteCanvas->SetSnapshot(S);
		const auto* R=S.Routes.FindByPredicate([&](const auto& X){return X.RouteValue==S.SelectedRouteValue;});
		RouteMetrics->SetText(R?FText::Format(LOCTEXT("Metrics","{0}\n{1}\nCapacity: {2}\nUpkeep: {3}\nExpected cash result: {4}\n{5}"),R->StopSummary,R->RoundTripTime,R->Capacity,R->Upkeep,R->ExpectedProfitRange,R->Uncertainty):FText());
		ReserveRisk->SetText(S.ReserveRisk);
		RouteStateHeading->SetText(R?R->StateHeading:FText());
		RouteStateDetail->SetText(R?R->StateDetail:FText());
		ToggleActiveText->SetText(R?R->ToggleActionLabel:FText());
		ToggleActiveHint->SetText(R?R->ToggleActionHint:FText());
		ToggleActiveButton->SetEnabled(R&&R->bCanToggleActive);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Editor.Cancel")))W->SetEnabled(R&&R->bCanCancel);
		// The action keeps one primary style; its label communicates pause/resume.
		const FLinearColor StateColor=!R?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment)
			:R->bTraveling?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticBlue)
			:R->bActive?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal)
			:R->bOwnedByPlayer?UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment)
			:UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk);
		RouteStateCard->SetBorderBackgroundColor(StateColor.CopyWithNewOpacity(R&&R->bActive?.24f:.36f));
		EditorStatus->SetText(S.EditorStatus);EditorStatus->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);
		FString Schedule=R?R->Label.ToString()+TEXT("  |  ")+R->State.ToString()+TEXT("  |  ")+R->Capacity.ToString():TEXT("Route schedule");
        for(const auto& Stop:S.Stops)Schedule+=TEXT("\n")+Stop.AccessibleLabel.ToString();
        ScheduleText->SetText(FText::FromString(Schedule));
        BottomPanel->SetVisibility(EVisibility::Visible);
        ApplySectionVisibility();
	}
    void SHansaTradeMap::ApplySectionVisibility()
    {
        const auto* P=Model.Get();if(!P)return;const auto& S=P->GetSnapshot();
        if(S.bCreating)ActiveSection=TEXT("Route");
        auto Show=[](bool Visible){return Visible?EVisibility::Visible:EVisibility::Collapsed;};
        EditPanel->SetVisibility(Show(!S.bReview&&ActiveSection==TEXT("Route")));
        PresencePanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Presence")));
        SpecializationPanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Specialization")));
        StationOrdersPanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Orders")));
        RouteDetailsPanel->SetVisibility(Show(!S.bCreating&&ActiveSection==TEXT("Route")));
        for(const TCHAR* Section:{TEXT("Route"),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders")})
            if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Navigate.")+FString(Section)))StaticCastSharedPtr<SHansaAction>(W)->SetState(ActiveSection==Section?EUiState::Selected:EUiState::Default);
    }
	void SHansaTradeMap::RebuildRoutes(const FHansaTradeMapSnapshot& S){FString Key=FString::Printf(TEXT("%lld"),S.SelectedRouteValue);for(const auto& R:S.Routes)Key+=FString::Printf(TEXT("%lld"),R.RouteValue)+R.Label.ToString()+R.Mode.ToString()+R.State.ToString()+R.StopSummary.ToString();
if(Key==PresentedContentKey)return;PresentedContentKey=MoveTemp(Key);RouteList->ClearChildren();for(const auto& R:S.Routes){const FString Id=FString::Printf(TEXT("TradeMap.Route.%lld"),R.RouteValue);TSharedPtr<SButton>B;RouteList->AddSlot().AutoHeight().Padding(0,3)[SAssignNew(B,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).State(R.RouteValue==S.SelectedRouteValue?EUiState::Selected:EUiState::Default).OnClicked(this,&SHansaTradeMap::Invoke,Id)[SNew(STextBlock).Text(FText::Format(LOCTEXT("RouteRow","{0}\n{1} · {2}\n{3}"),R.Label,R.Mode,R.State,R.StopSummary)).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]];MapWidget(Id,B);StaticCastSharedPtr<SHansaAction>(B)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Id]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*Id));}));}}
	void SHansaTradeMap::RebuildStops(const FHansaTradeMapSnapshot& S){FString Key=FString::FromInt(S.SelectedStopIndex);for(const auto& Stop:S.Stops)Key+=FString::FromInt(Stop.Index)+Stop.AccessibleLabel.ToString();
if(Key==PresentedStopKey)return;PresentedStopKey=MoveTemp(Key);StopList->ClearChildren();for(const auto& Stop:S.Stops){const FString Id=FString::Printf(TEXT("TradeMap.Stop.%d"),Stop.Index);TSharedPtr<SButton>B;StopList->AddSlot().AutoHeight().Padding(0,3)[SAssignNew(B,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).State(Stop.Index==S.SelectedStopIndex?EUiState::Selected:EUiState::Default).OnClicked(this,&SHansaTradeMap::Invoke,Id)[SNew(STextBlock).Text(Stop.AccessibleLabel).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]];MapWidget(Id,B);StaticCastSharedPtr<SHansaAction>(B)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Id]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*Id));}));}}
	FReply SHansaTradeMap::Invoke(const FString Id){ActivateSemanticId(Id);return FReply::Handled();}
	bool SHansaTradeMap::ActivateSemanticId(const FString& Id){auto* P=Model.Get();if(!P||!P->GetSnapshot().bOpen)return false;
        if(Id.StartsWith(TEXT("TradeMap.Navigate.")))
        {
            if(P->GetSnapshot().bCreating||!EditorScroll)return false;
            const FString Section=Id.RightChop(18);
            if(Section!=TEXT("Route")&&Section!=TEXT("Presence")&&Section!=TEXT("Specialization")&&Section!=TEXT("Orders"))return false;
            ActiveSection=Section;ApplySectionVisibility();
            FocusSemanticId(Id);
            EditorScroll->ScrollToStart();
            return true;
        }
        if(Id==TEXT("TradeMap.Selection.PreviousCity"))return P->CycleCityIntent(-1);
        if(Id==TEXT("TradeMap.Selection.NextCity"))return P->CycleCityIntent(1);
        if(Id==TEXT("TradeMap.Chart.ZoomIn")){RouteCanvas->ChangeZoom(.5f);return true;}
        if(Id==TEXT("TradeMap.Chart.ZoomOut")){RouteCanvas->ChangeZoom(-.5f);return true;}
        if(Id==TEXT("TradeMap.Chart.Reset")){RouteCanvas->ResetView();return true;}
        if(Id.StartsWith(TEXT("TradeMap.City.City_")))return P->SelectCityIntent(FName(*Id.RightChop(14).Replace(TEXT("_"),TEXT("."))));
        if(Id==TEXT("TradeMap.Editor.Cancel"))
        {
            const bool Result=P->CancelRouteIntent();
            if(Result){FocusSemanticId(TEXT("TradeMap.New"));if(EditorScroll)EditorScroll->ScrollToEnd();}
            return Result;
        }
        if(Id==TEXT("TradeMap.Editor.Visit"))return P->VisitSelectedStopIntent();
		if(Id.StartsWith(TEXT("TradeMap.Orders.")))return P->StationOrderIntent(Id.RightChop(16));
        if(Id==TEXT("TradeMap.Station.Action"))return P->TradeStationActionIntent();
		if(Id==TEXT("TradeMap.Presence.Upgrade"))return P->PresenceUpgradeActionIntent();if(Id.StartsWith(TEXT("TradeMap.Presence.Specialization.")))return P->PresenceSpecializationIntent(Id.RightChop(33));
        if(Id==TEXT("TradeMap.New")){const bool Result=P->BeginCreateIntent();if(Result)FocusSemanticId(TEXT("TradeMap.Creator.Name"));return Result;}
        if(Id==TEXT("TradeMap.Creator.Cog"))return P->CycleCogIntent();
        if(Id==TEXT("TradeMap.Creator.City"))return P->CycleStopCityIntent();
        if(Id==TEXT("TradeMap.Creator.Good"))return P->CycleStopGoodIntent();
        if(Id==TEXT("TradeMap.Creator.Add"))return P->AddStopIntent();
        if(Id==TEXT("TradeMap.Creator.Remove"))return P->RemoveStopIntent();
        if(Id==TEXT("TradeMap.Creator.Discard"))return P->DiscardCreateIntent();
        if(Id==TEXT("TradeMap.Creator.Review")){const bool Result=P->ReviewCreateIntent();if(Result)FocusSemanticId(P->GetSnapshot().bCanCreate?TEXT("TradeMap.Creator.Activate"):TEXT("TradeMap.Creator.Edit"));return Result;}
        if(Id==TEXT("TradeMap.Creator.Edit")){const bool Result=P->EditCreateIntent();if(Result)FocusSemanticId(TEXT("TradeMap.Creator.Name"));return Result;}
        if(Id==TEXT("TradeMap.Creator.Activate"))return P->CreateAndActivateIntent();
        if(Id==TEXT("TradeMap.Close"))return P->CloseIntent();if(Id==TEXT("TradeMap.Mode.Filter"))return P->CycleModeFilterIntent();if(Id==TEXT("TradeMap.City.Filter"))return P->CycleCityFilterIntent();if(Id==TEXT("TradeMap.Good.Filter"))return P->CycleSelectedGoodIntent();if(Id==TEXT("TradeMap.Route.Page.Previous"))return P->MoveRouteWindowIntent(-1);if(Id==TEXT("TradeMap.Route.Page.Next"))return P->MoveRouteWindowIntent(1);if(Id==TEXT("TradeMap.Editor.Action.Cycle"))return P->CycleCargoActionIntent();if(Id==TEXT("TradeMap.Editor.Quantity.Decrease"))return P->AdjustQuantityIntent(-5000);if(Id==TEXT("TradeMap.Editor.Quantity.Increase"))return P->AdjustQuantityIntent(5000);if(Id==TEXT("TradeMap.Editor.Reserve.Decrease"))return P->AdjustMinimumReserveIntent(-5000);if(Id==TEXT("TradeMap.Editor.Reserve.Increase"))return P->AdjustMinimumReserveIntent(5000);if(Id==TEXT("TradeMap.Editor.Stop.Up"))return P->MoveStopIntent(-1);if(Id==TEXT("TradeMap.Editor.Stop.Down"))return P->MoveStopIntent(1);if(Id==TEXT("TradeMap.Editor.Save"))return P->CommitIntent();if(Id==TEXT("TradeMap.Editor.ToggleActive"))return P->ToggleActiveIntent();if(Id.StartsWith(TEXT("TradeMap.Route.")))return P->SelectRouteIntent(FCString::Atoi64(*Id.RightChop(15)));if(Id.StartsWith(TEXT("TradeMap.Stop.")))return P->SelectStopIntent(FCString::Atoi(*Id.RightChop(14)));return false;}
	bool SHansaTradeMap::FocusSemanticId(const FString& Id)
    {
        auto* P=Model.Get();const auto Widget=ResolveSemanticWidget(Id);
        if(P&&!P->GetSnapshot().bCreating&&Widget&&Widget->IsEnabled())
        {
            if(Id.StartsWith(TEXT("TradeMap.Presence.Specialization.")))ActiveSection=TEXT("Specialization");
            else if(Id.StartsWith(TEXT("TradeMap.Presence."))||Id.StartsWith(TEXT("TradeMap.Station.")))ActiveSection=TEXT("Presence");
            else if(Id.StartsWith(TEXT("TradeMap.Orders.")))ActiveSection=TEXT("Orders");
            else if(Id.StartsWith(TEXT("TradeMap.Editor."))||Id.StartsWith(TEXT("TradeMap.Stop.")))ActiveSection=TEXT("Route");
            ApplySectionVisibility();
        }
        if(!P||!P->GetSnapshot().bOpen||!GetControllerFocusOrder().Contains(Id)||!Widget||!Widget->IsEnabled())return false;
        P->SetFocusedSemanticId(FName(*Id));
        // Reveal descendants by ancestry, so newly added presence controls cannot be omitted by a prefix list.
        for(const auto& Scroll:{EditorScroll,RouteScroll})
        {
            for(auto Ancestor=Widget;Scroll&&Ancestor;Ancestor=Ancestor->GetParentWidget())
                if(Ancestor==Scroll){Scroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);break;}
        }
        if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
        return true;
    }
    TArray<FString> SHansaTradeMap::GetControllerFocusOrder()const
    {
        TArray<FString> R; const auto* P=Model.Get(); if(!P||!P->GetSnapshot().bOpen)return R;
        const auto& S=P->GetSnapshot(); R.Add(TEXT("TradeMap.Close"));
        if(!S.bCreating) { R.Append({TEXT("TradeMap.New"),TEXT("TradeMap.Mode.Filter"),TEXT("TradeMap.City.Filter"),TEXT("TradeMap.Good.Filter"),TEXT("TradeMap.City.Search"),TEXT("TradeMap.Route.Page.Previous"),TEXT("TradeMap.Route.Page.Next")}); for(const auto& X:S.Routes)R.Add(FString::Printf(TEXT("TradeMap.Route.%lld"),X.RouteValue)); }
        if(S.bCreating&&!S.bReview)R.Append({TEXT("TradeMap.Creator.Name"),TEXT("TradeMap.Creator.Cog")});
        if(!S.bReview) {
            for(const auto& X:S.Stops)R.Add(FString::Printf(TEXT("TradeMap.Stop.%d"),X.Index));
            if(S.bCreating)R.Append({TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good")});
            R.Append({TEXT("TradeMap.Editor.Action.Cycle"),TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("TradeMap.Editor.Quantity.Increase"),TEXT("TradeMap.Editor.Reserve.Decrease"),TEXT("TradeMap.Editor.Reserve.Increase"),TEXT("TradeMap.Editor.Stop.Up"),TEXT("TradeMap.Editor.Stop.Down")});
            if(S.bCreating) R.Append({TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove"),TEXT("TradeMap.Creator.Review")});
            else {if(S.bCanTradeStationAction)R.Add(TEXT("TradeMap.Station.Action"));R.Add(TEXT("TradeMap.Editor.Visit"));R.Add(TEXT("TradeMap.Editor.Save"));const auto* Selected=S.Routes.FindByPredicate([&](const auto& X){return X.RouteValue==S.SelectedRouteValue;});if(Selected&&Selected->bCanToggleActive)R.Add(TEXT("TradeMap.Editor.ToggleActive"));if(Selected&&Selected->bCanCancel)R.Add(TEXT("TradeMap.Editor.Cancel"));}
        }
        if(!S.bCreating)R.Append({TEXT("TradeMap.Navigate.Route"),TEXT("TradeMap.Navigate.Presence"),TEXT("TradeMap.Navigate.Specialization"),TEXT("TradeMap.Navigate.Orders")});
        if(!S.bCreating&&S.bCanPresenceUpgradeAction)R.Add(TEXT("TradeMap.Presence.Upgrade"));
        if(!S.bCreating&&!S.PresenceSpecializationComparison.IsEmpty())R.Append({TEXT("TradeMap.Presence.Specialization.Warehouse"),TEXT("TradeMap.Presence.Specialization.Market"),TEXT("TradeMap.Presence.Specialization.Harbor"),TEXT("TradeMap.Presence.Specialization.Apply")});
        if(!S.bCreating&&S.TradeStationValue>0)R.Append({TEXT("TradeMap.Orders.Select"),TEXT("TradeMap.Orders.Good"),TEXT("TradeMap.Orders.Side"),TEXT("TradeMap.Orders.Target.Decrease"),TEXT("TradeMap.Orders.Target.Increase"),TEXT("TradeMap.Orders.Cap.Decrease"),TEXT("TradeMap.Orders.Cap.Increase"),TEXT("TradeMap.Orders.Budget.Decrease"),TEXT("TradeMap.Orders.Budget.Increase"),TEXT("TradeMap.Orders.Save"),TEXT("TradeMap.Orders.Pause"),TEXT("TradeMap.Orders.Cancel")});
        if(S.bCreating&&S.bReview){R.Add(TEXT("TradeMap.Creator.Edit"));if(S.bCanCreate)R.Add(TEXT("TradeMap.Creator.Activate"));}
        if(S.bCreating)R.Add(TEXT("TradeMap.Creator.Discard"));
        R.Append({TEXT("TradeMap.Chart.ZoomIn"),TEXT("TradeMap.Chart.ZoomOut"),TEXT("TradeMap.Chart.Reset")});
        if(!S.bCreating)R.Append({TEXT("TradeMap.Selection.PreviousCity"),TEXT("TradeMap.Selection.NextCity")});
        R.RemoveAll([&](const FString& Id){auto W=ResolveSemanticWidget(Id);while(W&&W.Get()!=this){if(!W->GetVisibility().IsVisible())return true;W=W->GetParentWidget();}return false;});
        return R;
    }

	FReply SHansaTradeMap::OnKeyDown(const FGeometry& MyGeometry,const FKeyEvent& InKeyEvent)
    {
        (void)MyGeometry;const auto Intent=ClassifyNavigationIntent(InKeyEvent);auto* P=Model.Get();if(!P)return FReply::Unhandled();
        if(Intent==EHansaUiNavigationIntent::Back)return P->CloseIntent()?FReply::Handled():FReply::Unhandled();
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
			if(Node.Id.StartsWith(TEXT("TradeMap.Orders."))&&bActivate)bEnabled=Pinned->CanStationOrderAction(Node.Id.RightChop(16));if(Node.Id.StartsWith(TEXT("TradeMap.Presence.Specialization."))&&bActivate)bEnabled=Pinned->CanPresenceSpecializationIntent(Node.Id.RightChop(33));
            Node.bCanActivate = bActivate; Node.bCanFocus = bActivate; Node.State.bEnabled = bEnabled; Node.State.bVisible = State.bOpen;
			Node.State.bSelected = bSelected; Node.State.bWarning = bWarning; Node.State.bFocused = State.FocusedSemanticId == FName(*Node.Id);
			Node.State.ValueType = MoveTemp(ValueType); Node.State.Value = MoveTemp(Value); Nodes.Add(MoveTemp(Node));
		};
		Add(TEXT("TradeMap.Root"), TEXT("HUD.TradeMapHost"), State.Title.ToString(), EHansaHudSemanticRole::Panel,
			false, TEXT("layout"), State.bCompact ? TEXT("compact") : TEXT("wide"));
		Add(TEXT("TradeMap.Close"), TEXT("TradeMap.Root"), TEXT("Close trade map"), EHansaHudSemanticRole::Button, true);
        for(const TCHAR* Id:{TEXT("TradeMap.Chart.ZoomIn"),TEXT("TradeMap.Chart.ZoomOut"),TEXT("TradeMap.Chart.Reset"),TEXT("TradeMap.Selection.PreviousCity"),TEXT("TradeMap.Selection.NextCity")})
            Add(Id,TEXT("TradeMap.Root"),Id,EHansaHudSemanticRole::Button,true);
        if(!State.bCreating)
        {
            Add(TEXT("TradeMap.Navigate.Route"),TEXT("TradeMap.Root"),LOCTEXT("JumpRoute","Route actions").ToString(),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Navigate.Presence"),TEXT("TradeMap.Root"),LOCTEXT("JumpPresence","Foreign presence").ToString(),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Navigate.Specialization"),TEXT("TradeMap.Root"),LOCTEXT("JumpSpecialization","Specializations").ToString(),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Navigate.Orders"),TEXT("TradeMap.Root"),LOCTEXT("JumpOrders","Station orders").ToString(),EHansaHudSemanticRole::Button,true);
            if(State.TradeStationValue<=0)Add(TEXT("TradeMap.Orders.Status"),TEXT("TradeMap.Root"),TEXT("Station orders locked"),EHansaHudSemanticRole::Status,false,TEXT("orders"),StationOrderText->GetText().ToString());
        }
		Add(TEXT("TradeMap.Presence.Progress"),TEXT("TradeMap.Root"),State.PresenceProgress.ToString(),EHansaHudSemanticRole::Status,false,TEXT("presence-progress"),TEXT("Authoritative requirements, unlocks and history."),false,false,false);
		Add(TEXT("TradeMap.Presence.Commercial"),TEXT("TradeMap.Presence.Progress"),TEXT("Commercial presence"),EHansaHudSemanticRole::Status,false,TEXT("presence-kind"),TEXT("Trade access and merchant facilities do not imply city ownership."));
		Add(TEXT("TradeMap.Presence.Privilege"),TEXT("TradeMap.Presence.Progress"),TEXT("Scoped privilege review"),EHansaHudSemanticRole::Status,false,TEXT("privilege-review"),State.PresenceProgress.ToString());
		Add(TEXT("TradeMap.Presence.Project"),TEXT("TradeMap.Presence.Progress"),TEXT("City partnership project review"),EHansaHudSemanticRole::Status,false,TEXT("project-review"),State.PresenceProgress.ToString());
		Add(TEXT("TradeMap.Presence.Governance"),TEXT("TradeMap.Presence.Progress"),TEXT("Exceptional governance charter review"),EHansaHudSemanticRole::Status,false,TEXT("governance-review"),State.PresenceProgress.ToString());
		Add(TEXT("TradeMap.Presence.Upgrade"),TEXT("TradeMap.Root"),State.PresenceUpgradeAction.ToString(),EHansaHudSemanticRole::Button,true,TEXT("presence-upgrade"),State.PresenceProgress.ToString(),false,!State.bCanPresenceUpgradeAction,State.bCanPresenceUpgradeAction);
		for(const TCHAR* Branch:{TEXT("Warehouse"),TEXT("Market"),TEXT("Harbor")})Add(TEXT("TradeMap.Presence.Specialization.")+FString(Branch),TEXT("TradeMap.Root"),FString(Branch)+TEXT(" specialization"),EHansaHudSemanticRole::Button,true,TEXT("exclusive-specialization"),State.PresenceSpecializationComparison.ToString(),State.SelectedPresenceSpecializationId==Branch);
		Add(TEXT("TradeMap.Presence.Specialization.Apply"),TEXT("TradeMap.Root"),State.PresenceSpecializationAction.ToString(),EHansaHudSemanticRole::Button,true,TEXT("reviewed-specialization-command"),State.PresenceSpecializationFeedback.ToString(),false,!State.bCanPresenceSpecializationAction,State.bCanPresenceSpecializationAction);
		Add(TEXT("TradeMap.Station.Action"), TEXT("TradeMap.Root"), State.TradeStationAction.ToString(), EHansaHudSemanticRole::Button, true,
			TEXT("station-state"), State.TradeStationState.ToString()+TEXT(" · ")+State.TradeStationDetail.ToString(), false, !State.bCanTradeStationAction, State.bCanTradeStationAction);
        if(!State.bCreating&&State.TradeStationValue>0){
            Add(TEXT("TradeMap.Orders.Status"),TEXT("TradeMap.Root"),TEXT("Station orders"),EHansaHudSemanticRole::Status,false,TEXT("orders"),State.StationOrderText.ToString());
            Add(TEXT("TradeMap.Orders.Select"),TEXT("TradeMap.Root"),TEXT("Next order / new"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Good"),TEXT("TradeMap.Root"),TEXT("Change good"),EHansaHudSemanticRole::Button,true);
            Add(TEXT("TradeMap.Orders.Side"),TEXT("TradeMap.Root"),TEXT("Acquire / release"),EHansaHudSemanticRole::Button,true);
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
				TEXT("TradeMap.Canvas"), City.Label.ToString(), EHansaHudSemanticRole::Status, false, TEXT("report"),
				City.Information.ToString()+TEXT("; ")+City.CapabilitySummary.ToString(), false, City.bStale || City.bUnknown);
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
        for(const FString& Id:{TEXT("TradeMap.Creator.Name"),TEXT("TradeMap.Creator.Cog"),TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good"),TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove"),TEXT("TradeMap.Creator.Review"),TEXT("TradeMap.Creator.Edit"),TEXT("TradeMap.Creator.Activate"),TEXT("TradeMap.Creator.Discard")})
            Add(Id,TEXT("TradeMap.Root"),Id,EHansaHudSemanticRole::Button,true);
        Add(TEXT("TradeMap.Creator.ReviewText"),TEXT("TradeMap.Root"),State.CreatorReview.ToString(),EHansaHudSemanticRole::Status,false,TEXT("review"),State.CreatorReview.ToString());
        Add(TEXT("TradeMap.Creator.Validation"),TEXT("TradeMap.Root"),State.Validation.ToString(),EHansaHudSemanticRole::Status,false,TEXT("validation"),State.Validation.ToString());

        const auto Order=GetControllerFocusOrder();
        for(auto& Node:Nodes) {
            if(Node.Id.StartsWith(TEXT("TradeMap.Navigate.")))Node.State.bSelected=Node.Id==TEXT("TradeMap.Navigate.")+ActiveSection;
            if(Node.Id.StartsWith(TEXT("TradeMap.Presence.")))Node.State.bVisible&=!State.bCreating&&(Node.Id.StartsWith(TEXT("TradeMap.Presence.Specialization."))?ActiveSection==TEXT("Specialization"):ActiveSection==TEXT("Presence"));
            if(Node.bCanFocus) {Node.bCanFocus=Order.Contains(Node.Id)&&Node.State.bEnabled;Node.bCanActivate=Node.bCanFocus;Node.State.bEnabled=Node.bCanFocus;}
            if(Node.Id.StartsWith(TEXT("TradeMap.Creator.")))Node.State.bVisible=State.bOpen&&State.bCreating;
            if(auto Widget=ResolveSemanticWidget(Node.Id)) {
                bool Visible=State.bOpen; auto Parent=Widget;
                while(Parent.IsValid()&&Parent.Get()!=this){Visible &= Parent->GetVisibility().IsVisible();Parent=Parent->GetParentWidget();}
                const auto G=Widget->GetCachedGeometry();const auto A=G.GetAbsolutePosition();const auto Z=G.GetAbsoluteSize();
                FVector2D Min=A,Max=A+Z;
                auto Scroll=Node.Id.StartsWith(TEXT("TradeMap.Route."))?RouteScroll:EditorScroll;
                if(Scroll&&Node.Id!=TEXT("TradeMap.Root")&&Node.Id!=TEXT("TradeMap.Canvas")&&Node.Id!=TEXT("TradeMap.Close")&&Node.Id!=TEXT("TradeMap.New")&&Node.Id!=TEXT("TradeMap.Mode.Filter")) {
                    bool Descendant=false;auto Ancestor=Widget;while(Ancestor){if(Ancestor==Scroll){Descendant=true;break;}Ancestor=Ancestor->GetParentWidget();}
                    if(Descendant){const auto Clip=Scroll->GetCachedGeometry();const auto C=Clip.GetAbsolutePosition(),D=C+Clip.GetAbsoluteSize();Min.X=FMath::Max(Min.X,C.X);Min.Y=FMath::Max(Min.Y,C.Y);Max.X=FMath::Min(Max.X,D.X);Max.Y=FMath::Min(Max.Y,D.Y);}
                }
                Visible &= Max.X>Min.X&&Max.Y>Min.Y;
                Node.State.bVisible &= Visible;
                if(Visible)Node.Bounds=FIntRect(FMath::RoundToInt(Min.X),FMath::RoundToInt(Min.Y),FMath::RoundToInt(Max.X),FMath::RoundToInt(Max.Y));
                if(Node.Id==TEXT("TradeMap.Creator.Name")){Node.Label=TEXT("Route name");Node.State.Value=State.DraftName;Node.bCanActivate=false;}
                if(Node.Id==TEXT("TradeMap.Creator.Cog"))Node.Label=State.CogLabel.ToString();
            }
        }
        return Nodes;
	}
	void SHansaTradeMap::MapWidget(const FString& Id,const TSharedPtr<SWidget>& W){SemanticWidgets.Add(Id,W);}
}

#undef LOCTEXT_NAMESPACE
