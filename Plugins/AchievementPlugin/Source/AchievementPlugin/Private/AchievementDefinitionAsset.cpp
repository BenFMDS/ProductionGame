#include "AchievementDefinitionAsset.h"

FAchievementDefinition UAchievementDefinitionAsset::GetDefinitionSnapshot() const
{
	FAchievementDefinition Definition;
	Definition.AchievementId = AchievementId;
	Definition.Title = Title;
	Definition.Description = Description;
	Definition.Type = Type;
	Definition.LockedImage = LockedImage;
	Definition.UnlockedImage = UnlockedImage;
	Definition.RequiredAmount = RequiredAmount;
	Definition.bSecret = bSecret;
	Definition.EarnedText = EarnedText;
	Definition.LockedHint = LockedHint;
	return Definition;
}

void UAchievementDefinitionAsset::ApplyDefinitionSnapshot(const FAchievementDefinition& Definition)
{
	AchievementId = Definition.AchievementId;
	Title = Definition.Title;
	Description = Definition.Description;
	Type = Definition.Type;
	LockedImage = Definition.LockedImage;
	UnlockedImage = Definition.UnlockedImage;
	RequiredAmount = Definition.RequiredAmount;
	bSecret = Definition.bSecret;
	EarnedText = Definition.EarnedText;
	LockedHint = Definition.LockedHint;
}
