#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberEntryPoint.generated.h"

class APlayerState;
class ARobberCrowbar;
class ARobberGameState;
class ARobberHidingSpot;
class ARobberRollUpDoor;
class ARobberSewerGrate;
class ARobberSewerWater;
class ARobberSkylight;
class UArrowComponent;
class UBoxComponent;
class UTextRenderComponent;

/** Where one quest marker goes and what it's called. */
struct FRobberMarkerTarget
{
	FVector Location = FVector::ZeroVector;
	FText Label;
};

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

	/** Which lobby exit sends the crew in (ARobberLobbyExit::ExitId). None = the museum door. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Entry")
	FName LobbyExit;

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

	/** Runs on every machine. Adds a marker for each thing the viewer's current break-in step points at. */
	virtual void GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const {}

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
	virtual void GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const override;
};

/**
 * A way in where the crew has to physically get into the museum. Done when every player is inside
 * InsideArea (however they got there: rope, grate, or falling through the glass).
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API ARobberSneakInEntryPoint : public ARobberEntryPoint
{
	GENERATED_BODY()

public:
	ARobberSneakInEntryPoint();

	/** The museum's interior, in world space. Keep the roof and the tunnels outside it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Entry")
	TObjectPtr<UBoxComponent> InsideArea;

	bool IsInside(const APawn* Pawn) const;

	virtual void TickBreakIn(ARobberGameState* GameState) override;
	virtual void EndBreakIn(ARobberGameState* GameState) override;
	virtual FText GetBreakInStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const override;

protected:
	/** Status for a player who isn't in yet. */
	virtual FText GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const { return BreakInObjective; }
};

/** Skylight: start on the roof, cut the glass without making too much noise, drop the rope, climb down it. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSkylightEntryPoint : public ARobberSneakInEntryPoint
{
	GENERATED_BODY()

public:
	ARobberSkylightEntryPoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Entry")
	TObjectPtr<ARobberSkylight> Skylight;

	virtual void BeginBreakIn(ARobberGameState* GameState) override;
	/** Nobody counts as in until the rope is down; the skylight sends early jumpers back up. */
	virtual void TickBreakIn(ARobberGameState* GameState) override;
	virtual void GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const override;

protected:
	virtual FText GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
};

/**
 * Sewer: drop in through the manhole by the lobby, drain the flooded tunnel (two valves open at once), find the
 * crowbar, pry the grate under the museum and climb up into the basement.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSewerEntryPoint : public ARobberSneakInEntryPoint
{
	GENERATED_BODY()

public:
	ARobberSewerEntryPoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Entry")
	TObjectPtr<ARobberSewerWater> Water;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Entry")
	TObjectPtr<ARobberSewerGrate> Grate;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Entry")
	TObjectPtr<ARobberCrowbar> Crowbar;

	virtual void BeginBreakIn(ARobberGameState* GameState) override;
	virtual void GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const override;

protected:
	virtual FText GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
};

/** Loading dock: start in the truck bay, lift the roll-up door without rattling it too long, and get everyone under it. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLoadingDockEntryPoint : public ARobberSneakInEntryPoint
{
	GENERATED_BODY()

public:
	ARobberLoadingDockEntryPoint();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Entry")
	TObjectPtr<ARobberRollUpDoor> RollUpDoor;

	virtual void BeginBreakIn(ARobberGameState* GameState) override;
	virtual void GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const override;

protected:
	virtual FText GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
};
