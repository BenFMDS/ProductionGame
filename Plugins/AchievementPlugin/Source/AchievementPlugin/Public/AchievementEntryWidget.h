#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "Blueprint/UserWidget.h"
#include "AchievementEntryWidget.generated.h"

class UAchievementSubsystem;
class UButton;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class ACHIEVEMENTPLUGIN_API UAchievementEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void SetAchievementData(
		UTexture2D* InImage,
		const FText& InTitle,
		const FText& InDescription,
		EAchievementType InType,
		int32 InCurrentAmount = 0,
		int32 InRequiredAmount = 1,
		bool bInUnlocked = false,
		FName InAchievementId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool ResetAchievementProgress();

	UFUNCTION(BlueprintPure, Category = "Achievements")
	float GetProgressPercent() const;

protected:
	void SynchronizeVisuals();
	FText GetProgressText() const;
	bool RefreshFromSubsystem();

	UFUNCTION()
	void HandleResetButtonClicked();

protected:
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UImage> AchievementImage;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UWidget> ProgressContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UButton> ResetButton;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> DisplayImage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	FText DisplayTitle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	FText DisplayDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	EAchievementType DisplayType = EAchievementType::Goal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	int32 CurrentAmount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	int32 RequiredAmount = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	bool bUnlocked = false;

	UPROPERTY(VisibleAnywhere, Category = "Achievements", meta = (AllowPrivateAccess = "true"))
	FName AchievementId = NAME_None;
};
