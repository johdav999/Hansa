#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSurveyOpeningTest,
	"Hansa.Integration.RuntimeSimulationHost.SurveyWaterfrontOpening",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSurveyOpeningTest::RunTest(const FString&)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Game::LubeckPlacementGrid;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("SurveyAcceptance")),
		CreatePackage(TEXT("/Temp/Lubeck_Terrain_Preview_Acceptance")));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	TestTrue(TEXT("Survey map profile is recognized"),IsSurveyWorld(World));
	auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	TestTrue(TEXT("Camera begins at the surveyed bank"), Foundation->GetAutomationStartTransform().GetLocation().Equals(SurveyStartLocation()));
	TestTrue(TEXT("Camera can reach the full survey"),Foundation->GetCameraBoundsMax().X-Foundation->GetCameraBoundsMin().X > 400000);
	int32 PointerX=0, PointerY=0;
	TestTrue(TEXT("Player pointer can target the waterfront start"),
		Foundation->WorldToPlacementCell(SurveyStartLocation(),PointerX,PointerY));
	for (const FHansaGridCoordinate Cell : {
		SurveyFisheryAnchor(), FHansaGridCoordinate{SurveyMinX,SurveyMinY},
		FHansaGridCoordinate{SurveyMaxX,SurveyMinY}, FHansaGridCoordinate{SurveyMinX,SurveyMaxY},
		FHansaGridCoordinate{SurveyMaxX,SurveyMaxY}})
	{
		TestTrue(TEXT("Player pointer accepts surveyed cells outside the old prototype"),
			Foundation->WorldToPlacementCell(Foundation->PlacementCellToWorld(Cell.X,Cell.Y),PointerX,PointerY));
		TestEqual(TEXT("Pointer round trip preserves X"),PointerX,Cell.X);
		TestEqual(TEXT("Pointer round trip preserves Y"),PointerY,Cell.Y);
	}
	TestFalse(TEXT("Player pointer still rejects locations outside the survey"),
		Foundation->WorldToPlacementCell(GridToWorld({SurveyMaxX+1,SurveyMaxY}),PointerX,PointerY));
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(*Error, Host->InitializeForLubeck(World,Error) && Host->StartNewGame(Error))) return false;
	Host->SetMerchantAIEnabled(false);
	const auto Opening = Host->BuildProjection();
	if (!Opening) return false;
	int64 Planks=0;
	int64 Timber=0;
	int64 Tools=0;
	for (const auto& Inventory : Opening.Value.GetInventories())
		if (Inventory.CityId==Host->GetCityId() && Inventory.OwnerKind==EHansaInventoryOwnerKind::City)
			for (const auto& Stock : Inventory.Stocks)
			{
				if (Stock.GoodId.ToString()==TEXT("Good.Planks")) Planks+=Stock.Available.GetRawValue();
				if (Stock.GoodId.ToString()==TEXT("Good.Timber")) Timber+=Stock.Available.GetRawValue();
				if (Stock.GoodId.ToString()==TEXT("Good.Tools")) Tools+=Stock.Available.GetRawValue();
			}
	TestEqual(TEXT("New Game adds 1,000 construction-test planks to the 112-plank survey opening"),
		Planks,int64(1'112'000));
	TestEqual(TEXT("New Game adds 100 construction-test timber to the 34-timber authored opening"),
		Timber,int64(134'000));
	TestEqual(TEXT("New Game adds 1,000 construction-test tools to the 18-tool authored opening"),
		Tools,int64(1'018'000));
	const auto Anchor=SurveyFisheryAnchor();
	TArray<FHansaPlacementSpec> Specs;
	const auto Add=[&](const TCHAR* Id,int32 X,int32 Y)
	{
		FHansaPlacementSpec Spec; Spec.CityId=Host->GetCityId();
		Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(Id).Value;
		int32 TargetX=0,TargetY=0;
		TestTrue(TEXT("Construction targets pass the player's world-to-cell gate"),
			Foundation->WorldToPlacementCell(Foundation->PlacementCellToWorld(X,Y),TargetX,TargetY));
		Spec.Anchor={TargetX,TargetY}; Specs.Add(Spec);
	};
	for(int32 X=Anchor.X-10; X<Anchor.X; ++X) Add(TEXT("Building.Road"),X,Anchor.Y-1);
	Add(TEXT("Building.Road"),Anchor.X-1,Anchor.Y);
	Add(TEXT("Building.Residence.Laborer"),Anchor.X-10,Anchor.Y);
	Add(TEXT("Building.Residence.Laborer"),Anchor.X-8,Anchor.Y);
	Add(TEXT("Building.Market"),Anchor.X-6,Anchor.Y);
	Add(TEXT("Building.Fishery"),Anchor.X,Anchor.Y);
	// Opposite survey corners, kilometres beyond the old prototype restriction.
	Add(TEXT("Building.Road"),SurveyMinX+5,SurveyMinY+5);
	Add(TEXT("Building.Road"),SurveyMaxX-5,SurveyMinY+5);
	Add(TEXT("Building.Road"),SurveyMinX+5,SurveyMaxY-5);
	const auto Placed=Host->PlaceBuildings(Specs);
	if (!Placed) AddError(FString::Printf(TEXT("Placement gateway: %s; terrain: %s"),LexToString(Placed.GetError()),
		Placed.GetPlacementValidation().IsSet() ? LexToString(Placed.GetPlacementValidation()->GetPrimaryFailure()) : TEXT("n/a")));
	if (!TestTrue(TEXT("Shore chain and remote survey plots accept normal paid construction"),Placed.IsSuccess())) return false;
	FHansaPlacementSpec Invalid = Specs[0];
	TestEqual(TEXT("Occupied roads still reject a second building"),
		Host->ValidatePlacement(Invalid).GetPrimaryFailure(),EHansaPlacementFailure::Occupied);
	Invalid.Anchor={SurveyMaxX+1,SurveyMaxY};
	TestEqual(TEXT("Survey boundary is still enforced"),
		Host->ValidatePlacement(Invalid).GetPrimaryFailure(),EHansaPlacementFailure::OutsideBounds);
	const auto* Map=Host->FindPlacementMap();
	if (!TestNotNull(TEXT("Survey placement map exists"),Map)) return false;
	TestEqual(TEXT("The immutable topology contains the complete surveyed grid"), Map->Cells.Num(), 1'014'049);
	const auto* Water=Map->Cells.FindByPredicate([&](const auto& Cell)
		{return Cell.Coordinate.X==Anchor.X+3 && Cell.Coordinate.Y>=Anchor.Y &&
			Cell.Coordinate.Y<Anchor.Y+2 && Cell.Terrain==EHansaPlacementTerrain::Water;});
	if (!TestNotNull(TEXT("The founding fishery faces mapped water"),Water)) return false;
	Invalid.Anchor=Water->Coordinate;
	TestEqual(TEXT("Ordinary construction cannot cover the river"),
		Host->ValidatePlacement(Invalid).GetPrimaryFailure(),EHansaPlacementFailure::TerrainNotBuildable);
	const double Start=FPlatformTime::Seconds();
	TestTrue(TEXT("Survey economy advances through construction and fishing"),Host->AdvanceTicks(300));
	const double TickSeconds=FPlatformTime::Seconds()-Start;
	AddInfo(FString::Printf(TEXT("Survey grid: %d cells, 300 ticks %.3f seconds"),
		(SurveyMaxX-SurveyMinX+1)*(SurveyMaxY-SurveyMinY+1), TickSeconds));
	TestTrue(TEXT("Three hundred Development ticks stay within the full-survey CPU budget"), TickSeconds < 2.0);
	TestTrue(TEXT("Tick transactions retain the same immutable topology allocation"), Host->FindPlacementMap() == Map);
	const auto Running=Host->BuildProjection();
	if (!Running) return false;
	bool bProduced=false;
	for(const auto& Production : Running.Value.GetProductions())
		if(Production.RecipeId.ToString()==TEXT("Recipe.CatchFish")) bProduced=Production.CompletedCycles>0;
	TestTrue(TEXT("Waterfront fishery actually completes a batch"),bProduced);
	for (const auto& Home : Running.Value.GetPopulationCohorts())
		TestTrue(TEXT("Waterfront homes receive market services"),Home.bHasMarketAccess && Home.Residents>=2);
	TArray<uint8> Bytes;
	const auto Saved=Host->CaptureSaveBytes(Bytes,TEXT("Survey"),TEXT("2026-09-12T00:00:00Z"));
	if (!TestTrue(TEXT("Full survey saves within archive limits"),Saved.IsSuccess())) return false;
	AddInfo(FString::Printf(TEXT("Sparse full-survey save: %d bytes"), Bytes.Num()));
	TestTrue(TEXT("Format 7 does not serialize the million immutable cells"), Bytes.Num() < 1024 * 1024);
	const auto Restored=Host->RestoreSaveBytes(Bytes);
	TestTrue(TEXT("Full survey restores"),Restored.IsSuccess());
	TestEqual(TEXT("Survey save/load preserves full authoritative state"),Restored.AuthoritativeHash,Saved.AuthoritativeHash);
	return !HasAnyErrors();
}
#endif
