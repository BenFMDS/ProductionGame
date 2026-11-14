#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RobberVisitor.generated.h"

class UAnimationAsset;
class UStaticMeshComponent;

/**
 * A daytime museum visitor on the Closing-Time Tour: wanders from exhibit to exhibit and has a look. The ticket
 * seller is one too (bClerk), standing still behind the desk. Spawned and removed by ARobberTourEntryPoint, server
 * side; they replicate like any character.
 *
 * Looks like the crew: the mesh is copied from BP_RobberCharacter's defaults at BeginPlay, with M_VisitorTint
 * (M_robber x a "Tint" colour) on every slot.
 */
UCLASS(NotPlaceable)
class PRODUCTIONGAME_API ARobberVisitor : public ACharacter
{
	GENERATED_BODY()

public:
	ARobberVisitor();

	/** Server only. bClerk stands still facing along Transform; everyone else wanders. */
	static ARobberVisitor* Spawn(UWorld* World, const FTransform& Transform, bool bClerk, FLinearColor Colour, float Size);

	bool IsClerk() const { return bClerk; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Replicated)
	bool bClerk = false;

	UPROPERTY(Replicated)
	FLinearColor Colour = FLinearColor::White;

	/** Uniform scale, so the crowd isn't all one height. */
	UPROPERTY(Replicated)
	float Size = 1.f;

	/** The ticket seller's cap. */
	UPROPERTY(VisibleAnywhere, Category = "Robber|Visitor")
	TObjectPtr<UStaticMeshComponent> Cap;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Visitor")
	TObjectPtr<UStaticMeshComponent> CapBrim;

private:
	void CopyCrewLook();
	void UpdateAnimation();
	void ThinkServer(float DeltaSeconds);
	bool PickNextStop();

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> IdleAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> WalkAnim;

	/** Which of the two is playing, so it's only restarted on a change. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> PlayingAnim;

	/** What this visitor is walking to or looking at. */
	TWeakObjectPtr<AActor> LookingAt;
	FVector Stop = FVector::ZeroVector;
	bool bWalking = false;
	/** Seconds left to stand and look, or before giving up on a walk that's stuck. */
	float Timer = 0.f;
};
