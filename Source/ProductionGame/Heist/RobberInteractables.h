#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberPropBase.h"
#include "RobberInteractables.generated.h"

class APlayerState;
class ARobberEntryPoint;
class ARobberGameState;
class URobberHighlightComponent;
class URobberInteractComponent;
class URobberMarkerComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** How the BP's model parts (under ModelRoot) are fitted to Mesh's box (the unit cube Mesh is scaled to). */
UENUM(BlueprintType)
enum class ERobberModelFit : uint8
{
	/** Parts are placed by hand in Mesh's own space, where the box is -50..50 on every axis. */
	None,
	/** The parts' combined bounds are stretched to fill the box: for meshes that should match the gap exactly (doors). */
	Stretch,
	/** As big as fits inside the box without changing shape, centred (keys). */
	KeepProportions,
};

/**
 * Base for heist props whose behaviour is in C++. BPI_Interaction is a Blueprint interface, so each
 * of these has a thin Blueprint child that implements it: Interact calls HandleInteract and
 * GetInteractData returns GetInteractPrompt. Both pass straight to the Interact component.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API ARobberInteractable : public ARobberPropBase
{
	GENERATED_BODY()

public:
	ARobberInteractable();

	/** Server only, one press per player per 0.2 s: see URobberInteractComponent::bServerOnly. */
	UFUNCTION(BlueprintCallable, Category = "Robber|Interact")
	void HandleInteract(AActor* InteractingPlayer);

	UFUNCTION(BlueprintPure, Category = "Robber|Interact")
	FString GetInteractPrompt() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLoad() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;

	/** Any part under ModelRoot has a mesh. */
	bool HasModel() const;

	/** The Interact component's OwningEntry gate. */
	bool IsActive(const ARobberGameState* GameState) const;
	/** Shows or removes a part: hidden with its children, and nothing to bump into while it's gone. */
	static void ShowPart(UPrimitiveComponent* Part, bool bShown);

	virtual FText GetInteractText(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual void OnInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override { OnServerInteract(Pawn, Player, GameState); }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) {}
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const { return Prompt; }
	virtual FText GetLabel(const ARobberGameState* GameState) const { return Label; }

	/**
	 * The prop's box: a unit cube scaled to its size, which is what the interact trace hits and what blocks. The shape is
	 * set in the BP_ child. With a model (and bModelReplacesMesh) it stays as the invisible collision box.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<UTextRenderComponent> LabelText;

	/** Under Mesh. Its direct children are the model, fitted to Mesh's box by ModelFit. Placed by FitModel, not by hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Model")
	TObjectPtr<USceneComponent> ModelRoot;

	/** Model1..Model6: empty unless the BP_ child gives them meshes. No collision: Mesh does that. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Model")
	TArray<TObjectPtr<UStaticMeshComponent>> ModelParts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Model")
	ERobberModelFit ModelFit = ERobberModelFit::None;

	/**
	 * Turns the whole model before it's fitted (Stretch/KeepProportions only). Turn the model here, not its parts: a part
	 * turned inside an unevenly stretched box gets the wrong axes stretched (a door turned 90 came out 20 cm-box narrow).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Model")
	FRotator ModelRotation = FRotator::ZeroRotator;

	/** Hide Mesh when there's a model. Off for models that dress the box rather than replace it (a frame round the skylight glass). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Model")
	bool bModelReplacesMesh = true;

	/** Model parts with no collision under ModelRoot, made in the constructor. */
	UStaticMeshComponent* MakeModelPart(const TCHAR* Name);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	FText Prompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	FText Label;

private:
	void FitModel();

	/**
	 * Load-only: maps saved before the Interact component kept the way in here, under this name. PostLoad moves it to
	 * InteractComponent->OwningEntry and clears it, so a resave stops writing it. Use the component's.
	 */
	UPROPERTY()
	TObjectPtr<ARobberEntryPoint> OwningEntry;
};

/** Lobby board. Interact to vote for this way in. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberVoteStation : public ARobberInteractable
{
	GENERATED_BODY()

public:
	/** Only for an entry in the same level (L_Ben). In L_GameLevel the entries are in the night, so use EntryId. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Lobby")
	TObjectPtr<ARobberEntryPoint> Entry;

	/** Votes for the loaded night's entry with this id. The board is blank on nights without one. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Lobby")
	FName EntryId;

	ARobberEntryPoint* GetEntry(const ARobberGameState* GameState) const;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;
};

/** Closing-Time Tour ticket desk. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberTicketDesk : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberTicketDesk();

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
};

/**
 * Something big enough to climb inside. One player at a time. Inside, guards and cameras can't see you unless a guard
 * was already onto you when you climbed in. The object's +X side is its open front; the unit cube is scaled to its size.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberHidingSpot : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberHidingSpot();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** A guard's detection of this player at or above this when they climb in means the guard saw it happen. */
	UPROPERTY(EditAnywhere, Category = "Robber|Hiding")
	float SeenDetection = 50.f;

	/** Above 0, the hider's camera sits this high (cm) above the spot's floor, for low spots like under a desk. 0 keeps standing height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Hiding", meta = (ClampMin = "0"))
	float HiddenEyeHeight = 0.f;

	double GetFloorZ() const { return GetActorLocation().Z - GetActorScale3D().Z * 50.; }

	/** Just inside the front, so a guard who saw you go in can get within its catch radius. */
	FVector GetHideLocation() const;
	FVector GetExitLocation() const;
	FRotator GetFacing() const { return GetActorForwardVector().Rotation(); }
	const FText& GetDisplayLabel() const { return Label; }

	APawn* GetOccupant() const { return Occupant; }
	void SetOccupant(APawn* Pawn);

	/** On the hider's own machine only: the door panel is right in front of their camera, and would black out the view. */
	void SetViewedFromInside(bool bInside) { Opening->SetVisibility(!bInside); }

	/** Server only. Is any guard targeting this pawn with at least this much detection? */
	static bool IsGuardOnto(const APawn* Pawn, float MinDetection);

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;

	/** Dark panel on the front face: this is what reads as "you could get in there". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Hiding")
	TObjectPtr<UStaticMeshComponent> Opening;

private:
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant = nullptr;
};

/**
 * A door that only a player holding its key can open (keys come from quests, UQuestDefinition::RewardKey).
 * Once unlocked it stays open for the whole crew until the night is reloaded.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLockedDoor : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberLockedDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	FName KeyId;

	/** What the prompt calls the key, e.g. "Generator Room key". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	FText KeyName;

	/** The door's +Y side (local) is the inside: anyone there opens it without the key, like a staff door with a push bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	bool bOpensFromInside = false;

	bool IsOpen() const { return bOpen; }
	bool IsInside(const APawn* Pawn) const;

	/** Seconds the leaves take to slide into the walls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Door", meta = (ClampMin = "0"))
	float SlideTime = 0.8f;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

	/** Model parts (meshes set in BP_LockedDoor). The leaves slide apart, each a leaf's width away from the middle, into the walls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	TObjectPtr<UStaticMeshComponent> LeftLeaf;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	TObjectPtr<UStaticMeshComponent> RightLeaf;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;

	UFUNCTION()
	void OnRep_Open();

	/** Where a leaf ends up open, in ModelRoot space: out along its own width, away from the middle. */
	static FVector OpenOffset(const UStaticMeshComponent* Leaf);
	void StepSlide();

	FVector LeftClosed = FVector::ZeroVector;
	FVector RightClosed = FVector::ZeroVector;
	double SlideStart = 0.;
	FTimerHandle SlideTimer;
};

/** A key lying somewhere in the museum. Whoever picks it up carries it (UPlayerQuestComponent::Keys) until the night ends. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberKeyPickup : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberKeyPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	FName KeyId;

	/** What the prompt and the crew message call it, e.g. "staff badge". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	FText KeyName;

	bool IsTaken() const { return bTaken; }

	virtual void Tick(float DeltaSeconds) override;

	bool IsAlertShowing() const;

protected:
	/** The "!" over the key while it's still there, drawn per machine: each player sees it only near the key themselves (MarkRange). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Door")
	TObjectPtr<URobberMarkerComponent> Alert;

	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Taken)
	bool bTaken = false;

	UFUNCTION()
	void OnRep_Taken();
};
