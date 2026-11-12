// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberGameState.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberAchievementTracker.h"

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Components/LightComponent.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberExhibitTalk.h"
#include "Heist/RobberStage.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberSecurityCamera.h"
#include "Heist/RobberVault.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "UObject/UnrealType.h"
#include "ProductionGameCharacter.h"

namespace
{
	const FName PowerJob(TEXT("Power"));
	const FName GemJob(TEXT("Gem"));
	const FName PowerLightTag(TEXT("PowerLight"));

	void PlacePawn(APawn* Pawn, const FVector& Location, const FRotator& Rotation)
	{
		Pawn->TeleportTo(Location, Rotation);
		if (AController* Controller = Pawn->GetController())
		{
			Controller->ClientSetRotation(Rotation);
		}
	}

	// Dev shortcuts until the real win/lose conditions exist. Type them on the host.
	void RunOnHostGameState(UWorld* World, TFunctionRef<void(ARobberGameState&)> Command)
	{
		ARobberGameState* GameState = World ? World->GetGameState<ARobberGameState>() : nullptr;
		if (GameState && GameState->HasAuthority())
		{
			Command(*GameState);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Robber night commands only work on the host."));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs FinishNightCommand(TEXT("Robber.FinishNight"),
		TEXT("End the current night. 'Robber.FinishNight 1' wins it, 'Robber.FinishNight 0' loses it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunOnHostGameState(World, [&Args](ARobberGameState& GameState) { GameState.FinishNight(Args.Num() > 0 && Args[0].ToBool()); });
		}));

	FAutoConsoleCommandWithWorldAndArgs LoadNightCommand(TEXT("Robber.LoadNight"),
		TEXT("Load a night, dropping any night in progress: 'Robber.LoadNight 2' for Night 2."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunOnHostGameState(World, [&Args](ARobberGameState& GameState) { GameState.SwitchNight(Args.Num() > 0 ? FMath::Max(FCString::Atoi(*Args[0]) - 1, 0) : 0); });
		}));

	FAutoConsoleCommandWithWorldAndArgs LoadTutorialCommand(TEXT("Robber.LoadTutorial"),
		TEXT("Load the tutorial, dropping any night in progress."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			RunOnHostGameState(World, [](ARobberGameState& GameState) { GameState.SwitchNight(INDEX_NONE); });
		}));
}

ARobberGameState::ARobberGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	AlarmClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/OurStuff/Blueprints/Props/BP_Alarm.BP_Alarm_C")));
	// The museum greybox is Night 1 and L_NightN is Night N. L_Night1 isn't played (its game loop was lost in a map merge).
	Nights.Emplace(FSoftObjectPath(TEXT("/Game/OurStuff/Maps/DevLevels/L_MuseumGreybox.L_MuseumGreybox")));
	for (int32 Night = 2; Night <= 7; ++Night)
	{
		Nights.Emplace(FSoftObjectPath(FString::Printf(TEXT("/Game/OurStuff/Maps/L_Night%d.L_Night%d"), Night, Night)));
	}
	TutorialLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/OurStuff/Maps/L_Tutorial.L_Tutorial")));
}

void ARobberGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDialogueComponent::OnConversationStarted.Remove(AchievementTalkHandle);
	UPlayerQuestComponent::OnQuestTurnedIn.Remove(AchievementQuestHandle);
	Super::EndPlay(EndPlayReason);
}

void ARobberGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		AchievementTalkHandle = UDialogueComponent::OnConversationStarted.AddStatic(&RobberAchievements::OnConversationStarted);
		AchievementQuestHandle = UPlayerQuestComponent::OnQuestTurnedIn.AddStatic(&RobberAchievements::OnQuestTurnedIn);
	}

	PlayerControllerRef = Cast<ARobberController>(UGameplayStatics::GetPlayerController(this, 0));
	if (HasAuthority())
	{
		SetGameStage(GetLegacyGameState());
		// Maps without the night sublevels (the dev levels) keep everything in one level.
		LoadNight(0);
#if WITH_EDITOR
		// A night map played on its own in the editor has no lobby to leave, so it would sit in the lobby stage, where
		// hiding, talking and quests are all off. Start it mid-heist instead. L_GameLevel streams nights and isn't affected.
		const bool bStreamsNights = Nights.IsValidIndex(0) && CanStreamLevel(Nights[0]);
		if (GetWorld()->IsPlayInEditor() && !bStreamsNights && RobberStage::IsInLobby(ReplicatedGameStage)
			&& TActorIterator<ARobberNightDirector>(GetWorld()))
		{
			SetGameStage(RobberStage::Heist);
		}
#endif
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
		TickHeat(DeltaSeconds);
	}
	TickHeatTier();
}

ERobberHeatTier ARobberGameState::GetHeatTier() const
{
	if (bAlarmRaised || Heat >= MaxHeat)
	{
		return ERobberHeatTier::Alarm;
	}
	return Heat >= RobberHeatCosts::AlertAt ? ERobberHeatTier::Alert
		: Heat >= RobberHeatCosts::SuspiciousAt ? ERobberHeatTier::Suspicious : ERobberHeatTier::Calm;
}

void ARobberGameState::TickHeatTier()
{
	const ERobberHeatTier Tier = GetHeatTier();
	if (Tier == LastHeatTier)
	{
		return;
	}
	const ERobberHeatTier Old = LastHeatTier;
	LastHeatTier = Tier;
	if (HasAuthority() && !RobberStage::IsInLobby(ReplicatedGameStage))
	{
		if (Tier == ERobberHeatTier::Alert)
		{
			bReachedAlert = true;
		}
		// Only going up gets a line; the alarm has its own.
		if (Tier > Old && Tier == ERobberHeatTier::Suspicious)
		{
			TellCrew(NSLOCTEXT("RobberHeat", "TierSuspicious", "A guard thinks they heard something. Keep it down."));
		}
		else if (Tier > Old && Tier == ERobberHeatTier::Alert)
		{
			TellCrew(NSLOCTEXT("RobberHeat", "TierAlert", "The museum's on alert: the cameras are sweeping faster."));
		}
	}
	OnHeatTierChanged.Broadcast(Tier, Old);
}

void ARobberGameState::ReportSighting(FVector Location, AActor* ReportedBy)
{
	if (!HasAuthority())
	{
		return;
	}
	++SightingCount;
	LastSighting = Location;
	UE_LOG(LogTemp, Log, TEXT("Sighting at %s by %s"), *Location.ToCompactString(), *GetNameSafe(ReportedBy));
	OnInvestigate.Broadcast(Location, ReportedBy);
}

void ARobberGameState::DisturbanceChecked(FVector Location, float Radius)
{
	if (!HasAuthority())
	{
		return;
	}
	for (TActorIterator<ARobberSecurityCamera> It(GetWorld()); It; ++It)
	{
		if (It->IsAwaitingGuardNear(Location, Radius))
		{
			It->ResumeScanning();
		}
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
	if (!RobberStage::IsInLobby(ReplicatedGameStage) && !IsPowerOn() && !bPowerCut)
	{
		bPowerCut = true;
		ForceNetUpdate();
		ApplyPowerCut();
		RobberAchievements::AwardCrew(GetWorld(), TEXT("LightsOut"));
	}
	if (bAlarmRaised && !RobberStage::IsInLobby(ReplicatedGameStage) && GuardsArriveServerTime > 0. && GetServerWorldTimeSeconds() >= GuardsArriveServerTime)
	{
		GuardsArriveServerTime = 0.;
		TellCrew(NSLOCTEXT("RobberHeist", "GuardsArrived", "The guards got here first."));
		FinishNight(false);
	}
}

void ARobberGameState::TickHeat(float DeltaSeconds)
{
	// Cools once the crew has been quiet a while; stays full once the alarm's up.
	const float Floor = bReachedAlert ? RobberHeatCosts::SuspiciousAt : 0.f;
	if (Heat > Floor && !bAlarmRaised && GetWorld()->GetTimeSeconds() - LastHeatTime > HeatCoolDelay)
	{
		const float Rate = HeatCoolPerSecond * (GetHeatTier() == ERobberHeatTier::Alert ? AlertCoolScale : 1.f);
		Heat = FMath::Max(Floor, Heat - Rate * DeltaSeconds);
	}
}

void ARobberGameState::AddHeat(float Amount, const TCHAR* Source)
{
	if (!HasAuthority() || Amount <= 0.f || bAlarmRaised || RobberStage::IsInLobby(ReplicatedGameStage))
	{
		return;
	}
	Heat = FMath::Min(MaxHeat, Heat + Amount);
	PeakHeat = FMath::Max(PeakHeat, Heat);
	LastHeatTime = GetWorld()->GetTimeSeconds();
	ForceNetUpdate();
	UE_LOG(LogTemp, Verbose, TEXT("Heat +%.1f from %s: %.1f"), Amount, Source, Heat);
	if (Heat >= MaxHeat)
	{
		RaiseAlarm(FText::Format(NSLOCTEXT("RobberHeat", "TooHot", "Too much noise: the alarm's gone off! Guards in {0} seconds."), FMath::RoundToInt(GuardsArriveSeconds)));
	}
}

void ARobberGameState::CoolHeat(float Amount)
{
	if (!HasAuthority() || Amount <= 0.f || bAlarmRaised)
	{
		return;
	}
	Heat = FMath::Max(0.f, Heat - Amount);
	ForceNetUpdate();
}

void ARobberGameState::AddGuardsTime(float Seconds)
{
	if (!HasAuthority() || Seconds <= 0.f)
	{
		return;
	}
	if (bAlarmRaised)
	{
		GuardsArriveServerTime += Seconds;
		ForceNetUpdate();
	}
	else
	{
		BonusGuardsSeconds += Seconds;
	}
}

void ARobberGameState::RaiseAlarm(const FText& Why)
{
	if (!HasAuthority() || bAlarmRaised)
	{
		return;
	}
	bAlarmRaised = true;
	Heat = MaxHeat;
	GuardsArriveServerTime = GetServerWorldTimeSeconds() + GuardsArriveSeconds + BonusGuardsSeconds;
	ForceNetUpdate();
	ActivateAlarms();
	TellCrew(Why);
}

float ARobberGameState::GetGuardsTimeRemaining() const
{
	return bAlarmRaised && GuardsArriveServerTime > 0. ? FMath::Max(0., GuardsArriveServerTime - GetServerWorldTimeSeconds()) : -1.f;
}

void ARobberGameState::TripLaser(APawn* Pawn)
{
	const bool bInMuseum = ReplicatedGameStage == RobberStage::Heist || ReplicatedGameStage == RobberStage::Escape;
	if (!HasAuthority() || !bInMuseum || bAlarmRaised || bPowerCut)
	{
		return;
	}
	const APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	RobberAchievements::AwardPlayer(Player, TEXT("TrippedUp"));
	RaiseAlarm(FText::Format(NSLOCTEXT("RobberHeist", "LaserTripped", "{0} walked into a laser! Guards in {1} seconds."),
		Player ? FText::FromString(Player->GetPlayerName()) : NSLOCTEXT("RobberHeist", "Someone", "Someone"), FMath::RoundToInt(GuardsArriveSeconds)));
}

void ARobberGameState::OnRep_AlarmRaised()
{
	if (bAlarmRaised)
	{
		ActivateAlarms();
	}
}

void ARobberGameState::OnRep_PowerCut()
{
	if (bPowerCut)
	{
		ApplyPowerCut();
	}
}

void ARobberGameState::ApplyPowerCut()
{
	// Each night's room lights are tagged PowerLight; they drop to a dim red emergency glow. The retag stops a second
	// call dimming them again; a reloaded night brings fresh lights.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->Tags.Contains(PowerLightTag))
		{
			continue;
		}
		for (UActorComponent* Component : It->GetComponents())
		{
			if (ULightComponent* Light = Cast<ULightComponent>(Component))
			{
				Light->SetIntensity(Light->Intensity * 0.12f);
				Light->SetLightColor(FLinearColor(1.f, 0.12f, 0.08f));
			}
		}
		It->Tags.Remove(PowerLightTag);
		It->Tags.Add(TEXT("PowerLightCut"));
	}
}

bool ARobberGameState::IsPowerOn() const
{
	const FBoolProperty* PowerProperty = FindFProperty<FBoolProperty>(GetClass(), TEXT("PowerOn?"));
	return !PowerProperty || PowerProperty->GetPropertyValue_InContainer(this);
}

void ARobberGameState::CastVote(APlayerState* Voter, ARobberEntryPoint* Entry)
{
	if (!HasAuthority() || !Voter || !Entry || !RobberStage::IsInLobby(ReplicatedGameStage))
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
	if (!HasAuthority() || !Entry || !RobberStage::IsInLobby(ReplicatedGameStage))
	{
		return false;
	}

	ActiveEntry = Entry;
	NightResultText = FText::GetEmpty();
	Crew.Reset();
	int32 SpawnIndex = 0;
	for (APlayerState* Player : PlayerArray)
	{
		FindOrAddCrewStatus(Player);
		if (APawn* Pawn = Player ? Player->GetPawn() : nullptr)
		{
			const FTransform Spawn = Entry->GetSpawnTransform(SpawnIndex++);
			PlacePawn(Pawn, Spawn.GetLocation(), Spawn.Rotator());
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
			return FString::Printf(TEXT("%s ticket=%d hidden=%d inside=%d"), Status.Player ? *Status.Player->GetPlayerName() : TEXT("?"), Status.bHasTicket, Status.HidingSpot != nullptr, Status.bInside);
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

void ARobberGameState::SetInside(APlayerState* Player)
{
	FRobberCrewStatus* Status = HasAuthority() ? FindOrAddCrewStatus(Player) : nullptr;
	if (Status && !Status->bInside)
	{
		Status->bInside = true;
		ForceNetUpdate();
	}
}

void ARobberGameState::TellCrew(const FText& Message) const
{
	for (const APlayerState* Player : PlayerArray)
	{
		ARobberController::Tell(Player, Message);
	}
}

TArray<APlayerState*> ARobberGameState::GetPlayersInIdOrder() const
{
	TArray<APlayerState*> Players(PlayerArray);
	Players.Remove(nullptr);
	Players.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Players;
}

TArray<TPair<const ARobberEntryPoint*, int32>> ARobberGameState::GetVoteTallies() const
{
	TArray<TPair<const ARobberEntryPoint*, int32>> Tallies;
	for (const FRobberEntryVote& Vote : Votes)
	{
		if (Vote.Entry && !Tallies.ContainsByPredicate([&Vote](const TPair<const ARobberEntryPoint*, int32>& Each) { return Each.Key == Vote.Entry; }))
		{
			Tallies.Emplace(Vote.Entry, GetVoteCount(Vote.Entry));
		}
	}
	return Tallies;
}

bool ARobberGameState::CanTalkToExhibits() const
{
	return ReplicatedGameStage != RobberStage::BreakIn && ReplicatedGameStage != RobberStage::Investigate;
}

bool ARobberGameState::IsExhibitSmallTalk() const
{
	return ReplicatedGameStage == RobberStage::Investigate;
}

bool ARobberGameState::CanChatWithExhibits() const
{
	return CanTalkToExhibits() || IsExhibitSmallTalk();
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
	// A night with a vault door has the crew crack it first.
	for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
	{
		if (!It->IsOpen())
		{
			Jobs.Insert({ ARobberVaultDoor::JobId, NSLOCTEXT("RobberHeist", "VaultJob", "Open the vault (the exhibits know the code)"), false }, 0);
			break;
		}
	}
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
	case RobberStage::Lose:
	case RobberStage::Continue:
	case RobberStage::Win:
	{
		FText Objective = NSLOCTEXT("RobberHeist", "LobbyObjective", "Vote for a way in, then walk through the museum door together.");
		if (CurrentNight != INDEX_NONE || bTutorial)
		{
			Objective = FText::Format(NSLOCTEXT("RobberHeist", "UpNext", "Up next: {0}\n{1}"), GetNightName(CurrentNight, bTutorial), Objective);
		}
		return NightResultText.IsEmpty() ? Objective : FText::Format(NSLOCTEXT("RobberHeist", "WithResult", "{0}\n{1}"), NightResultText, Objective);
	}
	case RobberStage::BreakIn:
		return ActiveEntry ? ActiveEntry->GetBreakInStatus(this, Viewer) : FText::GetEmpty();
	case RobberStage::Investigate:
		return NSLOCTEXT("RobberHeist", "InvestigateObjective", "Sneak to the gem room door. The exhibits only want to chat for now.");
	case RobberStage::Heist:
		return bAlarmRaised ? NSLOCTEXT("RobberHeist", "HeistTripped", "The alarm's gone off! Grab the gem and get out before the guards arrive.")
			: NSLOCTEXT("RobberHeist", "HeistObjective", "Do the jobs. The exhibits will talk now.");
	case RobberStage::Escape:
		return bAlarmRaised ? NSLOCTEXT("RobberHeist", "EscapeTripped", "Get out the way you came in, before the guards arrive!")
			: NSLOCTEXT("RobberHeist", "EscapeObjective", "Get out the way you came in!");
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

void ARobberGameState::FinishNight(bool bWon)
{
	if (!HasAuthority() || RobberStage::IsInLobby(ReplicatedGameStage))
	{
		return;
	}
	const bool bLastNight = !bTutorial && CurrentNight >= Nights.Num() - 1;
	if (bWon)
	{
		RobberAchievements::OnNightWon(this, bTutorial, CurrentNight + 1, PeakHeat, bAlarmRaised, ActiveEntry ? ActiveEntry->EntryId : NAME_None);
	}
	SetGameStage(!bWon ? RobberStage::Lose : bLastNight ? RobberStage::Win : RobberStage::Continue);
}

void ARobberGameState::ReturnToLobby()
{
	NightResultText = FText::Format(ReplicatedGameStage == RobberStage::Lose ? NSLOCTEXT("RobberNight", "Lost", "{0} lost. Try it again.")
		: ReplicatedGameStage == RobberStage::Continue ? NSLOCTEXT("RobberNight", "Won", "{0} done!")
		: NSLOCTEXT("RobberNight", "WonLast", "{0} done! That was the last night."), GetNightName(CurrentNight, bTutorial));
	ResetCrewToLobby();
	// A lost night reloads, so nothing from the failed attempt carries over.
	if (ReplicatedGameStage == RobberStage::Lose && bTutorial)
	{
		LoadTutorial();
	}
	else
	{
		LoadNight(ReplicatedGameStage == RobberStage::Lose ? CurrentNight : ReplicatedGameStage == RobberStage::Win || bTutorial ? 0 : CurrentNight + 1);
	}
	ForceNetUpdate();
}

void ARobberGameState::ResetCrewToLobby()
{
	if (ActiveEntry)
	{
		ActiveEntry->EndBreakIn(this);
	}
	ActiveEntry = nullptr;
	for (TActorIterator<AProductionGameCharacter> It(GetWorld()); It; ++It)
	{
		It->ExitHiding(false);
	}
	Votes.Reset();
	Crew.Reset();
	Jobs.Reset();
	OnRep_Jobs();
	PhaseEndServerTime = 0.;
	bPowerCut = false;
	bAlarmRaised = false;
	Heat = 0.f;
	PeakHeat = 0.f;
	bReachedAlert = false;
	SightingCount = 0;
	GuardsArriveServerTime = 0.;
	BonusGuardsSeconds = 0.f;
	if (FBoolProperty* PowerProperty = FindFProperty<FBoolProperty>(GetClass(), TEXT("PowerOn?")))
	{
		PowerProperty->SetPropertyValue_InContainer(this, true);
	}

	// No transition yet: everyone just appears back in the lobby, one start each.
	TArray<APlayerStart*> LobbyStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == TEXT("Lobby"))
		{
			LobbyStarts.Add(*It);
		}
	}
	int32 StartIndex = 0;
	for (APlayerState* Player : PlayerArray)
	{
		if (URobberJournalComponent* Journal = URobberJournalComponent::Of(Player))
		{
			Journal->ResetForNight();
		}
		if (UPlayerQuestComponent* Quests = UPlayerQuestComponent::Of(Player))
		{
			Quests->ResetForNight();
		}
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (Pawn && LobbyStarts.Num() > 0)
		{
			const APlayerStart* Start = LobbyStarts[StartIndex++ % LobbyStarts.Num()];
			PlacePawn(Pawn, Start->GetActorLocation(), Start->GetActorRotation());
		}
	}
}

void ARobberGameState::SwitchNight(int32 NightIndex)
{
	if (!HasAuthority())
	{
		return;
	}
	if (!RobberStage::IsInLobby(ReplicatedGameStage))
	{
		// Straight to Lobby, not via a result stage, so ReturnToLobby doesn't load a night of its own.
		SetGameStage(RobberStage::Lobby);
		ResetCrewToLobby();
	}
	NightResultText = FText::GetEmpty();
	if (NightIndex == INDEX_NONE)
	{
		LoadTutorial();
	}
	else
	{
		LoadNight(NightIndex);
	}
	ForceNetUpdate();
}

void ARobberGameState::LoadNight(int32 NightIndex)
{
	if (!HasAuthority() || !RobberStage::IsInLobby(ReplicatedGameStage) || !Nights.IsValidIndex(NightIndex) || !CanStreamLevel(Nights[NightIndex]))
	{
		return;
	}
	CurrentNight = NightIndex;
	bTutorial = false;
	SwapNightLevel(Nights[NightIndex]);
}

void ARobberGameState::LoadTutorial()
{
	if (!HasAuthority() || !RobberStage::IsInLobby(ReplicatedGameStage) || !CanStreamLevel(TutorialLevel))
	{
		return;
	}
	CurrentNight = INDEX_NONE;
	bTutorial = true;
	SwapNightLevel(TutorialLevel);
}

bool ARobberGameState::CanStreamLevel(const TSoftObjectPtr<UWorld>& Level) const
{
	return !Level.IsNull() && UGameplayStatics::GetStreamingLevel(this, Level.ToSoftObjectPath().GetLongPackageFName()) != nullptr;
}

void ARobberGameState::SwapNightLevel(const TSoftObjectPtr<UWorld>& Level)
{
	// Votes point at the old night's entries.
	Votes.Reset();
	ForceNetUpdate();
	PendingNightLevel = Level.ToSoftObjectPath().GetLongPackageFName();
	GetWorldTimerManager().ClearTimer(NightLoadTimer);
	if (LoadedNightLevel.IsNone())
	{
		LoadPendingNightLevel();
		return;
	}
	FLatentActionInfo LatentInfo(0, NextLatentActionId++, TEXT("OnNightLevelUnloaded"), this);
	UGameplayStatics::UnloadStreamLevel(this, LoadedNightLevel, LatentInfo, false);
	LoadedNightLevel = NAME_None;
}

void ARobberGameState::OnNightLevelUnloaded()
{
	// An unloaded level that hasn't been garbage collected yet is reused as it was left, so a lost night
	// would come back with the gem gone. Collect it first and give clients the same moment to.
	GEngine->ForceGarbageCollection(true);
	GetWorldTimerManager().SetTimer(NightLoadTimer, this, &ARobberGameState::LoadPendingNightLevel, 0.5f, false);
}

void ARobberGameState::LoadPendingNightLevel()
{
	if (PendingNightLevel.IsNone())
	{
		return;
	}
	FLatentActionInfo LatentInfo(0, NextLatentActionId++, TEXT(""), this);
	UGameplayStatics::LoadStreamLevel(this, PendingNightLevel, true, false, LatentInfo);
	LoadedNightLevel = PendingNightLevel;
	PendingNightLevel = NAME_None;
}

FText ARobberGameState::GetNightName(int32 NightIndex, bool bIsTutorial) const
{
	return bIsTutorial ? NSLOCTEXT("RobberNight", "Tutorial", "The tutorial")
		: FText::Format(NSLOCTEXT("RobberNight", "Night", "Night {0}"), NightIndex + 1);
}

ARobberEntryPoint* ARobberGameState::FindEntry(FName EntryId) const
{
	if (EntryId.IsNone() || !GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ARobberEntryPoint> It(GetWorld()); It; ++It)
	{
		if (It->EntryId == EntryId && IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
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
	DOREPLIFETIME(ARobberGameState, CurrentNight);
	DOREPLIFETIME(ARobberGameState, bTutorial);
	DOREPLIFETIME(ARobberGameState, NightResultText);
	DOREPLIFETIME(ARobberGameState, bPowerCut);
	DOREPLIFETIME(ARobberGameState, bAlarmRaised);
	DOREPLIFETIME(ARobberGameState, Heat);
	DOREPLIFETIME(ARobberGameState, GuardsArriveServerTime);
}

void ARobberGameState::SetGameStage(uint8 NewGameStage)
{
	const uint8 OldGameStage = ReplicatedGameStage;
	const bool bStageChanged = OldGameStage != NewGameStage;
	ReplicatedGameStage = NewGameStage;
	SetLegacyGameState(NewGameStage);
	if (HasAuthority())
	{
		ForceNetUpdate();
		// Here rather than in FinishNight so a Blueprint setting the stage (WBP_GameUI's Lose) ends the night too.
		if (bStageChanged && RobberStage::IsNightResult(NewGameStage) && !RobberStage::IsInLobby(OldGameStage))
		{
			ReturnToLobby();
		}
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
