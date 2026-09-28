// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "DialogueHelperStyle.h"

class FDialogueHelperCommands : public TCommands<FDialogueHelperCommands>
{
public:

	FDialogueHelperCommands()
		: TCommands<FDialogueHelperCommands>(TEXT("DialogueHelper"), NSLOCTEXT("Contexts", "DialogueHelper", "DialogueHelper Plugin"), NAME_None, FDialogueHelperStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenDialogueHelperWindow;
};
