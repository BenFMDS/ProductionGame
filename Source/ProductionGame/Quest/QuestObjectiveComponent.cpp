#include "Quest/QuestObjectiveComponent.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

bool UQuestObjectiveComponent::HasBeenCollectedBy(const UPlayerQuestComponent* Player) const
{
	return ClaimedBy.Contains(Player);
}

bool UQuestObjectiveComponent::Collect(UPlayerQuestComponent* Player)
{
	if (!GetOwner()->HasAuthority() || !GetOwner()->GetIsReplicated() || GetOwner()->IsActorBeingDestroyed()
		|| !IsValid(Player) || Player->GetWorld() != GetWorld() || ProgressEvent.IsNone() || Amount <= 0 || HasBeenCollectedBy(Player)) return false;
	const auto* PC = Cast<APlayerController>(Player->GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()
		|| FVector::DistSquared(Pawn->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(FMath::Max(1.f, InteractionDistance))) return false;
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);
	Params.AddIgnoredActor(GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), GetOwner()->GetActorLocation(), ECC_Visibility, Params)) return false;
	for (const FPlayerQuest& Quest : Player->Quests)
	{
		if (IsValid(Quest.Definition) && Quest.Definition->ProgressEvent == ProgressEvent && Quest.State == EPlayerQuestState::Active)
		{
			// Commit before progress callbacks; repeated input cannot claim the same object twice.
			ClaimedBy.Add(Player);
			Player->ReportQuestEvent(ProgressEvent, Amount);
			return true;
		}
	}
	return false;
}
