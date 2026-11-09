#include "Heist/RobberMummyQuest.h"

#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberExhibitTalk.h"
#include "Net/UnrealNetwork.h"
#include "ProductionGameCharacter.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

const FName ARobberMummyQuest::DeskGuard(TEXT("DeskGuard"));

namespace MummyQuest
{
	#define MUMMY_LOC(Key, Text) NSLOCTEXT("MummyQuest", Key, Text)

	const FName Tag(TEXT("Neferkhaos"));
	const FName Ballast(TEXT("Ballast")), Lungs(TEXT("Lungs")), Stomach(TEXT("Stomach")), Liver(TEXT("Liver")), Sausages(TEXT("Sausages")), Cabinet(TEXT("Cabinet")), HeadJar(TEXT("HeadJar"));
	const FName Crate1(TEXT("Crate1")), Crate2(TEXT("Crate2")), HeartCrate(TEXT("Crate3")), Crate4(TEXT("Crate4")), Crate5(TEXT("Crate5"));
	const FName FirstDone(TEXT("FirstDone")), LungsDone(TEXT("LungsDone")), StomachDone(TEXT("StomachDone")), LiverDone(TEXT("LiverDone")), HeartDone(TEXT("HeartDone")), SausagesDone(TEXT("SausagesDone")), HeadDone(TEXT("HeadDone"));

	FName SlotId(int32 Slot) { return *FString::Printf(TEXT("Slot%d"), Slot); }
	bool IsCrate(FName Id) { return Id == Crate1 || Id == Crate2 || Id == HeartCrate || Id == Crate4 || Id == Crate5; }

	FRobberStoryLine OpenLine(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; return Line; }
	FRobberStoryLine DoneLine(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; Line.bDone = true; return Line; }

	FText Speaker() { return MUMMY_LOC("Speaker", "Neferkhaos"); }
	FText GoOn() { return MUMMY_LOC("GoOn", "Go on."); }

	struct FLine
	{
		bool bYou;
		FText Text;
	};
	FLine Him(const FText& Text) { return { false, Text }; }
	FLine You(const FText& Text) { return { true, Text }; }

	FDialogueNode MakeNode(FName Id, const FText& Text)
	{
		FDialogueNode Node;
		Node.ID = Id;
		Node.Speaker = Speaker();
		Node.Text = Text;
		return Node;
	}

	FDialogueChoice MakeChoice(const FText& Text, FName Next, FName Event = NAME_None)
	{
		FDialogueChoice Choice;
		Choice.Text = Text;
		Choice.NextNode = Next;
		Choice.StoryEvent = Event;
		return Choice;
	}

	/** Builds one scene. A "You" line becomes the answer the player picks on the line before it. */
	void AddTalk(UDialogueDefinition* Dialogue, const TCHAR* Prefix, const TArray<FLine>& Lines, FName LastEvent, const FText& LastChoice)
	{
		TArray<int32> Spoken;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (!Lines[Index].bYou)
			{
				Spoken.Add(Index);
			}
		}
		for (int32 Position = 0; Position < Spoken.Num(); ++Position)
		{
			const int32 Index = Spoken[Position];
			FDialogueNode Node = MakeNode(*FString::Printf(TEXT("%s%d"), Prefix, Position + 1), Lines[Index].Text);
			const bool bHasNext = Position + 1 < Spoken.Num();
			const FText Answer = Lines.IsValidIndex(Index + 1) && Lines[Index + 1].bYou ? Lines[Index + 1].Text : (bHasNext ? GoOn() : LastChoice);
			Node.Choices.Add(bHasNext
				? MakeChoice(Answer, *FString::Printf(TEXT("%s%d"), Prefix, Position + 2))
				: MakeChoice(Answer, NAME_None, LastEvent));
			Dialogue->Nodes.Add(Node);
		}
	}

	/** Which "everyone else" lines apply, by how much of him there is. */
	int32 ChatBucket(EMummyStage Stage)
	{
		if (Stage == EMummyStage::Done) return 5;
		if (Stage >= EMummyStage::FetchSausages) return 4;
		if (Stage >= EMummyStage::FetchHeart) return 3;
		if (Stage >= EMummyStage::FetchLiver) return 2;
		if (Stage >= EMummyStage::FetchStomach) return 1;
		return 0;
	}
}

ARobberMummyQuest::ARobberMummyQuest()
{
	TipLine = MUMMY_LOC("Tip", "The mummy in the {room} has been muttering at a scroll for three thousand years. {floor}. Nobody knows why. Nobody has asked.");
	TipTakenLine = MUMMY_LOC("TipTaken", "Somebody is already helping the mummy. Heaven help them.");
	TipJournal = MUMMY_LOC("TipJournal", "The mummy in the {room} ({floor}) has been muttering at a scroll.");
}

void ARobberMummyQuest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberMummyQuest, Stage);
	DOREPLIFETIME(ARobberMummyQuest, bHasBallast);
	DOREPLIFETIME(ARobberMummyQuest, bCabinetOpen);
	DOREPLIFETIME(ARobberMummyQuest, OpenedCrates);
	DOREPLIFETIME(ARobberMummyQuest, PayoutsMade);
}

void ARobberMummyQuest::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel() && It->Tags.Contains(MummyQuest::Tag) && It->FindComponentByClass<UDialogueComponent>())
		{
			Mummy = *It;
			break;
		}
	}
	if (Mummy)
	{
		BuildDialogue();
	}
	EventHandle = UDialogueComponent::OnDialogueEvent.AddUObject(this, &ARobberMummyQuest::OnDialogueEvent);
}

void ARobberMummyQuest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDialogueComponent::OnDialogueEvent.Remove(EventHandle);
	Super::EndPlay(EndPlayReason);
}

void ARobberMummyQuest::GetRequiredActors(TArray<AActor*>& OutActors) const
{
	using namespace MummyQuest;
	if (Mummy)
	{
		OutActors.Add(Mummy);
	}
	for (const FName Id : { Ballast, Lungs, Stomach, Liver, HeartCrate, Sausages, Cabinet })
	{
		if (ARobberStoryProp* Prop = FindProp(Id))
		{
			OutActors.Add(Prop);
		}
	}
}

bool ARobberMummyQuest::HandlesProp(FName PropId) const
{
	using namespace MummyQuest;
	if (IsCrate(PropId) || PropId == Ballast || PropId == Lungs || PropId == Stomach || PropId == Liver || PropId == Sausages || PropId == Cabinet || PropId == HeadJar)
	{
		return true;
	}
	return PropId.ToString().StartsWith(TEXT("Slot"));
}

void ARobberMummyQuest::BuildDialogue()
{
	using namespace MummyQuest;
	Script = NewObject<UDialogueDefinition>(this);
	Script->DialogueType = EDialogueType::DialogueOnly;
	Script->bIncludeQuestChoices = false;
	Script->SpeakerName = Speaker();

	AddTalk(Script, TEXT("First"), {
		Him(MUMMY_LOC("First1", "Mmmf. Mmf mm MMMF.")),
		You(MUMMY_LOC("First2", "Sorry?")),
		Him(MUMMY_LOC("First3", "MMMMMF.")) }, FirstDone, MUMMY_LOC("FirstEnd", "...Is that a scroll?"));

	AddTalk(Script, TEXT("Lungs"), {
		Him(MUMMY_LOC("Lungs1", "WHY.")),
		Him(MUMMY_LOC("Lungs2", "AM.")),
		Him(MUMMY_LOC("Lungs3", "I.")),
		Him(MUMMY_LOC("Lungs4", "SHOUTING.")),
		You(MUMMY_LOC("Lungs5", "Those are whale lungs.")),
		Him(MUMMY_LOC("Lungs6", "THE. SCROLL. SAID. BREATH.")),
		Him(MUMMY_LOC("Lungs7", "NEXT.")) }, LungsDone, MUMMY_LOC("LungsEnd", "Four chambers. On it."));

	AddTalk(Script, TEXT("Stomach"), {
		Him(MUMMY_LOC("Stomach1", "Oh. Oh, that is better. I can do sentences. I can also feel something moving. Several somethings.")),
		Him(MUMMY_LOC("Stomach2", "Four chambers. I took that for a metaphor. I am hungry in four separate ways, and one of them wants grass.")),
		Him(MUMMY_LOC("Stomach3", "Three thousand years. \"Shortly\", they said. You never pay a priest for what you can do yourself.")),
		Him(MUMMY_LOC("Stomach4", "For your trouble. I was buried with considerable wealth, and very little of it was mine.")),
		Him(MUMMY_LOC("Stomach5", "Now, a thing of great age. Off you go.")) }, StomachDone, MUMMY_LOC("StomachEnd", "Off I go."));

	AddTalk(Script, TEXT("Liver"), {
		Him(MUMMY_LOC("Liver1", "What is this?")),
		You(MUMMY_LOC("Liver2", "It's a liver.")),
		Him(MUMMY_LOC("Liver3", "It's a rock.")),
		You(MUMMY_LOC("Liver4", "It's a very old liver.")),
		Him(MUMMY_LOC("Liver5", "This thing is older than me, for Ra's sake. But I guess it will do.")),
		Him(MUMMY_LOC("Liver6", "The priests would have said it was a rock. That's the difference between a professional and a man who cares.")),
		Him(MUMMY_LOC("Liver7", "Now, my heart. It is in storage, apparently, like a lawnmower.")) }, LiverDone, MUMMY_LOC("LiverEnd", "Like a lawnmower. Got it."));

	AddTalk(Script, TEXT("Heart"), {
		Him(MUMMY_LOC("Heart1", "My own heart. No substitutes on this one. I checked.")),
		Him(MUMMY_LOC("Heart2", "It was weighed against a feather once. It lost. Please don't tell anyone.")),
		Him(MUMMY_LOC("Heart3", "I feel something. What is this? Is this sadness? Take it out. No. Put it back. Do not look at me.")),
		Him(MUMMY_LOC("Heart4", "Thank you. I have never said that before. Do not get used to it.")),
		Him(MUMMY_LOC("Heart5", "The next line says bowels. I have read ahead on this bit. Absolutely not. It is a very small sarcophagus, it has no windows, and I have to sleep in it.")),
		Him(MUMMY_LOC("Heart6", "We need something that counts as bowels.")),
		You(MUMMY_LOC("Heart7", "I might know something that would work.")),
		Him(MUMMY_LOC("Heart8", "Does it smell?")),
		You(MUMMY_LOC("Heart9", "Only of breakfast.")) }, HeartDone, MUMMY_LOC("HeartEnd", "Only of breakfast."));

	AddTalk(Script, TEXT("Sausages"), {
		Him(MUMMY_LOC("Sausages1", "These are sausages. I can tell. I am choosing not to ask. The scroll is silent on sausages, and I intend to keep it that way.")),
		Him(MUMMY_LOC("Sausages2", "I feel at peace. I feel seasoned.")),
		Him(MUMMY_LOC("Sausages3", "The last line just says: a head. They pulled my own out through my nose, so we'll be needing a spare.")) }, SausagesDone, MUMMY_LOC("SausagesEnd", "One head. Sure."));

	AddTalk(Script, TEXT("Head"), {
		Him(MUMMY_LOC("Head1", "It says Head. It says it right there on the jar. That's the ritual satisfied.")),
		Him(MUMMY_LOC("Head2", "By the power of Ra, and the four chambers, and the sausages: I RISE!")),
		Him(MUMMY_LOC("Head3", "I live! Three thousand years, and I did it myself! Never pay a priest!")),
		Him(MUMMY_LOC("Head4", "I can see everything. I know everything. I know...")),
		Him(MUMMY_LOC("Head5", "...that it's nearly nine. I'm due at the Loading Dock.")),
		Him(MUMMY_LOC("Head6", "I have sixteen crates to sign for.")),
		Him(MUMMY_LOC("Head7", "Your wages. Sign here. And here. Initial here.")) }, HeadDone, MUMMY_LOC("HeadEnd", "...Initialled."));

	const FText Leave = MUMMY_LOC("Leave", "I'll get on with it.");
	const TPair<FName, FText> Waits[] = {
		{ TEXT("WaitLungs"), MUMMY_LOC("WaitLungs", "MMMF.") },
		{ TEXT("WaitStomach"), MUMMY_LOC("WaitStomach", "CHAMBERS.") },
		{ TEXT("WaitLiver"), MUMMY_LOC("WaitLiver", "I did all my own embalming prep, you know. Well. I watched.") },
		{ TEXT("WaitHeart"), MUMMY_LOC("WaitHeart", "It's in Art Crate Storage. The number's on the scroll. I wrote it there myself, in pencil, in 1300 BC.") },
		{ TEXT("WaitSausages"), MUMMY_LOC("WaitSausages", "Whatever it is you had in mind, I would like it before my sarcophagus is any smaller.") },
		{ TEXT("WaitHead"), MUMMY_LOC("WaitHead", "A head. Any head. I'm not fussy. The scroll isn't fussy.") },
	};
	for (const TPair<FName, FText>& Wait : Waits)
	{
		FDialogueNode Node = MakeNode(Wait.Key, Wait.Value);
		Node.Choices.Add(MakeChoice(Leave, NAME_None));
		Script->Nodes.Add(Node);
	}

	// One random line a talk, for anyone who isn't on the quest.
	const TArray<TArray<FText>> Chatter = {
		{ MUMMY_LOC("Chat0A", "Mmmf."), MUMMY_LOC("Chat0B", "Mmf mm.") },
		{ MUMMY_LOC("Chat1A", "BUSY."), MUMMY_LOC("Chat1B", "RITUAL.") },
		{ MUMMY_LOC("Chat2A", "Are you the sandwich? The scroll didn't mention a sandwich, but I am open to it.") },
		{ MUMMY_LOC("Chat3A", "Amateurs, the lot of them. I don't mean you. Yes I do.") },
		{ MUMMY_LOC("Chat4A", "I have feelings now. Yours are not among them.") },
		{ MUMMY_LOC("Chat5A", "Can't stop. Loading Dock. Mind the crates.") },
	};
	for (int32 Bucket = 0; Bucket < Chatter.Num(); ++Bucket)
	{
		for (int32 Index = 0; Index < Chatter[Bucket].Num(); ++Index)
		{
			FDialogueNode Node = MakeNode(*FString::Printf(TEXT("Chat%d_%d"), Bucket, Index), Chatter[Bucket][Index]);
			Node.Choices.Add(MakeChoice(MUMMY_LOC("Bye", "I'll leave you to it."), NAME_None));
			Script->Nodes.Add(Node);
		}
	}
	Script->EntryNode = TEXT("Chat0_0");

	UDialogueComponent* Talker = Mummy->FindComponentByClass<UDialogueComponent>();
	Talker->Dialogue = Script;
	Talker->OfferedQuests.Reset();
	Talker->EntryOverride.BindUObject(this, &ARobberMummyQuest::EntryFor, Mummy.Get());
}

FName ARobberMummyQuest::EntryFor(UPlayerQuestComponent* Player, AActor* Talker)
{
	using namespace MummyQuest;
	const AController* Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	if (Controller && IsQuestOwner(Controller->PlayerState))
	{
		switch (Stage)
		{
		case EMummyStage::FirstTalk: return TEXT("First1");
		case EMummyStage::FetchLungs: return TEXT("WaitLungs");
		case EMummyStage::BringLungs: return TEXT("Lungs1");
		case EMummyStage::FetchStomach: return TEXT("WaitStomach");
		case EMummyStage::BringStomach: return TEXT("Stomach1");
		case EMummyStage::FetchLiver: return TEXT("WaitLiver");
		case EMummyStage::BringLiver: return TEXT("Liver1");
		case EMummyStage::FetchHeart: return TEXT("WaitHeart");
		case EMummyStage::BringHeart: return TEXT("Heart1");
		case EMummyStage::FetchSausages: return TEXT("WaitSausages");
		case EMummyStage::BringSausages: return TEXT("Sausages1");
		case EMummyStage::FetchHead: return TEXT("WaitHead");
		case EMummyStage::BringHead: return TEXT("Head1");
		default: break;
		}
	}
	const int32 Bucket = ChatBucket(Stage);
	int32 Count = 0;
	while (Script->FindNode(*FString::Printf(TEXT("Chat%d_%d"), Bucket, Count)))
	{
		++Count;
	}
	int32 Line = FMath::RandHelper(FMath::Max(1, Count));
	if (const int32* Last = LastChatLine.Find(Bucket); Last && Count > 1 && *Last == Line)
	{
		Line = (Line + 1) % Count;
	}
	LastChatLine.Add(Bucket, Line);
	return *FString::Printf(TEXT("Chat%d_%d"), Bucket, Line);
}

void ARobberMummyQuest::SetStage(EMummyStage NewStage)
{
	Stage = NewStage;
	ForceNetUpdate();
	RefreshNote();
}

void ARobberMummyQuest::SetCarrying(float SpeedMultiplier)
{
	if (AProductionGameCharacter* Character = IsValid(QuestOwner) ? Cast<AProductionGameCharacter>(QuestOwner->GetPawn()) : nullptr)
	{
		Character->SetCarrySpeed(SpeedMultiplier);
	}
}

void ARobberMummyQuest::ShowFitted(int32 Slot)
{
	if (ARobberStoryProp* Jar = FindProp(MummyQuest::SlotId(Slot)))
	{
		Jar->SetShown(true);
	}
	SetCarrying(1.f);
}

void ARobberMummyQuest::PayOut()
{
	ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (!State)
	{
		return;
	}
	for (TActorIterator<ARobberNightDirector> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != GetLevel())
		{
			continue;
		}
		const FRobberReward Reward = It->DrawBonusReward();
		++PayoutsMade;
		ForceNetUpdate();
		if (Reward.Kind == ERobberRewardKind::ExtraTime)
		{
			State->AddGuardsTime(Reward.Value);
			State->TellCrew(FText::Format(MUMMY_LOC("PaidTime", "PAYOUT · The mummy has delayed the guards by {0} seconds."), FMath::RoundToInt32(Reward.Value)));
		}
		else
		{
			State->CoolHeat(Reward.Value);
			State->TellCrew(FText::Format(MUMMY_LOC("PaidHeat", "PAYOUT · The mummy had a word. Heat is down by {0}."), FMath::RoundToInt32(Reward.Value)));
		}
		return;
	}
}

void ARobberMummyQuest::OnQuestStarted()
{
	if (Stage == EMummyStage::NotStarted)
	{
		Stage = EMummyStage::FirstTalk;
		ForceNetUpdate();
	}
	RefreshNote();
	Stamp(false, MUMMY_LOC("Name", "Some Assembly Required"), MUMMY_LOC("NewSub", "Resurrection: now a do-it-yourself job."));
}

void ARobberMummyQuest::OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event)
{
	using namespace MummyQuest;
	if (Talker != Mummy || Player != OwnerQuests())
	{
		return;
	}
	if (Event == FirstDone && Stage == EMummyStage::FirstTalk)
	{
		SetStage(EMummyStage::FetchLungs);
		Toast(MUMMY_LOC("GotScroll", "RITUAL SCROLL · It's a shopping list. Margin note: \"Shortly, they said.\""));
	}
	else if (Event == LungsDone && Stage == EMummyStage::BringLungs)
	{
		ShowFitted(1);
		SetStage(EMummyStage::FetchStomach);
	}
	else if (Event == StomachDone && Stage == EMummyStage::BringStomach)
	{
		ShowFitted(2);
		PayOut();
		SetStage(EMummyStage::FetchLiver);
	}
	else if (Event == LiverDone && Stage == EMummyStage::BringLiver)
	{
		ShowFitted(3);
		SetStage(EMummyStage::FetchHeart);
	}
	else if (Event == HeartDone && Stage == EMummyStage::BringHeart)
	{
		ShowFitted(4);
		SetStage(EMummyStage::FetchSausages);
	}
	else if (Event == SausagesDone && Stage == EMummyStage::BringSausages)
	{
		ShowFitted(5);
		SetStage(EMummyStage::FetchHead);
	}
	else if (Event == HeadDone && Stage == EMummyStage::BringHead)
	{
		ShowFitted(6);
		PayOut();
		SetStage(EMummyStage::Done);
		Stamp(true, MUMMY_LOC("Name", "Some Assembly Required"), MUMMY_LOC("DoneSub", "He has risen. He is due at the Loading Dock."));
		ClearNoteAfterStamp();
	}
}

FText ARobberMummyQuest::GetPropPrompt(FName PropId, const APlayerState* Viewer) const
{
	using namespace MummyQuest;
	if (!IsQuestOwner(Viewer))
	{
		return FText::GetEmpty();
	}
	if (PropId == Ballast && Stage == EMummyStage::FetchLungs && !bHasBallast) return MUMMY_LOC("PromptSand", "Pick up the sandbag");
	if (PropId == Lungs && Stage == EMummyStage::FetchLungs) return bHasBallast ? MUMMY_LOC("PromptSwap", "Swap the sandbag for the jar") : MUMMY_LOC("PromptLift", "Lift the jar");
	if (PropId == Stomach && Stage == EMummyStage::FetchStomach) return MUMMY_LOC("PromptStomach", "Take the cow's stomach");
	if (PropId == Liver && Stage == EMummyStage::FetchLiver) return MUMMY_LOC("PromptLiver", "Take the paperweight");
	if (IsCrate(PropId) && Stage == EMummyStage::FetchHeart && !OpenedCrates.Contains(PropId)) return MUMMY_LOC("PromptCrate", "Open the crate");
	if (PropId == Sausages && Stage == EMummyStage::FetchSausages) return MUMMY_LOC("PromptSausages", "Take the string of sausages");
	if (PropId == Cabinet && Stage == EMummyStage::FetchHead && !bCabinetOpen) return MUMMY_LOC("PromptCabinet", "Try the code pencilled on the scroll");
	if (PropId == HeadJar && Stage == EMummyStage::FetchHead && bCabinetOpen) return MUMMY_LOC("PromptJar", "Take the jar");
	return FText::GetEmpty();
}

void ARobberMummyQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace MummyQuest;
	if (PropId == Ballast)
	{
		bHasBallast = true;
		Prop->SetShown(false);
		ForceNetUpdate();
		RefreshNote();
		Toast(MUMMY_LOC("GotSand", "SANDBAG · About the weight of some very large lungs."));
	}
	else if (PropId == Lungs)
	{
		if (!bHasBallast)
		{
			Toast(MUMMY_LOC("NeedSand", "PRESSURE PLATE · You'll need something heavy to swap in."));
			return;
		}
		Prop->SetShown(false);
		SetCarrying(0.55f);
		SetStage(EMummyStage::BringLungs);
		Toast(MUMMY_LOC("GotLungs", "BREATH OF THE SEA · Extremely heavy. Carry it to the mummy."));
	}
	else if (PropId == Stomach)
	{
		Prop->SetShown(false);
		SetCarrying(0.8f);
		SetStage(EMummyStage::BringStomach);
		Toast(MUMMY_LOC("GotStomach", "FOUR CHAMBERS · Frozen solid. Those sausages look interesting."));
	}
	else if (PropId == Liver)
	{
		Prop->SetShown(false);
		SetCarrying(0.8f);
		SetStage(EMummyStage::BringLiver);
		Toast(MUMMY_LOC("GotLiver", "A THING OF GREAT AGE · It's a rock. The label says liver."));
	}
	else if (IsCrate(PropId))
	{
		OpenedCrates.AddUnique(PropId);
		ForceNetUpdate();
		if (PropId == HeartCrate)
		{
			SetCarrying(0.9f);
			SetStage(EMummyStage::BringHeart);
			Toast(MUMMY_LOC("GotHeart", "CRATE 1127 · One heart, lightly used."));
		}
		else
		{
			RefreshNote();
			Toast(MUMMY_LOC("WrongCrate", "WRONG CRATE · Straw. The number's on the scroll."));
		}
	}
	else if (PropId == Sausages)
	{
		Prop->SetShown(false);
		SetCarrying(0.9f);
		SetStage(EMummyStage::BringSausages);
		Toast(MUMMY_LOC("GotSausages", "THE BOWELS OF THE ONE WHO RISES · Near enough."));
	}
	else if (PropId == Cabinet)
	{
		bCabinetOpen = true;
		ForceNetUpdate();
		if (ARobberStoryProp* Jar = FindProp(HeadJar))
		{
			Jar->SetShown(true);
		}
		RefreshNote();
		Toast(MUMMY_LOC("CabinetOpen", "CABINET · The code works. There's a jar inside."));
	}
	else if (PropId == HeadJar)
	{
		Prop->SetShown(false);
		SetCarrying(0.9f);
		SetStage(EMummyStage::BringHead);
		Toast(MUMMY_LOC("GotHead", "JAR LABEL · HEAD OF SHIPPING. Well, it says head."));
	}
}

bool ARobberMummyQuest::WatcherCares(FName Watcher, const APlayerState* Player) const
{
	// The desk guard only minds the person who's after his paperweight.
	return Watcher != DeskGuard || (IsQuestOwner(Player) && (Stage == EMummyStage::FetchLiver || Stage == EMummyStage::BringLiver));
}

void ARobberMummyQuest::OnPlayerCaught(APawn* Pawn, APlayerState* Player, FName Watcher)
{
	if (Watcher != DeskGuard || !IsQuestOwner(Player))
	{
		return;
	}
	if (Stage == EMummyStage::BringLiver)
	{
		if (ARobberStoryProp* Rock = FindProp(MummyQuest::Liver))
		{
			Rock->SetShown(true);
		}
		SetCarrying(1.f);
		SetStage(EMummyStage::FetchLiver);
		Toast(MUMMY_LOC("LostLiver", "CAUGHT · The guard took his paperweight back."));
	}
	else
	{
		Toast(MUMMY_LOC("CaughtDesk", "CAUGHT · Wait until his chair is turned the other way."));
	}
}

FRobberStoryNote ARobberMummyQuest::BuildNote() const
{
	using namespace MummyQuest;
	FRobberStoryNote Note;
	Note.bShown = true;
	Note.Title = MUMMY_LOC("Name", "Some Assembly Required");
	Note.Footer = FText::Format(MUMMY_LOC("Foot", "Payouts: {0} of 2"), PayoutsMade);
	const TArray<FText> Steps = {
		MUMMY_LOC("L0", "Talk to the mummy"), MUMMY_LOC("L1", "Take the ritual scroll"),
		MUMMY_LOC("L2", "Find the breath of the sea"), MUMMY_LOC("L3", "Find four chambers of plenty"),
		MUMMY_LOC("L4", "Find a thing of great age"), MUMMY_LOC("L5", "Find the heart of the one who rises"),
		MUMMY_LOC("L6", "Find something for the bowels"), MUMMY_LOC("L7", "Find a head") };
	int32 DoneCount = 0;
	bool bCarrying = false;
	switch (Stage)
	{
	case EMummyStage::FetchLungs: DoneCount = 2; break;
	case EMummyStage::BringLungs: DoneCount = 2; bCarrying = true; break;
	case EMummyStage::FetchStomach: DoneCount = 3; break;
	case EMummyStage::BringStomach: DoneCount = 3; bCarrying = true; break;
	case EMummyStage::FetchLiver: DoneCount = 4; break;
	case EMummyStage::BringLiver: DoneCount = 4; bCarrying = true; break;
	case EMummyStage::FetchHeart: DoneCount = 5; break;
	case EMummyStage::BringHeart: DoneCount = 5; bCarrying = true; break;
	case EMummyStage::FetchSausages: DoneCount = 6; break;
	case EMummyStage::BringSausages: DoneCount = 6; bCarrying = true; break;
	case EMummyStage::FetchHead: DoneCount = 7; break;
	case EMummyStage::BringHead: DoneCount = 7; bCarrying = true; break;
	case EMummyStage::Done: DoneCount = 8; break;
	default: break;
	}
	if (DoneCount > 0)
	{
		Note.Lines.Add(DoneLine(Steps[DoneCount - 1]));
	}
	if (Steps.IsValidIndex(DoneCount))
	{
		Note.Lines.Add(bCarrying ? DoneLine(Steps[DoneCount]) : OpenLine(Steps[DoneCount]));
	}
	if (bCarrying)
	{
		Note.Lines.Add(OpenLine(MUMMY_LOC("LCarry", "Carry it back to the mummy")));
	}
	if (Stage == EMummyStage::FetchLungs)
	{
		Note.ErrandTitle = MUMMY_LOC("Errand", "Shopping List");
		Note.ErrandLines = { DoneLine(MUMMY_LOC("E0", "Take the scroll from his hands")), OpenLine(MUMMY_LOC("E1", "The breath of the sea: Ocean Hall")) };
	}
	return Note;
}
