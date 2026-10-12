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
	// Blueprint component defaults are the authored source of truth. Older assets may only
	// have the legacy field populated, so use it solely as a fallback and then keep it synced.
	if (!NativeDialogue->Dialogue && NativeDialogueDefinition)
	{
		NativeDialogue->Dialogue = NativeDialogueDefinition;
	}
	NativeDialogueDefinition = NativeDialogue->Dialogue;
	for (UQuestDefinition* Quest : NativeQuestDefinitions) if (Quest) NativeDialogue->OfferedQuests.AddUnique(Quest);
	// Preserve old Blueprint data, but the Talker Component is now the runtime source.
	if (NativeQuestGiver) NativeQuestGiver->AvailableQuests = NativeQuestDefinitions;
	Super::BeginPlay();
}
