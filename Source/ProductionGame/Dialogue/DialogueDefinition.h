#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DialogueDefinition.generated.h"

class UQuestDefinition;
class USoundBase;
class UTexture2D;

/** Controls whether this dialogue can show or perform quest interactions. */
UENUM(BlueprintType)
enum class EDialogueType : uint8
{
	DialogueOnly UMETA(DisplayName="Dialogue only"),
	QuestDialogue UMETA(DisplayName="Dialogue + quests")
};

UENUM(BlueprintType)
enum class EDialogueQuestAction : uint8
{
	None = 0,
	Accept = 1,
	TurnIn = 2,
	/** Marks one active quest step complete from a validated dialogue answer. */
	Progress = 3
};

USTRUCT(BlueprintType)
struct FDialogueChoice
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Text;
	/** None closes the conversation. Otherwise this must name an existing node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDialogueQuestAction QuestAction = EDialogueQuestAction::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UQuestDefinition> Quest;
	/** Amount awarded by a correct dialogue answer. Ignored by non-progress actions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 ProgressAmount = 1;
	/** Told to story quests when this is picked, so a conversation can move a quest on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StoryEvent;
};

USTRUCT(BlueprintType)
struct FDialogueNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Text;
	/** Who says this line, when it isn't the dialogue's own speaker (a group talking together). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Speaker;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> Voice;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDialogueChoice> Choices;
};

/** Native replacement for speaker/choice/end dialogue Blueprint tasks. */
UCLASS(BlueprintType)
class PRODUCTIONGAME_API UDialogueDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	/** Dialogue only is a normal conversation. Dialogue + quests supports quest-state greetings and quest choices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") EDialogueType DialogueType = EDialogueType::QuestDialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText SpeakerName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Portrait;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EntryNode;
	/** Optional pool of normal greetings. When populated, one valid node is chosen at random each time this dialogue starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") TArray<FName> RandomEntryNodes;
	/** Optional quest-state greetings; the initial greeting is used until this quest is accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UQuestDefinition> ContextQuest;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ActiveEntryNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ReadyEntryNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CompletedEntryNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDialogueNode> Nodes;
	/** Append available quest offers/turn-ins to every node. Disable for fully authored choices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIncludeQuestChoices = true;
	bool UsesQuests() const { return DialogueType == EDialogueType::QuestDialogue; }
	const FDialogueNode* FindNode(FName ID) const
	{
		return Nodes.FindByPredicate([ID](const FDialogueNode& Node) { return Node.ID == ID; });
	}
	/** What the talker can open with: the entry node's line and the random entries'. */
	TArray<FText> GetGreetingLines() const
	{
		TArray<FText> Lines;
		for (const FDialogueNode& Node : Nodes)
		{
			if (!Node.Text.IsEmpty() && (RandomEntryNodes.Contains(Node.ID) || EntryNode == Node.ID)) Lines.Add(Node.Text);
		}
		return Lines;
	}
};
