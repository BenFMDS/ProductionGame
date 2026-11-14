#include "Heist/RobberTriggerZones.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
	TAutoConsoleVariable<bool> CVarShowTriggers(TEXT("Robber.ShowTriggers"), false,
		TEXT("Draw the triggers' coloured shapes in game (they always show in the editor)."), ECVF_Default);
}

bool ARobberTriggerZone::AreShapesShownInGame()
{
	return CVarShowTriggers.GetValueOnGameThread();
}

void ARobberTriggerZone::SetShapesShownInGame(bool bShow)
{
	CVarShowTriggers->Set(bShow, ECVF_SetByConsole);
	// Straight away rather than on each trigger's next tick.
	for (TObjectIterator<ARobberTriggerZone> It; It; ++It)
	{
		if (It->DoorMesh && !It->IsTemplate())
		{
			It->DoorMesh->SetHiddenInGame(!bShow);
		}
	}
}

bool ARobberTriggerZone::IsShapeShownInGame() const
{
	return DoorMesh->IsVisible() && !DoorMesh->bHiddenInGame;
}

ARobberTriggerZone::ARobberTriggerZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(Size);
	Box->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Box;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shape(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	ShapeMaterial = Shape.Object;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Box);
	DoorMesh->SetStaticMesh(CubeMesh);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Box);
	LabelText->SetRelativeRotation(FRotator(0, 180, 0));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextBottom);

	ApplyLook();
}

void ARobberTriggerZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void ARobberTriggerZone::BeginPlay()
{
	Super::BeginPlay();
	// Placed actors don't rerun construction in a cooked game; make sure the look matches the settings.
	ApplyLook();
}

ERobberTriggerLook ARobberTriggerZone::GetResolvedLook() const
{
	if (Look != ERobberTriggerLook::Auto)
	{
		return Look;
	}
	switch (Kind)
	{
	case ERobberTriggerKind::Message:    return ERobberTriggerLook::Hidden;
	case ERobberTriggerKind::EscapeExit: return ERobberTriggerLook::FloorPad;
	case ERobberTriggerKind::Manhole:    return ERobberTriggerLook::ManholeLid;
	default:                             return ERobberTriggerLook::Door;
	}
}

void ARobberTriggerZone::ApplyLook()
{
	Box->SetBoxExtent(Size);
	const ERobberTriggerLook Resolved = GetResolvedLook();
	// The basic shapes are 100 units across, centred on their origin.
	switch (Resolved)
	{
	case ERobberTriggerLook::FloorPad:
		DoorMesh->SetStaticMesh(CubeMesh);
		DoorMesh->SetRelativeLocation(FVector(0, 0, -Size.Z + 3.));
		DoorMesh->SetRelativeScale3D(FVector(Size.X / 50., Size.Y / 50., 0.05));
		break;
	case ERobberTriggerLook::ManholeLid:
	{
		DoorMesh->SetStaticMesh(CylinderMesh);
		DoorMesh->SetRelativeLocation(FVector(0, 0, -Size.Z + 3.));
		const double Across = FMath::Min(Size.X, Size.Y) * 0.8 / 50.;
		DoorMesh->SetRelativeScale3D(FVector(Across, Across, 0.05));
		break;
	}
	default:
		DoorMesh->SetStaticMesh(CubeMesh);
		DoorMesh->SetRelativeLocation(FVector::ZeroVector);
		DoorMesh->SetRelativeScale3D(Size / 50.);
		break;
	}
	DoorMesh->SetVisibility(Resolved != ERobberTriggerLook::Hidden);
	// Hidden-in-game is ignored by editor viewports, so level designers still see every shape.
	DoorMesh->SetHiddenInGame(!AreShapesShownInGame());

	// No material and no colour: leave whatever the mesh already has (old maps painted their pads directly).
	if (Material || Colour.A > 0.f)
	{
		UMaterialInterface* Base = Material ? Material.Get() : ShapeMaterial.Get();
		if (Colour.A > 0.f && Base)
		{
			UMaterialInstanceDynamic* Tinted = Cast<UMaterialInstanceDynamic>(DoorMesh->GetMaterial(0));
			if (!Tinted || Tinted->Parent != Base)
			{
				Tinted = UMaterialInstanceDynamic::Create(Base, this);
			}
			Tinted->SetVectorParameterValue(TEXT("Color"), Colour);
			DoorMesh->SetMaterial(0, Tinted);
		}
		else
		{
			DoorMesh->SetMaterial(0, Base);
		}
	}

	LabelText->SetRelativeLocation(FVector(-(Size.X + 10.), 0, (Resolved == ERobberTriggerLook::Door ? Size.Z : 0.) + 20.));
	LabelText->SetWorldSize(LabelSize);
	LabelText->SetTextRenderColor(Colour.A > 0.f ? Colour.ToFColor(true) : FColor::White);
	LabelText->SetText(Label);
	LabelText->SetVisibility(bShowLabel && Resolved != ERobberTriggerLook::Hidden);
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

bool ARobberTriggerZone::IsActive(const ARobberGameState* GameState) const
{
	return Kind != ERobberTriggerKind::EscapeExit || !OwningEntry || (GameState && GameState->GetActiveEntry() == OwningEntry);
}

TArray<ARobberTriggerZone*> ARobberTriggerZone::FindAll(const UWorld* World, ERobberTriggerKind Kind)
{
	TArray<ARobberTriggerZone*> Found;
	if (!World)
	{
		return Found;
	}
	for (TActorIterator<ARobberTriggerZone> It(World); It; ++It)
	{
		if (It->Kind == Kind || (Kind == ERobberTriggerKind::LobbyExit && It->IsLobbyExit()))
		{
			Found.Add(*It);
		}
	}
	return Found;
}

ARobberTriggerZone* ARobberTriggerZone::Find(const UWorld* World, ERobberTriggerKind Kind)
{
	TArray<ARobberTriggerZone*> Found = FindAll(World, Kind);
	return Found.Num() ? Found[0] : nullptr;
}

ARobberTriggerZone* ARobberTriggerZone::FindLobbyExit(const UWorld* World, FName ExitId)
{
	for (ARobberTriggerZone* Exit : FindAll(World, ERobberTriggerKind::LobbyExit))
	{
		if (Exit->ExitId == ExitId)
		{
			return Exit;
		}
	}
	return nullptr;
}

void ARobberTriggerZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (DoorMesh->bHiddenInGame == AreShapesShownInGame())
	{
		DoorMesh->SetHiddenInGame(!AreShapesShownInGame());
	}
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!GameState)
	{
		return;
	}
	int32 Inside = 0, Total = 0;
	CountPlayers(Inside, Total);
	if (HasAuthority())
	{
		NoticeEntries();
		ServerUpdate(GameState, Inside, Total);
	}
	const bool bActive = IsActive(GameState);
	const ERobberTriggerLook Resolved = GetResolvedLook();
	DoorMesh->SetVisibility(bActive && Resolved != ERobberTriggerLook::Hidden);
	LabelText->SetVisibility(bActive && bShowLabel && Resolved != ERobberTriggerLook::Hidden);
	LabelText->SetText(GetStatusLabel(GameState, Inside, Total));
}

void ARobberTriggerZone::NoticeEntries()
{
	TSet<TWeakObjectPtr<APawn>> Now;
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping, APawn::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || !Pawn->GetPlayerState())
		{
			continue;
		}
		Now.Add(Pawn);
		if (PawnsInside.Contains(Pawn))
		{
			continue;
		}
		ReceivePlayerEntered(Pawn);
		if (Kind == ERobberTriggerKind::Message && !Message.IsEmpty() && (!bOncePerPlayer || !PawnsTold.Contains(Pawn)))
		{
			PawnsTold.Add(Pawn);
			ARobberController::Tell(Pawn, Message);
		}
	}
	PawnsInside = MoveTemp(Now);
}

void ARobberTriggerZone::ServerUpdate(ARobberGameState* GameState, int32 Inside, int32 Total)
{
	switch (Kind)
	{
	case ERobberTriggerKind::GemRoom:
		if (GameState->GetGameStage() == RobberStage::Investigate && Inside > 0)
		{
			GameState->StartHeist();
		}
		break;
	case ERobberTriggerKind::EscapeExit:
		if (GameState->GetGameStage() == RobberStage::Escape && IsActive(GameState) && Total > 0 && Inside == Total)
		{
			GameState->FinishNight(true);
		}
		break;
	case ERobberTriggerKind::LobbyExit:
	case ERobberTriggerKind::Manhole:
	{
		const ARobberEntryPoint* Winner = GameState->GetWinningEntry();
		if (RobberStage::IsInLobby(GameState->GetGameStage()) && Winner && Winner->LobbyExit == ExitId && Total > 0 && Inside == Total)
		{
			GameState->StartBreakIn();
		}
		break;
	}
	default:
		break;
	}
}

FText ARobberTriggerZone::GetStatusLabel(const ARobberGameState* GameState, int32 Inside, int32 Total) const
{
	switch (Kind)
	{
	case ERobberTriggerKind::EscapeExit:
		if (!IsActive(GameState))
		{
			return FText::GetEmpty();
		}
		if (GameState->GetGameStage() == RobberStage::Escape)
		{
			return FText::Format(NSLOCTEXT("RobberZone", "EscapeCount", "{0}\nEveryone out this way! ({1}/{2})"), Label, Inside, Total);
		}
		return Label;
	case ERobberTriggerKind::LobbyExit:
	case ERobberTriggerKind::Manhole:
		if (!RobberStage::IsInLobby(GameState->GetGameStage()))
		{
			return Label;
		}
		if (const ARobberEntryPoint* Winner = GameState->GetWinningEntry())
		{
			if (Winner->LobbyExit != ExitId)
			{
				return FText::Format(NSLOCTEXT("RobberZone", "DoorOtherExit", "{0}\nNot this way for: {1}"), Label, Winner->DisplayName);
			}
			return FText::Format(NSLOCTEXT("RobberZone", "DoorReady", "{0}\nGoing in by: {1}\nEveryone walk in ({2}/{3})"), Label, Winner->DisplayName, Inside, Total);
		}
		return FText::Format(NSLOCTEXT("RobberZone", "DoorVote", "{0}\nVote for a way in first"), Label);
	default:
		return Label;
	}
}

ARobberLobbyExit::ARobberLobbyExit()
{
	Kind = ERobberTriggerKind::LobbyExit;
	Label = NSLOCTEXT("RobberZone", "MuseumDoor", "MUSEUM DOOR");
	ApplyLook();
}

ARobberManhole::ARobberManhole()
{
	Kind = ERobberTriggerKind::Manhole;
	ExitId = TEXT("Manhole");
	Label = NSLOCTEXT("RobberZone", "Manhole", "MANHOLE");
	Size = FVector(150, 150, 100);
	ApplyLook();
}

ARobberEscapeExit::ARobberEscapeExit()
{
	Kind = ERobberTriggerKind::EscapeExit;
	Label = NSLOCTEXT("RobberZone", "WayOut", "WAY OUT");
	ApplyLook();
}

ARobberGemRoomApproach::ARobberGemRoomApproach()
{
	Kind = ERobberTriggerKind::GemRoom;
	Label = NSLOCTEXT("RobberZone", "GemRoomDoor", "GEM ROOM");
	ApplyLook();
}
