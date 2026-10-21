#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestConversationWidget.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Quest/QuestSequenceStep.h"
#include "Dialogue/DialogueComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberGameState.h"

UPlayerQuestComponent::UPlayerQuestComponent()
{
	SetIsReplicatedByDefault(true);
}

void UPlayerQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UPlayerQuestComponent, Quests, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UPlayerQuestComponent, Conversation, COND_OwnerOnly);
}

void UPlayerQuestComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshDefaultUI();
}

void UPlayerQuestComponent::SetQuestUIEnabled(bool bEnabled)
{
	bCreateDefaultUI = bEnabled;
	RefreshDefaultUI();
}

void UPlayerQuestComponent::RefreshDefaultUI()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	if (!bCreateDefaultUI)
	{
		if (Widget)
		{
			Widget->RemoveFromParent();
			Widget = nullptr;
		}
		return;
	}

	if (!Widget)
	{
		// The tracker layout lives in WBP_QuestTracker; the bare C++ class has no widgets of its own.
		static const TSoftClassPtr<UQuestConversationWidget> DefaultClass(FSoftObjectPath(TEXT("/Game/OurStuff/Widgets/Quest/WBP_QuestTracker.WBP_QuestTracker_C")));
		UClass* Class = WidgetClass ? WidgetClass.Get() : DefaultClass.LoadSynchronous();
		if (!Class)
		{
			UE_LOG(LogTemp, Warning, TEXT("WBP_QuestTracker is missing; the quest tracker will be empty."));
			Class = UQuestConversationWidget::StaticClass();
		}
		Widget = CreateWidget<UQuestConversationWidget>(PC, Class);
		if (Widget) Widget->AddToPlayerScreen(20);
	}
}

void UPlayerQuestComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner()->HasAuthority())
	{
		if (IsValid(Conversation.NPC))
			if (auto* Dialogue = Conversation.NPC->FindComponentByClass<UDialogueComponent>()) Dialogue->Release(this);
	}
	if (Widget) Widget->RemoveFromParent();
	Super::EndPlay(Reason);
}

void UPlayerQuestComponent::ServerTalkTo_Implementation(AActor* NPC)
{
	if (GetWorld()->GetTimeSeconds() < NextTalkTime) return;
	NextTalkTime = GetWorld()->GetTimeSeconds() + 0.2;
	if (!IsValid(NPC) || NPC->GetWorld() != GetWorld() || !NPC->GetIsReplicated()) return;
	if (Conversation.NPC) return;
	if (auto* Dialogue = NPC->FindComponentByClass<UDialogueComponent>())
	{
		const auto* RobberState = GetWorld()->GetGameState<ARobberGameState>();
		if (RobberState && !RobberState->CanTalkToExhibits())
		{
			ClientRejectConversation(NSLOCTEXT("Dialogue", "NotYet", "The exhibits won't talk yet. Find the gem room first."));
			return;
		}
		if (!Dialogue->HasValidDialogue())
		{
			ClientRejectConversation(NSLOCTEXT("Dialogue", "MissingAsset", "This actor has no valid Dialogue Asset assigned."));
			return;
		}
		if (!Dialogue->TryStart(this))
			ClientRejectConversation(NSLOCTEXT("Dialogue", "Unavailable", "This NPC is busy or out of reach."));
	}
}

void UPlayerQuestComponent::ServerChoose_Implementation(int32 ChoiceID, int32 Revision)
{
	if (IsValid(Conversation.NPC))
		if (auto* Dialogue = Conversation.NPC->FindComponentByClass<UDialogueComponent>()) Dialogue->Choose(this, ChoiceID, Revision);
}

void UPlayerQuestComponent::ServerEndConversation_Implementation(int32 Revision)
{
	if (Revision != Conversation.Revision) return;
	if (IsValid(Conversation.NPC))
		if (auto* Dialogue = Conversation.NPC->FindComponentByClass<UDialogueComponent>()) Dialogue->Release(this);
}

void UPlayerQuestComponent::ServerCollectQuestObject_Implementation(AActor* QuestObject)
{
	if (!IsValid(QuestObject) || QuestObject->GetWorld() != GetWorld()) return;
	if (auto* Objective = QuestObject->FindComponentByClass<UQuestObjectiveComponent>()) Objective->Collect(this);
}

void UPlayerQuestComponent::ServerActivateQuestPuzzle_Implementation(AActor* PuzzleStep)
{
	if (!IsValid(PuzzleStep) || PuzzleStep->GetWorld() != GetWorld()) return;
	if (auto* Step = Cast<AQuestSequenceStep>(PuzzleStep)) Step->Activate(this);
}

bool UPlayerQuestComponent::GetQuest(FName QuestID, FPlayerQuest& OutQuest) const
{
	for (const FPlayerQuest& Quest : Quests)
		if (IsValid(Quest.Definition) && Quest.Definition->QuestID == QuestID) { OutQuest = Quest; return true; }
	OutQuest = FPlayerQuest();
	return false;
}

bool UPlayerQuestComponent::CanAccept(UQuestDefinition* Definition, AActor* Giver) const
{
	if (!IsValid(Definition) || Definition->QuestID.IsNone() || Definition->RequiredAmount <= 0 || !IsValid(Giver)) return false;
	const auto* Talker = Giver->FindComponentByClass<UDialogueComponent>();
	FPlayerQuest Existing;
	return Talker && Talker->OffersQuest(Definition) && !GetQuest(Definition->QuestID, Existing);
}

bool UPlayerQuestComponent::CanTurnIn(UQuestDefinition* Definition, AActor* Giver) const
{
	if (!IsValid(Definition) || !IsValid(Giver)) return false;
	FPlayerQuest Quest;
	return GetQuest(Definition->QuestID, Quest) && Quest.Definition == Definition && Quest.Giver == Giver
		&& Quest.State == EPlayerQuestState::ReadyToTurnIn;
}

bool UPlayerQuestComponent::CanProgressQuest(UQuestDefinition* Definition, AActor* Giver) const
{
	if (!IsValid(Definition) || !IsValid(Giver)) return false;
	FPlayerQuest Quest;
	return GetQuest(Definition->QuestID, Quest) && Quest.Definition == Definition && Quest.Giver == Giver
		&& Quest.State == EPlayerQuestState::Active;
}

bool UPlayerQuestComponent::AcceptQuest(UQuestDefinition* Definition, AActor* Giver)
{
	if (!GetOwner()->HasAuthority() || !CanAccept(Definition, Giver)) return false;
	const auto* Dialogue = Giver->FindComponentByClass<UDialogueComponent>();
	if (!Dialogue || !Dialogue->IsParticipant(this) || !Dialogue->CanReach(this)) return false;
	FPlayerQuest Quest;
	Quest.Definition = Definition;
	Quest.Giver = Giver;
	Quests.Add(Quest);
	GetOwner()->ForceNetUpdate();
	OnRep_Quests();
	return true;
}

bool UPlayerQuestComponent::TurnInQuest(UQuestDefinition* Definition, AActor* Giver)
{
	if (!GetOwner()->HasAuthority() || !CanTurnIn(Definition, Giver)) return false;
	const auto* Dialogue = Giver->FindComponentByClass<UDialogueComponent>();
	if (!Dialogue || !Dialogue->IsParticipant(this) || !Dialogue->CanReach(this)) return false;
	for (FPlayerQuest& Quest : Quests)
	{
		if (Quest.Definition != Definition) continue;
		// Commit before callbacks so reentrant or duplicate requests cannot award twice.
		Quest.State = EPlayerQuestState::Completed;
		GetOwner()->ForceNetUpdate();
		OnRep_Quests();
		OnRewardGranted.Broadcast(Definition);
		return true;
	}
	return false;
}

bool UPlayerQuestComponent::ProgressQuest(UQuestDefinition* Definition, AActor* Giver, int32 Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0 || !CanProgressQuest(Definition, Giver)) return false;
	return AddQuestProgress(Definition->QuestID, Amount);
}

bool UPlayerQuestComponent::AddQuestProgress(FName QuestID, int32 Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0) return false;
	for (FPlayerQuest& Quest : Quests)
	{
		if (!IsValid(Quest.Definition) || Quest.Definition->QuestID != QuestID || Quest.State != EPlayerQuestState::Active) continue;
		const int32 Required = Quest.Definition->RequiredAmount;
		if (Required <= 0) return false;
		Quest.Amount += FMath::Min(Amount, Required - Quest.Amount);
		if (Quest.Amount >= Required) Quest.State = EPlayerQuestState::ReadyToTurnIn;
		GetOwner()->ForceNetUpdate();
		OnRep_Quests();
		if (IsValid(Conversation.NPC))
			if (auto* Dialogue = Conversation.NPC->FindComponentByClass<UDialogueComponent>()) Dialogue->Refresh();
		return true;
	}
	return false;
}

void UPlayerQuestComponent::ReportQuestEvent(FName Event, int32 Amount)
{
	if (!GetOwner()->HasAuthority() || Event.IsNone() || Amount <= 0) return;
	TArray<FName> Matches;
	for (const FPlayerQuest& Quest : Quests)
		if (IsValid(Quest.Definition) && Quest.Definition->ProgressEvent == Event) Matches.Add(Quest.Definition->QuestID);
	for (FName ID : Matches) AddQuestProgress(ID, Amount);
}

void UPlayerQuestComponent::SetConversation(const FPlayerConversation& Snapshot)
{
	if (!GetOwner()->HasAuthority()) return;
	const int32 Revision = Conversation.Revision == MAX_int32 ? 1 : Conversation.Revision + 1;
	Conversation = Snapshot;
	Conversation.Revision = Revision;
	GetOwner()->ForceNetUpdate();
	OnRep_Conversation();
}

void UPlayerQuestComponent::CloseConversation() { SetConversation(FPlayerConversation()); }
void UPlayerQuestComponent::OnRep_Quests() { OnQuestsChanged.Broadcast(); }
void UPlayerQuestComponent::OnRep_Conversation() { OnConversationChanged.Broadcast(); }
void UPlayerQuestComponent::ClientRejectConversation_Implementation(const FText& Reason) { OnConversationRejected.Broadcast(Reason); }

void UPlayerQuestComponent::TalkToLookedAtNPC()
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController()) return;
	FVector Location;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Location, Rotation);
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PC->GetPawn());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location, Location + Rotation.Vector() * 500.f, ECC_Visibility, Params))
		ServerTalkTo(Hit.GetActor());
}
