#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberEntryPoint.generated.h"

class APlayerState;
class ARobberGameState;
class ARobberHidingSpot;
class UArrowComponent;
class UTextRenderComponent;

/**
 * One way into the museum. Placed where the crew spawns for that break-in; vote stations point at it.
 * Subclasses add the break-in's own rules and decide when it is done.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberEntryPoint : public AActor
{
	GENERATED_BODY()

public:
	ARobberEntryPoint();

	/** Lobby vote stations find this entry by id, because the lobby can't point into a night level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Entry")
	FName EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Entry")
	FText DisplayName = NSLOCTEXT("RobberEntry", "DefaultName", "Entry");

	/** Shown on the HUD during Break In. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Entry", meta = (MultiLine = "true"))
	FText BreakInObjective;

	/** 0 for no timer. When it runs out, OnBreakInTimerExpired decides what happens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Entry", meta = (ClampMin = "0"))
	float BreakInTimeLimit = 0.f;

	/** Where each player appears, relative to this actor. Players past the end wrap round. */
	UPROPERTY(EditAnywhere, Category = "Robber|Entry", meta = (MakeEditWidget = "true"))
	TArray<FVector> SpawnOffsets = { FVector(0, -100, 0), FVector(0, 100, 0), FVector(-150, -100, 0), FVector(-150, 100, 0) };

	FTransform GetSpawnTransform(int32 PlayerIndex) const;

	virtual void BeginBreakIn(ARobberGameState* GameState);
	virtual void TickBreakIn(ARobberGameState* GameState) {}
	virtual void OnBreakInTimerExpired(ARobberGameState* GameState);
	virtual void EndBreakIn(ARobberGameState* GameState) {}
	virtual FText GetBreakInStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Robber|Entry")
	TObjectPtr<UArrowComponent> Arrow;

	/** Editor-only marker so the spawn is easy to find in the level. */
	UPROPERTY(VisibleAnywhere, Category = "Robber|Entry")
	TObjectPtr<UTextRenderComponent> EditorLabel;
};

/**
 * Closing-Time Tour: the crew walks in as visitors, buys tickets, and hides in an exhibit before closing.
 * Closing comes when the timer runs out, or as soon as everyone is hidden.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberTourEntryPoint : public ARobberEntryPoint
{
	GENERATED_BODY()

public:
	ARobberTourEntryPoint();

	/** Server only. False (with the reason) when this player can't hide yet. */
	bool TryHide(ARobberGameState* GameState, APlayerState* Player, ARobberHidingSpot* HidingSpot, FText& OutReason);

	virtual void BeginBreakIn(ARobberGameState* GameState) override;
	virtual void TickBreakIn(ARobberGameState* GameState) override;
	virtual void EndBreakIn(ARobberGameState* GameState) override;
	virtual FText GetBreakInStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
};
