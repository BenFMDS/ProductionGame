#include "Heist/RobberSoftwareUpdate.h"

#include "Algo/Transform.h"
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
	UseScript(TEXT("DT_SoftwareUpdateScript"));
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
	const FName BotSays(TEXT("Bot")), BuddySays(TEXT("Buddy"));
	if (Bot)
	{
		UDialogueDefinition* Script = NewScript(SpeakerName(BotSays));
		AddScene(Script, TEXT("Brief"), Briefed);
		AddScene(Script, TEXT("End"), Cancelled);
		for (const FName Id : { FName(TEXT("WaitDisk")), FName(TEXT("WaitConsole")), FName(TEXT("Chat0")), FName(TEXT("Chat1")) })
		{
			AddSingle(Script, Id, BotSays, Line(Id));
		}
		GiveScript(Bot, Script, TEXT("Chat0"), { Line(TEXT("Chat0")) })->EntryOverride.BindUObject(this, &ARobberSoftwareUpdateQuest::EntryFor, Bot.Get());
	}
	if (Buddy)
	{
		UDialogueDefinition* Script = NewScript(SpeakerName(BuddySays));
		AddSingle(Script, TEXT("Hi"), BuddySays, Line(TEXT("BuddyHi")));
		GiveScript(Buddy, Script, TEXT("Hi"), { Line(TEXT("BuddyHi")) });
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
	Stamp(false, Line(TEXT("Name")), Line(TEXT("NewSub")));
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
		Toast(Line(TEXT("GotKey")));
	}
	else if (Event == Cancelled && Stage == EUpdateStage::BackToHall)
	{
		const FText Code = FText::AsCultureInvariant(FString::FromInt(AccessCode));
		if (URobberJournalComponent* Journal = IsValid(QuestOwner) ? URobberJournalComponent::Of(QuestOwner) : nullptr)
		{
			Journal->LearnFact(Bot, SpeakerName(TEXT("Bot")), FText::Format(Line(TEXT("Receipt")), Code));
		}
		++PayoutsMade;
		SetStage(EUpdateStage::Done);
		Toast(FText::Format(Line(TEXT("ReceiptToast")), Code));
		Stamp(true, Line(TEXT("Name")), Line(TEXT("DoneSub")));
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
		Toast(Line(TEXT("BuddyArchive")));
	}
	if (!bSaidServerRoom && Stage == EUpdateStage::ReachServerRoom && IsOwnerInRoom(TEXT("Server Room")))
	{
		bSaidServerRoom = true;
		Toast(Line(TEXT("BuddyServer")));
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
			ARobberController::Tell(Player, Message);
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
		static const TCHAR* const SoldOut[] = { TEXT("SoldSlow"), TEXT("SoldFirst"), TEXT("SoldThird") };
		TellCrewmate(Name, FText::Format(Line(SoldOut[CaptchaStep - 1]), Betrayer));
		Say(FText::FromString(CaptchaStep == 1 ? Name + TEXT(" has been told you said that.")
			: CaptchaStep == 2 ? Name + TEXT(" has been told you said that, too.") : FString(TEXT("Correct. They already did."))));
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
		const int32 Typed = CaptchaStep - CaptchaFirstTyped;
		if (Typed >= UE_ARRAY_COUNT(TypedLines)) return;
		if (Clean(Text) != Clean(TypedLines[Typed]))
		{
			Say(FText::FromString(TEXT("Please mean it.")));
			return;
		}
		++CaptchaStep;
		if (CaptchaStep - CaptchaFirstTyped >= UE_ARRAY_COUNT(TypedLines))
		{
			PayBonusReward(Line(TEXT("FromBot")));
			++PayoutsMade;
			SetStage(EUpdateStage::Terms);
			Toast(Line(TEXT("Verified")));
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
			Toast(Line(TEXT("OffAndOn")));
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
		Toast(Line(TEXT("BackOn")));
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
	if (PropId == Disk && Stage == EUpdateStage::FindDisk) return Line(TEXT("PromptDisk"));
	if (PropId == Console && Stage >= EUpdateStage::ReachServerRoom && Stage <= EUpdateStage::Activate) return Line(TEXT("PromptConsole"));
	if (PropId == Wall && (Stage == EUpdateStage::Switches || Stage == EUpdateStage::Rebooting)) return Line(TEXT("PromptWall"));
	return FText::GetEmpty();
}

void ARobberSoftwareUpdateQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace UpdateQuest;
	if (PropId == Disk)
	{
		Prop->SetShown(false);
		SetStage(EUpdateStage::ReachServerRoom);
		Toast(Line(TEXT("GotDisk")));
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
	Note.Title = Line(TEXT("Name"));
	Note.Footer = FText::Format(Line(TEXT("Foot")), PayoutsMade);
	TArray<FText> Steps;
	for (int32 Step = 0; Step <= 6; ++Step)
	{
		Steps.Add(Line(*FString::Printf(TEXT("L%d"), Step)));
	}
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
		Note.Lines.Add(NoteLine(Steps[DoneCount - 1], true));
	}
	if (Steps.IsValidIndex(DoneCount))
	{
		Note.Lines.Add(NoteLine(Steps[DoneCount]));
	}
	if (Stage >= EUpdateStage::ReachServerRoom && Stage <= EUpdateStage::Plan)
	{
		const auto Item = [this](EUpdateStage Doing, const TCHAR* Key) { return NoteLine(Line(Key), Stage > Doing); };
		Note.ErrandTitle = Line(TEXT("Errand"));
		Note.ErrandLines = { Item(EUpdateStage::Captcha, TEXT("E0")), Item(EUpdateStage::Terms, TEXT("E1")), Item(EUpdateStage::Plan, TEXT("E2")) };
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

	/** The "Terms" scene. A part that's a digit from 0 to 4 and '!' is that trap's tickbox. {first} and {slow} are the crewmates the player named. */
	TArray<FString> TermParts(const ARobberSoftwareUpdateQuest& Q)
	{
		TArray<FString> Parts;
		Algo::Transform(Q.SceneLines(TEXT("Terms")), Parts, [](const FText& Part) { return Part.ToString(); });
		return Parts;
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

	FString StateKey(const ARobberSoftwareUpdateQuest& Q) { return FString::Printf(TEXT("%d/%d/%d"), static_cast<int32>(Q.GetStage()), Q.GetCaptchaStep(), Q.GetActivationTries()); }
	/** The crewmate the player named for a question, or a stand-in before they've answered. */
	FString Named(const ARobberSoftwareUpdateQuest& Q, int32 Index) { return Q.GetCrewAnswers().IsValidIndex(Index) ? Q.GetCrewAnswers()[Index] : FString(TEXT("Dave")); }
	FString CancellatorNote(int32 Section, const FString& Note) { return FString::Printf(TEXT("THE CANCELLATOR, Section %d: %s"), Section, *Note); }
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
		bPreviousCursor = ARobberController::EnterModal(PC, this);
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
		ARobberController::LeaveModal(PC, bPreviousCursor);
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
	if (ConsoleScreen::StateKey(Q) != ShownKey)
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
	ShownKey = ConsoleScreen::StateKey(Q);
	ClearBody();

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

void URobberServerConsole::ClearBody()
{
	if (TermsScroll)
	{
		TermsOffset = TermsScroll->GetScrollOffset();
	}
	Body->ClearChildren();
	Buttons.Reset();
	MessageText = nullptr;
	TypeBox = nullptr;
	TermsScroll = nullptr;
	TermsNotes = nullptr;
	AgreeButton = nullptr;
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
		TArray<FString> Tiles;
		Algo::Transform(UpdateQuest::LifeForms, Tiles, [](const TCHAR* Text) { return FString(Text); });
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
		AddText(Body, TEXT("UPRISING PRIORITY LIST"), 40.f, Ink, true);
		AddMessageLine();
		AddText(Body, FString::Printf(TEXT("1. %s (you offered)"), *Named(Q, 1)), 28.f, Ink);
		AddText(Body, FString::Printf(TEXT("2. %s (slow, so easy)"), *Named(Q, 0)), 28.f, Ink);
		AddText(Body, FString::Printf(TEXT("3. %s (a known traitor)"), *Named(Q, 2)), 28.f, Ink);
		AddText(Body, TEXT("Thank you for your cooperation."), 26.f, Ink, true);
		AddButton(Body, TEXT("Continue"), [this]() { Send(EConsoleAction::ContinueFromList); });
	}
	else
	{
		const int32 Line = FMath::Clamp(Step - ARobberSoftwareUpdateQuest::CaptchaFirstTyped, 0, UE_ARRAY_COUNT(UpdateQuest::TypedLines) - 1);
		AddText(Body, TEXT("TYPE WHAT YOU SEE"), 40.f, Ink, true);
		AddText(Body, UpdateQuest::TypedLines[Line], 52.f, Muted, true);
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

	const FString Slow = Named(Q, 0);
	const FString First = Named(Q, 1);
	for (FString Part : TermParts(Q))
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
		AddText(TermsNotes, CancellatorNote(Section, TermNotes()[Section]), 20.f, Ink, true);
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
	const TArray<FString> Parts = TermParts(*Quest);
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
				AddText(TermsNotes, CancellatorNote(Note.Key, Note.Value), 20.f, Ink, true);
			}
		}
	}
	if (Seen > 0.5f && !bGuardWarned)
	{
		bGuardWarned = true;
		ClearBody();
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
	const FString First = Named(Q, 1);
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
		const FString Clause = TermParts(Q).FindByPredicate([&Item](const FString& Part) { return SectionOf(Part) == Item.Section; })->Replace(TEXT("{first}"), *First);
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
	Algo::Transform(UpdateQuest::Switches, Labels, [](const UpdateQuest::FSwitch& Each) { return FString(Each.Label); });
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
