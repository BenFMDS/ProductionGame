#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AchievementPluginSettings.generated.h"

class UAchievementDatabase;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Achievement Plugin"))
class ACHIEVEMENTPLUGIN_API UAchievementPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UAchievementDatabase> AchievementDatabase;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Save Game")
	FString SaveSlotName = TEXT("AchievementProgress");

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Save Game", meta = (ClampMin = "0"))
	int32 SaveUserIndex = 0;
};
