#include "AchievementPopupWidget.h"

#include "Engine/Engine.h"
#include "AchievementDefinitionAsset.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UAchievementPopupWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UAchievementPopupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SynchronizeVisuals();
}

UAchievementPopupWidget* UAchievementPopupWidget::CreateAndShowPopup(
	UObject* WorldContextObject,
	TSubclassOf<UAchievementPopupWidget> WidgetClass,
	UAchievementDefinitionAsset* AchievementAsset,
	FAchievementProgressState ProgressState,
	float InAutoHideDelay)
{
	if (!WorldContextObject || !WidgetClass)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);

	if (!World)
	{
		return nullptr;
	}

	UAchievementPopupWidget* Widget = CreateWidget<UAchievementPopupWidget>(World, WidgetClass);

	if (!Widget)
	{
		return nullptr;
	}

	Widget->AddToViewport(9999);
	Widget->SetPopupDataFromDefinition(AchievementAsset, ProgressState);
	Widget->ShowPopup(InAutoHideDelay);

	return Widget;
}

void UAchievementPopupWidget::SetPopupData(
	UTexture2D* InImage,
	const FText& InTitle,
	const FText& InDescription,
	EAchievementType InType,
	int32 InCurrentAmount,
	int32 InRequiredAmount,
	bool bInUnlocked)
{
	DisplayImage = InImage;
	DisplayTitle = InTitle;
	DisplayDescription = InDescription;
	DisplayType = InType;
	CurrentAmount = InCurrentAmount;
	RequiredAmount = FMath::Max(InRequiredAmount, 1);
	bUnlocked = bInUnlocked;

	SynchronizeVisuals();
}

void UAchievementPopupWidget::SetPopupDataFromDefinition(
	UAchievementDefinitionAsset* AchievementAsset,
	FAchievementProgressState ProgressState)
{
	if (!AchievementAsset)
	{
		return;
	}

	UTexture2D* ImageToUse = ProgressState.bUnlocked
		? AchievementAsset->UnlockedImage.LoadSynchronous()
		: AchievementAsset->LockedImage.LoadSynchronous();

	SetPopupData(
		ImageToUse,
		AchievementAsset->Title,
		AchievementAsset->Description,
		AchievementAsset->Type,
		ProgressState.CurrentAmount,
		AchievementAsset->RequiredAmount,
		ProgressState.bUnlocked);
}

void UAchievementPopupWidget::ShowPopup(float InAutoHideDelay)
{
	UE_LOG(LogTemp, Warning, TEXT("Achievement Popup: ShowPopup"));

	AutoHideDelay = FMath::Max(InAutoHideDelay, 1.0f);

	SetVisibility(ESlateVisibility::Visible);
	SynchronizeVisuals();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoHideTimerHandle);
		World->GetTimerManager().SetTimer(
			AutoHideTimerHandle,
			this,
			&UAchievementPopupWidget::HidePopup,
			AutoHideDelay,
			false);
	}

	if (PopupAnimation)
	{
		PlayAnimation(PopupAnimation);
	}
}

void UAchievementPopupWidget::HidePopup()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoHideTimerHandle);
		World->GetTimerManager().ClearTimer(RemoveTimerHandle);
	}

	if (PopupAnimation)
	{
		PlayAnimationReverse(PopupAnimation);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RemoveTimerHandle,
				this,
				&UAchievementPopupWidget::RemovePopup,
				0.35f,
				false);
		}
		return;
	}

	RemovePopup();
}

float UAchievementPopupWidget::GetProgressPercent() const
{
	if (DisplayType != EAchievementType::Number)
	{
		return bUnlocked ? 1.0f : 0.0f;
	}

	return RequiredAmount > 0
		? FMath::Clamp((float)CurrentAmount / (float)RequiredAmount, 0.f, 1.f)
		: 0.f;
}

void UAchievementPopupWidget::SynchronizeVisuals()
{
	if (AchievementImage)
	{
		AchievementImage->SetBrushFromTexture(DisplayImage);
	}

	if (TitleText)
	{
		TitleText->SetText(DisplayTitle);
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(DisplayDescription);
	}

	const bool bShowProgress = DisplayType == EAchievementType::Number;

	if (ProgressContainer)
	{
		ProgressContainer->SetVisibility(bShowProgress ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(GetProgressPercent());
	}

	if (ProgressText)
	{
		ProgressText->SetText(GetProgressText());
	}
}

FText UAchievementPopupWidget::GetProgressText() const
{
	if (DisplayType != EAchievementType::Number)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		FText::FromString(TEXT("{0}/{1}")),
		FText::AsNumber(CurrentAmount),
		FText::AsNumber(RequiredAmount));
}

void UAchievementPopupWidget::RemovePopup()
{
	RemoveFromParent();
}