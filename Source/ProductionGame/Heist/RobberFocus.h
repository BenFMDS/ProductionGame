#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RobberFocus.generated.h"

class APlayerController;

/**
 * What this machine's player is aiming at: one Visibility trace from the camera per frame, which the outline
 * (URobberHighlightSubsystem) and the map table's hover both read, so they can't disagree about what's focused.
 * Sits on the local PlayerController rather than the pawn: the menu and the game use different pawns, and the
 * controller survives hiding and respawns. Local only; the press itself still goes to the server through
 * BPC_Interaction, which keeps its own trace for the prompt until stage 5 (it can read GetTarget/GetPrompt then).
 */
UCLASS(ClassGroup = Robber, meta = (BlueprintSpawnableComponent))
class PRODUCTIONGAME_API URobberFocusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URobberFocusComponent();

	/** BPC_Interaction's trace length. */
	static constexpr double Reach = 300.;

	/** The local player's focus, added on first use. Null for a remote player's controller (server copies). */
	static URobberFocusComponent* For(APlayerController* LocalPlayer);

	/** The first local player's focus in World, or null (dedicated server, no player yet). */
	UFUNCTION(BlueprintPure, Category = "Robber|Focus", meta = (WorldContext = "WorldContext"))
	static URobberFocusComponent* GetLocalFocus(const UObject* WorldContext);

	/** Whatever the trace hit first within Reach, interactable or not. */
	UFUNCTION(BlueprintPure, Category = "Robber|Focus")
	AActor* GetHitActor() const { return HitActor.Get(); }

	/** The hit actor if it has an Interact or Highlight component and IsUsable: pressing E on it would do something now. */
	UFUNCTION(BlueprintPure, Category = "Robber|Focus")
	AActor* GetTarget() const { return Target.Get(); }

	/** The target's prompt, empty when there's no target or it has no Interact component. */
	UFUNCTION(BlueprintPure, Category = "Robber|Focus")
	FString GetPrompt() const;

	/** Would pressing Interact on it do anything right now? False for a prompt-less Interact (wrong stage, key taken). */
	static bool IsUsable(const AActor* Actor);

	/** Trace again now. The tick does this each frame; tests call it to skip a frame's wait. */
	void Refresh();

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<AActor> HitActor;
	TWeakObjectPtr<AActor> Target;
};
