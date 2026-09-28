#include "AchievementListWidget.h"

#include "AchievementDatabase.h"
#include "AchievementEntryWidget.h"
#include "AchievementSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"

void UAchievementListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ResetAllButton != nullptr)
	{
		ResetAllButton->OnClicked.AddUniqueDynamic(this, &UAchievementListWidget::HandleResetAllButtonClicked);
	}

	UpdateResetAllButtonVisibility();
}

void UAchievementListWidget::ClearAchievements()
{
	if (AchievementScrollBox != nullptr)
	{
		AchievementScrollBox->ClearChildren();
	}
}

UAchievementEntryWidget* UAchievementListWidget::AddAchievementEntry(
	UTexture2D* InImage,
	const FText& InTitle,
	const FText& InDescription,
	const EAchievementType InType,
	const int32 InCurrentAmount,
	const int32 InRequiredAmount,
	const bool bInUnlocked,
	const FName AchievementId)
{
	if (AchievementEntryWidgetClass == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	UAchievementEntryWidget* EntryWidget = CreateWidget<UAchievementEntryWidget>(World, AchievementEntryWidgetClass);
	if (EntryWidget == nullptr)
	{
		return nullptr;
	}

	EntryWidget->SetAchievementData(InImage, InTitle, InDescription, InType, InCurrentAmount, InRequiredAmount, bInUnlocked, AchievementId);

	if (AchievementScrollBox != nullptr)
	{
		AchievementScrollBox->AddChild(EntryWidget);
	}

	return EntryWidget;
}

bool UAchievementListWidget::ResetAllAchievements()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return false;
	}

	UAchievementSubsystem* Subsystem = GameInstance->GetSubsystem<UAchievementSubsystem>();
	if (Subsystem == nullptr)
	{
		return false;
	}

	if (!Subsystem->ResetAllAchievementProgress())
	{
		return false;
	}

	if (CurrentDatabase != nullptr)
	{
		PopulateFromDatabase(CurrentDatabase);
	}
	else
	{
		ClearAchievements();
	}

	return true;
}

void UAchievementListWidget::PopulateFromDatabase(UAchievementDatabase* Database)
{
	CurrentDatabase = Database;
	ClearAchievements();
	UpdateResetAllButtonVisibility();

	if (Database == nullptr)
	{
		return;
	}

	UAchievementSubsystem* AchievementSubsystem = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			AchievementSubsystem = GameInstance->GetSubsystem<UAchievementSubsystem>();
		}
	}

	for (const FAchievementDefinition& Definition : Database->GetAchievementDefinitions())
	{
		const FAchievementProgressState Progress = AchievementSubsystem != nullptr
			? AchievementSubsystem->GetAchievementProgress(Definition.AchievementId)
			: FAchievementProgressState();

		UTexture2D* ImageToUse = nullptr;
		if (Progress.bUnlocked)
		{
			ImageToUse = Definition.UnlockedImage.LoadSynchronous();
		}

		if (ImageToUse == nullptr)
		{
			ImageToUse = Definition.LockedImage.LoadSynchronous();
		}

		AddAchievementEntry(
			ImageToUse,
			Definition.Title,
			Definition.Description,
			Definition.Type,
			Progress.CurrentAmount,
			Definition.RequiredAmount,
			Progress.bUnlocked,
			Definition.AchievementId);
	}
}

void UAchievementListWidget::HandleResetAllButtonClicked()
{
	ResetAllAchievements();
}

void UAchievementListWidget::UpdateResetAllButtonVisibility()
{
	if (ResetAllButton != nullptr)
	{
		ResetAllButton->SetVisibility(CurrentDatabase != nullptr ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
