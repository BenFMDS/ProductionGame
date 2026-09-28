#pragma once

#include "Modules/ModuleManager.h"

class IDetailsView;
class UAchievementDatabase;
class UAchievementDefinitionAsset;
class UAchievementSaveGame;
class UToolMenu;
class UObject;

class FAchievementPluginEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void PluginButtonClicked();

private:
	void RegisterMenus();
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);
	void RefreshDatabaseFromSettings();
	void LoadDatabaseAsset();
	void BrowseToDatabaseAsset() const;
	void CreateDatabaseAsset();
	void CreateAchievementAsset();
	void RefreshAchievementList();
	void SetSelectedAchievement(UAchievementDefinitionAsset* AchievementAsset);
	void SetActiveDatabase(UAchievementDatabase* Database);
	void ResetSelectedAchievementProgress();
	void ResetAllAchievementProgress();
	FString GetPluginContentRoot() const;
	bool SaveObjectPackage(UObject* Object) const;
	UAchievementSaveGame* LoadEditorSaveGame() const;
	bool SaveEditorSaveGame(UAchievementSaveGame* SaveGameObject) const;

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<class SVerticalBox> AchievementListContainer;
	TWeakObjectPtr<UAchievementDatabase> ActiveDatabase;
	TWeakObjectPtr<UAchievementDefinitionAsset> SelectedAchievementAsset;
};
