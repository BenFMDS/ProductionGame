#include "Heist/RobberMummyQuest.h"

#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberExhibitTalk.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberVault.h"
#include "Net/UnrealNetwork.h"
#include "ProductionGameCharacter.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

const FName ARobberMummyQuest::DeskGuard(TEXT("DeskGuard"));

namespace MummyQuest
{
	const FName Tag(TEXT("Neferkhaos"));
	const FName Ballast(TEXT("Ballast")), Lungs(TEXT("Lungs")), Stomach(TEXT("Stomach")), Liver(TEXT("Liver")), Sausages(TEXT("Sausages")), Cabinet(TEXT("Cabinet")), HeadJar(TEXT("HeadJar"));
	const FName Crate1(TEXT("Crate1")), Crate2(TEXT("Crate2")), HeartCrate(TEXT("Crate3")), Crate4(TEXT("Crate4")), Crate5(TEXT("Crate5"));
	const FName FirstDone(TEXT("FirstDone")), LungsDone(TEXT("LungsDone")), StomachDone(TEXT("StomachDone")), LiverDone(TEXT("LiverDone")), HeartDone(TEXT("HeartDone")), SausagesDone(TEXT("SausagesDone")), HeadDone(TEXT("HeadDone"));

	bool IsCrate(FName Id) { return Id == Crate1 || Id == Crate2 || Id == HeartCrate || Id == Crate4 || Id == Crate5; }

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
	UseScript(TEXT("DT_MummyScript"));
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
	return IsCrate(PropId) || PropId == Ballast || PropId == Lungs || PropId == Stomach || PropId == Liver || PropId == Sausages || PropId == Cabinet || PropId == HeadJar;
}

void ARobberMummyQuest::BuildDialogue()
{
	using namespace MummyQuest;
	const FName Him(TEXT("Neferkhaos"));
	Script = NewScript(Line(TEXT("Speaker")));
	AddScene(Script, TEXT("First"), FirstDone);
	AddScene(Script, TEXT("Lungs"), LungsDone);
	AddScene(Script, TEXT("Stomach"), StomachDone);
	AddScene(Script, TEXT("Liver"), LiverDone);
	AddScene(Script, TEXT("Heart"), HeartDone);
	AddScene(Script, TEXT("Sausages"), SausagesDone);
	AddScene(Script, TEXT("Head"), HeadDone);
	for (const TCHAR* Wait : { TEXT("WaitLungs"), TEXT("WaitStomach"), TEXT("WaitLiver"), TEXT("WaitHeart"), TEXT("WaitSausages"), TEXT("WaitHead") })
	{
		AddSingle(Script, Wait, Him, Line(Wait));
	}
	// One random line a talk, for anyone who isn't on the quest.
	for (int32 Bucket = 0; Bucket <= 5; ++Bucket)
	{
		const TArray<FText> Chatter = SceneLines(*FString::Printf(TEXT("Chat%d"), Bucket));
		for (int32 Index = 0; Index < Chatter.Num(); ++Index)
		{
			AddSingle(Script, *FString::Printf(TEXT("Chat%d_%d"), Bucket, Index), Him, Chatter[Index], TEXT("Bye"));
		}
	}
	GiveScript(Mummy, Script, TEXT("Chat0_0"), SceneLines(TEXT("Chat0")))->EntryOverride.BindUObject(this, &ARobberMummyQuest::EntryFor, Mummy.Get());
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
	int32 Pick = FMath::RandHelper(FMath::Max(1, Count));
	if (const int32* Last = LastChatLine.Find(Bucket); Last && Count > 1 && *Last == Pick)
	{
		Pick = (Pick + 1) % Count;
	}
	LastChatLine.Add(Bucket, Pick);
	return *FString::Printf(TEXT("Chat%d_%d"), Bucket, Pick);
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
	// The organ goes inside him (Ben's call: no jars on show). The carried prop was hidden at pickup, so nothing to show.
	SetCarrying(1.f);
}

void ARobberMummyQuest::PayOut()
{
	const int32 Index = DigitIndexes.IsValidIndex(PayoutsMade) ? DigitIndexes[PayoutsMade] : INDEX_NONE;
	const int32 Value = Index == INDEX_NONE ? INDEX_NONE : VaultDigit(Index);
	if (Value == INDEX_NONE)
	{
		return;
	}
	TellOwnerDigit(Mummy, Line(TEXT("Speaker")), Index, Value);
	++PayoutsMade;
	ForceNetUpdate();
	Toast(FText::Format(Line(TEXT("DigitToast")), Value));
}

void ARobberMummyQuest::OnQuestStarted()
{
	if (Stage == EMummyStage::NotStarted)
	{
		Stage = EMummyStage::FirstTalk;
		ForceNetUpdate();
	}
	RefreshNote();
	Stamp(false, Line(TEXT("Name")), Line(TEXT("NewSub")));
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
		Toast(Line(TEXT("GotScroll")));
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
		Stamp(true, Line(TEXT("Name")), Line(TEXT("DoneSub")));
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
	if (PropId == Ballast && Stage == EMummyStage::FetchLungs && !bHasBallast) return Line(TEXT("PromptSand"));
	if (PropId == Lungs && Stage == EMummyStage::FetchLungs) return Line(bHasBallast ? TEXT("PromptSwap") : TEXT("PromptLift"));
	if (PropId == Stomach && Stage == EMummyStage::FetchStomach) return Line(TEXT("PromptStomach"));
	if (PropId == Liver && Stage == EMummyStage::FetchLiver) return Line(TEXT("PromptLiver"));
	if (IsCrate(PropId) && Stage == EMummyStage::FetchHeart && !OpenedCrates.Contains(PropId)) return Line(TEXT("PromptCrate"));
	if (PropId == Sausages && Stage == EMummyStage::FetchSausages) return Line(TEXT("PromptSausages"));
	if (PropId == Cabinet && Stage == EMummyStage::FetchHead && !bCabinetOpen) return Line(TEXT("PromptCabinet"));
	if (PropId == HeadJar && Stage == EMummyStage::FetchHead && bCabinetOpen) return Line(TEXT("PromptJar"));
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
		Toast(Line(TEXT("GotSand")));
	}
	else if (PropId == Lungs)
	{
		if (!bHasBallast)
		{
			Toast(Line(TEXT("NeedSand")));
			return;
		}
		Prop->SetShown(false);
		SetCarrying(0.55f);
		SetStage(EMummyStage::BringLungs);
		Toast(Line(TEXT("GotLungs")));
	}
	else if (PropId == Stomach)
	{
		Prop->SetShown(false);
		SetCarrying(0.8f);
		SetStage(EMummyStage::BringStomach);
		Toast(Line(TEXT("GotStomach")));
	}
	else if (PropId == Liver)
	{
		Prop->SetShown(false);
		SetCarrying(0.8f);
		SetStage(EMummyStage::BringLiver);
		Toast(Line(TEXT("GotLiver")));
	}
	else if (IsCrate(PropId))
	{
		OpenedCrates.AddUnique(PropId);
		ForceNetUpdate();
		if (PropId == HeartCrate)
		{
			SetCarrying(0.9f);
			SetStage(EMummyStage::BringHeart);
			Toast(Line(TEXT("GotHeart")));
		}
		else
		{
			RefreshNote();
			Toast(Line(TEXT("WrongCrate")));
		}
	}
	else if (PropId == Sausages)
	{
		Prop->SetShown(false);
		SetCarrying(0.9f);
		SetStage(EMummyStage::BringSausages);
		Toast(Line(TEXT("GotSausages")));
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
		Toast(Line(TEXT("CabinetOpen")));
	}
	else if (PropId == HeadJar)
	{
		Prop->SetShown(false);
		SetCarrying(0.9f);
		SetStage(EMummyStage::BringHead);
		Toast(Line(TEXT("GotHead")));
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
		Toast(Line(TEXT("LostLiver")));
	}
	else
	{
		Toast(Line(TEXT("CaughtDesk")));
	}
}

FRobberStoryNote ARobberMummyQuest::BuildNote() const
{
	using namespace MummyQuest;
	FRobberStoryNote Note;
	Note.bShown = true;
	Note.Title = Line(TEXT("Name"));
	Note.Footer = FText::Format(Line(TEXT("Foot")), PayoutsMade);
	TArray<FText> Steps;
	for (int32 Step = 0; Step <= 7; ++Step)
	{
		Steps.Add(Line(*FString::Printf(TEXT("L%d"), Step)));
	}
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
		Note.Lines.Add(NoteLine(Steps[DoneCount - 1], true));
	}
	if (Steps.IsValidIndex(DoneCount))
	{
		Note.Lines.Add(NoteLine(Steps[DoneCount], bCarrying));
	}
	if (bCarrying)
	{
		Note.Lines.Add(NoteLine(Line(TEXT("LCarry"))));
	}
	if (Stage == EMummyStage::FetchLungs)
	{
		Note.ErrandTitle = Line(TEXT("Errand"));
		Note.ErrandLines = { NoteLine(Line(TEXT("E0")), true), NoteLine(Line(TEXT("E1"))) };
	}
	return Note;
}
