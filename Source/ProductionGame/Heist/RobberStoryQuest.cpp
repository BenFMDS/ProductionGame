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
	const AController* Controller = IsValid(QuestOwner) ? QuestOwner->GetOwningController() : nullptr;
	return Controller ? Controller->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
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
	#define CAVE_LOC(Key, Text) NSLOCTEXT("CaveMen", Key, Text)

	FRobberStoryLine Open(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; return Line; }
	FRobberStoryLine Done(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; Line.bDone = true; return Line; }
	FRobberStoryLine Warn(const FText& Text) { FRobberStoryLine Line; Line.Text = Text; Line.bWarning = true; return Line; }

	FDialogueChoice Choice(const FText& Text, FName Next, FName Event = NAME_None)
	{
		FDialogueChoice Result;
		Result.Text = Text;
		Result.NextNode = Next;
		Result.StoryEvent = Event;
		return Result;
	}

	FDialogueNode Node(FName Id, FName Speaker, const FText& Text)
	{
		FDialogueNode Result;
		Result.ID = Id;
		Result.Speaker = Who(Speaker);
		Result.Text = Text;
		return Result;
	}

	FText GoOn() { return CAVE_LOC("GoOn", "Go on, then."); }
	FText OffIGo() { return CAVE_LOC("OffIGo", "I'll get on with it."); }
	FText Right() { return CAVE_LOC("Right", "...Right."); }
}

ARobberCaveMenQuest::ARobberCaveMenQuest()
{
	TipLine = CAVE_LOC("Tip", "Have you met the cave men in the {room}? {floor}. They've been watching the vault for years, if you can work out a word they say.");
	TipTakenLine = CAVE_LOC("TipTaken", "One of your lot's already been asking after the cave men. Best leave them to it.");
	TipJournal = CAVE_LOC("TipJournal", "The cave men in the {room} ({floor}) know part of the vault code.");
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

void ARobberCaveMenQuest::AddScene(UDialogueDefinition* Dialogue, const FString& Prefix, const TArray<TPair<FText, FText>>& Lines, FName LastEvent, FName Then, const FText& LastChoice) const
{
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FName Id(*FString::Printf(TEXT("%s%d"), *Prefix, Index + 1));
		FDialogueNode Node = CaveMen::Node(Id, FName(*Lines[Index].Key.ToString()), Lines[Index].Value);
		const bool bLast = Index == Lines.Num() - 1;
		Node.Choices.Add(bLast
			? CaveMen::Choice(LastChoice, Then, LastEvent)
			: CaveMen::Choice(CaveMen::GoOn(), FName(*FString::Printf(TEXT("%s%d"), *Prefix, Index + 2))));
		Dialogue->Nodes.Add(Node);
	}
}

void ARobberCaveMenQuest::BuildDialogues()
{
	using namespace CaveMen;
	using FLines = TArray<TPair<FText, FText>>;
	const auto R = [](const FText& Text) { return TPair<FText, FText>(Who(Reginald), Text); };
	const auto M = [](const FText& Text) { return TPair<FText, FText>(Who(Montague), Text); };
	const auto U = [](const FText& Text) { return TPair<FText, FText>(Who(Ug), Text); };

	const auto Stranger = [](const FText& Text) { return TPair<FText, FText>(Who(Unknown), Text); };
	const FLines NoBook = {
		Stranger(CAVE_LOC("NoBook1", "UNGA. Bunga bunga? Ooga.")),
		Stranger(CAVE_LOC("NoBook2", "Bunga! Ooga bunga bunga.")),
		Stranger(CAVE_LOC("NoBook3", "UG.")) };
	const FLines WithBook = {
		R(CAVE_LOC("Book1", "By Jove, he can understand us! It's a miracle! Thank God. You have no idea how dull the past few million years have been.")),
		M(CAVE_LOC("Book2", "Forty thousand years I've been telling Ug the one about the mammoth.")),
		U(CAVE_LOC("Book3", "Unga.")),
		R(CAVE_LOC("Book4", "Ignore Ug. He never took to the language. Now then, I should very much like an omelette, and there is only one place to get an egg.")),
		R(CAVE_LOC("Book5", "Now, poor Tina is as dumb as a dodo. Lucky for you, she can't count awfully well, so she won't notice an egg missing.")),
		R(CAVE_LOC("BookTinaHome", "You'll find her nesting in Fossil Storage, down in the basement.")),
		R(CAVE_LOC("Book6", "Bad for you, if she catches you she will get awfully mad and attempt to eat you. So don't get caught. Right, well, off you go, and good luck.")) };
	const FLines Deliver = {
		R(CAVE_LOC("Deliver1", "Good heavens, you've still got all your limbs. Tina must be slipping.")),
		M(CAVE_LOC("Deliver2", "Is it warm? Do say it's warm.")),
		U(CAVE_LOC("Deliver3", "Bunga!")) };
	const FLines AskFire = {
		R(CAVE_LOC("Fire1", "We now have an egg and no means of cooking it. We've been rubbing these sticks together since the Ice Age.")),
		M(CAVE_LOC("Fire2", "Between you and me, it's the sticks.")),
		R(CAVE_LOC("Fire3", "Find us something that makes fire, would you? I believe the staff keep little metal ones in their lockers.")) };
	const FLines Lit = {
		R(CAVE_LOC("Lit1", "Fire! Glorious, terrifying fire! Montague, do stop touching it.")),
		M(CAVE_LOC("Lit2", "It's hot, Reginald.")),
		U(CAVE_LOC("Lit3", "UNGA BUNGA!")),
		R(CAVE_LOC("Lit4", "Ug has eaten the egg raw. I can't say I'm surprised.")) };

	// One line each, so a player who isn't on the quest never sits through the lot.
	const TMap<FName, TArray<FText>> Chatter = {
		{ Reginald, {
			CAVE_LOC("RegA", "Do mind the display, there's a good chap. It took us ages to get this grass to look this fake."),
			CAVE_LOC("RegB", "Forty thousand years in the same pose. I've got a crick in my everything."),
			CAVE_LOC("RegC", "Your friend's the one running our errands. You may stand there and look decorative."),
			CAVE_LOC("RegD", "I invented the wheel, you know. Montague ate it.") } },
		{ Montague, {
			CAVE_LOC("MonA", "Have you heard the one about the mammoth? No? Marvellous. It starts with a mammoth."),
			CAVE_LOC("MonB", "Between you and me, I don't think we're the real cave men."),
			CAVE_LOC("MonC", "They call this the Stone Age. I'd call it the Quite Damp Age."),
			CAVE_LOC("MonD", "The children press their faces on the glass. There is no glass. It's very upsetting.") } },
		{ Ug, {
			CAVE_LOC("UgA", "UG."),
			CAVE_LOC("UgB", "Ooga?"),
			CAVE_LOC("UgC", "Bunga bunga."),
			CAVE_LOC("UgD", "Unga. Unga unga.") } } };
	const TArray<FText> Gibberish = {
		CAVE_LOC("GibA", "Ooga bunga. Bunga?"),
		CAVE_LOC("GibB", "UNGA! Unga unga bunga."),
		CAVE_LOC("GibC", "Bunga. Ooga ooga, bunga."),
		CAVE_LOC("GibD", "Ug. Ug ug.") };

	for (AActor* Man : CaveMen)
	{
		const FName Name = NameOf(Man);
		UDialogueDefinition* Dialogue = NewObject<UDialogueDefinition>(this);
		Generated.Add(Dialogue);
		Dialogue->DialogueType = EDialogueType::DialogueOnly;
		Dialogue->bIncludeQuestChoices = false;
		Dialogue->SpeakerName = Who(Unknown);

		AddScene(Dialogue, TEXT("NoBook"), NoBook, HeardGibberish, NAME_None, Right());
		AddScene(Dialogue, TEXT("Book"), WithBook, GotEggJob, NAME_None, OffIGo());
		AddScene(Dialogue, TEXT("Deliver"), Deliver, NAME_None, TEXT("Digit1"), GoOn());
		AddScene(Dialogue, TEXT("Fire"), AskFire, GotFireJob, NAME_None, OffIGo());
		AddScene(Dialogue, TEXT("Lit"), Lit, NAME_None, TEXT("Digit2"), GoOn());

		// The numbers are filled in when the talking starts, because the vault rolls its code after this runs.
		FDialogueNode Digit1 = Node(TEXT("Digit1"), Reginald, FText::GetEmpty());
		Digit1.Choices.Add(Choice(GoOn(), TEXT("Fire1"), EggDelivered));
		Dialogue->Nodes.Add(Digit1);
		FDialogueNode Digit2 = Node(TEXT("Digit2"), Reginald, FText::GetEmpty());
		Digit2.Choices.Add(Choice(CAVE_LOC("Thanks", "Pleasure doing business."), NAME_None, FireDone));
		Dialogue->Nodes.Add(Digit2);

		FDialogueNode EggWait = Node(TEXT("EggWait"), Reginald, CAVE_LOC("EggWait", "No egg, no omelette, dear fellow. She's in Fossil Storage. Do try not to be eaten."));
		EggWait.Choices.Add(Choice(OffIGo(), NAME_None));
		Dialogue->Nodes.Add(EggWait);
		FDialogueNode LighterWait = Node(TEXT("LighterWait"), Reginald, CAVE_LOC("LighterWait", "Little metal fire. Staff lockers. Do keep up."));
		LighterWait.Choices.Add(Choice(OffIGo(), NAME_None));
		Dialogue->Nodes.Add(LighterWait);
		FDialogueNode LightWait = Node(TEXT("LightWait"), Reginald, CAVE_LOC("LightWait", "Well, don't show it to us. Light the fire!"));
		LightWait.Choices.Add(Choice(OffIGo(), NAME_None));
		Dialogue->Nodes.Add(LightWait);

		for (int32 Index = 0; Index < Gibberish.Num(); ++Index)
		{
			FDialogueNode Line = Node(*FString::Printf(TEXT("Gib%d"), Index), Unknown, Gibberish[Index]);
			Line.Choices.Add(Choice(Right(), NAME_None));
			Dialogue->Nodes.Add(Line);
		}
		const TArray<FText>& Lines = Chatter[Name];
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			FDialogueNode Line = Node(*FString::Printf(TEXT("Chat%d"), Index), Name, Lines[Index]);
			Line.Choices.Add(Choice(CAVE_LOC("Bye", "I'll leave you to it."), NAME_None));
			Dialogue->Nodes.Add(Line);
		}
		Dialogue->EntryNode = TEXT("Gib0");

		UDialogueComponent* Talker = Man->FindComponentByClass<UDialogueComponent>();
		Talker->Dialogue = Dialogue;
		Talker->OfferedQuests.Reset();
		// Nobody can have found the book this early, so it's cave speak and no names.
		Talker->SmallTalkLines = Gibberish;
		Talker->SmallTalkSpeaker = Who(Unknown);
		Talker->EntryOverride.BindUObject(this, &ARobberCaveMenQuest::EntryFor, Man);
	}
}

int32 ARobberCaveMenQuest::DigitValue(int32 Which) const
{
	if (!DigitIndexes.IsValidIndex(Which))
	{
		return INDEX_NONE;
	}
	for (TActorIterator<ARobberVaultDoor> It(GetWorld()); It; ++It)
	{
		if (It->GetLevel() == GetLevel() && It->GetCode().IsValidIndex(DigitIndexes[Which]))
		{
			return It->GetCode()[DigitIndexes[Which]];
		}
	}
	return INDEX_NONE;
}

void ARobberCaveMenQuest::UpdateDigitLines()
{
	const int32 First = DigitValue(0);
	const int32 Second = DigitValue(1);
	const FText FirstLine = First == INDEX_NONE
		? CAVE_LOC("NoDigit1", "Sadly there's no vault tonight, so you'll have to take our thanks instead.")
		: FText::Format(CAVE_LOC("Digit1", "A deal is a deal. We've watched the guards punch in that vault code for years, and one does notice the buttons. The first number you want is a {0}."), First);
	const FText SecondLine = Second == INDEX_NONE
		? CAVE_LOC("NoDigit2", "You've been a credit to the species, whichever one you are.")
		: FText::Format(CAVE_LOC("Digit2", "You've been a credit to the species, whichever one you are. The next number is a {0}."), Second);
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
	int32 Line = FMath::RandHelper(Count);
	// Never the same line twice running from the same man.
	if (const int32* Last = LastRandomLine.Find(Talker); Last && Count > 1 && *Last == Line)
	{
		Line = (Line + 1) % Count;
	}
	LastRandomLine.Add(Talker, Line);
	if (!bUnderstood)
	{
		if (ARobberController* Robber = Cast<ARobberController>(Player->GetOwner()))
		{
			Robber->ClientHeistMessage(CAVE_LOC("CantUnderstand", "CAVE SPEAK · You can't understand a word of this."));
		}
	}
	return *FString::Printf(TEXT("%s%d"), Prefix, Line);
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
	Stamp(false, CAVE_LOC("Name", "Stone Age Small Talk"), CAVE_LOC("NewSub", "Three cave men have been discovering fire for a very long time."));
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
		Toast(CAVE_LOC("NeedTranslate", "CAVE SPEAK · You can't understand a word of this. Find a way to translate it."));
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
		Stamp(true, CAVE_LOC("Name", "Stone Age Small Talk"), CAVE_LOC("DoneSub", "Both code digits are in your journal."));
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
	const AController* Controller = OwnerController();
	if (URobberJournalComponent* Journal = Controller ? Controller->FindComponentByClass<URobberJournalComponent>() : nullptr)
	{
		const UDialogueComponent* Dialogue = Talker->FindComponentByClass<UDialogueComponent>();
		Journal->LearnVaultDigit(Talker, Dialogue->Dialogue->SpeakerName, DigitIndexes[Which], Value, ARobberVaultDoor::CodeLength);
	}
	Toast(FText::Format(CAVE_LOC("DigitToast", "CODE DIGIT · {0} jotted in your journal (J)"), Value));
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
	if (PropId == GiftShop && Stage == ECaveMenStage::FindBook && !bGiftShopChecked)
	{
		return CAVE_LOC("PromptGift", "Look for a phrase book");
	}
	if (PropId == Book && Stage == ECaveMenStage::FindBook && bReachedArchive)
	{
		return CAVE_LOC("PromptBook", "Take Cave Speak for Dummies");
	}
	if (PropId == Egg && Stage == ECaveMenStage::StealEgg)
	{
		return CAVE_LOC("PromptEgg", "Steal an egg");
	}
	if (PropId == Campfire && Stage == ECaveMenStage::LightFire)
	{
		return CAVE_LOC("PromptFire", "Light the campfire");
	}
	if ((PropId == Locker1 || PropId == Locker2 || PropId == Locker3) && Stage == ECaveMenStage::FindLighter && bReachedStaffArea && !OpenedLockers.Contains(PropId))
	{
		return CAVE_LOC("PromptLocker", "Break into the locker");
	}
	return FText::GetEmpty();
}

void ARobberCaveMenQuest::OnPropUsed(FName PropId, ARobberStoryProp* Prop, APawn* Pawn, APlayerState* Player)
{
	using namespace CaveMen;
	if (PropId == GiftShop)
	{
		bGiftShopChecked = true;
		ForceNetUpdate();
		RefreshNote();
		Toast(CAVE_LOC("SoldOut", "SOLD OUT · Last copy sent down to Archive Storage"));
	}
	else if (PropId == Book)
	{
		bBookFound = true;
		UpdateTalkPrompts();
		Prop->SetShown(false);
		SetStage(ECaveMenStage::TalkWithBook);
		Toast(CAVE_LOC("GotBook", "CAVE SPEAK FOR DUMMIES · Now you can understand them"));
	}
	else if (PropId == Egg)
	{
		Prop->SetShown(false);
		SetStage(ECaveMenStage::BringEgg);
		Toast(CAVE_LOC("GotEgg", "EGG · Still warm. Get out of there!"));
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
			Toast(CAVE_LOC("GotLighter", "LIGHTER · Little metal fire, as promised"));
		}
		else
		{
			RefreshNote();
			Toast(OpenedLockers.Num() == 1
				? CAVE_LOC("LockerSock", "LOCKER · One sock and a sandwich from March. No lighter.")
				: CAVE_LOC("LockerMug", "LOCKER · A mug that says WORLD'S OKAYEST GUARD. No lighter."));
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
		Toast(CAVE_LOC("LostEgg", "CAUGHT · Tina took her egg back."));
	}
	else if (Player && Player->GetOwningController())
	{
		if (ARobberController* Robber = Cast<ARobberController>(Player->GetOwningController()))
		{
			Robber->ClientHeistMessage(CAVE_LOC("Caught", "CAUGHT · Tina is awfully mad."));
		}
	}
}

FRobberStoryNote ARobberCaveMenQuest::BuildNote() const
{
	using namespace CaveMen;
	FRobberStoryNote Note;
	Note.bShown = true;
	Note.Title = CAVE_LOC("Name", "Stone Age Small Talk");
	if (DigitValue(0) != INDEX_NONE)
	{
		Note.Footer = FText::Format(CAVE_LOC("Foot", "Code digits: {0} of {1}"), DigitsPaid, DigitIndexes.Num());
	}
	const FText TalkTo = CAVE_LOC("LTalk", "Talk to the cave men");
	const FText Translate = CAVE_LOC("LTrans", "Find a way to translate cave speak");
	const FText StealText = CAVE_LOC("LSteal", "Steal an egg from under Tina");
	const FText TakeEgg = CAVE_LOC("LTake", "Take the egg to the cave men");
	const FText FindFire = CAVE_LOC("LFind", "Find something that makes fire");
	const FText LightText = CAVE_LOC("LLight", "Light the cave men's campfire");
	const auto Mark = [](const FText& Text, bool bIsDone) { return bIsDone ? Done(Text) : Open(Text); };
	switch (Stage)
	{
	case ECaveMenStage::TalkNoBook:
		Note.Lines = { Open(TalkTo) };
		break;
	case ECaveMenStage::FindBook:
		Note.Lines = { Done(TalkTo), Open(Translate) };
		Note.ErrandTitle = CAVE_LOC("Reading", "Required Reading");
		Note.ErrandLines = {
			Mark(CAVE_LOC("LGift", "Check the Gift Shop"), bGiftShopChecked),
			Mark(CAVE_LOC("LArchive", "Get to Archive Storage"), bReachedArchive),
			Open(CAVE_LOC("LBook", "Find Cave Speak for Dummies")) };
		break;
	case ECaveMenStage::TalkWithBook:
		Note.Lines = { Done(Translate), Open(TalkTo) };
		break;
	case ECaveMenStage::StealEgg:
		Note.Lines = { Done(TalkTo), Open(StealText), Warn(CAVE_LOC("LCaught", "Don't get caught!")) };
		break;
	case ECaveMenStage::BringEgg:
		Note.Lines = { Done(StealText), Open(TakeEgg) };
		break;
	case ECaveMenStage::AskForFire:
		Note.Lines = { Done(TakeEgg), Open(FindFire), Open(LightText) };
		break;
	case ECaveMenStage::FindLighter:
		Note.Lines = { Open(FindFire), Open(LightText) };
		Note.ErrandTitle = CAVE_LOC("Hot", "Hot Property");
		Note.ErrandLines = {
			Mark(CAVE_LOC("LStaff", "Get into the staff area"), bReachedStaffArea),
			Mark(CAVE_LOC("LLocker", "Break into a locker"), bBrokeInLocker),
			Open(CAVE_LOC("LLighter", "Find the lighter")) };
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
