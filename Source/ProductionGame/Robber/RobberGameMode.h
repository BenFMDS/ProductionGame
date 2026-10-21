// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProductionGameGameMode.h"
#include "RobberGameMode.generated.h"

/**
 * Native game mode for the Robber gameplay maps. The replacement Blueprint is
 * only a stable content reference; runtime class selection lives here.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberGameMode : public AProductionGameGameMode
{
	GENERATED_BODY()

public:
	ARobberGameMode();

	/** Prefers PlayerStarts tagged "Lobby", so a map with a heist lobby always starts the crew there. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
