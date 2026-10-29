#pragma once

#include "CoreMinimal.h"
#include "HeistTypes.generated.h"

class APlayerState;
class ARobberEntryPoint;

/** One player's vote in the lobby. A player has at most one; voting again moves it. */
USTRUCT(BlueprintType)
struct FRobberEntryVote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	TObjectPtr<APlayerState> Voter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	TObjectPtr<ARobberEntryPoint> Entry = nullptr;
};

/** Per-player progress through the current night's break-in. Reset when a break-in starts. */
USTRUCT(BlueprintType)
struct FRobberCrewStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	TObjectPtr<APlayerState> Player = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	bool bHasTicket = false;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	TObjectPtr<AActor> HidingSpot = nullptr;

	/** Sneak-in entries (skylight, sewer): this player has made it into the museum. */
	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	bool bInside = false;
};

/** A main objective for the night, shared by the whole crew. */
USTRUCT(BlueprintType)
struct FRobberJob
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Robber|Heist")
	bool bDone = false;
};
