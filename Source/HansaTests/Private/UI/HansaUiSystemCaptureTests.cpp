#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaUiComponents.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SViewport.h"

using namespace Hansa::UI;
namespace
{
class FUiSystemViewportCapture final:public IAutomationLatentCommand
{
public:
	explicit FUiSystemViewportCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
	bool Update() override
	{
		if(FPlatformTime::Seconds()-Start>60){Test->AddError(TEXT("P21 real viewport timed out"));Cleanup();return true;}
		if(!GEngine||!GEngine->GameViewport||!GEngine->GameViewport->GetWorld()||!GEngine->GameViewport->GetWorld()->HasBegunPlay())return false;
		UGameViewportClient* Viewport=GEngine->GameViewport.Get();
		if(!Gallery.IsValid())
		{
			FUiPreferences Prefs;
			Prefs.bHighContrast=FParse::Param(FCommandLine::Get(),TEXT("P21Accessible"));
			Prefs.bLargeText=Prefs.bHighContrast;Prefs.bReducedMotion=Prefs.bHighContrast;
			auto Rows=SNew(SVerticalBox);
			for(EUiState State:{EUiState::Default,EUiState::Selected,EUiState::Disabled,EUiState::Loading,EUiState::Warning,EUiState::Error})
			{
				TSharedPtr<SHansaAction> Action;
				Rows->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(Action,SHansaAction).Preferences(Prefs).State(State)
					.Label(FText::FromString(TEXT("Open production overview"))).Reason(FText::FromString(TEXT("Inspect the supplying chain.")))];
				if(State==EUiState::Selected) FocusAction=Action;
			}
			TArray<FUiChartSeries> Series={{EUiSeries::Price,{{0,.2},{.3,.7},{.65,.4},{1,.8}},FText::FromString(TEXT("Price")),false},
				{EUiSeries::Incoming,{{0,.1},{.5,.3},{1,.5}},FText::FromString(TEXT("Incoming (dashed)")),true}};
			Gallery=SNew(SOverlay)
			+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(24,96,24,24)
			[SNew(SBox).WidthOverride(360).MaxDesiredHeight(760)
			[SNew(SHansaSurface).Preferences(Prefs).Surface(EUiSurface::Panel).Title(FText::FromString(TEXT("P21 component review")))
			[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]]]
			+SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24,96,24,24)
			[SNew(SBox).WidthOverride(360)
			[SNew(SHansaSurface).Preferences(Prefs).Surface(EUiSurface::Panel).Title(FText::FromString(TEXT("Lübeck — production")))
			[SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()[SNew(SHansaSurface).Preferences(Prefs).Surface(EUiSurface::Notification)
			.State(EUiState::Warning).Reason(FText::FromString(TEXT("Bread reserve low. Add supply before stocks run out.")))]
			+SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(Prefs.bLargeText?220:150)[SNew(SHansaDiagram).Preferences(Prefs).Series(Series).Summary(FText::FromString(TEXT("Presentation fixture — normalized values")))]]
			+SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(72)[SNew(SHansaDiagram).Preferences(Prefs).Kind(EUiDiagram::Progress).Fraction(.6f).Summary(FText::FromString(TEXT("Construction: 60%")))]]
			+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth()[SNew(SHansaGlyph).Glyph(EUiGlyph::Warning)]
			+SHorizontalBox::Slot().AutoWidth()[SNew(SHansaGlyph).Glyph(EUiGlyph::Error)]
			+SHorizontalBox::Slot().AutoWidth()[SNew(SHansaGlyph).Glyph(EUiGlyph::Check)]
			+SHorizontalBox::Slot().AutoWidth()[SNew(SHansaGlyph).Glyph(EUiGlyph::Loading)]]]]];
			float Scale=1.f;FParse::Value(FCommandLine::Get(),TEXT("P21Scale="),Scale);
			Gallery=SNew(SDPIScaler).DPIScale(Scale)[SNew(SHansaScreenShell)[Gallery.ToSharedRef()]];
			Viewport->AddViewportWidgetContent(Gallery.ToSharedRef(),1000);
			FSlateApplication::Get().SetKeyboardFocus(FocusAction, EFocusCause::Navigation);
			AddedAt=FPlatformTime::Seconds();return false;
		}
		if(FPlatformTime::Seconds()-AddedAt<0.5)return false;
		TArray<FColor> Pixels;FIntVector Size;
		if(!FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size))
		{Test->AddError(TEXT("Real viewport readback failed"));Cleanup();return true;}
		int32 Width=1280,Height=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),Width);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Height);
		Test->TestEqual(TEXT("Native viewport width"),Size.X,Width);Test->TestEqual(TEXT("Native viewport height"),Size.Y,Height);
		TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
		float Scale=1.f;FParse::Value(FCommandLine::Get(),TEXT("P21Scale="),Scale);
		const FString Path=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P21/UI-system-%dx%d-%s-%dpercent.png"),Width,Height,
			FParse::Param(FCommandLine::Get(),TEXT("P21Accessible"))?TEXT("accessible"):TEXT("default"),FMath::RoundToInt(Scale*100));
		Test->TestTrue(TEXT("Native screenshot saved"),FFileHelper::SaveArrayToFile(Png,*Path));
		Test->AddInfo(Path);Cleanup();return true;
	}
private:
	void Cleanup(){if(Gallery.IsValid()&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(Gallery.ToSharedRef());Gallery.Reset();}
	FAutomationTestBase* Test;double Start;double AddedAt=0;TSharedPtr<SWidget> Gallery;TSharedPtr<SHansaAction> FocusAction;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaUiSystemViewportTest,"Hansa.UI.Style.RealViewport",
	EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaUiSystemViewportTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FUiSystemViewportCapture(this));return true;
}
#endif
