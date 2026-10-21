#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberTriggerZones.generated.h"

class ARobberGameState;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A walk-in volume drawn as a door-sized block with a label, so players can see where to go. */
UCLASS(Abstract)
class PRODUCTIONGAME_API ARobberTriggerZone : public AActor
{
	GENERATED_BODY()

public:
	ARobberTriggerZone();

	virtual void Tick(float DeltaSeconds) override;

	/** Player-controlled pawns standing in the zone, and the number of players in the game. */
	void CountPlayers(int32& OutInside, int32& OutTotal) const;

protected:
	/** Server only, a few times a second. */
	virtual void ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total) {}
	virtual FText GetLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const { return Label; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Zone")
	TObjectPtr<UBoxComponent> Box;

	/** Visual only: players walk through it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Zone")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Zone")
	TObjectPtr<UTextRenderComponent> LabelText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Zone")
	FText Label;
};

/** The lobby's museum door. When everyone is inside and a way in has votes, the break-in starts. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLobbyExit : public ARobberTriggerZone
{
	GENERATED_BODY()

public:
	ARobberLobbyExit();

protected:
	virtual void ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total) override;
	virtual FText GetLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const override;
};

/** In front of the gem room door. The first player to reach it during Investigate starts the heist. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberGemRoomApproach : public ARobberTriggerZone
{
	GENERATED_BODY()

public:
	ARobberGemRoomApproach();

protected:
	virtual void ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total) override;
	virtual FText GetLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const override;
};
