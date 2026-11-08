#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Heist/RobberExhibitTalk.h"
#include "Heist/RobberInteractables.h"
#include "RobberPuzzle.generated.h"

class APawn;
class ARobberKeyPickup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobberPuzzleSolvedSignature, APawn*, SolvedBy);

/**
 * Sits on a puzzle's controller Blueprint (wires, combination, code lock, pressure plates). When the puzzle is solved its
 * graph calls Solve, which calls BPI_PuzzleSolver's On Puzzle Solved on every actor in Solve Targets: doors, chests and,
 * for the night, the exhibit whose quest is to solve it. Server only; Is Solved replicates.
 */
UCLASS(ClassGroup = (Robber), meta = (BlueprintSpawnableComponent, DisplayName = "Puzzle"))
class PRODUCTIONGAME_API URobberPuzzleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URobberPuzzleComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Which quest lines can ask for this puzzle. Set on each puzzle Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	ERobberPuzzleType PuzzleType = ERobberPuzzleType::Any;

	/** Everything that reacts when this is solved. Each must implement BPI_PuzzleSolver (map check warns, the PuzzleQuest test fails). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	TArray<TObjectPtr<AActor>> SolveTargets;

	/** What quests and the journal call it ({object}), e.g. "fuse box wires". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FText PuzzleName;

	/** Code lock only. The night director rolls a new one each night on the server; clients never get it, only its length. */
	UPROPERTY(EditAnywhere, Category = "Puzzle")
	FString Answer;

	/** The answer written for a person ("red, then blue"). Left empty, it's worked out from the puzzle (wire order, digits, plates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FText Hint;

	/** Pressure plates only: indexes into the controller's Plates that must all be pressed at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	TArray<int32> RequiredPlates;

	/** For the puzzle's own effects (lights, sounds). Fires on the server, and on clients when Is Solved arrives. */
	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FRobberPuzzleSolvedSignature OnSolved;

	/** Server only, once: calls On Puzzle Solved on every Solve Target. Solved By can be empty (pressure plates don't know who). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Puzzle")
	void Solve(APawn* SolvedBy);

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsSolved() const { return bIsSolved; }

	/** Pressure plates: true when every Required Plates entry is pressed (reads each plate's IsPressed). */
	UFUNCTION(BlueprintPure, Category = "Puzzle|Pressure Plates")
	bool ArePlatesPressed(const TArray<AActor*>& Plates) const;

	/** Pressure plates, from Tick: server only, solves once when they're all pressed. */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Pressure Plates")
	void CheckPlates(const TArray<AActor*>& Plates);

	/** Code lock screen: how many digits to take. */
	UFUNCTION(BlueprintPure, Category = "Puzzle|Code Lock")
	static int32 GetCodeLength(AActor* Lock);

	/** Code lock Interact: open the screen only on the machine of the player who used it, once, and not after it's solved. */
	UFUNCTION(BlueprintPure, Category = "Puzzle|Code Lock")
	static bool ShouldOpenCodeScreen(AActor* Lock, AActor* InteractingPlayer);

	/** Code lock screen Enter: sends the typed digits to the server. The screen hears back through its On Code Result event. */
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Code Lock")
	static void SubmitCode(AActor* Lock, int32 TypedNumber, int32 Digits);

	static URobberPuzzleComponent* Find(const AActor* Puzzle);

	/** Server. Also sets the length clients see. */
	void SetAnswer(const FString& NewAnswer);
	bool CheckAnswer(const FString& Typed) const { return !Answer.IsEmpty() && Typed == Answer; }
	FText GetHintText() const;
	/** Where the crew goes to solve it: the first wire / button / plate, or the actor itself. */
	FVector GetPuzzleLocation() const;

	/** Every exhibit that hears its puzzle is solved passes it here (BP_TalkingMaster -> AQuestDialogueCharacter). */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnExhibitPuzzleSolved, AActor* /*Exhibit*/, AActor* /*Puzzle*/, APawn* /*SolvedBy*/);
	static FOnExhibitPuzzleSolved OnExhibitPuzzleSolved;

	/** Fired for each target called, for tests. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTargetCalled, URobberPuzzleComponent* /*Puzzle*/, AActor* /*Target*/);
	static FOnTargetCalled OnTargetCalled;

	/** BPI_PuzzleSolver's class, or null if the asset is missing. */
	static UClass* GetSolverInterface();
	static const FName OnPuzzleSolvedName;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnRep_IsSolved();

	UPROPERTY(ReplicatedUsing = OnRep_IsSolved) bool bIsSolved = false;
	UPROPERTY(Replicated) int32 AnswerLength = 0;
};

/**
 * Greybox chest that a puzzle opens (it implements BPI_PuzzleSolver in BP_PuzzleChest). The night director deals what's
 * inside from the night's rewards: a key or tool lies in it as a pickup, notes (vault digits, a hint, a tip-off) go into
 * the journal of whoever reads them, and crew-wide favours pay out the moment it opens.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberPuzzleChest : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberPuzzleChest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** BP_PuzzleChest's On Puzzle Solved calls this. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Puzzle")
	void OpenChest(APawn* SolvedBy);

	/** Key and tool rewards spawn one of these inside. */
	UPROPERTY(EditAnywhere, Category = "Puzzle")
	TSubclassOf<ARobberKeyPickup> KeyPickupClass;

	/** Server, from the night director. Note is what a reader's journal gets; Digits are vault code positions and values. */
	void SetContents(const FRobberReward& InReward, const FText& InNote, const FText& InCrewMessage, const TArray<int32>& InDigitIndexes, const TArray<int32>& InDigitValues, int32 InCodeLength);

	bool IsOpen() const { return bOpen; }
	const FRobberReward& GetContents() const { return Contents; }
	ARobberKeyPickup* GetSpawnedKey() const { return SpawnedKey; }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;

private:
	UFUNCTION() void OnRep_Open();
	bool HasNote() const;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle") TObjectPtr<UStaticMeshComponent> Lid;
	UPROPERTY(ReplicatedUsing = OnRep_Open) bool bOpen = false;
	UPROPERTY(Replicated) bool bHasNote = false;
	UPROPERTY(Transient) TObjectPtr<ARobberKeyPickup> SpawnedKey;

	FRobberReward Contents;
	FText Note;
	FText CrewMessage;
	TArray<int32> DigitIndexes;
	TArray<int32> DigitValues;
	int32 CodeLength = 0;
};
