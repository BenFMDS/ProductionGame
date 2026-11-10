#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Heist/RobberStoryQuest.h"
#include "RobberSoftwareUpdate.generated.h"

class UBorder;
class UCanvasPanel;
class UEditableTextBox;
class UDialogueDefinition;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UENUM()
enum class EUpdateStage : uint8
{
	NotStarted,
	MeetBot,
	FindDisk,
	ReachServerRoom,
	Captcha,
	Terms,
	Plan,
	Activate,
	Switches,
	Rebooting,
	BackToHall,
	Done
};

/** What the console sends to the server. Sent as a byte so RobberController.h needn't know this header. */
enum class EConsoleAction : uint8
{
	VerifyLifeForms,
	NameCrewmate,
	ContinueFromList,
	TypeLine,
	FinishTerms,
	PickPlan,
	Retry,
	FlipSwitch,
	TurnBackOn
};

/**
 * Software Update: The Cancellator saw a film in 1984, took it for a documentary, and can't get past the login page's robot
 * check. All the rules live here on the server; URobberServerConsole is only the screen. Pays a bonus for passing the
 * captcha and the backup generator's access code at the end.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberSoftwareUpdateQuest : public ARobberStoryQuest
{
	GENERATED_BODY()

public:
	ARobberSoftwareUpdateQuest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual AActor* GetTipSubject() const override { return Bot; }
	virtual void GetRequiredActors(TArray<AActor*>& OutActors) const override;
	virtual bool HandlesProp(FName PropId) const override;
	virtual FText GetPropPrompt(FName PropId, const APlayerState* Viewer) const override;
	virtual void OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player) override;

	virtual FString GetDevStatus() const override { return TEXT("Software Update: ") + StaticEnum<EUpdateStage>()->GetNameStringByValue(static_cast<int64>(Stage)); }

	/** Server only. Everything the console's buttons do. Ignored unless Player owns the quest and the step matches. */
	void HandleConsole(APlayerState* Player, EConsoleAction Action, int32 Value, const FString& Text);

	static constexpr int32 CaptchaFirstTyped = 5;
	static constexpr int32 CaptchaPriority = 4;
	static constexpr int32 ServersSwitch = 7;

	/** Seconds the servers must stay off. */
	UPROPERTY(EditAnywhere, Category = "Software Update") float RebootSeconds = 10.f;
	UPROPERTY(EditAnywhere, Category = "Software Update") float WrongAnswerHeat = 8.f;
	UPROPERTY(EditAnywhere, Category = "Software Update") float BadSwitchHeat = 5.f;

	EUpdateStage GetStage() const { return Stage; }
	int32 GetCaptchaStep() const { return CaptchaStep; }
	int32 GetActivationTries() const { return ActivationTries; }
	int32 GetPayoutsMade() const { return PayoutsMade; }
	int32 GetAccessCode() const { return AccessCode; }
	int32 GetFeesMask() const { return FeesMask; }
	const TArray<FString>& GetCrewNames() const { return CrewNames; }
	const TArray<FString>& GetCrewAnswers() const { return CrewAnswers; }
	AActor* GetBot() const { return Bot; }
	AActor* GetBuddy() const { return Buddy; }

protected:
	virtual void OnQuestStarted() override;
	virtual FRobberStoryNote BuildNote() const override;

private:
	void BuildDialogues();
	FName EntryFor(UPlayerQuestComponent* Player, AActor* Talker);
	void OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event);
	void SetStage(EUpdateStage NewStage);
	void Say(const FText& Message) const;
	void CloseConsole() const;
	void FillCrewNames();
	void TellCrewmate(const FString& Name, const FText& Message) const;
	void UpdateBuddy();
	void CheckRooms();
	UFUNCTION() void OnRep_BuddyFollowing();

	UPROPERTY(Replicated) EUpdateStage Stage = EUpdateStage::NotStarted;
	UPROPERTY(Replicated) int32 CaptchaStep = 0;
	UPROPERTY(Replicated) int32 ActivationTries = 0;
	UPROPERTY(Replicated) int32 PayoutsMade = 0;
	UPROPERTY(Replicated) int32 FeesMask = 0;
	UPROPERTY(Replicated) TArray<FString> CrewNames;
	UPROPERTY(Replicated) TArray<FString> CrewAnswers;
	UPROPERTY(ReplicatedUsing = OnRep_BuddyFollowing) bool bBuddyFollowing = false;

	UPROPERTY(Transient) TObjectPtr<AActor> Bot;
	UPROPERTY(Transient) TObjectPtr<AActor> Buddy;
	UPROPERTY(Transient) TArray<TObjectPtr<UDialogueDefinition>> Scripts;
	int32 AccessCode = 0;
	double ServersOffAt = 0.;
	bool bSaidArchive = false;
	bool bSaidServerRoom = false;
	FDelegateHandle EventHandle;
	FTimerHandle BuddyTimer;
	FTimerHandle RoomTimer;
};

/** A button that runs a lambda. Runs it a tick later, because most of them rebuild the screen the button is on. */
UCLASS()
class PRODUCTIONGAME_API URobberConsoleButton : public UButton
{
	GENERATED_BODY()
public:
	URobberConsoleButton();
	TFunction<void()> Action;
	FString Label;
private:
	UFUNCTION() void Fire();
};

/** The Server Room console, one screen per quest step. Built in code. Local state is kept between openings, so nothing sends the player back. */
UCLASS()
class PRODUCTIONGAME_API URobberServerConsole : public UUserWidget
{
	GENERATED_BODY()

public:
	URobberServerConsole(const FObjectInitializer& Initializer);
	void SetMessage(const FText& Message);
	void CloseConsole();

	/** For tests: the buttons on the screen now, by label. */
	bool PressForTest(const FString& Label);
	FString GetScreenTextForTest() const;
	void ScrollTermsForTest(float Fraction);
	float GetTermsOffsetForTest() const;
	bool IsAgreeEnabledForTest() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	enum class ETermsView : uint8 { Reading, Hidden, Quiz, Agreed, Results };

	ARobberSoftwareUpdateQuest* FindQuest() const;
	void Send(EConsoleAction Action, int32 Value = 0, const FString& Text = FString());
	void Rebuild();

	void BuildCaptcha(const ARobberSoftwareUpdateQuest& Quest);
	void BuildTerms(const ARobberSoftwareUpdateQuest& Quest);
	void BuildQuiz(const ARobberSoftwareUpdateQuest& Quest);
	void BuildPlan();
	void BuildActivate(const ARobberSoftwareUpdateQuest& Quest);
	void BuildSwitches();
	void BuildRebooting();
	void TickTerms();

	UTextBlock* AddText(UVerticalBox* Into, const FString& Text, float Size, const FLinearColor& Colour, bool bBold = false);
	URobberConsoleButton* MakeButton(const FString& Label, TFunction<void()> Action, float Size = 24.f);
	URobberConsoleButton* AddButton(UVerticalBox* Into, const FString& Label, TFunction<void()> Action);
	void AddGrid(const TArray<FString>& Labels, int32 Columns, TFunction<void(int32)> Action, const TSet<int32>* Selected = nullptr);
	void AddQuip(const FString& Line);
	void AddMessageLine();

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Root;
	UPROPERTY(Transient) TObjectPtr<UBorder> Card;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Body;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> TypeBox;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> TermsScroll;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TermsNotes;
	UPROPERTY(Transient) TObjectPtr<URobberConsoleButton> AgreeButton;
	UPROPERTY(Transient) TArray<TObjectPtr<URobberConsoleButton>> Buttons;

	TWeakObjectPtr<ARobberSoftwareUpdateQuest> Quest;
	bool bPreviousCursor = false;
	FString ShownKey;
	FString Message;
	TSet<int32> PickedTiles;

	// Local on purpose: reading the terms is between the player and the screen, and survives closing the console.
	ETermsView TermsView = ETermsView::Reading;
	float TermsOffset = 0.f;
	int32 TrapsTicked = 0b11111;
	TSet<int32> NotesShown;
	bool bGuardWarned = false;
	float HiddenFor = 0.f;
	int32 QuizIndex = 0;
	int32 QuizScore = 0;
	FString QuizNote;
	bool bQuizShowsClause = false;

	bool bPlanSummary = false;
	float ActivationPercent = 0.f;
	float StalledFor = 0.f;
	double RebootStartedAt = 0.;
};
