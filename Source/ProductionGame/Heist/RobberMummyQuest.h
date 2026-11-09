#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberStoryQuest.h"
#include "RobberMummyQuest.generated.h"

class UDialogueDefinition;

UENUM()
enum class EMummyStage : uint8
{
	NotStarted,
	FirstTalk,
	FetchLungs,
	BringLungs,
	FetchStomach,
	BringStomach,
	FetchLiver,
	BringLiver,
	FetchHeart,
	BringHeart,
	FetchSausages,
	BringSausages,
	FetchHead,
	BringHead,
	Done
};

/**
 * Some Assembly Required: Pharaoh Neferkhaos has given up waiting for the priests and is resurrecting himself. His scroll is
 * the shopping list. Every ingredient carried back makes him more coherent.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberMummyQuest : public ARobberStoryQuest
{
	GENERATED_BODY()

public:
	ARobberMummyQuest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual AActor* GetTipSubject() const override { return Mummy; }
	virtual void GetRequiredActors(TArray<AActor*>& OutActors) const override;
	virtual bool HandlesProp(FName PropId) const override;
	virtual FText GetPropPrompt(FName PropId, const APlayerState* Viewer) const override;
	virtual void OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player) override;
	virtual bool WatcherCares(FName Watcher, const APlayerState* Player) const override;
	virtual void OnPlayerCaught(APawn* Pawn, APlayerState* Player, FName Watcher) override;

	/** The guard in the Security Office, on its ARobberTina. */
	static const FName DeskGuard;

	virtual FString GetDevStatus() const override { return TEXT("Some Assembly Required: ") + StaticEnum<EMummyStage>()->GetNameStringByValue(static_cast<int64>(Stage)); }

	EMummyStage GetStage() const { return Stage; }
	int32 GetPayoutsMade() const { return PayoutsMade; }
	AActor* GetMummy() const { return Mummy; }

protected:
	virtual void OnQuestStarted() override;
	virtual FRobberStoryNote BuildNote() const override;

private:
	void BuildDialogue();
	FName EntryFor(UPlayerQuestComponent* Player, AActor* Talker);
	void OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event);
	void SetStage(EMummyStage NewStage);
	void SetCarrying(float SpeedMultiplier);
	void ShowFitted(int32 Slot);
	void PayOut();

	UPROPERTY(Replicated) EMummyStage Stage = EMummyStage::NotStarted;
	UPROPERTY(Replicated) bool bHasBallast = false;
	UPROPERTY(Replicated) bool bCabinetOpen = false;
	UPROPERTY(Replicated) TArray<FName> OpenedCrates;
	UPROPERTY(Replicated) int32 PayoutsMade = 0;

	UPROPERTY(Transient) TObjectPtr<AActor> Mummy;
	UPROPERTY(Transient) TObjectPtr<UDialogueDefinition> Script;
	TMap<int32, int32> LastChatLine;
	FDelegateHandle EventHandle;
};
