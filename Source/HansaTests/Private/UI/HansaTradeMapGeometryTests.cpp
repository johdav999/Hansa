#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/HansaTradeMapGeometry.h"
#include "Algo/Reverse.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeGeographyGeometry,"Hansa.UI.TradeMap.GeographicGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaTradeGeographyGeometry::RunTest(const FString&)
{
 using namespace Hansa::UI::TradeGeometry;
 const auto London=Project(-.12574,51.50853),Novgorod=Project(31.27104,58.52131);
 TestTrue(TEXT("Western city is not clamped inward"),London.X<.04);
 TestTrue(TEXT("Eastern city is not clamped inward"),Novgorod.X>.96);
 TestFalse(TEXT("Missing location cannot become an origin marker"),IsLocated({-1,-1}));
 FCamera Camera;const FVector2D Size(800,500),Anchor(135,240),Center(.3,.6);
 const auto Old=Camera.Point(Center,Size);Camera.ZoomAt(2,Anchor,Size);
 TestTrue(TEXT("Wheel zoom preserves the geographic point under cursor"),(Camera.Point(Center,Size)-(Anchor+(Old-Anchor)*3)).Size()<.01);
 Camera.Move({50000,-50000},Size);Camera.Reveal(Center,Size);
 TestTrue(TEXT("Focus reveal restores offscreen city"),(Camera.Point(Center,Size)-Size*.5).Size()<.01);
 Camera.Reset();TestEqual(TEXT("Fit restores zoom"),Camera.Zoom,1.);
 TArray<FLabelInput> Inputs={ {TEXT("City.C"),{140,210},{90,30},0},{TEXT("City.A"),{120,200},{90,30},2},{TEXT("City.B"),{150,190},{90,30},1} };
 const auto Labels=PlaceLabels(Inputs,Size);Algo::Reverse(Inputs);const auto Reversed=PlaceLabels(Inputs,Size);
 TestEqual(TEXT("Label placement independent of discovery order"),Labels.Num(),Reversed.Num());
 for(int32 I=0;I<Labels.Num();++I)
 {
  TestEqual(TEXT("Deterministic label identity"),Labels[I].Id,Reversed[I].Id);
  TestTrue(TEXT("Deterministic label bounds"),Labels[I].Bounds==Reversed[I].Bounds);
  for(int32 J=I+1;J<Labels.Num();++J)TestFalse(TEXT("Labels never overlap"),FSlateRect::DoRectanglesIntersect(Labels[I].Bounds,Labels[J].Bounds));
 }
 TestTrue(TEXT("Selected city retains label priority"),!Labels.IsEmpty()&&Labels[0].Id==TEXT("City.A"));
 TArray<TPair<FName,FVector2D>> Points;Points.Emplace(FName(TEXT("A")),FVector2D(0,0));Points.Emplace(FName(TEXT("B")),FVector2D(100,10));Points.Emplace(FName(TEXT("C")),FVector2D(20,100));
 TestEqual(TEXT("Spatial right chooses eastern neighbor"),SpatialNeighbor(Points,TEXT("A"),{1,0}),FName(TEXT("B")));
 TestEqual(TEXT("Spatial down chooses southern neighbor"),SpatialNeighbor(Points,TEXT("A"),{0,1}),FName(TEXT("C")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeGeographyInput,"Hansa.UI.TradeMap.GeographicInput",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaTradeGeographyInput::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::UI;
 const auto Fixture=FHansaProductionFixture::TryCreateGrainShortage();if(!Fixture)return false;
 const auto Projection=Fixture.Value.BuildProjection();if(!Projection)return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();
 Model->ApplyProjection(Projection.Value,*Fixture.Value.GetDefinitions().GetEconomicRegistry());Model->Open();
 auto View=SNew(SHansaTradeMap).Model(Model.Get()).InitialViewportSize(FIntPoint(1280,720));
 const auto Cities=Model->GetSnapshot().Cities;
 for(const auto& City:Cities)
 {
  TestTrue(TEXT("Every fixture city is georeferenced"),TradeGeometry::IsLocated(City.NormalizedPosition));
  const FString Id=TEXT("TradeMap.City.")+City.StableId.ToString().Replace(TEXT("."),TEXT("_"));
  TestTrue(TEXT("Every city can receive semantic focus"),View->FocusSemanticId(Id));
  TestTrue(TEXT("Every city can be selected through normal intent"),View->ActivateSemanticId(Id));
  TestEqual(TEXT("Inspector selection follows map"),Model->GetSnapshot().SelectedCityStableId,City.StableId);
 }
 TestTrue(TEXT("Map overlay is actionable"),View->ActivateSemanticId(TEXT("TradeMap.Chart.Overlay")));
 const auto Nodes=View->GetSemanticSnapshot();const auto* Summary=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Chart.Summary");});
 TestTrue(TEXT("No knowledge source means unknown, not fabricated zero"),Summary&&Summary->State.Value.Contains(TEXT("unknown")));
 TestTrue(TEXT("Overlay thickness has accessible control"),View->ActivateSemanticId(TEXT("TradeMap.Chart.Thickness")));
 TestTrue(TEXT("Route selection has non-pointer control"),View->ActivateSemanticId(TEXT("TradeMap.Chart.NextRoute")));
 TestTrue(TEXT("Fit has non-drag control"),View->ActivateSemanticId(TEXT("TradeMap.Chart.Reset")));
 return !HasAnyErrors();
}
#endif
