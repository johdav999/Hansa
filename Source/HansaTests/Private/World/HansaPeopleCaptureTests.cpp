#if WITH_DEV_AUTOMATION_TESTS
#include "../UI/HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "World/HansaAmbientPeople.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "ShaderCompiler.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "World/HansaLubeckPlacementGrid.h"
namespace
{
class FPeopleCapture final : public IAutomationLatentCommand
{
public:
    explicit FPeopleCapture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started>180) { Test->AddError(TEXT("People capture timed out")); return true; }
        if (!GEngine || !GEngine->GameViewport) return false;
        auto* View=GEngine->GameViewport.Get(); auto* World=View->GetWorld(); auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Hud=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;
        auto* Mode=World?World->GetAuthGameMode<AHansaGameMode>():nullptr;
        auto* Host=Mode?Mode->GetSimulationHost():nullptr;
        auto* Camera=PC?Cast<AHansaStrategyCameraPawn>(PC->GetPawn()):nullptr;
        if (!Hud || !Host || !Camera || !Hud->GetFrontendPresentationModel()) return false;
        if (!Prepared)
        {
            // Build through the ordinary command gateway on an empty surveyed map.
            if (Host->BuildProjection().Value.GetPlacements().IsEmpty())
            {
                using namespace Hansa::Simulation;
                const auto Start=Hansa::Game::LubeckPlacementGrid::WorldToGrid(Hansa::Game::LubeckPlacementGrid::SurveyStartLocation());
                TArray<FHansaPlacementSpec> Plan;
                bool Found=false; TSet<FString> Failures;
                for (int32 DY=-80;DY<=80 && !Found;DY+=4) for (int32 DX=-80;DX<=80 && !Found;DX+=4)
                {
                    Plan.Reset(); int32 X=Start.X+DX, Y=Start.Y+DY;
                    bool Valid=true;
                    for (const TCHAR* Id:{TEXT("Building.Market"),TEXT("Building.Residence.Laborer"),TEXT("Building.Residence.Laborer"),TEXT("Building.Residence.Laborer"),TEXT("Building.Bakery")})
                    {
                        const auto* D=Host->FindBuildingDefinition(Id);
                        if (!D) { Valid=false; Failures.Add(FString(TEXT("Missing definition: "))+Id); break; }
                        FHansaPlacementSpec S; S.CityId=Host->GetCityId(); S.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(Id).Value; S.Anchor={X,Y+1};
                        const auto Validation=Host->ValidatePlacement(S); for (const auto& R:Validation.GetReasons()) { if (R.Failure!=EHansaPlacementFailure::RoadRequired) Valid=false; Failures.Add(S.BuildingDefinitionId.ToString()+TEXT(": ")+R.MessageKey.ToString()); } Plan.Add(S); X+=D->FootprintWidthCells+1;
                    }
                    for (int32 RX=Start.X+DX;RX<X;++RX)
                    {
                        FHansaPlacementSpec S; S.CityId=Host->GetCityId(); S.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value; S.Anchor={RX,Y};
                        const auto Validation=Host->ValidatePlacement(S); for (const auto& R:Validation.GetReasons()) { if (R.Failure!=EHansaPlacementFailure::RoadRequired) Valid=false; Failures.Add(S.BuildingDefinitionId.ToString()+TEXT(": ")+R.MessageKey.ToString()); } Plan.Add(S);
                    }
                    Found=Valid;
                }
                if (!Found) { for (const auto& F:Failures) Test->AddInfo(F); Test->AddError(TEXT("No valid empty-town placement rectangle")); return true; }
                Plan.StableSort([](const auto& A,const auto& B) { return A.BuildingDefinitionId.ToString()==TEXT("Building.Road") && B.BuildingDefinitionId.ToString()!=TEXT("Building.Road"); });
                const auto Placed=Host->PlaceBuildings(Plan);
                if (!Placed.IsSuccess()) { Test->AddError(FString::Printf(TEXT("Town placement rejected: %s at %d"),LexToString(Placed.GetError()),Placed.GetFailedCommandIndex())); return true; }
                if (!Host->AdvanceTicks(200)) { Test->AddError(TEXT("Construction simulation failed")); return true; } Host->SynchronizeWorldProjection();
            }
            Hud->GetFrontendPresentationModel()->SessionStarted();
            Hud->GetScenarioPresentationModel()->AcknowledgeBriefing(); Hud->GetScenarioPresentationModel()->DismissHelp();
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal); Host->SynchronizeWorldProjection();
            Camera->bEnableMouseEdgePan=false; Camera->MinimumZoomDistance=500; Camera->ClearCameraIntents();
            Camera->AddZoomIntent((Camera->GetZoomDistance()-1000)/Camera->ZoomUnitsPerStep);
            Prepared=true;
        }
        AHansaAmbientPeople* Manager=nullptr;
        for (TActorIterator<AHansaAmbientPeople> It(World);It;++It) { Manager=*It; break; }
        if (!Manager) return false;
        if (!Manager->ValidationError.IsEmpty()) { Test->AddError(Manager->ValidationError); return true; }
        const auto People=Manager->QueryPeople();
        const auto* Person=People.FindByPredicate([&](const auto& P){return P.bVisible && (FocusId==0 || P.PersonId==FocusId);});
        if (!Person) return false;
        FocusId=Person->PersonId;
        Camera->FocusWorldLocationIntent(Person->Location);
        Camera->SetActorLocation(FVector(Camera->GetActorLocation().X,Camera->GetActorLocation().Y,Person->Location.Z));
        if (Ready==0) Ready=FPlatformTime::Seconds()+6;
        if (FPlatformTime::Seconds()<Ready || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())) return false;
        if (Stage==1 && Person->Activity!=EHansaPersonActivity::Visiting) return false;
        TArray<FColor> Pixels; FIntVector Size;
        if (!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)) { Test->AddError(TEXT("People screenshot failed")); return true; }
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("AmbientPeople"); IFileManager::Get().MakeDirectory(*Directory,true);
        const FString Path=Directory/FString::Printf(TEXT("gameplay-%d-%dx%d"),Stage,Size.X,Size.Y);
        for (auto& Pixel:Pixels) Pixel.A=255;
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG); FFileHelper::SaveArrayToFile(PNG,*(Path+TEXT(".png")));
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("person=%d\nactivity=%d\nlocation=%s\ntarget=%d\nvisits=%d\n"),Person->PersonId,int32(Person->Activity),*Person->Location.ToString(),Manager->TargetPopulation,Manager->CompletedVisits),*(Path+TEXT(".txt")));
        Test->AddInfo(Path); ++Stage; Ready=FPlatformTime::Seconds()+4;
        return Stage==2;
    }
private:
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(),Ready=0;
    bool Prepared=false;
    int32 Stage=0,FocusId=0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPeopleCaptureTest,"Hansa.World.People.GameplayCapture",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHansaPeopleCaptureTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FPeopleCapture(this)); return true; }
#endif
