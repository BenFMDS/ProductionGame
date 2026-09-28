#include "AchievementPluginEditorCommands.h"

#define LOCTEXT_NAMESPACE "FAchievementPluginEditorModule"

void FAchievementPluginEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Achievement Editor", "Open the Achievement Editor tab", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
