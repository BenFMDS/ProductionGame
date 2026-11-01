// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Heist/HeistTypes.h"
#include "RobberGameState.generated.h"

class APawn;
class APlayerState;
class ARobberController;
class ARobberEntryPoint;

/**
 * How much heat (of 100) each kind of noise costs; one place to tune them. Getting heard by a guard twice puts the crew
 * at HOT, a third time sets the alarm off. Guards and cameras will add their own later.
 */
namespace RobberHeatCosts
{
	/** A prop's noise meter filled up ("a guard heard..."). */
	constexpr float Heard = 20.f;
	/** Per second the roll-up door is up. */
	constexpr float RattlePerSecond = 2.f;
	/** Per second per player cutting the skylight. */
	constexpr float CuttingPerSecond = 1.5f;
	/** A sewer valve springing open. */
	constexpr float Clank = 4.f;
	/** Prying the sewer grate. */
	constexpr float Prise = 12.f;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobberStageChangedSignature, uint8, NewGameStage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRobberJobsChangedSignature);

/** Native replicated game state for the Robber gameplay maps. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARobberGameState();

	/** Sets the replicated C++ state and mirrors it to the compatibility field on the thin Blueprint child. */
	UFUNCTION(BlueprintCallable, Category = "Robber|Stage")
	void SetGameStage(uint8 NewGameStage);

	UFUNCTION(BlueprintPure, Category = "Robber|Stage")
	uint8 GetGameStage() const { return ReplicatedGameStage; }

	UFUNCTION(BlueprintCallable, Category = "Robber|Stage")
	void ActivateAlarms();

	/** Fires on every machine when the stage changes. */
	UPROPERTY(BlueprintAssignable, Category = "Robber|Stage")
	FRobberStageChangedSignature OnGameStageChanged;

	UPROPERTY(BlueprintAssignable, Category = "Robber|Heist")
	FRobberJobsChangedSignature OnJobsChanged;

	// --- Lobby ---

	/** Server only. Moves this player's vote to Entry. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Lobby")
	void CastVote(APlayerState* Voter, ARobberEntryPoint* Entry);

	UFUNCTION(BlueprintPure, Category = "Robber|Lobby")
	int32 GetVoteCount(const ARobberEntryPoint* Entry) const;

	/** Most votes wins; a tie goes to the entry that got its first vote earliest. Null if nobody voted. */
	UFUNCTION(BlueprintPure, Category = "Robber|Lobby")
	ARobberEntryPoint* GetWinningEntry() const;

	const TArray<FRobberEntryVote>& GetVotes() const { return Votes; }

	/** Server only. Sends the crew to the winning entry's spawn. False if nobody has voted. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Lobby")
	bool StartBreakIn();

	// --- Break in ---

	UFUNCTION(BlueprintPure, Category = "Robber|BreakIn")
	ARobberEntryPoint* GetActiveEntry() const { return ActiveEntry; }

	/** Server only. Called by the entry when its break-in goal is met or its timer runs out. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|BreakIn")
	void FinishBreakIn();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|BreakIn")
	void SetPhaseTimer(float Seconds);

	/** Seconds left on the current phase timer, or -1 when none is running. */
	UFUNCTION(BlueprintPure, Category = "Robber|BreakIn")
	float GetPhaseTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Robber|BreakIn")
	FRobberCrewStatus GetCrewStatus(const APlayerState* Player) const;

	const TArray<FRobberCrewStatus>& GetCrew() const { return Crew; }

	/** Server only. */
	void GiveTicket(APlayerState* Player);
	void SetHidingSpot(APlayerState* Player, AActor* HidingSpot);
	void SetInside(APlayerState* Player);

	/** Server only. Shows Message to every player. */
	void TellCrew(const FText& Message) const;

	// --- Investigate / Heist ---

	/** Exhibits stay silent during Break In and Investigate, so the crew has to find the gem room first. */
	UFUNCTION(BlueprintPure, Category = "Robber|Stage")
	bool CanTalkToExhibits() const;

	/** Server only. Called when the crew reaches the gem room door. Sets the night's jobs. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Heist")
	void StartHeist();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Heist")
	void CompleteJob(FName JobId);

	UFUNCTION(BlueprintPure, Category = "Robber|Heist")
	const TArray<FRobberJob>& GetJobs() const { return Jobs; }

	/** The generator's been switched off this night: room lights are on emergency red and the lasers are down. */
	UFUNCTION(BlueprintPure, Category = "Robber|Heist")
	bool IsPowerCut() const { return bPowerCut; }

	// --- Heat: how much the museum has noticed, crew-wide, 0-100. Full heat raises the alarm. ---

	static constexpr float MaxHeat = 100.f;

	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	float GetHeat() const { return Heat; }

	/** Server only. Noise from the crew (Source names it for the log). Ignored in the lobby and once the alarm's up. */
	void AddHeat(float Amount, const TCHAR* Source);

	/** The alarm's gone off (full heat or a laser): the night is lost if the crew isn't out by the guards timer. */
	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	bool IsAlarmRaised() const { return bAlarmRaised; }

	/** Seconds until the guards arrive, or -1 when the alarm isn't up. */
	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	float GetGuardsTimeRemaining() const;

	/** Server only. Someone crossed a live laser: straight to full heat. */
	void TripLaser(APawn* Pawn);

	// --- Nights ---
	// Each night is a sublevel of L_GameLevel, streamed in at its own origin while the lobby stays loaded.

	/**
	 * Server only. Ends the night: Lose, Continue (more nights to go) or Win (that was the last one).
	 * Everyone goes back to the lobby; a lost night reloads fresh, a won one swaps to the next.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Night")
	void FinishNight(bool bWon);

	/** Server only, from the lobby. NightIndex is into Nights (0 = Night 1). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Night")
	void LoadNight(int32 NightIndex);

	/** Server only, from the lobby. Finishing the tutorial moves on to Night 1. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Night")
	void LoadTutorial();

	/**
	 * Server only, dev tool. Loads a night (INDEX_NONE = the tutorial) from any stage: a night in progress is
	 * dropped and everyone goes back to the lobby first, with no result line.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Night")
	void SwitchNight(int32 NightIndex);

	/** Index into Nights of the loaded night; -1 for the tutorial or when no night is loaded. */
	UFUNCTION(BlueprintPure, Category = "Robber|Night")
	int32 GetCurrentNight() const { return CurrentNight; }

	bool IsTutorial() const { return bTutorial; }
	int32 GetNightCount() const { return Nights.Num(); }

	/** The loaded night's entry with this id, or null. */
	ARobberEntryPoint* FindEntry(FName EntryId) const;

	/** What Viewer should be doing right now, for the HUD. */
	UFUNCTION(BlueprintPure, Category = "Robber|Stage")
	FText GetObjectiveText(const APlayerState* Viewer) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnRep_GameState();

private:
	UFUNCTION()
	void OnRep_ReplicatedGameStage();

	UFUNCTION()
	void OnRep_Jobs();

	FRobberCrewStatus* FindOrAddCrewStatus(APlayerState* Player);
	void TickPhase();
	bool IsPowerOn() const;

	uint8 GetLegacyGameState() const;
	void SetLegacyGameState(uint8 NewGameStage);
	void HandleGameStageChanged();
	void BindLocalStageReaders();
	void NotifyDevUIOfStageChange();

	void ReturnToLobby();
	void ResetCrewToLobby();
	bool CanStreamLevel(const TSoftObjectPtr<UWorld>& Level) const;
	void SwapNightLevel(const TSoftObjectPtr<UWorld>& Level);
	void LoadPendingNightLevel();
	FText GetNightName(int32 NightIndex, bool bIsTutorial) const;

	UFUNCTION()
	void OnNightLevelUnloaded();

	UFUNCTION()
	void OnRep_PowerCut();

	UFUNCTION()
	void OnRep_AlarmRaised();

	void ApplyPowerCut();
	void RaiseAlarm(const FText& Why);
	void TickHeat(float DeltaSeconds);

	UPROPERTY(ReplicatedUsing = OnRep_PowerCut)
	bool bPowerCut = false;

	UPROPERTY(ReplicatedUsing = OnRep_AlarmRaised)
	bool bAlarmRaised = false;

	UPROPERTY(Replicated)
	float Heat = 0.f;

	/** Server world time the guards arrive; 0 when the alarm isn't up. Separate from the phase timer (the break-in uses that). */
	UPROPERTY(Replicated)
	double GuardsArriveServerTime = 0.;

	/** Server only: when heat last went up. It starts cooling HeatCoolDelay seconds after. */
	double LastHeatTime = 0.;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float GuardsArriveSeconds = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float HeatCoolDelay = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float HeatCoolPerSecond = 2.f;

	/** Soft so the Blueprint alarm can be resolved at runtime; ConstructorHelpers is only legal in constructors. */
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Alarm")
	TSoftClassPtr<AActor> AlarmClass;

	/** Native replication backing for the E_GameStage Blueprint enum compatibility member. */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedGameStage, VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Stage", meta = (AllowPrivateAccess = "true"))
	uint8 ReplicatedGameStage = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Robber|Stage", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ARobberController> PlayerControllerRef;

	UPROPERTY(Replicated)
	TArray<FRobberEntryVote> Votes;

	UPROPERTY(Replicated)
	TObjectPtr<ARobberEntryPoint> ActiveEntry;

	UPROPERTY(Replicated)
	TArray<FRobberCrewStatus> Crew;

	UPROPERTY(ReplicatedUsing = OnRep_Jobs)
	TArray<FRobberJob> Jobs;

	/** Server world time the current phase timer ends at; 0 when none is running. */
	UPROPERTY(Replicated)
	double PhaseEndServerTime = 0.;

	/** In play order. Each must be a streaming sublevel of the map (see HeistSliceSetup -GameLevel). */
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Night")
	TArray<TSoftObjectPtr<UWorld>> Nights;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Night")
	TSoftObjectPtr<UWorld> TutorialLevel;

	UPROPERTY(Replicated)
	int32 CurrentNight = INDEX_NONE;

	UPROPERTY(Replicated)
	bool bTutorial = false;

	/** How the last night went, shown in the lobby until the next break-in. */
	UPROPERTY(Replicated)
	FText NightResultText;

	/** Server only. Long package names of the night sublevel that is loaded and the one waiting to load. */
	FName LoadedNightLevel;
	FName PendingNightLevel;
	FTimerHandle NightLoadTimer;
	int32 NextLatentActionId = 1;
};
