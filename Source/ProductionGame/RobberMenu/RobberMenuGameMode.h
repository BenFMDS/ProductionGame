#pragma once

#include "CoreMinimal.h"
#include "ProductionGameGameMode.h"
#include "RobberMenuGameMode.generated.h"

/**
 * Game mode used only by L_MainMenu's new RobberMenu experience.
 * Create a Blueprint child to choose the existing first-person pawn without
 * changing the project's current menu or gameplay game modes.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberMenuGameMode : public AProductionGameGameMode
{
	GENERATED_BODY()

public:
	ARobberMenuGameMode();
};
