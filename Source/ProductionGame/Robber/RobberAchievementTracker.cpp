#include "Robber/RobberAchievementTracker.h"

#include "AchievementSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

namespace RobberAchievementDetail
{
	const TCHAR* SlotName = TEXT("RobberAchievementTracker");

	/** Blueprint exhibits are told apart by class: BP_NPC_Winton_C gives "Winton". */
	FString ExhibitId(const AActor* Talker)
	{
		FString Name = Talker ? Talker->GetClass()->GetName() : FString();
		Name.RemoveFromStart(TEXT("BP_NPC_"));
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}
}

void URobberAchievementTracker::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Save = UGameplayStatics::DoesSaveGameExist(RobberAchievementDetail::SlotName, 0)
		? Cast<URobberAchievementSave>(UGameplayStatics::LoadGameFromSlot(RobberAchievementDetail::SlotName, 0)) : nullptr;
	if (!Save)
	{
		Save = NewObject<URobberAchievementSave>(this);
	}
}

void URobberAchievementTracker::Apply(FName AchievementId, ERobberAchievementStep Step, FName Detail, int32 Amount)
{
	UAchievementSubsystem* Achievements = GetGameInstance()->GetSubsystem<UAchievementSubsystem>();
	if (!Achievements)
	{
		return;
	}
	switch (Step)
	{
	case ERobberAchievementStep::Unlock:
		Achievements->UnlockAchievement(AchievementId);
		break;
	case ERobberAchievementStep::Add:
		Achievements->AddAchievementProgress(AchievementId, 1);
		break;
	case ERobberAchievementStep::AtLeast:
		if (Amount > Achievements->GetAchievementCurrentAmount(AchievementId))
		{
			Achievements->UpdateAchievementProgress(AchievementId, Amount);
		}
		break;
	case ERobberAchievementStep::Distinct:
	{
		const FName Entry(*FString::Printf(TEXT("%s:%s"), *AchievementId.ToString(), *Detail.ToString()));
		if (!Save->Seen.Contains(Entry))
		{
			Save->Seen.Add(Entry);
			UGameplayStatics::SaveGameToSlot(Save, RobberAchievementDetail::SlotName, 0);
			Achievements->AddAchievementProgress(AchievementId, 1);
		}
		break;
	}
	}
}

void URobberAchievementTracker::ResetAll()
{
	Save->Seen.Reset();
	UGameplayStatics::SaveGameToSlot(Save, RobberAchievementDetail::SlotName, 0);
	if (UAchievementSubsystem* Achievements = GetGameInstance()->GetSubsystem<UAchievementSubsystem>())
	{
		Achievements->ResetAllAchievementProgress();
	}
}

void RobberAchievements::AwardCrew(const UWorld* World, FName AchievementId, ERobberAchievementStep Step, FName Detail, int32 Amount)
{
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ARobberController* Controller = Cast<ARobberController>(It->Get()))
		{
			Controller->ClientAchievement(AchievementId, Step, Detail, Amount);
		}
	}
}

void RobberAchievements::AwardPlayer(const APlayerState* Player, FName AchievementId, ERobberAchievementStep Step, FName Detail, int32 Amount)
{
	if (ARobberController* Controller = Player ? Cast<ARobberController>(Player->GetOwningController()) : nullptr)
	{
		Controller->ClientAchievement(AchievementId, Step, Detail, Amount);
	}
}

void RobberAchievements::OnNightWon(const ARobberGameState* GameState, bool bTutorial, int32 NightNumber, float PeakHeat, bool bAlarmRaised, FName EntryId)
{
	const UWorld* World = GameState ? GameState->GetWorld() : nullptr;
	if (!World || bTutorial)
	{
		return;
	}
	AwardCrew(World, TEXT("OpeningNight"));
	AwardCrew(World, TEXT("SevenNights"), ERobberAchievementStep::AtLeast, NAME_None, NightNumber);
	if (GameState->PlayerArray.Num() >= 4)
	{
		AwardCrew(World, TEXT("FullCrew"));
	}
	if (PeakHeat < 25.f)
	{
		AwardCrew(World, TEXT("QuietNight"));
	}
	if (bAlarmRaised)
	{
		AwardCrew(World, TEXT("ThievesAtLarge"));
	}
	if (!EntryId.IsNone())
	{
		AwardCrew(World, TEXT("EveryWayIn"), ERobberAchievementStep::Distinct, EntryId);
	}
}

void RobberAchievements::OnConversationStarted(AActor* Talker, APlayerState* Player)
{
	const FString Exhibit = RobberAchievementDetail::ExhibitId(Talker);
	if (Exhibit.IsEmpty() || !Player)
	{
		return;
	}
	AwardPlayer(Player, TEXT("GoodListener"), ERobberAchievementStep::Distinct, *Exhibit);
	if (Exhibit == TEXT("Ghostface"))
	{
		AwardPlayer(Player, TEXT("WrongScream"));
	}
	else if (Exhibit == TEXT("Winton"))
	{
		AwardPlayer(Player, TEXT("WhoIsWinton"));
	}
	static const TSet<FString> Presidents = { TEXT("BarackObama"), TEXT("BillClinton"), TEXT("GeorgeHWBush"), TEXT("GeorgeWBush"), TEXT("RichardNixon"), TEXT("RonaldReagan"),
		TEXT("WilliamHowardTaft"), TEXT("WilliamHenryHarrison"), TEXT("CalvinCoolidge"), TEXT("GroverCleveland") };
	if (Presidents.Contains(Exhibit))
	{
		AwardPlayer(Player, TEXT("PresidentialPardon"), ERobberAchievementStep::Distinct, *Exhibit);
	}
}

void RobberAchievements::OnQuestTurnedIn(AActor* Giver, UPlayerQuestComponent* Player)
{
	if (ARobberController* Controller = Player ? Cast<ARobberController>(Player->GetOwner()) : nullptr)
	{
		Controller->ClientAchievement(TEXT("ErrandRunner"), ERobberAchievementStep::Add, NAME_None, 0);
	}
}
