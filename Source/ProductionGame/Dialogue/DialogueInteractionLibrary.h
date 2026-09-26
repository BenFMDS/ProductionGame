#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DialogueInteractionLibrary.generated.h"

/** Bridge from the project's existing BPI_Interaction interface to the owned server RPC. */
UCLASS()
class PRODUCTIONGAME_API UDialogueInteractionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	static void TalkToNPC(AActor* NPC, AActor* InteractingPlayer);
	UFUNCTION(BlueprintCallable, Category="Quest")
	static void CollectQuestObject(AActor* QuestObject, AActor* InteractingPlayer);
};
