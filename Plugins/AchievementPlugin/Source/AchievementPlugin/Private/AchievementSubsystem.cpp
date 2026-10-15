#include "AchievementSubsystem.h"

#include "AchievementDatabase.h"
#include "AchievementDefinitionAsset.h"
#include "AchievementPluginSettings.h"
#include "AchievementSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UAchievementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadOrCreateSaveGame();
}

bool UAchievementSubsystem::UnlockAchievement(const FName AchievementId)
{
	const UAchievementDefinitionAsset* DefinitionAsset = FindDefinitionAsset(AchievementId);
	if (!DefinitionAsset || !SaveGameObject)
	{
		return false;
	}

	return SetAchievementProgressInternal(AchievementId, INT32_MAX);
}

bool UAchievementSubsystem::AddAchievementProgress(const FName AchievementId, const int32 Delta)
{
	const FAchievementProgressState Progress = GetAchievementProgress(AchievementId);
	return SetAchievementProgressInternal(AchievementId, Progress.CurrentAmount + Delta);
}

bool UAchievementSubsystem::UpdateAchievementProgress(const FName AchievementId, const int32 CurrentAmount)
{
	return SetAchievementProgressInternal(AchievementId, CurrentAmount);
}

bool UAchievementSubsystem::RemoveAchievementProgress(const FName AchievementId)
{
	if (!SaveGameObject)
	{
		return false;
	}

	return SaveGameObject->AchievementProgress.Remove(AchievementId) > 0;
}

bool UAchievementSubsystem::ResetAllAchievementProgress()
{
	if (!SaveGameObject)
	{
		return false;
	}

	SaveGameObject->AchievementProgress.Empty();
	return SaveProgress();
}

bool UAchievementSubsystem::IsAchievementUnlocked(const FName AchievementId) const
{
	return GetAchievementProgress(AchievementId).bUnlocked;
}

FAchievementProgressState UAchievementSubsystem::GetAchievementProgress(const FName AchievementId) const
{
	if (!SaveGameObject)
	{
		return {};
	}

	if (const FAchievementProgressState* Found = SaveGameObject->AchievementProgress.Find(AchievementId))
	{
		return *Found;
	}

	return {};
}

int32 UAchievementSubsystem::GetAchievementCurrentAmount(const FName AchievementId) const
{
	return GetAchievementProgress(AchievementId).CurrentAmount;
}

int32 UAchievementSubsystem::GetAchievementRequiredAmount(const FName AchievementId) const
{
	const UAchievementDefinitionAsset* Def = FindDefinitionAsset(AchievementId);
	return Def ? FMath::Max(Def->RequiredAmount, 1) : 0;
}

float UAchievementSubsystem::GetAchievementProgressPercent(const FName AchievementId) const
{
	const UAchievementDefinitionAsset* Def = FindDefinitionAsset(AchievementId);
	if (!Def)
	{
		return 0.f;
	}

	const FAchievementProgressState Progress = GetAchievementProgress(AchievementId);

	if (Def->Type != EAchievementType::Number)
	{
		return Progress.bUnlocked ? 1.f : 0.f;
	}

	const int32 Required = FMath::Max(Def->RequiredAmount, 1);
	return FMath::Clamp((float)Progress.CurrentAmount / (float)Required, 0.f, 1.f);
}

TArray<FAchievementDefinition> UAchievementSubsystem::GetAchievementDefinitions() const
{
	const UAchievementDatabase* DB = GetAchievementDatabase();
	return DB ? DB->GetAchievementDefinitions() : TArray<FAchievementDefinition>();
}

UAchievementDefinitionAsset* UAchievementSubsystem::GetAchievementDefinitionAsset(const FName AchievementId) const
{
	return const_cast<UAchievementDefinitionAsset*>(FindDefinitionAsset(AchievementId));
}

const UAchievementDatabase* UAchievementSubsystem::GetAchievementDatabase() const
{
	const UAchievementPluginSettings* Settings = GetDefault<UAchievementPluginSettings>();
	return Settings->AchievementDatabase.LoadSynchronous();
}

const UAchievementDefinitionAsset* UAchievementSubsystem::FindDefinitionAsset(const FName AchievementId) const
{
	const UAchievementDatabase* DB = GetAchievementDatabase();
	return DB ? DB->FindAchievementAsset(AchievementId) : nullptr;
}

FAchievementProgressState& UAchievementSubsystem::GetOrCreateProgress(const FName AchievementId)
{
	return SaveGameObject->AchievementProgress.FindOrAdd(AchievementId);
}

bool UAchievementSubsystem::SetAchievementProgressInternal(
	const FName AchievementId,
	const int32 CurrentAmount)
{
	const UAchievementDefinitionAsset* Def = FindDefinitionAsset(AchievementId);

	if (!Def || !SaveGameObject)
	{
		return false;
	}


	FAchievementProgressState& Progress =
		GetOrCreateProgress(AchievementId);


	const bool bWasUnlocked = Progress.bUnlocked;


	if (Def->Type == EAchievementType::Number)
	{
		const int32 Required = FMath::Max(Def->RequiredAmount, 1);

		Progress.CurrentAmount =
			FMath::Clamp(CurrentAmount, 0, Required);

		Progress.bUnlocked =
			Progress.CurrentAmount >= Required;
	}
	else
	{
		Progress.CurrentAmount =
			CurrentAmount > 0 ? 1 : 0;

		Progress.bUnlocked =
			Progress.CurrentAmount > 0;
	}

	// Milestone checks

	if (Def->Type == EAchievementType::Number)
	{
		float Percent =
			(float)Progress.CurrentAmount /
			(float)FMath::Max(Def->RequiredAmount, 1);



		if (Percent >= 0.25f && !Progress.b25PercentShown)
		{
			Progress.b25PercentShown = true;

			OnAchievementMilestoneReached.Broadcast(
				const_cast<UAchievementDefinitionAsset*>(Def),
				Progress
			);
		}



		if (Percent >= 0.50f && !Progress.b50PercentShown)
		{
			Progress.b50PercentShown = true;

			OnAchievementMilestoneReached.Broadcast(
				const_cast<UAchievementDefinitionAsset*>(Def),
				Progress
			);
		}



		if (Percent >= 0.75f && !Progress.b75PercentShown)
		{
			Progress.b75PercentShown = true;

			OnAchievementMilestoneReached.Broadcast(
				const_cast<UAchievementDefinitionAsset*>(Def),
				Progress
			);
		}
	}

	const bool bJustCompleted =
		!bWasUnlocked && Progress.bUnlocked;

	SaveProgress();

	// Normal progress update
	OnAchievementProgressUpdated.Broadcast(
		const_cast<UAchievementDefinitionAsset*>(Def),
		Progress
	);



	// Completed once
	if (bJustCompleted)
	{
		OnAchievementCompleted.Broadcast(
			const_cast<UAchievementDefinitionAsset*>(Def),
			Progress
		);
	}


	return true;
}

bool UAchievementSubsystem::SaveProgress() const
{
	if (!SaveGameObject)
	{
		return false;
	}

	const UAchievementPluginSettings* Settings = GetDefault<UAchievementPluginSettings>();

	return UGameplayStatics::SaveGameToSlot(
		SaveGameObject,
		Settings->SaveSlotName,
		Settings->SaveUserIndex
	);
}

void UAchievementSubsystem::LoadOrCreateSaveGame()
{
	const UAchievementPluginSettings* Settings = GetDefault<UAchievementPluginSettings>();

	if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(
		Settings->SaveSlotName,
		Settings->SaveUserIndex))
	{
		SaveGameObject = Cast<UAchievementSaveGame>(Loaded);
	}

	if (!SaveGameObject)
	{
		SaveGameObject = Cast<UAchievementSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UAchievementSaveGame::StaticClass())
		);
	}
}

bool UAchievementSubsystem::ResetAchievementProgress(const FName AchievementId)
{
	if (!SaveGameObject)
	{
		return false;
	}

	const int32 Removed = SaveGameObject->AchievementProgress.Remove(AchievementId);

	if (Removed > 0)
	{
		SaveProgress();
	}

	return Removed > 0;
}
