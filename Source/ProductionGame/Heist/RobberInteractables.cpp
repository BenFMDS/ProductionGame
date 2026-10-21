#include "Heist/RobberInteractables.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberInteractable::ARobberInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	RootComponent = Mesh;

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Mesh);
	LabelText->SetRelativeLocation(FVector(0, 0, 90));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextBottom);
	LabelText->SetWorldSize(30.f);
	LabelText->SetTextRenderColor(FColor::White);
	LabelText->SetAbsolute(false, false, true);
}

void ARobberInteractable::HandleInteract(AActor* InteractingPlayer)
{
	if (!HasAuthority())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(InteractingPlayer);
	if (AController* Controller = Cast<AController>(InteractingPlayer))
	{
		Pawn = Controller->GetPawn();
	}
	APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (Player && GameState)
	{
		OnServerInteract(Pawn, Player, GameState);
	}
}

FString ARobberInteractable::GetInteractPrompt() const
{
	const APlayerController* LocalPlayer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ARobberGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
	return GameState ? GetPrompt(GameState, LocalPlayer ? LocalPlayer->PlayerState : nullptr).ToString() : Prompt.ToString();
}

void ARobberInteractable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>())
	{
		LabelText->SetText(GetLabel(GameState));
	}
}

void ARobberVoteStation::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	GameState->CastVote(Player, Entry);
}

FText ARobberVoteStation::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (GameState->GetGameStage() != RobberStage::Lobby || !Entry)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("RobberLobby", "VotePrompt", "Vote: {0}"), Entry->DisplayName);
}

FText ARobberVoteStation::GetLabel(const ARobberGameState* GameState) const
{
	if (!Entry)
	{
		return NSLOCTEXT("RobberLobby", "NoEntry", "No entry set");
	}
	return FText::Format(NSLOCTEXT("RobberLobby", "VoteLabel", "{0}\n{1} {1}|plural(one=vote,other=votes)"), Entry->DisplayName, GameState->GetVoteCount(Entry));
}

ARobberTicketDesk::ARobberTicketDesk()
{
	Label = NSLOCTEXT("RobberTour", "DeskLabel", "TICKETS");
	Prompt = NSLOCTEXT("RobberTour", "DeskPrompt", "Buy a ticket");
}

void ARobberTicketDesk::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (GameState->GetGameStage() == RobberStage::BreakIn)
	{
		GameState->GiveTicket(Player);
	}
}

FText ARobberTicketDesk::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (GameState->GetGameStage() != RobberStage::BreakIn)
	{
		return FText::GetEmpty();
	}
	return GameState->GetCrewStatus(Viewer).bHasTicket ? NSLOCTEXT("RobberTour", "HaveTicket", "You have a ticket") : Prompt;
}

ARobberHidingSpot::ARobberHidingSpot()
{
	Label = NSLOCTEXT("RobberTour", "SpotLabel", "Exhibit");
	Prompt = NSLOCTEXT("RobberTour", "SpotPrompt", "Hide here");
}

void ARobberHidingSpot::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	ARobberTourEntryPoint* Tour = Cast<ARobberTourEntryPoint>(GameState->GetActiveEntry());
	if (!Tour || GameState->GetGameStage() != RobberStage::BreakIn || GameState->GetCrewStatus(Player).HidingSpot == this)
	{
		return;
	}
	FText Reason;
	if (!Tour->TryHide(GameState, Player, this, Reason) && !Reason.IsEmpty())
	{
		if (ARobberController* Controller = Cast<ARobberController>(Pawn->GetController()))
		{
			Controller->ClientHeistMessage(Reason);
		}
	}
}

FText ARobberHidingSpot::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (GameState->GetGameStage() != RobberStage::BreakIn || GameState->GetCrewStatus(Viewer).HidingSpot)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("RobberTour", "HideIn", "Hide in the {0}"), Label);
}
