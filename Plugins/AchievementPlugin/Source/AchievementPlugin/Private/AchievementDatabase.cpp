#include "AchievementDatabase.h"

#include "AchievementDefinitionAsset.h"

TArray<FAchievementDefinition> UAchievementDatabase::GetAchievementDefinitions() const
{
	TArray<FAchievementDefinition> Definitions;
	Definitions.Reserve(AchievementAssets.Num());

	for (const TObjectPtr<UAchievementDefinitionAsset>& Asset : AchievementAssets)
	{
		if (Asset != nullptr)
		{
			Definitions.Add(Asset->GetDefinitionSnapshot());
		}
	}

	return Definitions;
}

const UAchievementDefinitionAsset* UAchievementDatabase::FindAchievementAsset(const FName AchievementId) const
{
	for (const TObjectPtr<UAchievementDefinitionAsset>& Asset : AchievementAssets)
	{
		if (Asset != nullptr && Asset->AchievementId == AchievementId)
		{
			return Asset;
		}
	}

	return nullptr;
}
