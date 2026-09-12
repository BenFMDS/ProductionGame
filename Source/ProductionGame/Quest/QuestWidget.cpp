// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/QuestWidget.h"

#include "Engine/GameInstance.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestSubsystem.h"

void UQuestWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		QuestSubsystem = GameInstance->GetSubsystem<UQuestSubsystem>();
	}

	if (QuestSubsystem)
	{
		QuestSubsystem->OnQuestStarted.AddUniqueDynamic(this, &UQuestWidget::HandleQuestStarted);
		QuestSubsystem->OnQuestProgressUpdated.AddUniqueDynamic(this, &UQuestWidget::HandleQuestProgressUpdated);
		QuestSubsystem->OnQuestCompleted.AddUniqueDynamic(this, &UQuestWidget::HandleQuestCompleted);
		QuestSubsystem->OnQuestAbandoned.AddUniqueDynamic(this, &UQuestWidget::HandleQuestAbandoned);
	}

	RefreshQuestDisplay();
}

void UQuestWidget::NativeDestruct()
{
	if (QuestSubsystem)
	{
		QuestSubsystem->OnQuestStarted.RemoveDynamic(this, &UQuestWidget::HandleQuestStarted);
		QuestSubsystem->OnQuestProgressUpdated.RemoveDynamic(this, &UQuestWidget::HandleQuestProgressUpdated);
		QuestSubsystem->OnQuestCompleted.RemoveDynamic(this, &UQuestWidget::HandleQuestCompleted);
		QuestSubsystem->OnQuestAbandoned.RemoveDynamic(this, &UQuestWidget::HandleQuestAbandoned);
	}

	Super::NativeDestruct();
}

void UQuestWidget::RefreshQuestDisplay()
{
	if (!QuestSubsystem)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			QuestSubsystem = GameInstance->GetSubsystem<UQuestSubsystem>();
		}
	}

	if (!QuestSubsystem)
	{
		ClearQuestDisplay();
		return;
	}

	const FName TrackedQuestID = QuestSubsystem->GetTrackedQuest();
	UQuestDefinition* QuestDefinition = QuestSubsystem->GetQuestDefinition(TrackedQuestID);
	FQuestProgress QuestProgress;
	if (TrackedQuestID.IsNone() || !QuestDefinition || !QuestSubsystem->GetQuestProgress(TrackedQuestID, QuestProgress))
	{
		ClearQuestDisplay();
		return;
	}

	bHasTrackedQuest = true;
	DisplayedQuest = QuestProgress;
	QuestName = QuestDefinition->QuestName;
	QuestDescription = QuestDefinition->Description;
	ObjectiveText = QuestDefinition->ObjectiveText;
	ProgressPercent = QuestSubsystem->GetQuestProgressPercent(TrackedQuestID);
	ProgressText = FText::Format(NSLOCTEXT("QuestWidget", "ProgressText", "{0} / {1}"), QuestProgress.CurrentAmount, QuestDefinition->RequiredAmount);
	QuestIcon = QuestDefinition->QuestIcon;

	BP_QuestDisplayUpdated();
}

void UQuestWidget::HandleQuestStarted(FName QuestID)
{
	RefreshQuestDisplay();
}

void UQuestWidget::HandleQuestProgressUpdated(FName QuestID, int32 CurrentAmount, int32 RequiredAmount)
{
	if (QuestID == QuestSubsystem->GetTrackedQuest())
	{
		RefreshQuestDisplay();
	}
}

void UQuestWidget::HandleQuestCompleted(FName QuestID)
{
	BP_QuestCompleted(QuestID);
	RefreshQuestDisplay();
}

void UQuestWidget::HandleQuestAbandoned(FName QuestID)
{
	if (!bHasTrackedQuest || DisplayedQuest.QuestID == QuestID)
	{
		RefreshQuestDisplay();
	}
}

void UQuestWidget::ClearQuestDisplay()
{
	bHasTrackedQuest = false;
	DisplayedQuest = FQuestProgress();
	QuestName = FText::GetEmpty();
	QuestDescription = FText::GetEmpty();
	ObjectiveText = FText::GetEmpty();
	ProgressPercent = 0.0f;
	ProgressText = FText::GetEmpty();
	QuestIcon.Reset();

	BP_QuestDisplayCleared();
}
