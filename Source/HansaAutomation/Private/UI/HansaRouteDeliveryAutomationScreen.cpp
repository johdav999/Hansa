#include "UI/HansaRouteDeliveryAutomationScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "Gameplay/HansaProductionFixtureService.h"
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
		const FLinearColor Oxblood = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oxblood);
		const FLinearColor Teal = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal);
		const FLinearColor Chalk = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk);

		TSharedRef<SWidget> Button(TSharedPtr<SWidget>& Out, const FString& Label, TFunction<bool()> Intent)
		{
			TSharedPtr<SButton> Value;
			SAssignNew(Value, SButton).Text(FText::FromString(Label)).ContentPadding(FMargin(16, 10))
				.ButtonColorAndOpacity(Parchment).ForegroundColor(Ink)
				.OnClicked_Lambda([Intent = MoveTemp(Intent)] { Intent(); return FReply::Handled(); });
			Out = Value;
			return Value.ToSharedRef();
		}

		TSharedRef<FJsonObject> Command(const TCHAR* Name)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("command"), Name);
			Json->SetNumberField(TEXT("routeId"), 1);
			return Json;
		}
	}

	void SHansaRouteDeliveryAutomationScreen::Construct(const FArguments&,
		FHansaProductionFixtureService& InService, FHansaSemanticUiRegistry& InRegistry)
	{
		Service = &InService;
		Registry = &InRegistry;
		RegisterSemantics();
		TSharedPtr<SWidget> Root, RouteTab, MarketTab, Save, Start, Cancel;
		ChildSlot[SAssignNew(PresentationBox, SBox).WidthOverride(1280).HeightOverride(720)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox).WidthOverride(1280).HeightOverride(720)
				[
					SAssignNew(ScreenWidget, SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
					.BorderBackgroundColor(Navy).Padding(24)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("LÜBECK SHORTAGE RELIEF  ·  ROUTE DELIVERY"))).ColorAndOpacity(Chalk).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8)[Button(RouteTab, TEXT("ROUTE EDITOR"), [this]{ return ShowRouteIntent(); })]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8)[Button(MarketTab, TEXT("MARKET EFFECT"), [this]{ return ShowMarketIntent(); })]
						]
						+ SVerticalBox::Slot().FillHeight(1).Padding(0, 18)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1)
							[
								SAssignNew(RoutePanel, SBorder).Visibility_Lambda([this]{ return bShowingMarket ? EVisibility::Collapsed : EVisibility::Visible; })
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Harbor).Padding(26)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("COG 1  ·  BALTIC SEA  ·  LÜBECK ⇄ ROSTOCK"))).ColorAndOpacity(Chalk).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20))]
									+ SVerticalBox::Slot().AutoHeight().Padding(0,18)[SNew(STextBlock).Text(FText::FromString(TEXT("STOP 1  Lübeck   UNLOAD Grain ≤ 20,000\nSTOP 2  Rostock  LOAD Grain ≤ 20,000   KEEP 30,000 RESERVE\n\nRound trip 20 travel ticks  ·  Capacity 60,000  ·  Upkeep 12 pf/tick\nExpected Lübeck arrival tick 24  ·  Delivery action tick 25"))).ColorAndOpacity(Chalk)]
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(RouteStatusText, STextBlock).ColorAndOpacity(Brass).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17))]
									+ SVerticalBox::Slot().AutoHeight().Padding(0,12)[SAssignNew(CargoText, STextBlock).ColorAndOpacity(Chalk)]
									+ SVerticalBox::Slot().FillHeight(1)
									+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(5)[Button(Save, TEXT("SAVE RELIEF ROUTE"), [this]{ return SaveRouteIntent(); })]
										+ SHorizontalBox::Slot().AutoWidth().Padding(5)[Button(Start, TEXT("START ROUTE"), [this]{ return StartRouteIntent(); })]
										+ SHorizontalBox::Slot().AutoWidth().Padding(5)[Button(Cancel, TEXT("CANCEL ROUTE"), [this]{ return CancelRouteIntent(); })]]
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1)
							[
								SAssignNew(MarketPanel, SBorder).Visibility_Lambda([this]{ return bShowingMarket ? EVisibility::Visible : EVisibility::Collapsed; })
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Linen).Padding(28)
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("LÜBECK GRAIN MARKET"))).ColorAndOpacity(Ink).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))]
									+ SVerticalBox::Slot().AutoHeight().Padding(0,20)[SAssignNew(MarketText, STextBlock).ColorAndOpacity(Oxblood).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))]
									+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("Price updates on the five-tick market cadence. Remote reports may be stale even while authoritative route cargo continues to move."))).ColorAndOpacity(Ink).AutoWrapText(true)]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(HashText, STextBlock).ColorAndOpacity(Parchment)]
					]
				]
			]
		]];
		Root = ScreenWidget;
		SemanticWidgets = {{TEXT("RouteDelivery.Root"),Root},{TEXT("RouteDelivery.Tab.Route"),RouteTab},{TEXT("RouteDelivery.Tab.Market"),MarketTab},
			{TEXT("RouteDelivery.RouteEditor"),RoutePanel},{TEXT("RouteDelivery.Market"),MarketPanel},{TEXT("RouteEditor.Action.Save"),Save},
			{TEXT("RouteEditor.Action.Start"),Start},{TEXT("RouteEditor.Action.Cancel"),Cancel}};
		SynchronizeSemantics();
	}

	void SHansaRouteDeliveryAutomationScreen::RegisterSemantics()
	{
		Registry->Reset();
		auto Add = [this](const TCHAR* Id, EHansaSemanticRole Role, const TCHAR* Label, const TCHAR* Parent,
			TFunction<bool()> Activate = {})
		{
			FHansaSemanticNode Node; Node.Id=Id; Node.Role=Role; Node.Label=Label; Node.ParentId=Parent;
			FHansaSemanticActionHandlers Handlers;
			if (Activate) { Node.Actions.Add(EHansaSemanticAction::Activate); Handlers.Activate=MoveTemp(Activate); }
			Registry->RegisterNode(MoveTemp(Node), MoveTemp(Handlers));
		};
		Add(TEXT("RouteDelivery.Root"), EHansaSemanticRole::Screen, TEXT("Route delivery evidence"), TEXT(""));
		Add(TEXT("RouteDelivery.Tab.Route"), EHansaSemanticRole::Button, TEXT("Route editor"), TEXT("RouteDelivery.Root"), [this]{return ShowRouteIntent();});
		Add(TEXT("RouteDelivery.Tab.Market"), EHansaSemanticRole::Button, TEXT("Market effect"), TEXT("RouteDelivery.Root"), [this]{return ShowMarketIntent();});
		Add(TEXT("RouteDelivery.RouteEditor"), EHansaSemanticRole::Panel, TEXT("Relief route editor"), TEXT("RouteDelivery.Root"));
		Add(TEXT("RouteEditor.Action.Save"), EHansaSemanticRole::Button, TEXT("Save relief route"), TEXT("RouteDelivery.RouteEditor"), [this]{return SaveRouteIntent();});
		Add(TEXT("RouteEditor.Action.Start"), EHansaSemanticRole::Button, TEXT("Start route"), TEXT("RouteDelivery.RouteEditor"), [this]{return StartRouteIntent();});
		Add(TEXT("RouteEditor.Action.Cancel"), EHansaSemanticRole::Button, TEXT("Cancel route"), TEXT("RouteDelivery.RouteEditor"), [this]{return CancelRouteIntent();});
		Add(TEXT("RouteDelivery.Status.Departed"), EHansaSemanticRole::Status, TEXT("Route departed"), TEXT("RouteDelivery.Root"));
		Add(TEXT("RouteDelivery.Status.Arrived"), EHansaSemanticRole::Status, TEXT("Route arrived"), TEXT("RouteDelivery.Root"));
		Add(TEXT("RouteDelivery.Status.Delivered"), EHansaSemanticRole::Status, TEXT("Cargo delivered"), TEXT("RouteDelivery.Root"));
		Add(TEXT("RouteDelivery.Cargo"), EHansaSemanticRole::Status, TEXT("Cog cargo"), TEXT("RouteDelivery.Root"));
		Add(TEXT("RouteDelivery.Market"), EHansaSemanticRole::Panel, TEXT("Lubeck grain market"), TEXT("RouteDelivery.Root"));
		Add(TEXT("RouteDelivery.Market.State"), EHansaSemanticRole::Status, TEXT("Reserve and price response"), TEXT("RouteDelivery.Market"));
	}

	bool SHansaRouteDeliveryAutomationScreen::ShowRouteIntent(){bShowingMarket=false;SynchronizeSemantics();return true;}
	bool SHansaRouteDeliveryAutomationScreen::ShowMarketIntent(){bShowingMarket=true;SynchronizeSemantics();return true;}
	bool SHansaRouteDeliveryAutomationScreen::SaveRouteIntent()
	{
		TSharedRef<FJsonObject> Request=Command(TEXT("route.edit")); Request->SetStringField(TEXT("sourceCityId"),TEXT("City.Lubeck")); Request->SetStringField(TEXT("destinationCityId"),TEXT("City.Rostock")); Request->SetStringField(TEXT("goodId"),TEXT("Good.Grain")); Request->SetNumberField(TEXT("quantityMilliUnits"),20000); Request->SetNumberField(TEXT("minimumReserveMilliUnits"),30000);
		TSharedRef<FJsonObject> Result=MakeShared<FJsonObject>(); FString Error; const bool bOk=Service->Command(Request,Result,Error); SynchronizeSemantics(); return bOk;
	}
	bool SHansaRouteDeliveryAutomationScreen::StartRouteIntent(){TSharedRef<FJsonObject> R=Command(TEXT("route.set_active"));R->SetBoolField(TEXT("active"),true);TSharedRef<FJsonObject> O=MakeShared<FJsonObject>();FString E;const bool b=Service->Command(R,O,E);SynchronizeSemantics();return b;}
	bool SHansaRouteDeliveryAutomationScreen::CancelRouteIntent(){TSharedRef<FJsonObject> R=Command(TEXT("route.cancel"));TSharedRef<FJsonObject> O=MakeShared<FJsonObject>();FString E;const bool b=Service->Command(R,O,E);SynchronizeSemantics();return b;}

	void SHansaRouteDeliveryAutomationScreen::RefreshText()
	{
		const auto* Fixture=Service->GetFixture(); if(!Fixture)return; const auto Projection=Fixture->BuildProjection(); if(!Projection)return;
		const auto* Route=Projection.Value.GetRoutes().FindByPredicate([](const auto& V){return V.Id.GetValue()==1;});
		const auto* Vehicle=Projection.Value.GetVehicles().FindByPredicate([](const auto& V){return V.Id.GetValue()==1;});
		const auto Lubeck=Hansa::Simulation::FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")); const auto Grain=Hansa::Simulation::FHansaGoodId::TryParse(TEXT("Good.Grain"));
		const auto Market=Lubeck&&Grain?Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryMarket(Lubeck.Value,Grain.Value):TOptional<Hansa::Simulation::FHansaCityMarketProjection>();
		if(Route)RouteStatusText->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  leg %lld  ·  %d ticks remaining"),Hansa::Simulation::LexToString(Route->Lifecycle),static_cast<long long>(Route->CompletedLegCount),Route->RemainingTravelTicks)));
		if(Vehicle)CargoText->SetText(FText::FromString(FString::Printf(TEXT("Cargo %lld / %lld milli-units  ·  Current city %s"),static_cast<long long>(Vehicle->Cargo.GetRawValue()),static_cast<long long>(Vehicle->Capacity.GetRawValue()),*Vehicle->CurrentCityId.ToString())));
		if(Market)MarketText->SetText(FText::FromString(FString::Printf(TEXT("Stock %lld / reserve %lld\nPrice %lld milli-marks\nReport age %lld ticks  ·  %s"),static_cast<long long>(Market->CurrentStock.GetRawValue()),static_cast<long long>(Market->DesiredReserve.GetRawValue()),static_cast<long long>(Market->CurrentPriceMilliMarks),static_cast<long long>(Market->ReportAgeTicks),Market->bIsStale?TEXT("STALE REPORT"):TEXT("CURRENT REPORT"))));
		HashText->SetText(FText::FromString(FString::Printf(TEXT("Tick %lld  ·  State %016llX  ·  Events %d"),static_cast<long long>(Projection.Value.GetClock().GetTick().GetValue()),static_cast<unsigned long long>(Fixture->BuildStateHashes().GetOverallHash()),Fixture->GetEvents().Num())));
	}

	void SHansaRouteDeliveryAutomationScreen::SynchronizeSemantics()
	{
		if(!Service->IsRouteDeliveryLoaded())return; RefreshText(); const auto* Fixture=Service->GetFixture();
		bool bDeparted=false,bArrived=false,bDelivered=false; int64 Cargo=0; FString MarketValue;
		for(const auto& Event:Fixture->GetEvents()){bDeparted|=Event.GetType()==Hansa::Simulation::EHansaDomainEventType::RouteDeparted;bArrived|=Event.GetType()==Hansa::Simulation::EHansaDomainEventType::RouteArrived;bDelivered|=(Event.GetType()==Hansa::Simulation::EHansaDomainEventType::RouteCargoTransferred||Event.GetType()==Hansa::Simulation::EHansaDomainEventType::RouteCargoMissed)&&Event.GetRouteCargoActionKind()==Hansa::Simulation::EHansaRouteCargoActionKind::Unload&&Event.GetValue()>0;}
		const auto P=Fixture->BuildProjection(); if(P){const auto* V=P.Value.GetVehicles().FindByPredicate([](const auto& X){return X.Id.GetValue()==1;});if(V)Cargo=V->Cargo.GetRawValue();const auto C=Hansa::Simulation::FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));const auto G=Hansa::Simulation::FHansaGoodId::TryParse(TEXT("Good.Grain"));const auto M=Fixture->GetState().CreateReadOnlyAccess(Fixture->GetDefinitions()).QueryMarket(C.Value,G.Value);if(M)MarketValue=FString::Printf(TEXT("stock=%lld;reserve=%lld;price=%lld;stale=%s"),static_cast<long long>(M->CurrentStock.GetRawValue()),static_cast<long long>(M->DesiredReserve.GetRawValue()),static_cast<long long>(M->CurrentPriceMilliMarks),M->bIsStale?TEXT("true"):TEXT("false"));}
		auto State=[this](const TCHAR* Id,bool Selected,const FString& Type,const FString& Value,bool Visible=true){const auto* N=Registry->FindNode(Id);if(!N)return;auto S=N->State;S.bSelected=Selected;S.bVisible=Visible;S.ValueType=Type;S.Value=Value;Registry->UpdateNode(Id,S,N->Bounds);};
		State(TEXT("RouteDelivery.Tab.Route"),!bShowingMarket,TEXT("view"),TEXT("route")); State(TEXT("RouteDelivery.Tab.Market"),bShowingMarket,TEXT("view"),TEXT("market"));
		State(TEXT("RouteDelivery.RouteEditor"),!bShowingMarket,TEXT("panel"),TEXT("route-editor"),!bShowingMarket); State(TEXT("RouteDelivery.Market"),bShowingMarket,TEXT("panel"),TEXT("market"),bShowingMarket);
		State(TEXT("RouteDelivery.Status.Departed"),bDeparted,TEXT("boolean"),bDeparted?TEXT("true"):TEXT("false")); State(TEXT("RouteDelivery.Status.Arrived"),bArrived,TEXT("boolean"),bArrived?TEXT("true"):TEXT("false")); State(TEXT("RouteDelivery.Status.Delivered"),bDelivered,TEXT("boolean"),bDelivered?TEXT("true"):TEXT("false"));
		State(TEXT("RouteDelivery.Cargo"),Cargo>0,TEXT("milli-units"),FString::Printf(TEXT("%lld"),static_cast<long long>(Cargo))); State(TEXT("RouteDelivery.Market.State"),bDelivered,TEXT("market-response"),MarketValue);
		for(const auto& Pair:SemanticWidgets)UpdateGeometry(Pair.Key,Pair.Value);
	}

	void SHansaRouteDeliveryAutomationScreen::UpdateGeometry(const FString& Id,const TSharedPtr<SWidget>& Widget){const auto* N=Registry->FindNode(Id);if(!N||!Widget.IsValid())return;const auto& G=Widget->GetCachedGeometry();const FVector2f P=G.GetAbsolutePosition()-ScreenWidget->GetCachedGeometry().GetAbsolutePosition();const FVector2f S=G.GetDrawSize();Registry->UpdateNode(Id,N->State,{FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(S.X),FMath::RoundToInt(S.Y)});}
	void SHansaRouteDeliveryAutomationScreen::SetPresentationSize(const FIntPoint& Size){PresentationBox->SetWidthOverride(Size.X);PresentationBox->SetHeightOverride(Size.Y);}
	TSharedRef<SWidget> SHansaRouteDeliveryAutomationScreen::GetCaptureWidget() const{return PresentationBox.ToSharedRef();}

	FHansaRouteDeliveryAutomationScreenHost::FHansaRouteDeliveryAutomationScreenHost(FHansaProductionFixtureService& S,FHansaSemanticUiRegistry& R):Service(S),Registry(R){}
	FHansaRouteDeliveryAutomationScreenHost::~FHansaRouteDeliveryAutomationScreenHost(){if(Window.IsValid()&&FSlateApplication::IsInitialized())FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());}
	bool FHansaRouteDeliveryAutomationScreenHost::EnsureScreen(const FIntPoint& Size){if(!Service.IsRouteDeliveryLoaded()||!FSlateApplication::IsInitialized())return false;if(!Window.IsValid()){SAssignNew(Window,SWindow).Title(FText::FromString(TEXT("Hansa Route Delivery Automation"))).ClientSize(FVector2D(Size.X,Size.Y)).SizingRule(ESizingRule::FixedSize).SupportsMaximize(false).SupportsMinimize(false);SAssignNew(Screen,SHansaRouteDeliveryAutomationScreen,Service,Registry);Window->SetContent(Screen.ToSharedRef());FSlateApplication::Get().AddWindow(Window.ToSharedRef(),true);}else if(CurrentClientSize!=Size)Window->Resize(FVector2D(Size.X,Size.Y));CurrentClientSize=Size;Screen->SetPresentationSize(Size);FSlateApplication::Get().ForceRedrawWindow(Window.ToSharedRef());SynchronizeSemantics();return true;}
	bool FHansaRouteDeliveryAutomationScreenHost::CaptureNative(const FIntPoint& Size,TArray<FColor>& Pixels){if(!EnsureScreen(Size))return false;FIntVector Captured;return FSlateApplication::Get().TakeScreenshot(Screen->GetCaptureWidget(),FIntRect(0,0,Size.X,Size.Y),Pixels,Captured)&&Captured.X==Size.X&&Captured.Y==Size.Y&&Pixels.Num()==Size.X*Size.Y;}
	void FHansaRouteDeliveryAutomationScreenHost::SynchronizeSemantics(){if(Screen.IsValid())Screen->SynchronizeSemantics();}
}
