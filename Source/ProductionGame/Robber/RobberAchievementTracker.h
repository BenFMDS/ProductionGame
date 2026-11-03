#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RobberAchievementTracker.generated.h"

class AActor;
class APlayerState;
class ARobberGameState;
class UPlayerQuestComponent;
class UWorld;

/** How a piece of news moves an achievement. */
UENUM()
enum class ERobberAchievementStep : uint8
{
	Unlock,
	/** Counts up by one. */
	Add,
	/** Raises the count to Amount if it's lower. */
	AtLeast,
	/** Counts Detail once ever, so repeats don't add up. */
	Distinct
};

/** Which different things this player has done towards each Distinct achievement. */
UCLASS()
class PRODUCTIONGAME_API URobberAchievementSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** Entries are "AchievementId:Detail". */
	UPROPERTY() TSet<FName> Seen;
};

/**
 * Lives on each player's machine, because that's where the plugin saves progress. The server decides what happened
 * and tells each player through ARobberController::ClientAchievement; see the RobberAchievements namespace.
 */
UCLASS()
class PRODUCTIONGAME_API URobberAchievementTracker : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void Apply(FName AchievementId, ERobberAchievementStep Step, FName Detail, int32 Amount);

	/** For tests: forgets everything, including the plugin's progress. */
	void ResetAll();

private:
	UPROPERTY(Transient)
	TObjectPtr<URobberAchievementSave> Save;
};

/** Server side: what happened in the night, and who earns what for it. */
namespace RobberAchievements
{
	PRODUCTIONGAME_API void AwardCrew(const UWorld* World, FName AchievementId, ERobberAchievementStep Step = ERobberAchievementStep::Unlock, FName Detail = NAME_None, int32 Amount = 0);
	PRODUCTIONGAME_API void AwardPlayer(const APlayerState* Player, FName AchievementId, ERobberAchievementStep Step = ERobberAchievementStep::Unlock, FName Detail = NAME_None, int32 Amount = 0);

	void OnNightWon(const ARobberGameState* GameState, bool bTutorial, int32 NightNumber, float PeakHeat, bool bAlarmRaised, FName EntryId);
	void OnConversationStarted(AActor* Talker, APlayerState* Player);
	void OnQuestTurnedIn(AActor* Giver, UPlayerQuestComponent* Player);
}
