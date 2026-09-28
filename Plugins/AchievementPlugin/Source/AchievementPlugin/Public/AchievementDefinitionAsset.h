#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AchievementTypes.h"
#include "AchievementDefinitionAsset.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class ACHIEVEMENTPLUGIN_API UAchievementDefinitionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement")
	FName AchievementId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement")
	EAchievementType Type = EAchievementType::Goal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement")
	TSoftObjectPtr<UTexture2D> LockedImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement")
	TSoftObjectPtr<UTexture2D> UnlockedImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievement", meta = (ClampMin = "1", EditCondition = "Type == EAchievementType::Number", EditConditionHides))
	int32 RequiredAmount = 1;

	UFUNCTION(BlueprintPure, Category = "Achievement")
	FAchievementDefinition GetDefinitionSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "Achievement")
	void ApplyDefinitionSnapshot(const FAchievementDefinition& Definition);
};
