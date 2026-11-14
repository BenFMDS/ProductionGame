#include "Heist/RobberInteractables.h"
#include "Robber/RobberAchievementTracker.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProductionGameCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberStage.h"
#include "Net/UnrealNetwork.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"
#include "UObject/ConstructorHelpers.h"

ARobberInteractable::ARobberInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	RootComponent = Mesh;

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Mesh);
	LabelText->SetRelativeLocation(FVector(0, 0, 90));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextBottom);
	LabelText->SetWorldSize(30.f);
	LabelText->SetTextRenderColor(FColor::White);
	LabelText->SetAbsolute(false, false, true);
}

void ARobberInteractable::HandleInteract(AActor* InteractingPlayer)
{
	if (!HasAuthority())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(InteractingPlayer);
	if (AController* Controller = Cast<AController>(InteractingPlayer))
	{
		Pawn = Controller->GetPawn();
	}
	APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	if (!Player || !GameState || !IsActive(GameState))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastInteractTime.Find(Player); Last && Now - *Last < 0.2)
	{
		return;
	}
	LastInteractTime.Add(Player, Now);
	OnServerInteract(Pawn, Player, GameState);
}

void ARobberInteractable::ClearStaleOutline(AActor* Previous, AActor* Now)
{
	if (!IsValid(Previous) || Previous == Now)
	{
		return;
	}
	// Same component the trace outlines: BP_InteractMaster's Mesh.
	for (UActorComponent* Component : Previous->GetComponents())
	{
		UPrimitiveComponent* Primitive = Component->GetFName() == TEXT("Mesh") ? Cast<UPrimitiveComponent>(Component) : nullptr;
		if (Primitive)
		{
			Primitive->SetRenderCustomDepth(false);
		}
	}
}

FString ARobberInteractable::GetInteractPrompt() const
{
	const APlayerController* LocalPlayer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ARobberGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARobberGameState>() : nullptr;
	if (!GameState)
	{
		return Prompt.ToString();
	}
	return IsActive(GameState) ? GetPrompt(GameState, LocalPlayer ? LocalPlayer->PlayerState : nullptr).ToString() : FString();
}

void ARobberInteractable::ShowPart(UPrimitiveComponent* Part, bool bShown)
{
	Part->SetHiddenInGame(!bShown, true);
	Part->SetCollisionEnabled(bShown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

bool ARobberInteractable::IsActive(const ARobberGameState* GameState) const
{
	return !OwningEntry || (GameState && GameState->GetActiveEntry() == OwningEntry);
}

void ARobberInteractable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>())
	{
		LabelText->SetText(IsActive(GameState) ? GetLabel(GameState) : FText::GetEmpty());
	}
}

ARobberEntryPoint* ARobberVoteStation::GetEntry(const ARobberGameState* GameState) const
{
	return Entry ? Entry.Get() : GameState->FindEntry(EntryId);
}

void ARobberVoteStation::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	GameState->CastVote(Player, GetEntry(GameState));
}

FText ARobberVoteStation::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	const ARobberEntryPoint* Target = GetEntry(GameState);
	if (!RobberStage::IsInLobby(GameState->GetGameStage()) || !Target)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("RobberLobby", "VotePrompt", "Vote: {0}"), Target->DisplayName);
}

FText ARobberVoteStation::GetLabel(const ARobberGameState* GameState) const
{
	const ARobberEntryPoint* Target = GetEntry(GameState);
	if (!Target)
	{
		return EntryId.IsNone() ? NSLOCTEXT("RobberLobby", "NoEntry", "No entry set") : NSLOCTEXT("RobberLobby", "NotTonight", "Not tonight");
	}
	return FText::Format(NSLOCTEXT("RobberLobby", "VoteLabel", "{0}\n{1} {1}|plural(one=vote,other=votes)"), Target->DisplayName, GameState->GetVoteCount(Target));
}

ARobberTicketDesk::ARobberTicketDesk()
{
	Label = NSLOCTEXT("RobberTour", "DeskLabel", "TICKETS");
	Prompt = NSLOCTEXT("RobberTour", "DeskPrompt", "Buy a ticket");
}

void ARobberTicketDesk::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (GameState->GetGameStage() == RobberStage::BreakIn && Cast<ARobberTourEntryPoint>(GameState->GetActiveEntry()))
	{
		GameState->GiveTicket(Player);
	}
}

FText ARobberTicketDesk::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (GameState->GetGameStage() != RobberStage::BreakIn || !Cast<ARobberTourEntryPoint>(GameState->GetActiveEntry()))
	{
		return FText::GetEmpty();
	}
	return GameState->GetCrewStatus(Viewer).bHasTicket ? NSLOCTEXT("RobberTour", "HaveTicket", "You have a ticket") : Prompt;
}

ARobberHidingSpot::ARobberHidingSpot()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberTour", "SpotLabel", "Exhibit");
	Prompt = NSLOCTEXT("RobberTour", "SpotPrompt", "Hide here");

	Opening = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Opening"));
	Opening->SetupAttachment(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> OpeningCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpeningMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Opening->SetStaticMesh(OpeningCube.Object);
	Opening->SetMaterial(0, OpeningMaterial.Object);
	// The parent's scale applies, so this stays in proportion on any size of spot: it just needs to sit proud of the +X face.
	Opening->SetRelativeLocation(FVector(50.5, 0., -8.));
	Opening->SetRelativeScale3D(FVector(0.02, 0.6, 0.82));
	Opening->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Opening->SetCastShadow(false);
}

void ARobberHidingSpot::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInstanceDynamic* Material = Opening->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.008f, 0.006f));
	}
}

void ARobberHidingSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberHidingSpot, Occupant);
}

FVector ARobberHidingSpot::GetHideLocation() const
{
	const FVector Scale = GetActorScale3D();
	// 50 is half the unit cube, 98 lifts the capsule (half height 96) off the floor.
	return GetActorLocation() + GetActorForwardVector() * FMath::Max(0., Scale.X * 50. - 45.) + FVector(0., 0., 98. - Scale.Z * 50.);
}

FVector ARobberHidingSpot::GetExitLocation() const
{
	const FVector Scale = GetActorScale3D();
	return GetActorLocation() + GetActorForwardVector() * (Scale.X * 50. + 90.) + FVector(0., 0., 98. - Scale.Z * 50.);
}

void ARobberHidingSpot::SetOccupant(APawn* Pawn)
{
	Occupant = Pawn;
	ForceNetUpdate();
}

bool ARobberHidingSpot::IsGuardOnto(const APawn* Pawn, float MinDetection)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return false;
	}
	for (TActorIterator<AAIController> It(Pawn->GetWorld()); It; ++It)
	{
		UBlackboardComponent* Blackboard = It->GetBlackboardComponent();
		const APawn* Guard = It->GetPawn();
		if (!Blackboard || !Guard || Blackboard->GetValueAsObject(TEXT("TargetActor")) != Pawn)
		{
			continue;
		}
		// Detection lives on Oscar's BP_Guard, so it's read by name.
		const FDoubleProperty* Detection = FindFProperty<FDoubleProperty>(Guard->GetClass(), TEXT("Detection"));
		if (Detection && Detection->GetPropertyValue_InContainer(Guard) >= MinDetection)
		{
			return true;
		}
	}
	return false;
}

void ARobberHidingSpot::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	AProductionGameCharacter* Character = Cast<AProductionGameCharacter>(Pawn);
	if (!Character)
	{
		return;
	}
	ARobberTourEntryPoint* Tour = GameState->GetGameStage() == RobberStage::BreakIn ? Cast<ARobberTourEntryPoint>(GameState->GetActiveEntry()) : nullptr;
	if (Character->GetHidingSpot() == this)
	{
		Character->ExitHiding();
		if (Tour)
		{
			GameState->SetHidingSpot(Player, nullptr);
		}
		return;
	}
	if (Occupant || Character->IsHiding() || RobberStage::IsInLobby(GameState->GetGameStage()))
	{
		return;
	}
	if (Tour)
	{
		// On the Tour, hiding is also how you get through closing, and that needs a ticket.
		FText Reason;
		if (!Tour->TryHide(GameState, Player, this, Reason))
		{
			if (!Reason.IsEmpty())
			{
				ARobberController::Tell(Pawn, Reason);
			}
			return;
		}
	}
	Character->EnterHiding(this, IsGuardOnto(Pawn, SeenDetection));
}

FText ARobberHidingSpot::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	const AProductionGameCharacter* Character = Viewer ? Cast<AProductionGameCharacter>(Viewer->GetPawn()) : nullptr;
	if (Character && Character->GetHidingSpot() == this)
	{
		return NSLOCTEXT("RobberTour", "ClimbOut", "Climb out");
	}
	if (RobberStage::IsInLobby(GameState->GetGameStage()) || (Character && Character->IsHiding()))
	{
		return FText::GetEmpty();
	}
	if (Occupant)
	{
		return NSLOCTEXT("RobberTour", "SpotTaken", "Occupied");
	}
	return FText::Format(NSLOCTEXT("RobberTour", "HideIn", "Hide in the {0}"), Label);
}

ARobberLockedDoor::ARobberLockedDoor()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberDoor", "Label", "LOCKED");
	KeyName = NSLOCTEXT("RobberDoor", "DefaultKey", "key");
}

void ARobberLockedDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberLockedDoor, bOpen);
}

void ARobberLockedDoor::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	if (bOpen)
	{
		return;
	}
	const UPlayerQuestComponent* Quests = UPlayerQuestComponent::Of(Player);
	if (!IsInside(Pawn) && (!Quests || !Quests->HasKey(KeyId)))
	{
		ARobberController::Tell(Pawn, FText::Format(NSLOCTEXT("RobberDoor", "NeedKey", "Locked. You need the {0}."), KeyName));
		return;
	}
	if (KeyId == TEXT("Crowbar"))
	{
		RobberAchievements::AwardPlayer(Player, TEXT("DoorWhatDoor"));
	}
	bOpen = true;
	OnRep_Open();
	ForceNetUpdate();
}

FText ARobberLockedDoor::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (bOpen)
	{
		return FText::GetEmpty();
	}
	if (IsInside(Viewer ? Viewer->GetPawn() : nullptr))
	{
		return NSLOCTEXT("RobberDoor", "OpenInside", "Open the door");
	}
	const UPlayerQuestComponent* Quests = UPlayerQuestComponent::Of(Viewer);
	return Quests && Quests->HasKey(KeyId)
		? FText::Format(NSLOCTEXT("RobberDoor", "Unlock", "Unlock with the {0}"), KeyName)
		: FText::Format(NSLOCTEXT("RobberDoor", "Locked", "Locked (needs the {0})"), KeyName);
}

FText ARobberLockedDoor::GetLabel(const ARobberGameState* GameState) const
{
	return bOpen ? FText::GetEmpty() : Label;
}

void ARobberLockedDoor::OnRep_Open()
{
	// No door model yet: an open door just gets out of the way.
	ShowPart(Mesh, !bOpen);
}

bool ARobberLockedDoor::IsInside(const APawn* Pawn) const
{
	return bOpensFromInside && Pawn && FVector::DotProduct(Pawn->GetActorLocation() - GetActorLocation(), GetActorRightVector()) > 0.;
}

ARobberKeyPickup::ARobberKeyPickup()
{
	bReplicates = true;
	Label = NSLOCTEXT("RobberDoor", "KeyLabel", "KEY");
	KeyName = NSLOCTEXT("RobberDoor", "DefaultKey", "key");

	// Every frame while shown, so it can turn to the camera and bob smoothly; cheap, there are only a few keys.
	PrimaryActorTick.TickInterval = 0.f;
	Alert = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Alert"));
	Alert->SetupAttachment(Mesh);
	// Absolute so the key's flat scale (a badge is 4 cm thick) doesn't squash it; placed in world space in Tick.
	Alert->SetAbsolute(true, true, true);
	Alert->SetText(NSLOCTEXT("RobberDoor", "KeyAlert", "!"));
	Alert->SetHorizontalAlignment(EHTA_Center);
	Alert->SetVerticalAlignment(EVRTA_TextCenter);
	Alert->SetWorldSize(70.f);
	Alert->SetTextRenderColor(FColor(0xff, 0xc8, 0x1e));
	Alert->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Alert->SetCastShadow(false);
	Alert->SetHiddenInGame(true);
}

bool ARobberKeyPickup::IsAlertShowing() const
{
	return Alert && !Alert->bHiddenInGame;
}

void ARobberKeyPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The local player's own view: a dedicated server has none and never shows it.
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	const APawn* Viewer = Local && Local->IsLocalController() ? Local->GetPawn() : nullptr;
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	const bool bShow = Viewer && GameState && !bTaken && IsActive(GameState) && !RobberStage::IsInLobby(GameState->GetGameStage())
		&& FVector::DistSquared(Viewer->GetActorLocation(), GetActorLocation()) < FMath::Square(AlertRange);
	Alert->SetHiddenInGame(!bShow);
	if (!bShow)
	{
		return;
	}
	// Above the key, bobbing, and turned to face the camera (text is mirrored from behind). Pulled 40 cm towards the
	// viewer so a key hung on a wall rack (the staff badge) doesn't have the shelf above it hide the "!".
	const double Bob = 6. * FMath::Sin(GetWorld()->GetTimeSeconds() * 3.);
	const FVector Camera = Local->PlayerCameraManager ? Local->PlayerCameraManager->GetCameraLocation() : Viewer->GetActorLocation();
	FVector ToCamera = Camera - GetActorLocation();
	ToCamera.Z = 0.;
	const FVector Dir = ToCamera.GetSafeNormal();
	Alert->SetWorldLocationAndRotation(GetActorLocation() + Dir * 40. + FVector(0., 0., 60. + Bob), Dir.IsNearlyZero() ? FRotator::ZeroRotator : Dir.Rotation());
}

void ARobberKeyPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARobberKeyPickup, bTaken);
}

void ARobberKeyPickup::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	UPlayerQuestComponent* Quests = UPlayerQuestComponent::Of(Player);
	if (bTaken || !Quests || RobberStage::IsInLobby(GameState->GetGameStage()))
	{
		return;
	}
	Quests->GiveKey(KeyId);
	bTaken = true;
	OnRep_Taken();
	ForceNetUpdate();
	GameState->TellCrew(FText::Format(NSLOCTEXT("RobberDoor", "FoundKey", "{0} found the {1}."), FText::FromString(Player->GetPlayerName()), KeyName));
}

FText ARobberKeyPickup::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	return bTaken || RobberStage::IsInLobby(GameState->GetGameStage()) ? FText::GetEmpty()
		: FText::Format(NSLOCTEXT("RobberDoor", "TakeKey", "Take the {0}"), KeyName);
}

FText ARobberKeyPickup::GetLabel(const ARobberGameState* GameState) const
{
	return bTaken ? FText::GetEmpty() : Label;
}

void ARobberKeyPickup::OnRep_Taken()
{
	ShowPart(Mesh, !bTaken);
}

bool URobberLookOutlineSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

ARobberInteractable* URobberLookOutlineSubsystem::FindTarget() const
{
	const APlayerController* LocalPlayer = GetWorld()->GetFirstPlayerController();
	const APlayerCameraManager* Camera = LocalPlayer && LocalPlayer->IsLocalController() ? LocalPlayer->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return nullptr;
	}
	const FVector Eye = Camera->GetCameraLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RobberLookOutline), false, LocalPlayer->GetPawn());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Camera->GetCameraRotation().Vector() * Reach, ECC_Visibility, Params))
	{
		return nullptr;
	}
	ARobberInteractable* Prop = Cast<ARobberInteractable>(Hit.GetActor());
	// An empty prompt means pressing it would do nothing now (wrong stage, key taken), so it shouldn't light up.
	return Prop && !Prop->OutlinesItself() && !Prop->GetInteractPrompt().IsEmpty() ? Prop : nullptr;
}

void URobberLookOutlineSubsystem::SetOutline(bool bOn)
{
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Now;
	if (bOn && Outlined.IsValid())
	{
		// Every visible part, not just Mesh: doors, valves and cases are several pieces. Text and widgets would draw a box.
		for (UActorComponent* Component : Outlined->GetComponents())
		{
			UPrimitiveComponent* Part = Cast<UPrimitiveComponent>(Component);
			if (Part && Part->IsRegistered() && Part->IsVisible() && !Part->bHiddenInGame
				&& !Part->IsA<UTextRenderComponent>() && !Part->IsA<UWidgetComponent>())
			{
				Now.Add(Part);
			}
		}
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Part : Parts)
	{
		if (Part.IsValid() && !Now.Contains(Part))
		{
			Part->SetRenderCustomDepth(false);
		}
	}
	// Set every frame, not just on change: BPC_Interaction's trace clears Mesh on any non-BP_InteractMaster it hits.
	for (const TWeakObjectPtr<UPrimitiveComponent>& Part : Now)
	{
		Part->SetRenderCustomDepth(true);
	}
	Parts = MoveTemp(Now);
}

void URobberLookOutlineSubsystem::Tick(float DeltaTime)
{
	ARobberInteractable* Target = FindTarget();
	if (Target != Outlined.Get())
	{
		SetOutline(false);
		Outlined = Target;
	}
	SetOutline(Target != nullptr);
}
