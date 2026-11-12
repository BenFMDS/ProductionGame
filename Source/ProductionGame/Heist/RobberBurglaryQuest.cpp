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
	const FName Nixon(TEXT("Nixon")), Lincoln(TEXT("Lincoln")), Washington(TEXT("Washington")), Jfk(TEXT("JFK")), Liberty(TEXT("Liberty"));
	const FName Board(TEXT("PollsBoard")), Tape(TEXT("TapeRoll")), Latch(TEXT("TapedLatch")), Hat(TEXT("LincolnHat")), Reel(TEXT("TapeReel")), Recorder(TEXT("TapeRecorder"));
	const FName Hired(TEXT("Hired")), Reported(TEXT("Reported")), SourceDone(TEXT("SourceDone")), Leaked(TEXT("LeakedIt")), Counted(TEXT("Counted"));
	const FName GeneratorKey(TEXT("GeneratorRoom"));
}

ARobberBurglaryQuest::ARobberBurglaryQuest()
{
	UseScript(TEXT("DT_BurglaryScript"));
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
		UDialogueDefinition* Script = NewScript(SpeakerName(Pair.Key));
		if (Pair.Key == Nixon)
		{
			AddScene(Script, TEXT("Hire"), Hired);
			AddScene(Script, TEXT("Report"), Reported);
			AddSingle(Script, TEXT("WaitHat"), Nixon, Line(TEXT("WaitHat")));
		}
		else if (Pair.Key == Liberty)
		{
			AddScene(Script, TEXT("Source"), SourceDone);
			AddScene(Script, TEXT("Leak"), Leaked);
			AddSingle(Script, TEXT("NotAlone"), Liberty, Line(TEXT("NotAlone")));
		}
		else if (Pair.Key == Washington)
		{
			AddScene(Script, TEXT("CountErased"), Counted);
			AddScene(Script, TEXT("CountLeaked"), Counted);
		}
		// One random line a talk for anyone not on this step: 0 before the hat, 1 after it, 2 Nixon won, 3 Lincoln won.
		const TCHAR* Letter = Pair.Key == Nixon ? TEXT("N") : Pair.Key == Lincoln ? TEXT("L") : Pair.Key == Washington ? TEXT("W") : Pair.Key == Jfk ? TEXT("J") : TEXT("S");
		for (int32 Bucket = 0; Bucket < 4; ++Bucket)
		{
			AddSingle(Script, *FString::Printf(TEXT("Chat%d_0"), Bucket), Pair.Key, Line(*FString::Printf(TEXT("Chat%s%d"), Letter, Bucket)));
		}
		GiveScript(Pair.Value, Script, TEXT("Chat0_0"), { Line(*FString::Printf(TEXT("Chat%s0"), Letter)) })
			->EntryOverride.BindUObject(this, &ARobberBurglaryQuest::EntryFor, Pair.Value.Get());
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
	Stamp(false, Line(TEXT("Name")), Line(TEXT("NewSub")));
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
		PayBonusReward(Line(TEXT("FromNixon")));
		++PayoutsMade;
		SetStage(EBurglaryStage::MeetSource);
		Toast(Line(TEXT("Note")));
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
		Toast(Line(TEXT("KeyToast")));
		Stamp(true, Line(TEXT("Name")), Line(TEXT("DoneSub")));
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
	if (PropId == Tape && Stage == EBurglaryStage::GetTape) return Line(TEXT("PromptTape"));
	if (PropId == Latch && Stage == EBurglaryStage::TapeLatch) return Line(TEXT("PromptLatch"));
	if (PropId == Hat && Stage == EBurglaryStage::PlantBug) return Line(TEXT("PromptHat"));
	if (PropId == Latch && Stage == EBurglaryStage::RemoveTape) return Line(TEXT("PromptUntape"));
	if (PropId == Reel && Stage == EBurglaryStage::FindReel) return Line(TEXT("PromptReel"));
	if (PropId == Recorder && Stage == EBurglaryStage::Decide) return Line(TEXT("PromptErase"));
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
	return Line(Ending == EBurglaryEnding::Erased ? TEXT("NewsErased") : TEXT("NewsLeaked"));
}

void ARobberBurglaryQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace BurglaryQuest;
	if (PropId == Tape)
	{
		Prop->SetShown(false);
		SetStage(EBurglaryStage::TapeLatch);
		Toast(Line(TEXT("GotTape")));
	}
	else if (PropId == Latch && Stage == EBurglaryStage::TapeLatch)
	{
		GetWorldTimerManager().SetTimer(TapeTimer, this, &ARobberBurglaryQuest::OnTapeFound, TapeSeconds, false);
		SetStage(EBurglaryStage::PlantBug);
		Toast(Line(TEXT("Taped")));
	}
	else if (PropId == Hat)
	{
		SetStage(EBurglaryStage::RemoveTape);
		Toast(Line(TEXT("Bugged")));
	}
	else if (PropId == Latch && Stage == EBurglaryStage::RemoveTape)
	{
		GetWorldTimerManager().ClearTimer(TapeTimer);
		SetStage(EBurglaryStage::ReportBack);
		Toast(Line(TEXT("Clean")));
	}
	else if (PropId == Reel)
	{
		Prop->SetShown(false);
		SetStage(EBurglaryStage::Decide);
		Toast(Line(TEXT("GotReel")));
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
		Toast(Line(TEXT("Found")));
	}
	else if (Stage == EBurglaryStage::RemoveTape)
	{
		// The bug's already in, so the job still counts; losing it here would only make the player repeat a finished step.
		SetStage(EBurglaryStage::ReportBack);
		Toast(Line(TEXT("FoundLate")));
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
		Toast(Line(TEXT("Erased")));
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
	Note.Title = Line(TEXT("Name"));
	Note.Footer = FText::Format(Line(TEXT("Foot")), PayoutsMade);
	TArray<FText> Steps;
	for (int32 Step = 0; Step <= 5; ++Step)
	{
		Steps.Add(Line(*FString::Printf(TEXT("L%d"), Step)));
	}
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
		Note.Lines.Add(NoteLine(Steps[DoneCount - 1], true));
	}
	if (Steps.IsValidIndex(DoneCount))
	{
		Note.Lines.Add(NoteLine(Stage == EBurglaryStage::ReportBack ? Line(TEXT("LReport")) : Steps[DoneCount]));
	}
	if (Stage >= EBurglaryStage::GetTape && Stage <= EBurglaryStage::RemoveTape)
	{
		const auto Item = [this](EBurglaryStage Doing, const TCHAR* Key) { return NoteLine(Line(Key), Stage > Doing); };
		Note.ErrandTitle = Line(TEXT("Errand"));
		Note.ErrandLines = { Item(EBurglaryStage::GetTape, TEXT("E0")), Item(EBurglaryStage::TapeLatch, TEXT("E1")),
			Item(EBurglaryStage::PlantBug, TEXT("E2")), Item(EBurglaryStage::RemoveTape, TEXT("E3")) };
	}
	if (Stage == EBurglaryStage::Erasing)
	{
		Note.ErrandTitle = Line(TEXT("ErrandErase"));
		Note.ErrandLines = { NoteLine(FText::Format(Line(TEXT("EErase")), FMath::FloorToInt32(ErasedSeconds))) };
	}
	return Note;
}
