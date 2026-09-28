// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogueHelperStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FDialogueHelperStyle::StyleInstance = nullptr;

void FDialogueHelperStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FDialogueHelperStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FDialogueHelperStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("DialogueHelperStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FDialogueHelperStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("DialogueHelperStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("DialogueHelper")->GetBaseDir() / TEXT("Resources"));

	Style->Set("DialogueHelper.OpenDialogueHelperWindow", new IMAGE_BRUSH(TEXT("Dialogue"), Icon20x20));
	Style->Set("DialogueHelper.OpenDialogueHelperWindow.Small", new IMAGE_BRUSH(TEXT("Dialogue"), Icon20x20));

	return Style;
}

void FDialogueHelperStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FDialogueHelperStyle::Get()
{
	return *StyleInstance;
}
