#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Heist/RobberInteractables.h"
#include "RobberExhibitTalk.generated.h"

class UDialogueComponent;
class UDialogueDefinition;
class UPlayerQuestComponent;
class UQuestDefinition;
class APlayerState;

UENUM(BlueprintType)
enum class ERobberExhibitRole : uint8 { Silly, Helpful, Quest };

// New values go on the end: the Data Tables store these by name, but saved Blueprints store the byte.
UENUM(BlueprintType)
enum class ERobberQuestKind : uint8 { Riddle, Fetch, Gossip, Deliver, Tamper, Flattery, Puzzle };

UENUM(BlueprintType)
enum class ERobberClueKind : uint8 { KeyHolder, ObjectLocation, DigitHolder, ToolUse, PuzzleHint };

UENUM(BlueprintType)
enum class ERobberRewardKind : uint8 { None, VaultDigits, Key, Tool, GuardDistraction, ExtraTime, TipOff, PuzzleHint };

/** Which kind of museum puzzle a Puzzle component sits on, so quest lines can talk about wires or plates. */
UENUM(BlueprintType)
enum class ERobberPuzzleType : uint8 { Any, Wire, Combination, CodeLock, PressurePlate, Scale };

class URobberPuzzleComponent;
class ARobberPuzzleChest;
class ARobberStoryQuest;

/** What an exhibit pays for its quest tonight. It belongs to the exhibit for the night, so any quest can pay any reward. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberReward
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") ERobberRewardKind Kind = ERobberRewardKind::None;
	/** Heat removed, or seconds added to the guards' arrival. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") float Value = 0.f;
	/** Key or Tool: the id a locked door asks for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") FName KeyId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") FText KeyName;
	/** VaultDigits: which positions of the code. */
	UPROPERTY(BlueprintReadOnly, Category = "Reward") TArray<int32> DigitIndexes;
	/** PuzzleHint: the puzzle whose answer this gives away. */
	UPROPERTY(BlueprintReadOnly, Category = "Reward") TObjectPtr<URobberPuzzleComponent> HintPuzzle;
};

// --- Data Table rows. Slots in lines: {giver} {exhibit} {room} {floor} {object} {item} {key} {digits} {clue}. ---

/** DT_ExhibitJokes. The row name is the exhibit's id: its Blueprint name without BP_NPC_ / BP_CQ_ (MonaLisa, SirSteel). */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberJokeRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jokes", meta = (MultiLine = true)) TArray<FText> Lines;
};

/** DT_HelpfulLines. One way of saying a true fact. Line is spoken; JournalLine goes in the hearer's journal. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberClueLineRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue") ERobberClueKind Kind = ERobberClueKind::ObjectLocation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue", meta = (MultiLine = true)) FText Line;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue", meta = (MultiLine = true)) FText JournalLine;
};

/** DT_RandomQuests. One quest template; the night director fills in who, what and where. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberQuestRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") ERobberQuestKind Kind = ERobberQuestKind::Fetch;
	/** Puzzle quests: which kind of puzzle the lines are written for. Any fits every puzzle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") ERobberPuzzleType PuzzleType = ERobberPuzzleType::Any;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText QuestName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText Objective;
	/** The thing to find, carry or flip. Unused by Riddle, Flattery and Gossip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText ItemName;
	/** The framing line, written to work for any exhibit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true)) FText Offer;
	/** Optional replacements for Offer, by exhibit id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true)) TMap<FName, FText> ExhibitOffers;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText AcceptChoice;
	/** Said when you come back before it's done. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true)) FText Active;
	/** Said when you come back with it done. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true)) FText Ready;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText TurnInChoice;
	/** Gossip, Deliver: the player's option at the other exhibit, and what they say back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText ErrandChoice;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = true)) FText ErrandReply;
	/** Fetch, Tamper: shown to the player when they interact with the item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest") FText PropPrompt;
};

/** DT_QuestPuzzles. A riddle or a choice of compliment for Riddle and Flattery quests. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberPuzzleRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") ERobberQuestKind Kind = ERobberQuestKind::Riddle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle", meta = (MultiLine = true)) FText Question;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FText Right;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FText Wrong1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle") FText Wrong2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle", meta = (MultiLine = true)) FText WrongReply;
};

/** DT_Rewards. Weight above 0 means quests can draw it as a bonus; required kinds keep 0 and are only placed by the director. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberRewardRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") ERobberRewardKind Kind = ERobberRewardKind::GuardDistraction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") float Value = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") float Weight = 0.f;
	/** The exhibit says this as it pays up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward", meta = (MultiLine = true)) FText Line;
	/** Crew-wide rewards: shown to everyone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reward") FText CrewMessage;
};

/** DT_TouringExhibits, empty for now: a touring exhibit is a row here plus its Blueprint, once they're built. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberTouringExhibitRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Touring") TSoftClassPtr<AActor> ExhibitClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Touring") FText QuestName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Touring") FText ItemName;
};

// --- Level markers ---

/** A room of the night, for clues ({room}, {floor}) and the solvability check. Rooms that aren't rectangles get one per part. */
UCLASS(NotPlaceable)
class PRODUCTIONGAME_API ARobberRoomInfo : public AActor
{
	GENERATED_BODY()
public:
	ARobberRoomInfo();
	UPROPERTY(EditAnywhere, Category = "Room") FText RoomName;
	UPROPERTY(EditAnywhere, Category = "Room") FText FloorName;
	/** Half size of the room's box round the actor. */
	UPROPERTY(EditAnywhere, Category = "Room") FVector Extent = FVector(100.);
	/** Keys a crew member needs to get in here. */
	UPROPERTY(EditAnywhere, Category = "Room") TArray<FName> RequiredKeys;
	bool Contains(const FVector& Location) const;
};

/** Somewhere a quest's lost item or tamper prop may be put tonight. */
UCLASS()
class PRODUCTIONGAME_API ARobberQuestSpot : public AActor
{
	GENERATED_BODY()
public:
	ARobberQuestSpot();
};

/** The lost item or thing to flip for one quest, put at a quest spot by the night director. Only the quest's holders can use it. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberQuestProp : public ARobberInteractable
{
	GENERATED_BODY()
public:
	ARobberQuestProp();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, before the actor finishes spawning. */
	void Setup(FName InProgressEvent, const FText& InPrompt, const FText& InDoneMessage, bool bInFlips);
	bool IsFlipped() const { return bFlipped; }

protected:
	virtual void OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState) override;
	virtual FText GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const override;

private:
	UFUNCTION() void OnRep_Flipped();

	UPROPERTY(Replicated) FName ProgressEvent;
	UPROPERTY(Replicated) FText PropPrompt;
	UPROPERTY(ReplicatedUsing = OnRep_Flipped) bool bFlipped = false;
	bool bFlips = false;
	FText DoneMessage;
};

// --- The deal ---

/** One exhibit's job tonight. Server only. */
USTRUCT(BlueprintType)
struct PRODUCTIONGAME_API FRobberExhibitAssignment
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Exhibit;
	UPROPERTY() ERobberExhibitRole Role = ERobberExhibitRole::Silly;
	UPROPERTY() FText Speaker;
	UPROPERTY() TObjectPtr<UQuestDefinition> Quest;
	UPROPERTY() ERobberQuestKind QuestKind = ERobberQuestKind::Fetch;
	UPROPERTY() FName QuestRow;
	UPROPERTY() FName PuzzleRow;
	UPROPERTY() FRobberReward Reward;
	/** Other exhibit the quest sends you to (Gossip, Deliver). */
	UPROPERTY() TObjectPtr<AActor> Target;
	/** Fetch, Tamper: where its prop goes. */
	UPROPERTY() TObjectPtr<ARobberQuestSpot> Spot;
	UPROPERTY() TObjectPtr<ARobberQuestProp> Prop;
	/** Puzzle quests: the puzzle to solve. */
	UPROPERTY() TObjectPtr<URobberPuzzleComponent> Puzzle;
	UPROPERTY() bool bPuzzleSolved = false;
	/** Helpful: the journal line, written when they say it. TipOff: the free clue. PuzzleHint: the hint. */
	UPROPERTY() FText JournalClue;
	/** Crew-wide rewards pay out once per exhibit, however many players finish the quest. */
	UPROPERTY() bool bCrewRewardPaid = false;
	/** Helpful: the story quest this exhibit's clue starts. */
	UPROPERTY() TObjectPtr<ARobberStoryQuest> StoryTip;
};

/** What's in one puzzle chest tonight. Server only. */
USTRUCT()
struct PRODUCTIONGAME_API FRobberChestDeal
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<ARobberPuzzleChest> Chest;
	/** The puzzle that opens it (has the chest in its Solve Targets). */
	UPROPERTY() TObjectPtr<URobberPuzzleComponent> Opener;
	UPROPERTY() FRobberReward Reward;
	/** TipOff / PuzzleHint: the note's text. */
	UPROPERTY() FText JournalClue;
};

/**
 * Deals out the night's exhibit jobs when its level loads: shuffles the exhibits into quests, helpful and silly, places the
 * required rewards first, re-deals until the night can be finished, then writes clues from the final deal so none can be wrong.
 * Everything stays on the server; clients only ever receive the line being spoken. A night without a director keeps its
 * exhibits' own dialogue assets. A lost night reloads its level, so it gets a fresh director and a fresh deal.
 */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API ARobberNightDirector : public AActor
{
	GENERATED_BODY()
public:
	ARobberNightDirector();

	UPROPERTY(EditAnywhere, Category = "Deal", meta = (ClampMin = "0")) int32 QuestCount = 8;
	UPROPERTY(EditAnywhere, Category = "Deal", meta = (ClampMin = "0")) int32 HelpfulCount = 5;
	/** The vault code is split evenly between this many quests. */
	UPROPERTY(EditAnywhere, Category = "Deal", meta = (ClampMin = "1")) int32 DigitQuests = 2;
	/** Keys only a quest can give. Any key pickup in the level with one of these ids is removed. */
	UPROPERTY(EditAnywhere, Category = "Deal") TArray<FName> RequiredQuestKeys;
	/** Tools a quest gives tonight, by the id their door asks for. They open optional shortcuts, so a night can be won without them. */
	UPROPERTY(EditAnywhere, Category = "Deal") TArray<FName> QuestTools;
	/** Most quests of one kind in a night, so a night isn't all riddles. */
	UPROPERTY(EditAnywhere, Category = "Deal", meta = (ClampMin = "1")) int32 MaxQuestsPerKind = 2;

	UPROPERTY(EditAnywhere, Category = "Tables") TObjectPtr<UDataTable> Jokes;
	UPROPERTY(EditAnywhere, Category = "Tables") TObjectPtr<UDataTable> HelpfulLines;
	UPROPERTY(EditAnywhere, Category = "Tables") TObjectPtr<UDataTable> RandomQuests;
	UPROPERTY(EditAnywhere, Category = "Tables") TObjectPtr<UDataTable> Puzzles;
	UPROPERTY(EditAnywhere, Category = "Tables") TObjectPtr<UDataTable> Rewards;
	UPROPERTY(EditAnywhere, Category = "Tables") TObjectPtr<UDataTable> TouringExhibits;

	UPROPERTY(EditAnywhere, Category = "Classes") TSubclassOf<ARobberQuestProp> PropClass;
	/** Something the physics carry can pick up, for Deliver quests. A new one appears by the giver for each player who accepts. */
	UPROPERTY(EditAnywhere, Category = "Classes") TSubclassOf<AActor> ParcelClass;

	UPROPERTY(EditAnywhere, Category = "Lines") FText TellAnotherChoice = NSLOCTEXT("ExhibitTalk", "TellAnother", "Go on, then.");
	UPROPERTY(EditAnywhere, Category = "Lines") FText GoodbyeChoice = NSLOCTEXT("ExhibitTalk", "Goodbye", "I'll leave you to it.");
	UPROPERTY(EditAnywhere, Category = "Lines") FText NotNowChoice = NSLOCTEXT("ExhibitTalk", "NotNow", "Not right now.");
	UPROPERTY(EditAnywhere, Category = "Lines") FText OnItChoice = NSLOCTEXT("ExhibitTalk", "OnIt", "I'm on it.");
	UPROPERTY(EditAnywhere, Category = "Lines") FText RetryChoice = NSLOCTEXT("ExhibitTalk", "Retry", "Let me try again.");
	UPROPERTY(EditAnywhere, Category = "Lines") FText ThanksChoice = NSLOCTEXT("ExhibitTalk", "Thanks", "Thanks for that.");

	bool IsDealt() const { return bDealt; }
	int32 GetDealAttempts() const { return DealAttempts; }
	const TArray<FRobberExhibitAssignment>& GetAssignments() const { return Assignments; }
	const TArray<FRobberChestDeal>& GetChestDeals() const { return ChestDeals; }
	const FRobberExhibitAssignment* FindAssignment(const AActor* Exhibit) const;
	/** Could a crew finish this deal: every required reward reachable, with no key needing itself? */
	bool IsSolvable(const TArray<FRobberExhibitAssignment>& Deal) const;
	/** The exhibit's id in DT_ExhibitJokes. */
	static FName ExhibitId(const AActor* Exhibit);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FClueFact;

	void TryDeal();
	bool DealOnce(const TArray<AActor*>& Exhibits, const TArray<ARobberQuestSpot*>& Spots);
	TArray<FName> KeysNeededAt(const AActor* Actor) const;
	TArray<FName> KeysNeededAt(const URobberPuzzleComponent* Puzzle) const;
	/** "the fuse box wires in the Server Room: red, then blue." */
	FText PuzzleHintLine(const URobberPuzzleComponent* Puzzle) const;
	void FillChests();
	void OnExhibitPuzzleSolved(AActor* Exhibit, AActor* Puzzle, APawn* SolvedBy);
public:
	const ARobberRoomInfo* FindRoom(const FVector& Location) const;
private:
	void CollectFacts(TArray<FClueFact>& Required, TArray<FClueFact>& Others) const;
	FText TellFact(const FClueFact& Fact, FText& OutJournal) const;
	void BuildSilly(FRobberExhibitAssignment& Each, UDialogueComponent* Talker, const TArray<FText>& ExhibitJokes);
	void BuildHelpful(FRobberExhibitAssignment& Each, UDialogueComponent* Talker, const TArray<FText>& ExhibitJokes, const FText& Clue);
	void BuildQuest(FRobberExhibitAssignment& Each, UDialogueComponent* Talker, const TArray<FText>& ExhibitJokes);
	UDialogueDefinition* NewDialogue(const FRobberExhibitAssignment& Each, const UDialogueComponent* Talker);
	TArray<FText> JokesFor(const AActor* Exhibit, const UDialogueComponent* Talker) const;
	FText RewardLine(const FRobberExhibitAssignment& Each, FText* OutCrewMessage = nullptr) const;
	FFormatNamedArguments SlotsFor(const FRobberExhibitAssignment& Each) const;

	void OnQuestAccepted(AActor* Giver, UPlayerQuestComponent* Player);
	void OnQuestTurnedIn(AActor* Giver, UPlayerQuestComponent* Player);
	void OnConversationStarted(AActor* Talker, APlayerState* Player);

	UPROPERTY(Transient) TArray<FRobberExhibitAssignment> Assignments;
	/** Keeps the night's generated dialogue and quest objects alive. */
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Generated;
	UPROPERTY(Transient) TArray<TObjectPtr<ARobberRoomInfo>> Rooms;
	UPROPERTY(Transient) TArray<FRobberChestDeal> ChestDeals;
	/** Every Puzzle component in the night's level. */
	UPROPERTY(Transient) TArray<TObjectPtr<URobberPuzzleComponent>> LevelPuzzles;
	UPROPERTY(Transient) TArray<TObjectPtr<ARobberStoryQuest>> StoryQuests;

public:
	/** A heat or guard-time bonus, drawn from the rewards table by weight. For story quests, so an optional job can't hold back something the night needs. */
	FRobberReward DrawBonusReward() const;

	/** For the dev menu: every exhibit tonight, what it does and what it pays, plus the story quests. */
	void DescribeDealForDev(TArray<FString>& OutLines, TArray<AActor*>& OutTargets) const;

private:

	bool bDealt = false;
	int32 DealAttempts = 0;
	FTimerHandle DealTimer;
	FDelegateHandle AcceptedHandle, TurnedInHandle, ConversationHandle, PuzzleHandle;
};
