#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "Blueprint/UserWidget.h"
#include "AchievementListWidget.generated.h"

class UAchievementDatabase;
class UAchievementEntryWidget;
class UAchievementSubsystem;
class UButton;
class UScrollBox;
class UTexture2D;

UCLASS(BlueprintType, Blueprintable)
class ACHIEVEMENTPLUGIN_API UAchievementListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void ClearAchievements();

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	UAchievementEntryWidget* AddAchievementEntry(
		UTexture2D* InImage,
		const FText& InTitle,
		const FText& InDescription,
		EAchievementType InType,
		int32 InCurrentAmount = 0,
		int32 InRequiredAmount = 1,
		bool bInUnlocked = false,
		FName AchievementId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void PopulateFromDatabase(UAchievementDatabase* Database);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool ResetAllAchievements();

protected:
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UScrollBox> AchievementScrollBox;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UButton> ResetAllButton;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	TSubclassOf<UAchievementEntryWidget> AchievementEntryWidgetClass;

private:
	UFUNCTION()
	void HandleResetAllButtonClicked();

	void UpdateResetAllButtonVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UAchievementDatabase> CurrentDatabase;
};
