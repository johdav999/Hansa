#include "UI/SHansaTradeMap.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
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
	class STradeRouteCanvas final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STradeRouteCanvas){} SLATE_ARGUMENT(FUiPreferences,Preferences) SLATE_END_ARGS()
		void Construct(const FArguments& A) {Preferences=A._Preferences;}
		void SetSnapshot(const FHansaTradeMapSnapshot& In){Snapshot=In;Invalidate(EInvalidateWidgetReason::Paint);}
		virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(720,520);}
		virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled)const override
		{
			(void)Args;(void)Cull;(void)Style;(void)ParentEnabled;
			const FVector2D Size=Geometry.GetLocalSize();
			auto Pos=[&](FName Id){const auto* C=Snapshot.Cities.FindByPredicate([Id](const auto& X){return X.StableId==Id;});return C?C->NormalizedPosition*Size:FVector2D::ZeroVector;};
            const auto Color=[](EHansaUiColorToken T){return UHansaUiStyleLibrary::GetColor(T);};
            FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(),FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")),ESlateDrawEffect::None,Color(EHansaUiColorToken::BalticNavy));
            auto Line=[&](const TArray<FVector2D>& Points,int32 L,FLinearColor C,float Width=1.f){FSlateDrawElement::MakeLines(Out,L,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,C,true,Width);};
            auto Label=[&](FVector2D P,const FText& Text,FLinearColor C,EHansaUiTypographyToken Font=EHansaUiTypographyToken::Caption){
                FSlateDrawElement::MakeText(Out,Layer+5,Geometry.ToPaintGeometry(FVector2f(Size.X,32),FSlateLayoutTransform(FVector2f(P))),Text,GetComponentFont(Font,Preferences),ESlateDrawEffect::None,C);
            };
            for(int32 I=1;I<8;++I) {const double F=I/8.0;Line({{F*Size.X,0},{F*Size.X,Size.Y}},Layer+1,Color(EHansaUiColorToken::HarborSlate));Line({{0,F*Size.Y},{Size.X,F*Size.Y}},Layer+1,Color(EHansaUiColorToken::HarborSlate));}
            // Generalized regional chart, deliberately not a navigational/georeferenced map.
            const TArray<FVector2D> Shore={{0,.55},{.08,.60},{.14,.59},{.19,.67},{.25,.72},{.30,.62},{.38,.58},{.45,.63},{.52,.66},{.58,.60},{.68,.54},{.75,.52},{.78,.56},{.84,.48},{.92,.45},{1,.50}};
            TArray<FVector2D> Coast;
            for(int32 I=0;I<Shore.Num()-1;++I)for(int32 J=0;J<8;++J){
                const double T=J/8.0; const auto A=Shore[FMath::Max(0,I-1)],B=Shore[I],C=Shore[I+1],D=Shore[FMath::Min(Shore.Num()-1,I+2)];
                const double Y=.5*((2*B.Y)+(-A.Y+C.Y)*T+(2*A.Y-5*B.Y+4*C.Y-D.Y)*T*T+(-A.Y+3*B.Y-3*C.Y+D.Y)*T*T*T);
                Coast.Add(FVector2D(FMath::Lerp(B.X,C.X,T),Y)*Size);
            }
            Coast.Add(Shore.Last()*Size);
            for(int32 I=1;I<Coast.Num();++I) {
                const auto A=Coast[I-1], B=Coast[I];
                for(double X=FMath::CeilToDouble(A.X/2)*2;X<B.X;X+=2) {const double Y=FMath::Lerp(A.Y,B.Y,(X-A.X)/(B.X-A.X));FSlateDrawElement::MakeBox(Out,Layer+2,Geometry.ToPaintGeometry(FVector2f(2.1f,Size.Y-Y),FSlateLayoutTransform(FVector2f(X,Y))),FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")),ESlateDrawEffect::None,Color(EHansaUiColorToken::Parchment));}
            }
            Line(Coast,Layer+3,Color(EHansaUiColorToken::Brass),2.f);
            Label({20,18},LOCTEXT("ChartHeading","SOUTHERN BALTIC"),Color(EHansaUiColorToken::Chalk));
            Label({20,42},LOCTEXT("ChartType","Generalized regional chart"),Color(EHansaUiColorToken::Chalk));
            Label({Size.X*.40,Size.Y*.24},LOCTEXT("Sea","Baltic Sea"),Color(EHansaUiColorToken::Chalk),EHansaUiTypographyToken::Heading2);
            if(Size.Y>500)Label({Size.X*.42,Size.Y*.87},LOCTEXT("Region","Mecklenburg"),Color(EHansaUiColorToken::Ink));
            if(Snapshot.Stops.Num()>1) {
                const FVector2D A=Pos(Snapshot.Stops[0].CityStableId),B=Pos(Snapshot.Stops[1].CityStableId);
                const FVector2D Mid=(A+B)*.5-FVector2D(0,Size.Y*.42);
                auto Curve=[&](double T){return A*(1-T)*(1-T)+Mid*(2*T*(1-T))+B*T*T;};
                for(int32 I=0;I<40;I+=2)Line({Curve(I/40.0),Curve((I+1)/40.0)},Layer+4,Color(EHansaUiColorToken::Chalk),2.f);
                const FVector2D Tip=Curve(.75),Prev=Curve(.72),D=(Tip-Prev).GetSafeNormal(),N(-D.Y,D.X);
                Line({Tip-D*9+N*5,Tip,Tip-D*9-N*5},Layer+4,Color(EHansaUiColorToken::Brass),2.f);
            }
            if(Snapshot.bShipInTransit) {
                const auto P=Snapshot.ShipPosition*Size;
                FSlateDrawElement::MakeBox(Out,Layer+4,Geometry.ToPaintGeometry(FVector2f(28,28),FSlateLayoutTransform(FVector2f(P-FVector2D(14,14)))),GetGeneratedIconBrush(EUiGlyph::Ship,FMath::CeilToInt(28*Geometry.GetAccumulatedLayoutTransform().GetScale())),ESlateDrawEffect::None,FLinearColor::White);
            }
            for(const auto& City:Snapshot.Cities) {
                if(City.StableId!=TEXT("City.Lubeck")&&City.StableId!=TEXT("City.Rostock"))continue;
                const FVector2D P=Pos(City.StableId); TArray<FVector2D> Ring;
                for(int32 I=0;I<=20;++I){const double A=2*PI*I/20;Ring.Add(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*8);}
                Line(Ring,Layer+4,Color(EHansaUiColorToken::Ink),3.f);
                const double CardWidth=FMath::Min(200.0,Size.X-24.0);
                const double X=FMath::Clamp(P.X-32.0,12.0,FMath::Max(12.0,Size.X-CardWidth-12.0));
                FSlateDrawElement::MakeBox(Out,Layer+4,Geometry.ToPaintGeometry(FVector2f(CardWidth,66),FSlateLayoutTransform(FVector2f(X-4,P.Y+14))),FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")),ESlateDrawEffect::None,Color(EHansaUiColorToken::Parchment));
                Label({X,P.Y+18},City.Label,Color(EHansaUiColorToken::Ink),EHansaUiTypographyToken::Data);
                Label({X,P.Y+40},City.Information,Color(EHansaUiColorToken::Ink));
            }

			return Layer+5;
		}
	private: FHansaTradeMapSnapshot Snapshot;FUiPreferences Preferences;
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
        [SNew(SBorder).BorderImage(&OverlayBrush).Padding(16)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(RouteTitle,STextBlock).TextStyle(&HeadingStyle)]
                +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[MakeControl(TEXT("TradeMap.New"),LOCTEXT("New","+ New route"),EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[MakeControl(TEXT("TradeMap.Mode.Filter"),LOCTEXT("Filter","Filter routes"),EHansaUiButtonStyle::Secondary)]
                +SHorizontalBox::Slot().AutoWidth()[MakeControl(TEXT("TradeMap.Close"),LOCTEXT("Close","Close"),EHansaUiButtonStyle::Icon)]]
            +SVerticalBox::Slot().FillHeight(1).Padding(0,12)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(.18f).Padding(0,0,12,0)[SNew(SBorder).BorderImage(&WorkingBrush).Padding(12)
                    [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Directory","Routes")).TextStyle(&LightHeadingStyle)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ModeText,STextBlock).TextStyle(&LightCaptionStyle)]
                    +SVerticalBox::Slot().FillHeight(1)[SAssignNew(RouteScroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(RouteList,SVerticalBox)]]]]
                +SHorizontalBox::Slot().FillWidth(.42f)[SAssignNew(CanvasHost,SBox)[SAssignNew(RouteCanvas,STradeRouteCanvas).Preferences(Preferences)]]
                +SHorizontalBox::Slot().FillWidth(.40f).Padding(12,0,0,0)[SNew(SBorder).BorderImage(&WorkingBrush).Padding(16)
                    [SNew(SVerticalBox)
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
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("CancelHint","Cancel when stopped with an empty hold. To keep cargo moving, resume and wait for unloading.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(EditorStatus,STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(ValidationText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).Visibility_Lambda([CreationVisibility]{return CreationVisibility(false);})[MakeControl(TEXT("TradeMap.Creator.Review"),LOCTEXT("Review","Review voyage"),EHansaUiButtonStyle::Primary)]]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).Visibility_Lambda([CreationVisibility]{return CreationVisibility(true);})[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(.36f).Padding(0,0,4,0)[MakeControl(TEXT("TradeMap.Creator.Edit"),LOCTEXT("Edit","Edit stops"),EHansaUiButtonStyle::Secondary)]+SHorizontalBox::Slot().FillWidth(.64f)[MakeControl(TEXT("TradeMap.Creator.Activate"),LOCTEXT("Activate","Create and activate"),EHansaUiButtonStyle::Primary)]]]
                    +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this]{return Model.IsValid()&&Model->GetSnapshot().bCreating?EVisibility::Visible:EVisibility::Collapsed;})[MakeControl(TEXT("TradeMap.Creator.Discard"),LOCTEXT("Discard","Discard draft"),EHansaUiButtonStyle::Icon)]]]]]
            +SVerticalBox::Slot().AutoHeight()[SAssignNew(BottomPanel,SBorder).BorderImage(&FloatingBrush).Padding(8)
                [SNew(STextBlock).Text(LOCTEXT("Legend","Southern Baltic · Generalized chart · Routes repeat in stop order · Cargo and reports update with the simulation")).TextStyle(&DarkBodyStyle).AutoWrapText(true)]]]]];
        MapWidget(TEXT("TradeMap.Root"),SharedThis(this)); MapWidget(TEXT("TradeMap.Canvas"),CanvasHost);
        MapWidget(TEXT("TradeMap.Creator.Name"),RouteNameInput); MapWidget(TEXT("TradeMap.Creator.Cog"),CogLabel->GetParentWidget());
        MapWidget(TEXT("TradeMap.Creator.ReviewText"),ReviewText); MapWidget(TEXT("TradeMap.Creator.Validation"),ValidationText);
        MapWidget(TEXT("TradeMap.Editor.RouteState"),RouteStateCard); MapWidget(TEXT("TradeMap.Editor.ToggleActive"),ToggleActiveButton);

		if(auto* P=Model.Get()){ChangedHandle=P->OnChanged().AddSP(SharedThis(this),&SHansaTradeMap::Refresh);Refresh(P->GetSnapshot(),P->GetRevision());}
	}
	void SHansaTradeMap::SetPresentationSize(FIntPoint S){PresentationSize=S;if(PresentationBox){PresentationBox->SetWidthOverride(S.X);PresentationBox->SetHeightOverride(S.Y);}if(auto* P=Model.Get())P->SetCompact(S.X<1500||S.Y<850);}
	void SHansaTradeMap::Refresh(const FHansaTradeMapSnapshot& S,uint64)
	{
        if(auto Visit=ResolveSemanticWidget(TEXT("TradeMap.Editor.Visit")))Visit->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);
        RouteTitle->SetText(S.Title);
        if (RouteNameInput->GetText().ToString() != S.DraftName) RouteNameInput->SetText(FText::FromString(S.DraftName));
        SetupPanel->SetVisibility(S.bCreating&&!S.bReview?EVisibility::Visible:EVisibility::Collapsed);
        EditPanel->SetVisibility(S.bReview?EVisibility::Collapsed:EVisibility::Visible);
        ReviewPanel->SetVisibility(S.bCreating&&S.bReview?EVisibility::Visible:EVisibility::Collapsed);
        ExistingPanel->SetVisibility(S.bCreating?EVisibility::Collapsed:EVisibility::Visible);
        ValidationText->SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);
        ValidationText->SetText(S.Validation); CogLabel->SetText(S.CogLabel);
        ReviewText->SetText(FText::Format(LOCTEXT("NamedReview","{0}\n{1}\n\n{2}"),FText::FromString(S.DraftName),S.CogLabel,S.CreatorReview));
        for(const TCHAR* Id:{TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good"),TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove")})
            if(auto W=ResolveSemanticWidget(Id))W->SetVisibility(S.bCreating?EVisibility::Visible:EVisibility::Collapsed);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Creator.Activate"))) W->SetEnabled(S.bCanCreate);
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.New"))) {W->SetEnabled(!S.bCreating);W->SetToolTipText(S.bCreating?LOCTEXT("FinishDraft","Finish or discard this draft before choosing another route."):FText());}
        if(auto W=ResolveSemanticWidget(TEXT("TradeMap.Mode.Filter"))) {W->SetEnabled(!S.bCreating);W->SetToolTipText(S.bCreating?LOCTEXT("FinishDraft","Finish or discard this draft before choosing another route."):FText());}
        RouteList->SetEnabled(!S.bCreating);
		ModeText->SetText(S.ModeFilter==EHansaTradeMapModeFilter::All?LOCTEXT("All","All routes"):S.ModeFilter==EHansaTradeMapModeFilter::Sea?LOCTEXT("SeaOnly","Sea routes"):LOCTEXT("LandOnly","Land routes"));
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
		BottomPanel->SetVisibility(S.bCompact?EVisibility::Collapsed:EVisibility::Visible);
	}
	void SHansaTradeMap::RebuildRoutes(const FHansaTradeMapSnapshot& S){FString Key=FString::Printf(TEXT("%lld"),S.SelectedRouteValue);for(const auto& R:S.Routes)Key+=FString::Printf(TEXT("%lld"),R.RouteValue)+R.Label.ToString()+R.Mode.ToString()+R.State.ToString()+R.StopSummary.ToString();
if(Key==PresentedContentKey)return;PresentedContentKey=MoveTemp(Key);RouteList->ClearChildren();for(const auto& R:S.Routes){const FString Id=FString::Printf(TEXT("TradeMap.Route.%lld"),R.RouteValue);TSharedPtr<SButton>B;RouteList->AddSlot().AutoHeight().Padding(0,3)[SAssignNew(B,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).State(R.RouteValue==S.SelectedRouteValue?EUiState::Selected:EUiState::Default).OnClicked(this,&SHansaTradeMap::Invoke,Id)[SNew(STextBlock).Text(FText::Format(LOCTEXT("RouteRow","{0}\n{1} · {2}\n{3}"),R.Label,R.Mode,R.State,R.StopSummary)).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]];MapWidget(Id,B);StaticCastSharedPtr<SHansaAction>(B)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Id]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*Id));}));}}
	void SHansaTradeMap::RebuildStops(const FHansaTradeMapSnapshot& S){FString Key=FString::FromInt(S.SelectedStopIndex);for(const auto& Stop:S.Stops)Key+=FString::FromInt(Stop.Index)+Stop.AccessibleLabel.ToString();
if(Key==PresentedStopKey)return;PresentedStopKey=MoveTemp(Key);StopList->ClearChildren();for(const auto& Stop:S.Stops){const FString Id=FString::Printf(TEXT("TradeMap.Stop.%d"),Stop.Index);TSharedPtr<SButton>B;StopList->AddSlot().AutoHeight().Padding(0,3)[SAssignNew(B,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).State(Stop.Index==S.SelectedStopIndex?EUiState::Selected:EUiState::Default).OnClicked(this,&SHansaTradeMap::Invoke,Id)[SNew(STextBlock).Text(Stop.AccessibleLabel).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]];MapWidget(Id,B);StaticCastSharedPtr<SHansaAction>(B)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,Id]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*Id));}));}}
	FReply SHansaTradeMap::Invoke(const FString Id){ActivateSemanticId(Id);return FReply::Handled();}
	bool SHansaTradeMap::ActivateSemanticId(const FString& Id){auto* P=Model.Get();if(!P||!P->GetSnapshot().bOpen)return false;
        if(Id==TEXT("TradeMap.Editor.Cancel"))
        {
            const bool Result=P->CancelRouteIntent();
            if(Result){FocusSemanticId(TEXT("TradeMap.New"));if(EditorScroll)EditorScroll->ScrollToEnd();}
            return Result;
        }
        if(Id==TEXT("TradeMap.Editor.Visit"))return P->VisitSelectedStopIntent();
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
        if(Id==TEXT("TradeMap.Close"))return P->CloseIntent();if(Id==TEXT("TradeMap.Mode.Filter"))return P->CycleModeFilterIntent();if(Id==TEXT("TradeMap.Editor.Action.Cycle"))return P->CycleCargoActionIntent();if(Id==TEXT("TradeMap.Editor.Quantity.Decrease"))return P->AdjustQuantityIntent(-5000);if(Id==TEXT("TradeMap.Editor.Quantity.Increase"))return P->AdjustQuantityIntent(5000);if(Id==TEXT("TradeMap.Editor.Reserve.Decrease"))return P->AdjustMinimumReserveIntent(-5000);if(Id==TEXT("TradeMap.Editor.Reserve.Increase"))return P->AdjustMinimumReserveIntent(5000);if(Id==TEXT("TradeMap.Editor.Stop.Up"))return P->MoveStopIntent(-1);if(Id==TEXT("TradeMap.Editor.Stop.Down"))return P->MoveStopIntent(1);if(Id==TEXT("TradeMap.Editor.Save"))return P->CommitIntent();if(Id==TEXT("TradeMap.Editor.ToggleActive"))return P->ToggleActiveIntent();if(Id.StartsWith(TEXT("TradeMap.Route.")))return P->SelectRouteIntent(FCString::Atoi64(*Id.RightChop(15)));if(Id.StartsWith(TEXT("TradeMap.Stop.")))return P->SelectStopIntent(FCString::Atoi(*Id.RightChop(14)));return false;}
	bool SHansaTradeMap::FocusSemanticId(const FString& Id){auto* P=Model.Get();const auto* Found=SemanticWidgets.Find(Id);const TSharedPtr<SWidget> Widget=Found?Found->Pin():nullptr;if(!P||!P->GetSnapshot().bOpen||!GetControllerFocusOrder().Contains(Id)||!Widget.IsValid()||!Widget->IsEnabled())return false;P->SetFocusedSemanticId(FName(*Id));if(EditorScroll&&(Id.StartsWith(TEXT("TradeMap.Editor."))||Id.StartsWith(TEXT("TradeMap.Stop."))||Id.StartsWith(TEXT("TradeMap.Creator."))))EditorScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);if(RouteScroll&&Id.StartsWith(TEXT("TradeMap.Route.")))RouteScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);return true;}
    TArray<FString> SHansaTradeMap::GetControllerFocusOrder()const
    {
        TArray<FString> R; const auto* P=Model.Get(); if(!P||!P->GetSnapshot().bOpen)return R;
        const auto& S=P->GetSnapshot(); R.Add(TEXT("TradeMap.Close"));
        if(!S.bCreating) { R.Append({TEXT("TradeMap.New"),TEXT("TradeMap.Mode.Filter")}); for(const auto& X:S.Routes)R.Add(FString::Printf(TEXT("TradeMap.Route.%lld"),X.RouteValue)); }
        if(S.bCreating&&!S.bReview)R.Append({TEXT("TradeMap.Creator.Name"),TEXT("TradeMap.Creator.Cog")});
        if(!S.bReview) {
            for(const auto& X:S.Stops)R.Add(FString::Printf(TEXT("TradeMap.Stop.%d"),X.Index));
            if(S.bCreating)R.Append({TEXT("TradeMap.Creator.City"),TEXT("TradeMap.Creator.Good")});
            R.Append({TEXT("TradeMap.Editor.Action.Cycle"),TEXT("TradeMap.Editor.Quantity.Decrease"),TEXT("TradeMap.Editor.Quantity.Increase"),TEXT("TradeMap.Editor.Reserve.Decrease"),TEXT("TradeMap.Editor.Reserve.Increase"),TEXT("TradeMap.Editor.Stop.Up"),TEXT("TradeMap.Editor.Stop.Down")});
            if(S.bCreating) R.Append({TEXT("TradeMap.Creator.Add"),TEXT("TradeMap.Creator.Remove"),TEXT("TradeMap.Creator.Review")});
            else {R.Add(TEXT("TradeMap.Editor.Visit"));R.Add(TEXT("TradeMap.Editor.Save"));const auto* Selected=S.Routes.FindByPredicate([&](const auto& X){return X.RouteValue==S.SelectedRouteValue;});if(Selected&&Selected->bCanToggleActive)R.Add(TEXT("TradeMap.Editor.ToggleActive"));if(Selected&&Selected->bCanCancel)R.Add(TEXT("TradeMap.Editor.Cancel"));}
        }
        if(S.bCreating&&S.bReview){R.Add(TEXT("TradeMap.Creator.Edit"));if(S.bCanCreate)R.Add(TEXT("TradeMap.Creator.Activate"));}
        if(S.bCreating)R.Add(TEXT("TradeMap.Creator.Discard")); return R;
    }

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
		Add(TEXT("TradeMap.Canvas"), TEXT("TradeMap.Root"), TEXT("Four-city Trade network map"), EHansaHudSemanticRole::Panel,
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
            if(Node.bCanFocus) {Node.bCanFocus=Order.Contains(Node.Id);Node.bCanActivate=Node.bCanFocus;Node.State.bEnabled=Node.bCanFocus;}
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
