#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaHarborPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "HansaCargoVehicleReviewFixture.h"
#include "Definitions/HansaTradeDefinitions.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FHansaVehicleCaptureWait final : public IAutomationLatentCommand
{
public:
    FHansaVehicleCaptureWait(FAutomationTestBase* InTest, FString InPath) : Test(InTest), Path(MoveTemp(InPath)), Start(FPlatformTime::Seconds()) {}
    bool Update() override
    {
        if (!bRequested)
        {
            if(FPlatformTime::Seconds()-Start<2.0)return false;
            auto* Viewport=GEditor->GetActiveViewport();
            auto& Config=GetHighResScreenshotConfig();Config.SetResolution(1280,720,1);Config.FilenameOverride=Path;
            if(!Viewport || !Viewport->TakeHighResScreenShot()){Test->AddError(TEXT("Capture request failed"));return true;}
            Viewport->Draw();bRequested=true;return false;
        }
        TArray<uint8> Bytes;
        if (FFileHelper::LoadFileToArray(Bytes,*Path) && Bytes.Num()>24)
        {
            const auto Dim=[&Bytes](int32 I) { return uint32(Bytes[I])<<24 | uint32(Bytes[I+1])<<16 | uint32(Bytes[I+2])<<8 | Bytes[I+3]; };
            Test->TestEqual(TEXT("Native width"),Dim(16),uint32(1280));
            Test->TestEqual(TEXT("Native height"),Dim(20),uint32(720));
            Test->AddInfo(Path);return true;
        }
        if(FPlatformTime::Seconds()-Start>30){Test->AddError(TEXT("Vehicle capture timeout"));return true;}
        return false;
    }
private:
    FAutomationTestBase* Test; FString Path; double Start; bool bRequested=false;
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FHansaCargoVehicleCaptureTest,"Hansa.World.Vehicles.SimulationReviewCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FHansaCargoVehicleCaptureTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    for(const FString Name:{TEXT("Berth"),TEXT("Underway"),TEXT("RouteDistance"),TEXT("WagonDetail")}){Names.Add(Name);Commands.Add(Name);}
}
bool FHansaCargoVehicleCaptureTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Simulation;
    UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
    if (!World || World->GetOutermost()->GetName()!=TEXT("/Game/Hansa/Generated/Staging/Vehicles_P19/L_Vehicles_Review"))
    {AddError(TEXT("Open the isolated P19 review map first."));return false;}
    auto* Viewport=GEditor->GetActiveViewport(); if(!Viewport)return false;
    auto* CogDefinition=LoadObject<UHansaVehicleDefinition>(nullptr,TEXT("/Game/Hansa/Core/Vehicles/DA_Vehicle_Cog.DA_Vehicle_Cog"));
    auto* WagonDefinition=LoadObject<UHansaVehicleDefinition>(nullptr,TEXT("/Game/Hansa/Core/Vehicles/DA_Vehicle_Wagon.DA_Vehicle_Wagon"));
    UClass* CogClass=CogDefinition?CogDefinition->LoadPresentationActorClass():nullptr;
    UClass* WagonClass=WagonDefinition?WagonDefinition->LoadPresentationActorClass():nullptr;
    if(!TestNotNull(TEXT("Production Cog binding"),CogClass)||!TestNotNull(TEXT("Production wagon binding"),WagonClass))return false;
    for(TActorIterator<AHansaCargoVehiclePresentation> It(World);It;++It) It->Destroy();
    auto* Cog=World->SpawnActor<AHansaCargoVehiclePresentation>(CogClass);
    auto* Wagon=World->SpawnActor<AHansaCargoVehiclePresentation>(WagonClass);
    if(!Cog||!Wagon)return false;
    Cog->SetActorLabel(TEXT("P19_ActualVehicleProjection"));Wagon->SetActorLabel(TEXT("P19_ActualLocalJobProjection"));
    AHansaHarborPresentation* Harbor=nullptr;
    for(TActorIterator<AHansaHarborPresentation> It(World);It;++It){Harbor=*It;break;}
    if(!TestNotNull(TEXT("Review berth"),Harbor))return false;
    Harbor->OnConstruction(Harbor->GetActorTransform());
    Cog->SetActorTransform(Harbor->Berth->GetComponentTransform());
    Wagon->SetActorLocation(FVector(-450,0,225));
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if(!TestTrue(TEXT("Real scenario host"),Host->InitializeForLubeck(nullptr,Error))) {AddError(Error);return false;}
    bool bFoundCog=false,bFoundJob=false;
    FHansaLogisticsJobProjection FixtureJob;
    bFoundJob=Hansa::Editor::VehicleReview::FindInTransitJob(FixtureJob) && Wagon->ApplyLocalDelivery(FixtureJob);
    for(int32 Tick=0;Tick<250 && (!bFoundCog||!bFoundJob);++Tick)
    {
        const auto Snapshot=Host->BuildProjection();if(!Snapshot)return false;
        if(!bFoundCog) for(const auto& Vehicle:Snapshot.Value.GetVehicles())
        {
            if(Vehicle.DefinitionId.ToString()!=TEXT("Vehicle.Cog"))continue;
            const FHansaRouteProjection* Route=nullptr;
            for(const auto& Candidate:Snapshot.Value.GetRoutes())if(Candidate.VehicleId==Vehicle.Id){Route=&Candidate;break;}
            if(Parameters==TEXT("Underway") || Parameters==TEXT("RouteDistance"))
            {
                if(!Route)continue;
                if(Route->Lifecycle!=EHansaRouteLifecycleState::Traveling)
                {Host->SetRouteActive(Route->Id,true);continue;}
            }
            if(Cog->ApplyVehicle(Vehicle,Route)){bFoundCog=true;AddInfo(TEXT("Real vehicle definition ")+Vehicle.DefinitionId.ToString());break;}
        }
        if(!bFoundJob)for(const auto& Job:Snapshot.Value.GetLogisticsJobs())
        {
            if(Job.Status!=EHansaLogisticsJobStatus::InTransit)continue;
            if(Wagon->ApplyLocalDelivery(Job)){bFoundJob=true;AddInfo(TEXT("Real local cargo ")+Job.GoodId.ToString());break;}
        }
        if(!bFoundCog||!bFoundJob)Host->AdvanceTicks(1);
    }
    TestTrue(TEXT("Cog bound to real scenario entity"),bFoundCog);
    TestTrue(TEXT("Wagon bound to real in-transit logistics job"),bFoundJob);
    TestTrue(TEXT("Full hull clears pier"),Cog->GetActorLocation().X-Cog->Body->Bounds.BoxExtent.X>800);
    auto* Client=static_cast<FEditorViewportClient*>(Viewport->GetClient());
    const FRotator Rotation(-30,145,0);
    const bool bDetail=Parameters==TEXT("WagonDetail");
    const double Distance=bDetail?750.0:(Parameters==TEXT("RouteDistance")?12000.0:4400.0);
    Client->SetViewLocation((bDetail?FVector(-400,0,310):FVector(650,0,650))-Rotation.Vector()*Distance);
    Client->SetViewRotation(Rotation);Client->SetGameView(true);
    const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("GenerationJobs/hansa-vehicles_P19_20260908/renders/native-%s-%llu.png"),*Parameters,FPlatformTime::Cycles64()));
    ADD_LATENT_AUTOMATION_COMMAND(FHansaVehicleCaptureWait(this,Path));
    return !HasAnyErrors();
}
#endif
