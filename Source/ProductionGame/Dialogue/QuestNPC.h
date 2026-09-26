#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QuestNPC.generated.h"

class UDialogueComponent;
class UQuestGiverComponent;
class UStaticMeshComponent;

/** Placeable native NPC; use a Blueprint child for your character mesh and animations. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API AQuestNPC : public AActor
{
	GENERATED_BODY()
public:
	AQuestNPC();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UDialogueComponent> DialogueComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UQuestGiverComponent> QuestGiver;
};
