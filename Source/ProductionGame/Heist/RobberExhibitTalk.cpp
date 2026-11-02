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
	// A streamed level's actors can begin play over several frames, and the deal needs every exhibit and the vault's code.
	GetWorldTimerManager().SetTimer(DealTimer, this, &ARobberNightDirector::TryDeal, 0.1f, true);
}

void ARobberNightDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UPlayerQuestComponent::OnQuestAccepted.Remove(AcceptedHandle);
	UPlayerQuestComponent::OnQuestTurnedIn.Remove(TurnedInHandle);
	UDialogueComponent::OnConversationStarted.Remove(ConversationHandle);
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
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != GetLevel())
		{
			continue;
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
	if (Pay.Num() > Givers.Num())
	{
		UE_LOG(LogExhibitTalk, Error, TEXT("%d required rewards but only %d quests tonight."), Pay.Num(), Givers.Num());
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
	while (Pay.Num() < Givers.Num() && TotalWeight > 0.f)
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
		return HaveAll(KeysNeededAt(Each.Exhibit)) && HaveAll(KeysNeededAt(Each.Target)) && HaveAll(KeysNeededAt(Each.Spot));
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
	const ARobberRoomInfo* Room = Each.Spot ? FindRoom(Each.Spot->GetActorLocation()) : Each.Target ? FindRoom(Each.Target->GetActorLocation()) : nullptr;
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
