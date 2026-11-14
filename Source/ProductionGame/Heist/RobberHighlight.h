#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "RobberHighlight.generated.h"

class APlayerController;
class UPrimitiveComponent;

/** When a prop's red outline is on. */
UENUM(BlueprintType)
enum class ERobberHighlightMode : uint8
{
	/** While this machine's player aims at it within interact reach and pressing it would do something. */
	OnLook,
	/** While this machine's player is within Range of it with a clear view (menu objects). */
	InRange,
	/** Only when code calls SetManualHighlight (map table pieces under the cursor). */
	Manual,
};

/**
 * The red outline on a prop: custom depth on its visible parts, which M_HoverOutline on the player camera draws red.
 * Local only, each machine lights what its own player sees. URobberHighlightSubsystem drives every one of these
 * once a frame from a single look trace, so two pieces of code can't fight over the same outline.
 */
UCLASS(ClassGroup = (Robber), meta = (BlueprintSpawnableComponent))
class PRODUCTIONGAME_API URobberHighlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URobberHighlightComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Highlight")
	ERobberHighlightMode Mode = ERobberHighlightMode::OnLook;

	/** InRange only: how close (cm) the player's camera has to be. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Highlight", meta = (ClampMin = "0", EditCondition = "Mode == ERobberHighlightMode::InRange"))
	float Range = 500.f;

	/** Components to light, by name. Empty lights every visible part except text and widgets, which would draw a box. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Highlight")
	TArray<FName> PartNames;

	UFUNCTION(BlueprintCallable, Category = "Robber|Highlight")
	void SetManualHighlight(bool bOn);

	UFUNCTION(BlueprintPure, Category = "Robber|Highlight")
	bool IsHighlighted() const { return bHighlighted; }

	/** BPC_Interaction's trace length, which OnLook uses too. */
	static constexpr double Reach = 300.;

	/** True when Target is within Range of Viewer's camera with nothing in the way. */
	static bool IsInClearView(const AActor* Target, const APlayerController* Viewer, double Range);

	/** Called by the subsystem every frame. Re-applied while on, because BPC_Interaction clears "Mesh" on whatever it last aimed at. */
	void Apply(bool bOn);

	bool WantsManual() const { return bManualOn; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void GatherParts(TArray<TWeakObjectPtr<UPrimitiveComponent>>& Out) const;

	TArray<TWeakObjectPtr<UPrimitiveComponent>> LitParts;
	bool bHighlighted = false;
	bool bManualOn = false;
};

/**
 * Drives every URobberHighlightComponent in the world: one Visibility trace from this machine's camera per frame
 * finds the OnLook target, InRange ones check their own distance. Game worlds only.
 */
UCLASS()
class PRODUCTIONGAME_API URobberHighlightSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(URobberHighlightSubsystem, STATGROUP_Tickables); }

	void Register(URobberHighlightComponent* Component) { Components.AddUnique(Component); }
	void Unregister(URobberHighlightComponent* Component) { Components.Remove(Component); }

	/** The actor this machine's player is aiming at and has lit, or null. */
	AActor* GetLookTarget() const { return LookTarget.Get(); }

	/** Would pressing Interact on it do anything right now? Nothing lights up if not (wrong stage, key taken). */
	static bool IsUsable(const AActor* Actor);

private:
	AActor* FindLookTarget(const APlayerController* LocalPlayer) const;

	TArray<TWeakObjectPtr<URobberHighlightComponent>> Components;
	TWeakObjectPtr<AActor> LookTarget;
};
