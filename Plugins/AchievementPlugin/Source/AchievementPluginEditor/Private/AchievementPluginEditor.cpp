#include "AchievementPluginEditor.h"

#include "AchievementDatabase.h"
#include "AchievementDefinitionAsset.h"
#include "AchievementPluginEditorCommands.h"
#include "AchievementPluginEditorStyle.h"
#include "AchievementPluginSettings.h"
#include "AchievementSaveGame.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Kismet/GameplayStatics.h"

static const FName AchievementPluginEditorTabName("AchievementPluginEditor");

#define LOCTEXT_NAMESPACE "FAchievementPluginEditorModule"

void FAchievementPluginEditorModule::StartupModule()
{
#if WITH_EDITOR

	if (IsRunningCommandlet())
	{
		return;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FAchievementPluginEditorStyle::Initialize();
	FAchievementPluginEditorStyle::ReloadTextures();

	FAchievementPluginEditorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FAchievementPluginEditorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FAchievementPluginEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bHideSelectionTip = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.NotifyHook = nullptr;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(
			this,
			&FAchievementPluginEditorModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		AchievementPluginEditorTabName,
		FOnSpawnTab::CreateRaw(
			this,
			&FAchievementPluginEditorModule::OnSpawnPluginTab))
		.SetDisplayName(
			LOCTEXT("AchievementPluginEditorTabTitle", "Achievement Editor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	RefreshDatabaseFromSettings();

#endif
}

void FAchievementPluginEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FAchievementPluginEditorStyle::Shutdown();
	FAchievementPluginEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AchievementPluginEditorTabName);
}

void FAchievementPluginEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(AchievementPluginEditorTabName);
}

void FAchievementPluginEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntryWithCommandList(FAchievementPluginEditorCommands::Get().OpenPluginWindow, PluginCommands);
	}

	if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar"))
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FAchievementPluginEditorCommands::Get().OpenPluginWindow));
		Entry.SetCommandList(PluginCommands);
	}
}

TSharedRef<SDockTab> FAchievementPluginEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (ActiveDatabase.IsValid())
						{
							return FText::Format(
								LOCTEXT("EditingDatabaseText", "Database: {0}"),
								FText::FromString(ActiveDatabase->GetPathName()));
						}

						return LOCTEXT("NoDatabaseText", "Create or load an achievement database to start editing assets.");
					})
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("CreateDatabaseButton", "Create Database"))
						.OnClicked_Lambda([this]()
						{
							CreateDatabaseAsset();
							return FReply::Handled();
						})
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("LoadDatabaseButton", "Load Database"))
						.OnClicked_Lambda([this]()
						{
							LoadDatabaseAsset();
							return FReply::Handled();
						})
					]
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("CreateAchievementButton", "Create Achievement"))
						.IsEnabled_Lambda([this]()
						{
							return ActiveDatabase.IsValid();
						})
						.OnClicked_Lambda([this]()
						{
							CreateAchievementAsset();
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("BrowseButton", "Browse To Database"))
						.IsEnabled_Lambda([this]()
						{
							return ActiveDatabase.IsValid();
						})
						.OnClicked_Lambda([this]()
						{
							BrowseToDatabaseAsset();
							return FReply::Handled();
						})
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ReloadButton", "Reload From Settings"))
						.OnClicked_Lambda([this]()
						{
							RefreshDatabaseFromSettings();
							return FReply::Handled();
						})
					]
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("RefreshListButton", "Refresh List"))
						.IsEnabled_Lambda([this]()
						{
							return ActiveDatabase.IsValid();
						})
						.OnClicked_Lambda([this]()
						{
							RefreshAchievementList();
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ResetSelectedProgressButton", "Reset Selected Progress"))
						.ToolTipText(LOCTEXT("ResetSelectedProgressTooltip", "Clears the saved progress for the achievement selected in the editor."))
						.IsEnabled_Lambda([this]()
						{
							return SelectedAchievementAsset.IsValid();
						})
						.OnClicked_Lambda([this]()
						{
							ResetSelectedAchievementProgress();
							return FReply::Handled();
						})
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ResetAllProgressButton", "Reset All Progress"))
						.ToolTipText(LOCTEXT("ResetAllProgressTooltip", "Clears the saved progress for every achievement in the configured save slot."))
						.OnClicked_Lambda([this]()
						{
							ResetAllAchievementProgress();
							return FReply::Handled();
						})
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f, 0.0f, 8.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBorder)
					.Padding(8.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 6.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AchievementListHeader", "Achievements"))
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(AchievementListContainer, SVerticalBox)
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.65f)
				[
					SNew(SBorder)
					.Padding(8.0f)
					[
						DetailsView.ToSharedRef()
					]
				]
			]
		];

	RefreshAchievementList();
	return Tab;
}

void FAchievementPluginEditorModule::RefreshDatabaseFromSettings()
{
	const UAchievementPluginSettings* Settings = GetDefault<UAchievementPluginSettings>();
	SetActiveDatabase(Settings->AchievementDatabase.LoadSynchronous());
}

void FAchievementPluginEditorModule::LoadDatabaseAsset()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FOpenAssetDialogConfig OpenAssetDialogConfig;
	OpenAssetDialogConfig.DialogTitleOverride = LOCTEXT("LoadAchievementDatabaseDialogTitle", "Load Achievement Database");
	OpenAssetDialogConfig.bAllowMultipleSelection = false;
	OpenAssetDialogConfig.AssetClassNames.Add(UAchievementDatabase::StaticClass()->GetClassPathName());
	OpenAssetDialogConfig.DefaultPath = GetPluginContentRoot();

	const TArray<FAssetData> SelectedAssets = ContentBrowserModule.Get().CreateModalOpenAssetDialog(OpenAssetDialogConfig);
	if (SelectedAssets.Num() == 0)
	{
		return;
	}

	if (UAchievementDatabase* Database = Cast<UAchievementDatabase>(SelectedAssets[0].GetAsset()))
	{
		SetActiveDatabase(Database);
	}
}

void FAchievementPluginEditorModule::BrowseToDatabaseAsset() const
{
	if (!ActiveDatabase.IsValid())
	{
		return;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> AssetsToSync;
	AssetsToSync.Add(FAssetData(ActiveDatabase.Get()));
	ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
}

void FAchievementPluginEditorModule::CreateDatabaseAsset()
{
	FString PackageName;
	FString AssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(
		FString::Printf(TEXT("%s/AchievementDatabase"), *GetPluginContentRoot()),
		TEXT(""),
		PackageName,
		AssetName);

	UPackage* Package = CreatePackage(*PackageName);
	if (Package == nullptr)
	{
		return;
	}

	UAchievementDatabase* NewDatabase = NewObject<UAchievementDatabase>(Package, *AssetName, RF_Public | RF_Standalone);
	if (NewDatabase == nullptr)
	{
		return;
	}

	FAssetRegistryModule::AssetCreated(NewDatabase);
	NewDatabase->MarkPackageDirty();
	Package->MarkPackageDirty();
	SaveObjectPackage(NewDatabase);

	SetActiveDatabase(NewDatabase);
	BrowseToDatabaseAsset();
}

void FAchievementPluginEditorModule::CreateAchievementAsset()
{
	if (!ActiveDatabase.IsValid())
	{
		return;
	}

	FString PackageName;
	FString AssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(
		FString::Printf(TEXT("%s/Achievements/Achievement"), *GetPluginContentRoot()),
		TEXT(""),
		PackageName,
		AssetName);

	UPackage* Package = CreatePackage(*PackageName);
	if (Package == nullptr)
	{
		return;
	}

	UAchievementDefinitionAsset* NewAchievement = NewObject<UAchievementDefinitionAsset>(Package, *AssetName, RF_Public | RF_Standalone);
	if (NewAchievement == nullptr)
	{
		return;
	}

	NewAchievement->AchievementId = FName(*AssetName);
	NewAchievement->Title = FText::FromString(AssetName);
	NewAchievement->Description = LOCTEXT("DefaultAchievementDescription", "Describe this achievement.");
	NewAchievement->Type = EAchievementType::Goal;
	NewAchievement->RequiredAmount = 1;

	FAssetRegistryModule::AssetCreated(NewAchievement);
	NewAchievement->MarkPackageDirty();
	Package->MarkPackageDirty();
	SaveObjectPackage(NewAchievement);

	ActiveDatabase->Modify();
	ActiveDatabase->AchievementAssets.AddUnique(NewAchievement);
	ActiveDatabase->MarkPackageDirty();
	SaveObjectPackage(ActiveDatabase.Get());

	SetSelectedAchievement(NewAchievement);
}

void FAchievementPluginEditorModule::RefreshAchievementList()
{
	if (!AchievementListContainer.IsValid())
	{
		return;
	}

	AchievementListContainer->ClearChildren();

	if (!ActiveDatabase.IsValid())
	{
		AchievementListContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoDatabaseListText", "No database is loaded."))
		];
		return;
	}

	const TArray<TObjectPtr<UAchievementDefinitionAsset>>& AchievementAssets = ActiveDatabase->AchievementAssets;
	if (AchievementAssets.Num() == 0)
	{
		AchievementListContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EmptyDatabaseListText", "This database does not contain any achievement assets yet."))
		];
		return;
	}

	for (const TObjectPtr<UAchievementDefinitionAsset>& Asset : AchievementAssets)
	{
		UAchievementDefinitionAsset* AchievementAsset = Asset.Get();
		if (AchievementAsset == nullptr)
		{
			continue;
		}

		const bool bIsSelected = SelectedAchievementAsset == AchievementAsset;
		const FText TitleText = AchievementAsset->Title.IsEmpty()
			? FText::FromString(AchievementAsset->GetName())
			: AchievementAsset->Title;
		const FText SelectedPrefix = bIsSelected ? LOCTEXT("SelectedAchievementPrefix", "> ") : FText::GetEmpty();

		AchievementListContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, WeakAchievement = TWeakObjectPtr<UAchievementDefinitionAsset>(AchievementAsset)]()
			{
				if (WeakAchievement.IsValid())
				{
					SetSelectedAchievement(WeakAchievement.Get());
				}

				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("AchievementRowText", "{0}{1} [{2}]"),
					SelectedPrefix,
					TitleText,
					FText::FromName(AchievementAsset->AchievementId)))
			]
		];
	}
}

void FAchievementPluginEditorModule::SetSelectedAchievement(UAchievementDefinitionAsset* AchievementAsset)
{
	SelectedAchievementAsset = AchievementAsset;

	if (DetailsView.IsValid())
	{
		UObject* ObjectToDisplay = AchievementAsset != nullptr
			? Cast<UObject>(AchievementAsset)
			: Cast<UObject>(ActiveDatabase.Get());
		DetailsView->SetObject(ObjectToDisplay);
	}

	RefreshAchievementList();
}

void FAchievementPluginEditorModule::SetActiveDatabase(UAchievementDatabase* Database)
{
	ActiveDatabase = Database;
	SelectedAchievementAsset = nullptr;

	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(Database);
	}

	if (Database != nullptr)
	{
		UAchievementPluginSettings* MutableSettings = GetMutableDefault<UAchievementPluginSettings>();
		MutableSettings->AchievementDatabase = Database;
		MutableSettings->SaveConfig();
	}

	RefreshAchievementList();
}

void FAchievementPluginEditorModule::ResetSelectedAchievementProgress()
{
	if (!SelectedAchievementAsset.IsValid())
	{
		return;
	}

	const FText AchievementName = SelectedAchievementAsset->Title.IsEmpty()
		? FText::FromName(SelectedAchievementAsset->AchievementId)
		: SelectedAchievementAsset->Title;
	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("ResetSelectedConfirm", "Reset progress for '{0}'?"),
			AchievementName));

	if (Result != EAppReturnType::Yes)
	{
		return;
	}

	if (UAchievementSaveGame* SaveGameObject = LoadEditorSaveGame())
	{
		SaveGameObject->AchievementProgress.Remove(SelectedAchievementAsset->AchievementId);
		SaveEditorSaveGame(SaveGameObject);
	}
}

void FAchievementPluginEditorModule::ResetAllAchievementProgress()
{
	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("ResetAllConfirm", "Reset progress for all achievements?"));

	if (Result != EAppReturnType::Yes)
	{
		return;
	}

	if (UAchievementSaveGame* SaveGameObject = LoadEditorSaveGame())
	{
		SaveGameObject->AchievementProgress.Empty();
		SaveEditorSaveGame(SaveGameObject);
	}
}

FString FAchievementPluginEditorModule::GetPluginContentRoot() const
{
	return TEXT("/AchievementPlugin/Data");
}

UAchievementSaveGame* FAchievementPluginEditorModule::LoadEditorSaveGame() const
{
	const UAchievementPluginSettings* Settings = GetDefault<UAchievementPluginSettings>();
	if (Settings == nullptr)
	{
		return nullptr;
	}

	if (USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(Settings->SaveSlotName, Settings->SaveUserIndex))
	{
		if (UAchievementSaveGame* AchievementSave = Cast<UAchievementSaveGame>(LoadedSave))
		{
			return AchievementSave;
		}
	}

	return Cast<UAchievementSaveGame>(UGameplayStatics::CreateSaveGameObject(UAchievementSaveGame::StaticClass()));
}

bool FAchievementPluginEditorModule::SaveEditorSaveGame(UAchievementSaveGame* SaveGameObject) const
{
	if (SaveGameObject == nullptr)
	{
		return false;
	}

	const UAchievementPluginSettings* Settings = GetDefault<UAchievementPluginSettings>();
	if (Settings == nullptr)
	{
		return false;
	}

	return UGameplayStatics::SaveGameToSlot(SaveGameObject, Settings->SaveSlotName, Settings->SaveUserIndex);
}

bool FAchievementPluginEditorModule::SaveObjectPackage(UObject* Object) const
{
	if (Object == nullptr)
	{
		return false;
	}

	UPackage* Package = Object->GetOutermost();
	if (Package == nullptr)
	{
		return false;
	}

	const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, Object, *PackageFileName, SaveArgs);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAchievementPluginEditorModule, AchievementPluginEditor)
