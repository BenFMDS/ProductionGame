#include "Heist/RobberHideout.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Heist/RobberStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberLobbyObject::ARobberLobbyObject()
{
	// Every frame, not the base 0.25 s: the label turns with the camera, as BP_MenuObject's does.
	PrimaryActorTick.TickInterval = 0.f;
	Prompt = NSLOCTEXT("RobberHideout", "Prompt", "Interact With Object");
}

void ARobberLobbyObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = LocalPlayer ? LocalPlayer->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}

	const FVector Eye = Camera->GetCameraLocation();
	const FVector ToCamera = Eye - LabelText->GetComponentLocation();
	const FVector CameraUp = FRotationMatrix(Camera->GetCameraRotation()).GetUnitAxis(EAxis::Z);
	LabelText->SetWorldRotation(FRotationMatrix::MakeFromXZ(ToCamera, CameraUp).Rotator());

	// BPC_Interaction only outlines BP_InteractMaster children, so these outline themselves off the same trace.
	bool bAimed = false;
	if (FVector::Dist(Eye, GetActorLocation()) < 600.)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LobbyObjectHover), false, LocalPlayer->GetPawn());
		FHitResult Hit;
		bAimed = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Camera->GetCameraRotation().Vector() * 300., ECC_Visibility, Params)
			&& Hit.GetActor() == this;
	}
	if (bAimed != bHovered)
	{
		bHovered = bAimed;
		Mesh->SetRenderCustomDepth(bHovered);
	}
}

void ARobberLobbyObject::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (ARobberController* Controller = Pawn ? Cast<ARobberController>(Pawn->GetController()) : nullptr)
	{
		Controller->ClientOpenLobbyMenu(Menu);
	}
}

namespace RobberGems
{
	// One colour per night, Night 1 first.
	const FLinearColor Colours[ARobberGemCase::GemCount] = {
		FLinearColor(0.80f, 0.04f, 0.04f), FLinearColor(0.90f, 0.35f, 0.02f), FLinearColor(0.90f, 0.75f, 0.05f), FLinearColor(0.05f, 0.65f, 0.12f),
		FLinearColor(0.04f, 0.65f, 0.75f), FLinearColor(0.05f, 0.18f, 0.85f), FLinearColor(0.50f, 0.08f, 0.75f),
	};
}

ARobberGemCase::ARobberGemCase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	CaseRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CaseRoot"));
	RootComponent = CaseRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// Has the "Color" parameter BeginPlay tints; the sphere's own default material doesn't.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	for (int32 Index = 0; Index < GemCount; ++Index)
	{
		UStaticMeshComponent* Gem = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Gem%d"), Index + 1));
		Gem->SetupAttachment(CaseRoot);
		if (Sphere.Succeeded())
		{
			Gem->SetStaticMesh(Sphere.Object);
		}
		if (ShapeMaterial.Succeeded())
		{
			Gem->SetMaterial(0, ShapeMaterial.Object);
		}
		Gem->SetRelativeScale3D(FVector(0.22));
		Gem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Gems.Add(Gem);
	}
}

void ARobberGemCase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		Gems[Index]->SetRelativeLocation(GemSpots.IsValidIndex(Index) && GemSpots.Num() >= GemCount ? GemSpots[Index]
			: FVector((Index - (GemCount - 1) / 2.) * Spacing, 0., 0.));
	}
}

void ARobberGemCase::BeginPlay()
{
	Super::BeginPlay();
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Gems[Index]->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), RobberGems::Colours[Index]);
		}
	}
	ShowGems(GemsWon(GetWorld()->GetGameState<ARobberGameState>()));
}

int32 ARobberGemCase::GemsWon(const ARobberGameState* GameState)
{
	if (!GameState)
	{
		return 0;
	}
	if (GameState->GetGameStage() == RobberStage::Win)
	{
		return GemCount;
	}
	// The loaded night is the one being tried, so every night before it was won. The tutorial reads -1.
	return FMath::Clamp(GameState->GetCurrentNight(), 0, GemCount);
}

void ARobberGemCase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShowGems(GemsWon(GetWorld()->GetGameState<ARobberGameState>()));
}

void ARobberGemCase::ShowGems(int32 Count)
{
	if (Count == ShownCount)
	{
		return;
	}
	ShownCount = Count;
	for (int32 Index = 0; Index < Gems.Num(); ++Index)
	{
		Gems[Index]->SetVisibility(Index < Count);
	}
}
