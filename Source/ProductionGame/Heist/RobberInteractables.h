#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobberInteractables.generated.h"

class APlayerState;
class ARobberEntryPoint;
class ARobberGameState;
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
	 * only the server acts and every action has to be safe to repeat.
	 */
	UFUNCTION(BlueprintCallable, Category = "Robber|Interact")
	void HandleInteract(AActor* InteractingPlayer);

	UFUNCTION(BlueprintPure, Category = "Robber|Interact")
	FString GetInteractPrompt() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) {}
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const { return Prompt; }
	virtual FText GetLabel(const ARobberGameState* GameState) const { return Label; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	TObjectPtr<UTextRenderComponent> LabelText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	FText Prompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Interact")
	FText Label;
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

/** An exhibit a ticket holder can hide in until closing. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberHidingSpot : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberHidingSpot();

	/** Where a hidden player waits, relative to this actor. */
	UPROPERTY(EditAnywhere, Category = "Robber|BreakIn", meta = (MakeEditWidget = "true"))
	FVector HideOffset = FVector(0, 0, 100);

	FVector GetHideLocation() const { return GetActorTransform().TransformPosition(HideOffset); }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
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
