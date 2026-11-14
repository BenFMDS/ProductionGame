#include "Heist/RobberEntryPoint.h"

#include "AIController.h"
#include "Algo/AllOf.h"
#include "BrainComponent.h"
#include "EngineUtils.h"
#include "Heist/RobberVisitor.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig.h"
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
	if (bDaytimeMuseum)
	{
		// No timer: closing waits for the whole crew.
		GameState->SetPhaseTimer(0.f);
		OpenForTheDay(GameState);
	}
	for (const FRobberCrewStatus& Status : GameState->GetCrew())
	{
		ARobberController::Tell(Status.Player, bDaytimeMuseum
			? NSLOCTEXT("RobberEntry", "TourStartDay", "The museum's open. Buy a ticket, then find somewhere to hide: it closes once you're all tucked away.")
			: NSLOCTEXT("RobberEntry", "TourStart", "The museum closes soon. Buy a ticket and find somewhere to hide."));
	}
}

void ARobberTourEntryPoint::OpenForTheDay(ARobberGameState* GameState)
{
	UWorld* World = GetWorld();
	bOpenForTheDay = true;

	// Guards are BP_Guard pawns placed in the night map. Asleep and out of sight until closing, rather than
	// destroyed, so whatever the level designer set on each one (patrol points) survives.
	SleepingGuards.Reset();
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		AAIController* Brain = Cast<AAIController>(It->GetController());
		if (!Brain || !It->GetClass()->GetName().StartsWith(TEXT("BP_Guard")))
		{
			continue;
		}
		SleepingGuards.Add(*It);
		It->SetActorHiddenInGame(true);
		It->SetActorEnableCollision(false);
		It->SetActorTickEnabled(false);
		Brain->StopMovement();
		if (Brain->BrainComponent)
		{
			Brain->BrainComponent->StopLogic(TEXT("Museum open"));
		}
		if (UAIPerceptionComponent* Perception = Brain->FindComponentByClass<UAIPerceptionComponent>())
		{
			for (auto Sense = Perception->GetSensesConfigIterator(); Sense; ++Sense)
			{
				if (*Sense)
				{
					Perception->SetSenseEnabled((*Sense)->GetSenseImplementation(), false);
				}
			}
			Perception->ForgetAll();
		}
	}

	// The ticket seller stands on the far side of the desk from where the crew walks in, facing them.
	const ARobberTicketDesk* Desk = nullptr;
	for (TActorIterator<ARobberTicketDesk> It(World); It && !Desk; ++It)
	{
		Desk = *It;
	}
	FVector Centre = GetActorLocation();
	if (Desk)
	{
		FVector Origin, Extent;
		Desk->GetActorBounds(true, Origin, Extent);
		Centre = Origin;
		const FVector Away = (Origin - GetActorLocation()).GetSafeNormal2D();
		// Snap to whichever of the desk's own sides faces away, so the seller is square behind it.
		const FVector DeskX = Desk->GetActorForwardVector().GetSafeNormal2D();
		const FVector DeskY = Desk->GetActorRightVector().GetSafeNormal2D();
		const bool bAlongX = FMath::Abs(FVector::DotProduct(Away, DeskX)) >= FMath::Abs(FVector::DotProduct(Away, DeskY));
		const FVector Axis = (bAlongX ? DeskX : DeskY) * FMath::Sign(FVector::DotProduct(Away, bAlongX ? DeskX : DeskY) + UE_SMALL_NUMBER);
		const float Depth = FMath::Abs(FVector::DotProduct(Extent, FVector(FMath::Abs(Axis.X), FMath::Abs(Axis.Y), 0.f)));
		const FVector Spot = FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z + 100.f) + Axis * (Depth + 55.f);
		ARobberVisitor::Spawn(World, FTransform((-Axis).Rotation(), Spot), true, FLinearColor(0.25f, 0.45f, 1.f), 1.f);
	}

	static const FLinearColor Clothes[] = {
		FLinearColor(0.80f, 0.25f, 0.20f), FLinearColor(0.20f, 0.45f, 0.75f), FLinearColor(0.30f, 0.60f, 0.30f),
		FLinearColor(0.90f, 0.70f, 0.25f), FLinearColor(0.55f, 0.35f, 0.65f), FLinearColor(0.95f, 0.55f, 0.65f),
		FLinearColor(0.35f, 0.70f, 0.70f), FLinearColor(0.85f, 0.45f, 0.15f), FLinearColor(0.55f, 0.50f, 0.40f),
		FLinearColor(0.85f, 0.85f, 0.80f),
	};
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	int32 Spawned = 0;
	for (int32 Index = 0; Nav && Index < VisitorCount; ++Index)
	{
		FNavLocation Found;
		if (!Nav->GetRandomReachablePointInRadius(Centre, 3000.f, Found))
		{
			continue;
		}
		const FRotator Facing(0.f, FMath::FRandRange(-180.f, 180.f), 0.f);
		Spawned += ARobberVisitor::Spawn(World, FTransform(Facing, Found.Location + FVector(0.f, 0.f, 100.f)), false,
			Clothes[Index % UE_ARRAY_COUNT(Clothes)], FMath::FRandRange(0.92f, 1.08f)) ? 1 : 0;
	}
	UE_LOG(LogTemp, Display, TEXT("Closing-Time Tour: museum open, %d visitors, ticket seller %s, %d guards asleep"),
		Spawned, Desk ? TEXT("at the desk") : TEXT("missing (no desk)"), SleepingGuards.Num());
}

void ARobberTourEntryPoint::CloseForTheNight()
{
	if (!bOpenForTheDay)
	{
		return;
	}
	bOpenForTheDay = false;
	int32 Removed = 0;
	for (TActorIterator<ARobberVisitor> It(GetWorld()); It; ++It)
	{
		It->Destroy();
		++Removed;
	}
	int32 Woken = 0;
	for (const TWeakObjectPtr<APawn>& Weak : SleepingGuards)
	{
		APawn* Guard = Weak.Get();
		AAIController* Brain = Guard ? Cast<AAIController>(Guard->GetController()) : nullptr;
		if (!Brain)
		{
			continue;
		}
		Guard->SetActorHiddenInGame(false);
		Guard->SetActorEnableCollision(true);
		Guard->SetActorTickEnabled(true);
		if (UAIPerceptionComponent* Perception = Brain->FindComponentByClass<UAIPerceptionComponent>())
		{
			for (auto Sense = Perception->GetSensesConfigIterator(); Sense; ++Sense)
			{
				if (*Sense)
				{
					Perception->SetSenseEnabled((*Sense)->GetSenseImplementation(), true);
				}
			}
		}
		if (Brain->BrainComponent)
		{
			Brain->BrainComponent->RestartLogic();
		}
		++Woken;
	}
	SleepingGuards.Reset();
	UE_LOG(LogTemp, Display, TEXT("Closing-Time Tour: museum closed, %d visitors gone, %d guards on shift"), Removed, Woken);
}

void ARobberTourEntryPoint::TickBreakIn(ARobberGameState* GameState)
{
	const float Closing = GameState->GetClosingTimeElapsed();
	if (Closing >= 0.f)
	{
		if (Closing >= NightSwapAt)
		{
			CloseForTheNight();
		}
		if (Closing >= ARobberGameState::ClosingTimeSeconds)
		{
			GameState->FinishBreakIn();
		}
		return;
	}
	const TArray<FRobberCrewStatus>& Crew = GameState->GetCrew();
	const bool bEveryoneHidden = Crew.Num() > 0 && Algo::AllOf(Crew, [](const FRobberCrewStatus& Status) { return Status.HidingSpot != nullptr; });
	if (bEveryoneHidden)
	{
		if (bDaytimeMuseum)
		{
			GameState->StartClosingTime();
		}
		else
		{
			GameState->FinishBreakIn();
		}
	}
}

void ARobberTourEntryPoint::EndBreakIn(ARobberGameState* GameState)
{
	if (bDaytimeMuseum)
	{
		// Normally done mid-card already; this catches a break-in ended another way (dev menu skip).
		CloseForTheNight();
		for (const FRobberCrewStatus& Status : GameState->GetCrew())
		{
			ARobberController::Tell(Status.Player, Status.HidingSpot
				? NSLOCTEXT("RobberEntry", "ClosedStay", "The museum is closed and the guards are on. Climb out when the coast is clear.")
				: NSLOCTEXT("RobberEntry", "ClosedSkipped", "The museum is closed. The guards are on shift."));
		}
		return;
	}
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
	if (Status.HidingSpot && bDaytimeMuseum)
	{
		if (GameState->IsClosingTime())
		{
			return NSLOCTEXT("RobberEntry", "ClosingNow", "Closing time. Stay put.");
		}
		TArray<FString> Waiting;
		for (const FRobberCrewStatus& Each : GameState->GetCrew())
		{
			if (!Each.HidingSpot && Each.Player)
			{
				Waiting.Add(Each.Player->GetPlayerName());
			}
		}
		return Waiting.Num() == 0
			? NSLOCTEXT("RobberEntry", "AllHidden", "Everyone's hidden. Closing time...")
			: FText::Format(NSLOCTEXT("RobberEntry", "WaitingFor", "You're hidden. Waiting for: {0}"), FText::FromString(FString::Join(Waiting, TEXT(", "))));
	}
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
