#include "Heist/RobberTriggerZones.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberTriggerZone::ARobberTriggerZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(100, 150, 150));
	Box->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Box;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Box);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		DoorMesh->SetStaticMesh(Cube.Object);
	}
	// The cube is 100 units across; match the default box.
	DoorMesh->SetRelativeScale3D(FVector(2.f, 3.f, 3.f));
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Box);
	LabelText->SetRelativeLocation(FVector(-110, 0, 170));
	LabelText->SetRelativeRotation(FRotator(0, 180, 0));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextBottom);
	LabelText->SetWorldSize(36.f);
}

void ARobberTriggerZone::CountPlayers(int32& OutInside, int32& OutTotal) const
{
	OutInside = OutTotal = 0;
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!GameState)
	{
		return;
	}
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		++OutTotal;
		OutInside += Box->IsOverlappingActor(Pawn);
	}
}

void ARobberTriggerZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!GameState)
	{
		return;
	}
	int32 Inside = 0, Total = 0;
	CountPlayers(Inside, Total);
	if (HasAuthority())
	{
		ServerUpdate(GameState, Inside, Total);
	}
	LabelText->SetText(GetLabel(GameState, Inside, Total));
}

ARobberLobbyExit::ARobberLobbyExit()
{
	Label = NSLOCTEXT("RobberZone", "MuseumDoor", "MUSEUM DOOR");
}

void ARobberLobbyExit::ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total)
{
	if (RobberStage::IsInLobby(GameState->GetGameStage()) && Total > 0 && Inside == Total)
	{
		GameState->StartBreakIn();
	}
}

FText ARobberLobbyExit::GetLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const
{
	if (!RobberStage::IsInLobby(GameState->GetGameStage()))
	{
		return Label;
	}
	if (const ARobberEntryPoint* Winner = GameState->GetWinningEntry())
	{
		return FText::Format(NSLOCTEXT("RobberZone", "DoorReady", "{0}\nGoing in by: {1}\nEveryone walk in ({2}/{3})"), Label, Winner->DisplayName, Inside, Total);
	}
	return FText::Format(NSLOCTEXT("RobberZone", "DoorVote", "{0}\nVote for a way in first"), Label);
}

ARobberGemRoomApproach::ARobberGemRoomApproach()
{
	Label = NSLOCTEXT("RobberZone", "GemRoomDoor", "GEM ROOM");
}

void ARobberGemRoomApproach::ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total)
{
	if (GameState->GetGameStage() == RobberStage::Investigate && Inside > 0)
	{
		GameState->StartHeist();
	}
}

FText ARobberGemRoomApproach::GetLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const
{
	return Label;
}
