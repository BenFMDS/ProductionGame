#include "Dialogue/DialogueInteractionLibrary.h"
#include "Quest/PlayerQuestComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void UDialogueInteractionLibrary::TalkToNPC(AActor* NPC, AActor* InteractingPlayer)
{
	APlayerController* PC = Cast<APlayerController>(InteractingPlayer);
	if (APawn* Pawn = Cast<APawn>(InteractingPlayer)) PC = Cast<APlayerController>(Pawn->GetController());
	if (!IsValid(PC) || (!PC->HasAuthority() && !PC->IsLocalController())) return;
	if (auto* Player = PC->FindComponentByClass<UPlayerQuestComponent>()) Player->ServerTalkTo(NPC);
}

void UDialogueInteractionLibrary::CollectQuestObject(AActor* QuestObject, AActor* InteractingPlayer)
{
	APlayerController* PC = Cast<APlayerController>(InteractingPlayer);
	if (APawn* Pawn = Cast<APawn>(InteractingPlayer)) PC = Cast<APlayerController>(Pawn->GetController());
	// The existing interaction Blueprint also forwards this event to the server.
	// Only the local input sends our owned RPC; never trust its forwarded player parameter.
	if (!IsValid(PC) || !PC->IsLocalController()) return;
	if (auto* Player = PC->FindComponentByClass<UPlayerQuestComponent>()) Player->ServerCollectQuestObject(QuestObject);
}
