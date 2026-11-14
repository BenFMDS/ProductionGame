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
#include "Heist/RobberHighlight.h"
#include "Heist/RobberMarker.h"
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

	// No shape here: each BP_ child sets its own (object restructure stage 4).
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));
	ModelRoot->SetupAttachment(Mesh);
	ModelRoot->SetMobility(EComponentMobility::Movable);
	for (int32 Index = 1; Index <= 6; ++Index)
	{
		ModelParts.Add(MakeModelPart(*FString::Printf(TEXT("Model%d"), Index)));
	}

	LabelText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelText->SetupAttachment(Mesh);
	LabelText->SetRelativeLocation(FVector(0, 0, 90));
	LabelText->SetHorizontalAlignment(EHTA_Center);
	LabelText->SetVerticalAlignment(EVRTA_TextBottom);
	LabelText->SetWorldSize(30.f);
	LabelText->SetTextRenderColor(FColor::White);
	LabelText->SetAbsolute(false, false, true);

	Highlight = CreateDefaultSubobject<URobberHighlightComponent>(TEXT("RobberHighlight"));
	InteractComponent = CreateDefaultSubobject<URobberInteractComponent>(TEXT("RobberInteract"));
}

void ARobberInteractable::PostLoad()
{
	Super::PostLoad();
	if (OwningEntry)
	{
		InteractComponent->OwningEntry = OwningEntry;
		OwningEntry = nullptr;
	}
}

UStaticMeshComponent* ARobberInteractable::MakeModelPart(const TCHAR* Name)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(ModelRoot);
	// Movable sits under any parent, static or not (map pieces and dials are movable), and door leaves slide.
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	return Part;
}

void ARobberInteractable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FitModel();
}

void ARobberInteractable::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Maps saved before the model existed (and cooked ones) don't rerun construction, so fit again here.
	FitModel();
}

bool ARobberInteractable::HasModel() const
{
	for (const USceneComponent* Child : ModelRoot->GetAttachChildren())
	{
		const UStaticMeshComponent* Part = Cast<UStaticMeshComponent>(Child);
		if (Part && Part->GetStaticMesh())
		{
			return true;
		}
	}
	return false;
}

void ARobberInteractable::FitModel()
{
	FBox Bounds(ForceInit);
	for (const USceneComponent* Child : ModelRoot->GetAttachChildren())
	{
		const UStaticMeshComponent* Part = Cast<UStaticMeshComponent>(Child);
		if (Part && Part->GetStaticMesh())
		{
			Bounds += Part->GetStaticMesh()->GetBoundingBox().TransformBy(Part->GetRelativeTransform());
		}
	}
	// bVisible, not hidden-in-game: ShowPart toggles that one (with children) and must not bring the box back.
	Mesh->SetVisibility(!(Bounds.IsValid && bModelReplacesMesh), false);
	if (!Bounds.IsValid || ModelFit == ERobberModelFit::None)
	{
		// Hand-placed parts live in Mesh's own (stretched) space.
		ModelRoot->SetUsingAbsoluteScale(false);
		ModelRoot->SetRelativeTransform(FTransform::Identity);
		return;
	}
	// Scale is set in world terms (absolute), so the turned model isn't stretched along Mesh's axes by Mesh's own scale.
	ModelRoot->SetUsingAbsoluteScale(true);
	const FVector MeshScale = Mesh->GetComponentScale().GetAbs().ComponentMax(FVector(KINDA_SMALL_NUMBER));
	const FVector BoxSize = MeshScale * 100.;
	const FQuat Turn = ModelRotation.Quaternion();
	FVector Room;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		FVector Along(0.);
		Along[Axis] = 1.;
		// How much of the box lies along this model axis once turned.
		Room[Axis] = FVector::DotProduct(Turn.RotateVector(Along).GetAbs(), BoxSize);
	}
	const FVector Size = Bounds.GetSize().ComponentMax(FVector(0.1));
	FVector Scale = Room / Size;
	if (ModelFit == ERobberModelFit::KeepProportions)
	{
		Scale = FVector(Scale.GetMin());
	}
	ModelRoot->SetRelativeRotation(ModelRotation);
	ModelRoot->SetRelativeScale3D(Scale);
	// The location is still in Mesh's scaled space, so undo that scale to centre the model on the box.
	ModelRoot->SetRelativeLocation(-Turn.RotateVector(Bounds.GetCenter() * Scale) / MeshScale);
}

void ARobberInteractable::HandleInteract(AActor* InteractingPlayer)
{
	InteractComponent->Press(InteractingPlayer);
}

FText ARobberInteractable::GetInteractText(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	// The subclasses' GetPrompt assumes a game state; without one (menus) the plain field is all there is.
	return GameState ? GetPrompt(GameState, Viewer) : Prompt;
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
	return InteractComponent->GetPrompt();
}

void ARobberInteractable::ShowPart(UPrimitiveComponent* Part, bool bShown)
{
	Part->SetHiddenInGame(!bShown, true);
	Part->SetCollisionEnabled(bShown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

bool ARobberInteractable::IsActive(const ARobberGameState* GameState) const
{
	return InteractComponent->IsActive(GameState);
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
	// Shape and material are in BP_HidingSpot. The parent's scale applies, so this stays in proportion on any size of spot: it just needs to sit proud of the +X face.
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
	DoorFrame = MakeModelPart(TEXT("DoorFrame"));
	LeftLeaf = MakeModelPart(TEXT("LeftLeaf"));
	RightLeaf = MakeModelPart(TEXT("RightLeaf"));
}

FVector ARobberLockedDoor::OpenOffset(const UStaticMeshComponent* Leaf)
{
	if (!Leaf->GetStaticMesh())
	{
		return FVector::ZeroVector;
	}
	// A leaf's width is along its own Y. It slides that far, away from the frame's middle.
	const FBox Box = Leaf->GetStaticMesh()->GetBoundingBox();
	const FTransform Relative = Leaf->GetRelativeTransform();
	const FVector Across = Relative.TransformVectorNoScale(FVector::YAxisVector);
	const FVector Centre = Relative.TransformPosition(Box.GetCenter());
	const double Side = FVector::DotProduct(Centre, Across) < 0. ? -1. : 1.;
	return Across * Side * Box.GetSize().Y * FMath::Abs(Relative.GetScale3D().Y);
}

void ARobberLockedDoor::StepSlide()
{
	const double Alpha = SlideTime > 0.f ? FMath::Clamp((GetWorld()->GetTimeSeconds() - SlideStart) / SlideTime, 0., 1.) : 1.;
	const double Eased = FMath::InterpEaseInOut(0., 1., Alpha, 2.);
	LeftLeaf->SetRelativeLocation(LeftClosed + OpenOffset(LeftLeaf) * Eased);
	RightLeaf->SetRelativeLocation(RightClosed + OpenOffset(RightLeaf) * Eased);
	if (Alpha >= 1.)
	{
		GetWorldTimerManager().ClearTimer(SlideTimer);
	}
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
	if (!LeftLeaf->GetStaticMesh() && !RightLeaf->GetStaticMesh())
	{
		// No leaves to slide: an open door just gets out of the way.
		ShowPart(Mesh, !bOpen);
		return;
	}
	// The frame stays; the box stops blocking and the leaves slide into the walls.
	Mesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	LeftClosed = CastChecked<USceneComponent>(LeftLeaf->GetArchetype())->GetRelativeLocation();
	RightClosed = CastChecked<USceneComponent>(RightLeaf->GetArchetype())->GetRelativeLocation();
	if (!bOpen)
	{
		GetWorldTimerManager().ClearTimer(SlideTimer);
		LeftLeaf->SetRelativeLocation(LeftClosed);
		RightLeaf->SetRelativeLocation(RightClosed);
		return;
	}
	SlideStart = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(SlideTimer, this, &ARobberLockedDoor::StepSlide, 1.f / 60.f, true);
	StepSlide();
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

	// New name: placed keys saved the old text "!" component as "Alert", and a different class under that name won't load cleanly.
	Alert = CreateDefaultSubobject<URobberMarkerComponent>(TEXT("KeyMarker"));
	Alert->SetupAttachment(Mesh);
	Alert->MarkRange = 600.f;
	// The staff badge hangs on a wall rack under a shelf, which would otherwise hide it.
	Alert->bNeedsClearView = false;
}

bool ARobberKeyPickup::IsAlertShowing() const
{
	return Alert && Alert->IsMarkShowing();
}

void ARobberKeyPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ARobberGameState* GameState = GetWorld()->GetGameState<ARobberGameState>();
	Alert->SetMarkerEnabled(GameState && !bTaken && IsActive(GameState) && !RobberStage::IsInLobby(GameState->GetGameStage()));
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

