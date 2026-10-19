// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RobberGameState.generated.h"

class ARobberController;

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void OnRep_GameState();

private:
	UFUNCTION()
	void OnRep_ReplicatedGameStage();

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
};
