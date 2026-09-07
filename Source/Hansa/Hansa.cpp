#include "Hansa.h"
#include "Modules/ModuleManager.h"
#include "UI/HansaUiStyle.h"

#if UE_BUILD_SHIPPING && WITH_HANSA_AUTOMATION
#error WITH_HANSA_AUTOMATION must be 0 in Shipping runtime modules.
#endif

DEFINE_LOG_CATEGORY(LogHansa);

void FHansaModule::StartupModule()
{
	FHansaUiStyle::Initialize();
}

void FHansaModule::ShutdownModule()
{
	FHansaUiStyle::Shutdown();
}

IMPLEMENT_PRIMARY_GAME_MODULE(FHansaModule, Hansa, "Hansa");
