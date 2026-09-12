// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quest/QuestTypes.h"
#include "QuestWidget.generated.h"

class UQuestDefinition;
class UQuestSubsystem;
class UTexture2D;

/**
 * Event-driven base class for WBP_QuestWidget. Create a Widget Blueprint from this
 * class and implement the Blueprint events to update its progress bar and text.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PRODUCTIONGAME_API UQuestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Runtime state currently displayed by this widget. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FQuestProgress DisplayedQuest;

	/** Whether there is a tracked quest to display. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	bool bHasTrackedQuest = false;

	/** Display values copied from the current quest definition. */
	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FText QuestName;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FText QuestDescription;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FText ObjectiveText;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	float ProgressPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	FText ProgressText;

	UPROPERTY(BlueprintReadOnly, Category="Quest")
	TSoftObjectPtr<UTexture2D> QuestIcon;

	/** Refresh the HUD from the currently tracked quest. This is event-driven; it is also safe to call after creating the widget. */
	UFUNCTION(BlueprintCallable, Category="Quest", meta=(DisplayName="Refresh Quest Display"))
	void RefreshQuestDisplay();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** Implement in WBP_QuestWidget to set text, icon, and progress-bar state. */
	UFUNCTION(BlueprintImplementableEvent, Category="Quest", meta=(DisplayName="Quest Display Updated"))
	void BP_QuestDisplayUpdated();

	/** Implement in WBP_QuestWidget to hide or clear the quest panel. */
	UFUNCTION(BlueprintImplementableEvent, Category="Quest", meta=(DisplayName="Quest Display Cleared"))
	void BP_QuestDisplayCleared();

	/** Optional hook for a completion animation or message. */
	UFUNCTION(BlueprintImplementableEvent, Category="Quest", meta=(DisplayName="Quest Completed"))
	void BP_QuestCompleted(FName QuestID);

private:
	UPROPERTY(Transient)
	TObjectPtr<UQuestSubsystem> QuestSubsystem;

	UFUNCTION()
	void HandleQuestStarted(FName QuestID);

	UFUNCTION()
	void HandleQuestProgressUpdated(FName QuestID, int32 CurrentAmount, int32 RequiredAmount);

	UFUNCTION()
	void HandleQuestCompleted(FName QuestID);

	UFUNCTION()
	void HandleQuestAbandoned(FName QuestID);

	void ClearQuestDisplay();
};
