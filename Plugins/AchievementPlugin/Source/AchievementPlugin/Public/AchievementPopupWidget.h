#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "Blueprint/UserWidget.h"
#include "AchievementPopupWidget.generated.h"

class UAchievementDefinitionAsset;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;
class UWidgetAnimation;

UCLASS(BlueprintType, Blueprintable)
class ACHIEVEMENTPLUGIN_API UAchievementPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Achievements", meta = (WorldContext = "WorldContextObject"))
	static UAchievementPopupWidget* CreateAndShowPopup(
		UObject* WorldContextObject,
		TSubclassOf<UAchievementPopupWidget> WidgetClass,
		UAchievementDefinitionAsset* AchievementAsset,
		FAchievementProgressState ProgressState,
		float AutoHideDelay = 2.5f,
		int32 ZOrder = 100);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void SetPopupData(
		UTexture2D* InImage,
		const FText& InTitle,
		const FText& InDescription,
		EAchievementType InType,
		int32 InCurrentAmount = 0,
		int32 InRequiredAmount = 1,
		bool bInUnlocked = false);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void SetPopupDataFromDefinition(
		UAchievementDefinitionAsset* AchievementAsset,
		FAchievementProgressState ProgressState);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void ShowPopup(float InAutoHideDelay = 2.5f);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void HidePopup();

	UFUNCTION(BlueprintPure, Category = "Achievements")
	float GetProgressPercent() const;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void SynchronizeVisuals();
	FText GetProgressText() const;

	void RemovePopup();

protected:

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> AchievementImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ProgressContainer;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> PopupAnimation;

private:

	UPROPERTY()
	TObjectPtr<UTexture2D> DisplayImage;

	UPROPERTY()
	FText DisplayTitle;

	UPROPERTY()
	FText DisplayDescription;

	UPROPERTY()
	EAchievementType DisplayType = EAchievementType::Goal;

	/** Used to refresh an existing popup instead of stacking a duplicate for the same achievement. */
	UPROPERTY(Transient)
	TObjectPtr<UAchievementDefinitionAsset> SourceAchievement;

	UPROPERTY()
	int32 CurrentAmount = 0;

	UPROPERTY()
	int32 RequiredAmount = 1;

	UPROPERTY()
	bool bUnlocked = false;

	UPROPERTY()
	float AutoHideDelay = 2.5f;

	FTimerHandle AutoHideTimerHandle;
	FTimerHandle RemoveTimerHandle;
};
