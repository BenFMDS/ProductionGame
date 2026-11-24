#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberInteractables.h"
#include "RobberLaunch.generated.h"

class UMaterialInterface;
class USoundBase;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobberLaunchEvent, APlayerState*, Launcher);

/**
 * One hidden step of the Escape Velocity ending (NADA, Night 5): fuel pumps, oxygen and so on. No prompt, no outline and
 * no label, but E still works on it. Once done it stays done, and Mission Control's ARobberLaunchBoard shows it green.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLaunchTask : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberLaunchTask();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The line on the board, e.g. "FUEL PUMPS". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	FText TaskName;

	/** Board order, lowest first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	int32 Order = 0;

	/** The small lamp on the task: OffMaterial until it's done, then OnMaterial. Unset = the lamp keeps its own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	TObjectPtr<UMaterialInterface> OffMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	TObjectPtr<UMaterialInterface> OnMaterial;

	UFUNCTION(BlueprintPure, Category = "Robber|Launch")
	bool IsDone() const { return bDone; }

	/** Server only. Marks it done whatever the stage (the dev menu's "Secrets: all GO"). */
	void MarkDone();

	/** Every task in World, by Order. */
	static TArray<ARobberLaunchTask*> All(const UWorld* World);
	/** True when World has tasks and all of them are done. */
	static bool AllDone(const UWorld* World);

protected:
	virtual void BeginPlay() override;
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override { return FText::GetEmpty(); }
	virtual FText GetLabel(const ARobberGameState* GameState) const override { return FText::GetEmpty(); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	TObjectPtr<UStaticMeshComponent> Lamp;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Done)
	bool bDone = false;

	UFUNCTION()
	void OnRep_Done();
};

/**
 * The car keys in Mission Control that start the launch. Prompt "Launch" and the outline only once every
 * ARobberLaunchTask is done; before that it's as silent as the tasks.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberLaunchKeys : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberLaunchKeys();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Robber|Launch")
	bool IsLaunched() const { return bLaunched; }

	UFUNCTION(BlueprintPure, Category = "Robber|Launch")
	bool IsReady() const;

	/** Fires on every machine when someone turns the keys, as URobberLaunchEnding starts. */
	UPROPERTY(BlueprintAssignable, Category = "Robber|Launch")
	FRobberLaunchEvent OnLaunch;

	/** Starts on the key turn and plays on under the credits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	TSoftObjectPtr<USoundBase> LaunchMusic;

	/** Tests that only check the checklist turn this off, so the key turn doesn't end their run. */
	static bool bPlayEndingInTests;

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override { return FText::GetEmpty(); }

private:
	UPROPERTY(Replicated)
	bool bLaunched = false;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLaunch(APlayerState* Launcher);
};

/**
 * The launch checklist on Mission Control's main screen: one line per ARobberLaunchTask, red NO-GO until it's done, then
 * green GO, with the keys last. Each machine draws it from the replicated task states, so it needs no replication itself.
 */
UCLASS()
class PRODUCTIONGAME_API ARobberLaunchBoard : public AActor
{
	GENERATED_BODY()

public:
	ARobberLaunchBoard();

	virtual void Tick(float DeltaSeconds) override;

	/** What row Index reads now ("FUEL PUMPS", "GO"), for tests. False past the last row. */
	bool GetRow(int32 Index, FString& OutName, FString& OutStatus, FColor& OutColour) const;

	/** The screen behind the text. Faces +X; the text reads along -Y. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	TObjectPtr<UStaticMeshComponent> Panel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	TObjectPtr<UTextRenderComponent> Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robber|Launch")
	FText KeysName;

private:
	void Refresh();
	void MakeRows(int32 Count);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Names;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Statuses;
};
