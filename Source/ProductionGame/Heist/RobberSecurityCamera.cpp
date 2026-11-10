#include "Heist/RobberSecurityCamera.h"

#include "ProductionGameCharacter.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberStage.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

namespace RobberCamera
{
	UStaticMeshComponent* MakeMesh(AActor* Owner, const TCHAR* Name, UStaticMesh* Shape, USceneComponent* Parent, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(Shape);
		Mesh->SetRelativeLocation(Location);
		Mesh->SetRelativeScale3D(Scale);
		// Nothing should snag on it or stop the camera's own traces.
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		return Mesh;
	}

	/** The basic cylinder is 100 across and 100 tall, pivot in the middle. */
	constexpr double DiscThickness = 0.01;

	struct FCircleColours
	{
		FLinearColor Fill;
		FLinearColor Border;
	};

	FCircleColours ColoursFor(ERobberCameraStatus Status)
	{
		switch (Status)
		{
		case ERobberCameraStatus::Watching: return { FLinearColor(1.f, 0.8f, 0.1f), FLinearColor(0.3f, 0.2f, 0.f) };
		case ERobberCameraStatus::Spotted: return { FLinearColor(1.f, 0.12f, 0.05f), FLinearColor(0.25f, 0.f, 0.f) };
		default: return { FLinearColor(0.45f, 0.75f, 1.f), FLinearColor(0.04f, 0.12f, 0.3f) };
		}
	}
}

ARobberSecurityCamera::ARobberSecurityCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetNetUpdateFrequency(10.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// Mount sticks out of the wall behind the head (local -X is the wall).
	Mount = RobberCamera::MakeMesh(this, TEXT("Mount"), Cube.Object, Root, FVector(-15.f, 0.f, 0.f), FVector(0.3f, 0.1f, 0.1f));

	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Body = RobberCamera::MakeMesh(this, TEXT("Body"), Cube.Object, Head, FVector(10.f, 0.f, 0.f), FVector(0.4f, 0.2f, 0.2f));
	Lens = RobberCamera::MakeMesh(this, TEXT("Lens"), Cylinder.Object, Head, FVector(32.f, 0.f, 0.f), FVector(0.12f, 0.12f, 0.06f));
	Lens->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));

	Cone = CreateDefaultSubobject<USpotLightComponent>(TEXT("Cone"));
	Cone->SetupAttachment(Head);
	Cone->SetRelativeLocation(FVector(36.f, 0.f, 0.f));
	Cone->SetMobility(EComponentMobility::Movable);
	Cone->SetIntensityUnits(ELightUnits::Lumens);
	Cone->SetIntensity(600.f);
	Cone->SetCastShadows(false);

	CircleBorder = RobberCamera::MakeMesh(this, TEXT("CircleBorder"), Cylinder.Object, Root, FVector::ZeroVector, FVector::OneVector);
	CircleFill = RobberCamera::MakeMesh(this, TEXT("CircleFill"), Cylinder.Object, Root, FVector::ZeroVector, FVector::OneVector);
	for (UStaticMeshComponent* Disc : { CircleBorder.Get(), CircleFill.Get() })
	{
		// Placed in the world each frame, so the head's pan and the actor's yaw don't tilt it.
		Disc->SetUsingAbsoluteLocation(true);
		Disc->SetUsingAbsoluteRotation(true);
		Disc->SetUsingAbsoluteScale(true);
		Disc->SetMaterial(0, ShapeMaterial.Object);
		Disc->SetHiddenInGame(true);
	}
}

void ARobberSecurityCamera::BeginPlay()
{
	Super::BeginPlay();
	BorderMaterial = CircleBorder->CreateDynamicMaterialInstance(0);
	FillMaterial = CircleFill->CreateDynamicMaterialInstance(0);
	const double Fill = FMath::Max(10., CircleRadius - BorderWidth);
	CircleBorder->SetWorldScale3D(FVector(CircleRadius / 50., CircleRadius / 50., RobberCamera::DiscThickness));
	CircleFill->SetWorldScale3D(FVector(Fill / 50., Fill / 50., RobberCamera::DiscThickness));
}

float ARobberSecurityCamera::SweepScale(uint8 Tier)
{
	static const float Scales[] = { 1.f, 1.25f, 1.6f, 1.6f };
	return Scales[FMath::Min<uint8>(Tier, 3)];
}

float ARobberSecurityCamera::SpotScale(uint8 Tier)
{
	static const float Scales[] = { 1.f, 0.8f, 0.5f, 0.5f };
	return Scales[FMath::Min<uint8>(Tier, 3)];
}

bool ARobberSecurityCamera::IsWatching(const ARobberGameState* GameState) const
{
	if (!GameState || GameState->IsPowerCut())
	{
		return false;
	}
	const uint8 Stage = GameState->GetGameStage();
	// A Tour crew is meant to be seen during the break-in; the cameras care once the museum's shut.
	return Stage == RobberStage::Investigate || Stage == RobberStage::Heist || Stage == RobberStage::Escape;
}

bool ARobberSecurityCamera::GetFloorCircle(FVector& OutCentre) const
{
	const FVector Eye = Cone->GetComponentLocation();
	// Players and the circle itself don't count: the view goes to the floor under them.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RobberCameraFloor), false, this);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerControlled())
		{
			Params.AddIgnoredActor(*It);
		}
	}
	FHitResult Hit;
	// A wall or a tall exhibit in the way gives no circle rather than one stuck to its side.
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Head->GetForwardVector() * Range, ECC_Visibility, Params) || Hit.ImpactNormal.Z < 0.7)
	{
		return false;
	}
	OutCentre = Hit.ImpactPoint;
	return true;
}

bool ARobberSecurityCamera::CanSee(const APawn* Pawn) const
{
	FVector Centre;
	const AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(Pawn);
	if (!Pawn || (Robber && (Robber->IsHiding() || Robber->IsDevImmune())) || !GetFloorCircle(Centre))
	{
		return false;
	}
	const FVector At = Pawn->GetActorLocation();
	// Standing on that floor (not on the storey above or below), inside the circle.
	if (FVector::Dist2D(At, Centre) > CircleRadius || FMath::Abs(At.Z - Centre.Z) > 250.)
	{
		return false;
	}
	// Other players don't hide each other: only the building blocks the view.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RobberCameraSight), false, this);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerControlled())
		{
			Params.AddIgnoredActor(*It);
		}
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Cone->GetComponentLocation(), At, ECC_Visibility, Params);
}

bool ARobberSecurityCamera::IsAwaitingGuardNear(const FVector& Location, float Radius) const
{
	return bAwaitingGuard && FVector::Dist(Disturbance, Location) <= Radius;
}

void ARobberSecurityCamera::ResumeScanning()
{
	if (!HasAuthority() || !bAwaitingGuard)
	{
		return;
	}
	bAwaitingGuard = false;
	WatchTime = 0.f;
	Target.Reset();
	// Pick the pan up from where it stopped rather than jumping.
	SweepPhase = SweepHalfAngle > 0.f ? FMath::Asin(FMath::Clamp(SweepYaw / SweepHalfAngle, -1.f, 1.f)) : 0.f;
}

void ARobberSecurityCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (HasAuthority())
	{
		TickServer(DeltaSeconds, GameState);
	}
	ApplyLook();
}

void ARobberSecurityCamera::TickServer(float DeltaSeconds, ARobberGameState* GameState)
{
	if (GameState && RobberStage::IsInLobby(GameState->GetGameStage()))
	{
		// New night: whatever it was waiting on is gone.
		bAwaitingGuard = false;
		SpottedUntil = 0.;
	}
	if (!IsWatching(GameState))
	{
		Status = ERobberCameraStatus::Off;
		WatchTime = 0.f;
		Target.Reset();
		return;
	}
	const uint8 Tier = static_cast<uint8>(GameState->GetHeatTier());
	const double Now = GetWorld()->GetTimeSeconds();
	if (bAwaitingGuard && GuardFallbackSeconds > 0.f && Now >= AwaitingSince + GuardFallbackSeconds)
	{
		ResumeScanning();
	}

	// Keep the one we're on while they're still in view, else take the first player in the circle.
	APawn* Seen = Target.IsValid() && CanSee(Target.Get()) ? Target.Get() : nullptr;
	if (!Seen)
	{
		// No hiding-spot check: the crew status keeps the spot they hid in for closing all night, and nobody is
		// actually tucked away once the cameras come on.
		for (TActorIterator<APawn> It(GetWorld()); It && !Seen; ++It)
		{
			if (It->IsPlayerControlled() && CanSee(*It))
			{
				Seen = *It;
			}
		}
	}
	Target = Seen;

	WatchTime = Seen ? WatchTime + DeltaSeconds : FMath::Max(0.f, WatchTime - DeltaSeconds);
	if (Seen && Now >= SpottedUntil && WatchTime >= SpotSeconds * SpotScale(Tier))
	{
		SpottedUntil = Now + SpottedCooldown;
		WatchTime = 0.f;
		bAwaitingGuard = true;
		AwaitingSince = Now;
		Disturbance = Seen->GetActorLocation();
		const APlayerState* Player = Seen->GetPlayerState();
		GameState->TellCrew(FText::Format(NSLOCTEXT("RobberCamera", "Spotted", "A camera spotted {0}! The guards are coming to look."),
			Player ? FText::FromString(Player->GetPlayerName()) : NSLOCTEXT("RobberCamera", "Someone", "someone")));
		GameState->AddHeat(RobberHeatCosts::CameraSpotted, TEXT("Camera"));
		GameState->ReportSighting(Disturbance, this);
	}
	// Stops panning while it has someone, and stays put on a spotted disturbance until a guard has been.
	if (!Seen && !bAwaitingGuard && SweepPeriod > 0.f)
	{
		SweepPhase = FMath::Fmod(SweepPhase + DeltaSeconds * SweepScale(Tier) * UE_TWO_PI / SweepPeriod, UE_TWO_PI);
		SweepYaw = SweepHalfAngle * FMath::Sin(SweepPhase);
	}
	Status = bAwaitingGuard ? ERobberCameraStatus::Spotted : Seen ? ERobberCameraStatus::Watching : ERobberCameraStatus::Sweeping;
}

void ARobberSecurityCamera::ApplyLook()
{
	Head->SetRelativeRotation(FRotator(-PitchDown, SweepYaw, 0.f));

	FVector Centre;
	const bool bCircle = Status != ERobberCameraStatus::Off && GetFloorCircle(Centre);
	CircleBorder->SetHiddenInGame(!bCircle);
	CircleFill->SetHiddenInGame(!bCircle);
	if (bCircle)
	{
		// Just above the floor so it doesn't flicker into it; fill a hair above the border.
		CircleBorder->SetWorldLocationAndRotation(Centre + FVector(0., 0., 1.), FRotator::ZeroRotator);
		CircleFill->SetWorldLocationAndRotation(Centre + FVector(0., 0., 1.6), FRotator::ZeroRotator);
		// Light cone about as wide as the circle.
		const float Distance = FVector::Dist(Cone->GetComponentLocation(), Centre);
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(CircleRadius, FMath::Max(1.f, Distance)));
		if (!FMath::IsNearlyEqual(Cone->OuterConeAngle, Angle, 0.5f))
		{
			Cone->SetOuterConeAngle(Angle);
			Cone->SetInnerConeAngle(Angle * 0.7f);
		}
	}

	if (ShownStatus == Status)
	{
		return;
	}
	ShownStatus = Status;
	const RobberCamera::FCircleColours Colours = RobberCamera::ColoursFor(Status);
	if (FillMaterial)
	{
		FillMaterial->SetVectorParameterValue(TEXT("Color"), Colours.Fill);
	}
	if (BorderMaterial)
	{
		BorderMaterial->SetVectorParameterValue(TEXT("Color"), Colours.Border);
	}
	Cone->SetAttenuationRadius(Range);
	Cone->SetVisibility(Status != ERobberCameraStatus::Off);
	Cone->SetLightColor(Colours.Fill);
}

void ARobberSecurityCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberSecurityCamera, SweepYaw);
	DOREPLIFETIME(ARobberSecurityCamera, Status);
}
