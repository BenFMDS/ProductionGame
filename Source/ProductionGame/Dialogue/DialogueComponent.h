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

/** Add to a replicated NPC. Never give NPC ownership to a client. */
UCLASS(ClassGroup=(Dialogue), BlueprintType, meta=(BlueprintSpawnableComponent, DisplayName="Talker Component"))
class PRODUCTIONGAME_API UDialogueComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDialogueComponent();
	/** Any talker, any world; listeners check the world themselves (the vault door's code hints use it). */
	static FOnConversationStarted OnConversationStarted;
	/** The one dialogue asset this actor speaks. A conversation cannot begin without it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(DisplayName="Dialogue Asset")) TObjectPtr<UDialogueDefinition> Dialogue;
	/** Optional quests offered through this conversation. New talkers use this instead of a separate Quest Giver Component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quests", meta=(DisplayName="Offered Quests")) TArray<TObjectPtr<UQuestDefinition>> OfferedQuests;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(ClampMin="1")) float InteractionDistance = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(ClampMin="1")) float IdleTimeout = 120.f;
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
	void ValidateSession();
	void PublishNode();
};
