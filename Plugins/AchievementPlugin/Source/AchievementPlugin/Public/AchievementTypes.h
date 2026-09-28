#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "AchievementTypes.generated.h"

UENUM(BlueprintType)
enum class EAchievementType : uint8
{
	Goal UMETA(DisplayName = "Goal"),
	Number UMETA(DisplayName = "Number")
};

USTRUCT(BlueprintType)
struct ACHIEVEMENTPLUGIN_API FAchievementDefinition
{
	GENERATED_BODY()

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
};

USTRUCT(BlueprintType)
struct ACHIEVEMENTPLUGIN_API FAchievementProgressState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievement")
	int32 CurrentAmount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievement")
	bool bUnlocked = false;


	// Milestones
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievement")
	bool b25PercentShown = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievement")
	bool b50PercentShown = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Achievement")
	bool b75PercentShown = false;
};