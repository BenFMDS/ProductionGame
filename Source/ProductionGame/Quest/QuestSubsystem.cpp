// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quest/QuestSubsystem.h"

#include "Quest/QuestDefinition.h"
#include "ProductionGame.h"

bool UQuestSubsystem::StartQuest(UQuestDefinition* QuestDefinition)
{
	if (!IsDefinitionValid(QuestDefinition))
	{
		return false;
	}

	const FName QuestID = QuestDefinition->QuestID;
	if (IsQuestActive(QuestID))
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot start because it is already active."), *QuestID.ToString());
		return false;
	}

	if (IsQuestCompleted(QuestID))
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot start because it was already completed."), *QuestID.ToString());
		return false;
	}

	FQuestProgress NewProgress;
	NewProgress.QuestID = QuestID;
	QuestProgressByID.Add(QuestID, NewProgress);
	QuestDefinitionsByID.Add(QuestID, QuestDefinition);
	ActiveQuestIDs.Add(QuestID);

	if (TrackedQuestID.IsNone())
	{
		TrackedQuestID = QuestID;
	}

	OnQuestStarted.Broadcast(QuestID);
	return true;
}

bool UQuestSubsystem::AddQuestProgress(FName QuestID, int32 Amount)
{
	if (Amount <= 0)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' received an invalid progress amount of %d."), *QuestID.ToString(), Amount);
		return false;
	}

	FQuestProgress* Progress = FindActiveQuest(QuestID);
	if (!Progress)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot receive progress because it is not active."), *QuestID.ToString());
		return false;
	}

	const int32 RequiredAmount = GetQuestRequiredAmount(QuestID);
	if (RequiredAmount <= 0)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' has an invalid required amount."), *QuestID.ToString());
		return false;
	}

	const int32 RemainingAmount = RequiredAmount - Progress->CurrentAmount;
	return UpdateQuestProgress(QuestID, Progress->CurrentAmount + FMath::Min(Amount, RemainingAmount));
}

bool UQuestSubsystem::UpdateQuestProgress(FName QuestID, int32 NewAmount)
{
	if (NewAmount < 0)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' received a negative progress amount."), *QuestID.ToString());
		return false;
	}

	FQuestProgress* Progress = FindActiveQuest(QuestID);
	if (!Progress)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot update because it is not active."), *QuestID.ToString());
		return false;
	}

	const int32 RequiredAmount = GetQuestRequiredAmount(QuestID);
	if (RequiredAmount <= 0)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' has an invalid required amount."), *QuestID.ToString());
		return false;
	}

	const int32 ClampedAmount = FMath::Clamp(NewAmount, 0, RequiredAmount);
	if (Progress->CurrentAmount != ClampedAmount)
	{
		Progress->CurrentAmount = ClampedAmount;
		OnQuestProgressUpdated.Broadcast(QuestID, Progress->CurrentAmount, RequiredAmount);
	}

	// Delegate listeners may abandon or otherwise modify this quest, so reacquire state after broadcasting.
	if (const FQuestProgress* UpdatedProgress = FindActiveQuest(QuestID); UpdatedProgress && UpdatedProgress->CurrentAmount >= RequiredAmount)
	{
		return CompleteQuest(QuestID);
	}

	return true;
}

bool UQuestSubsystem::CompleteQuest(FName QuestID)
{
	FQuestProgress* Progress = FindActiveQuest(QuestID);
	if (!Progress)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot complete because it is not active."), *QuestID.ToString());
		return false;
	}

	if (Progress->bCompleted)
	{
		return false;
	}

	const int32 RequiredAmount = GetQuestRequiredAmount(QuestID);
	if (RequiredAmount <= 0)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' has an invalid required amount."), *QuestID.ToString());
		return false;
	}

	if (Progress->CurrentAmount != RequiredAmount)
	{
		Progress->CurrentAmount = RequiredAmount;
		OnQuestProgressUpdated.Broadcast(QuestID, Progress->CurrentAmount, RequiredAmount);
	}

	// Blueprint listeners are allowed to make quest decisions in response to progress updates.
	Progress = FindActiveQuest(QuestID);
	if (!Progress || Progress->bCompleted)
	{
		return false;
	}

	Progress->bCompleted = true;
	ActiveQuestIDs.RemoveSingle(QuestID);

	if (TrackedQuestID == QuestID)
	{
		ClearTrackedQuest();
	}

	OnQuestCompleted.Broadcast(QuestID);
	return true;
}

bool UQuestSubsystem::AbandonQuest(FName QuestID)
{
	if (!FindActiveQuest(QuestID))
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot be abandoned because it is not active."), *QuestID.ToString());
		return false;
	}

	ActiveQuestIDs.RemoveSingle(QuestID);
	QuestProgressByID.Remove(QuestID);
	QuestDefinitionsByID.Remove(QuestID);

	if (TrackedQuestID == QuestID)
	{
		ClearTrackedQuest();
	}

	OnQuestAbandoned.Broadcast(QuestID);
	return true;
}

bool UQuestSubsystem::IsQuestActive(FName QuestID) const
{
	return !QuestID.IsNone() && ActiveQuestIDs.Contains(QuestID);
}

bool UQuestSubsystem::IsQuestCompleted(FName QuestID) const
{
	if (const FQuestProgress* Progress = QuestProgressByID.Find(QuestID))
	{
		return Progress->bCompleted;
	}

	return false;
}

bool UQuestSubsystem::GetQuestProgress(FName QuestID, FQuestProgress& OutProgress) const
{
	if (const FQuestProgress* Progress = QuestProgressByID.Find(QuestID))
	{
		OutProgress = *Progress;
		return true;
	}

	OutProgress = FQuestProgress();
	return false;
}

int32 UQuestSubsystem::GetQuestCurrentAmount(FName QuestID) const
{
	if (const FQuestProgress* Progress = QuestProgressByID.Find(QuestID))
	{
		return Progress->CurrentAmount;
	}

	return 0;
}

int32 UQuestSubsystem::GetQuestRequiredAmount(FName QuestID) const
{
	if (const TObjectPtr<UQuestDefinition>* QuestDefinition = QuestDefinitionsByID.Find(QuestID))
	{
		return IsValid(*QuestDefinition) ? (*QuestDefinition)->RequiredAmount : 0;
	}

	return 0;
}

float UQuestSubsystem::GetQuestProgressPercent(FName QuestID) const
{
	const int32 RequiredAmount = GetQuestRequiredAmount(QuestID);
	if (RequiredAmount <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(GetQuestCurrentAmount(QuestID)) / static_cast<float>(RequiredAmount), 0.0f, 1.0f);
}

TArray<FQuestProgress> UQuestSubsystem::GetActiveQuests() const
{
	TArray<FQuestProgress> ActiveQuests;
	ActiveQuests.Reserve(ActiveQuestIDs.Num());

	for (const FName QuestID : ActiveQuestIDs)
	{
		if (const FQuestProgress* Progress = QuestProgressByID.Find(QuestID))
		{
			ActiveQuests.Add(*Progress);
		}
	}

	return ActiveQuests;
}

bool UQuestSubsystem::GetActiveQuest(FQuestProgress& OutQuestProgress) const
{
	for (const FName QuestID : ActiveQuestIDs)
	{
		if (GetQuestProgress(QuestID, OutQuestProgress))
		{
			return true;
		}
	}

	OutQuestProgress = FQuestProgress();
	return false;
}

bool UQuestSubsystem::SetTrackedQuest(FName QuestID)
{
	if (!IsQuestActive(QuestID))
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest '%s' cannot be tracked because it is not active."), *QuestID.ToString());
		return false;
	}

	TrackedQuestID = QuestID;
	return true;
}

FName UQuestSubsystem::GetTrackedQuest() const
{
	return TrackedQuestID;
}

void UQuestSubsystem::ClearTrackedQuest()
{
	TrackedQuestID = NAME_None;
}

UQuestDefinition* UQuestSubsystem::GetQuestDefinition(FName QuestID) const
{
	if (const TObjectPtr<UQuestDefinition>* QuestDefinition = QuestDefinitionsByID.Find(QuestID))
	{
		return QuestDefinition->Get();
	}

	return nullptr;
}

void UQuestSubsystem::Deinitialize()
{
	QuestProgressByID.Empty();
	QuestDefinitionsByID.Empty();
	ActiveQuestIDs.Empty();
	TrackedQuestID = NAME_None;

	Super::Deinitialize();
}

FQuestProgress* UQuestSubsystem::FindActiveQuest(FName QuestID)
{
	return IsQuestActive(QuestID) ? QuestProgressByID.Find(QuestID) : nullptr;
}

const FQuestProgress* UQuestSubsystem::FindActiveQuest(FName QuestID) const
{
	return IsQuestActive(QuestID) ? QuestProgressByID.Find(QuestID) : nullptr;
}

bool UQuestSubsystem::IsDefinitionValid(const UQuestDefinition* QuestDefinition) const
{
	if (!IsValid(QuestDefinition))
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Cannot start a null quest definition."));
		return false;
	}

	if (QuestDefinition->QuestID.IsNone())
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest definition '%s' has no Quest ID."), *GetNameSafe(QuestDefinition));
		return false;
	}

	if (QuestDefinition->RequiredAmount <= 0)
	{
		UE_LOG(LogProductionGame, Warning, TEXT("Quest definition '%s' has an invalid required amount of %d."), *GetNameSafe(QuestDefinition), QuestDefinition->RequiredAmount);
		return false;
	}

	return true;
}
