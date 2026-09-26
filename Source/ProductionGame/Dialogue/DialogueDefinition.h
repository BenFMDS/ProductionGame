#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DialogueDefinition.generated.h"

class UQuestDefinition;
class USoundBase;
class UTexture2D;

UENUM(BlueprintType)
enum class EDialogueQuestAction : uint8 { None, Accept, TurnIn };

USTRUCT(BlueprintType)
struct FDialogueChoice
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Text;
	/** None closes the conversation. Otherwise this must name an existing node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName NextNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EDialogueQuestAction QuestAction = EDialogueQuestAction::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UQuestDefinition> Quest;
};

USTRUCT(BlueprintType)
struct FDialogueNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ID;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Text;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> Voice;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDialogueChoice> Choices;
};

/** Native replacement for speaker/choice/end dialogue Blueprint tasks. */
UCLASS(BlueprintType)
class PRODUCTIONGAME_API UDialogueDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText SpeakerName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Portrait;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EntryNode;
	/** Optional quest-state greetings; the initial greeting is used until this quest is accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UQuestDefinition> ContextQuest;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ActiveEntryNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ReadyEntryNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CompletedEntryNode;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FDialogueNode> Nodes;
	/** Append available quest offers/turn-ins to every node. Disable for fully authored choices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIncludeQuestChoices = true;
	const FDialogueNode* FindNode(FName ID) const
	{
		return Nodes.FindByPredicate([ID](const FDialogueNode& Node) { return Node.ID == ID; });
	}
};
