// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogueHelper.h"
#include "DialogueHelperStyle.h"
#include "DialogueHelperCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "SDialogueManager.h"

static const FName DialogueHelperTabName("DialogueHelper");

#define LOCTEXT_NAMESPACE "FDialogueHelperModule"

void FDialogueHelperModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FDialogueHelperStyle::Initialize();
	FDialogueHelperStyle::ReloadTextures();

	FDialogueHelperCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FDialogueHelperCommands::Get().OpenDialogueHelperWindow,
		FExecuteAction::CreateRaw(this, &FDialogueHelperModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDialogueHelperModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DialogueHelperTabName, FOnSpawnTab::CreateRaw(this, &FDialogueHelperModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FDialogueHelperTabTitle", "DialogueHelper"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FDialogueHelperModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FDialogueHelperStyle::Shutdown();

	FDialogueHelperCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DialogueHelperTabName);
}

TSharedRef<SDockTab> FDialogueHelperModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SDialogueManager)
		];
}

void FDialogueHelperModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(DialogueHelperTabName);
}

void FDialogueHelperModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FDialogueHelperCommands::Get().OpenDialogueHelperWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FDialogueHelperCommands::Get().OpenDialogueHelperWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FDialogueHelperModule, DialogueHelper)

