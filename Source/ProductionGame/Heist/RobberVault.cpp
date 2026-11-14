#include "Heist/RobberVault.h"
#include "Robber/RobberAchievementTracker.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dialogue/DialogueComponent.h"
#include "Dialogue/DialogueDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberStage.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

const FName ARobberVaultDoor::JobId(TEXT("Vault"));

namespace RobberVault
{
	bool InHeist(const ARobberGameState* GameState)
	{
		return GameState && (GameState->GetGameStage() == RobberStage::Heist || GameState->GetGameStage() == RobberStage::Escape);
	}

	UStaticMeshComponent* MakeMesh(AActor* Owner, const TCHAR* Name, UStaticMesh* Shape, USceneComponent* Parent)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Shape);
		return Mesh;
	}
}

// --- Door ---

ARobberVaultDoor::ARobberVaultDoor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	RootComponent = DoorRoot;
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(DoorRoot);
	Slab = RobberVault::MakeMesh(this, TEXT("Slab"), Cube.Object, Hinge);
	Wheel = RobberVault::MakeMesh(this, TEXT("Wheel"), Cylinder.Object, Hinge);
	// The wheel is decoration: traces and the crew go by the slab.
	Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Sign"));
	Sign->SetupAttachment(Hinge);
	Sign->SetHorizontalAlignment(EHTA_Center);
	Sign->SetVerticalAlignment(EVRTA_TextCenter);
	Sign->SetWorldSize(34.f);
	Sign->SetTextRenderColor(FColor(0xf2, 0xc1, 0x4e));
	Sign->SetText(NSLOCTEXT("RobberVault", "SignShut", "VAULT\nCODE  ? ? ? ?"));
}

void ARobberVaultDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Hinge->SetRelativeLocation(FVector(0., -Width / 2., 0.));
	Slab->SetRelativeLocation(FVector(0., Width / 2., Height / 2.));
	Slab->SetRelativeScale3D(FVector(Thickness / 100., Width / 100., Height / 100.));
	const double WheelSize = FMath::Min(Width, Height) * 0.6;
	Wheel->SetRelativeLocation(FVector(Thickness / 2. + 4., Width / 2., Height * 0.6));
	Wheel->SetRelativeRotation(FRotator(90., 0., 0.));
	Wheel->SetRelativeScale3D(FVector(WheelSize / 100., WheelSize / 100., 0.08));
	Sign->SetRelativeLocation(FVector(Thickness / 2. + 10., Width / 2., Height - 45.));
}

FTransform ARobberVaultDoor::GetDialTransform(int32 Index) const
{
	const FVector Local(Thickness / 2. + 6., Width / 2. + (Index - (CodeLength - 1) / 2.) * 50., 135.);
	return FTransform(FRotator::ZeroRotator, Local) * Hinge->GetComponentTransform();
}

void ARobberVaultDoor::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	// All zeros is where the dials start, so it would open at the first turn.
	do
	{
		Code.Reset();
		for (int32 Each = 0; Each < CodeLength; ++Each)
		{
			Code.Add(FMath::RandRange(0, 9));
		}
	} while (!Code.ContainsByPredicate([](int32 Digit) { return Digit != 0; }));
	UpdateSign();
}

void ARobberVaultDoor::CheckDials()
{
	if (!HasAuthority() || bOpen || Code.Num() != CodeLength)
	{
		return;
	}
	int32 Matching = 0;
	for (TActorIterator<ARobberVaultDial> It(GetWorld()); It; ++It)
	{
		Matching += It->Door == this && Code.IsValidIndex(It->Index) && It->GetDigit() == Code[It->Index];
	}
	if (Matching < CodeLength)
	{
		return;
	}
	bOpen = true;
	ForceNetUpdate();
	OnRep_Open();
	UpdateSign();
	if (ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>())
	{
		GameState->CompleteJob(JobId);
		GameState->TellCrew(NSLOCTEXT("RobberVault", "Opened", "Clunk. The vault's open!"));
		RobberAchievements::AwardCrew(GetWorld(), TEXT("CodeCracked"));
	}
}

void ARobberVaultDoor::OnRep_Open()
{
	OpenStartTime = GetWorld()->GetTimeSeconds();
	UpdateSign();
}

void ARobberVaultDoor::UpdateSign()
{
	// The code itself is only in the journals of whoever the exhibits told.
	Sign->SetText(bOpen ? NSLOCTEXT("RobberVault", "SignOpen", "VAULT\nOPEN") : NSLOCTEXT("RobberVault", "SignShut", "VAULT\nCODE  ? ? ? ?"));
}

void ARobberVaultDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (OpenStartTime < 0.)
	{
		return;
	}
	const float Alpha = FMath::Clamp(float((GetWorld()->GetTimeSeconds() - OpenStartTime) / OpenSeconds), 0.f, 1.f);
	// Negative yaw swings the free edge out of the vault (+X).
	Hinge->SetRelativeRotation(FRotator(0., -100. * FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f), 0.));
	if (Alpha >= 1.f)
	{
		OpenStartTime = -1.;
	}
}

void ARobberVaultDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberVaultDoor, bOpen);
}

// --- Dial ---

ARobberVaultDial::ARobberVaultDial()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	DialRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DialRoot"));
	RootComponent = DialRoot;
	Mesh->SetupAttachment(DialRoot);
	Mesh->SetMobility(EComponentMobility::Movable);
	// The cylinder and the dial's dressing are in BP_VaultDial. Face out along +X: 32 cm across, sticking 8 cm out of the door.
	Mesh->SetRelativeRotation(FRotator(90., 0., 0.));
	Mesh->SetRelativeScale3D(FVector(0.32, 0.32, 0.08));
	LabelText->SetupAttachment(DialRoot);
	LabelText->SetRelativeLocation(FVector(6., 0., 22.));
	LabelText->SetWorldSize(26.f);
	LabelText->SetTextRenderColor(FColor(0xf2, 0xc1, 0x4e));
}

bool ARobberVaultDial::CanTurn(const ARobberGameState* GameState) const
{
	return Door && !Door->IsOpen() && RobberVault::InHeist(GameState);
}

void ARobberVaultDial::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!CanTurn(GameState))
	{
		return;
	}
	Digit = (Digit + 1) % 10;
	ForceNetUpdate();
	Door->CheckDials();
}

FText ARobberVaultDial::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return CanTurn(GameState) ? FText::Format(NSLOCTEXT("RobberVault", "DialPrompt", "Turn dial {0}"), Index + 1) : FText::GetEmpty();
}

FText ARobberVaultDial::GetLabel(const ARobberGameState* GameState) const
{
	return FText::AsNumber(Digit);
}

void ARobberVaultDial::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberVaultDial, Digit);
}

// --- Laser ---

ARobberLaserBeam::ARobberLaserBeam()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	RootComponent = Beam;
	Beam->SetStaticMesh(Cube.Object);
	Beam->SetMobility(EComponentMobility::Movable);
	Beam->SetCastShadow(false);
	// Only feels pawns; interact traces and thrown things go straight through.
	Beam->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Beam->SetCollisionResponseToAllChannels(ECR_Ignore);
	Beam->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Beam->SetGenerateOverlapEvents(true);
}

void ARobberLaserBeam::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Beam->OnComponentBeginOverlap.AddDynamic(this, &ARobberLaserBeam::OnBeamOverlap);
	}
}

void ARobberLaserBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	const bool bShouldBeLive = !GameState || !GameState->IsPowerCut();
	if (bShouldBeLive != bLive)
	{
		bLive = bShouldBeLive;
		SetActorHiddenInGame(!bLive);
		SetActorEnableCollision(bLive);
	}
	// Standing in a beam when the heist starts counts too, not only walking into one.
	if (bLive && HasAuthority() && RobberVault::InHeist(GameState))
	{
		TArray<AActor*> Inside;
		Beam->GetOverlappingActors(Inside, APawn::StaticClass());
		for (AActor* Actor : Inside)
		{
			TripBy(Actor);
		}
	}
}

void ARobberLaserBeam::OnBeamOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TripBy(OtherActor);
}

void ARobberLaserBeam::TripBy(AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (bLive && Pawn && Pawn->IsPlayerControlled() && GameState)
	{
		GameState->TripLaser(Pawn);
	}
}
