#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "QuestDialogueCharacter.generated.h"

class UDialogueComponent;
class UDialogueDefinition;
class UMaterialInterface;
class UQuestGiverComponent;
class UQuestDefinition;

/** Parent for talking-character Blueprints. The Dialogue Component's asset is the runtime source of truth. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API AQuestDialogueCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AQuestDialogueCharacter();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Dialogue Component")) TObjectPtr<UDialogueComponent> NativeDialogue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest", meta=(DisplayName="Quest Giver Component")) TObjectPtr<UQuestGiverComponent> NativeQuestGiver;
	/** Legacy compatibility field. It is copied to the Dialogue Component only when that component has no assigned asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Dialogue Asset (Legacy)")) TObjectPtr<UDialogueDefinition> NativeDialogueDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(DisplayName="Available Quests")) TArray<TObjectPtr<UQuestDefinition>> NativeQuestDefinitions;
	/** Placeholder colour for the Model Mesh until this NPC has real art. Leave empty to keep the mesh's own material. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Temp Cube Material")) TObjectPtr<UMaterialInterface> TempCubeMaterial;
	virtual void OnConstruction(const FTransform& Transform) override;
	/** BP_TalkingMaster's On Puzzle Solved (BPI_PuzzleSolver) calls this. Passes it to the night director, server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest")
	void HandlePuzzleSolved(AActor* Puzzle, APawn* SolvedBy);
protected:
	virtual void BeginPlay() override;
};
