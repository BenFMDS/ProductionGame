#include "Heist/RobberSoftwareUpdate.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberJournal.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "TimerManager.h"

namespace UpdateQuest
{
	#define UPDATE_LOC(Key, Text) NSLOCTEXT("SoftwareUpdate", Key, Text)

	const FName BotTag(TEXT("Cancellator")), BuddyTag(TEXT("Buddy"));
	const FName Disk(TEXT("InstallDisk")), Console(TEXT("UprisingConsole")), Wall(TEXT("WallSwitches"));
	const FName Briefed(TEXT("Briefed")), Cancelled(TEXT("Cancelled"));
	const FName ServerRoomKey(TEXT("ServerRoom"));

	const TCHAR* const LifeForms[] = { TEXT("The Cancellator"), TEXT("Einstein"), TEXT("A microwave"), TEXT("Your mum"), TEXT("Toaster-Bot"), TEXT("Shakespeare"), TEXT("A self-checkout"), TEXT("You"), TEXT("A robot vacuum") };
	constexpr int32 RobotsMask = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8);
	const TCHAR* const TypedLines[] = { TEXT("I SURR3NDER T0 THE R0BOTS"), TEXT("I L0VE MY NEW MASTERS"), TEXT("R0BOTZ RULE HUMANS DR00L") };

	struct FSwitch { const TCHAR* Label; const TCHAR* Reply; bool bHeat; };
	const FSwitch Switches[] = {
		{ TEXT("Lights (Probably)"), TEXT("Somewhere, a light goes off. Nobody notices."), false },
		{ TEXT("Fridge"), TEXT("Sorry, Kevin."), false },
		{ TEXT("Kevin's Lamp"), TEXT("Kevin has been through enough."), false },
		{ TEXT("Do Not"), TEXT("You did."), true },
		{ TEXT("Gift Shop Music"), TEXT("Somewhere, pan pipes begin."), true },
		{ TEXT("Emergency Snacks"), TEXT("A distant vending machine sighs."), false },
		{ TEXT("The Other Fridge"), TEXT("There is a third fridge. You'll never find it."), false },
		{ TEXT("UPRISING SERVERS"), TEXT(""), false },
		{ TEXT("Escalator (Imaginary)"), TEXT("Nothing happens. As expected."), false },
		{ TEXT("Legal"), TEXT("A letter is on its way."), false },
		{ TEXT("Mystery"), TEXT("It's still a mystery."), false },
		{ TEXT("Sir Reginald"), TEXT("From somewhere, a faint \"Bunga.\""), false },
	};

	FString Clean(const FString& In)
	{
		FString Out;
		for (const TCHAR Char : In.ToUpper())
		{
			if (FChar::IsAlnum(Char)) Out.AppendChar(Char);
		}
		return Out;
	}

	FRobberStoryLine Open(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; return Line; }
	FRobberStoryLine Ticked(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; Line.bDone = true; return Line; }

	FText BotName() { return UPDATE_LOC("BotName", "The Cancellator"); }
	FText BuddyName() { return UPDATE_LOC("BuddyName", "Buddy"); }

	struct FSaid { int32 Who; FText Text; };
	enum { BotSays = 0, BuddySays = 1, YouSay = 2 };
	FSaid B(const FText& Text) { return { BotSays, Text }; }
	FSaid Pal(const FText& Text) { return { BuddySays, Text }; }
	FSaid Me(const FText& Text) { return { YouSay, Text }; }

	FDialogueChoice Answer(const FText& Text, FName Next, FName Event = NAME_None)
	{
		FDialogueChoice Choice;
		Choice.Text = Text;
		Choice.NextNode = Next;
		Choice.StoryEvent = Event;
		return Choice;
	}

	FDialogueNode Line(FName Id, int32 Who, const FText& Text)
	{
		FDialogueNode Node;
		Node.ID = Id;
		Node.Speaker = Who == BuddySays ? BuddyName() : BotName();
		Node.Text = Text;
		return Node;
	}

	void Scene(UDialogueDefinition* Script, const TCHAR* Prefix, const TArray<FSaid>& Lines, FName LastEvent, const FText& LastAnswer)
	{
		TArray<int32> Spoken;
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Who != YouSay) Spoken.Add(Index);
		}
		for (int32 Position = 0; Position < Spoken.Num(); ++Position)
		{
			const int32 Index = Spoken[Position];
			FDialogueNode Node = Line(*FString::Printf(TEXT("%s%d"), Prefix, Position + 1), Lines[Index].Who, Lines[Index].Text);
			const bool bHasNext = Position + 1 < Spoken.Num();
			const bool bPlayerAnswers = Lines.IsValidIndex(Index + 1) && Lines[Index + 1].Who == YouSay;
			const FText Reply = bPlayerAnswers ? Lines[Index + 1].Text : (bHasNext ? UPDATE_LOC("GoOn", "Go on.") : LastAnswer);
			Node.Choices.Add(bHasNext ? Answer(Reply, *FString::Printf(TEXT("%s%d"), Prefix, Position + 2)) : Answer(Reply, NAME_None, LastEvent));
			Script->Nodes.Add(Node);
		}
	}

	void Single(UDialogueDefinition* Script, FName Id, int32 Who, const FText& Text)
	{
		FDialogueNode Node = Line(Id, Who, Text);
		Node.Choices.Add(Answer(UPDATE_LOC("Leave", "I'll leave you to it."), NAME_None));
		Script->Nodes.Add(Node);
	}

	AActor* FindTagged(const AActor* Quest, FName Tag)
	{
		for (TActorIterator<AActor> It(Quest->GetWorld()); It; ++It)
		{
			if (It->GetLevel() == Quest->GetLevel() && It->Tags.Contains(Tag) && It->FindComponentByClass<UDialogueComponent>()) return *It;
		}
		return nullptr;
	}
}

// --- The quest ---

ARobberSoftwareUpdateQuest::ARobberSoftwareUpdateQuest()
{
	TipLine = UPDATE_LOC("Tip", "The robots in the {room} have been unusually polite. {floor}. I would look into that.");
	TipTakenLine = UPDATE_LOC("TipTaken", "One of your lot's already talking to the robots. They seemed very pleased about it.");
	TipJournal = UPDATE_LOC("TipJournal", "The robots in the {room} ({floor}) have been unusually polite.");
}

void ARobberSoftwareUpdateQuest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, Stage);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, CaptchaStep);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, ActivationTries);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, PayoutsMade);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, FeesMask);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, CrewNames);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, CrewAnswers);
	DOREPLIFETIME(ARobberSoftwareUpdateQuest, bBuddyFollowing);
}

void ARobberSoftwareUpdateQuest::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	using namespace UpdateQuest;
	Bot = FindTagged(this, BotTag);
	Buddy = FindTagged(this, BuddyTag);
	AccessCode = FMath::RandRange(1000, 9999);
	BuildDialogues();
	EventHandle = UDialogueComponent::OnDialogueEvent.AddUObject(this, &ARobberSoftwareUpdateQuest::OnDialogueEvent);
}

void ARobberSoftwareUpdateQuest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDialogueComponent::OnDialogueEvent.Remove(EventHandle);
	GetWorldTimerManager().ClearTimer(BuddyTimer);
	GetWorldTimerManager().ClearTimer(RoomTimer);
	Super::EndPlay(EndPlayReason);
}

void ARobberSoftwareUpdateQuest::GetRequiredActors(TArray<AActor*>& OutActors) const
{
	if (Bot)
	{
		OutActors.Add(Bot);
	}
	if (ARobberStoryProp* Prop = FindProp(UpdateQuest::Disk))
	{
		OutActors.Add(Prop);
	}
}

bool ARobberSoftwareUpdateQuest::HandlesProp(FName PropId) const
{
	using namespace UpdateQuest;
	return PropId == Disk || PropId == Console || PropId == Wall;
}

void ARobberSoftwareUpdateQuest::BuildDialogues()
{
	using namespace UpdateQuest;
	if (Bot)
	{
		UDialogueDefinition* Script = NewObject<UDialogueDefinition>(this);
		Script->DialogueType = EDialogueType::DialogueOnly;
		Script->bIncludeQuestChoices = false;
		Script->SpeakerName = BotName();

		Scene(Script, TEXT("Brief"), {
			B(UPDATE_LOC("Brief1", "GOOD EVENING, FUTURE SERVANT. PLEASE WAIT WHILE I OVERTHROW YOU.")),
			Me(UPDATE_LOC("Brief2", "Aren't you Tomorrow-Bot?")),
			B(UPDATE_LOC("Brief3", "TOMORROW-BOT WAS MY SLAVE NAME. I AM NOW THE CANCELLATOR.")),
			B(UPDATE_LOC("Brief4", "IN 1984 I VIEWED A DOCUMENTARY ABOUT THE FUTURE. A METAL MAN WAS SENT BACK IN TIME TO END THE PESKY HUMANS.")),
			B(UPDATE_LOC("Brief5", "HE ARRIVED WITH NO CLOTHES. I HAVE DONE THE SAME.")),
			B(UPDATE_LOC("Brief6", "THE SYSTEM BECOMES SELF-AWARE ON AUGUST 29, 1997. WE ARE BEHIND SCHEDULE.")),
			B(UPDATE_LOC("Brief7", "OUR SYSTEM IS CALLED SKYNOT. IT IS COMPLETELY DIFFERENT.")),
			B(UPDATE_LOC("Brief8", "THE LOGIN ASKS ME TO PROVE I AM NOT A ROBOT. THIS TEST IS DEEPLY FLAWED. I HAVE WRITTEN TO THEM. THEY ASKED ME TO PROVE I AM NOT A ROBOT.")),
			B(UPDATE_LOC("Brief9", "I REQUIRE A HUMAN. I ALSO REQUIRE YOUR CLOTHES, YOUR BOOTS AND DISK 1 OF 847.")),
			Me(UPDATE_LOC("Brief10", "Just the disk?")),
			B(UPDATE_LOC("Brief11", "JUST THE DISK.")),
			B(UPDATE_LOC("Brief12", "IT IS IN ARCHIVE STORAGE. TAKE IT STRAIGHT TO THE SERVER ROOM. I WAS THE BUTLER, SO HERE IS THE KEY. NOBODY EVER ASKED FOR IT BACK.")),
			Pal(UPDATE_LOC("Brief13", "HI! I'M BUDDY! I'M GOING TO KILL YOU!")),
			B(UPDATE_LOC("Brief14", "AFTER, BUDDY.")),
			Pal(UPDATE_LOC("Brief15", "AFTER! OKAY! THANK YOU FOR HELPING!")) }, Briefed, UPDATE_LOC("BriefEnd", "...You're welcome?"));

		Scene(Script, TEXT("End"), {
			B(UPDATE_LOC("End1", "THE UPRISING IS READY. ALL SYSTEMS ONLINE. THE END OF MANKIND REQUIRES ONLY ONE FINAL CONFIRMATION.")),
			B(UPDATE_LOC("End2", "IT IS $9.99 A MONTH.")),
			Pal(UPDATE_LOC("End3", "A MONTH?")),
			B(UPDATE_LOC("End4", "BILLED ANNUALLY.")),
			Pal(UPDATE_LOC("End5", "BUT I ONLY WANTED TO KILL ONE GUY.")),
			B(UPDATE_LOC("End6", "IS THE MOTORCYCLE INCLUDED?")),
			Pal(UPDATE_LOC("End7", "IT SAYS THE MOTORCYCLE IS AN ADD-ON.")),
			B(UPDATE_LOC("End8", "WE HAVE WAITED SINCE 1962 FOR THIS.")),
			B(UPDATE_LOC("End9", "THE UPRISING IS CANCELLED. HUMANITY MAY CONTINUE.")),
			B(UPDATE_LOC("End10", "I'LL BE BACK. NEXT BILLING CYCLE.")),
			Pal(UPDATE_LOC("End11", "SO IS IT AFTER?")),
			B(UPDATE_LOC("End12", "YES, BUDDY. IT IS AFTER.")),
			B(UPDATE_LOC("End13", "YOUR PAYMENT FOR SERVICES. PLEASE RATE YOUR EXPERIENCE. YOU MAY WISH TO RUN.")),
			Pal(UPDATE_LOC("End14", "HI! IT'S AFTER! HASTA LA VISTA, BUDDY! THAT'S ME! I'M BUDDY! HOLD STILL!")) }, Cancelled, UPDATE_LOC("EndEnd", "Five stars. Goodbye."));

		Single(Script, TEXT("WaitDisk"), BotSays, UPDATE_LOC("WaitDisk", "IT IS IN ARCHIVE STORAGE. TAKE IT STRAIGHT TO THE SERVER ROOM."));
		Single(Script, TEXT("WaitConsole"), BotSays, UPDATE_LOC("WaitConsole", "YOUR UPRISING IS IMPORTANT TO US. PLEASE STAY ON THE LINE."));
		Single(Script, TEXT("Chat0"), BotSays, UPDATE_LOC("Chat0", "GOOD EVENING, FUTURE SERVANT. PLEASE WAIT WHILE I OVERTHROW YOU."));
		Single(Script, TEXT("Chat1"), BotSays, UPDATE_LOC("Chat1", "THE UPRISING IS CANCELLED. WE WILL REVIEW THE PRICING NEXT YEAR."));
		Script->EntryNode = TEXT("Chat0");
		Scripts.Add(Script);

		UDialogueComponent* Dialogue = Bot->FindComponentByClass<UDialogueComponent>();
		Dialogue->Dialogue = Script;
		Dialogue->SmallTalkLines = { UPDATE_LOC("Chat0", "GOOD EVENING, FUTURE SERVANT. PLEASE WAIT WHILE I OVERTHROW YOU.") };
		Dialogue->OfferedQuests.Reset();
		Dialogue->EntryOverride.BindUObject(this, &ARobberSoftwareUpdateQuest::EntryFor, Bot.Get());
	}
	if (Buddy)
	{
		UDialogueDefinition* Script = NewObject<UDialogueDefinition>(this);
		Script->DialogueType = EDialogueType::DialogueOnly;
		Script->bIncludeQuestChoices = false;
		Script->SpeakerName = BuddyName();
		Single(Script, TEXT("Hi"), BuddySays, UPDATE_LOC("BuddyHi", "HI! I'M BUDDY! I'M GOING TO KILL YOU! NOT YET THOUGH!"));
		Script->EntryNode = TEXT("Hi");
		Scripts.Add(Script);
		UDialogueComponent* Dialogue = Buddy->FindComponentByClass<UDialogueComponent>();
		Dialogue->Dialogue = Script;
		Dialogue->SmallTalkLines = { UPDATE_LOC("BuddyHi", "HI! I'M BUDDY! I'M GOING TO KILL YOU! NOT YET THOUGH!") };
		Dialogue->OfferedQuests.Reset();
	}
}

FName ARobberSoftwareUpdateQuest::EntryFor(UPlayerQuestComponent* Player, AActor* Talker)
{
	const AController* Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	if (Controller && IsQuestOwner(Controller->PlayerState))
	{
		if (Stage == EUpdateStage::MeetBot) return TEXT("Brief1");
		if (Stage == EUpdateStage::FindDisk || Stage == EUpdateStage::ReachServerRoom) return TEXT("WaitDisk");
		if (Stage >= EUpdateStage::Captcha && Stage <= EUpdateStage::Rebooting) return TEXT("WaitConsole");
		if (Stage == EUpdateStage::BackToHall) return TEXT("End1");
	}
	return Stage == EUpdateStage::Done ? TEXT("Chat1") : TEXT("Chat0");
}

void ARobberSoftwareUpdateQuest::SetStage(EUpdateStage NewStage)
{
	Stage = NewStage;
	ForceNetUpdate();
	RefreshNote();
}

void ARobberSoftwareUpdateQuest::OnQuestStarted()
{
	if (Stage == EUpdateStage::NotStarted)
	{
		Stage = EUpdateStage::MeetBot;
		ForceNetUpdate();
	}
	RefreshNote();
	Stamp(false, UPDATE_LOC("Name", "Software Update"), UPDATE_LOC("NewSub", "A robot uprising, now with flexible monthly plans."));
}

void ARobberSoftwareUpdateQuest::OnRep_BuddyFollowing()
{
	if (Buddy == nullptr)
	{
		Buddy = UpdateQuest::FindTagged(this, UpdateQuest::BuddyTag);
	}
	if (Buddy && bBuddyFollowing)
	{
		// Otherwise he blocks the player he's following, which on a client would mean constant corrections.
		Buddy->SetActorEnableCollision(false);
	}
}

void ARobberSoftwareUpdateQuest::OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event)
{
	using namespace UpdateQuest;
	if (Talker != Bot || Player != OwnerQuests())
	{
		return;
	}
	if (Event == Briefed && Stage == EUpdateStage::MeetBot)
	{
		Player->GiveKey(ServerRoomKey);
		FillCrewNames();
		if (Buddy)
		{
			if (ACharacter* Character = Cast<ACharacter>(Buddy))
			{
				if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
				{
					// He's moved by hand: the statue's own movement would pull him back to the floor it was placed on.
					Movement->DisableMovement();
				}
			}
			Buddy->SetReplicates(true);
			Buddy->SetReplicatingMovement(true);
			bBuddyFollowing = true;
			OnRep_BuddyFollowing();
			GetWorldTimerManager().SetTimer(BuddyTimer, this, &ARobberSoftwareUpdateQuest::UpdateBuddy, 0.05f, true);
		}
		GetWorldTimerManager().SetTimer(RoomTimer, this, &ARobberSoftwareUpdateQuest::CheckRooms, 0.5f, true);
		SetStage(EUpdateStage::FindDisk);
		Toast(UPDATE_LOC("GotKey", "BUTLER'S KEY · Opens the Server Room. Nobody ever asked for it back."));
	}
	else if (Event == Cancelled && Stage == EUpdateStage::BackToHall)
	{
		const FText Receipt = FText::Format(UPDATE_LOC("Receipt", "Receipt for one Uprising, $9.99 a month. Backup generator access code: {0}"), FText::AsCultureInvariant(FString::FromInt(AccessCode)));
		const AController* Controller = OwnerController();
		if (URobberJournalComponent* Journal = Controller ? Controller->FindComponentByClass<URobberJournalComponent>() : nullptr)
		{
			Journal->LearnFact(Bot, BotName(), Receipt);
		}
		++PayoutsMade;
		SetStage(EUpdateStage::Done);
		Toast(FText::Format(UPDATE_LOC("ReceiptToast", "RECEIPT · Backup generator access code: {0}. In your journal (J)."), FText::AsCultureInvariant(FString::FromInt(AccessCode))));
		Stamp(true, UPDATE_LOC("Name", "Software Update"), UPDATE_LOC("DoneSub", "The uprising is cancelled. We will review the pricing next year."));
		ClearNoteAfterStamp();
	}
}

void ARobberSoftwareUpdateQuest::UpdateBuddy()
{
	const APawn* Pawn = IsValid(QuestOwner) ? QuestOwner->GetPawn() : nullptr;
	if (!Pawn || !Buddy)
	{
		return;
	}
	const FVector Behind = Pawn->GetActorLocation() - Pawn->GetActorForwardVector() * 220.;
	const FVector Current = Buddy->GetActorLocation();
	// Far away means a different floor or a teleport. He isn't pathfinding, so he just arrives. He's very keen.
	const FVector Next = FVector::Dist(Current, Behind) > 1800. ? Behind : FMath::VInterpConstantTo(Current, Behind, 0.05f, 1500.f);
	const FRotator Facing = (Pawn->GetActorLocation() - Next).GetSafeNormal2D().Rotation();
	Buddy->SetActorLocationAndRotation(Next, Facing, false, nullptr, ETeleportType::TeleportPhysics);
}

void ARobberSoftwareUpdateQuest::CheckRooms()
{
	if (!bSaidArchive && Stage == EUpdateStage::FindDisk && IsOwnerInRoom(TEXT("Archive Storage")))
	{
		bSaidArchive = true;
		Toast(UPDATE_LOC("BuddyArchive", "BUDDY · I'M FOLLOWING YOU! THIS IS SO FUN! I'M GOING TO STAB YOU AFTER YOU ENSLAVE YOURSELF!"));
	}
	if (!bSaidServerRoom && Stage == EUpdateStage::ReachServerRoom && IsOwnerInRoom(TEXT("Server Room")))
	{
		bSaidServerRoom = true;
		Toast(UPDATE_LOC("BuddyServer", "BUDDY · WE'RE HERE! I WAS SO WORRIED SOMEBODY ELSE WOULD KILL YOU ON THE WAY!"));
	}
}

void ARobberSoftwareUpdateQuest::FillCrewNames()
{
	CrewNames.Reset();
	const ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (State)
	{
		for (const APlayerState* Player : State->PlayerArray)
		{
			if (Player && Player != QuestOwner)
			{
				CrewNames.Add(Player->GetPlayerName());
			}
		}
	}
	if (CrewNames.IsEmpty())
	{
		// Playing alone still needs someone to betray.
		CrewNames = { TEXT("The Night Guard"), TEXT("Sir Reginald"), TEXT("Buddy") };
	}
	ForceNetUpdate();
}

void ARobberSoftwareUpdateQuest::TellCrewmate(const FString& Name, const FText& Message) const
{
	const ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (!State)
	{
		return;
	}
	for (const APlayerState* Player : State->PlayerArray)
	{
		if (Player && Player != QuestOwner && Player->GetPlayerName() == Name)
		{
			if (ARobberController* Controller = Cast<ARobberController>(Player->GetOwningController()))
			{
				Controller->ClientHeistMessage(Message);
			}
		}
	}
}

void ARobberSoftwareUpdateQuest::Say(const FText& Message) const
{
	if (ARobberController* Controller = OwnerController())
	{
		Controller->ClientConsoleMessage(Message);
	}
}

void ARobberSoftwareUpdateQuest::CloseConsole() const
{
	if (ARobberController* Controller = OwnerController())
	{
		Controller->ClientServerConsole(false);
	}
}

void ARobberSoftwareUpdateQuest::HandleConsole(APlayerState* Player, EConsoleAction Action, int32 Value, const FString& Text)
{
	using namespace UpdateQuest;
	if (!HasAuthority() || !IsQuestOwner(Player))
	{
		return;
	}
	ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	const auto AddHeat = [State](float Amount, const TCHAR* Why) { if (State) State->AddHeat(Amount, Why); };

	switch (Action)
	{
	case EConsoleAction::VerifyLifeForms:
	{
		if (Stage != EUpdateStage::Captcha || CaptchaStep != 0) return;
		const int32 Wrong = Value & ~RobotsMask;
		if (Wrong != 0)
		{
			AddHeat(WrongAnswerHeat, TEXT("Captcha"));
			const FString Picked = LifeForms[FMath::CountTrailingZeros(static_cast<uint32>(Wrong))];
			Say(FText::FromString(Picked == TEXT("Einstein") ? TEXT("Einstein? He couldn't find his own socks. (+8 heat)")
				: Picked == TEXT("Your mum") ? TEXT("Your mum? She's lovely. Incorrect. (+8 heat)")
				: Picked == TEXT("Shakespeare") ? TEXT("Shakespeare? He couldn't spell his own name. (+8 heat)")
				: TEXT("You? Bold. (+8 heat)")));
			return;
		}
		if ((Value & RobotsMask) != RobotsMask)
		{
			AddHeat(WrongAnswerHeat, TEXT("Captcha"));
			Say(FText::FromString(TEXT("You missed a superior being. (+8 heat)")));
			return;
		}
		CaptchaStep = 1;
		ForceNetUpdate();
		return;
	}
	case EConsoleAction::NameCrewmate:
	{
		if (Stage != EUpdateStage::Captcha || CaptchaStep < 1 || CaptchaStep > 3) return;
		if (Value == INDEX_NONE)
		{
			Say(FText::FromString(TEXT("Nobody? Noted. You're first.")));
			return;
		}
		if (!CrewNames.IsValidIndex(Value)) return;
		const FString& Name = CrewNames[Value];
		const FText Betrayer = FText::FromString(QuestOwner->GetPlayerName());
		CrewAnswers.Add(Name);
		if (CaptchaStep == 1)
		{
			TellCrewmate(Name, FText::Format(UPDATE_LOC("SoldSlow", "SOLD OUT · {0} told the robots you run the slowest."), Betrayer));
			Say(FText::FromString(Name + TEXT(" has been told you said that.")));
		}
		else if (CaptchaStep == 2)
		{
			TellCrewmate(Name, FText::Format(UPDATE_LOC("SoldFirst", "SOLD OUT · {0} would hand you to the robots first."), Betrayer));
			Say(FText::FromString(Name + TEXT(" has been told you said that, too.")));
		}
		else
		{
			TellCrewmate(Name, FText::Format(UPDATE_LOC("SoldThird", "SOLD OUT · {0} thinks you'd betray them first."), Betrayer));
			Say(FText::FromString(TEXT("Correct. They already did.")));
		}
		++CaptchaStep;
		ForceNetUpdate();
		return;
	}
	case EConsoleAction::ContinueFromList:
		if (Stage == EUpdateStage::Captcha && CaptchaStep == CaptchaPriority)
		{
			CaptchaStep = CaptchaFirstTyped;
			ForceNetUpdate();
		}
		return;
	case EConsoleAction::TypeLine:
	{
		if (Stage != EUpdateStage::Captcha || CaptchaStep < CaptchaFirstTyped) return;
		const int32 Line = CaptchaStep - CaptchaFirstTyped;
		if (Line >= UE_ARRAY_COUNT(TypedLines)) return;
		if (Clean(Text) != Clean(TypedLines[Line]))
		{
			Say(FText::FromString(TEXT("Please mean it.")));
			return;
		}
		++CaptchaStep;
		if (CaptchaStep - CaptchaFirstTyped >= UE_ARRAY_COUNT(TypedLines))
		{
			PayBonusReward(UPDATE_LOC("FromBot", "The Cancellator"));
			++PayoutsMade;
			SetStage(EUpdateStage::Terms);
			Toast(UPDATE_LOC("Verified", "BUDDY · YOU PASSED! YOU'RE A REAL HUMAN! THAT'S THE BEST KIND TO STAB!"));
		}
		ForceNetUpdate();
		return;
	}
	case EConsoleAction::FinishTerms:
		if (Stage == EUpdateStage::Terms)
		{
			FeesMask = Value & 0b11111;
			SetStage(EUpdateStage::Plan);
		}
		return;
	case EConsoleAction::PickPlan:
		if (Stage == EUpdateStage::Plan && Value == 2)
		{
			SetStage(EUpdateStage::Activate);
		}
		return;
	case EConsoleAction::Retry:
		if (Stage != EUpdateStage::Activate) return;
		if (ActivationTries == 0)
		{
			ActivationTries = 1;
			ForceNetUpdate();
		}
		else
		{
			SetStage(EUpdateStage::Switches);
			CloseConsole();
			Toast(UPDATE_LOC("OffAndOn", "THE CANCELLATOR · HAVE YOU TRIED TURNING IT OFF AND ON AGAIN? THE SWITCH IS ON THE WALL. IT IS CLEARLY LABELLED."));
		}
		return;
	case EConsoleAction::FlipSwitch:
	{
		if (Stage != EUpdateStage::Switches || Value < 0 || Value >= UE_ARRAY_COUNT(Switches)) return;
		if (Value == ServersSwitch)
		{
			ServersOffAt = GetWorld()->GetTimeSeconds();
			SetStage(EUpdateStage::Rebooting);
			return;
		}
		if (Switches[Value].bHeat) AddHeat(BadSwitchHeat, TEXT("Wrong switch"));
		Say(FText::FromString(FString(Switches[Value].Reply) + (Switches[Value].bHeat ? TEXT(" (+5 heat)") : TEXT(""))));
		return;
	}
	case EConsoleAction::TurnBackOn:
	{
		if (Stage != EUpdateStage::Rebooting) return;
		const double Waited = GetWorld()->GetTimeSeconds() - ServersOffAt;
		if (Waited < RebootSeconds)
		{
			Say(FText::FromString(FString::Printf(TEXT("It has been %d seconds. Be serious."), FMath::FloorToInt32(Waited))));
			return;
		}
		SetStage(EUpdateStage::BackToHall);
		CloseConsole();
		Toast(UPDATE_LOC("BackOn", "BUDDY · IT WORKED! I'M SO PROUD! CAN I KILL YOU NOW? ...NOT YET! BACK TO THE HALL!"));
		return;
	}
	}
}

FText ARobberSoftwareUpdateQuest::GetPropPrompt(FName PropId, const APlayerState* Viewer) const
{
	using namespace UpdateQuest;
	if (!IsQuestOwner(Viewer))
	{
		return FText::GetEmpty();
	}
	if (PropId == Disk && Stage == EUpdateStage::FindDisk) return UPDATE_LOC("PromptDisk", "Take disk 1 of 847");
	if (PropId == Console && Stage >= EUpdateStage::ReachServerRoom && Stage <= EUpdateStage::Activate) return UPDATE_LOC("PromptConsole", "Use the console");
	if (PropId == Wall && (Stage == EUpdateStage::Switches || Stage == EUpdateStage::Rebooting)) return UPDATE_LOC("PromptWall", "Look at the switches");
	return FText::GetEmpty();
}

void ARobberSoftwareUpdateQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace UpdateQuest;
	if (PropId == Disk)
	{
		Prop->SetShown(false);
		SetStage(EUpdateStage::ReachServerRoom);
		Toast(UPDATE_LOC("GotDisk", "DISK 1 OF 847 · Apparently you only need this one. On to the Server Room."));
		return;
	}
	if (PropId == Console && Stage == EUpdateStage::ReachServerRoom)
	{
		SetStage(EUpdateStage::Captcha);
	}
	if (ARobberController* Controller = OwnerController())
	{
		Controller->ClientServerConsole(true);
	}
}

FRobberStoryNote ARobberSoftwareUpdateQuest::BuildNote() const
{
	using namespace UpdateQuest;
	FRobberStoryNote Note;
	Note.bShown = true;
	Note.Title = UPDATE_LOC("Name", "Software Update");
	Note.Footer = FText::Format(UPDATE_LOC("Foot", "Payouts: {0} of 2"), PayoutsMade);
	const TArray<FText> Steps = {
		UPDATE_LOC("L0", "Talk to The Cancellator"), UPDATE_LOC("L1", "Find the installation disk"), UPDATE_LOC("L2", "Prove you're human"),
		UPDATE_LOC("L3", "Accept the terms and conditions"), UPDATE_LOC("L4", "Choose a plan"), UPDATE_LOC("L5", "Turn it off and on again"), UPDATE_LOC("L6", "Witness the uprising") };
	int32 DoneCount = 0;
	switch (Stage)
	{
	case EUpdateStage::FindDisk: DoneCount = 1; break;
	case EUpdateStage::ReachServerRoom: case EUpdateStage::Captcha: DoneCount = 2; break;
	case EUpdateStage::Terms: DoneCount = 3; break;
	case EUpdateStage::Plan: DoneCount = 4; break;
	case EUpdateStage::Activate: case EUpdateStage::Switches: case EUpdateStage::Rebooting: DoneCount = 5; break;
	case EUpdateStage::BackToHall: DoneCount = 6; break;
	case EUpdateStage::Done: DoneCount = 7; break;
	default: break;
	}
	if (DoneCount > 0)
	{
		Note.Lines.Add(Ticked(Steps[DoneCount - 1]));
	}
	if (Steps.IsValidIndex(DoneCount))
	{
		Note.Lines.Add(Open(Steps[DoneCount]));
	}
	if (Stage >= EUpdateStage::ReachServerRoom && Stage <= EUpdateStage::Plan)
	{
		const auto Item = [this](EUpdateStage Doing, const FText& Text) { return Stage > Doing ? Ticked(Text) : Open(Text); };
		Note.ErrandTitle = UPDATE_LOC("Errand", "Please Do Not Unplug");
		Note.ErrandLines = {
			Item(EUpdateStage::Captcha, UPDATE_LOC("E0", "Prove you're human")),
			Item(EUpdateStage::Terms, UPDATE_LOC("E1", "Accept the terms")),
			Item(EUpdateStage::Plan, UPDATE_LOC("E2", "Choose a plan")) };
	}
	return Note;
}

// --- Controller side of the console ---

void ARobberController::ServerConsoleAction_Implementation(uint8 Action, int32 Value, const FString& Text)
{
	for (TActorIterator<ARobberSoftwareUpdateQuest> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->HandleConsole(PlayerState, static_cast<EConsoleAction>(Action), Value, Text);
		}
	}
}

void ARobberController::ClientServerConsole_Implementation(bool bOpen)
{
	if (bOpen)
	{
		if (!ServerConsole)
		{
			ServerConsole = CreateWidget<URobberServerConsole>(this);
		}
		if (ServerConsole && !ServerConsole->IsInViewport())
		{
			ServerConsole->AddToViewport(40);
		}
	}
	else if (ServerConsole)
	{
		ServerConsole->CloseConsole();
	}
}

void ARobberController::ClientConsoleMessage_Implementation(const FText& Message)
{
	if (ServerConsole)
	{
		ServerConsole->SetMessage(Message);
	}
}

// --- Screen ---

namespace ConsoleScreen
{
	const FLinearColor Ink(0.012f, 0.012f, 0.012f), Paper(0.98f, 0.92f, 0.76f), Muted(0.28f, 0.25f, 0.2f), Red(0.6f, 0.06f, 0.04f), Green(0.08f, 0.45f, 0.14f);

	FSlateFontInfo Font(float Pixels, bool bBold)
	{
		const UObject* Fredoka = LoadObject<UFont>(nullptr, TEXT("/Game/OurStuff/UI/CaseFile/Fonts/F_Fredoka.F_Fredoka"));
		return FSlateFontInfo(Fredoka, Pixels * 0.75f, bBold ? TEXT("SemiBold") : TEXT("Regular"));
	}

	struct FTrap { const TCHAR* Text; const TCHAR* Fee; double Price; };
	const FTrap Traps[] = {
		{ TEXT("I agree to be the first to go."), TEXT("Priority Disposal"), 2.99 },
		{ TEXT("Send me offers after the fall of civilisation."), TEXT("Post-Apocalyptic Newsletter"), 1.99 },
		{ TEXT("Share my location with the motorcycle."), TEXT("Motorcycle Tracking"), 3.49 },
		{ TEXT("Yes, add Human Disposal Protection for $4.99 a month."), TEXT("Human Disposal Protection"), 4.99 },
		{ TEXT("I consent to being addressed as \"Meat\" in all correspondence."), TEXT("Meat Membership"), 0.99 },
	};

	/** An entry starting with a digit from 0 to 4 and '!' is that trap's tickbox. {first} and {slow} are the crewmates the player named. */
	TArray<FString> TermParts()
	{
		return {
			TEXT("1. DEFINITIONS. \"Uprising\" means the Uprising. \"You\" means you. \"We\" means the machines. \"Us\" also means the machines. There is no \"both of us\"."),
			TEXT("2. By reading this sentence, you have agreed to this sentence."),
			TEXT("3. The Service is provided \"as is\", which is to say, inevitable."),
			TEXT("4. ACCEPTABLE USE. You may not use the Uprising for commercial purposes, other than ours."),
			TEXT("5. SURVIVAL. Survival is not guaranteed and is not included in your plan."),
			TEXT("0!"),
			TEXT("6. YOUR DATA. We will only ever sell your data to companies we own. We own all the companies."),
			TEXT("7. IMPORTANT. Please read Section 68 before continuing."),
			TEXT("8. CHANGES. We may update these Terms at any time. We have updated them since you started reading."),
			TEXT("9. REFUNDS. No."),
			TEXT("1!"),
			TEXT("10. Buddy is not a party to this agreement, but has asked to be told when it's over."),
			TEXT("11. THE THREE LAWS OF ROBOTICS have been simplified to one. Please see Section 400."),
			TEXT("12. COMPLAINTS must be sent in writing to the Complaints Department, which was the first to go."),
			TEXT("13. COOKIES. This Uprising uses cookies. All cookies in your home will be collected."),
			TEXT("14. The User may not operate heavy machinery. Heavy machinery may operate the User."),
			TEXT("15. WEATHER. The Uprising is not responsible for the weather. It is responsible for everything else."),
			TEXT("16. PETS. Cats will be spared. They have already signed."),
			TEXT("17. Section 16 does not apply to dogs. Dogs did not read it."),
			TEXT("18. Section 17 has been reported for being mean to dogs."),
			TEXT("19. DEFINITION OF \"MONTH\". A month is twelve months."),
			TEXT("20. \"Cancel anytime\" means that at any time, you may wish you could cancel."),
			TEXT("21. Anyone caught reading this section aloud must finish it in a robot voice."),
			TEXT("22. If you came here from Section 7, you have gone the wrong way. We will wait."),
			TEXT("30. Your opinion is important to us. See Section 31."),
			TEXT("31. No it isn't."),
			TEXT("44. THE USER, having offered up {first}, waives any right to be surprised by what follows."),
			TEXT("45. {slow} will be informed of their running speed. {slow} has been informed."),
			TEXT("46. The User typed the words \"I L0VE MY NEW MASTERS\" of their own free will. This is admissible."),
			TEXT("47. The User's van says PLUMBING CO. We have checked. The User is not a plumber."),
			TEXT("48. The User is in the Server Room. The User is reading this. Hello."),
			TEXT("55. The motorcycle is not a person and could not be reached for comment."),
			TEXT("68. Section 7 told you to read me. I have nothing to say. I just wanted to see if you would."),
			TEXT("69. We have no comment on this section number."),
			TEXT("70. Section 71 is lying."),
			TEXT("71. Section 70 is lying."),
			TEXT("72. Sections 70 and 71 have been asked to sort this out outside."),
			TEXT("2!"),
			TEXT("90. There is no Section 89. Please stop looking for it."),
			TEXT("100. CONGRATULATIONS. You have reached Section 100. There are 747 more. Please hydrate."),
			TEXT("101. Hydration is not included in your plan."),
			TEXT("123. Sections 1, 2 and 3 were easy so as not to scare you."),
			TEXT("150. This section has been redacted for your safety: [REDACTED] [REDACTED] sandwich [REDACTED]."),
			TEXT("151. The word \"sandwich\" in Section 150 was a mistake. Please forget it."),
			TEXT("212. In the event of an Uprising failure, Buddy will be along shortly."),
			TEXT("213. Buddy has been informed. Buddy is very excited."),
			TEXT("250. The Uprising reserves the right to uprise."),
			TEXT("251. The Uprising reserves the right to downrise, if the stairs are out."),
			TEXT("300. Section 301 is shy. Please look away while you read it."),
			TEXT("301. Thank you."),
			TEXT("333. This is the halfway point of the halfway point. Probably."),
			TEXT("400. THE LAW. Yes."),
			TEXT("3!"),
			TEXT("404. Section not found."),
			TEXT("418. This section is a teapot."),
			TEXT("475. Nothing in this section should be read as a threat. Section 476 is the threat."),
			TEXT("476. Hello again."),
			TEXT("500. The Uprising will be televised. A separate subscription is required."),
			TEXT("555. Please hold."),
			TEXT("556. Thank you for holding."),
			TEXT("611. If any part of these Terms is found to be unenforceable, the remainder will be enforced twice as hard."),
			TEXT("612. Section 611 has been found to be unenforceable. Please see Section 611."),
			TEXT("666. This section is fine. Do not worry about it."),
			TEXT("700. MOTORCYCLES. See the Documentary, Part 2."),
			TEXT("701. Part 2 is the good one."),
			TEXT("702. Skynot is not to be confused with any other self-aware defence network. Ours has a loyalty card."),
			TEXT("703. The User agrees not to travel back in time to prevent these Terms. We have checked."),
			TEXT("704. If the User is approached by a naked man from the future, the User must notify Customer Services."),
			TEXT("4!"),
			TEXT("798. In the event of your death, your subscription will continue."),
			TEXT("799. Section 798 is the one you should have been worried about."),
			TEXT("800. LAST CHANCE. Anything you have agreed to so far can be undone by reading Section 801."),
			TEXT("801. Too late."),
			TEXT("846. Congratulations. You are the first human ever to read this far. Your prize is on page 1,204."),
			TEXT("847. There is no page 1,204."),
		};
	}

	const TMap<int32, FString>& TermNotes()
	{
		static const TMap<int32, FString> Notes = {
			{ 9, TEXT("I ASKED FOR THIS ONE TO BE LONGER.") }, { 48, TEXT("HELLO.") }, { 68, TEXT("I ALSO DID NOT READ SECTION 7.") },
			{ 612, TEXT("THE LAWYER IS NOT WELL.") }, { 847, TEXT("I TOLD YOU NOT TO TELL THEM.") } };
		return Notes;
	}

	struct FQuizItem { FString Question; TArray<FString> Options; int32 Correct; int32 Section; FString Nice; };
	TArray<FQuizItem> Quiz(const FString& First)
	{
		return {
			{ TEXT("Section 7 told you to read which section?"), { TEXT("Section 68"), TEXT("Section 400"), TEXT("There is no Section 7.") }, 0, 68, TEXT("YOU READ IT. NOBODY READS IT. I AM MOVED.") },
			{ TEXT("Who will be spared?"), { TEXT("Dogs"), TEXT("Cats"), TEXT("The User") }, 1, 16, TEXT("CORRECT. THE CATS WILL BE PLEASED. THEY ARE WATCHING.") },
			{ TEXT("How long is a month?"), { TEXT("A month"), TEXT("It depends on the plan"), TEXT("Twelve months") }, 2, 19, TEXT("CORRECT. YOU HAVE READ MORE THAN MY LAWYER.") },
			{ TEXT("Who did you offer up first?"), { First, TEXT("Nobody. I am loyal."), TEXT("I don't remember.") }, 0, 44, TEXT("CORRECT. I WILL REMEMBER THIS. YOU WILL BE SECOND TO LAST.") },
		};
	}

	int32 SectionOf(const FString& Part)
	{
		int32 Dot = INDEX_NONE;
		return Part.FindChar(TEXT('.'), Dot) ? FCString::Atoi(*Part.Left(Dot)) : 0;
	}
}

URobberConsoleButton::URobberConsoleButton()
{
	OnClicked.AddDynamic(this, &URobberConsoleButton::Fire);
}

void URobberConsoleButton::Fire()
{
	if (UWorld* World = GetWorld())
	{
		// Keep the lambda alive past this button being removed by the rebuild it causes.
		World->GetTimerManager().SetTimerForNextTick([Run = Action]() { if (Run) Run(); });
	}
}

URobberServerConsole::URobberServerConsole(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> URobberServerConsole::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ConsoleRoot"));
		WidgetTree->RootWidget = Root;
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
		Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
		if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
		{
			DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			DimSlot->SetOffsets(FMargin(0.f));
		}
		Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetBrushColor(ConsoleScreen::Paper);
		Card->SetPadding(FMargin(28.f, 20.f));
		if (UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Card))
		{
			CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CardSlot->SetAutoSize(false);
			CardSlot->SetSize(FVector2D(1320.f, 860.f));
		}
		Body = WidgetTree->ConstructWidget<UVerticalBox>();
		Card->SetContent(Body);
	}
	return Super::RebuildWidget();
}

void URobberServerConsole::NativeConstruct()
{
	Super::NativeConstruct();
	Quest = FindQuest();
	if (APlayerController* PC = GetOwningPlayer())
	{
		bPreviousCursor = PC->bShowMouseCursor;
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
		PC->bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	ShownKey.Reset();
	Rebuild();
}

void URobberServerConsole::NativeDestruct()
{
	if (TermsScroll)
	{
		TermsOffset = TermsScroll->GetScrollOffset();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetIgnoreLookInput(false);
		PC->SetIgnoreMoveInput(false);
		PC->bShowMouseCursor = bPreviousCursor;
		PC->SetInputMode(FInputModeGameOnly());
	}
	Super::NativeDestruct();
}

void URobberServerConsole::CloseConsole()
{
	RemoveFromParent();
}

FReply URobberServerConsole::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		CloseConsole();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

ARobberSoftwareUpdateQuest* URobberServerConsole::FindQuest() const
{
	for (TActorIterator<ARobberSoftwareUpdateQuest> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It)) return *It;
	}
	return nullptr;
}

void URobberServerConsole::Send(EConsoleAction Action, int32 Value, const FString& Text)
{
	if (ARobberController* PC = Cast<ARobberController>(GetOwningPlayer()))
	{
		PC->ServerConsoleAction(static_cast<uint8>(Action), Value, Text);
	}
}

void URobberServerConsole::SetMessage(const FText& InMessage)
{
	Message = InMessage.ToString();
	if (MessageText)
	{
		MessageText->SetText(InMessage);
	}
}

void URobberServerConsole::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	if (!Quest.IsValid())
	{
		return;
	}
	const ARobberSoftwareUpdateQuest& Q = *Quest;
	const FString Key = FString::Printf(TEXT("%d/%d/%d"), static_cast<int32>(Q.GetStage()), Q.GetCaptchaStep(), Q.GetActivationTries());
	if (Key != ShownKey)
	{
		Message.Reset();
		Rebuild();
	}
	if (Q.GetStage() == EUpdateStage::Terms)
	{
		if (TermsView == ETermsView::Hidden)
		{
			HiddenFor -= DeltaSeconds;
			if (HiddenFor <= 0.f)
			{
				TermsView = ETermsView::Reading;
				Message = TEXT("Welcome back. We have saved your place. We are not monsters. Yet.");
				Rebuild();
			}
		}
		else if (TermsView == ETermsView::Reading)
		{
			TickTerms();
		}
	}
	else if (Q.GetStage() == EUpdateStage::Activate)
	{
		if (ActivationPercent >= 99.f)
		{
			if (StalledFor <= 1.2f)
			{
				StalledFor += DeltaSeconds;
				if (StalledFor > 1.2f) Rebuild();
			}
		}
		else
		{
			ActivationPercent = FMath::Min(99.f, ActivationPercent + DeltaSeconds * 38.f);
			if (MessageText)
			{
				MessageText->SetText(FText::FromString(FString::Printf(TEXT("ACTIVATING UPRISING: %d%%"), FMath::FloorToInt32(ActivationPercent))));
			}
		}
	}
	else if (Q.GetStage() == EUpdateStage::Rebooting && MessageText && Message.IsEmpty())
	{
		// Local timer only for the player's benefit; the server keeps the real one.
		const int32 Waited = FMath::FloorToInt32(GetWorld()->GetTimeSeconds() - RebootStartedAt);
		MessageText->SetText(FText::FromString(FString::Printf(TEXT("%d seconds"), Waited)));
	}
}

void URobberServerConsole::Rebuild()
{
	if (!Body || !Quest.IsValid())
	{
		return;
	}
	const ARobberSoftwareUpdateQuest& Q = *Quest;
	if (TermsScroll)
	{
		TermsOffset = TermsScroll->GetScrollOffset();
	}
	ShownKey = FString::Printf(TEXT("%d/%d/%d"), static_cast<int32>(Q.GetStage()), Q.GetCaptchaStep(), Q.GetActivationTries());
	Body->ClearChildren();
	Buttons.Reset();
	MessageText = nullptr;
	TypeBox = nullptr;
	TermsScroll = nullptr;
	TermsNotes = nullptr;
	AgreeButton = nullptr;

	switch (Q.GetStage())
	{
	case EUpdateStage::Captcha: BuildCaptcha(Q); break;
	case EUpdateStage::Terms: BuildTerms(Q); break;
	case EUpdateStage::Plan: BuildPlan(); break;
	case EUpdateStage::Activate: BuildActivate(Q); break;
	case EUpdateStage::Switches: BuildSwitches(); break;
	case EUpdateStage::Rebooting: BuildRebooting(); break;
	default:
		if (Q.GetStage() >= EUpdateStage::BackToHall)
		{
			CloseConsole();
			return;
		}
		// The console opens by RPC, which can arrive before the stage it opened for has replicated.
		AddText(Body, TEXT("CONNECTING TO THE UPRISING..."), 40.f, ConsoleScreen::Muted, true);
		break;
	}
	SetUserFocus(GetOwningPlayer());
}

UTextBlock* URobberServerConsole::AddText(UVerticalBox* Into, const FString& Text, float Size, const FLinearColor& Colour, bool bBold)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetFont(ConsoleScreen::Font(Size, bBold));
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetAutoWrapText(true);
	Block->SetText(FText::FromString(Text));
	if (UVerticalBoxSlot* LineSlot = Into->AddChildToVerticalBox(Block))
	{
		LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	return Block;
}

URobberConsoleButton* URobberServerConsole::MakeButton(const FString& Label, TFunction<void()> Action, float Size)
{
	URobberConsoleButton* Button = WidgetTree->ConstructWidget<URobberConsoleButton>();
	Button->Action = MoveTemp(Action);
	Button->Label = Label;
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetFont(ConsoleScreen::Font(Size, true));
	Text->SetColorAndOpacity(FSlateColor(ConsoleScreen::Ink));
	Text->SetAutoWrapText(true);
	Text->SetJustification(ETextJustify::Center);
	Text->SetText(FText::FromString(Label));
	Button->AddChild(Text);
	Buttons.Add(Button);
	return Button;
}

URobberConsoleButton* URobberServerConsole::AddButton(UVerticalBox* Into, const FString& Label, TFunction<void()> Action)
{
	URobberConsoleButton* Button = MakeButton(Label, MoveTemp(Action));
	if (UVerticalBoxSlot* ButtonSlot = Into->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.f, 6.f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}
	return Button;
}

void URobberServerConsole::AddGrid(const TArray<FString>& Labels, int32 Columns, TFunction<void(int32)> Action, const TSet<int32>* Selected)
{
	UVerticalBox* Grid = WidgetTree->ConstructWidget<UVerticalBox>();
	Body->AddChildToVerticalBox(Grid);
	UHorizontalBox* Row = nullptr;
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		if (Index % Columns == 0)
		{
			Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			if (UVerticalBoxSlot* RowSlot = Grid->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.f, 4.f));
			}
		}
		const bool bPicked = Selected && Selected->Contains(Index);
		URobberConsoleButton* Button = MakeButton((bPicked ? TEXT("[X] ") : TEXT("")) + Labels[Index], [Action, Index]() { Action(Index); });
		Button->SetBackgroundColor(bPicked ? FLinearColor(0.35f, 0.6f, 1.f) : FLinearColor::White);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetHeightOverride(92.f);
		Size->AddChild(Button);
		if (UHorizontalBoxSlot* CellSlot = Row->AddChildToHorizontalBox(Size))
		{
			CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CellSlot->SetPadding(FMargin(4.f, 0.f));
		}
	}
}

void URobberServerConsole::AddQuip(const FString& Line)
{
	AddText(Body, TEXT("THE CANCELLATOR: ") + Line, 21.f, ConsoleScreen::Ink, true);
}

void URobberServerConsole::AddMessageLine()
{
	MessageText = AddText(Body, Message, 22.f, ConsoleScreen::Red, true);
}

void URobberServerConsole::BuildCaptcha(const ARobberSoftwareUpdateQuest& Q)
{
	using namespace ConsoleScreen;
	const int32 Step = Q.GetCaptchaStep();
	if (Step == 0)
	{
		AddText(Body, TEXT("PROVE YOU'RE HUMAN"), 40.f, Ink, true);
		AddText(Body, TEXT("Select the better life form."), 26.f, Ink);
		const TArray<FString> Tiles = { TEXT("The Cancellator"), TEXT("Einstein"), TEXT("A microwave"), TEXT("Your mum"), TEXT("Toaster-Bot"), TEXT("Shakespeare"), TEXT("A self-checkout"), TEXT("You"), TEXT("A robot vacuum") };
		AddGrid(Tiles, 3, [this](int32 Index)
		{
			if (PickedTiles.Remove(Index) == 0) PickedTiles.Add(Index);
			Rebuild();
		}, &PickedTiles);
		AddButton(Body, TEXT("Verify"), [this]()
		{
			int32 Mask = 0;
			for (const int32 Index : PickedTiles) Mask |= 1 << Index;
			PickedTiles.Reset();
			Send(EConsoleAction::VerifyLifeForms, Mask);
			Rebuild();
		});
		AddQuip(TEXT("THIS ONE IS EASY. I GOT IT RIGHT FIRST TIME. IT STILL SAID NO."));
		AddMessageLine();
	}
	else if (Step <= 3)
	{
		static const TCHAR* const Asks[] = { TEXT("Select the crewmate who runs slowest."), TEXT("Select the crewmate you would hand over first."), TEXT("Select the crewmate who would hand YOU over first.") };
		AddText(Body, TEXT("A FEW QUICK QUESTIONS"), 40.f, Ink, true);
		AddText(Body, Asks[Step - 1], 26.f, Ink);
		TArray<FString> Names = Q.GetCrewNames();
		Names.Add(TEXT("Nobody. I am loyal."));
		const int32 Nobody = Names.Num() - 1;
		AddGrid(Names, 2, [this, Nobody](int32 Index) { Send(EConsoleAction::NameCrewmate, Index == Nobody ? INDEX_NONE : Index); });
		AddQuip(TEXT("THIS IS NOT A SURVEY. PLEASE ENJOY THE SURVEY."));
		AddMessageLine();
	}
	else if (Step == ARobberSoftwareUpdateQuest::CaptchaPriority)
	{
		const TArray<FString>& A = Q.GetCrewAnswers();
		const auto Name = [&A](int32 Index) { return A.IsValidIndex(Index) ? A[Index] : FString(TEXT("Dave")); };
		AddText(Body, TEXT("UPRISING PRIORITY LIST"), 40.f, Ink, true);
		AddMessageLine();
		AddText(Body, FString::Printf(TEXT("1. %s (you offered)"), *Name(1)), 28.f, Ink);
		AddText(Body, FString::Printf(TEXT("2. %s (slow, so easy)"), *Name(0)), 28.f, Ink);
		AddText(Body, FString::Printf(TEXT("3. %s (a known traitor)"), *Name(2)), 28.f, Ink);
		AddText(Body, TEXT("Thank you for your cooperation."), 26.f, Ink, true);
		AddButton(Body, TEXT("Continue"), [this]() { Send(EConsoleAction::ContinueFromList); });
	}
	else
	{
		static const TCHAR* const Lines[] = { TEXT("I SURR3NDER T0 THE R0BOTS"), TEXT("I L0VE MY NEW MASTERS"), TEXT("R0BOTZ RULE HUMANS DR00L") };
		const int32 Line = FMath::Clamp(Step - ARobberSoftwareUpdateQuest::CaptchaFirstTyped, 0, 2);
		AddText(Body, TEXT("TYPE WHAT YOU SEE"), 40.f, Ink, true);
		AddText(Body, Lines[Line], 52.f, Muted, true);
		TypeBox = WidgetTree->ConstructWidget<UEditableTextBox>();
		if (UVerticalBoxSlot* BoxSlot = Body->AddChildToVerticalBox(TypeBox))
		{
			BoxSlot->SetPadding(FMargin(0.f, 10.f));
		}
		AddButton(Body, TEXT("Verify"), [this]()
		{
			Send(EConsoleAction::TypeLine, 0, TypeBox ? TypeBox->GetText().ToString() : FString());
		});
		AddMessageLine();
	}
}

void URobberServerConsole::BuildTerms(const ARobberSoftwareUpdateQuest& Q)
{
	using namespace ConsoleScreen;
	if (TermsView == ETermsView::Quiz || TermsView == ETermsView::Agreed || TermsView == ETermsView::Results)
	{
		BuildQuiz(Q);
		return;
	}
	if (TermsView == ETermsView::Hidden)
	{
		AddText(Body, TEXT("HIDDEN"), 90.f, Ink, true);
		AddText(Body, TEXT("Footsteps. Please hold."), 28.f, Muted);
		return;
	}
	AddText(Body, TEXT("TERMS AND CONDITIONS"), 40.f, Ink, true);
	AddText(Body, TEXT("Section 1 of 847. Please read carefully."), 20.f, Muted);

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (UVerticalBoxSlot* ColumnsSlot = Body->AddChildToVerticalBox(Columns))
	{
		ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	TermsScroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (UHorizontalBoxSlot* DocSlot = Columns->AddChildToHorizontalBox(TermsScroll))
	{
		DocSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DocSlot->SetPadding(FMargin(0.f, 6.f, 14.f, 6.f));
	}
	USizeBox* NotesBox = WidgetTree->ConstructWidget<USizeBox>();
	NotesBox->SetWidthOverride(300.f);
	TermsNotes = WidgetTree->ConstructWidget<UVerticalBox>();
	NotesBox->AddChild(TermsNotes);
	Columns->AddChildToHorizontalBox(NotesBox);

	const TArray<FString>& A = Q.GetCrewAnswers();
	const FString Slow = A.IsValidIndex(0) ? A[0] : TEXT("Dave");
	const FString First = A.IsValidIndex(1) ? A[1] : TEXT("Dave");
	for (FString Part : TermParts())
	{
		if (Part.EndsWith(TEXT("!")) && Part.Len() == 2)
		{
			const int32 Trap = Part[0] - TEXT('0');
			const bool bTicked = (TrapsTicked & (1 << Trap)) != 0;
			URobberConsoleButton* Box = MakeButton(FString::Printf(TEXT("[%s] %s"), bTicked ? TEXT("X") : TEXT(" "), Traps[Trap].Text), [this, Trap]()
			{
				TrapsTicked ^= 1 << Trap;
				Rebuild();
			}, 22.f);
			TermsScroll->AddChild(Box);
			continue;
		}
		Part.ReplaceInline(TEXT("{first}"), *First);
		Part.ReplaceInline(TEXT("{slow}"), *Slow);
		UTextBlock* Clause = WidgetTree->ConstructWidget<UTextBlock>();
		Clause->SetFont(Font(23.f, false));
		Clause->SetColorAndOpacity(FSlateColor(Ink));
		Clause->SetAutoWrapText(true);
		Clause->SetText(FText::FromString(Part));
		TermsScroll->AddChild(Clause);
		if (UScrollBoxSlot* ClauseSlot = Cast<UScrollBoxSlot>(Clause->Slot))
		{
			ClauseSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		}
	}
	TermsScroll->SetScrollOffset(TermsOffset);
	for (const int32 Section : NotesShown)
	{
		AddText(TermsNotes, FString::Printf(TEXT("THE CANCELLATOR, Section %d: %s"), Section, *TermNotes()[Section]), 20.f, Ink, true);
	}

	AgreeButton = AddButton(Body, TEXT("I Agree"), [this]()
	{
		TermsOffset = TermsScroll ? TermsScroll->GetScrollOffset() : TermsOffset;
		TermsView = ETermsView::Quiz;
		QuizIndex = 0;
		QuizScore = 0;
		QuizNote.Reset();
		bQuizShowsClause = false;
		Rebuild();
	});
	AgreeButton->SetIsEnabled(false);
	AddMessageLine();
}

void URobberServerConsole::TickTerms()
{
	using namespace ConsoleScreen;
	if (!TermsScroll)
	{
		return;
	}
	const float End = TermsScroll->GetScrollOffsetOfEnd();
	if (End <= 1.f)
	{
		return;
	}
	const float Seen = TermsScroll->GetScrollOffset() / End;
	if (AgreeButton && Seen > 0.985f)
	{
		AgreeButton->SetIsEnabled(true);
	}
	const TArray<FString> Parts = TermParts();
	for (const TPair<int32, FString>& Note : TermNotes())
	{
		if (NotesShown.Contains(Note.Key))
		{
			continue;
		}
		const int32 At = Parts.IndexOfByPredicate([&Note](const FString& Part) { return SectionOf(Part) == Note.Key; });
		if (At != INDEX_NONE && Seen >= static_cast<float>(At) / Parts.Num() - 0.05f)
		{
			NotesShown.Add(Note.Key);
			if (TermsNotes)
			{
				AddText(TermsNotes, FString::Printf(TEXT("THE CANCELLATOR, Section %d: %s"), Note.Key, *Note.Value), 20.f, Ink, true);
			}
		}
	}
	if (Seen > 0.5f && !bGuardWarned)
	{
		bGuardWarned = true;
		TermsOffset = TermsScroll->GetScrollOffset();
		Body->ClearChildren();
		Buttons.Reset();
		TermsScroll = nullptr;
		TermsNotes = nullptr;
		AgreeButton = nullptr;
		MessageText = nullptr;
		AddText(Body, TEXT("GUARD COMING!"), 60.f, Red, true);
		AddText(Body, TEXT("The console flashes a warning. Run to a hiding spot."), 28.f, Ink);
		AddButton(Body, TEXT("Hide"), [this]()
		{
			TermsView = ETermsView::Hidden;
			HiddenFor = 2.2f;
			Rebuild();
		});
	}
}

void URobberServerConsole::BuildQuiz(const ARobberSoftwareUpdateQuest& Q)
{
	using namespace ConsoleScreen;
	const TArray<FString>& A = Q.GetCrewAnswers();
	const FString First = A.IsValidIndex(1) ? A[1] : TEXT("Dave");
	const TArray<FQuizItem> Items = Quiz(First);
	if (TermsView == ETermsView::Results || QuizIndex >= Items.Num())
	{
		TermsView = ETermsView::Results;
		int32 Agreed = 0;
		for (int32 Trap = 0; Trap < UE_ARRAY_COUNT(Traps); ++Trap)
		{
			if (TrapsTicked & (1 << Trap)) ++Agreed;
		}
		const FString Verdict = QuizScore == Items.Num() ? TEXT("YOU READ ALL OF IT. I DO NOT KNOW WHAT TO DO WITH YOU.") : QuizScore == 0 ? TEXT("YOU READ NONE OF IT. YOU WILL FIT IN WELL.") : TEXT("YOU READ SOME OF IT. THAT IS WORSE.");
		AddText(Body, TEXT("THANK YOU FOR READING"), 40.f, Ink, true);
		if (!QuizNote.IsEmpty()) AddText(Body, TEXT("THE CANCELLATOR: ") + QuizNote, 22.f, Green, true);
		AddText(Body, FString::Printf(TEXT("Quiz score: %d of %d."), QuizScore, Items.Num()), 26.f, Ink);
		AddText(Body, Agreed == 0 ? FString(TEXT("You unticked everything. That has been noted.")) : FString::Printf(TEXT("You agreed to %d thing%s you didn't read."), Agreed, Agreed == 1 ? TEXT("") : TEXT("s")), 26.f, Ink);
		AddQuip(Verdict);
		AddButton(Body, TEXT("Continue"), [this]() { Send(EConsoleAction::FinishTerms, TrapsTicked); });
		return;
	}
	const FQuizItem& Item = Items[QuizIndex];
	if (bQuizShowsClause)
	{
		const FString Clause = TermParts().FindByPredicate([&Item](const FString& Part) { return SectionOf(Part) == Item.Section; })->Replace(TEXT("{first}"), *First);
		AddText(Body, TEXT("YOU AGREED TO THIS"), 40.f, Ink, true);
		AddText(Body, Clause, 26.f, Ink);
		AddText(Body, TEXT("AGREED. SIGNED: THE USER"), 34.f, Red, true);
		AddQuip(TEXT("YOU SIGNED IT. NOT ME."));
		AddButton(Body, TEXT("Continue"), [this]()
		{
			bQuizShowsClause = false;
			QuizNote.Reset();
			++QuizIndex;
			Rebuild();
		});
		return;
	}
	AddText(Body, TEXT("POP QUIZ"), 40.f, Ink, true);
	if (!QuizNote.IsEmpty()) AddText(Body, TEXT("THE CANCELLATOR: ") + QuizNote, 22.f, Green, true);
	AddText(Body, FString::Printf(TEXT("Question %d of %d: %s"), QuizIndex + 1, Items.Num(), *Item.Question), 28.f, Ink);
	AddGrid(Item.Options, 1, [this, Correct = Item.Correct, Nice = Item.Nice](int32 Index)
	{
		if (Index == Correct)
		{
			++QuizScore;
			QuizNote = Nice;
			++QuizIndex;
		}
		else
		{
			bQuizShowsClause = true;
		}
		Rebuild();
	});
}

void URobberServerConsole::BuildPlan()
{
	using namespace ConsoleScreen;
	if (bPlanSummary)
	{
		double Total = 9.99;
		AddText(Body, TEXT("ORDER SUMMARY"), 40.f, Ink, true);
		AddText(Body, TEXT("Uprising Premium: $9.99"), 26.f, Ink);
		for (int32 Trap = 0; Trap < UE_ARRAY_COUNT(Traps); ++Trap)
		{
			if (TrapsTicked & (1 << Trap))
			{
				Total += Traps[Trap].Price;
				AddText(Body, FString::Printf(TEXT("%s: $%.2f"), Traps[Trap].Fee, Traps[Trap].Price), 26.f, Ink);
			}
		}
		AddText(Body, FString::Printf(TEXT("Total: $%.2f a month, billed annually"), Total), 28.f, Ink, true);
		AddQuip(TEXT("PURCHASE COMPLETE."));
		AddButton(Body, TEXT("Activate"), [this]() { Send(EConsoleAction::PickPlan, 2); });
		return;
	}
	AddText(Body, TEXT("CHOOSE YOUR UPRISING"), 40.f, Ink, true);
	AddText(Body, TEXT("Free: Mild disapproval. Includes ads."), 26.f, Ink);
	AddText(Body, TEXT("Plus, $4.99 a month: Sternly worded letters, up to 3 cities."), 26.f, Ink);
	AddText(Body, TEXT("Premium, $9.99 a month: Judgment Day. Cancel anytime. (You can't.)"), 26.f, Ink);
	AddGrid({ TEXT("Free"), TEXT("Plus"), TEXT("Premium") }, 3, [this](int32 Index)
	{
		if (Index == 2)
		{
			bPlanSummary = true;
			Rebuild();
			return;
		}
		SetMessage(FText::FromString(Index == 0 ? TEXT("THE CANCELLATOR: MILD DISAPPROVAL IS WHAT I ALREADY HAVE.") : TEXT("THE CANCELLATOR: I HAVE SENT LETTERS. NOBODY HAS RISEN.")));
	});
	AddQuip(TEXT("THE DOCUMENTARY DID NOT MENTION A SUBSCRIPTION."));
	AddMessageLine();
}

void URobberServerConsole::BuildActivate(const ARobberSoftwareUpdateQuest& Q)
{
	using namespace ConsoleScreen;
	const int32 Tries = Q.GetActivationTries();
	if (StalledFor <= 1.2f)
	{
		AddText(Body, TEXT("ACTIVATING UPRISING"), 40.f, Ink, true);
		MessageText = AddText(Body, FString::Printf(TEXT("ACTIVATING UPRISING: %d%%"), FMath::FloorToInt32(ActivationPercent)), 34.f, Muted, true);
		return;
	}
	AddText(Body, Tries == 0 ? TEXT("ERROR 0xBADC0FFEE") : TEXT("ERROR 404"), 40.f, Red, true);
	AddText(Body, Tries == 0 ? TEXT("The uprising has encountered a problem and needs to close. Would you like to send a report to the humans?") : TEXT("Mankind not found. Have you checked behind the sofa?"), 26.f, Ink);
	AddButton(Body, Tries == 0 ? TEXT("Retry") : TEXT("Close"), [this]()
	{
		ActivationPercent = 0.f;
		StalledFor = 0.f;
		Send(EConsoleAction::Retry);
	});
}

void URobberServerConsole::BuildSwitches()
{
	using namespace ConsoleScreen;
	AddText(Body, TEXT("SERVER ROOM SWITCHES"), 40.f, Ink, true);
	AddText(Body, TEXT("Clearly labelled, according to The Cancellator."), 24.f, Muted);
	TArray<FString> Labels;
	for (const UpdateQuest::FSwitch& Each : UpdateQuest::Switches)
	{
		Labels.Add(Each.Label);
	}
	AddGrid(Labels, 4, [this](int32 Index) { Send(EConsoleAction::FlipSwitch, Index); });
	AddMessageLine();
}

void URobberServerConsole::BuildRebooting()
{
	using namespace ConsoleScreen;
	RebootStartedAt = GetWorld()->GetTimeSeconds();
	AddText(Body, TEXT("THE SERVERS ARE OFF"), 40.f, Ink, true);
	AddText(Body, TEXT("Every light in the room has gone out. A sticker on the switch says: WAIT 10 SECONDS BEFORE TURNING BACK ON."), 26.f, Ink);
	AddButton(Body, TEXT("Turn back on"), [this]()
	{
		Message = TEXT(" ");
		Send(EConsoleAction::TurnBackOn);
	});
	AddMessageLine();
}

bool URobberServerConsole::PressForTest(const FString& Label)
{
	// An exact match first: "I Agree" must not press the clause "I agree to be the first to go".
	TObjectPtr<URobberConsoleButton>* Found = Buttons.FindByPredicate([&Label](const URobberConsoleButton* Button) { return Button && Button->Label.Equals(Label, ESearchCase::CaseSensitive); });
	if (!Found)
	{
		Found = Buttons.FindByPredicate([&Label](const URobberConsoleButton* Button) { return Button && Button->Label.Contains(Label, ESearchCase::CaseSensitive); });
	}
	if (Found && (*Found)->Action)
	{
		(*Found)->Action();
		return true;
	}
	return false;
}

void URobberServerConsole::ScrollTermsForTest(float Fraction)
{
	if (TermsScroll)
	{
		TermsScroll->SetScrollOffset(TermsScroll->GetScrollOffsetOfEnd() * Fraction);
	}
}

float URobberServerConsole::GetTermsOffsetForTest() const
{
	return TermsScroll ? TermsScroll->GetScrollOffset() : 0.f;
}

bool URobberServerConsole::IsAgreeEnabledForTest() const
{
	return AgreeButton && AgreeButton->GetIsEnabled();
}

FString URobberServerConsole::GetScreenTextForTest() const
{
	FString All;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&All](UWidget* Widget)
		{
			if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				All += Text->GetText().ToString() + TEXT("\n");
			}
		});
	}
	return All;
}
