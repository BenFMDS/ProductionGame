#include "Heist/RobberVisitor.h"

#include "AIController.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dialogue/QuestDialogueCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Navigation/PathFollowingComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace RobberVisitorAssets
{
	// All cooked through DirectoriesToAlwaysCook: only these text paths point at them.
	const TCHAR* CrewClass = TEXT("/Game/OurStuff/Core/Player/Game/BP_RobberCharacter.BP_RobberCharacter_C");
	const TCHAR* CrewMesh = TEXT("/Game/OurStuff/Animations/Character/Walking__3_.Walking__3_");
	const TCHAR* TintMaterial = TEXT("/Game/OurStuff/Assets/Materials/Character/M_VisitorTint.M_VisitorTint");
	const TCHAR* Idle = TEXT("/Game/OurStuff/Animations/Character/Idle.Idle");
	const TCHAR* Walk = TEXT("/Game/OurStuff/Animations/Character/Walking__3__Anim.Walking__3__Anim");

	// How far a visitor wanders for the next exhibit, and how close to its own floor.
	constexpr float PickRange = 4000.f;
	constexpr float FloorTolerance = 250.f;
	constexpr float StandOff = 200.f;
}

ARobberVisitor::ARobberVisitor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Spawned;
	bUseControllerRotationYaw = false;

	// Visitors mustn't stand between a player and the thing they're pressing E on, or be what a camera snaps to.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 140.f;
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 300.f, 0.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto MakeCapPart = [&](const TCHAR* Name, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(GetCapsuleComponent());
		Part->SetStaticMesh(Cylinder.Object);
		Part->SetMaterial(0, Plain.Object);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetHiddenInGame(true);
		return Part;
	};
	// Placed on the capsule's top in BeginPlay, once the crew's capsule size is known.
	Cap = MakeCapPart(TEXT("Cap"), FVector::ZeroVector, FVector(0.2f, 0.2f, 0.09f));
	CapBrim = MakeCapPart(TEXT("CapBrim"), FVector::ZeroVector, FVector(0.16f, 0.16f, 0.015f));
}

ARobberVisitor* ARobberVisitor::Spawn(UWorld* World, const FTransform& Transform, bool bInClerk, FLinearColor InColour, float InSize)
{
	ARobberVisitor* Visitor = World ? World->SpawnActorDeferred<ARobberVisitor>(StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn) : nullptr;
	if (!Visitor)
	{
		return nullptr;
	}
	Visitor->bClerk = bInClerk;
	Visitor->Colour = InColour;
	Visitor->Size = InSize;
	Visitor->FinishSpawning(Transform);
	if (!Visitor->GetController())
	{
		Visitor->SpawnDefaultController();
	}
	return Visitor;
}

void ARobberVisitor::BeginPlay()
{
	Super::BeginPlay();
	CopyCrewLook();
	SetActorScale3D(FVector(Size));

	if (HasAuthority() && !bClerk)
	{
		// Staggered, so the crowd doesn't all set off on the same frame.
		Timer = FMath::FRandRange(0.5f, 4.f);
	}
}

void ARobberVisitor::CopyCrewLook()
{
	using namespace RobberVisitorAssets;
	UClass* CrewClassObject = TSoftClassPtr<ACharacter>(FSoftObjectPath(CrewClass)).LoadSynchronous();
	const ACharacter* Crew = CrewClassObject ? GetDefault<ACharacter>(CrewClassObject) : nullptr;
	const USkeletalMeshComponent* From = Crew ? Crew->GetMesh() : nullptr;

	USkeletalMesh* MeshAsset = From ? From->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset)
	{
		MeshAsset = LoadObject<USkeletalMesh>(nullptr, CrewMesh);
	}
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(MeshAsset);
	if (From && Crew->GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCapsuleSize(Crew->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), Crew->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		Body->SetRelativeLocationAndRotation(From->GetRelativeLocation(), From->GetRelativeRotation());
		Body->SetRelativeScale3D(From->GetRelativeScale3D());
	}
	else
	{
		Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), FRotator(0.f, -90.f, 0.f));
	}
	// Network smoothing on clients offsets the mesh from where it was at PostInitializeComponents.
	CacheInitialMeshOffset(Body->GetRelativeLocation(), Body->GetRelativeRotation());

	UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TintMaterial);
	if (!Tint)
	{
		UE_LOG(LogTemp, Warning, TEXT("Visitor: %s is missing (run -run=ClayMaterial -VisitorTint), so visitors aren't tinted"), TintMaterial);
	}
	for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Base = Tint ? Tint : (From && Slot < From->GetNumMaterials() ? From->GetMaterial(Slot) : nullptr);
		if (UMaterialInstanceDynamic* Dynamic = Base ? Body->CreateDynamicMaterialInstance(Slot, Base) : nullptr)
		{
			Dynamic->SetVectorParameterValue(TEXT("Tint"), Colour);
		}
	}

	// Drive the crew's own idle/walk clips directly: the crew's character BP picks them in its graph, not an anim BP.
	const USkeleton* Skeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	IdleAnim = LoadObject<UAnimationAsset>(nullptr, Idle);
	WalkAnim = LoadObject<UAnimationAsset>(nullptr, Walk);
	if (!IdleAnim || !WalkAnim || IdleAnim->GetSkeleton() != Skeleton || WalkAnim->GetSkeleton() != Skeleton)
	{
		UE_LOG(LogTemp, Warning, TEXT("Visitor: idle/walk clips don't fit %s, using the crew's anim settings"), *GetNameSafe(MeshAsset));
		IdleAnim = WalkAnim = nullptr;
		if (From)
		{
			Body->SetAnimationMode(From->GetAnimationMode());
			Body->SetAnimInstanceClass(From->AnimClass);
		}
	}
	UpdateAnimation();

	// The clerk's cap sits on the head: top of the capsule, a touch down so it covers the crown.
	const float Top = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	Cap->SetRelativeLocation(FVector(0.f, 0.f, Top - 4.f));
	CapBrim->SetRelativeLocation(FVector(9.f, 0.f, Top - 8.f));
	Cap->SetHiddenInGame(!bClerk);
	CapBrim->SetHiddenInGame(!bClerk);
	for (UStaticMeshComponent* Part : { Cap.Get(), CapBrim.Get() })
	{
		if (UMaterialInstanceDynamic* Dynamic = Part->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.08f, 0.25f));
		}
	}
}

void ARobberVisitor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAnimation();
	if (HasAuthority() && !bClerk)
	{
		ThinkServer(DeltaSeconds);
	}
}

void ARobberVisitor::UpdateAnimation()
{
	if (!IdleAnim || !WalkAnim)
	{
		return;
	}
	const float Speed = GetVelocity().Size2D();
	UAnimationAsset* Want = Speed > 20.f ? WalkAnim.Get() : IdleAnim.Get();
	if (Want != PlayingAnim)
	{
		PlayingAnim = Want;
		GetMesh()->PlayAnimation(Want, true);
	}
}

void ARobberVisitor::ThinkServer(float DeltaSeconds)
{
	AAIController* Brain = Cast<AAIController>(GetController());
	if (!Brain)
	{
		return;
	}
	Timer -= DeltaSeconds;
	if (bWalking)
	{
		// Arrived, failed, or stuck for too long: either way, stop and have a look.
		if (Brain->GetMoveStatus() == EPathFollowingStatus::Idle || Timer <= 0.f)
		{
			Brain->StopMovement();
			bWalking = false;
			Timer = FMath::FRandRange(4.f, 9.f);
		}
		return;
	}
	if (const AActor* Target = LookingAt.Get())
	{
		const FVector To = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), To.Rotation(), DeltaSeconds, 3.f));
		}
	}
	if (Timer <= 0.f && !PickNextStop())
	{
		Timer = FMath::FRandRange(3.f, 6.f);
	}
}

bool ARobberVisitor::PickNextStop()
{
	using namespace RobberVisitorAssets;
	AAIController* Brain = Cast<AAIController>(GetController());
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Brain || !Nav)
	{
		return false;
	}
	const FVector Here = GetActorLocation();
	TArray<AActor*> Exhibits;
	for (TActorIterator<AQuestDialogueCharacter> It(GetWorld()); It; ++It)
	{
		const FVector There = It->GetActorLocation();
		if (*It != LookingAt.Get() && FMath::Abs(There.Z - Here.Z) < FloorTolerance + 200.f && FVector::Dist2D(There, Here) < PickRange)
		{
			Exhibits.Add(*It);
		}
	}

	FNavLocation Found;
	bool bFound = false;
	AActor* Target = nullptr;
	// A few tries: an exhibit on a wall faces out into the room, so stand off along its forward, else behind.
	for (int32 Try = 0; Try < 4 && !bFound && Exhibits.Num() > 0; ++Try)
	{
		Target = Exhibits[FMath::RandRange(0, Exhibits.Num() - 1)];
		for (const float Side : { 1.f, -1.f })
		{
			const FVector Want = Target->GetActorLocation() + Target->GetActorForwardVector() * StandOff * Side;
			if (Nav->ProjectPointToNavigation(Want, Found, FVector(100.f, 100.f, FloorTolerance)))
			{
				bFound = true;
				break;
			}
		}
	}
	if (!bFound)
	{
		Target = nullptr;
		bFound = Nav->GetRandomReachablePointInRadius(Here, 1500.f, Found);
	}
	if (!bFound)
	{
		return false;
	}
	LookingAt = Target;
	Stop = Found.Location;
	if (Brain->MoveToLocation(Stop, 60.f) == EPathFollowingRequestResult::Failed)
	{
		return false;
	}
	bWalking = true;
	Timer = 25.f;
	return true;
}

void ARobberVisitor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberVisitor, bClerk);
	DOREPLIFETIME(ARobberVisitor, Colour);
	DOREPLIFETIME(ARobberVisitor, Size);
}
