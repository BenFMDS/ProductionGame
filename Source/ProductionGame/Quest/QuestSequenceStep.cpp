#include "Quest/QuestSequenceStep.h"

#include "Quest/QuestSequencePuzzle.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AQuestSequenceStep::AQuestSequenceStep()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded()) Mesh->SetStaticMesh(DefaultMesh.Object);
}

bool AQuestSequenceStep::Activate(UPlayerQuestComponent* Player)
{
	return IsValid(Puzzle) && Puzzle->TryActivate(Player, this);
}
