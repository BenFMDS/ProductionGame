// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberGameState.h"

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberController.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName PowerJob(TEXT("Power"));
	const FName GemJob(TEXT("Gem"));
}

ARobberGameState::ARobberGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	AlarmClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/OurStuff/Blueprints/Props/BP_Alarm.BP_Alarm_C")));
}

void ARobberGameState::BeginPlay()
{
	Super::BeginPlay();

	PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	if (HasAuthority())
	{
		SetGameStage(GetLegacyGameState());
	}
	else
	{
		SetLegacyGameState(ReplicatedGameStage);
		// Widgets created before this actor replicated in cached a null GameStateRef.
		BindLocalStageReaders();
	}
}

void ARobberGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Blueprints (e.g. BP_Gem) still write the legacy field; promote those writes to the replicated stage.
	if (HasAuthority() && GetLegacyGameState() != ReplicatedGameStage)
	{
		SetGameStage(GetLegacyGameState());
	}
	if (HasAuthority())
	{
		TickPhase();
	}
}

void ARobberGameState::TickPhase()
{
	if (ReplicatedGameStage == RobberStage::BreakIn && ActiveEntry)
	{
		ActiveEntry->TickBreakIn(this);
		if (ReplicatedGameStage == RobberStage::BreakIn && PhaseEndServerTime > 0. && GetServerWorldTimeSeconds() >= PhaseEndServerTime)
		{
			ActiveEntry->OnBreakInTimerExpired(this);
		}
	}
	// BP_PowerGenerator only flips the Blueprint PowerOn? flag, so watch it rather than wiring every generator.
	if (ReplicatedGameStage == RobberStage::Heist && !IsPowerOn())
	{
		CompleteJob(PowerJob);
	}
}

bool ARobberGameState::IsPowerOn() const
{
	const FBoolProperty* PowerProperty = FindFProperty<FBoolProperty>(GetClass(), TEXT("PowerOn?"));
	return !PowerProperty || PowerProperty->GetPropertyValue_InContainer(this);
}

void ARobberGameState::CastVote(APlayerState* Voter, ARobberEntryPoint* Entry)
{
	if (!HasAuthority() || !Voter || !Entry || ReplicatedGameStage != RobberStage::Lobby)
	{
		return;
	}
	for (FRobberEntryVote& Vote : Votes)
	{
		if (Vote.Voter == Voter)
		{
			Vote.Entry = Entry;
			ForceNetUpdate();
			return;
		}
	}
	Votes.Add({ Voter, Entry });
	ForceNetUpdate();
}

int32 ARobberGameState::GetVoteCount(const ARobberEntryPoint* Entry) const
{
	int32 Count = 0;
	for (const FRobberEntryVote& Vote : Votes)
	{
		Count += Vote.Voter && Vote.Entry == Entry;
	}
	return Count;
}

ARobberEntryPoint* ARobberGameState::GetWinningEntry() const
{
	ARobberEntryPoint* Winner = nullptr;
	int32 WinnerVotes = 0;
	for (const FRobberEntryVote& Vote : Votes)
	{
		const int32 Count = Vote.Voter ? GetVoteCount(Vote.Entry) : 0;
		if (Vote.Entry && Count > WinnerVotes)
		{
			Winner = Vote.Entry;
			WinnerVotes = Count;
		}
	}
	return Winner;
}

bool ARobberGameState::StartBreakIn()
{
	ARobberEntryPoint* Entry = GetWinningEntry();
	if (!HasAuthority() || !Entry || ReplicatedGameStage != RobberStage::Lobby)
	{
		return false;
	}

	ActiveEntry = Entry;
	Crew.Reset();
	int32 SpawnIndex = 0;
	for (APlayerState* Player : PlayerArray)
	{
		FindOrAddCrewStatus(Player);
		if (APawn* Pawn = Player ? Player->GetPawn() : nullptr)
		{
			const FTransform Spawn = Entry->GetSpawnTransform(SpawnIndex++);
			Pawn->TeleportTo(Spawn.GetLocation(), Spawn.Rotator());
			if (AController* Controller = Pawn->GetController())
			{
				Controller->ClientSetRotation(Spawn.Rotator());
			}
		}
	}
	SetGameStage(RobberStage::BreakIn);
	Entry->BeginBreakIn(this);
	return true;
}

void ARobberGameState::FinishBreakIn()
{
	if (!HasAuthority() || ReplicatedGameStage != RobberStage::BreakIn)
	{
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("Break-in finished with %.1fs left on the timer. Crew: %s"), GetPhaseTimeRemaining(),
		*FString::JoinBy(Crew, TEXT(", "), [](const FRobberCrewStatus& Status)
		{
			return FString::Printf(TEXT("%s ticket=%d hidden=%d"), Status.Player ? *Status.Player->GetPlayerName() : TEXT("?"), Status.bHasTicket, Status.HidingSpot != nullptr);
		}));
	PhaseEndServerTime = 0.;
	if (ActiveEntry)
	{
		ActiveEntry->EndBreakIn(this);
	}
	SetGameStage(RobberStage::Investigate);
}

void ARobberGameState::SetPhaseTimer(float Seconds)
{
	if (HasAuthority())
	{
		PhaseEndServerTime = Seconds > 0.f ? GetServerWorldTimeSeconds() + Seconds : 0.;
		ForceNetUpdate();
	}
}

float ARobberGameState::GetPhaseTimeRemaining() const
{
	return PhaseEndServerTime > 0. ? FMath::Max(0., PhaseEndServerTime - GetServerWorldTimeSeconds()) : -1.f;
}

FRobberCrewStatus ARobberGameState::GetCrewStatus(const APlayerState* Player) const
{
	const FRobberCrewStatus* Status = Crew.FindByPredicate([Player](const FRobberCrewStatus& Entry) { return Entry.Player == Player; });
	return Status ? *Status : FRobberCrewStatus();
}

FRobberCrewStatus* ARobberGameState::FindOrAddCrewStatus(APlayerState* Player)
{
	if (!Player)
	{
		return nullptr;
	}
	FRobberCrewStatus* Status = Crew.FindByPredicate([Player](const FRobberCrewStatus& Entry) { return Entry.Player == Player; });
	if (!Status)
	{
		Status = &Crew.AddDefaulted_GetRef();
		Status->Player = Player;
	}
	return Status;
}

void ARobberGameState::GiveTicket(APlayerState* Player)
{
	if (FRobberCrewStatus* Status = HasAuthority() ? FindOrAddCrewStatus(Player) : nullptr)
	{
		Status->bHasTicket = true;
		ForceNetUpdate();
	}
}

void ARobberGameState::SetHidingSpot(APlayerState* Player, AActor* HidingSpot)
{
	if (FRobberCrewStatus* Status = HasAuthority() ? FindOrAddCrewStatus(Player) : nullptr)
	{
		Status->HidingSpot = HidingSpot;
		ForceNetUpdate();
	}
}

bool ARobberGameState::CanTalkToExhibits() const
{
	return ReplicatedGameStage != RobberStage::BreakIn && ReplicatedGameStage != RobberStage::Investigate;
}

void ARobberGameState::StartHeist()
{
	if (!HasAuthority() || ReplicatedGameStage != RobberStage::Investigate)
	{
		return;
	}
	Jobs = {
		{ PowerJob, NSLOCTEXT("RobberHeist", "PowerJob", "Turn off the power"), false },
		{ GemJob, NSLOCTEXT("RobberHeist", "GemJob", "Steal the gem"), false },
	};
	OnRep_Jobs();
	SetGameStage(RobberStage::Heist);
}

void ARobberGameState::CompleteJob(FName JobId)
{
	FRobberJob* Job = HasAuthority() ? Jobs.FindByPredicate([JobId](const FRobberJob& Entry) { return Entry.Id == JobId; }) : nullptr;
	if (Job && !Job->bDone)
	{
		Job->bDone = true;
		ForceNetUpdate();
		OnRep_Jobs();
	}
}

void ARobberGameState::OnRep_Jobs()
{
	OnJobsChanged.Broadcast();
}

FText ARobberGameState::GetObjectiveText(const APlayerState* Viewer) const
{
	switch (ReplicatedGameStage)
	{
	case RobberStage::Lobby:
		return NSLOCTEXT("RobberHeist", "LobbyObjective", "Vote for a way in, then walk through the museum door together.");
	case RobberStage::BreakIn:
		return ActiveEntry ? ActiveEntry->GetBreakInStatus(this, Viewer) : FText::GetEmpty();
	case RobberStage::Investigate:
		return NSLOCTEXT("RobberHeist", "InvestigateObjective", "Sneak to the gem room door. The exhibits won't talk to you yet.");
	case RobberStage::Heist:
		return NSLOCTEXT("RobberHeist", "HeistObjective", "Do the jobs. The exhibits will talk now.");
	case RobberStage::Escape:
		return NSLOCTEXT("RobberHeist", "EscapeObjective", "Get out the way you came in!");
	default:
		return FText::GetEmpty();
	}
}

void ARobberGameState::ActivateAlarms()
{
	UClass* LoadedAlarmClass = AlarmClass.LoadSynchronous();
	if (!LoadedAlarmClass || !GetWorld())
	{
		return;
	}

	// BP_Alarm doesn't replicate, so its server/multicast RPCs never leave the server. Every machine
	// runs this on the stage change and calls the multicast body on its own level-placed copies.
	for (TActorIterator<AActor> It(GetWorld(), LoadedAlarmClass); It; ++It)
	{
		if (UFunction* ActivateAlarm = It->FindFunction(TEXT("ActivateAlarmMulti")))
		{
			It->ProcessEvent(ActivateAlarm, nullptr);
		}
	}
}

void ARobberGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberGameState, ReplicatedGameStage);
	DOREPLIFETIME(ARobberGameState, Votes);
	DOREPLIFETIME(ARobberGameState, ActiveEntry);
	DOREPLIFETIME(ARobberGameState, Crew);
	DOREPLIFETIME(ARobberGameState, Jobs);
	DOREPLIFETIME(ARobberGameState, PhaseEndServerTime);
}

void ARobberGameState::SetGameStage(uint8 NewGameStage)
{
	const bool bStageChanged = ReplicatedGameStage != NewGameStage;
	ReplicatedGameStage = NewGameStage;
	SetLegacyGameState(NewGameStage);
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	// Clients react in OnRep; this covers the listen-server host.
	if (bStageChanged)
	{
		HandleGameStageChanged();
	}
}

void ARobberGameState::OnRep_GameState()
{
	// The old Blueprint declared this callback but intentionally left it empty.
}

void ARobberGameState::OnRep_ReplicatedGameStage()
{
	SetLegacyGameState(ReplicatedGameStage);
	OnRep_GameState();
	HandleGameStageChanged();
}

void ARobberGameState::HandleGameStageChanged()
{
	BindLocalStageReaders();
	NotifyDevUIOfStageChange();
	if (ReplicatedGameStage == RobberStage::Escape)
	{
		ActivateAlarms();
		CompleteJob(GemJob);
	}
	OnGameStageChanged.Broadcast(ReplicatedGameStage);
}

void ARobberGameState::BindLocalStageReaders()
{
	if (!PlayerControllerRef)
	{
		PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	}
	if (!PlayerControllerRef)
	{
		return;
	}

	// BP_RobberController (read by WBP_DevUI) and WBP_GameUI each cache GameStateRef once, at BeginPlay/Construct.
	UObject* Readers[] = { PlayerControllerRef, PlayerControllerRef->GetGameUIWidget() };
	for (UObject* Reader : Readers)
	{
		FObjectPropertyBase* RefProperty = Reader ? FindFProperty<FObjectPropertyBase>(Reader->GetClass(), TEXT("GameStateRef")) : nullptr;
		if (RefProperty && IsA(RefProperty->PropertyClass) && !RefProperty->GetObjectPropertyValue_InContainer(Reader))
		{
			RefProperty->SetObjectPropertyValue_InContainer(Reader, this);
		}
	}
}

uint8 ARobberGameState::GetLegacyGameState() const
{
	const FByteProperty* LegacyProperty = FindFProperty<FByteProperty>(GetClass(), TEXT("GameState"));
	return LegacyProperty ? LegacyProperty->GetPropertyValue_InContainer(this) : ReplicatedGameStage;
}

void ARobberGameState::SetLegacyGameState(uint8 NewGameStage)
{
	if (FByteProperty* LegacyProperty = FindFProperty<FByteProperty>(GetClass(), TEXT("GameState")))
	{
		LegacyProperty->SetPropertyValue_InContainer(this, NewGameStage);
	}
}

void ARobberGameState::NotifyDevUIOfStageChange()
{
	if (!PlayerControllerRef)
	{
		PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	UUserWidget* DevUI = PlayerControllerRef ? PlayerControllerRef->GetDevUIWidget() : nullptr;
	if (DevUI)
	{
		// Must match the custom event name in WBP_DevUI exactly, spaces included.
		UFunction* UpdateStageText = DevUI->FindFunction(TEXT("Update StageText Server"));
		if (UpdateStageText && UpdateStageText->ParmsSize == 0)
		{
			DevUI->ProcessEvent(UpdateStageText, nullptr);
		}

		// The widget's Switch on E_GameStage has no pin for stages added after it was built (Investigate), so
		// it leaves the text blank for them. Write the label from C++ so every stage shows.
		const FObjectPropertyBase* TextProperty = FindFProperty<FObjectPropertyBase>(DevUI->GetClass(), TEXT("TXT_GameStage"));
		if (UTextBlock* StageText = TextProperty ? Cast<UTextBlock>(TextProperty->GetObjectPropertyValue_InContainer(DevUI)) : nullptr)
		{
			StageText->SetText(FText::Format(NSLOCTEXT("RobberStage", "DevUIStage", "Game Stage: {0}"), RobberStage::GetDisplayName(ReplicatedGameStage)));
		}
	}
}
