#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberInteractables.generated.h"

class APlayerState;
class ARobberEntryPoint;
class ARobberGameState;
class URobberHighlightComponent;
class URobberMarkerComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Base for heist props the player presses Interact on. BPI_Interaction is a Blueprint interface, so each
 * of these has a thin Blueprint child that implements it: Interact calls HandleInteract and
 * GetInteractData returns GetInteractPrompt.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API ARobberInteractable : public AActor
{
	GENERATED_BODY()

public:
	ARobberInteractable();

	/**
	 * BPC_Interaction calls Interact locally and again on the server (twice on a listen-server host), so
	 * only the server acts, and a second press from the same player straight after the first is dropped
	 * (toggles like the skylight would otherwise undo themselves).
	 */
	UFUNCTION(BlueprintCallable, Category = "Robber|Interact")
	void HandleInteract(AActor* InteractingPlayer);

	UFUNCTION(BlueprintPure, Category = "Robber|Interact")
	FString GetInteractPrompt() const;

	/**
	 * From BPC_Interaction's trace, before it overwrites Hit Actor: takes the outline off the old target. The trace's
	 * own clear-up runs on the new Hit Actor, so aiming straight from a prop onto a wall or another prop left it lit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Robber|Interact")
	static void ClearStaleOutline(AActor* Previous, AActor* Now);

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The way in this prop belongs to. When set, it only works (and only shows its label) while that entry is
	 * the one the crew voted for, so a night with several ways in only runs one of them.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<ARobberEntryPoint> OwningEntry;

	bool IsActive(const ARobberGameState* GameState) const;
	/** Shows or removes a part: hidden with its children, and nothing to bump into while it's gone. */
	static void ShowPart(UPrimitiveComponent* Part, bool bShown);

	URobberHighlightComponent* GetHighlight() const { return Highlight; }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) {}
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const { return Prompt; }
	virtual FText GetLabel(const ARobberGameState* GameState) const { return Label; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<UTextRenderComponent> LabelText;

	/** Red outline while aimed at, by default. Subclasses that light up another way change its Mode. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<URobberHighlightComponent> Highlight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	FText Prompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	FText Label;

private:
	/** Server only. World time of each player's last handled press. */
	TMap<TWeakObjectPtr<APlayerState>, double> LastInteractTime;
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
