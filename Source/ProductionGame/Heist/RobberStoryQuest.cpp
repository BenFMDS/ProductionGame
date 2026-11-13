#include "Heist/RobberStoryQuest.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberExhibitTalk.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberVault.h"
#include "Net/UnrealNetwork.h"
#include "ProductionGameCharacter.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogStoryQuest, Log, All);

const FName ARobberStoryQuest::NpcTag(TEXT("StoryNpc"));

// --- Story quest ---

ARobberStoryQuest::ARobberStoryQuest()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorHiddenInGame(true);
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARobberStoryQuest::UseScript(const TCHAR* Name)
{
	ConstructorHelpers::FObjectFinder<UDataTable> Table(*FString::Printf(TEXT("/Game/OurStuff/Core/Dialogue/StoryScripts/%s.%s"), Name, Name));
	ScriptTable = Table.Object;
}

void ARobberStoryQuest::BeginPlay()
{
	Super::BeginPlay();
	// Before the director deals: its deal waits for the level to be visible, which is after this.
	TipLine = Line(TEXT("Tip"));
	TipTakenLine = Line(TEXT("TipTaken"));
	TipJournal = Line(TEXT("TipJournal"));
}

FText ARobberStoryQuest::Line(FName Key) const
{
	if (const FRobberScriptRow* Row = ScriptTable ? ScriptTable->FindRow<FRobberScriptRow>(Key, TEXT("StoryQuest"), false) : nullptr)
	{
		return Row->Text;
	}
	UE_LOG(LogStoryQuest, Error, TEXT("%s: no script row %s"), *GetClass()->GetName(), *Key.ToString());
	return FText::FromName(Key);
}

TArray<FText> ARobberStoryQuest::SceneLines(FName Scene) const
{
	TArray<FText> Lines;
	if (ScriptTable)
	{
		ScriptTable->ForeachRow<FRobberScriptRow>(TEXT("StoryQuest"), [&Lines, Scene](const FName&, const FRobberScriptRow& Row)
		{
			if (Row.Scene == Scene)
			{
				Lines.Add(Row.Text);
			}
		});
	}
	return Lines;
}

FText ARobberStoryQuest::SpeakerName(FName Speaker) const
{
	const FName Key(*(TEXT("Speaker") + Speaker.ToString()));
	const FRobberScriptRow* Row = ScriptTable ? ScriptTable->FindRow<FRobberScriptRow>(Key, TEXT("StoryQuest"), false) : nullptr;
	return Row ? Row->Text : FText::FromName(Speaker);
}

UDialogueDefinition* ARobberStoryQuest::NewScript(const FText& Speaker)
{
	UDialogueDefinition* Script = NewObject<UDialogueDefinition>(this);
	Generated.Add(Script);
	Script->DialogueType = EDialogueType::DialogueOnly;
	Script->bIncludeQuestChoices = false;
	Script->SpeakerName = Speaker;
	return Script;
}

void ARobberStoryQuest::AddScene(UDialogueDefinition* Script, FName Scene, FName LastEvent, FName Then) const
{
	const FName You(TEXT("You"));
	TArray<const FRobberScriptRow*> Rows;
	if (ScriptTable)
	{
		ScriptTable->ForeachRow<FRobberScriptRow>(TEXT("StoryQuest"), [&Rows, Scene](const FName&, const FRobberScriptRow& Row)
		{
			if (Row.Scene == Scene)
			{
				Rows.Add(&Row);
			}
		});
	}
	const auto NodeId = [&Scene](int32 Position) { return FName(*FString::Printf(TEXT("%s%d"), *Scene.ToString(), Position)); };
	int32 Position = 0;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Rows[Index]->Speaker == You)
		{
			continue;
		}
		bool bHasNext = false;
		for (int32 Later = Index + 1; Later < Rows.Num() && !bHasNext; ++Later)
		{
			bHasNext = Rows[Later]->Speaker != You;
		}
		const bool bAnswered = Rows.IsValidIndex(Index + 1) && Rows[Index + 1]->Speaker == You;
		FDialogueNode Node;
		Node.ID = NodeId(++Position);
		Node.Speaker = SpeakerName(Rows[Index]->Speaker);
		Node.Text = Rows[Index]->Text;
		FDialogueChoice& Choice = Node.Choices.AddDefaulted_GetRef();
		Choice.Text = bAnswered ? Rows[Index + 1]->Text : Line(bHasNext ? FName(TEXT("GoOn")) : FName(*(Scene.ToString() + TEXT("End"))));
		Choice.NextNode = bHasNext ? NodeId(Position + 1) : Then;
		Choice.StoryEvent = bHasNext ? NAME_None : LastEvent;
		Script->Nodes.Add(Node);
	}
}

void ARobberStoryQuest::AddSingle(UDialogueDefinition* Script, FName Id, FName Speaker, const FText& Text, FName Answer, FName Next, FName Event) const
{
	FDialogueNode& Node = Script->Nodes.AddDefaulted_GetRef();
	Node.ID = Id;
	Node.Speaker = SpeakerName(Speaker);
	Node.Text = Text;
	FDialogueChoice& Choice = Node.Choices.AddDefaulted_GetRef();
	Choice.Text = Line(Answer);
	Choice.NextNode = Next;
	Choice.StoryEvent = Event;
}

UDialogueComponent* ARobberStoryQuest::GiveScript(AActor* Talker, UDialogueDefinition* Script, FName Entry, const TArray<FText>& SmallTalk) const
{
	Script->EntryNode = Entry;
	UDialogueComponent* Dialogue = Talker->FindComponentByClass<UDialogueComponent>();
	Dialogue->Dialogue = Script;
	Dialogue->SmallTalkLines = SmallTalk;
	Dialogue->OfferedQuests.Reset();
	return Dialogue;
}

FRobberStoryLine ARobberStoryQuest::NoteLine(const FText& Text, bool bDone, bool bWarning)
{
	FRobberStoryLine Note;
	Note.Text = Text;
	Note.bDone = bDone;
	Note.bWarning = bWarning;
	return Note;
}

int32 ARobberStoryQuest::VaultDigit(int32 Index) const
{
	for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel() && It->GetCode().IsValidIndex(Index))
		{
			return It->GetCode()[Index];
		}
	}
	return INDEX_NONE;
}

void ARobberStoryQuest::TellOwnerDigit(AActor* Talker, const FText& Speaker, int32 Index, int32 Value) const
{
	if (URobberJournalComponent* Journal = IsValid(QuestOwner) ? URobberJournalComponent::Of(QuestOwner) : nullptr)
	{
		Journal->LearnVaultDigit(Talker, Speaker, Index, Value, ARobberVaultDoor::CodeLength);
	}
}

void ARobberStoryQuest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberStoryQuest, QuestOwner);
}

FString ARobberStoryQuest::GetDevOwnerName() const
{
	return IsValid(QuestOwner) ? QuestOwner->GetPlayerName() : TEXT("nobody");
}

bool ARobberStoryQuest::CanTakeTip(const APlayerState* Player) const
{
	// IsValid: an owner who has left the game frees the quest up for whoever hears the tip next.
	return Player && (!IsValid(QuestOwner) || QuestOwner == Player);
}

void ARobberStoryQuest::TakeTip(APlayerState* Player)
{
	if (!HasAuthority() || !CanTakeTip(Player) || QuestOwner == Player)
	{
		return;
	}
	QuestOwner = Player;
	ForceNetUpdate();
	OnQuestStarted();
}

UPlayerQuestComponent* ARobberStoryQuest::OwnerQuests() const
{
	return IsValid(QuestOwner) ? UPlayerQuestComponent::Of(QuestOwner) : nullptr;
}

ARobberController* ARobberStoryQuest::OwnerController() const
{
	return IsValid(QuestOwner) ? Cast<ARobberController>(QuestOwner->GetOwningController()) : nullptr;
}

void ARobberStoryQuest::RefreshNote()
{
	if (UPlayerQuestComponent* Quests = OwnerQuests())
	{
		Quests->SetStoryNote(BuildNote());
	}
}

void ARobberStoryQuest::Stamp(bool bComplete, const FText& Title, const FText& Subtitle)
{
	if (UPlayerQuestComponent* Quests = OwnerQuests())
	{
		Quests->ClientStoryStamp(bComplete, Title, Subtitle);
	}
}

void ARobberStoryQuest::ClearNoteAfterStamp()
{
	GetWorldTimerManager().SetTimer(ClearNoteTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UPlayerQuestComponent* Quests = OwnerQuests())
		{
			Quests->SetStoryNote(FRobberStoryNote());
		}
	}), 2.5f, false);
}

void ARobberStoryQuest::Toast(const FText& Message) const
{
	if (ARobberController* Controller = OwnerController())
	{
		Controller->ClientHeistMessage(Message);
	}
}

void ARobberStoryQuest::PayBonusReward(const FText& FromWho)
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
		if (Reward.Kind == ERobberRewardKind::ExtraTime)
		{
			State->AddGuardsTime(Reward.Value);
			State->TellCrew(FText::Format(NSLOCTEXT("StoryQuest", "PaidTime", "PAYOUT · {0} delayed the guards by {1} seconds."), FromWho, FMath::RoundToInt32(Reward.Value)));
		}
		else
		{
			State->CoolHeat(Reward.Value);
			State->TellCrew(FText::Format(NSLOCTEXT("StoryQuest", "PaidHeat", "PAYOUT · {0} had a word. Heat is down by {1}."), FromWho, FMath::RoundToInt32(Reward.Value)));
		}
		return;
	}
}

ARobberStoryProp* ARobberStoryQuest::FindProp(FName PropId) const
{
	for (TActorIterator<ARobberStoryProp> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel() && It->PropId == PropId)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ARobberStoryQuest::IsOwnerInRoom(const FString& RoomName) const
{
	const APawn* Pawn = IsValid(QuestOwner) ? QuestOwner->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}
	for (TActorIterator<ARobberRoomInfo> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel() && It->RoomName.ToString() == RoomName && It->Contains(Pawn->GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

// --- Prop ---

ARobberStoryProp::ARobberStoryProp()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARobberStoryProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberStoryProp, bShown);
}

void ARobberStoryProp::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bStartsHidden)
	{
		SetShown(false);
	}
}

void ARobberStoryProp::SetShown(bool bInShown)
{
	bShown = bInShown;
	OnRep_Shown();
	ForceNetUpdate();
}

void ARobberStoryProp::OnRep_Shown()
{
	SetActorHiddenInGame(!bShown);
	SetActorEnableCollision(bShown);
}

ARobberStoryQuest* ARobberStoryProp::FindStory() const
{
	for (TActorIterator<ARobberStoryQuest> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel() && It->HandlesProp(PropId))
		{
			return *It;
		}
	}
	return nullptr;
}

FText ARobberStoryProp::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	const ARobberStoryQuest* Story = FindStory();
	return Story && bShown ? Story->GetPropPrompt(PropId, Viewer) : FText::GetEmpty();
}

FText ARobberStoryProp::GetLabel(const ARobberGameState* GameState) const
{
	const ARobberStoryQuest* Story = bLiveLabel ? FindStory() : nullptr;
	return Story ? Story->GetPropLabel(PropId) : Super::GetLabel(GameState);
}

void ARobberStoryProp::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	ARobberStoryQuest* Story = FindStory();
	if (Story && bShown && Story->IsQuestOwner(Player) && !Story->GetPropPrompt(PropId, Player).IsEmpty())
	{
		Story->OnPropUsed(PropId, this, Pawn, Player);
	}
}

// --- Tina ---

ARobberTina::ARobberTina()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetMobility(EComponentMobility::Movable);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Head);
	Body->SetMobility(EComponentMobility::Movable);
	Gaze = CreateDefaultSubobject<USpotLightComponent>(TEXT("Gaze"));
	Gaze->SetupAttachment(Head);
	Gaze->SetMobility(EComponentMobility::Movable);
	Gaze->SetIntensityUnits(ELightUnits::Lumens);
	Gaze->SetIntensity(12000.f);
	Gaze->SetLightColor(FLinearColor(1.f, 0.18f, 0.08f));
	Gaze->SetInnerConeAngle(20.f);
	Gaze->SetOuterConeAngle(25.f);
	Gaze->SetAttenuationRadius(1400.f);
	// Above her head and clear of her body, so the beam isn't shadowed by her own mesh.
	Gaze->SetRelativeLocation(FVector(40., 0., 320.));
	// Steep enough to land on the floor in the middle of what she can see.
	Gaze->SetRelativeRotation(FRotator(-28., 0., 0.));
	Gaze->SetCastShadows(false);
}

void ARobberTina::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(LookTimer, this, &ARobberTina::LookForPlayers, 0.1f, true);
	}
}

double ARobberTina::SweepTime() const
{
	// Server time, so every machine sees her looking the same way without sending it.
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float ARobberTina::CurrentYaw() const
{
	const double Sweep = FMath::Max(0.1f, SweepSeconds);
	const double Pause = FMath::Max(0.f, PauseSeconds);
	const double Cycle = 2. * (Sweep + Pause);
	const double T = FMath::Fmod(SweepTime(), Cycle);
	double Alpha;
	if (T < Sweep) Alpha = T / Sweep;
	else if (T < Sweep + Pause) Alpha = 1.;
	else if (T < 2. * Sweep + Pause) Alpha = 1. - (T - Sweep - Pause) / Sweep;
	else Alpha = 0.;
	return FMath::Lerp(-SweepAngle, SweepAngle, FMath::SmoothStep(0., 1., Alpha));
}

void ARobberTina::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Head->SetRelativeRotation(FRotator(0., CurrentYaw(), 0.));
}

FVector ARobberTina::GetGazeDirection() const
{
	return (GetActorRotation() + FRotator(0., CurrentYaw(), 0.)).Vector();
}

bool ARobberTina::CanSee(const APawn* Pawn) const
{
	// Chest height at both ends, so the bone piles in her room really do hide you.
	const FVector Eye = GetActorLocation() + FVector(0., 0., EyeHeight);
	const FVector Target = Pawn->GetActorLocation();
	FVector ToTarget = Target - Eye;
	ToTarget.Z = 0.;
	const double Distance = ToTarget.Size();
	if (Distance > ViewDistance || Distance < KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const double Dot = FVector::DotProduct(ToTarget / Distance, GetGazeDirection().GetSafeNormal2D());
	if (Dot < FMath::Cos(FMath::DegreesToRadians(ViewAngle)))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Pawn);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);
}

void ARobberTina::LookForPlayers()
{
	const ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	// The exhibits (Tina included) stay asleep until the museum comes alive.
	if (!State || !State->CanTalkToExhibits())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	for (APlayerState* Player : State->PlayerArray)
	{
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		const double* Safe = Player ? SafeUntil.Find(Player) : nullptr;
		const AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(Pawn);
		if (!Pawn || Pawn->IsHidden() || (Robber && Robber->IsDevImmune()) || (Safe && Now < *Safe) || !CanSee(Pawn))
		{
			continue;
		}
		bool bCares = true;
		for (TActorIterator<ARobberStoryQuest> It(GetWorld()); It && bCares; ++It)
		{
			bCares = It->GetLevel() != GetLevel() || It->WatcherCares(WatcherId, Player);
		}
		if (bCares)
		{
			Catch(Pawn, Player);
		}
	}
}

void ARobberTina::Catch(APawn* Pawn, APlayerState* Player)
{
	SafeUntil.Add(Player, GetWorld()->GetTimeSeconds() + BlackoutSeconds + 3.f);
	FVector Away = Pawn->GetActorLocation() - GetActorLocation();
	Away.Z = 0.;
	const FVector Launch = Away.GetSafeNormal() * KnockBackSpeed + FVector(0., 0., 350.);
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->LaunchCharacter(Launch, true, true);
	}
	const AController* Controller = Pawn->GetController();
	if (UPlayerQuestComponent* Quests = Controller ? Controller->FindComponentByClass<UPlayerQuestComponent>() : nullptr)
	{
		Quests->ClientStoryCaught(BlackoutSeconds, Launch);
	}
	for (TActorIterator<ARobberStoryQuest> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel())
		{
			It->OnPlayerCaught(Pawn, Player, WatcherId);
		}
	}
}

// --- Stone Age Small Talk ---

namespace CaveMen
{
	const FName Reginald(TEXT("Reginald")), Montague(TEXT("Montague")), Ug(TEXT("Ug"));
	const FName GiftShop(TEXT("GiftShop")), Book(TEXT("Book")), Egg(TEXT("Egg")), Campfire(TEXT("Campfire")), Flame(TEXT("Flame"));
	const FName Locker1(TEXT("Locker1")), Locker2(TEXT("Locker2")), Locker3(TEXT("Locker3"));
	const FName HeardGibberish(TEXT("HeardGibberish")), GotEggJob(TEXT("GotEggJob")), EggDelivered(TEXT("EggDelivered"));
	const FName GotFireJob(TEXT("GotFireJob")), FireDone(TEXT("FireDone"));
	const FName Unknown(TEXT("???"));

	FText Who(FName Name) { return FText::FromName(Name); }
}

ARobberCaveMenQuest::ARobberCaveMenQuest()
{
	UseScript(TEXT("DT_CaveMenScript"));
}

void ARobberCaveMenQuest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberCaveMenQuest, Stage);
	DOREPLIFETIME(ARobberCaveMenQuest, bGiftShopChecked);
	DOREPLIFETIME(ARobberCaveMenQuest, bReachedArchive);
	DOREPLIFETIME(ARobberCaveMenQuest, bReachedStaffArea);
	DOREPLIFETIME(ARobberCaveMenQuest, bBrokeInLocker);
	DOREPLIFETIME(ARobberCaveMenQuest, OpenedLockers);
	DOREPLIFETIME(ARobberCaveMenQuest, bBookFound);
}

void ARobberCaveMenQuest::OnRep_BookFound()
{
	UpdateTalkPrompts();
}

FName ARobberCaveMenQuest::NameOf(const AActor* Man) const
{
	for (const FName Each : { CaveMen::Reginald, CaveMen::Montague, CaveMen::Ug })
	{
		if (Man->Tags.Contains(Each))
		{
			return Each;
		}
	}
	return CaveMen::Ug;
}

void ARobberCaveMenQuest::UpdateTalkPrompts()
{
	// The prompt is a Blueprint variable read on each machine, so every machine sets it for itself.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() != GetLevel() || !It->Tags.Contains(NpcTag) || !(It->Tags.Contains(CaveMen::Reginald) || It->Tags.Contains(CaveMen::Montague) || It->Tags.Contains(CaveMen::Ug)))
		{
			continue;
		}
		for (TFieldIterator<FStrProperty> Property(It->GetClass()); Property; ++Property)
		{
			if (Property->GetName().Replace(TEXT(" "), TEXT("")).StartsWith(TEXT("InteractText")))
			{
				Property->SetPropertyValue_InContainer(*It, TEXT("Talk to ") + (bBookFound ? NameOf(*It) : CaveMen::Unknown).ToString());
			}
		}
	}
}

void ARobberCaveMenQuest::BeginPlay()
{
	Super::BeginPlay();
	UpdateTalkPrompts();
	if (!HasAuthority())
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const bool bCaveMan = It->Tags.Contains(CaveMen::Reginald) || It->Tags.Contains(CaveMen::Montague) || It->Tags.Contains(CaveMen::Ug);
		if (It->GetLevel() == GetLevel() && bCaveMan && It->FindComponentByClass<UDialogueComponent>())
		{
			CaveMen.Add(*It);
		}
	}
	const FName Lockers[] = { CaveMen::Locker1, CaveMen::Locker2, CaveMen::Locker3 };
	LighterLocker = Lockers[FMath::RandHelper(UE_ARRAY_COUNT(Lockers))];
	BuildDialogues();
	EventHandle = UDialogueComponent::OnDialogueEvent.AddUObject(this, &ARobberCaveMenQuest::OnDialogueEvent);
	GetWorldTimerManager().SetTimer(RoomTimer, this, &ARobberCaveMenQuest::CheckRooms, 0.25f, true);
}

void ARobberCaveMenQuest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDialogueComponent::OnDialogueEvent.Remove(EventHandle);
	Super::EndPlay(EndPlayReason);
}

AActor* ARobberCaveMenQuest::GetTipSubject() const
{
	return CaveMen.IsEmpty() ? nullptr : CaveMen[0].Get();
}

void ARobberCaveMenQuest::GetRequiredActors(TArray<AActor*>& OutActors) const
{
	for (AActor* Man : CaveMen)
	{
		OutActors.Add(Man);
	}
	for (const FName Id : { CaveMen::GiftShop, CaveMen::Book, CaveMen::Egg, CaveMen::Campfire, LighterLocker })
	{
		if (ARobberStoryProp* Prop = FindProp(Id))
		{
			OutActors.Add(Prop);
		}
	}
}

void ARobberCaveMenQuest::BuildDialogues()
{
	using namespace CaveMen;
	const TArray<FText> Gibberish = SceneLines(TEXT("Gib"));
	for (AActor* Man : CaveMen)
	{
		const FName Name = NameOf(Man);
		UDialogueDefinition* Dialogue = NewScript(Who(Unknown));
		AddScene(Dialogue, TEXT("NoBook"), HeardGibberish);
		AddScene(Dialogue, TEXT("Book"), GotEggJob);
		AddScene(Dialogue, TEXT("Deliver"), NAME_None, TEXT("Digit1"));
		AddScene(Dialogue, TEXT("Fire"), GotFireJob);
		AddScene(Dialogue, TEXT("Lit"), NAME_None, TEXT("Digit2"));

		// The numbers are filled in when the talking starts, because the vault rolls its code after this runs.
		AddSingle(Dialogue, TEXT("Digit1"), Reginald, FText::GetEmpty(), TEXT("GoOn"), TEXT("Fire1"), EggDelivered);
		AddSingle(Dialogue, TEXT("Digit2"), Reginald, FText::GetEmpty(), TEXT("Thanks"), NAME_None, FireDone);
		for (const FName Wait : { FName(TEXT("EggWait")), FName(TEXT("LighterWait")), FName(TEXT("LightWait")) })
		{
			AddSingle(Dialogue, Wait, Reginald, Line(Wait), TEXT("OffIGo"));
		}
		for (int32 Index = 0; Index < Gibberish.Num(); ++Index)
		{
			AddSingle(Dialogue, *FString::Printf(TEXT("Gib%d"), Index), Unknown, Gibberish[Index], TEXT("Right"));
		}
		// One line each, so a player who isn't on the quest never sits through the lot.
		const TArray<FText> Chatter = SceneLines(*(TEXT("Chat") + Name.ToString()));
		for (int32 Index = 0; Index < Chatter.Num(); ++Index)
		{
			AddSingle(Dialogue, *FString::Printf(TEXT("Chat%d"), Index), Name, Chatter[Index]);
		}
		// Nobody can have found the book this early, so it's cave speak and no names.
		UDialogueComponent* Talker = GiveScript(Man, Dialogue, TEXT("Gib0"), Gibberish);
		Talker->SmallTalkSpeaker = Who(Unknown);
		Talker->EntryOverride.BindUObject(this, &ARobberCaveMenQuest::EntryFor, Man);
	}
}

int32 ARobberCaveMenQuest::DigitValue(int32 Which) const
{
	return DigitIndexes.IsValidIndex(Which) ? VaultDigit(DigitIndexes[Which]) : INDEX_NONE;
}

void ARobberCaveMenQuest::UpdateDigitLines()
{
	const int32 First = DigitValue(0);
	const int32 Second = DigitValue(1);
	const FText FirstLine = First == INDEX_NONE ? Line(TEXT("NoDigit1")) : FText::Format(Line(TEXT("Digit1")), First);
	const FText SecondLine = Second == INDEX_NONE ? Line(TEXT("NoDigit2")) : FText::Format(Line(TEXT("Digit2")), Second);
	for (AActor* Man : CaveMen)
	{
		const UDialogueComponent* Talker = Man->FindComponentByClass<UDialogueComponent>();
		if (Talker && Talker->Dialogue)
		{
			for (FDialogueNode& Node : Talker->Dialogue->Nodes)
			{
				if (Node.ID == TEXT("Digit1")) Node.Text = FirstLine;
				if (Node.ID == TEXT("Digit2")) Node.Text = SecondLine;
			}
		}
	}
}

FName ARobberCaveMenQuest::EntryFor(UPlayerQuestComponent* Player, AActor* Talker)
{
	UpdateDigitLines();
	if (const UDialogueComponent* Who = Talker->FindComponentByClass<UDialogueComponent>(); Who && Who->Dialogue)
	{
		// The journal files what you heard under this name.
		Who->Dialogue->SpeakerName = FText::FromName(bBookFound ? NameOf(Talker) : CaveMen::Unknown);
	}
	const AController* Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	const bool bOwner = Controller && IsQuestOwner(Controller->PlayerState);
	if (bOwner)
	{
		switch (Stage)
		{
		case ECaveMenStage::TalkNoBook: return TEXT("NoBook1");
		case ECaveMenStage::TalkWithBook: return TEXT("Book1");
		case ECaveMenStage::StealEgg: return TEXT("EggWait");
		case ECaveMenStage::BringEgg: return TEXT("Deliver1");
		case ECaveMenStage::AskForFire: return TEXT("Fire1");
		case ECaveMenStage::FindLighter: return TEXT("LighterWait");
		case ECaveMenStage::LightFire: return TEXT("LightWait");
		case ECaveMenStage::TalkAfterFire: return TEXT("Lit1");
		default: break;
		}
	}
	const bool bUnderstood = bBookFound;
	const UDialogueComponent* Dialogue = Talker->FindComponentByClass<UDialogueComponent>();
	const TCHAR* Prefix = bUnderstood ? TEXT("Chat") : TEXT("Gib");
	int32 Count = 0;
	while (Dialogue->Dialogue->FindNode(*FString::Printf(TEXT("%s%d"), Prefix, Count)))
	{
		++Count;
	}
	int32 Pick = FMath::RandHelper(Count);
	// Never the same line twice running from the same man.
	if (const int32* Last = LastRandomLine.Find(Talker); Last && Count > 1 && *Last == Pick)
	{
		Pick = (Pick + 1) % Count;
	}
	LastRandomLine.Add(Talker, Pick);
	if (!bUnderstood)
	{
		if (ARobberController* Robber = Cast<ARobberController>(Player->GetOwner()))
		{
			Robber->ClientHeistMessage(Line(TEXT("CantUnderstand")));
		}
	}
	return *FString::Printf(TEXT("%s%d"), Prefix, Pick);
}

void ARobberCaveMenQuest::SetStage(ECaveMenStage NewStage)
{
	Stage = NewStage;
	ForceNetUpdate();
	RefreshNote();
}

void ARobberCaveMenQuest::OnQuestStarted()
{
	// Someone taking over from a player who left carries on from where they got to.
	if (Stage == ECaveMenStage::NotStarted)
	{
		Stage = ECaveMenStage::TalkNoBook;
		ForceNetUpdate();
	}
	RefreshNote();
	Stamp(false, Line(TEXT("Name")), Line(TEXT("NewSub")));
}

void ARobberCaveMenQuest::OnDialogueEvent(AActor* Talker, UPlayerQuestComponent* Player, FName Event)
{
	if (!CaveMen.Contains(Talker) || Player != OwnerQuests())
	{
		return;
	}
	using namespace CaveMen;
	if (Event == HeardGibberish && Stage == ECaveMenStage::TalkNoBook)
	{
		SetStage(ECaveMenStage::FindBook);
		Toast(Line(TEXT("NeedTranslate")));
	}
	else if (Event == GotEggJob && Stage == ECaveMenStage::TalkWithBook)
	{
		SetStage(ECaveMenStage::StealEgg);
	}
	else if (Event == EggDelivered && Stage == ECaveMenStage::BringEgg)
	{
		PayDigit(0, Talker);
		SetStage(ECaveMenStage::AskForFire);
	}
	else if (Event == GotFireJob && Stage == ECaveMenStage::AskForFire)
	{
		SetStage(ECaveMenStage::FindLighter);
	}
	else if (Event == FireDone && Stage == ECaveMenStage::TalkAfterFire)
	{
		PayDigit(1, Talker);
		SetStage(ECaveMenStage::Done);
		Stamp(true, Line(TEXT("Name")), Line(TEXT("DoneSub")));
		ClearNoteAfterStamp();
	}
}

void ARobberCaveMenQuest::PayDigit(int32 Which, AActor* Talker)
{
	const int32 Value = DigitValue(Which);
	if (Value == INDEX_NONE || Which < DigitsPaid)
	{
		return;
	}
	DigitsPaid = Which + 1;
	TellOwnerDigit(Talker, Talker->FindComponentByClass<UDialogueComponent>()->Dialogue->SpeakerName, DigitIndexes[Which], Value);
	Toast(FText::Format(Line(TEXT("DigitToast")), Value));
	RefreshNote();
}

void ARobberCaveMenQuest::CheckRooms()
{
	if (Stage == ECaveMenStage::FindBook && bGiftShopChecked && !bReachedArchive && IsOwnerInRoom(TEXT("Archive Storage")))
	{
		bReachedArchive = true;
		ForceNetUpdate();
		RefreshNote();
	}
	if (Stage == ECaveMenStage::FindLighter && !bReachedStaffArea && IsOwnerInRoom(TEXT("Staff Lockers")))
	{
		bReachedStaffArea = true;
		ForceNetUpdate();
		RefreshNote();
	}
}

bool ARobberCaveMenQuest::HandlesProp(FName PropId) const
{
	using namespace CaveMen;
	return PropId == GiftShop || PropId == Book || PropId == Egg || PropId == Campfire || PropId == Flame || PropId == Locker1 || PropId == Locker2 || PropId == Locker3;
}

FText ARobberCaveMenQuest::GetPropPrompt(FName PropId, const APlayerState* Viewer) const
{
	using namespace CaveMen;
	if (!IsQuestOwner(Viewer))
	{
		return FText::GetEmpty();
	}
	const bool bLocker = PropId == Locker1 || PropId == Locker2 || PropId == Locker3;
	const FName Prompt = PropId == GiftShop && Stage == ECaveMenStage::FindBook && !bGiftShopChecked ? FName(TEXT("PromptGift"))
		: PropId == Book && Stage == ECaveMenStage::FindBook && bReachedArchive ? FName(TEXT("PromptBook"))
		: PropId == Egg && Stage == ECaveMenStage::StealEgg ? FName(TEXT("PromptEgg"))
		: PropId == Campfire && Stage == ECaveMenStage::LightFire ? FName(TEXT("PromptFire"))
		: bLocker && Stage == ECaveMenStage::FindLighter && bReachedStaffArea && !OpenedLockers.Contains(PropId) ? FName(TEXT("PromptLocker"))
		: NAME_None;
	return Prompt.IsNone() ? FText::GetEmpty() : Line(Prompt);
}

void ARobberCaveMenQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace CaveMen;
	if (PropId == GiftShop)
	{
		bGiftShopChecked = true;
		ForceNetUpdate();
		RefreshNote();
		Toast(Line(TEXT("SoldOut")));
	}
	else if (PropId == Book)
	{
		bBookFound = true;
		UpdateTalkPrompts();
		Prop->SetShown(false);
		SetStage(ECaveMenStage::TalkWithBook);
		Toast(Line(TEXT("GotBook")));
	}
	else if (PropId == Egg)
	{
		Prop->SetShown(false);
		SetStage(ECaveMenStage::BringEgg);
		Toast(Line(TEXT("GotEgg")));
	}
	else if (PropId == Campfire)
	{
		if (ARobberStoryProp* FlameProp = FindProp(CaveMen::Flame))
		{
			FlameProp->SetShown(true);
		}
		SetStage(ECaveMenStage::TalkAfterFire);
	}
	else if (PropId == Locker1 || PropId == Locker2 || PropId == Locker3)
	{
		OpenedLockers.AddUnique(PropId);
		bBrokeInLocker = true;
		ForceNetUpdate();
		if (PropId == LighterLocker)
		{
			SetStage(ECaveMenStage::LightFire);
			Toast(Line(TEXT("GotLighter")));
		}
		else
		{
			RefreshNote();
			Toast(Line(OpenedLockers.Num() == 1 ? TEXT("LockerSock") : TEXT("LockerMug")));
		}
	}
}

void ARobberCaveMenQuest::OnPlayerCaught(APawn* Pawn, APlayerState* Player, FName Watcher)
{
	if (!Watcher.IsNone())
	{
		return;
	}
	// She only cares if you've got one of her eggs. Everyone else just gets the fright.
	if (IsQuestOwner(Player) && Stage == ECaveMenStage::BringEgg)
	{
		if (ARobberStoryProp* EggProp = FindProp(CaveMen::Egg))
		{
			EggProp->SetShown(true);
		}
		SetStage(ECaveMenStage::StealEgg);
		Toast(Line(TEXT("LostEgg")));
	}
	else
	{
		ARobberController::Tell(Player, Line(TEXT("Caught")));
	}
}

FRobberStoryNote ARobberCaveMenQuest::BuildNote() const
{
	using namespace CaveMen;
	FRobberStoryNote Note;
	Note.bShown = true;
	Note.Title = Line(TEXT("Name"));
	if (DigitValue(0) != INDEX_NONE)
	{
		Note.Footer = FText::Format(Line(TEXT("Foot")), DigitsPaid, DigitIndexes.Num());
	}
	const auto Open = [this](const TCHAR* Key, bool bDone = false) { return NoteLine(Line(Key), bDone); };
	const auto Done = [&Open](const TCHAR* Key) { return Open(Key, true); };
	const TCHAR* TalkTo = TEXT("LTalk");
	const TCHAR* Translate = TEXT("LTrans");
	const TCHAR* StealText = TEXT("LSteal");
	const TCHAR* TakeEgg = TEXT("LTake");
	const TCHAR* FindFire = TEXT("LFind");
	const TCHAR* LightText = TEXT("LLight");
	switch (Stage)
	{
	case ECaveMenStage::TalkNoBook:
		Note.Lines = { Open(TalkTo) };
		break;
	case ECaveMenStage::FindBook:
		Note.Lines = { Done(TalkTo), Open(Translate) };
		Note.ErrandTitle = Line(TEXT("Reading"));
		Note.ErrandLines = { Open(TEXT("LGift"), bGiftShopChecked), Open(TEXT("LArchive"), bReachedArchive), Open(TEXT("LBook")) };
		break;
	case ECaveMenStage::TalkWithBook:
		Note.Lines = { Done(Translate), Open(TalkTo) };
		break;
	case ECaveMenStage::StealEgg:
		Note.Lines = { Done(TalkTo), Open(StealText), NoteLine(Line(TEXT("LCaught")), false, true) };
		break;
	case ECaveMenStage::BringEgg:
		Note.Lines = { Done(StealText), Open(TakeEgg) };
		break;
	case ECaveMenStage::AskForFire:
		Note.Lines = { Done(TakeEgg), Open(FindFire), Open(LightText) };
		break;
	case ECaveMenStage::FindLighter:
		Note.Lines = { Open(FindFire), Open(LightText) };
		Note.ErrandTitle = Line(TEXT("Hot"));
		Note.ErrandLines = { Open(TEXT("LStaff"), bReachedStaffArea), Open(TEXT("LLocker"), bBrokeInLocker), Open(TEXT("LLighter")) };
		break;
	case ECaveMenStage::LightFire:
		Note.Lines = { Done(FindFire), Open(LightText) };
		break;
	case ECaveMenStage::TalkAfterFire:
		Note.Lines = { Done(LightText), Open(TalkTo) };
		break;
	case ECaveMenStage::Done:
		Note.Lines = { Done(LightText), Done(TalkTo) };
		break;
	default:
		break;
	}
	return Note;
}
