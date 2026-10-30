#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberInteractables.h"
#include "RobberBreakInProps.generated.h"

class USceneComponent;

/**
 * The skylight's glass. Interact to start or stop cutting; each cutter speeds it up and adds noise. Noise
 * cools while nobody cuts. If it maxes out a guard hears it: cutting stops until it has gone quiet.
 * Once cut, someone has to drop the rope from the anchor; then Interact climbs down it. Anyone who jumps
 * through before the rope is down is put back on the roof (it's too far to drop).
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSkylight : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberSkylight();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Width and length of the glass. The rope anchor sits on the roof just past the -Y end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	FVector2D PaneSize = FVector2D(1000., 2200.);

	/** How far the rope hangs: from the glass down to the floor below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight", meta = (ClampMin = "100"))
	float RopeLength = 1500.f;

	/** Seconds one player takes to cut the glass alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight", meta = (ClampMin = "0.1"))
	float CutSeconds = 8.f;

	/** Noise added per second per cutter (1 = a guard hears it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	float NoisePerCutterPerSecond = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	float QuietPerSecond = 0.25f;

	/** After a guard hears it, nobody can cut until the noise drops below this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	float ResumeBelowNoise = 0.4f;

	/** Cutters further than this from the glass stop cutting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	float CutReach = 350.f;

	bool IsCut() const { return bCut; }
	bool IsRopeDown() const { return bRopeDown; }
	float GetCutProgress() const { return CutProgress; }
	float GetNoise() const { return Noise; }
	bool IsTooLoud() const { return bTooLoud; }
	bool IsCutting(const APlayerState* Player) const { return Cutters.Contains(Player); }
	int32 GetCutterCount() const { return Cutters.Num(); }
	/** Times a guard has heard it this night. */
	int32 GetAlerts() const { return Alerts; }
	/** Foot of the rope, on the floor below. */
	FVector GetClimbDestination() const;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Skylight")
	TObjectPtr<USceneComponent> Root;

	/** Stays solid after the glass is gone, so there is still something to look at and press to climb. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	TObjectPtr<UStaticMeshComponent> RopeAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Skylight")
	TObjectPtr<UStaticMeshComponent> Rope;

private:
	void ServerTick(float DeltaSeconds, ARobberGameState* GameState);
	void SendBackUpJumpers();

	UPROPERTY(ReplicatedUsing = OnRep_Cut)
	bool bCut = false;

	UPROPERTY(ReplicatedUsing = OnRep_Cut)
	bool bRopeDown = false;

	UPROPERTY(Replicated)
	float CutProgress = 0.f;

	UPROPERTY(Replicated)
	float Noise = 0.f;

	UPROPERTY(Replicated)
	bool bTooLoud = false;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<APlayerState>> Cutters;

	UPROPERTY(Replicated)
	int32 Alerts = 0;

	UFUNCTION()
	void OnRep_Cut();
};

/**
 * One of the sewer valves. Interact to open it; it springs shut after OpenSeconds unless the water it feeds
 * drains first, so a crew has to open them together. A crew of one gets it jammed open.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSewerValve : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberSewerValve();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Sewer", meta = (ClampMin = "1"))
	float OpenSeconds = 6.f;

	bool IsOpen(const ARobberGameState* GameState) const;
	/** Server only. Keeps it open for good. */
	void Jam();

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	/** Server world time it shuts at. */
	UPROPERTY(Replicated)
	double OpenUntil = 0.;

	UPROPERTY(Replicated)
	bool bJammed = false;
};

/** Deep water blocking a tunnel. Drains for good once all its valves are open at the same time. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSewerWater : public AActor
{
	GENERATED_BODY()

public:
	ARobberSewerWater();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Sewer")
	TArray<TObjectPtr<ARobberSewerValve>> Valves;

	/** Size of the water block, centred on the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Sewer")
	FVector Size = FVector(400., 600., 400.);

	bool IsDrained() const { return bDrained; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Robber|Sewer")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Sewer")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Sewer")
	TObjectPtr<UTextRenderComponent> LabelText;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Drained)
	bool bDrained = false;

	UFUNCTION()
	void OnRep_Drained();
};

/** Pick it up to carry it (it's a key, UPlayerQuestComponent::Keys, cleared when the night ends). */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberCrowbar : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberCrowbar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Sewer")
	FName KeyId = TEXT("Crowbar");

	bool IsTaken() const { return bTaken; }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;

	UFUNCTION()
	void OnRep_Taken();
};

/** Grate in the tunnel roof under the museum. Pry it open with the crowbar, then Interact climbs up through it. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSewerGrate : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberSewerGrate();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Sewer")
	FName KeyId = TEXT("Crowbar");

	/** Where a climbing player comes out, relative to the grate (unscaled). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Sewer", meta = (MakeEditWidget = "true"))
	FVector ClimbOffset = FVector(0, 0, 500);

	bool IsOpen() const { return bOpen; }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UFUNCTION()
	void OnRep_Open();
};

/** Interact to climb to ClimbOffset. Night 1's basement stairwell has no stairs, so the way up is a ladder. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLadder : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberLadder();

	/** Where a climbing player comes out, relative to the ladder (unscaled, yaw only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Ladder", meta = (MakeEditWidget = "true"))
	FVector ClimbOffset = FVector(0, 0, 600);

	FVector GetClimbDestination() const;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
};
