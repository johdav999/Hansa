#include "HansaAutomationModule.h"

#include "HAL/Platform.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

#if UE_BUILD_SHIPPING
#error HansaAutomation must never be compiled into a Shipping target.
#endif

#if !WITH_HANSA_AUTOMATION
#error HansaAutomation requires WITH_HANSA_AUTOMATION=1 in approved development targets.
#endif

DEFINE_LOG_CATEGORY(LogHansaAutomation);

namespace Hansa::Automation
{
	constexpr TCHAR ConfigSection[] = TEXT("Hansa.Automation");
	constexpr TCHAR EnableTransportKey[] = TEXT("bEnableTransport");
	constexpr TCHAR EnableCommandLineFlag[] = TEXT("HansaAutomation");
}

void FHansaAutomationModule::StartupModule()
{
	bool bConfigEnabled = false;
	if (GConfig != nullptr)
	{
		GConfig->GetBool(
			Hansa::Automation::ConfigSection,
			Hansa::Automation::EnableTransportKey,
			bConfigEnabled,
			GGameIni);
	}

	const bool bCommandLineEnabled = FParse::Param(
		FCommandLine::Get(),
		Hansa::Automation::EnableCommandLineFlag);

	bTransportRequested = bConfigEnabled || bCommandLineEnabled;

	if (bTransportRequested)
	{
		UE_LOG(
			LogHansaAutomation,
			Display,
			TEXT("Hansa automation was explicitly requested. S00-P02 provides no transport endpoint, so no pipe or socket was opened."));
	}
	else
	{
		UE_LOG(
			LogHansaAutomation,
			Verbose,
			TEXT("Hansa automation transport is disabled. Use -HansaAutomation or a local non-Shipping config override to request it."));
	}
}

void FHansaAutomationModule::ShutdownModule()
{
	bTransportRequested = false;
}

IMPLEMENT_MODULE(FHansaAutomationModule, HansaAutomation)
