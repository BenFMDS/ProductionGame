// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberGameMode.h"

#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberGameMode::ARobberGameMode()
{
	// Spawn the Blueprint children: BP_Gem, WBP_GameUI and WBP_DevUI cast to them, so the native classes alone
	// leave every one of those casts failing.
	static ConstructorHelpers::FClassFinder<ARobberController> RobberController(
		TEXT("/Game/OurStuff/Core/Player/Game/BP_RobberController"));
	static ConstructorHelpers::FClassFinder<ARobberGameState> RobberGameState(
		TEXT("/Game/OurStuff/Core/Player/Game/BP_RobberGameState"));
	PlayerControllerClass = RobberController.Succeeded() ? RobberController.Class.Get() : ARobberController::StaticClass();
	GameStateClass = RobberGameState.Succeeded() ? RobberGameState.Class.Get() : ARobberGameState::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> RobberPawn(
		TEXT("/Game/OurStuff/Core/Player/Game/BP_RobberCharacter"));
	if (RobberPawn.Succeeded())
	{
		DefaultPawnClass = RobberPawn.Class;
	}
}
