// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.generated.h"

/** The supported ways a quest can be progressed. */
UENUM(BlueprintType)
enum class EQuestType : uint8
{
	Collect UMETA(DisplayName = "Collect"),
	Deliver UMETA(DisplayName = "Deliver"),
	Interact UMETA(DisplayName = "Interact"),
	Kill UMETA(DisplayName = "Kill"),
	ReachLocation UMETA(DisplayName = "Reach Location"),
	Custom UMETA(DisplayName = "Custom")
};

/** Mutable, per-player state for a quest. Quest definitions remain immutable data assets. */
USTRUCT(BlueprintType)
struct FQuestProgress
{
	GENERATED_BODY()

	/** Identifier of the quest this state belongs to. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FName QuestID = NAME_None;

	/** Amount currently contributed toward this quest's objective. */
	UPROPERTY(BlueprintReadOnly, Category="Quest", meta=(ClampMin="0"))
	int32 CurrentAmount = 0;

	/** True after the quest has completed. Completed quests are no longer active. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	bool bCompleted = false;
};
