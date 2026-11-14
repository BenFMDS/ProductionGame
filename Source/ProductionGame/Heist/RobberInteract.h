#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/Interface.h"
#include "RobberInteract.generated.h"

class APawn;
class APlayerState;
class ARobberEntryPoint;
class ARobberGameState;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class URobberInteractOwner : public UInterface
{
	GENERATED_BODY()
};

/** What a URobberInteractComponent asks the actor it sits on: the words on screen, and what a press does. */
class PRODUCTIONGAME_API IRobberInteractOwner
{
	GENERATED_BODY()

public:
	/** Empty means nothing to do here for this viewer: no prompt, no outline. GameState is null outside a Robber world (menus). */
	virtual FText GetInteractText(const ARobberGameState* GameState, const APlayerState* Viewer) const = 0;
	/** Only called once the component's checks pass. Server only unless the component's bServerOnly is off. */
	virtual void OnInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) = 0;
};

/**
 * Pressing E on a thing: the prompt it shows, whether it works right now, and getting the press to the right
 * machine once. The actor it's on implements IRobberInteractOwner for the actual words and behaviour.
 */
UCLASS(ClassGroup = Robber, meta = (BlueprintSpawnableComponent))
class PRODUCTIONGAME_API URobberInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URobberInteractComponent();

	static URobberInteractComponent* Of(const AActor* Actor);

	/**
	 * The way in this belongs to. When set, it only works (and only shows its prompt and label) while that entry is
	 * the one the crew voted for, so a night with several ways in only runs one of them.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<ARobberEntryPoint> OwningEntry;

	/**
	 * On: BPC_Interaction calls Interact locally and again on the server (twice on a listen-server host), so only
	 * the server acts, and a second press from the same player straight after the first is dropped (toggles like the
	 * skylight would otherwise undo themselves). Off: the owner gets the press wherever it happens and sends its own
	 * RPC (talkers do, through ServerTalkTo).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Robber|Interact")
	bool bServerOnly = true;

	bool IsActive(const ARobberGameState* GameState) const;

	/** What this machine's player sees when aiming at it. Empty when there's nothing to do. */
	UFUNCTION(BlueprintPure, Category = "Robber|Interact")
	FString GetPrompt() const;

	/** InteractingPlayer may be the pawn or its controller. */
	UFUNCTION(BlueprintCallable, Category = "Robber|Interact")
	void Press(AActor* InteractingPlayer);

private:
	/** Server only. World time of each player's last handled press. */
	TMap<TWeakObjectPtr<APlayerState>, double> LastPressTime;
};
