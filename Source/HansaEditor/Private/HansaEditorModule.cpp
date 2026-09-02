#include "HansaEditorModule.h"

#include "Modules/ModuleManager.h"

#if !WITH_EDITOR
#error HansaEditor must never be compiled without WITH_EDITOR.
#endif

DEFINE_LOG_CATEGORY(LogHansaEditor);

void FHansaEditorModule::StartupModule()
{
	UE_LOG(LogHansaEditor, Log, TEXT("HansaEditor module started."));
}

void FHansaEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FHansaEditorModule, HansaEditor)
