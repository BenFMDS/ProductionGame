#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuestSequencePuzzle.generated.h"

class UPlayerQuestComponent;
class AQuestSequenceStep;
class USceneComponent;

/**
 * Server-authoritative ordered interaction puzzle. A step can only progress
 * the matching active quest, and each player keeps an independent sequence.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API AQuestSequencePuzzle : public AActor
{
	GENERATED_BODY()
public:
	AQuestSequencePuzzle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Puzzle") TObjectPtr<USceneComponent> Root;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puzzle") TArray<FName> RequiredSequence;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puzzle") FName ProgressEvent;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puzzle", meta=(ClampMin="1")) float InteractionDistance = 350.f;

	/** Called by the click RPC after it resolves the native puzzle step. */
	bool TryActivate(UPlayerQuestComponent* Player, AQuestSequenceStep* Step);

private:
	/** Server-only partial answers; each owning player has their own progress. */
	TMap<TWeakObjectPtr<UPlayerQuestComponent>, int32> PerPlayerProgress;
	bool CanUseStep(const UPlayerQuestComponent* Player, const AQuestSequenceStep* Step) const;
};
