#include "Dialogue/QuestNPC.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/QuestGiverComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AQuestNPC::AQuestNPC()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultMesh.Succeeded()) Mesh->SetStaticMesh(DefaultMesh.Object);
	DialogueComponent = CreateDefaultSubobject<UDialogueComponent>(TEXT("Dialogue"));
	QuestGiver = CreateDefaultSubobject<UQuestGiverComponent>(TEXT("QuestGiver"));
}
