Scythe is only for UE 5.5 and newer, with no plans to support versions below that.

Getting Started Guide
https://ScytheEditor.com/guide/

HOW TO INSTALL

You have 2 options.

A. Install into Engine: Extract the zip into YourUnrealEngine55Location\Engine\Plugins\Marketplace\ folder. If the Marketplace doesn't exist, create it. Doing it this way will enable it for use in all your projects, and also allows you to use DebugGame Editor if you use C++. Unsure if there's any limitations doing it this way at this time.

B. Install into Project: Extract the zip into your UE project in the \YourProject\Plugins\ folder.

Once you're in your Unreal project, enable Scythe in your plugins.

An optional, but recommended additional change is to edit your .uproject file and make sure Scythe's block looks like this under plugins:
{
	"Name": "Scythe",
	"Enabled": true,
	"TargetAllowList": [
	  "Editor"
	],
	"TargetConfigurationAllowList": [
	  "Development"
	]
},

Which will eliminate a lot of strange compilation/debug behavior if you run into them.

WHERE IS SOURCE CODE?

Source code isn't included in these Patreon builds while I develop it. Source code will be released on Fab when officially released.


IF YOU USE A UE SOURCE ENGINE BUILD

Since these are pre-compiled binaries for Win64 and you don't have access to the Scythe soure code, you have to do some extra things to get it working. See this link (https://www.gradientspace.com/uetoolbox-installation) and and follow the instructions in the "Binary Plugin, Source Engine Build" section, replacing GradientspaceUEToolbox with Scythe, of course.

tldr: Open Engine\Binaries\Win64\UnrealEditor.modules, then open Scythe\Binaries\Win64\UnrealEditor.modules
Replace the BuildID in the latter with the one in the former


SCYTHE DISCORD

Be sure to join the official Scythe discord: discord.gg/7mgtspVcCg

SPECIAL THANKS
Ryan Schmidt for all his help with answering my questions about Dynamic Meshes and the tools he originally built.
My partner and biggest supporter, Angela.