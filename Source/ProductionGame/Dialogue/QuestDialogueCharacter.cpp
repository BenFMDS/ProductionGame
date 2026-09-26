#include "Dialogue/QuestDialogueCharacter.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/QuestGiverComponent.h"

AQuestDialogueCharacter::AQuestDialogueCharacter()
{
	bReplicates = true;
	NativeDialogue = CreateDefaultSubobject<UDialogueComponent>(TEXT("NativeDialogue"));
	NativeQuestGiver = CreateDefaultSubobject<UQuestGiverComponent>(TEXT("NativeQuestGiver"));
}

void AQuestDialogueCharacter::BeginPlay()
{
	NativeDialogue->Dialogue = NativeDialogueDefinition;
	NativeQuestGiver->AvailableQuests = NativeQuestDefinitions;
	Super::BeginPlay();
}
