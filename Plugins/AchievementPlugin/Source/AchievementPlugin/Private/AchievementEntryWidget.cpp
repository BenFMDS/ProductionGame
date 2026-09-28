#include "AchievementEntryWidget.h"

#include "AchievementSubsystem.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UAchievementEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ResetButton != nullptr)
	{
		ResetButton->OnClicked.AddUniqueDynamic(this, &UAchievementEntryWidget::HandleResetButtonClicked);
	}
}

void UAchievementEntryWidget::SetAchievementData(
	UTexture2D* InImage,
	const FText& InTitle,
	const FText& InDescription,
	const EAchievementType InType,
	const int32 InCurrentAmount,
	const int32 InRequiredAmount,
	const bool bInUnlocked,
	const FName InAchievementId)
{
	DisplayImage = InImage;
	DisplayTitle = InTitle;
	DisplayDescription = InDescription;
	DisplayType = InType;
	CurrentAmount = InCurrentAmount;
	RequiredAmount = FMath::Max(InRequiredAmount, 1);
	bUnlocked = bInUnlocked;
	AchievementId = InAchievementId;

	SynchronizeVisuals();
}

bool UAchievementEntryWidget::ResetAchievementProgress()
{
	if (AchievementId.IsNone())
	{
		return false;
	}

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

	if (!Subsystem->ResetAchievementProgress(AchievementId))
	{
		return false;
	}

	RefreshFromSubsystem();
	return true;
}

float UAchievementEntryWidget::GetProgressPercent() const
{
	if (DisplayType != EAchievementType::Number)
	{
		return bUnlocked ? 1.0f : 0.0f;
	}

	return RequiredAmount > 0 ? FMath::Clamp(static_cast<float>(CurrentAmount) / static_cast<float>(RequiredAmount), 0.0f, 1.0f) : 0.0f;
}

void UAchievementEntryWidget::SynchronizeVisuals()
{
	if (AchievementImage != nullptr)
	{
		AchievementImage->SetBrushFromTexture(DisplayImage, true);
	}

	if (TitleText != nullptr)
	{
		TitleText->SetText(DisplayTitle);
	}

	if (DescriptionText != nullptr)
	{
		DescriptionText->SetText(DisplayDescription);
	}

	if (ResetButton != nullptr)
	{
		ResetButton->SetVisibility(AchievementId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	const bool bShowProgress = DisplayType == EAchievementType::Number;
	if (ProgressContainer != nullptr)
	{
		ProgressContainer->SetVisibility(bShowProgress ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ProgressBar != nullptr)
	{
		ProgressBar->SetVisibility(bShowProgress ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ProgressBar->SetPercent(GetProgressPercent());
	}

	if (ProgressText != nullptr)
	{
		ProgressText->SetVisibility(bShowProgress ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ProgressText->SetText(GetProgressText());
	}
}

FText UAchievementEntryWidget::GetProgressText() const
{
	if (DisplayType != EAchievementType::Number)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		NSLOCTEXT("AchievementEntryWidget", "ProgressFormat", "{0}/{1}"),
		FText::AsNumber(CurrentAmount),
		FText::AsNumber(RequiredAmount));
}

bool UAchievementEntryWidget::RefreshFromSubsystem()
{
	if (AchievementId.IsNone())
	{
		return false;
	}

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

	const TArray<FAchievementDefinition> Definitions = Subsystem->GetAchievementDefinitions();
	const FAchievementDefinition* Definition = Definitions.FindByPredicate([this](const FAchievementDefinition& Entry)
	{
		return Entry.AchievementId == AchievementId;
	});

	if (Definition == nullptr)
	{
		return false;
	}

	const FAchievementProgressState Progress = Subsystem->GetAchievementProgress(AchievementId);
	UTexture2D* ImageToUse = Progress.bUnlocked
		? Definition->UnlockedImage.LoadSynchronous()
		: Definition->LockedImage.LoadSynchronous();

	if (ImageToUse == nullptr)
	{
		ImageToUse = Progress.bUnlocked
			? Definition->LockedImage.LoadSynchronous()
			: Definition->UnlockedImage.LoadSynchronous();
	}

	SetAchievementData(
		ImageToUse,
		Definition->Title,
		Definition->Description,
		Definition->Type,
		Progress.CurrentAmount,
		Definition->RequiredAmount,
		Progress.bUnlocked,
		AchievementId);

	return true;
}

void UAchievementEntryWidget::HandleResetButtonClicked()
{
	ResetAchievementProgress();
}
