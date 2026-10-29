#include "Heist/RobberEntryPoint.h"

#include "Algo/AllOf.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberBreakInProps.h"
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

ARobberSneakInEntryPoint::ARobberSneakInEntryPoint()
{
	InsideArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InsideArea"));
	InsideArea->SetupAttachment(Arrow);
	// Placed in world space by whoever lays out the night, so moving the spawn doesn't drag the museum with it.
	InsideArea->SetUsingAbsoluteLocation(true);
	InsideArea->SetUsingAbsoluteRotation(true);
	InsideArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InsideArea->SetBoxExtent(FVector(500, 500, 300));
	InsideArea->ShapeColor = FColor::Green;
}

bool ARobberSneakInEntryPoint::IsInside(const APawn* Pawn) const
{
	return Pawn && InsideArea->Bounds.GetBox().IsInside(Pawn->GetActorLocation());
}

void ARobberSneakInEntryPoint::TickBreakIn(ARobberGameState* GameState)
{
	for (const FRobberCrewStatus& Status : GameState->GetCrew())
	{
		if (!Status.bInside && Status.Player && IsInside(Status.Player->GetPawn()))
		{
			GameState->SetInside(Status.Player);
		}
	}
	const TArray<FRobberCrewStatus>& Crew = GameState->GetCrew();
	if (Crew.Num() > 0 && Algo::AllOf(Crew, [](const FRobberCrewStatus& Status) { return Status.bInside; }))
	{
		GameState->FinishBreakIn();
	}
}

void ARobberSneakInEntryPoint::EndBreakIn(ARobberGameState* GameState)
{
	for (const FRobberCrewStatus& Status : GameState->GetCrew())
	{
		if (Status.bInside)
		{
			TellPlayer(Status.Player, NSLOCTEXT("RobberEntry", "AllIn", "Everyone's in. Find the gem room."));
		}
		else
		{
			// Getting caught isn't built yet, so for now they just carry on.
			UE_LOG(LogTemp, Warning, TEXT("%s: %s wasn't inside when the break-in ended"), *DisplayName.ToString(), Status.Player ? *Status.Player->GetPlayerName() : TEXT("?"));
			TellPlayer(Status.Player, NSLOCTEXT("RobberEntry", "NotInside", "Out of time and you're not in! (Getting caught isn't built yet.)"));
		}
	}
}

FText ARobberSneakInEntryPoint::GetBreakInStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return GameState->GetCrewStatus(Viewer).bInside
		? NSLOCTEXT("RobberEntry", "InsideWait", "You're in. Wait for the rest of the crew.")
		: GetOutsideStatus(GameState, Viewer);
}

ARobberSkylightEntryPoint::ARobberSkylightEntryPoint()
{
	DisplayName = NSLOCTEXT("RobberEntry", "SkylightName", "Skylight");
	BreakInObjective = NSLOCTEXT("RobberEntry", "SkylightObjective", "Cut through the skylight without making too much noise, drop the rope and climb down.");
	BreakInTimeLimit = 120.f;
}

void ARobberSkylightEntryPoint::BeginBreakIn(ARobberGameState* GameState)
{
	Super::BeginBreakIn(GameState);
	GameState->TellCrew(NSLOCTEXT("RobberEntry", "SkylightStart", "You're on the roof. Cut the skylight, but pause when it gets loud or a guard will hear."));
}

void ARobberSkylightEntryPoint::TickBreakIn(ARobberGameState* GameState)
{
	if (!Skylight || Skylight->IsRopeDown())
	{
		Super::TickBreakIn(GameState);
	}
}

FText ARobberSkylightEntryPoint::GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (!Skylight)
	{
		return BreakInObjective;
	}
	if (Skylight->IsRopeDown())
	{
		return NSLOCTEXT("RobberEntry", "SkylightOpen", "The rope's down. Climb down into the museum.");
	}
	if (Skylight->IsCut())
	{
		return NSLOCTEXT("RobberEntry", "SkylightDropRope", "The glass is out. Drop the rope down from the anchor.");
	}
	if (Skylight->IsTooLoud())
	{
		return NSLOCTEXT("RobberEntry", "SkylightLoud", "A guard heard you! Stop cutting and wait for it to go quiet.");
	}
	const FText Action = Skylight->IsCutting(Viewer)
		? NSLOCTEXT("RobberEntry", "SkylightCutting", "Cutting. Interact again to stop before it gets too loud.")
		: NSLOCTEXT("RobberEntry", "SkylightIdle", "Interact with the skylight to start cutting.");
	return FText::Format(NSLOCTEXT("RobberEntry", "SkylightProgress", "{0}\nGlass cut {1}%, noise {2}%"), Action,
		FMath::RoundToInt(Skylight->GetCutProgress() * 100.f), FMath::RoundToInt(Skylight->GetNoise() * 100.f));
}

ARobberSewerEntryPoint::ARobberSewerEntryPoint()
{
	DisplayName = NSLOCTEXT("RobberEntry", "SewerName", "Sewer");
	BreakInObjective = NSLOCTEXT("RobberEntry", "SewerObjective", "Get through the sewer tunnels and up through the grate under the museum.");
	BreakInTimeLimit = 240.f;
	LobbyExit = TEXT("Manhole");
}

void ARobberSewerEntryPoint::BeginBreakIn(ARobberGameState* GameState)
{
	Super::BeginBreakIn(GameState);
	GameState->TellCrew(NSLOCTEXT("RobberEntry", "SewerStart", "You're in the sewers. Find the way to the grate under the museum."));
}

FText ARobberSewerEntryPoint::GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (Grate && Grate->IsOpen())
	{
		return NSLOCTEXT("RobberEntry", "SewerGrateOpen", "The grate's open. Climb up into the museum.");
	}
	if (Water && !Water->IsDrained())
	{
		return NSLOCTEXT("RobberEntry", "SewerFlooded", "The tunnel to the museum is flooded. Find the two valves and open them at the same time.");
	}
	if (Crowbar && !Crowbar->IsTaken())
	{
		return NSLOCTEXT("RobberEntry", "SewerFindCrowbar", "The grate's rusted shut. Find something down here to pry it open with.");
	}
	return NSLOCTEXT("RobberEntry", "SewerPry", "Bring the crowbar to the grate and pry it open.");
}
