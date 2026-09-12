// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestGiverComponent.generated.h"

class UQuestDefinition;
class UQuestSubsystem;

/** Reusable quest-offering component for NPCs and any other interactable actor. */
UCLASS(ClassGroup=(Quest), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class PRODUCTIONGAME_API UQuestGiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Definitions this actor is allowed to offer. The component never owns quest progress. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
	TArray<TObjectPtr<UQuestDefinition>> AvailableQuests;

	/** Return the quest definitions this actor offers. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Available Quests"))
	TArray<UQuestDefinition*> GetAvailableQuests() const;

	/** Returns true when this actor offers the supplied quest definition. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Has Quest"))
	bool HasQuest(const UQuestDefinition* QuestDefinition) const;

	/** Request that the central quest subsystem starts one of this actor's offered quests. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Start Quest"))
	bool StartQuest(UQuestDefinition* QuestDefinition);

	/** Return this world's central quest subsystem, if available. */
	UFUNCTION(BlueprintPure, Category="Quest")
	UQuestSubsystem* GetQuestSubsystem() const;
};
