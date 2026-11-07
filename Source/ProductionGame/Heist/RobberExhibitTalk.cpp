#include "Heist/RobberExhibitTalk.h"

#include "Algo/RandomShuffle.h"
#include "Components/StaticMeshComponent.h"
#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberPuzzle.h"
#include "Heist/RobberVault.h"
#include "Net/UnrealNetwork.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogExhibitTalk, Log, All);

namespace ExhibitTalk
{
	const FName ClueObjectTag(TEXT("ClueObject"));
	constexpr int32 MaxDealAttempts = 50;

	/** Slots are filled on the server, and FText's format history doesn't need to travel, so clients just get the string. */
	FText Fill(const FText& Template, const FFormatNamedArguments& Slots)
	{
		return FText::AsCultureInvariant(FText::Format(FTextFormat(Template), Slots).ToString());
	}

	FText JoinLines(const FText& First, const FText& Second)
	{
		return First.IsEmpty() ? Second : FText::AsCultureInvariant(First.ToString() + TEXT("\n\n") + Second.ToString());
	}

	template <typename RowType>
	TArray<TPair<FName, const RowType*>> RowsOf(const UDataTable* Table)
	{
		TArray<TPair<FName, const RowType*>> Result;
		if (Table)
		{
			for (const FName Name : Table->GetRowNames())
			{
				if (const RowType* Row = Table->FindRow<RowType>(Name, TEXT("ExhibitTalk"), false))
				{
					Result.Emplace(Name, Row);
				}
			}
		}
		return Result;
	}

	FDialogueNode MakeNode(FName Id, const FText& Text)
	{
		FDialogueNode Node;
		Node.ID = Id;
		Node.Text = Text;
		return Node;
	}

	FDialogueChoice MakeChoice(const FText& Text, FName Next, EDialogueQuestAction Action = EDialogueQuestAction::None, UQuestDefinition* Quest = nullptr)
	{
		FDialogueChoice Choice;
		Choice.Text = Text;
		Choice.NextNode = Next;
		Choice.QuestAction = Action;
		Choice.Quest = Quest;
		return Choice;
	}

	FString NumberWords(const TArray<int32>& Indexes, const TArray<int32>& Code)
	{
		TArray<FString> Parts;
		for (const int32 Index : Indexes)
		{
			Parts.Add(FString::Printf(TEXT("number %d is %d"), Index + 1, Code.IsValidIndex(Index) ? Code[Index] : 0));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	URobberJournalComponent* JournalOf(const UPlayerQuestComponent* Player)
	{
		return Player && Player->GetOwner() ? Player->GetOwner()->FindComponentByClass<URobberJournalComponent>() : nullptr;
	}

	constexpr int32 CodeLockDigits = 4;

	/** {object} for a puzzle: its own name, or what kind of puzzle it is. */
	FText PuzzleName(const URobberPuzzleComponent* Puzzle)
	{
		if (!Puzzle)
		{
			return FText::GetEmpty();
		}
		if (!Puzzle->PuzzleName.IsEmpty())
		{
			return Puzzle->PuzzleName;
		}
		switch (Puzzle->PuzzleType)
		{
		case ERobberPuzzleType::Wire: return NSLOCTEXT("ExhibitTalk", "PuzzleWire", "fuse box wires");
		case ERobberPuzzleType::Combination: return NSLOCTEXT("ExhibitTalk", "PuzzleCombination", "combination lock");
		case ERobberPuzzleType::CodeLock: return NSLOCTEXT("ExhibitTalk", "PuzzleCodeLock", "code lock");
		case ERobberPuzzleType::PressurePlate: return NSLOCTEXT("ExhibitTalk", "PuzzlePlates", "pressure plates");
		case ERobberPuzzleType::Scale: return NSLOCTEXT("ExhibitTalk", "PuzzleScale", "scale");
		default: return NSLOCTEXT("ExhibitTalk", "PuzzleAny", "puzzle");
		}
	}

	bool ImplementsSolver(const AActor* Actor)
	{
		UClass* Interface = URobberPuzzleComponent::GetSolverInterface();
		return Actor && Interface && Actor->GetClass()->ImplementsInterface(Interface);
	}
}

// --- Markers ---

ARobberRoomInfo::ARobberRoomInfo()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorHiddenInGame(true);
}

bool ARobberRoomInfo::Contains(const FVector& Location) const
{
	const FVector Local = Location - GetActorLocation();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

ARobberQuestSpot::ARobberQuestSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorHiddenInGame(true);
}

ARobberQuestProp::ARobberQuestProp()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARobberQuestProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberQuestProp, ProgressEvent);
	DOREPLIFETIME(ARobberQuestProp, PropPrompt);
	DOREPLIFETIME(ARobberQuestProp, bFlipped);
}

void ARobberQuestProp::Setup(FName InProgressEvent, const FText& InPrompt, const FText& InDoneMessage, bool bInFlips)
{
	ProgressEvent = InProgressEvent;
	PropPrompt = InPrompt;
	DoneMessage = InDoneMessage;
	bFlips = bInFlips;
}

FText ARobberQuestProp::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	// Prompts are worked out on the viewer's machine, where their own quests have replicated. The lost item is just
	// an odd object in a room until you've been asked to look for it.
	const AController* Controller = Viewer ? Viewer->GetOwningController() : nullptr;
	const UPlayerQuestComponent* Quests = Controller ? Controller->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	if (Quests)
	{
		for (const FPlayerQuest& Quest : Quests->Quests)
		{
			if (Quest.ProgressEvent == ProgressEvent && Quest.State == EPlayerQuestState::Active)
			{
				return PropPrompt;
			}
		}
	}
	return FText::GetEmpty();
}

void ARobberQuestProp::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UPlayerQuestComponent* Quests = Controller ? Controller->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	if (!Quests)
	{
		return;
	}
	bool bHadQuest = false;
	for (const FPlayerQuest& Quest : Quests->Quests)
	{
		bHadQuest |= Quest.ProgressEvent == ProgressEvent && Quest.State == EPlayerQuestState::Active;
	}
	if (!bHadQuest)
	{
		return;
	}
	Quests->ReportQuestEvent(ProgressEvent, 1);
	if (bFlips && !bFlipped)
	{
		bFlipped = true;
		OnRep_Flipped();
		ForceNetUpdate();
	}
	if (ARobberController* Robber = Cast<ARobberController>(Controller))
	{
		Robber->ClientHeistMessage(DoneMessage);
	}
}

void ARobberQuestProp::OnRep_Flipped()
{
	Mesh->AddRelativeRotation(FRotator(0., 0., 180.));
}

// --- Director ---

struct ARobberNightDirector::FClueFact
{
	ERobberClueKind Kind = ERobberClueKind::ObjectLocation;
	FFormatNamedArguments Slots;
};

ARobberNightDirector::ARobberNightDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorHiddenInGame(true);
	// The Server Room key stays a pickup in the Curator's Office, which makes the curator's key the start of a chain.
	RequiredQuestKeys = { TEXT("GeneratorRoom"), TEXT("SecurityKeycard"), TEXT("CuratorsOffice") };
	QuestTools.Add(TEXT("Crowbar"));
}

void ARobberNightDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	AcceptedHandle = UPlayerQuestComponent::OnQuestAccepted.AddUObject(this, &ARobberNightDirector::OnQuestAccepted);
	TurnedInHandle = UPlayerQuestComponent::OnQuestTurnedIn.AddUObject(this, &ARobberNightDirector::OnQuestTurnedIn);
	ConversationHandle = UDialogueComponent::OnConversationStarted.AddUObject(this, &ARobberNightDirector::OnConversationStarted);
	PuzzleHandle = URobberPuzzleComponent::OnExhibitPuzzleSolved.AddUObject(this, &ARobberNightDirector::OnExhibitPuzzleSolved);
	// A streamed level's actors can begin play over several frames, and the deal needs every exhibit and the vault's code.
	GetWorldTimerManager().SetTimer(DealTimer, this, &ARobberNightDirector::TryDeal, 0.1f, true);
}

void ARobberNightDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UPlayerQuestComponent::OnQuestAccepted.Remove(AcceptedHandle);
	UPlayerQuestComponent::OnQuestTurnedIn.Remove(TurnedInHandle);
	UDialogueComponent::OnConversationStarted.Remove(ConversationHandle);
	URobberPuzzleComponent::OnExhibitPuzzleSolved.Remove(PuzzleHandle);
	Super::EndPlay(EndPlayReason);
}

FName ARobberNightDirector::ExhibitId(const AActor* Exhibit)
{
	FString Name = Exhibit ? Exhibit->GetClass()->GetName() : FString();
	Name.RemoveFromEnd(TEXT("_C"));
	if (!Name.RemoveFromStart(TEXT("BP_NPC_")))
	{
		Name.RemoveFromStart(TEXT("BP_CQ_"));
	}
	return FName(*Name);
}

const FRobberExhibitAssignment* ARobberNightDirector::FindAssignment(const AActor* Exhibit) const
{
	return Assignments.FindByPredicate([Exhibit](const FRobberExhibitAssignment& Each) { return Each.Exhibit == Exhibit; });
}

const ARobberRoomInfo* ARobberNightDirector::FindRoom(const FVector& Location) const
{
	for (const ARobberRoomInfo* Room : Rooms)
	{
		if (Room && Room->Contains(Location))
		{
			return Room;
		}
	}
	return nullptr;
}

TArray<FName> ARobberNightDirector::KeysNeededAt(const AActor* Actor) const
{
	const ARobberRoomInfo* Room = Actor ? FindRoom(Actor->GetActorLocation()) : nullptr;
	return Room ? Room->RequiredKeys : TArray<FName>();
}

TArray<FName> ARobberNightDirector::KeysNeededAt(const URobberPuzzleComponent* Puzzle) const
{
	const ARobberRoomInfo* Room = Puzzle ? FindRoom(Puzzle->GetPuzzleLocation()) : nullptr;
	return Room ? Room->RequiredKeys : TArray<FName>();
}

FText ARobberNightDirector::PuzzleHintLine(const URobberPuzzleComponent* Puzzle) const
{
	const FText Hint = Puzzle ? Puzzle->GetHintText() : FText::GetEmpty();
	if (Hint.IsEmpty())
	{
		return FText::GetEmpty();
	}
	const ARobberRoomInfo* Room = FindRoom(Puzzle->GetPuzzleLocation());
	FFormatNamedArguments Slots;
	Slots.Add(TEXT("object"), ExhibitTalk::PuzzleName(Puzzle));
	Slots.Add(TEXT("room"), Room ? Room->RoomName : NSLOCTEXT("ExhibitTalk", "SomeRoom", "the museum"));
	Slots.Add(TEXT("clue"), Hint);
	return ExhibitTalk::Fill(NSLOCTEXT("ExhibitTalk", "PuzzleHintLine", "The {object} in the {room}: {clue}."), Slots);
}

void ARobberNightDirector::TryDeal()
{
	if (bDealt || !GetLevel() || !GetLevel()->bIsVisible)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(DealTimer);

	TArray<AActor*> Exhibits;
	TArray<ARobberQuestSpot*> Spots;
	Rooms.Reset();
	LevelPuzzles.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != GetLevel())
		{
			continue;
		}
		if (URobberPuzzleComponent* Puzzle = It->FindComponentByClass<URobberPuzzleComponent>())
		{
			LevelPuzzles.Add(Puzzle);
		}
		if (It->FindComponentByClass<UDialogueComponent>())
		{
			Exhibits.Add(*It);
		}
		else if (ARobberQuestSpot* Spot = Cast<ARobberQuestSpot>(*It))
		{
			Spots.Add(Spot);
		}
		else if (ARobberRoomInfo* Room = Cast<ARobberRoomInfo>(*It))
		{
			Rooms.Add(Room);
		}
		else if (ARobberKeyPickup* Key = Cast<ARobberKeyPickup>(*It); Key && RequiredQuestKeys.Contains(Key->KeyId))
		{
			Key->Destroy();
		}
	}
	if (!Jokes || !HelpfulLines || !RandomQuests || !Puzzles || !Rewards)
	{
		UE_LOG(LogExhibitTalk, Error, TEXT("%s is missing a data table; exhibits keep their own dialogue tonight."), *GetName());
		return;
	}

	// A fresh code lock code every night, so hints are worth having and nobody can learn it across nights.
	// Rolled before the deal: hints are written from it.
	for (URobberPuzzleComponent* Puzzle : LevelPuzzles)
	{
		if (Puzzle->PuzzleType == ERobberPuzzleType::CodeLock)
		{
			FString Code;
			for (int32 Digit = 0; Digit < ExhibitTalk::CodeLockDigits; ++Digit)
			{
				Code.AppendInt(FMath::RandRange(0, 9));
			}
			Puzzle->SetAnswer(Code);
		}
	}
	// A chest is dealt something when a puzzle opens it.
	ChestDeals.Reset();
	for (URobberPuzzleComponent* Puzzle : LevelPuzzles)
	{
		for (AActor* Target : Puzzle->SolveTargets)
		{
			ARobberPuzzleChest* Chest = Cast<ARobberPuzzleChest>(Target);
			if (Chest && !ChestDeals.ContainsByPredicate([Chest](const FRobberChestDeal& Each) { return Each.Chest == Chest; }))
			{
				FRobberChestDeal& Deal = ChestDeals.AddDefaulted_GetRef();
				Deal.Chest = Chest;
				Deal.Opener = Puzzle;
			}
		}
	}

	bool bSolvable = false;
	for (DealAttempts = 1; DealAttempts <= ExhibitTalk::MaxDealAttempts; ++DealAttempts)
	{
		if (DealOnce(Exhibits, Spots) && IsSolvable(Assignments))
		{
			bSolvable = true;
			break;
		}
	}
	if (!bSolvable)
	{
		// The tables or the level can't make a finishable night. Exhibits keep their own assets rather than stranding the crew.
		UE_LOG(LogExhibitTalk, Error, TEXT("No finishable deal in %d attempts (%d exhibits, %d spots)."), ExhibitTalk::MaxDealAttempts, Exhibits.Num(), Spots.Num());
		Assignments.Reset();
		return;
	}

	// Clues and quest lines name other exhibits, so every speaker has to be known before any line is written.
	for (FRobberExhibitAssignment& Each : Assignments)
	{
		const UDialogueComponent* Talker = Each.Exhibit->FindComponentByClass<UDialogueComponent>();
		Each.Speaker = Talker->Dialogue && !Talker->Dialogue->SpeakerName.IsEmpty() ? Talker->Dialogue->SpeakerName : FText::FromName(ExhibitId(Each.Exhibit));
	}

	// Each fact goes out once: required ones first, so helpful exhibits point at what the crew has to find.
	TArray<FClueFact> Facts, OtherFacts;
	CollectFacts(Facts, OtherFacts);
	Algo::RandomShuffle(Facts);
	Algo::RandomShuffle(OtherFacts);
	Facts.Append(OtherFacts);
	int32 NextFact = 0;
	TMap<const AActor*, FText> SpokenClues;
	for (const ERobberExhibitRole Wanted : { ERobberExhibitRole::Helpful, ERobberExhibitRole::Quest })
	{
		for (FRobberExhibitAssignment& Each : Assignments)
		{
			const bool bWantsClue = Each.Role == Wanted && (Wanted == ERobberExhibitRole::Helpful || Each.Reward.Kind == ERobberRewardKind::TipOff);
			if (bWantsClue && Facts.IsValidIndex(NextFact))
			{
				SpokenClues.Add(Each.Exhibit.Get(), TellFact(Facts[NextFact++], Each.JournalClue));
			}
		}
	}
	for (FRobberChestDeal& Deal : ChestDeals)
	{
		if (Deal.Reward.Kind == ERobberRewardKind::TipOff && Facts.IsValidIndex(NextFact))
		{
			TellFact(Facts[NextFact++], Deal.JournalClue);
		}
	}
	// A hint is the puzzle's answer, not a fact from the pool, so it's written straight from the puzzle.
	for (FRobberExhibitAssignment& Each : Assignments)
	{
		if (Each.Role == ERobberExhibitRole::Quest && Each.Reward.Kind == ERobberRewardKind::PuzzleHint)
		{
			Each.JournalClue = PuzzleHintLine(Each.Reward.HintPuzzle);
		}
	}
	for (FRobberChestDeal& Deal : ChestDeals)
	{
		if (Deal.Reward.Kind == ERobberRewardKind::PuzzleHint)
		{
			Deal.JournalClue = PuzzleHintLine(Deal.Reward.HintPuzzle);
		}
	}

	for (FRobberExhibitAssignment& Each : Assignments)
	{
		Each.Exhibit->FindComponentByClass<UDialogueComponent>()->Errands.Reset();
	}
	for (FRobberExhibitAssignment& Each : Assignments)
	{
		UDialogueComponent* Talker = Each.Exhibit->FindComponentByClass<UDialogueComponent>();
		const TArray<FText> ExhibitJokes = JokesFor(Each.Exhibit, Talker);
		Talker->OfferedQuests.Reset();
		switch (Each.Role)
		{
		case ERobberExhibitRole::Silly:
			BuildSilly(Each, Talker, ExhibitJokes);
			break;
		case ERobberExhibitRole::Helpful:
			BuildHelpful(Each, Talker, ExhibitJokes, SpokenClues.FindRef(Each.Exhibit.Get()));
			break;
		case ERobberExhibitRole::Quest:
			BuildQuest(Each, Talker, ExhibitJokes);
			break;
		}
	}
	// The quest's exhibit hears about the solve the same way a door does: it goes on the puzzle's list for the night.
	for (const FRobberExhibitAssignment& Each : Assignments)
	{
		if (Each.Puzzle)
		{
			Each.Puzzle->SolveTargets.AddUnique(Each.Exhibit);
		}
	}
	FillChests();
	bDealt = true;
	UE_LOG(LogExhibitTalk, Display, TEXT("Dealt %d exhibits in %d attempt(s), %d facts for %d helpful."), Assignments.Num(), DealAttempts, Facts.Num(), HelpfulCount);
}

bool ARobberNightDirector::DealOnce(const TArray<AActor*>& Exhibits, const TArray<ARobberQuestSpot*>& Spots)
{
	Assignments.Reset();
	TArray<AActor*> Shuffled = Exhibits;
	Algo::RandomShuffle(Shuffled);
	const ARobberVaultDoor* Vault = nullptr;
	for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel())
		{
			Vault = *It;
		}
	}

	for (int32 Index = 0; Index < Shuffled.Num(); ++Index)
	{
		FRobberExhibitAssignment& Each = Assignments.AddDefaulted_GetRef();
		Each.Exhibit = Shuffled[Index];
		Each.Role = Index < QuestCount ? ERobberExhibitRole::Quest : Index < QuestCount + HelpfulCount ? ERobberExhibitRole::Helpful : ERobberExhibitRole::Silly;
	}
	TArray<FRobberExhibitAssignment*> Givers;
	TArray<AActor*> NotGivers;
	for (FRobberExhibitAssignment& Each : Assignments)
	{
		if (Each.Role == ERobberExhibitRole::Quest)
		{
			Givers.Add(&Each);
		}
		else
		{
			NotGivers.Add(Each.Exhibit);
		}
	}

	// --- Required rewards first, then bonuses. ---
	TArray<FRobberReward> Pay;
	if (Vault)
	{
		TArray<int32> Digits;
		for (int32 Digit = 0; Digit < ARobberVaultDoor::CodeLength; ++Digit)
		{
			Digits.Add(Digit);
		}
		Algo::RandomShuffle(Digits);
		const int32 Holders = FMath::Clamp(DigitQuests, 1, FMath::Max(1, FMath::Min(Givers.Num(), ARobberVaultDoor::CodeLength)));
		for (int32 Holder = 0; Holder < Holders; ++Holder)
		{
			FRobberReward& Reward = Pay.AddDefaulted_GetRef();
			Reward.Kind = ERobberRewardKind::VaultDigits;
			for (int32 Digit = Holder; Digit < Digits.Num(); Digit += Holders)
			{
				Reward.DigitIndexes.Add(Digits[Digit]);
			}
			Reward.DigitIndexes.Sort();
		}
	}
	auto AddKeyReward = [this, &Pay](const FName Key, const ERobberRewardKind Kind)
	{
		FRobberReward& Reward = Pay.AddDefaulted_GetRef();
		Reward.Kind = Kind;
		Reward.KeyId = Key;
		for (TActorIterator<ARobberLockedDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetLevel() && It->KeyId == Key)
			{
				Reward.KeyName = It->KeyName;
			}
		}
		if (Reward.KeyName.IsEmpty())
		{
			Reward.KeyName = FText::FromName(Key);
		}
	};
	for (const FName Key : RequiredQuestKeys)
	{
		AddKeyReward(Key, ERobberRewardKind::Key);
	}
	for (const FName Tool : QuestTools)
	{
		AddKeyReward(Tool, ERobberRewardKind::Tool);
	}
	// Chests a puzzle opens take rewards from the same pool as quests.
	for (FRobberChestDeal& Deal : ChestDeals)
	{
		Deal.Reward = FRobberReward();
		Deal.JournalClue = FText::GetEmpty();
	}
	const int32 Slots = Givers.Num() + ChestDeals.Num();
	if (Pay.Num() > Slots)
	{
		UE_LOG(LogExhibitTalk, Error, TEXT("%d required rewards but only %d quests and %d chests tonight."), Pay.Num(), Givers.Num(), ChestDeals.Num());
		return false;
	}
	// Bonus rewards come from the table by weight.
	TArray<const FRobberRewardRow*> Bonuses;
	float TotalWeight = 0.f;
	for (const auto& Row : ExhibitTalk::RowsOf<FRobberRewardRow>(Rewards))
	{
		if (Row.Value->Weight > 0.f)
		{
			Bonuses.Add(Row.Value);
			TotalWeight += Row.Value->Weight;
		}
	}
	while (Pay.Num() < Slots && TotalWeight > 0.f)
	{
		float Roll = FMath::FRandRange(0.f, TotalWeight);
		const FRobberRewardRow* Drawn = Bonuses.Last();
		for (const FRobberRewardRow* Bonus : Bonuses)
		{
			Roll -= Bonus->Weight;
			if (Roll <= 0.f)
			{
				Drawn = Bonus;
				break;
			}
		}
		FRobberReward& Reward = Pay.AddDefaulted_GetRef();
		Reward.Kind = Drawn->Kind;
		Reward.Value = Drawn->Value;
	}
	Algo::RandomShuffle(Pay);
	for (int32 Index = 0; Index < ChestDeals.Num(); ++Index)
	{
		if (Pay.IsValidIndex(Givers.Num() + Index))
		{
			ChestDeals[Index].Reward = Pay[Givers.Num() + Index];
		}
	}
	TArray<URobberPuzzleComponent*> FreePuzzles;
	for (URobberPuzzleComponent* Puzzle : LevelPuzzles)
	{
		if (Puzzle && !Puzzle->IsSolved())
		{
			FreePuzzles.Add(Puzzle);
		}
	}
	Algo::RandomShuffle(FreePuzzles);

	// --- A template per quest, not too many of a kind, nothing twice. ---
	auto QuestRows = ExhibitTalk::RowsOf<FRobberQuestRow>(RandomQuests);
	Algo::RandomShuffle(QuestRows);
	auto PuzzleRows = ExhibitTalk::RowsOf<FRobberPuzzleRow>(Puzzles);
	Algo::RandomShuffle(PuzzleRows);
	TArray<ARobberQuestSpot*> FreeSpots = Spots;
	Algo::RandomShuffle(FreeSpots);
	TMap<ERobberQuestKind, int32> KindCounts;
	Algo::RandomShuffle(NotGivers);
	int32 NextTarget = 0;
	int32 NextRow = 0;
	for (int32 Index = 0; Index < Givers.Num(); ++Index)
	{
		FRobberExhibitAssignment& Each = *Givers[Index];
		Each.Reward = Pay.IsValidIndex(Index) ? Pay[Index] : FRobberReward();
		const FRobberQuestRow* Chosen = nullptr;
		for (; NextRow < QuestRows.Num() && !Chosen; ++NextRow)
		{
			const auto& Row = QuestRows[NextRow];
			const ERobberQuestKind Kind = Row.Value->Kind;
			const bool bNeedsPuzzle = Kind == ERobberQuestKind::Riddle || Kind == ERobberQuestKind::Flattery;
			const bool bNeedsSpot = Kind == ERobberQuestKind::Fetch || Kind == ERobberQuestKind::Tamper;
			const bool bNeedsTarget = Kind == ERobberQuestKind::Gossip || Kind == ERobberQuestKind::Deliver;
			if (KindCounts.FindOrAdd(Kind) >= MaxQuestsPerKind || (bNeedsSpot && FreeSpots.IsEmpty()) || (bNeedsTarget && !NotGivers.IsValidIndex(NextTarget)))
			{
				continue;
			}
			if (Kind == ERobberQuestKind::Puzzle)
			{
				// One quest per puzzle a night, and the giver has to be able to hear the solve (BP_TalkingMaster children can).
				const ERobberPuzzleType Wanted = Row.Value->PuzzleType;
				const int32 Found = FreePuzzles.IndexOfByPredicate([Wanted](const URobberPuzzleComponent* Puzzle)
				{
					return Wanted == ERobberPuzzleType::Any || Puzzle->PuzzleType == Wanted;
				});
				if (Found == INDEX_NONE || !ExhibitTalk::ImplementsSolver(Each.Exhibit))
				{
					continue;
				}
				Each.Puzzle = FreePuzzles[Found];
				FreePuzzles.RemoveAt(Found);
			}
			if (bNeedsPuzzle)
			{
				const int32 Puzzle = PuzzleRows.IndexOfByPredicate([Kind](const TPair<FName, const FRobberPuzzleRow*>& Each2) { return Each2.Value->Kind == Kind; });
				if (Puzzle == INDEX_NONE)
				{
					continue;
				}
				Each.PuzzleRow = PuzzleRows[Puzzle].Key;
				PuzzleRows.RemoveAt(Puzzle);
			}
			if (bNeedsSpot)
			{
				Each.Spot = FreeSpots.Pop();
			}
			if (bNeedsTarget)
			{
				Each.Target = NotGivers[NextTarget++];
			}
			++KindCounts[Kind];
			Each.QuestKind = Kind;
			Each.QuestRow = Row.Key;
			Chosen = Row.Value;
		}
		if (!Chosen)
		{
			UE_LOG(LogExhibitTalk, Error, TEXT("Ran out of quest templates at quest %d of %d."), Index + 1, Givers.Num());
			return false;
		}
	}

	// --- Hints point at some other puzzle than the one the reward comes from. ---
	auto PickHint = [this](FRobberReward& Reward, const URobberPuzzleComponent* Not)
	{
		if (Reward.Kind != ERobberRewardKind::PuzzleHint)
		{
			return;
		}
		TArray<URobberPuzzleComponent*> Choices;
		for (URobberPuzzleComponent* Puzzle : LevelPuzzles)
		{
			if (Puzzle && Puzzle != Not && !Puzzle->GetHintText().IsEmpty())
			{
				Choices.Add(Puzzle);
			}
		}
		if (Choices.IsEmpty())
		{
			// No other puzzle tonight: a free clue is the next best thing.
			Reward.Kind = ERobberRewardKind::TipOff;
			return;
		}
		Reward.HintPuzzle = Choices[FMath::RandHelper(Choices.Num())];
	};
	for (FRobberExhibitAssignment* Each : Givers)
	{
		PickHint(Each->Reward, Each->Puzzle);
	}
	for (FRobberChestDeal& Deal : ChestDeals)
	{
		PickHint(Deal.Reward, Deal.Opener);
	}
	return true;
}

bool ARobberNightDirector::IsSolvable(const TArray<FRobberExhibitAssignment>& Deal) const
{
	TSet<FName> Keys;
	auto HaveAll = [&Keys](const TArray<FName>& Needed)
	{
		return !Needed.ContainsByPredicate([&Keys](const FName Key) { return !Keys.Contains(Key); });
	};
	auto CanFinish = [&](const FRobberExhibitAssignment& Each)
	{
		return HaveAll(KeysNeededAt(Each.Exhibit)) && HaveAll(KeysNeededAt(Each.Target)) && HaveAll(KeysNeededAt(Each.Spot))
			&& HaveAll(KeysNeededAt(Each.Puzzle.Get()));
	};
	// A chest needs its own room and the room of the puzzle that opens it. Only room keys are known here: a puzzle
	// standing behind the very door it opens isn't caught.
	auto CanOpen = [&](const FRobberChestDeal& Chest)
	{
		return HaveAll(KeysNeededAt(Chest.Chest.Get())) && HaveAll(KeysNeededAt(Chest.Opener.Get()));
	};
	auto PaysKey = [](const FRobberReward& Reward)
	{
		return Reward.Kind == ERobberRewardKind::Key || Reward.Kind == ERobberRewardKind::Tool;
	};
	// Keep collecting until nothing new turns up. A key behind its own door never gets collected, so that deal fails.
	bool bProgress = true;
	while (bProgress)
	{
		bProgress = false;
		for (TActorIterator<ARobberKeyPickup> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetLevel() && !RequiredQuestKeys.Contains(It->KeyId) && !Keys.Contains(It->KeyId) && HaveAll(KeysNeededAt(*It)))
			{
				Keys.Add(It->KeyId);
				bProgress = true;
			}
		}
		for (const FRobberExhibitAssignment& Each : Deal)
		{
			const bool bPaysKey =Each.Reward.Kind == ERobberRewardKind::Key || Each.Reward.Kind == ERobberRewardKind::Tool;
			if (bPaysKey && !Keys.Contains(Each.Reward.KeyId) && CanFinish(Each))
			{
				Keys.Add(Each.Reward.KeyId);
				bProgress = true;
			}
		}
		for (const FRobberChestDeal& Chest : ChestDeals)
		{
			if (PaysKey(Chest.Reward) && !Keys.Contains(Chest.Reward.KeyId) && CanOpen(Chest))
			{
				Keys.Add(Chest.Reward.KeyId);
				bProgress = true;
			}
		}
	}
	for (const FRobberChestDeal& Chest : ChestDeals)
	{
		if ((PaysKey(Chest.Reward) || Chest.Reward.Kind == ERobberRewardKind::VaultDigits) && !CanOpen(Chest))
		{
			return false;
		}
	}
	for (const FRobberExhibitAssignment& Each : Deal)
	{
		const bool bRequired =Each.Reward.Kind == ERobberRewardKind::VaultDigits || Each.Reward.Kind == ERobberRewardKind::Key || Each.Reward.Kind == ERobberRewardKind::Tool;
		if (Each.Role == ERobberExhibitRole::Quest && bRequired && !CanFinish(Each))
		{
			return false;
		}
	}
	return true;
}

void ARobberNightDirector::CollectFacts(TArray<FClueFact>& Required, TArray<FClueFact>& Others) const
{
	Required.Reset();
	Others.Reset();
	auto AddRoomSlots = [this](FFormatNamedArguments& Slots, const AActor* Actor)
	{
		const ARobberRoomInfo* Room = Actor ? FindRoom(Actor->GetActorLocation()) : nullptr;
		if (Room)
		{
			Slots.Add(TEXT("room"), Room->RoomName);
			Slots.Add(TEXT("floor"), Room->FloorName);
		}
		return Room != nullptr;
	};
	for (const FRobberExhibitAssignment& Each : Assignments)
	{
		if (Each.Role != ERobberExhibitRole::Quest)
		{
			continue;
		}
		if (Each.Reward.Kind == ERobberRewardKind::VaultDigits)
		{
			FClueFact& Fact = Required.AddDefaulted_GetRef();
			Fact.Kind = ERobberClueKind::DigitHolder;
			Fact.Slots.Add(TEXT("exhibit"), Each.Speaker);
			Fact.Slots.Add(TEXT("digits"), Each.Reward.DigitIndexes.Num());
		}
		else if (Each.Reward.Kind == ERobberRewardKind::Key || Each.Reward.Kind == ERobberRewardKind::Tool)
		{
			FClueFact& Fact = Required.AddDefaulted_GetRef();
			Fact.Kind = ERobberClueKind::KeyHolder;
			Fact.Slots.Add(TEXT("exhibit"), Each.Speaker);
			Fact.Slots.Add(TEXT("key"), Each.Reward.KeyName);
		}
	}
	// A key or digits in a chest: say where the chest is.
	for (const FRobberChestDeal& Deal : ChestDeals)
	{
		const bool bKey = Deal.Reward.Kind == ERobberRewardKind::Key || Deal.Reward.Kind == ERobberRewardKind::Tool;
		if (!bKey && Deal.Reward.Kind != ERobberRewardKind::VaultDigits)
		{
			continue;
		}
		FClueFact Fact;
		Fact.Kind = ERobberClueKind::ObjectLocation;
		Fact.Slots.Add(TEXT("object"), bKey
			? FText::Format(NSLOCTEXT("ExhibitTalk", "ChestKey", "a chest with the {0} in it"), Deal.Reward.KeyName)
			: NSLOCTEXT("ExhibitTalk", "ChestDigits", "a chest with part of the vault code in it"));
		if (AddRoomSlots(Fact.Slots, Deal.Chest))
		{
			Required.Add(Fact);
		}
	}
	for (const URobberPuzzleComponent* Puzzle : LevelPuzzles)
	{
		const FText Hint = Puzzle ? Puzzle->GetHintText() : FText::GetEmpty();
		const ARobberRoomInfo* Room = Puzzle ? FindRoom(Puzzle->GetPuzzleLocation()) : nullptr;
		if (!Hint.IsEmpty() && Room)
		{
			FClueFact Fact;
			Fact.Kind = ERobberClueKind::PuzzleHint;
			Fact.Slots.Add(TEXT("object"), ExhibitTalk::PuzzleName(Puzzle));
			Fact.Slots.Add(TEXT("room"), Room->RoomName);
			Fact.Slots.Add(TEXT("floor"), Room->FloorName);
			Fact.Slots.Add(TEXT("clue"), Hint);
			Others.Add(Fact);
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != GetLevel())
		{
			continue;
		}
		if (const ARobberKeyPickup* Key = Cast<ARobberKeyPickup>(*It); Key && !RequiredQuestKeys.Contains(Key->KeyId))
		{
			FClueFact Fact;
			Fact.Kind = ERobberClueKind::ObjectLocation;
			Fact.Slots.Add(TEXT("object"), Key->KeyName);
			if (AddRoomSlots(Fact.Slots, Key))
			{
				Others.Add(Fact);
			}
		}
		else if (const ARobberLockedDoor* Door = Cast<ARobberLockedDoor>(*It))
		{
			// The door stands on its room's edge, so go by the rooms that list its key.
			for (const ARobberRoomInfo* Room : Rooms)
			{
				if (Room && Room->RequiredKeys.Contains(Door->KeyId))
				{
					FClueFact Fact;
					Fact.Kind = ERobberClueKind::ToolUse;
					Fact.Slots.Add(TEXT("key"), Door->KeyName);
					Fact.Slots.Add(TEXT("room"), Room->RoomName);
					Fact.Slots.Add(TEXT("floor"), Room->FloorName);
					Others.Add(Fact);
					break;
				}
			}
		}
		else if (It->Tags.Contains(ExhibitTalk::ClueObjectTag) && It->Tags.Num() > 1)
		{
			// Tags: ClueObject, then what to call it.
			FClueFact Fact;
			Fact.Kind = ERobberClueKind::ObjectLocation;
			const FName Name = It->Tags[0] == ExhibitTalk::ClueObjectTag ? It->Tags[1] : It->Tags[0];
			Fact.Slots.Add(TEXT("object"), FText::FromName(Name));
			if (AddRoomSlots(Fact.Slots, *It))
			{
				Others.Add(Fact);
			}
		}
	}
}

FText ARobberNightDirector::TellFact(const FClueFact& Fact, FText& OutJournal) const
{
	TArray<const FRobberClueLineRow*> Lines;
	for (const auto& Row : ExhibitTalk::RowsOf<FRobberClueLineRow>(HelpfulLines))
	{
		if (Row.Value->Kind == Fact.Kind)
		{
			Lines.Add(Row.Value);
		}
	}
	if (Lines.IsEmpty())
	{
		UE_LOG(LogExhibitTalk, Error, TEXT("DT_HelpfulLines has no line for clue kind %d."), (int32)Fact.Kind);
		return FText::GetEmpty();
	}
	const FRobberClueLineRow* Line = Lines[FMath::RandHelper(Lines.Num())];
	OutJournal = ExhibitTalk::Fill(Line->JournalLine, Fact.Slots);
	return ExhibitTalk::Fill(Line->Line, Fact.Slots);
}

TArray<FText> ARobberNightDirector::JokesFor(const AActor* Exhibit, const UDialogueComponent* Talker) const
{
	TArray<FText> Lines;
	if (const FRobberJokeRow* Row = Jokes ? Jokes->FindRow<FRobberJokeRow>(ExhibitId(Exhibit), TEXT("ExhibitTalk"), false) : nullptr)
	{
		Lines = Row->Lines;
	}
	if (Lines.IsEmpty() && Talker && Talker->Dialogue)
	{
		// An exhibit nobody's added to the table yet still has the lines in its own dialogue asset.
		for (const FDialogueNode& Node : Talker->Dialogue->Nodes)
		{
			if (!Node.Text.IsEmpty() && (Talker->Dialogue->RandomEntryNodes.Contains(Node.ID) || Talker->Dialogue->EntryNode == Node.ID))
			{
				Lines.Add(Node.Text);
			}
		}
	}
	Algo::RandomShuffle(Lines);
	return Lines;
}

UDialogueDefinition* ARobberNightDirector::NewDialogue(const FRobberExhibitAssignment& Each, const UDialogueComponent* Talker)
{
	UDialogueDefinition* Dialogue = NewObject<UDialogueDefinition>(this);
	Generated.Add(Dialogue);
	Dialogue->SpeakerName = Each.Speaker;
	Dialogue->Portrait = Talker->Dialogue ? Talker->Dialogue->Portrait : nullptr;
	Dialogue->bIncludeQuestChoices = false;
	return Dialogue;
}

void ARobberNightDirector::BuildSilly(FRobberExhibitAssignment& Each, UDialogueComponent* Talker, const TArray<FText>& ExhibitJokes)
{
	UDialogueDefinition* Dialogue = NewDialogue(Each, Talker);
	Dialogue->DialogueType = EDialogueType::DialogueOnly;
	const int32 Count = ExhibitJokes.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FName Id(*FString::Printf(TEXT("Joke%d"), Index));
		FDialogueNode Node = ExhibitTalk::MakeNode(Id, ExhibitJokes[Index]);
		if (Count > 1)
		{
			Node.Choices.Add(ExhibitTalk::MakeChoice(TellAnotherChoice, FName(*FString::Printf(TEXT("Joke%d"), (Index + 1) % Count))));
		}
		Node.Choices.Add(ExhibitTalk::MakeChoice(GoodbyeChoice, NAME_None));
		Dialogue->Nodes.Add(Node);
		Dialogue->RandomEntryNodes.Add(Id);
	}
	if (Count == 0)
	{
		FDialogueNode Node = ExhibitTalk::MakeNode(TEXT("Joke0"), NSLOCTEXT("ExhibitTalk", "NoJokes", "..."));
		Node.Choices.Add(ExhibitTalk::MakeChoice(GoodbyeChoice, NAME_None));
		Dialogue->Nodes.Add(Node);
		Dialogue->EntryNode = TEXT("Joke0");
	}
	Talker->Dialogue = Dialogue;
	Talker->bNotedInJournal = false;
}

void ARobberNightDirector::BuildHelpful(FRobberExhibitAssignment& Each, UDialogueComponent* Talker, const TArray<FText>& ExhibitJokes, const FText& Clue)
{
	UDialogueDefinition* Dialogue = NewDialogue(Each, Talker);
	Dialogue->DialogueType = EDialogueType::DialogueOnly;
	FDialogueNode Node = ExhibitTalk::MakeNode(TEXT("Clue"), ExhibitTalk::JoinLines(ExhibitJokes.IsEmpty() ? FText::GetEmpty() : ExhibitJokes[0], Clue));
	Node.Choices.Add(ExhibitTalk::MakeChoice(ThanksChoice, NAME_None));
	Dialogue->Nodes.Add(Node);
	Dialogue->EntryNode = Node.ID;
	Talker->Dialogue = Dialogue;
	Talker->bNotedInJournal = true;
	Talker->JournalQuote = Clue;
}

FFormatNamedArguments ARobberNightDirector::SlotsFor(const FRobberExhibitAssignment& Each) const
{
	FFormatNamedArguments Slots;
	Slots.Add(TEXT("giver"), Each.Speaker);
	const FRobberQuestRow* Row = RandomQuests->FindRow<FRobberQuestRow>(Each.QuestRow, TEXT("ExhibitTalk"), false);
	Slots.Add(TEXT("item"), Row ? Row->ItemName : FText::GetEmpty());
	const FRobberExhibitAssignment* Target = FindAssignment(Each.Target);
	Slots.Add(TEXT("exhibit"), Target ? Target->Speaker : FText::GetEmpty());
	const ARobberRoomInfo* Room = Each.Spot ? FindRoom(Each.Spot->GetActorLocation()) : Each.Target ? FindRoom(Each.Target->GetActorLocation())
		: Each.Puzzle ? FindRoom(Each.Puzzle->GetPuzzleLocation()) : nullptr;
	Slots.Add(TEXT("object"), ExhibitTalk::PuzzleName(Each.Puzzle));
	Slots.Add(TEXT("room"), Room ? Room->RoomName : NSLOCTEXT("ExhibitTalk", "SomeRoom", "the museum"));
	Slots.Add(TEXT("floor"), Room ? Room->FloorName : FText::GetEmpty());
	return Slots;
}

FText ARobberNightDirector::RewardLine(const FRobberExhibitAssignment& Each, FText* OutCrewMessage) const
{
	FFormatNamedArguments Slots;
	Slots.Add(TEXT("key"), Each.Reward.KeyName);
	Slots.Add(TEXT("clue"), Each.JournalClue);
	FString Digits;
	if (Each.Reward.Kind == ERobberRewardKind::VaultDigits)
	{
		for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetLevel())
			{
				Digits = ExhibitTalk::NumberWords(Each.Reward.DigitIndexes, It->GetCode());
			}
		}
	}
	Slots.Add(TEXT("digits"), FText::AsCultureInvariant(Digits));
	for (const auto& Row : ExhibitTalk::RowsOf<FRobberRewardRow>(Rewards))
	{
		if (Row.Value->Kind == Each.Reward.Kind)
		{
			if (OutCrewMessage)
			{
				*OutCrewMessage = Row.Value->CrewMessage;
			}
			return ExhibitTalk::Fill(Row.Value->Line, Slots);
		}
	}
	UE_LOG(LogExhibitTalk, Error, TEXT("DT_Rewards has no row for reward kind %d."), (int32)Each.Reward.Kind);
	return FText::GetEmpty();
}

void ARobberNightDirector::BuildQuest(FRobberExhibitAssignment& Each, UDialogueComponent* Talker, const TArray<FText>& ExhibitJokes)
{
	using namespace ExhibitTalk;
	const FRobberQuestRow* Row = RandomQuests->FindRow<FRobberQuestRow>(Each.QuestRow, TEXT("ExhibitTalk"));
	if (!Row)
	{
		return;
	}
	const FFormatNamedArguments Slots = SlotsFor(Each);
	const FString Id = FString::Printf(TEXT("Night_%s"), *Each.Exhibit->GetName());

	UQuestDefinition* Quest = NewObject<UQuestDefinition>(this);
	Generated.Add(Quest);
	Quest->QuestID = FName(*Id);
	Quest->ProgressEvent = Quest->QuestID;
	Quest->QuestName = Fill(Row->QuestName, Slots);
	Quest->ObjectiveText = Fill(Row->Objective, Slots);
	Quest->Description = Quest->ObjectiveText;
	Quest->RequiredAmount = 1;
	Each.Quest = Quest;
	Talker->OfferedQuests.Add(Quest);

	UDialogueDefinition* Dialogue = NewDialogue(Each, Talker);
	Dialogue->DialogueType = EDialogueType::QuestDialogue;
	Dialogue->ContextQuest = Quest;
	Dialogue->EntryNode = TEXT("Offer");
	Dialogue->ActiveEntryNode = TEXT("Active");
	Dialogue->ReadyEntryNode = TEXT("Ready");
	Dialogue->CompletedEntryNode = TEXT("Done");

	const bool bPuzzle = Each.QuestKind == ERobberQuestKind::Riddle || Each.QuestKind == ERobberQuestKind::Flattery;
	const FText* ExhibitOffer = Row->ExhibitOffers.Find(ExhibitId(Each.Exhibit));
	Talker->JournalQuote = Fill(ExhibitOffer ? *ExhibitOffer : Row->Offer, Slots);
	FDialogueNode Offer = MakeNode(TEXT("Offer"), JoinLines(ExhibitJokes.IsEmpty() ? FText::GetEmpty() : ExhibitJokes[0], Talker->JournalQuote));
	Offer.Choices.Add(MakeChoice(Fill(Row->AcceptChoice, Slots), TEXT("Active"), EDialogueQuestAction::Accept, Quest));
	Offer.Choices.Add(MakeChoice(NotNowChoice, NAME_None));
	Dialogue->Nodes.Add(Offer);

	if (bPuzzle)
	{
		const FRobberPuzzleRow* Puzzle = Puzzles->FindRow<FRobberPuzzleRow>(Each.PuzzleRow, TEXT("ExhibitTalk"));
		if (!Puzzle)
		{
			return;
		}
		// Puzzle answers are server-validated progress choices, so a wrong one can't be replayed into a pass.
		FDialogueNode Ask = MakeNode(TEXT("Active"), Fill(Puzzle->Question, Slots));
		TArray<FDialogueChoice> Answers;
		FDialogueChoice Right = MakeChoice(Fill(Puzzle->Right, Slots), TEXT("Ready"), EDialogueQuestAction::Progress, Quest);
		Answers.Add(Right);
		Answers.Add(MakeChoice(Fill(Puzzle->Wrong1, Slots), TEXT("Wrong")));
		Answers.Add(MakeChoice(Fill(Puzzle->Wrong2, Slots), TEXT("Wrong")));
		Algo::RandomShuffle(Answers);
		Ask.Choices = Answers;
		Ask.Choices.Add(MakeChoice(NotNowChoice, NAME_None));
		Dialogue->Nodes.Add(Ask);
		FDialogueNode Wrong = MakeNode(TEXT("Wrong"), Fill(Puzzle->WrongReply, Slots));
		Wrong.Choices.Add(MakeChoice(RetryChoice, TEXT("Active")));
		Wrong.Choices.Add(MakeChoice(NotNowChoice, NAME_None));
		Dialogue->Nodes.Add(Wrong);
	}
	else
	{
		FDialogueNode Active = MakeNode(TEXT("Active"), Fill(Row->Active, Slots));
		Active.Choices.Add(MakeChoice(OnItChoice, NAME_None));
		Dialogue->Nodes.Add(Active);
	}

	FDialogueNode Ready = MakeNode(TEXT("Ready"), Fill(Row->Ready, Slots));
	Ready.Choices.Add(MakeChoice(Fill(Row->TurnInChoice, Slots), TEXT("Done"), EDialogueQuestAction::TurnIn, Quest));
	Dialogue->Nodes.Add(Ready);

	FDialogueNode Done = MakeNode(TEXT("Done"), RewardLine(Each));
	Done.Choices.Add(MakeChoice(ThanksChoice, NAME_None));
	Dialogue->Nodes.Add(Done);

	Talker->Dialogue = Dialogue;
	Talker->bNotedInJournal = true;

	if (Each.Spot)
	{
		// The prop's class needs its Blueprint child for BPI_Interaction, so no class means no way to do the quest.
		if (PropClass)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.OverrideLevel = GetLevel();
			Params.bDeferConstruction = true;
			const FTransform At(Each.Spot->GetActorRotation(), Each.Spot->GetActorLocation(), FVector(0.3));
			if (ARobberQuestProp* Prop = GetWorld()->SpawnActor<ARobberQuestProp>(PropClass, At, Params))
			{
				Prop->Setup(Quest->ProgressEvent, Fill(Row->PropPrompt, Slots),
					Fill(NSLOCTEXT("ExhibitTalk", "PropDone", "Done. Back to {giver}."), Slots), Each.QuestKind == ERobberQuestKind::Tamper);
				Prop->FinishSpawning(At);
				Each.Prop = Prop;
			}
		}
		else
		{
			UE_LOG(LogExhibitTalk, Error, TEXT("%s has no Prop Class, so %s can't be done."), *GetName(), *Id);
		}
	}
	if (const FRobberExhibitAssignment* Target = FindAssignment(Each.Target))
	{
		FDialogueErrand Errand;
		Errand.Quest = Quest;
		Errand.Choice = Fill(Row->ErrandChoice, Slots);
		Errand.Reply = Fill(Row->ErrandReply, Slots);
		Errand.RequiredNearbyTag = Each.QuestKind == ERobberQuestKind::Deliver ? Quest->QuestID : NAME_None;
		if (UDialogueComponent* TargetTalker = Target->Exhibit->FindComponentByClass<UDialogueComponent>())
		{
			TargetTalker->Errands.Add(Errand);
		}
	}
}

void ARobberNightDirector::OnQuestAccepted(AActor* Giver, UPlayerQuestComponent* Player)
{
	const FRobberExhibitAssignment* Each = FindAssignment(Giver);
	if (Each && Each->QuestKind == ERobberQuestKind::Puzzle && Each->bPuzzleSolved && Each->Quest && Player)
	{
		// Solved before anyone asked: it counts, so the quest is ready straight away.
		Player->ReportQuestEvent(Each->Quest->ProgressEvent, 1);
		return;
	}
	if (!Each || Each->QuestKind != ERobberQuestKind::Deliver || !Each->Quest || !Giver || Giver->GetWorld() != GetWorld())
	{
		return;
	}
	if (!ParcelClass)
	{
		UE_LOG(LogExhibitTalk, Error, TEXT("%s has no Parcel Class, so %s can't be delivered."), *GetName(), *Each->Quest->QuestID.ToString());
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.OverrideLevel = GetLevel();
	const FVector At = Giver->GetActorLocation() + Giver->GetActorForwardVector() * 120. + FVector(0., 0., -40.);
	if (AActor* Parcel = GetWorld()->SpawnActor<AActor>(ParcelClass, At, FRotator::ZeroRotator, Params))
	{
		Parcel->Tags.Add(Each->Quest->QuestID);
	}
}

void ARobberNightDirector::OnQuestTurnedIn(AActor* Giver, UPlayerQuestComponent* Player)
{
	FRobberExhibitAssignment* Each = Assignments.FindByPredicate([Giver](const FRobberExhibitAssignment& One) { return One.Exhibit == Giver; });
	if (!Each || !Player || Each->Role != ERobberExhibitRole::Quest || !Giver || Giver->GetWorld() != GetWorld())
	{
		return;
	}
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	URobberJournalComponent* Journal = ExhibitTalk::JournalOf(Player);
	switch (Each->Reward.Kind)
	{
	case ERobberRewardKind::VaultDigits:
		for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
		{
			if (It->GetLevel() == GetLevel() && Journal)
			{
				for (const int32 Index : Each->Reward.DigitIndexes)
				{
					if (It->GetCode().IsValidIndex(Index))
					{
						Journal->LearnVaultDigit(Giver, Each->Speaker, Index, It->GetCode()[Index], ARobberVaultDoor::CodeLength);
					}
				}
			}
		}
		break;
	case ERobberRewardKind::Key:
	case ERobberRewardKind::Tool:
		Player->GiveKey(Each->Reward.KeyId);
		if (Journal)
		{
			Journal->LearnFact(Giver, Each->Speaker, FText::Format(NSLOCTEXT("ExhibitTalk", "GotKey", "Gave you the {0}."), Each->Reward.KeyName));
		}
		break;
	case ERobberRewardKind::TipOff:
	case ERobberRewardKind::PuzzleHint:
		if (Journal && !Each->JournalClue.IsEmpty())
		{
			Journal->LearnFact(Giver, Each->Speaker, Each->JournalClue);
		}
		break;
	case ERobberRewardKind::GuardDistraction:
	case ERobberRewardKind::ExtraTime:
		// These change the night for the whole crew, so a second player finishing the same favour doesn't stack them.
		if (GameState && !Each->bCrewRewardPaid)
		{
			Each->bCrewRewardPaid = true;
			if (Each->Reward.Kind == ERobberRewardKind::GuardDistraction)
			{
				GameState->CoolHeat(Each->Reward.Value);
			}
			else
			{
				GameState->AddGuardsTime(Each->Reward.Value);
			}
			FText CrewMessage;
			RewardLine(*Each, &CrewMessage);
			if (!CrewMessage.IsEmpty())
			{
				FFormatNamedArguments Slots;
				Slots.Add(TEXT("giver"), Each->Speaker);
				GameState->TellCrew(ExhibitTalk::Fill(CrewMessage, Slots));
			}
		}
		break;
	default:
		break;
	}
}

void ARobberNightDirector::OnConversationStarted(AActor* Talker, APlayerState* Player)
{
	const FRobberExhibitAssignment* Each = FindAssignment(Talker);
	if (!Each || Each->Role != ERobberExhibitRole::Helpful || Each->JournalClue.IsEmpty() || !Player || !Talker || Talker->GetWorld() != GetWorld())
	{
		return;
	}
	const AController* Controller = Player->GetOwningController();
	if (URobberJournalComponent* Journal = Controller ? Controller->FindComponentByClass<URobberJournalComponent>() : nullptr)
	{
		Journal->LearnFact(Talker, Each->Speaker, Each->JournalClue);
	}
}

void ARobberNightDirector::OnExhibitPuzzleSolved(AActor* Exhibit, AActor* Puzzle, APawn* SolvedBy)
{
	FRobberExhibitAssignment* Each = Assignments.FindByPredicate([Exhibit](const FRobberExhibitAssignment& One) { return One.Exhibit == Exhibit; });
	if (!Each || !Each->Puzzle || Each->Puzzle->GetOwner() != Puzzle || !Each->Quest || Each->bPuzzleSolved || !Exhibit || Exhibit->GetWorld() != GetWorld())
	{
		return;
	}
	Each->bPuzzleSolved = true;
	// The whole crew gets the tick, whoever solved it. Anyone who takes the quest later gets it on accepting.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (UPlayerQuestComponent* Quests = It->IsValid() ? It->Get()->FindComponentByClass<UPlayerQuestComponent>() : nullptr)
		{
			Quests->ReportQuestEvent(Each->Quest->ProgressEvent, 1);
		}
	}
	if (ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>())
	{
		const APlayerState* By = SolvedBy ? SolvedBy->GetPlayerState() : nullptr;
		FFormatNamedArguments Slots;
		Slots.Add(TEXT("who"), By ? FText::FromString(By->GetPlayerName()) : NSLOCTEXT("ExhibitTalk", "SomeoneSolved", "Someone"));
		Slots.Add(TEXT("object"), ExhibitTalk::PuzzleName(Each->Puzzle));
		Slots.Add(TEXT("giver"), Each->Speaker);
		GameState->TellCrew(ExhibitTalk::Fill(NSLOCTEXT("ExhibitTalk", "PuzzleSolvedCrew", "{who} solved the {object}. {giver} will want to hear."), Slots));
	}
}

void ARobberNightDirector::FillChests()
{
	const ARobberVaultDoor* Vault = nullptr;
	for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel())
		{
			Vault = *It;
		}
	}
	for (const FRobberChestDeal& Deal : ChestDeals)
	{
		if (!Deal.Chest)
		{
			continue;
		}
		TArray<int32> Indexes, Values;
		if (Deal.Reward.Kind == ERobberRewardKind::VaultDigits && Vault)
		{
			for (const int32 Index : Deal.Reward.DigitIndexes)
			{
				if (Vault->GetCode().IsValidIndex(Index))
				{
					Indexes.Add(Index);
					Values.Add(Vault->GetCode()[Index]);
				}
			}
		}
		FText CrewMessage;
		if (Deal.Reward.Kind == ERobberRewardKind::GuardDistraction || Deal.Reward.Kind == ERobberRewardKind::ExtraTime)
		{
			FRobberExhibitAssignment AsQuest;
			AsQuest.Reward = Deal.Reward;
			RewardLine(AsQuest, &CrewMessage);
			FFormatNamedArguments Slots;
			Slots.Add(TEXT("giver"), NSLOCTEXT("ExhibitTalk", "ChestGiver", "A chest"));
			CrewMessage = CrewMessage.IsEmpty() ? CrewMessage : ExhibitTalk::Fill(CrewMessage, Slots);
		}
		Deal.Chest->SetContents(Deal.Reward, Deal.JournalClue, CrewMessage, Indexes, Values, ARobberVaultDoor::CodeLength);
	}
}
