#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Network/HansaLandQueryTransport.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaLandOverlayRenderer.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaTerrainPlacement.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaLandOverlay.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

using namespace Hansa::Simulation;
namespace
{
FHansaLandQueryReply ReplyFor(const FHansaLandQueryRequest& Q, uint64 Owner=1)
{
    FHansaLandQueryReply Reply; Reply.Request=Q; Reply.bAccepted=true;
    auto& R=Reply.Result;
    R.CityId=FHansaCityDefinitionId::TryParse(Q.City.ToString()).Value;
    R.ViewerHouseId=FHansaHouseId::TryCreate(Owner).Value;
    R.BoundsMin={Q.Min.X,Q.Min.Y}; R.BoundsMax={Q.Max.X,Q.Max.Y}; R.StateRevision=42;
    for(int64 X=Q.Min.X;X<=Q.Max.X;++X) for(int64 Y=Q.Min.Y;Y<=Q.Max.Y;++Y)
    {
        FHansaLandCellView C; C.Coordinate={int32(X),int32(Y)};
        C.RecordedOwnerId=FHansaHouseId::TryCreate(8).Value;
        C.bSurveyKnown=true; C.Access=EHansaLandAccess::Permitted; C.Reason=EHansaLandAccessReason::StartingCity;
        R.Cells.Add(C);
    }
    return Reply;
}
bool RoundTrip(FHansaLandQueryReply Source, FHansaLandQueryReply& Target, TArray<uint8>& Bytes)
{
    bool Ok=false; FMemoryWriter Writer(Bytes,true);
    if(!Source.NetSerialize(Writer,nullptr,Ok)||!Ok)return false;
    FMemoryReader Reader(Bytes,true);
    return Target.NetSerialize(Reader,nullptr,Ok)&&Ok&&Reader.Tell()==Bytes.Num();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandWireTest,"Hansa.Multiplayer.Land.WireBoundsAndIdentity",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLandWireTest::RunTest(const FString&)
{
    FHansaLandQueryRequest Q;Q.RequestId=1;Q.City=TEXT("City.Lubeck");Q.Min={-16,-16};Q.Max={15,15};
    auto Sent=ReplyFor(Q,2);
    Sent.Result.Cells[0].OccupyingBuildingId=FHansaBuildingId::TryCreate(123,7).Value;
    FHansaLandLeaseView Lease;Lease.Id=FHansaLeasedPlotId::TryCreate(19,2).Value;
    Lease.BoundsMin={-16,-16};Lease.BoundsMax={-10,-10};Lease.bActive=true;
    Lease.PermittedBuildingCategories={TEXT("Storage"),TEXT("Harbor")};Sent.Result.ViewerLeases.Add(Lease);
    Sent.Result.Cells[0].ViewerLeaseId=Lease.Id;
    Sent.Result.Cells.Last()={};Sent.Result.Cells.Last().Coordinate={15,15};
    FHansaLandQueryReply Received;TArray<uint8> Bytes;
    TestTrue(TEXT("Maximum bounded chunk round-trips"),RoundTrip(Sent,Received,Bytes));
    TestEqual(TEXT("Wire includes exactly requested cells"),Received.Result.Cells.Num(),1024);
    TestTrue(TEXT("Compressed chunk stays within RPC cap"),Bytes.Num()<60*1024);
    TestEqual(TEXT("Viewer identity survives"),Received.Result.ViewerHouseId.GetValue(),uint64(2));
    if(Received.Result.Cells.Num()==1024)
    {
        TestEqual(TEXT("Occupant generation survives"),Received.Result.Cells[0].OccupyingBuildingId.GetGeneration(),uint32(7));
        TestTrue(TEXT("Viewer lease identity and allowed categories survive"),Received.Result.ViewerLeases.Num()==1&&
            Received.Result.ViewerLeases[0].Id==Lease.Id&&Received.Result.ViewerLeases[0].PermittedBuildingCategories==Lease.PermittedBuildingCategories&&Received.Result.ViewerLeases[0].bActive);
        TestFalse(TEXT("Unknown survey remains unknown"),Received.Result.Cells.Last().bSurveyKnown);
        TestTrue(TEXT("Unknown rights never become permitted"),Received.Result.Cells.Last().Access==EHansaLandAccess::Unavailable);
    }
    auto Bad=Q;Bad.Max.X=16;TestFalse(TEXT("Oversized request rejected"),Bad.IsValid());
    Bad=Q;Bad.Slot=9;TestFalse(TEXT("Tenth cache slot rejected"),Bad.IsValid());
    Bad=Q;Bad.Min.X=MIN_int32;Bad.Max.X=MAX_int32;TestFalse(TEXT("Overflow bounds rejected"),Bad.IsValid());
    Bad=Q;Bad.City=TEXT("House.1");TestFalse(TEXT("Wrong stable-ID domain rejected"),Bad.IsValid());
    auto Truncated=Bytes;Truncated.SetNum(Truncated.Num()-1);FMemoryReader Reader(Truncated,true);bool Ok=true;
    TestFalse(TEXT("Truncated payload rejected"),Received.NetSerialize(Reader,nullptr,Ok));
    TestFalse(TEXT("Failed payload has no cached facts"),Received.bAccepted);
    TestTrue(TEXT("Failed payload clears cells"),Received.Result.Cells.IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandCacheTest,"Hansa.Multiplayer.Land.CacheCorrelationAndExpiry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLandCacheTest::RunTest(const FString&)
{
    FHansaLandQueryClientCache Cache;FHansaLandQueryRequest Q,Coalesced;FHansaLandQueryResult Result;
    TestTrue(TEXT("New area is pending"),Cache.Poll(0,TEXT("City.Lubeck"),{0,0},{1,1},1,10,Q,Result)==EHansaLandViewStatus::Pending);
    TestTrue(TEXT("Initial request emitted"),Q.RequestId>0);
    Cache.Poll(0,Q.City,Q.Min,Q.Max,1,10.1,Coalesced,Result);
    TestEqual(TEXT("Outstanding request coalesces repeated HUD refreshes"),Coalesced.RequestId,int64(0));
    TestFalse(TEXT("Reply cannot substitute another viewer"),Cache.Receive(ReplyFor(Q,2),1,10.2));
    TestTrue(TEXT("Matching reply accepted"),Cache.Receive(ReplyFor(Q),1,10.2));
    TestTrue(TEXT("Accepted data becomes ready"),Cache.Poll(0,Q.City,Q.Min,Q.Max,1,10.3,Coalesced,Result)==EHansaLandViewStatus::Ready);
    FHansaLandQueryRequest Moved;
    Cache.Poll(0,Q.City,{2,2},{3,3},1,10.4,Moved,Result);
    TestFalse(TEXT("Old bounds cannot overwrite the new target"),Cache.Receive(ReplyFor(Q),1,10.45));
    TestTrue(TEXT("Moved target accepted"),Cache.Receive(ReplyFor(Moved),1,10.5));
    TestTrue(TEXT("Expired data becomes unavailable while refreshing"),Cache.Poll(0,Moved.City,Moved.Min,Moved.Max,1,12.1,Q,Result)==EHansaLandViewStatus::Pending);
    TestTrue(TEXT("Expired view has no stale permission cells"),Result.Cells.IsEmpty());
    Cache.Invalidate();
    TestFalse(TEXT("Save/session invalidation discards in-flight replies"),Cache.Receive(ReplyFor(Q),1,12.2));
    Cache.Poll(0,TEXT("City.Rostock"),{0,0},{0,0},1,13,Moved,Result);
    auto OlderRevision=ReplyFor(Moved);OlderRevision.Result.StateRevision=1;
    TestTrue(TEXT("Restored saves may have lower revision tokens"),Cache.Receive(OlderRevision,1,13.1));
    Cache.Poll(0,Moved.City,Moved.Min,Moved.Max,2,14,Q,Result);
    TestFalse(TEXT("Changing admitted house discards old private facts"),Cache.Receive(OlderRevision,2,14.1));
    FHansaLandQueryBudget Budget;
    for(int32 I=0;I<8;++I)TestTrue(TEXT("Small burst admitted"),Budget.Consume(1));
    TestFalse(TEXT("Flood is bounded"),Budget.Consume(1));
    TestTrue(TEXT("Budget refills"),Budget.Consume(1.1));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandPrivacyTest,"Hansa.Multiplayer.Land.AuthorityScope",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLandPrivacyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
    if(!TestTrue(TEXT("Runtime ready"),Host->InitializeForLubeck(nullptr,Error)))return false;
    Hansa::Multiplayer::FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);
    FHansaClientInterest Interest;Interest.CityIds={TEXT("City.Lubeck"),TEXT("City.Rostock")};
    TestTrue(TEXT("Viewer one admitted"),Authority.RegisterAdmittedClient({101,FHansaParticipantId::TryCreate(101).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error));
    TestTrue(TEXT("Viewer two admitted"),Authority.RegisterAdmittedClient({202,FHansaParticipantId::TryCreate(202).Value,Host->GetRivalHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error));
    FHansaLandQueryResult A,B;const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    TestTrue(TEXT("First admitted viewer can query"),Authority.QueryLand(101,City,{0,0},{1,1},A,Error));
    TestTrue(TEXT("Second admitted viewer can query"),Authority.QueryLand(202,City,{0,0},{1,1},B,Error));
    TestTrue(TEXT("Connection fixes viewer identity"),A.ViewerHouseId==Host->GetHouseId()&&B.ViewerHouseId==Host->GetRivalHouseId());
    TestFalse(TEXT("Unknown principal rejected"),Authority.QueryLand(303,City,{0,0},{1,1},B,Error));
    TestTrue(TEXT("Unauthorized query leaves no facts"),B.Cells.IsEmpty()&&B.ViewerLeases.IsEmpty());
    TestTrue(TEXT("Admitted compact survey is viewer scoped"),Authority.QueryLand(101,City,{0,0},{0,0},A,Error,true) && A.ViewerHouseId==Host->GetHouseId() && A.Cells.Num()<=256);
    TestFalse(TEXT("Unadmitted compact survey denied"),Authority.QueryLand(303,City,{0,0},{0,0},A,Error,true));
    Interest.CityIds={TEXT("City.Rostock")};Authority.SetClientInterest(101,Interest,Error);
    TestFalse(TEXT("Interest revocation enforced"),Authority.QueryLand(101,City,{0,0},{1,1},A,Error));
    TestTrue(TEXT("Revoked response is empty"),A.Cells.IsEmpty()&&A.ViewerLeases.IsEmpty());
    TestFalse(TEXT("Compact survey also obeys interest revocation"),Authority.QueryLand(101,City,{0,0},{0,0},A,Error,true));
    return !HasAnyErrors();
}

namespace
{
class FLandLiveClient final : public IAutomationLatentCommand
{
public:
    explicit FLandLiveClient(FAutomationTestBase* In):Test(In),Started(FPlatformTime::Seconds()){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>90){Test->AddError(FString::Printf(TEXT("Live land RPC timed out at stage %d: %s"),Stage,*WaitingFor));return true;}
        auto* World=GEngine&&GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
        auto* PC=World?Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController()):nullptr;
        if(!PC||World->GetNetMode()!=NM_Client||PC->GetClientProjection().OwnerHouseId<=0)return false;
        FHansaLandQueryResult Result;
        if(Stage==0)
        {
            const auto Status=PC->QueryLandForView(0,TEXT("City.Lubeck"),{0,0},{25,25},Result);
            if(Status!=EHansaLandViewStatus::Ready)return false;
            Test->TestNull(TEXT("Remote land view has no authoritative game mode"),World->GetAuthGameMode());
            Test->TestEqual(TEXT("RPC binds to this player's admitted house"),int64(Result.ViewerHouseId.GetValue()),PC->GetClientProjection().OwnerHouseId);
            Test->TestEqual(TEXT("RPC returns bounded requested cells"),Result.Cells.Num(),676);
            if(const auto* C=Result.Cells.FindByPredicate([](const auto& V){return V.Coordinate.X==22&&V.Coordinate.Y==4;}))ExpectedCell=*C;
            Test->AddInfo(FString::Printf(TEXT("Land RPC viewer=%lld cells=%d"),PC->GetClientProjection().OwnerHouseId,Result.Cells.Num()));
            Stage=1;
        }
        if(Stage==1)
        {
            const auto Status=PC->QueryLandForView(1,TEXT("City.Rostock"),{0,0},{0,0},Result);
            if(Status==EHansaLandViewStatus::Pending)return false;
            Test->TestTrue(TEXT("Foreign city outside interest is unavailable"),Status==EHansaLandViewStatus::Unavailable&&Result.Cells.IsEmpty());
            FHansaClientInterest Interest;Interest.CityIds={TEXT("City.Lubeck"),TEXT("City.Rostock")};PC->ServerSetHansaInterest(Interest);
            PC->InvalidateLandQueries();Stage=2;
        }
        if(Stage==2)
        {
            if(PC->QueryLandForView(1,TEXT("City.Rostock"),{0,0},{0,0},Result)!=EHansaLandViewStatus::Ready)return false;
            Test->TestEqual(TEXT("Foreign city response is still bound to this viewer"),int64(Result.ViewerHouseId.GetValue()),PC->GetClientProjection().OwnerHouseId);
            Test->TestEqual(TEXT("Subscribed foreign city query is exact"),Result.Cells.Num(),1);
            Stage=3;
        }
        if(Stage==3)
        {
            auto* Hud=Cast<AHansaRootHud>(PC->GetHUD());auto Root=Hud?Hud->GetRootWidget():nullptr;
            auto State=Root?Root->GetLandState():nullptr;
            if(!State){WaitingFor=TEXT("Land HUD state");return false;}
            if(!bFramed)
            {
                auto* Camera=Cast<AHansaStrategyCameraPawn>(PC->GetPawn());
                if(!Camera){WaitingFor=TEXT("Camera pawn");return false;}
                for(TActorIterator<AHansaLubeckWorldFoundation> It(World);It;++It)
                {
                    FTransform Grid;
                    if(!AHansaLandOverlayRenderer::MakeGridTransform(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value,**It,Grid))continue;
                    Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();
                    Camera->FocusWorldLocationIntent(Grid.TransformPosition(FVector(22.5,4.5,0)));
                    bFramed=true;break;
                }
                if(!bFramed){WaitingFor=TEXT("Replicated city foundation");return false;}
            }
            if(State->GetMode()!=Hansa::Game::LandOverlay::EMode::Buildable)State->SetMode(Hansa::Game::LandOverlay::EMode::Buildable);
            State->Update();
            if(!State->IsAvailable()){WaitingFor=State->GetAvailabilityText().ToString();return false;}
            bool Rendered=false;
            for(TActorIterator<AHansaLandOverlayRenderer> It(World);It;++It)Rendered|=It->IsDisplayingCity(TEXT("City.Lubeck"));
            if(!Rendered){WaitingFor=TEXT("Terrain-conforming land mesh");return false;}
            Test->TestTrue(TEXT("Remote HUD renders the server's bounded land survey"),Rendered);
            for(TActorIterator<AHansaLubeckWorldFoundation> It(World);It;++It)
            {
                FTransform Grid;
                if(!AHansaLandOverlayRenderer::MakeGridTransform(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value,**It,Grid))continue;
                const FVector XY=Grid.TransformPosition(FVector(22.5,4.5,0));FHitResult Hit;
                if(Hansa::Game::TerrainPlacement::Trace(World,XY+FVector(0,0,1000000),XY-FVector(0,0,1000000),Hit))
                    Test->TestTrue(TEXT("Remote terrain can be selected"),State->SelectWorldHit(Hit));
                else Test->AddError(TEXT("Remote terrain selection trace failed"));
                Stage=4;break;
            }
        }
        if(Stage==4)
        {
            auto* Hud=Cast<AHansaRootHud>(PC->GetHUD());auto State=Hud->GetRootWidget()->GetLandState();State->Update();
            if(!State->HasSelection()||!State->GetSelectedView().bSurveyKnown){WaitingFor=TEXT("Selected cell survey");return false;}
            Test->TestTrue(TEXT("Remote inspector retains authoritative owner and permission"),
                State->GetSelectedView().RecordedOwnerId==ExpectedCell.RecordedOwnerId&&State->GetSelectedView().Access==ExpectedCell.Access);
            Test->TestTrue(TEXT("Remote selection resolves complete territory"),State->GetSelectedRegion()>0 && State->GetSurvey().RegionBounds(State->GetSelectedRegion()).GetArea()>1);
            State->SetMode(Hansa::Game::LandOverlay::EMode::Off);FinishedAt=Now;Stage=5;
        }
        return Stage==5&&Now-FinishedAt>=5;
    }
private:
    FAutomationTestBase* Test;double Started,FinishedAt=0;int32 Stage=0;FHansaLandCellView ExpectedCell;
    bool bFramed=false;FString WaitingFor;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandLiveClientTest,"Hansa.Multiplayer.Land.LiveClient",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHansaLandLiveClientTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FLandLiveClient(this));return true;}
#endif
