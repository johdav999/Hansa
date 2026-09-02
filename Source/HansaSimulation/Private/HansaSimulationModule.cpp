#include "HansaSimulationModule.h"

#include "Modules/ModuleManager.h"

#if UE_BUILD_SHIPPING && WITH_HANSA_AUTOMATION
#error WITH_HANSA_AUTOMATION must be 0 in Shipping runtime modules.
#endif

DEFINE_LOG_CATEGORY(LogHansaSimulation);

void FHansaSimulationModule::StartupModule()
{
	UE_LOG(LogHansaSimulation, Log, TEXT("HansaSimulation module started."));
}

void FHansaSimulationModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FHansaSimulationModule, HansaSimulation)
