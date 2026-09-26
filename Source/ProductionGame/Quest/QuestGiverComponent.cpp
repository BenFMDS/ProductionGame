// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/QuestGiverComponent.h"

#include "Quest/QuestDefinition.h"

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

