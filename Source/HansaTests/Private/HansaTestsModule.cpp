#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#if UE_BUILD_SHIPPING
#error HansaTests must never be compiled into a Shipping target.
#endif

class FHansaTestsModule final : public IModuleInterface
{
};

IMPLEMENT_MODULE(FHansaTestsModule, HansaTests)
