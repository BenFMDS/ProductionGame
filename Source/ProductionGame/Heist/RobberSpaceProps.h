#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "RobberSpaceProps.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * One piece of the Escape Velocity space shot (Ben's sketch: the Earth with the sun peeking round it, the rocket on its
 * way to the Moon, stars behind). They sit in L_GameLevel far above the museum (folder SpaceEnding), hidden in game, and
 * only the ending on one machine shows them to that machine, so nobody can stumble on them. Built from basic shapes in
 * flat unlit colours (M_SpaceFlat), shaded by a fixed "light from the upper right" rather than real lights, so the
 * level's own sun and sky can't change the look.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API ARobberSpaceProp : public AActor
{
	GENERATED_BODY()

public:
	ARobberSpaceProp();

	/** Shows or hides every space prop in the world, on this machine only, starting their animation from the top. */
	static void ShowAll(UWorld* World, bool bShow);
	void ShowInShot(bool bShow);
	bool IsInShot() const { return bInShot; }

	/** M_SpaceFlat: Color/ShadeColor/LightFrom/Softness/Bias/Glow/Twinkle. */
	UPROPERTY(EditAnywhere, Category = "Space")
	TObjectPtr<UMaterialInterface> FlatMaterial;

	/** Where the shading light comes from, in world space. The camera looks along +X, so this is upper right, a bit in front. */
	UPROPERTY(EditAnywhere, Category = "Space")
	FVector LightFrom = FVector(-.4, .8, .45);

	/** Where the prop drifts to during the shot, relative to itself (drag the diamond in the editor). Zero stays put. */
	UPROPERTY(EditAnywhere, Category = "Space", meta = (MakeEditWidget = true))
	FVector TravelTo = FVector::ZeroVector;

	/** How long the drift takes; it eases out, slowing as it arrives. */
	UPROPERTY(EditAnywhere, Category = "Space", meta = (ClampMin = "0.1"))
	float TravelSeconds = 14.f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Makes the materials (and anything else built from settings). Runs in the editor and again at BeginPlay. */
	virtual void Rebuild() {}
	/** Per-frame motion while shown; Seconds counts from the shot starting. */
	virtual void Animate(float Seconds) {}

	UStaticMeshComponent* AddPart(FName Name, UStaticMesh* Mesh, USceneComponent* Parent);
	UMaterialInstanceDynamic* Paint(UPrimitiveComponent* Part, const FLinearColor& Colour, const FLinearColor& Shade, float Glow = 1.f);
	/** 0 to 1 over TravelSeconds, easing out. */
	float TravelAlpha(float Seconds) const;

	static UStaticMesh* Shape(const TCHAR* Name);

private:
	FTransform Home;
	float ShotTime = 0.f;
	bool bInShot = false;
};

UCLASS()
class PRODUCTIONGAME_API ARobberSpaceEarth : public ARobberSpaceProp
{
	GENERATED_BODY()

public:
	ARobberSpaceEarth();

	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor SeaColour = FLinearColor(FColor(0x00, 0xa2, 0xe8));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor NightColour = FLinearColor(FColor(0x00, 0x4a, 0x9a));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor LandColour = FLinearColor(FColor(0xb5, 0xe6, 0x1d));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor LandShadeColour = FLinearColor(FColor(0x5c, 0x8a, 0x10));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor AirColour = FLinearColor(FColor(0x9c, 0xe8, 0xff));
	/** Degrees a second; the continents start where Ben drew them. */
	UPROPERTY(EditAnywhere, Category = "Space") float SpinSpeed = .6f;

protected:
	virtual void Rebuild() override;
	virtual void Animate(float Seconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<USceneComponent> Globe;
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Sea;
	/** A disc behind the globe, just wider and nudged to the sun's side: the thin lit rim of air. */
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Air;
	UPROPERTY(VisibleAnywhere, Category = "Space") TArray<TObjectPtr<UStaticMeshComponent>> Continents;
};

UCLASS()
class PRODUCTIONGAME_API ARobberSpaceSun : public ARobberSpaceProp
{
	GENERATED_BODY()

public:
	ARobberSpaceSun();

	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor SunColour = FLinearColor(FColor(0xff, 0xf2, 0x00));
	/** Over 1 blooms on the space camera; this is the glow round it. */
	UPROPERTY(EditAnywhere, Category = "Space") float Glow = 6.f;

protected:
	virtual void Rebuild() override;
	virtual void Animate(float Seconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Disc;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DiscPaint;
};

UCLASS()
class PRODUCTIONGAME_API ARobberSpaceMoon : public ARobberSpaceProp
{
	GENERATED_BODY()

public:
	ARobberSpaceMoon();

	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor MoonColour = FLinearColor(FColor(0x9a, 0x9a, 0x9a));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor MoonShadeColour = FLinearColor(FColor(0x45, 0x45, 0x4a));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor CraterColour = FLinearColor(FColor(0x70, 0x70, 0x70));

protected:
	virtual void Rebuild() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Ball;
	UPROPERTY(VisibleAnywhere, Category = "Space") TArray<TObjectPtr<UStaticMeshComponent>> Craters;
};

/** Nose along +X. Flies to TravelTo over the shot, bobbing, flame flickering. */
UCLASS()
class PRODUCTIONGAME_API ARobberSpaceRocket : public ARobberSpaceProp
{
	GENERATED_BODY()

public:
	ARobberSpaceRocket();

	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor HullColour = FLinearColor(FColor(0xb4, 0xb4, 0xba));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor HullShadeColour = FLinearColor(FColor(0x5e, 0x5e, 0x64));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor TrimColour = FLinearColor(FColor(0x6a, 0x6a, 0x72));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor WindowColour = FLinearColor(FColor(0x1c, 0x3c, 0x6e));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor FlameColour = FLinearColor(FColor(0xed, 0x1c, 0x24));
	UPROPERTY(EditAnywhere, Category = "Space") FLinearColor FlameCoreColour = FLinearColor(FColor(0xff, 0xc9, 0x0e));

protected:
	virtual void Rebuild() override;
	virtual void Animate(float Seconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<USceneComponent> Craft;
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Nose;
	UPROPERTY(VisibleAnywhere, Category = "Space") TArray<TObjectPtr<UStaticMeshComponent>> Fins;
	UPROPERTY(VisibleAnywhere, Category = "Space") TArray<TObjectPtr<UStaticMeshComponent>> Windows;
	UPROPERTY(VisibleAnywhere, Category = "Space") TArray<TObjectPtr<UStaticMeshComponent>> WindowRims;
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Flame;
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> FlameCore;
	FVector FlameScale;
	FVector FlameCoreScale;
};

/**
 * Put it where the space camera is: a black sphere round the whole shot (so the level's sky never shows) with little
 * dashes of stars, as Ben drew them, scattered in front of the camera. Turns slowly so the sky drifts.
 */
UCLASS()
class PRODUCTIONGAME_API ARobberSpaceStars : public ARobberSpaceProp
{
	GENERATED_BODY()

public:
	ARobberSpaceStars();

	UPROPERTY(EditAnywhere, Category = "Space", meta = (ClampMin = "0")) int32 StarCount = 450;
	/** Same sky on every machine. */
	UPROPERTY(EditAnywhere, Category = "Space") int32 Seed = 1969;
	UPROPERTY(EditAnywhere, Category = "Space") float StarDistance = 60000.f;
	UPROPERTY(EditAnywhere, Category = "Space") float SkyRadius = 70000.f;
	/** Degrees a second the sky turns. */
	UPROPERTY(EditAnywhere, Category = "Space") float DriftSpeed = .15f;

protected:
	virtual void Rebuild() override;
	virtual void Animate(float Seconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UStaticMeshComponent> Sky;
	UPROPERTY(VisibleAnywhere, Category = "Space") TObjectPtr<UInstancedStaticMeshComponent> Stars;
};

/** The view during the space shot. Looks along +X with its own fixed exposure and bloom; eases forward a little. */
UCLASS()
class PRODUCTIONGAME_API ARobberSpaceCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ARobberSpaceCamera();

	static ARobberSpaceCamera* Find(UWorld* World);
	void StartShot();
	void EndShot();

	/** How far forward it moves over the shot, easing out: a slow push in. */
	UPROPERTY(EditAnywhere, Category = "Space") float PushIn = 500.f;
	UPROPERTY(EditAnywhere, Category = "Space", meta = (ClampMin = "0.1")) float PushSeconds = 14.f;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	FVector Home = FVector::ZeroVector;
	float ShotTime = 0.f;
	bool bShooting = false;
};
