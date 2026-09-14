#include "HansaEditorModule.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Terrain/HansaCityTerrainToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Misc/CoreDelegates.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaStrategyPlayerController.h"

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

namespace
{
class FHansaPieEscapeInputProcessor final : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
	{
		if (KeyEvent.GetKey() != EKeys::Escape || KeyEvent.GetModifierKeys().AnyModifiersDown())
		{
			return false;
		}

		// Unreal Editor binds bare Escape to StopPlaySession. Consume it only while focus is
		// inside a Hansa PIE viewport so the game's normal Back/Escape stack gets first refusal.
		if (!GEditor || !GEditor->PlayWorld || !GEngine)
		{
			return false;
		}

		const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(KeyEvent.GetUserIndex());
		if (!FocusedWidget.IsValid())
		{
			return false;
		}

		FWidgetPath FocusPath;
		if (!SlateApp.GeneratePathToWidgetUnchecked(FocusedWidget.ToSharedRef(), FocusPath))
		{
			return false;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (WorldContext.WorldType != EWorldType::PIE || !World)
			{
				continue;
			}

			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(It->Get());
				if (!Controller || !Controller->IsLocalController())
				{
					continue;
				}

				const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
				const TSharedPtr<SViewport> GameViewport = LocalPlayer && LocalPlayer->ViewportClient
					? LocalPlayer->ViewportClient->GetGameViewportWidget()
					: nullptr;
				const AHansaRootHud* Hud = Cast<AHansaRootHud>(Controller->GetHUD());
				const TSharedPtr<Hansa::UI::SHansaRootHud> RootHud = Hud ? Hud->GetRootWidget() : nullptr;

				const bool bFocusInsideGame = (GameViewport.IsValid() && FocusPath.ContainsWidget(GameViewport.Get()))
					|| (RootHud.IsValid() && FocusPath.ContainsWidget(RootHud.Get()));
				if (!bFocusInsideGame)
				{
					continue;
				}

				if (!KeyEvent.IsRepeat())
				{
					Controller->HandleEscapeIntent();
				}
				return true;
			}
		}

		return false;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("Hansa PIE Escape");
	}
};
}

void FHansaEditorModule::StartupModule()
{
	if (FSlateApplication::IsInitialized())
	{
		PieEscapeInputProcessor = MakeShared<FHansaPieEscapeInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(PieEscapeInputProcessor, 0);
	}

	if (UToolsetRegistry::IsAvailable()) RegisterTerrainTools();
	else TerrainRegistrationHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FHansaEditorModule::RegisterTerrainTools);
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
	if (PieEscapeInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(PieEscapeInputProcessor);
	}
	PieEscapeInputProcessor.Reset();

	FCoreDelegates::OnPostEngineInit.Remove(TerrainRegistrationHandle);
	UToolsetRegistry::UnregisterToolsetClass(UHansaCityTerrainToolset::StaticClass());
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AuthoringStudioTabId);
}

void FHansaEditorModule::RegisterTerrainTools()
{
	UToolsetRegistry::RegisterToolsetClass(UHansaCityTerrainToolset::StaticClass());
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
