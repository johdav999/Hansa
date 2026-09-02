#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHansaSimulation, Log, All);

class HANSASIMULATION_API FHansaSimulationModule final : public IModuleInterface
{
public:
	static FHansaSimulationModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FHansaSimulationModule>(TEXT("HansaSimulation"));
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("HansaSimulation"));
	}

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
