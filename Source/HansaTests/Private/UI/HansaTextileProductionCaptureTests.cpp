#if WITH_DEV_AUTOMATION_TESTS

#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

namespace
{
class FHansaTextileProductionCapture final : public IAutomationLatentCommand
{
public:
	explicit FHansaTextileProductionCapture(FAutomationTestBase* InTest)
		: Test(InTest), Started(FPlatformTime::Seconds()) {}

	bool Update() override
	{
		using namespace Hansa::Simulation;
		if (FPlatformTime::Seconds() - Started > 240)
		{
			Test->AddError(TEXT("Textile production playable review timed out"));
			return true;
		}
		if (!FParse::Param(FCommandLine::Get(), TEXT("TextileProductionCandidate")) && !FParse::Param(FCommandLine::Get(), TEXT("TextileProductionApprovedReview")))
		{
			Test->AddError(TEXT("Run this review with -TextileProductionCandidate."));
			return true;
		}
		auto* Viewport = GEngine ? GEngine->GameViewport.Get() : nullptr;
		UWorld* World = Viewport ? Viewport->GetWorld() : nullptr;
		if (!World || !World->HasBegunPlay()) return false;
		auto* Controller = Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController());
		auto* Hud = Controller ? Cast<AHansaRootHud>(Controller->GetHUD()) : nullptr;
		auto* GameMode = World->GetAuthGameMode<AHansaGameMode>();
		auto* Host = GameMode ? GameMode->GetSimulationHost() : nullptr;
		if (!Hud || !Host || !Hud->GetRootWidget().IsValid() || HansaWaitForFrontend(Hud)) return false;
		const auto Root = Hud->GetRootWidget();
        if (Initialized && Root->ActivateSemanticId(TEXT("Scenario.Close"))) Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);

		if (!Initialized)
		{
			FParse::Value(FCommandLine::Get(), TEXT("TextileUIScale="), UiScale);
            bLargeText = FParse::Param(FCommandLine::Get(), TEXT("TextileLargeText"));
            Root->SetPreferences({false, true, bLargeText, UiScale});
			Hud->GetScenarioPresentationModel()->Close();
			Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
			if (auto* Camera = Cast<AHansaStrategyCameraPawn>(Controller->GetPawn()))
			{
				Camera->bEnableMouseEdgePan = false;
				Camera->ClearCameraIntents();
			}
			Initialized = true;
		}

		if (Stage < 3)
		{
			const TCHAR* Chains[] = {TEXT("BuildMenu.Chain.Good_LinenClothing"), TEXT("BuildMenu.Chain.Good_Candles"), TEXT("BuildMenu.Chain.Good_Rope")};
			const TCHAR* Goods[] = {TEXT("Good.LinenClothing"), TEXT("Good.Candles"), TEXT("Good.Rope")};
			if (!Prepared)
			{
				Test->TestTrue(TEXT("Open native Production category"), Root->ActivateSemanticId(TEXT("BuildMenu.Category.Production")));
				Test->TestTrue(TEXT("Open native Craftsmen tier"), Root->ActivateSemanticId(TEXT("BuildMenu.Tier.Craftsmen")));
				Test->TestTrue(TEXT("Open textile end-product chain"), Root->ActivateSemanticId(Chains[Stage]));
				Test->TestTrue(TEXT("End-product selector receives controller focus"), Root->FocusSemanticId(Chains[Stage]));
				const auto* Build = Hud->GetBuildMenuPresentationModel();
				Test->TestTrue(TEXT("Selected textile chain is visible to Craftsmen"), Build->IsChainVisible(FName(Goods[Stage])));
				Prepared = true;
				ReadyFrame = GFrameCounter + 8;
				return false;
			}
			if (GFrameCounter < ReadyFrame) return false;
            if (!CardFocused)
            {
                Capture(Viewport, Root, FString::Printf(TEXT("tray-stage%d"), Stage));
                const TCHAR* Cards[] = {TEXT("BuildMenu.Card.Building_Tailor"), TEXT("BuildMenu.Card.Building_Chandler"), TEXT("BuildMenu.Card.Building_Ropewalk")};
                Test->TestTrue(TEXT("Workshop card receives keyboard/controller focus"), Root->FocusSemanticId(Cards[Stage]));
                CardFocused = true;
                ReadyFrame = GFrameCounter + 8;
                return false;
            }
            Capture(Viewport, Root, FString::Printf(TEXT("card-focus-stage%d"), Stage));
            CardFocused = false;
			Prepared = false;
			++Stage;
			return false;
		}

		if (!Placed)
		{
			const auto* Map = Host->FindPlacementMap();
			if (!Test->TestNotNull(TEXT("Playable placement map exists"), Map)) return true;
			for (const TCHAR* Id : {TEXT("Building.Weaver"), TEXT("Building.Tailor"), TEXT("Building.Chandler"), TEXT("Building.Ropewalk")})
			{
				if (!Test->TestTrue(Id, Place(World, Host, Hud->GetBuildMenuPresentationModel(), Map, Id))) return true;
			}
			Hud->GetBuildMenuPresentationModel()->SetOpen(false);
			Placed = true;
			ReadyTime = FPlatformTime::Seconds() + 2;
			return false;
		}

		if (FPlatformTime::Seconds() < ReadyTime) return false;
		if (WorkshopIndex >= 4) return true;
		const TCHAR* Workshops[] = {TEXT("Building.Weaver"), TEXT("Building.Tailor"), TEXT("Building.Chandler"), TEXT("Building.Ropewalk")};
		if (!Selected)
		{
			for (TActorIterator<AHansaBuildingWorldProjectionActor> It(World); It; ++It)
			{
				if (It->GetBuildingDefinitionId() != Workshops[WorkshopIndex]) continue;
				Controller->OnWorldSelectionChanged.Broadcast(*It, FHitResult());
				bool bUsesStagedMesh = false;
				TArray<UStaticMeshComponent*> Meshes;
				It->GetComponents(Meshes);
				for (const auto* Mesh : Meshes)
				{
					if (Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName().Contains(FParse::Param(FCommandLine::Get(), TEXT("TextileProductionCandidate")) ? TEXT("/Game/Hansa/Generated/Staging/TextileProductionModelsV1/") : TEXT("/Game/Mesh/hansa-textile-production/")))
						bUsesStagedMesh = true;
				}
				Test->TestTrue(TEXT("Placed workshop uses its imported candidate mesh"), bUsesStagedMesh);
				if (auto* Camera = Cast<AHansaStrategyCameraPawn>(Controller->GetPawn())) Camera->FocusWorldLocationIntent(It->GetActorLocation());
				Selected = true;
				break;
			}
			if (!Selected) return false;
			Root->ActivateSemanticId(TEXT("Session.Help.Hide"));
			ReadyTime = FPlatformTime::Seconds() + 2;
			return false;
		}

		if (WorkshopIndex == 3 && !RopeRecipeChecked)
		{
			const auto& Inspector = Hud->GetInspectorPresentationModel()->GetSnapshot();
			const auto* Hemp = Inspector.Actions.FindByPredicate([](const auto& Action) { return Action.StableId == TEXT("Inspector.Recipe.Recipe.LayHempRope"); });
			const auto* Flax = Inspector.Actions.FindByPredicate([](const auto& Action) { return Action.StableId == TEXT("Inspector.Recipe.Recipe.LayFlaxRope"); });
			Test->TestTrue(TEXT("Ropewalk exposes hemp and flax as separate actions"), Hemp && Flax);
            Test->TestTrue(TEXT("Recipe has readable authored label"), Flax && Flax->Label.ToString() == TEXT("Lay flax rope"));
			Test->TestTrue(TEXT("Hemp recipe selects through the native inspector action"), Root->ActivateSemanticId(TEXT("Inspector.Recipe.Recipe.LayHempRope")));
            Test->TestTrue(TEXT("Flax recipe selects through the native inspector action"), Root->ActivateSemanticId(TEXT("Inspector.Recipe.Recipe.LayFlaxRope")));
			
            Test->TestTrue(TEXT("Recipe action accepts keyboard/controller focus"), Root->FocusSemanticId(TEXT("Inspector.Recipe.Recipe.LayFlaxRope")));
            RecipeWidget = Root->ResolveSemanticWidget(TEXT("Inspector.Recipe.Recipe.LayFlaxRope"));
            TArray<uint8> SaveBytes;
			Test->TestTrue(TEXT("Candidate save captures selected Ropewalk recipe"), Host->CaptureSaveBytes(SaveBytes, TEXT("Textile UAT"), TEXT("2026-09-19T00:00:00Z")).IsSuccess());
			Test->TestTrue(TEXT("Candidate save restores selected Ropewalk recipe"), Host->RestoreSaveBytes(SaveBytes).IsSuccess());
			const auto Projection = Host->BuildProjection();
			const auto* Rope = Projection ? Projection.Value.GetProductions().FindByPredicate([&](const auto& Production)
			{
				return Production.BuildingId.IsValid() && Projection.Value.GetBuildingWorldProjections().ContainsByPredicate([&](const auto& Building)
				{
					return Building.BuildingId == Production.BuildingId && Building.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Ropewalk");
				});
			}) : nullptr;
			Test->TestTrue(TEXT("Restored Ropewalk retains only the flax request"), Rope && Rope->RequestedRecipeId.ToString() == TEXT("Recipe.LayFlaxRope"));
			RopeRecipeChecked = true;
			ReadyTime = FPlatformTime::Seconds() + 1;
			return false;
		}

		if (WorkshopIndex == 3)
        {
            Test->TestTrue(TEXT("Recipe widget identity survives refresh and save/load"), RecipeWidget == Root->ResolveSemanticWidget(TEXT("Inspector.Recipe.Recipe.LayFlaxRope")));
            Test->TestTrue(TEXT("Recipe action remains focusable after restore"), Root->FocusSemanticId(TEXT("Inspector.Recipe.Recipe.LayFlaxRope")));
        }
        Capture(Viewport, Root, FString::Printf(TEXT("workshop-stage%d"), WorkshopIndex));
		++WorkshopIndex;
		Selected = false;
		ReadyTime = FPlatformTime::Seconds() + 1;
		return false;
	}

private:
	bool Place(UWorld* World, UHansaRuntimeSimulationHost* Host, UHansaBuildMenuPresentationModel* Build,
		const Hansa::Simulation::FHansaPlacementMapInitialization* Map, const TCHAR* Id)
	{
		using namespace Hansa::Simulation;
		for (TActorIterator<AHansaBuildingWorldProjectionActor> It(World); It; ++It)
			if (It->GetBuildingDefinitionId() == Id) return true;
		const auto* Definition = Host->FindBuildingDefinition(Id);
		if (!Definition) return false;
		for (const auto& Cell : Map->Cells)
		{
			if (Cell.Terrain != EHansaPlacementTerrain::Land) continue;
			FHansaPlacementSpec Spec{Host->GetCityId(), FHansaBuildingTypeId::TryParse(Id).Value, Cell.Coordinate, EHansaGridRotation::North};
			const auto Initial = Host->ValidatePlacement(Spec);
			if (!Initial.CanPlace() && !(Initial.GetReasons().Num() == 1 && Initial.GetPrimaryFailure() == EHansaPlacementFailure::RoadRequired)) continue;
			if (!Initial.CanPlace())
			{
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const int32 X = Side == 0 ? Spec.Anchor.X - 1 : Side == 1 ? Spec.Anchor.X + Definition->FootprintWidthCells : Spec.Anchor.X;
					const int32 Y = Side == 2 ? Spec.Anchor.Y - 1 : Side == 3 ? Spec.Anchor.Y + Definition->FootprintHeightCells : Spec.Anchor.Y;
					Build->SelectBuilding(TEXT("Building.Road"));
					Build->TargetGridCell(X, Y);
					if (Build->GetSnapshot().bCanConfirm && Build->ConfirmIntent())
					{
						Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Road"))->BuildTicks + 2);
						break;
					}
				}
			}
			if (!Host->ValidatePlacement(Spec).CanPlace()) continue;
			Build->SelectBuilding(Id);
			Build->TargetGridCell(Cell.Coordinate.X, Cell.Coordinate.Y);
			if (Build->GetSnapshot().bCanConfirm && Build->ConfirmIntent())
			{
				Host->AdvanceTicks(Definition->BuildTicks + 2);
				return true;
			}
		}
		return false;
	}

	void Capture(UGameViewportClient* Viewport, const TSharedPtr<Hansa::UI::SHansaRootHud>& Root, const FString& Name)
	{
		TArray<FColor> Pixels;
		FIntVector Size;
		if (!Test->TestTrue(TEXT("Capture actual game viewport"), FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(), Pixels, Size))) return;
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("TextileProduction");
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Base = Directory / FString::Printf(TEXT("%s-%dx%d-scale%d%s"), *Name, Size.X, Size.Y, FMath::RoundToInt(UiScale * 100), bLargeText ? TEXT("-large") : TEXT(""));
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
		Test->TestTrue(TEXT("Save actual game viewport"), FFileHelper::SaveArrayToFile(Png, *(Base + TEXT(".png"))));
		FString Evidence;
		for (const auto& Node : Root->GetSemanticSnapshot())
			Evidence += FString::Printf(TEXT("%s\tvisible=%d\tenabled=%d\tselected=%d\tfocused=%d\t%s\n"),
				*Node.Id, Node.State.bVisible, Node.State.bEnabled, Node.State.bSelected, Node.State.bFocused, *Node.State.Value);
		FFileHelper::SaveStringToFile(Evidence, *(Base + TEXT(".tsv")));
	}

	TSharedPtr<SWidget> RecipeWidget;
    float UiScale = 1.f;
    bool bLargeText = false;
    FAutomationTestBase* Test;
	double Started = 0;
	double ReadyTime = 0;
	uint64 ReadyFrame = 0;
	int32 Stage = 0;
	int32 WorkshopIndex = 0;
	bool Initialized = false;
	bool Prepared = false;
    bool CardFocused = false;
	bool Placed = false;
	bool Selected = false;
	bool RopeRecipeChecked = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTextileProductionPlayableReview,
	"Hansa.UI.TextileProduction.PlayableReview",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHansaTextileProductionPlayableReview::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FHansaTextileProductionCapture(this));
	return true;
}

#endif
