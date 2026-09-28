#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AchievementTypes.h"
#include "AchievementSubsystem.generated.h"

class UAchievementDatabase;
class UAchievementDefinitionAsset;
class UAchievementSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAchievementProgressUpdated,
	UAchievementDefinitionAsset*, AchievementAsset,
	FAchievementProgressState, ProgressState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAchievementCompleted,
	UAchievementDefinitionAsset*, AchievementAsset,
	FAchievementProgressState, ProgressState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAchievementMilestoneReached,
	UAchievementDefinitionAsset*, AchievementAsset,
	FAchievementProgressState, ProgressState
);

UCLASS()
class ACHIEVEMENTPLUGIN_API UAchievementSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementProgressUpdated OnAchievementProgressUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementCompleted OnAchievementCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementMilestoneReached OnAchievementMilestoneReached;

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool UnlockAchievement(FName AchievementId);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool AddAchievementProgress(FName AchievementId, int32 Delta = 1);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool UpdateAchievementProgress(FName AchievementId, int32 CurrentAmount);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool RemoveAchievementProgress(FName AchievementId);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool ResetAllAchievementProgress();

	UFUNCTION(BlueprintPure, Category = "Achievements")
	bool IsAchievementUnlocked(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	FAchievementProgressState GetAchievementProgress(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetAchievementCurrentAmount(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetAchievementRequiredAmount(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	float GetAchievementProgressPercent(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	TArray<FAchievementDefinition> GetAchievementDefinitions() const;

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool ResetAchievementProgress(FName AchievementId);

private:
	const UAchievementDatabase* GetAchievementDatabase() const;
	const UAchievementDefinitionAsset* FindDefinitionAsset(FName AchievementId) const;

	FAchievementProgressState& GetOrCreateProgress(FName AchievementId);

	bool SetAchievementProgressInternal(FName AchievementId, int32 CurrentAmount);
	void LoadOrCreateSaveGame();
	bool SaveProgress() const;

private:
	UPROPERTY()
	TObjectPtr<UAchievementSaveGame> SaveGameObject;
};