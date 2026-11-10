#include "Heist/RobberBurglaryQuest.h"

#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberGameState.h"
#include "TimerManager.h"

namespace BurglaryQuest
{
	#define BURGLARY_LOC(Key, Text) NSLOCTEXT("BurglaryQuest", Key, Text)

	const FName Nixon(TEXT("Nixon")), Lincoln(TEXT("Lincoln")), Washington(TEXT("Washington")), Jfk(TEXT("JFK")), Liberty(TEXT("Liberty"));
	const FName Board(TEXT("PollsBoard")), Tape(TEXT("TapeRoll")), Latch(TEXT("TapedLatch")), Hat(TEXT("LincolnHat")), Reel(TEXT("TapeReel")), Recorder(TEXT("TapeRecorder"));
	const FName Hired(TEXT("Hired")), Reported(TEXT("Reported")), SourceDone(TEXT("SourceDone")), Leaked(TEXT("LeakedIt")), Counted(TEXT("Counted"));
	const FName GeneratorKey(TEXT("GeneratorRoom"));

	FRobberStoryLine Open(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; return Line; }
	FRobberStoryLine Ticked(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; Line.bDone = true; return Line; }

	FText Called(FName Who)
	{
		if (Who == Nixon) return BURGLARY_LOC("NameNixon", "Richard Nixon");
		if (Who == Lincoln) return BURGLARY_LOC("NameLincoln", "Abraham Lincoln");
		if (Who == Washington) return BURGLARY_LOC("NameWashington", "George Washington");
		if (Who == Jfk) return BURGLARY_LOC("NameJfk", "JFK");
		return BURGLARY_LOC("NameSource", "The Source");
	}

	struct FSaid
	{
		/** None is the player: that line becomes the answer picked on the line before it. */
		FName Who;
		FText Text;
	};
	FSaid Says(FName Who, const FText& Text) { return { Who, Text }; }
	FSaid You(const FText& Text) { return { NAME_None, Text }; }

	FDialogueNode MakeLine(FName Id, FName Who, const FText& Text)
	{
		FDialogueNode Node;
		Node.ID = Id;
		Node.Speaker = Called(Who);
		Node.Text = Text;
		return Node;
	}

	FDialogueChoice MakeAnswer(const FText& Text, FName Next, FName Event = NAME_None)
	{
		FDialogueChoice Choice;
		Choice.Text = Text;
		Choice.NextNode = Next;
		Choice.StoryEvent = Event;
		return Choice;
	}

	void AddScene(UDialogueDefinition* Script, const TCHAR* Prefix, const TArray<FSaid>& Lines, FName LastEvent, const FText& LastAnswer)
	{
		TArray<int32> Spoken;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (!Lines[Index].Who.IsNone())
			{
				Spoken.Add(Index);
			}
		}
		for (int32 Position = 0; Position < Spoken.Num(); ++Position)
		{
			const int32 Index = Spoken[Position];
			FDialogueNode Node = MakeLine(*FString::Printf(TEXT("%s%d"), Prefix, Position + 1), Lines[Index].Who, Lines[Index].Text);
			const bool bHasNext = Position + 1 < Spoken.Num();
			const bool bPlayerAnswers = Lines.IsValidIndex(Index + 1) && Lines[Index + 1].Who.IsNone();
			const FText Answer = bPlayerAnswers ? Lines[Index + 1].Text : (bHasNext ? BURGLARY_LOC("GoOn", "Go on.") : LastAnswer);
			Node.Choices.Add(bHasNext
				? MakeAnswer(Answer, *FString::Printf(TEXT("%s%d"), Prefix, Position + 2))
				: MakeAnswer(Answer, NAME_None, LastEvent));
			Script->Nodes.Add(Node);
		}
	}

	void AddSingle(UDialogueDefinition* Script, FName Id, FName Who, const FText& Text)
	{
		FDialogueNode Node = MakeLine(Id, Who, Text);
		Node.Choices.Add(MakeAnswer(BURGLARY_LOC("Leave", "I'll leave you to it."), NAME_None));
		Script->Nodes.Add(Node);
	}
}

ARobberBurglaryQuest::ARobberBurglaryQuest()
{
	TipLine = BURGLARY_LOC("Tip", "They're voting for the Best President Ever in the {room}. {floor}. Nixon has been asking after some plumbers.");
	TipTakenLine = BURGLARY_LOC("TipTaken", "One of your lot's already seen Nixon about his leak. Best not ask.");
	TipJournal = BURGLARY_LOC("TipJournal", "Nixon, in the {room} ({floor}), wants some plumbers.");
}

void ARobberBurglaryQuest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberBurglaryQuest, Stage);
	DOREPLIFETIME(ARobberBurglaryQuest, Ending);
	DOREPLIFETIME(ARobberBurglaryQuest, PayoutsMade);
}

void ARobberBurglaryQuest::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	using namespace BurglaryQuest;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != GetLevel() || !It->FindComponentByClass<UDialogueComponent>())
		{
			continue;
		}
		for (const FName Name : { Nixon, Lincoln, Washington, Jfk, Liberty })
		{
			if (It->Tags.Contains(Name))
			{
				Talkers.Add(Name, *It);
			}
		}
	}
	BuildDialogues();
	EventHandle = UDialogueComponent::OnDialogueEvent.AddUObject(this, &ARobberBurglaryQuest::OnDialogueEvent);
}

void ARobberBurglaryQuest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDialogueComponent::OnDialogueEvent.Remove(EventHandle);
	GetWorldTimerManager().ClearTimer(TapeTimer);
	GetWorldTimerManager().ClearTimer(EraseTimer);
	Super::EndPlay(EndPlayReason);
}

void ARobberBurglaryQuest::GetPaidKeys(TArray<FName>& OutKeys) const
{
	OutKeys.AddUnique(BurglaryQuest::GeneratorKey);
}

void ARobberBurglaryQuest::GetRequiredActors(TArray<AActor*>& OutActors) const
{
	using namespace BurglaryQuest;
	for (const FName Name : { Nixon, Washington, Liberty })
	{
		if (AActor* Talker = Talkers.FindRef(Name))
		{
			OutActors.Add(Talker);
		}
	}
	for (const FName Id : { Tape, Latch, Hat, Reel, Recorder })
	{
		if (ARobberStoryProp* Prop = FindProp(Id))
		{
			OutActors.Add(Prop);
		}
	}
}

bool ARobberBurglaryQuest::HandlesProp(FName PropId) const
{
	using namespace BurglaryQuest;
	return PropId == Board || PropId == Tape || PropId == Latch || PropId == Hat || PropId == Reel || PropId == Recorder;
}

void ARobberBurglaryQuest::BuildDialogues()
{
	using namespace BurglaryQuest;
	for (const TPair<FName, TObjectPtr<AActor>>& Pair : Talkers)
	{
		UDialogueDefinition* Script = NewObject<UDialogueDefinition>(this);
		Script->DialogueType = EDialogueType::DialogueOnly;
		Script->bIncludeQuestChoices = false;
		Script->SpeakerName = Called(Pair.Key);

		if (Pair.Key == Nixon)
		{
			AddScene(Script, TEXT("Hire"), {
				Says(Nixon, BURGLARY_LOC("Hire1", "Zero votes. I voted for myself twice, and still zero. Somebody in this hall is cheating, and it isn't me yet.")),
				Says(Nixon, BURGLARY_LOC("Hire2", "Plumbers! It says so on the van, and nobody lies on a van. I have a leak.")),
				Says(Nixon, BURGLARY_LOC("Hire3", "I am not a crook. You are. That's why I called.")),
				Says(Nixon, BURGLARY_LOC("Hire4", "Get into Lincoln's campaign office. Bug the hat.")) }, Hired, BURGLARY_LOC("HireEnd", "One bugged hat, coming up."));
			AddScene(Script, TEXT("Report"), {
				Says(Nixon, BURGLARY_LOC("Report1", "Twenty-two points! Four score and seven years ago, I brought forth a nation! Where did I get that? Nobody knows.")) }, Reported, BURGLARY_LOC("ReportEnd", "Nobody knows."));
			AddSingle(Script, TEXT("WaitHat"), Nixon, BURGLARY_LOC("WaitHat", "Get into Lincoln's campaign office. Bug the hat."));
		}
		else if (Pair.Key == Liberty)
		{
			AddScene(Script, TEXT("Source"), {
				Says(Liberty, BURGLARY_LOC("Source1", "I know what you did.")),
				You(BURGLARY_LOC("Source2", "Who are you?")),
				Says(Liberty, BURGLARY_LOC("Source3", "I can't tell you that.")),
				You(BURGLARY_LOC("Source4", "You're holding a torch.")),
				Says(Liberty, BURGLARY_LOC("Source5", "Lots of people hold torches.")),
				You(BURGLARY_LOC("Source6", "You're forty-six metres tall.")),
				Says(Liberty, BURGLARY_LOC("Source7", "Follow the money. There isn't much. It's a museum.")),
				Says(Liberty, BURGLARY_LOC("Source8", "He taped everything. Archive Storage.")) }, SourceDone, BURGLARY_LOC("SourceEnd", "Archive Storage."));
			AddScene(Script, TEXT("Leak"), {
				Says(Liberty, BURGLARY_LOC("Leak1", "You've done the right thing.")),
				You(BURGLARY_LOC("Leak2", "Thanks, Lady Liberty.")),
				Says(Liberty, BURGLARY_LOC("Leak3", "Who?")) }, Leaked, BURGLARY_LOC("LeakEnd", "Nobody. Never mind."));
			AddSingle(Script, TEXT("NotAlone"), Liberty, BURGLARY_LOC("NotAlone", "I can't be seen talking to you. Come back alone."));
		}
		else if (Pair.Key == Washington)
		{
			AddScene(Script, TEXT("CountErased"), {
				Says(Washington, BURGLARY_LOC("Erased1", "I cannot tell a lie. Nixon has received forty-seven votes, and there are forty-five of us.")),
				Says(Nixon, BURGLARY_LOC("Erased2", "Eighteen and a half minutes, gone. A tragic accident. A long, slow, careful accident.")),
				Says(Nixon, BURGLARY_LOC("Erased3", "We have never met.")) }, Counted, BURGLARY_LOC("ErasedEnd", "We have never met."));
			AddScene(Script, TEXT("CountLeaked"), {
				Says(Washington, BURGLARY_LOC("Leaked1", "I cannot tell a lie. Lincoln is the Best President Ever. Nixon is on zero, again.")),
				Says(Nixon, BURGLARY_LOC("Leaked2", "I am not a crook! Ask my plumbers!")),
				Says(Lincoln, BURGLARY_LOC("Leaked3", "Thank you. I should also like my hat back.")),
				Says(Liberty, BURGLARY_LOC("Leaked4", "I was never here.")) }, Counted, BURGLARY_LOC("LeakedEnd", "Never here. Got it."));
		}

		// One random line a talk for anyone not on this step: 0 before the hat, 1 after it, 2 Nixon won, 3 Lincoln won.
		TArray<TArray<FText>> Chatter;
		if (Pair.Key == Nixon)
		{
			Chatter = { { BURGLARY_LOC("ChatN0", "Zero votes. Somebody in this hall is cheating, and it isn't me yet.") },
				{ BURGLARY_LOC("ChatN1", "I am not a crook. You are. That's why I called.") },
				{ BURGLARY_LOC("ChatN2", "We have never met.") },
				{ BURGLARY_LOC("ChatN3", "I am not a crook! Ask my plumbers!") } };
		}
		else if (Pair.Key == Lincoln)
		{
			const FText Bugged = BURGLARY_LOC("ChatL1", "Someone has bugged my hat. It's a very tall hat, sir. There is a great deal of room for bugs.");
			Chatter = { { BURGLARY_LOC("ChatL0", "Four score and seven years ago, I was already ahead in the polls.") }, { Bugged }, { Bugged },
				{ BURGLARY_LOC("ChatL3", "Thank you. I should also like my hat back.") } };
		}
		else if (Pair.Key == Washington)
		{
			const FText Hopeful = BURGLARY_LOC("ChatW0", "I cannot tell a lie. I would very much like to win.");
			Chatter = { { Hopeful }, { Hopeful },
				{ BURGLARY_LOC("ChatW2", "I cannot tell a lie. Nixon has received forty-seven votes, and there are forty-five of us.") },
				{ BURGLARY_LOC("ChatW3", "I cannot tell a lie. Lincoln is the Best President Ever. Nixon is on zero, again.") } };
		}
		else if (Pair.Key == Jfk)
		{
			const FText Hair = BURGLARY_LOC("ChatJ0", "Ask not who you can vote for. Ask who's got the best hair.");
			Chatter = { { Hair }, { Hair }, { Hair }, { Hair } };
		}
		else
		{
			const FText Never = BURGLARY_LOC("ChatS0", "I was never here.");
			Chatter = { { Never }, { Never }, { Never }, { Never } };
		}
		for (int32 Bucket = 0; Bucket < Chatter.Num(); ++Bucket)
		{
			for (int32 Index = 0; Index < Chatter[Bucket].Num(); ++Index)
			{
				AddSingle(Script, *FString::Printf(TEXT("Chat%d_%d"), Bucket, Index), Pair.Key, Chatter[Bucket][Index]);
			}
		}
		Script->EntryNode = TEXT("Chat0_0");
		Scripts.Add(Script);

		UDialogueComponent* Dialogue = Pair.Value->FindComponentByClass<UDialogueComponent>();
		Dialogue->Dialogue = Script;
		Dialogue->SmallTalkLines = Chatter[0];
		Dialogue->OfferedQuests.Reset();
		Dialogue->EntryOverride.BindUObject(this, &ARobberBurglaryQuest::EntryFor, Pair.Value.Get());
	}
}

bool ARobberBurglaryQuest::IsOwnerAlone(const AActor* Near) const
{
	const ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (!State || !Near)
	{
		return true;
	}
	for (const APlayerState* Player : State->PlayerArray)
	{
		const APawn* Pawn = Player && Player != QuestOwner ? Player->GetPawn() : nullptr;
		if (Pawn && FVector::Dist(Pawn->GetActorLocation(), Near->GetActorLocation()) < AloneRadius)
		{
			return false;
		}
	}
	return true;
}

FName ARobberBurglaryQuest::EntryFor(UPlayerQuestComponent* Player, AActor* Talker)
{
	using namespace BurglaryQuest;
	const AController* Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	const bool bOwner = Controller && IsQuestOwner(Controller->PlayerState);
	if (bOwner && Talker == Talkers.FindRef(Nixon))
	{
		if (Stage == EBurglaryStage::MeetNixon) return TEXT("Hire1");
		if (Stage >= EBurglaryStage::GetTape && Stage <= EBurglaryStage::RemoveTape) return TEXT("WaitHat");
		if (Stage == EBurglaryStage::ReportBack) return TEXT("Report1");
	}
	if (bOwner && Talker == Talkers.FindRef(Liberty) && (Stage == EBurglaryStage::MeetSource || Stage == EBurglaryStage::Decide))
	{
		if (!IsOwnerAlone(Talker)) return TEXT("NotAlone");
		return Stage == EBurglaryStage::MeetSource ? TEXT("Source1") : TEXT("Leak1");
	}
	if (bOwner && Talker == Talkers.FindRef(Washington) && Stage == EBurglaryStage::AttendCount)
	{
		return Ending == EBurglaryEnding::Erased ? TEXT("CountErased1") : TEXT("CountLeaked1");
	}
	const int32 Bucket = Stage == EBurglaryStage::Done ? (Ending == EBurglaryEnding::Erased ? 2 : 3) : (Stage >= EBurglaryStage::MeetSource ? 1 : 0);
	return *FString::Printf(TEXT("Chat%d_0"), Bucket);
}

void ARobberBurglaryQuest::SetStage(EBurglaryStage NewStage)
{
	Stage = NewStage;
	ForceNetUpdate();
	RefreshNote();
}

void ARobberBurglaryQuest::OnQuestStarted()
{
	if (Stage == EBurglaryStage::NotStarted)
	{
		Stage = EBurglaryStage::MeetNixon;
		ForceNetUpdate();
	}
	RefreshNote();
	Stamp(false, BURGLARY_LOC("Name", "A Third-Rate Burglary"), BURGLARY_LOC("NewSub", "Five Presidents, one vote and a van that says PLUMBING CO."));
}

void ARobberBurglaryQuest::OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event)
{
	using namespace BurglaryQuest;
	if (Player != OwnerQuests())
	{
		return;
	}
	if (Event == Hired && Stage == EBurglaryStage::MeetNixon)
	{
		SetStage(EBurglaryStage::GetTape);
	}
	else if (Event == Reported && Stage == EBurglaryStage::ReportBack)
	{
		PayBonusReward(BURGLARY_LOC("FromNixon", "Nixon"));
		++PayoutsMade;
		SetStage(EBurglaryStage::MeetSource);
		Toast(BURGLARY_LOC("Note", "ANONYMOUS NOTE · Basement. Come alone."));
	}
	else if (Event == SourceDone && Stage == EBurglaryStage::MeetSource)
	{
		SetStage(EBurglaryStage::FindReel);
	}
	else if (Event == Leaked && Stage == EBurglaryStage::Decide)
	{
		Ending = EBurglaryEnding::Leaked;
		SetStage(EBurglaryStage::AttendCount);
	}
	else if (Event == Counted && Stage == EBurglaryStage::AttendCount)
	{
		Player->GiveKey(GeneratorKey);
		++PayoutsMade;
		SetStage(EBurglaryStage::Done);
		Toast(BURGLARY_LOC("KeyToast", "GENERATOR ROOM KEY · Left in the ballot box. Nobody is asking why."));
		Stamp(true, BURGLARY_LOC("Name", "A Third-Rate Burglary"), BURGLARY_LOC("DoneSub", "The people have spoken."));
		ClearNoteAfterStamp();
	}
}

FText ARobberBurglaryQuest::GetPropPrompt(FName PropId, const APlayerState* Viewer) const
{
	using namespace BurglaryQuest;
	if (!IsQuestOwner(Viewer))
	{
		return FText::GetEmpty();
	}
	if (PropId == Tape && Stage == EBurglaryStage::GetTape) return BURGLARY_LOC("PromptTape", "Take the roll of tape");
	if (PropId == Latch && Stage == EBurglaryStage::TapeLatch) return BURGLARY_LOC("PromptLatch", "Tape the latch");
	if (PropId == Hat && Stage == EBurglaryStage::PlantBug) return BURGLARY_LOC("PromptHat", "Plant the bug in the hat");
	if (PropId == Latch && Stage == EBurglaryStage::RemoveTape) return BURGLARY_LOC("PromptUntape", "Take the tape off");
	if (PropId == Reel && Stage == EBurglaryStage::FindReel) return BURGLARY_LOC("PromptReel", "Take the reel");
	if (PropId == Recorder && Stage == EBurglaryStage::Decide) return BURGLARY_LOC("PromptErase", "Erase the tape");
	return FText::GetEmpty();
}

void ARobberBurglaryQuest::GetPolls(TArray<int32>& OutPolls, bool& bFinal) const
{
	bFinal = Stage == EBurglaryStage::Done;
	if (bFinal)
	{
		OutPolls = Ending == EBurglaryEnding::Erased ? TArray<int32>{ 11, 9, 4, 2, 47 } : TArray<int32>{ 41, 36, 14, 9, 0 };
	}
	else if (Stage >= EBurglaryStage::MeetSource)
	{
		OutPolls = { 28, 26, 14, 10, 22 };
	}
	else
	{
		OutPolls = { 34, 33, 19, 14, 0 };
	}
}

FText ARobberBurglaryQuest::GetPropLabel(FName PropId) const
{
	if (PropId != BurglaryQuest::Board)
	{
		return FText::GetEmpty();
	}
	TArray<int32> Polls;
	bool bFinal = false;
	GetPolls(Polls, bFinal);
	return FText::FromString(FString::Printf(TEXT("%s\nLINCOLN %d\nWASHINGTON %d\nROOSEVELT %d\nJFK %d\nNIXON %d"),
		bFinal ? TEXT("FINAL RESULT") : TEXT("BEST PRESIDENT EVER"), Polls[0], Polls[1], Polls[2], Polls[3], Polls[4]));
}

FText ARobberBurglaryQuest::GetHeadlineOverride() const
{
	if (Stage != EBurglaryStage::Done)
	{
		return FText::GetEmpty();
	}
	return Ending == EBurglaryEnding::Erased ? BURGLARY_LOC("NewsErased", "NIXON \"BEST EVER\", SAYS NIXON") : BURGLARY_LOC("NewsLeaked", "PRESIDENT RESIGNS FROM PLINTH");
}

void ARobberBurglaryQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace BurglaryQuest;
	if (PropId == Tape)
	{
		Prop->SetShown(false);
		SetStage(EBurglaryStage::TapeLatch);
		Toast(BURGLARY_LOC("GotTape", "ROLL OF TAPE · Now the latch on the Coat Check door."));
	}
	else if (PropId == Latch && Stage == EBurglaryStage::TapeLatch)
	{
		GetWorldTimerManager().SetTimer(TapeTimer, this, &ARobberBurglaryQuest::OnTapeFound, TapeSeconds, false);
		SetStage(EBurglaryStage::PlantBug);
		Toast(BURGLARY_LOC("Taped", "THE LATCH IS TAPED · Be quick. The next guard round will spot it."));
	}
	else if (PropId == Hat)
	{
		SetStage(EBurglaryStage::RemoveTape);
		Toast(BURGLARY_LOC("Bugged", "BUG PLANTED · It's a very tall hat. Now take the tape off."));
	}
	else if (PropId == Latch && Stage == EBurglaryStage::RemoveTape)
	{
		GetWorldTimerManager().ClearTimer(TapeTimer);
		SetStage(EBurglaryStage::ReportBack);
		Toast(BURGLARY_LOC("Clean", "CLEAN JOB · Nobody saw a thing. Report back to Nixon."));
	}
	else if (PropId == Reel)
	{
		Prop->SetShown(false);
		SetStage(EBurglaryStage::Decide);
		Toast(BURGLARY_LOC("GotReel", "THE TAPE · One reel, labelled DO NOT LISTEN, in Nixon's handwriting. Erase it, or take it to the Source."));
	}
	else if (PropId == Recorder)
	{
		ErasedSeconds = 0.f;
		GetWorldTimerManager().SetTimer(EraseTimer, this, &ARobberBurglaryQuest::TickErase, 0.25f, true);
		SetStage(EBurglaryStage::Erasing);
	}
}

void ARobberBurglaryQuest::OnTapeFound()
{
	ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (State)
	{
		State->AddHeat(TapeFoundHeat, TEXT("Taped latch"));
	}
	if (Stage == EBurglaryStage::PlantBug)
	{
		SetStage(EBurglaryStage::TapeLatch);
		Toast(BURGLARY_LOC("Found", "THE GUARD FOUND THE TAPE · The door's locked again. Tape it again."));
	}
	else if (Stage == EBurglaryStage::RemoveTape)
	{
		// The bug's already in, so the job still counts; losing it here would only make the player repeat a finished step.
		SetStage(EBurglaryStage::ReportBack);
		Toast(BURGLARY_LOC("FoundLate", "THE GUARD FOUND THE TAPE · Too late to matter, but they're looking now. Report back to Nixon."));
	}
}

void ARobberBurglaryQuest::TickErase()
{
	const APawn* Pawn = IsValid(QuestOwner) ? QuestOwner->GetPawn() : nullptr;
	const ARobberStoryProp* Machine = FindProp(BurglaryQuest::Recorder);
	if (!Pawn || !Machine || FVector::Dist(Pawn->GetActorLocation(), Machine->GetActorLocation()) > RecorderReach)
	{
		// Walking off pauses it. The owner keeps their progress and only has to come back.
		return;
	}
	const int32 Before = FMath::FloorToInt32(ErasedSeconds);
	ErasedSeconds = FMath::Min(EraseSeconds, ErasedSeconds + 0.25f);
	if (ErasedSeconds >= EraseSeconds)
	{
		GetWorldTimerManager().ClearTimer(EraseTimer);
		Ending = EBurglaryEnding::Erased;
		SetStage(EBurglaryStage::AttendCount);
		Toast(BURGLARY_LOC("Erased", "ERASED · A long, slow, careful accident. Go and hear the count."));
	}
	else if (FMath::FloorToInt32(ErasedSeconds) != Before)
	{
		RefreshNote();
	}
}

FRobberStoryNote ARobberBurglaryQuest::BuildNote() const
{
	using namespace BurglaryQuest;
	FRobberStoryNote Note;
	Note.bShown = true;
	Note.Title = BURGLARY_LOC("Name", "A Third-Rate Burglary");
	Note.Footer = FText::Format(BURGLARY_LOC("Foot", "Payouts: {0} of 2"), PayoutsMade);
	const TArray<FText> Steps = {
		BURGLARY_LOC("L0", "See the man about the leak"), BURGLARY_LOC("L1", "Bug Lincoln's campaign office"),
		BURGLARY_LOC("L2", "Meet the source in the Basement"), BURGLARY_LOC("L3", "Find the missing tape"),
		BURGLARY_LOC("L4", "Erase it, or leak it"), BURGLARY_LOC("L5", "Attend the count") };
	int32 DoneCount = 0;
	switch (Stage)
	{
	case EBurglaryStage::GetTape: case EBurglaryStage::TapeLatch: case EBurglaryStage::PlantBug: case EBurglaryStage::RemoveTape: DoneCount = 1; break;
	case EBurglaryStage::ReportBack: DoneCount = 1; break;
	case EBurglaryStage::MeetSource: DoneCount = 2; break;
	case EBurglaryStage::FindReel: DoneCount = 3; break;
	case EBurglaryStage::Decide: case EBurglaryStage::Erasing: DoneCount = 4; break;
	case EBurglaryStage::AttendCount: DoneCount = 5; break;
	case EBurglaryStage::Done: DoneCount = 6; break;
	default: break;
	}
	if (DoneCount > 0)
	{
		Note.Lines.Add(Ticked(Steps[DoneCount - 1]));
	}
	if (Steps.IsValidIndex(DoneCount))
	{
		Note.Lines.Add(Open(Stage == EBurglaryStage::ReportBack ? BURGLARY_LOC("LReport", "Report back to Nixon") : Steps[DoneCount]));
	}
	if (Stage >= EBurglaryStage::GetTape && Stage <= EBurglaryStage::RemoveTape)
	{
		const auto Item = [this](EBurglaryStage Doing, const FText& Text) { return Stage > Doing ? Ticked(Text) : Open(Text); };
		Note.ErrandTitle = BURGLARY_LOC("Errand", "The Plumbers");
		Note.ErrandLines = {
			Item(EBurglaryStage::GetTape, BURGLARY_LOC("E0", "Get some tape from the Boiler Room")),
			Item(EBurglaryStage::TapeLatch, BURGLARY_LOC("E1", "Tape the latch on the Coat Check door")),
			Item(EBurglaryStage::PlantBug, BURGLARY_LOC("E2", "Plant the bug in Lincoln's hat")),
			Item(EBurglaryStage::RemoveTape, BURGLARY_LOC("E3", "Take the tape off on your way out")) };
	}
	if (Stage == EBurglaryStage::Erasing)
	{
		Note.ErrandTitle = BURGLARY_LOC("ErrandErase", "The 18½ Seconds");
		Note.ErrandLines = { Open(FText::Format(BURGLARY_LOC("EErase", "Stay at the recorder: {0} of 18.5"), FMath::FloorToInt32(ErasedSeconds))) };
	}
	return Note;
}
