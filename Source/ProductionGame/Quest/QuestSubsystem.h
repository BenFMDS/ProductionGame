// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "QuestTypes.h"
#include "QuestSubsystem.generated.h"

class UQuestDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestStarted, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestProgressUpdated, FName, QuestID, int32, CurrentAmount, int32, RequiredAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestCompleted, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAbandoned, FName, QuestID);

/**
 * Central runtime manager for the local player's quests.
 *
 * Definitions are static data assets. This subsystem owns only mutable progress,
 * broadcasts state changes for UI/gameplay, and deliberately has no reward logic.
 */
UCLASS(BlueprintType)
class PRODUCTIONGAME_API UQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Broadcast after a valid quest has become active. */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events")
	FOnQuestStarted OnQuestStarted;

	/** Broadcast whenever an active quest's amount changes. */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events")
	FOnQuestProgressUpdated OnQuestProgressUpdated;

	/** Broadcast exactly once when an active quest completes. */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events")
	FOnQuestCompleted OnQuestCompleted;

	/** Broadcast after an active quest has been abandoned. */
	UPROPERTY(BlueprintAssignable, Category="Quest|Events")
	FOnQuestAbandoned OnQuestAbandoned;

	/** Start a quest from an authored quest definition. Completed quests cannot be replayed by this foundation. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Start Quest"))
	bool StartQuest(UQuestDefinition* QuestDefinition);

	/** Add a positive amount of progress to an active quest. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Add Quest Progress"))
	bool AddQuestProgress(FName QuestID, int32 Amount);

	/** Set an active quest's progress directly. The value is clamped to the definition's required amount. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Update Quest Progress"))
	bool UpdateQuestProgress(FName QuestID, int32 NewAmount);

	/** Complete an active quest immediately. This fills its progress before broadcasting completion. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Complete Quest"))
	bool CompleteQuest(FName QuestID);

	/** Stop tracking an active quest without marking it complete. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Abandon Quest"))
	bool AbandonQuest(FName QuestID);

	/** Returns true only while the quest is active. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Is Quest Active"))
	bool IsQuestActive(FName QuestID) const;

	/** Returns true after a quest has completed during this game instance's lifetime. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Is Quest Completed"))
	bool IsQuestCompleted(FName QuestID) const;

	/** Obtain the current state for an active or completed quest. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Quest Progress"))
	bool GetQuestProgress(FName QuestID, FQuestProgress& OutProgress) const;

	/** Returns an active or completed quest's current amount, or zero if it is unknown. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Quest Current Amount"))
	int32 GetQuestCurrentAmount(FName QuestID) const;

	/** Returns an active or completed quest's configured required amount, or zero if it is unknown. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Quest Required Amount"))
	int32 GetQuestRequiredAmount(FName QuestID) const;

	/** Returns an active or completed quest's normalized completion percentage. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Quest Progress Percent"))
	float GetQuestProgressPercent(FName QuestID) const;

	/** Returns runtime progress for every currently active quest. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Active Quests"))
	TArray<FQuestProgress> GetActiveQuests() const;

	/** Returns the first active quest, if one exists. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Active Quest"))
	bool GetActiveQuest(FQuestProgress& OutQuestProgress) const;

	/** Assign an active quest to the main HUD. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Set Tracked Quest"))
	bool SetTrackedQuest(FName QuestID);

	/** Returns the quest currently selected for the main HUD, or None. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Tracked Quest"))
	FName GetTrackedQuest() const;

	/** Remove the current main-HUD quest selection. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Clear Tracked Quest"))
	void ClearTrackedQuest();

	/** Retrieve a cached definition for an active or completed quest. */
	UFUNCTION(BlueprintPure, Category="Quest", meta=(DisplayName="Get Quest Definition"))
	UQuestDefinition* GetQuestDefinition(FName QuestID) const;

	virtual void Deinitialize() override;

private:
	/** All state seen during this game instance. Completed entries are retained for completion queries and later save integration. */
	UPROPERTY(Transient)
	TMap<FName, FQuestProgress> QuestProgressByID;

	/** Definition references corresponding to runtime state. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UQuestDefinition>> QuestDefinitionsByID;

	/** Ordered IDs for the active-quest list. */
	UPROPERTY(Transient)
	TArray<FName> ActiveQuestIDs;

	/** Quest selected for the primary HUD. */
	UPROPERTY(Transient)
	FName TrackedQuestID = NAME_None;

	FQuestProgress* FindActiveQuest(FName QuestID);
	const FQuestProgress* FindActiveQuest(FName QuestID) const;
	bool IsDefinitionValid(const UQuestDefinition* QuestDefinition) const;
};
