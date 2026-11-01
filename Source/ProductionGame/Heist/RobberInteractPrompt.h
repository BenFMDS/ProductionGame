#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobberInteractPrompt.generated.h"

/**
 * Native parent of WBP_Interaction (the "E  Turn dial 2" prompt). BPC_Interaction still drives the Blueprint
 * (UpdateWidget shows the widget and sets InteractText); this only hides the Case File prompt row (PromptRow: the E
 * button + pill) while InteractText is empty, so props with no prompt don't show a bare E.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberInteractPrompt : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
