#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AchievementTypes.h"
#include "AchievementSaveGame.generated.h"

UCLASS()
class ACHIEVEMENTPLUGIN_API UAchievementSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievements")
	TMap<FName, FAchievementProgressState> AchievementProgress;
};
