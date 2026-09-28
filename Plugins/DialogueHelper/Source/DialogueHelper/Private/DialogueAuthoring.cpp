#include "DialogueAuthoring.h"
#include "Dialogue/DialogueDefinition.h"
#include "Dialogue/DialogueComponent.h"
#include "Dialogue/QuestDialogueCharacter.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestGiverComponent.h"
#include "Quest/QuestObjectiveComponent.h"
#include "GameFramework/Actor.h"

namespace DialogueAuthoring
{
UDialogueDefinition* GetDialogue(AActor* Actor)
{
    if (auto* Character = Cast<AQuestDialogueCharacter>(Actor)) return Character->NativeDialogueDefinition;
    if (Actor) if (auto* Component = Actor->FindComponentByClass<UDialogueComponent>()) return Component->Dialogue;
    return nullptr;
}

TArray<UQuestDefinition*> GetQuests(AActor* Actor)
{
    TArray<UQuestDefinition*> Result;
    if (auto* Character = Cast<AQuestDialogueCharacter>(Actor))
    {
        for (UQuestDefinition* Quest : Character->NativeQuestDefinitions) if (Quest) Result.Add(Quest);
    }
    else if (Actor)
    {
        if (auto* Giver = Actor->FindComponentByClass<UQuestGiverComponent>())
            for (UQuestDefinition* Quest : Giver->AvailableQuests) if (Quest) Result.Add(Quest);
    }
    return Result;
}

bool AssignDialogue(AActor* Actor, UDialogueDefinition* Dialogue)
{
    if (!Actor) return false;
    auto* Component = Actor->FindComponentByClass<UDialogueComponent>();
    if (!Component) return false;
    Actor->Modify();
    Component->Modify();
    Component->Dialogue = Dialogue;
    if (auto* Character = Cast<AQuestDialogueCharacter>(Actor)) Character->NativeDialogueDefinition = Dialogue;
    Component->PostEditChange();
    Actor->MarkPackageDirty();
    return true;
}

bool SetQuestOffered(AActor* Actor, UQuestDefinition* Quest, bool bOffered)
{
    if (!Actor || !Quest) return false;
    auto* Giver = Actor->FindComponentByClass<UQuestGiverComponent>();
    if (!Giver) return false;
    Actor->Modify();
    Giver->Modify();
    if (auto* Character = Cast<AQuestDialogueCharacter>(Actor))
    {
        if (bOffered) Character->NativeQuestDefinitions.AddUnique(Quest);
        else Character->NativeQuestDefinitions.Remove(Quest);
        Giver->AvailableQuests = Character->NativeQuestDefinitions;
    }
    else if (bOffered) Giver->AvailableQuests.AddUnique(Quest);
    else Giver->AvailableQuests.Remove(Quest);
    Giver->PostEditChange();
    Actor->MarkPackageDirty();
    return true;
}

bool RenameNode(UDialogueDefinition* Dialogue, FName OldID, FName NewID)
{
    if (!Dialogue || NewID.IsNone() || OldID == NewID || !Dialogue->FindNode(OldID) || Dialogue->FindNode(NewID)) return false;
    Dialogue->Modify();
    for (auto& Node : Dialogue->Nodes)
    {
        if (Node.ID == OldID) Node.ID = NewID;
        for (auto& Choice : Node.Choices) if (Choice.NextNode == OldID) Choice.NextNode = NewID;
    }
    for (FName* Entry : { &Dialogue->EntryNode, &Dialogue->ActiveEntryNode, &Dialogue->ReadyEntryNode, &Dialogue->CompletedEntryNode })
        if (*Entry == OldID) *Entry = NewID;
    Dialogue->PostEditChange();
    Dialogue->MarkPackageDirty();
    return true;
}

TArray<FString> Validate(const UDialogueDefinition* Dialogue, const UQuestDefinition* Quest, AActor* Actor)
{
    TArray<FString> Issues;
    if (Dialogue)
    {
        TSet<FName> IDs;
        for (const auto& Node : Dialogue->Nodes)
        {
            if (Node.ID.IsNone() || IDs.Contains(Node.ID)) Issues.Add(FString::Printf(TEXT("Invalid or duplicate line ID: %s"), *Node.ID.ToString()));
            IDs.Add(Node.ID);
            if (Node.Text.IsEmpty()) Issues.Add(Node.ID.ToString() + TEXT(": empty dialogue text."));
            for (const auto& Choice : Node.Choices)
            {
                if (Choice.Text.IsEmpty()) Issues.Add(Node.ID.ToString() + TEXT(": empty response text."));
                if (!Choice.NextNode.IsNone() && !Dialogue->FindNode(Choice.NextNode)) Issues.Add(Node.ID.ToString() + TEXT(": response links to missing line ") + Choice.NextNode.ToString());
                if (Choice.QuestAction != EDialogueQuestAction::None)
                {
                    if (!Choice.Quest) Issues.Add(Node.ID.ToString() + TEXT(": quest action has no quest."));
                    else if (Actor && !GetQuests(Actor).Contains(Choice.Quest)) Issues.Add(Node.ID.ToString() + TEXT(": action quest is not offered by the selected object."));
                }
            }
        }
        if (!Dialogue->FindNode(Dialogue->EntryNode)) Issues.Add(TEXT("Starting line is missing."));
        TArray<FName> Queue;
        for (FName Entry : {Dialogue->EntryNode, Dialogue->ActiveEntryNode, Dialogue->ReadyEntryNode, Dialogue->CompletedEntryNode})
        {
            if (!Entry.IsNone())
            {
                if (!Dialogue->FindNode(Entry)) Issues.Add(TEXT("Greeting links to missing line: ") + Entry.ToString());
                else Queue.AddUnique(Entry);
            }
        }
        for (int32 Index = 0; Index < Queue.Num(); ++Index)
            if (const auto* Node = Dialogue->FindNode(Queue[Index]))
                for (const auto& Choice : Node->Choices) if (!Choice.NextNode.IsNone()) Queue.AddUnique(Choice.NextNode);
        for (const auto& Node : Dialogue->Nodes) if (!Queue.Contains(Node.ID)) Issues.Add(TEXT("Unreachable line: ") + Node.ID.ToString());
        if (!Dialogue->ContextQuest && (!Dialogue->ActiveEntryNode.IsNone() || !Dialogue->ReadyEntryNode.IsNone() || !Dialogue->CompletedEntryNode.IsNone()))
            Issues.Add(TEXT("Quest-state greetings need a Context Quest."));
    }
    if (Quest)
    {
        if (Quest->QuestID.IsNone()) Issues.Add(TEXT("Quest needs a stable Quest ID."));
        if (Quest->QuestName.IsEmpty()) Issues.Add(TEXT("Quest title is empty."));
        if (Quest->RequiredAmount < 1) Issues.Add(TEXT("Quest required amount must be at least one."));
        if (Quest->ProgressEvent.IsNone()) Issues.Add(TEXT("Quest has no progress event."));
    }
    if (Actor)
    {
        if (!Actor->GetIsReplicated()) Issues.Add(TEXT("Object is not replicated; multiplayer dialogue requires replication."));
        if (!Actor->FindComponentByClass<UDialogueComponent>()) Issues.Add(TEXT("Object has no Dialogue Component. Use Add dialogue support to configure it."));
        if (auto* Objective = Actor->FindComponentByClass<UQuestObjectiveComponent>())
        {
            if (Quest && Objective->ProgressEvent != Quest->ProgressEvent) Issues.Add(TEXT("Pickup progress event differs from the selected quest."));
            if (Objective->Amount < 1) Issues.Add(TEXT("Pickup amount must be positive."));
        }
    }
    return Issues;
}
}

