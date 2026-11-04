#include "Dialogue/QuestDialogueCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/QuestGiverComponent.h"

namespace
{
	/** The mesh is a component on BP_TalkingMaster, so it can only be found by name from here. */
	void ApplyTempCube(AQuestDialogueCharacter* Npc)
	{
		if (!Npc->TempCubeMaterial)
		{
			return;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes;
		Npc->GetComponents(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() == TEXT("ModelMesh"))
			{
				Mesh->SetMaterial(0, Npc->TempCubeMaterial);
			}
		}
	}
}

AQuestDialogueCharacter::AQuestDialogueCharacter()
{
	bReplicates = true;
	NativeDialogue = CreateDefaultSubobject<UDialogueComponent>(TEXT("NativeDialogue"));
	NativeQuestGiver = CreateDefaultSubobject<UQuestGiverComponent>(TEXT("NativeQuestGiver"));
}

void AQuestDialogueCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyTempCube(this);
}

void AQuestDialogueCharacter::BeginPlay()
{
	// Actors already placed in a saved map don't rerun construction, so this catches a colour changed since the map was saved.
	ApplyTempCube(this);
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
