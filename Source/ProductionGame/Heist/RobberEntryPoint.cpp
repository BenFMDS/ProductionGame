#include "Heist/RobberEntryPoint.h"

#include "Algo/AllOf.h"
#include "Components/ArrowComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberInteractables.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

ARobberEntryPoint::ARobberEntryPoint()
{
	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->ArrowSize = 2.f;
	RootComponent = Arrow;

	EditorLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EditorLabel"));
	EditorLabel->SetupAttachment(Arrow);
	EditorLabel->SetRelativeLocation(FVector(0, 0, 150));
	EditorLabel->SetHorizontalAlignment(EHTA_Center);
	EditorLabel->SetText(NSLOCTEXT("RobberEntry", "EditorLabel", "Break-in spawn"));
	EditorLabel->SetHiddenInGame(true);
}

FTransform ARobberEntryPoint::GetSpawnTransform(int32 PlayerIndex) const
{
	const FVector Offset = SpawnOffsets.Num() > 0 ? SpawnOffsets[PlayerIndex % SpawnOffsets.Num()] : FVector::ZeroVector;
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Offset));
}

void ARobberEntryPoint::BeginBreakIn(ARobberGameState* GameState)
{
	GameState->SetPhaseTimer(BreakInTimeLimit);
}

void ARobberEntryPoint::OnBreakInTimerExpired(ARobberGameState* GameState)
{
	GameState->FinishBreakIn();
}

FText ARobberEntryPoint::GetBreakInStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return BreakInObjective;
}

ARobberTourEntryPoint::ARobberTourEntryPoint()
{
	DisplayName = NSLOCTEXT("RobberEntry", "TourName", "Closing-Time Tour");
	BreakInObjective = NSLOCTEXT("RobberEntry", "TourObjective", "Buy a ticket, then hide in an exhibit before the museum closes.");
	BreakInTimeLimit = 90.f;
}

namespace
{
	void SetFrozen(APlayerState* Player, bool bFrozen)
	{
		if (ARobberController* Controller = Player ? Cast<ARobberController>(Player->GetOwningController()) : nullptr)
		{
			Controller->SetHeistFrozen(bFrozen);
		}
	}

	void TellPlayer(APlayerState* Player, const FText& Message)
	{
		if (ARobberController* Controller = Player ? Cast<ARobberController>(Player->GetOwningController()) : nullptr)
		{
			Controller->ClientHeistMessage(Message);
		}
	}
}

bool ARobberTourEntryPoint::TryHide(ARobberGameState* GameState, APlayerState* Player, ARobberHidingSpot* HidingSpot, FText& OutReason)
{
	if (!GameState->GetCrewStatus(Player).bHasTicket)
	{
		OutReason = NSLOCTEXT("RobberEntry", "NeedTicket", "Buy a ticket at the desk first.");
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (!Pawn)
	{
		return false;
	}
	GameState->SetHidingSpot(Player, HidingSpot);
	Pawn->TeleportTo(HidingSpot->GetHideLocation(), Pawn->GetActorRotation());
	SetFrozen(Player, true);
	return true;
}

void ARobberTourEntryPoint::BeginBreakIn(ARobberGameState* GameState)
{
	Super::BeginBreakIn(GameState);
	for (const FRobberCrewStatus& Status : GameState->GetCrew())
	{
		TellPlayer(Status.Player, NSLOCTEXT("RobberEntry", "TourStart", "The museum closes soon. Buy a ticket and find somewhere to hide."));
	}
}

void ARobberTourEntryPoint::TickBreakIn(ARobberGameState* GameState)
{
	const TArray<FRobberCrewStatus>& Crew = GameState->GetCrew();
	const bool bEveryoneHidden = Crew.Num() > 0 && Algo::AllOf(Crew, [](const FRobberCrewStatus& Status) { return Status.HidingSpot != nullptr; });
	if (bEveryoneHidden)
	{
		GameState->FinishBreakIn();
	}
}

void ARobberTourEntryPoint::EndBreakIn(ARobberGameState* GameState)
{
	for (const FRobberCrewStatus& Status : GameState->GetCrew())
	{
		if (Status.HidingSpot)
		{
			SetFrozen(Status.Player, false);
			TellPlayer(Status.Player, NSLOCTEXT("RobberEntry", "Closed", "The museum is closed. Time to sneak out."));
		}
		else
		{
			// Getting caught isn't built yet, so for now they just carry on.
			UE_LOG(LogTemp, Warning, TEXT("Closing-Time Tour: %s wasn't hidden at closing"), Status.Player ? *Status.Player->GetPlayerName() : TEXT("?"));
			TellPlayer(Status.Player, NSLOCTEXT("RobberEntry", "NotHidden", "You weren't hidden at closing! (Getting caught isn't built yet.)"));
		}
	}
}

FText ARobberTourEntryPoint::GetBreakInStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	const FRobberCrewStatus Status = GameState->GetCrewStatus(Viewer);
	if (Status.HidingSpot)
	{
		return NSLOCTEXT("RobberEntry", "Hidden", "You're hidden. Wait for closing.");
	}
	return Status.bHasTicket
		? NSLOCTEXT("RobberEntry", "PickSpot", "Pick an exhibit to hide in before the museum closes.")
		: NSLOCTEXT("RobberEntry", "BuyTicket", "Buy a ticket at the desk.");
}
