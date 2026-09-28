#pragma once

#include "AchievementPluginEditorStyle.h"
#include "Framework/Commands/Commands.h"

class FAchievementPluginEditorCommands : public TCommands<FAchievementPluginEditorCommands>
{
public:
	FAchievementPluginEditorCommands()
		: TCommands<FAchievementPluginEditorCommands>(
			TEXT("AchievementPluginEditor"),
			NSLOCTEXT("Contexts", "AchievementPluginEditor", "Achievement Plugin Editor"),
			NAME_None,
			FAchievementPluginEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> OpenPluginWindow;
};
