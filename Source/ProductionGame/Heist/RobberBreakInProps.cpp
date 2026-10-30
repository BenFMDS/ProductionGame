#include "Heist/RobberBreakInProps.h"

#include "Algo/AllOf.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberStage.h"
#include "Net/UnrealNetwork.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Props keep working after the break-in timer runs out, so nobody gets stuck on the roof or in the tunnels.
	bool IsNightRunning(const ARobberGameState* GameState)
	{
		return GameState && !RobberStage::IsInLobby(GameState->GetGameStage());
	}

	void TellPlayer(const APawn* Pawn, const FText& Message)
	{
		if (ARobberController* Controller = Pawn ? Cast<ARobberController>(Pawn->GetController()) : nullptr)
		{
			Controller->ClientHeistMessage(Message);
		}
	}

	UPlayerQuestComponent* FindPlayerQuests(const APlayerState* Player)
	{
		const AController* Controller = Player ? Player->GetOwningController() : nullptr;
		return Controller ? Controller->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	}

	void SetShown(UPrimitiveComponent* Component, bool bShown)
	{
		Component->SetHiddenInGame(!bShown, true);
		Component->SetCollisionEnabled(bShown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}

	FText Percent(float Fraction)
	{
		return FText::AsNumber(FMath::RoundToInt(Fraction * 100.f));
	}
}

// --- Skylight ---

ARobberSkylight::ARobberSkylight()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberSkylight", "Label", "SKYLIGHT");
	Prompt = NSLOCTEXT("RobberSkylight", "Prompt", "Cut the glass");

	// The glass is scaled to PaneSize, so the anchor and rope hang off an unscaled root instead.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh->SetupAttachment(Root);
	LabelText->SetupAttachment(Root);
	LabelText->SetRelativeRotation(FRotator(0, -90, 0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	RopeAnchor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RopeAnchor"));
	RopeAnchor->SetupAttachment(Root);
	Rope = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rope"));
	Rope->SetupAttachment(Root);
	if (Cube.Succeeded())
	{
		RopeAnchor->SetStaticMesh(Cube.Object);
	}
	if (Cylinder.Succeeded())
	{
		Rope->SetStaticMesh(Cylinder.Object);
	}
	Rope->SetHiddenInGame(true);
	Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARobberSkylight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberSkylight, bCut);
	DOREPLIFETIME(ARobberSkylight, bRopeDown);
	DOREPLIFETIME(ARobberSkylight, CutProgress);
	DOREPLIFETIME(ARobberSkylight, Noise);
	DOREPLIFETIME(ARobberSkylight, bTooLoud);
	DOREPLIFETIME(ARobberSkylight, Cutters);
	DOREPLIFETIME(ARobberSkylight, Alerts);
}

void ARobberSkylight::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const double EndY = -PaneSize.Y / 2;
	Mesh->SetRelativeScale3D(FVector(PaneSize.X / 100., PaneSize.Y / 100., 0.5));
	// Glass top is flush with the roof; the anchor sits on the roof just past the end of the hole.
	RopeAnchor->SetRelativeLocation(FVector(0, EndY - 60, 50));
	RopeAnchor->SetRelativeScale3D(FVector(1.0, 0.8, 0.5));
	Rope->SetRelativeLocation(FVector(0, EndY + 60, -RopeLength / 2));
	Rope->SetRelativeScale3D(FVector(0.1, 0.1, RopeLength / 100.));
	LabelText->SetRelativeLocation(FVector(0, EndY - 60, 200));
}

FVector ARobberSkylight::GetClimbDestination() const
{
	return GetActorTransform().TransformPosition(FVector(0, -PaneSize.Y / 2 + 200, -RopeLength + 100));
}

void ARobberSkylight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (HasAuthority() && IsNightRunning(GameState) && IsActive(GameState))
	{
		ServerTick(DeltaSeconds, GameState);
	}
}

void ARobberSkylight::SendBackUpJumpers()
{
	// The column under the hole, from just below the glass to the floor.
	const FBox Glass = Mesh->Bounds.GetBox();
	const FBox Column(FVector(Glass.Min.X - 50, Glass.Min.Y - 50, Glass.Min.Z - RopeLength - 200), FVector(Glass.Max.X + 50, Glass.Max.Y + 50, Glass.Min.Z - 150));
	const FVector Roof = GetActorTransform().TransformPosition(FVector(0, -PaneSize.Y / 2 - 200, 150));
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (Pawn && Column.IsInside(Pawn->GetActorLocation()))
		{
			Pawn->TeleportTo(Roof, Pawn->GetActorRotation());
			TellPlayer(Pawn, NSLOCTEXT("RobberSkylight", "TooFar", "Too far to jump! Drop the rope down first."));
		}
	}
}

void ARobberSkylight::ServerTick(float DeltaSeconds, ARobberGameState* GameState)
{
	if (bCut)
	{
		if (!bRopeDown)
		{
			SendBackUpJumpers();
		}
		return;
	}
	const FBox Reach = Mesh->Bounds.GetBox().ExpandBy(CutReach);
	Cutters.RemoveAll([&Reach](const APlayerState* Player)
	{
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		return !Pawn || !Reach.IsInside(Pawn->GetActorLocation());
	});

	const int32 CutterCount = Cutters.Num();
	if (CutterCount > 0)
	{
		CutProgress = FMath::Min(1.f, CutProgress + DeltaSeconds * CutterCount / CutSeconds);
		Noise += DeltaSeconds * CutterCount * NoisePerCutterPerSecond;
	}
	else
	{
		Noise = FMath::Max(0.f, Noise - DeltaSeconds * QuietPerSecond);
	}

	if (CutProgress >= 1.f)
	{
		bCut = true;
		Cutters.Reset();
		OnRep_Cut();
		GameState->TellCrew(NSLOCTEXT("RobberSkylight", "Cut", "The glass is out. Drop the rope down from the anchor!"));
	}
	else if (Noise >= 1.f)
	{
		Noise = 1.f;
		bTooLoud = true;
		Cutters.Reset();
		++Alerts;
		// Guards don't react to it yet; for now it's a warning and a forced pause.
		UE_LOG(LogTemp, Warning, TEXT("Skylight: a guard heard the cutting (%d time(s))"), Alerts);
		GameState->TellCrew(NSLOCTEXT("RobberSkylight", "Heard", "A guard heard the glass! Stop and wait for it to go quiet."));
	}
	else if (bTooLoud && Noise < ResumeBelowNoise)
	{
		bTooLoud = false;
		GameState->TellCrew(NSLOCTEXT("RobberSkylight", "Quiet", "It's gone quiet. Keep cutting."));
	}
}

void ARobberSkylight::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!IsNightRunning(GameState))
	{
		return;
	}
	if (bCut && !bRopeDown)
	{
		bRopeDown = true;
		OnRep_Cut();
		ForceNetUpdate();
		GameState->TellCrew(FText::Format(NSLOCTEXT("RobberSkylight", "RopeDropped", "{0} dropped the rope. Climb down!"), FText::FromString(Player->GetPlayerName())));
		return;
	}
	if (bCut)
	{
		Pawn->TeleportTo(GetClimbDestination(), Pawn->GetActorRotation());
		return;
	}
	if (Cutters.Contains(Player))
	{
		Cutters.Remove(Player);
		return;
	}
	if (bTooLoud)
	{
		TellPlayer(Pawn, NSLOCTEXT("RobberSkylight", "WaitQuiet", "Too loud. Wait for it to go quiet."));
		return;
	}
	Cutters.Add(Player);
}

FText ARobberSkylight::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (!IsNightRunning(GameState))
	{
		return FText::GetEmpty();
	}
	if (bCut)
	{
		return bRopeDown ? NSLOCTEXT("RobberSkylight", "Climb", "Climb down the rope") : NSLOCTEXT("RobberSkylight", "DropRope", "Drop the rope down");
	}
	if (IsCutting(Viewer))
	{
		return NSLOCTEXT("RobberSkylight", "Stop", "Stop cutting");
	}
	return bTooLoud ? NSLOCTEXT("RobberSkylight", "TooLoudPrompt", "Too loud, wait") : Prompt;
}

FText ARobberSkylight::GetLabel(const ARobberGameState* GameState) const
{
	if (bCut || !IsNightRunning(GameState))
	{
		if (!bCut)
		{
			return Label;
		}
		return bRopeDown ? NSLOCTEXT("RobberSkylight", "RopeLabel", "ROPE") : NSLOCTEXT("RobberSkylight", "RopeCoiled", "ROPE\nDrop it down");
	}
	return FText::Format(NSLOCTEXT("RobberSkylight", "Progress", "{0}\nCut {1}%   Noise {2}%{3}"), Label, Percent(CutProgress), Percent(Noise),
		bTooLoud ? NSLOCTEXT("RobberSkylight", "TooLoudLabel", "\nTOO LOUD - WAIT") : FText::GetEmpty());
}

void ARobberSkylight::OnRep_Cut()
{
	SetShown(Mesh, !bCut);
	Rope->SetHiddenInGame(!bRopeDown);
}

// --- Valve ---

ARobberSewerValve::ARobberSewerValve()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberSewer", "ValveLabel", "VALVE");
	Prompt = NSLOCTEXT("RobberSewer", "ValvePrompt", "Open the valve");
}

void ARobberSewerValve::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberSewerValve, OpenUntil);
	DOREPLIFETIME(ARobberSewerValve, bJammed);
}

bool ARobberSewerValve::IsOpen(const ARobberGameState* GameState) const
{
	return bJammed || (GameState && OpenUntil > GameState->GetServerWorldTimeSeconds());
}

void ARobberSewerValve::Jam()
{
	if (HasAuthority() && !bJammed)
	{
		bJammed = true;
		ForceNetUpdate();
	}
}

void ARobberSewerValve::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!IsNightRunning(GameState) || IsOpen(GameState))
	{
		return;
	}
	// Nobody to open the other one at the same time.
	if (GameState->PlayerArray.Num() <= 1)
	{
		Jam();
		TellPlayer(Pawn, NSLOCTEXT("RobberSewer", "Jammed", "The valve jams open."));
		return;
	}
	OpenUntil = GameState->GetServerWorldTimeSeconds() + OpenSeconds;
	ForceNetUpdate();
	TellPlayer(Pawn, FText::Format(NSLOCTEXT("RobberSewer", "Opened", "It springs shut in {0} seconds. Someone has to open the other valve at the same time!"), FMath::RoundToInt(OpenSeconds)));
}

FText ARobberSewerValve::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return IsNightRunning(GameState) && !IsOpen(GameState) ? Prompt : FText::GetEmpty();
}

FText ARobberSewerValve::GetLabel(const ARobberGameState* GameState) const
{
	if (bJammed)
	{
		return FText::Format(NSLOCTEXT("RobberSewer", "ValveJammed", "{0}\nOPEN"), Label);
	}
	if (IsOpen(GameState))
	{
		const int32 SecondsLeft = FMath::CeilToInt(OpenUntil - GameState->GetServerWorldTimeSeconds());
		return FText::Format(NSLOCTEXT("RobberSewer", "ValveOpen", "{0}\nOPEN {1}s"), Label, SecondsLeft);
	}
	return FText::Format(NSLOCTEXT("RobberSewer", "ValveShut", "{0}\nSHUT"), Label);
}

// --- Water ---

ARobberSewerWater::ARobberSewerWater()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Root);
	LabelText->SetRelativeRotation(FRotator(0, -90, 0));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextCenter);
	LabelText->SetWorldSize(26.f);
	LabelText->SetTextRenderColor(FColor::White);
	LabelText->SetText(NSLOCTEXT("RobberSewer", "WaterLabel", "TOO DEEP\nOpen both valves at the same time to drain it"));
}

void ARobberSewerWater::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberSewerWater, bDrained);
}

void ARobberSewerWater::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Mesh->SetRelativeScale3D(Size / 100.);
	// On the face the crew walks up to (-Y).
	LabelText->SetRelativeLocation(FVector(0, -Size.Y / 2 - 5, 0));
}

void ARobberSewerWater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!HasAuthority() || bDrained || Valves.IsEmpty() || !IsNightRunning(GameState))
	{
		return;
	}
	if (Algo::AllOf(Valves, [GameState](const ARobberSewerValve* Valve) { return Valve && Valve->IsOpen(GameState); }))
	{
		bDrained = true;
		OnRep_Drained();
		ForceNetUpdate();
		for (ARobberSewerValve* Valve : Valves)
		{
			Valve->Jam();
		}
		GameState->TellCrew(NSLOCTEXT("RobberSewer", "Drained", "The water drains away. The tunnel to the grate is clear."));
	}
}

void ARobberSewerWater::OnRep_Drained()
{
	SetShown(Mesh, !bDrained);
	LabelText->SetHiddenInGame(bDrained);
}

// --- Crowbar ---

ARobberCrowbar::ARobberCrowbar()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberSewer", "CrowbarLabel", "CROWBAR");
	Prompt = NSLOCTEXT("RobberSewer", "CrowbarPrompt", "Pick up the crowbar");
}

void ARobberCrowbar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberCrowbar, bTaken);
}

void ARobberCrowbar::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	UPlayerQuestComponent* Quests = FindPlayerQuests(Player);
	if (bTaken || !Quests || !IsNightRunning(GameState))
	{
		return;
	}
	Quests->GiveKey(KeyId);
	bTaken = true;
	OnRep_Taken();
	ForceNetUpdate();
	GameState->TellCrew(FText::Format(NSLOCTEXT("RobberSewer", "FoundCrowbar", "{0} found a crowbar."), FText::FromString(Player->GetPlayerName())));
}

FText ARobberCrowbar::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return bTaken || !IsNightRunning(GameState) ? FText::GetEmpty() : Prompt;
}

FText ARobberCrowbar::GetLabel(const ARobberGameState* GameState) const
{
	return bTaken ? FText::GetEmpty() : Label;
}

void ARobberCrowbar::OnRep_Taken()
{
	SetShown(Mesh, !bTaken);
	LabelText->SetHiddenInGame(bTaken);
}

// --- Grate ---

ARobberSewerGrate::ARobberSewerGrate()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberSewer", "GrateLabel", "GRATE");
	// Hangs under the grate, facing the way the crew comes in (-Y). Offsets are scaled by the flat grate.
	LabelText->SetRelativeLocation(FVector(0, 0, -300));
	LabelText->SetRelativeRotation(FRotator(0, -90, 0));
	LabelText->SetVerticalAlignment(EVRTA_TextTop);
}

void ARobberSewerGrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberSewerGrate, bOpen);
}

void ARobberSewerGrate::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!IsNightRunning(GameState))
	{
		return;
	}
	if (bOpen)
	{
		// Yaw only: the open grate is tipped over.
		Pawn->TeleportTo(GetActorLocation() + FRotator(0, GetActorRotation().Yaw, 0).RotateVector(ClimbOffset), Pawn->GetActorRotation());
		return;
	}
	const UPlayerQuestComponent* Quests = FindPlayerQuests(Player);
	if (!Quests || !Quests->HasKey(KeyId))
	{
		TellPlayer(Pawn, NSLOCTEXT("RobberSewer", "Rusted", "It's rusted shut. You need something to pry it with."));
		return;
	}
	bOpen = true;
	OnRep_Open();
	ForceNetUpdate();
	GameState->TellCrew(FText::Format(NSLOCTEXT("RobberSewer", "Pried", "{0} pried the grate open."), FText::FromString(Player->GetPlayerName())));
}

FText ARobberSewerGrate::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (!IsNightRunning(GameState))
	{
		return FText::GetEmpty();
	}
	if (bOpen)
	{
		return NSLOCTEXT("RobberSewer", "ClimbUp", "Climb up into the museum");
	}
	const UPlayerQuestComponent* Quests = FindPlayerQuests(Viewer);
	return Quests && Quests->HasKey(KeyId)
		? NSLOCTEXT("RobberSewer", "Pry", "Pry the grate open")
		: NSLOCTEXT("RobberSewer", "NeedCrowbar", "Rusted shut (needs a crowbar)");
}

FText ARobberSewerGrate::GetLabel(const ARobberGameState* GameState) const
{
	return bOpen ? NSLOCTEXT("RobberSewer", "GrateOpen", "GRATE (open)\nClimb up") : Label;
}

void ARobberSewerGrate::OnRep_Open()
{
	// No model yet: a pried grate just swings down out of the way, still there to press for the climb.
	Mesh->SetRelativeRotation(FRotator(0, 0, 80));
}

// --- Ladder ---

ARobberLadder::ARobberLadder()
{
	Label = NSLOCTEXT("RobberLadder", "Label", "LADDER");
	Prompt = NSLOCTEXT("RobberLadder", "Prompt", "Climb the ladder");
	// On the face of a thin ladder rather than on top of a tall one (offsets are scaled by the mesh).
	LabelText->SetRelativeLocation(FVector(60, 0, 0));
}

FVector ARobberLadder::GetClimbDestination() const
{
	return GetActorLocation() + FRotator(0, GetActorRotation().Yaw, 0).RotateVector(ClimbOffset);
}

void ARobberLadder::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (IsNightRunning(GameState))
	{
		Pawn->TeleportTo(GetClimbDestination(), Pawn->GetActorRotation());
	}
}

FText ARobberLadder::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return IsNightRunning(GameState) ? Prompt : FText::GetEmpty();
}

// --- Roll-up door ---

ARobberRollUpDoor::ARobberRollUpDoor()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberRollUpDoor", "Label", "ROLL-UP DOOR");
	Prompt = NSLOCTEXT("RobberRollUpDoor", "Prompt", "Lift the door (stay close to hold it up)");

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh->SetupAttachment(Root);
	LabelText->SetupAttachment(Root);
}

void ARobberRollUpDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberRollUpDoor, Openness);
	DOREPLIFETIME(ARobberRollUpDoor, Noise);
	DOREPLIFETIME(ARobberRollUpDoor, bTooLoud);
	DOREPLIFETIME(ARobberRollUpDoor, Holder);
	DOREPLIFETIME(ARobberRollUpDoor, Alerts);
}

void ARobberRollUpDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	OnRep_Openness();
	// Over the outside (-Y) face, below the lintel.
	LabelText->SetRelativeLocation(FVector(0, -40, DoorSize.Y - 120));
}

FBox ARobberRollUpDoor::GetDoorwayBox() const
{
	return FBox(FVector(-DoorSize.X / 2, -10, 0), FVector(DoorSize.X / 2, 10, DoorSize.Y)).TransformBy(GetActorTransform());
}

void ARobberRollUpDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (HasAuthority() && IsNightRunning(GameState) && IsActive(GameState))
	{
		ServerTick(DeltaSeconds, GameState);
	}
}

void ARobberRollUpDoor::Release()
{
	Holder = nullptr;
	ForceNetUpdate();
}

void ARobberRollUpDoor::ServerTick(float DeltaSeconds, ARobberGameState* GameState)
{
	const APawn* HolderPawn = Holder ? Holder->GetPawn() : nullptr;
	if (Holder && (!HolderPawn || !GetDoorwayBox().ExpandBy(HoldReach).IsInside(HolderPawn->GetActorLocation())))
	{
		Release();
	}

	const float Target = Holder ? 1.f : 0.f;
	const float NewOpenness = FMath::FInterpConstantTo(Openness, Target, DeltaSeconds, 1.f / LiftSeconds);
	if (NewOpenness != Openness)
	{
		Openness = NewOpenness;
		OnRep_Openness();
	}

	if (Openness > 0.f)
	{
		Noise = FMath::Min(1.f, Noise + DeltaSeconds * NoisePerSecond);
	}
	else
	{
		Noise = FMath::Max(0.f, Noise - DeltaSeconds * QuietPerSecond);
	}

	if (Noise >= 1.f && !bTooLoud)
	{
		bTooLoud = true;
		++Alerts;
		Release();
		// Guards don't react to it yet; for now it's a warning and a forced pause.
		UE_LOG(LogTemp, Warning, TEXT("Roll-up door: a guard heard it (%d time(s))"), Alerts);
		GameState->TellCrew(NSLOCTEXT("RobberRollUpDoor", "Heard", "A guard heard the door rattling! Let it down and wait for it to go quiet."));
	}
	else if (bTooLoud && Openness <= 0.f && Noise < ResumeBelowNoise)
	{
		bTooLoud = false;
		GameState->TellCrew(NSLOCTEXT("RobberRollUpDoor", "Quiet", "It's gone quiet. Try the door again."));
	}
}

void ARobberRollUpDoor::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!IsNightRunning(GameState))
	{
		return;
	}
	if (Holder == Player)
	{
		Release();
		return;
	}
	if (bTooLoud)
	{
		TellPlayer(Pawn, NSLOCTEXT("RobberRollUpDoor", "WaitQuiet", "Too loud. Wait for it to go quiet."));
		return;
	}
	if (Holder)
	{
		TellPlayer(Pawn, FText::Format(NSLOCTEXT("RobberRollUpDoor", "AlreadyHeld", "{0} is already holding it up."), FText::FromString(Holder->GetPlayerName())));
		return;
	}
	Holder = Player;
	ForceNetUpdate();
}

FText ARobberRollUpDoor::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (!IsNightRunning(GameState))
	{
		return FText::GetEmpty();
	}
	if (IsHolding(Viewer))
	{
		return NSLOCTEXT("RobberRollUpDoor", "LetGo", "Let the door down");
	}
	if (bTooLoud)
	{
		return NSLOCTEXT("RobberRollUpDoor", "TooLoudPrompt", "Too loud, wait");
	}
	return Holder ? FText::GetEmpty() : Prompt;
}

FText ARobberRollUpDoor::GetLabel(const ARobberGameState* GameState) const
{
	if (!IsNightRunning(GameState))
	{
		return Label;
	}
	return FText::Format(NSLOCTEXT("RobberRollUpDoor", "Progress", "{0}\nUp {1}%   Noise {2}%{3}"), Label, Percent(Openness), Percent(Noise),
		bTooLoud ? NSLOCTEXT("RobberRollUpDoor", "TooLoudLabel", "\nTOO LOUD - WAIT") : FText::GetEmpty());
}

void ARobberRollUpDoor::OnRep_Openness()
{
	// Rolls up into the lintel: the bottom edge rises, a strip always stays at the top.
	const double Height = FMath::Max(20., DoorSize.Y * (1. - Openness));
	Mesh->SetRelativeLocation(FVector(0, 0, DoorSize.Y - Height / 2));
	Mesh->SetRelativeScale3D(FVector(DoorSize.X / 100., 0.2, Height / 100.));
}
