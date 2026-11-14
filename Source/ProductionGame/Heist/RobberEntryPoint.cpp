#include "Heist/RobberEntryPoint.h"

#include "Algo/AllOf.h"
#include "EngineUtils.h"
#include "ProductionGameCharacter.h"
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

bool ARobberTourEntryPoint::TryHide(ARobberGameState* GameState, APlayerState* Player, ARobberHidingSpot* HidingSpot, FText& OutReason)
{
	if (!GameState->GetCrewStatus(Player).bHasTicket)
	{
		OutReason = NSLOCTEXT("RobberEntry", "NeedTicket", "Buy a ticket at the desk first.");
		return false;
	}
	if (!Player->GetPawn())
	{
		return false;
	}
	GameState->SetHidingSpot(Player, HidingSpot);
	return true;
}

void ARobberTourEntryPoint::GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const
{
	// Past the ticket the next step is "pick a spot", and which one is the player's choice.
	if (GameState->GetCrewStatus(Viewer).bHasTicket)
	{
		return;
	}
	for (TActorIterator<ARobberTicketDesk> It(GetWorld()); It; ++It)
	{
		if (It->IsActive(GameState))
		{
			Out.Add({ It->GetActorLocation(), NSLOCTEXT("RobberMarker", "Desk", "Ticket desk") });
			return;
		}
	}
}

void ARobberSkylightEntryPoint::GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const
{
	if (!Skylight || IsInside(Viewer ? Viewer->GetPawn() : nullptr))
	{
		return;
	}
	if (Skylight->IsCut() && !Skylight->IsRopeDown())
	{
		Out.Add({ Skylight->GetRopeAnchorLocation(), NSLOCTEXT("RobberMarker", "Anchor", "Rope anchor") });
		return;
	}
	Out.Add({ Skylight->GetActorLocation(), NSLOCTEXT("RobberMarker", "Skylight", "Skylight") });
}

void ARobberSewerEntryPoint::GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const
{
	if (IsInside(Viewer ? Viewer->GetPawn() : nullptr))
	{
		return;
	}
	if (Water && !Water->IsDrained())
	{
		for (const ARobberSewerValve* Valve : Water->Valves)
		{
			if (Valve)
			{
				Out.Add({ Valve->GetActorLocation(), NSLOCTEXT("RobberMarker", "Valve", "Valve") });
			}
		}
		return;
	}
	// Finding the crowbar is the job here, so nothing points at it.
	if (Grate && (Grate->IsOpen() || !Crowbar || Crowbar->IsTaken()))
	{
		Out.Add({ Grate->GetActorLocation(), NSLOCTEXT("RobberMarker", "Grate", "Grate") });
	}
}

void ARobberLoadingDockEntryPoint::GetBreakInMarkers(const ARobberGameState* GameState, const APlayerState* Viewer, TArray<FRobberMarkerTarget>& Out) const
{
	if (RollUpDoor && !IsInside(Viewer ? Viewer->GetPawn() : nullptr))
	{
		Out.Add({ RollUpDoor->GetActorLocation(), NSLOCTEXT("RobberMarker", "RollUp", "Roll-up door") });
	}
}

void ARobberTourEntryPoint::BeginBreakIn(ARobberGameState* GameState)
{
	Super::BeginBreakIn(GameState);
	for (const FRobberCrewStatus& Status : GameState->GetCrew())
	{
		ARobberController::Tell(Status.Player, NSLOCTEXT("RobberEntry", "TourStart", "The museum closes soon. Buy a ticket and find somewhere to hide."));
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
			// Closing's the point of hiding on the Tour, so everyone comes back out to start the night.
			if (AProductionGameCharacter* Character = Status.Player ? Cast<AProductionGameCharacter>(Status.Player->GetPawn()) : nullptr)
			{
				Character->ExitHiding();
			}
			ARobberController::Tell(Status.Player, NSLOCTEXT("RobberEntry", "Closed", "The museum is closed. Time to sneak out."));
		}
		else
		{
			// Getting caught isn't built yet, so for now they just carry on.
			UE_LOG(LogTemp, Warning, TEXT("Closing-Time Tour: %s wasn't hidden at closing"), Status.Player ? *Status.Player->GetPlayerName() : TEXT("?"));
			ARobberController::Tell(Status.Player, NSLOCTEXT("RobberEntry", "NotHidden", "You weren't hidden at closing! (Getting caught isn't built yet.)"));
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
			ARobberController::Tell(Status.Player, NSLOCTEXT("RobberEntry", "AllIn", "Everyone's in. Find the gem room."));
		}
		else
		{
			// Getting caught isn't built yet, so for now they just carry on.
			UE_LOG(LogTemp, Warning, TEXT("%s: %s wasn't inside when the break-in ended"), *DisplayName.ToString(), Status.Player ? *Status.Player->GetPlayerName() : TEXT("?"));
			ARobberController::Tell(Status.Player, NSLOCTEXT("RobberEntry", "NotInside", "Out of time and you're not in! (Getting caught isn't built yet.)"));
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

ARobberLoadingDockEntryPoint::ARobberLoadingDockEntryPoint()
{
	DisplayName = NSLOCTEXT("RobberEntry", "DockName", "Loading Dock");
	BreakInObjective = NSLOCTEXT("RobberEntry", "DockObjective", "Lift the roll-up door from the truck bay and get everyone under it before it gets too loud.");
	BreakInTimeLimit = 120.f;
}

void ARobberLoadingDockEntryPoint::BeginBreakIn(ARobberGameState* GameState)
{
	Super::BeginBreakIn(GameState);
	GameState->TellCrew(NSLOCTEXT("RobberEntry", "DockStart", "You're in the truck bay. One of you lifts the door, everyone else gets under it fast."));
}

FText ARobberLoadingDockEntryPoint::GetOutsideStatus(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (!RollUpDoor)
	{
		return BreakInObjective;
	}
	if (RollUpDoor->IsTooLoud())
	{
		return NSLOCTEXT("RobberEntry", "DockLoud", "A guard heard the door! Wait for it to go quiet.");
	}
	FText Action;
	if (RollUpDoor->IsHolding(Viewer))
	{
		Action = NSLOCTEXT("RobberEntry", "DockHolding", "You're holding the door up. Go through last; it drops when you walk away.");
	}
	else if (RollUpDoor->IsHeld())
	{
		Action = NSLOCTEXT("RobberEntry", "DockHeld", "The door's going up. Get under it!");
	}
	else
	{
		Action = NSLOCTEXT("RobberEntry", "DockIdle", "Interact with the roll-up door to lift it.");
	}
	return FText::Format(NSLOCTEXT("RobberEntry", "DockProgress", "{0}\nDoor up {1}%, noise {2}%"), Action,
		FMath::RoundToInt(RollUpDoor->GetOpenness() * 100.f), FMath::RoundToInt(RollUpDoor->GetNoise() * 100.f));
}
