#include "UI/HansaStrategicAutomationScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "Gameplay/HansaStrategicAutomationFixture.h"
#include "Research/HansaResearch.h"
#include "Scenario/HansaScenario.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Simulation;

namespace Hansa::Automation
{
	namespace
	{
		const FLinearColor Navy = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy);
		const FLinearColor Harbor = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HarborSlate);
		const FLinearColor Ink = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink);
		const FLinearColor Linen = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen);
		const FLinearColor Parchment = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment);
		const FLinearColor Brass = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
		const FLinearColor Teal = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal);
		const FLinearColor Chalk = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk);

		TSharedRef<SWidget> ActionButton(TSharedPtr<SWidget>& Out, const FString& Label, TFunction<bool()> Intent)
		{
			TSharedPtr<SButton> Button;
			SAssignNew(Button, SButton).Text(FText::FromString(Label)).ContentPadding(FMargin(14, 10))
				.ButtonColorAndOpacity(Parchment).ForegroundColor(Ink)
				.OnClicked_Lambda([Intent = MoveTemp(Intent)] { Intent(); return FReply::Handled(); });
			Out = Button;
			return Button.ToSharedRef();
		}

		TSharedRef<FJsonObject> Command(const TCHAR* Name)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("command"), Name);
			return Json;
		}
	}

	void SHansaStrategicAutomationScreen::Construct(const FArguments&,
		FHansaStrategicAutomationFixture& InFixture, FHansaSemanticUiRegistry& InRegistry)
	{
		Fixture = &InFixture;
		Registry = &InRegistry;
		RegisterSemantics();
		TSharedPtr<SWidget> Root, Build, Diagnose, Routes, Research;
		ChildSlot[SAssignNew(PresentationBox, SBox).WidthOverride(1280).HeightOverride(720)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox).WidthOverride(1280).HeightOverride(720)
				[
					SAssignNew(ScreenWidget, SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
					.BorderBackgroundColor(Navy).Padding(28)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("LÜBECK STRATEGIC PROVING RUN")))
							.ColorAndOpacity(Chalk).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 18)
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Native semantic controls · authoritative commands · deterministic seeded simulation")))
							.ColorAndOpacity(Brass)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1).Padding(5)[ActionButton(Build, TEXT("1  BUILD ROAD"), [this]{ return BuildRoadIntent(); })]
							+ SHorizontalBox::Slot().FillWidth(1).Padding(5)[ActionButton(Diagnose, TEXT("2  DIAGNOSE SHORTAGE"), [this]{ return DiagnoseShortageIntent(); })]
							+ SHorizontalBox::Slot().FillWidth(1).Padding(5)[ActionButton(Routes, TEXT("3  START RELIEF ROUTES"), [this]{ return StartReliefRoutesIntent(); })]
							+ SHorizontalBox::Slot().FillWidth(1).Padding(5)[ActionButton(Research, TEXT("4  QUEUE MARKET REPORTS"), [this]{ return QueueResearchIntent(); })]
						]
						+ SVerticalBox::Slot().FillHeight(1).Padding(5, 22)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 10, 0)
							[
								SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Harbor).Padding(24)
								[
									SAssignNew(SummaryText, STextBlock).ColorAndOpacity(Chalk).AutoWrapText(true)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1).Padding(10, 0, 0, 0)
							[
								SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Linen).Padding(24)
								[
									SAssignNew(EvidenceText, STextBlock).ColorAndOpacity(Ink).AutoWrapText(true)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Evidence bundle: state hash · causal events · research effects · AI decisions · objective progress"))).ColorAndOpacity(Teal)
						]
					]
				]
			]
		]];
		Root = ScreenWidget;
		SemanticWidgets = {{TEXT("Strategic.Root"), Root}, {TEXT("Strategic.Action.Build"), Build},
			{TEXT("Strategic.Action.Diagnose"), Diagnose}, {TEXT("Strategic.Action.StartRoutes"), Routes},
			{TEXT("Strategic.Action.QueueResearch"), Research},
			{TEXT("BuildMenu.Action.ConfirmBreadChain"), Build},
			{TEXT("Market.Action.DiagnoseGrain"), Diagnose},
			{TEXT("TradeRoute.Editor.Action.StartRelief"), Routes},
			{TEXT("Research.Action.QueueMarketReports"), Research}};
		SynchronizeSemantics();
	}

	void SHansaStrategicAutomationScreen::RegisterSemantics()
	{
		Registry->Reset();
		auto Add = [this](const TCHAR* Id, EHansaSemanticRole Role, const TCHAR* Label, const TCHAR* Parent,
			TFunction<bool()> Activate = {})
		{
			FHansaSemanticNode Node; Node.Id = Id; Node.Role = Role; Node.Label = Label; Node.ParentId = Parent;
			FHansaSemanticActionHandlers Handlers;
			if (Activate) { Node.Actions.Add(EHansaSemanticAction::Activate); Handlers.Activate = MoveTemp(Activate); }
			Registry->RegisterNode(MoveTemp(Node), MoveTemp(Handlers));
		};
		Add(TEXT("Strategic.Root"), EHansaSemanticRole::Screen, TEXT("Strategic proving run"), TEXT(""));
		Add(TEXT("Strategic.Action.Build"), EHansaSemanticRole::Button, TEXT("Build road"), TEXT("Strategic.Root"), [this]{ return BuildRoadIntent(); });
		Add(TEXT("Strategic.Action.Diagnose"), EHansaSemanticRole::Button, TEXT("Diagnose shortage"), TEXT("Strategic.Root"), [this]{ return DiagnoseShortageIntent(); });
		Add(TEXT("Strategic.Action.StartRoutes"), EHansaSemanticRole::Button, TEXT("Start relief routes"), TEXT("Strategic.Root"), [this]{ return StartReliefRoutesIntent(); });
		Add(TEXT("Strategic.Action.QueueResearch"), EHansaSemanticRole::Button, TEXT("Queue market reports"), TEXT("Strategic.Root"), [this]{ return QueueResearchIntent(); });
		Add(TEXT("Strategic.Status.Building"), EHansaSemanticRole::Status, TEXT("Building placed"), TEXT("Strategic.Root"));
		Add(TEXT("Strategic.Status.Shortage"), EHansaSemanticRole::Status, TEXT("Shortage diagnosed"), TEXT("Strategic.Root"));
		Add(TEXT("Strategic.Status.RouteRecovery"), EHansaSemanticRole::Status, TEXT("Route recovery"), TEXT("Strategic.Root"));
		Add(TEXT("Strategic.Status.Research"), EHansaSemanticRole::Status, TEXT("Research effect applied"), TEXT("Strategic.Root"));
		Add(TEXT("Strategic.Status.AI"), EHansaSemanticRole::Status, TEXT("Merchant AI progressed"), TEXT("Strategic.Root"));
		Add(TEXT("Strategic.Status.Victory"), EHansaSemanticRole::Status, TEXT("Scenario victory"), TEXT("Strategic.Root"));
		Add(TEXT("HUD.Root"), EHansaSemanticRole::Screen, TEXT("Lübeck shortage recovery"), TEXT(""));
		Add(TEXT("BuildMenu.Action.ConfirmBreadChain"), EHansaSemanticRole::Button, TEXT("Confirm local bread chain"), TEXT("HUD.Root"), [this]{ return BuildRoadIntent(); });
		Add(TEXT("BuildMenu.Status.BreadChain"), EHansaSemanticRole::Status, TEXT("Local grain, mill and bakery chain"), TEXT("HUD.Root"));
		Add(TEXT("Market.Good.Grain"), EHansaSemanticRole::Status, TEXT("Lübeck grain market"), TEXT("HUD.Root"));
		Add(TEXT("Market.Action.DiagnoseGrain"), EHansaSemanticRole::Button, TEXT("Diagnose grain shortage"), TEXT("HUD.Root"), [this]{ return DiagnoseShortageIntent(); });
		Add(TEXT("Market.Status.GrainDiagnosed"), EHansaSemanticRole::Status, TEXT("Grain shortage diagnosis"), TEXT("HUD.Root"));
		Add(TEXT("TradeRoute.Editor.Action.StartRelief"), EHansaSemanticRole::Button, TEXT("Start sea and land relief routes"), TEXT("HUD.Root"), [this]{ return StartReliefRoutesIntent(); });
		Add(TEXT("TradeRoute.Editor.Status.Delivered"), EHansaSemanticRole::Status, TEXT("Relief route delivery"), TEXT("HUD.Root"));
		Add(TEXT("Research.Action.QueueMarketReports"), EHansaSemanticRole::Button, TEXT("Queue market reports"), TEXT("HUD.Root"), [this]{ return QueueResearchIntent(); });
		Add(TEXT("Research.Status.MarketReports"), EHansaSemanticRole::Status, TEXT("Market reports research"), TEXT("HUD.Root"));
		Add(TEXT("HUD.Status.MerchantAI"), EHansaSemanticRole::Status, TEXT("Rival merchant activity"), TEXT("HUD.Root"));
		Add(TEXT("SaveLoad.Status.RoundTrip"), EHansaSemanticRole::Status, TEXT("Save and load round trip"), TEXT("HUD.Root"));
		Add(TEXT("Scenario.Status.Victory"), EHansaSemanticRole::Status, TEXT("Authored scenario victory"), TEXT("HUD.Root"));
	}

	bool SHansaStrategicAutomationScreen::BuildRoadIntent()
	{
		TSharedRef<FJsonObject> Request = Command(TEXT("building.place"));
		Request->SetStringField(TEXT("buildingDefinitionId"), TEXT("Building.Road")); Request->SetNumberField(TEXT("x"), 10); Request->SetNumberField(TEXT("y"), 30);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>(); FString Error;
		const bool bOk = Fixture->Command(Request, Out, Error); SynchronizeSemantics(); return bOk;
	}

	bool SHansaStrategicAutomationScreen::DiagnoseShortageIntent()
	{
		TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>(); Request->SetStringField(TEXT("query"), TEXT("market.alerts"));
		Request->SetStringField(TEXT("cityId"), TEXT("City.Lubeck")); Request->SetStringField(TEXT("goodId"), TEXT("Good.Grain"));
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>(); FString Error;
		bShortageDiagnosed = Fixture->Query(Request, Out, Error) && Out->GetBoolField(TEXT("shortageDiagnosed"));
		SynchronizeSemantics(); return bShortageDiagnosed;
	}

	bool SHansaStrategicAutomationScreen::StartReliefRoutesIntent()
	{
		for (int32 RouteId : {1, 2})
		{
			TSharedRef<FJsonObject> Request = Command(TEXT("route.set_active")); Request->SetNumberField(TEXT("routeId"), RouteId); Request->SetBoolField(TEXT("active"), true);
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>(); FString Error; if (!Fixture->Command(Request, Out, Error)) return false;
		}
		SynchronizeSemantics(); return true;
	}

	bool SHansaStrategicAutomationScreen::QueueResearchIntent()
	{
		TSharedRef<FJsonObject> Request = Command(TEXT("research.queue")); Request->SetStringField(TEXT("technologyId"), TEXT("Technology.Commerce.MarketReports"));
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>(); FString Error;
		const bool bOk = Fixture->Command(Request, Out, Error); SynchronizeSemantics(); return bOk;
	}

	void SHansaStrategicAutomationScreen::SynchronizeSemantics()
	{
		if (!Fixture->IsLoaded()) return;
		UHansaRuntimeSimulationHost* Host = Fixture->GetHost();
		const auto Projection = Host->BuildProjection();
		if (!Projection) return;
		const int32 InitialCount = static_cast<int32>(Fixture->MakeSummary()->GetNumberField(TEXT("initialBuildingCount")));
		const bool bBuilt = Host->GetPlacedBuildingCount() > InitialCount;
		int32 ActivePlayerRoutes = 0; int64 CompletedPlayerLegs = 0;
		for (const FHansaRouteProjection& Route : Projection.Value.GetRoutes())
		{
			if (Route.Id.GetValue() > 2) continue;
			ActivePlayerRoutes += Route.Lifecycle != EHansaRouteLifecycleState::Inactive ? 1 : 0;
			CompletedPlayerLegs += Route.CompletedLegCount;
		}
		const bool bDelivered = ActivePlayerRoutes == 2 && CompletedPlayerLegs >= 2;
		const FHansaHouseResearchState* Research = Projection.Value.GetResearch().FindByPredicate([Host](const FHansaHouseResearchState& Value){ return Value.HouseId == Host->GetHouseId(); });
		const bool bResearched = Research != nullptr && Research->IsCompleted(TEXT("Technology.Commerce.MarketReports")) && !Research->AppliedEffects.IsEmpty();
		const bool bAi = !Host->GetMerchantAIDecisionHistory().IsEmpty();
		const FHansaScenarioProgress* Scenario = Host->GetScenarioProgress();
		const bool bVictory = Scenario != nullptr && Scenario->Outcome == EHansaScenarioOutcome::Victory;
		const FHansaCityMarketProjection* Grain = Projection.Value.GetMarkets().FindByPredicate([](const FHansaCityMarketProjection& Candidate)
		{
			return Candidate.CityId.ToString() == TEXT("City.Lubeck") && Candidate.GoodId.ToString() == TEXT("Good.Grain");
		});
		const bool bGrainDeficit = Grain != nullptr && Grain->CurrentStock.GetRawValue() < Grain->DesiredReserve.GetRawValue();
		SummaryText->SetText(FText::FromString(FString::Printf(TEXT("CAMPAIGN %s\n\nTick %lld\nState %s\nBuildings %d\nCausal events %d\nMerchant decisions %d"),
			*Fixture->MakeSummary()->GetStringField(TEXT("campaignSeed")), static_cast<long long>(Host->GetSimulationTick()),
			*Fixture->MakeSummary()->GetStringField(TEXT("stateHash")), Host->GetPlacedBuildingCount(), Host->GetEventHistory().Num(), Host->GetMerchantAIDecisionHistory().Num())));
		EvidenceText->SetText(FText::FromString(FString::Printf(TEXT("CHECKPOINTS\n\n%s  Building placed\n%s  Grain shortage diagnosed\n%s  Player cargo delivered\n%s  Research effect applied\n%s  Rival AI progressed\n%s  %s"),
			bBuilt ? TEXT("✓") : TEXT("○"), bShortageDiagnosed ? TEXT("✓") : TEXT("○"), bDelivered ? TEXT("✓") : TEXT("○"),
			bResearched ? TEXT("✓") : TEXT("○"), bAi ? TEXT("✓") : TEXT("○"), bVictory ? TEXT("✓") : TEXT("○"),
			Scenario != nullptr && !Scenario->WinningVictoryId.IsEmpty() ? *Scenario->WinningVictoryId : TEXT("Scenario active"))));
		auto State = [this](const TCHAR* Id, bool Selected, const FString& Value)
		{
			const FHansaSemanticNode* Node = Registry->FindNode(Id); if (Node == nullptr) return;
			auto NewState = Node->State; NewState.bSelected = Selected; NewState.ValueType = TEXT("checkpoint"); NewState.Value = Value;
			Registry->UpdateNode(Id, NewState, Node->Bounds);
		};
		State(TEXT("Strategic.Status.Building"), bBuilt, bBuilt ? TEXT("complete") : TEXT("pending"));
		State(TEXT("Strategic.Status.Shortage"), bShortageDiagnosed, bShortageDiagnosed ? TEXT("diagnosed") : TEXT("pending"));
		State(TEXT("Strategic.Status.RouteRecovery"), bDelivered, bDelivered ? TEXT("delivered") : TEXT("pending"));
		State(TEXT("Strategic.Status.Research"), bResearched, bResearched ? TEXT("effect-applied") : TEXT("pending"));
		State(TEXT("Strategic.Status.AI"), bAi, FString::Printf(TEXT("%d"), Host->GetMerchantAIDecisionHistory().Num()));
		State(TEXT("Strategic.Status.Victory"), bVictory, Scenario != nullptr ? Scenario->WinningVictoryId : FString());
		State(TEXT("BuildMenu.Status.BreadChain"), bBuilt, bBuilt ? TEXT("confirmed") : TEXT("pending"));
		State(TEXT("Market.Status.GrainDiagnosed"), bShortageDiagnosed, bShortageDiagnosed ? TEXT("diagnosed") : TEXT("pending"));
		State(TEXT("TradeRoute.Editor.Status.Delivered"), bDelivered, bDelivered ? TEXT("delivered") : TEXT("pending"));
		State(TEXT("Research.Status.MarketReports"), bResearched, bResearched ? TEXT("effect-applied") : TEXT("pending"));
		State(TEXT("HUD.Status.MerchantAI"), bAi, FString::Printf(TEXT("%d"), Host->GetMerchantAIDecisionHistory().Num()));
		State(TEXT("SaveLoad.Status.RoundTrip"), Fixture->HasVerifiedRoundTrip(), Fixture->HasVerifiedRoundTrip() ? TEXT("verified") : TEXT("pending"));
		State(TEXT("Scenario.Status.Victory"), bVictory, Scenario != nullptr ? Scenario->WinningVictoryId : FString());
		if (const FHansaSemanticNode* GrainNode = Registry->FindNode(TEXT("Market.Good.Grain")))
		{
			auto GrainState = GrainNode->State;
			GrainState.bWarning = bGrainDeficit;
			GrainState.ValueType = TEXT("market-good");
			GrainState.Value = Grain != nullptr
				? FString::Printf(TEXT("stock=%lld;reserve=%lld;price=%lld"),
					static_cast<long long>(Grain->CurrentStock.GetRawValue()),
					static_cast<long long>(Grain->DesiredReserve.GetRawValue()),
					static_cast<long long>(Grain->CurrentPriceMilliMarks))
				: TEXT("unavailable");
			Registry->UpdateNode(TEXT("Market.Good.Grain"), GrainState, GrainNode->Bounds);
		}
		for (const auto& Pair : SemanticWidgets) UpdateGeometry(Pair.Key, Pair.Value);
	}

	void SHansaStrategicAutomationScreen::UpdateGeometry(const FString& Id, const TSharedPtr<SWidget>& Widget)
	{
		const FHansaSemanticNode* Node = Registry->FindNode(Id); if (Node == nullptr || !Widget.IsValid()) return;
		const FGeometry& Geometry = Widget->GetCachedGeometry(); const FVector2f Position = Geometry.GetAbsolutePosition() - ScreenWidget->GetCachedGeometry().GetAbsolutePosition(); const FVector2f Size = Geometry.GetDrawSize();
		Registry->UpdateNode(Id, Node->State, {FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y), FMath::RoundToInt(Size.X), FMath::RoundToInt(Size.Y)});
	}

	void SHansaStrategicAutomationScreen::SetPresentationSize(const FIntPoint& Size) { PresentationBox->SetWidthOverride(Size.X); PresentationBox->SetHeightOverride(Size.Y); }
	TSharedRef<SWidget> SHansaStrategicAutomationScreen::GetCaptureWidget() const { return PresentationBox.ToSharedRef(); }

	FHansaStrategicAutomationScreenHost::FHansaStrategicAutomationScreenHost(FHansaStrategicAutomationFixture& InFixture, FHansaSemanticUiRegistry& InRegistry)
		: Fixture(InFixture), Registry(InRegistry) {}
	FHansaStrategicAutomationScreenHost::~FHansaStrategicAutomationScreenHost()
	{
		if (Window.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
	}
	bool FHansaStrategicAutomationScreenHost::EnsureScreen(const FIntPoint& ClientSize)
	{
		if (!Fixture.IsLoaded() || !FSlateApplication::IsInitialized()) return false;
		if (!Window.IsValid())
		{
			SAssignNew(Window, SWindow).Title(FText::FromString(TEXT("Hansa Strategic Automation"))).ClientSize(FVector2D(ClientSize.X, ClientSize.Y)).SizingRule(ESizingRule::FixedSize).SupportsMaximize(false).SupportsMinimize(false);
			SAssignNew(Screen, SHansaStrategicAutomationScreen, Fixture, Registry); Window->SetContent(Screen.ToSharedRef()); FSlateApplication::Get().AddWindow(Window.ToSharedRef(), true);
		}
		else if (CurrentClientSize != ClientSize) Window->Resize(FVector2D(ClientSize.X, ClientSize.Y));
		CurrentClientSize = ClientSize; Screen->SetPresentationSize(ClientSize); FSlateApplication::Get().ForceRedrawWindow(Window.ToSharedRef()); SynchronizeSemantics(); return true;
	}
	bool FHansaStrategicAutomationScreenHost::CaptureNative(const FIntPoint& Size, TArray<FColor>& Pixels)
	{
		if (!EnsureScreen(Size)) return false; FIntVector Captured; return FSlateApplication::Get().TakeScreenshot(Screen->GetCaptureWidget(), FIntRect(0, 0, Size.X, Size.Y), Pixels, Captured) && Captured.X == Size.X && Captured.Y == Size.Y && Pixels.Num() == Size.X * Size.Y;
	}
	void FHansaStrategicAutomationScreenHost::SynchronizeSemantics() { if (Screen.IsValid()) Screen->SynchronizeSemantics(); }
}
