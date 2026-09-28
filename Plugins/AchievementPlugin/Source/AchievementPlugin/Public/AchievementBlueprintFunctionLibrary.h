#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AchievementBlueprintFunctionLibrary.generated.h"

class UAchievementDefinitionAsset;
class UAchievementPopupWidget;
class UAchievementSubsystem;

UCLASS()
class ACHIEVEMENTPLUGIN_API UAchievementBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static UAchievementSubsystem* GetAchievementSubsystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool AddAchievementProgress(const UObject* WorldContextObject, FName AchievementId, int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool UnlockAchievement(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool UpdateAchievementProgress(const UObject* WorldContextObject, FName AchievementId, int32 CurrentAmount);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool RemoveAchievementProgress(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool ResetAchievementProgress(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool ResetAllAchievementProgress(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static int32 GetAchievementCurrentAmount(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static int32 GetAchievementRequiredAmount(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static float GetAchievementProgressPercent(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static bool IsAchievementUnlocked(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static FAchievementProgressState GetAchievementProgress(const UObject* WorldContextObject, FName AchievementId);

	UFUNCTION(BlueprintPure, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static TArray<FAchievementDefinition> GetAchievementDefinitions(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static UAchievementPopupWidget* ShowAchievementPopup(
		const UObject* WorldContextObject,
		TSubclassOf<UAchievementPopupWidget> PopupWidgetClass,
		UAchievementDefinitionAsset* AchievementAsset,
		FAchievementProgressState ProgressState,
		float AutoHideDelay = 2.5f,
		int32 ZOrder = 100);

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject", DisplayName = "Show Achievement Popup (By Id)"))
	static UAchievementPopupWidget* ShowAchievementPopupById(
		const UObject* WorldContextObject,
		FName AchievementId,
		TSubclassOf<UAchievementPopupWidget> PopupWidgetClass,
		float AutoHideDelay = 2.5f,
		int32 ZOrder = 100);
};
