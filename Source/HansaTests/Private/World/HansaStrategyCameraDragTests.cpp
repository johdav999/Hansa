#include "World/HansaStrategyCameraModel.h"
#include "World/HansaStrategyCameraPawn.h"
#include "Engine/World.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include <limits>
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStrategyCameraDragTest,
    "Hansa.UI.World.StrategyCameraDrag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaStrategyCameraDragTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Game;
    FHansaStrategyCameraState Initial;
    Initial.YawDegrees = 0.0f;
    FHansaStrategyCameraSettings Settings;
    FHansaStrategyCameraIntent Drag;
    Drag.PanDisplacement = FVector2D(120.0, 60.0);
    const auto Result = FHansaStrategyCameraModel::Advance(Initial, Drag, Settings, 1.0f / 30.0f);
    TestTrue(TEXT("Right/up moves right/forward"), Result.Focus.Equals(FVector2D(60.0, 120.0)));
    TestTrue(TEXT("Drag is frame-rate independent"),
        FHansaStrategyCameraModel::Advance(Initial, Drag, Settings, 1.0f / 144.0f).Focus.Equals(Result.Focus));
    Drag.bFastPan = true;
    TestTrue(TEXT("Shift does not multiply drag"),
        FHansaStrategyCameraModel::Advance(Initial, Drag, Settings, 0.0f).Focus.Equals(Result.Focus));
    Initial.YawDegrees = 90.0f;
    TestTrue(TEXT("Drag follows rotated camera axes"),
        FHansaStrategyCameraModel::Advance(Initial, Drag, Settings, 0.0f).Focus.Equals(FVector2D(-120.0, 60.0), 0.001));
    Drag.PanDisplacement = FVector2D(100000.0, 100000.0);
    TestTrue(TEXT("Drag respects map bounds"),
        FHansaStrategyCameraModel::Advance(Initial, Drag, Settings, 0.0f).Focus.Equals(
            FVector2D(Settings.BoundsMin.X, Settings.BoundsMax.Y)));
	TestEqual(TEXT("Yaw uses horizontal pointer travel"),
		FHansaStrategyCameraModel::YawPointerDisplacement(FVector2D(40.0f, 125.0f)), 40.0f);
	TestEqual(TEXT("Opposing diagonal travel cannot cancel yaw"),
		FHansaStrategyCameraModel::YawPointerDisplacement(FVector2D(40.0f, -40.0f)), 40.0f);
	TestEqual(TEXT("Vertical pointer travel does not change yaw"),
		FHansaStrategyCameraModel::YawPointerDisplacement(FVector2D(0.0f, 125.0f)), 0.0f);
	TestTrue(TEXT("Captured drag continues while the pointer remains inside the viewport"),
		FHansaStrategyCameraModel::ShouldContinuePointerDrag(true, true, true, true));
	TestFalse(TEXT("Released button ends captured drag"),
		FHansaStrategyCameraModel::ShouldContinuePointerDrag(false, true, true, true));
	TestFalse(TEXT("Application focus loss ends captured drag"),
		FHansaStrategyCameraModel::ShouldContinuePointerDrag(true, true, false, true));
	TestFalse(TEXT("Leaving viewport geometry ends captured drag"),
		FHansaStrategyCameraModel::ShouldContinuePointerDrag(true, true, true, false));

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Camera test world"), World)) return false;
    auto* Pawn = World->SpawnActor<AHansaStrategyCameraPawn>();
    if (TestNotNull(TEXT("Camera pawn"), Pawn))
    {
        Pawn->bEnableMouseEdgePan = false;
        // This isolated world never begins play; activate the camera as normal
        // actor initialization does, otherwise CalcCamera falls back to pawn eyes.
        Pawn->Camera->Activate(true);
        Initial.Focus = FVector2D::ZeroVector;
        Initial.YawDegrees = 0.0f;
        Pawn->RestoreViewState(Initial);
        Pawn->SetDragPanIntent(FVector2D(10.0, -10.0), true);
        Pawn->Tick(1.0f / 60.0f);
        const auto After = Pawn->GetFocusLocation2D();
        TestTrue(TEXT("Dragging right/up moves the camera left/backward"), After.Equals(FVector2D(-100.0, -100.0)));
        Pawn->Tick(1.0f / 60.0f);
        TestTrue(TEXT("Stationary hold does not repeat delta"), Pawn->GetFocusLocation2D().Equals(After));
        Pawn->SetDragPanIntent(FVector2D(100.0, 0.0), true);
        Pawn->SetDragPanIntent(FVector2D::ZeroVector, false);
        Pawn->Tick(1.0f / 60.0f);
        TestTrue(TEXT("Release clears pending movement"), Pawn->GetFocusLocation2D().Equals(After));
        Pawn->SetDragPanIntent(FVector2D(-10.0, 10.0), true);
        Pawn->Tick(1.0f / 60.0f);
        TestTrue(TEXT("Left/down reverses original drag"), Pawn->GetFocusLocation2D().Equals(Initial.Focus));
        Pawn->SetDragPanIntent(FVector2D(100.0, 0.0), true);
        Pawn->ClearCameraIntents();
        Pawn->Tick(1.0f / 60.0f);
        TestTrue(TEXT("Reset clears pending drag"), Pawn->GetFocusLocation2D().Equals(Initial.Focus));

        Pawn->SetDragRotateIntent(40.0f, true);
        Pawn->Tick(1.0f / 30.0f);
        TestEqual(TEXT("Horizontal rotation drag applies sensitivity"), Pawn->GetCameraYawDegrees(), 10.0f);
        TestTrue(TEXT("Rotation preserves focus"), Pawn->GetFocusLocation2D().Equals(Initial.Focus));
        TestEqual(TEXT("Rotation preserves zoom"), Pawn->GetZoomDistance(), Initial.ZoomDistance);
        Pawn->Tick(1.0f / 30.0f);
        TestEqual(TEXT("Stationary rotation does not repeat"), Pawn->GetCameraYawDegrees(), 10.0f);
        Pawn->SetDragRotateIntent(-40.0f, true);
        Pawn->Tick(1.0f / 144.0f);
        TestEqual(TEXT("Opposite drag reverses yaw independent of frame rate"), Pawn->GetCameraYawDegrees(), 0.0f);
        Pawn->SetDragRotateIntent(40.0f, true);
        Pawn->SetDragRotateIntent(0.0f, false);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Release clears pending rotation"), Pawn->GetCameraYawDegrees(), 0.0f);
        Pawn->SetDragRotateIntent(40.0f, true);
        Pawn->ClearCameraIntents();
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Reset clears pending rotation"), Pawn->GetCameraYawDegrees(), 0.0f);
        Pawn->SetDragRotateIntent(1480.0f, true);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Rotation wraps safely across a full turn"), Pawn->GetCameraYawDegrees(), 10.0f);

        Pawn->RestoreViewState(Initial);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, -40.0), true);
        Pawn->Tick(1.0f / 30.0f);
        TestEqual(TEXT("Upward mouse movement tilts upward"), Pawn->GetCameraPitchDegrees(), -45.0f);
        TestEqual(TEXT("Pure vertical orbit preserves yaw"), Pawn->GetCameraYawDegrees(), 0.0f);
        TestTrue(TEXT("Tilt preserves focus"), Pawn->GetFocusLocation2D().Equals(Initial.Focus));
        TestEqual(TEXT("Tilt preserves zoom"), Pawn->GetZoomDistance(), Initial.ZoomDistance);
        Pawn->CameraBoom->TickComponent(1.0f / 30.0f, LEVELTICK_All, nullptr);
        FMinimalViewInfo View;
        Pawn->CalcCamera(1.0f / 30.0f, View);
        AddInfo(FString::Printf(TEXT("Vertical drag POV: pitch=%.3f yaw=%.3f location=%s"),
            View.Rotation.Pitch, View.Rotation.Yaw, *View.Location.ToString()));
        TestTrue(TEXT("Actual camera POV receives upward pitch"), FMath::IsNearlyEqual(View.Rotation.Pitch, -45.0, 0.001));
        const FVector Focus(Pawn->GetActorLocation());
        TestTrue(TEXT("Camera still looks at its orbit focus"),
            (View.Location + View.Rotation.Vector() * Pawn->GetZoomDistance()).Equals(Focus, 0.01));
        Pawn->Tick(1.0f / 30.0f);
        TestEqual(TEXT("Stationary hold does not repeat pitch"), Pawn->GetCameraPitchDegrees(), -45.0f);
        Pawn->SetDragOrbitIntent(FVector2D(40.0, 40.0), true);
        Pawn->Tick(1.0f / 144.0f);
        TestEqual(TEXT("Downward mouse movement reverses pitch independent of frame rate"), Pawn->GetCameraPitchDegrees(), -55.0f);
        TestEqual(TEXT("Diagonal orbit also changes yaw"), Pawn->GetCameraYawDegrees(), 10.0f);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, -40.0), true);
        Pawn->SetDragOrbitIntent(FVector2D::ZeroVector, false);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Release or Ctrl-to-pan switch clears pending pitch"), Pawn->GetCameraPitchDegrees(), -55.0f);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, -40.0), true);
        Pawn->ClearCameraIntents();
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Focus-loss reset clears pitch"), Pawn->GetCameraPitchDegrees(), -55.0f);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, -10000.0), true);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Upward tilt clamps above the focus plane"), Pawn->GetCameraPitchDegrees(), -15.0f);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, 10000.0), true);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Downward tilt clamps before camera flip"), Pawn->GetCameraPitchDegrees(), -80.0f);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, -4.0), true);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Tilt immediately reverses away from limit"), Pawn->GetCameraPitchDegrees(), -79.0f);
        const auto SavedView = Pawn->GetViewState();
        Pawn->RestoreViewState(Initial);
        Pawn->RestoreViewState(SavedView);
        TestEqual(TEXT("City view restoration preserves pitch"), Pawn->GetCameraPitchDegrees(), -79.0f);
        Pawn->SetDragOrbitIntent(FVector2D(0.0, std::numeric_limits<double>::infinity()), true);
        Pawn->Tick(1.0f / 60.0f);
        TestEqual(TEXT("Invalid pointer cannot corrupt pitch"), Pawn->GetCameraPitchDegrees(), -79.0f);
        FHansaStrategyCameraIntent InvalidPitch;
        InvalidPitch.PitchDisplacement = std::numeric_limits<float>::quiet_NaN();
        TestEqual(TEXT("Reducer rejects non-finite pitch intent"),
            FHansaStrategyCameraModel::Advance(SavedView, InvalidPitch, Settings, 0.1f).PitchDegrees, -79.0f);
        Settings.MinimumPitchDegrees = -90.0f;
        TestFalse(TEXT("Pitch settings cannot permit a vertical flip"), Settings.IsValid());
    }
    World->DestroyWorld(false);
    return !HasAnyErrors();
}
#endif
