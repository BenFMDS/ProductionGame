#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogueComponent.generated.h"

class UDialogueDefinition;
class UQuestDefinition;
class UPlayerQuestComponent;
class APlayerState;
class APawn;

/** Server only: an NPC (Talker) started a conversation with Player. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConversationStarted, AActor* /*Talker*/, APlayerState* /*Player*/);
/** Server only: a player picked a choice carrying a StoryEvent. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnDialogueEvent, AActor* /*Talker*/, UPlayerQuestComponent* /*Player*/, FName /*Event*/);
/** Which node to open on for this player, or None for the dialogue's usual choice. */
DECLARE_DELEGATE_RetVal_OneParam(FName, FDialogueEntryOverride, UPlayerQuestComponent* /*Player*/);

/** A line this talker can have with a player who's on someone else's quest (ask about the gossip, hand over the parcel). */
USTRUCT()
struct PRODUCTIONGAME_API FDialogueErrand
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UQuestDefinition> Quest;
	UPROPERTY() FText Choice;
	UPROPERTY() FText Reply;
	/** When set, an actor with this tag has to be here (the carried parcel). It loses the tag, so it only counts once. */
	UPROPERTY() FName RequiredNearbyTag;
};

/** Add to a replicated NPC. Never give NPC ownership to a client. */
UCLASS(ClassGroup=(Dialogue), BlueprintType, meta=(BlueprintSpawnableComponent, DisplayName="Talker Component"))
class PRODUCTIONGAME_API UDialogueComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDialogueComponent();
	/** Any talker, any world; listeners check the world themselves (the journal notes who you've spoken to). */
	static FOnConversationStarted OnConversationStarted;
	static FOnDialogueEvent OnDialogueEvent;
	/** Set by the night director and story quests: the same talker can open differently for each player. */
	FDialogueEntryOverride EntryOverride;
	/** The one dialogue asset this actor speaks. A conversation cannot begin without it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Dialogue Asset")) TObjectPtr<UDialogueDefinition> Dialogue;
	/** Optional quests offered through this conversation. New talkers use this instead of a separate Quest Giver Component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quests", meta=(DisplayName="Offered Quests")) TArray<TObjectPtr<UQuestDefinition>> OfferedQuests;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(ClampMin="1")) float InteractionDistance = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(ClampMin="1")) float IdleTimeout = 120.f;
	/** Server only. False for talkers with nothing to say worth writing down, so their jokes don't bury the clues. */
	UPROPERTY(Transient) bool bNotedInJournal = true;
	/** Server only. What the journal quotes, when the spoken line has a joke in front of the part worth keeping. */
	UPROPERTY(Transient) FText JournalQuote;
	/** Server only, set by the night director. */
	UPROPERTY(Transient) TArray<FDialogueErrand> Errands;
	/** Server only. What this talker can say during Investigate. Must never hold a clue. */
	UPROPERTY(Transient) TArray<FText> SmallTalkLines;
	/** Server only. Replaces the dialogue's speaker name during small talk, for talkers whose names are a secret. */
	UPROPERTY(Transient) FText SmallTalkSpeaker;
	/** Replicated busy indicator; lock ownership itself is private server state. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Dialogue") bool bIsInConversation = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Dialogue") TObjectPtr<APlayerState> TalkingPlayer;
	bool TryStart(UPlayerQuestComponent* Player);
	bool HasValidDialogue() const;
	TArray<UQuestDefinition*> GetOfferedQuests() const;
	bool OffersQuest(const UQuestDefinition* Quest) const;
	void Choose(UPlayerQuestComponent* Player, int32 ChoiceID, int32 Revision);
	void Release(UPlayerQuestComponent* Player);
	bool IsParticipant(const UPlayerQuestComponent* Player) const;
	/** Server only. The line the talker is on right now (the greeting, just after TryStart). */
	FText GetCurrentLine() const;
	bool CanReach(const UPlayerQuestComponent* Player) const;
	void Refresh();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	TWeakObjectPtr<UPlayerQuestComponent> Participant;
	TWeakObjectPtr<APawn> StartingPawn;
	FName CurrentNode;
	TArray<int32> VisibleChoiceIDs;
	double LastActivity = 0.0;
	FTimerHandle SessionTimer;
	/** An errand's reply, said instead of the current node's line until the next choice. */
	FText ErrandLine;
	bool bSmallTalk = false;
	FText SmallTalkLine;
	int32 LastSmallTalkLine = INDEX_NONE;
	FText PickSmallTalkLine();
	/** Where a conversation opens: a random greeting, the quest's step, or what the entry override says. */
	FName PickEntryNode(UPlayerQuestComponent* Player) const;
	AActor* FindErrandItem(const FDialogueErrand& Errand) const;
	bool CanDoErrand(const UPlayerQuestComponent* Player, const FDialogueErrand& Errand) const;
	void ValidateSession();
	void PublishNode();
};
