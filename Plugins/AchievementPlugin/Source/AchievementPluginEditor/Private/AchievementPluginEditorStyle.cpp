#include "AchievementPluginEditorStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FAchievementPluginEditorStyle::StyleInstance = nullptr;

void FAchievementPluginEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAchievementPluginEditorStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

FName FAchievementPluginEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AchievementPluginEditorStyle"));
	return StyleSetName;
}

const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef<FSlateStyleSet> FAchievementPluginEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style =
		MakeShareable(new FSlateStyleSet("AchievementPluginEditorStyle"));

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("AchievementPlugin");

	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	Style->Set(
		"AchievementPluginEditor.OpenPluginWindow",
		new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20)
	);

	return Style;
}

void FAchievementPluginEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FAchievementPluginEditorStyle::Get()
{
	return *StyleInstance;
}
