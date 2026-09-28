#pragma once

#include "CoreMinimal.h"

class UDialogueDefinition;
class UQuestDefinition;
class AActor;

/** Shared authoring rules. These never change live quest progress. */
namespace DialogueAuthoring
{
    UDialogueDefinition* GetDialogue(AActor* Actor);
    TArray<UQuestDefinition*> GetQuests(AActor* Actor);
    bool AssignDialogue(AActor* Actor, UDialogueDefinition* Dialogue);
    bool SetQuestOffered(AActor* Actor, UQuestDefinition* Quest, bool bOffered);
    bool RenameNode(UDialogueDefinition* Dialogue, FName OldID, FName NewID);
    TArray<FString> Validate(const UDialogueDefinition* Dialogue, const UQuestDefinition* Quest, AActor* Actor);
}
