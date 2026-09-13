#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaFrontendPresentationModel.h"
namespace {
class FDisplayRollback:public IAutomationLatentCommand{
 FAutomationTestBase* Test;int32 Stage=0;EWindowMode::Type Original=EWindowMode::Windowed;double Start=FPlatformTime::Seconds(),Ready=0;
public:explicit FDisplayRollback(FAutomationTestBase* T):Test(T){}
 bool Update()override{
  if(FPlatformTime::Seconds()-Start>45){Test->AddError(TEXT("Display rollback timeout"));return true;}
  auto* W=GEngine&&GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;auto* C=W?W->GetFirstPlayerController():nullptr;auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!H||!H->GetFrontendPresentationModel())return false;
  auto* M=H->GetFrontendPresentationModel();auto* G=GEngine->GetGameUserSettings();
  if(Stage==0){Original=G->GetFullscreenMode();M->OpenSettings();M->ChangeSetting(TEXT("Window"));Test->TestTrue(TEXT("Display change asks for confirmation"),M->GetSnapshot().Page==EHansaFrontendPage::Confirmation);Ready=FPlatformTime::Seconds();++Stage;return false;}
  if(Stage==1){if(FPlatformTime::Seconds()-Ready<1)return false;M->Back();Test->TestEqual(TEXT("Cancel restores original engine mode"),int32(G->GetFullscreenMode()),int32(Original));M->ChangeSetting(TEXT("Window"));Ready=FPlatformTime::Seconds();++Stage;return false;}
  if(FPlatformTime::Seconds()-Ready<16)return false;
  Test->TestEqual(TEXT("Timeout restores original engine mode"),int32(G->GetFullscreenMode()),int32(Original));Test->TestTrue(TEXT("Timeout returns to settings"),M->GetSnapshot().Page==EHansaFrontendPage::Settings);M->Back();Test->TestTrue(TEXT("Settings back returns to title"),M->GetSnapshot().Page==EHansaFrontendPage::Title);return true;
 }
};}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplayRollbackTest,"Hansa.UI.Frontend.DisplayRollbackViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FDisplayRollbackTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FDisplayRollback(this));return true;}
#endif
