#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberLaunchScene.generated.h"

class ALevelSequenceActor;
class APlayerController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPoseableMeshComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * Escape Velocity's first cutscene (Ben, 2026-11-24): in Mission Control, three robbers watch the launch checklist while
 * the fourth walks to the flight console and turns the key; the first note of the music hits, the pad floodlights snap
 * on, they look at each other, then turn to the rocket. Then the suit-up scene: they run through the Director's Office,
 * walk up to the four suits in the Spacesuit Lab and, after the cut, dance in them. It's a Level Sequence (LS_LaunchScene1) on a sequence actor
 * tagged LaunchScene in L_Night5, played on each machine on its own by the launch ending, with stand-in robbers so it
 * looks the same for 1 to 4 players. The actors all sit in the map's LaunchScene folder, made by -run=LaunchScene.
 */
namespace RobberLaunchScene
{
	/** The tag on the scene's sequence actor. */
	PRODUCTIONGAME_API extern const FName Tag;

	PRODUCTIONGAME_API ALevelSequenceActor* Find(UWorld* World);
	/** Local only: shows the stand-ins and starts the sequence (its camera cuts take this machine's view). */
	PRODUCTIONGAME_API bool Play(UWorld* World);
	/** Stops the sequence (handing the camera back) and hides the stand-ins again. */
	PRODUCTIONGAME_API void Stop(UWorld* World);
	PRODUCTIONGAME_API bool IsPlaying(UWorld* World);
}

/**
 * A stand-in robber for the cutscene: the crew's mesh with no capsule or collision, hidden in game outside the scene.
 * There are only idle and walk clips, so a reach is made here: Body plays Sequencer's clips unseen, and Posed (what
 * shows) copies its pose each frame and bends one arm to ReachFor as Sequencer raises Reach, leaning in if it's far.
 *
 * The suit-up scene uses it twice over until the artist's spacesuit arrives (Ben, 2026-11-24): Suit puts a robber in a
 * plain white suit with an orange visor plane over his face, and a bMannequin one is a still, painted dummy on a suit
 * stand, wearing the suit until the cut. A bMirrorVisor visor shows a live reflection (a capture from the visor).
 */
UCLASS()
class PRODUCTIONGAME_API ARobberSceneRobber : public AActor
{
	GENERATED_BODY()

public:
	ARobberSceneRobber();

	static void ShowAll(UWorld* World, bool bShow);

	virtual void Tick(float DeltaSeconds) override;
	/** So scrubbing the sequence in the editor shows the pose too. */
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** Feet on the actor's origin, facing the actor's +X. The tool copies the crew's mesh and offset onto it. Never drawn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** The robber you see: Body's pose, plus the reach. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene")
	TObjectPtr<UPoseableMeshComponent> Posed;

	/** 0 = the clip's arm, 1 = hand on ReachFor. */
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, BlueprintSetter = SetReach, Category = "Scene", meta = (ClampMin = "0", ClampMax = "1"))
	float Reach = 0.f;

	UFUNCTION(BlueprintSetter)
	void SetReach(float Value);

	/** What the hand goes to: a launch key's grip (and the hand turns with the key), or else this actor's location. */
	UPROPERTY(EditAnywhere, Category = "Scene")
	TObjectPtr<AActor> ReachFor;

	UPROPERTY(EditAnywhere, Category = "Scene")
	bool bLeftHand = true;

	/** How far forward the robber may bend (degrees) to get a hand to something out of reach. */
	UPROPERTY(EditAnywhere, Category = "Scene", meta = (ClampMin = "0", ClampMax = "60"))
	float MaxLean = 35.f;

	/** From the wrist to the middle of the grip, in cm. */
	UPROPERTY(EditAnywhere, Category = "Scene")
	float HandLength = 6.f;

	/** 0 = as he is, 1 = in the spacesuit (SuitColour all over, visor down). Steps over at a half. */
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, BlueprintSetter = SetSuit, Category = "Suit", meta = (ClampMin = "0", ClampMax = "1"))
	float Suit = 0.f;

	UFUNCTION(BlueprintSetter)
	void SetSuit(float Value);

	/** A dummy on a suit stand: holds the first frame of its clip and is painted MannequinColour out of the suit. */
	UPROPERTY(EditAnywhere, Category = "Suit")
	bool bMannequin = false;

	/** The visor shows what's in front of it (one suit in the scene does: the one the camera walks up to). */
	UPROPERTY(EditAnywhere, Category = "Suit")
	bool bMirrorVisor = false;

	UPROPERTY(EditAnywhere, Category = "Suit") FLinearColor SuitColour = FLinearColor(.85f, .85f, .82f);
	UPROPERTY(EditAnywhere, Category = "Suit") FLinearColor MannequinColour = FLinearColor(.62f, .5f, .38f);
	/** Plain lit colour (Color, Roughness) for the suit and the mannequin, made by -run=LaunchScene -Assets. */
	UPROPERTY(EditAnywhere, Category = "Suit") TObjectPtr<UMaterialInterface> SolidMaterial;
	/** The visor glass (Tint, Reflection, MirrorStrength), made by -run=LaunchScene -Assets. */
	UPROPERTY(EditAnywhere, Category = "Suit") TObjectPtr<UMaterialInterface> VisorMaterial;
	/** The visor's middle from the head bone, along the robber's facing (X) and up (Z), cm, fitted on his first pose. */
	UPROPERTY(EditAnywhere, Category = "Suit") FVector VisorOffset = FVector(24.f, 0.f, 2.f);
	/** Across, cm. */
	UPROPERTY(EditAnywhere, Category = "Suit") float VisorSize = 26.f;

	/** Actors in the map hidden while the scene plays: the greybox suit this one stands in for. */
	UPROPERTY(EditAnywhere, Category = "Suit")
	TArray<TObjectPtr<AActor>> HideInScene;

	/** The visor plane: rides on the head, shown in the suit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suit")
	TObjectPtr<UStaticMeshComponent> Visor;

	/** Looks out of the visor for its reflection, only while a bMirrorVisor suit shows. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suit")
	TObjectPtr<USceneCaptureComponent2D> Mirror;

	bool IsSuited() const { return Suit >= .5f; }
	bool IsMirroring() const;

protected:
	virtual void BeginPlay() override;

private:
	void Pose();
	void Show(bool bShow);
	/** Paint and visor for in or out of the suit, and the mirror on or off. */
	void Dress();
	void PlaceVisor();

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SuitPaint;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MannequinPaint;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> VisorPaint;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Reflection;
	/** The visor relative to the head bone, worked out from the first pose after he's shown (standing, facing ahead). */
	FTransform VisorOnHead;
	bool bVisorFitted = false;
	/** HideInScene actors with whether each was hidden before. */
	TArray<TPair<TWeakObjectPtr<AActor>, bool>> Hid;
};

/**
 * The launch key in its lock, sitting on the game's LAUNCH KEYS box. Sequencer turns it with Turn (0 = off, 1 = a
 * quarter turn to LAUNCH) and the lamp beside it goes from red to green; a stand-in robber's hand reaches for its grip.
 */
UCLASS()
class PRODUCTIONGAME_API ARobberLaunchKey : public AActor
{
	GENERATED_BODY()

public:
	ARobberLaunchKey();

	/** Hides the floating text of whatever the key sits on (the game's LAUNCH KEYS box) for the scene, and back. */
	static void DressScene(UWorld* World, bool bInScene);

	/** Where a hand holds the key. */
	FVector GetGripLocation() const;
	/** The key's own turn so far, as a world rotation for the hand to follow. */
	FQuat GetTurnRotation() const;

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, BlueprintSetter = SetTurn, Category = "Scene", meta = (ClampMin = "0", ClampMax = "1"))
	float Turn = 0.f;

	/** Sequencer finds this by name (Set + the property) for the Turn track. */
	UFUNCTION(BlueprintSetter)
	void SetTurn(float Value);

	UPROPERTY(EditAnywhere, Category = "Scene") FLinearColor MetalColour = FLinearColor(.55f, .42f, .12f);
	UPROPERTY(EditAnywhere, Category = "Scene") FLinearColor TagColour = FLinearColor(.6f, .02f, .02f);
	/** BasicShapeMaterial (Color) for the parts, M_Greybox_Glow (Color, Glow) for the lamp. */
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<UMaterialInterface> PlainMaterial;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<UMaterialInterface> GlowMaterial;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Paint();
	void Apply();

	UStaticMeshComponent* AddPart(FName Name, UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);

	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Plate;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Barrel;
	/** Turns about its Z: the key rides on it. */
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<USceneComponent> Pivot;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Blade;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Bow;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> KeyTag;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Lamp;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LampPaint;
	/** Text hidden for the scene, with whether it was hidden before. */
	TArray<TPair<TWeakObjectPtr<USceneComponent>, bool>> HiddenText;
};

/** A launch pad floodlight: post, lamp head and a spot light that Sequencer brings up with Power (0 off, 1 full). */
UCLASS()
class PRODUCTIONGAME_API ARobberFloodlight : public AActor
{
	GENERATED_BODY()

public:
	ARobberFloodlight();

	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, BlueprintSetter = SetPower, Category = "Scene", meta = (ClampMin = "0"))
	float Power = 0.f;

	UFUNCTION(BlueprintSetter)
	void SetPower(float Value);

	/** Candelas at Power 1. */
	UPROPERTY(EditAnywhere, Category = "Scene") float Brightness = 400000.f;
	UPROPERTY(EditAnywhere, Category = "Scene") FLinearColor LightColour = FLinearColor(1.f, .93f, .8f);
	/** How high the lamp head sits on its post. */
	UPROPERTY(EditAnywhere, Category = "Scene", meta = (ClampMin = "0")) float PostHeight = 250.f;
	/** What the lamp points at, relative to the light (drag the diamond in the editor). */
	UPROPERTY(EditAnywhere, Category = "Scene", meta = (MakeEditWidget = true)) FVector AimAt = FVector(1000.f, 0.f, 1000.f);
	UPROPERTY(EditAnywhere, Category = "Scene", meta = (ClampMin = "1", ClampMax = "80")) float ConeAngle = 18.f;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<UMaterialInterface> PlainMaterial;
	UPROPERTY(EditAnywhere, Category = "Scene") TObjectPtr<UMaterialInterface> GlowMaterial;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void Apply();

	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Post;
	/** Aims along its +X at AimAt; the head and the light ride on it. */
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<USceneComponent> Aim;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<UStaticMeshComponent> Lens;
	UPROPERTY(VisibleAnywhere, Category = "Scene") TObjectPtr<USpotLightComponent> Spot;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LensPaint;
};
