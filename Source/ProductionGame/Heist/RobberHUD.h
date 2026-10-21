#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RobberHUD.generated.h"

/**
 * Placeholder heist HUD drawn on the canvas: phase, timer, votes and messages.
 * Stand-in until the team builds a proper widget on top of the game state's Blueprint getters.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Every line DrawHUD would draw this frame. Public so tests can read what the player sees. */
	TArray<FString> BuildLines() const;
};
