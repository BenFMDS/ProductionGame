#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuestSequenceStep.generated.h"

class AQuestSequencePuzzle;
class UPlayerQuestComponent;
class UStaticMeshComponent;

/** A visible, non-destructive input for an AQuestSequencePuzzle. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API AQuestSequenceStep : public AActor
{
	GENERATED_BODY()
public:
	AQuestSequenceStep();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<AQuestSequencePuzzle> Puzzle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puzzle") FName StepID;

	bool Activate(UPlayerQuestComponent* Player);
};
