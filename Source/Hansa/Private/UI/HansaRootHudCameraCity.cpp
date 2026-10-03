#include "UI/HansaRootHud.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaCityCentrePresentation.h"
#include "World/HansaRostockQuarter.h"
#include "World/HansaTradeStationPresentation.h"
#include "EngineUtils.h"

namespace {
#include "HansaMinimapCityLocations.inl"
// Presentation catchment in world centimetres, covering the town and its quay.
constexpr double CityNameRadius = 30000.;
}

void AHansaRootHud::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CameraCityRefreshElapsed += DeltaSeconds;
    if (CameraCityRefreshElapsed < .1f) return;
    CameraCityRefreshElapsed = 0.f;
    RefreshCameraCity();
}

void AHansaRootHud::RefreshCameraCity()
{
    const auto* Camera = PlayerOwner ? Cast<AHansaStrategyCameraPawn>(PlayerOwner->GetPawn()) : nullptr;
    if (!Camera || !PresentationModel || !GetWorld()) return;
    const bool bCampaign = Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(GetWorld());
    const FVector2D Focus = Camera->GetFocusLocation2D();
    FName CityId;
    double NearestSquared = FMath::Square(CityNameRadius);
    auto Consider = [&](FName Id, FVector2D Location) {
        const double DistanceSquared = FVector2D::DistSquared(Focus, Location);
        if (DistanceSquared < NearestSquared) { NearestSquared = DistanceSquared; CityId = Id; }
    };
    if (bCampaign)
    {
        for (const auto& City : CampaignCityLocations)
        {
            FVector2D Location = City.Value;
            // Match the minimap's promoted coastal gameplay positions.
            if (City.Key == TEXT("City.Lubeck")) Location = FVector2D(Hansa::Game::LubeckPlacementGrid::CampaignLubeckCenter());
            if (City.Key == TEXT("City.Rostock")) Location = FVector2D(AHansaTradeStationPresentation::SiteTransform(GetWorld()).TransformPosition(FVector(60000, 0, 100)));
            Consider(City.Key, Location);
        }
    }
    else
    {
        const FVector Home = Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(GetWorld())
            ? Hansa::Game::LubeckPlacementGrid::SurveyStartLocation() : FVector::ZeroVector;
        Consider(TEXT("City.Lubeck"), FVector2D(Home));
        for (TActorIterator<AHansaRostockQuarter> It(GetWorld()); It; ++It)
            if (!It->IsHidden()) Consider(TEXT("City.Rostock"), FVector2D(It->GetActorLocation()));
    }
    // Loaded municipal scenery also supplies its actual authored position.
    for (TActorIterator<AHansaCityCentrePresentation> It(GetWorld()); It; ++It)
        if (!It->IsHidden()) Consider(It->CityId, FVector2D(It->GetMarketLocation()));

    FText Label = NSLOCTEXT("HansaCameraCity", "Region", "Northern Europe");
    if (!CityId.IsNone())
    {
        Label = CityId == TEXT("City.Lubeck") ? NSLOCTEXT("HansaCameraCity", "Lubeck", "Lübeck")
            : FText::FromString(CityId.ToString().RightChop(5));
        if (TradeMapPresentationModel)
            if (const auto* City = TradeMapPresentationModel->GetMapCities().FindByPredicate([&](const auto& C) { return C.StableId == CityId; }))
                Label = City->Label;
    }
    auto Snapshot = PresentationModel->GetSnapshot();
    if (Snapshot.CityBreadcrumb.EqualTo(Label)) return;
    Snapshot.CityBreadcrumb = Label;
    PresentationModel->ApplySnapshot(Snapshot);
}
