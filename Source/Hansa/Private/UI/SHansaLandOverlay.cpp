#include "UI/SHansaLandOverlay.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLandOverlayRenderer.h"
#include "Placement/HansaRostockPlacement.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Camera/CameraComponent.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "HansaLandOverlay"
namespace Hansa::UI
{
using namespace Hansa::Simulation;
using namespace Hansa::Game::LandOverlay;

class SHansaLandLegendSwatch final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaLandLegendSwatch):_Surface(ESurface::Permitted){}
        SLATE_ARGUMENT(ESurface,Surface)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args){Surface=Args._Surface;}
    FVector2D ComputeDesiredSize(float) const override{return FVector2D(22,22);}
    int32 OnPaint(const FPaintArgs&,const ::FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,
        int32 Layer,const FWidgetStyle&,bool)const override
    {
        const auto White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        const auto Token=[](EHansaUiColorToken Id){return UHansaUiStyleLibrary::GetColor(Id);};
        FLinearColor Fill=Token(EHansaUiColorToken::BalticNavy);
        FLinearColor Edge=Token(EHansaUiColorToken::Chalk);
        if(Surface==ESurface::Permitted)Fill=Token(EHansaUiColorToken::ProsperityTeal);
        if(Surface==ESurface::Conditional)Edge=Token(EHansaUiColorToken::WarningAmber);
        if(Surface==ESurface::Restricted)Edge=Token(EHansaUiColorToken::Oxblood);
        if(Surface==ESurface::Hidden)Edge=Token(EHansaUiColorToken::Brass);
        FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),White,ESlateDrawEffect::None,Fill);
        const FVector2D A(2,2),B(20,2),C(20,20),D(2,20);
        auto Line=[&](TArray<FVector2D> Points,FLinearColor Color,float Width=2.f)
        {FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width);};
        if(Surface==ESurface::Unknown || Surface==ESurface::Conditional)
        {
            for(int32 X=2;X<=18;X+=6){Line({FVector2D(X,2),FVector2D(X+2,2)},Edge);Line({FVector2D(X,20),FVector2D(X+2,20)},Edge);}
            for(int32 Y=2;Y<=18;Y+=6){Line({FVector2D(2,Y),FVector2D(2,Y+2)},Edge);Line({FVector2D(20,Y),FVector2D(20,Y+2)},Edge);}
        }
        else Line({A,B,C,D,A},Edge);
        if(Surface==ESurface::Restricted || Surface==ESurface::Ownership)
            for(int32 I=0;I<3;++I)Line({FVector2D(4+I*5,17),FVector2D(8+I*5,5)},Edge,1.5f);
        return Layer+1;
    }
private:ESurface Surface;
};

FHansaLandOverlayUiState::FHansaLandOverlayUiState(AHansaStrategyPlayerController* InController)
    : Controller(InController), Availability(LOCTEXT("NoSession", "Land survey unavailable.")) {}
FHansaLandOverlayUiState::~FHansaLandOverlayUiState()
{
    if (Renderer.IsValid()) Renderer->Destroy();
}
void FHansaLandOverlayUiState::ToggleLand()
{
    if (Mode == EMode::Off) { Mode = LastMode; bPanelOpen = true; }
    else if (!bPanelOpen) bPanelOpen = true;
    else { Mode = EMode::Off; bPanelOpen = false; CloseSelection(); if(Renderer.IsValid()) Renderer->Clear(); }
    Update();
}
void FHansaLandOverlayUiState::SetMode(EMode NewMode)
{
    Mode = NewMode;
    if (Mode != EMode::Off) { LastMode = Mode; bPanelOpen = true; }
    else { bPanelOpen = false; CloseSelection(); if(Renderer.IsValid()) Renderer->Clear(); }
    Update();
}
void FHansaLandOverlayUiState::CloseSelection()
{
    bSelectionOpen = false;
    SelectedCell = FIntPoint::ZeroValue; SelectedRegion=0; SelectedBoundaries.Reset();SelectionRevision=0;
    SelectedStatus = EHansaLandViewStatus::Unavailable;
}
static AHansaLubeckWorldFoundation* Foundation(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AHansaLubeckWorldFoundation> It(World); It; ++It) return *It;
    return nullptr;
}
bool FHansaLandOverlayUiState::SelectWorldHit(const FHitResult& Hit)
{
    auto* PC = Controller.Get();
    auto* Hud = PC ? Cast<AHansaRootHud>(PC->GetHUD()) : nullptr;
    if (!IsActive() || !PC || !Hud || !Hit.bBlockingHit) return false;
    const auto* Actor = Hit.GetActor();
    const auto* Component = Hit.GetComponent();
    if (!Actor || (!Actor->IsA<ALandscapeProxy>() && !Actor->ActorHasTag(TEXT("Hansa.Terrain")) &&
        (!Component || !Component->ComponentHasTag(TEXT("Hansa.Terrain"))))) return false;
    if (const auto* Build = Hud->GetBuildMenuPresentationModel(); Build &&
        (Build->GetSnapshot().bDemolitionMode || !Build->GetSnapshot().SelectedBuildingId.IsNone())) return false;
    const FName City = Hud->GetViewedCity();
    int32 X = 0, Y = 0;
    if (City == TEXT("City.Lubeck"))
    {
        auto* Origin = Foundation(PC->GetWorld());
        if (!Origin || !Origin->WorldToPlacementCell(Hit.ImpactPoint, X, Y)) return false;
    }
    else if (City == TEXT("City.Rostock"))
    {
        const auto Cell = RostockPlacement::WorldToCell(Hit.ImpactPoint);
        X = Cell.X; Y = Cell.Y;
    }
    else return false;
    const auto CityId=FHansaCityDefinitionId::TryParse(City.ToString());
    if(!CityId)return false;
    if (City != ActiveCity) Update();
    SelectedBoundaries.Reset();SelectionRevision=0;
    SelectedCell = FIntPoint(X,Y);
    SelectedView = {};
    SelectedStatus = EHansaLandViewStatus::Pending;
    bSelectionOpen = true;
    Update();
    return true;
}
void FHansaLandOverlayUiState::FrameSelection()
{
    auto* PC = Controller.Get();
    auto* Camera = PC ? Cast<AHansaStrategyCameraPawn>(PC->GetPawn()) : nullptr;
    if (!Camera || !bSelectionOpen) return;
    FTransform Grid;
    const auto City = FHansaCityDefinitionId::TryParse(ActiveCity.ToString());
    if(!City)return;
    if(ActiveCity==TEXT("City.Rostock"))
        Grid=FTransform(FQuat::Identity,RostockPlacement::CellCenter(0,0,0)-FVector(200,200,0),FVector(400));
    else
    {
        auto* Origin = Foundation(PC->GetWorld());
        if(!Origin || !AHansaLandOverlayRenderer::MakeGridTransform(City.Value,*Origin,Grid))return;
    }
    const FBox2D Bounds=Survey.RegionBounds(SelectedRegion);
    if(!Bounds.bIsValid) return;
    const FVector2D Mid=Bounds.GetCenter();
    Camera->FocusWorldLocationIntent(Grid.TransformPosition(FVector(Mid,0)));
    int32 W=0,H=0;PC->GetViewportSize(W,H);
    const double Aspect=W>0 && H>0?double(W)/H:16./9.;
    const double HalfFov=FMath::DegreesToRadians((Camera->Camera?Camera->Camera->FieldOfView:60.f)*.5);
    const double Radius=Bounds.GetExtent().Size()*Grid.GetScale3D().X;
    // A conservative sphere fit reserves 35% for the inspector, tray and minimap.
    const double Distance=Radius/FMath::Sin(FMath::Atan(FMath::Tan(HalfFov)/FMath::Max(1.,Aspect)))/.65;
    Camera->AddZoomIntent((Camera->GetZoomDistance()-FMath::Max(double(Camera->MinimumZoomDistance),Distance))/Camera->ZoomUnitsPerStep);
}
void FHansaLandOverlayUiState::Update()
{
    auto* PC = Controller.Get();
    auto* Hud = PC ? Cast<AHansaRootHud>(PC->GetHUD()) : nullptr;
    auto* Camera = PC ? Cast<AHansaStrategyCameraPawn>(PC->GetPawn()) : nullptr;
    const FName City = Hud ? Hud->GetViewedCity() : NAME_None;
    bAvailable = false;
    bPlacementAssistanceActive = false;
    PlacementAssistanceCell.Reset();
    if (City != ActiveCity)
    {
        ActiveCity = City; CloseSelection();
        if (PC) PC->InvalidateLandQueries();
        if (Renderer.IsValid()) Renderer->Clear();
    }
    CityLabel = City == TEXT("City.Lubeck") ? TEXT("Lübeck") : City == TEXT("City.Rostock") ? TEXT("Rostock") : City.ToString();
    const auto CityId=FHansaCityDefinitionId::TryParse(City.ToString());
    const auto* Build = Hud ? Hud->GetBuildMenuPresentationModel() : nullptr;
    if (Build && !Build->GetSnapshot().SelectedBuildingId.IsNone()) CloseSelection();
    if (!PC || !Camera || !CityId)
    {
        Availability = LOCTEXT("Unavailable", "Land survey unavailable in this view.");
        if(Renderer.IsValid()) Renderer->Clear();
        return;
    }
    // The construction ghost owns the world interaction. Only its current target
    // receives a small, read-only permission view; the player's explicit mode and
    // panel disclosure survive repeat placement, cancellation and city visits.
    if (Build && Build->IsConstructionAllowed() && Build->GetPlacementCity() == City)
    {
        const FHansaBuildMenuSnapshot& Preview = Build->GetSnapshot();
        if (!Preview.bDemolitionMode && !Preview.SelectedBuildingId.IsNone() && Preview.bHasTarget)
        {
            bPlacementAssistanceActive = true;
            PlacementAssistanceCell = Preview.AnchorCell;
        }
    }
    FTransform Grid;
    FIntPoint Center;
    if (City == TEXT("City.Lubeck"))
    {
        auto* Origin = Foundation(PC->GetWorld());
        if (!Origin || !Origin->WorldToPlacementCell(FVector(Camera->GetFocusLocation2D(),0),Center.X,Center.Y) ||
            !AHansaLandOverlayRenderer::MakeGridTransform(CityId.Value,*Origin,Grid)) return;
    }
    else if (City == TEXT("City.Rostock"))
    {
        const auto P = RostockPlacement::WorldToCell(FVector(Camera->GetFocusLocation2D(),0));
        Center = FIntPoint(P.X,P.Y);
        Grid = FTransform(FQuat::Identity,RostockPlacement::CellCenter(0,0,0)-FVector(200,200,0),FVector(400));
    }
    else return;
    SurveyGrid=Grid;CameraFootprint=CameraGroundFootprint(PC,Grid.GetTranslation().Z);
    bAvailable = true;
    Availability = bPlacementAssistanceActive
        ? LOCTEXT("PlacementAssistance", "Nearby construction rights shown during placement. Exact footprint rules still apply; your Land mode returns when placement ends.")
        : LOCTEXT("SurveyAvailable", "Select land to inspect its owner and construction rights.");
    if (Mode == EMode::Off && !bPlacementAssistanceActive) { if(Renderer.IsValid()) Renderer->Clear(); return; }
    if (!Renderer.IsValid() || Renderer->GetWorld()!=PC->GetWorld())
    {
        Renderer = PC->GetWorld()->SpawnActor<AHansaLandOverlayRenderer>();
        if (!Renderer.IsValid() || !Renderer->HasGroundMaterial()) { Availability=LOCTEXT("MaterialUnavailable","Land overlay material unavailable."); return; }
    }
    SurveyGrid=Grid;
    if(SurveyEpoch!=PC->GetLandSurveyEpoch()) { Survey.Reset();SelectedBoundaries.Reset();SurveyEpoch=PC->GetLandSurveyEpoch();LastSurveyConfirmation=-10; }
    const double Now=FPlatformTime::Seconds();
    EHansaLandViewStatus PageStatus=EHansaLandViewStatus::Pending;
    if(Mode!=EMode::Off) for(int32 Pass=0;Pass<4;++Pass)
    {
        FHansaLandQueryResult Page;
        const int32 PageNumber=Survey.NextPage();
        PageStatus=PC->QueryLandForView(uint8(5+PageNumber%4),City,{PageNumber,0},{PageNumber,0},Page);
        if(PageStatus==EHansaLandViewStatus::Ready)
        {
            if(Survey.AcceptPage(PageNumber,Page)) { LastSurveyConfirmation=Now;break; }
        }
        else
        {
            if(PageStatus==EHansaLandViewStatus::Unavailable || (Survey.IsReady() && Now-LastSurveyConfirmation>1.5))Survey.Reset();
            break;
        }
    }
    // Four page channels amortize latency without flooding the existing rate budget.
    if(Mode!=EMode::Off && !Survey.IsReady())for(int32 I=0;I<4 && Survey.NextPage()+I<Survey.PageCount();++I)
    {
        const int32 N=Survey.NextPage()+I;FHansaLandQueryResult Ignored;
        PC->QueryLandForView(uint8(5+N%4),City,{N,0},{N,0},Ignored);
    }
    if(bSelectionOpen)
    {
        SelectedRegion=Survey.RegionAt(SelectedCell);
        auto Cell=Survey.Extract(SelectedCell,SelectedCell);
        SelectedStatus=Survey.IsReady()?EHansaLandViewStatus::Ready:(PageStatus==EHansaLandViewStatus::Unavailable?PageStatus:EHansaLandViewStatus::Pending);
        SelectedView=Cell.Cells.Num()==1?Cell.Cells[0]:FHansaLandCellView();
        if(SelectionRevision!=Survey.Get().StateRevision || !SelectedBoundaries.IsValid())
        {
            SelectedBoundaries=MakeShared<const TArray<FBoundary>>(SurveySelectionBoundaries(Survey.Get().Cells,SelectedRegion));
            SelectionRevision=Survey.Get().StateRevision;
        }
    }
    TArray<FIntPoint> Keep;
    int32 ViewWidth=0,ViewHeight=0;
    PC->GetViewportSize(ViewWidth,ViewHeight);
    const float LineWidth=RibbonWidthForView(Camera->GetZoomDistance(),
        Camera->Camera?Camera->Camera->FieldOfView:60.f,ViewWidth,Camera->GetCameraPitchDegrees(),bHighContrast?3.5f:2.5f);
    bool bPendingSurvey=false, bUnavailableSurvey=false;
    auto ApplyVisibleChunk=[&](uint8 Slot, FIntPoint Id, FIntPoint Min, FIntPoint Max, const FHansaLandOverlayOptions& Options)
    {
        FHansaLandQueryResult Query;
        const auto Status=PC->QueryLandForView(Slot,City,Min-FIntPoint(1,1),Max+FIntPoint(1,1),Query);
        bPendingSurvey|=Status==EHansaLandViewStatus::Pending;
        bUnavailableSurvey|=Status==EHansaLandViewStatus::Unavailable;
        if (Status==EHansaLandViewStatus::Ready && Renderer->ApplyChunk(Id,Query,Min,Max,Grid,Options)) Keep.Add(Id);
    };
    if (bPlacementAssistanceActive)
    {
        // A 13x13 core plus one-cell halo stays well below the query and renderer
        // bounds. It follows the road endpoint or rotated building anchor, rather
        // than painting the entire city during construction.
        const FIntPoint Min = *PlacementAssistanceCell-FIntPoint(6,6);
        const FIntPoint Max = *PlacementAssistanceCell+FIntPoint(6,6);
        FHansaLandOverlayOptions Options;
        Options.Mode=EMode::Buildable; Options.bHighContrast=bHighContrast;
        Options.LineWidthCm=LineWidth;
        const FIntPoint Id(0,0);
        ApplyVisibleChunk(0,Id,Min,Max,Options);
    }
    else if(Survey.IsReady())
    {
        const auto& Data=Survey.Get();
        int32 Stride=1;
        TArray<FIntPoint> Chunks;
        FTransform RenderGrid=Grid;
        do
        {
            RenderGrid=Grid;RenderGrid.SetScale3D(Grid.GetScale3D()*Stride);
            Chunks=VisibleSurveyChunks(CameraFootprint,RenderGrid,
                {FMath::FloorToInt32(double(Data.BoundsMin.X)/Stride),FMath::FloorToInt32(double(Data.BoundsMin.Y)/Stride)},
                {FMath::FloorToInt32(double(Data.BoundsMax.X)/Stride),FMath::FloorToInt32(double(Data.BoundsMax.Y)/Stride)},
                {FMath::FloorToInt32(double(Center.X)/Stride),FMath::FloorToInt32(double(Center.Y)/Stride)});
            if(Chunks.Num()<=64)break;
            Stride*=2;
        }while(Stride<=128);
        // Retire obsolete chunks before acquiring pooled components. Spatial IDs
        // retain overlapping geometry during ordinary panning.
        Renderer->RetainChunks(Chunks);
        for(const FIntPoint Id:Chunks)
        {
            const FIntPoint Min=Id*30,Max=Min+FIntPoint(29,29);
            auto Query=Survey.Extract(Min-FIntPoint(1,1),Max+FIntPoint(1,1),Stride);
            FHansaLandOverlayOptions Options;Options.Mode=Mode;Options.bHighContrast=bHighContrast;
            Options.LineWidthCm=LineWidth;Options.SurveyStride=Stride;
            Options.SelectionBoundaries=bSelectionOpen?SelectedBoundaries:nullptr;
            Options.SelectionRevision=HashCombineFast(GetTypeHash(SelectionRevision),GetTypeHash(SelectedRegion));
            if(Renderer->ApplyChunk(Id,Query,Min,Max,RenderGrid,Options))Keep.Add(Id);
        }
    }
    else { bPendingSurvey=PageStatus!=EHansaLandViewStatus::Unavailable; bUnavailableSurvey=!bPendingSurvey; }

    Renderer->RetainChunks(Keep);
    bAvailable=!Keep.IsEmpty();
    if (bPendingSurvey) Availability=LOCTEXT("SurveyPending","Waiting for the land survey. Unconfirmed areas are unavailable.");
    else if (bUnavailableSurvey) Availability=LOCTEXT("SurveyRejected","Land survey unavailable in this view.");

}
FText FHansaLandOverlayUiState::GetOwnerText() const
{
    return SelectedView.bSurveyKnown && SelectedView.RecordedOwnerId.IsValid()
        ? FText::Format(LOCTEXT("OwnerHouse", "House {0}"), FText::AsNumber(SelectedView.RecordedOwnerId.GetValue()))
        : LOCTEXT("OwnerUnknown", "Unavailable");
}
FText FHansaLandOverlayUiState::GetAccessText() const
{
    if (!SelectedView.bSurveyKnown) return LOCTEXT("NoSurvey","Unavailable");
    if (SelectedRegion<=0 && SelectedView.Terrain==EHansaPlacementTerrain::Water) return LOCTEXT("Water","Water — no construction");
    if (SelectedRegion<=0 && SelectedView.OccupyingBuildingId.IsValid()) return LOCTEXT("Occupied","Occupied — no new building");
    if (SelectedRegion<=0 && SelectedView.bProtected) return LOCTEXT("Protected","Protected — no construction");
    switch(SelectedView.Access)
    {
    case EHansaLandAccess::Permitted: return LOCTEXT("Permitted","Permitted");
    case EHansaLandAccess::Conditional: return LOCTEXT("Conditional","Conditional");
    case EHansaLandAccess::Denied: return LOCTEXT("Denied","Not permitted");
    default: return LOCTEXT("UnknownAccess","Unavailable");
    }
}
FText FHansaLandOverlayUiState::GetReasonText() const
{
    if (SelectedStatus==EHansaLandViewStatus::Pending) return LOCTEXT("SelectedPending","Waiting for the land survey.");
    if (SelectedStatus==EHansaLandViewStatus::Unavailable) return LOCTEXT("SelectedUnavailable","Land survey unavailable in this view.");
    if(!SelectedView.bSurveyKnown) return LOCTEXT("Unsure","This land has not been surveyed.");
    if(SelectedRegion<=0 && SelectedView.Terrain==EHansaPlacementTerrain::Water) return LOCTEXT("WaterReason","Buildings need dry land.");
    if(SelectedRegion<=0 && SelectedView.OccupyingBuildingId.IsValid()) return LOCTEXT("OccupiedReason","An existing building occupies this cell.");
    if(SelectedRegion<=0 && SelectedView.bProtected) return LOCTEXT("ProtectedReason","This land is reserved for public use.");
    switch(SelectedView.Reason)
    {
    case EHansaLandAccessReason::StartingCity: return LOCTEXT("StartingCity","Ownership does not restrict construction in Lübeck.");
    case EHansaLandAccessReason::RecordedOwner: return LOCTEXT("RecordedOwner","Your house holds this land.");
    case EHansaLandAccessReason::ActiveLease: return LOCTEXT("ActiveLease","Your active lease grants conditional access; the selected building category still needs validation.");
    case EHansaLandAccessReason::ForeignLeaseRequired: return LOCTEXT("LeaseRequired","An eligible lease is required for construction here.");
    case EHansaLandAccessReason::ForeignLeaseInactive: return LOCTEXT("LeaseInactive","The lease for this land is inactive.");
    case EHansaLandAccessReason::ForeignPresenceInsufficient: return LOCTEXT("PresenceRequired","Your presence in this city does not yet permit construction here.");
    default: return LOCTEXT("UnknownReason","Construction rights are unavailable.");
    }
}
FText FHansaLandOverlayUiState::GetSelectionHelpText() const
{
    return SelectedView.Access==EHansaLandAccess::Permitted && SelectedView.bSurveyKnown &&
        SelectedView.Terrain!=EHansaPlacementTerrain::Water && !SelectedView.bProtected && !SelectedView.OccupyingBuildingId.IsValid()
        ? LOCTEXT("PlacementCaveat","Buildings you construct belong to your house. Terrain, footprint, road and cost rules still apply.")
        : LOCTEXT("PermissionCaveat","Exact building placement still depends on its footprint and the construction quote.");
}

void SHansaLandPanel::Construct(const FArguments& Args)
{
    State=Args._State;
    OnClosed=Args._OnClosed;
    const auto Pref=Args._Preferences;
    const auto Caption=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption,true);
    const auto Heading=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,true);
    struct FRow{ESurface Surface;FText Label;};
    auto Legend=[Caption](std::initializer_list<FRow> Lines)->TSharedRef<SWidget>
    {
        auto Rows=SNew(SVerticalBox);
        for(const auto& Row:Lines) Rows->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaLandLegendSwatch).Surface(Row.Surface)]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(Row.Label).TextStyle(&Caption).AutoWrapText(true)]];
        return Rows;
    };
    ChildSlot[SNew(SBox).WidthOverride(304.f)
      [SNew(SHansaReferenceFrame).Dark(true).Padding(12)
       [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("Title","Land")).TextStyle(&Heading)]
            +SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton,SHansaAction).Compact(true).Kind(EHansaUiButtonStyle::Icon).Preferences(Pref).Label(LOCTEXT("Hide","Close")).Reason(LOCTEXT("HideTip","Close the Land panel; keep the chosen overlay visible.")).OnClicked_Lambda([this]{State->ClosePanel();OnClosed.ExecuteIfBound();return FReply::Handled();})]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,8)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(BuildableButton,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Preferences(Pref).Label(LOCTEXT("Buildable","Buildable land")).OnClicked_Lambda([this]{State->SetMode(EMode::Buildable);return FReply::Handled();})]
            +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[SAssignNew(OwnershipButton,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Preferences(Pref).Label(LOCTEXT("Ownership","Ownership")).OnClicked_Lambda([this]{State->SetMode(EMode::Ownership);return FReply::Handled();})]]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(BuildableLegend,SBox)[Legend({{ESurface::Permitted,LOCTEXT("LegendPermitted","Construction permitted")},{ESurface::Conditional,LOCTEXT("LegendConditional","Conditional access")},{ESurface::Restricted,LOCTEXT("LegendRestricted","Restricted placement")},{ESurface::Unknown,LOCTEXT("LegendUnknown","Unavailable")}})]]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(OwnershipLegend,SBox)[Legend({{ESurface::Ownership,LOCTEXT("LegendOwner","Recorded owner")},{ESurface::Hidden,LOCTEXT("LegendSelected","Selected land")},{ESurface::Unknown,LOCTEXT("LegendOwnerUnknown","Owner unavailable")}})]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SAssignNew(AvailabilityText,STextBlock).TextStyle(&Caption).AutoWrapText(true)]
       ]]];
    RegisterActiveTimer(.5f,FWidgetActiveTimerDelegate::CreateSP(this,&SHansaLandPanel::Refresh));
    Refresh(0,0);
}
EActiveTimerReturnType SHansaLandPanel::Refresh(double,float)
{
    if(!State) return EActiveTimerReturnType::Stop;
    const bool BuildableSelected=State->GetMode()==EMode::Buildable;
    const bool ShowBuildableLegend=State->GetRenderedMode()==EMode::Buildable;
    BuildableButton->SetUnderlinedSelection(BuildableSelected);
    OwnershipButton->SetUnderlinedSelection(!BuildableSelected);
    BuildableButton->SetState(BuildableSelected?EUiState::Selected:EUiState::Default);
    OwnershipButton->SetState(BuildableSelected?EUiState::Default:EUiState::Selected);
    BuildableLegend->SetVisibility(ShowBuildableLegend?EVisibility::Visible:EVisibility::Collapsed);
    OwnershipLegend->SetVisibility(ShowBuildableLegend?EVisibility::Collapsed:EVisibility::Visible);
    AvailabilityText->SetText(State->GetAvailabilityText());
    return EActiveTimerReturnType::Continue;
}
TSharedPtr<SWidget> SHansaLandPanel::Resolve(const FString& Id) const
{
    if(Id==TEXT("LandOverlay.Panel")) return ConstCastSharedRef<SHansaLandPanel>(SharedThis(this));
    if(Id==TEXT("LandOverlay.Mode.Buildable")) return BuildableButton;
    if(Id==TEXT("LandOverlay.Mode.Ownership")) return OwnershipButton;
    if(Id==TEXT("LandOverlay.Legend")) return State->GetRenderedMode()==EMode::Buildable?BuildableLegend:OwnershipLegend;
    if(Id==TEXT("LandOverlay.Close")) return CloseButton;
    return nullptr;
}
bool SHansaLandPanel::Activate(const FString& Id)
{
    if(!State || !State->IsPanelOpen()) return false;
    if(Id==TEXT("LandOverlay.Mode.Buildable")) State->SetMode(EMode::Buildable);
    else if(Id==TEXT("LandOverlay.Mode.Ownership")) State->SetMode(EMode::Ownership);
    else if(Id==TEXT("LandOverlay.Close")) State->ClosePanel();
    else return false;
    return true;
}

void SHansaLandInspector::Construct(const FArguments& Args)
{
    State=Args._State;
    const auto Pref=Args._Preferences;
    const auto Heading=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,false);
    const auto Body=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::SerifBody,false);
    const auto Caption=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption,false);
    ChildSlot[SNew(SHansaReferenceFrame).Dark(false).Padding(14)
       [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(TitleText,STextBlock).TextStyle(&Heading)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,4)[SAssignNew(OwnerText,STextBlock).TextStyle(&Body).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,10)[SAssignNew(AccessText,STextBlock).TextStyle(&Body).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,6)[SAssignNew(ReasonText,STextBlock).TextStyle(&Body).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,10)[SAssignNew(HelpText,STextBlock).TextStyle(&Caption).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(FrameButton,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Pref).Label(LOCTEXT("Frame","Frame land")).OnClicked_Lambda([this]{State->FrameSelection();return FReply::Handled();})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SAssignNew(CloseButton,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Pref).Label(LOCTEXT("CloseSelection","Close")).OnClicked_Lambda([this]{State->CloseSelection();return FReply::Handled();})]
       ]];
    RegisterActiveTimer(.5f,FWidgetActiveTimerDelegate::CreateLambda([this](double,float)
    {
        if(!State)return EActiveTimerReturnType::Stop;
        SetVisibility(State->HasSelection()?EVisibility::Visible:EVisibility::Collapsed);
        FrameButton->SetState(State->CanFrameSelection()?EUiState::Default:EUiState::Disabled,
            State->CanFrameSelection()?FText():LOCTEXT("FrameUnavailable","A complete surveyed territory is required to frame land."));
        if(State->HasSelection())
        {
            TitleText->SetText(FText::Format(LOCTEXT("LandTitle","{0} land"),FText::FromString(State->GetCityLabel())));
            OwnerText->SetText(FText::Format(LOCTEXT("OwnerRow","Owner: {0}"),State->GetOwnerText()));
            AccessText->SetText(FText::Format(LOCTEXT("AccessRow","Construction: {0}"),State->GetAccessText()));
            ReasonText->SetText(State->GetReasonText()); HelpText->SetText(State->GetSelectionHelpText());
        }
        return EActiveTimerReturnType::Continue;
    }));
    SetVisibility(EVisibility::Collapsed);
}
TSharedPtr<SWidget> SHansaLandInspector::Resolve(const FString& Id) const
{
    if(Id==TEXT("LandOverlay.SelectedLand")) return ConstCastSharedRef<SHansaLandInspector>(SharedThis(this));
    if(Id==TEXT("LandOverlay.Owner")) return OwnerText;
    if(Id==TEXT("LandOverlay.Permission")) return AccessText;
    if(Id==TEXT("LandOverlay.Reason")) return ReasonText;
    if(Id==TEXT("LandOverlay.Frame")) return FrameButton;
    if(Id==TEXT("LandOverlay.Selection.Close")) return CloseButton;
    return nullptr;
}
bool SHansaLandInspector::Activate(const FString& Id)
{
    if(!State || !State->HasSelection()) return false;
    if(Id==TEXT("LandOverlay.Frame")) { if(!State->CanFrameSelection())return false;State->FrameSelection(); }
    else if(Id==TEXT("LandOverlay.Selection.Close")) State->CloseSelection();
    else return false;
    return true;
}
}
#undef LOCTEXT_NAMESPACE
