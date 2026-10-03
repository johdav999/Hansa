#include "World/HansaAmbientView.h"
#include "World/HansaCityCentrePresentation.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"

bool Hansa::Game::FAmbientView::Refresh(UWorld* World, const AHansaLubeckWorldFoundation& Foundation, FName HomeCity, float Delta)
{
    PollIn -= Delta;
    if (PollIn > 0) return false;
    PollIn = .25f;
    if (!bHasHomeCenter) { HomeCenter=Foundation.GetAutomationStartTransform().GetLocation(); bHasHomeCenter=true; }
    const FName Previous = City;
    const auto PreviousCentre = Centre;
    Centre.Reset(); City = HomeCity; bEnabled = true; bHasCamera = false; bHasFrustum = false;
    const auto* Controller = World->GetFirstPlayerController();
    const auto* Camera = Controller ? Cast<AHansaStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
    // Explicit headless fixtures have no player camera and exercise the home-city behavior.
    if (!Camera) return Previous != City || PreviousCentre != Centre;
    bHasCamera = true;
    if (const auto* Player=Controller->GetLocalPlayer(); Player && Player->ViewportClient && Player->ViewportClient->Viewport)
    {
        FSceneViewProjectionData Projection;
        if (Player->GetProjectionData(Player->ViewportClient->Viewport,Projection))
        {
            GetViewFrustumBounds(Frustum,Projection.ComputeViewProjectionMatrix(),false);
            bHasFrustum=true;
        }
    }
    const auto XY = Camera->GetFocusLocation2D(); Focus = FVector(XY.X, XY.Y, 0);
    double Best = FMath::Square(PreviousCentre.IsValid()?20000.:18000.);
    for (TActorIterator<AHansaCityCentrePresentation> It(World); It; ++It)
    {
        if (It->IsHidden()) continue;
        const double Distance = FVector::DistSquared2D(Focus, It->GetMarketLocation());
        const double Radius=PreviousCentre.Get()==*It?20000.:18000.;
        if (Distance < Best && Distance <= FMath::Square(Radius)) { Best = Distance; Centre = *It; City = It->CityId; }
    }
    // At strategic zoom the characters are too small to read. Hysteresis prevents edge flicker.
    const double Radius = Previous == City ? 20000. : 18000.;
    bEnabled = Camera->GetZoomDistance() <= 20000.f &&
        (Centre.IsValid() || FVector::DistSquared2D(Focus, HomeCenter) <= FMath::Square(Radius));
    return Previous != City || PreviousCentre != Centre;
}

bool Hansa::Game::FAmbientView::Includes(const FVector& Position) const
{
    // A margin covers body height/shadows and the 250ms camera polling interval.
    return bEnabled && (!bHasCamera || FVector::DistSquared2D(Position, Focus) <= FMath::Square(12000.)) &&
        (!bHasFrustum || Frustum.IntersectSphere(Position+FVector(0,0,100),600.));
}
