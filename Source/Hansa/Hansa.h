#pragma once

#include "HansaLog.h"
#include "Modules/ModuleInterface.h"

class HANSA_API FHansaModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
