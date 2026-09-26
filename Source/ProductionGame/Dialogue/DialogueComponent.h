#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogueComponent.generated.h"

class UDialogueDefinition;
class UPlayerQuestComponent;
class APlayerState;
class APawn;

/** Add to a replicated NPC. Never give NPC ownership to a client. */
UCLASS(ClassGroup=(Dialogue), BlueprintType, meta=(BlueprintSpawnableComponent))
class PRODUCTIONGAME_API UDialogueComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UDialogueComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") TObjectPtr<UDialogueDefinition> Dialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(ClampMin="1")) float InteractionDistance = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue", meta=(ClampMin="1")) float IdleTimeout = 120.f;
	/** Replicated busy indicator; lock ownership itself is private server state. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Dialogue") bool bIsInConversation = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Dialogue") TObjectPtr<APlayerState> TalkingPlayer;
	bool TryStart(UPlayerQuestComponent* Player);
	void Choose(UPlayerQuestComponent* Player, int32 ChoiceID, int32 Revision);
	void Release(UPlayerQuestComponent* Player);
	bool IsParticipant(const UPlayerQuestComponent* Player) const;
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
