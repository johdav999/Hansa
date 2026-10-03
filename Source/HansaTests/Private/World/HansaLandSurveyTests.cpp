#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/HansaLandSurveyView.h"
#include "World/HansaLandOverlayGeometry.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Network/HansaLandQueryTransport.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/StrongObjectPtr.h"
using namespace Hansa::Simulation;
using namespace Hansa::Game::LandOverlay;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandSurveyTests,"Hansa.World.LandOverlay.SurveyPagesAndRegions",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLandSurveyTests::RunTest(const FString&)
{
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
    if(!TestTrue(TEXT("Runtime ready"),Host->InitializeForLubeck(nullptr,Error)))return false;
    FSurveyView View;FHansaLandQueryResult First;
    for(int32 Page=0;Page<256;++Page)
    {
        auto R=Host->QueryLand(Host->GetHouseId(),Host->GetCityId(),{Page,0},{Page,0},true);
        if(Page==0)First=R;
        TestTrue(TEXT("Bounded run page"),R.Cells.Num()<=256 && R.SurveyPages>0);
        FHansaLandQueryReply Wire;Wire.bAccepted=true;Wire.Result=R;
        Wire.Request.RequestId=Page+1;Wire.Request.Slot=5;Wire.Request.City=FName(*Host->GetCityId().ToString());Wire.Request.Min={Page,0};Wire.Request.Max=Wire.Request.Min;
        TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);bool OK=false;
        if(!TestTrue(TEXT("Survey wire saves"),Wire.NetSerialize(Writer,nullptr,OK)&&OK))return false;
        FHansaLandQueryReply Decoded;FMemoryReader Reader(Bytes);
        if(!TestTrue(TEXT("Survey wire loads"),Decoded.NetSerialize(Reader,nullptr,OK)&&OK))return false;
        View.AcceptPage(Page,Decoded.Result);
        if(Page+1==R.SurveyPages)break;
        TestFalse(TEXT("Partial snapshot never publishes permission"),View.IsReady());
    }
    TestTrue(TEXT("Complete survey atomically publishes"),View.IsReady());
    const auto Exact=Host->QueryLand(Host->GetHouseId(),Host->GetCityId(),{0,0},{20,20});
    const auto Expanded=View.Extract({0,0},{20,20});
    TestEqual(TEXT("Expanded compact survey has exact cells"),Expanded.Cells.Num(),Exact.Cells.Num());
    for(int32 I=0;I<Exact.Cells.Num();++I)
    {
        const auto& A=Exact.Cells[I];const auto& B=Expanded.Cells[I];
        TestTrue(TEXT("Exact semantics survive runs"),A.Coordinate==B.Coordinate && A.RecordedOwnerId==B.RecordedOwnerId && A.Access==B.Access && A.bSurveyKnown==B.bSurveyKnown && A.OccupyingBuildingId==B.OccupyingBuildingId);
    }
    FSurveyView Paged;
    for(int32 Page=0;Page<2;++Page)
    {
        FHansaLandQueryReply Wire;Wire.bAccepted=true;Wire.Request.RequestId=Page+1;Wire.Request.Slot=uint8(5+Page);
        Wire.Request.City=FName(*First.CityId.ToString());Wire.Request.Min={Page,0};Wire.Request.Max=Wire.Request.Min;
        auto& R=Wire.Result;R.CityId=First.CityId;R.ViewerHouseId=First.ViewerHouseId;R.BoundsMin={0,0};R.BoundsMax={511,0};R.SurveyPages=2;R.StateRevision=123;
        for(int32 I=0;I<256;++I) { FHansaLandCellView C;C.Coordinate={Page*256+I,0};C.RegionId=1;C.bSurveyKnown=true;C.RecordedOwnerId=First.ViewerHouseId;C.Access=EHansaLandAccess::Permitted;R.Cells.Add(C); }
        TArray<uint8> Bytes;bool OK=false;FMemoryWriter Writer(Bytes);
        TestTrue(TEXT("Each compact page channel serializes"),Wire.NetSerialize(Writer,nullptr,OK)&&OK);
        FHansaLandQueryReply Decoded;FMemoryReader Reader(Bytes);
        if(!TestTrue(TEXT("Each compact page channel decodes"),Decoded.NetSerialize(Reader,nullptr,OK)&&OK))return false;
        Paged.AcceptPage(Page,Decoded.Result);
        TestEqual(TEXT("Two-page snapshot publishes only after its final page"),Paged.IsReady(),Page==1);
    }
    TestEqual(TEXT("Both pages retained"),Paged.Get().Cells.Num(),512);
    TestEqual(TEXT("Territory crosses page boundaries"),Paged.RegionAt({511,0}),Paged.RegionAt({0,0}));
    auto Changed=First;Changed.StateRevision++;Changed.SurveyPages=2;
    View.AcceptPage(0,Changed);TestFalse(TEXT("Changed permissions clear prior snapshot immediately"),View.IsReady());
    View.AcceptPage(1,First);TestFalse(TEXT("Mixed revisions cannot publish"),View.IsReady());
    TestEqual(TEXT("Mixed revisions restart transfer"),View.NextPage(),0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandSurveyGeometryTests,"Hansa.World.LandOverlay.SurveyCoverageAndContours",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaLandSurveyGeometryTests::RunTest(const FString&)
{
    FHansaLandQueryResult Q;Q.SurveyPages=1;Q.StateRevision=1;Q.BoundsMin={0,0};Q.BoundsMax={5,2};
    auto Run=[&](int32 X,int32 Y,int32 Length,int32 Region)
    {FHansaLandCellView C;C.Coordinate={X,Y};C.RunLength=Length;C.RegionId=Region;C.bSurveyKnown=true;C.Access=EHansaLandAccess::Permitted;C.RecordedOwnerId=FHansaHouseId::TryCreate(1).Value;Q.Cells.Add(C);};
    Run(0,0,3,1);Run(1,0,1,1);Run(1,2,1,1);Run(2,0,3,1);Run(5,0,2,2);
    FSurveyView V;V.AcceptPage(0,Q);
    TestEqual(TEXT("Disconnected holding has separate identity"),V.RegionAt({5,1}),2);
    TestEqual(TEXT("Unsupplied hole has no selected region"),V.RegionAt({1,1}),0);
    TestTrue(TEXT("Frame bounds cover complete selected territory only"),V.RegionBounds(1).Max.Equals(FVector2D(3,3)));
    const auto Contours=SurveySelectionBoundaries(Q.Cells,1);
    TestEqual(TEXT("Selected territory preserves hole"),Contours.Num(),2);
    for(const auto& C:Contours)TestTrue(TEXT("Territory outline is closed"),C.bClosed);
    TestTrue(TEXT("Chunk padding outside authored survey remains invisible"),Classify(V.Extract({6,0},{6,0}).Cells[0],EMode::Buildable).Surface==ESurface::Hidden);
    const auto Coarse=V.Extract({0,0},{1,1},2);
    TestFalse(TEXT("Mixed distant summary cannot claim permission"),Coarse.Cells[0].bSurveyKnown);
    const FVector Points[]={{-120,-70,0},{160,-70,0},{160,110,0},{-120,110,0}};
    const auto Chunks=VisibleSurveyChunks(Points,FTransform::Identity,{-1000,-1000},{1000,1000},{0,0});
    TestTrue(TEXT("Camera extent exceeds original four chunks"),Chunks.Num()>4);
    TestTrue(TEXT("Negative coordinates use floor alignment"),Chunks.Contains(FIntPoint(-4,-3)));
    TestTrue(TEXT("Far visible edge included"),Chunks.Contains(FIntPoint(5,3)));
    const FVector2D World(437,982),Center(100,200);
    TestTrue(TEXT("Minimap transform round trips"),LandMapToWorld(WorldToLandMap(World,Center,1700),Center,1700).Equals(World,1.e-6));
    return !HasAnyErrors();
}
#endif
