#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerQuestComponent.generated.h"

class UQuestDefinition;
class UDialogueComponent;
class UQuestConversationWidget;
class USoundBase;
class UTexture2D;
class AQuestSequenceStep;

UENUM(BlueprintType)
enum class EPlayerQuestState : uint8 { Active, ReadyToTurnIn, Completed };

USTRUCT(BlueprintType)
struct FPlayerQuest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UQuestDefinition> Definition;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Giver;
	UPROPERTY(BlueprintReadOnly) int32 Amount = 0;
	UPROPERTY(BlueprintReadOnly) EPlayerQuestState State = EPlayerQuestState::Active;
};

/** One atomic replicated snapshot prevents choices from targeting a different dialogue node. */
USTRUCT(BlueprintType)
struct FPlayerConversation
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> NPC;
	UPROPERTY(BlueprintReadOnly) int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly) FText Speaker;
	UPROPERTY(BlueprintReadOnly) FText Text;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture2D> Portrait;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<USoundBase> Voice;
	UPROPERTY(BlueprintReadOnly) TArray<FText> Choices;
	UPROPERTY(BlueprintReadOnly) TArray<int32> ChoiceIDs;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayerQuestChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQuestRewardGranted, UQuestDefinition*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConversationRejected, FText, Reason);

/** Lives on the owning player controller. All mutations and NPC arbitration run on the server. */
UCLASS(ClassGroup=(Quest), BlueprintType, meta=(BlueprintSpawnableComponent))
class PRODUCTIONGAME_API UPlayerQuestComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(ReplicatedUsing=OnRep_Quests, BlueprintReadOnly, Category="Quest") TArray<FPlayerQuest> Quests;
	UPROPERTY(ReplicatedUsing=OnRep_Conversation, BlueprintReadOnly, Category="Dialogue") FPlayerConversation Conversation;
	/** Keys from turned-in quests (UQuestDefinition::RewardKey). Cleared with the quests when a night ends. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Quest") TArray<FName> Keys;
	UPROPERTY(BlueprintAssignable) FPlayerQuestChanged OnQuestsChanged;
	UPROPERTY(BlueprintAssignable) FPlayerQuestChanged OnConversationChanged;
	/** Server only, once per turn-in. Connect future item rewards (keys, equipment, etc.) here. */
	UPROPERTY(BlueprintAssignable) FQuestRewardGranted OnRewardGranted;
	UPROPERTY(BlueprintAssignable) FConversationRejected OnConversationRejected;
	/** Controls whether this controller currently owns a quest/dialogue HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Quest|UI") bool bCreateDefaultUI = true;
	UPROPERTY(EditAnywhere, Category="Quest|UI") TSubclassOf<UQuestConversationWidget> WidgetClass;

	/** Creates/removes the quest UI immediately. Safe to call from lobby or match flow Blueprints. */
	UFUNCTION(BlueprintCallable, Category="Quest|UI")
	void SetQuestUIEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Quest|UI")
	bool IsQuestUIEnabled() const { return bCreateDefaultUI; }

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Dialogue") void ServerTalkTo(AActor* NPC);
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Dialogue") void ServerChoose(int32 ChoiceID, int32 Revision);
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Dialogue") void ServerEndConversation(int32 Revision);
	/** Interaction requests carry only the object. The server determines event, amount, and eligibility. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Quest") void ServerCollectQuestObject(AActor* QuestObject);
	/** Puzzle interactions carry only the clicked step. The server validates the puzzle state and answer. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="Quest") void ServerActivateQuestPuzzle(AActor* PuzzleStep);
	/** Call only from validated server gameplay; deliberately not a client RPC. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") bool AddQuestProgress(FName QuestID, int32 Amount = 1);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") void ReportQuestEvent(FName Event, int32 Amount = 1);
	UFUNCTION(BlueprintPure, Category="Quest") bool GetQuest(FName QuestID, FPlayerQuest& OutQuest) const;
	UFUNCTION(BlueprintPure, Category="Quest") bool HasKey(FName KeyId) const { return !KeyId.IsNone() && Keys.Contains(KeyId); }
	/** Server only. For keys found in the level rather than earned from a quest (e.g. the sewer crowbar). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") void GiveKey(FName KeyId);
	/** Server only. Each night's quest givers are fresh copies, so quests, keys and any open conversation go. */
	void ResetForNight();
	UFUNCTION(BlueprintCallable, Category="Dialogue") void TalkToLookedAtNPC();

	bool CanAccept(UQuestDefinition* Definition, AActor* Giver) const;
	bool CanTurnIn(UQuestDefinition* Definition, AActor* Giver) const;
	bool CanProgressQuest(UQuestDefinition* Definition, AActor* Giver) const;
	bool AcceptQuest(UQuestDefinition* Definition, AActor* Giver);
	bool TurnInQuest(UQuestDefinition* Definition, AActor* Giver);
	bool ProgressQuest(UQuestDefinition* Definition, AActor* Giver, int32 Amount = 1);
	void SetConversation(const FPlayerConversation& Snapshot);
	void CloseConversation();
	UFUNCTION(Client, Reliable) void ClientRejectConversation(const FText& Reason);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	void RefreshDefaultUI();
	UPROPERTY(Transient) TObjectPtr<UQuestConversationWidget> Widget;
	UFUNCTION() void OnRep_Quests();
	UFUNCTION() void OnRep_Conversation();
	double NextTalkTime = 0.0;
};
