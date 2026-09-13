#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaContextInspector.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

namespace
{
class FProductionInspectorCapture final:public IAutomationLatentCommand
{
public:
    explicit FProductionInspectorCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){if(FParse::Param(FCommandLine::Get(),TEXT("HansaConstructionInspectorOnly")))Stage=8;if(FParse::Param(FCommandLine::Get(),TEXT("HansaProductStockOnly")))Stage=14;}
    ~FProductionInspectorCapture() override
    {
        if(CursorSaved&&FSlateApplication::IsInitialized()){FSlateApplication::Get().CloseToolTip();FSlateApplication::Get().SetCursorPos(OriginalCursor);}
    }
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>120){Test->AddError(TEXT("Production inspector viewport timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
        auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
        auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
        auto* GM=W->GetAuthGameMode<AHansaGameMode>();auto* Host=GM?GM->GetSimulationHost():nullptr;
        if(!Hud||!Host||!Hud->GetRootWidget().IsValid()||HansaWaitForFrontend(Hud))return false;
        auto Root=Hud->GetRootWidget();auto* Inspector=Hud->GetInspectorPresentationModel();
        if(!Prepared)
        {
            if(!CursorSaved){OriginalCursor=FSlateApplication::Get().GetCursorPos();CursorSaved=true;}
            if(Stage!=7 && Stage<14){FSlateApplication::Get().CloseToolTip();FSlateApplication::Get().SetCursorPos(FVector2D(2,2));}
            Hud->GetScenarioPresentationModel()->Close();Hud->GetBuildMenuPresentationModel()->SetOpen(false);
            Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
            if(Stage<3)
            {
                const TCHAR* Definitions[]={TEXT("Building.GrainFarm"),TEXT("Building.Mill"),TEXT("Building.Bakery")};
                bool Found=false;
                for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==Definitions[Stage])
                {
                    C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
                    if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(It->GetActorLocation());}
                    Found=true;break;
                }
                if(!Test->TestTrue(TEXT("Bread-chain unit selected in real world"),Found))return true;
                Test->TestTrue(TEXT("Actual selection exposes production detail"),Inspector->GetSnapshot().Production.bValid);
            }
            if(Stage==3) { Host->AdvanceTicks(FMath::Max(1,Inspector->GetSnapshot().Production.CycleTicks/2));Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Normal); }
            if(Stage==4) { Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Normal); }
            if(Stage==5) { Inspector->OpenCauseIntent();Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.Pin")); }
            if(Stage==6) { if(Inspector->GetSnapshot().bCauseExpanded)Inspector->OpenCauseIntent();Root->SetPreferences({true,true,true});Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.ToggleProduction")); }
            if(Stage==7){Root->SetPreferences({});if(Inspector->GetSnapshot().bCauseExpanded)Inspector->OpenCauseIntent();}
            if(Stage==8)
            {
                auto* Build=Hud->GetBuildMenuPresentationModel();
                const bool Bakery = FParse::Param(FCommandLine::Get(), TEXT("HansaConstructedBakery"));
                const TCHAR* Definition = Bakery ? TEXT("Building.Bakery") : TEXT("Building.Market");
                Build->SelectBuilding(Definition);
                bool Placed=false;
                const auto* Map=Host->FindPlacementMap();
                if(!Map)return true;
                for(const auto& Cell:Map->Cells)
                {
                    using namespace Hansa::Simulation;
                    FHansaPlacementSpec Spec;
                    Spec.CityId=Host->GetCityId();Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(Definition).Value;
                    Spec.Anchor=Cell.Coordinate;
                    const auto Valid=Host->ValidatePlacement(Spec);
                    if(!Valid.CanPlace() && Valid.GetReasons().Num()==1 && Valid.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired)
                    {
                        Build->SelectBuilding(TEXT("Building.Road"));
                        Build->TargetGridCell(Cell.Coordinate.X-1,Cell.Coordinate.Y);
                        if(Build->GetSnapshot().bCanConfirm)Build->ConfirmIntent();
                        Build->SelectBuilding(Definition);
                    }
                    Build->TargetGridCell(Cell.Coordinate.X,Cell.Coordinate.Y);
                    if(Build->GetSnapshot().bCanConfirm && Build->ConfirmIntent()){Placed=true;break;}
                }
                if(!Test->TestTrue(TEXT("Market construction placed through ordinary build intent"),Placed))return true;
                Build->SetOpen(false);
                auto Projection=Host->BuildProjection();
                int64 Id=0;
                for(const auto& Site:Projection.Value.GetConstructions())if(Site.State==Hansa::Simulation::EHansaConstructionState::UnderConstruction)Id=int64(Site.BuildingId.GetValue());
                for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(int64(It->GetBuildingId().GetValue())==Id)
                {
                    C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
                    break;
                }
                if(!Test->TestTrue(TEXT("Selected site uses construction circle"),Inspector->GetSnapshot().Production.bConstruction))return true;
                Host->AdvanceTicks(FMath::Max(1,Inspector->GetSnapshot().Production.CycleTicks/3));
                Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Normal);
            }
            if(Stage==9)
            {
                Inspector->OpenCauseIntent();
                Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.CancelConstruction"));
            }
            if(Stage==10)
            {
                Inspector->OpenCauseIntent();
                Root->SetPreferences({true,true,true});
            }
            if(Stage==11)
            {
                Root->SetPreferences({});
                Host->AdvanceTicks(Inspector->GetSnapshot().Production.CycleTicks);
                Test->TestFalse(TEXT("Completion leaves construction mode"),Inspector->GetSnapshot().Production.bConstruction);
                if (FParse::Param(FCommandLine::Get(), TEXT("HansaConstructedBakery")))
                    Test->TestTrue(TEXT("Player-built bakery returns to compact operating inspector"), Inspector->GetSnapshot().Production.bValid && !Inspector->GetSnapshot().Production.bConstruction);
                else Test->TestTrue(TEXT("Completed market returns to its ordinary inspector"),Inspector->GetSnapshot().Kind==EHansaInspectorObjectKind::Market);
            }
			if(Stage==12)
			{
				bool Found=false;
				for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingId().GetValue()==3)
				{
					C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());Found=true;break;
				}
				if(!Test->TestTrue(TEXT("Bakery selected for physical road interruption"),Found))return true;
				using namespace Hansa::Simulation;
				for(const uint64 Id:{uint64(50),uint64(51),uint64(52),uint64(61),uint64(62)})
					Test->TestTrue(TEXT("Adjacent road segment removed through gameplay command"),Host->RemoveBuilding(FHansaBuildingId::TryCreate(Id).Value).IsSuccess());
				Test->TestFalse(TEXT("Selected bakery immediately reports lost market access"),Inspector->GetSnapshot().Production.bHasMarketAccess);
				bool bMarkerVisible=false;
				for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)
					if(It->GetBuildingId().GetValue()==3)bMarkerVisible=It->IsRoadDisconnectedIndicatorVisible()&&It->RoadDisconnectedMarker->IsVisible();
				Test->TestTrue(TEXT("Disconnected bakery shows the imported rotating road marker"),bMarkerVisible);
				if(!Inspector->GetSnapshot().bCauseExpanded)Inspector->OpenCauseIntent();
				Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.ViewStorage"));
			}
			if(Stage==13)
			{
				using namespace Hansa::Simulation;
				FHansaPlacementSpec Road;Road.CityId=Host->GetCityId();Road.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;Road.Anchor={21,19};
				Test->TestTrue(TEXT("Road reconnection placed through gameplay command"),Host->PlaceBuildings(MakeArrayView(&Road,1)).IsSuccess());
				const auto* Definition=Host->FindBuildingDefinition(TEXT("Building.Road"));
				if(!Test->TestNotNull(TEXT("Road definition available for capture recovery"),Definition))return true;
				Host->AdvanceTicks(Definition->BuildTicks+1);
				Test->TestTrue(TEXT("Selected bakery recovers physical market access"),Inspector->GetSnapshot().Production.bHasMarketAccess);
				bool bMarkerHidden=false;
				for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)
					if(It->GetBuildingId().GetValue()==3)bMarkerHidden=!It->IsRoadDisconnectedIndicatorVisible()&&!It->RoadDisconnectedMarker->IsVisible();
				Test->TestTrue(TEXT("Reconnected bakery hides the road marker"),bMarkerHidden);
				Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.ViewStorage"));
			}
            if(Stage>=14)
            {
                FSlateApplication::Get().CloseToolTip();FSlateApplication::Get().SetCursorPos(FVector2D(2,2));
                Root->SetPreferences(Stage==16?Hansa::UI::FUiPreferences{true,true,true}:Hansa::UI::FUiPreferences{});
                for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)
                    if(It->GetBuildingDefinitionId()==TEXT("Building.Bakery")){C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());break;}
                if(Inspector->GetSnapshot().bCauseExpanded)Inspector->OpenCauseIntent();
                HoverPlaced=false;
            }
            Prepared=true;Ready=FPlatformTime::Seconds();return false;
        }
        // Wait for a real Slate paint after the event-driven layout; no synthetic screenshot surface.
        if(FPlatformTime::Seconds()-Ready<.35)return false;
        if(Stage==3 && Inspector->GetSnapshot().Production.ProgressTicks==0)return false;
        if(Stage==7)
        {
            auto Owner=Root->GetInspector()->ResolveSemanticWidget(TEXT("Inspector.Production.Batch.Hover"));
            if(!Test->TestTrue(TEXT("Circle has a nonempty batch tooltip"),Owner.IsValid()&&Owner->GetToolTip().IsValid()&&!Owner->GetToolTip()->IsEmpty()))return true;
            if(!HoverPlaced)
            {
                const auto Pos=Owner->GetCachedGeometry().GetAbsolutePosition()+Owner->GetCachedGeometry().GetAbsoluteSize()*.5f;
                FSlateApplication::Get().SetCursorPos(Pos);
                FSlateApplication::Get().ProcessMouseMoveEvent(FPointerEvent(0,Pos,OriginalCursor,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState()));
                HoverPlaced=true;HoverStart=FPlatformTime::Seconds();return false;
            }
            FSlateApplication::Get().UpdateToolTip(true);
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Tip=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Batch.Tooltip");});
            if(!Tip||!Tip->State.bVisible)
            {
                if(FPlatformTime::Seconds()-HoverStart<5)return false;
                Test->AddError(TEXT("Hover did not open the native batch popup"));return true;
            }
            if(FPlatformTime::Seconds()-HoverStart<1)return false;
            if(!TooltipAdvanced)
            {
                const auto* Before=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Batch.Percent");});
                const FString PreviousPercent=Before?Before->State.Value:FString();
                Host->AdvanceTicks(1);
                const auto AfterNodes=Root->GetSemanticSnapshot();
                const auto* After=AfterNodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Batch.Percent");});
                Test->TestTrue(TEXT("Open popup updates percentage without re-hovering"),After&&After->State.bVisible&&After->State.Value!=PreviousPercent);
                TooltipAdvanced=true;return false;
            }
            const auto* Percent=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Batch.Percent");});
            const auto* Progress=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Result");});
            Test->TestTrue(TEXT("Popup percentage matches live batch state"),Percent&&Progress&&Percent->State.Value==Progress->State.Value);
        }
        if(Stage>=14)
        {
            const auto& Ports=Stage==15?Inspector->GetSnapshot().Production.Outputs:Inspector->GetSnapshot().Production.Inputs;
            if(!Test->TestTrue(TEXT("Selected bakery has products"),!Ports.IsEmpty()))return true;
            const FString Id=FString(Stage==15?TEXT("Inspector.Production.Output."):TEXT("Inspector.Production.Input."))+Ports[0].GoodId.ToString();
            auto Owner=Root->GetInspector()->ResolveSemanticWidget(Id);
            if(!Test->TestTrue(TEXT("Product has a native tooltip"),Owner.IsValid()&&Owner->GetToolTip().IsValid()))return true;
            if(!HoverPlaced)
            {
                if(Stage==16)Test->TestTrue(TEXT("Product can receive keyboard focus"),Root->GetInspector()->FocusSemanticId(Id));
                else
                {
                    const auto Pos=Owner->GetCachedGeometry().GetAbsolutePosition()+Owner->GetCachedGeometry().GetAbsoluteSize()*.5f;
                    FSlateApplication::Get().SetCursorPos(Pos);
                    FSlateApplication::Get().ProcessMouseMoveEvent(FPointerEvent(0,Pos,OriginalCursor,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState()));
                }
                HoverPlaced=true;HoverStart=FPlatformTime::Seconds();return false;
            }
            if(Stage!=16)FSlateApplication::Get().UpdateToolTip(true);
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Tip=Nodes.FindByPredicate([&](const auto& N){return N.Id==Id+TEXT(".Tooltip");});
            if(!Tip||!Tip->State.bVisible)
            {
                if(FPlatformTime::Seconds()-HoverStart<5)return false;
                Test->AddError(TEXT("Product hover/focus did not open stock popup"));return true;
            }
            if(FPlatformTime::Seconds()-HoverStart<1)return false;
            Test->TestTrue(TEXT("Popup displays storage and market quantities"),Tip->State.Value.Contains(TEXT("in storage\n"))&&Tip->State.Value.Contains(TEXT("in markets")));
            if(!TooltipAdvanced)
            {
                auto OriginalTip=Owner->GetToolTip();Host->AdvanceTicks(1);
                Test->TestTrue(TEXT("Live stock update retains open tooltip widget"),Owner->GetToolTip()==OriginalTip);
                TooltipAdvanced=true;return false;
            }
        }
        TArray<FColor> Pixels;FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size))
        {Test->AddError(TEXT("Native production screenshot failed"));return true;}
		const TCHAR* Names[]={TEXT("farm"),TEXT("mill"),TEXT("bakery"),TEXT("batch-a"),TEXT("batch-b"),TEXT("focus"),TEXT("accessible"),TEXT("tooltip"),TEXT("construction"),TEXT("construction-details"),TEXT("construction-accessible"),TEXT("construction-completed"),TEXT("market-access-disconnected"),TEXT("market-access-reconnected"),TEXT("product-input-tooltip"),TEXT("product-output-tooltip"),TEXT("product-focus-tooltip")};
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("ProductionInspector");IFileManager::Get().MakeDirectory(*Dir,true);
        const FString Base=Dir/FString::Printf(TEXT("production-%dx%d-%s%s"),Size.X,Size.Y,FParse::Param(FCommandLine::Get(), TEXT("HansaConstructedBakery")) ? TEXT("built-bakery-") : TEXT(""),Names[Stage]);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Native viewport saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
        for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("Inspector.")))
        {
            Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,
                N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
            if(N.Id==TEXT("Inspector.Root"))Test->TestTrue(TEXT("Inspector fits native viewport"),N.Bounds.Min.X>=0&&N.Bounds.Min.Y>=0&&N.Bounds.Max.X<=Size.X&&N.Bounds.Max.Y<=Size.Y);
        }
        if(Stage>=8 && Stage<=10)
        {
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Panel=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Root");});
            const auto* Ring=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Result");});
            const auto* Details=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Action.OpenCause");});
            Test->TestTrue(TEXT("Construction circle and Details fit the panel"),Panel&&Ring&&Details&&Ring->Bounds.Min.X>=Panel->Bounds.Min.X&&Ring->Bounds.Max.X<=Panel->Bounds.Max.X&&Ring->Bounds.Max.Y<=Details->Bounds.Min.Y);
            if(Stage==9)
            {
                const auto* Cancel=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Action.CancelConstruction");});
                Test->TestTrue(TEXT("Focused cancellation fits construction Details viewport"),Panel&&Cancel&&Cancel->State.bVisible&&Cancel->Bounds.Min.Y>=Panel->Bounds.Min.Y+60&&Cancel->Bounds.Max.Y<=Details->Bounds.Min.Y);
            }
        }
        if(Stage<5)
        {
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Panel=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Root");});
            const auto* Cost=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Cost");});
            Test->TestTrue(TEXT("Compact panel is at most 320 by 480 at standard scale"),Panel&&Panel->Bounds.Width()<=320&&Panel->Bounds.Height()<=480);
            Test->TestTrue(TEXT("Undefined operating cost stays blank"),Cost&&Cost->State.Value.IsEmpty());
        }
        if(Stage<5 && Size.X>=1920)
        {
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Panel=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Root");});
            const auto* Last=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Action.OpenCause");});
            Test->TestTrue(TEXT("All main actions fit at normal 1080p or larger"),Panel&&Last&&Last->Bounds.Max.Y<=Panel->Bounds.Max.Y-1);
        }
        if(Stage==5||Stage==6)
        {
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Panel=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Root");});
            const FString FocusId=Stage==5?TEXT("Inspector.Action.Pin"):TEXT("Inspector.Action.ToggleProduction");
            const auto* Focus=Nodes.FindByPredicate([&](const auto& N){return N.Id==FocusId;});
            Test->TestTrue(TEXT("Entire focused control is inside inspector viewport"),Panel&&Focus&&Focus->Bounds.Min.Y>=Panel->Bounds.Min.Y+60&&Focus->Bounds.Max.Y<=Panel->Bounds.Max.Y-1);
        }
        if(Stage==6)
        {
            const auto Nodes=Root->GetSemanticSnapshot();
            const auto* Flow=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Flows");});
            const auto* Pause=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Action.ToggleProduction");});
			Test->TestTrue(TEXT("The process flow remains in the scroll viewport above the fixed controls with large text"),Flow&&Pause&&Flow->Bounds.Min.Y<Pause->Bounds.Min.Y);
        }
        if(Stage>=14)
        {
            const auto& Ports=Stage==15?Inspector->GetSnapshot().Production.Outputs:Inspector->GetSnapshot().Production.Inputs;
            const FString Id=FString(Stage==15?TEXT("Inspector.Production.Output."):TEXT("Inspector.Production.Input."))+Ports[0].GoodId.ToString()+TEXT(".Tooltip");
            auto TipWidget=Root->GetInspector()->ResolveSemanticWidget(Id);
            TArray<FColor> TipPixels;FIntVector TipSize;
            if(TipWidget&&FSlateApplication::Get().TakeScreenshot(TipWidget.ToSharedRef(),TipPixels,TipSize))
            {TArray64<uint8> TipPng;FImageUtils::PNGCompressImageArray(TipSize.X,TipSize.Y,TipPixels,TipPng);FFileHelper::SaveArrayToFile(TipPng,*(Base+TEXT("-popup.png")));}
            else Test->AddError(TEXT("Actual product popup screenshot failed"));
        }
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
        if(Stage==3)FirstFraction=Inspector->GetBatchVisualFraction();
        if(Stage==4)Test->TestNotEqual(TEXT("Batch circle changes during live simulation"),Inspector->GetBatchVisualFraction(),FirstFraction);
        if(Stage==5)
        {
            const bool Before=Inspector->GetSnapshot().bPinned;
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
            Test->TestNotEqual(TEXT("Native controller activation pins production"),Inspector->GetSnapshot().bPinned,Before);
        }
        if(Stage==7)
        {
            auto TipWidget=Root->GetInspector()->ResolveSemanticWidget(TEXT("Inspector.Production.Batch.Tooltip"));
            TArray<FColor> TipPixels;FIntVector TipSize;
            if(Test->TestTrue(TEXT("Real hovered tooltip window can be captured"),FSlateApplication::Get().TakeScreenshot(TipWidget.ToSharedRef(),TipPixels,TipSize)))
            {TArray64<uint8> TipPng;FImageUtils::PNGCompressImageArray(TipSize.X,TipSize.Y,TipPixels,TipPng);FFileHelper::SaveArrayToFile(TipPng,*(Base+TEXT("-popup.png")));}
            FSlateApplication::Get().CloseToolTip();FSlateApplication::Get().SetCursorPos(OriginalCursor);
        }
		if(Stage==12||Stage==13)
		{
			const auto Nodes=Root->GetSemanticSnapshot();
			const auto* Access=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Logistics.MarketAccess");});
			Test->TestTrue(TEXT("Physical access is present in the semantic snapshot"),Access!=nullptr);
			if(Access)Test->TestTrue(TEXT("Semantic access matches the captured gameplay state"),Stage==12
				? Access->State.Value.Contains(TEXT("connected=false"))
				: Access->State.Value.Contains(TEXT("connected=true"))&&Access->State.Value.Contains(TEXT("selectedMarketBuildingId=")));
		}
		++Stage;Prepared=false;TooltipAdvanced=false;return Stage==17;
    }
private:
    FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;float FirstFraction=0;bool HoverPlaced=false,TooltipAdvanced=false,CursorSaved=false;double HoverStart=0;FVector2D OriginalCursor;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionInspectorViewport,"Hansa.UI.ProductionInspector.RealViewport",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FProductionInspectorViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FProductionInspectorCapture(this));return true;}
#endif
