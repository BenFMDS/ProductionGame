#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberSecurityCamera.generated.h"

class APawn;
class ARobberGameState;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ERobberCameraStatus : uint8
{
	/** Power's cut or the crew isn't in the museum. */
	Off,
	Sweeping,
	/** Someone's in the circle; the sweep has stopped on them. */
	Watching,
	/** Seen for long enough: heat added, guards told. Stays frozen on the spot until a guard has checked it. */
	Spotted,
};

/**
 * Wall/ceiling camera that pans side to side. Where it looks is shown as a circle on the floor (light centre, darker
 * border; blue sweeping, yellow watching, red spotted), and that circle is exactly what it sees: a player standing in
 * it, in line of sight and not in a hiding spot, for SpotSeconds gets the crew heat and sends the guards
 * (ARobberGameState::ReportSighting). Then it stops panning and stares at the spot until a guard calls
 * ARobberGameState::DisturbanceChecked there (or GuardFallbackSeconds pass). Faster at higher heat tiers; dead while
 * the power's cut. Server decides; yaw and status replicate, every machine places its own circle from them.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSecurityCamera : public AActor
{
	GENERATED_BODY()

public:
	ARobberSecurityCamera();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Robber|Camera")
	ERobberCameraStatus GetStatus() const { return Status; }

	/** Where the camera's looking on the floor. False when the view hits nothing within Range. */
	bool GetFloorCircle(FVector& OutCentre) const;

	/** Whether Pawn is in the circle and in plain sight right now (ignores the stage and power). */
	bool CanSee(const APawn* Pawn) const;

	/** Server only. A spotted camera waiting on a guard: is its disturbance near Location? */
	bool IsAwaitingGuardNear(const FVector& Location, float Radius) const;

	/** Server only. A guard has looked: start panning again. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Robber|Camera")
	void ResumeScanning();

	/** Degrees either side of the facing the head pans. 0 = fixed camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float SweepHalfAngle = 45.f;

	/** Seconds for one pan there and back at Calm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float SweepPeriod = 8.f;

	/** How far the head looks down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float PitchDown = 30.f;

	/** How far it can see along its view to find the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float Range = 1500.f;

	/** The circle on the floor: what the camera sees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float CircleRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float BorderWidth = 25.f;

	/** Seconds in the circle before it's a sighting, at Calm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float SpotSeconds = 1.2f;

	/** After a sighting, seconds before this camera can report again (it can while still waiting on the guard). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float SpottedCooldown = 6.f;

	/**
	 * Seconds a spotted camera waits for a guard before panning again on its own. Guards don't call
	 * DisturbanceChecked yet, so without this every camera would freeze for good. 0 = wait for a guard however long.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Camera")
	float GuardFallbackSeconds = 45.f;

	/** Sweep speed and spot time multipliers per heat tier (Calm, Suspicious, Alert, Alarm). */
	static float SweepScale(uint8 Tier);
	static float SpotScale(uint8 Tier);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<UStaticMeshComponent> Mount;

	/** Yaws with the sweep; pitched down by PitchDown. Everything that looks hangs off this. */
	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<USceneComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<UStaticMeshComponent> Lens;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<USpotLightComponent> Cone;

	/** Dark disc under a slightly smaller light one = circle with a border. World-placed, not attached to the head's rotation. */
	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<UStaticMeshComponent> CircleBorder;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Camera")
	TObjectPtr<UStaticMeshComponent> CircleFill;

private:
	bool IsWatching(const ARobberGameState* GameState) const;
	void TickServer(float DeltaSeconds, ARobberGameState* GameState);
	void ApplyLook();

	UPROPERTY(Replicated)
	float SweepYaw = 0.f;

	UPROPERTY(Replicated)
	ERobberCameraStatus Status = ERobberCameraStatus::Off;

	/** Server only. */
	float SweepPhase = 0.f;
	float WatchTime = 0.f;
	double SpottedUntil = 0.;
	bool bAwaitingGuard = false;
	double AwaitingSince = 0.;
	FVector Disturbance = FVector::ZeroVector;
	TWeakObjectPtr<APawn> Target;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BorderMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FillMaterial;

	/** Each machine: what ApplyLook last showed. */
	ERobberCameraStatus ShownStatus = ERobberCameraStatus::Spotted;
};
