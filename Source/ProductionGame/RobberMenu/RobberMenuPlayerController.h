#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RobberMenuPlayerController.generated.h"

class AProductionGamePlayerController;

/**
 * Quest-free controller used only by the RobberMenu map. It copies the existing
 * menu controller's input mapping profile without inheriting its quest component.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARobberMenuPlayerController();

protected:
	virtual void SetupInputComponent() override;

	/** Existing controller used only as the source of input mapping contexts. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Input Mappings")
	TSubclassOf<AProductionGamePlayerController> InputProfileControllerClass;
};
