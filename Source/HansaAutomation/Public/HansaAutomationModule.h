#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

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

private:
	bool bTransportRequested = false;
};
