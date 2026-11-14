#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberInteract.h"
#include "RobberPropBase.generated.h"

class APlayerState;
class ARobberGameState;
class URobberHighlightComponent;
class URobberInteractComponent;

/**
 * The one parent of every prop the player presses E on: the C++ props (ARobberInteractable) and the Blueprint ones
 * (BP_InteractMaster is reparented onto this). It only adds the shared parts: the Interact component (prompt, way-in
 * gate) and the Highlight component (red outline while aimed at). No mesh or root: each family keeps its own.
 *
 * Blueprint props still get their presses through BPI_Interaction (BPC_Interaction calls Interact locally, then on
 * the server), so their graphs are unchanged; their prompt is their BPI GetInteractData.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberPropBase : public AActor, public IRobberInteractOwner
{
	GENERATED_BODY()

public:
	ARobberPropBase();

	URobberHighlightComponent* GetHighlight() const { return Highlight; }
	URobberInteractComponent* GetInteract() const { return InteractComponent; }

	/** BPI_Interaction's GetInteractData, for Blueprint props. C++ props override this. */
	virtual FText GetInteractText(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	/** Blueprint props act in their BPI Interact event, so there's nothing to do here. */
	virtual void OnInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override {}

	/** The BPI_Interaction GetInteractData text of any actor that has it, or empty. */
	static FString GetInteractData(AActor* Actor);

protected:
	/** Red outline while aimed at, by default. Props that light up another way change its Mode. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<URobberHighlightComponent> Highlight;

	/** Prompt and the OwningEntry gate. Not called "Interact": that's BPI_Interaction's function in the BP children. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<URobberInteractComponent> InteractComponent;
};
