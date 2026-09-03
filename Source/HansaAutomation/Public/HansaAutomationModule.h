#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"

namespace Hansa::Automation
{
	class FHansaAutomationNamedPipeEndpoint;
	class FHansaAutomationSessionService;
}

DECLARE_LOG_CATEGORY_EXTERN(LogHansaAutomation, Log, All);

class HANSAAUTOMATION_API FHansaAutomationModule final : public IModuleInterface
{
public:
	static FHansaAutomationModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FHansaAutomationModule>(TEXT("HansaAutomation"));
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("HansaAutomation"));
	}

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsTransportRequested() const
	{
		return bTransportRequested;
	}

	bool IsSessionBoundaryEnabled() const;
	Hansa::Automation::FHansaAutomationSessionService& GetSessionService();
	const Hansa::Automation::FHansaAutomationSessionService& GetSessionService() const;

private:
	bool bTransportRequested = false;
	TUniquePtr<Hansa::Automation::FHansaAutomationNamedPipeEndpoint> NamedPipeEndpoint;
	TUniquePtr<Hansa::Automation::FHansaAutomationSessionService> SessionService;
};
