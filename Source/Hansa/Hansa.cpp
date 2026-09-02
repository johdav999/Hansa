#include "Hansa.h"
#include "Modules/ModuleManager.h"

#if UE_BUILD_SHIPPING && WITH_HANSA_AUTOMATION
#error WITH_HANSA_AUTOMATION must be 0 in Shipping runtime modules.
#endif

DEFINE_LOG_CATEGORY(LogHansa);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Hansa, "Hansa");
