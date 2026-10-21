// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Heist/HeistTypes.h"
#include "RobberGameState.generated.h"

class APlayerState;
class ARobberController;
class ARobberEntryPoint;

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
};
