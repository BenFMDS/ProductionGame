#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberStoryQuest.h"
#include "RobberBurglaryQuest.generated.h"

class UDialogueDefinition;

UENUM()
enum class EBurglaryStage : uint8
{
	NotStarted,
	MeetNixon,
	GetTape,
	TapeLatch,
	PlantBug,
	RemoveTape,
	ReportBack,
	MeetSource,
	FindReel,
	Decide,
	Erasing,
	AttendCount,
	Done
};

UENUM()
enum class EBurglaryEnding : uint8
{
	None,
	Erased,
	Leaked
};

/**
 * A Third-Rate Burglary: the Hall of Presidents votes for the Best President Ever, Nixon is on zero, and he has seen the
 * PLUMBING CO. van. Pays a bonus when the hat is bugged and the Generator Room key at the count.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberBurglaryQuest : public ARobberStoryQuest
{
	GENERATED_BODY()

public:
	ARobberBurglaryQuest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetPaidKeys(TArray<FName>& OutKeys) const override;
	virtual AActor* GetTipSubject() const override { return Talkers.FindRef(TEXT("Nixon")); }
	virtual void GetRequiredActors(TArray<AActor*>& OutActors) const override;
	virtual bool HandlesProp(FName PropId) const override;
	virtual FText GetPropPrompt(FName PropId, const APlayerState* Viewer) const override;
	virtual FText GetPropLabel(FName PropId) const override;
	virtual FText GetHeadlineOverride() const override;
	virtual void OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player) override;

	virtual FString GetDevStatus() const override { return TEXT("A Third-Rate Burglary: ") + StaticEnum<EBurglaryStage>()->GetNameStringByValue(static_cast<int64>(Stage)); }

	/** How long the tape can stay on the latch before a guard's round finds it. */
	UPROPERTY(EditAnywhere, Category = "Burglary") float TapeSeconds = 45.f;
	UPROPERTY(EditAnywhere, Category = "Burglary") float EraseSeconds = 18.5f;
	/** How close the player has to stay to the recorder while it erases. */
	UPROPERTY(EditAnywhere, Category = "Burglary") float RecorderReach = 300.f;
	/** The Source won't talk with another player this close. */
	UPROPERTY(EditAnywhere, Category = "Burglary") float AloneRadius = 900.f;
	UPROPERTY(EditAnywhere, Category = "Burglary") float TapeFoundHeat = 10.f;

	EBurglaryStage GetStage() const { return Stage; }
	EBurglaryEnding GetEnding() const { return Ending; }
	int32 GetPayoutsMade() const { return PayoutsMade; }
	AActor* GetTalker(FName Name) const { return Talkers.FindRef(Name); }
	float GetErasedSeconds() const { return ErasedSeconds; }

protected:
	virtual void OnQuestStarted() override;
	virtual FRobberStoryNote BuildNote() const override;

private:
	void BuildDialogues();
	FName EntryFor(UPlayerQuestComponent* Player, AActor* Talker);
	void OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event);
	void SetStage(EBurglaryStage NewStage);
	void OnTapeFound();
	void TickErase();
	bool IsOwnerAlone(const AActor* Near) const;
	void GetPolls(TArray<int32>& OutPolls, bool& bFinal) const;

	UPROPERTY(Replicated) EBurglaryStage Stage = EBurglaryStage::NotStarted;
	UPROPERTY(Replicated) EBurglaryEnding Ending = EBurglaryEnding::None;
	UPROPERTY(Replicated) int32 PayoutsMade = 0;

	UPROPERTY(Transient) TMap<FName, TObjectPtr<AActor>> Talkers;
	float ErasedSeconds = 0.f;
	TMap<FName, int32> LastChatLine;
	FDelegateHandle EventHandle;
	FTimerHandle TapeTimer;
	FTimerHandle EraseTimer;
};
