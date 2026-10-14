#include "RobberMenu/RobberMenuGameMode.h"

#include "RobberMenu/RobberMenuPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ARobberMenuGameMode::ARobberMenuGameMode()
{
	PlayerControllerClass = ARobberMenuPlayerController::StaticClass();

	// This is the current first-person pawn already used by the project.  The
	// RobberMenu map remains walkable without modifying its existing game mode.
	static ConstructorHelpers::FClassFinder<APawn> FirstPersonPawn(
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	if (FirstPersonPawn.Succeeded())
	{
		DefaultPawnClass = FirstPersonPawn.Class;
	}
}
