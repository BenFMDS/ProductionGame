// Copyright Epic Games, Inc. All Rights Reserved.

#include "Robber/RobberGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
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

AActor* ARobberGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> LobbyStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == TEXT("Lobby"))
		{
			LobbyStarts.Add(*It);
		}
	}
	if (LobbyStarts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
	// Spread the crew out: one start per player already in the game.
	return LobbyStarts[GetNumPlayers() % LobbyStarts.Num()];
}
