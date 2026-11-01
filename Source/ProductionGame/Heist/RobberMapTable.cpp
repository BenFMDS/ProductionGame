#include "Heist/RobberMapTable.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

namespace RobberMap
{
	constexpr double TopThickness = 6.;
	constexpr double Rim = 10.;
	constexpr double LegSize = 8.;
	constexpr double BaseHeight = 2.;
	constexpr double BaseRadius = 5.;
	constexpr double PinRing = 8.;
	constexpr double PinHeight = 10.;
	constexpr double PinBallSize = 3.5;
	constexpr double PinDropHeight = 20.;
	constexpr double PinDropSeconds = 0.3;
	/** Leaning out of reach of the table, or the night starting, puts the view back. */
	constexpr double StepBackDistance = 400.;

	const FLinearColor PinPalette[] = {
		FLinearColor(0.80f, 0.08f, 0.06f), FLinearColor(0.05f, 0.25f, 0.85f),
		FLinearColor(0.08f, 0.60f, 0.12f), FLinearColor(0.50f, 0.10f, 0.70f),
	};

	UStaticMesh* LoadShape(const TCHAR* Path)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(Path);
		return Shape.Succeeded() ? Shape.Object : nullptr;
	}

	ARobberController* LocalRobber(const UWorld* World)
	{
		return World ? Cast<ARobberController>(World->GetFirstPlayerController()) : nullptr;
	}
}

// --- Table ---

ARobberMapTable::ARobberMapTable()
{
	using namespace RobberMap;
	PrimaryActorTick.TickInterval = 0.f;

	TableRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TableRoot"));
	RootComponent = TableRoot;
	Mesh->SetupAttachment(TableRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MapMaterialBase(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided.Widget3DPassThrough_Opaque_OneSided"));
	MapSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MapSurface"));
	MapSurface->SetupAttachment(TableRoot);
	MapSurface->SetStaticMesh(Plane.Object);
	MapSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MapSurface->SetCastShadow(false);
	if (MapMaterialBase.Succeeded())
	{
		MapSurface->SetMaterial(0, MapMaterialBase.Object);
	}

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Leg = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Leg%d"), Index));
		Leg->SetupAttachment(TableRoot);
		Leg->SetStaticMesh(Mesh->GetStaticMesh());
		Legs.Add(Leg);
	}

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(TableRoot);
	ViewCamera->SetFieldOfView(70.f);
	ViewCamera->bConstrainAspectRatio = false;

	MapCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MapCapture"));
	MapCapture->SetupAttachment(TableRoot);
	MapCapture->SetUsingAbsoluteLocation(true);
	MapCapture->SetUsingAbsoluteRotation(true);
	MapCapture->SetUsingAbsoluteScale(true);
	MapCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	MapCapture->bCaptureEveryFrame = false;
	MapCapture->bCaptureOnMovement = false;
	// Flat colours, no lighting: reads like a printed plan, and the same in the dark nights as in daylight.
	MapCapture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	MapCapture->ShowFlags.SetFog(false);
	MapCapture->ShowFlags.SetVolumetricFog(false);
	MapCapture->ShowFlags.SetAtmosphere(false);
	MapCapture->ShowFlags.SetCloud(false);
	MapCapture->ShowFlags.SetParticles(false);

	LabelText->SetVisibility(false);
	Prompt = NSLOCTEXT("RobberMap", "TablePrompt", "Look at the map");
}

void ARobberMapTable::OnConstruction(const FTransform& Transform)
{
	using namespace RobberMap;
	Super::OnConstruction(Transform);
	const FVector2D Size = MapWorldSize * MapScale;
	const double TopZ = MapTopZ - TopThickness / 2.;
	Mesh->SetRelativeLocation(FVector(0., 0., TopZ));
	Mesh->SetRelativeScale3D(FVector((Size.X + 2. * Rim) / 100., (Size.Y + 2. * Rim) / 100., TopThickness / 100.));
	MapSurface->SetRelativeLocation(FVector(0., 0., MapTopZ + 0.2));
	MapSurface->SetRelativeScale3D(FVector(Size.X / 100., Size.Y / 100., 1.));
	const double LegHeight = MapTopZ - TopThickness;
	for (int32 Index = 0; Index < Legs.Num(); ++Index)
	{
		const FVector2D Corner(Index & 1 ? 1. : -1., Index & 2 ? 1. : -1.);
		Legs[Index]->SetRelativeLocation(FVector(Corner * (Size / 2. + Rim - LegSize), LegHeight / 2.));
		Legs[Index]->SetRelativeScale3D(FVector(LegSize / 100., LegSize / 100., LegHeight / 100.));
		Legs[Index]->SetMaterial(0, Mesh->GetMaterial(0));
	}
	ViewCamera->SetRelativeLocation(FVector(0., 0., MapTopZ + ViewHeight));
	AimViewCamera();

	// Straight down with the image's right = +X and down = +Y, so texture UV = world XY across the mapped area.
	MapCapture->SetWorldLocationAndRotation(FVector(MapWorldCentre, 8000.), FRotator(-90., -90., 0.));
	MapCapture->OrthoWidth = MapWorldSize.X;
}

void ARobberMapTable::BeginPlay()
{
	Super::BeginPlay();
	// Placed tables keep the camera spot saved with them; follow ViewHeight even if it changed since.
	ViewCamera->SetRelativeLocation(FVector(0., 0., MapTopZ + ViewHeight));
	AimViewCamera();
	if (GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender())
	{
		MapCapture->Deactivate();
		return;
	}
	MapTexture = NewObject<UTextureRenderTarget2D>(this, TEXT("MapTexture"));
	MapTexture->ClearColor = FLinearColor(0.55f, 0.50f, 0.40f);
	MapTexture->InitCustomFormat(MapResolution.X, MapResolution.Y, PF_FloatRGBA, true);
	MapTexture->UpdateResourceImmediate(true);
	MapCapture->TextureTarget = MapTexture;

	MapMaterial = MapSurface->CreateDynamicMaterialInstance(0);
	if (MapMaterial)
	{
		MapMaterial->SetTextureParameterValue(TEXT("SlateUI"), MapTexture);
		// The pass-through material is unlit; dim it so the map doesn't glow in a lamp-lit room.
		MapMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor(0.75f, 0.75f, 0.75f, 1.f));
		MapMaterial->SetScalarParameterValue(TEXT("OpacityFromTexture"), 1.f);
	}
}

FString ARobberMapTable::GetShownLevels() const
{
	FString Shown;
	for (const ULevelStreaming* Streaming : GetWorld()->GetStreamingLevels())
	{
		if (Streaming && Streaming->IsLevelVisible())
		{
			Shown += Streaming->GetWorldAssetPackageName() + TEXT(";");
		}
	}
	return Shown;
}

void ARobberMapTable::CaptureMap()
{
	if (MapTexture)
	{
		MapCapture->CaptureScene();
		++CaptureCount;
	}
}

void ARobberMapTable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	if (MapTexture && Now >= LevelsCheckTime)
	{
		LevelsCheckTime = Now + 0.5;
		const FString Shown = GetShownLevels();
		if (Shown != CapturedLevels)
		{
			CapturedLevels = Shown;
			PendingCaptures = { Now + 0.5, Now + 4. };
		}
	}
	if (PendingCaptures.Num() > 0 && Now >= PendingCaptures[0])
	{
		PendingCaptures.RemoveAt(0);
		CaptureMap();
	}

	if (APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController(); LocalPlayer && LocalPlayer->IsLocalController())
	{
		UpdateViewer(LocalPlayer);
		UpdateHover(LocalPlayer);
	}
}

void ARobberMapTable::UpdateViewer(APlayerController* LocalPlayer)
{
	ARobberController* Robber = Cast<ARobberController>(LocalPlayer);
	if (!Robber || Robber->GetViewedMapTable() != this)
	{
		return;
	}
	const APawn* Pawn = Robber->GetPawn();
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!Pawn || FVector::Dist2D(Pawn->GetActorLocation(), GetActorLocation()) > RobberMap::StepBackDistance
		|| (GameState && !RobberStage::IsInLobby(GameState->GetGameStage())))
	{
		Robber->StepBackFromMap();
	}
}

void ARobberMapTable::UpdateHover(APlayerController* LocalPlayer)
{
	ARobberMapObject* Hit = nullptr;
	const ARobberController* Robber = Cast<ARobberController>(LocalPlayer);
	if (Robber && Robber->GetViewedMapTable() == this)
	{
		// Leaning in: whatever is under the mouse cursor.
		Hit = Robber->GetMapPieceUnderCursor();
	}
	// Standing: the same crosshair trace as BPC_Interaction, which only outlines BP_InteractMaster children.
	else if (const APlayerCameraManager* Camera = LocalPlayer->PlayerCameraManager;
		Camera && FVector::Dist(Camera->GetCameraLocation(), GetActorLocation()) < 600.)
	{
		const FVector Start = Camera->GetCameraLocation();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MapTableHover), false, LocalPlayer->GetPawn());
		FHitResult Result;
		if (GetWorld()->LineTraceSingleByChannel(Result, Start, Start + Camera->GetCameraRotation().Vector() * 300., ECC_Visibility, Params))
		{
			Hit = Cast<ARobberMapObject>(Result.GetActor());
		}
	}
	if (Hit != HoveredPiece)
	{
		if (HoveredPiece)
		{
			HoveredPiece->SetHovered(false);
		}
		HoveredPiece = Hit;
		if (HoveredPiece)
		{
			HoveredPiece->SetHovered(true);
		}
	}
}

void ARobberMapTable::AimViewCamera()
{
	const FRotator Facing = GetActorRotation() + FRotator(0., ViewerYaw, 0.);
	ViewCamera->SetWorldRotation(FRotationMatrix::MakeFromXZ(-FVector::UpVector, Facing.Vector()).Rotator());
}

bool ARobberMapTable::ProjectToMap(const FVector& WorldLocation, FVector& OutMapLocation) const
{
	const FVector2D Offset = FVector2D(WorldLocation) - MapWorldCentre;
	if (FMath::Abs(Offset.X) > MapWorldSize.X / 2. || FMath::Abs(Offset.Y) > MapWorldSize.Y / 2.)
	{
		return false;
	}
	OutMapLocation = GetActorTransform().TransformPositionNoScale(FVector(Offset * MapScale, MapTopZ));
	return true;
}

void ARobberMapTable::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (!bCanLeanIn)
	{
		return;
	}
	if (ARobberController* Robber = Cast<ARobberController>(Pawn->GetController()))
	{
		Robber->ClientToggleMapView(this);
	}
}

FText ARobberMapTable::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	const ARobberController* Robber = RobberMap::LocalRobber(GetWorld());
	if (!Robber || Robber->GetViewedMapTable() != this)
	{
		return bCanLeanIn ? Prompt : FText::GetEmpty();
	}
	// The on-screen prompt comes from the centre of the view, which is on the table: name what the cursor is on instead.
	if (HoveredPiece)
	{
		return FText::FromString(HoveredPiece->GetInteractPrompt());
	}
	return NSLOCTEXT("RobberMap", "LeaningIn", "Click a piece to vote  |  Space: step back");
}

// --- Pieces ---

ARobberMapObject::ARobberMapObject()
{
	using namespace RobberMap;
	PrimaryActorTick.TickInterval = 0.f;

	PieceRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PieceRoot"));
	RootComponent = PieceRoot;
	Mesh->SetupAttachment(PieceRoot);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCastShadow(false);
	// Attached to the root, not the scaled mesh, so ObjectScale doesn't move the name.
	LabelText->SetupAttachment(PieceRoot);
	LabelText->SetRelativeLocation(FVector(0., 0., 17.));
	LabelText->SetWorldSize(3.f);
	LabelText->SetVerticalAlignment(EVRTA_TextBottom);
	LabelText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(PieceRoot);
	Base->SetMobility(EComponentMobility::Movable);
	Base->SetCastShadow(false);
	Base->SetStaticMesh(LoadShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Base->SetRelativeLocation(FVector(0., 0., BaseHeight / 2.));
	Base->SetRelativeScale3D(FVector(BaseRadius / 50., BaseRadius / 50., BaseHeight / 100.));

	PinNeedleMesh = Base->GetStaticMesh();
	PinBallMesh = LoadShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PinMaterial = ShapeMaterial.Object;
	ObjectMesh = Mesh->GetStaticMesh();
}

void ARobberMapObject::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyObjectMesh();
}

void ARobberMapObject::BeginPlay()
{
	Super::BeginPlay();
	ApplyObjectMesh();
}

void ARobberMapObject::ApplyObjectMesh()
{
	if (ObjectMesh && Mesh->GetStaticMesh() != ObjectMesh)
	{
		Mesh->SetStaticMesh(ObjectMesh);
	}
	Mesh->SetRelativeScale3D(ObjectScale);
	// Engine shapes are centred on their origin; stand it on the base.
	const double HalfHeight = ObjectMesh ? ObjectMesh->GetBounds().BoxExtent.Z * ObjectScale.Z : 0.;
	Mesh->SetRelativeLocation(FVector(0., 0., RobberMap::BaseHeight + HalfHeight));
}

FText ARobberMapObject::GetLabel(const ARobberGameState* GameState) const
{
	if (!bOnMap)
	{
		return FText::GetEmpty();
	}
	if (!DisplayText.IsEmpty())
	{
		return DisplayText;
	}
	const ARobberEntryPoint* Target = GetEntry(GameState);
	return Target ? Target->DisplayName : FText::GetEmpty();
}

void ARobberMapObject::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!GameState)
	{
		return;
	}
	UpdatePlacement(GameState);
	if (bOnMap)
	{
		UpdatePins(GameState);
		FaceLabelToCamera();
	}
}

void ARobberMapObject::UpdatePlacement(const ARobberGameState* GameState)
{
	// Entries come and go with the night sublevel, so look again a few times a second.
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < PlacementCheckTime)
	{
		return;
	}
	PlacementCheckTime = Now + 0.25;
	FVector Spot = GetActorLocation();
	const ARobberEntryPoint* Target = GetEntry(GameState);
	const bool bShow = Table && Target && Table->ProjectToMap(Target->GetActorLocation(), Spot);
	if (bShow && !GetActorLocation().Equals(Spot, 0.1))
	{
		SetActorLocation(Spot);
	}
	if (bShow != bOnMap || IsHidden() == bShow)
	{
		bOnMap = bShow;
		SetActorHiddenInGame(!bShow);
		SetActorEnableCollision(bShow);
		if (!bShow)
		{
			SetHovered(false);
		}
	}
}

void ARobberMapObject::UpdatePins(const ARobberGameState* GameState)
{
	using namespace RobberMap;
	const ARobberEntryPoint* Target = GetEntry(GameState);
	// Ordered by PlayerId, which every machine agrees on, so a player's pin has the same colour and spot everywhere.
	TArray<APlayerState*> Players;
	for (APlayerState* Player : GameState->PlayerArray)
	{
		if (Player)
		{
			Players.Add(Player);
		}
	}
	Players.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	if (Pins.Num() < Players.Num())
	{
		Pins.SetNum(Players.Num());
	}

	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Slot = 0; Slot < Pins.Num(); ++Slot)
	{
		APlayerState* Player = Players.IsValidIndex(Slot) ? Players[Slot] : nullptr;
		bool bVoted = false;
		for (const FRobberEntryVote& Vote : GameState->GetVotes())
		{
			bVoted |= Player && Target && Vote.Voter == Player && Vote.Entry == Target;
		}
		FPin& Pin = Pins[Slot];
		if (!bVoted)
		{
			Pin.Voter = nullptr;
			if (PinNeedles.IsValidIndex(Slot))
			{
				PinNeedles[Slot]->SetVisibility(false);
				PinBalls[Slot]->SetVisibility(false);
			}
			continue;
		}
		if (Pin.Voter != Player)
		{
			Pin.Voter = Player;
			Pin.DropTime = Now;
		}
		while (PinNeedles.Num() <= Slot)
		{
			AddPinParts();
		}
		const double Angle = FMath::DegreesToRadians(45. + 90. * Slot + 22.5 * (Slot / 4));
		const double Drop = 1. - FMath::Clamp((Now - Pin.DropTime) / PinDropSeconds, 0., 1.);
		const FVector Foot(FMath::Cos(Angle) * PinRing, FMath::Sin(Angle) * PinRing, Drop * Drop * PinDropHeight);
		PinNeedles[Slot]->SetRelativeLocation(Foot + FVector(0., 0., PinHeight / 2.));
		PinBalls[Slot]->SetRelativeLocation(Foot + FVector(0., 0., PinHeight));
		PinColours[Slot]->SetVectorParameterValue(TEXT("Color"), PinPalette[Player->GetPlayerId() % UE_ARRAY_COUNT(PinPalette)]);
		PinNeedles[Slot]->SetVisibility(true);
		PinBalls[Slot]->SetVisibility(true);
	}
}

void ARobberMapObject::AddPinParts()
{
	const auto MakePart = [this](UStaticMesh* Shape, const FVector& Scale, const FString& Name)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, *Name);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Shape);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetupAttachment(PieceRoot);
		Part->SetRelativeScale3D(Scale);
		Part->RegisterComponent();
		return Part;
	};
	const int32 Index = PinNeedles.Num();
	UStaticMeshComponent* Needle = MakePart(PinNeedleMesh, FVector(0.006, 0.006, RobberMap::PinHeight / 100.), FString::Printf(TEXT("PinNeedle%d"), Index));
	UStaticMeshComponent* Ball = MakePart(PinBallMesh, FVector(RobberMap::PinBallSize / 100.), FString::Printf(TEXT("PinBall%d"), Index));
	UMaterialInstanceDynamic* NeedleColour = Needle->CreateDynamicMaterialInstance(0, PinMaterial);
	if (NeedleColour)
	{
		NeedleColour->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.6f, 0.62f));
	}
	PinNeedles.Add(Needle);
	PinBalls.Add(Ball);
	PinColours.Add(Ball->CreateDynamicMaterialInstance(0, PinMaterial));
}

int32 ARobberMapObject::GetPinCount() const
{
	int32 Count = 0;
	for (const UStaticMeshComponent* Ball : PinBalls)
	{
		Count += Ball->IsVisible();
	}
	return Count;
}

void ARobberMapObject::FaceLabelToCamera()
{
	// Like BP_MenuObject's "Look at player": the name turns to whoever is looking, upright on their screen.
	const APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = LocalPlayer ? LocalPlayer->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	const FVector ToCamera = Camera->GetCameraLocation() - LabelText->GetComponentLocation();
	const FVector CameraUp = FRotationMatrix(Camera->GetCameraRotation()).GetUnitAxis(EAxis::Z);
	LabelText->SetWorldRotation(FRotationMatrix::MakeFromXZ(ToCamera, CameraUp).Rotator());
}

void ARobberMapObject::SetHovered(bool bInHovered)
{
	bHovered = bInHovered;
	Mesh->SetRenderCustomDepth(bHovered);
	Base->SetRenderCustomDepth(bHovered);
}
