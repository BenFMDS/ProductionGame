#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberInteractables.h"
#include "Quest/PlayerQuestComponent.h"
#include "RobberStoryQuest.generated.h"

class APlayerState;
class ARobberController;
class ARobberStoryProp;
class UDialogueComponent;
class UDialogueDefinition;
class USceneComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/** One line of a story quest's script (DT_<Quest>Script). A scene is its rows in table order. */
USTRUCT(BlueprintType)
struct FRobberScriptRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The scene the line belongs to, or None for a line used on its own: a toast, a prompt, a line of the note. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Script") FName Scene;
	/** Who says it. "You" is the player: in a scene, that line is the answer picked on the line before it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Script") FName Speaker;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Script", meta = (MultiLine = true)) FText Text;
};

/**
 * A hand-written, multi-step quest. One player owns it: they started it from a tip, and only they get its note and can do its
 * steps. Lives in the level as a single actor, and tells the night director about its tip and any vault digits it pays.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API ARobberStoryQuest : public AActor
{
	GENERATED_BODY()

public:
	ARobberStoryQuest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Every word this quest says. Tip, TipTaken and TipJournal rows fill the tip lines below. */
	UPROPERTY(EditDefaultsOnly, Category = "Story") TObjectPtr<UDataTable> ScriptTable;
	/** The row's text, or the key itself (and an error) when the row is missing. */
	FText Line(FName Key) const;
	/** A scene's lines in table order. */
	TArray<FText> SceneLines(FName Scene) const;

	/** On talkers the story quest runs itself, so the night director leaves them out of the deal. */
	static const FName NpcTag;

	bool IsQuestOwner(const APlayerState* Player) const { return Player && QuestOwner == Player; }

	/** For the dev menu's quest list. */
	virtual FString GetDevStatus() const { return GetClass()->GetName(); }
	FString GetDevOwnerName() const;

	// --- The director's side. ---
	virtual bool WantsVaultDigits() const { return false; }
	virtual void SetVaultDigits(const TArray<int32>& Indexes) {}
	/** Where this quest's two digits start in the code, so a script can say "the first number" and mean it. */
	virtual int32 GetFirstVaultDigit() const { return 0; }
	/** Keys this quest pays out. The director leaves them out of the exhibit quests' rewards. */
	virtual void GetPaidKeys(TArray<FName>& OutKeys) const {}
	/** Whoever the tip points at, for its {room} and {floor}. */
	virtual AActor* GetTipSubject() const { return nullptr; }
	/** Everything a player has to reach, so the deal can check the night can be finished. */
	virtual void GetRequiredActors(TArray<AActor*>& OutActors) const {}
	/** Nobody owns it yet, or this player already does. */
	bool CanTakeTip(const APlayerState* Player) const;
	void TakeTip(APlayerState* Player);

	UPROPERTY(EditAnywhere, Category = "Tip", meta = (MultiLine = true)) FText TipLine;
	UPROPERTY(EditAnywhere, Category = "Tip", meta = (MultiLine = true)) FText TipTakenLine;
	UPROPERTY(EditAnywhere, Category = "Tip", meta = (MultiLine = true)) FText TipJournal;

	// --- The props' side. ---
	/** Several story quests can share a night, so each prop has to be claimed by the one it belongs to. */
	virtual bool HandlesProp(FName PropId) const { return false; }
	virtual FText GetPropPrompt(FName PropId, const APlayerState* Viewer) const { return FText::GetEmpty(); }
	/** What a prop placed with bLiveLabel says right now. Built from replicated state, so every machine agrees. */
	virtual FText GetPropLabel(FName PropId) const { return FText::GetEmpty(); }
	/** Replaces the HUD newspaper's headline for the whole crew while it isn't empty. */
	virtual FText GetHeadlineOverride() const { return FText::GetEmpty(); }
	virtual void OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player) {}
	/** May this watcher (ARobberTina::WatcherId) catch this player right now? Any quest can say no. */
	virtual bool WatcherCares(FName Watcher, const APlayerState* Player) const { return true; }
	/** A watcher has just caught this player. */
	virtual void OnPlayerCaught(APawn* Pawn, APlayerState* Player, FName Watcher) {}

protected:
	virtual void OnQuestStarted() {}
	virtual FRobberStoryNote BuildNote() const { return FRobberStoryNote(); }

	UPlayerQuestComponent* OwnerQuests() const;
	ARobberController* OwnerController() const;
	void RefreshNote();
	void Stamp(bool bComplete, const FText& Title, const FText& Subtitle);
	/** The note stays up behind the QUEST COMPLETE stamp, then drops off the screen. */
	void ClearNoteAfterStamp();
	void Toast(const FText& Message) const;
	/** Draws a heat or guard-time reward from the night's table and tells the crew who it came from. */
	void PayBonusReward(const FText& FromWho);

	ARobberStoryProp* FindProp(FName PropId) const;
	bool IsOwnerInRoom(const FString& RoomName) const;
	/** The vault's digit at this place in the code, or INDEX_NONE when there's no vault tonight. */
	int32 VaultDigit(int32 Index) const;
	/** Gives the owner's journal a vault digit, heard from Talker. */
	void TellOwnerDigit(AActor* Talker, const FText& Speaker, int32 Index, int32 Value) const;

	// --- Scripts, built from ScriptTable. ---
	/** Points ScriptTable at /Game/OurStuff/Core/Dialogue/StoryScripts/<Name>. Constructors only. */
	void UseScript(const TCHAR* Name);
	/** The row "Speaker<Name>" if there is one, else the name itself. */
	FText SpeakerName(FName Speaker) const;
	UDialogueDefinition* NewScript(const FText& Speaker);
	/**
	 * Nodes <Scene>1, <Scene>2... for the scene's spoken lines, joined by the "GoOn" row unless the player answers.
	 * The last answer is the row <Scene>End; it fires LastEvent and goes on to Then.
	 */
	void AddScene(UDialogueDefinition* Script, FName Scene, FName LastEvent, FName Then = NAME_None) const;
	/** One line with one answer, the row Answer, which fires Event and goes on to Next. */
	void AddSingle(UDialogueDefinition* Script, FName Id, FName Speaker, const FText& Text, FName Answer = TEXT("Leave"), FName Next = NAME_None, FName Event = NAME_None) const;
	/** A talker gets the script, these small-talk lines and no quests of its own. */
	UDialogueComponent* GiveScript(AActor* Talker, UDialogueDefinition* Script, FName Entry, const TArray<FText>& SmallTalk) const;
	static FRobberStoryLine NoteLine(const FText& Text, bool bDone = false, bool bWarning = false);

	UPROPERTY(Replicated) TObjectPtr<APlayerState> QuestOwner;
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Generated;

private:
	FTimerHandle ClearNoteTimer;
};

/** Something a story quest's owner presses Interact on. Only the owner sees a prompt, and only on the right step. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberStoryProp : public ARobberInteractable
{
	GENERATED_BODY()

public:
	ARobberStoryProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") FName PropId;
	/** Starts the night out of sight (the flame, before anyone has lit it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") bool bStartsHidden = false;
	/** The quest writes this prop's label (the polls board). Off by default so ordinary props don't look their quest up every tick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story") bool bLiveLabel = false;

	/** Server only. A prop that's gone can't be seen, aimed at or walked into. */
	void SetShown(bool bInShown);
	bool IsShown() const { return bShown; }
	FText GetLiveLabelForTest() const { return GetLabel(nullptr); }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;
	virtual FText GetLabel(const ARobberGameState* GameState) const override;

private:
	ARobberStoryQuest* FindStory() const;
	UFUNCTION() void OnRep_Shown();
	UPROPERTY(ReplicatedUsing = OnRep_Shown) bool bShown = true;
};

/** The T-Rex. Stays on her nest and sweeps her gaze across the room; anyone she sees is caught. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberTina : public AActor
{
	GENERATED_BODY()

public:
	ARobberTina();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tina") TObjectPtr<USceneComponent> Root;
	/** Turns with the gaze. Parts of her that don't move can sit on Root. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tina") TObjectPtr<USceneComponent> Head;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tina") TObjectPtr<UStaticMeshComponent> Body;
	/** Lights what she's looking at, so players can read where it's safe. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tina") TObjectPtr<USpotLightComponent> Gaze;

	/** Degrees either side of the way the actor faces. */
	UPROPERTY(EditAnywhere, Category = "Tina") float SweepAngle = 60.f;
	UPROPERTY(EditAnywhere, Category = "Tina") float SweepSeconds = 3.5f;
	/** How long she holds still at each end, which is the player's window. */
	UPROPERTY(EditAnywhere, Category = "Tina") float PauseSeconds = 1.25f;
	/** Half the width of what she can see. */
	UPROPERTY(EditAnywhere, Category = "Tina") float ViewAngle = 25.f;
	UPROPERTY(EditAnywhere, Category = "Tina") float ViewDistance = 1100.f;
	/** Height her line of sight is checked at, above the actor. Low enough for waist-high cover to count. */
	UPROPERTY(EditAnywhere, Category = "Tina") float EyeHeight = 100.f;
	UPROPERTY(EditAnywhere, Category = "Tina") float BlackoutSeconds = 3.f;
	/** Who this is, for quests that care. None is Tina herself. */
	UPROPERTY(EditAnywhere, Category = "Tina") FName WatcherId;
	UPROPERTY(EditAnywhere, Category = "Tina") float KnockBackSpeed = 1400.f;

	/** Which way she's looking now, in world space. */
	FVector GetGazeDirection() const;

private:
	double SweepTime() const;
	float CurrentYaw() const;
	bool CanSee(const APawn* Pawn) const;
	void LookForPlayers();
	void Catch(APawn* Pawn, APlayerState* Player);

	FTimerHandle LookTimer;
	/** Server only. Nobody gets caught again the moment the screen comes back. */
	TMap<TWeakObjectPtr<APlayerState>, double> SafeUntil;
};

UENUM()
enum class ECaveMenStage : uint8
{
	NotStarted,
	TalkNoBook,
	FindBook,
	TalkWithBook,
	StealEgg,
	BringEgg,
	AskForFire,
	FindLighter,
	LightFire,
	TalkAfterFire,
	Done
};

/** Stone Age Small Talk: Reginald, Montague and Ug, who know two numbers of the vault code and would like an omelette. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberCaveMenQuest : public ARobberStoryQuest
{
	GENERATED_BODY()

public:
	ARobberCaveMenQuest();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool WantsVaultDigits() const override { return true; }
	virtual void SetVaultDigits(const TArray<int32>& Indexes) override { DigitIndexes = Indexes; }
	virtual AActor* GetTipSubject() const override;
	virtual void GetRequiredActors(TArray<AActor*>& OutActors) const override;
	virtual bool HandlesProp(FName PropId) const override;
	virtual FText GetPropPrompt(FName PropId, const APlayerState* Viewer) const override;
	virtual void OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player) override;
	virtual void OnPlayerCaught(APawn* Pawn, APlayerState* Player, FName Watcher) override;

	virtual FString GetDevStatus() const override { return TEXT("Stone Age Small Talk: ") + StaticEnum<ECaveMenStage>()->GetNameStringByValue(static_cast<int64>(Stage)); }

	ECaveMenStage GetStage() const { return Stage; }
	bool IsBookFound() const { return bBookFound; }
	const TArray<int32>& GetDigitIndexes() const { return DigitIndexes; }
	const TArray<TObjectPtr<AActor>>& GetCaveMen() const { return CaveMen; }
	FName GetLighterLocker() const { return LighterLocker; }

protected:
	virtual void OnQuestStarted() override;
	virtual FRobberStoryNote BuildNote() const override;

private:
	void BuildDialogues();
	void UpdateDigitLines();
	FName EntryFor(UPlayerQuestComponent* Player, AActor* Talker);
	void OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event);
	void CheckRooms();
	void PayDigit(int32 Which, AActor* Talker);
	void SetStage(ECaveMenStage NewStage);
	int32 DigitValue(int32 Which) const;

	UPROPERTY(Replicated) ECaveMenStage Stage = ECaveMenStage::NotStarted;
	UPROPERTY(Replicated) bool bGiftShopChecked = false;
	UPROPERTY(Replicated) bool bReachedArchive = false;
	UPROPERTY(Replicated) bool bReachedStaffArea = false;
	UPROPERTY(Replicated) bool bBrokeInLocker = false;
	UPROPERTY(Replicated) TArray<FName> OpenedLockers;

	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> CaveMen;
	TArray<int32> DigitIndexes;
	int32 DigitsPaid = 0;
	UPROPERTY(ReplicatedUsing = OnRep_BookFound) bool bBookFound = false;
	UFUNCTION() void OnRep_BookFound();
	/** Their names stay ??? until someone can understand them. */
	void UpdateTalkPrompts();
	FName NameOf(const AActor* Man) const;
	FName LighterLocker;
	TMap<TWeakObjectPtr<AActor>, int32> LastRandomLine;
	FDelegateHandle EventHandle;
	FTimerHandle RoomTimer;
};
