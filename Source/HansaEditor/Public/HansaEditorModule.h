#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHansaEditor, Log, All);

class HANSAEDITOR_API FHansaEditorModule final : public IModuleInterface
{
public:
	static const FName AuthoringStudioTabId;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> SpawnAuthoringStudioTab(const class FSpawnTabArgs& SpawnTabArgs);
	void RegisterMenus();
};
