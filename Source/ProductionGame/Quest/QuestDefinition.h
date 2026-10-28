// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QuestTypes.h"
#include "QuestDefinition.generated.h"

class UTexture2D;

/** Immutable editor-authored quest data. Multiplayer runtime state belongs to UPlayerQuestComponent. */
UCLASS(BlueprintType)
class PRODUCTIONGAME_API UQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identifier used by gameplay and Blueprint quest events. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FName QuestID = NAME_None;

	/** Player-facing name of this quest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FText QuestName;

	/** Player-facing description of this quest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta=(MultiLine="true"))
	FText Description;

	/** Player-facing instruction for the current objective. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta=(MultiLine="true"))
	FText ObjectiveText;

	/** How gameplay is expected to progress this quest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	EQuestType QuestType = EQuestType::Custom;

	/** Amount required before the quest completes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest", meta=(ClampMin="1"))
	int32 RequiredAmount = 1;

	/** Server gameplay reports this event after validating a pickup, kill, delivery, etc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	FName ProgressEvent;

	/** Key the player gets on turn-in. A locked door with the same Key Id opens for them. None gives no key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest|Reward")
	FName RewardKey;

	/** Optional HUD or journal icon. It is loaded only when the UI needs it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest")
	TSoftObjectPtr<UTexture2D> QuestIcon;
};
