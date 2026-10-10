#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestGiverComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UDialogueComponent::UDialogueComponent() { SetIsReplicatedByDefault(true); }

void UDialogueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDialogueComponent, bIsInConversation);
	DOREPLIFETIME(UDialogueComponent, TalkingPlayer);
}

bool UDialogueComponent::CanReach(const UPlayerQuestComponent* Player) const
{
	const auto* PC = IsValid(Player) ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || Pawn->GetWorld() != GetWorld() || GetOwner()->IsActorBeingDestroyed()
		|| FVector::DistSquared(Pawn->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(FMath::Max(1.f, InteractionDistance))) return false;
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);
	Params.AddIgnoredActor(GetOwner());
	return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), GetOwner()->GetActorLocation(), ECC_Visibility, Params);
}

bool UDialogueComponent::IsParticipant(const UPlayerQuestComponent* Player) const
{
	return GetOwner()->HasAuthority() && bIsInConversation && Participant.Get() == Player;
}

bool UDialogueComponent::TryStart(UPlayerQuestComponent* Player)
{
	if (!GetOwner()->HasAuthority() || !GetOwner()->GetIsReplicated() || !CanReach(Player)) return false;
	ValidateSession();
	if (bIsInConversation || Player->Conversation.NPC) return false;
	if (Dialogue && !Dialogue->FindNode(Dialogue->EntryNode)) return false;
	Participant = Player;
	auto* PC = CastChecked<APlayerController>(Player->GetOwner());
	StartingPawn = PC->GetPawn();
	TalkingPlayer = PC->PlayerState;
	bIsInConversation = true;
	CurrentNode = Dialogue ? Dialogue->EntryNode : NAME_None;
	if (Dialogue && IsValid(Dialogue->ContextQuest))
	{
		FPlayerQuest Quest;
		if (Player->GetQuest(Dialogue->ContextQuest->QuestID, Quest))
		{
			FName StateEntry = Dialogue->ActiveEntryNode;
			if (Quest.State == EPlayerQuestState::Completed) StateEntry = Dialogue->CompletedEntryNode;
			else if (Player->CanTurnIn(Dialogue->ContextQuest, GetOwner())) StateEntry = Dialogue->ReadyEntryNode;
			if (Dialogue->FindNode(StateEntry)) CurrentNode = StateEntry;
		}
	}
	LastActivity = GetWorld()->GetTimeSeconds();
	GetOwner()->ForceNetUpdate();
	GetWorld()->GetTimerManager().SetTimer(SessionTimer, this, &UDialogueComponent::ValidateSession, 0.25f, true);
	PublishNode();
	return true;
}

void UDialogueComponent::ValidateSession()
{
	if (!GetOwner()->HasAuthority() || !bIsInConversation) return;
	const auto* Player = Participant.Get();
	const auto* PC = Player ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
	if (!PC || PC->IsActorBeingDestroyed() || !StartingPawn.IsValid() || PC->GetPawn() != StartingPawn.Get() || !CanReach(Player)
		|| GetWorld()->GetTimeSeconds() - LastActivity >= FMath::Max(1.f, IdleTimeout)) Release(Participant.Get());
}

void UDialogueComponent::Release(UPlayerQuestComponent* Player)
{
	if (!GetOwner()->HasAuthority() || Participant.Get() != Player) return;
	UPlayerQuestComponent* Previous = Participant.Get();
	Participant.Reset();
	StartingPawn.Reset();
	TalkingPlayer = nullptr;
	bIsInConversation = false;
	VisibleChoiceIDs.Reset();
	GetWorld()->GetTimerManager().ClearTimer(SessionTimer);
	GetOwner()->ForceNetUpdate();
	if (IsValid(Previous)) Previous->CloseConversation();
}

void UDialogueComponent::PublishNode()
{
	UPlayerQuestComponent* Player = Participant.Get();
	if (!Player) return;
	FPlayerConversation Snapshot;
	Snapshot.NPC = GetOwner();
	VisibleChoiceIDs.Reset();
	if (Dialogue)
	{
		const FDialogueNode* Node = Dialogue->FindNode(CurrentNode);
		if (!Node) { Release(Player); return; }
		Snapshot.Speaker = Dialogue->SpeakerName;
		Snapshot.Portrait = Dialogue->Portrait;
		Snapshot.Text = Node->Text;
		Snapshot.Voice = Node->Voice;
		for (int32 Index = 0; Index < Node->Choices.Num(); ++Index)
		{
			const FDialogueChoice& Choice = Node->Choices[Index];
			if (Choice.QuestAction == EDialogueQuestAction::Accept && !Player->CanAccept(Choice.Quest, GetOwner())) continue;
			if (Choice.QuestAction == EDialogueQuestAction::TurnIn && !Player->CanTurnIn(Choice.Quest, GetOwner())) continue;
			if (Choice.QuestAction == EDialogueQuestAction::Progress && !Player->CanProgressQuest(Choice.Quest, GetOwner())) continue;
			if (!Choice.NextNode.IsNone() && !Dialogue->FindNode(Choice.NextNode)) continue;
			Snapshot.Choices.Add(Choice.Text);
			Snapshot.ChoiceIDs.Add(Index);
		}
	}
	else
	{
		Snapshot.Speaker = FText::FromString(GetOwner()->GetName());
		Snapshot.Text = NSLOCTEXT("Dialogue", "Greeting", "Looking for work? You can accept a quest or collect your reward here.");
	}
	if (!Dialogue || Dialogue->bIncludeQuestChoices)
	{
		if (auto* Giver = GetOwner()->FindComponentByClass<UQuestGiverComponent>())
		{
			for (int32 Index = 0; Index < Giver->AvailableQuests.Num(); ++Index)
			{
				UQuestDefinition* Quest = Giver->AvailableQuests[Index];
				if (Player->CanAccept(Quest, GetOwner()))
				{
					Snapshot.Choices.Add(FText::Format(NSLOCTEXT("Dialogue", "Accept", "Accept: {0}"), Quest->QuestName));
					Snapshot.ChoiceIDs.Add(100000 + Index * 2);
				}
				else if (Player->CanTurnIn(Quest, GetOwner()))
				{
					Snapshot.Choices.Add(FText::Format(NSLOCTEXT("Dialogue", "TurnInQuest", "Complete: {0}"), Quest->QuestName));
					Snapshot.ChoiceIDs.Add(100000 + Index * 2 + 1);
				}
			}
		}
	}
	VisibleChoiceIDs = Snapshot.ChoiceIDs;
	Player->SetConversation(Snapshot);
}

void UDialogueComponent::Choose(UPlayerQuestComponent* Player, int32 ChoiceID, int32 Revision)
{
	if (!IsParticipant(Player) || Player->Conversation.Revision != Revision) return;
	ValidateSession();
	if (!IsParticipant(Player) || !VisibleChoiceIDs.Contains(ChoiceID)) return;
	LastActivity = GetWorld()->GetTimeSeconds();
	if (Dialogue && ChoiceID < 100000)
	{
		const FDialogueNode* Node = Dialogue->FindNode(CurrentNode);
		if (!Node || !Node->Choices.IsValidIndex(ChoiceID)) return;
		const FDialogueChoice Choice = Node->Choices[ChoiceID];
		bool bSuccess = true;
		if (Choice.QuestAction == EDialogueQuestAction::Accept) bSuccess = Player->AcceptQuest(Choice.Quest, GetOwner());
		if (Choice.QuestAction == EDialogueQuestAction::TurnIn) bSuccess = Player->TurnInQuest(Choice.Quest, GetOwner());
		if (Choice.QuestAction == EDialogueQuestAction::Progress) bSuccess = Player->ProgressQuest(Choice.Quest, GetOwner(), Choice.ProgressAmount);
		if (!IsParticipant(Player)) return;
		if (bSuccess)
		{
			if (Choice.NextNode.IsNone()) { Release(Player); return; }
			CurrentNode = Choice.NextNode;
		}
	}
	else if (auto* Giver = GetOwner()->FindComponentByClass<UQuestGiverComponent>())
	{
		ChoiceID -= 100000;
		if (ChoiceID < 0) return;
		if (!Giver->AvailableQuests.IsValidIndex(ChoiceID / 2)) return;
		UQuestDefinition* Quest = Giver->AvailableQuests[ChoiceID / 2];
		if (ChoiceID % 2 == 0) Player->AcceptQuest(Quest, GetOwner());
		else Player->TurnInQuest(Quest, GetOwner());
	}
	if (IsParticipant(Player)) PublishNode();
}

void UDialogueComponent::Refresh() { if (GetOwner()->HasAuthority() && Participant.IsValid()) PublishNode(); }

void UDialogueComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner()->HasAuthority()) Release(Participant.Get());
	Super::EndPlay(Reason);
}
