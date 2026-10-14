#include "Quest/QuestSequencePuzzle.h"

#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestSequenceStep.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

AQuestSequencePuzzle::AQuestSequencePuzzle()
{
	bReplicates = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

bool AQuestSequencePuzzle::CanUseStep(const UPlayerQuestComponent* Player, const AQuestSequenceStep* Step) const
{
	if (!HasAuthority() || !GetIsReplicated() || IsActorBeingDestroyed() || !IsValid(Player) || !IsValid(Step)
		|| Player->GetWorld() != GetWorld() || Step->GetWorld() != GetWorld() || Step->IsActorBeingDestroyed()
		|| Step->Puzzle != this || ProgressEvent.IsNone() || RequiredSequence.IsEmpty()) return false;

	const APlayerController* PC = Cast<APlayerController>(Player->GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()
		|| FVector::DistSquared(Pawn->GetActorLocation(), Step->GetActorLocation()) > FMath::Square(FMath::Max(1.f, InteractionDistance))) return false;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);
	Params.AddIgnoredActor(Step);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), Step->GetActorLocation(), ECC_Visibility, Params)) return false;

	for (const FPlayerQuest& Quest : Player->Quests)
	{
		if (IsValid(Quest.Definition) && Quest.Definition->ProgressEvent == ProgressEvent && Quest.State == EPlayerQuestState::Active)
			return true;
	}
	return false;
}

bool AQuestSequencePuzzle::TryActivate(UPlayerQuestComponent* Player, AQuestSequenceStep* Step)
{
	if (!CanUseStep(Player, Step)) return false;

	int32& Progress = PerPlayerProgress.FindOrAdd(Player);
	Progress = FMath::Clamp(Progress, 0, RequiredSequence.Num() - 1);
	if (Step->StepID == RequiredSequence[Progress])
	{
		++Progress;
		if (Progress < RequiredSequence.Num()) return true;
		PerPlayerProgress.Remove(Player);
		for (const FPlayerQuest& Quest : Player->Quests)
		{
			if (IsValid(Quest.Definition) && Quest.Definition->ProgressEvent == ProgressEvent && Quest.State == EPlayerQuestState::Active)
				return Player->AddQuestProgress(Quest.Definition->QuestID, 1);
		}
		return false;
	}

	// A wrong input resets the sequence, but immediately lets the first answer start a fresh attempt.
	Progress = Step->StepID == RequiredSequence[0] ? 1 : 0;
	return false;
}
