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

