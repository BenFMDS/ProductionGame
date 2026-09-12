// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/QuestGiverComponent.h"

#include "Engine/GameInstance.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestSubsystem.h"

TArray<UQuestDefinition*> UQuestGiverComponent::GetAvailableQuests() const
{
	TArray<UQuestDefinition*> QuestDefinitions;
	QuestDefinitions.Reserve(AvailableQuests.Num());

	for (UQuestDefinition* QuestDefinition : AvailableQuests)
	{
		if (IsValid(QuestDefinition))
		{
			QuestDefinitions.Add(QuestDefinition);
		}
	}

	return QuestDefinitions;
}

bool UQuestGiverComponent::HasQuest(const UQuestDefinition* QuestDefinition) const
{
	return IsValid(QuestDefinition) && AvailableQuests.Contains(QuestDefinition);
}

bool UQuestGiverComponent::StartQuest(UQuestDefinition* QuestDefinition)
{
	if (!HasQuest(QuestDefinition))
	{
		return false;
	}

	if (UQuestSubsystem* QuestSubsystem = GetQuestSubsystem())
	{
		return QuestSubsystem->StartQuest(QuestDefinition);
	}

	return false;
}

UQuestSubsystem* UQuestGiverComponent::GetQuestSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UQuestSubsystem>();
		}
	}

	return nullptr;
}
