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

/** How worked up the museum is. Bands of heat; the HUD headline and the cameras follow it. */
UENUM(BlueprintType)
enum class ERobberHeatTier : uint8
{
	Calm,
	Suspicious,
	Alert,
	Alarm,
};

/**
 * How much heat (of 100) each kind of noise costs; one place to tune them. Getting heard by a guard twice puts the crew
 * at HOT, a third time sets the alarm off. Guards will add their own later.
 */
namespace RobberHeatCosts
{
	/** Heat where each tier starts. Alarm is MaxHeat or a tripped laser. */
	constexpr float SuspiciousAt = 25.f;
	constexpr float AlertAt = 50.f;

	/** A security camera watched someone long enough to be sure. */
	constexpr float CameraSpotted = 15.f;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRobberHeatTierChangedSignature, ERobberHeatTier, NewTier, ERobberHeatTier, OldTier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRobberInvestigateSignature, FVector, Location, AActor*, ReportedBy);

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

	/** How long the closing-time card plays (Tour). Input is off and interacting does nothing meanwhile. */
	static constexpr float ClosingTimeSeconds = 5.5f;

	/** Server only. Starts the closing-time card on every screen; the entry ends the break-in once it's played. */
	void StartClosingTime();

	/** Seconds since closing time started, or -1 when it isn't playing. Every machine. */
	float GetClosingTimeElapsed() const;
	bool IsClosingTime() const { return ClosingStartServerTime > 0.; }

	UFUNCTION(BlueprintPure, Category = "Robber|BreakIn")
	FRobberCrewStatus GetCrewStatus(const APlayerState* Player) const;

	const TArray<FRobberCrewStatus>& GetCrew() const { return Crew; }

	/** Server only. */
	void GiveTicket(APlayerState* Player);
	void SetHidingSpot(APlayerState* Player, AActor* HidingSpot);
	void SetInside(APlayerState* Player);

	/** Server only. Shows Message to every player. */
	void TellCrew(const FText& Message) const;
	/** The players in PlayerId order, which every machine agrees on (crew cards, map pins). */
	TArray<APlayerState*> GetPlayersInIdOrder() const;
	/** Each way in that has votes, with its count, in the order they were first voted for. */
	TArray<TPair<const ARobberEntryPoint*, int32>> GetVoteTallies() const;

	// --- Investigate / Heist ---

	/** Exhibits stay silent during Break In and Investigate, so the crew has to find the gem room first. */
	UFUNCTION(BlueprintPure, Category = "Robber|Stage")
	bool CanTalkToExhibits() const;

	/** During Investigate exhibits only make small talk: one random line, no clues, quests or journal notes. */
	bool IsExhibitSmallTalk() const;
	/** Whether pressing interact on an exhibit gets any answer at all. */
	bool CanChatWithExhibits() const;

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

	/** The generator's running. BP_PowerGenerator turns it back on 10 seconds after a cut; IsPowerCut stays set for the night. */
	bool IsPowerOn() const;

	// --- Heat: how much the museum has noticed, crew-wide, 0-100. Full heat raises the alarm. ---

	static constexpr float MaxHeat = 100.f;

	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	float GetHeat() const { return Heat; }

	/** Server only. Noise from the crew (Source names it for the log). Ignored in the lobby and once the alarm's up. */
	void AddHeat(float Amount, const TCHAR* Source);

	/** Server only. Something's drawn the guards off. Too late once the alarm's up. */
	void CoolHeat(float Amount);

	/** Server only. Pushes the guards' arrival back, including an alarm that hasn't gone off yet. */
	void AddGuardsTime(float Seconds);

	/** The alarm's gone off (full heat or a laser): the night is lost if the crew isn't out by the guards timer. */
	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	bool IsAlarmRaised() const { return bAlarmRaised; }

	/** Seconds until the guards arrive, or -1 when the alarm isn't up. */
	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	float GetGuardsTimeRemaining() const;

	/** Server only. Someone crossed a live laser: straight to full heat. */
	void TripLaser(APawn* Pawn);

	/** Same bands on every machine: worked out from the replicated heat and alarm. */
	UFUNCTION(BlueprintPure, Category = "Robber|Heat")
	ERobberHeatTier GetHeatTier() const;

	/** Fires on every machine when the tier changes (it's checked each tick against the replicated heat). */
	UPROPERTY(BlueprintAssignable, Category = "Robber|Heat")
	FRobberHeatTierChangedSignature OnHeatTierChanged;

	/**
	 * Server only. Something (a camera) saw the crew at Location. Fires OnInvestigate for the guards to pick up;
	 * nothing in C++ moves a guard, so BP_GuardController binds this and walks over.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Heat")
	void ReportSighting(FVector Location, AActor* ReportedBy);

	/**
	 * Server only. A guard has been to Location and had a look: every camera frozen on a disturbance within Radius
	 * starts panning again. Call it from the guard when it reaches an OnInvestigate spot.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Heat")
	void DisturbanceChecked(FVector Location, float Radius = 600.f);

	/** Server only. Guards should go and look at Location. */
	UPROPERTY(BlueprintAssignable, Category = "Robber|Heat")
	FRobberInvestigateSignature OnInvestigate;

	/** Server only: sightings reported tonight, and where the last one was. */
	int32 GetSightingCount() const { return SightingCount; }
	FVector GetLastSighting() const { return LastSighting; }

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/** Server only: the hottest it's been tonight. */
	float PeakHeat = 0.f;

	FDelegateHandle AchievementTalkHandle;
	FDelegateHandle AchievementQuestHandle;

	/** Server world time the guards arrive; 0 when the alarm isn't up. Separate from the phase timer (the break-in uses that). */
	UPROPERTY(Replicated)
	double GuardsArriveServerTime = 0.;

	/** Server only: extra seconds earned before the alarm went off. */
	float BonusGuardsSeconds = 0.f;

	/** Server only: when heat last went up. It starts cooling HeatCoolDelay seconds after. */
	double LastHeatTime = 0.;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float GuardsArriveSeconds = 60.f;

	/** Time to get out once the gem's taken. Runs on the phase timer; the night's lost when it hits zero. */
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heist")
	float EscapeSeconds = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float HeatCoolDelay = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float HeatCoolPerSecond = 2.f;

	/** Cooling at Alert is this fraction of HeatCoolPerSecond: an alert museum calms down slowly. */
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Heat")
	float AlertCoolScale = 0.5f;

	/** Each machine: the tier last broadcast, to spot changes. */
	ERobberHeatTier LastHeatTier = ERobberHeatTier::Calm;

	/** Server only: once the museum's been on Alert tonight it never cools below Suspicious again. */
	bool bReachedAlert = false;

	int32 SightingCount = 0;
	FVector LastSighting = FVector::ZeroVector;

	void TickHeatTier();

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

	/** Server world time the closing-time card started; 0 when it isn't playing. */
	UPROPERTY(Replicated)
	double ClosingStartServerTime = 0.;

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
