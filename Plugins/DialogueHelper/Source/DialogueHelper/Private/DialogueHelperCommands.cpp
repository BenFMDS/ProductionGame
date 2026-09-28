// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogueHelperCommands.h"

#define LOCTEXT_NAMESPACE "FDialogueHelperModule"

void FDialogueHelperCommands::RegisterCommands()
{
	UI_COMMAND(OpenDialogueHelperWindow, "DialogueHelper", "Bring up DialogueHelper window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

