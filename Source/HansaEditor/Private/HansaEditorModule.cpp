#include "HansaEditorModule.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Studio/SHansaAuthoringStudio.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#if !WITH_EDITOR
#error HansaEditor must never be compiled without WITH_EDITOR.
#endif

DEFINE_LOG_CATEGORY(LogHansaEditor);
const FName FHansaEditorModule::AuthoringStudioTabId(TEXT("HansaAuthoringStudio"));

void FHansaEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AuthoringStudioTabId,
		FOnSpawnTab::CreateRaw(this, &FHansaEditorModule::SpawnAuthoringStudioTab))
		.SetDisplayName(NSLOCTEXT("HansaEditor", "AuthoringStudioTab", "Hansa Authoring Studio"))
		.SetTooltipText(NSLOCTEXT("HansaEditor", "AuthoringStudioTabTooltip", "Open the schema-driven Hansa definition authoring workspace."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHansaEditorModule::RegisterMenus));
	UE_LOG(LogHansaEditor, Log, TEXT("HansaEditor module started."));
}

void FHansaEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AuthoringStudioTabId);
}

TSharedRef<SDockTab> FHansaEditorModule::SpawnAuthoringStudioTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(NSLOCTEXT("HansaEditor", "AuthoringStudioTabLabel", "Hansa Authoring Studio"))
		[
			SNew(SHansaAuthoringStudio)
		];
}

void FHansaEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("WindowLayout"));
	Section.AddMenuEntry(
		TEXT("HansaAuthoringStudio"),
		NSLOCTEXT("HansaEditor", "OpenAuthoringStudio", "Hansa Authoring Studio"),
		NSLOCTEXT("HansaEditor", "OpenAuthoringStudioTooltip", "Open the schema-driven Hansa definition authoring workspace."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Edit")),
		FUIAction(FExecuteAction::CreateLambda([]
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FHansaEditorModule::AuthoringStudioTabId);
		})));
}

IMPLEMENT_MODULE(FHansaEditorModule, HansaEditor)
