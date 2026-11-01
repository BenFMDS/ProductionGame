#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RobberMenuPlayerController.generated.h"

class AProductionGamePlayerController;
class UUserWidget;

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
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

	void RemoveDuplicateMenuWidgets();

	/** Existing controller used only as the source of input mapping contexts. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Input Mappings")
	TSubclassOf<AProductionGamePlayerController> InputProfileControllerClass;

	/** BPC_Interaction only line-traces once it finds this widget, so without it the menu has no prompt or highlight. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UUserWidget> InteractionWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> InteractWidgetRef;
};
