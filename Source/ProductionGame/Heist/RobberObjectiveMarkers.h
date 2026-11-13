#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberEntryPoint.h"

class APlayerState;
class ARobberGameState;

namespace RobberMarkers
{
	/**
	 * What the viewer's current main-objective step points at. Runs on each machine from replicated state. Exhibits and
	 * their errands are deliberately never here: finding who to talk to is the game.
	 */
	PRODUCTIONGAME_API void Gather(const ARobberGameState* State, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out);
}
