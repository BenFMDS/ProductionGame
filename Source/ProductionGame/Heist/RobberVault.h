#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberInteractables.h"
#include "RobberVault.generated.h"

class ARobberVaultDial;
class UPlayerQuestComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * The gem vault's door: shut until the crew sets its dials (BP_VaultDial) to a 4-digit code that's new each night.
 * Nobody starts with the code; ARobberNightDirector puts its digits on random quests, and turning one in writes them
 * into that player's journal, so the crew has to pool what they earned. Swings open (outward) when the dials match,
 * which finishes the "Open the vault" job.
 *
 * Local frame: +X faces out of the vault, the door spans Width along Y, hinged at -Y.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberVaultDoor : public AActor
{
	GENERATED_BODY()

public:
	ARobberVaultDoor();

	static const FName JobId;
	static constexpr int32 CodeLength = 4;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault")
	double Width = 300.;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault")
	double Height = 400.;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault")
	double Thickness = 40.;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault")
	float OpenSeconds = 3.f;

	/**
	 * Optional real model for the door (Harry's Vault_Door_f): on the hinge, so it swings open with the code. With one set the
	 * grey slab and wheel are hidden but the slab still blocks. Transform is relative to the hinge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault|Model")
	TObjectPtr<UStaticMesh> DoorModelMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault|Model")
	FTransform DoorModelTransform;

	/**
	 * Where the model's own hinge is, in the door's space. The model is smaller than the doorway, so it turns round its own
	 * edge (same angle as the slab), and the dials ride on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault|Model")
	FVector DoorModelHinge = FVector::ZeroVector;

	/** Slides the row of dials along the door from the model's middle, so none ends up under the frame's lip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault|Model")
	double DialShift = 0.;

	/** Optional frame round the doorway (Harry's Vault_Wall_f). Doesn't move, doesn't block. Relative to the door's root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault|Model")
	TObjectPtr<UStaticMesh> FrameModelMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Vault|Model")
	FTransform FrameModelTransform;

	bool IsOpen() const { return bOpen; }

	/** Server only (it isn't replicated): the night's code. */
	const TArray<int32>& GetCode() const { return Code; }

	/** Where dial Index sits on the door's outside face (dials are attached to the hinge, so they swing with it). */
	FTransform GetDialTransform(int32 Index) const;
	/** What the dials hang on: the model's hinge when there's a model, else the slab's. */
	USceneComponent* GetHinge() const { return DoorModelMesh ? ModelHinge.Get() : Hinge.Get(); }

	/** Server only. Opens the door when every dial reads its digit. */
	void CheckDials();

	/** Server only (dev menu). Turns every dial to its digit, which opens the door as if the crew dialled it. */
	void DialInCode();

protected:
	virtual void BeginPlay() override;

private:
	void UpdateSign();

	/** How far out (hinge-space X) the door's front face is: the slab's, or the model's when there is one. */
	double FrontFaceX() const;

	UFUNCTION()
	void OnRep_Open();

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<USceneComponent> ModelHinge;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<UStaticMeshComponent> DoorModel;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<UStaticMeshComponent> FrameModel;


	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<USceneComponent> DoorRoot;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<UStaticMeshComponent> Slab;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<UStaticMeshComponent> Wheel;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<UTextRenderComponent> Sign;

	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	TArray<int32> Code;
	double OpenStartTime = -1.;
};

/** One of the vault door's dials. Interact turns it to the next digit. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberVaultDial : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberVaultDial();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Vault")
	TObjectPtr<ARobberVaultDoor> Door;

	/** Which digit of the code this dial sets, 0 = first. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Vault")
	int32 Index = 0;

	int32 GetDigit() const { return Digit; }
	/** Server only. */
	void SetDigit(int32 NewDigit);

protected:
	virtual void BeginPlay() override;
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	bool CanTurn(const ARobberGameState* GameState) const;

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<USceneComponent> DialRoot;

	UPROPERTY(Replicated)
	int32 Digit = 0;
};

/**
 * One laser across the vault, live while the museum has power. Walking into a live beam during the heist sets the
 * alarm off (ARobberGameState::TripLaser). The generator kills them all. Scale X is the beam's length in metres.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLaserBeam : public AActor
{
	GENERATED_BODY()

public:
	ARobberLaserBeam();

	virtual void Tick(float DeltaSeconds) override;

	bool IsLive() const { return bLive; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnBeamOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TripBy(AActor* Actor);

	UPROPERTY(VisibleAnywhere, Category = "Robber|Vault")
	TObjectPtr<UStaticMeshComponent> Beam;

	bool bLive = true;
};
