#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AchievementTypes.h"
#include "AchievementDatabase.generated.h"

class UAchievementDefinitionAsset;

UCLASS(BlueprintType)
class ACHIEVEMENTPLUGIN_API UAchievementDatabase : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	TArray<TObjectPtr<UAchievementDefinitionAsset>> AchievementAssets;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	TArray<FAchievementDefinition> GetAchievementDefinitions() const;

	const UAchievementDefinitionAsset* FindAchievementAsset(FName AchievementId) const;
};
