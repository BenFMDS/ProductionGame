#include "Heist/RobberInteractables.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
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
	Label = NSLOCTEXT("RobberTour", "SpotLabel", "Exhibit");
	Prompt = NSLOCTEXT("RobberTour", "SpotPrompt", "Hide here");
}

void ARobberHidingSpot::OnServerInteract(APawn* Pawn, APlayerState* Player, ARobberGameState* GameState)
{
	ARobberTourEntryPoint* Tour = Cast<ARobberTourEntryPoint>(GameState->GetActiveEntry());
	if (!Tour || GameState->GetGameStage() != RobberStage::BreakIn || GameState->GetCrewStatus(Player).HidingSpot == this)
	{
		return;
	}
	FText Reason;
	if (!Tour->TryHide(GameState, Player, this, Reason) && !Reason.IsEmpty())
	{
		if (ARobberController* Controller = Cast<ARobberController>(Pawn->GetController()))
		{
			Controller->ClientHeistMessage(Reason);
		}
	}
}

FText ARobberHidingSpot::GetPrompt(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	if (GameState->GetGameStage() != RobberStage::BreakIn || GameState->GetCrewStatus(Viewer).HidingSpot)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("RobberTour", "HideIn", "Hide in the {0}"), Label);
}

namespace
{
	const UPlayerQuestComponent* FindQuestsConst(const APlayerState* Player)
	{
		const AController* Controller = Player ? Player->GetOwningController() : nullptr;
		return Controller ? Controller->FindComponentByClass<UPlayerQuestComponent>() : nullptr;
	}
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
	const UPlayerQuestComponent* Quests = FindQuestsConst(Player);
	if (!Quests || !Quests->HasKey(KeyId))
	{
		if (ARobberController* Controller = Cast<ARobberController>(Pawn->GetController()))
		{
			Controller->ClientHeistMessage(FText::Format(NSLOCTEXT("RobberDoor", "NeedKey", "Locked. You need the {0}."), KeyName));
		}
		return;
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
	const UPlayerQuestComponent* Quests = FindQuestsConst(Viewer);
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
	Mesh->SetHiddenInGame(bOpen, true);
	Mesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}
